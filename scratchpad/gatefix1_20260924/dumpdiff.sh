#!/bin/bash
# GATEFIX1: dump one .bto (all blocks) from two pair dirs with the gatefix1 exe and diff them.
# usage: dumpdiff.sh <pairA> <pairB> <relfile>
W=/e/Projects/NifskopeWWE-gatefix1
P=$W/scratchpad/gatefix1_20260924/pairs
NS=$W/release/NifSkope.exe
d() {
	local f="$1" n
	n=$("$NS" -no-gui info "$f" 2>/dev/null | grep -oiE "blocks?[^0-9]*[0-9]+" | head -1 | grep -oE "[0-9]+$")
	[ -z "$n" ] && n=64
	for ((i=0;i<n;i++)); do "$NS" -no-gui dump "$f" -b $i --all -n 1000000 2>/dev/null; done
}
tag=$(echo "$3" | tr '/' '_')
d "$P/$1/$3" > "$P/$1.$tag.dump.txt"
d "$P/$2/$3" > "$P/$2.$tag.dump.txt"
wc -l "$P/$1.$tag.dump.txt" "$P/$2.$tag.dump.txt"
diff "$P/$1.$tag.dump.txt" "$P/$2.$tag.dump.txt" | head -60
echo "difflines: $(diff "$P/$1.$tag.dump.txt" "$P/$2.$tag.dump.txt" | grep -c '^[<>]')"
echo "--- manifest"
diff "$P/$1/$3.manifest.txt" "$P/$2/$3.manifest.txt" | head -30
