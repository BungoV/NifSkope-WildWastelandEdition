#!/bin/bash
# Syntax-only pass for lane NATIVEVIEW1, with the FLAGS THE BUILD USES read out
# of Makefile.Release rather than typed (nifskope-ww-build-verify). It proves
# the files compile; it proves nothing about linking or behaviour.
cd /e/Projects/NifskopeWildWastelandEdition || exit 2
DEFINES=$(sed -n 's/^DEFINES *= *//p' Makefile.Release)
CXXFLAGS=$(sed -n 's/^CXXFLAGS *= *//p' Makefile.Release | sed 's/\$(DEFINES)//')
INCPATH=$(sed -n 's/^INCPATH *= *//p' Makefile.Release)
rc=0
for f in "$@"; do
	echo "--- $f"
	g++ -fsyntax-only $CXXFLAGS $DEFINES $INCPATH "$f" 2>&1 | grep -v "sfinae-incomplete" | head -40
	s=${PIPESTATUS[0]}
	echo "SX-RC $f = $s"
	[ "$s" -eq 0 ] || rc=1
done
exit $rc
