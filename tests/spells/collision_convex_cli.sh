#!/bin/bash
#
# `convex` and `settle`, headless: a loose part gets dynamic convex collision
# the way a vanilla loose item carries it, and it comes to rest on a floor.
#
# WHY THIS EXISTS
#
# Lane MAGDROP1 turns weapon magazines into physics objects that drop on a quick
# reload. Create Convex Shapes needs a GL scene, so it cannot run in a batch; the
# `convex` command is its batch twin (drop the old collision, recentre the root on
# the hull's centre of mass, one hull, a dynamic body, Compile). Two defects came
# out of the first run and are held here:
#
#   * A FRESH hull whose vertex count is not a multiple of four was refused by
#     the polytope writer ("No encoder for the shape"), and Compile silently fell
#     back to a triangle mesh on a dynamic body. Decoded vanilla polytopes always
#     arrive padded, which is why the corpus never hit it.
#   * A fresh body's inertia tensor was zero, so the engine got invInertia 1,1,1.
#     Vanilla loose items carry 1.5 x the polytope's stored inertia scaled to the
#     body's mass (GaussRifleAmmo: 1.498 / 1.498 / 1.500).
#
# WHAT IS MEASURED (fixture: the vanilla combat rifle small magazine, whose hull is not a
# multiple of four, and GaussRifleAmmo, a vanilla loose magazine)
#
#   1. the fixture's fresh hull really has a count that is not a multiple of 4
#   2. convex writes hknpConvexPolytopeShape, NOT a compressed mesh  <- defect 1
#   3. layer 4 (clutter), the material asked for, mass asked for
#   4. the body's inverse inertia is not 1,1,1                        <- defect 2
#   5. ...and body / shape inertia is 1.5 x mass/volume, as on GaussRifleAmmo
#   6. ...and that ratio really is 1.5 on GaussRifleAmmo (not vacuous)
#   7. the body flags carry RAISE_CONTACT_IMPULSE_EVENTS (0x80), as vanilla's do
#   8. the root sits on the centre of mass (|com| < 0.001 m)
#   9. the packfile re-encodes byte-exact (--roundtrip)
#  10. settle: it comes to rest, no sinking, no jitter, no rocking
#  11. settle refuses a jointed system (a ragdoll) instead of guessing
#
# USAGE
#   bash tests/spells/collision_convex_cli.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
D="E:/Tools/Fallout 4/DataUnpacked/Data/Meshes"
SRC="${SRC:-$D/Weapons/CombatShotgun/CombatRifleMagSmall.nif}"
GAUSS="${GAUSS:-$D/Ammo/GaussRifleAmmo/GaussRifleAmmo.nif}"
RAG="${RAG:-$D/Actors/Brahmin/CharacterAssets/Skeleton.nif}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT
[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$SRC" ] && [ -f "$GAUSS" ] || { echo "no fixture"; exit 2; }

checks=0; fails=0
check() { checks=$((checks+1)); if [ "$2" = "1" ]; then echo "  ok   $1"; else echo "  FAIL $1"; fails=$((fails+1)); fi; }
cli() { "$NS" -no-gui "$@" 2>&1; }

log=$(cli convex "$SRC" -o "$W/m.nif" --mass 0.5 --layer 4 --material 0xE538F7DB --radius 0.01 --center com)
echo "$log" | sed 's/^/    /'
hull=$(echo "$log" | sed -n 's/^convex verts [0-9]* hull \([0-9]*\).*/\1/p')
check "the fresh hull's vertex count ($hull) is not a multiple of 4" "$([ -n "$hull" ] && [ $((hull % 4)) -ne 0 ] && echo 1 || echo 0)"

row=$(cli collision "$W/m.nif" | awk '/shape  class/{getline; print}')
echo "    shape: $row"
check "convex writes a polytope, not a triangle mesh" "$(echo "$row" | grep -q hknpConvexPolytopeShape && echo 1 || echo 0)"
check "...with the material asked for" "$(echo "$row" | grep -qi 0XE538F7DB && echo 1 || echo 0)"

body=$(cli collision "$W/m.nif" --bodies | grep "body 0")
echo "    body: $body"
check "layer 4 and mass 0.5" "$(echo "$body" | grep -q "layer 4" && echo "$body" | grep -q "mass 0.5 " && echo 1 || echo 0)"
check "the inverse inertia is not 1,1,1" "$(echo "$body" | grep -q "invInertia 1,1,1 " && echo 0 || echo 1)"
check "the body raises contact-impulse events (flags 0x80), as vanilla's does" "$(echo "$body" | grep -q "flags 0x80" && echo 1 || echo 0)"

# body inertia / (shape inertia * mass / shape mass), sorted, so the axis frame does not matter
ratio() {   # $1 nif
	"$NS" -no-gui collision "$1" --bodies 2>/dev/null | grep "body 0" > "$W/b.txt"
	"$NS" -no-gui collision "$1" 2>/dev/null | awk '/shape  class/{getline; print}' > "$W/s.txt"
	python - "$W/b.txt" "$W/s.txt" <<'EOF'
import re, sys
b = open(sys.argv[1]).read(); s = open(sys.argv[2]).read()
m = float(re.search(r'mass ([0-9.eE+-]+)', b).group(1))
inv = [float(x) for x in re.search(r'invInertia ([^ ]+)', b).group(1).split(',')]
sm = float(re.search(r'mass ([0-9.eE+-]+) com', s).group(1))
si = [float(x) for x in re.search(r' I ([^ ]+)', s).group(1).split(',')]
body = sorted(1.0 / x for x in inv); shape = sorted(x * m / sm for x in si)
print(' '.join('%.3f' % (a / c) for a, c in zip(body, shape)))
EOF
}
ours=$(ratio "$W/m.nif"); van=$(ratio "$GAUSS")
echo "    body/shape inertia ratio: ours $ours, GaussRifleAmmo $van"
near15() { python -c "import sys; v=[float(x) for x in sys.argv[1:]]; print(1 if v and all(abs(x-1.5)<0.02 for x in v) else 0)" $1; }
check "body / shape inertia is 1.5 x mass/volume" "$(near15 "$ours")"
check "...as it is on GaussRifleAmmo (not vacuous)" "$(near15 "$van")"

com=$(echo "$body" | sed -n 's/.* com \([^ ]*\).*/\1/p')
check "the root sits on the centre of mass ($com)" "$(python -c "import sys; print(1 if max(abs(float(x)) for x in sys.argv[1].split(','))<1e-3 else 0)" "$com")"
rt=$(cli collision "$W/m.nif" --roundtrip | grep -o "byte-exact [0-9]* / [0-9]*")
echo "$rt" | sed 's/^/    /'
check "the packfile re-encodes byte-exact" "$(echo "$rt" | awk 'NF{n++; if($2!=$4||$2==0)bad=1} END{print (n>0&&!bad)?1:0}')"

st=$(cli settle "$W/m.nif")
echo "    $st"
check "settle: it comes to rest, no sinking, no jitter" "$(echo "$st" | grep -q "^settle PASS" && echo 1 || echo 0)"
if [ -f "$RAG" ]; then
	check "settle refuses a jointed system (a ragdoll)" "$(cli settle "$RAG" | grep -q "^settle FAIL expected one loose body" && echo 1 || echo 0)"
fi

echo "$checks checks, $fails failures"
if [ "$fails" = "0" ]; then echo PASS; exit 0; else echo FAIL; exit 1; fi
