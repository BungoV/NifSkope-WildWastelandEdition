#!/bin/bash
#
# A ragdoll's JOINTS survive Decompile, and the NIF form loses nothing.
#
# WHY THIS EXISTS
#
# The backlog said multi-body systems were what ragdolls needed. Measured on the
# Brahmin skeleton that was backwards: the assembler already writes a 39-body
# ragdoll byte for byte, constraints included -- 2/2 packfiles, 30/30 ragdoll
# joints, 8/8 hinges, 41/41 capsules. What broke was the round trip through the
# EDITABLE form. Decompile produced 41 bodies and ZERO constraints, and a ragdoll
# without its articulation is 41 loose capsules.
#
# Both ends already existed: the decoder fills HknpConstraint completely and the
# encoders write it back. Only the middle was missing, exactly as it had been for
# the mesh shape builder and for cinfoFlags.
#
# WHAT IS CHECKED
#
#   1. the fixture really is a ragdoll carrying joints, so nothing here is vacuous
#   2. every joint goes through its NIF block and comes back BYTE-IDENTICAL to
#      what the encoder builds directly (collision --constraints)
#   3. the frame field NAMES hold their own identity when read back by name --
#      the check that a self-consistent but wrongly named mapping fails
#   4. Decompile emits one NIF block per joint, of the right kinds and counts
#   5. every joint names two DIFFERENT bhkRigidBody blocks, none dangling
#   6. every joint is listed on exactly one body, and the totals add up
#   7. the carried frames are real data, not a block full of zeros
#   8. Compile All Collision writes ONE bhkPhysicsSystem, the way vanilla does
#   9. every joint comes back out of it -- on a plain physics system AND on a
#      file whose joints are BREAKABLE, the wrapper only 11 corpus joints use
#  10. the joints' own limit numbers survive vanilla -> decompile -> compile ->
#      decompile unchanged
#  11. the per-body Compile still drops them, so 8..10 are not free
#  12. a RAGDOLL comes back as a bhkRagdollSystem beside a bhkPhysicsSystem,
#      the way vanilla ships one, with its bone count, its joints, and the
#      same bone tree parent for parent -- the skeleton is BUILT from the
#      bodies and the joints, not carried, and the parent array is what
#      catches a bone ORDER that is merely a valid permutation
#  13. a ragdoll with MORE BODIES THAN BONES still comes back -- the bones
#      are a prefix of the bodies, and the body the tree never reaches has
#      to sort last
#  14. a rebuilt ragdoll WEIGHS what vanilla's does -- per-body mass, density
#      and centre of mass -- which is the pair of defects the game found and
#      no check of shapes, bones or joints could
#  15. a TRIGGER material survives Compile -- the character bumper's, which
#      came back as ordinary solid collision because Compile zeroed it
#
# WHY 11 IS THERE: a joint names TWO bodies, and the per-body Compile writes one
# system per body, so it cannot express one. Compile All is what captures the
# joints first, compiles every body, and hands them to the merge to rebind --
# and if the per-body path ever started carrying them, 8..10 would stop
# measuring the thing they were written for.
#
# WHY 3 IS SEPARATE FROM 2: writer and reader share one name table, so swapping
# two field names cancels out and 2 still passes -- it did, 38 of 38, with
# "Plane A" and "Motor A" deliberately exchanged. What separates a right naming
# from a wrong one is a property of the fields themselves: the third basis vector
# is the cross product of the first two, which is how NifSkope's own "Recompute B
# Frame from A" authors Motor A. With the names swapped that check reports 8 of 38.
#
# NOTE ON PORTS: not needed, this is headless - the CLI takes no port.
#
# USAGE
#   bash tests/spells/collision_constraints.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
# 39 bones, 30 ragdoll joints and 8 limited hinges in two packfiles
RAG="${RAG:-E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Actors/Brahmin/CharacterAssets/Skeleton.nif}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$RAG" ] || { echo "no fixture at $RAG"; exit 2; }

checks=0; fails=0
check() { checks=$((checks+1)); if [ "$2" = "1" ]; then echo "  ok   $1"; else echo "  FAIL $1"; fails=$((fails+1)); fi; }
blocks() { "$NS" -no-gui list "$1" 2>/dev/null; }
field() {   # $1 nif, $2 block, $3 field name -> the value column
	"$NS" -no-gui dump "$1" -b "$2" 2>/dev/null \
		| sed -n "s/^ *$3  *<[^>]*>  *= *//p" | head -1
}

echo "fixture: $RAG"

# --- 1: the fixture carries joints in the first place ------------------------
rt=$("$NS" -no-gui collision "$RAG" --roundtrip 2>/dev/null)
vanrag=$(echo "$rt" | awk '/^ragdollcon /{print $2}')
vanhinge=$(echo "$rt" | awk '/^hingecon /{print $2}')
vanrag="${vanrag:-0}"; vanhinge="${vanhinge:-0}"
echo "  vanilla packfile: $vanrag ragdoll joints, $vanhinge limited hinges"
check "the fixture really is a jointed ragdoll" \
	"$([ "$vanrag" -gt 0 ] && [ "$vanhinge" -gt 0 ] && echo 1 || echo 0)"

# --- 2 and 3: the carrier, and the names --------------------------------------
cc=$("$NS" -no-gui collision "$RAG" --constraints 2>/dev/null); ccrc=$?
echo "$cc" | sed 's/^/  | /'
gotcar=$(echo "$cc" | sed -n 's/.*byte-identical *\([0-9]*\) \/ \([0-9]*\).*/\1 \2/p')
carok=$(echo "$gotcar" | awk '{print ($1 == $2 && $2 > 0) ? 1 : 0}')
check "every joint comes back byte-identical through its NIF block" "${carok:-0}"
gotname=$(echo "$cc" | sed -n 's/.*BY FIELD NAME *\([0-9]*\) \/ \([0-9]*\),.*/\1 \2/p')
nameok=$(echo "$gotname" | awk '{print ($1 == $2 && $2 > 0) ? 1 : 0}')
check "the frame field names hold their own cross-product identity" "${nameok:-0}"
check "the check itself exits clean" "$([ "$ccrc" = "0" ] && echo 1 || echo 0)"

# --- 4: Decompile writes one block per joint ---------------------------------
"$NS" -no-gui cast "$RAG" -s "Havok/Decompile All Compiled Collision" -o "$W/dec.nif" >/dev/null 2>&1
[ -s "$W/dec.nif" ] || { echo "  FAIL the ragdoll would not decompile"; echo "$((checks+1)) checks, $((fails+1)) failures"; echo FAIL; exit 1; }
lst=$(blocks "$W/dec.nif")
gotrag=$(echo "$lst" | grep -c '\] bhkRagdollConstraint$' || true)
gothinge=$(echo "$lst" | grep -c '\] bhkLimitedHingeConstraint$' || true)
gotbody=$(echo "$lst" | grep -c '\] bhkRigidBody$' || true)
echo "  decompiled: $gotbody bodies, $gotrag ragdoll joints, $gothinge limited hinges"
check "Decompile emits vanilla's own joint counts, by kind" \
	"$([ "$gotrag" = "$vanrag" ] && [ "$gothinge" = "$vanhinge" ] && echo 1 || echo 0)"

# --- 5..7: the joints are bound to real, distinct bodies ---------------------
conblocks=$(echo "$lst" | sed -n 's/^\[\([0-9]*\)\] bhk\(Ragdoll\|LimitedHinge\)Constraint$/\1/p')
bodyset=" $(echo "$lst" | sed -n 's/^\[\([0-9]*\)\] bhkRigidBody$/\1/p' | tr '\n' ' ') "
bound=0; distinct=0; nonzero=0; total=0
for c in $conblocks; do
	total=$((total+1))
	ea=$(field "$W/dec.nif" "$c" "Entity A")
	eb=$(field "$W/dec.nif" "$c" "Entity B")
	case "$bodyset" in *" $ea "*) case "$bodyset" in *" $eb "*) bound=$((bound+1));; esac;; esac
	[ "$ea" != "$eb" ] && distinct=$((distinct+1))
	# Pivot B is the child bone's offset in its parent's space: zero on a root
	# joint at most, never on all of them
	pb=$(field "$W/dec.nif" "$c" "Pivot B")
	case "$pb" in *"X 0.000000 Y 0.000000 Z 0.000000"*) ;; *) nonzero=$((nonzero+1));; esac
done
echo "  bound to real bodies: $bound/$total, distinct pairs: $distinct/$total, non-zero pivot B: $nonzero/$total"
check "every joint names two existing bhkRigidBody blocks" \
	"$([ "$total" -gt 0 ] && [ "$bound" = "$total" ] && echo 1 || echo 0)"
check "no joint binds a body to itself" \
	"$([ "$total" -gt 0 ] && [ "$distinct" = "$total" ] && echo 1 || echo 0)"
check "the carried frames hold real values, not zeros" \
	"$([ "$nonzero" -gt 0 ] && echo 1 || echo 0)"

# --- 6: each joint listed on exactly one body --------------------------------
listed=0
for b in $(echo "$lst" | sed -n 's/^\[\([0-9]*\)\] bhkRigidBody$/\1/p'); do
	n=$(field "$W/dec.nif" "$b" "Num Constraints")
	listed=$((listed + ${n:-0}))
done
echo "  joints listed across all bodies: $listed"
check "every joint is listed on exactly one body" \
	"$([ "$listed" = "$total" ] && [ "$total" -gt 0 ] && echo 1 || echo 0)"


# ============================================================================
# The way back: Compile All Collision, which is the only place a joint can be
# written. Compile builds one system per body and a joint names TWO of them, so
# the per-body Compile drops every joint it meets -- check 15 proves that is
# still true, which is what makes 11..14 mean something.
# ============================================================================

# 2 bodies and one limited hinge in a plain bhkPhysicsSystem: a swinging plank
PLANK="${PLANK:-E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/SetDressing/Animated/PlankHinges/PlankHinge01.nif}"
# 3 bodies and two BREAKABLE joints -- the wrapper, which only 11 corpus joints use
MEAT="${MEAT:-E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/SetDressing/ButcherMeat/ButcherMeat01.nif}"

jointcount() { "$NS" -no-gui collision "$1" --constraints 2>/dev/null | sed -n 's/^joints *\([0-9]*\).*/\1/p' | head -1; }
systems()    { "$NS" -no-gui info "$1" 2>/dev/null | awk '/bhkPhysicsSystem/{gsub("x","",$2); print $2}'; }
# every joint block's limit numbers, in block order: what has to survive the trip
jointvalues() {
	local n
	"$NS" -no-gui cast "$1" -s "Havok/Decompile All Compiled Collision" -o "$W/jv.nif" >/dev/null 2>&1
	[ -s "$W/jv.nif" ] || return 1
	for n in $("$NS" -no-gui list "$W/jv.nif" 2>/dev/null | sed -n 's/^\[\([0-9]*\)\] bhk\(Ragdoll\|LimitedHinge\|Breakable\)Constraint$/\1/p'); do
		"$NS" -no-gui dump "$W/jv.nif" -b "$n" 2>/dev/null \
			| grep -E '(Min|Max) Angle|Max Friction|Cone Max Angle|Twist (Min|Max) Angle|Threshold' \
			| tr -s ' '
	done
}
compileall() {  # $1 source, $2 out
	"$NS" -no-gui cast "$1" -s "Havok/Decompile All Compiled Collision" -o "$W/ca_dec.nif" >/dev/null 2>&1
	[ -s "$W/ca_dec.nif" ] || return 1
	"$NS" -no-gui cast "$W/ca_dec.nif" -s "Havok/Compile All Collision" -o "$2" >/dev/null 2>&1
	[ -s "$2" ]
}

for fx in "$PLANK" "$MEAT"; do
	[ -f "$fx" ] || { echo "  (no fixture at $fx, skipped)"; continue; }
	echo "fixture: $fx"
	want=$(jointcount "$fx"); want="${want:-0}"
	wantval=$(jointvalues "$fx")
	echo "  vanilla joints: $want"
	check "the fixture carries joints in a plain physics system" \
		"$([ "$want" -gt 0 ] && echo 1 || echo 0)"

	if compileall "$fx" "$W/all.nif"; then
		got=$(jointcount "$W/all.nif"); got="${got:-0}"
		sys=$(systems "$W/all.nif"); sys="${sys:-0}"
		gotval=$(jointvalues "$W/all.nif")
		echo "  after Compile All: $got joints in $sys system(s)"
		check "Compile All writes ONE bhkPhysicsSystem" "$([ "$sys" = "1" ] && echo 1 || echo 0)"
		check "every joint came back" "$([ "$got" = "$want" ] && echo 1 || echo 0)"
		check "the joints' own limits survived the whole trip" \
			"$([ -n "$wantval" ] && [ "$gotval" = "$wantval" ] && echo 1 || echo 0)"
		[ "$gotval" = "$wantval" ] || { echo "    wanted: $(echo "$wantval" | tr '\n' '|')"; echo "    got:    $(echo "$gotval" | tr '\n' '|')"; }
	else
		check "Compile All writes ONE bhkPhysicsSystem" 0
		check "every joint came back" 0
		check "the joints' own limits survived the whole trip" 0
	fi
done

# --- a body whose joint reaches OUT of the file ------------------------------
# A robot part is ONE body carrying a joint whose parent body is 0x7fffffff: it
# attaches to whatever assembles the part. Requiring both sides to resolve
# dropped that joint and with it the attachment, on 30 of 102 corpus files.
PART="${PART:-E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Actors/Robot/Parts/FrontArmorProtectron.nif}"
if [ -f "$PART" ]; then
	echo "fixture: $PART"
	pwant=$(jointcount "$PART"); pwant="${pwant:-0}"
	bodies=$("$NS" -no-gui collision "$PART" 2>/dev/null | awk '/decoded/{for(i=1;i<=NF;i++) if($i=="body/bodies,") print $(i-1)}' | head -1)
	echo "  vanilla: $pwant joint(s), ${bodies:-?} body/bodies"
	check "the part really is one body with a joint reaching out of the file" 		"$([ "$pwant" -gt 0 ] && [ "${bodies:-0}" = "1" ] && echo 1 || echo 0)"
	if compileall "$PART" "$W/part.nif"; then
		pgot=$(jointcount "$W/part.nif"); pgot="${pgot:-0}"
		# the sentinel has to come back, not a body index
		psent=$("$NS" -no-gui collision "$W/part.nif" 2>/dev/null | grep -c 'body 2147483647' || true)
		echo "  after Compile All: $pgot joint(s), $psent naming no body"
		check "the part keeps its attachment joint" "$([ "$pgot" = "$pwant" ] && echo 1 || echo 0)"
		check "...still naming NO body on the far side, not a body index" 			"$([ "$psent" -ge 1 ] && echo 1 || echo 0)"
	else
		check "the part keeps its attachment joint" 0
		check "...still naming NO body on the far side, not a body index" 0
	fi
fi

# --- 15: and the per-body Compile still cannot, which is the point -----------
"$NS" -no-gui cast "$PLANK" -s "Havok/Decompile All Compiled Collision" -o "$W/pb.nif" >/dev/null 2>&1
if [ -s "$W/pb.nif" ]; then
	cp -f "$W/pb.nif" "$W/pbcur.nif"
	for _ in $(seq 1 20); do
		o=$("$NS" -no-gui list "$W/pbcur.nif" 2>/dev/null | sed -n 's/^\[\([0-9]*\)\] bhkCollisionObject.*/\1/p' | head -1)
		[ -n "$o" ] || break
		"$NS" -no-gui cast "$W/pbcur.nif" -s "Havok/Compile Collision" -b "$o" -o "$W/pbnxt.nif" >/dev/null 2>&1
		[ -s "$W/pbnxt.nif" ] || break
		mv -f "$W/pbnxt.nif" "$W/pbcur.nif"
	done
	"$NS" -no-gui cast "$W/pbcur.nif" -s "Havok/Merge Physics Systems" -o "$W/pbm.nif" >/dev/null 2>&1
	[ -s "$W/pbm.nif" ] && mv -f "$W/pbm.nif" "$W/pbcur.nif"
	pbj=$(jointcount "$W/pbcur.nif"); pbj="${pbj:-0}"
	echo "  per-body Compile + Merge leaves $pbj joints"
	check "the per-body path still drops them, so the checks above are not free" \
		"$([ "$pbj" = "0" ] && echo 1 || echo 0)"
fi


# ============================================================================
# And the ragdoll itself: bodies, joints AND the skeleton, back into a
# bhkRagdollSystem. The skeleton is BUILT, not carried -- see WW_CHANGES
# 2026-08-23i for the measurement that says every field of it is either already
# in the file or a rule with no exceptions.
# ============================================================================

echo "fixture: $RAG"
systemtypes() { "$NS" -no-gui info "$1" 2>/dev/null | awk '/bhkRagdollSystem|bhkPhysicsSystem/{gsub("x","",$2); print $1"="$2}' | sort | tr '\n' ' '; }
# the decoder's own reading of the skeleton: index, parent, rest pose, per bone
bonetable() { "$NS" -no-gui collision "$1" 2>/dev/null | sed -n '/hkaSkeleton bones/,/^  shape/p' | grep -E '^  [0-9]+ +-?[0-9]+ '; }
parents() { bonetable "$1" | awk '{printf "%s ", $2}'; }

vantypes=$(systemtypes "$RAG")
echo "  vanilla systems: $vantypes"
check "the fixture is a bhkRagdollSystem beside a bhkPhysicsSystem" \
	"$(case "$vantypes" in *bhkRagdollSystem=1*bhkPhysicsSystem=1*|*bhkPhysicsSystem=1*bhkRagdollSystem=1*) echo 1;; *) echo 0;; esac)"
vanbones=$(bonetable "$RAG" | wc -l)
vanparents=$(parents "$RAG")
echo "  vanilla bones: $vanbones"
check "and it really carries a skeleton, so the checks below are not vacuous" \
	"$([ "$vanbones" -gt 0 ] && echo 1 || echo 0)"

if compileall "$RAG" "$W/rag.nif"; then
	gottypes=$(systemtypes "$W/rag.nif")
	gotbones=$(bonetable "$W/rag.nif" | wc -l)
	gotparents=$(parents "$W/rag.nif")
	gotjoints=$(jointcount "$W/rag.nif"); gotjoints="${gotjoints:-0}"
	vanjoints=$(jointcount "$RAG"); vanjoints="${vanjoints:-0}"
	echo "  rebuilt systems: $gottypes"
	echo "  rebuilt bones: $gotbones, joints: $gotjoints (vanilla $vanjoints)"
	check "Compile All writes a bhkRagdollSystem beside a bhkPhysicsSystem, as vanilla does" \
		"$([ "$gottypes" = "$vantypes" ] && echo 1 || echo 0)"
	check "the ragdoll comes back with vanilla's bone count" \
		"$([ "$gotbones" = "$vanbones" ] && echo 1 || echo 0)"
	check "and with all of its joints" \
		"$([ "$gotjoints" = "$vanjoints" ] && [ "$vanjoints" -gt 0 ] && echo 1 || echo 0)"
	# The bone ORDER is the joint order and the parent array is the proof: built
	# breadth-first instead it came out a permutation -- right generations, wrong
	# siblings -- and this is the check that caught it.
	check "the bone tree is vanilla's own, parent for parent" \
		"$([ -n "$vanparents" ] && [ "$gotparents" = "$vanparents" ] && echo 1 || echo 0)"
	[ "$gotparents" = "$vanparents" ] || { echo "    vanilla: $vanparents"; echo "    ours:    $gotparents"; }
else
	check "Compile All writes a bhkRagdollSystem beside a bhkPhysicsSystem, as vanilla does" 0
	check "the ragdoll comes back with vanilla's bone count" 0
	check "and with all of its joints" 0
	check "the bone tree is vanilla's own, parent for parent" 0
fi


# --- a ragdoll with MORE BODIES THAN BONES -----------------------------------
# TorsoProtectron is three bodies -- C-BotCore, Chest, CombatInhibitor -- and two
# bones: the joint binds the first two and nothing reaches the third. The bones
# are a PREFIX of the bodies, so the one the tree never reaches has to sort last.
# Assuming bones == bodies refused nine corpus ragdolls outright, all robots.
TORSO="${TORSO:-E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Actors/Robot/Parts/TorsoProtectron.nif}"
if [ -f "$TORSO" ]; then
	echo "fixture: $TORSO"
	# body index -> node name, in body order: what has to be reproduced
	bodyorder() { "$NS" -no-gui collision "$1" 2>/dev/null | sed -n '/^  body   node/,/^  hkaSkeleton/p' | awk '/^  [0-9]+ /{printf "%s=%s ", $1, $2}'; }
	tbones=$(bonetable "$TORSO" | wc -l)
	tbodies=$("$NS" -no-gui collision "$TORSO" 2>/dev/null | awk '/decoded/{for(i=1;i<=NF;i++) if($i ~ /^body\/bodies/) print $(i-1)}' | head -1)
	tvanorder=$(bodyorder "$TORSO")
	tvanparents=$(parents "$TORSO")
	echo "  vanilla: ${tbodies:-?} bodies, $tbones bones"
	check "the fixture really has more bodies than bones" \
		"$([ -n "$tbodies" ] && [ "$tbodies" -gt "$tbones" ] && [ "$tbones" -gt 0 ] && echo 1 || echo 0)"
	if compileall "$TORSO" "$W/torso.nif"; then
		gbones=$(bonetable "$W/torso.nif" | wc -l)
		gorder=$(bodyorder "$W/torso.nif")
		gparents=$(parents "$W/torso.nif")
		grag=$("$NS" -no-gui info "$W/torso.nif" 2>/dev/null | awk '/bhkRagdollSystem/{gsub("x","",$2); print $2}')
		echo "  rebuilt: $gbones bones, ${grag:-0} ragdoll system(s)"
		check "it still compiles back into a ragdoll" "$([ "${grag:-0}" = "1" ] && echo 1 || echo 0)"
		check "with vanilla's bone count, not one per body" \
			"$([ "$gbones" = "$tbones" ] && echo 1 || echo 0)"
		check "the body the bone tree never reaches still sorts last" \
			"$([ -n "$tvanorder" ] && [ "$gorder" = "$tvanorder" ] && echo 1 || echo 0)"
		check "and its bone tree is vanilla's own" \
			"$([ -n "$tvanparents" ] && [ "$gparents" = "$tvanparents" ] && echo 1 || echo 0)"
		[ "$gorder" = "$tvanorder" ] || { echo "    vanilla: $tvanorder"; echo "    ours:    $gorder"; }
	else
		check "it still compiles back into a ragdoll" 0
		check "with vanilla's bone count, not one per body" 0
		check "the body the bone tree never reaches still sorts last" 0
		check "and its bone tree is vanilla's own" 0
	fi
fi


# --- what a rebuilt ragdoll WEIGHS -------------------------------------------
# Found in game, not here: human ragdolls thrashed on death and weighed almost
# nothing to pick up, while every offline check of that file was green. Spheres
# and capsules never set their mass properties, so a body made of them summed to
# volume ZERO -- which collapsed its DENSITY onto its mass (18 where vanilla has
# 788) and its CENTRE OF MASS onto its own origin. The centre of mass then needed
# the body's own rotation applied, which the pelvis exposed by coming back with
# its offset's x and z exchanged.
#
# Nothing about shapes, bones or joints can see either one, which is why this
# compares the numbers a body is SIMULATED from.
HUMAN="${HUMAN:-E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Actors/Character/CharacterAssets/skeleton.nif}"
if [ -f "$HUMAN" ]; then
	echo "fixture: $HUMAN"
	# per ragdoll body: index, mass, density, centre of mass
	ragbodies() { "$NS" -no-gui collision "$1" --bodies 2>/dev/null \
		| awk '/^system/{on=($2=="bhkRagdollSystem")} on&&/^  body /{print $2, $4, $6, $NF}'; }
	vanb=$(ragbodies "$HUMAN")
	echo "  vanilla ragdoll bodies: $(echo "$vanb" | grep -c .)"
	# not vacuous: vanilla's density must be far from its mass, which is what the
	# broken build produced
	spread=$(echo "$vanb" | awk '{ if ($3 > $2 * 10) n++ } END { print n+0 }')
	check "vanilla's densities are nothing like its masses, so this can fail" \
		"$([ "${spread:-0}" -gt 0 ] && echo 1 || echo 0)"
	if compileall "$HUMAN" "$W/hum.nif"; then
		echo "$vanb" > "$W/van.bod"
		ragbodies "$W/hum.nif" > "$W/our.bod"
		# worst mass difference, worst relative density error, worst com error
		read -r wm wd wc <<-EOF
		$(join "$W/van.bod" "$W/our.bod" | awk '
			function abs(x) { return x < 0 ? -x : x }
			{
				split($4, a, ","); split($7, b, ",");
				dm = abs($2 - $5); if (dm > wm) wm = dm;
				dd = abs($3 - $6) / ($3 > 0 ? $3 : 1); if (dd > wd) wd = dd;
				for (i = 1; i <= 3; i++) { dc = abs(a[i] - b[i]); if (dc > wc) wc = dc }
			}
			END { printf "%g %g %g", wm+0, wd+0, wc+0 }')
		EOF
		echo "  worst mass diff $wm kg, density rel $wd, centre of mass $wc m"
		check "every body comes back with vanilla's mass" \
			"$(awk -v v="$wm" 'BEGIN{print (v < 1e-4) ? 1 : 0}')"
		check "...and vanilla's density, so it is not weightless in game" \
			"$(awk -v v="$wd" 'BEGIN{print (v < 1e-4) ? 1 : 0}')"
		check "...and vanilla's centre of mass, in the right frame" \
			"$(awk -v v="$wc" 'BEGIN{print (v < 1e-4) ? 1 : 0}')"
	else
		check "every body comes back with vanilla's mass" 0
		check "...and vanilla's density, so it is not weightless in game" 0
		check "...and vanilla's centre of mass, in the right frame" 0
	fi
fi


# --- a TRIGGER stays a trigger ----------------------------------------------
# hknpMaterial carries its own flags word and trigger type, and Compile wrote a
# ZEROED material for every body it rebuilt -- so the character bumper came back
# as ordinary solid collision. A trigger that turns solid is a body the world
# starts colliding with. Derived rather than carried: over 1,363 sampled corpus
# bodies, RAISE_TRIGGER_EVENTS set means flags 0x200000 and trigger type 2, clear
# means both zero, no exceptions.
matsig() { "$NS" -no-gui collision "$1" --bodies 2>/dev/null 	| awk '/^system/{s=$2} /^  body /{for(i=1;i<=NF;i++){if($i=="matFlags")m=$(i+1); if($i=="trigger")t=$(i+1)} print s":"m":"t}' 	| sort | uniq -c | tr -s " " | tr "
" " "; }
for fx in "$HUMAN" "$RAG"; do
	[ -f "$fx" ] || continue
	vsig=$(matsig "$fx")
	# not vacuous: the fixture must actually contain a trigger material
	check "$(basename "$fx") really carries a trigger material" 		"$(case "$vsig" in *0x200000:2*) echo 1;; *) echo 0;; esac)"
	if compileall "$fx" "$W/mat.nif"; then
		gsig=$(matsig "$W/mat.nif")
		check "...and it is still a trigger after Compile All" 			"$([ "$gsig" = "$vsig" ] && echo 1 || echo 0)"
		[ "$gsig" = "$vsig" ] || { echo "    vanilla: $vsig"; echo "    ours:    $gsig"; }
	else
		check "...and it is still a trigger after Compile All" 0
	fi
done

echo "$checks checks, $fails failures"
[ "$fails" = "0" ] && echo PASS || echo FAIL
exit "$fails"
