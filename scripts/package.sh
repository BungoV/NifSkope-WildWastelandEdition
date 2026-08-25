#!/bin/bash
#
# Assemble a distributable Windows x64 build into dist/.
#
# This does NOT just zip release/. That directory is a working directory: it
# accumulates harness logs (ww_*.log), screenshots, scratch .bin files and old
# backup exes, and at time of writing it was 120 MB of which ~100 MB had no
# business in a download. The file list below is the authoritative one, derived
# from the copy rules in NifSkope.pro - if a build step starts shipping a new
# runtime asset, add it here too or the package silently lacks it.
#
# Usage:  bash scripts/package.sh [version]
#         version defaults to WW_VER in NifSkope.pro
set -eu

cd "$(dirname "$0")/.." || exit 1
ROOT="$PWD"
REL="$ROOT/release"

VER="${1:-$(sed -n 's/^WW_VER *= *//p' NifSkope.pro | tr -d ' \r')}"
[ -n "$VER" ] || { echo "could not determine version"; exit 1; }

NAME="NifSkope-WildWastelandEdition-$VER-win64"
STAGE="$ROOT/dist/$NAME"

[ -x "$REL/NifSkope.exe" ] || { echo "no release/NifSkope.exe - build first"; exit 1; }

rm -rf "$STAGE"
mkdir -p "$STAGE"

# --- the program -----------------------------------------------------------
cp "$REL/NifSkope.exe"    "$STAGE/"
cp "$REL/nifskope-cli.cmd" "$STAGE/" 2>/dev/null || true

# --- runtime data (NifSkope.pro: XML, QSS, qt.conf, shaders) ----------------
cp "$REL/nif.xml" "$REL/kfm.xml" "$REL/style.qss" "$REL/qt.conf" "$STAGE/"
cp -R "$REL/shaders" "$STAGE/"

# --- Qt runtime + plugins --------------------------------------------------
cp "$REL"/*.dll "$STAGE/"
for d in platforms imageformats styles; do
	cp -R "$REL/$d" "$STAGE/"
done

# --- documentation ---------------------------------------------------------
for f in README.txt LICENSE.txt CHANGELOG.txt WW_FEATURES.txt CLI.txt \
         TROUBLESHOOTING.txt README_GLTF.txt \
         TIMELINE_README.txt TIMELINE_SHORTCUTS.txt; do
	[ -f "$REL/$f" ] && cp "$REL/$f" "$STAGE/"
done

# --- the build stamp -------------------------------------------------------
#
# main.cpp PREFERS this file to the compiled-in NIFSKOPE_REVISION, because the
# define is baked when qmake runs and after an incremental build it names a
# commit the binary is not. Leaving it out of the package is the same defect
# wearing the packager's clothes: a download whose title bar cites whatever
# commit qmake last saw. It is 8 bytes.
[ -f "$REL/build_rev.txt" ] && cp "$REL/build_rev.txt" "$STAGE/"

# --- prove nothing scratch rode along --------------------------------------
STRAY=$(find "$STAGE" -maxdepth 1 \
	\( -name 'ww_*' -o -name 'ls_*' -o -name '*.log' -o -name '*.bin*' \
	   -o -name '*backup*' -o -name 'RiggingIntegration.exe' \) | wc -l)
if [ "$STRAY" -ne 0 ]; then
	echo "REFUSING: $STRAY scratch file(s) reached the staging dir"
	find "$STAGE" -maxdepth 1 \( -name 'ww_*' -o -name 'ls_*' -o -name '*.log' \
		-o -name '*.bin*' -o -name '*backup*' \) -printf '  %f\n'
	exit 1
fi

echo "staged $NAME"
echo "  files:   $(find "$STAGE" -type f | wc -l)"
echo "  size:    $(du -sh "$STAGE" | cut -f1)"
echo "  path:    $STAGE"
