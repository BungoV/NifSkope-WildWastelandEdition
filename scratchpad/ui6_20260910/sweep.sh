#!/bin/bash
# Lane UI6 -- G7: the exe is newer than EVERY changed path, and every object
# that includes a header this lane touched is newer than that header.
cd /e/Projects/NifskopeWildWastelandEdition || exit 2
EXE=release/NifSkope.exe
echo "exe: $(ls -l --time-style=full-iso $EXE)"
n=0; stale=0
for f in $(git status --porcelain -- src res tools tests | awk '{print $NF}'); do
	[ -f "$f" ] || continue
	n=$((n+1))
	if [ "$EXE" -nt "$f" ]; then :; else echo "STALE $f"; stale=$((stale+1)); fi
done
echo "EXE-NEWER: $((n-stale)) of $n changed paths; STALE=$stale"

echo "--- objects vs the headers this lane changed ---"
for H in src/nifskope.h src/wwskin.h src/animworkspace.h src/hkxplayback.h src/hkxanimui.h src/ui/widgets/timeline.h; do
	tot=0; bad=0
	for f in $(grep -rl "#include \"$(basename $H)\"" src/ 2>/dev/null; grep -rl "#include \"ui/widgets/$(basename $H)\"" src/ 2>/dev/null); do
		case "$f" in *.cpp) ;; *) continue ;; esac
		o=GeneratedFiles/.obj/$(basename "$f" .cpp).o
		[ -f "$o" ] || continue
		tot=$((tot+1))
		[ "$o" -nt "$H" ] || { echo "  STALE $o vs $H"; bad=$((bad+1)); }
	done
	echo "  $H: $((tot-bad)) of $tot objects newer; STALE=$bad"
done

echo "--- the stylesheet ---"
cmp res/style.qss release/style.qss && echo "res/style.qss == release/style.qss"

echo "--- the rollback rung ---"
ls -l --time-style=full-iso release/NifSkope.before_ui6.exe
