"""Lane CELLVIEW4B -- splice this lane's entries into the root MISTAKES.md.

The file is pure CRLF (10,590 CR, 10,590 LF before this runs). It is spliced as
BYTES, the new text is written with explicit \r\n, the insertion point is the
first `## ` heading (newest at the top), and the CR count is asserted to rise by
exactly the number of lines added -- never measured with grep.
"""
import io
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
PATH = os.path.join(ROOT, 'MISTAKES.md')

ENTRIES = """\
## 2026-09-19 -- a census field the gate READ but nothing ever WROTE, inside a green run

Lane CELLVIEW4B rewrote `cell_pick.sh`'s ground rows to read the blended census
as well as the mosaic one, and one of its `sed` patterns has a single capture
group and asks for `\\2`. GNU sed said so out loud -- `invalid reference \\2 on
's' command's RHS` -- the variable came back empty, the arm detection fell
through to `mosaic` on a BLEND run, and the gate printed
`ground [mosaic]: 0 textured quads of 0`.

**The gate still said PASS.** Not one row read that variable, so a field that was
scraped, printed and wrong cost nothing. That is the failure this project's own
rule is about: a check that cannot fail is not a check, and a number printed
beside real numbers is read as a real number.

Found by reading the gate's own output instead of its verdict. Repaired in
fix04 with two single-group attempts, and -- the part that matters -- a new row
that FAILS when any scraped field comes back empty, so the next rewording of a
census line goes red instead of quietly printing zeros.

Rule: every scraped field gets a row that fails when the scrape misses. If a
gate prints a number, some row must read it.

## 2026-09-19 -- a rule mirrored in two languages, and only one copy widened

Lane CELLVIEW4B widened `isMarkerModel()` in `src/cellview.cpp` so that markers
at the meshes ROOT -- `markerxheading.nif`, the black arrow the director asked
about -- stop being drawn as ordinary statics. `tests/spells/cell_open_check.py`
carries a Python copy of that rule whose docstring says it mirrors the C++
"element for element". The copy was not widened, so `cell_open.sh` went RED and
called three deliberately hidden markers "references dropped although their
model is present".

The red was correct behaviour by a stale rule, and the tempting repair -- copy
the new clause across and move on -- would have left the same trap armed for the
next change. Two sources that LOOK alike, compared by eye, is not a measurement.

Repaired by replacing the row with an accounting identity the run already
supplies: the plugin draws 1438 references in downtown 5,-11, the scene has
1426, and the viewer's own census names 12 (`disabled 0, markers 12, deleted 0,
no base 0`). Every reference the plugin draws and the scene does not must fall
into a category the viewer NAMED. Widening or narrowing the marker rule moves a
reference between two named categories and leaves the sum alone, so the row can
no longer be satisfied by editing the rule -- and it was proved to fail by
feeding it a census with one marker removed.

Rule: when a gate duplicates a rule the product owns, prefer a closed sum over a
mirrored copy. A mirror needs a human to keep it honest; a sum does not.

## 2026-09-19 -- the wrong instrument, then believing what it said

Lane CELLVIEW4B checked whether `WW_CELLSPLAT_LAYER_INDEX` had reached the
compiler by running `grep -c WW_CELLSPLAT_LAYER_INDEX Makefile.Release`, got 0,
and concluded the 13-edit hook-up was incomplete and needed a 14th edit. It is a
`#define` in `src/esmdata.h`, included by `cellsplat.cpp` before the function
that reads it. The Makefile has no reason to mention it and never would.

Self-corrected in the same step, and then confirmed the right way -- at RUNTIME,
by the census saying "layers composited in the ATXT paint order" rather than its
"WITHOUT THE PAINT ORDER" wording. The near-miss was inventing an edit to the
build to satisfy an instrument that was never pointed at the question.

Rule: `grep Makefile.Release` answers "did the .pro pass a DEFINE", and nothing
else. A header macro is confirmed by compiling, or by the behaviour it gates.

## 2026-09-19 -- a failed patch script repaired with sed, and left broken source

Lane CELLVIEW4B's fix01 anchors for `src/cellsplat.cpp` carried five tabs where
the file has six, so `--check` matched 0 of them. Rather than reading the file's
actual bytes, the lane tried to repair its own patch script with a `sed` one
liner, lost a round of escaping, and wrote a stray `> = 'out.quadsBare++;'` line
into the script -- a syntax error introduced while fixing a syntax problem.

Fixed by printing the real bytes (`repr(t[i-12:i+80])`) and using the editor
with the exact anchors, after which all 11 matched once.

Rule: when an anchor misses, read the bytes. A patch script is source too, and
it is never repaired with a one-liner in the shell.

"""


def main():
    with io.open(PATH, 'rb') as fh:
        raw = fh.read()
    cr_before = raw.count(b'\r')
    lf_before = raw.count(b'\n')
    marker = b'\r\n## '
    i = raw.index(marker) + 2          # just after the \r\n, before the '## '
    body = ENTRIES.replace('\n', '\r\n').encode('utf-8')
    out = raw[:i] + body + raw[i:]
    cr_after = out.count(b'\r')
    lf_after = out.count(b'\n')
    added = body.count(b'\r')
    assert cr_after == cr_before + added, 'CR moved by %d, expected %d' % (
        cr_after - cr_before, added)
    assert cr_after == lf_after, 'CR and LF disagree -- a bare LF got in'
    with io.open(PATH, 'wb') as fh:
        fh.write(out)
    print('MISTAKES.md  bytes %d -> %d' % (len(raw), len(out)))
    print('  CR %d -> %d   LF %d -> %d   (lines added %d)'
          % (cr_before, cr_after, lf_before, lf_after, added))


main()
