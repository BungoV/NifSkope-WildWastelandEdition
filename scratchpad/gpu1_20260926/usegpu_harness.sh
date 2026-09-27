#!/bin/bash
# GPU1: GUI harness for the Settings > NIF > LOD bake > Use GPU row (WW_USEGPU_TEST in src/nifskope_ui.cpp).
# Own settings scope with the key PLANTED off (never the user's key); second monitor; unused port;
# the machine-wide NifSkope turn around the run. Pictures + log land beside the run-folder exe, then copied out.
# usage: usegpu_harness.sh <run dir holding NifSkope.exe> <out dir>
RUN="$1"; OUT="$2"
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
NIF=E:/Projects/NifskopeWWE-gpu1/tests/render/refraction_fixture.nif
SCOPE=gpu1ui
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
mkdir -p "$OUT"
rm -f "$RUN/ww_usegpu_test.log" "$RUN/ww_usegpu_on.png" "$RUN/ww_usegpu_off.png"
wipe() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
wipe
# Settings/Version first: without it SettingsDialog treats the scope as a first install and saves every
# pane's default over the plant (run 1 of this harness read "on" because of that).
reg add "$REGKEY\\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null || { echo "plant failed"; exit 1; }
reg add "$REGKEY\\Settings\\Nif" //v "Use GPU" //t REG_SZ //d false //f > /dev/null || { echo "plant failed"; exit 1; }
bash $TURN acquire gpu1 || exit 1
for i in $(seq 1 20); do
	WW_SETTINGS_SCOPE=$SCOPE WW_USEGPU_TEST=1 WW_WINDOW_AT=1960,40 timeout 300 "$RUN/NifSkope.exe" --port 45391 "$NIF" > "$OUT/harness_stdout.log" 2>&1
	rc=$?; echo "$(date +%H:%M:%S) rc=$rc"
	[ $rc -ne 126 ] && break
	sleep 30
done
bash $TURN release gpu1
wipe
cat "$RUN/ww_usegpu_test.log"
# the forced state must have been read: the plant is OFF, so the bake must read off before the dialog
if grep -q "bake reads before the dialog = off" "$RUN/ww_usegpu_test.log" && grep -q "^PASS" "$RUN/ww_usegpu_test.log"; then
	echo "HARNESS PASS (planted off was read, row round-trips)"
else
	echo "HARNESS FAIL"
fi
cp "$RUN/ww_usegpu_test.log" "$RUN"/ww_usegpu_*.png "$OUT/" 2>/dev/null
ls "$OUT"
