# Mistakes

What went wrong, why it went wrong, and what stops it next time. Newest first.
Kept because the same shapes keep coming back in different clothes.

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
