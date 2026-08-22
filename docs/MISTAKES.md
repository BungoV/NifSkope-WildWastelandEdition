# Mistakes

What went wrong, why it went wrong, and what stops it next time. Newest first.
Kept because the same shapes keep coming back in different clothes.

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
