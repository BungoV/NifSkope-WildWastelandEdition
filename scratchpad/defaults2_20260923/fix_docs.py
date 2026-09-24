"""DEFAULTS2 docs: the two new defaults, dated. Exact-once anchors, LF-only files."""
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'
CHECK = '--check' in sys.argv

EDITS = {
    'docs/LODGEN_TERRAIN_VT.md': [
        ("**`--blend-edges off|quadrant`** (default `off`), with **`--blend-margin`**\n",
         "**`--blend-edges off|quadrant`** (default **`quadrant`** since 2026-09-23 --\n"
         "bungo's ruling \"Yes, default on\", lane DEFAULTS2; it was `off` until then, and\n"
         "`--blend-edges off` is the exact way back, byte for byte; the panel row *Quadrant\n"
         "edges* defaults to Cross-faded), with **`--blend-margin`**\n"),
    ],
    'docs/LODGEN_IMPOSTOR_SPEC.md': [
        ("## The frame law: one resolution, two ladders\n\n"
         "The resolution chosen in the panel (or `TILE=` on the driver) is what the run's\n"
         "LARGEST base gets.",
         "## The frame law: one resolution, two ladders\n\n"
         "**The default is 8 x 8 frames at 256 px a frame -- a 2048-texel sheet for the\n"
         "largest base** (bungo 2026-09-23, \"8x8 at 2k\"; lane DEFAULTS2). Until then it was\n"
         "8 x 8 at 128 px, a 1024-texel sheet. The grid was already 8 everywhere; only the\n"
         "frame moved: the driver's `TILE` default, the panel's *Card resolution* default\n"
         "and the bake hook's fallback when `WW_IMPOSTOR_TILE` is unset are all 256 now.\n"
         "`TILE=128` (or the panel row) is the way back. The ladders below are unchanged.\n\n"
         "The resolution chosen in the panel (or `TILE=` on the driver) is what the run's\n"
         "LARGEST base gets."),
    ],
    'docs/LODGEN_CARD_SHEETS.md': [
        ("`WW_IMPOSTOR_OCT` / `WW_IMPOSTOR_TILE` in `src/nifskope_ui.cpp` driven by\n"
         "`tools/bake_impostor_cards.sh`;",
         "`WW_IMPOSTOR_OCT` / `WW_IMPOSTOR_TILE` in `src/nifskope_ui.cpp` driven by\n"
         "`tools/bake_impostor_cards.sh` (default 8 x 8 frames at 256 px = a 2048-texel\n"
         "sheet for the largest base since 2026-09-23, bungo's \"8x8 at 2k\"; was 128 px);"),
    ],
}

ok = True
out = {}
for rel, reps in EDITS.items():
    b = open(ROOT + rel, 'rb').read(); s = b.decode('utf-8')
    for old, new in reps:
        n = s.count(old)
        if n != 1:
            print('ANCHOR x%d in %s: %r' % (n, rel, old[:60])); ok = False; continue
        s = s.replace(old, new)
    nb = s.encode('utf-8')
    if nb.count(b'\r') != b.count(b'\r'):
        print('CR moved', rel); ok = False
    out[rel] = nb
if ok and not CHECK:
    for rel, nb in out.items():
        open(ROOT + rel, 'wb').write(nb)
print(('checked' if CHECK else 'patched'), len(out), 'files;', 'OK' if ok else 'REFUSED')
sys.exit(0 if ok else 1)
