#!/bin/bash
# VTNORMAL1 addition (bungo 2026-09-24): the normal-sheets folder takes his MOD
# ROOT as-is, and "auto" finds an upscaled set in the resource stack.
# Sanctuary chunk (bake.sh), new exe vs the rung. Prints ok/FAIL lines.
#   A  new, --msn-cache <sheets' own folder>            the reference
#   B  new, --msn-cache <mod root>                       == A
#   Br rung, --msn-cache <mod root>                      != A (fails on the old code)
#   C  new, --msn-cache auto --resource <mod root>       == A, census "(auto)"
#   E  new, no cache,  --resource <vanilla-512 fixture>  reference for D
#   D  new, --msn-cache auto --resource <that fixture>   == E (vanilla 512 is not an upscaled set)
#   F  new, --msn-cache auto, no resources               == new no-cache
#   N  new, no cache
L=/e/Projects/NifskopeWildWastelandEdition/scratchpad/vtnormal1_20260923
LW=E:/Projects/NifskopeWildWastelandEdition/scratchpad/vtnormal1_20260923
MOD="E:/Projects/Fallout 4 Mods/mods/Upscaled Terrain Normals"
SUB="$MOD/Textures/Terrain/Commonwealth"
VAN="/e/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth"
FIX="$L/out/vanfix/Textures/Terrain/Commonwealth"
fails=0; oks=0
ok()   { echo "  ok    $*"; oks=$((oks+1)); }
bad()  { echo "  FAIL  $*"; fails=$((fails+1)); }
# the tree minus the provenance record (it names the exe, clock and paths)
same() { diff -rq -x '*.lodb' "$L/out/$1/tex" "$L/out/$2/tex" >/dev/null 2>&1 \
	&& diff -rq -x '*.lodb' "$L/out/$1/mod" "$L/out/$2/mod" >/dev/null 2>&1; }
cen()  { grep -o "$2 [^ ]*" "$L/out/$1/bake.log" | head -1; }

mkdir -p "$FIX"
cp "$VAN/Commonwealth.4.-20.24_msn.DDS" "$FIX/"

EXE=new  bash "$L/bake.sh" au_A --msn-cache "$SUB"                         >/dev/null
EXE=new  bash "$L/bake.sh" au_B --msn-cache "$MOD"                         >/dev/null
EXE=rung bash "$L/bake.sh" au_Br --msn-cache "$MOD"                        >/dev/null
EXE=new  bash "$L/bake.sh" au_C --msn-cache auto --resource "$MOD"         >/dev/null
EXE=new  bash "$L/bake.sh" au_E --resource "$LW/out/vanfix"                >/dev/null
EXE=new  bash "$L/bake.sh" au_D --msn-cache auto --resource "$LW/out/vanfix" >/dev/null
EXE=new  bash "$L/bake.sh" au_F --msn-cache auto                           >/dev/null
EXE=new  bash "$L/bake.sh" au_N                                            >/dev/null

echo "census: A $(cen au_A normalMsnCache) | B $(cen au_B normalMsnCache) | Br $(cen au_Br normalMsnCache) | C $(cen au_C normalMsnCache) | D $(cen au_D normalMsnCache) | F $(cen au_F normalMsnCache)"
echo "dirs:   B [$(grep -o 'msnCacheDir .*' "$L/out/au_B/bake.log" | head -1)] C [$(grep -o 'msnCacheDir .*' "$L/out/au_C/bake.log" | head -1)] D [$(grep -o 'msnCacheDir .*' "$L/out/au_D/bake.log" | head -1)]"
a=$(cen au_A normalMsnCache | cut -d' ' -f2)
[ -n "$a" ] && [ "$a" != 0 ] && ok "A the sheets' own folder is read ($a tile(s) from sheets)" || bad "A read no sheets ($a)"
same au_A au_B && ok "B the mod root gives A's bytes" || bad "B the mod root differs from A"
same au_A au_Br && bad "Br the RUNG read the mod root too (the check cannot fail)" || ok "Br the rung, given the mod root, does not produce A (the check fails on the old code)"
same au_A au_C && ok "C auto over a --resource mod root gives A's bytes" || bad "C auto differs from A"
grep -q 'msnCacheDir .*(auto)' "$L/out/au_C/bake.log" && ok "C the census says (auto) and names the folder" || bad "C no (auto) census"
same au_E au_D && ok "D auto over a vanilla-512 set reads nothing (== no cache, same resources)" || bad "D auto took vanilla's 512 sheets"
same au_N au_F && ok "F auto with no resources == no cache" || bad "F auto without resources moved bytes"
echo "gate_auto: $oks ok, $fails fail"
