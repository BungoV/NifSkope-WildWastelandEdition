#!/bin/bash
# NIFPARSE1: the DIAGNOSTIC RELINK (nifskope-ww-crash-diagnose section 3).
#
# Makefile.Release carries LFLAGS = -Wl,-s, so every gdb frame inside the exe
# prints "?? ()". One relink puts the symbol table back. It changes NO source
# and NO behaviour -- only the file size (21 MB -> ~26 MB).
#
# THIS IS A LINK, therefore a BUILD: it may only run while this lane holds the
# build slot (scratchpad/nifparse1_20260911/BUILDING up, no other lane's
# BUILDING, Fallout4.exe down). It is COUNTED in the report.
#
# Afterwards relink normally so the shipped exe matches the project's flags.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
cd "$ROOT" || exit 1

# the stripped exe is kept so the normal relink can be skipped if nothing changed
cp -p release/NifSkope.exe scratchpad/nifparse1_20260911/NifSkope.stripped.exe || exit 1

MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc \
 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition \
  && rm -f release/NifSkope.exe \
  && make -j4 LFLAGS="-Wl,-subsystem,windows -mthreads"'
RC=$?
echo "RELINK-RC=$RC"
[ $RC -ne 0 ] && exit $RC
ls -la release/NifSkope.exe
echo "SYMBOLS=$(nm release/NifSkope.exe 2>/dev/null | wc -l)"
