#!/bin/bash
# In-application gate of the glTF export options dialog.
# One NifSkope at a time; a harness instance ALWAYS carries --port <unused>
# so it can never be mistaken for bungo's own window.
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
LOG=release/ww_gltf_export_dialog.log
rm -f "$LOG"
WW_GLTF_EXPORT_DIALOG=1 WW_WINDOW_AT=1960,40 \
	./release/NifSkope.exe --port 27731 > /dev/null 2>&1 &
pid=$!
for i in $(seq 1 60); do
	if [ -f "$LOG" ] && grep -q "^done" "$LOG"; then break; fi
	sleep 1
done
sleep 1
kill $pid 2>/dev/null
wait $pid 2>/dev/null
cat "$LOG"
