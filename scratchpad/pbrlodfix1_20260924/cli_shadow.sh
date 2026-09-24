#!/bin/bash
# PBRLODFIX1 side gate: the two flags the gltf parser shadowed, per exe.
#  (1) collision <nif> --skeleton : must run the skeleton report, not "gltf: --skeleton needs a value"
#  (2) lodgen --data-root X --print-source : must name X as the loose root
# usage: cli_shadow.sh <exe> ...
RAG="E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Actors/Brahmin/CharacterAssets/Skeleton.nif"
W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT
mkdir -p "$W/root"
for e in "$@"; do
	out="$("$e" -no-gui collision "$RAG" --skeleton 2>&1 | tr -d '\r')"; rc=$?
	if echo "$out" | grep -q "gltf:"; then c1="SHADOWED ($(echo "$out" | grep -m1 gltf:))"; else c1="ok (rc $rc, $(echo "$out" | wc -l) lines)"; fi
	echo "$(basename "$(dirname "$e")")/$(basename "$e"): collision --skeleton $c1"
done
