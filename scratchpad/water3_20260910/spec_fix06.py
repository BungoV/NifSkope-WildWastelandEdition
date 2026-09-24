# -*- coding: utf-8 -*-
"""spec_water.md section 7's P gates, restated against what lane WATER3 built."""
import sys
P = '../specs_20260909/spec_water.md'
t = open(P, 'rb').read().decode('utf-8')

OLD = """| gate | pass condition |
|---|---|
| P1 isolation | §5.4 steps 1-4: 0 texels outside the body changed, ≥ 60% inside it did |
| P2 the refuter fires | §5.4 step 5 shows the test RED before it is trusted green |
| P3 undo | §5.4 step 6, byte-identical |
| P4 survives a re-bake | a stroke laid at sample rate 32, re-baked at 8, still points the same body the same way (angle within 5°) |
| P5 panel style | the counts in §5.3 with their floors, in `WW_WATER_TEST` |
| P6 the picture | `SHOT=<png>` dock grab, and a body-ID render of the Charles region through the render hook, before and after a stroke |"""

NEW = """| gate | pass condition | as built |
|---|---|---|
| P0 identity (ADDED) | on an unmarked file, the tool's body-table encoder and its flow-plane packer reproduce the WRITER's own bytes exactly | the first two checks of the model half; everything else rests on them |
| P1 isolation | §5.4 steps 1-4: 0 texels outside the body changed, ≥ 60 per cent inside it did | measured by a WHOLE-PLANE sweep, not over the marked body's bounding box — "only the bbox could have moved" is the claim under test |
| P2 the refuter fires | §5.4 step 5 shows the test RED before it is trusted green | the neighbour is marked FIRST and the river's own changed-texel count printed as 0 |
| P3 undo | §5.4 step 6, byte-identical | strengthened to the WHOLE FILE, not the plane |
| P4 survives a re-bake | a stroke laid at sample rate 32, re-baked at 8, still points the same body the same way (angle within 5°) | **the mechanism shipped, the gate is NOT in the harness.** `--water-flow-samples` is the WRITER's switch and the writer does not read the stroke store; what ships instead is `WaterMarkDoc::setFlowRate` (the Bake section's row), which re-derives the flow plane at 8 or 16 from the same world-coordinate strokes. Wiring the angle comparison into `water_mark.sh` is owed |
| P5 panel style | the counts in §5.3 with their floors, in `WW_WATER_MARK_TEST` | the dock half |
| P6 the picture | `SHOT=<png>` dock grab, and a flow render of the Charles region through the render hook, before and after a stroke | the grab is in the harness; the before/after pair is owed with the build (`Fallout4.exe` was up) |
| P7 dry land (ADDED) | a stroke whose first point is dry is refused in words and stored nowhere | both halves check it |
| P8 round trip (ADDED) | save, reopen, save again is byte-identical, and the strokes come back OUT OF THE FILE | the model half |"""

n = t.count(OLD)
if n != 1:
    sys.stderr.write('REFUSED: %d matches\n' % n)
    sys.exit(1)
t = t.replace(OLD, NEW)
open(P, 'wb').write(t.encode('utf-8'))
nb = open(P, 'rb').read()
print('ok: %d bytes, %d lines, CR=%d' % (len(nb), nb.count(b'\n'), nb.count(b'\r')))
