#!/bin/bash
cd /e/Projects/NifskopeWildWastelandEdition || exit 2
echo "== src/wateruitest.cpp"
bash sx_UI6.sh src/wateruitest.cpp > /tmp/ui6_sx4.txt 2>&1
rc=$?
grep -E "error:" /tmp/ui6_sx4.txt | head -10
echo "RC=$rc"
