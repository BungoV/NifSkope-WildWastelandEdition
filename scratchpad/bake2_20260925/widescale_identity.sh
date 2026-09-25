#!/bin/bash
# BAKE2, ruling (a), G1 byte identity: a file whose scales all fit the old range is the file today's code writes.
# Pre-registered 2026-09-25 before the build.
#   fixture  : lodgen <Fallout4.esm> --native-fixture, rung exe vs new exe: every file cmp-identical
#   prewar   : the new exe's full pre-war bake (bake.sh, same flags) vs the installed pre-war set: .lodi/.lodo identical
#   cw       : the new exe's Commonwealth objects bake (BAKE1's chunks flags minus the VT) vs the INSTALLED
#              Commonwealth .lodi (read only; its sha1 is read before and after, since lane TINT1 may reinstall)
# usage: widescale_identity.sh fixture|prewar|cw
set -u
L=E:/Projects/NifskopeWWE-bake2/scratchpad/bake2_20260925
RUNG=E:/Projects/NifskopeWWE-bake2/release/NifSkope.before_bake2.exe
NEW=$L/run2/release/NifSkope.exe
P="E:/Projects/Fallout 4 Mods/profiles/Default"
INST="E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
gate() { if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi; }
same() {  # label a b
	if cmp -s "$1" "$2"; then echo "IDENTICAL $3: $(sha1sum < "$1" | cut -c1-12) $(stat -c %s "$1") B"
	else echo "DIFFER $3: $(cmp "$1" "$2" | head -1); $(cmp -l "$1" "$2" 2>/dev/null | wc -l) bytes"; fi; }
case "$1" in
fixture)
	gate; O=$L/wsgate/fixture; rm -rf "$O"; mkdir -p "$O/rung" "$O/new"
	"$RUNG" -no-gui lodgen "$ESM" --native-fixture "$O/rung" > "$O/rung.log" 2>&1; echo "rung rc=$?"
	"$NEW" -no-gui lodgen "$ESM" --native-fixture "$O/new" > "$O/new.log" 2>&1; echo "new rc=$?"
	n=0; d=0
	while IFS= read -r f; do n=$((n+1)); cmp -s "$O/rung/$f" "$O/new/$f" || { d=$((d+1)); echo "DIFFER fixture $f"; }; done < <(cd "$O/rung" && find . -type f | sort)
	echo "fixture: $n files, $d differ; new-only files: $(comm -13 <(cd "$O/rung" && find . -type f | sort) <(cd "$O/new" && find . -type f | sort) | wc -l)"
	;;
prewar)
	gate; ( cd $L && FILL=on bash bake.sh 000A7FF4 -28 -12 2 25 wsgate/prewar_new > /dev/null 2>&1 ); echo "bake rc=$?"
	N=$L/wsgate/prewar_new/mod/FO4CSLOD/SanctuaryHillsWorld
	same "$N/SanctuaryHillsWorld.lodi" "$INST/SanctuaryHillsWorld/SanctuaryHillsWorld.lodi" "prewar .lodi (new exe vs installed)"
	same "$N/SanctuaryHillsWorld.lodo" "$INST/SanctuaryHillsWorld/SanctuaryHillsWorld.lodo" "prewar .lodo (new exe vs installed)"
	d=0; while IFS= read -r f; do cmp -s "$N/$f" "$L/prewar/mod/FO4CSLOD/SanctuaryHillsWorld/$f" || { d=$((d+1)); echo "  differs: $f"; }; done < <(cd "$N" && find . -type f | sort)
	echo "prewar: $(cd "$N" && find . -type f | wc -l) files, $d differ from the installed bake's scratch copy"
	;;
cw)
	gate; O=$L/wsgate/cw_new; rm -rf "$O"; mkdir -p "$O/mod" "$O/scratch"
	before=$(sha1sum "$INST/Commonwealth/Commonwealth.lodi" | cut -c1-40); echo "installed CW .lodi before: $before"
	t0=$(date +%s)
	"$NEW" -no-gui lodgen --mo2-profile "$P" --worldspace 3C --terrain-region -96 -96 95 95 --dim all \
		--out-dir "$O/scratch" --tex-dir "$O/scratch/textures" --native "$O/mod" \
		--impostors E:/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/cards --arrays --fo4cs-one-root \
		> "$O/chunks.log" 2>&1
	echo "bake rc=$? $(( $(date +%s) - t0 )) s"
	after=$(sha1sum "$INST/Commonwealth/Commonwealth.lodi" | cut -c1-40); echo "installed CW .lodi after:  $after"
	same "$O/mod/FO4CSLOD/Commonwealth/Commonwealth.lodi" "$INST/Commonwealth/Commonwealth.lodi" "Commonwealth .lodi (new exe vs installed)"
	grep -a "native-wide-scale" "$O/chunks.log" | cut -c1-200
	;;
esac
