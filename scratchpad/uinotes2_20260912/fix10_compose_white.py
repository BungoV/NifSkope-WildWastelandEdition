# Ruling 07:3x: the before/after composer still captions the lit state "accent
# #f0a54a".  Re-caption it from a MEASUREMENT of the ON picture instead of a
# constant, so the picture and the report cannot disagree -- and print the ink
# each checked button actually shows, which is the measurement that caught the
# loop-button claim at 06:35.
import io, os, sys

P = "E:/Projects/NifskopeWWE_ui/scratchpad/uinotes2_20260912/compose_icons.py"

OLD_HEAD = '''BG = (48, 50, 54)
INK = (230, 232, 235)
'''
NEW_HEAD = '''BG = (48, 50, 54)
INK = (230, 232, 235)
# the QSS ":checked" plate a lit toggle sits on (wwBoxedButtonQss -> bgBtnDown)
BTN_DOWN = (53, 95, 134)
'''

OLD_FN = '''def stack(rows, out, title):
'''
NEW_FN = '''def lit_ink(im, tog):
    """The mean colour of what is DRAWN inside each checked button's box.

    The gate can pass on an icon the button never asks for (2026-09-12 06:35,
    the loop button), so the lit colour is read back off the picture: inside the
    toggle's own columns, every pixel far enough from the ":checked" plate is
    ink, and their mean is the colour the eye sees.
    """
    a = np.asarray(im.convert("RGB")).astype(int)
    out = []
    for x0, x1, _ in tog:
        sub = a[:, x0:x1 + 1]
        m = (np.abs(sub - np.array(BTN_DOWN)).sum(axis=2) > 60)
        out.append("#%02x%02x%02x" % tuple(int(round(v)) for v in sub[m].mean(axis=0)) if m.any() else "-")
    return out


def stack(rows, out, title):
'''

OLD_CAP = '''    rows = []
    for name, im, tog in (("BEFORE  a word and a speck", b, tb),
                          ("AFTER   OFF", ao, to),
                          ("AFTER   ON  (accent %s)" % "#f0a54a", an, tn)):
'''
NEW_CAP = '''    litOn = lit_ink(an, tn)
    litOff = lit_ink(ao, to)
    print("  lit ink read back off the picture: ON %s  (OFF, over the plain plate, %s)" % (litOn, litOff))

    rows = []
    for name, im, tog in (("BEFORE  a word and a speck", b, tb),
                          ("AFTER   OFF", ao, to),
                          ("AFTER   ON  (lit: measured ink %s -- textBright #f2f3f5 over the checked plate)"
                           % " and ".join(litOn), an, tn)):
'''

OLD_TITLE = '''        "bungo 2026-09-12 06:1x \\"Both icons\\" -- the transport row at 2:1, nearest-neighbour, same widget, same gate",
'''
NEW_TITLE = '''        "bungo 06:1x \\"Both icons\\" + 07:3x \\"keep them white\\" -- the transport row at 2:1, nearest-neighbour, same widget, same gate",
'''

with io.open(P, "rb") as f:
    raw = f.read()
if raw.count(b"\r"):
    sys.exit("REFUSED: %s carries CR bytes" % P)
s = raw.decode("utf-8")
for old, new in ((OLD_HEAD, NEW_HEAD), (OLD_FN, NEW_FN), (OLD_CAP, NEW_CAP), (OLD_TITLE, NEW_TITLE)):
    n = s.count(old)
    if n != 1:
        sys.exit("REFUSED: anchor found %d times: %r" % (n, old[:60]))
    s = s.replace(old, new, 1)
data = s.encode("utf-8")
assert b"\r" not in data
with io.open(P, "wb") as f:
    f.write(data)
print("compose_icons.py %d B" % len(data))
