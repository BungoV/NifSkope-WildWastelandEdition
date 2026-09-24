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
#   G5 a one-chunk Sanctuary bake (--native into $OUT) loads with the MO2 input
#      and its .lodb lists BNS Trees.esp and TrueGrass.esp by full path.
#      RED: the rung given the same load order through --plugins-txt.
#   Also: --plugins-txt on his profile now REFUSES by name and points at
#      --mo2-profile (the rung passed bare names and failed later).
#
# USAGE
#   bash tests/spells/lodgen_loadorder.sh
#   RUNG=<pre-lane exe> OUT=<dir> LEGS=12345 bash tests/spells/lodgen_loadorder.sh

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
RUNG="${RUNG:-}"
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
# His live load order first, as information: the ESM reader (libfo76utils) refuses a plugin whose raw form
# IDs sit above 0x0FFFFFFF (TestWorldspace.esp: 483 records under top byte FF on 2026-09-24); the error
# names the plugin. The gate bake then runs on a COPY of his profile with exactly those plugins unticked.
mkdir -p "$W/g5live/stock" "$W/g5live/tex" "$W/prof_g5"
lg --mo2-profile "$PROFILE" --worldspace 3C --terrain-region -20 24 -20 24 --dim 4 \
	--out-dir "$WA/g5live/stock" --tex-dir "$WA/g5live/tex" --native "$WA/g5live" > "$W/g5live.log"; lrc=$?
echo "  his live profile: bake rc=$lrc; $(grep -i -m1 -E 'error|refused' "$W/g5live.log" | cut -c1-200)"
lg --mo2-profile "$PROFILE" --print-source > "$W/g5_src.txt"
cp "$PROFILE/modlist.txt" "$W/prof_g5/"
"$PY" "$CHK" untick "$W/g5_src.txt" "$PROFILE/plugins.txt" "$W/prof_g5/plugins.txt"
mkdir -p "$W/g5/stock" "$W/g5/tex"
t0=$(date +%s)
lg --mo2-profile "$WA/prof_g5" --mo2-mods "$MODS" --data-root "$DATA" --worldspace 3C \
	--terrain-region -20 24 -20 24 --dim 4 \
	--out-dir "$WA/g5/stock" --tex-dir "$WA/g5/tex" --native "$WA/g5" > "$W/g5.log"; rc=$?
echo "  bake rc=$rc in $(( $(date +%s) - t0 )) s; $(grep -c '' "$W/g5.log") log lines; $(grep -i -m3 -E 'error|refused' "$W/g5.log" | head -3)"
REC="$(find "$W/g5" -name '*.lodb' | head -1)"
if [ "$rc" = "0" ] && [ -n "$REC" ]; then ok "G5 the bake ran (rc 0) and wrote $(basename "$REC")"; else bad "G5 the bake: rc=$rc, record '${REC}'"; fi
if [ -n "$REC" ]; then
	RECW="$(cd "$(dirname "$REC")" && { pwd -W 2>/dev/null || pwd; })/$(basename "$REC")"
	lg --bake-record "$RECW" > "$W/g5_rec.txt"
	grep -iE "^bake-record plugin (0|[0-9]+ (BNS Trees|TrueGrass))" "$W/g5_rec.txt" | cut -c1-200 | sed 's/^/  | /'
	"$PY" "$CHK" record "$W/g5_rec.txt" "$W/prof_g5" "$MODS" "$DATA" > "$W/g5.chk"; tally "$W/g5.chk"
fi
if [ -n "$RUNG" ]; then
	mkdir -p "$W/g5red/stock" "$W/g5red/tex"
	"$RUNG" -no-gui lodgen "$DATA/Fallout4.esm" --plugins-txt "$WA/prof_g5/plugins.txt" --data-root "$DATA" --worldspace 3C \
		--terrain-region -20 24 -20 24 --dim 4 --out-dir "$WA/g5red/stock" --tex-dir "$WA/g5red/tex" \
		--native "$WA/g5red" 2>&1 | tr -d '\r' > "$W/g5red.log"; rrc=${PIPESTATUS[0]}
	RREC="$(find "$W/g5red" -name '*.lodb' | head -1)"
	echo "  rung bake rc=$rrc, record '${RREC}', $(grep -i -m1 -E 'error' "$W/g5red.log" | cut -c1-160)"
	if [ "$rrc" != "0" ] || [ -z "$RREC" ]; then ok "RED G5: the rung cannot bake his load order from --plugins-txt (rc $rrc)"
	else bad "RED G5: the rung baked it (rc 0)"; fi
fi
fi

echo
echo "$checks checks, $fails failures"
[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ $fails -eq 0 ]
