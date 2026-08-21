# Mistakes

What went wrong, why it went wrong, and what stops it next time. Newest first.
Kept because the same shapes keep coming back in different clothes.

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
