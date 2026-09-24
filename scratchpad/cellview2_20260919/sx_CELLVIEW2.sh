#!/usr/bin/env bash
# Syntax-only pass on lane CELLVIEW2's new files, with the REAL flags read out
# of Makefile.Release (never a hand-typed subset). Throwaway: delete after use.
set -u
cd /e/Projects/NifskopeWildWastelandEdition || exit 1

# this tree is built with the MSYS2 UCRT64 toolchain; the Git-Bash PATH has no g++
export PATH="/c/msys64/ucrt64/bin:$PATH"
command -v g++ >/dev/null || { echo "no g++ on PATH"; exit 2; }

DEFS=$(grep -m1 '^DEFINES' Makefile.Release | sed 's/^DEFINES *= *//')
INCS=$(grep -m1 '^INCPATH' Makefile.Release | sed 's/^INCPATH *= *//')
CXXF=$(grep -m1 '^CXXFLAGS' Makefile.Release | sed 's/^CXXFLAGS *= *//; s/\$(DEFINES)//')

rc=0
for f in src/cellidentity.cpp src/cellground.cpp src/cellclick.cpp src/cellpanel.cpp src/cellpicktest.cpp; do
	printf '=== %s\n' "$f"
	# shellcheck disable=SC2086
	g++ -fsyntax-only $CXXF $DEFS $INCS "$f" || rc=1
done
# the headers on their own, so a missing include in a header is not masked
for h in src/cellidentity.h src/cellground.h src/cellclick.h src/cellpanel.h src/cellpicktest.h; do
	printf '=== %s (header alone)\n' "$h"
	# shellcheck disable=SC2086
	g++ -fsyntax-only -x c++ $CXXF $DEFS $INCS "$h" || rc=1
done
echo "rc=$rc"
exit $rc
