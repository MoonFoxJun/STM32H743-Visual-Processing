/**
 * @file cv.c
 * @brief Lightweight computer vision library (see cv.h).
 *
 * Fixed-point, stack-light, no heap. Mirrors tools/cv_reference.py exactly
 * (same weights, same FAST circle order, same LCG pattern for BRIEF).
 */
#include "cv.h"

/* ------------------------------------------------------------------ */
/* Color conversion                                                    */
/* ------------------------------------------------------------------ */

void cv_rgb565_to_gray(const uint8_t *rgb565, uint8_t *gray)
{
    uint32_t i;
    for (i = 0U; i < CV_W * CV_H; i++)
    {
        uint16_t p = (uint16_t)((rgb565[i * 2U] << 8) | rgb565[i * 2U + 1U]);
        uint32_t r = (p >> 11) & 0x1FU;
        uint32_t g = (p >> 5) & 0x3FU;
        uint32_t b = p & 0x1FU;
        /* expand 5/6/5 -> 8 bit */
        r = (r << 3) | (r >> 2);
        g = (g << 2) | (g >> 4);
        b = (b << 3) | (b >> 2);
        /* BT.601 luma, fixed point */
        gray[i] = (uint8_t)((77U * r + 150U * g + 29U * b) >> 8);
    }
}

/* ------------------------------------------------------------------ */
/* Smoothing                                                           */
/* ------------------------------------------------------------------ */

void cv_smooth3(const uint8_t *src, uint8_t *dst)
{
    uint32_t x, y;
    for (y = 1U; y < CV_H - 1U; y++)
    {
        for (x = 1U; x < CV_W - 1U; x++)
        {
            uint32_t s = 0U;
            uint32_t dy, dx;
            for (dy = 0U; dy < 3U; dy++)
            {
                for (dx = 0U; dx < 3U; dx++)
                {
                    s += src[(y - 1U + dy) * CV_W + (x - 1U + dx)];
                }
            }
            dst[y * CV_W + x] = (uint8_t)(s / 9U);
        }
    }
    /* borders: copy */
    for (x = 0U; x < CV_W; x++)
    {
        dst[x] = src[x];
        dst[(CV_H - 1U) * CV_W + x] = src[(CV_H - 1U) * CV_W + x];
    }
    for (y = 1U; y < CV_H - 1U; y++)
    {
        dst[y * CV_W] = src[y * CV_W];
        dst[y * CV_W + CV_W - 1U] = src[y * CV_W + CV_W - 1U];
    }
}

/* ------------------------------------------------------------------ */
/* FAST-9 corner detection                                             */
/* ------------------------------------------------------------------ */

/* Bresenham circle of radius 3 around (0,0), starting at 12 o'clock,
 * clockwise (the classic FAST ordering) */
static const int8_t s_circle_x[16] = {0, 1, 2, 3, 3, 3, 2, 1,
                                      0, -1, -2, -3, -3, -3, -2, -1};
static const int8_t s_circle_y[16] = {-3, -3, -2, -1, 0, 1, 2, 3,
                                      3, 3, 2, 1, 0, -1, -2, -3};

static uint16_t fast_score(const uint8_t *gray, int32_t x, int32_t y, uint8_t p)
{
    uint32_t s = 0U;
    uint32_t i;
    for (i = 0U; i < 16U; i++)
    {
        int32_t px = x + s_circle_x[i];
        int32_t py = y + s_circle_y[i];
        int32_t v;
        if (px < 0)
        {
            px = 0;
        }
        else if (px >= (int32_t)CV_W)
        {
            px = CV_W - 1;
        }
        if (py < 0)
        {
            py = 0;
        }
        else if (py >= (int32_t)CV_H)
        {
            py = CV_H - 1;
        }
        v = gray[py * (int32_t)CV_W + px];
        s += (uint32_t)((v > p) ? (v - p) : (p - v));
    }
    return (uint16_t)s;
}

/* true if the 16-bit ring mask has 9 consecutive set bits (circular) */
static int ring_has9(uint16_t mask)
{
    uint32_t r;
    for (r = 0U; r < 16U; r++)
    {
        if ((mask & 0x1FFU) == 0x1FFU)
        {
            return 1;
        }
        mask = (uint16_t)((mask >> 1) | ((mask & 1U) << 15));
    }
    return 0;
}

uint16_t cv_fast9(const uint8_t *gray, uint8_t threshold,
                  cv_keypoint_t *kp, uint16_t max)
{
    uint16_t count = 0U;
    int32_t x, y;

    for (y = 3; y < (int32_t)CV_H - 3; y++)
    {
        for (x = 3; x < (int32_t)CV_W - 3; x++)
        {
            uint8_t p = gray[y * (int32_t)CV_W + x];
            uint16_t bright = 0U;
            uint16_t dark = 0U;
            uint32_t i;

            for (i = 0U; i < 16U; i++)
            {
                uint8_t v = gray[(y + s_circle_y[i]) * (int32_t)CV_W + (x + s_circle_x[i])];
                if (v > (uint8_t)(p + threshold))
                {
                    bright |= (uint16_t)(1U << i);
                }
                if (v < (uint8_t)(p - threshold))
                {
                    dark |= (uint16_t)(1U << i);
                }
            }

            if (ring_has9(bright) || ring_has9(dark))
            {
                uint16_t score = fast_score(gray, x, y, p);
                int32_t dy, dx;
                int is_max = 1;
                /* non-maximum suppression over the 8 neighbours */
                for (dy = -1; dy <= 1 && is_max; dy++)
                {
                    for (dx = -1; dx <= 1 && is_max; dx++)
                    {
                        if (dx == 0 && dy == 0)
                        {
                            continue;
                        }
                        if (fast_score(gray, x + dx, y + dy, gray[(y + dy) * (int32_t)CV_W + (x + dx)]) >= score)
                        {
                            is_max = 0;
                        }
                    }
                }
                if (is_max && count < max)
                {
                    kp[count].x = (uint16_t)x;
                    kp[count].y = (uint16_t)y;
                    kp[count].score = score;
                    count++;
                }
            }
        }
    }
    return count;
}

/* ------------------------------------------------------------------ */
/* BRIEF descriptors                                                   */
/* ------------------------------------------------------------------ */

/* Deterministic LCG (Numerical Recipes) shared with the Python twin */
static uint32_t s_lcg_state = 0x9E3779B9U;

static uint32_t lcg_next(void)
{
    s_lcg_state = (uint32_t)(1664525U * s_lcg_state + 1013904223U);
    return s_lcg_state;
}

void cv_brief(const uint8_t *smooth, const cv_keypoint_t *kp, uint16_t n,
              uint32_t *desc)
{
    uint16_t k;
    for (k = 0U; k < n; k++)
    {
        uint32_t *d = &desc[k * CV_BRIEF_WORDS];
        uint32_t bit;
        /* Reset the LCG for EVERY keypoint: all descriptors must use the
         * SAME 256-pair sampling pattern to be comparable across images. */
        s_lcg_state = 0x9E3779B9U;
        for (bit = 0U; bit < 256U; bit++)
        {
            int32_t x1, y1, x2, y2;
            int32_t cx = kp[k].x;
            int32_t cy = kp[k].y;

            x1 = (int32_t)(lcg_next() % 17U) - 8;
            y1 = (int32_t)(lcg_next() % 17U) - 8;
            x2 = (int32_t)(lcg_next() % 17U) - 8;
            y2 = (int32_t)(lcg_next() % 17U) - 8;

            x1 += cx; if (x1 < 0) x1 = 0; else if (x1 >= (int32_t)CV_W) x1 = CV_W - 1;
            y1 += cy; if (y1 < 0) y1 = 0; else if (y1 >= (int32_t)CV_H) y1 = CV_H - 1;
            x2 += cx; if (x2 < 0) x2 = 0; else if (x2 >= (int32_t)CV_W) x2 = CV_W - 1;
            y2 += cy; if (y2 < 0) y2 = 0; else if (y2 >= (int32_t)CV_H) y2 = CV_H - 1;

            if (smooth[y1 * (int32_t)CV_W + x1] < smooth[y2 * (int32_t)CV_W + x2])
            {
                d[bit >> 5] |= (uint32_t)1U << (bit & 31U);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Matching                                                            */
/* ------------------------------------------------------------------ */

uint16_t cv_desc_distance(const uint32_t *a, const uint32_t *b)
{
    uint16_t d = 0U;
    uint32_t i;
    for (i = 0U; i < CV_BRIEF_WORDS; i++)
    {
        d += (uint16_t)__builtin_popcount(a[i] ^ b[i]);
    }
    return d;
}

uint16_t cv_match_bf(const uint32_t *desc_a, uint16_t na,
                     const uint32_t *desc_b, uint16_t nb,
                     uint16_t *matches_a, uint16_t *matches_b)
{
    uint16_t count = 0U;
    uint16_t i, j;
    for (i = 0U; i < na; i++)
    {
        uint16_t best = 0xFFFFU;
        uint16_t second = 0xFFFFU;
        uint16_t best_j = 0U;
        for (j = 0U; j < nb; j++)
        {
            uint16_t d = cv_desc_distance(&desc_a[i * CV_BRIEF_WORDS],
                                          &desc_b[j * CV_BRIEF_WORDS]);
            if (d < best)
            {
                second = best;
                best = d;
                best_j = j;
            }
            else if (d < second)
            {
                second = d;
            }
        }
        /* Lowe's ratio test: best must be clearly better than second-best */
        if (best < 64U && (uint32_t)best * 10U < (uint32_t)second * 7U)
        {
            matches_a[count] = i;
            matches_b[count] = best_j;
            count++;
        }
    }
    return count;
}

void cv_shift_gray(const uint8_t *src, uint8_t *dst, int16_t dx, int16_t dy)
{
    int32_t x, y;
    for (y = 0; y < (int32_t)CV_H; y++)
    {
        for (x = 0; x < (int32_t)CV_W; x++)
        {
            int32_t sx = x - dx;
            int32_t sy = y - dy;
            if (sx >= 0 && sx < (int32_t)CV_W && sy >= 0 && sy < (int32_t)CV_H)
            {
                dst[y * (int32_t)CV_W + x] = src[sy * (int32_t)CV_W + sx];
            }
            else
            {
                dst[y * (int32_t)CV_W + x] = 0U;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Overlay drawing (RGB565 bytes, MSB-first)                           */
/* ------------------------------------------------------------------ */

static void put_pixel(uint8_t *rgb565, int32_t x, int32_t y, uint16_t color)
{
    if (x < 0 || y < 0 || x >= (int32_t)CV_W || y >= (int32_t)CV_H)
    {
        return;
    }
    rgb565[(y * (int32_t)CV_W + x) * 2] = (uint8_t)(color >> 8);
    rgb565[(y * (int32_t)CV_W + x) * 2 + 1] = (uint8_t)(color & 0xFF);
}

void cv_draw_cross(uint8_t *rgb565, uint16_t x, uint16_t y,
                   uint16_t color, uint8_t r)
{
    uint8_t i;
    for (i = 0U; i <= r; i++)
    {
        put_pixel(rgb565, (int32_t)x + i, (int32_t)y, color);
        put_pixel(rgb565, (int32_t)x - i, (int32_t)y, color);
        put_pixel(rgb565, (int32_t)x, (int32_t)y + i, color);
        put_pixel(rgb565, (int32_t)x, (int32_t)y - i, color);
    }
}

void cv_draw_line(uint8_t *rgb565, uint16_t x0, uint16_t y0,
                  uint16_t x1, uint16_t y1, uint16_t color)
{
    int32_t dx = (int32_t)x1 - (int32_t)x0;
    int32_t dy = (int32_t)y1 - (int32_t)y0;
    int32_t steps = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy)
                        ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
    int32_t i;
    if (steps == 0)
    {
        put_pixel(rgb565, x0, y0, color);
        return;
    }
    for (i = 0; i <= steps; i++)
    {
        put_pixel(rgb565, (int32_t)x0 + dx * i / steps,
                  (int32_t)y0 + dy * i / steps, color);
    }
}
