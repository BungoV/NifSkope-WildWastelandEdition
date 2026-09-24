#!/bin/bash
#
# HIS MO2 LOAD ORDER, READ OFF DISK (lane LOADORDER1, 2026-09-24).
#
# `lodgen --mo2-profile <profile> [--mo2-mods <mods>]` builds the plugin list
# and the resource stack from modlist.txt + plugins.txt with Mod Organizer NOT
# running. Everything here reads his MO2 folders READ ONLY; the only files
# written are copies of modlist.txt / plugins.txt and one tiny bake, both under
# $OUT (default a mktemp dir).
#
#   G1 --print-source: Fallout4.esm first, the DLC masters next, all 30 enabled
#      plugins as existing full paths, the whole list equal to an independent
#      re-derivation (lodgen_loadorder_check.py), the stack = Data, enabled
#      mods bottom-up, overwrite.            RED: the rung's --plugins-txt run.
#   G2 --probe a BNS LOD model names `BNS Trees - Main.ba2`, a True Grass mesh
#      names `TrueGrass - Main.ba2`.         RED: the rung's --mo2 --plugins-txt
#      from a shell (no usvfs) finds neither.
#   G3 a mesh two enabled mods both ship LOOSE resolves to the higher one, and
#      swapping the two lines in a COPY of modlist.txt flips the answer.
#   G4 no disabled mod is in the stack (G1's checker); a file only a disabled
#      mod ships is not found, and IS found once that mod is named with
#      --resource (the control: the path is real); a file a disabled and an
#      enabled mod both ship resolves to the enabled one.
#   G5 a one-chunk Sanctuary bake (--native into $OUT) from his LIVE profile
#      as-is (every ticked plugin, 47 on 2026-09-24) loads, and its .lodb lists
#      the whole load order path for path, BNS Trees.esp, TrueGrass.esp and
#      TestWorldspace.esp by full path (lane TOOLFIX1: ESMFIX1 made
#      TestWorldspace.esp load; G5 no longer unticks it).
#      RED: RUNG5 (default the pre-ESMFIX1 exe) through the same G5 fails.
#   Also: --plugins-txt on his profile now REFUSES by name and points at
#      --mo2-profile (the rung passed bare names and failed later).
#
# USAGE
#   bash tests/spells/lodgen_loadorder.sh
#   RUNG=<pre-lane exe> OUT=<dir> LEGS=12345 bash tests/spells/lodgen_loadorder.sh
#   RUNG5=<exe> overrides G5's red rung; RUNG5=none skips it.

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
RUNG="${RUNG:-}"
RUNG5="${RUNG5:-E:/Projects/NifskopeWWE-esmfix1/release/NifSkope.before_esmfix1.exe}"
MUST5="${MUST5:-TestWorldspace.esp}"
PROFILE="${PROFILE:-E:/Projects/Fallout 4 Mods/profiles/Default}"
MODS="${MODS:-E:/Projects/Fallout 4 Mods/mods}"
DATA="${DATA:-X:/Programs/Steam/steamapps/common/Fallout 4/Data}"
LEGS="${LEGS:-12345}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
CHK="$ROOT/tests/spells/lodgen_loadorder_check.py"
if [ -n "${OUT:-}" ]; then mkdir -p "$OUT"; W="$OUT"; else W="$(mktemp -d)"; fi
WA="$(cd "$W" && { pwd -W 2>/dev/null || pwd; })"

BNS="meshes/bns/lod/landscape/trees/aspen/treeaspen01_lod_0.nif"
TG="meshes/landscape/grass/tg_bushycane.nif"
LAMP="meshes/dlc03/setdressing/dlc03lightoillampon_hanging.nif"
MOD_HI="Ultra Exterior Lighting"
MOD_LO="Ultra Interior Lighting"
DIS_ONLY="materials/actors/msmechanic/msmechanicarmor.bgsm"
DIS_MOD="X03"
DIS_BOTH="meshes/actors/powerarmor/x01/x01_torso.nif"
DIS_BOTH_WIN="X01Tesla"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$PROFILE/modlist.txt" ] || { echo "no profile at $PROFILE"; exit 2; }
echo "exe   : $NS ($(stat -c%s "$NS") bytes, $(stat -c%y "$NS" | cut -c1-19))"
[ -n "$RUNG" ] && echo "rung  : $RUNG"
echo "work  : $WA"

fails=0; checks=0
ok() { echo "  ok   $1"; checks=$((checks + 1)); }
bad() { echo "  FAIL $1"; checks=$((checks + 1)); fails=$((fails + 1)); }
# tally <checker output file>: echo its lines, count its ok/FAIL
tally() {
	sed 's/^/  /' "$1"
	checks=$((checks + $(grep -c -E '^(ok|FAIL) ' "$1")))
	fails=$((fails + $(grep -c '^FAIL ' "$1")))
}
# red <checker output file> <what>: a red control PASSES when its checks FAIL
red() {
	local n; n="$(grep -c '^FAIL ' "$1")"
	sed 's/^/    red| /' "$1"
	if [ "$n" -gt 0 ]; then ok "RED $2: the rung fails $n check(s), as it must"; else bad "RED $2: the rung passed a check it cannot pass"; fi
}
# lg <args>: the exe's output, CR stripped; returns the EXE's exit code
lg() { "$NS" -no-gui lodgen "$@" > "$W/.lg.raw" 2>&1; local r=$?; tr -d '\r' < "$W/.lg.raw"; return $r; }

# ============================================================================
if [ -z "${LEGS##*1*}" ]; then
echo; echo "== G1 the plugin list and the stack =="
lg --mo2-profile "$PROFILE" --print-source > "$W/g1.txt"; echo "  rc=$?"
grep -E '^(mo2-[a-z-]+|plugins|source|resources):' "$W/g1.txt" | sed 's/^/  | /'
"$PY" "$CHK" g1 "$W/g1.txt" "$PROFILE" "$MODS" "$DATA" > "$W/g1.chk"; tally "$W/g1.chk"

# --plugins-txt keeps its masters, and refuses a plugin that is not in Data by name
lg --plugins-txt "$PROFILE/plugins.txt" --data-root "$DATA" --print-source > "$W/g1_ptxt.txt"; rc=$?
if [ "$rc" = "2" ] && grep -q -- "--plugins-txt refused: plugin '.*' is not in .*--mo2-profile" "$W/g1_ptxt.txt"; then
	ok "--plugins-txt on his profile refuses by name and points at --mo2-profile ($(grep -o "plugin '[^']*'" "$W/g1_ptxt.txt" | head -1))"
else
	bad "--plugins-txt on his profile: rc=$rc, $(head -c 200 "$W/g1_ptxt.txt")"
fi
if [ -n "$RUNG" ]; then
	"$RUNG" -no-gui lodgen --plugins-txt "$PROFILE/plugins.txt" --data-root "$DATA" --print-source 2>&1 | tr -d '\r' > "$W/g1_red.txt"
	echo "  rung plugin 0: $(grep '^plugin 0: ' "$W/g1_red.txt")"
	"$PY" "$CHK" g1 "$W/g1_red.txt" "$PROFILE" "$MODS" "$DATA" > "$W/g1_red.chk"; red "$W/g1_red.chk" "G1 (today's --plugins-txt)"
fi
fi

# ============================================================================
if [ -z "${LEGS##*2*}" ]; then
echo; echo "== G2 the tree and grass mods' archives =="
lg --mo2-profile "$PROFILE" --probe "$BNS" > "$W/g2_bns.txt"
lg --mo2-profile "$PROFILE" --probe "$TG" > "$W/g2_tg.txt"
{ "$PY" "$CHK" probe "$W/g2_bns.txt" path "BNS Trees - Main.ba2" "G2 $BNS comes from BNS Trees - Main.ba2"
  "$PY" "$CHK" probe "$W/g2_bns.txt" entry "Boston Natural Surroundings" "G2 ... through the Boston Natural Surroundings entry"
  "$PY" "$CHK" probe "$W/g2_tg.txt" path "TrueGrass - Main.ba2" "G2 $TG comes from TrueGrass - Main.ba2"
} > "$W/g2.chk"; tally "$W/g2.chk"
if [ -n "$RUNG" ]; then
	"$RUNG" -no-gui lodgen "$DATA/Fallout4.esm" --mo2 --plugins-txt "$PROFILE/plugins.txt" --data-root "$DATA" --probe "$BNS" 2>&1 | tr -d '\r' > "$W/g2_red.txt"
	"$PY" "$CHK" probe "$W/g2_red.txt" path "BNS Trees - Main.ba2" "G2 (rung, --mo2 from a shell)" > "$W/g2_red.chk"; red "$W/g2_red.chk" "G2 (today's --mo2 off usvfs)"
fi
fi

# ============================================================================
if [ -z "${LEGS##*3*}" ]; then
echo; echo "== G3 priority between two enabled mods, and the swap =="
mkdir -p "$W/prof_a" "$W/prof_b"
cp "$PROFILE/modlist.txt" "$PROFILE/plugins.txt" "$W/prof_a/"
cp "$PROFILE/plugins.txt" "$W/prof_b/"
"$PY" "$CHK" swap "$W/prof_a/modlist.txt" "$W/prof_b/modlist.txt" "$MOD_HI" "$MOD_LO"
for m in "$MOD_HI" "$MOD_LO"; do [ -f "$MODS/$m/$LAMP" ] && echo "  $m ships it loose ($(stat -c%s "$MODS/$m/$LAMP") bytes)"; done
lg --mo2-profile "$PROFILE" --probe "$LAMP" > "$W/g3_real.txt"
lg --mo2-profile "$WA/prof_a" --mo2-mods "$MODS" --data-root "$DATA" --probe "$LAMP" > "$W/g3_a.txt"
lg --mo2-profile "$WA/prof_b" --mo2-mods "$MODS" --data-root "$DATA" --probe "$LAMP" > "$W/g3_b.txt"
{ "$PY" "$CHK" probe "$W/g3_real.txt" entry "$MOD_HI" "G3 his order: the higher mod ($MOD_HI) wins"
  "$PY" "$CHK" probe "$W/g3_a.txt" entry "$MOD_HI" "G3 the unswapped copy agrees"
  "$PY" "$CHK" probe "$W/g3_b.txt" entry "$MOD_LO" "G3 the swapped copy flips it to $MOD_LO"
} > "$W/g3.chk"; tally "$W/g3.chk"
sa="$(grep '^sha1:' "$W/g3_a.txt")"; sb="$(grep '^sha1:' "$W/g3_b.txt")"
echo "  bytes: unswapped $sa / swapped $sb"
fi

# ============================================================================
if [ -z "${LEGS##*4*}" ]; then
echo; echo "== G4 a disabled mod never resolves =="
grep -q "^-$DIS_MOD\$" <(tr -d '\r' < "$PROFILE/modlist.txt") && echo "  $DIS_MOD is disabled in modlist.txt"
lg --mo2-profile "$PROFILE" --probe "$DIS_ONLY" > "$W/g4_off.txt"
lg --mo2-profile "$PROFILE" --resource "$MODS/$DIS_MOD" --probe "$DIS_ONLY" > "$W/g4_ctl.txt"
lg --mo2-profile "$PROFILE" --probe "$DIS_BOTH" > "$W/g4_both.txt"
{ "$PY" "$CHK" probe "$W/g4_off.txt" found "(no)" "G4 $DIS_ONLY (only in -$DIS_MOD) is not found"
  "$PY" "$CHK" probe "$W/g4_ctl.txt" entry "$DIS_MOD" "G4 control: named with --resource the same path IS found"
  "$PY" "$CHK" probe "$W/g4_both.txt" entry "$DIS_BOTH_WIN" "G4 $DIS_BOTH resolves to the enabled +$DIS_BOTH_WIN, never the disabled mod"
} > "$W/g4.chk"; tally "$W/g4.chk"
fi

# ============================================================================
if [ -z "${LEGS##*5*}" ]; then
echo; echo "== G5 a one-chunk Sanctuary bake from his load order =="
# His live profile AS-IS: every ticked plugin, TestWorldspace.esp included (its 483 records under top byte FF
# name the plugin itself since ESMFIX1). g5run <exe> <dir tag> writes the checker's lines to $W/<tag>.chk.
g5run() {
	local x="$1" t="$2" r rec recw
	rm -rf "$W/$t"; mkdir -p "$W/$t/stock" "$W/$t/tex"
	local t0; t0=$(date +%s)
	"$x" -no-gui lodgen --mo2-profile "$PROFILE" --worldspace 3C --terrain-region -20 24 -20 24 --dim 4 \
		--out-dir "$WA/$t/stock" --tex-dir "$WA/$t/tex" --native "$WA/$t" > "$W/$t.raw" 2>&1; r=$?
	tr -d '\r' < "$W/$t.raw" > "$W/$t.log"
	echo "  $t: bake rc=$r in $(( $(date +%s) - t0 )) s; $(grep -c '' "$W/$t.log") log lines; $(grep -i -m2 -E 'error|refused' "$W/$t.log" | cut -c1-200 | tr '\n' ' ')"
	rec="$(find "$W/$t" -name '*.lodb' | head -1)"
	{ if [ "$r" = "0" ] && [ -n "$rec" ]; then echo "ok   G5 the bake of his live profile ran (rc 0) and wrote $(basename "$rec")"
	  else echo "FAIL G5 the bake of his live profile: rc=$r, record '${rec}'"; fi
	  if [ -n "$rec" ]; then
		recw="$(cd "$(dirname "$rec")" && { pwd -W 2>/dev/null || pwd; })/$(basename "$rec")"
		"$x" -no-gui lodgen --bake-record "$recw" 2>&1 | tr -d '\r' > "$W/${t}_rec.txt"
		grep -iE "^bake-record plugin (0|[0-9]+ (BNS Trees|TrueGrass|${MUST5%.*}))" "$W/${t}_rec.txt" | cut -c1-200 | sed 's/^/  | /' >&2
		"$PY" "$CHK" record "$W/${t}_rec.txt" "$PROFILE" "$MODS" "$DATA" $MUST5
	  else
		echo "FAIL G5 no record, so $MUST5 is not in it"
	  fi
	} > "$W/$t.chk"
}
g5run "$NS" g5; tally "$W/g5.chk"
if [ "$RUNG5" != "none" ] && [ -x "$RUNG5" ]; then
	echo "  red rung: $RUNG5"
	g5run "$RUNG5" g5red; red "$W/g5red.chk" "G5 (the pre-ESMFIX1 exe on his live profile)"
elif [ "$RUNG5" != "none" ]; then
	bad "RED G5: no rung exe at $RUNG5 (RUNG5=none to skip)"
fi
fi

echo
echo "$checks checks, $fails failures"
[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ $fails -eq 0 ]
