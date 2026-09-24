#!/bin/bash
# Lane UI6 -- syntax+semantics pass over every translation unit this lane changed.
# Writes nothing, needs no build slot, does not touch release/NifSkope.exe.
cd /e/Projects/NifskopeWildWastelandEdition || exit 2
FILES="
src/ui/widgets/timeline.cpp
src/ui/widgets/timelineedit.cpp
src/ui/widgets/timelineviews.cpp
src/hkxplayback.cpp
src/hkxanimui.cpp
src/animworkspace.cpp
src/animworkspacetest.cpp
src/hkxanimuitest.cpp
src/wateruitest.cpp
src/wateruitest_lod.cpp
src/nifskope.cpp
src/nifskope_ui.cpp
"
bad=0
for f in $FILES; do
	echo "== $f"
	bash sx_UI6.sh "$f" > /tmp/ui6_sx.txt 2>&1
	rc=$?
	grep -E "error:" /tmp/ui6_sx.txt | head -12
	echo "RC=$rc"
	[ "$rc" -ne 0 ] && bad=$((bad+1))
done
echo "SYNTAX-BAD=$bad"
