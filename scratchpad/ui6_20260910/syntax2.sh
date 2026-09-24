#!/bin/bash
cd /e/Projects/NifskopeWildWastelandEdition || exit 2
bad=0
for f in src/wateruitest.cpp src/hkxanimuitest.cpp src/animworkspacetest.cpp; do
	echo "== $f"
	bash sx_UI6.sh "$f" > /tmp/ui6_sx2.txt 2>&1
	rc=$?
	grep -E "error:" /tmp/ui6_sx2.txt | head -10
	echo "RC=$rc"
	[ "$rc" -ne 0 ] && bad=$((bad+1))
done
echo "SYNTAX-BAD=$bad"
