#!/usr/bin/env bash
# Does a harness force its own window, and leave the user's alone?
#
# ============================ NOT RUN ========================================
# Lane HARNESSWIN1 wrote this on 2026-09-19 as a CODE-ONLY lane: lane
# IMPOSTORSHOW owned the build slot, so nothing here has ever been executed and
# the code it gates has never been compiled. Every number below is a
# PRE-REGISTRATION, not a result, and this script's own defects are still in it
# (ww-test-harness-add 9). The check-count floor at the bottom is 0 on purpose
# and is written back from the first green run, measured, never predicted
# (ww-test-harness-add 5c).
# =============================================================================
#
# THE BUG THIS EXISTS FOR
#
# tests/spells/native_open.sh row (c) -- "the .lodi scene draws everything the
# .BTO draws (covered 0.8978 >= 0.90)" -- sat RED, and it was never a rendering
# defect. Lane HORIZONOUT ran the same script minutes apart on the shipped exe
# and on the exe from before its own lane and got the same number to four
# decimals, so no code change moved it. What moved was the WINDOW:
#
#     2026-09-18 22:38   vp=1024x989   upp=16.000000   COVER 0.9331
#     2026-09-19         vp=1822x989   upp= 8.992316   COVER 0.8978
#
# HKCU\Software\NifTools\NifSkope 2.0\UI\Window Geometry decoded to frame
# 1920x1048 on screen 1, MAXIMIZED, normal 1280x800 -- bungo's last window.
# restoreUi() replayed it into every harness run, and lane IMPOSTORSHOW then
# named the mechanism exactly (src/impostorpreviewtest.cpp, forceWindow()):
# showNormal() ALONE DOES NOT CLEAR a WindowMaximized bit that arrived with a
# restored geometry, and a resize() applied to a still-maximized window is
# discarded WITHOUT A WORD -- "asked 1024x1024, got 1822x989".
#
# So eleven spells set WW_RENDER_SIZE and not one of them could tell whether it
# got the size it asked for. The harness measured the machine.
#
# WHAT IS BEING GATED
#
#   1. restoreUi() does not replay UI/Window Geometry on a harness run, and the
#      state bits are cleared by hand as well.
#   2. The size is explicit and the window comes out at it.
#   3. The size obtained is written down, and a floor REFUSES with both numbers
#      instead of measuring. This is the part that matters: the floor above was
#      silent for a day.
#
# WHY THIS ONE DOES NOT BORROW THE USER'S SETTINGS
#
# tests/spells/window_state_roundtrip.sh has to plant into the real key and put
# it back, and its own comments record two occasions when the restore failed and
# test-only values were left in bungo's profile. This gate instead sets
# WW_SETTINGS_SCOPE, which moves the WHOLE QSettings tree to
# HKCU\Software\NifTools\NifSkope 2.0 <scope>. Nothing is borrowed, so nothing
# has to be given back -- and row (b) proves his key is byte-identical anyway,
# because "by construction" is a claim and an export diff is a measurement.
#
# Run:  bash tests/spells/harness_window.sh
set -u

. "$(dirname "$0")/_harness.sh"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
# The rung: the exe from BEFORE the repair. Row (c) needs it and SKIPS by name
# without it -- a red control that cannot run is never a pass.
BEFORE="${BEFORE:-$ROOT/release/NifSkope.before_harnesswin1.exe}"
PORT="${PORT:-42713}"
SCOPE="${SCOPE:-harnesswin1gate}"
LOG="$ROOT/release/ww_harness_window.log"
WINLOG="$ROOT/release/ww_headless_windows.log"

[ -x "$EXE" ] || { echo "FAIL: no binary at $EXE"; exit 1; }

W="$(mktemp -d "$ROOT/scratchpad/harnesswin1_gate.XXXXXX")"
trap 'rm -rf "$W"' EXIT

checks=0; fails=0; skips=0
check() { checks=$((checks+1)); if [ "$2" = "1" ]; then echo "  ok   $1"; else echo "  FAIL $1"; fails=$((fails+1)); fi; }
skip()  { skips=$((skips+1)); echo "  SKIP $1"; }

USERKEY='HKCU\Software\NifTools\NifSkope 2.0'
GATEKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"

ps() { powershell.exe -NoProfile -NonInteractive -Command "$1"; }

# A fixture the exe can open. Any NIF will do -- the SCENE is not what is
# measured here, only the window around it. Taken from IN THE TREE, so the gate
# does not depend on bungo's unpacked Fallout data being present
# (ww-test-harness-add 6). tests/fixtures/ holds no .nif, which is what the
# first draft assumed and is why this fell over before its first run.
FIX="${FIX:-$ROOT/tests/render/refraction_fixture.nif}"
if [ ! -f "$FIX" ]; then
	FIX="$(find "$ROOT/tests" -name '*.nif' 2>/dev/null | head -1)"
fi

# ---------------------------------------------------------------------------
# plant <maximized 0|1>  -- write a Window Geometry blob into the GATE key only.
#
# Qt QWidget::saveGeometry: magic 0-3, major 4-5, minor 6-7, frame(x1,y1,x2,y2)
# 8-23, normal(x1,y1,x2,y2) 24-39, screenNumber 40-43, maximized 44,
# fullScreen 45. QSettings stores the QByteArray as the UTF-16 string
# "@ByteArray(<raw>)", so payload byte i sits at blob offset 22 + 2i -- the same
# arithmetic window_state_roundtrip.sh uses, including its [int] cast trap
# (PowerShell's -shl on a BYTE shifts within the byte's own width).
#
# The template is READ from the user's key and NEVER written back.
# ---------------------------------------------------------------------------
cat > "$W/plant.ps1" <<'PSEOF'
$ErrorActionPreference = "Stop"
$src  = "HKCU:\Software\NifTools\NifSkope 2.0\UI"
$dst  = "HKCU:\Software\NifTools\NifSkope 2.0 $($env:HW_SCOPE)\UI"
$max  = [int]$env:HW_MAX
$g = (Get-ItemProperty $src -EA SilentlyContinue)."Window Geometry"
if (-not $g) { Write-Output "NOTEMPLATE"; exit 0 }
function GetI([byte[]]$b, [int]$i) {
  ([int]$b[22+2*$i] -shl 24) -bor ([int]$b[22+2*($i+1)] -shl 16) `
    -bor ([int]$b[22+2*($i+2)] -shl 8) -bor [int]$b[22+2*($i+3)]
}
function SetI([byte[]]$b, [int]$i, [int]$v) {
  $b[22+2*$i]     = ($v -shr 24) -band 0xff; $b[22+2*($i+1)] = ($v -shr 16) -band 0xff
  $b[22+2*($i+2)] = ($v -shr 8)  -band 0xff; $b[22+2*($i+3)] = $v -band 0xff
}
if ((GetI $g 0) -ne 0x01D9D0CB) { Write-Output "BADMAGIC"; exit 0 }
# the second monitor, full screen, so a maximized blob is a plausible one
SetI $g  8 1920; SetI $g 12 0; SetI $g 16 3839; SetI $g 20 1079
SetI $g 24 1960; SetI $g 28 40; SetI $g 32 2983; SetI $g 36 839
SetI $g 40 1
$g[22+2*44] = $max        # maximized
$g[22+2*45] = 0           # fullScreen
New-Item -Path $dst -Force | Out-Null
Set-ItemProperty -Path $dst -Name "Window Geometry" -Value $g -Type Binary
Write-Output "PLANTED max=$max"
PSEOF

plant() {
	HW_SCOPE="$SCOPE" HW_MAX="$1" powershell.exe -NoProfile -NonInteractive \
		-ExecutionPolicy Bypass -File "$(cygpath -w "$W/plant.ps1")" 2>&1 | tr -d '\r'
}

export_key() {  # export_key <reg path> <out file>
	# //y, NOT /y. MSYS2 rewrites a lone-slash argument as a path before reg.exe
	# sees it, and reg.exe answers "ERROR: Invalid syntax." on stderr with rc=1 --
	# which, redirected to /dev/null, is a silent no-export. Every comparison
	# built on it then compares two absent files and passes. Measured directly:
	# /y -> rc=1 and no file; //y -> rc=0 and an 809612-byte export.
	reg.exe export "$1" "$(cygpath -w "$2")" //y >/dev/null 2>&1
	test -s "$2"
}

# The PLANTED VALUE, on its own, as hex.
#
# Not a whole-key export: a run legitimately writes other things into its own
# scope (recent files, a splitter, the theme), so comparing the exported KEY
# before and after asks "did the run write anything at all", which is not the
# question and would go red on a correct build. The question is whether the
# geometry this gate planted came back unchanged. bungo's key is a different
# matter and IS compared whole, because there the answer must be "nothing".
cat > "$W/readgeom.ps1" <<'PSEOF'
$ErrorActionPreference = "SilentlyContinue"
$k = "HKCU:\Software\NifTools\NifSkope 2.0 $($env:HW_SCOPE)\UI"
$g = (Get-ItemProperty $k)."Window Geometry"
if (-not $g) { Write-Output "ABSENT" } else { Write-Output ([BitConverter]::ToString($g)) }
PSEOF

readgeom() {
	HW_SCOPE="$SCOPE" powershell.exe -NoProfile -NonInteractive \
		-ExecutionPolicy Bypass -File "$(cygpath -w "$W/readgeom.ps1")" 2>&1 | tr -d '\r'
}

run_exe() {  # run_exe <exe> <extra env...>  -- always in the GATE scope
	local exe="$1"; shift
	rm -f "$LOG"
	env "$@" \
		WW_SETTINGS_SCOPE="$SCOPE" \
		WW_WINDOW_AT="${WW_WINDOW_AT:-1960,40}" \
		WW_RENDER_SHOT="$(winpath "$W/shot.png")" \
		timeout 180 "$exe" --port "$PORT" "$(winpath "$FIX")" >/dev/null 2>&1
}

# THE LAST LINE, not the first. The recorder installs a resize watcher and
# re-records on every resize and state change, so the first line is the window
# as it was shown and the LAST is the window as it settled -- which is the one
# the picture was taken at. Reading -m1 here reported window=1020x480 for a run
# whose grab happened at 1024x480.
logline() { [ -f "$LOG" ] && grep '^harness-window ' "$LOG" | tail -1 || echo ""; }
field()   { echo "$1" | sed -n "s/.* $2=\([^ ]*\).*/\1/p"; }

echo "harness_window: a harness forces its own window (NOT RUN -- see the head comment)"
echo "  exe    $EXE"
echo "  scope  $GATEKEY"
[ -n "${FIX:-}" ] && [ -f "$FIX" ] || { echo "FAIL: no fixture NIF"; exit 1; }

# ===========================================================================
echo
echo "=== (a) a planted MAXIMIZED geometry does not reach the window ==========="
# What it refutes: restoreUi() replaying the maximized bit, and showNormal()
# failing to clear it. Both end in the same visible symptom -- the window is
# maximized and the resize was swallowed -- so the check reads BOTH the state
# bit and the size, not just the size.
P="$(plant 1)"
echo "  plant: $P"
if [ "$P" = "NOTEMPLATE" ] || [ "$P" = "BADMAGIC" ]; then
	skip "(a): no usable Window Geometry template to build a maximized blob from ($P)"
	skip "(b): nothing was planted, so there is nothing to compare"
else
	export_key "$USERKEY" "$W/user_before.reg"
	export_key "$GATEKEY" "$W/gate_before.reg"
	GEOM_BEFORE="$(readgeom)"
	run_exe "$EXE" WW_RENDER_SIZE=1024x1024
	L="$(logline)"
	echo "  $L"
	check "the harness wrote release/ww_harness_window.log" \
		"$([ -n "$L" ] && echo 1 || echo 0)"
	check "the window is NOT maximized (maximised=$(field "$L" maximised))" \
		"$([ "$(field "$L" maximised)" = "0" ] && echo 1 || echo 0)"
	check "the persisted geometry was not restored (settings=$(field "$L" settings))" \
		"$([ "$(field "$L" settings)" = "not-restored" ] && echo 1 || echo 0)"
	check "the window came out at the size asked for (window=$(field "$L" window), asked=$(field "$L" asked))" \
		"$([ "$(field "$L" window)" = "1024x1024" ] && echo 1 || echo 0)"
	check "and the line does not say FLOORED" \
		"$(echo "$L" | grep -q FLOORED && echo 0 || echo 1)"

	# ===================================================================
	echo
	echo "=== (b) the settings are byte-identical after the run ===================="
	# Two questions, and only the first is the one the brief asked for. The
	# second is the one that would actually hurt.
	#
	# THE FIRST ONE IS ASKED OF THE PLANTED VALUE, NOT OF THE WHOLE KEY -- and
	# the first run of this row got that wrong and went red on a correct build.
	# The readgeom() helper above was written for exactly this and then not
	# wired in, so the row compared two whole-key exports and reported a defect
	# when what it had found was a run writing its own recent-file list into its
	# own scope. That is allowed, and it is not the question. The question is
	# whether "Window Geometry" -- the value this gate planted, the one the
	# defect was about -- came back untouched.
	#
	# bungo's key is a different matter and IS compared whole, because there the
	# only acceptable answer is "nothing was written at all".
	export_key "$GATEKEY" "$W/gate_after.reg"
	export_key "$USERKEY" "$W/user_after.reg"
	GEOM_AFTER="$(readgeom)"
	check "the planted Window Geometry is byte-identical after the run" \
		"$([ -n "$GEOM_BEFORE" ] && [ "$GEOM_BEFORE" = "$GEOM_AFTER" ] && echo 1 || echo 0)"
	if [ "$GEOM_BEFORE" != "$GEOM_AFTER" ]; then
		echo "  before: ${GEOM_BEFORE:0:96}"
		echo "  after : ${GEOM_AFTER:0:96}"
	fi
	# Said out loud rather than hidden: the rest of the gate's OWN scope may
	# legitimately move, and the row names what did.
	if ! cmp -s "$W/gate_before.reg" "$W/gate_after.reg"; then
		echo "  (the gate's own scope did change elsewhere, which is allowed:"
		diff "$W/gate_before.reg" "$W/gate_after.reg" | grep '^[<>]' \
			| sed 's/=.*/=.../' | head -8 | sed 's/^/     /'
		echo "   -- none of these is Window Geometry)"
	fi
	check "and bungo's own key is byte-identical after the run (it was never reached)" \
		"$(cmp -s "$W/user_before.reg" "$W/user_after.reg" && echo 1 || echo 0)"
	if ! cmp -s "$W/user_before.reg" "$W/user_after.reg"; then
		echo "  --- what changed in HIS key (this must be empty) ---"
		diff "$W/user_before.reg" "$W/user_after.reg" | head -20 | sed 's/^/    /'
	fi
fi

# ===========================================================================
echo
echo "=== (c) the RED CONTROL: the old path floors ============================="
# The repair is only shown to be a repair by an exe that does NOT have it.
# Not a mutation and not a toggle -- bungo's rule is that a repair never ships
# behind a switch, so the control is the rung exe, and without it this SKIPS by
# name rather than passing.
# THE CONTROL CANNOT BE RUN IN THE ISOLATED SCOPE, and the first version of this
# row did not notice. WW_SETTINGS_SCOPE is part of THIS lane's change: the rung
# exe has never heard of the variable, so it ignores the planted blob entirely
# and reads bungo's real key. The row duly reported maximised=0 and went red for
# a reason that had nothing to do with the defect. A control that cannot see the
# input it is given is not a control.
#
# So the control is run the only way it can be: against the settings the rung exe
# CAN see -- bungo's own, READ-ONLY. Nothing is planted. What makes this a fair
# control is that his key is what produced the 0.8978 in the first place (lane
# HORIZONOUT: vp=1822x989 out of a 1024x1024 request), so it is the exact input
# under test.
#
# "READ-ONLY" IS CHECKED, AND THE CHECK FOUND SOMETHING -- so read this before
# changing it. The row first asserted that his WHOLE key came back byte-identical.
# That passed at 14:47 and failed at 14:58 on the same two binaries, which is the
# signature of a real defect rather than a flaky check:
#
#   src/nifskope.cpp:8238, in setCurrentFile(), writes "File/Recent File List"
#   with no WW_* guard at all. It is OUTSIDE saveUi(), so saveUi()'s guard --
#   the one that has protected the window geometry since 2026-07-27 -- does not
#   cover it. EVERY harness run that opens a file rewrites that list.
#
# It only shows up as a diff when the ORDER actually changes, which is why it is
# intermittent: run the same fixture twice and the second run is a no-op
# (measured directly, 808842 bytes before and after, empty diff), but run
# gltf_export_options.sh and body_build.sh in between and the fixture is no
# longer at the head of the list, so re-opening it reorders them.
#
# This lane's WW_SETTINGS_SCOPE incidentally fixes it for anything that sets the
# variable -- which is why row (b) above can assert, deterministically, that the
# NEW exe leaves his key alone. The rung exe predates the variable and cannot.
#
# So the row asserts the thing that is actually promised and is deterministic --
# his WINDOW GEOMETRY and WINDOW STATE are untouched -- and NAMES the recent-file
# churn instead of going red for a defect that belongs to the binary being used
# as a control. It is written up as CHANGE_NEEDED 3 in the lane's PENDING.md.
#
# If his window state is ever left un-maximized, this row SKIPS by name rather
# than passing: it says the old path could not be shown to floor today, and that
# is a different sentence from "the old path is fine".
if [ -x "$BEFORE" ]; then
	export_key "$USERKEY" "$W/ctl_before.reg" \
		&& echo "  his key exported: $(wc -c < "$W/ctl_before.reg") bytes" \
		|| echo "  his key: EXPORT FAILED"
	rm -f "$LOG" "$WINLOG"
	env WW_WINDOW_AT="${WW_WINDOW_AT:-1960,40}" \
		WW_RENDER_SIZE=1024x1024 \
		WW_RENDER_SHOT="$(winpath "$W/shot_before.png")" \
		timeout 180 "$BEFORE" --port "$PORT" "$(winpath "$FIX")" >/dev/null 2>&1
	export_key "$USERKEY" "$W/ctl_after.reg"
	# The promise, and it is deterministic: his WINDOW keys are untouched.
	geomlines() { grep -a -iE '"Window (Geometry|State)"' "$1" 2>/dev/null; }
	check "the control run left his window geometry and state untouched" \
		"$([ "$(geomlines "$W/ctl_before.reg" | md5sum)" = "$(geomlines "$W/ctl_after.reg" | md5sum)" ] && echo 1 || echo 0)"
	# And whatever else moved is NAMED rather than swallowed -- see the comment
	# above; on the rung exe this is the unguarded recent-file writer.
	if ! cmp -s "$W/ctl_before.reg" "$W/ctl_after.reg"; then
		echo "  the rung exe DID write to his key. What moved:"
		diff <(tr -d '\000' < "$W/ctl_before.reg") <(tr -d '\000' < "$W/ctl_after.reg") \
			| grep '^[<>]' | sed 's/\(.\{110\}\).*/\1.../' | head -4 | sed 's/^/     /'
		echo "     (src/nifskope.cpp:8238 File/Recent File List, no WW_ guard -- CHANGE_NEEDED 3)"
	else
		echo "  his key came back byte-identical this run"
	fi

	if [ -f "$WINLOG" ]; then
		GB="$(grep '^grab ' "$WINLOG" | grep -v '0x0' | tail -1)"
		[ -n "$GB" ] || GB="$(grep '^shown ' "$WINLOG" | grep -v '0x0' | tail -1)"
		GEOM="$(echo "$GB" | sed -n 's/.*geom=[-0-9]*,[-0-9]*,\([0-9]*x[0-9]*\).*/\1/p')"
		MX="$(echo "$GB" | sed -n 's/.*maximised=\([0-9]\).*/\1/p')"
		echo "  before-exe: $GB"
		if [ "${MX:-0}" = "1" ] || [ -n "$GEOM" -a "${GEOM:-}" != "1024x1024" ]; then
			check "the rung exe FLOORS a 1024x1024 request from his real settings (window=${GEOM:-?} maximised=${MX:-?})" \
				"$([ "${GEOM:-}" != "1024x1024" ] && echo 1 || echo 0)"
		else
			skip "(c): the rung exe came up at 1024x1024 today -- his saved window is not maximized right now, so the old path cannot be shown to floor from this input"
		fi
	else
		skip "(c): the rung exe wrote no ww_headless_windows.log, so the old path could not be observed"
	fi
else
	skip "(c) the red control: no rung exe at $BEFORE -- copy release/NifSkope.exe there BEFORE applying the hook-up"
fi

# ===========================================================================
echo
echo "=== (d) native_open.sh (c) re-measured ==================================="
# The row this lane exists for. 0.8978 was measured with vp=1822x989; the
# 2026-09-18 run at vp=1024x989 gave 0.9331. The bar is 0.90 and it is NOT
# re-based here: if it still refuses at the right viewport, the disagreement is
# real and belongs to whoever owns the .lodi scene, not to this lane.
if [ "${RUN_NATIVE_OPEN:-1}" = "1" ] && [ -f "$ROOT/tests/spells/native_open.sh" ]; then
	no_out="$(bash "$ROOT/tests/spells/native_open.sh" 2>&1)"
	echo "$no_out" | grep -E 'covered|vp=|checks,' | sed 's/^/    /'
	COV="$(echo "$no_out" | sed -n 's/.*covered \([0-9.]*\) >=.*/\1/p' | head -1)"
	VP="$(echo "$no_out" | sed -n 's/.*vp=\([0-9]*x[0-9]*\).*/\1/p' | head -1)"
	check "native_open.sh (c) now measures at the viewport it asked for (vp=${VP:-?})" \
		"$([ "${VP:-}" = "1024x989" ] && echo 1 || echo 0)"
	check "and its coverage row is green (covered ${COV:-?} >= 0.90)" \
		"$(awk -v a="${COV:-0}" 'BEGIN{print (a >= 0.90) ? 1 : 0}')"
else
	skip "(d) native_open.sh (RUN_NATIVE_OPEN=0)"
fi

# ===========================================================================
echo
echo "=== (e) the SECOND candidate: is the resize also floored by the docks? ==="
# NOT a claim, a question this lane could not answer without building. In the
# WW_RENDER_SHOT block the resize() at src/nifskope_ui.cpp:~22096 runs BEFORE
# the docks are hidden at ~22104, so on a window that is NOT maximized the
# request can still be floored by the dock layout's minimum width. Plant a
# NORMAL geometry, ask for something narrow, and read the width back. A red row
# here means a second defect and a one-line reorder, in a block lane
# IMPOSTORSHOW owns -- see the lane's CHANGE_NEEDED.
#
# RE-AIMED after the first run, and the first aim was wrong. The row used to
# require window=640x480 and go red otherwise. It went red -- and the answer is
# that the request is genuinely unsatisfiable: the block list, the details tree
# and the docks have a layout minimum, measured at 1024 px wide on this build, and
# Qt will not make a window narrower than its layout minimum for anybody. A check
# that demands the impossible is not a floor, it is a second defect (skill
# ww-test-harness-add 5b), so it cannot stay as it was.
#
# What the repair actually promises is the thing worth checking: A FLOOR IS NEVER
# SILENT. Ask for something below the minimum and the run must SAY it was floored,
# in one line, carrying both numbers, so no later harness can quietly measure a
# machine and call it code. That is what is checked here. The measured minimum is
# printed either way, because it is the number the next lane needs.
if [ "$P" != "NOTEMPLATE" ] && [ "$P" != "BADMAGIC" ]; then
	plant 0 >/dev/null
	run_exe "$EXE" WW_RENDER_SIZE=640x480
	LE="$(logline)"
	echo "  $LE"
	GOTW="$(field "$LE" window)"
	echo "  measured layout minimum for this build: ${GOTW:-?} (asked 640x480)"
	if [ "$GOTW" = "640x480" ]; then
		check "an un-maximized window gets a NARROW size too (window=$GOTW)" 1
	else
		check "the floor is NOT silent: the line says FLOORED (window=$GOTW, asked 640x480)" \
			"$(echo "$LE" | grep -q FLOORED && echo 1 || echo 0)"
		check "and the log carries the refusal with BOTH numbers" \
			"$(grep -q '^REFUSED: the window size asked for was 640x480 ' "$LOG" && echo 1 || echo 0)"
	fi
else
	skip "(e): no plant, so the un-maximized case could not be set up"
fi

# ===========================================================================
echo
echo "=== (f) a harness run does not edit the recent-file list =================="
# THE THIRD SETTINGS WRITER (lane HARNESSWIN2, 2026-09-19).
#
# saveUi() has refused to persist from a WW_* run since 2026-07-27 and
# restoreUi() was given the same refusal by HARNESSWIN1. setCurrentFile() and
# clearCurrentFile() write "File/Recent File List" from OUTSIDE saveUi(), so
# neither guard reached them: every gate in tests/spells/ that opens a .nif
# rewrote the user's recent-file list, and had done so for as long as the
# harnesses have existed.
#
# It surfaced as an INTERMITTENT row (c): pass at 14:47, fail at 14:58, same two
# binaries, no edit in between -- because the exported key only differs when the
# list ORDER changes, and opening the same fixture twice leaves it already at the
# head. So this row does not ask "did anything change", it SEEDS a list the
# fixture is not in, and then asks whether the fixture appeared.
#
# In the GATE SCOPE only. bungo's own key is never written by this gate, here or
# anywhere else in it.
if [ "$P" = "NOTEMPLATE" ] || [ "$P" = "BADMAGIC" ]; then
	skip "(f): the gate scope was never created, so there is nothing to seed"
else
	# The rung for THIS repair. It must be a post-HARNESSWIN1 binary, because a
	# control that cannot see WW_SETTINGS_SCOPE would be run against settings it
	# was never given -- the mistake row (c) paid for (MISTAKES.md 2026-09-19).
	BEFORE2="${BEFORE2:-$ROOT/release/NifSkope.before_harnesswin2.exe}"
	[ -x "$BEFORE2" ] || BEFORE2="$ROOT/release/NifSkope.before_impostorfix3.exe"

	cat > "$W/recent.ps1" <<'PSEOF'
$ErrorActionPreference = "SilentlyContinue"
# QSettings maps "File/Recent File List" onto the subkey File, value
# "Recent File List" -- the slash is a key separator, not part of the name.
$k = "HKCU:\Software\NifTools\NifSkope 2.0 $($env:HW_SCOPE)\File"
if ($env:HW_SEED -eq "1") {
    New-Item -Path $k -Force | Out-Null
    Set-ItemProperty -Path $k -Name "Recent File List" `
        -Value @("Z:\harnesswin2\seed_one.nif", "Z:\harnesswin2\seed_two.nif") `
        -Type MultiString
}
$v = (Get-ItemProperty -Path $k)."Recent File List"
if ($null -eq $v) { Write-Output "ABSENT" } else { Write-Output ($v -join "|") }
PSEOF
	recent() {  # recent [seed]
		HW_SCOPE="$SCOPE" HW_SEED="${1:-0}" powershell.exe -NoProfile -NonInteractive \
			-ExecutionPolicy Bypass -File "$(cygpath -w "$W/recent.ps1")" 2>&1 | tr -d '\r'
	}

	SEED="$(recent 1)"
	if [ "$SEED" = "ABSENT" ]; then
		skip "(f): the seeded recent-file list could not be written to the gate scope"
	else
		run_exe "$EXE" WW_RENDER_SIZE=1024x1024
		AFTER="$(recent)"
		check "the repaired exe left the recent-file list exactly as seeded" \
			"$([ "$AFTER" = "$SEED" ] && echo 1 || echo 0)"
		[ "$AFTER" = "$SEED" ] || echo "     seeded: $SEED
     after : $AFTER"

		# THE RED CONTROL. Without it this row passes on a build where the write
		# never happened for some other reason -- a floor that cannot fire.
		if [ -x "$BEFORE2" ]; then
			recent 1 >/dev/null
			run_exe "$BEFORE2" WW_RENDER_SIZE=1024x1024
			CTL="$(recent)"
			check "the floor fires: the rung exe DOES add the fixture to that list" \
				"$([ "$CTL" != "$SEED" ] && echo 1 || echo 0)"
			echo "     rung ($(basename "$BEFORE2")) left: $CTL"
			recent 1 >/dev/null
		else
			skip "(f) control: no pre-HARNESSWIN2 rung at $BEFORE2"
		fi
	fi
fi

# ===========================================================================
echo
echo "$checks checks, $fails failures, $skips skips"
if [ "$skips" -gt 0 ]; then
	echo "--- a SKIP is never a pass ---"
fi
# MEASURED, NEVER PREDICTED (ww-test-harness-add 5c). 0 until this has had one
# green run; write the count back here with the exe timestamp and the date.
# MEASURED 2026-09-19 14:47 on release/NifSkope.exe 23,367,168 B: 13 checks with
# RUN_NATIVE_OPEN=1, 11 with it off. 11 is the floor, so the gate refuses if a
# row stops running -- which is the failure this floor exists for, a row that
# silently disappears reading as a pass.
#
# RAISED TO 15, MEASURED (lane HARNESSWIN2, 2026-09-19 16:5x). The run that
# raised it: release/NifSkope.exe 23,505,408 B, 16:48:03, sha1 072d78f8629aac43,
# the first binary carrying the dock-order repair -- "15 checks, 0 failures, 0
# skips, PASS", every row including (f) and the red controls. Until that run the
# number here was 11 and this comment said so: row (f) was PREDICTED to make it
# 13 or 15, and a prediction is not a floor (ww-test-harness-add 5c). It came
# out 15; 15 is what is written. If a row is ever added, this number is raised
# the same way -- from a run, never from counting the source.
FLOOR="${FLOOR:-15}"
[ "$checks" -ge "$FLOOR" ] || { echo "FAIL: $checks checks is below the floor $FLOOR"; exit 1; }
[ "$fails" -eq 0 ] || exit 1
echo "PASS"
