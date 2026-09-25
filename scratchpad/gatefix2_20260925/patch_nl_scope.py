"""patch_nl_scope.py [--check] -- give every native_lighting.sh window its own wiped
WW_SETTINGS_SCOPE (the native_open.sh fix of GATEFIX1, 2026-09-24). Exact-once anchors, LF only."""
import sys
P = r'E:\Projects\NifskopeWWE-gatefix2\tests\spells\native_lighting.sh'
check_only = '--check' in sys.argv
src = open(P, 'rb').read()
assert src.count(b'\r') == 0
t = src.decode('utf-8')

edits = [
(
'LOG="$ROOT/release/ww_native_lighting.log"\n',
'LOG="$ROOT/release/ww_native_lighting.log"\n'
'\n'
'# Every window runs in its OWN settings scope (src/harnesswindow.cpp,\n'
'# WW_SETTINGS_SCOPE): the QSettings tree moves to\n'
'# HKCU\\Software\\NifTools\\NifSkope 2.0 <scope>, wiped before EACH window (a\n'
'# window saves its layout on close, and the next would open at another size)\n'
'# and at exit. Lane GATEFIX2, 2026-09-25: gate (a) read 2 failures on every exe\n'
'# back to before_vt1 -- the very exe the four legacy baselines were measured on\n'
'# (2026-09-16 11:54:47, 22,288,896 B) -- because the gate rendered under\n'
'# bungo\'s own profile, and that profile now has "Vertex Color" unticked in the\n'
'# Lighting shading mode\'s Material Contributions (GLView/Display/Contributions/2\n'
'# = 0x00184b00; bit 0x80 alone moves the picture). The .BTR water shape then\n'
'# drew PURE WHITE instead of its dark vertex colour: legacy_btr_top 93,893\n'
'# pixels, mean luma 109.87 -> 132.89. A gate that reads the user\'s profile\n'
'# measures the profile. The baselines were re-pinned under the empty scope; the\n'
'# only change from the old ones is the viewport: 1024x991 under default toolbars\n'
'# against 1024x989 under his saved layout (same picture one row down; top view\n'
'# 325 pixels differ by <= 21 levels, the oblique <= 3 levels from the 2 px\n'
'# aspect change). The water is dark in both.\n'
'SCOPE="${SCOPE:-nativelighting}"\n'
'REGKEY="HKCU\\\\Software\\\\NifTools\\\\NifSkope 2.0 $SCOPE"\n'
'wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1; }\n'
'# SEED_REG (a .reg file whose keys are already under $REGKEY) is imported after\n'
'# each wipe -- the red control\'s way of rendering under a chosen profile\n'
'# without touching the user\'s. Empty in a normal run.\n'
'seed_scope() { wipe_scope; [ -n "${SEED_REG:-}" ] && reg import "$(winpath "$SEED_REG")" > /dev/null 2>&1; return 0; }\n',
),
(
'mkdir -p "$OUT"\nrm -f "$OUT"/*.png "$OUT"/*.census.txt\n',
'mkdir -p "$OUT"\nrm -f "$OUT"/*.png "$OUT"/*.census.txt\nwipe_scope\ntrap wipe_scope EXIT\n',
),
(
'\tlocal name="$1" file="$2" view="$3" ctr="$4"; shift 4\n\tenv "$@" \\\n',
'\tlocal name="$1" file="$2" view="$3" ctr="$4"; shift 4\n\tseed_scope\n\tenv "$@" WW_SETTINGS_SCOPE="$SCOPE" \\\n',
),
]
for a, b in edits:
    n = t.count(a)
    assert n == 1, (n, a[:60])
    t = t.replace(a, b)
# winpath must be defined before seed_scope is CALLED (it is: shot() runs after the
# fallback definition); assert the order anyway
assert t.index('winpath() {') < t.index('terrain_arm own')
out = t.encode('utf-8')
assert out.count(b'\r') == 0
print('ok, %d -> %d bytes' % (len(src), len(out)))
if not check_only:
    open(P, 'wb').write(out)
