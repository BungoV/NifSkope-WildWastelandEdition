"""ROADS3's un-spliced deliverables still recommend `--road-detail 0`.

bungo overruled that on 2026-09-12 after looking at both bakes -- "--road-detail
1 is always on, do not ever use road detail 0, that looks terrible" -- and
docs/LODGEN_TERRAIN_VT.md 1a.5d paragraph 7 already carries the overrule. The
report, the changelog entry and the handoff block do not, and they are what the
director splices, so they would publish an overruled recommendation.

The MEASUREMENT is untouched everywhere: vanilla really does keep none of the
road texture's own detail, and that is why the flag exists. What is removed is
the lane's recommendation that the DEFAULT stay 0.

Written as a file: prose apostrophes, and heredocs arrive CRLF.
"""
import io
import sys

R = r'E:\Projects\NifskopeWildWastelandEdition'
RULING = (u'bungo looked at both bakes on 2026-09-12 and ruled *"--road-detail '
          u'1 is always\non, do not ever use road detail 0, that looks '
          u'terrible"*')


def edit(rel, pairs, guard):
    p = R + '\\' + rel
    s = io.open(p, encoding='utf-8', newline='').read()
    if guard in s:
        print('%-46s already carries the overrule' % rel)
        return
    for old, new in pairs:
        if s.count(old) != 1:
            sys.exit('REFUSED: %d matches in %s for %r' % (s.count(old), rel, old[:60]))
        s = s.replace(old, new)
    io.open(p, 'w', encoding='utf-8', newline='').write(s)
    d = io.open(p, 'rb').read()
    print('%-46s amended (%d bytes, CR %d)' % (rel, len(d), d.count(b'\r')))


GUARD = 'that looks terrible'

# ------------------------------------------------------------------ report
edit(r'scratchpad\lane_roads3_report.md', [
    (u"### 1.5 Vanilla keeps NO road-texture detail -- `--road-detail` stays 0",
     u"### 1.5 Vanilla keeps NO road-texture detail -- and bungo has since ruled that this does NOT decide the default"),
    (u"| F1 detail strength | **MET** | correlation +0.0275 against a phase-twin floor of 0.0270 mean / 0.0644 max; +0.0162 against 0.0124 / 0.0163; best-fit strength negative. `--road-detail` stays 0 |",
     u"| F1 detail strength | **MET -- null result, and OVERRULED as a default** | correlation +0.0275 against a phase-twin floor of 0.0270 mean / 0.0644 max; +0.0162 against 0.0124 / 0.0163; best-fit strength negative. The measurement stands; " + RULING.replace('\n', ' ') + u", so it is not what picks the default. See the note below section 1.5 |"),
    (u"| **F1** detail strength | **MET -- null result** | correlation +0.0275 vs phase-twin floor 0.0270 / 0.0644; +0.0162 vs 0.0124 / 0.0163. `--road-detail` stays 0 |",
     u"| **F1** detail strength | **MET -- null result, OVERRULED as a default** | correlation +0.0275 vs phase-twin floor 0.0270 / 0.0644; +0.0162 vs 0.0124 / 0.0163. The measurement stands; bungo ruled `--road-detail 1` always on, 2026-09-12, after seeing both |"),
], GUARD)

# ------------------------------------------------------------ WW_CHANGES
edit(r'scratchpad\roads3_20260911\WW_CHANGES_ENTRY.md', [
    (u"negative. `--road-detail` stays at 0; ROADS2's default is confirmed by a",
     u"negative. That is what vanilla does, and it is **not** what decides how ours\nshould look: " + RULING + u". The measurement is why the flag exists; his eye\npicks its default. The rest of this paragraph is the measurement, kept because a\ntest that could have overturned it did not, and"),
], GUARD)

# ------------------------------------------------------------- HANDOFF
edit(r'scratchpad\roads3_20260911\HANDOFF_BLOCK.md', [
    (u"  ROADS2's `--road-detail 0` default is confirmed by a test that could have\n  overturned it.",
     u"  **That measurement is now OVERRULED as a default-picker**: " + RULING.replace('\n', '\n  ') + u", and\n  `docs/LODGEN_TERRAIN_VT.md` 1a.5d paragraph 7 carries the overrule. Vanilla\n  really does keep no road-texture detail -- that is why the flag exists -- and\n  it is not what chooses ours. Nothing in this lane's own change depends on it:\n  ROADS3 never touched `--road-detail`."),
], GUARD)
