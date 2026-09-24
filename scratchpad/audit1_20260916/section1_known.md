### 1.4 The reds that are already on the record, re-measured but not re-reported as news

The brief names four standing reds that are not this lane's to discover. All
four are exactly where they were, and each is quoted here from the line that
registered it so the board can be read without the history.

**`ground_cover` -- the grass-feature red.** HANDOFF's BTOFREE1 landing line
registers it by check name: *"lodgen_ground_cover 29/4 (C1 green on the frozen
file; C2 x3 / C6a / C9 / C16 = pre-existing grass-feature red, open)"*. My run
is that set name for name and nothing else: outer block 29 checks / 4 failures,
where the fourth is the roll-up line `C3..C17 the measurements on the files`
over an inner block of 21 / 3. The six distinct failing checks are `C2 this
ground has no cover (coverMax=69)`, `C2 all three grass-free sheets are
byte-identical with --cover`, `C2 a grass-free chunk stays DXT1 (fourCC DXT5)`,
`C6a the model and the bake agree on the composite`, `C9 the slope gate is
there and it bites`, `C16 the cover survives the mip chain`. Unmoved. Not news.

**The stock `.BTO` ~6 percent silent drop.** Registered in the same landing
line as *"stock .BTO ~6 percent drop (his call)"* -- a decision owed by bungo,
not a defect anyone has been told to fix. It did not surface as a failing check
in any of the 27 gates on this run: no gate asserts a chunk count against the
plugin's placements, which is why it is a standing item rather than a red line.
I have not added one; a gate for it would be a new assertion about what a bake
should contain, and that is a design question, not an audit finding.

**`byte_gate` phase (c), the panel-versus-CLI divergence.** Registered as
*"NEW UNOWNED byte_gate phase (c) panel-vs-CLI divergence on
Commonwealth.4.-20.24.DDS + Commonwealth.lodi at identical sizes, present on
the rung too (14/3 vs 15/2)"*. My run reproduces it exactly: `panel vs command
line: 15 identical, 2 differ, 0 missing`, the two files being
`Commonwealth.4.-20.24.DDS` (174,888 vs 174,888 bytes) and `Commonwealth.lodi`
(41,638 vs 41,638). Since it was registered as UNOWNED I measured WHERE the two
files diverge, which the registration does not say
(`scratchpad/audit1_20260916/bytegate_probe.py`):

| file | bytes | differing bytes | first difference | what sits there |
|---|---|---|---|---|
| `Commonwealth.4.-20.24.DDS` | 174,888 | 123,515 | 0x80 | the first byte of the DDS payload -- the whole compressed image, not a header field |
| `Commonwealth.lodi` | 41,638 | 1,376 | 0x0C | the header CRC, i.e. the header itself already differs |

So it is not a timestamp or a path string embedded in a header: the panel and
the command line are producing different terrain-sheet PIXELS for the same
chunk, and a `.lodi` that differs from its twelfth byte. That is a finding
about the SHAPE of a known red, not a new red, and it is the one thing I would
put at the top of a follow-up lane's brief. I did not chase it here: it
reproduces on the rung as well, so it is not this campaign's doing, and running
it down means driving the panel under a harness, which is a lane of its own.

**`octahedral` F1, the impostor cube's texel span.** Registered by lane
CARDWIDTH on 2026-09-10 as *"octahedral 110/1 (F1 cube 1.78 vs bar 1, unmoved
by the fix, left red)"*. My run reads `F1: every frame of the cube spans its
predicted texels within 1 AT THE READER THRESHOLD (worst 1.74)`. Still red,
0.04 texels better than when it was registered, and left alone: closing it is a
change to how a card frame is laid out, which is a design change and out of
scope by the brief's own line.

### 1.5 The gate that cannot run

One row on the board is a gate that could not do its job at all, and the brief
asks for it as a row with its reason.

| gate | phase | reason it cannot run | whose |
|---|---|---|---|
| `byte_gate` | (a) the panel against a rung | `FAIL: no exe at release/NifSkope.before_panel1.exe` -- the rung the phase pins its bytes to is not in the tree | **KNOWN**, LAYOUT1's landing line records the same missing rung; not restorable by me, and the brief forbids deleting or manufacturing rung exes |

Measured, not assumed: `release/` holds 11 `NifSkope.before_*.exe` rungs on
this tree and `before_panel1` is not one of them, and LAYOUT1's own landing
line reads *"(a) cannot run here (no NifSkope.before_panel1.exe ...)"*.

The other two of that gate's three failures are the phase-(c) divergence in
1.4. Phase (b) ran and passed. So `byte_gate` is red for one missing exe and
one known unowned divergence, and for nothing else.
