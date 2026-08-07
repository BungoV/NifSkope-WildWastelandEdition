#!/bin/bash
# Launch the list-mode rename harness; if it is still alive after 25 s it has
# hung, so take a stack of every thread before the script's deadline kills it.
cd /e/Projects/NifskopeWildWastelandEdition
SP=/c/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/c80dd4eb-7ceb-431a-a539-4e2af763a7c8/scratchpad
out="$SP/stack.txt"
: > "$out"
for attempt in 1 2 3 4; do
	MODES=list bash tests/spells/block_rename.sh >/dev/null 2>&1 &
	runner=$!
	sleep 25
	pid=$(powershell.exe -NoProfile -NonInteractive -Command "(Get-Process NifSkope -EA SilentlyContinue | Select-Object -First 1).Id" 2>/dev/null | tr -d '\r ')
	if [ -n "$pid" ]; then
		echo "=== attempt $attempt: NifSkope $pid still alive at 25 s, taking a stack" >> "$out"
		/c/msys64/ucrt64/bin/gdb.exe -p "$pid" -batch \
			-ex "set pagination off" -ex "thread apply all bt 25" >> "$out" 2>&1
		wait "$runner" 2>/dev/null
		echo "=== done" >> "$out"
		exit 0
	fi
	echo "attempt $attempt: finished before 25 s, retrying" >> "$out"
	wait "$runner" 2>/dev/null
done
echo "never hung in 4 attempts" >> "$out"
