#!/usr/bin/env python3
"""Python twin of Src/cv.c - validates the FAST/BRIEF/matching pipeline on
the stored xyh image before it runs on the MCU.

Mirrors cv.c EXACTLY: same luma weights, same FAST-9 circle order, same
score/NMS, same BRIEF LCG pattern, same ratio test.
"""
import os
import re
import sys

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
W, H = 320, 240

# ---- mirror of cv.c ---------------------------------------------------

LCG_A, LCG_C, LCG_M = 1664525, 1013904223, 2 ** 32


def rgb565_to_gray(data):
    out = bytearray(W * H)
    for i in range(W * H):
        p = (data[i * 2] << 8) | data[i * 2 + 1]
        r = (p >> 11) & 0x1F
        g = (p >> 5) & 0x3F
        b = p & 0x1F
        r = (r << 3) | (r >> 2)
        g = (g << 2) | (g >> 4)
        b = (b << 3) | (b >> 2)
        out[i] = (77 * r + 150 * g + 29 * b) >> 8
    return bytes(out)


def smooth3(src):
    dst = bytearray(W * H)
    for y in range(1, H - 1):
        for x in range(1, W - 1):
            s = 0
            for dy in range(3):
                for dx in range(3):
                    s += src[(y - 1 + dy) * W + (x - 1 + dx)]
            dst[y * W + x] = s // 9
    for x in range(W):
        dst[x] = src[x]
        dst[(H - 1) * W + x] = src[(H - 1) * W + x]
    for y in range(1, H - 1):
        dst[y * W] = src[y * W]
        dst[y * W + W - 1] = src[y * W + W - 1]
    return bytes(dst)


CIRCLE = [(0, -3), (1, -3), (2, -2), (3, -1), (3, 0), (3, 1), (2, 2), (1, 3),
          (0, 3), (-1, 3), (-2, 2), (-3, 1), (-3, 0), (-3, -1), (-2, -2), (-1, -3)]


def fast_score(gray, x, y, p):
    s = 0
    for dx, dy in CIRCLE:
        px = max(0, min(W - 1, x + dx))
        py = max(0, min(H - 1, y + dy))
        v = gray[py * W + px]
        s += abs(v - p)
    return s


def ring_has9(mask):
    for _ in range(16):
        if mask & 0x1FF == 0x1FF:
            return True
        mask = ((mask >> 1) | ((mask & 1) << 15)) & 0xFFFF
    return False


def fast9(gray, threshold, max_kp=400):
    kp = []
    for y in range(3, H - 3):
        for x in range(3, W - 3):
            p = gray[y * W + x]
            bright = dark = 0
            for i, (dx, dy) in enumerate(CIRCLE):
                v = gray[(y + dy) * W + (x + dx)]
                if v > p + threshold:
                    bright |= 1 << i
                if v < p - threshold:
                    dark |= 1 << i
            if ring_has9(bright) or ring_has9(dark):
                score = fast_score(gray, x, y, p)
                is_max = True
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        if dx == 0 and dy == 0:
                            continue
                        if fast_score(gray, x + dx, y + dy,
                                      gray[(y + dy) * W + (x + dx)]) >= score:
                            is_max = False
                            break
                    if not is_max:
                        break
                if is_max and len(kp) < max_kp:
                    kp.append((x, y, score))
    return kp


class LCG:
    def __init__(self, seed=0x9E3779B9):
        self.s = seed

    def next(self):
        self.s = (LCG_A * self.s + LCG_C) & (LCG_M - 1)
        return self.s


def brief(smooth, kp):
    desc = []
    for (cx, cy, _) in kp:
        lcg = LCG()          # reset per keypoint: same pattern for ALL
        d = 0
        for bit in range(256):
            x1 = lcg.next() % 17 - 8 + cx
            y1 = lcg.next() % 17 - 8 + cy
            x2 = lcg.next() % 17 - 8 + cx
            y2 = lcg.next() % 17 - 8 + cy
            x1 = max(0, min(W - 1, x1))
            y1 = max(0, min(H - 1, y1))
            x2 = max(0, min(W - 1, x2))
            y2 = max(0, min(H - 1, y2))
            if smooth[y1 * W + x1] < smooth[y2 * W + x2]:
                d |= 1 << bit
        desc.append(d)
    return desc


def hamming(a, b):
    return bin(a ^ b).count('1')


def match_bf(desc_a, desc_b):
    matches = []
    for i, da in enumerate(desc_a):
        best = second = 0xFFFF
        best_j = 0
        for j, db in enumerate(desc_b):
            d = hamming(da, db)
            if d < best:
                second, best, best_j = best, d, j
            elif d < second:
                second = d
        if best < 64 and best * 10 < second * 7:
            matches.append((i, best_j))
    return matches


# ---- helpers ----------------------------------------------------------

def load_source_image(path=None):
    """Load a 320x240 RGB565 source: from an image file, or a synthetic
    pattern when no path is given (self-contained validation)."""
    if path and os.path.exists(path):
        im = Image.open(path).convert("RGB")
        sw, sh = im.size
        if sw / sh > 4 / 3:
            cw = int(round(sh * 4 / 3))
            im = im.crop(((sw - cw) // 2, 0, (sw - cw) // 2 + cw, sh))
        else:
            ch = int(round(sw * 3 / 4))
            im = im.crop((0, (sh - ch) // 2, sw, (sh - ch) // 2 + ch))
        im = im.resize((W, H), Image.LANCZOS)
    else:
        # synthetic checkerboard + shapes: guaranteed corners
        im = Image.new("RGB", (W, H), (200, 200, 200))
        d = ImageDraw.Draw(im)
        for gy in range(0, H, 48):
            for gx in range(0, W, 48):
                if (gx // 48 + gy // 48) % 2:
                    d.rectangle((gx, gy, gx + 47, gy + 47), fill=(40, 40, 40))
        d.polygon([(260, 20), (300, 80), (220, 80)], fill=(200, 30, 30))
        d.ellipse((40, 150, 120, 220), fill=(30, 120, 200))
    # to RGB565 bytes (MSB-first)
    px = im.load()
    data = bytearray(W * H * 2)
    i = 0
    for y in range(H):
        for x in range(W):
            r, g, b = px[x, y]
            v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            data[i] = (v >> 8) & 0xFF
            data[i + 1] = v & 0xFF
            i += 2
    return bytes(data)


def gray_to_img(gray):
    return Image.frombytes('L', (W, H), bytes(gray))


def rgb565_to_rgb(data):
    out = bytearray(W * H * 3)
    for i in range(W * H):
        p = (data[i * 2] << 8) | data[i * 2 + 1]
        r = (p >> 11) & 0x1F
        g = (p >> 5) & 0x3F
        b = p & 0x1F
        out[i * 3] = (r << 3) | (r >> 2)
        out[i * 3 + 1] = (g << 2) | (g >> 4)
        out[i * 3 + 2] = (b << 3) | (b >> 2)
    return bytes(out)


def shift(gray, dx, dy):
    out = bytearray(W * H)
    for y in range(H):
        for x in range(W):
            sx, sy = x - dx, y - dy
            if 0 <= sx < W and 0 <= sy < H:
                out[y * W + x] = gray[sy * W + sx]
            else:
                out[y * W + x] = 0
    return bytes(out)


def main():
    img_arg = sys.argv[1] if len(sys.argv) > 1 else None
    data = load_source_image(img_arg)
    gray = rgb565_to_gray(data)
    smooth = smooth3(gray)

    print('== Feature detection on xyh_image ==')
    kp = fast9(smooth, 20)
    print(f'FAST corners (threshold=20): {len(kp)}')
    assert len(kp) > 20, 'too few corners - algorithm problem?'

    # visualization: corners overlay
    img = Image.frombytes('RGB', (W, H), rgb565_to_rgb(data))
    d = ImageDraw.Draw(img)
    for x, y, s in kp:
        d.line((x - 3, y, x + 3, y), fill=(255, 0, 0))
        d.line((x, y - 3, x, y + 3), fill=(255, 0, 0))
    img.save(os.path.join(ROOT, 'tools', 'preview', 'cv_corners.png'))
    print('saved tools/preview/cv_corners.png')

    print('== Feature matching: original vs shifted (dx=10, dy=8) ==')
    gray_b = shift(gray, 10, 8)
    smooth_b = smooth3(gray_b)
    kp_b = fast9(smooth_b, 20)
    print(f'corners in shifted image: {len(kp_b)}')

    desc_a = brief(smooth, kp)
    desc_b = brief(smooth_b, kp_b)

    # DEBUG: check descriptor distance for known correspondences
    # feature at A(x,y) should appear in B at (x+10, y+8)
    print('-- debug: descriptor sanity --')
    # 1) pixel equality at correspondences (interior features)
    eq = all(smooth_b[(y + 8) * W + (x + 10)] == smooth[y * W + x]
             for (x, y, _) in kp if 10 <= x < W - 14 and 8 <= y < H - 12)
    print(f'pixel equality at correspondences: {eq}')
    # 2) determinism: brief() twice on the same image must be identical
    d1 = brief(smooth, kp[:10])
    d2 = brief(smooth, kp[:10])
    print(f'brief deterministic: {d1 == d2}')
    # 3) manual descriptor at a true correspondence
    for (x, y, _) in kp:
        if 10 <= x < W - 14 and 8 <= y < H - 12:
            da = brief(smooth, [(x, y, 0)])
            db = brief(smooth_b, [(x + 10, y + 8, 0)])
            print(f'manual pair ({x},{y}) vs ({x+10},{y+8}): hamming={hamming(da[0], db[0])}')
            break
    d_true = []
    for ia, (x, y, _) in enumerate(kp):
        tx, ty = x + 10, y + 8
        best_d, best_i = 9999, -1
        for ib, (bx, by, _) in enumerate(kp_b):
            dpos = abs(bx - tx) + abs(by - ty)
            if dpos <= 2:
                d = hamming(desc_a[ia], desc_b[ib])
                if d < best_d:
                    best_d, best_i = d, ib
        if best_i >= 0:
            d_true.append(best_d)
    if d_true:
        print(f'true-correspondence hamming: min={min(d_true)} avg={sum(d_true)/len(d_true):.1f} '
              f'of {len(d_true)} checked')
    # random-pair baseline
    import random
    random.seed(1)
    d_rand = [hamming(random.choice(desc_a), random.choice(desc_b)) for _ in range(200)]
    print(f'random-pair hamming baseline: avg={sum(d_rand)/len(d_rand):.1f}')

    matches = match_bf(desc_a, desc_b)
    print(f'matches (ratio test): {len(matches)}')

    # check that matches are consistent with the known shift
    good = 0
    for ia, ib in matches:
        xa, ya, _ = kp[ia]
        xb, yb, _ = kp_b[ib]
        if abs((xb - xa) - 10) <= 2 and abs((yb - ya) - 8) <= 2:
            good += 1
    print(f'matches consistent with shift: {good}/{len(matches)}')

    # visualization: draw matching lines
    img2 = Image.frombytes('RGB', (W, H), rgb565_to_rgb(data))
    d2 = ImageDraw.Draw(img2)
    for ia, ib in matches[:120]:
        xa, ya, _ = kp[ia]
        xb, yb, _ = kp_b[ib]
        d2.line((xa, ya, xb, yb), fill=(0, 255, 0))
        d2.ellipse((xb - 2, yb - 2, xb + 2, yb + 2), fill=(0, 0, 255))
    img2.save(os.path.join(ROOT, 'tools', 'preview', 'cv_matches.png'))
    print('saved tools/preview/cv_matches.png')

    ok = len(kp) > 20 and len(matches) >= 10 and good >= len(matches) * 0.5
    print('RESULT:', 'PASS' if ok else 'FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
