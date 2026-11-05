#!/bin/bash
# lane FARTOGGLE2: the CPU twin of the GPU-driven step B occlusion cull (src/gl/cellgpucull.cpp kHiz0/kHizN/kCull)
# over 24 TAA-jittered frames of a steady camera, synthetic far scene. No exe, no window (python + numpy).
#   today's cull            must PASS (no visible object culled, the cull adds no frame-to-frame flip)
#   --feedback               must PASS (the occluder list fed back frame to frame: no loop with a conservative test)
#   --red minhiz             must FAIL (a nearest pyramid: the instrument sees a cull that hides visible objects)
# The "jitter blink" lines are the raw sub-pixel signal (no cull involved) the temporal AA has to hold together.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${OUT:-${TMPDIR:-$HERE/../../drop}/gpucull_twin}
mkdir -p "$OUT"
fail=0
arm() {	# arm <tag> <want PASS|FAIL> [args]
	local tag=$1 want=$2 got
	shift 2
	python -I "$HERE/twin.py" --frames 24 "$@" > "$OUT/$tag.txt" 2>&1 < /dev/null
	got=$(grep -oE "VERDICT (PASS|FAIL)" "$OUT/$tag.txt" | tail -1 | cut -d' ' -f2)
	if [ "$got" = "$want" ]; then echo "ok   $tag: $got (want $want)"; else echo "FAIL $tag: ${got:-none} (want $want)"; fail=1; fi
	grep -E "^gpucull twin" "$OUT/$tag.txt" | sed 's/^/     /'
}
arm today PASS
arm feedback PASS --feedback
arm red_minhiz FAIL --red minhiz
grep -E "^jitter blink" "$OUT/today.txt" | sed 's/^/     /'
[ $fail = 0 ] && echo "GPUCULL TWIN RESULT PASS" || echo "GPUCULL TWIN RESULT FAIL"
exit $fail
