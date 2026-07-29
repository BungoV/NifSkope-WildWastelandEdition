#!/bin/bash
#
# "Load skeleton" (nifMergeFile) across categories -- clothing, armour, bodies,
# faceBones, creatures, unskinned props -- not just human bodies.
#
# WHY THIS EXISTS
#
# The only automated coverage the merge had was WW_MERGEARCH_TEST, which merges
# a NIF into ITSELF to compare the file and in-memory paths. A file shares its
# own header string table, so that test could never see the bug this sweep was
# written for: from NIF 20.1.0.3 on, names are an INDEX into the file's own
# string table, and a block spliced from a DIFFERENT file carried its index
# verbatim. Merging the human skeleton into an outfit produced 32 duplicated
# bone names, a "LLeg_Toe1" parented under a forearm, and a rig that folded up
# the moment it was posed.
#
# THE INVARIANT
#
# The merged file's NiNode names must be exactly the union of the two inputs',
# with no name twice. The donor's ROOT is excluded: it is a per-file wrapper
# ("skeleton.nif") that the merge deliberately does not import. The target's
# skinned shape must also still bind to the same bone names it did before.
#
# USAGE
#   bash tests/merge/skeleton_merge_sweep.sh [path-to-Data/meshes]
# Defaults to the FO4 corpus. Needs release/NifSkope.exe built.

set -u
EXE="${EXE:-$(cd "$(dirname "$0")/../.." && pwd)/release/NifSkope.exe}"
D="${1:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes}"
OUT="${TMPDIR:-/tmp}/skeleton_merge_sweep"
mkdir -p "$OUT"
pass=0; fail=0; skip=0

nodenames() { timeout 180 "$EXE" -no-gui list "$1" 2>/dev/null | sed -n "s/^\[[0-9]*\] NiNode '\(.*\)'$/\1/p"; }
rootname()  { timeout 180 "$EXE" -no-gui skeleton "$1" 2>/dev/null | sed -n 's/^root *\(.*\) \[[0-9]*\]$/\1/p'; }
bonenames() { timeout 180 "$EXE" -no-gui skeleton "$1" 2>/dev/null | sed -n 's/^ *\([A-Za-z0-9_]*\) *[0-9]* *[0-9]* *[0-9.]*$/\1/p' | sort -u; }

check() {
  local label="$1" target="$2" donor="$3"
  if [ ! -f "$target" ] || [ ! -f "$donor" ]; then
    echo "  skip $label - missing input"; skip=$((skip+1)); return
  fi
  local merged="$OUT/$(echo "$label" | tr ' /' '__').nif"
  local before donorn expect after got dupes bonesbefore bonesafter log ok why
  before=$(nodenames "$target")
  donorn=$(nodenames "$donor" | grep -vxF "$(rootname "$donor")")
  bonesbefore=$(bonenames "$target")
  log=$(timeout 300 "$EXE" -no-gui merge "$target" --add "$donor" -o "$merged" 2>&1)
  if [ ! -f "$merged" ]; then
    echo "  FAIL $label - merge produced nothing: $(echo "$log" | tail -1)"; fail=$((fail+1)); return
  fi
  after=$(nodenames "$merged")
  expect=$(printf '%s\n%s\n' "$before" "$donorn" | sort -u)
  got=$(printf '%s\n' "$after" | sort -u)
  dupes=$(printf '%s\n' "$after" | sort | uniq -d)
  bonesafter=$(bonenames "$merged")
  ok=1; why=""
  if [ "$got" != "$expect" ]; then
    ok=0
    why="node names differ from the union: missing [$(comm -23 <(printf '%s\n' "$expect") <(printf '%s\n' "$got") | tr '\n' ' ')] extra [$(comm -13 <(printf '%s\n' "$expect") <(printf '%s\n' "$got") | tr '\n' ' ')]"
  fi
  [ -n "$dupes" ] && { ok=0; why="$why; duplicate names: $(echo "$dupes" | tr '\n' ' ')"; }
  if [ -n "$bonesbefore" ] && [ "$bonesbefore" != "$bonesafter" ]; then
    ok=0; why="$why; the shape's bone bindings changed"
  fi
  # the CLI's own report must agree with what this script measured
  echo "$log" | grep -q "no duplicate bone names introduced" || {
    [ -z "$dupes" ] && { ok=0; why="$why; CLI reported duplicates the file does not have"; }
  }
  local nb nd na
  nb=$(printf '%s\n' "$before" | grep -c .); nd=$(printf '%s\n' "$donorn" | grep -c .)
  na=$(printf '%s\n' "$after" | grep -c .)
  if [ $ok -eq 1 ]; then
    echo "  ok   $label  ($nb + $nd -> $na nodes)"; pass=$((pass+1))
  else
    echo "  FAIL $label  ($nb + $nd -> $na nodes) - $why"; fail=$((fail+1))
  fi
}

HUMAN="$D/Actors/Character/CharacterAssets/skeleton.nif"

echo "== clothing =="
check "clothing InstituteWorksuit"  "$D/Clothes/InstituteWorksuit/FOutfit.nif" "$HUMAN"
check "clothing Vault111 1stperson" "$D/Actors/Character/CharacterAssets/1stPersonFemaleVault111Suit.nif" "$HUMAN"
echo "== armour =="
check "armour CombatArmor torso"    "$D/Armor/CombatArmor/F_Torso_Mid.nif" "$HUMAN"
check "armour CombatArmor arm"      "$D/Armor/CombatArmor/F_Arm_Heavy_L.nif" "$HUMAN"
echo "== body / head =="
check "head BaseFemaleHead"         "$D/Actors/Character/CharacterAssets/BaseFemaleHead.nif" "$HUMAN"
check "faceBones head"              "$D/Actors/Character/CharacterAssets/BaseFemaleHead_faceBones.nif" "$D/Actors/Character/CharacterAssets/skeleton_female_faceBones.nif"
echo "== creatures =="
check "deathclaw + human"           "$D/Actors/Deathclaw/CharacterAssets/skeleton.nif" "$HUMAN"
check "dogmeat + human"             "$D/Actors/Dogmeat/CharacterAssets/skeleton.nif" "$HUMAN"
echo "== unskinned props =="
check "weapon 10mmPistol"           "$D/Weapons/10mmPistol/10mmPistol.nif" "$HUMAN"

echo
echo "$pass ok, $fail failures, $skip skipped"
[ $fail -eq 0 ]
