#!/bin/bash
# lane VOLAIR1: the .recx census over the whole base game + official add-ons. One shot with every master loaded and
# the files NifSkope ships (no WW_RECX_ROOT pin); the census lines say, per weather record, which file and rung
# matched. Verdict: every weather the masters define is matched by its own Plugins/FO4CS/Weathers file at the form
# rung (expected 107: tools/recx_weathers.py --table).
set -u
. "$(dirname "$0")/_harness.sh"
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
GD="${GD:-X:/Programs/Steam/steamapps/common/Fallout 4/Data}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
OUT="${OUT:-$REPO/scratchpad/volair1_20261014/census}"
EXPECT="${EXPECT:-107}"
mkdir -p "$OUT"
PL=""
for m in Fallout4 DLCRobot DLCworkshop01 DLCCoast DLCworkshop02 DLCworkshop03 DLCNukaWorld; do PL="$PL${PL:+,}$GD/$m.esm"; done
SCOPE=volair1_census
REGKEY="HKCU\Software\NifTools\NifSkope 2.0 $SCOPE"
reg delete "$REGKEY" //f > /dev/null 2>&1
trap 'reg delete "$REGKEY" //f > /dev/null 2>&1' EXIT
rm -f "$OUT/census.png" "$OUT/census.notes"
env -u WW_RECX_ROOT WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthFoggy WW_LOOKDEV_HOUR=14 WW_LOOKDEV_PLUGINS="$PL" \
	WW_LOOKDEV_FOG=1 WW_LOOKDEV_SKY=0 WW_LOOKDEV_SUN=0 WW_LOOKDEV_CLOUDS=0 WW_LOOKDEV_MOON=0 WW_LOOKDEV_GROUND=0 \
	WW_CELL_LIT=1 WW_CELL_GI=0 WW_CELL_IS=0 WW_CELL_FX_RED=hide WW_CELL_DECAL_RED=none \
	WW_CELL_OPEN="$GD/Fallout4.esm|Commonwealth|-19,22|1" WW_CELL_DATAROOT="$DATA" WW_LODGEN_RESOURCES="$DATA" \
	WW_CELL_AIR=1 WW_CELL_AIR_TERMS=1 \
	WW_RENDER_VIEW=5 WW_RENDER_CENTER=-75776,92160,8400 WW_RENDER_DIST=20 WW_RENDER_FOV=70 \
	WW_RENDER_SHOT="$(winpath "$OUT/census.png")" WW_RENDER_SIZE=640x400 WW_RENDER_CLEAN=1 WW_SETTINGS_SCOPE="$SCOPE" \
	timeout 1500 "$EXE" --port 14934 "$(winpath "$REPO/tests/fixtures/empty.wwcell")" > "$OUT/census.notes" 2>&1
grep "^recx census: " "$OUT/census.notes" > "$OUT/census.txt"
python - "$OUT/census.txt" "$EXPECT" <<'PY'
import sys, re
lines = open(sys.argv[1], encoding='utf-8', errors='replace').read().splitlines()
exp = int(sys.argv[2])
rows = [l for l in lines if ' -> ' in l]
total = [l for l in lines if re.search(r'\d+ weathers, \d+ matched', l)]
own = 0
bad = []
for l in rows:
    m = re.match(r'recx census: (\S*) (\S+)\|0x([0-9A-F]{8}) -> (\S+)', l)
    if not m:
        bad.append(l); continue
    edid, plug, lid, hit = m.groups()
    name = edid if edid else lid
    want = 'FO4CS/Weathers/%s.recx' % name
    if hit.startswith('form:') and hit.replace('\', '/').endswith(want):
        own += 1
    else:
        bad.append(l)
print('census: %d weather records listed, %d matched by their own FO4CS file at the form rung; summary: %s'
      % (len(rows), own, total[-1] if total else 'none'))
for l in bad[:20]:
    print('  NOT OWN: ' + l)
ok = len(rows) == exp and own == exp
print('VOLAIR1 CENSUS VERDICT %s (%d/%d, expected %d)' % ('PASS' if ok else 'FAIL', own, len(rows), exp))
sys.exit(0 if ok else 1)
PY
