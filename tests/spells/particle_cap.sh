#!/bin/bash
#
# A particle system simulates the number of particles its FILE authorises.
#
# WHY THIS EXISTS
#
# PSysSimController asked the NiParticleSystem block for "Num Vertices". No
# version of that block has ever had one: the count is a NiGeometryData row and
# lives on the DATA block, and on Bethesda 20.2 nif.xml renames it "BS Max
# Vertices" for NiPSysData because a particle system's vertex arrays have no
# length there. The read therefore returned 0 on every file in every game, and
# the 512 fallback beneath it always won.
#
# Measured over the stock Fallout 4 mesh tree, 1,345 NiPSysData blocks:
#
#     exactly 512      0
#     fewer than 512   1,287
#     more than 512    58
#     smallest         3
#     largest          38,464
#
# So no file in the game got the count it asked for. A smoke plume authored for
# 17 particles previewed with up to 512 - not a busier version of the same
# effect, a different one.
#
# WHAT IS MEASURED
#
#   1. the fixture has a particle system                     <- not vacuous
#   2. it authorises fewer than the 512 fallback             <- not vacuous
#   3. the systems are actually in the scene
#   4. none simulates more particles than the file allows
#   5. at least one SATURATES its cap
#
# Check 5 is what makes check 4 mean anything. "Live <= cap" passes on the
# broken code whenever the emitter happens to be quiet; sitting exactly ON the
# cap says the emitter wanted more and was held back by the authored number.
#
# NOTE ON PORTS: NifSkope EXITS SILENTLY if it cannot bind its IPC port, and UDP
# above ~49152 is refused on this machine. Keep the number below that.
#
# USAGE
#   bash tests/spells/particle_cap.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
# ShockHAndLeft has two systems authored for 12 particles whose emitters both
# reach 12 within three seconds - the saturation check 5 needs. CryoJet01 and
# most of the effects tree do NOT saturate in that time, so they cannot tell the
# fixed code from the broken code.
SRC="${SRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Actors/CreateABot/CharacterAssets/ShockHAndLeft.nif}"
LOG="$ROOT/release/ww_particlecap_test.log"
PORT="${PORT:-42291}"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }

rm -f "$LOG"
WW_PARTICLECAP_TEST=1 "$NS" --port "$PORT" "$SRC" >/dev/null 2>&1

[ -f "$LOG" ] || { echo "FAIL: no log — did the app exit before the harness ran?"; exit 1; }
cat "$LOG"
grep -q "^PASS" "$LOG" && exit 0
exit 1
