#!/bin/bash
# Lane SHOWCASE1 -- the LOOK bake.
#
# Why it exists.  The showcase bake carries the two identity payloads:
#   * terrain  (--terrain-identity, default on): BTR vertex R = material class,
#     G = wetness, B = AO, A = shore  (src/lodgen.cpp:945-957)
#   * objects  (--identity, default on): BTO vertex R+G = 16-bit object index,
#     B = baked AO, A = tree sway     (src/lodgen.h:594-604)
# NifSkope's normal shading MULTIPLIES vertex colour into the diffuse, so both
# turn a look picture into a picture of the payload.  Measured on the same
# camera, chunk (-20,24): vanilla's own BTO reads mean RGB 120/119/114 over the
# same 88% background, ours reads 62/2/84 -- the green channel crushed to 2.
# So this bake is the SAME switches with both identity payloads off, and it is
# the only bake the "what does it look like" pictures are taken from.
set -u
L="E:/Projects/NifskopeWildWastelandEdition/scratchpad/showcase1_20260912"
EXE="$L/ns_run/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
VAN="E:/Tools/Fallout 4/DataUnpacked/Data"
O="$L/out/look"

if tasklist 2>/dev/null | grep -qiE '^"?Fallout4\.exe'; then echo "REFUSED: game is up"; exit 2; fi
mkdir -p "$O/obj" "$O/tex" "$O/mod" "$O/native"
{ echo "ARGV look:"; } > "$L/logs/look.argv"
set -- "$ESM" --worldspace 3C --terrain-region -20 24 -9 35 --dim 4 \
	--out-dir "$O/obj" --data-root "$VAN" --vt "$O/mod" --tex-dir "$O/tex" \
	--native "$O/native" --native-mesh-report "$O/native/mesh_report.txt" \
	--land-guide aspecthex --land-guide-scale 256 --land-hex 256 \
	--terrain-object-ao --erosion 1 --erosion-iterations 4 --erosion-seed 7 \
	--msn-cache "E:/Tools/Upscale/esrgan-bat/output" --sheet-format legacy \
	--road-detail 1 --cover --arrays --atlas \
	--no-terrain-identity --no-identity
printf '%q ' "$EXE" -no-gui lodgen "$@" >> "$L/logs/look.argv"; echo >> "$L/logs/look.argv"
"$EXE" -no-gui lodgen "$@" > "$L/logs/look.log" 2>&1
echo "exit $?"; tail -3 "$L/logs/look.log"
