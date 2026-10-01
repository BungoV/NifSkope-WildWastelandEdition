#!/bin/bash
#
# A VANILLA MATERIAL'S ALPHA TEST IN PBR MODE (lane PRTPGI, 2026-10-01; src/gl/renderer.cpp, the
# derived material's OpacityTexture bit).
#
# Without a .pbrm, PBR mode drew vanilla alpha-tested shapes solid (Vault 111's floor grates, the door
# decal). Judged by position, not by eye: the cell view's position probe (WW_CELL_LIT_PROBE=2) is shot
# top-down over the Vault111Cryo grates in Legacy and in PBR; where a fragment is cut, the surface
# below shows, so the share of pixels with the same position code is the share of matching coverage.
# PASS: >= 98.5% (measured 99.1%; the unfixed exe read 92.8%).
#
# RED CONTROL:  --red  (WW_R2A_RED=noopacity: the bit dropped again; must FAIL)
#
# USAGE  bash tests/spells/pbr_alpha_vanilla.sh [--red]

set -u
. "$(dirname "$0")/_harness.sh"
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE=pbr_alpha_vanilla
REGKEY="HKCU\Software\NifTools\NifSkope 2.0 $SCOPE"
RED=""; [ "${1:-}" = "--red" ] && RED=noopacity
OUT="$REPO/scratchpad/pbr_alpha_vanilla${RED:+_red}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
mkdir -p "$OUT"
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ "$REPO/src/gl/renderer.cpp" -nt "$EXE" ] && { echo "FAIL  src/gl/renderer.cpp is NEWER than the exe"; exit 1; }

shot() {   # shot <tag> <env...>
	local tag="$1"; shift
	reg delete "$REGKEY" //f > /dev/null 2>&1
	reg add "$REGKEY\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1
	reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1
	local gm; gm="$(mktemp)"
	python "$(dirname "$0")/settings_scope_game.py" "$SCOPE" "$(cygpath -w "$gm")" > /dev/null 2>&1 \
		&& reg import "$(cygpath -w "$gm")" > /dev/null 2>&1
	rm -f "$gm" "$OUT/$tag.png"
	env "$@" ${RED:+WW_R2A_RED=$RED} WW_RENDER_CENTER=-4600,-280,0 WW_RENDER_DIST=250 WW_RENDER_FOV=70 WW_RENDER_VIEW=1 \
		WW_CELL_OPEN="$ESM|interior|Vault111Cryo" WW_CELL_DATAROOT="$DATA" WW_CELL_LIT=1 WW_CELL_LIT_PROBE=2 \
		WW_RENDER_SHOT="$(winpath "$OUT/$tag.png")" WW_RENDER_SIZE=960x600 WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE=$SCOPE timeout 900 "$EXE" --port 14745 "$(winpath "$REPO/tests/fixtures/empty.wwcell")" \
		> "$OUT/$tag.notes" 2>&1
	reg delete "$REGKEY" //f > /dev/null 2>&1
}
shot legacy WW_PBRM_MODE=legacy
shot pbr WW_PBRM_MODE=pbr WW_PBRM_AUTOREPLACE=1
python - "$OUT" <<'PY'
import sys, numpy as np
from PIL import Image
d = sys.argv[1]
try:
    a, b = [np.asarray(Image.open('%s/%s.png' % (d, t)).convert('RGB')) for t in ('legacy', 'pbr')]
except OSError as e:
    print('FAIL  no picture: %s' % e); sys.exit(1)
lit = np.any(a != a[0, 0], axis=2)
share = np.all(a == b, axis=2)[lit].mean()
ok = lit.mean() > 0.5 and share >= 0.985
print('%s  same coverage on %.2f%% of %d cell pixels (Legacy vs PBR)' % ('PASS' if ok else 'FAIL', 100 * share, lit.sum()))
sys.exit(0 if ok else 1)
PY
