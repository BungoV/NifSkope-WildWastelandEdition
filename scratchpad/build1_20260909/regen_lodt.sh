#!/bin/bash
# BUILD1: regenerate the .lodt set bungo has installed, on to header version 2
# with the landless-cell fix, into his mod folder.
#
# Backups (<name>.lodt.bak-20260909) are taken BEFORE the first write and are
# never overwritten by a re-run.
#
# Each file is written into a scratch directory first, verified there with
# --verify-only, and only then moved over the installed one -- a half-written
# file in his mod folder would be worse than an old one.

set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
NS="$ROOT/release/NifSkope.exe"
DEST="E:/Projects/Fallout 4 Mods/mods/FO4CS/Terrain"
TMP="$ROOT/scratchpad/build1_20260909/regen"
DATA="X:/Programs/Steam/steamapps/common/Fallout 4/Data"

mkdir -p "$TMP"

# name  worldspace-hex  esm
SET="
Commonwealth 3C Fallout4.esm
DLC03FarHarbor B0F DLCCoast.esm
DiamondCity F94 Fallout4.esm
NukaWorld 290F DLCNukaWorld.esm
NukaWorldAmphitheater 52931 DLCNukaWorld.esm
"

fails=0
echo "$SET" | while read -r NAME WS ESM; do
	[ -n "$NAME" ] || continue
	B="$DEST/$NAME.lodt.bak-20260909"
	[ -f "$B" ] || { echo "FAIL $NAME: no backup at $B"; fails=1; continue; }
	rm -rf "$TMP/$NAME"; mkdir -p "$TMP/$NAME"
	echo "=== $NAME (ws $WS, $ESM)"
	"$NS" -no-gui lodgen "$DATA/$ESM" --worldspace "$WS" --lodt "$TMP/$NAME" \
		> "$TMP/$NAME.write.txt" 2>&1
	wrc=$?
	F="$TMP/$NAME/Terrain/$NAME.lodt"
	echo "  write rc=$wrc  $( [ -f "$F" ] && stat -c %s "$F" || echo NOFILE ) bytes"
	[ "$wrc" -eq 0 ] && [ -s "$F" ] || { echo "  FAIL write"; fails=1; continue; }
	"$NS" -no-gui lodgen "$DATA/$ESM" --worldspace "$WS" --lodt "$TMP/$NAME" --verify-only \
		> "$TMP/$NAME.verify.txt" 2>&1
	vrc=$?
	echo "  verify rc=$vrc"
	[ "$vrc" -eq 0 ] || { echo "  FAIL verify"; fails=1; continue; }
	cp "$F" "$DEST/$NAME.lodt" && echo "  installed"
done
