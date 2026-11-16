#!/bin/bash
# lane WEATHERRECX3: the whole offline gate.  bash run.sh <work dir (not C:)> [original] [regenerated] [shipped]
#  original    = the 1.4 files before WEATHERRECX2 (its backup)
#  regenerated = WEATHERRECX2's 1.8 set (the backup taken before this lane wrote; also the groundFog set both read)
#  shipped     = the live Fusion Shaders - Assets Weathers folder (after this lane wrote it: must PASS like migrated)
#  1. dump_old.exe = origin/modules1-20261105's reader (git archive), dump_new.exe = this tree's
#  2. migrate.py original -> <work>/migrated (vanillaRecord carried from the regenerated set); reds.py -> broken sets
#  3. gate.py: migrated and shipped PASS; regenerated, red_value, red_unit, red_ref FAIL;
#     control (no file at all): the generated rung alone differs (what the 1.8 reader gives a weather with no file)
set -u
H="$(cd "$(dirname "$0")" && pwd)"
TREE="$(cd "$H/../../../.." && pwd)"
WK="$1"
# BATCH1121: the trash copies are gone; the fixtures were rebuilt (notes/batch1121/fixtures/weatherrecx3: before = 220 of 220
# files byte-equal to notes/weatherrecx3/before_sha256.txt; ship = weathergen.py @5c5e0e25, value-equal to WEATHERRECX2's diff)
ORIG="${2:-E:/Projects/Claude/notes/batch1121/fixtures/weatherrecx3/weatherrecx2_ship}"
REGEN="${3:-E:/Projects/Claude/notes/batch1121/fixtures/weatherrecx3/weatherrecx3_before/Weathers}"
SHIP="${4:-E:/Projects/Fallout 4 Mods/mods/Fusion Shaders - Assets/Plugins/Fusion Shaders - Assets/Weathers}"
LIST=E:/Projects/Claude/notes/weatherrecx1/wthr_list.json
mkdir -p "$WK/old" "$WK/noflat"
rm -rf "$WK/old/src"
git -C "$TREE" archive origin/modules1-20261105 src | tar -x -C "$WK/old" || exit 1
bash "$H/build.sh" "$WK/old" "$WK/dump_old.exe" -DWX3_OLD || exit 1
bash "$H/build.sh" "$TREE" "$WK/dump_new.exe" || exit 1
rm -rf "$WK/migrated"
python -I "$H/migrate.py" "$ORIG" "$WK/migrated" --record-from "$REGEN" || exit 1
python -I "$H/reds.py" "$ORIG" "$WK/migrated" "$WK/reds" || exit 1
echo "== gate (migrated, shipped PASS; regen, red_value, red_unit FAIL)"
python -I "$H/gate.py" "$WK/dump_old.exe" "$WK/dump_new.exe" $LIST "$ORIG" "$REGEN" "$WK/work" \
  migrated="$WK/migrated" shipped="$SHIP" regen="$REGEN" red_value="$WK/reds/red_value" red_unit="$WK/reds/red_unit"
echo "== red_ref (reference = a broken ORIGINAL; the true migrated set must FAIL)"
python -I "$H/gate.py" "$WK/dump_old.exe" "$WK/dump_new.exe" $LIST "$WK/reds/red_ref" "$REGEN" "$WK/work_ref" \
  migrated="$WK/migrated"
echo "== WEATHERRECX4 wired-key reds (one key set in CommonwealthClear: each must FAIL on that key and its use.* term)"
WX4=""
for n in $(cat "$WK/reds/wx4_reds.txt"); do WX4="$WX4 $n=$WK/reds/$n"; done
python -I "$H/gate.py" "$WK/dump_old.exe" "$WK/dump_new.exe" $LIST "$ORIG" "$REGEN" "$WK/work_wx4" $WX4
echo "== control (no file at all, no groundFog set: the generated rung alone)"
python -I "$H/gate.py" --plan "$ORIG" "$WK/dump_old.exe" "$WK/dump_new.exe" $LIST "$WK/reds/empty" "$WK/noflat" "$WK/work_ctl" \
  nofile="$WK/reds/empty"
