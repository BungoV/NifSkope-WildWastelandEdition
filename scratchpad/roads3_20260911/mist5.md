## 2026-09-12 -- ROADS3 -- a measurement was allowed to carry a recommendation about how something should LOOK

**What was done.** ROADS3 measured whether vanilla's far road keeps any of the
road texture's own detail. It does not: the residual after the best wash
correlates with our full-detail bake's departure from its flat average at
+0.0275 against a phase-twin floor of 0.0270 / 0.0644, and +0.0162 against
0.0124 / 0.0163, with the best-fit strength negative. That measurement is sound
and a test that could have overturned it did not.

**What was wrong.** The lane then wrote, in the report, in the changelog entry,
in the handoff block and in the format contract, that **`--road-detail` stays
0** -- "ROADS2's default is confirmed". The measurement confirmed no such thing.
It answered *what Bethesda's sheets contain*; it was quoted as an answer to
*what our roads should look like*, which is a different question and not one a
correlation can settle.

**How it was found.** bungo looked at the two bakes side by side on 2026-09-12
and said *"--road-detail 1 is always on, do not ever use road detail 0, that
looks terrible"*. The solid-colour ribbons he had complained about in the brief
that started this lane were partly that default. So the lane's own opening
complaint and the lane's own recommendation pointed in opposite directions and
nobody noticed, because the recommendation arrived wearing a number.

**The rule.** A measurement of vanilla settles what vanilla does, and may set a
floor, a ceiling or a gate. It does not settle taste. When a lane's finding
touches how something LOOKS, the lane produces the picture and the number and
**stops there** -- the default is bungo's call, named as his call, in the
report's "bungo's calls" section and nowhere else. ROADS3 did exactly this for
`--road-opacity`, correctly, in the same report, and then failed to do it for
`--road-detail` one section earlier. The tell is the verb: "stays", "is
confirmed", "should be" in a sentence whose only evidence is a correlation.
