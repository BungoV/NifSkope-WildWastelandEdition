#!/bin/bash
# TERR1 build: wait until no make/g++/cc1plus runs on the machine (night rule: one build at a time),
# then the in-tree gated chain. Arg 1 = a tag; the built exe + runtime are copied to runs/<tag>/.
set -u
W=/e/Projects/NifskopeWWE-terr1
TAG=${1:-build}; shift || true
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo GAME UP; exit 1; fi
busy() {
  powershell -NoProfile -Command "(Get-CimInstance Win32_Process | Where-Object { \$_.Name -match '^(make|mingw32-make|g\+\+|cc1plus|ld)\.exe$' }).Count" | tr -d '\r'
}
t0=$(date +%s)
while :; do
  b=$(busy)
  [ "${b:-0}" = 0 ] && break
  echo "$(date +%H:%M:%S) other build running ($b procs); waiting"
  sleep 60
done
echo "$(date +%H:%M:%S) machine free after $(( $(date +%s) - t0 )) s"
bash $W/tools/ww_build.sh "$@"
rc=$?
echo "WW_BUILD-RC=$rc"
[ $rc = 0 ] || exit $rc
R=$W/scratchpad/terr1_20260927/runs/$TAG
rm -rf $R; mkdir -p $R
cd $W/release && cp -r NifSkope.exe *.dll *.xml *.qss *.bin qt.conf hkclasses_fo4.json hkx_annotation_vocabulary.txt imageformats platforms shaders styles $R/ 2>/dev/null
echo "run copy: $R  sha1 $(sha1sum $R/NifSkope.exe | cut -c1-8)"
