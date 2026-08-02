#!/bin/bash
#
# Make Asset Paths Relative — does it anchor correctly, and refuse to guess?
#
# An absolute texture path resolves on the machine that authored it and nowhere
# else, which is why spErrorInvalidPaths reports it as an error rather than a
# note. This is the repair.
#
# THE INTERESTING PART IS WHAT IT DECLINES. An adversarial review of the first
# sketch of this spell returned six corrections, and three of them are the cases
# below. All run through the CLI, so this needs no GUI.
#
#   1. anchors at the LAST data directory
#      C:\Users\me\modding\textures\effects\x.dds -> textures\effects\x.dds
#
#   2. "tex" MUST NOT match "textures"
#      BA2File::checkDataDirName compares only as many characters as the
#      component has, so the archive code's own rule anchors C:\work\tex\foo.dds
#      at "tex". Reusing it verbatim would have inherited that. This spell
#      matches components exactly, so the path is left alone.
#
#   3. a path with no data directory in it is LEFT ALONE, not guessed at
#
#   4. the innermost of two data directories wins
#      C:\a\textures\x\textures\y\z.dds -> textures\y\z.dds
#
# There is deliberately NO verification against the loaded archives. findResource-
# File looks like the obvious gate and is wrong as one three times over: it
# returns a lowercased, forward-slashed path with the extension COERCED (so
# writing it back could name a different file), it appends the open NIF's own
# folder to the search path (so a path that only resolves on this machine
# verifies happily — the exact problem being fixed), and the first call loads
# every configured archive from disk.
#
# USAGE
#   bash tests/spells/relative_paths.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/Effects/ElectricalExplosionSmall.nif}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }

winpath() { printf '%s' "$1" | sed 's|^/\([a-zA-Z]\)/|\1:/|'; }
W="$(winpath "$TMP")"

SHADER="$("$EXE" -no-gui list "$(winpath "$SRC")" 2>/dev/null \
	| sed -n 's/^\[\([0-9]*\)\] BSEffectShaderProperty *$/\1/p' | head -1)"
[ -n "$SHADER" ] || { echo "no BSEffectShaderProperty in the fixture"; exit 2; }

pass=0; fail=0
probe() {
	local desc="$1" input="$2" want="$3"
	"$EXE" -no-gui set "$(winpath "$SRC")" -b "$SHADER" -f "Source Texture" \
		-v "$input" -o "$W/in.nif" > /dev/null 2>&1
	"$EXE" -no-gui cast "$W/in.nif" -s "Sanitize/Make Asset Paths Relative" \
		-o "$W/out.nif" > /dev/null 2>&1
	local got
	got="$("$EXE" -no-gui get "$W/out.nif" -b "$SHADER" -f "Source Texture" 2>/dev/null | tr -d '\r')"
	if [ "$got" = "$want" ]; then
		pass=$((pass+1)); printf '  ok   %s\n' "$desc"
	else
		fail=$((fail+1))
		printf '  FAIL %s\n       in:   %s\n       want: %s\n       got:  %s\n' \
			"$desc" "$input" "$want" "$got"
	fi
}

echo "fixture: block $SHADER Source Texture"

probe "an absolute path is anchored at its data directory" \
	'C:\Users\me\modding\textures\effects\Spark.dds' \
	'textures\effects\Spark.dds'

probe "a relative path is left exactly as it is" \
	'textures\effects\Spark.dds' \
	'textures\effects\Spark.dds'

probe "'tex' does not match 'textures' (the archive code's rule would)" \
	'C:\work\tex\foo.dds' \
	'C:\work\tex\foo.dds'

probe "a path with no data directory is left alone, not guessed at" \
	'C:\random\stuff\bar.dds' \
	'C:\random\stuff\bar.dds'

probe "the innermost data directory wins" \
	'C:\a\textures\x\textures\y\z.dds' \
	'textures\y\z.dds'

probe "forward slashes are normalised on the way out" \
	'C:/Users/me/textures/effects/Spark.dds' \
	'textures\effects\Spark.dds'

echo
echo "checks passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]
