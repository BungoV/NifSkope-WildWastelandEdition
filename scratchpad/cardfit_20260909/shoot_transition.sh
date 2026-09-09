#!/bin/bash
# THE TRANSITION GATE, source half: render each tree's SOURCE model through the
# render hook with the camera pointed at its card's own centre (`card.center`,
# the offset from the object's pivot), orthographic, at two framings.
#
# One NifSkope at a time, second monitor, never the primary, game-down gate.
#   bash shoot_transition.sh <cards dir> <out dir> <id>:<nif> ...
set -u
REPO=E:/Projects/NifskopeWildWastelandEdition
NS=$REPO/release/NifSkope.exe
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
CARDS=${1:?cards dir}
OUT=${2:?out dir}
shift 2
export WW_WINDOW_AT=1960,40
mkdir -p "$OUT"

if tasklist | grep -qi Fallout4.exe; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi
if tasklist | grep -qi NifSkope.exe; then echo "REFUSED: a NifSkope is already running"; exit 2; fi

echo "{" > "$OUT/dists.json.tmp"
first=1
for spec in "$@"; do
	id="${spec%%:*}"; nif="${spec#*:}"
	read -r cx cy cz hh <<< "$(python - "$CARDS/${id}_oct.lodm" <<'PY'
import sys, json, struct
b = open(sys.argv[1], 'rb').read()
n = struct.unpack('<I', b[8:12])[0]
c = json.loads(b[12:12+n].decode('utf-8'))['card']
print(c['center'][0], c['center'][1], c['center'][2], c['half'][1])
PY
)"
	mid=$(python -c "print('%.4f' % (1.5 * $hh))")
	ring=$(python -c "print('%.4f' % (6.0 * $hh))")
	[ $first -eq 1 ] || echo "," >> "$OUT/dists.json.tmp"
	first=0
	printf '"%s": {"mid": %s, "ring": %s}' "$id" "$mid" "$ring" >> "$OUT/dists.json.tmp"
	for tag in mid ring; do
		d=$mid; [ "$tag" = ring ] && d=$ring
		echo "[$id $tag] centre $cx,$cy,$cz  ortho half-height $d"
		WW_RENDER_SHOT="$(cygpath -w "$OUT/${id}_src_${tag}.png" 2>/dev/null || echo "$OUT/${id}_src_${tag}.png")" \
		WW_RENDER_SIZE=900x900 WW_RENDER_VIEW=5 WW_RENDER_CLEAN=1 \
		WW_RENDER_CENTER="$cx,$cy,$cz" WW_RENDER_DIST="$d" WW_RENDER_TIME=1 \
			"$NS" "$DATA/meshes/$nif" --port 45918 >/dev/null 2>&1
		ls -l "$OUT/${id}_src_${tag}.png" 2>/dev/null | awk '{print "   ->", $5, "bytes"}' \
			|| echo "   -> NO PNG"
	done
done
echo "" >> "$OUT/dists.json.tmp"
echo "}" >> "$OUT/dists.json.tmp"
mv "$OUT/dists.json.tmp" "$OUT/dists.json"
cat "$OUT/dists.json"
