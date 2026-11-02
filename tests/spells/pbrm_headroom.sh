#!/bin/bash
# lane PBRMCSMHEADROOM (SKYCLOUD1101, 11-01): the .pbrm cell programs read the PRT red's march once a fragment, offline
# through the real driver (no NifSkope run).   bash tests/spells/pbrm_headroom.sh
# 1 green: pbrm_cell / pbrm_cellcsm in WW_FRAME_UBO + WW_CELL_GI_MARCH + WW_CELL_PRT keep >= 25 % of the fragment
#          instruction limit (bar written before the run; main 4c9ea88e: pbrm_cellcsm 8.7 %).
# 2 red:   WW_CELL_GI_MARCH_COPIES (a march copy per call site, the old shader) must fall under the bar.
# 3 off:   every other mode (none, ubo, march without PRT; PRT none/ubo; RTDDGI none/ubo) gives the red's driver binary
#          but the run stamp (bytes <= 80) and the 8-byte source hash: the cut is preprocessor lines there.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
mkdir -p "$REPO/scratchpad"; T="$(mktemp -d "$REPO/scratchpad/pbrm_headroom.XXXXXX")"; trap 'rm -rf "$T"' EXIT
CHK="python -I $HERE/shader_link_check.py"; CNT="python -I $HERE/nv_instructions.py"
S="$REPO/res/shaders"; BAR=25
fail=0
M="--define WW_FRAME_UBO --define WW_CELL_GI_MARCH --define WW_CELL_PRT"
for p in pbrm_cell pbrm_cellcsm; do
	for arm in green red; do
		x=""; [ $arm = red ] && x="--define WW_CELL_GI_MARCH_COPIES"
		$CHK "$S" $p.prog $M $x --save-binary "$(cygpath -m "$T/$arm.bin" 2>/dev/null || echo "$T/$arm.bin")" < /dev/null > "$T/$arm.txt"
		l="$(sed -n 2p "$T/$arm.txt")"
		case "$l" in "LINK OK"*) c="$($CNT "$T/$arm.bin" < /dev/null)" ;; *) c="fp none headroom 0 (0.000 %)"; [ $arm = green ] && fail=1 ;; esac
		pc="$(echo "$c" | sed -E 's/.*\(([0-9.]+) %\).*/\1/')"
		ok=$(python -I -c "import sys; print(int(float(sys.argv[1]) >= float(sys.argv[2])))" "$pc" $BAR < /dev/null)
		echo "$arm  $p [prt:march] $c (bar >= $BAR %: $([ $ok = 1 ] && echo pass || echo under))"
		if [ $arm = green ]; then [ $ok = 1 ] || fail=1; else [ $ok = 0 ] && echo "red FAILS the bar as designed" || { echo "RED DID NOT FAIL"; fail=1; }; fi
	done
done
for p in pbrm_cell pbrm_cellcsm; do
	for mode in "none:" "ubo:WW_FRAME_UBO" "march:WW_FRAME_UBO WW_CELL_GI_MARCH" "prtnone:WW_CELL_PRT" "prtubo:WW_FRAME_UBO WW_CELL_PRT" \
	            "rtxnone:WW_CELL_RTDDGI" "rtxubo:WW_FRAME_UBO WW_CELL_RTDDGI"; do
		n="${mode%%:*}"; d=""; for k in ${mode#*:}; do d="$d --define $k"; done
		$CHK "$S" $p.prog $d --save-binary "$(cygpath -m "$T/g.bin")" < /dev/null > /dev/null
		$CHK "$S" $p.prog $d --define WW_CELL_GI_MARCH_COPIES --save-binary "$(cygpath -m "$T/r.bin")" < /dev/null > /dev/null
		s1=$(stat -c %s "$T/g.bin" 2>/dev/null || echo 0); s2=$(stat -c %s "$T/r.bin" 2>/dev/null || echo 0)
		o="$(cmp -l "$T/g.bin" "$T/r.bin" 2>/dev/null | awk '$1 > 80 { print $1 }' | tr '\n' ' ')"
		v=$(python -I -c "import sys; o=[int(x) for x in sys.argv[1:]]; print('ok' if not o or o[-1]-o[0] < 8 else 'DIFF %d bytes over %d' % (len(o), o[-1]-o[0]+1))" $o < /dev/null)
		[ "$s1" = "$s2" ] && [ "$s1" != 0 ] || v="SIZE $s1 vs $s2"
		echo "off   $p [$n] $v"
		[ "$v" = ok ] || fail=1
	done
done
[ $fail = 0 ] && echo "PASS pbrm headroom gate" || echo "FAIL pbrm headroom gate"
exit $fail
