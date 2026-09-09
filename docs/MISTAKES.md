# Mistakes

What went wrong, why it went wrong, and what stops it next time. Newest first.
Kept because the same shapes keep coming back in different clothes.

## 2026-09-06 — Compared two empty directories and printed a percentage

**What:** to check what the new height sheet costs, I ran the generator twice with
and without it and diffed the bytes. Both runs used `--vt-region`, a flag I had
invented; the real one is `--terrain-region`. Both refused with "unknown option",
wrote nothing, and the comparison reported "0 containers, 0 bytes" for each. Had
the script printed only the ratio it would have divided by zero or, worse, printed
a plausible-looking 0%.

**Why:** I wrote the measurement from memory of the interface instead of from the
gate that already invokes it correctly two files away.

**Solution:** the script printed the container COUNT and the byte total beside the
ratio, which is the only reason the emptiness was visible; keep doing that in every
ad-hoc measurement. And when a harness already drives the thing being measured,
copy its invocation rather than composing a new one.

## 2026-09-06 — Wrote a function declaration and called it a vector, twice in one evening

**What:** two declarations of the shape

    std::vector<T> name( size_t( count ) );

The functional cast around a BARE IDENTIFIER is a valid parameter declaration, so
this declares a function returning the vector and taking one unnamed `size_t`.
The first, in the new container reader, produced one confusing error about
assigning a vector to a vector. The second, the tile ring buffer in the virtual
texture writer, produced SEVEN errors across five lines — a reference that would
not bind, an assignment to a read-only location, and three member lookups on a
non-class type — which read like five unrelated bugs in unfamiliar code rather
than one declaration being the wrong kind of thing.

**Why:** the lane that wrote them could not compile, so nothing told it. And the
cast is only ambiguous when its argument is a lone identifier: the same line with
`size_t( levels[0].tilesX )` a few lines below is unambiguous and correct, which
is exactly what makes the trap easy to walk into and hard to see.

**Solution:** `static_cast<size_t>( count )`, and read an error list for a common
CAUSE before treating its entries as separate defects — five of those seven
errors were downstream of one line. A regex sweep for the shape now runs after
any lane that adds container declarations; it found no others.

## 2026-09-06 — A traversal written to a picture of the arithmetic, not the arithmetic

**What:** the terrain-pyramid specification fixed the streaming traversal as a
THREE-ROW ring: to build parent tile row `p`, stage finer rows `2p−1`, `2p` and
`2p+1`. It reads right — a parent covers two child rows, plus one above for the
border — and it is wrong. A parent's stored texel `j` reads the finer mosaic at
`v0 = 2·(p·256 + j − 8)`, so its last row, `j = 271`, reads mosaic rows
`512p + 526` and `527`; child row `2p+2` begins at `512p + 512`. The parent needs
**four** child rows, `2p−1` through `2p+2`. Implemented as written, every
parent tile's southern border strip — 8 of 272 rows, under 3% of the tile, and
only at its outer edge — would have been filled from a clamp instead of from the
neighbour that owns it.

**Why:** the number that decides it is `content + border` against `2 × content`
— 528 against 512 — and it never gets written down, because the sentence
describing the ring is about tiles and the failure is about texels. It would
also have survived a check: a content-only comparison of a filtered parent
against the box filter never looks at a border strip, and a p95 bar over the
whole tile hides one wholly wrong edge inside 3% of the samples.

**Solution:** the ring is four rows, the arithmetic is in the code comment and
in `docs/LODGEN_TERRAIN_VT.md` §2.3 with the 528-against-512 in it, and
`tests/spells/lodgen_terrain_vt.sh` compares the content **and each of the four
border strips separately**, on the R16 height sheet where the filter law is
exact and the bar is zero violations rather than a percentile. Rule: when a
traversal is described in units of tiles, re-derive its span in TEXELS before
writing the loop — and give every border strip its own check, because a bar
averaged over a tile cannot see one edge that is entirely wrong.

## 2026-09-06 — A comment that asserted someone else's file format, and was the argument for a decision

**What:** `lodgenBuildAtlas` wrote its diffuse sheet as BC3 under the comment
"BC3, like vanilla's sheet: 8-bit alpha (soft card edges survive)". Vanilla's
sheet is not BC3. Measured on the shipped file —
`Commonwealth.Objects.DDS`, 4096×2048, 13 mips, fourCC **DXT1**, 5,592,552
bytes — it is BC1, and its `_n` and `_s` are both `BC5U` where ours is BC3 for
the normal. So ours was twice the memory of vanilla's on the diffuse and twice
again on the normal, and the comment saying otherwise is what made that look
like parity rather than a cost.

**Why:** the comment was written from what the format *ought* to be for the
feature it was justifying (eight-bit alpha for soft card edges), and a
plausible sentence about another program's file never gets checked once it is
in the source. Reading a DDS header is thirty seconds of work; nobody spends it
on a line that already sounds settled.

**Solution:** the comment now carries the measurement, with the byte count, and
`tests/spells/lodgen_farring.sh` re-measures vanilla's own header every run, so
the claim the flag rests on cannot rot. Rule: a comment that states a fact
about a file we did not write is a MEASUREMENT, and it carries the numbers it
was measured from — a format, a size and a byte count — or it does not go in.

## 2026-09-06 — A mip chain that dropped the channel its top mip carried

**What:** `lodgenWriteDds` built its mip chain with
`( bc3 ? ( acc[3] / 4 ) : 0xFFU ) << 24` — BC3 averaged alpha down the chain,
BC1 forced it opaque. Every BC1 caller at the time was opaque anyway (the
terrain bakes, the emissive sheets), so the branch was invisible and correct.
The moment the atlas asked for BC1, mip 0 would have kept the cut-outs and
every mip below it would have been a solid rectangle — at exactly the distance
an atlas is looked at, which is the only distance it exists for.

**Why:** "BC1 has no alpha" is true of the common case and false of BC1's
punch-through mode, which the encoder in the same file already implements. A
default chosen for the callers that exist is a trap for the caller that
arrives; nothing about it reads as a decision when you find it.

**Solution:** the caller says (`bc1Alpha`), the default keeps every existing
byte identical, and the harness counts punch-through blocks PER MIP and
requires them past the top one — a check that fails on the old filter and
passes on the new. Rule: when a writer branches on a format flag to drop data,
the branch is a decision about the DATA, not about the format, and the caller
that has the data has to be able to say so.

## 2026-09-06 — A hand-written BGSM reader that answered instead of failing

**What:** the stadium measurement needed to know whether a vanilla LOD
material own-emits, and the only way to ask it from a session that cannot run
the exe was to mirror `ShaderMaterial::readFile` in Python. Two fields were
missed: `iAlphaTestRef`, one byte between the alpha blend modes and the alpha
test flag, and `sRootMaterialPath`, a length-prefixed string immediately before
`bAnisoLighting` and `bEmitEnabled`. Everything after each of them was read one
field late. On some versions the reader crashed — fine, that is a failure. On
others it did not: it printed "0 of 7 materials own-emit with a colour that is
not black", which is a plausible sentence, which was the answer I wanted, and
which was read out of the wrong bytes.

**Why:** a binary reader with a wrong offset does not usually fail. It returns
floats in range and booleans that are 0 or 1, and if the conclusion it supports
is the one you expected, nothing in the output objects. The crash on the OTHER
versions was the only reason the error was noticed at all — had every file been
version 2, the wrong number would have gone into WW_CHANGES.

**Solution:** the reader now returns how many bytes it consumed of how many the
file holds, and every reported material prints its version beside its values,
so a shifted read shows up as a consumption that does not track the file size.
It was then cross-checked against a source that does not share its code: the
same three quantities read out of the NIF's own shader property, which agreed
field for field on the Diamond City lights (colour (1,1,1), multiple 6.0).
Rule: a hand-written parser of someone else's format is not a measurement until
a second, independent reading of the same quantity agrees with it — and it must
report how much of its input it accounted for. The same shape caught a second
time the same day, before it shipped: a dump line of the form
`S <b> flags1 <u> ownemit <u> emit <r> <g> <b> mult <f> ...` parsed by stepping
two tokens at a time reads `mult` off the wrong word, because `emit` carries
three values. Parse by keyword, never by position, whenever a line has a field
that is not one token wide.

## 2026-09-06 — A panel self-test that asked isHidden() about a row hidden through its host

**What:** the Source section hides the Resources row by hiding the WIDGET THAT
HOLDS the list and its buttons, which is how a `label | field` grid row is made
to disappear. The first draft of the check asked `resourceList->isHidden()`.
`QWidget::isHidden()` is true only when the widget ITSELF was hidden; a child
of a hidden parent is not hidden, it is merely not visible. So the check would
have read false in both modes and passed for the wrong reason in one of them.

**Why:** `isHidden()` and `isVisible()` read like opposites and are not. The
panel's other visibility checks (`lodtSec->isHidden()`) are correct because
those sections are hidden directly, so the idiom looked established.

**Solution:** the check asks `isVisible()`, and it asserts BOTH states — the
row visible under Specified and not visible under Mod Organizer 2 — so a
predicate that cannot change value cannot pass. Rule: assert a visibility
change in both directions, and use `isVisible()` unless the widget you name is
the one the code calls `setVisible` on.

## 2026-09-06 — Appended to a file whose last byte I had never looked at

**What:** the chunk manifest was written as its lines joined by newlines, no
terminator, and the arrays pass opened it for append and wrote its `A` lines
straight after the last byte. The first appended line was glued to the last
manifest line (`M 47 Materials\LOD\Trees\MapleTrunksLOD.BGSMA 2 0 data\...`)
and one shape per chunk lost its `A` line, from the arrays pass's first day.
The harness counted eighteen `A` lines and passed, because it counted what it
could parse; it failed only when the swallowed line was the pbr fixture's.

**Why:** an appender assumes the file ends with a newline, and I never read
the file's last byte before appending to it. A count that omits the broken
line cannot see the breakage.

**Solution:** the manifest is terminated at the writer and the appender adds
a newline first when the file lacks one. When appending to a text file, read
its tail before trusting its shape; when counting lines a parser accepts, also
count the lines it rejects.

## 2026-09-06 — The CLI wrote the object atlas one directory above the path it bakes into the NIFs

**What:** `lodgenBuildAtlas` takes a file base and a game base. The game base
has always been `data\Textures\Terrain\<ws>\Objects\<ws>.LodgenObjects`,
and the panel writes the sheets to `<texDir>/Objects`; the CLI wrote them to
`<texDir>` itself. Every CLI-built worldspace therefore had every atlased
shape naming `...\Objects\<ws>.LodgenObjects.DDS` while the three sheets sat
in the parent directory. It surfaced only when the merge harness looked for
the new `_s` sheet on disk and failed one check while the check beside it,
which reads the NIF, passed: the shapes named a file that was not there.

**Why:** the atlas call site was written from the shape's point of view (the
game path is right, the shapes resolve in-app because the sheet is passed
around in memory) and never from the file system's. The texture arrays three
lines above it in the same function already appended `/Objects`; the two were
not read side by side.

**Solution:** the CLI derives its atlas directory the same way it derives the
array directory, `( texDir.isEmpty() ? outDir : texDir ) + "/Objects"`, and
the merge harness checks the sheet as a FILE, not only as a name inside a
texture set. A pass that writes a path into a mesh gets one check on the mesh
and one on the disk; either alone can pass while the pair disagrees.

## 2026-09-06 — Downsampled channel renders over black and read the edges as values

**What:** the impostor bake photographs each channel (normal, height, the
material pair, the mask) at the viewport's size and scales the frame down.
The background under those renders is black, so a texel that is three
quarters covered came out as three quarters of its value — a normal pulled
toward (−1, −1), a height pulled to the far plane, gloss and specular
darkened — and the hook skipped every texel under half coverage besides.
Every sheet since the first octahedral bake carried it. On the LOD maple's
opaque trunk the partial texels were the edge and nothing looked wrong; on a
bare near tree they are most of the tree, and a raw-diffuse comparison
measured the factor directly: 1.000 at full coverage, 0.75 at three
quarters.

**Why:** the colour sheet went through a matte that un-premultiplies, and I
assumed the channel sheets, taken through the same crop and scale, were on
the same footing. They were not: no matte, one pass over black. The opaque
test model hid the difference.

**Solution:** un-premultiply every channel texel by the coverage the matte
measured, write wherever coverage is above zero, and keep a test model on
which partial texels are the majority.

## 2026-09-06 — Wrote "unlit albedo" in a spec without checking a pixel against its texel

**What:** the impostor bake's colour sheet came from the lit shader path with
lighting switched off, and the spec called it "albedo, unlit". The lit path
tone-maps before it writes (`tonemap()` in `fo4_default.frag`, a filmic
curve), so every colour sheet was a curved albedo, and the crossed front/side
cards, which never switched lighting off at all, were lit renders. It
surfaced only when a harness compared a raw channel render of the diffuse
against the colour sheet and found them 10 apart on the same texels.

**Why:** "lighting off" was read as "no processing" — an assumption about a
path I had not read to its last line. The spec was written from the switch,
not from the pixel.

**Solution:** channel 12, the base colour times the vertex colour and
nothing else, for every matte pass; the harness now holds the colour sheet
against a raw render of the same texture. A sheet that claims to be a source
quantity gets compared with that quantity once, in numbers, before the claim
goes in a spec.

## 2026-09-06 — Calibrated a harness on one model and called the floors the law

**What:** switching the impostor bake from the LOD mesh to the base's near
model broke four octahedral checks at once: the coverage floor (12%, set
from the LOD maple's pre-baked crown; the near maple is a bare tree at 4.6%),
the opposite-view normal floor (20, set from an opaque trunk; camera-facing
cards agree between views), the subsurface-mask rows (trunk 0, set from an
opaque LOD trunk; every shape of a near tree is alpha-tested, bark card
included), and the "hidden `_L` steps" count, which I had set to at least one
after inspecting `TreeMapleForest02.nif` — the wrong file: the base's MODL
is `TreeMapleForest2.nif`, which has none. And the pbr fixture retargeted the
base colour to the normal map, which changed the alpha cut-out and emptied
the pixel set the comparison ran on.

**Why:** every floor was the number the one model gave minus a margin, not a
property of the thing being checked; and one of them rested on a file I had
matched by eye to the wrong name.

**Solution:** floors from the definition, not the sample — the frame's aspect
against the recorded extents, a hidden count measured from the model's own
string table, a mask rule that has a source on near meshes (the tree flag),
a fixture that keeps the cut-out (the diffuse as the third texture). When a
harness input changes class (a LOD derivative to a near mesh), re-read every
floor before trusting a FAIL or a PASS.

## 2026-09-06 — Appended a word to a line every reader split without trimming

**What:** the impostor sidecar's `oct` line gained a family token at the end
(`... 3072 pbr`). The card builder read lines with `readLine()`, which keeps
the newline, and split on spaces: the last token had always been a number,
and `toFloat()` forgives trailing whitespace, so nobody had noticed. The word
did not forgive it — `pbr\n` never equalled `pbr`, every pbr card set fell
back to the legacy names, looked for a `_gsaos.png` the bake had not written,
and converted nothing. The harness caught it on the first pbr run, after the
bake, the swap check and the retarget check had all passed.

**Why:** I changed a line's shape and checked only the writer. A format has as
many readers as writers, and the reader's tolerance was an accident of the
old content, not a property of the reader.

**Solution:** trim before splitting, everywhere a sidecar line is read
(`lodgenCard` now does). When a line format grows a field, run the reader
with the new line in hand before the next expensive step depends on it —
a one-line CLI run would have shown the missing sheets in ten seconds; the
harness showed them after a three-minute bake.

## 2026-09-06 — Assigned a field and took the driver's word for it, for a month

**What:** the impostor card hook set `cfg.background` to black and then white
for its two-pass matte. Nothing pushed the value to `glClearColor`: the ortho
paint clears with whatever the driver last received, set in `resizeGL`, and
`updateSettings()` rewrote the field from settings on every paint besides.
Both passes cleared to the theme grey, the difference was zero, and every card
baked since the hook was written was an opaque grey rectangle. The impostor
harness passed throughout — it fed synthetic PNGs and never ran the bake.

**Why:** the hook was verified by its structure (two passes, a difference, an
alpha) and not by its output (alpha statistics of one real card), and the
harness written for it measured the converter downstream of the bake rather
than the bake.

**Instead:** a bake hook's gate is a real bake with the output measured —
`lodgen_octahedral.sh` reports covered pixels per tile, and the first number
it printed (4096 of 4096) was the defect. A field that must reach the driver
gets a setter that applies it under the context (`GLView::setBackground`),
and a harness built on synthetic inputs says so in its first line.

## 2026-09-06 — Blamed the code I had just changed for a chunk that was empty by design

**What:** after routing the generator's asset reads through the game
manager, a byte-identity gate built the far chunk (-32,16) at dim 16 from the
unpacked folder and from the archives and got "no LOD-bearing refs" from
both. Three builds went into the loader: a diagnostic line, a state reset,
a shape count. None printed, because the loader was never called. An empty
MNAM slot drops a ref at that ring — vanilla parity, written in the loop —
and that chunk holds nothing at dim 16 without impostor cards; the impostor
harness passes there only because the cards stand in. The near chunk at
dim 4 placed 678 objects at once.

**Why:** the most recently changed code was the first suspect, and the
refusal message named no counters, so "no refs" read as "no models loaded".
The chunk was chosen because a harness used it, without reading why that
harness could use it.

**Instead:** before blaming a change, make the refusal say what it counted —
it now reports placed and without-a-usable-model — and check the input can
succeed at all on the unchanged code. A gate's input must be one the old
build passes.

## 2026-09-06 — Ran the harness on the previous exe after a build that failed

**What:** a patch-build-harness chain was joined with `&&`, but the build step
was `make | grep error | head`, whose exit status is `head`'s. The compile
failed, the chain went on, the harness ran the exe from the build before, and
its grab came back looking like a result. The exe timestamp in the same output
was the only thing that said otherwise.

**Why:** a pipe reports the last command's status, and the one line that
mattered was surrounded by thirty that looked like success.

**Instead:** `make` writes to a log and its own `$?` gates the chain; the
harness step also refuses to run unless the exe is newer than the sources it
is meant to test. Never read a harness verdict without reading the exe's
timestamp next to it.

## 2026-09-06 — Built a panel from Qt's parts while the fork's parts sat in the tree

**What:** the LOD Generation panel shipped with group-box titles, plain spin
boxes, default selector chrome, two settings to a row and the explanation of
each output after a dash in its label. `wwHeading`, `wwMakeScrubField` and
`wwMatchFieldStyle` existed, each with a changelog entry saying what it
replaced and why, and 2026-08-05h records bungo catching exactly this — from a
screenshot — in the collision panel. He caught it from a screenshot again.

**Why:** the panel was written for its mechanism (worker thread, progress
map, cancel between chunks) and its self-test measured that mechanism: 26
checks, none about which species of control was on screen. Nothing in the
build reads a new dock for the house helpers — the scrub sweep only covers the
Settings panes — so a panel that never calls them looks finished from inside.

**Instead:** a new dock's self-test counts the house style before it counts
anything else: spin boxes without the `wwScrubbed` stamp, group boxes,
selectors without the matched sheet, labels with a dash, settings sharing a
row — each with a floor so an empty panel cannot pass. The skill for this area
now lists the helpers by name, so the next panel starts from them.

## 2026-09-05 — Fixed the mechanism I could read before the one the render showed

**What:** an agent's code reading gave a precise, real defect behind the
terrain preview bug (a cached PBRM program with no preview uniform). I fixed
it and wrote the harness afterwards. The harness rendered three channels of a
terrain chunk in a fresh process and got three byte-identical images — no
cache involved. The cause was a `.prog` condition excluding the terrain shader
type from the only program with the preview branch.

**Why:** a mechanism that explains the symptom is not the same as the one
producing it, and a fix aimed by reading was declared before the one
measurement that could contradict it had been taken.

**Instead:** the harness first, before the fix, on the symptom itself; a fix
that does not change the harness's verdict has fixed something else. The
reading was still worth keeping — the cached-hint defect is real and stays
fixed — but it was the second thing, not the first.

## 2026-09-05 — Read a timer bucket as the phase it was named for

**What:** per-phase timers went into the `.lodt` writer to aim an optimisation.
The line said the coarsest pyramid level took 61 of 75 seconds. It was the AO
pass: the `tAo` restart had been anchored on the overview section's closing
brace, one section early, so AO's time fell into the next bucket. I reasoned
for a turn about why a level-3 block would thrash a cache that could hold it
eight times over.

**Why:** the instrument was trusted because it was an instrument. Its anchors
were placed by text match in a file I had just reordered, and nothing checked
that the bucket's boundaries enclosed the code its name claimed.

**Instead:** a second counter — decodes per phase — disagreed with the timer
within one run, and that disagreement is what found the misplacement. Two
instruments that must agree are worth more than one that is believed. Same
shape, same day, smaller: the first LOD Generation harness clicked a button
inside a greyed group and reported the panel broken; `click()` on a disabled
widget does nothing, by Qt's own contract, and the check now asserts the
greying first.

## 2026-09-05 — Baked four maps below native on a symmetry argument, with the lossless reference in the target folder

**What:** the heightmap baker put every texel at a cell-relative centre,
`(i + 0.5)`, "so the resample stays symmetric". At the native 6144 that makes
every texel the mean of two adjacent `LAND` samples: never a sample, the
44,872-unit peak shaved to 44,848, 41% of texels off. Lossy at every size, by
construction. On top of that the maps were baked at 4096 — two thirds of native
per side — and handed over as done. `Commonwealth_fine.HeightMap...dds`, a
sample-aligned 6144 map with the exact `LAND` range in its name, was sitting in
the same `FO4CS/Textures/Terrain` folder the whole time.

**Why:** a plausible-sounding property (symmetry) stood in for the property
that mattered (a texel IS a sample). And I never asked what native was until
bungo asked what resolution the terrain would be if every bump were a pixel —
the question that produces the number 6144 in one line. The reference file was
found by `find`, after the fact, not by looking before baking.

**Instead:** before writing a bake, ask two things: what is native, and does a
reference already exist to diff against. Then the diff is the test — ours is
now checked against `_fine` pixel-for-pixel, and the residual it exposed (seam
handling between disagreeing `VHGT` edges) is being resolved by measurement
against a full `--dump-land` of the ESM rather than by another symmetry
argument.

## 2026-09-05 — Decoded a packed colour from memory while the codec sat in the tree

**What:** the `.btd` converter read Fallout 76 terrain colour as RGB565 and
printed channel means as its "verification". The source is A1R5G5B5 —
libfo76utils' own codec for `pixelFormatRGBA16` says so in `filebuf.cpp`
(`rMask 0x7C00, gMask 0x03E0, bMask 0x001F, aMask 0x8000`), forty lines from
the reader I was already calling. Under 565 "red" was the alpha bit plus four
bits of red; every `.lodt` converted before the fix carries a wrong colour
plane. Same session, same shape, smaller: I created a second `MISTAKES.md` at
the repository root because I checked one directory and this file lives in
`docs/`.

**Why:** reached for the most common 16-bit layout instead of grepping the
third party. And the check I wrote had no known right answer to compare
against: means of (23, 17, 14) for a varying source look like a plausible warm
tint, so the test could only ever agree with me.

**Instead:** before decoding a third party's packed field, grep the third
party. And measure against a value with a KNOWN answer: an untouched
worldspace must decode neutral. Pitt's one constant word is (24, 16, 16) under
565 — a tint that cannot be "no tint" — and exactly (16, 16, 16, A=1) under the
right layout. That number was printed by the first run and not read.

## 2026-09-04 — Reported an invariant as confirmed on an input that could not violate it

**What:** the `.lodt` spec claimed FO76's five 3-bit LTEX weights partition an
implicit base's share, so they can never sum past 7. The converter tested it
and reported **0 violations in 245,760,000 samples** of
`EXM1PittWorldspace.btd`; I wrote that down as the packing being confirmed and
told bungo so. That worldspace has **zero land textures**: every alpha word in
it is zero, and zero satisfies any bound. Appalachia, with 43, broke the same
check on **72% of 21.6 billion samples**. The layers are independent
opacities composited in order over the base, not a partition of unity.

**Why:** the invariant never got a witness. A single number in the same tool
output — "LTEX 0" — disqualified the result before it was believed, and I did
not connect it. Related and earlier the same day: the terrain data map's alpha
channel and its RGB were written from one variable and correlated at
r = 0.969; measuring the correlation caught it, reading the code did not.

**Instead:** before reporting an invariant as holding, establish that the input
*could* have violated it. `tests/spells/lodt_btd.sh` now prints which paths
the small worldspace leaves UNCOVERED rather than scoring them as passes, and
the converter reports the sum as a statistic, not a gate.

## 2026-09-04 — Took a rounder number and a bare cast, and paid a quantum for each

**What:** the `.btd` height quantum was rounded up to a power of two because it
looked cleaner — 1.6× the quantisation error, measured 0.98 units against
0.32 at the exact bound `maxAbs / 32767`. Then quantising with a bare
`quint16()` truncated instead of rounding: a whole quantum of error where half
was available. Both passed every test.

**Why:** a cosmetic choice inside a numeric encoding is a numeric choice. And
the tolerance was set from what the code produced — "within one quantum" — so
it could not fail. Invisible on the FO4 path, where `VHGT` heights are exact
multiples of 8.

**Instead:** derive the bound from the maths first (half a quantum, from the
two grids sitting half a step apart), then make the code meet it. The bound
now precedes the run; it is what caught the truncation.

## 2026-09-04 — A DDS writer checked only through the reader that shared its offsets

**What:** the baked heightmap's DDS header put `ddspf` at header+76 instead of
+72, so the fourCC landed in `dwRGBBitCount` and `dwCaps` stayed zero. It read
back perfectly through the same wrong offsets. Parsing **Bethesda's shipped
map** with the same code showed `DX10` where flags belonged.

**Why:** the same shape as 2026-08-23's round-trip test that shared one table
with the code it tested. A writer and reader written together agree by
construction.

**Instead:** put something that did not come from the writer through the
reader, or the writer's output through a reader that did not come from it.
This is why `.lodt` has an independent reader and why `--lodt` cross-checks
against the ESM's own `LAND` records rather than against what the writer
believes it wrote.

## 2026-09-04 — Proxies moved the right way while the thing they stood for moved the wrong way

**What:** denser shoreline geometry was called "strictly better" on waterline
vertex count and sliver-triangle share. Against the undecimated source mesh it
made fidelity worse — p95 error 30.9 → 89.5. It defaults off.

**Why:** the measurements were the ones easy to collect, not the one that could
contradict the claim.

**Instead:** name the quantity the claim is actually about and measure that,
even when it costs a comparison against the full-resolution source.

## 2026-08-31b — One broken probe grew an architecture, then poisoned its own test

**What:** a scratch BA2 tool's hash lookup silently false-negatived on every
path queried, which became "the source LOD textures are CK-only, the game
ships none of them, the atlas pass is REQUIRED" — docs, commit messages, a
default flipped, loose-copy machinery built. All of it wrong: a plain
name-table grep found every texture in `Fallout4 - Textures6.ba2` and the
vanilla atlas in `Textures4.ba2`. Worse, the atlas initially shipped under
VANILLA'S name, so the "verification" screenshots resolved vanilla's sheet
under our UV layout — the wrong-textures-on-wrong-meshes bungo reported was
partly manufactured by the test setup itself.

**Why:** a single tool's negative result was treated as ground truth without
a positive control on the SAME kind of path (the beachgrass control used a
different path style and passed, which laundered the broken lookups). And
sharing vanilla's filename made every visual check ambiguous about WHICH
sheet was being sampled.

**Instead:** a probe that reports absence must first prove it can find a
thing known to be present in the same namespace, same path shape. And
generated artifacts NEVER reuse a vanilla filename — not even in tests —
because collision converts every downstream check into noise. bungo's
"compare vanilla colors with atlas colors" was the instrument that unwound
it; the per-vertex comparison now lives in the audit scripts.

## 2026-08-31 — 93% coverage passed while every rotated object was wrong

**What:** LODGEN applied REFR euler angles straight into `Matrix::fromEuler`;
the engine's convention is the NEGATED angles (world R = Rx(-x)·Ry(-y)·Rz(-z)).
Every object with a non-trivial rotation was spun wrongly all night, through
an audit that reported "89–96% mutual vertex coverage" — and bungo saw a
rotated highway in the first screenshot batch.

**Why (two shapes):** First, the aggregate metric: most refs are Z-rotated
boxes and radially-fuzzy trees/rocks, so a per-vertex median forgives a
wrongly-spun minority; a metric passed over a population cleared every
individual in it. Second, convention extrapolation: `fromEuler` was PROVEN
exact for SAM pose files, and that proof was silently carried over to a
different producer (engine REFR records). A convention proof binds one data
source only.

**Instead:** parity means per-ELEMENT checks on the elements most able to
fail — here, the refs with large X/Y rotations, found by grouping orphan
verts by identity index and ranking. And when adopting a rotation/axis
convention for a new record type, test candidate conventions against ground
truth for that record type before writing any of them into the generator
(four candidates, one afternoon vertex test, 62% vs 14% settled it).

Same family: 2026-08-25's five blind comparisons — agreement across weak
checks is not coverage. The tree classifier bug found in the same pass
("sTREEt" contains "tree") is the oldest shape of all: substring matching
is not word matching.

## 2026-08-25 — Five comparisons agreed, and none of them could see the field

**What:** the inertia FRAME at `dyn_inertia +0x40` was written as the identity on
every body Compile produced, for as long as Compile has produced bodies. Five
independent comparisons cleared those files: NIF blocks with links resolved, the
packfile container, every scalar through Havok's own deserializer, every pointer,
and the body-to-node map. All five were sound. All five were blind to this field
-- the SDK's reader does not expose the `dyn_inertia` array at all -- so their
agreement was not evidence of anything. It took a raw byte census of the root
object to find, after the game had already said something was wrong twice.

**Why:** stacking more comparisons feels like increasing coverage, and it is not.
Four of the five read the same parsed representation, so they shared its blind
spot; the fifth read pointers, which this field is not. Five green checks over one
blind spot is one green check.

**Instead:** before trusting a clean comparison, ask what it CANNOT see and say so
out loud. A checker earns the right to clear a field only by being able to fail on
it -- which is why `collision_constraints.sh` check 16 reports the frameless error
(0.89) beside the real one (4.4e-07): the run proves it would have failed.

Related, and the same shape in different clothes: 2026-08-24's "never checked what
vanilla meant", and 2026-08-23c's ten green checks that all measured what the
collision IS and none measured WHERE.

## 2026-08-24 — Never checked what "vanilla" meant

**What:** two rebuilt ragdolls misbehaved in game. I compared our output against
`DataUnpacked` at four levels -- NIF blocks with links resolved, the packfile
container, every scalar through Havok's own deserializer, every pointer -- and
reported "equivalent to vanilla" twice. Both times the comparison was sound and
the conclusion was useless, because DataUnpacked is a DIFFERENT BUILD of Fallout 4
from the installed game: 600 of 600 sampled NIFs differ, collision blobs included,
and the two builds order a ragdoll's bodies differently.

**Why:** "vanilla" was the one term in the whole investigation that never got
checked. It had been the corpus for months of collision work, it was right for
every format question ever asked of it, and that track record is exactly what made
it invisible. I questioned the writer, the encoder, the container, the engine, the
mod list -- and never the reference they were all measured against.

There was a signal, too, and I walked past it: the very first size comparison
showed 46367 bytes unpacked against 46327 in the archive. I saw a 40-byte header
string, said "not collision data", and moved on without asking why a shipped asset
had two sizes at all.

**Solution:** `tools/ba2get.py` reads the installed archives directly, and the
test mods are now built from those. **When a comparison against a reference keeps
saying "no difference" and reality keeps disagreeing, stop testing the subject and
test the reference.** A reference with a long history of being right is the last
thing you doubt and often the thing that is wrong -- and the cost of checking it
was one script and ten minutes, against hours spent diffing a file that was fine.

## 2026-08-23 — Diffed bytes for hours without opening the PDB

**What:** a rebuilt human ragdoll misbehaved in game. I spent the next stretch
comparing our packfile against vanilla's — object censuses, offsets, a
field-level diff through Havok's own deserializer, node transforms, block
censuses — and concluded the file matched. It did. The defect was in the second
packfile of the same file, which I had dismissed at "66 bytes differ" without
looking, and it was two bytes: a trigger material zeroed.

**Why:** two failures, and the second is the one that matters.

The small one: I decided "66 bytes, probably the capsule roll" and moved on
without checking. It was 64 bytes of capsule roll and 2 bytes of defect.

The real one: **wrong instrument, wrong order.** The rule here is already
written down — PDB first for vanilla engine behaviour, and our reader agreeing
with our writer is ONE measurement. Every previous in-game collision defect in
this project was found by disassembling the engine. A byte diff can only answer
"is our file the same as vanilla's"; when the answer is yes and the game still
disagrees, the diff has nothing left to say, and continuing to run it is motion
rather than progress. bungo asked "have you consulted the .pdb" after I had
already reported "everything matches" twice.

**Solution:** when a defect is visible in the GAME and not in our checks, the
first move is the PDB, not another comparison. Ask what the engine READS and
under what conditions it behaves differently — `checkConsistency` being `ret 0`
and `getOriginalMassOfBody`'s exact field path took minutes and closed off two
whole hypotheses. And when a diff is dismissed as "probably X", either check that
it is all X or say out loud that it was not checked.

## 2026-08-23 — Checked what the collision IS and WHERE it is, never what it WEIGHS

**What:** a rebuilt human ragdoll shipped for its first in-game test with every
offline check green: shape classes, bone tree parent for parent, joint counts,
body-to-node mapping, vanilla's exact byte sizes, an identical packfile object
census at identical offsets. The first raider killed with it thrashed on death and
could be shoved around like a paper bag. Its bodies' DENSITY was 43x too small and
their CENTRE OF MASS sat on the body origin, because spheres and capsules never
set their mass properties and a body of primitives summed to volume zero.

**Why:** this is the third instance of one family, and the family is now clear
enough to name. The checks measure the quantities we already model, and a defect
lives in whatever quantity nothing looked at:

  * 2026-08-23, mixed compounds: every check measured what the shape IS, none
    measured WHERE it is — 18 game units out;
  * 2026-08-23, joints: every check measured the carrier, none measured the
    OPERATION — 30 files silently lost one;
  * here: every check measured structure and placement, none measured MASS.

Adding a consumer of a value is what makes it a quantity, and both `density` and
`motionCom` had been consumed by the compile path for months without anything
comparing them to vanilla.

**Solution:** three checks comparing per-body mass, density and centre of mass
against vanilla, with a fourth asserting vanilla's densities are nothing like its
masses so they cannot pass vacuously. And the rule, which is cheap: **when code
starts computing a value, add the check that compares that value to the
reference, in the same change.** The compile path computed density from a volume
no test ever read. If a field is worth computing it is worth diffing.

## 2026-08-23 — Confirmed a property every instance had, and built the wrong rule

**What:** a ragdoll's bone order had to be reproduced. Measured across all 75
corpus ragdolls, the parent array is non-decreasing, every parent index is below
its child's, and the root is always bone 0 -- 75 of 75, no exceptions. That is a
breadth-first walk, so the writer walked the tree breadth-first. It produced a
permutation of vanilla's order: right generations, wrong siblings.

**Why:** the measurement confirmed a property that the real rule IMPLIES, not the
rule. Breadth-first is one of many orders satisfying "parents non-decreasing", and
nothing about a tree says which of a bone's five children comes first -- so the
measurement could not have distinguished the right answer from the wrong one, and
75 of 75 made it feel as though it had.

The actual rule was one command away and exact: bone k is the child of joint k-1.
The Brahmin's constraint array runs Tail1, SPINE2, RLeg1, Sack, LLeg1 and its
bones 1..5 are exactly those.

**Solution:** what caught it was comparing the whole parent array against
vanilla's rather than re-testing the property -- ours read
`-1 0 0 0 0 0 1 2 4 5 ...` against vanilla's `-1 0 0 0 0 0 1 2 3 5 ...`, and the
difference is visible at a glance where a summary statistic showed nothing.
**A property that every instance satisfies is not necessarily the rule that
generated them.** When a measurement is about to decide an implementation, ask
what OTHER rule would produce the same data; if there is one, the measurement has
not finished. And prefer comparing the whole artifact against the reference over
testing a property of it.

## 2026-08-23 — Generalised a count from the one fixture, past our own note

**What:** the ragdoll writer built one bone per body. That is true of the Brahmin,
which is the fixture everything was measured on. It is false on 9 of the 75 corpus
ragdolls -- `TorsoProtectron` has three bodies and two bones -- and those nine
refused to compile back as ragdolls at all, silently degrading to plain physics
systems.

**Why:** the rule that WAS measured is `bones == joints + 1`, 75 of 75. From that
plus "bone index equals body index" it is an easy step to "bones == bodies", and
the step is wrong: the bones are a PREFIX of the bodies and a ragdoll system may
carry bodies its bone tree never reaches.

The exception was already written down in this repository. `hknpEncodeSystem`'s
bone-map comment says the map is the identity "on all 37, including the three
parts kits where the counts differ" -- read earlier the same session, while
looking at something else.

**Solution:** the builder takes its bone count from the joints and the ordering
puts unreached bodies last; the corpus sweep now reports ragdoll-system counts
against vanilla's, which is what surfaced the nine. **A count that holds on the
fixture is a count from a sample of one.** Before turning a measured relation into
an assumption, grep the codebase for the field: this project writes its exceptions
down, and the note was already there.

## 2026-08-23 — Measured the carrier, not the operation, and 30 files lost a joint

**What:** the joint mapping shipped with a corpus measurement that read
1202 / 1202 -- every joint in the corpus, written into its NIF block and read back
byte-identically. An hour later the compile half went in, and the first sweep of
the whole operation showed 1172 of 1202: **30 files had silently lost a joint**,
every one of them under `Actors/Robot/Parts`.

**Why:** `--constraints` hands a decoded joint to the writer and asks whether it
survives. It never asks the question the OPERATION answers -- does Decompile hand
it every joint the file has? It did not. Decompile required both of a joint's
bodies to resolve to a block, and a robot part's joint names 0x7fffffff as its
parent because the part attaches to whatever assembles it. Both halves were right
about what they measured. Nothing measured the seam.

The unit measurement was not wrong, and it was not useless -- it caught the field
naming. It was just answering a smaller question than its number implied, and I
read the number as if it covered the feature.

**Solution:** the sweep now runs the whole operation end to end -- vanilla,
decompile, compile, count -- and compares joints IN against joints OUT per file,
which is the number a user would notice. It reports 155 / 155 and 1202 / 1202
after the fix and would have reported 125 / 155 before it. **A component
measurement is not a feature measurement.** When a number is quoted as evidence
that a feature works, check what it actually iterates over: if it starts from
data the component was handed rather than from the file the user opens, there is
a seam between them and the seam is where the loss lives.

## 2026-08-23 — A round-trip test that shared one table with the code it tested

**What:** the joint carrier shipped with a check that encoded every constraint
twice -- once straight from the decode, once after a trip through its new NIF
block -- and required the bytes to match. 38 of 38 on the Brahmin skeleton, 1202
of 1202 over the corpus. Then I exchanged `Plane A` and `Motor A` in the field
name table to see the check fail, and **it passed, 38 of 38, with the mapping
deliberately wrong**.

**Why:** the writer and the reader read the same `tlCollFrameNames` table.
Swapping two entries swaps them on the way in and on the way out, so the round
trip cancels and the bytes are identical. The check measured that the carrier is
SELF-CONSISTENT, which it would be for any naming at all, including one that puts
a ragdoll's plane axis in the field the engine reads as its motor axis.

This is the same shape as the 18-unit terminal a day earlier: a check that cannot
fail is not a check. The new form is sharper, though -- there the checks measured
the wrong QUANTITY, here the check measured a quantity that a wrong answer
satisfies by construction.

**Solution:** the discriminating check reads the block back BY NIF FIELD NAME and
requires an identity those names claim -- the third basis vector is the cross
product of the first two, which is how NifSkope's own "Recompute B Frame from A"
authors `Motor A`. With the names swapped it reports 8 of 38, worst error 2.0;
with them right, 1202 of 1202 at 8.8e-07. **A round trip through a mapping tests
the mapping only if the two directions cannot cancel: either the check reads the
destination in the destination's own terms, or it is measuring nothing.** Ask, of
any round-trip check: what wrong answer would still pass? Then go break the code
and watch it fail before believing the green.

## 2026-08-23 — Shipped a harness that passed while the collision was 18 units out

**What:** mixed compounds went out with a 9-check harness, a 114-file shape-class
comparison, a stored-solid comparison, a compound structural check and a
byte-exact round-trip. All green. bungo loaded the wall terminal and its
collision was in the wrong place -- the mesh child sat 18 game units from where
it belongs, because a Havok-unit translation was being applied to game-unit
vertices and arrived at 1/70 size.

**Why:** every check measured what the collision IS and none measured WHERE it
is. Shape classes, shape counts, header words, child order, round-trip
byte-exactness and "does the compound follow its own pointer" are all satisfied
perfectly by a correctly-built shape in the wrong place. `collision_ab.py` could
not have helped either: it compares stored convex solids, and a compressed mesh
has none, so the one shape that moved was the one nothing was looking at.

The AABB was sitting right there the whole time. `hkcompound.py --aabb` printed
ours and vanilla's side by side in one command, they differed in the second
decimal, and I had run that tool three times that night for other reasons.

**Solution:** the harness now compares the compiled compound's AABB against
vanilla's own, which fails on the pre-fix build. And the rule this is the second
instance of -- **a check that cannot fail is not a check** -- gets a sharper
form: when a change moves geometry, at least one check must measure a POSITION
against an external reference. Structure, counts and self-consistency are all
things a wrongly-placed object satisfies. Ask what the defect would look like,
then ask which check would see it; if the honest answer is "none of them", that
is the check to write before shipping, not after.

## 2026-08-23 — Widened a gate, and three files I was not aiming at changed

**What:** Compile refused to compound a body whose leaves were not all convex.
Relaxing that to "convex leaves AND mesh leaves are both allowed" fixed the three
mixed files it was aimed at -- and silently rewrote three OTHERS. ceilingfan01,
ceilingfan02 and cigarettemachine are mesh-only bodies with several mesh leaves;
they had never been near the convex path, and the moment the gate stopped
demanding "all convex" they qualified for it. They came out as seven mesh shapes
under a compound where vanilla has two plain meshes and no compound at all.

Then, having fixed that, the same change put the mesh child LAST in the compound
where vanilla puts it first -- a permutation, for no reason, in a codebase that
had already spent a week on a body-order permutation it could not explain.

**Why:** the change was framed as "let this case through" and tested on that
case. A gate does not let one case through; it moves a boundary, and everything
on the near side of it moves with it. Nothing in the work asked which OTHER
inputs newly satisfied the condition.

**Solution:** both were caught in minutes by comparing shape classes across all
114 files against vanilla rather than looking at the three that motivated the
change -- the population, not the sample, which is the same lesson as the entry
below and is becoming the house rule. So: **when a condition is widened, measure
the whole corpus before and after and diff the two, because the interesting
result is the file you were not thinking about.** The harness that came out of it
(`tests/spells/collision_mixed_compound.sh`) checks the mesh-only case beside the
mixed one for exactly this reason, and its check 8 is the one that failed on the
intermediate build.

## 2026-08-22 — Read 25 rows off a list truncated at 25 and wrote down what they said

**What:** a corpus scan printed "first 25 exceptions" and every one it printed was
`PrydwenDestruction.nif`, so "0x00000010 on the 25 statics of
PrydwenDestruction.nif" went into WW_CHANGES and a commit message. There are 25
such bodies in total and they are spread over about ten files -- both arcade coin
slots, seven gravestones, Prydwen. The cap and the count were the same number by
coincidence, which is exactly the coincidence that makes a truncated list look
complete.

**Why:** the scan was written to show a sample and was read as if it were the
population. Nothing in its output said "and 0 more", so there was nothing to
notice.

**Solution:** caught two hours later by a different check that printed the
histogram instead of the exceptions -- which is the lesson. When a scan reports
violations, print the GROUPED COUNTS, not the first N rows; a histogram cannot be
truncated into a false pattern. And a "first N" cap must always print how many it
withheld, even when that is zero. The mislabelled bit turned out to be
RAISE_TRIGGER_EVENTS on trigger volumes, real data that now round-trips, so the
correction was worth more than the tidy story it replaced.

## 2026-08-22 — Derived four Havok layouts by hand while the exe shipped their field names

**What:** the `hknpBodyCinfo` layout in these notes was assembled from signature
scans and controlled Elric pairs, and one word of it was read wrong. Cinfo +0x10
went into HANDOFF as "the per-body material word, whose high u16 is just the
body's own index"; it is two fields, `qualityId` (u8 at +0x10) and `materialId`
(u16 at +0x12). Fallout4.exe carries Havok's own `hkClass` reflection --
`<Class>Class_Members`, a const array of `hkClassMember`, 0x28 bytes each, with a
name pointer and an offset per field. One query named every field of
`hknpBodyCinfo`, `hknpBody`, `hknpMotionCinfo` and `hknpPhysicsSystemData`,
including `flags` at cinfo +0x18 -- the field whose absence the entire
impact-sound dig turned out to be about. `hknpMotionCinfo +0x08`, filed here as
"density", is `massFactor` by the same table.

**Why:** "the PDB says what the engine READS" was already the rule, and it was
taken to mean "disassemble the function that touches the field". Disassembly
answers what a field DOES. Reflection answers what it IS, in one read, and the
tables were never looked for because nothing had said they were there.

**Solution:** before deriving any Havok struct layout by hand, dump its
reflection. `<Class>Class_Members` for fields, `<Class><Name>EnumItems` for enum
values (that is how `IS_STATIC / IS_DYNAMIC / IS_KEYFRAMED / IS_ACTIVE` were
confirmed). The class objects themselves are runtime-initialised and read back
empty from the on-disk image; the member and item arrays are const and readable.
Note the reflection is the SERIALISED subset -- `hknpBody::FlagsEnum` reflects
only four of its bits -- so for the rest, look for the engine's own debug
printer, which in this case (`NVFlex::printHknpBodyInfo`) names all 29.

**And a second data point for an older rule.** 2026-08-22i closed with "a field
with a small closed set of values is data". `hknpBodyCinfo::flags` takes exactly
four values over 13,889 vanilla bodies and Compile writes 0 for all of them. Same
shape, eight entries apart, found the same way and not before.

## 2026-08-21 — Spent half an hour reproducing a bug that had been fixed for ten days

**What:** picked `block_rename.sh` hangs 7-in-10 off the backlog, built a stack
catcher, and ran it twenty times to reproduce. It never hung, because `2b0635d`
fixed it on 2026-08-11 — and that commit says so in its own message ("5 deaths in
8 on a CLEAN build before; 12/12 green after"). The backlog entry was simply
never updated when the fix landed.

**Why:** the backlog was treated as current because it is the single source of
truth. It is only as current as the last person to close an item in it, and a fix
that lands in WW_CHANGES does not walk itself over.

**Solution:** before starting anything marked OPEN, `git log --oneline -- <the
files it names>` and grep WW_CHANGES for the symptom. Thirty seconds against
half an hour. The twenty runs were not wasted — they are the independent
re-verification the entry now carries — but they were the second thing to do, not
the first.

## 2026-08-21 — `sed -i` flattened WW_CHANGES.md, hours after writing the rule against it

**What:** used `sed -i` for a one-line title change in WW_CHANGES.md. The file is
mixed CRLF/LF, and sed rewrote every line ending: `git diff --numstat` came back
**17360 insertions, 17339 deletions** for a change to one line.

**Why:** the whole reason the binary-splice helper exists is that ordinary text
tools flatten these files, and the entry three below this one says exactly that.
It was reached for anyway, because the edit was small — which is the same excuse
every time.

**Solution:** caught before committing, by the `git diff --numstat` habit that is
in that same entry, and repaired by rebuilding the file from `git show HEAD:` and
re-applying both edits as bytes. The rule stands and now has a second data point:
**no text tool touches a mixed-ending file — not sed, not the editor, not Python
text mode — regardless of how small the edit looks.** And numstat before every
commit is what makes the rule survive being forgotten.

## 2026-08-21 — "Elric is not on this machine" was written into two documents

**What:** searched `C:` and `E:` for `Elrich.exe`, found nothing, and recorded
"Elric is no longer installed on this machine" in `HANDOFF.md` and the backlog as
the reason `triangleIsInterior` was blocked. It is installed, at
`X:\Programs\Steam\steamapps\common\Fallout 4 1946160\Tools\Elric`. With it, that
item moved in an afternoon and the compound BVH (item 3b) was decoded outright.

**Why:** the search covered the drives that happened to come to mind, and its
result was then written down as a fact about the world rather than a fact about
the search.

**Solution:** enumerate every fixed drive (`Get-PSDrive -PSProvider FileSystem`)
before concluding a tool is absent — Steam lives on `X:` here. And do not write
"X is not installed" into a durable document at all: write "not found by <the
search that was run>", which is true, and which does not talk the next session
out of a whole line of work.

## 2026-08-21 — A harness check passed for the wrong reason

**What:** `collision_materials.sh` proved "the material follows its shape" by
swapping the materials of the FIRST and LAST decompiled leaf and requiring the
compiled run order to change. Its fixture has three leaves across two bodies
whose first and last happen to share a material, so the swap collapsed the
compiled table to a single material — and "the run order changed" duly reported a
pass, while measuring nothing of the kind.

**Why:** the check tested a consequence (the order differs) rather than the
property (each material sits on its own shape's triangles), and the fixture
quietly stopped satisfying the check's premise when Decompile started splitting
meshes by material.

**Solution:** the swap now picks the first two leaves that actually DISAGREE, and
the check prints both run orders so a collapse is visible rather than inferred.
More generally: when a check's premise is a property of the fixture, assert the
premise. Every harness here already has "not vacuous" checks at the top for
exactly this reason — this one just did not cover the pair it went on to use.

## 2026-08-20 — The splice helper put CRLF lines into an LF file

**What:** the binary-splice helper used for editing these mixed-ending sources
matched a single-line snippet, which reads identically as LF or CRLF, and then
wrote the REPLACEMENT back in CRLF. Ten CRLF lines landed in the middle of
`collisiontools.cpp`, which is LF throughout.

**Why:** the helper tried CRLF first and took the first style that matched. For a
snippet with no line break inside it, both styles always match.

**Solution:** it tries the file's own DOMINANT style first, so an ambiguous match
is written back the way the file is written. And `git diff --numstat` before every
commit — a flattened file shows up instantly as a diff the size of the file.

## 2026-08-21 — Screen-captured the desktop to find a harness window

**What:** chasing a GUI harness that stalled, grabbed the whole second monitor to
see whether a modal dialog was up. The harness window was not on that monitor;
the user's Discord was. Deleted at once and reported.

**Why:** the question was "is a dialog open", and a screenshot was the first tool
that came to hand rather than the narrowest one.

**Solution:** capture the WINDOW (`PrintWindow` with `PW_RENDERFULLCONTENT`) or
use the app's own framebuffer harness — never the screen or a monitor. To find
out whether a stalled Qt app is showing a dialog, enumerate its windows by pid
and read the titles, which answers it without an image at all.

## 2026-08-21 — Shipped a crash under a green suite, because the test could not fail

**What:** the compound writer emitted every byte of a BVH and no pointer to it.
Fallout 4 dereferenced null in `hknpDynamicCompoundShape::updateAabb` on the
first mesh that used one. Every check passed, `--roundtrip` included, and the
corpus comparison said 86 of 86.

**Why:** the checks were reflexive. `--roundtrip` decodes with our decoder and
re-encodes with our encoder, and our decoder never followed that pointer either —
it carried the object through as opaque bytes, so the missing fixup round-tripped
perfectly. **A round trip cannot see a pointer that neither end needs.** The
corpus check had the same shape: it read our output the same wrong way we wrote
it, and agreed with itself. Two mutually-confirming halves of one program are one
measurement, not two.

The layout error underneath is the same lesson. Reading the array from `+0x60` as
`2n` records instead of `+0x40` as `2n+1` is a window shifted by one, and it fits
every vanilla file — same object size, same self-consistent tree. Only an
EXTERNAL consumer distinguished them, and the only true external consumer is the
engine.

**Solution:** for anything crossing a boundary — a file another program reads —
at least one check must be written from the CONSUMER's rules, independently of
our own I/O. `tools/hkcompound.py` follows the fixup or fails, and its `--damage`
mode reproduces exactly what shipped so the check is proved able to fail.

And validate in the real consumer sooner. 1,619 rebuilt meshes in a mod folder
found this in minutes, after weeks of green harnesses. That test should have come
before the tenth check, not after.

## 2026-08-21 — Killed every NifSkope process without looking first

**What:** a link failed with "cannot open output file release/NifSkope.exe:
Permission denied", so the next command opened with
`Get-Process NifSkope | Stop-Process -Force` — every instance, no check for which
were headless CLI workers and which might be a window bungo had open. When the
survivors were finally inspected they were all `-no-gui` workers, but that was
luck, not care: he keeps NifSkope windows open for hours and the rule to close
one with `CloseMainWindow` rather than `Kill` was already written down.

**Why:** the lock had an obvious cause and killing everything was the one-line
fix. Enumerating first costs one command.

**Solution:** filter before killing. `Get-CimInstance Win32_Process -Filter
"Name='NifSkope.exe'"` gives the command line, and `-no-gui` in it is proof the
process is a worker; a `MainWindowHandle` of 0 says the same. Anything else gets
`CloseMainWindow`, or gets left alone and reported.

## 2026-08-21 — Two rebuild loops ran at once over one output directory

**What:** the mod rebuild was started with `nohup ... &` inside a tool call. The
call reported "completed, exit 0" a moment later, the mod folder held 14 files,
and that was read as the loop having died with its wrapper. So a second loop was
started. Both were alive an hour later, appending to the same manifest and
writing the same mod folder.

**Why:** the notification is about the WRAPPER, and `nohup ... &` deliberately
outlives it. Its exit code says nothing about the work. Worse, `TaskStop` on the
second run killed only the tracked shell — `rebuild.sh` kept going, and kept
spawning the NifSkope processes that then held the exe locked.

**Solution:** never `nohup ... &` inside a tool call — use the harness's own
background mode, which tracks the real process. And when a background job needs
to stop, verify it: `Get-CimInstance Win32_Process | Where-Object CommandLine
-match '<script>'` returning nothing is the proof, not the stop call's message.

## 2026-08-22 — Shipped a placeholder constant into 59 files

**What:** `tlCollCompileConvex` wrote the literal `0x01000001` as a compound's
header word. Bit 0 of that word is the engine's "I am convex" flag, so every
compound Compile built told Fallout 4 it was a vertex cloud. It crashed on the
first scaled reference that loaded one.

**Why:** the constant was never measured. Every other shape's word in the writer
came off the corpus and was documented with a count — polytope `0x01000143`,
"74 of 76 vanilla polytopes"; capsule `0x010001c3`, "51 of 51". The compound's
was typed in to make the encoder produce something, and nothing ever went back
for it. The decoder's own header even records the right values three lines from
the field it fills: "+0x10; 02020004 / 02030004 / 02040004 seen".

**Solution:** a constant in a writer needs the same provenance as a decoded
field — a count, or a symbol, in the comment beside it. Grep the writer for bare
hex with no measurement attached and treat each one as unverified. And when the
DECODER has already recorded the observed values for a field, the encoder must
not invent a different one; that mismatch is mechanically checkable.

## 2026-08-22 — A check that passed because both sides were empty

**What:** the new body-position check compared our file with vanilla's by running
a reader over both. The reader crashed on a syntax error, printed nothing for
each, and the check compared "" with "" and reported OK. The vacuity guard beside
it passed too: it asked "is vanilla's value not 0.0,0.0,0.0", and an empty string
is not that string.

**Why:** equality between two runs of the SAME broken tool is not evidence, and
the guard tested the wrong property — absence of a specific wrong value rather
than presence of a right one.

**Solution:** a vacuity guard must assert the reference value has the SHAPE it
should (`grep -qE '^-?[0-9]+\.[0-9],...'`), not merely that it differs from one
known-bad constant. And a tool that prints nothing must fail, not return empty.

## 2026-08-22 — Four checkers in a row that agreed with themselves

**What:** the tool built to prove our solids match vanilla's reported, in turn: a
mannequin hull 0.74 m out of place, railings 27% small, 91 of 114 files with
wrong planes, and deviations of exactly 1.0 and 1.8e21. Every one was the
checker. The writer was right the whole time; the real answer is 110 of 114
identical to 0.46 mm.

**Why:** each version compared a DERIVED number without asking what the number
depended on.

  * a centre of mass depends on the frame -- and vanilla puts a compound child's
    offset in the instance where we bake it into the vertices
  * a volume depends on the triangulation -- and our face tables decompose the
    same 6 faces into 18 triangles where vanilla uses 12
  * plane slots past the face count are uninitialised residue, and vanilla is not
    even self-consistent there
  * sorting tuples that contain tuples orders near-identical shapes differently
    in two files, so zipping compares a hull against its neighbour

**Solution:** compare the STORED DEFINITION, not a quantity computed from it, and
before trusting a comparison ask what would have to be true for the two numbers
to be comparable at all. When a checker says a corpus is broadly wrong, the
checker is the first suspect -- a writer that passed byte-exact round trips does
not suddenly get 91 of 114 files wrong. Each of these took one file dumped by
hand to expose, which should have been the first move rather than the fourth.

## 2026-08-22 — Shipped a crash inside the fix for the previous one

**What:** the KEYFRAMED body state fixed the doors and, in the same change, gave
those bodies a motion index without the matching sentinel in their inertia
record. The engine indexes `dyn_motion + index*0x40` whenever that index is not
0xffff, and a keyframed body has no dyn_motion array, so it dereferenced null.
Shipped in every build for a day, on the doors and the cabinet both.

**Why:** the change was verified against the thing it was FIXING -- motion index,
inertia count, orientation, position, all held against vanilla and all correct --
and not against the records those fields point INTO. A field that selects another
record is only half-checked until the record it selects is checked too.

**Solution:** `hkbodypos.py --state` now carries the inertia record's own index,
so the harness compares it on both compile paths. More generally: when a change
introduces an INDEX, the check has to follow it. The compound pointer, the
convex bit and this are the same defect three times -- a value that means
something to the engine and nothing to our reader.

**And the wrong turn it caused.** The crash was first blamed on our writing two
physics systems per file, which vanilla never does -- a real difference, measured
1,334 of 1,334, and worth fixing. It was not this crash. Three test rounds went
to it, and 46 meshes were pulled from the mod on the strength of it. The
disassembly of the faulting address took ten minutes and gave the answer outright;
it should have come before the hypothesis, not after it. **Structural identity is
not a diagnosis** -- "our file differs from vanilla here" does not make that
difference the cause.
