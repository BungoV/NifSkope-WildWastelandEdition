"""patch_nl_scope2.py [--check] -- second step on native_lighting.sh after patch_nl_scope.py:
seed every wiped scope with Settings/Version=1 so the window is NOT a first install
(src/ui/settingsdialog.cpp: a null Settings/Version saves every pane's widget values, among
them Background 46,46,46, which is not the viewport skin colour the check's BG constant
names), and rewrite the comment block to say what was measured. Exact-once anchors, LF only."""
import sys
P = r'E:\Projects\NifskopeWWE-gatefix2\tests\spells\native_lighting.sh'
check_only = '--check' in sys.argv
src = open(P, 'rb').read()
assert src.count(b'\r') == 0
t = src.decode('utf-8')

old_start = t.index('# Every window runs in its OWN settings scope')
old_end = t.index("seed_scope() {")
old_end = t.index('\n', old_end) + 1
BS = '\\'
new_block = (
'# Every window runs in its OWN settings scope (src/harnesswindow.cpp,\n'
'# WW_SETTINGS_SCOPE): the QSettings tree moves to\n'
'# HKCU' + BS + 'Software' + BS + 'NifTools' + BS + 'NifSkope 2.0 <scope>, wiped before EACH window (a\n'
'# window saves its layout on close, and the next would open at another size)\n'
'# and at exit. Lane GATEFIX2, 2026-09-25: gate (a) read 2 failures on every exe\n'
'# back to before_vt1 -- the very exe the four legacy baselines were measured on\n'
'# (2026-09-16 11:54:47, 22,288,896 B) -- because the gate rendered under\n'
'# bungo\'s own profile, and that profile now has "Vertex Color" unticked in the\n'
'# Lighting shading mode\'s Material Contributions (GLView/Display/Contributions/2\n'
'# = 0x00184b00; bit 0x80 alone moves the picture). The .BTR water shape then\n'
'# drew PURE WHITE instead of its dark vertex colour: legacy_btr_top 93,893\n'
'# pixels, mean luma 109.87 -> 132.89. A gate that reads the user\'s profile\n'
'# measures the profile. No baseline moved: under this scope all four legacy\n'
'# frames are byte-identical to the 2026-09-16 baselines again.\n'
'#\n'
'# The wiped scope is SEEDED with Settings/Version=1, i.e. "not a first install".\n'
'# On a first install (src/ui/settingsdialog.cpp, a null Settings/Version) the\n'
'# settings dialog saves every pane\'s widget value, among them Background\n'
'# 46,46,46 (src/ui/settingspane.cpp), which is not the viewport skin colour\n'
'# native_lighting_check.py\'s BG constant names; every coverage mask then counts\n'
'# the background as terrain and gates (b)(d)(e)(f) fail (measured: 21 checks,\n'
'# 7-8 failures in an empty scope, 0 with the seed).\n'
'SCOPE="${SCOPE:-nativelighting}"\n'
'REGKEY="HKCU' + BS * 2 + 'Software' + BS * 2 + 'NifTools' + BS * 2 + 'NifSkope 2.0 $SCOPE"\n'
'wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1; }\n'
'# SEED_REG (a .reg file whose keys are already under $REGKEY) is imported after\n'
'# the seed -- the red control\'s way of rendering under a chosen profile without\n'
'# touching the user\'s. Empty in a normal run.\n'
'seed_scope() {\n'
'\twipe_scope\n'
'\treg add "$REGKEY' + BS * 2 + 'Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1\n'
'\t[ -n "${SEED_REG:-}" ] && reg import "$(winpath "$SEED_REG")" > /dev/null 2>&1\n'
'\treturn 0\n'
'}\n'
)
assert t.count('# Every window runs in its OWN settings scope') == 1
t = t[:old_start] + new_block + t[old_end:]
out = t.encode('utf-8')
assert out.count(b'\r') == 0
print('ok, %d -> %d bytes' % (len(src), len(out)))
if not check_only:
    open(P, 'wb').write(out)
