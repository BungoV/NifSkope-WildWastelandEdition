#!/bin/bash
#
# Bake an assembled, posed rig into loading-screen art -- and prove the geometry
# is right, not just that the block list looks plausible.
#
# WHY THIS EXISTS
#
# A skin bake fails in ways a structural check cannot see. Drop the bind
# transform and every piece lands somewhere wrong but the file still loads; skip
# the weight normalisation and the seams tear but the bounding box barely moves;
# forget to re-centre and the vertices quantise into faceting that only shows up
# under a light. So the invariants here are about WHERE THINGS ARE.
#
# THE INVARIANTS
#
#   1. Structure matches vanilla. NiNode count is exactly 2 (root + zoom target),
#      no BSSkin::Instance / BSSkin::BoneData / bhk* / BSConnectPoint survives,
#      and the shapes and their shader properties do.
#   2. Vertex Desc and Data Size move together. Dropping the Skinned attribute
#      must shrink the stride by exactly 12 bytes per vertex (4 half weights +
#      4 byte indices), and Data Size must equal stride*verts + 6*tris. If these
#      disagree the file reads back as garbage -- and a plain reload will not
#      always say so.
#   3. Left/right symmetry survives. The arms and legs are separate shapes
#      skinned through separate bone chains; if the evaluation were wrong they
#      would not come back mirrored to within a rounding error.
#   4. Vertices are LOCAL. Every baked vertex must be small enough that FO4's
#      half-float storage does not visibly quantise it -- that is the entire
#      reason the format re-centres each shape.
#   5. A pose is actually baked, and only where it should be. Moving ONE bone
#      must move the shape that bone drives and leave its mirror image bit-exact.
#
# USAGE
#   bash tests/loadingscreen/bake_check.sh
# Needs release/NifSkope.exe, the FO4 corpus and the X01Tesla mod.

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
SK="${SK:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/powerarmor/CharacterAssets/skeleton.nif}"
X="${X:-/e/Projects/Fallout 4 Mods/mods/X01Tesla/meshes/actors/powerarmor/x01}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0; fail=0
ok()  { pass=$((pass+1)); }
bad() { fail=$((fail+1)); echo "  FAIL: $*"; }

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SK" ]  || { echo "no skeleton at $SK"; exit 2; }

# NOTE: quoted throughout. Both corpus paths contain a space ("Fallout 4"), and
# an unquoted expansion turns this whole script into a clean-exit no-op.
echo "assembling the rig"
"$EXE" -no-gui merge "$SK" \
	--add "$X/X01_Helmet.nif"  --add "$X/X01_Torso.nif" \
	--add "$X/X01_ArmLeft.nif" --add "$X/X01_ArmRight.nif" \
	--add "$X/X01_LegLeft.nif" --add "$X/X01_LegRight.nif" \
	-o "$TMP/rig.nif" > /dev/null 2>&1 || { echo "merge failed"; exit 2; }

echo "baking"
"$EXE" -no-gui loading-screen "$TMP/rig.nif" -o "$TMP/screen.nif" 2>&1 | sed 's/^/  /'

# info prints the tally as "NiNode  x2" -- the count needs the x stripped.
blocktypes() { "$EXE" -no-gui info "$1" 2>/dev/null | awk -v t="$2" '$1==t {sub(/^x/,"",$2); print $2}'; }
tally()      { "$EXE" -no-gui list "$1" 2>/dev/null | grep -c "$2"; }
field()      { "$EXE" -no-gui get "$1" -b "$2" -f "$3" 2>/dev/null; }
blockof()    { "$EXE" -no-gui list "$1" 2>/dev/null | grep -m1 -F "$2" | sed 's/^\[\([0-9]*\)\].*/\1/'; }

# --- 1. structure ----------------------------------------------------------
nodes="$(blocktypes "$TMP/screen.nif" NiNode)"
if [ "${nodes:-0}" = "2" ]; then ok
else bad "NiNode count is ${nodes:-0}, expected 2 (root + LoadingMenuZoomTarget)"; fi

if [ -n "$(blockof "$TMP/screen.nif" LoadingMenuZoomTarget)" ]; then ok
else bad "no LoadingMenuZoomTarget"; fi

for gone in "BSSkin::Instance" "BSSkin::BoneData" "bhk" "BSConnectPoint" "BSBoneLODExtraData"; do
	n="$(tally "$TMP/screen.nif" "$gone")"
	if [ "$n" = "0" ]; then ok; else bad "$n $gone block(s) survived"; fi
done

before="$(tally "$TMP/rig.nif" "BSLightingShaderProperty")"
after="$(tally "$TMP/screen.nif" "BSLightingShaderProperty")"
if [ "$before" = "$after" ]; then ok
else bad "shader properties went from $before to $after"; fi

# --- 2. Vertex Desc and Data Size move together ----------------------------
# Same shape either side, found by name so renumbering cannot confuse it.
rb="$(blockof "$TMP/rig.nif"    "X01_Torso:0")"
sb="$(blockof "$TMP/screen.nif" "X01_Torso:0")"
if [ -n "$rb" ] && [ -n "$sb" ]; then
	rv="$(field "$TMP/rig.nif" "$rb" "Num Vertices")"
	rt="$(field "$TMP/rig.nif" "$rb" "Num Triangles")"
	rd="$(field "$TMP/rig.nif" "$rb" "Data Size")"
	sv="$(field "$TMP/screen.nif" "$sb" "Num Vertices")"
	st="$(field "$TMP/screen.nif" "$sb" "Num Triangles")"
	sd="$(field "$TMP/screen.nif" "$sb" "Data Size")"

	if [ "$rv" = "$sv" ] && [ "$rt" = "$st" ]; then ok
	else bad "vertex/triangle counts changed: $rv/$rt -> $sv/$st"; fi

	rstride=$(( (rd - 6 * rt) / rv ))
	sstride=$(( (sd - 6 * st) / sv ))
	if [ "$(( rstride - sstride ))" -eq 12 ]; then ok
	else bad "stride shrank by $(( rstride - sstride )) bytes, expected 12 (4 half weights + 4 byte indices): $rstride -> $sstride"; fi
	if [ "$(( sstride * sv + 6 * st ))" -eq "$sd" ]; then ok
	else bad "Data Size $sd != stride*verts + 6*tris"; fi

	skin="$(field "$TMP/screen.nif" "$sb" "Skin")"
	if [ "$skin" = "-1" ]; then ok; else bad "Skin ref is $skin, expected -1"; fi
	echo "  stride $rstride -> $sstride bytes/vertex, Skin -> $skin"
fi

# --- 3. symmetry -----------------------------------------------------------
# awk, not bash arithmetic: these are floats.
sym() {
	local la lb a b
	la="$(blockof "$TMP/screen.nif" "$1")"; lb="$(blockof "$TMP/screen.nif" "$2")"
	[ -n "$la" ] && [ -n "$lb" ] || { bad "cannot find $1 / $2"; return; }
	a="$(field "$TMP/screen.nif" "$la" "Translation")"
	b="$(field "$TMP/screen.nif" "$lb" "Translation")"
	echo "$a|$b" | awk -F'|' -v n="$1 vs $2" '{
		split($1,L," "); split($2,R," ");
		lx=L[2]; ly=L[4]; lz=L[6]; rx=R[2]; ry=R[4]; rz=R[6];
		dx=lx+rx; dy=ly-ry; dz=lz-rz;
		if (dx<0) dx=-dx; if (dy<0) dy=-dy; if (dz<0) dz=-dz;
		if (dx<0.5 && dy<0.5 && dz<0.5) print "OK";
		else printf "BAD %s: mirror error (%.3f, %.3f, %.3f)\n", n, dx, dy, dz;
	}' > "$TMP/sym.txt"
	if grep -q '^OK' "$TMP/sym.txt"; then ok; else bad "$(cat "$TMP/sym.txt")"; fi
}
sym "X01_ArmLeft:0" "X01_ArmRight:0"
sym "X01_LegLeft:0" "X01_LegRight:0"

# --- 4a. THE load-bearing check: bind-pose round-trip ----------------------
# At bind pose the skin evaluation is a no-op, so node.translation + vertex must
# reproduce the ORIGINAL world position. This is the check whose absence let a
# completely broken converter ship: set<Vector3>() on a half-precision "Vertex"
# field does nothing and returns a false nobody read, so the node translations
# moved to their centroids while the vertices stayed put. Every shape ended up
# displaced by its own centroid -- different offsets per shape, everything out of
# place -- and the old "vertices are local" check below passed anyway, because FO4
# armour already stores its vertices near the body origin.
"$EXE" -no-gui loading-screen "$TMP/rig.nif" -o "$TMP/bind.nif" > /dev/null 2>&1
for name in "X01_Torso:0" "X01_Helmet:0" "X01_ArmLeft:0" "X01_LegRight:0"; do
	rb="$(blockof "$TMP/rig.nif"  "$name")"
	bb="$(blockof "$TMP/bind.nif" "$name")"
	[ -n "$rb" ] && [ -n "$bb" ] || { bad "cannot find $name either side"; continue; }
	echo "$(field "$TMP/rig.nif" "$rb" "Translation")|$(field "$TMP/rig.nif" "$rb" "Vertex Data/0/Vertex")|$(field "$TMP/bind.nif" "$bb" "Translation")|$(field "$TMP/bind.nif" "$bb" "Vertex Data/0/Vertex")" \
	| awk -F'|' -v n="$name" '{
		split($1,a," "); split($2,b," "); split($3,c," "); split($4,d," ");
		ox=a[2]+b[2]; oy=a[4]+b[4]; oz=a[6]+b[6];
		nx=c[2]+d[2]; ny=c[4]+d[4]; nz=c[6]+d[6];
		e=sqrt((ox-nx)^2+(oy-ny)^2+(oz-nz)^2);
		# 0.05 is generous for half-float rounding and far tighter than any real
		# displacement, which is tens of units.
		if (e < 0.05) printf "OK %s (err %.4f)\n", n, e;
		else printf "BAD %s: world position moved %.3f at bind pose -- (%.3f, %.3f, %.3f) became (%.3f, %.3f, %.3f)\n", n, e, ox,oy,oz, nx,ny,nz;
	}' > "$TMP/rt.txt"
	if grep -q '^OK' "$TMP/rt.txt"; then ok; echo "  $(cat "$TMP/rt.txt")"
	else bad "$(cat "$TMP/rt.txt")"; fi
done

# --- 4b. vertices are local -------------------------------------------------
# Kept, but demoted: on its own this proves nothing, since the ORIGINAL vertices
# already satisfy it. It only rules out a bake that wrote world coordinates.
# 60 is the bar, and it is set by vanilla rather than by taste: the X-01 screen
# stores its body vertices at magnitudes around 30 with the node at Z = 111. What
# must never happen is a vertex carrying the WORLD coordinate, which for a
# standing figure means 100+, where the half-float step reaches ~0.0078 and shows
# as faceting.
worst=0
for name in "X01_Torso:0" "X01_Helmet:0" "X01_LegLeft:0"; do
	b="$(blockof "$TMP/screen.nif" "$name")"
	[ -n "$b" ] || continue
	for i in 0 1 2 3 4 5 6 7; do
		v="$(field "$TMP/screen.nif" "$b" "Vertex Data/$i/Vertex")"
		[ -n "$v" ] || continue
		worst="$(echo "$v $worst" | awk '{
			for(i=2;i<=6;i+=2){a=$i; if(a<0)a=-a; if(a>$7)$7=a}
			print $7 }')"
	done
done
if [ -n "$worst" ] && [ "$(echo "$worst" | awk '{print ($1 < 60) ? 1 : 0}')" = "1" ]; then
	ok; echo "  largest sampled local coordinate: $worst"
else bad "baked vertices are not local (largest sampled coordinate $worst)"; fi

# --- 5. a pose is baked, and only where it should be -----------------------
arm="$(blockof "$TMP/rig.nif" "'LArm_UpperArm'")"
if [ -n "$arm" ]; then
	t="$(field "$TMP/rig.nif" "$arm" "Translation" | awk '{printf "%s,0.0,40.0", $2}')"
	"$EXE" -no-gui set "$TMP/rig.nif" -b "$arm" -f "Translation" -v "$t" -o "$TMP/posed.nif" > /dev/null 2>&1
	"$EXE" -no-gui loading-screen "$TMP/posed.nif" -o "$TMP/screen_posed.nif" > /dev/null 2>&1

	lb="$(blockof "$TMP/screen.nif" "X01_ArmLeft:0")"
	lp="$(blockof "$TMP/screen_posed.nif" "X01_ArmLeft:0")"
	rb="$(blockof "$TMP/screen.nif" "X01_ArmRight:0")"
	rp="$(blockof "$TMP/screen_posed.nif" "X01_ArmRight:0")"

	if [ "$(field "$TMP/screen.nif" "$lb" "Translation")" != "$(field "$TMP/screen_posed.nif" "$lp" "Translation")" ]; then
		ok; echo "  posed left arm moved to $(field "$TMP/screen_posed.nif" "$lp" "Translation")"
	else bad "moving LArm_UpperArm did not move the left arm -- the pose is not being baked"; fi

	if [ "$(field "$TMP/screen.nif" "$rb" "Translation")" = "$(field "$TMP/screen_posed.nif" "$rp" "Translation")" ]; then ok
	else bad "moving LArm_UpperArm also moved the RIGHT arm"; fi
fi

echo
echo "checks passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]
