#!/usr/bin/env python
"""IMPOSTORFIX1: two more of my own, appended to the root MISTAKES.md.

The file is CRLF throughout (9604 CR, 9604 LF measured before the edit), so the
text is spliced as bytes with \\r\\n and the counts are asserted to move by the
same amount afterwards.
"""
import io

P = 'MISTAKES.md'
b = io.open(P, 'rb').read()
cr0, lf0 = b.count(b'\r'), b.count(b'\n')
assert cr0 == lf0, (cr0, lf0)

TEXT = """
**A failed `git show` still creates the file it was redirecting into, empty.** I
wanted the pre-patch text of `release/shaders/impostor_oct.vert` and ran
`git show HEAD:res/shaders/impostor_oct.vert > release/shaders/impostor_oct.vert`.
The shader is not tracked in HEAD, so git wrote nothing and exited non-zero --
but the SHELL had already truncated the output file before git ever ran. The
deployed shader was 0 bytes, and the next harness run would have drawn nothing
with no error that named the cause. A redirection is performed by the shell
before the command starts, so `>` destroys the target whether or not the command
can produce a replacement. Read a file into a NEW path and move it into place,
or make a copy first; and when the old text is wanted, reverse the patch script
rather than asking version control for something it may not have.

**A re-bake that wrote nothing, while the new code's own log line printed.** I
changed the card writer, re-ran the bake over the fixtures, saw
`lodgen: card <id>: height repaired ...` in the output and took the sheets as
re-baked. They were byte-identical to the ones I had kept aside: `lodgenCard`
guards every write with `if ( !QFile::exists( ... ) )`, so the repair ran, said
so, and then skipped the file. The log line proved the code executed and proved
nothing about the bytes on disk. Delete the outputs before a re-bake, and prove
the re-bake by `cmp` against the copies you kept -- a generator that can skip
its own output is a generator whose log is not evidence.
"""

add = TEXT.replace('\n', '\r\n').encode('utf-8')
assert b.endswith(b'\r\n'), b[-4:]
b = b + add
io.open(P, 'wb').write(b)
cr1, lf1 = b.count(b'\r'), b.count(b'\n')
assert cr1 == lf1, (cr1, lf1)
print('MISTAKES.md %d -> %d bytes, CR %d -> %d, LF %d -> %d'
      % (len(b) - len(add), len(b), cr0, cr1, lf0, lf1))
