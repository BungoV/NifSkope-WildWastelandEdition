#!/bin/bash
#
# THE BODY BUILD TRIANGLE -- the character creator's build slider, live in the
# viewer (lane GLTFEXPORT1, 2026-09-19).
#
# bungo, 2026-09-19 05:0x, verbatim: "Fallout 4 has 'skin' bones, those are not
# used for animations, but only for the character's build? ... could you build a
# hkx animation that basically is the build slider ... thin to muscular to fat
# to thin".
#
# TWO INSTRUMENTS, AND THEY ARE INDEPENDENT
#
#   PY   scratchpad/gltfexport1_20260919/body_build_table.py evaluates the
#        combination in Python from the RACE record's Bone Scale Data, and is
#        itself compared, bone for bone and axis for axis, against the EIGHT
#        FILES BETHESDA SHIPPED:
#        Meshes/Actors/Character/CharacterAssets/HumanRaceBoneScales{Male,
#        Female}{Thin,Muscular,Fat,Default}.txt, which is what
#        TESRace::ImportBodyMorphBoneBaseScales reads at CK time. That is the
#        game's own answer, written down by the game's own tools.
#   C++  the panel, driven inside a real NifSkope on a real character, dumping
#        its applied per-bone scale through WW_BODY_BUILD_DUMP.
#
# A bug in src/bodybuild.cpp cannot hide: the two arms share no code and PY is
# pinned to the shipped corpus.
#
# WHAT IS NOT PROVEN, and the gate says so rather than pretending: the SHAPE of
# the centroid term k BETWEEN the centre and a corner. The corpus pins k=0 at
# every corner and k=1 at the centre, and any curve through those fits it;
# k = 3*min(w) is a rival that fits them exactly as well. They separate at the
# midpoint of an edge, w = (0.5, 0, 0.5), where this build says k = 0.5 and the
# rival says k = 0. THE REFUTER, and it is cheap: one character built at a
# half-way slider, its Belly_skin scale read in game.
#
# ROWS
#   B1  PY vs the shipped corpus          0 controls failed, eight files
#   B2  the harness                       every in-application check passes
#   B3  C++ == PY                         all four states, every bone, 1e-4
#   B4  a belly vertex moves OUTWARD      measured through Shape::skinVertex
#   B5  RED CONTROL                       the FEMALE table on the male body must
#                                         DIFFER, and the gate names the bones
#   B6  the pictures                      thin / muscular / fat / centroid,
#                                         male and female, the BODY front and
#                                         side, plus one contact sheet
#
# ONE NIFSKOPE AT A TIME, --port below 49152, absolute Windows paths, and the
# harness window goes to the second monitor (WW_WINDOW_AT, _harness.sh).
#
# USAGE
#   bash tests/spells/body_build.sh
set -u
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
R=E:/Projects/NifskopeWildWastelandEdition
NS="${EXE:-$ROOT/release/NifSkope.exe}"
CA="${CA:-E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Actors/Character/CharacterAssets}"
MALE="${MALE:-$CA/MaleBody.nif}"
FEMALE="${FEMALE:-$CA/FemaleBody.nif}"
TABLE="${TABLE:-E:/Projects/Claude/tools/body_build_anim/race_bone_data.json}"
PY="$ROOT/scratchpad/gltfexport1_20260919/body_build_table.py"
OUT="${OUT:-$ROOT/scratchpad/gltfexport1_20260919/bodybuild}"
LOG="$ROOT/release/ww_body_build_test.log"
PORT="${PORT:-42319}"

fails=0
note () { echo; echo "=== $* ==="; }
ok ()   { echo "  PASS  $*"; }
bad ()  { echo "  FAIL  $*"; fails=$((fails + 1)); }

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 9; }
[ -f "$MALE" ] || { echo "no male body at $MALE -- this gate needs the unpacked corpus"; exit 9; }
[ -f "$TABLE" ] || { echo "no RACE dump at $TABLE"; exit 9; }
mkdir -p "$OUT"

# One run of the application. $1 log tag, $2 the NIF, $3 WW_BODY_BUILD, $4 shot
# (may be empty), $5 dump path (may be empty), $6 viewport-shot prefix (may be
# empty; the run writes <prefix>_front.png and <prefix>_side.png).
runone () {
	local tag="$1" nif="$2" state="$3" shot="$4" dump="$5" view="${6:-}"
	rm -f "$LOG"
	WW_BODY_BUILD="$state" \
	WW_BODY_BUILD_SHOT="$shot" \
	WW_BODY_BUILD_VIEW_SHOT="$view" \
	WW_BODY_BUILD_DUMP="$dump" \
		"$NS" --port "$PORT" "$(winpath "$nif")" > "$OUT/$tag.stdout" 2>&1 &
	local pid=$!
	local i
	for i in $(seq 1 120); do
		[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
		kill -0 "$pid" 2>/dev/null || break
		sleep 1
	done
	kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
	[ -f "$LOG" ] && cp "$LOG" "$OUT/$tag.log"
}

# --------------------------------------------------------------------------
note "B1 the Python arm against the EIGHT FILES BETHESDA SHIPPED"
python "$PY" "$TABLE" > "$OUT/py_table.txt" 2>&1
rc=$?
grep -E "CONTROL|REFUTED|controls failed|X values|FLOOR" "$OUT/py_table.txt" | sed 's/^/  /'
if [ $rc = 0 ]; then ok "B1 every control holds (see $OUT/py_table.txt)"
else bad "B1 the Python arm does not reproduce the shipped files"; fi
echo "  rows written: $(grep -c '^ROW ' "$OUT/py_table.txt")"

# --------------------------------------------------------------------------
note "B2/B3/B4 the panel, in a real NifSkope, on the male body"
runone male_centroid "$MALE" "HumanRace|male|0.3333,0.3333,0.3333" \
	"$(winpath "$OUT/male_centroid.png")" "$(winpath "$OUT/dump_male.txt")"
if [ -f "$OUT/male_centroid.log" ]; then
	grep -E "^(FAIL|SAY|[0-9]+ checks|PASS)" "$OUT/male_centroid.log" | sed 's/^/  /' | head -60
	if grep -q '^PASS$' "$OUT/male_centroid.log"; then ok "B2 the harness passes"
	else bad "B2 the harness reports failures"; fi
	grep -E "vertex move thin->fat|worst-moving vertex" "$OUT/male_centroid.log" | sed 's/^/  /'
	if grep -q "it moved OUTWARD" "$OUT/male_centroid.log" \
		&& ! grep -q "FAIL.*moved OUTWARD" "$OUT/male_centroid.log"; then
		ok "B4 a belly vertex moves outward, thin -> fat, within the bone's own bracket"
	else bad "B4 the outward-movement row did not pass"; fi
else
	bad "B2 the harness wrote no log (did the app exit before it ran, or is port $PORT bound?)"
fi

note "B3 the panel's numbers against Python's, bone for bone"
if [ -f "$OUT/dump_male.txt" ]; then
	python - "$OUT/py_table.txt" "$OUT/dump_male.txt" male <<'PYEOF'
import sys
def load(p, gender):
    out = {}
    for line in open(p, encoding="utf-8", errors="replace"):
        if not line.startswith("ROW "):
            continue
        f = line.split()
        if len(f) != 7 or f[1] != gender:
            continue
        out[(f[2], f[3])] = tuple(float(x) for x in f[4:7])
    return out
want, got = load(sys.argv[1], sys.argv[3]), load(sys.argv[2], sys.argv[3])
shared = sorted(set(want) & set(got))
print("  python rows %d, panel rows %d, compared %d" % (len(want), len(got), len(shared)))
if not shared:
    print("  FAIL  B3 the two tables share no row at all"); sys.exit(1)
worst, wk, off = 0.0, None, 0
for k in shared:
    for i in range(3):
        d = abs(want[k][i] - got[k][i])
        if d > worst: worst, wk = d, k
        if d > 1e-4: off += 1
print("  worst disagreement %.3g on %s, axes over 1e-4: %d" % (worst, wk, off))
# THE FLOOR: the same comparison against a DELIBERATELY WRONG table must fail,
# so "0 off" cannot come from comparing a thing with itself.
fwrong = sum(1 for k in shared
             if k[0] == "thin" and any(abs(want[(k[0], k[1])][i] - got[("fat", k[1])][i]) > 1e-4
                                       for i in range(3) if ("fat", k[1]) in got))
print("  FLOOR: comparing THIN against the panel's FAT row disagrees on %d bones" % fwrong)
bad = off > 0 or fwrong == 0
sys.exit(1 if bad else 0)
PYEOF
	if [ $? = 0 ]; then ok "B3 the panel is Python's answer, and the comparison can fail"
	else bad "B3 the panel and Python disagree (or the comparison cannot fail)"; fi
else
	bad "B3 no dump at $OUT/dump_male.txt"
fi

# --------------------------------------------------------------------------
note "B5 RED CONTROL: the FEMALE table on the male body must differ, by name"
runone male_female_table "$MALE" "HumanRace|female|0.3333,0.3333,0.3333" \
	"" "$(winpath "$OUT/dump_male_femaletable.txt")"
if [ -f "$OUT/dump_male_femaletable.txt" ] && [ -f "$OUT/dump_male.txt" ]; then
	python - "$OUT/dump_male.txt" "$OUT/dump_male_femaletable.txt" <<'PYEOF'
import sys
def load(p):
    out = {}
    for line in open(p, encoding="utf-8", errors="replace"):
        if line.startswith("ROW "):
            f = line.split()
            if len(f) == 7:
                out[(f[1], f[2], f[3])] = tuple(float(x) for x in f[4:7])
    return out
m, f = load(sys.argv[1]), load(sys.argv[2])
gm = {g for (g, _, _) in m}; gf = {g for (g, _, _) in f}
print("  the two runs are labelled %s and %s" % (sorted(gm), sorted(gf)))
if gm == gf:
    print("  FAIL  B5 both runs carry the same gender label; the switch did nothing"); sys.exit(1)
diff = []
for (g, st, bone), v in m.items():
    for (g2, st2, bone2), w in f.items():
        if st2 == st and bone2 == bone:
            d = max(abs(v[i] - w[i]) for i in range(3))
            if d > 1e-3 and st == "fat":
                diff.append((d, bone))
diff.sort(reverse=True)
print("  bones where the female table gives a different FAT scale: %d" % len(diff))
for d, b in diff[:10]:
    print("    %-26s differs by %.4f" % (b, d))
if len(diff) < 5:
    print("  FAIL  B5 the two tables barely differ, so this control cannot catch a mix-up")
    sys.exit(1)
sys.exit(0)
PYEOF
	if [ $? = 0 ]; then ok "B5 the wrong table is visible, and the differing bones are named"
	else bad "B5 the red control does not discriminate"; fi
else
	bad "B5 one of the two dumps is missing"
fi

# --------------------------------------------------------------------------
note "B6 the pictures: thin / muscular / fat / centroid, male and female"
# The dock grab shows the NUMBERS; the two viewport grabs show the BODY they
# moved, front and side, which is what the brief asks to be able to look at.
shots=""; bodies=""; missing=""
for body in male female; do
	NIF="$MALE"; [ "$body" = female ] && NIF="$FEMALE"
	if [ ! -f "$NIF" ]; then echo "  (no $body body at $NIF)"; continue; fi
	for st in thin:1,0,0 muscular:0,1,0 fat:0,0,1 centroid:0.3333,0.3333,0.3333; do
		nm="${st%%:*}"; w="${st#*:}"
		png="$OUT/${body}_${nm}.png"
		vf="$OUT/${body}_${nm}_front.png"; vs="$OUT/${body}_${nm}_side.png"
		if [ ! -f "$png" ] || [ ! -f "$vf" ] || [ ! -f "$vs" ]; then
			runone "shot_${body}_${nm}" "$NIF" "HumanRace|$body|$w" \
				"$(winpath "$png")" "" "$(winpath "$OUT/${body}_${nm}")"
		fi
		[ -f "$png" ] && shots="$shots $png"
		for v in "$vf" "$vs"; do
			if [ -f "$v" ]; then bodies="$bodies $v"; else missing="$missing $(basename "$v")"; fi
		done
	done
done
n=$(echo $shots | wc -w); nb=$(echo $bodies | wc -w)
echo "  panel pictures: $n     body pictures (front+side): $nb"
[ -n "$missing" ] && echo "  missing:$missing"
if [ "$n" -ge 4 ]; then ok "B6 the panel was photographed at every state"
else bad "B6 only $n panel picture(s) were written"; fi
if [ "$nb" -ge 8 ]; then ok "B6 the BODY was photographed front and side at every state"
else bad "B6 only $nb body picture(s) were written (8 wanted)"; fi

# The contact sheet needs Pillow, and the MSYS2 UCRT64 python this gate runs
# under does not carry it. Look for one that does rather than silently skipping
# the row: a missing sheet is a missing deliverable, not a machine fact.
SHEETPY=""
for cand in python "$LOCALAPPDATA/Programs/Python/Python313/python.exe" \
		"$LOCALAPPDATA/Programs/Python/Python39/python.exe" \
		/c/Users/bungo/AppData/Local/Programs/Python/Python313/python.exe \
		/c/Users/bungo/AppData/Local/Programs/Python/Python39/python.exe; do
	command -v "$cand" >/dev/null 2>&1 || [ -x "$cand" ] || continue
	if "$cand" -c 'import PIL' >/dev/null 2>&1; then SHEETPY="$cand"; break; fi
done
if [ -z "$SHEETPY" ]; then
	bad "B6 no python with Pillow was found, so no contact sheet was written"
else
	echo "  contact sheet python: $SHEETPY ($("$SHEETPY" -c 'import PIL;print("Pillow "+PIL.__version__)'))"
	# winpath prints without a trailing newline, so the list is built with an
	# explicit separator -- concatenating them made ONE 2000-character path.
	tiles=""
	for p in $bodies $shots; do tiles="$tiles $(winpath "$p")"; done
	"$SHEETPY" - "$(winpath "$OUT/contact_sheet.png")" $tiles <<'PYEOF'
import os, sys
from PIL import Image, ImageDraw
out, paths = sys.argv[1], sys.argv[2:]
if not paths:
    print("  (nothing to composite)"); sys.exit(1)
ims = [(os.path.splitext(os.path.basename(p))[0], Image.open(p).convert("RGB")) for p in paths]
TW, TH, PAD, CAP = 320, 420, 6, 16
cols = 8
rows = (len(ims) + cols - 1) // cols
sheet = Image.new("RGB", (cols * (TW + PAD) + PAD, rows * (TH + CAP + PAD) + PAD), (24, 24, 26))
d = ImageDraw.Draw(sheet)
for k, (nm, im) in enumerate(ims):
    im = im.copy(); im.thumbnail((TW, TH))
    x = PAD + (k % cols) * (TW + PAD); y = PAD + (k // cols) * (TH + CAP + PAD)
    sheet.paste(im, (x + (TW - im.width) // 2, y + (TH - im.height) // 2))
    d.text((x + 2, y + TH + 2), nm, fill=(210, 210, 214))
sheet.save(out)
print("  contact sheet: %s (%dx%d, %d tiles)" % (out, sheet.width, sheet.height, len(ims)))
PYEOF
	if [ -f "$OUT/contact_sheet.png" ]; then ok "B6 one contact sheet"
	else bad "B6 the contact sheet was not written"; fi
fi

echo
echo "==== $fails row(s) not as registered ===="
exit $([ "$fails" = 0 ] && echo 0 || echo 1)
