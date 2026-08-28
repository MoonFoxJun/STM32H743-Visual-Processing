import re, sys

for f in ['Src/st7789.c', 'Src/sccb.c', 'Src/ov5640.c', 'Src/dcmi.c', 'Src/cv.c',
          'Src/main.c', 'Src/xyh_image.c',
          'Inc/st7789.h', 'Inc/sccb.h', 'Inc/ov5640.h', 'Inc/dcmi.h', 'Inc/cv.h',
          'Inc/xyh_image.h']:
    s = open(f, encoding='utf-8').read()
    # strip comments FIRST (they may contain quotes, e.g. "Lowe's"),
    # then strings and char literals
    s2 = re.sub(r'/\*.*?\*/', '', s, flags=re.S)
    s2 = re.sub(r'//[^\n]*', '', s2)
    s2 = re.sub(r'"(?:[^"\\]|\\.)*"', '""', s2)
    s2 = re.sub(r"'(?:[^'\\]|\\.)*'", "''", s2)
    ok = True
    for o, c in [('{', '}'), ('(', ')'), ('[', ']')]:
        cnt_o, cnt_c = s2.count(o), s2.count(c)
        st = 'OK' if cnt_o == cnt_c else 'MISMATCH'
        if cnt_o != cnt_c:
            ok = False
        print(f'{f}: {o}{c} {cnt_o}/{cnt_c} {st}')
    non_ascii = [ch for ch in s if ord(ch) > 127]
    if non_ascii:
        ok = False
        print(f'{f}: NON-ASCII: {set(non_ascii)}')
    print(f'{f}: {"PASS" if ok else "FAIL"}')
