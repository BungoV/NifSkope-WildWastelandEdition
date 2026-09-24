"""Lane LAYOUT1 (2026-09-16): the fourth entry -- the census that counted notes
instead of files. Written to a file because the text contains backslashes and
apostrophes."""
P = 'E:/Projects/NifskopeWildWastelandEdition/MISTAKES.md'
ANCHOR = '''2. **Wrote a shell harness through a heredoc twice more and shipped two'''
NEW = '''2. **Wrote a census field that counted NOTES instead of FILES, and it read
   six too high on the very first bake it described.**
   Every FO4CS-target write notes its path, and the clause states how many
   landed under the root. Two passes write into the same `Objects/` folder --
   the texture arrays, then the impostor card arrays -- and each notes the
   whole folder when it finishes, so the five array sheets and their sidecar
   were counted twice: `layout ... 85 file(s)` over a tree holding 79. A census
   field states what is on disk or it states nothing (CONSTITUTION 4), and this
   one stated an arithmetic artefact of how often a writer happened to report.
   **How it was found:** leg (f) of the new gate counts the tree itself and
   compares. It was written that way on purpose -- a census leg that merely
   re-printed the census would have agreed with the bug.
   **The rule:** a counted census field counts a SET of absolute paths, folded
   for case, never an increment per call. If two writers can touch one folder,
   they will.

'''
s = open(P, encoding='utf-8', newline='').read()
assert s.count(ANCHOR) == 1, s.count(ANCHOR)
s = s.replace(ANCHOR, NEW + '3. **Wrote a shell harness through a heredoc twice more and shipped two', 1)
# the two entries that followed shift down by one
s = s.replace('3. **Ran three failed builds in a row by guessing at the toolchain',
              '4. **Ran three failed builds in a row by guessing at the toolchain', 1)
open(P, 'w', encoding='utf-8', newline='').write(s)
d = open(P, 'rb').read()
print('ok CR %d LF %d' % (d.count(b'\r'), d.count(b'\n')))
