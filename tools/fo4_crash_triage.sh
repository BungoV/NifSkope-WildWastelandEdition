#!/bin/bash
#
# Read a Fallout 4 crash log and pull out the parts that matter to a collision
# test, then cross-reference them against a rebuilt-mesh manifest.
#
# WHY THIS EXISTS
#
# Compiled collision can only be proved in the engine, and the engine's answer
# arrives as a crash log. Addictol's logger ships Fallout4.pdb and msdia140.dll,
# so its stacks are SYMBOLISED — a Havok fault names Havok functions — and its
# "POSSIBLE RELEVANT OBJECTS" block names forms by plugin and editor ID. Both of
# those map back to a mesh, which is the whole game here.
#
# Logs live in %USERPROFILE%\Documents\My Games\Fallout4\F4SE as
# crash-YYYY-MM-DD-HH-MM-SS.log, twenty kept.
#
# USAGE
#   bash tools/fo4_crash_triage.sh [log] [manifest.tsv]
#
# With no arguments it takes the newest log and the Concord test manifest.

set -u
LOGDIR="${LOGDIR:-$USERPROFILE/Documents/My Games/Fallout4/F4SE}"
LOG="${1:-}"
MAN="${2:-E:/Projects/Fallout 4 Mods/mods/WW Concord Collision Test/collision_manifest.tsv}"

if [ -z "$LOG" ]; then
	LOG=$(ls -t "$LOGDIR"/crash-*.log 2>/dev/null | head -1)
fi
[ -n "$LOG" ] && [ -f "$LOG" ] || { echo "no crash log found in $LOGDIR"; exit 2; }

echo "=== $(basename "$LOG")"
sed -n '1,10p' "$LOG" | grep -E 'CRASH TIME|Fallout 4 v|Unhandled exception|Access Violation'

echo
echo "=== objects the logger could name"
awk '/^POSSIBLE RELEVANT OBJECTS:/{f=1; next} /^[A-Z].*:$/{f=0} f' "$LOG" | sed 's/^/  /' | head -25

echo
echo "=== collision and Havok frames in the stack"
if grep -qE 'bhk|hknp|Havok|Collision|Physics' "$LOG"; then
	grep -nE 'bhk[A-Za-z]|hknp[A-Za-z]|Havok[A-Za-z]|CollisionObject|PhysicsSystem' "$LOG" \
		| sed 's/^/  /' | cut -c1-200 | head -20
else
	echo "  none — the fault is not in collision code, which is itself an answer"
fi

echo
echo "=== forms and meshes named anywhere in the log"
# quoted editor IDs, and any .nif path the log mentions
grep -ohE '"[A-Za-z0-9_]{4,}"' "$LOG" | tr -d '"' | sort -u > /tmp/crash_ids.txt
grep -ohiE '[A-Za-z0-9_\\/-]+\.nif' "$LOG" | tr 'A-Z\\' 'a-z/' | sort -u > /tmp/crash_nifs.txt
echo "  $(wc -l < /tmp/crash_ids.txt) distinct quoted names, $(wc -l < /tmp/crash_nifs.txt) mesh paths"
if [ -s /tmp/crash_nifs.txt ] && [ -f "$MAN" ]; then
	echo
	echo "=== of those meshes, the ones THIS MOD replaced"
	hit=0
	while IFS= read -r n; do
		# the log names a full Windows path; the manifest holds the Data-relative
		# one, so match on the tail rather than the whole string
		tail_only="${n##*/meshes/}"
		row=$(grep -iF "$tail_only" "$MAN" | head -1)
		[ -n "$row" ] || continue
		hit=1
		printf '  %s\n' "$(printf '%s' "$row" | cut -f1,2,4,5 | tr '\t' ' ')"
	done < /tmp/crash_nifs.txt
	[ "$hit" = "1" ] || echo "  none of the meshes named in the log are ours"
fi
echo
echo "(manifest columns: status placements shapes path)"
