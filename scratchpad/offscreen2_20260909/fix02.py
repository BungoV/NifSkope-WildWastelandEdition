import sys
p = 'tests/spells/render_shot.sh'
b = open(p, 'rb').read()
before_cr = b.count(b'\r')
t = b.decode()

subs = [
    # The header note beside the _harness.sh source still described the dead
    # off-every-screen placement.
    ("""# Second monitor, one instance at a time; _harness.sh owns the coordinates.
# Since 2026-09-09 WW_WINDOW_AT only decides anything when WW_WINDOW_VISIBLE=1
# asks for a visible window: a headless run is placed off every screen.""",
     """# Second monitor, one instance at a time; _harness.sh owns the coordinates.
# WW_WINDOW_AT is honoured for a headless run AND for the visible control, but
# only if the point is not on the primary screen: wwHeadlessWindowOrigin()
# refuses a main-monitor position and falls back to the first non-primary
# screen, naming the refusal in the window log's arm= field."""),
    # Two seconds of noise sampling gave six samples: a CopyFromScreen +
    # DrawImage + 64 GetPixel round costs ~80 ms and PowerShell takes over a
    # second to start.
    ("""	-X "$SAMP_X" -Y "$SAMP_Y" -W "$SAMP_W" -H "$SAMP_H" >/dev/null 2>&1 &
NOISE_PID=$!
sleep 2""",
     """	-X "$SAMP_X" -Y "$SAMP_Y" -W "$SAMP_W" -H "$SAMP_H" >/dev/null 2>&1 &
NOISE_PID=$!
# Six seconds, not two: PowerShell takes over a second to start and a sample
# round costs ~80 ms, so two seconds produced six samples and the floor below
# failed on its own instrument.
sleep 6"""),
]

for old, new in subs:
    n = t.count(old)
    if n != 1:
        sys.stderr.write('anchor %d matches: %r\n' % (n, old[:60]))
        sys.exit(1)
    t = t.replace(old, new)

out = t.encode()
assert out.count(b'\r') == before_cr, 'CR count moved'
open(p, 'wb').write(out)
print('CR', out.count(b'\r'), 'LF', out.count(b'\n'), 'bytes', len(out))
