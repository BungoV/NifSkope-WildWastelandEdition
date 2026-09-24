#!/usr/bin/env bash
#
# THE HEADLESS ROUTES MUST EXIT, NOT ASK.
#
# Every WW_* hook ends in qApp->quit(), and since Qt 6.5 that is not exit(0):
# QCoreApplicationPrivate::quit() is virtual (QtCore/private/qcoreapplication_p.h)
# and QGuiApplicationPrivate overrides it (QtGui/private/qguiapplication_p.h) to
# CLOSE EVERY TOP-LEVEL WINDOW first. So the quit runs NifSkope::closeEvent, which
# asked saveConfirm(), which raised a modal "You have unsaved changes to X" box
# with nobody there to answer it. The run then sat until its own `timeout` killed
# it and produced nothing.
#
# bungo, 2026-09-09: "agents keep always hanging on save confirmation", with the
# box on TreeMapleForest3 during a card bake. The bake is one of the routes that
# EDITS the loaded document (it zeroes LOD1/LOD2 Size and sets the hidden flag on
# `_L*` shapes so the in-cell detail steps do not stack up in the card), so its
# document is genuinely modified by the time the hook quits.
#
# What this gate asserts, per case: the process EXITS 0, WITHIN its timeout, and
# well inside it by wall clock; and release/ww_headless_close.log names exactly
# the documents whose changes were discarded.
#
# THE FLOOR IS THE PAIR. "It did not hang" passes just as well on a case that was
# never dirty, so every case is run twice over the same code path with one input
# difference:
#
#   cube_plain.nif   nothing for the bake to edit  -> exits, and NO discard line
#   cube_lod.nif     one shape renamed `*_L1`      -> exits, and the discard line
#                                                     names it
#
# A guard that never fired fails the second; a guard that discards indiscriminately
# fails the first; the old code fails both on the timeout. The fixtures are built
# by the application's own CLI (`-no-gui new --cube`, then `set -f Name`), so this
# needs no game corpus and nothing hand-authored.
#
# AND THEY MUST NOT REACH HIS EYE OR HIS MAIN MONITOR
# (added 2026-09-09 lane OFFSCREEN, rewritten the same day by lane OFFSCREEN2).
#
# bungo gave two rules, both verbatim:
#
#   "Agent is launching nifskope on my main monitor, which is a no no"
#   "the screen is flashing white and black, that's a view hazard for epileptics"
#
# The impostor bake spends NINE full repaints per octahedral view -- two matte
# passes over a black clear and a white one, twice, plus five channel renders --
# so a model at OCT=8 repaints its window 64*9 + 4 = 580 times, model after
# model. That strobe is how the alpha is measured and it is not going away; what
# has to go away is anything of it reaching a monitor he is looking at.
#
# THE FIRST ATTEMPT MOVED THE WINDOW OFF EVERY SCREEN AND THAT IS DEAD. As
# written it was inert (restoreUi() restores a MAXIMISED window, and move() on a
# maximised window does nothing on Windows but choose a monitor); when the
# maximised bit was cleared it worked and then NOTHING RENDERED -- GLView is a
# QOpenGLWindow, an unexposed surface never gets a context, and the run wrote no
# PNG and no card while exiting 0. So "off every screen" is no longer the
# property under test. The property is INVISIBLE OR ABSENT, and NEVER ON THE
# PRIMARY: the window is un-maximised, placed on a non-primary screen, and shown
# at window opacity 0.
#
# Sections 5 and 6 test that from THREE instruments, each shown able to fail:
#
#   inside  release/ww_headless_windows.log -- the process's own record of every
#           top-level window at show, at each grab and at the matte, carrying
#           onprimary=0/1 and opacity=0.00..1.00 as separate numbers;
#   outside a PowerShell WINDOW sampler over the life of the run: every VISIBLE
#           top-level window of a NifSkope started with --port that intersects a
#           monitor, with that monitor's primary flag and the window's LAYERED
#           ALPHA read back from Windows (GetLayeredWindowAttributes);
#   pixels  a PowerShell SCREEN sampler over one region of the second monitor,
#           every ~50 ms, reporting mean luminance. This is the only instrument
#           that answers what bungo actually asked, because the other two echo
#           what Windows was TOLD. A layered alpha of 0 is a claim about an API;
#           a GL child window that presented over its parent anyway would
#           satisfy it and still strobe. What decides is the RANGE of the
#           desktop's own luminance while the bake runs.
#
# THE FLOORS ARE THE CONTROL RUNS, and there are two, because the zeros above
# are equally consistent with three broken instruments:
#
#   render_visible    WW_WINDOW_VISIBLE=1, the plain render: the process must
#                     report opacity=1.00, the window sampler must see a
#                     non-transparent window, and the pixel sampler's range must
#                     leave the desktop's own noise floor behind;
#   bake_oct_visible  WW_WINDOW_VISIBLE=1 with WW_IMPOSTOR_OCT=4: the strobe
#                     itself, run ONCE on the second monitor on purpose, so the
#                     pixel sampler is shown catching an ALTERNATION and not
#                     merely a window.
#
# And a NOISE FLOOR before any of it: the same screen region sampled with no
# NifSkope running at all, so "the desktop did not change" is measured against
# what the desktop does by itself rather than against zero.
#
# The control runs are also the identity checks: one build, one scene, opacity 0
# and opacity 1, byte-identical PNGs and byte-identical cards. That is a tighter
# comparison than a before/after exe, which would also carry every other change
# in the tree. Each identity check asserts its PRECONDITION first -- that the
# hidden run really was hidden -- because on 2026-09-09 one of them passed by
# comparing a picture with itself.
#
# BOTH CONTROL RUNS ARE PLACED BY THE SAME CODE AS THE HIDDEN ONES, on a
# non-primary screen. A visible-on-purpose control is not exempt from the
# main-monitor rule; one of them is how bungo came to see a NifSkope on his.
#
#   bash tests/spells/render_shot.sh
#
# Artifacts: release/ww_render_shot/ (kept: the cards, the sidecars and the logs).

set -u

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="$REPO/release/NifSkope.exe"
# Second monitor, one instance at a time; _harness.sh owns the coordinates.
# WW_WINDOW_AT is honoured for a headless run AND for the visible control, but
# only if the point is not on the primary screen: wwHeadlessWindowOrigin()
# refuses a main-monitor position and falls back to the first non-primary
# screen, naming the refusal in the window log's arm= field.
. "$(dirname "$0")/_harness.sh"
WORK="$REPO/release/ww_render_shot"
CLOSELOG="$REPO/release/ww_headless_close.log"
WINLOG="$REPO/release/ww_headless_windows.log"
PORT="${WW_RENDER_SHOT_PORT:-42327}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
# The whole point is the difference between "finished" and "sat on a dialog", so
# the cap has to be far above a real run and far below patience. A cube render on
# this machine is ~5 s; the hang was whatever cap the caller set.
CAP=90
# Above this, a run that exited 0 still did not exit PROMPTLY - a dialog that is
# dismissed by something else, or a second window closing behind it, would show
# up here rather than in the exit code.
SLOW=45

checks=0; fails=0
ok()   { checks=$((checks+1)); printf '  ok   %-44s %s\n' "$1" "$2"; }
bad()  { checks=$((checks+1)); fails=$((fails+1)); printf '  FAIL %-44s %s\n' "$1" "$2"; }
check(){ if [ "$1" = "1" ]; then ok "$2" "$3"; else bad "$2" "$3"; fi }

[ -x "$EXE" ] || { echo "FAIL  no release/NifSkope.exe"; exit 1; }

rm -rf "$WORK"; mkdir -p "$WORK"
rm -f "$CLOSELOG" "$WINLOG"

# THE WINDOW INSTRUMENT. Samples every 200 ms for the life of a run and writes
# one line for EVERY VISIBLE TOP-LEVEL WINDOW of a NifSkope process started with
# `--port` that intersects a monitor. The --port filter keeps a person's own open
# NifSkope out of the measurement: a harness instance always carries one, an
# interactive window never does.
#
# EnumWindows, not Get-Process().MainWindowHandle, and the reason is worth
# keeping. The .NET property returns ONE handle per process by a heuristic, so
# it can never support "there was no other window". When it reported a 426x306
# opaque window on the PRIMARY monitor once per run, that was first written off
# as its error -- and it was not: enumerating found the SAME window, in every
# hidden run, and named it (class Qt6111QWindowIcon, the application title, no
# filename). A 25 ms probe put it at t=371 ms, gone by t=1403 ms, replaced by a
# different HWND of class Qt6111QWindowOwnDCIcon at the asked-for place with
# layered alpha 0: Qt's Windows plugin picks the window class by whether the
# surface needs its own DC, so realising the GL container destroys the first
# native window and creates a second -- and the first was created while the
# widget still had Qt's default geometry. The cure is in the NifSkope
# constructor, before any native window can exist; this instrument is what
# proves it. Enumerating is also strictly stronger -- it can only see MORE
# windows -- and each line carries the class and title so a hit is NAMED.
#
# Three facts per line, and they are different questions:
#   primary=0|1   whether the monitor it is on is HIS MAIN ONE;
#   alpha=0..255  the window's LAYERED ALPHA, read back from Windows. A window
#                 that is not layered reads 255; a window layered with a colour
#                 key and no alpha reads 255 too, because a colour key hides
#                 nothing on its own. alpha=0 is invisible to an eye;
#   cls/title     what it was, for when one of the two above is not zero.
# A window on a screen is no longer a failure by itself -- it has to be on a
# screen or GLView never gets a context. alpha>0 is a failure, and so is
# primary=1 at any alpha.
WATCHPS="$WORK/window_watch.ps1"
cat > "$WATCHPS" <<'PSEOF'
param([string]$Out, [string]$Stop)
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Collections.Generic;
public class WwWin {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetLayeredWindowAttributes(
      IntPtr h, out uint crKey, out byte bAlpha, out uint dwFlags);
  public static List<string> Scan(uint want) {
    var found = new List<string>();
    EnumWindows(delegate(IntPtr h, IntPtr l) {
      uint pid; GetWindowThreadProcessId(h, out pid);
      if (pid != want) return true;
      if (!IsWindowVisible(h)) return true;
      RECT r; GetWindowRect(h, out r);
      // LWA_ALPHA is 0x2. Not layered, or layered without LWA_ALPHA, means the
      // window is fully opaque as far as an eye is concerned.
      uint k; byte a; uint f; int alpha = 255;
      if (GetLayeredWindowAttributes(h, out k, out a, out f)) { if ((f & 0x2) != 0) alpha = (int)a; }
      var cn = new StringBuilder(128); GetClassName(h, cn, 128);
      var tt = new StringBuilder(128); GetWindowTextW(h, tt, 128);
      found.Add(String.Format("{0},{1},{2},{3}\t{4}\tcls={5}\ttitle={6}",
        r.Left, r.Top, r.Right, r.Bottom, alpha, cn.ToString(), tt.ToString()));
      return true;
    }, IntPtr.Zero);
    return found;
  }
}
"@
$lines = New-Object System.Collections.Generic.List[string]
while (-not (Test-Path $Stop)) {
  foreach ($p in (Get-CimInstance Win32_Process -Filter "Name='NifSkope.exe'")) {
    if ($p.CommandLine -notmatch '--port') { continue }
    foreach ($w in [WwWin]::Scan([uint32]$p.ProcessId)) {
      $f = $w -split "`t"
      $xy = $f[0] -split ','
      $L = [int]$xy[0]; $T = [int]$xy[1]; $R = [int]$xy[2]; $B = [int]$xy[3]
      foreach ($s in [System.Windows.Forms.Screen]::AllScreens) {
        # $scr, NOT $b: PowerShell variables are case-insensitive, so $b would
        # be the same variable as $B, the window's bottom edge.
        $scr = $s.Bounds
        if (($L -lt ($scr.X + $scr.Width)) -and ($R -gt $scr.X) -and
            ($T -lt ($scr.Y + $scr.Height)) -and ($B -gt $scr.Y)) {
          $prim = 0; if ($s.Primary) { $prim = 1 }
          $lines.Add("$($p.ProcessId) $($f[0]) on $($s.DeviceName) primary=$prim alpha=$($f[1]) $($f[2]) $($f[3])")
          break
        }
      }
    }
  }
  Start-Sleep -Milliseconds 200
}
$lines | Out-File -Encoding ascii $Out
PSEOF

# THE PIXEL INSTRUMENT. The other two read back what Windows was TOLD; this one
# reads the desktop. Captures one region of the second monitor every ~50 ms,
# scales it to 8x8 so a sample costs 64 reads rather than 40,000, and records
# the mean luminance. The number that matters is the RANGE over a run: a strobe
# alternating a black clear and a white one cannot hide in it, and a window that
# is genuinely invisible cannot produce one.
#
# The samples are held in memory and written at the end: appending to a file 600
# times is slower than the thing being measured.
SCREENPS="$WORK/screen_watch.ps1"
cat > "$SCREENPS" <<'PSEOF'
param([string]$Out, [string]$Stop, [int]$X, [int]$Y, [int]$W, [int]$H)
Add-Type -AssemblyName System.Drawing
$big = New-Object System.Drawing.Bitmap $W, $H
$gb  = [System.Drawing.Graphics]::FromImage($big)
$sm  = New-Object System.Drawing.Bitmap 8, 8
$gs  = [System.Drawing.Graphics]::FromImage($sm)
$gs.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBilinear
$lines = New-Object System.Collections.Generic.List[string]
while (-not (Test-Path $Stop)) {
  try { $gb.CopyFromScreen($X, $Y, 0, 0, $big.Size) } catch { break }
  $gs.DrawImage($big, 0, 0, 8, 8)
  $sum = 0.0
  for ($i = 0; $i -lt 8; $i++) {
    for ($j = 0; $j -lt 8; $j++) {
      $p = $sm.GetPixel($i, $j)
      $sum += 0.299 * $p.R + 0.587 * $p.G + 0.114 * $p.B
    }
  }
  $lines.Add(("{0:F3}" -f ($sum / 64)))
  Start-Sleep -Milliseconds 50
}
$lines | Out-File -Encoding ascii $Out
PSEOF

# The sampled region: inside the window the placement code will make, on the
# second monitor. WW_WINDOW_AT is where _harness.sh puts it and where
# wwHeadlessWindowOrigin() honours it (it refuses a point on the primary), so
# the region is derived from that rather than guessed.
AT_X="${WW_WINDOW_AT%%,*}"; AT_Y="${WW_WINDOW_AT##*,}"
case "$AT_X$AT_Y" in *[!0-9-]*|"") AT_X=1920; AT_Y=0;; esac
SAMP_X=$((AT_X + 160)); SAMP_Y=$((AT_Y + 160)); SAMP_W=200; SAMP_H=200

WATCH_OUT=""; WATCH_STOP=""; WATCH_PID=""
LUM_OUT=""; LUM_STOP=""; LUM_PID=""
watch_start() { # label
	WATCH_OUT="$WORK/$1.onscreen"; : > "$WATCH_OUT"
	WATCH_STOP="$WORK/$1.watchstop"; rm -f "$WATCH_STOP"
	powershell -NoProfile -ExecutionPolicy Bypass -File "$(winpath "$WATCHPS")" \
		-Out "$(winpath "$WATCH_OUT")" -Stop "$(winpath "$WATCH_STOP")" >/dev/null 2>&1 &
	WATCH_PID=$!
	LUM_OUT="$WORK/$1.lum"; rm -f "$LUM_OUT"
	LUM_STOP="$WORK/$1.lumstop"; rm -f "$LUM_STOP"
	powershell -NoProfile -ExecutionPolicy Bypass -File "$(winpath "$SCREENPS")" \
		-Out "$(winpath "$LUM_OUT")" -Stop "$(winpath "$LUM_STOP")" \
		-X "$SAMP_X" -Y "$SAMP_Y" -W "$SAMP_W" -H "$SAMP_H" >/dev/null 2>&1 &
	LUM_PID=$!
}
watch_stop() {
	[ -n "$WATCH_STOP" ] && : > "$WATCH_STOP"
	[ -n "$LUM_STOP" ] && : > "$LUM_STOP"
	[ -n "$WATCH_PID" ] && wait "$WATCH_PID" 2>/dev/null
	[ -n "$LUM_PID" ] && wait "$LUM_PID" 2>/dev/null
	return 0
}

cli()  { "$EXE" -no-gui "$@" 2>&1; }
nget() { cli get -b "$1" -f "$2" "$3" | tail -1; }
# A block's Name is a tStringIndex: `get -f Name` prints the INDEX, not the
# string, and `set -f Name -v <text>` is refused for the same reason
# (NifValue::setFromString parses tStringIndex as a number). The resolved
# name is what `list` prints, so that is what is read here.
bname() { cli list "$2" | grep "^\[$1\]" | sed "s/.*'\(.*\)'.*/\1/"; }

echo "deriving fixtures through the app's own CLI"
PLAIN="$WORK/cube_plain.nif"
LODF="$WORK/cube_lod.nif"
cli new -o "$(winpath "$PLAIN")" --cube >/dev/null 2>&1
[ -s "$PLAIN" ] || { echo "FAIL  -no-gui new --cube wrote nothing"; exit 1; }
SHAPE=$(cli list "$PLAIN" | grep -i "BSTriShape" | head -1 | sed 's/^\[\([0-9]*\)\].*/\1/')
[ -n "$SHAPE" ] || { echo "FAIL  no BSTriShape in the cube fixture"; exit 1; }
SNAME=$(bname "$SHAPE" "$PLAIN")
[ -n "$SNAME" ] || { echo "FAIL  could not read the shape name"; exit 1; }
# The rename goes through the header string table because the CLI cannot
# introduce a new string (see bname above). The bytes still come from the
# app's own `new --cube`; only the one length-prefixed entry moves.
"$PY" - "$(winpath "$PLAIN")" "$(winpath "$LODF")" "$SNAME" <<'RENEOF'
import struct, sys
src, dst, name = sys.argv[1], sys.argv[2], sys.argv[3]
b = open(src, "rb").read()
old = struct.pack("<I", len(name)) + name.encode()
new = struct.pack("<I", len(name) + 3) + (name + "_L1").encode()
if b.count(old) != 1:
    sys.stderr.write("string table: %d matches for %r\n" % (b.count(old), name))
    sys.exit(1)
open(dst, "wb").write(b.replace(old, new))
RENEOF
[ -s "$LODF" ] || { echo "FAIL  could not rename the shape"; exit 1; }

# THE FIXTURES ARE WHAT THEY CLAIM. Without this the dirty case could be dirty
# for some other reason, or not dirty at all, and the section below would be
# measuring the wrong thing while looking green.
PLAIN_NAME=$(bname "$SHAPE" "$PLAIN")
LOD_NAME=$(bname "$SHAPE" "$LODF")
check "$( [ "${PLAIN_NAME%_L1}" = "$PLAIN_NAME" ] && echo 1 || echo 0 )" \
	"the plain fixture has nothing the bake edits" "shape [$SHAPE] named '$PLAIN_NAME'"
check "$( [ "${LOD_NAME%_L1}" != "$LOD_NAME" ] && echo 1 || echo 0 )" \
	"the dirty fixture really carries a _L1 shape" "shape [$SHAPE] named '$LOD_NAME'"

# One run. RC and SECS come back in globals, never through $( ), so a bail-out
# inside the run cannot be captured as a number.
RC=0; SECS=0
run() { # label file  (env for the run is exported by the caller)
	local t0 t1
	# The process rewrites ww_headless_windows.log on its first record, so each
	# run's copy is taken here rather than read from a file three runs overwrite.
	rm -f "$WINLOG"
	watch_start "$1"
	t0=$(date +%s)
	timeout "$CAP" "$EXE" --port "$PORT" "$2" >"$WORK/$1.out" 2>&1
	RC=$?
	t1=$(date +%s)
	SECS=$((t1-t0))
	watch_stop
	if [ -f "$WINLOG" ]; then cp "$WINLOG" "$WORK/$1.winlog"; else : > "$WORK/$1.winlog"; fi
}

bake() { # label file
	local dir="$WORK/$1_cards"
	mkdir -p "$dir"
	export WW_IMPOSTOR_BAKE="$(winpath "$dir")"
	run "$1" "$2"
	unset WW_IMPOSTOR_BAKE
}

# The OCTAHEDRAL bake: 4x4 views, nine full repaints each, two of every nine
# alternating a black clear and a white one. 148 repaints, which is the shape of
# what bungo saw (a real model at OCT=8 is 580) and is what the pixel sampler is
# pointed at. TILE=64 keeps it to well under a minute.
bake_oct() { # label file
	local dir="$WORK/$1_cards"
	mkdir -p "$dir"
	export WW_IMPOSTOR_BAKE="$(winpath "$dir")"
	export WW_IMPOSTOR_OCT=4 WW_IMPOSTOR_TILE=64
	run "$1" "$2"
	unset WW_IMPOSTOR_BAKE WW_IMPOSTOR_OCT WW_IMPOSTOR_TILE
}

shot() { # label file
	export WW_RENDER_SHOT="$(winpath "$WORK/$1.png")"
	export WW_RENDER_SIZE=640x480 WW_RENDER_TIME=1
	run "$1" "$2"
	unset WW_RENDER_SHOT WW_RENDER_SIZE WW_RENDER_TIME
}

# `grep -c` on a file with no match prints 0 and EXITS 1, so an `|| echo 0`
# fallback prints the count twice and every arithmetic test below breaks. The
# missing-file case is the only one that needs a default.
count_in() { if [ -f "$2" ]; then grep -c "$1" "$2"; else echo 0; fi; }
discards() { count_in "^discarded " "$CLOSELOG"; }
named()    { count_in "^discarded $1\$" "$CLOSELOG"; }

echo
echo "0. the desktop's own noise floor, with no NifSkope running at all"
# "The screen did not change" has to be measured against what the screen does by
# itself. Two seconds of the same region, same sampler, nothing launched.
: > "$WORK/noise.lumstop"; rm -f "$WORK/noise.lumstop"
rm -f "$WORK/noise.lum"
powershell -NoProfile -ExecutionPolicy Bypass -File "$(winpath "$SCREENPS")" \
	-Out "$(winpath "$WORK/noise.lum")" -Stop "$(winpath "$WORK/noise.lumstop")" \
	-X "$SAMP_X" -Y "$SAMP_Y" -W "$SAMP_W" -H "$SAMP_H" >/dev/null 2>&1 &
NOISE_PID=$!
# Six seconds, not two: PowerShell takes over a second to start and a sample
# round costs ~80 ms, so two seconds produced six samples and the floor below
# failed on its own instrument.
sleep 6
: > "$WORK/noise.lumstop"
wait "$NOISE_PID" 2>/dev/null
lum_range()  { if [ -s "$WORK/$1.lum" ]; then awk 'NF{v=$1+0; if(n++==0){mn=v;mx=v}else{if(v<mn)mn=v; if(v>mx)mx=v}} END{ if(n) printf "%.3f", mx-mn; else printf "0" }' "$WORK/$1.lum"; else printf 0; fi; }
lum_count()  { if [ -f "$WORK/$1.lum" ]; then grep -c . "$WORK/$1.lum"; else echo 0; fi; }
NOISE_RANGE=$(lum_range noise)
# The bar every hidden run has to stay under. The sampler sees the whole
# region, NifSkope's window and whatever else the second monitor has there, so
# the bar has to clear ordinary desktop activity: a console printing into the
# region moved it by 5.4 on 2026-09-09. Measured amplitudes, one run: 0.2 the
# region left alone, 5.4 a console print, 27.4 an opaque window appearing,
# 250.6 the matte strobing. 15 separates them by better than 1.8x in both
# directions, and both floors below are measured against it.
NOISE_BAR=$(awk -v n="$NOISE_RANGE" 'BEGIN{ b=3.0*n; if(b<15.0) b=15.0; printf "%.3f", b }')
check "$( [ "$(lum_count noise)" -ge 10 ] && echo 1 || echo 0 )" \
	"the pixel sampler produced samples at all" \
	"$(lum_count noise) samples of ${SAMP_W}x${SAMP_H} at $SAMP_X,$SAMP_Y"
echo "  ..   desktop noise range $NOISE_RANGE -> hidden runs must stay under $NOISE_BAR"

echo
echo "1. a plain render exits on its own"
shot render_plain "$PLAIN"
check "$( [ "$RC" -eq 0 ] && echo 1 || echo 0 )" "WW_RENDER_SHOT exits 0" \
	"rc=$RC after ${SECS}s (cap ${CAP}s)"
check "$( [ "$SECS" -lt "$SLOW" ] && echo 1 || echo 0 )" "and promptly" "${SECS}s < ${SLOW}s"
check "$( [ -s "$WORK/render_plain.png" ] && echo 1 || echo 0 )" "and wrote its framebuffer" \
	"$(ls -l "$WORK/render_plain.png" 2>/dev/null | awk '{print $5}') bytes"
# Nothing edited the document, so nothing may be reported as discarded. This is
# the half that fails if the guard simply throws every document away.
check "$( [ "$(discards)" -eq 0 ] && echo 1 || echo 0 )" "and discarded nothing" \
	"$(discards) lines in ww_headless_close.log"

echo
echo "2. the card bake, with nothing in the file for it to edit"
bake bake_plain "$PLAIN"
check "$( [ "$RC" -eq 0 ] && echo 1 || echo 0 )" "WW_IMPOSTOR_BAKE exits 0" \
	"rc=$RC after ${SECS}s (cap ${CAP}s)"
check "$( [ "$SECS" -lt "$SLOW" ] && echo 1 || echo 0 )" "and promptly" "${SECS}s < ${SLOW}s"
BAKE_META=$(ls "$WORK/bake_plain_cards"/*.txt 2>/dev/null | head -1)
check "$( [ -n "$BAKE_META" ] && echo 1 || echo 0 )" "and wrote its sidecar" \
	"$(basename "${BAKE_META:-none}")"
# AND A CARD IMAGE. The sidecar is written before a single pixel is grabbed, so
# it is green on a run whose GL context never existed: the off-screen attempt of
# 2026-09-09 wrote this .txt, no PNG at all, and exited 0. This is the check
# that fails if a headless window ever stops being exposed again.
BAKE_PNGS=$(ls "$WORK/bake_plain_cards"/*.png 2>/dev/null | wc -l | tr -d ' ')
check "$( [ "${BAKE_PNGS:-0}" -ge 1 ] && echo 1 || echo 0 )" "and a card IMAGE, not just a sidecar" \
	"${BAKE_PNGS:-0} png(s) in bake_plain_cards"
check "$( [ "$(discards)" -eq 0 ] && echo 1 || echo 0 )" "and still discarded nothing" \
	"$(discards) lines in ww_headless_close.log"

echo
echo "3. the card bake on a document it EDITS - bungo's case"
bake bake_lod "$LODF"
check "$( [ "$RC" -eq 0 ] && echo 1 || echo 0 )" "the bake exits 0 instead of asking" \
	"rc=$RC after ${SECS}s (cap ${CAP}s; rc=124 is the dialog)"
check "$( [ "$SECS" -lt "$SLOW" ] && echo 1 || echo 0 )" "and promptly" "${SECS}s < ${SLOW}s"
LOD_META=$(ls "$WORK/bake_lod_cards"/*.txt 2>/dev/null | head -1)
HIDDEN=$(count_in "^hidden " "${LOD_META:-/dev/null}")
# ANTI-VACUITY: the bake has to have actually written into the model, or the case
# is the same as section 2 wearing a different name.
check "$( [ "$HIDDEN" -ge 1 ] && echo 1 || echo 0 )" "the bake really edited the document" \
	"$HIDDEN 'hidden' line(s) in $(basename "${LOD_META:-none}")"
check "$( [ "$(named cube_lod)" -ge 1 ] && echo 1 || echo 0 )" \
	"and the close path says what it discarded" \
	"$(grep '^discarded' "$CLOSELOG" 2>/dev/null | tr '\n' ';' | sed 's/;$//')"

echo
echo "4. nothing is left running"
# Only HARNESS instances, which always carry --port. A person's own open
# NifSkope has no --port and is none of this gate's business; counting it made
# the check fail for a reason that had nothing to do with the code under test.
LEFT=$(powershell -NoProfile -Command \
	"@(Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.CommandLine -match '--port' }).Count" \
	2>/dev/null | tr -d '\r')
check "$( [ "${LEFT:-0}" -eq 0 ] && echo 1 || echo 0 )" "no harness NifSkope survived" \
	"${LEFT:-?} running with --port"

echo
echo "5. bungo's two rules: nothing on his main monitor, and nothing an eye can see"
# grep -c on a file with no match prints 0 and EXITS 1 (see count_in above).
winlog_records()   { count_in "onscreen=" "$WORK/$1.winlog"; }
winlog_onscreen()  { count_in "onscreen=1" "$WORK/$1.winlog"; }
winlog_onprimary() { count_in "onprimary=1" "$WORK/$1.winlog"; }
# The window really existed. Qt keeps an invisible 0x0 helper QWindow alive in
# every process; it is not mapped, shows nothing, and reports opacity 1.00
# because nobody ever set one on it. Counting it made four runs "fail" for
# having a window nobody can see, so visible=0 records are excluded HERE and
# nowhere else -- the primary-monitor count already ignores them, because the
# code that writes it only asks about a window it can see.
winlog_visible() { count_in "visible=1" "$WORK/$1.winlog"; }
# A record is EYE-VISIBLE when it is mapped AND its opacity is not 0.00.
# Counted per field rather than per line so a future field ending in the same
# text cannot be mistaken for it.
winlog_opaque() {
	if [ -f "$WORK/$1.winlog" ]; then
		awk '/ visible=1 / { for (i=1;i<=NF;i++) if ($i ~ /^opacity=/ && $i != "opacity=0.00") n++ } END { print n+0 }' \
			"$WORK/$1.winlog"
	else echo 0; fi
}
winlog_arm()       { sed -n 's/.* arm=\([^ ]*\).*/\1/p' "$WORK/$1.winlog" 2>/dev/null | tail -1; }
samples_any()      { if [ -f "$WORK/$1.onscreen" ]; then grep -c . "$WORK/$1.onscreen"; else echo 0; fi; }
samples_primary()  { count_in "primary=1" "$WORK/$1.onscreen"; }
samples_opaque() {
	if [ -f "$WORK/$1.onscreen" ]; then
		awk '{ for (i=1;i<=NF;i++) if ($i ~ /^alpha=/ && $i != "alpha=0") n++ } END { print n+0 }' \
			"$WORK/$1.onscreen"
	else echo 0; fi
}
# awk, not [ ], because these are decimals.
under() { awk -v a="$1" -v b="$2" 'BEGIN{ print (a+0 <= b+0) ? 1 : 0 }'; }
over()  { awk -v a="$1" -v b="$2" 'BEGIN{ print (a+0 >= b+0) ? 1 : 0 }'; }

echo "  the hidden runs, including one 4x4 octahedral bake (148 matte repaints)"
bake_oct bake_oct "$PLAIN"
check "$( [ "$RC" -eq 0 ] && echo 1 || echo 0 )" "the octahedral bake exits 0" \
	"rc=$RC after ${SECS}s (cap ${CAP}s)"
OCT_PNGS=$(ls "$WORK/bake_oct_cards"/*.png 2>/dev/null | wc -l | tr -d ' ')
check "$( [ "${OCT_PNGS:-0}" -ge 1 ] && echo 1 || echo 0 )" "and wrote its sheets" \
	"${OCT_PNGS:-0} png(s) in bake_oct_cards"

for L in render_plain bake_plain bake_lod bake_oct; do
	# ANTI-VACUITY FIRST: a log with no records, or a run that never mapped a
	# window at all, would pass every zero below.
	check "$( [ "$(winlog_visible $L)" -ge 1 ] && echo 1 || echo 0 )" \
		"$L: the run recorded a window it really had" \
		"$(winlog_visible $L) mapped of $(winlog_records $L) record(s), arm=$(winlog_arm $L)"
	# RULE 1: never his main monitor. Opacity does not excuse this.
	check "$( [ "$(winlog_onprimary $L)" -eq 0 ] && echo 1 || echo 0 )" \
		"$L: no window of it was on the primary" \
		"$(winlog_onprimary $L) of $(winlog_records $L) records onprimary=1"
	check "$( [ "$(samples_primary $L)" -eq 0 ] && echo 1 || echo 0 )" \
		"$L: and none from outside either" \
		"$(samples_primary $L) of $(samples_any $L) sample(s) on the primary"
	# RULE 2: invisible or absent. NOT "off every screen" -- a window outside
	# every screen is never exposed and then GLView renders nothing at all.
	check "$( [ "$(winlog_opaque $L)" -eq 0 ] && echo 1 || echo 0 )" \
		"$L: every window it had was invisible or absent" \
		"$(winlog_opaque $L) of $(winlog_visible $L) mapped records at opacity > 0"
	check "$( [ "$(samples_opaque $L)" -eq 0 ] && echo 1 || echo 0 )" \
		"$L: and Windows agrees its layered alpha was 0" \
		"$(samples_opaque $L) of $(samples_any $L) sample(s) with alpha > 0"
	# RULE 2, MEASURED ON THE DESKTOP RATHER THAN ON AN API. The two checks
	# above read back what Windows was TOLD; this one reads the pixels.
	R=$(lum_range $L)
	check "$(under "$R" "$NOISE_BAR")" \
		"$L: and the screen itself did not change" \
		"luminance range $R over $(lum_count $L) samples (bar $NOISE_BAR, desktop noise $NOISE_RANGE)"
done

echo
echo "6. the controls: the way back, the floors, and the pixels that must not move"
# THE CONTROLS ARE PLACED BY THE SAME CODE. WW_WINDOW_VISIBLE=1 makes the window
# opaque; it does NOT put it back on the primary monitor. A visible-on-purpose
# control put a NifSkope on bungo's main monitor once already.
export WW_WINDOW_VISIBLE=1
shot render_visible "$PLAIN"
unset WW_WINDOW_VISIBLE
check "$( [ "$RC" -eq 0 ] && echo 1 || echo 0 )" "the visible control run exits 0" \
	"rc=$RC after ${SECS}s"
check "$( [ "$(winlog_onprimary render_visible)" -eq 0 ] && echo 1 || echo 0 )" \
	"the control is on the second monitor, not the primary" \
	"$(winlog_onprimary render_visible) of $(winlog_records render_visible) onprimary=1, arm=$(winlog_arm render_visible)"
# THE FLOORS. Without these the zeros in section 5 are equally consistent with
# three instruments that can never see anything.
check "$( [ "$(winlog_opaque render_visible)" -ge 1 ] && echo 1 || echo 0 )" \
	"the process CAN report an opaque window" \
	"$(winlog_opaque render_visible) of $(winlog_visible render_visible) mapped records at opacity > 0"
check "$( [ "$(samples_opaque render_visible)" -ge 1 ] && echo 1 || echo 0 )" \
	"and the window sampler CAN see one" \
	"$(samples_opaque render_visible) of $(samples_any render_visible) sample(s) with alpha > 0"
check "$( over "$(lum_range render_visible)" "$NOISE_BAR" )" \
	"and the pixel sampler CAN see a window's pixels" \
	"luminance range $(lum_range render_visible) over $(lum_count render_visible) samples (bar $NOISE_BAR)"

# THE STROBE FLOOR. The check above proves the pixel sampler sees a window; it
# does not prove it would catch an ALTERNATION, which is what bungo reported.
# This runs the matte visibly, once, on the second monitor, on purpose.
export WW_WINDOW_VISIBLE=1
bake_oct bake_oct_visible "$PLAIN"
unset WW_WINDOW_VISIBLE
check "$( [ "$RC" -eq 0 ] && echo 1 || echo 0 )" "the visible octahedral bake exits 0" \
	"rc=$RC after ${SECS}s"
check "$( over "$(lum_range bake_oct_visible)" 30 )" \
	"and the pixel sampler catches the black/white matte" \
	"luminance range $(lum_range bake_oct_visible) over $(lum_count bake_oct_visible) samples (bar 30)"
check "$( [ "$(winlog_onprimary bake_oct_visible)" -eq 0 ] && echo 1 || echo 0 )" \
	"even the strobe control stayed off the primary" \
	"$(winlog_onprimary bake_oct_visible) of $(winlog_records bake_oct_visible) onprimary=1"

# ONE BUILD, ONE SCENE, TWO OPACITIES. grabFramebuffer() reads the back buffer
# after paintGL(), so making the window transparent should not change a pixel.
# These are the checks that say whether it did.
#
# EACH ASSERTS ITS PRECONDITION FIRST. On 2026-09-09 this comparison printed
# `off <sha> / on <sha>  ok` in the same run in which six checks said the window
# was on a screen: both runs were identical because neither was hidden, and it
# had compared a picture with itself.
check "$( [ "$(winlog_opaque render_plain)" -eq 0 ] && [ "$(winlog_visible render_plain)" -ge 1 ] && echo 1 || echo 0 )" \
	"precondition: the hidden render really was hidden" \
	"$(winlog_opaque render_plain) opaque of $(winlog_visible render_plain) mapped records"
SHA_HIDDEN=$(sha256sum "$WORK/render_plain.png" 2>/dev/null | cut -d' ' -f1)
SHA_SHOWN=$(sha256sum "$WORK/render_visible.png" 2>/dev/null | cut -d' ' -f1)
check "$( [ -n "$SHA_HIDDEN" ] && [ "$SHA_HIDDEN" = "$SHA_SHOWN" ] && echo 1 || echo 0 )" \
	"hidden and visible photograph the same pixels" \
	"hidden ${SHA_HIDDEN:0:16} / visible ${SHA_SHOWN:0:16}"

# AND THE SAME FOR THE CARDS, which is the output that matters: a bake that
# writes different bytes when nobody is looking is not a fix.
check "$( [ "$(winlog_opaque bake_oct)" -eq 0 ] && [ "$(winlog_visible bake_oct)" -ge 1 ] && echo 1 || echo 0 )" \
	"precondition: the hidden card bake really was hidden" \
	"$(winlog_opaque bake_oct) opaque of $(winlog_visible bake_oct) mapped records"
CARDS_HIDDEN=$(cd "$WORK/bake_oct_cards" 2>/dev/null && ls | sort | while read -r f; do sha256sum "$f"; done | sha256sum | cut -d' ' -f1)
CARDS_SHOWN=$(cd "$WORK/bake_oct_visible_cards" 2>/dev/null && ls | sort | while read -r f; do sha256sum "$f"; done | sha256sum | cut -d' ' -f1)
check "$( [ "${OCT_PNGS:-0}" -ge 1 ] && [ -n "$CARDS_HIDDEN" ] && [ "$CARDS_HIDDEN" = "$CARDS_SHOWN" ] && echo 1 || echo 0 )" \
	"hidden and visible bake the same cards" \
	"hidden ${CARDS_HIDDEN:0:16} / visible ${CARDS_SHOWN:0:16} over $OCT_PNGS sheet(s)"

echo
echo "7. THE PINNED CAMERA: a picture with a world-unit scale, or none"
#
# Until 2026-09-09 no picture out of this hook carried a number, and both
# switches that were supposed to give it one were broken in the same place.
#
#   WW_RENDER_CENTER did nothing on an axis view. Two look-ats 400 units apart
#   on the 512-unit cube produced BYTE-IDENTICAL PNGs (md5 fff710bd...). It
#   "worked" on ViewUser only by accident: setOrientation() returns early when
#   the requested state is already the current one, so on that one view it
#   never queued the auto-fit that eats the look-at everywhere else.
#
#   WW_RENDER_DIST scaled as 1 / D^2. Measured span of the cube at
#   WW_RENDER_DIST 400 / 500 / 600 / 700 / 768 / 800 / 900 / 1000 / 1500 /
#   2000: 539 / 273 / 169 / 117 / 95 / 87 / 67 / 53 / 23 / 11 px. Solving each
#   for the distance the picture implies gives want^2 / 532 to +-1%, and 532 is
#   the fixture's own auto-fit distance (bound radius 443.4 * 1.2).
#
# ONE CAUSE, in GLView and not in the hook: center() does not centre. It sets
# `doCenter` and asks for a repaint, and setCenter() -- Pos = -bounds.centre,
# Dist = radius * 1.2, Zoom = 1 -- runs inside the NEXT paintGL. So the hook
# set the camera, one frame ran, and paintGL replaced it. The distance survived
# in mangled form only because the hook read Dist back and "corrected" it,
# which is what turned an inverse law into an inverse-square one.
#
# The fix is a PIN that cancels the queued auto-fit and re-asserts itself at
# the top of every paint (GLView::WwCameraPin), so a later reframe -- a second
# compile, a generated .lodl/.btd rebuilding its scene -- cannot take the
# camera back. It is armed only by WW_RENDER_CENTER / _DIST / _FOV / _ORTHO;
# a WW_RENDER_VIEW-only capture is left on the old auto-fit path, which is what
# the control below measures.
#
# THE PREDICTION, which is what makes this a gate rather than a demonstration.
# `upp` is the units per pixel at the look-at plane, so an extent of E world
# units in that plane spans E / upp pixels.
#
#   orthographic (WW_RENDER_ORTHO=<half-width W>):
#       upp  = 2 * W / viewportWidth              -- the eye distance is free
#       span = 512 * viewportWidth / ( 2 * W )    -- and the SAME at every DIST
#
#   perspective (WW_RENDER_FOV=<full vertical fov>, WW_RENDER_DIST=<eye>):
#       upp  = 2 * tan( fov / 2 ) * eye / viewportHeight
#       and the cube's silhouette is its FRONT face, 256 units nearer:
#       span = 512 * viewportHeight / ( 2 * tan( fov / 2 ) * ( eye - 256 ) )
#
# Both are compared to within 1 px, at three distances and on two views, and
# the viewport is read back from the PNG rather than assumed: WW_RENDER_SIZE is
# clamped to the desktop and returns a squashed capture without saying so.
#
# THE FLOORS, because "the number came out right" is also what a broken
# instrument says:
#   * the CONTROL: no camera switch at all, which must still give the old
#     auto-fit framing -- 2.4 * 443.405 = 1064.17 units of eye distance at
#     fov 60, so 231 px, a number that shares no factor with any pinned one;
#   * the pin must MOVE the picture: two look-ats 400 units apart must
#     separate the silhouette by exactly 400 / upp pixels. This is the check
#     that was red before the fix, on byte-identical files;
#   * a REFUSAL must be named, not swallowed: WW_RENDER_ORTHO=-5 has to appear
#     in the census as a refusal and leave the perspective arm serving;
#   * two runs of one pinned case must be BYTE-IDENTICAL.
CUBE="$WORK/cube512.nif"
cli new -o "$(winpath "$CUBE")" --cube --size 512 >/dev/null 2>&1
check "$( [ -s "$CUBE" ] && echo 1 || echo 0 )" "the 512-unit cube fixture was written" \
	"$(ls -l "$CUBE" 2>/dev/null | awk '{print $5}') bytes from -no-gui new --cube --size 512"

# THE SPAN INSTRUMENT. The silhouette edge is taken at HALF INTENSITY between
# the background and the flat face, with linear interpolation across the two
# crossing pixels, so the number is sub-pixel and unbiased -- counting "pixels
# that are exactly the face colour" undercounts by the width of the antialiased
# edge, which is the same size as the tolerance being tested.
SPANPY="$WORK/span.py"
cat > "$SPANPY" <<'PYEOF'
import sys, json
import numpy as np
from PIL import Image

def edges(prof, thr, bright):
    """Sub-pixel left and right crossings of thr in a 1-D profile."""
    m = (prof > thr) if bright else (prof < thr)
    idx = np.nonzero(m)[0]
    if idx.size == 0:
        return None
    # the widest contiguous run, so any stray marker cannot widen the answer
    runs, s = [], idx[0]
    for a, b in zip(idx[:-1], idx[1:]):
        if b != a + 1:
            runs.append((s, a)); s = b
    runs.append((s, idx[-1]))
    lo, hi = max(runs, key=lambda r: r[1] - r[0])

    def cross(i, j):
        # Where the straight line through prof[i] and prof[j] passes thr.
        # The (j - i) factor is not decoration: the right-hand edge is called
        # with j = i - 1, and without it the crossing lands one pixel OUTSIDE
        # the object and every span reads 1 px too wide. Caught on a hard
        # (unantialiased) edge, where the answer must come out exactly on the
        # pixel boundary: 637.5 and 868.5 for a 231.0 px cube.
        a, b = float(prof[i]), float(prof[j])
        if a == b:
            return float(j)
        return i + (thr - a) / (b - a) * (j - i)
    left = cross(lo - 1, lo) if lo > 0 else float(lo) - 0.5
    right = cross(hi + 1, hi) if hi + 1 < prof.size else float(hi) + 0.5
    return left, right

def measure(path):
    a = np.asarray(Image.open(path).convert("RGB")).astype(float)
    h, w, _ = a.shape
    lum = 0.299 * a[:, :, 0] + 0.587 * a[:, :, 1] + 0.114 * a[:, :, 2]
    bg = float(np.median([lum[0, 0], lum[0, w - 1], lum[h - 1, 0], lum[h - 1, w - 1]]))
    far = lum[np.abs(lum - bg) > 8.0]
    out = dict(w=w, h=h, bg=bg, n=int(far.size), spanx=0.0, spany=0.0, cx=0.0, cy=0.0, face=bg)
    if far.size < 64:                       # the floor: no object, no number
        return out
    vals, counts = np.unique(np.round(far).astype(int), return_counts=True)
    face = float(vals[int(np.argmax(counts))])
    out["face"] = face
    bright = face > bg
    thr = 0.5 * (bg + face)
    ex = edges(lum[h // 2], thr, bright)
    if ex:
        out["spanx"] = ex[1] - ex[0]
        out["cx"] = 0.5 * (ex[0] + ex[1])
    ey = edges(lum[:, int(round(out["cx"])) if ex else w // 2], thr, bright)
    if ey:
        out["spany"] = ey[1] - ey[0]
        out["cy"] = 0.5 * (ey[0] + ey[1])
    return out

print(json.dumps(measure(sys.argv[1])))
PYEOF

# A camera run: no window/pixel samplers (section 5 owns those), its own census
# file, and the cube. RC comes back in a global, the way run() does it.
CAMRC=0
camshot() { # label  -- the caller exports the WW_RENDER_* under test
	rm -f "$WORK/$1.png" "$WORK/$1.camera"
	export WW_RENDER_SHOT="$(winpath "$WORK/$1.png")"
	export WW_RENDER_SIZE=640x480 WW_RENDER_TIME=1 WW_RENDER_CLEAN=1
	export WW_CAMERA_CENSUS="$(winpath "$WORK/$1.camera")"
	timeout "$CAP" "$EXE" --port "$PORT" "$CUBE" >"$WORK/$1.out" 2>&1
	CAMRC=$?
	unset WW_RENDER_SHOT WW_RENDER_SIZE WW_RENDER_TIME WW_RENDER_CLEAN WW_CAMERA_CENSUS
}
sp()   { "$PY" "$(winpath "$SPANPY")" "$(winpath "$WORK/$1.png")" 2>/dev/null; }
spget() { sp "$1" | "$PY" -c "import sys,json;print(json.load(sys.stdin)['$2'])" 2>/dev/null; }
camfield() { sed -n "s/.* $2=\([^ ]*\).*/\1/p" "$WORK/$1.camera" 2>/dev/null | tail -1; }
near() { awk -v a="$1" -v b="$2" -v t="$3" 'BEGIN{ d=a-b; if(d<0)d=-d; print (d<=t)?1:0 }'; }

# --- the control: no camera switch, so the old auto-fit framing, unchanged
export WW_RENDER_VIEW=5
camshot cam_control
unset WW_RENDER_VIEW
CTL_W=$(spget cam_control w); CTL_H=$(spget cam_control h); CTL_SPAN=$(spget cam_control spanx)
check "$( [ "$CAMRC" -eq 0 ] && [ -s "$WORK/cam_control.png" ] && echo 1 || echo 0 )" \
	"the control run photographed the cube" "rc=$CAMRC, ${CTL_W}x${CTL_H} px"
check "$( [ "$(camfield cam_control arm)" = "unpinned" ] && echo 1 || echo 0 )" \
	"and no pin was armed" "arm=$(camfield cam_control arm)"
# 512-unit cube, auto-fit eye 2 * 1.2 * 443.405 = 1064.17, fov 60, front face
# 256 nearer: 512 * H / ( 2 * tan30 * ( 1064.17 - 256 ) ).
CTL_PRED=$(awk -v h="$CTL_H" 'BEGIN{ printf "%.2f", 512.0*h/(2.0*0.5773503*(1064.17-256.0)) }')
check "$(near "$CTL_SPAN" "$CTL_PRED" 1.0)" "and framed it the way it always did" \
	"span $CTL_SPAN px, auto-fit predicts $CTL_PRED"

# --- ORTHOGRAPHIC: the metric arm. Three eye distances, two views, one scale.
ORTHO_W=1024
ORTHO_PRED=$(awk -v w="$CTL_W" -v o="$ORTHO_W" 'BEGIN{ printf "%.2f", 512.0*w/(2.0*o) }')
echo "  ..   orthographic half-width $ORTHO_W -> the cube must span $ORTHO_PRED px at every distance"
for V in 5 4; do
	for D in 500 1000 2000; do
		export WW_RENDER_VIEW=$V WW_RENDER_ORTHO=$ORTHO_W WW_RENDER_DIST=$D WW_RENDER_CENTER=0,0,256
		camshot "cam_o${V}_$D"
		unset WW_RENDER_VIEW WW_RENDER_ORTHO WW_RENDER_DIST WW_RENDER_CENTER
		S=$(spget "cam_o${V}_$D" spanx)
		check "$(near "$S" "$ORTHO_PRED" 1.0)" \
			"ortho view $V at eye $D spans what the projection says" \
			"$S px vs $ORTHO_PRED (persp=$(camfield "cam_o${V}_$D" persp), upp=$(camfield "cam_o${V}_$D" upp))"
	done
done
# The census is not decoration: its own units-per-pixel must agree with the one
# computed here from W and the PNG, or one of the two is lying.
UPP_SAID=$(camfield cam_o5_1000 upp)
UPP_CALC=$(awk -v w="$CTL_W" -v o="$ORTHO_W" 'BEGIN{ printf "%.6f", 2.0*o/w }')
check "$(near "$UPP_SAID" "$UPP_CALC" 0.001)" "the census agrees with the arithmetic" \
	"census upp=$UPP_SAID, computed $UPP_CALC"
check "$( [ "$(camfield cam_o5_1000 vp)" = "${CTL_W}x${CTL_H}" ] && echo 1 || echo 0 )" \
	"and reports the viewport the PNG actually has" \
	"census vp=$(camfield cam_o5_1000 vp), png ${CTL_W}x${CTL_H}"

# --- the ortho SCALE LAW: halving the half-width must double the span.
for O in 2048 4096; do
	export WW_RENDER_VIEW=5 WW_RENDER_ORTHO=$O WW_RENDER_DIST=1000 WW_RENDER_CENTER=0,0,256
	camshot "cam_ow$O"
	unset WW_RENDER_VIEW WW_RENDER_ORTHO WW_RENDER_DIST WW_RENDER_CENTER
	S=$(spget "cam_ow$O" spanx)
	P=$(awk -v w="$CTL_W" -v o="$O" 'BEGIN{ printf "%.2f", 512.0*w/(2.0*o) }')
	check "$(near "$S" "$P" 1.0)" "ortho half-width $O gives the span it should" "$S px vs $P"
done

# --- PERSPECTIVE: the arm the transition renders use, where DIST is a real
# world distance and the scale has to follow it.
for D in 500 1000 2000; do
	export WW_RENDER_VIEW=5 WW_RENDER_FOV=60 WW_RENDER_DIST=$D WW_RENDER_CENTER=0,0,256
	camshot "cam_p$D"
	unset WW_RENDER_VIEW WW_RENDER_FOV WW_RENDER_DIST WW_RENDER_CENTER
	S=$(spget "cam_p$D" spanx)
	P=$(awk -v h="$CTL_H" -v d="$D" 'BEGIN{ printf "%.2f", 512.0*h/(2.0*0.5773503*(d-256.0)) }')
	check "$(near "$S" "$P" 1.0)" "perspective at eye $D spans what the projection says" \
		"$S px vs $P (persp=$(camfield "cam_p$D" persp), eye=$(camfield "cam_p$D" eye))"
done
# ANTI-VACUITY: three distances that all photographed the same thing would pass
# every check above if the prediction were also constant. It is not, and the
# census has to move with it.
check "$( [ "$(camfield cam_p500 upp)" != "$(camfield cam_p1000 upp)" ] && \
	[ "$(camfield cam_p1000 upp)" != "$(camfield cam_p2000 upp)" ] && echo 1 || echo 0 )" \
	"and the census moved with the distance" \
	"upp $(camfield cam_p500 upp) / $(camfield cam_p1000 upp) / $(camfield cam_p2000 upp)"
check "$( [ "$(camfield cam_o5_500 upp)" = "$(camfield cam_o5_2000 upp)" ] && echo 1 || echo 0 )" \
	"while an orthographic scale did NOT" \
	"upp $(camfield cam_o5_500 upp) at eye 500 and $(camfield cam_o5_2000 upp) at 2000"

# --- THE LOOK-AT. This is the check that was red on byte-identical files.
export WW_RENDER_VIEW=5 WW_RENDER_ORTHO=$ORTHO_W WW_RENDER_DIST=1000 WW_RENDER_CENTER=400,0,256
camshot cam_moved
unset WW_RENDER_VIEW WW_RENDER_ORTHO WW_RENDER_DIST WW_RENDER_CENTER
CX0=$(spget cam_o5_1000 cx); CX1=$(spget cam_moved cx)
MOVE_PRED=$(awk -v w="$CTL_W" -v o="$ORTHO_W" 'BEGIN{ printf "%.2f", 400.0*w/(2.0*o) }')
MOVED=$(awk -v a="$CX0" -v b="$CX1" 'BEGIN{ d=b-a; if(d<0)d=-d; printf "%.2f", d }')
check "$(near "$MOVED" "$MOVE_PRED" 1.0)" "a look-at 400 units across moves it 400 units" \
	"silhouette centre moved $MOVED px, 400 units is $MOVE_PRED px"
check "$( [ "$(sha256sum "$WORK/cam_o5_1000.png" | cut -d' ' -f1)" != \
	"$(sha256sum "$WORK/cam_moved.png" | cut -d' ' -f1)" ] && echo 1 || echo 0 )" \
	"and the two files are not the same file" \
	"$(sha256sum "$WORK/cam_o5_1000.png" | cut -c1-16) / $(sha256sum "$WORK/cam_moved.png" | cut -c1-16)"

# --- A REFUSAL IS NAMED (CONSTITUTION 10: a fallback is never silent).
export WW_RENDER_VIEW=5 WW_RENDER_ORTHO=-5 WW_RENDER_FOV=60 WW_RENDER_DIST=1000 WW_RENDER_CENTER=0,0,256
camshot cam_refuse
unset WW_RENDER_VIEW WW_RENDER_ORTHO WW_RENDER_FOV WW_RENDER_DIST WW_RENDER_CENTER
check "$( case "$(camfield cam_refuse arm)" in *refused-WW_RENDER_ORTHO*) echo 1;; *) echo 0;; esac )" \
	"a bad half-width is refused BY NAME" "arm=$(camfield cam_refuse arm)"
check "$( [ "$(camfield cam_refuse persp)" = "1" ] && echo 1 || echo 0 )" \
	"and the perspective arm served instead" \
	"persp=$(camfield cam_refuse persp), fov=$(camfield cam_refuse fov)"

# --- DETERMINISM: same switches, same bytes.
export WW_RENDER_VIEW=5 WW_RENDER_ORTHO=$ORTHO_W WW_RENDER_DIST=1000 WW_RENDER_CENTER=0,0,256
camshot cam_o5_1000_again
unset WW_RENDER_VIEW WW_RENDER_ORTHO WW_RENDER_DIST WW_RENDER_CENTER
SHA_A=$(sha256sum "$WORK/cam_o5_1000.png" | cut -d' ' -f1)
SHA_B=$(sha256sum "$WORK/cam_o5_1000_again.png" | cut -d' ' -f1)
check "$( [ "$SHA_A" = "$SHA_B" ] && echo 1 || echo 0 )" "two runs of one pinned camera are byte-identical" \
	"${SHA_A:0:16} / ${SHA_B:0:16}"

# --- AND ON A GENERATED DOCUMENT, which is where the pin used to be lost for a
# second reason: a .lodl/.btd is BUILT rather than parsed and reframes itself
# after the hook has run. Skipped, loudly, when no such file is on this machine
# -- they are worldspace bakes and the repo does not carry one.
GENDOC="${WW_RENDER_SHOT_LODL:-E:/Projects/Fallout 4 Mods/mods/FO4CS/Terrain/DiamondCity.lodl}"
if [ -f "$GENDOC" ]; then
	for D in 40000 80000; do
		rm -f "$WORK/cam_gen$D.png" "$WORK/cam_gen$D.camera"
		WW_RENDER_SHOT="$(winpath "$WORK/cam_gen$D.png")" WW_RENDER_SIZE=640x480 \
		WW_RENDER_CLEAN=1 WW_RENDER_VIEW=1 WW_RENDER_ORTHO=$D WW_RENDER_DIST=200000 \
		WW_RENDER_CENTER=0,0,0 WW_CAMERA_CENSUS="$(winpath "$WORK/cam_gen$D.camera")" \
			timeout "$CAP" "$EXE" --port "$PORT" "$GENDOC" >"$WORK/cam_gen$D.out" 2>&1
	done
	G1=$(camfield cam_gen40000 upp); G2=$(camfield cam_gen80000 upp)
	check "$( [ -s "$WORK/cam_gen40000.png" ] && [ -s "$WORK/cam_gen80000.png" ] && echo 1 || echo 0 )" \
		"a generated document photographed at all" \
		"$(ls -l "$WORK/cam_gen40000.png" 2>/dev/null | awk '{print $5}') bytes"
	check "$( [ -n "$G1" ] && [ "$G1" != "$G2" ] && echo 1 || echo 0 )" \
		"and the pin held on it too" "upp $G1 at half-width 40000, $G2 at 80000"
	check "$( [ "$(sha256sum "$WORK/cam_gen40000.png" | cut -d' ' -f1)" != \
		"$(sha256sum "$WORK/cam_gen80000.png" | cut -d' ' -f1)" ] && echo 1 || echo 0 )" \
		"two half-widths on it are two different pictures" \
		"$(sha256sum "$WORK/cam_gen40000.png" | cut -c1-16) / $(sha256sum "$WORK/cam_gen80000.png" | cut -c1-16)"
else
	echo "  ..   NOT RUN: no generated document on this machine ($GENDOC)."
	echo "  ..   Point WW_RENDER_SHOT_LODL at a .lodl or .btd to run the three checks it gates."
fi

echo
echo "render_shot.sh: $checks checks, $fails failures"
[ "$fails" -eq 0 ] && echo PASS || echo FAIL
[ "$fails" -eq 0 ]
