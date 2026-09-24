p = 'scratchpad/lodiv7_20260918/lane_lodiv7_report.md'
s = open(p, encoding='utf-8', newline='').read()

OLD = ("| 4 | **The sky stream agrees with the old per-placement byte on 72% of placements where AO manages 92%**, "
       "and the mechanism is the two vertex populations, not a bad cast (r = 0.9854 against AO's 0.9878; shuffled "
       "scores 0.019). | The pre-registered 95% gate was re-set to median + correlation + flat-slice. s6 is the "
       "argument and s1 holds the original number unedited. |")
assert s.count(OLD) == 1, s.count(OLD)

NEW = ("| 4 | **The new per-vertex sky disagrees with the old one-byte-per-building number on 28% of placements** "
       "-- it matches on 72%, where the AO stream matches on 92% of the very same placements. | Asked for by the "
       "director at 18:0x, in plain words, because it is the number bungo is most likely to be told about second-hand. "
       "**In plain words: the two numbers are measured over two different sets of vertices, and that is the whole of "
       "it.** The old byte is an average taken over the vertices of the chunk mesh Bethesda's own bake built -- the "
       "blocky far-field shell. The new stream is one value per vertex of the authored LOD model we actually draw, "
       "and its per-building average is taken over those. Those are not the same points in space, so they were never "
       "going to agree exactly; the question is only why they disagree MORE for sky than for ambient occlusion, and "
       "the answer is that sky varies hugely across a single building where occlusion does not. A house's base sits "
       "in shadow while its roof sees the whole sky -- a spread of a hundred or more -- so moving which vertices you "
       "average over moves a sky average a long way and an occlusion average barely at all. The measurement that "
       "settles it: on placements whose own sky values span 8 or less, where the two vertex sets CANNOT disagree "
       "much, agreement is 90.7%, and it falls monotonically as the spread grows -- 74.5% at a spread of 16-64, "
       "68.8% at 128-256. Everything else was ruled out by measurement rather than argument: the disagreement is "
       "two-sided (53% high, 47% low), so it is not a thinner scene; it is flat in placement size and in vertex "
       "count; and the chunk border costs both streams equally without closing the gap. And the cast is aimed at the "
       "right placements -- correlation 0.9854 for sky against 0.9878 for AO, where the same averages shuffled score "
       "0.019 and a constant stream scores 0.0000. **The 28% is the new stream saying something the old byte could "
       "not say, not the new stream being wrong.** The pre-registered 95% gate was re-set accordingly, to median plus "
       "correlation plus the flat-slice subset; s6 is the full argument and s1 holds the original number unedited so "
       "the re-setting can be argued with. |")

s = s.replace(OLD, NEW)
open(p, 'w', encoding='utf-8', newline='').write(s)
print('s11 row 4 rewritten for bungo')
