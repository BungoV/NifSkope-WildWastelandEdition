# -*- coding: utf-8 -*-
"""spec_water.md 5.3 and 5.4, as the tool was actually built."""
import sys
P = '../specs_20260909/spec_water.md'
t = open(P, 'rb').read().decode('utf-8')
edits = []


def rep(old, new):
    edits.append((old, new))


OLD53 = """A **Water** section (`wwHeading`, never a `QGroupBox` title) in the LOD
Generation panel, one `label | field` `QGridLayout`, one setting a row, the
shared `labelW`, whole-word labels with the explanation in the tooltip:

| row | control | notes |
|---|---|---|
| Tool | `QComboBox` + `wwMatchFieldStyle` | Stroke / Pin / Barrier / Merge / Erase |
| Speed | `QDoubleSpinBox` + `wwMakeScrubField` | world units per second |
| Width | `QDoubleSpinBox` + `wwMakeScrubField` | world units, the influence radius |
| Class | `QComboBox` + `wwMatchFieldStyle` | Automatic / Sea / River / Lake — sets flags bit5 |
| Colour | colour button + tick | writes the body record's RGBA; A = 0 clears it |
| Name | `QLineEdit` | the name blob |

and a folding **Bake** section (`LodgenSection`, arrow folds, box enables, fold
persists):

| row | control |
|---|---|
| Body IDs | tick |
| Flow | tick |
| Shore distance | tick |
| Samples per cell | `QComboBox` 8 / 16 / 32 |
| Bridge gap | `QSpinBox` + `wwMakeScrubField`, texels, default 2 |

Under both, pinned outside the scroll area: the **selected-body summary**
(*"body 233 · ExtRiverCharlesUpper · river · 25,112 texels · plane 1300.0 ·
flow from a stroke"*) and the action bar, whose sentence `refreshSummary()`
owns — *"Bake 3 water planes into Commonwealth.lodl (590 bodies, 12 strokes)"*
or the ONE reason it cannot, e.g. *"no .lodl open"*. A greyed button with no
sentence beside it is a broken button."""

NEW53 = """**AS BUILT it is its OWN dock, not a section of the LOD Generation panel**
(`src/watermarkpanel.cpp`, in the Workspaces dropdown beside the other manager
docks, and hiding them the way they hide each other). The reason is the file
rule again — the LOD panel is `src/lodgenmanager.cpp`, another lane's file —
and, as with the canvas, it is the better shape: this dock stays open while its
own map is being drawn on, and the LOD panel's job is a bake that runs for
minutes.

`wwHeading` for every section (never a `QGroupBox` title), one
`label | field` `QGridLayout` a section, one setting a row, one `labelW` for the
page, whole-word labels with the explanation in the tooltip:

| section | row | control | notes |
|---|---|---|---|
| Landscape file | File | `QLineEdit` + Browse | the version-3 `.lodl` being marked |
| | Show | `QComboBox` + `wwMatchFieldStyle` | Body ID / Flow / Shore distance — which plane the map paints |
| Marking | Tool | `QComboBox` + `wwMatchFieldStyle` | Stroke / Pin / Source pin / Outlet pin / Erase |
| | Speed | `QDoubleSpinBox` + `wwMakeScrubField` | world units per second |
| | Width | `QDoubleSpinBox` + `wwMakeScrubField` | world units, the influence radius |
| Selected body | Class | `QComboBox` + `wwMatchFieldStyle` | Automatic / Sea / River / Lake — sets flags bit5 |
| | Water form | `QComboBox` + `wwMatchFieldStyle` | the forms the FILE interned, plus the worldspace default — zero-authoring: no list of water types is written down anywhere in this tree |
| | Colour | tick + colour button | writes the body record's RGBA; unticking clears it (A = 0) |
| | Flow | tick, "Still water" | bungo's lake rule, stored as a `ZeroFlow` mark |
| | Name | `QLineEdit` | the name blob |

and a folding **Bake** section (arrow folds, the fold persists in `QSettings`):

| row | control |
|---|---|
| Flow samples per cell | `QComboBox` 8 / 16 / 32 |

**Dropped from this page's list, with the reason.** The Body IDs / Flow / Shore
distance ticks and the Bridge gap belong to the WRITER, not to the marking tool:
this dock edits an EXISTING file's strokes and re-derives its flow plane, while
which planes exist at all and how components bridge are decided when the file is
generated (`--water-bodies`, `--water-bridge`). Rows that could not do what they
said would be worse than no rows.

Pinned outside the scroll area: the **summary**, carrying the sentence Save will
act on — *"Write 1 stroke(s) into Commonwealth.lodl (346 bodies, flow at 32
samples a cell)"* — the selected body's own line under it (*"body 3 - river -
25,114 texels - plane 1300.0 - water form 001c4995 - flow from a stroke"*), and
the last solve's numbers; or the ONE reason Save cannot run, e.g. *"No landscape
file is open"*, in `danger`. `refreshSummary()` owns the buttons' enabled state,
because a greyed button with no sentence beside it is a broken button. Then the
action bar: Reload, Solve, Save."""

rep(OLD53, NEW53)

rep("""### 5.4 The harness — `WW_WATER_TEST`""",
    """### 5.4 The harness — `WW_WATER_MARK_TEST` (`tests/spells/water_mark.sh`)

Renamed from this page's `WW_WATER_TEST`, which reads as the writer's own
control (`--water-selftest`); this one tests the TOOL. Two halves, because both
can fail on their own: the MODEL, headless
(`lodl <copy.lodl> --water-mark-selftest`), and the DOCK (`WW_WATER_MARK_TEST=1`,
with `WW_WATER_MARK_SHOT=<png>` for the grab).""")

rep("""6. Undo: after removing the stroke and re-solving, the flow plane is
   byte-identical to step 1's.""",
    """6. Undo: after removing the stroke and re-solving, the flow plane is
   byte-identical to step 1's. **As built the gate is stronger and cheaper: the
   whole FILE is byte-identical**, because the re-derivation is a pure function
   of the body-ID plane and the body table, and the two planes marking cannot
   change (body ID and shore) are copied through verbatim with their absolute
   offsets rebased.
7. **Added, and everything above rests on it: the two IDENTITY gates.** The
   marking tool re-implements the writer's body-table encoder and its plane
   packer (the file rule again — `src/lodtfile.cpp` was another lane's). A twin
   is only safe while something proves the two agree, so the harness re-encodes
   the table and re-derives the WHOLE flow plane on an unmarked file and
   compares both against the bytes the writer put there. If either drifts it
   goes red before any behaviour is believed.
8. **Added: one stroke sends the river to the sea**, which is bungo's own
   sentence as a measurement. The mouth is found IN THE FILE — the river texel
   nearest a texel of the body it drains into — the stroke is oriented to end
   there, and the gate is that the flow plane's MEAN direction over the whole
   body has a positive component toward the mouth. The before and after angles
   are both printed.""")

rep("""Panel-style counts, in the same self-test, with floors: plain spin boxes without
the `wwScrubbed` stamp 0 (of ≥ 4), `QGroupBox` 0 against weight-600 headings
≥ 2, selectors without the matched drop-down rule 0 (of ≥ 2), labels carrying
" - " 0, one setting a row (measured on geometry), the summary and the buttons
outside the scroll area, the fold persisting, and `SHOT=<png>` grabbed after any
layout change.""",
    """Panel-style counts, in the DOCK half, with the floors as built: plain spin
boxes without the `wwScrubbed` stamp 0 (of ≥ 2), `QGroupBox` 0 against
weight-600 headings ≥ 4, selectors without the matched drop-down rule 0 (of
≥ 5), check boxes carrying " - " 0 and without a tooltip 0 (of ≥ 2), six
settings on six distinct rows (measured on the laid-out panel's GEOMETRY, not on
its layout class), Save and the map and the summary outside the scroll area with
the settings inside it, the Bake fold opening and closing, the refusal sentence
with no file open against the Save sentence with one, and `SHOT=<png>` grabbed
in the state the test leaves — a river selected and marked.""")

for old, new in edits:
    n = t.count(old)
    if n != 1:
        sys.stderr.write('REFUSED: %d matches for %r...\n' % (n, old[:70]))
        sys.exit(1)
    t = t.replace(old, new)
open(P, 'wb').write(t.encode('utf-8'))
nb = open(P, 'rb').read()
print('ok: %d edits, %d bytes, %d lines, CR=%d'
      % (len(edits), len(nb), nb.count(b'\n'), nb.count(b'\r')))
