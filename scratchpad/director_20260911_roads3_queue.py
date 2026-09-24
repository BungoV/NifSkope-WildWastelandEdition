"""Director 21:1x: bungo's ruling on ROADS2's picture -> lane ROADS3 queued after GRADE1; ruling paragraph; MISTAKES entry. LF-only, CR asserted 0."""
R = "E:/Projects/NifskopeWildWastelandEdition/"
p = R + "HANDOFF.md"
b = open(p, "rb").read(); assert b.count(b"\r") == 0
old_q = b"  its tone curve on this lane's colour), GRADE1 (brief_grade1.md), INCR1 (brief_incr1.md),"
new_q = b"""  its tone curve on this lane's colour), GRADE1 (brief_grade1.md), ROADS3
  (brief_roads3.md, added 21:1x on his judgement of ROADS2's picture --
  after GRADE1 so the road's own gap is measured on graded ground), INCR1 (brief_incr1.md),"""
assert b.count(old_q) == 1
b = b.replace(old_q, new_q)
anchor = b"  RULING 19:2x (bungo, over the \"OURS default 341.3333\" panel of\n"
assert b.count(anchor) == 1
ruling = b"""  RULING 21:1x (bungo, over scratchpad/roads2_20260911/images/
  cmp_sanctuary_road_v2.png, "ours --roads (the new defaults)"): "ours
  before and after roads, looks like an issue, no diffuse is sampled from
  the road, only like solid colors" ... "our road wasn't flat before, but
  now it is, and it still has the seam at the edges of it" -> ROADS2's
  default `--road-detail 0` (diffuse flattened to its average) and the
  opaque hard-edged paint are NOT accepted as the shipped look; vanilla's
  road is a soft blue-grey wash the ground shows through, and ours is
  two-tone (skirt band around a darker core). Lane ROADS3
  (brief_roads3.md): fit vanilla's per-texel road opacity and edge
  profile, the residual detail strength, the hue; cross-road profile
  gate (no step where vanilla has none); switches off == rung bytes;
  `--roads-legacy` stays the way back.
"""
b = b.replace(anchor, ruling + anchor)
assert b.count(b"\r") == 0
open(p, "wb").write(b)
print("HANDOFF ok")

p = R + "MISTAKES.md"
m = open(p, "rb").read(); assert m.count(b"\r") == 0
entry = b"""## 2026-09-11 21:1x -- director: ROADS2's seam gate went green on a road bungo rejected on sight

**What was done.** The ROADS2 brief pre-registered the seam as a piece-boundary discontinuity metric; the lane closed it (11.8 -> 4.0 vs vanilla 4.2) by flattening the road diffuse to its whole-texture average and painting it opaque, and shipped that as the default. The director accepted the green and sent the picture.

**What was true instead.** bungo, on the picture: "no diffuse is sampled from the road, only like solid colors" and "it still has the seam at the edges of it". Vanilla's road is a soft wash the ground shows through; ours became a flat two-tone ribbon (skirt band around a darker core). The metric measured piece joins; the seam he sees is the cross-road step between the skirt pieces and the surface, which the metric never looked at, and the flatness is the cure's own side effect (the lane's own report had it as "too smooth, SD 4.37 vs 6.62" under "still red").

**How it was found.** bungo looked at the picture.

**The rule that prevents it.** A gate that goes green by removing the signal is not a gate; the brief must carry a "not worse than vanilla on the thing the fix could destroy" floor (here: road SD and the cross-road profile) beside the metric it asks to close. And a lane's own "still red" list is read by the director BEFORE the picture is sent, and named in the message with it -- the director summarised "interior too smooth" as a footnote instead of as the headline it was.
"""
marker = entry.split(b"\n", 1)[0]
if m.count(marker) == 0:
    m = m.rstrip(b"\n") + b"\n\n" + entry
    assert m.count(b"\r") == 0
    open(p, "wb").write(m); print("MISTAKES appended")
else:
    print("MISTAKES present")
