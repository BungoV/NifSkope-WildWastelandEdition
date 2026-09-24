#!/bin/bash
#
# THE CARD LINK (lane CARDLINK1, 2026-09-24): a `--native --impostors --arrays`
# bake writes, for every base whose card set went into a card array, the
# `.lodo` row's cardLayer ((set << 11) | layer), the header's cardCount and
# cardCorpusHash (the PROPOSED R19 contract, docs/LODGEN_NATIVE_LODO_LODI.md
# 4.13), and FORCE_CARD on the `.lodi` instances that stand on their card.
#
# Sanctuary, 9 chunks (cells -20 24 .. -9 35, dim 4), the REAL tree card sets
# of those cells (a GUI bake: tools/bake_impostor_cards.sh with
# CANDIDATES=trees), `--impostors-from-level 0` so every tree stands on its
# card at the near ring. Checks, on the files (tests/spells/lodgen_cardlink.py):
#   G1  cardCount > 0 and == the tree bases of the table with a card set
#   G2  every cardLayer resolves to that base's own layer of its array .lodm
#   G3  cardCorpusHash != 0, == the contract recomputed from the array bytes,
#       and MOVES when one albedo byte of one linked set is changed
#   G4  --native-verify refuses a .lodo whose cardCount was edited by one
#       (headerCrc32 recomputed), and names cardCount; the unedited pair passes
#   ID  a bake WITHOUT --impostors is byte-identical to the rung exe's
#   RED the same card bake on the rung exe: G1/G2/G3/G4 must FAIL there
#
# USAGE
#   CARDS=<card set dir> RUNG=<rung exe> bash tests/spells/lodgen_cardlink.sh
#   W=<work dir> keeps the bakes (default: a temp dir, removed on exit)

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
RUNG="${RUNG:-$ROOT/release/before_cardlink1.exe}"
CARDS="${CARDS:-$ROOT/scratchpad/cardlink1_20260924/cards}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
HELP="$ROOT/tests/spells/lodgen_cardlink.py"
if [ -z "${W:-}" ]; then W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT; fi
mkdir -p "$W"
WA="$(cd "$W" && { pwd -W 2>/dev/null || pwd; })"
CA="$(cd "$CARDS" && { pwd -W 2>/dev/null || pwd; })"

for f in "$NS" "$RUNG"; do [ -x "$f" ] || { echo "no exe at $f"; exit 2; }; done
[ -f "$ESM" ] || { echo "no ESM at $ESM"; exit 2; }
n=$(ls "$CARDS"/*_oct_albedo.png 2>/dev/null | wc -l)
[ "$n" -gt 0 ] || { echo "no card sets in $CARDS"; exit 2; }
echo "exe  $(sha1sum "$NS" | cut -c1-12)  rung $(sha1sum "$RUNG" | cut -c1-12)  card sets $n"

fails=0
ok() { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fails=$((fails + 1)); }
pairdir () { if [ -d "$1/FO4CSLOD/Commonwealth" ]; then echo "$1/FO4CSLOD/Commonwealth"; else echo "$1"; fi; }

# bake <exe> <dir> [extra...]: --out-dir and --native the SAME mod folder, so
# the card arrays land in <pair>/Objects beside the pair
bake () {
	local exe="$1" d="$2"; shift 2
	rm -rf "$W/$d"; mkdir -p "$W/$d"
	"$exe" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -9 35 --dim 4 \
		--data-root "$DATA" --out-dir "$WA/$d" --native "$WA/$d" --arrays --slot-fallback "$@" \
		> "$W/$d.log" 2>&1
	local rc=$?
	echo "  bake $d rc=$rc ($(grep -m1 -o 'native-cards: [A-Z]*[^;]*' "$W/$d.log" | cut -c1-110))"
	return $rc
}

echo "== G1-G3 on the new exe"
bake "$NS" new --impostors "$CA" --impostors-from-level 0 || bad "the card bake ran"
P="$(pairdir "$WA/new")"
"$PY" "$HELP" fields "$P" "$CA" new | tee "$W/fields_new.txt"
fn=$(grep -c '^  FAIL' "$W/fields_new.txt"); on=$(grep -c '^  ok' "$W/fields_new.txt")
[ "$fn" -eq 0 ] && [ "$on" -ge 7 ] && ok "G1-G3a all hold ($on checks)" || bad "G1-G3a ($on ok, $fn FAIL)"

echo "== G3 the hash moves with one card byte"
rm -rf "$W/cards_flip"
"$PY" "$HELP" flip "$CA" "$WA/cards_flip" "$P"
FA="$(cd "$W/cards_flip" && { pwd -W 2>/dev/null || pwd; })"
bake "$NS" flip --impostors "$FA" --impostors-from-level 0 || bad "the flipped bake ran"
PF="$(pairdir "$WA/flip")"
h1=$("$PY" -c "import struct,glob;b=open(glob.glob(r'$P/*.lodo')[0],'rb').read();print('%016x'%struct.unpack_from('<Q',b,0x28))")
h2=$("$PY" -c "import struct,glob;b=open(glob.glob(r'$PF/*.lodo')[0],'rb').read();print('%016x'%struct.unpack_from('<Q',b,0x28))")
r2=$("$PY" "$HELP" hash "$PF")
[ "$h1" != "$h2" ] && [ "$h2" != 0000000000000000 ] && ok "G3 cardCorpusHash moved: $h1 -> $h2" || bad "G3 cardCorpusHash did not move: $h1 -> $h2"
[ "$h2" = "$r2" ] && ok "G3 the flipped pair's hash is the contract of its arrays ($r2)" || bad "G3 flipped hash $h2 vs contract $r2"

echo "== G4 --native-verify refuses a cardCount off by one"
verify () { "$1" -no-gui lodgen "$ESM" --worldspace 3C --native-verify "$2" "$3" > "$W/verify.log" 2>&1; echo $?; }
LO=$(ls "$P"/*.lodo); LI=$(ls "$P"/*.lodi)
rc=$(verify "$NS" "$LO" "$LI")
[ "$rc" = 0 ] && ok "the unedited pair verifies (rc 0)" || { bad "the unedited pair verifies (rc $rc)"; tail -3 "$W/verify.log"; }
"$PY" "$HELP" bump "$LO" "$WA/bumped.lodo"
rc=$(verify "$NS" "$WA/bumped.lodo" "$LI")
why=$(grep -m1 -i 'cardCount' "$W/verify.log" | cut -c1-160)
[ "$rc" != 0 ] && [ -n "$why" ] && ok "G4 refused (rc $rc): $why" || { bad "G4 not refused by cardCount (rc $rc)"; tail -3 "$W/verify.log"; }

echo "== ID a bake without --impostors, new exe vs rung"
bake "$NS" idn || bad "the plain bake ran (new)"; mv "$W/idn" "$W/idn_new"
bake "$RUNG" idn || bad "the plain bake ran (rung)"; mv "$W/idn" "$W/idn_rung"
nd=0; nf=0
while IFS= read -r f; do
	nf=$((nf + 1))
	cmp -s "$W/idn_new/$f" "$W/idn_rung/$f" || { nd=$((nd + 1)); echo "    differs: $f"; }
done < <(cd "$W/idn_new" && find . -type f ! -name '*.lodb' ! -name '*.log' | sort)
nr=$(cd "$W/idn_rung" && find . -type f ! -name '*.lodb' ! -name '*.log' | wc -l)
[ "$nf" -gt 0 ] && [ "$nd" -eq 0 ] && [ "$nf" -eq "$nr" ] && ok "ID $nf files byte-identical to the rung" || bad "ID $nd of $nf files differ ($nr on the rung)"

echo "== RED the same card bake on the rung exe"
bake "$RUNG" red --impostors "$CA" --impostors-from-level 0 || bad "the rung card bake ran"
PR="$(pairdir "$WA/red")"
"$PY" "$HELP" fields "$PR" "$CA" rung > "$W/fields_red.txt"
grep 'cards-summary' "$W/fields_red.txt"
for g in 'G1 cardCount > 0' 'G2 every' 'G3 cardCorpusHash != 0' 'FORCE_CARD'; do
	grep -q "^  FAIL $g" "$W/fields_red.txt" && ok "RED $g fails on the rung" || bad "RED $g did not fail on the rung"
done
"$PY" "$HELP" bump "$(ls "$PR"/*.lodo)" "$WA/bumped_red.lodo"
rc=$(verify "$RUNG" "$WA/bumped_red.lodo" "$(ls "$PR"/*.lodi)")
grep -qi 'cardCount' "$W/verify.log" && bad "RED the rung names cardCount (rc $rc)" || ok "RED G4: the rung does not refuse by cardCount (rc $rc: $(grep -m1 -i 'refus\|error' "$W/verify.log" | cut -c1-100))"

echo "RESULT $([ $fails -eq 0 ] && echo PASS || echo FAIL) ($fails failures)"
exit $fails
