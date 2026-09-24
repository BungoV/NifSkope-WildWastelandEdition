#!/bin/bash
# AUDIT1 step 2: ONE end-to-end bake, measured.
#   usage: bash bake_one.sh <outdir> <label> <cellX0> <cellY0> <cellX1> <cellY1> [extra switches...]
# The FO4CS target (--native <outdir>) unless the first extra switch is --stock.
# Writes <outdir>/<label>/ with the bake, <outdir>/<label>.log, and prints a
# measured summary: wall seconds, peak working set (PowerShell sampler, and the
# exe's own census clause beside it), the census lines verbatim, the stage line.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
OUT="$1"; LABEL="$2"; X0="$3"; Y0="$4"; X1="$5"; Y1="$6"; shift 6

# --- the game check, its own step, before this bake -------------------------
if tasklist | grep -qi "Fallout4"; then echo "GAME UP before $LABEL -- ABORT"; exit 90; fi
if tasklist | grep -qi "NifSkope"; then echo "NIFSKOPE RUNNING before $LABEL -- ABORT"; exit 91; fi

STOCK=0
if [ "${1:-}" = "--stock" ]; then STOCK=1; shift; fi
D="$OUT/$LABEL"
# KEEPDIR=1 leaves whatever is already in the folder: that is how the null
# --incremental run is made to look at THIS lane's own fresh bake.
[ "${KEEPDIR:-0}" = "1" ] || rm -rf "$D"
mkdir -p "$D" "$D/tex"
LOG="$OUT/$LABEL.log"
NAT=""
[ "$STOCK" = "0" ] && NAT="--native $(cygpath -m "$D" 2>/dev/null || echo "$D")"

STOP="$OUT/.stop_$LABEL"; PEAKF="$OUT/.peak_$LABEL"
rm -f "$STOP" "$PEAKF"
powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$ROOT/scratchpad/audit1_20260916/peak_sampler.ps1")" \
	-OutFile "$(cygpath -w "$PEAKF")" -StopFile "$(cygpath -w "$STOP")" >/dev/null 2>&1 &
SAMPLER=$!

T0=$(date +%s)
echo "### $LABEL start $(date '+%F %T')  region $X0 $Y0 $X1 $Y1  stock=$STOCK  extra: $*" > "$LOG"
# shellcheck disable=SC2086
"$NS" -no-gui lodgen "$ESM" --worldspace 3C \
	--terrain-region "$X0" "$Y0" "$X1" "$Y1" --dim 4 --data-root "$DATA" \
	--out-dir "$(cygpath -m "$D")" --tex-dir "$(cygpath -m "$D/tex")" $NAT \
	"$@" >> "$LOG" 2>&1
RC=$?
T1=$(date +%s)
touch "$STOP"; wait $SAMPLER 2>/dev/null
PEAK="$(tr -d '\r' < "$PEAKF" 2>/dev/null)"
echo "### $LABEL rc=$RC wall=$((T1-T0))s peak_sampler='$PEAK' $(date '+%F %T')" >> "$LOG"
echo "$LABEL rc=$RC wall=$((T1-T0))s peak='$PEAK'"
grep -E "^bake census: |^stage times: |^native: |^native cache: |^incremental: |^native-library-build: |^bake-record: |^layout " "$LOG"
exit $RC
