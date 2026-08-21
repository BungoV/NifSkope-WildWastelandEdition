#!/bin/bash
#
# Recompile a NIF with Elric, Bethesda's own converter, and say where it landed.
#
# WHY THIS EXISTS
#
# Elric is the oracle for anything about the compiled collision format: it is the
# tool that produced every stock packfile, it is deterministic, and it will
# recompile a decompiled NIF. Feed it a fixture, change one thing, feed it again,
# and the diff is the answer - which is how both hkcd trees were decoded on
# 2026-08-17.
#
# That harness lived in a session scratchpad and went with it, and the next
# session recorded "Elric is not on this machine" after searching the wrong
# drives. This is the harness, in the repo, with the path written down.
#
#   ELRIC=X:/Programs/Steam/steamapps/common/Fallout 4 1946160/Tools/Elric
#
# WHAT IT DOES
#
# Elric converts a TREE, not a file: it walks ConvertTarget looking for a path
# with `Meshes\` in it and writes into OutputDirectory. So this stages the input
# under <work>/Source/Meshes/, points a copy of Settings/PCMeshes.esf at absolute
# paths, turns on CloseWhenFinished and AutoStartConversion, and turns OFF the
# filter script (its path in the stock file is relative, and a relative path is
# what makes the run need a human).
#
# It opens a window for a second or two and closes itself. Nothing to click.
#
# NOTE: Elric STRIPS collision it already finds compiled - vanilla's included -
# so this is a pair machine, not a load oracle. Give it EDITABLE collision
# (decompile first) if you want compiled collision back.
#
# USAGE
#   bash tools/elric_pair.sh <input.nif> [<work-dir>]
#
# Prints the output path. Compare two runs with tools/hkinterior.py,
# tools/hkmatrun.py, or nifskope-cli collision.

set -u
IN="${1:?usage: elric_pair.sh <input.nif> [work-dir]}"
WORK="${2:-$(mktemp -d)}"
ELRIC="${ELRIC:-X:/Programs/Steam/steamapps/common/Fallout 4 1946160/Tools/Elric}"

[ -f "$IN" ] || { echo "no such file: $IN" >&2; exit 2; }
[ -x "$ELRIC/Elrich.exe" ] || { echo "no Elrich.exe under $ELRIC" >&2; exit 2; }
[ -f "$ELRIC/Settings/PCMeshes.esf" ] || { echo "no Settings/PCMeshes.esf under $ELRIC" >&2; exit 2; }

mkdir -p "$WORK/Source/Meshes" "$WORK/Processed" "$WORK/Logs"
name="$(basename "$IN")"
cp -f "$IN" "$WORK/Source/Meshes/$name"

python - "$ELRIC" "$WORK" <<'PYEOF'
import sys
elric, work = sys.argv[1], sys.argv[2]
def win(p):
    return p.replace('/', chr(92))
s = open(elric + "/Settings/PCMeshes.esf", 'rb').read().decode('windows-1252')
s = s.replace("<OutputDirectory>.\\Processed\\</OutputDirectory>",
              "<OutputDirectory>" + win(work + "/Processed") + chr(92) + "</OutputDirectory>")
s = s.replace("<ConvertTarget>.\\Source\\Meshes\\</ConvertTarget>",
              "<ConvertTarget>" + win(work + "/Source/Meshes") + chr(92) + "</ConvertTarget>")
s = s.replace("<CloseWhenFinished>false</CloseWhenFinished>",
              "<CloseWhenFinished>true</CloseWhenFinished>")
s = s.replace("<UseFilterScript>true</UseFilterScript>",
              "<UseFilterScript>false</UseFilterScript>")
s = s.replace("<SaveLog>false</SaveLog>", "<SaveLog>true</SaveLog>")
s = s.replace("<LogFile>.\\Logs\\PCMeshes-%m-%d-%y_%h-%n-%s.elf</LogFile>",
              "<LogFile>" + win(work + "/Logs/run.elf") + "</LogFile>")
open(work + "/batch.esf", 'w', encoding='windows-1252', newline='\r\n').write(s)
PYEOF

powershell.exe -NoProfile -Command "
  \$p = Start-Process -FilePath '$ELRIC\Elrich.exe' -ArgumentList '\"$(echo "$WORK/batch.esf" | sed 's|/|\\|g')\"' -WorkingDirectory '$ELRIC' -PassThru
  \$p.WaitForExit(180000) | Out-Null
  if (-not \$p.HasExited) { \$p.Kill(); exit 1 }
  exit \$p.ExitCode
" >/dev/null 2>&1
code=$?

out="$WORK/Processed/$name"
if [ "$code" != "0" ] || [ ! -f "$out" ]; then
	echo "Elric produced nothing (exit $code); log:" >&2
	tail -5 "$WORK/Logs/run.elf" 2>/dev/null >&2
	exit 1
fi
echo "$out"
