#!/bin/bash
# Lane SHOWCASE1 -- one headless render through the WW render hook.
#   bash shot.sh <out.png> <file> <res-root|-> <VIEW> <CENTER|-> <ORTHO|-> [VAR=VAL ...]
# Every path absolute: a relative WW_RENDER_SHOT writes nothing and says nothing.
# The camera census ($WW_CAMERA_CENSUS) is copied to <out>.cam -- that file, not
# the arguments, is what the report quotes (upp, look-at, projection).
set -u
L="E:/Projects/NifskopeWildWastelandEdition/scratchpad/showcase1_20260912"
EXE="$L/ns_run/NifSkope.exe"
OUT="$1"; FILE="$2"; RES="$3"; VIEW="$4"; CTR="$5"; ORT="$6"; shift 6
PORT="${PORT:-45973}"
CAM="$L/logs/camera_pin.log"

# Rule 6: the game must be down before any exe launch.
if tasklist 2>/dev/null | grep -qiE '^"?Fallout4\.exe'; then
	echo "REFUSED: Fallout4.exe is running"; exit 2
fi
# One GUI NifSkope harness instance at a time across the running lanes -- WAIT,
# never kill.  A `-no-gui` lodgen bake is not a GUI instance and does not hold the
# slot (the brief says so), so the wait looks at the COMMAND LINE, not at the
# process name: only a NifSkope.exe without -no-gui counts.
gui_busy () {
	wmic process where "name='NifSkope.exe'" get CommandLine 2>/dev/null 		| grep -i 'nifskope.exe' | grep -qv -- '-no-gui'
}
while gui_busy; do sleep 5; done

E=()
[ "$RES" != "-" ] && E+=( "WW_LODGEN_RESOURCES=$RES" )
[ "$CTR" != "-" ] && E+=( "WW_RENDER_CENTER=$CTR" )
[ "$ORT" != "-" ] && E+=( "WW_RENDER_ORTHO=$ORT" )

rm -f "$OUT" "$OUT.cam" "$CAM"
env "${E[@]}" \
	WW_RENDER_SHOT="$OUT" WW_RENDER_SIZE="${SIZE:-1500x1059}" WW_RENDER_VIEW="$VIEW" \
	WW_RENDER_CLEAN="${CLEAN:-1}" WW_WINDOW_AT=1960,40 WW_CAMERA_CENSUS="$CAM" \
	"$@" \
	timeout 300 "$EXE" --port "$PORT" "$FILE" >/dev/null 2>&1
rc=$?
[ -f "$CAM" ] && cp "$CAM" "$OUT.cam"
if [ -f "$OUT" ]; then
	printf '  %-34s rc=%s %8d B  %s\n' "$(basename "$OUT")" "$rc" "$(stat -c %s "$OUT")" \
		"$(python -c "from PIL import Image;im=Image.open(r'''$OUT''');print('%dx%d'%im.size)")"
	[ -f "$CAM" ] && sed -n '$p' "$CAM" | tr -s ' ' | cut -c1-200 | sed 's/^/      cam: /'
else
	printf '  %-34s rc=%s NO FILE\n' "$(basename "$OUT")" "$rc"
fi
