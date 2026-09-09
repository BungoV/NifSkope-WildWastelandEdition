import sys
p = 'tests/spells/render_shot.sh'
b = open(p, 'rb').read()
before_cr = b.count(b'\r')
t = b.decode()

subs = [
    # POWERSHELL VARIABLES ARE CASE-INSENSITIVE. $B was the window's bottom edge
    # and $b was the screen's Bounds rectangle: the assignment clobbered the
    # bottom, and every comparison threw "not IComparable", so the sampler wrote
    # nothing at all for a whole gate run. Caught by its own floor.
    ("""      foreach ($s in [System.Windows.Forms.Screen]::AllScreens) {
        $b = $s.Bounds
        if (($L -lt ($b.X + $b.Width)) -and ($R -gt $b.X) -and
            ($T -lt ($b.Y + $b.Height)) -and ($B -gt $b.Y)) {""",
     """      foreach ($s in [System.Windows.Forms.Screen]::AllScreens) {
        # $scr, NOT $b: PowerShell variables are case-insensitive, so $b would
        # be the same variable as $B, the window's bottom edge.
        $scr = $s.Bounds
        if (($L -lt ($scr.X + $scr.Width)) -and ($R -gt $scr.X) -and
            ($T -lt ($scr.Y + $scr.Height)) -and ($B -gt $scr.Y)) {"""),

    # THE PIXEL BAR. A bar of "desktop noise + 2" was too tight, because the
    # sampled region of the second monitor contains whatever else is on that
    # monitor: one console print during the longest run stepped the region's
    # mean by 5.4 and failed a run in which every other instrument said the
    # window was invisible. The three amplitudes were measured in one run:
    #   0.2   the region left alone
    #   5.4   a console window printing into the same region
    #  27.4   an OPAQUE NifSkope window appearing there
    # 250.6   the black/white matte strobing on an opaque window
    # 15 sits above desktop activity and well under half of a window appearing,
    # let alone a strobe. It is not a threshold chosen to pass: both floors in
    # section 6 are measured against it in the same run, and the strobe floor
    # keeps its own bar of 30.
    ("""NOISE_BAR=$(awk -v n="$NOISE_RANGE" 'BEGIN{ b=n+2.0; if(b<3.0) b=3.0; printf "%.3f", b }')""",
     """NOISE_BAR=$(awk -v n="$NOISE_RANGE" 'BEGIN{ b=3.0*n; if(b<15.0) b=15.0; printf "%.3f", b }')"""),
    ("""# The bar every hidden run has to stay under. The desktop's own range plus a
# margin, and never less than 3 luminance steps -- a monitor's dither and a
# blinking caret are not a strobe, and a bar of 0 would fail on those.""",
     """# The bar every hidden run has to stay under. The sampler sees the whole
# region, NifSkope's window and whatever else the second monitor has there, so
# the bar has to clear ordinary desktop activity: a console printing into the
# region moved it by 5.4 on 2026-09-09. Measured amplitudes, one run: 0.2 the
# region left alone, 5.4 a console print, 27.4 an opaque window appearing,
# 250.6 the matte strobing. 15 separates them by better than 1.8x in both
# directions, and both floors below are measured against it."""),
]

for old, new in subs:
    n = t.count(old)
    if n != 1:
        sys.stderr.write('anchor %d matches: %r\n' % (n, old[:70]))
        sys.exit(1)
    t = t.replace(old, new)

out = t.encode()
assert out.count(b'\r') == before_cr, 'CR count moved'
open(p, 'wb').write(out)
print('CR', out.count(b'\r'), 'LF', out.count(b'\n'), 'bytes', len(out))
