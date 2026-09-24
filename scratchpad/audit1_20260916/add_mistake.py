import io

p = 'E:/Projects/NifskopeWildWastelandEdition/MISTAKES.md'
s = io.open(p, encoding='utf-8', newline='').read()
anchor = '## 2026-09-17 15:41 -- director, PERF1 landing splice'
entry = '''## 2026-09-17 16:27 -- lane AUDIT1, a patch script that truncated its target before it failed

- 16:27 -- I patched a new reader (`tests/spells/lodgen_lodl_pyramid.py`) with a script whose last two
  lines were `io.open(p, 'w', ...).write(s)` and then `ast.parse(s)`; the `open` argument carried a
  doubled backslash (`newline='\\\\n'`), so Python raised `ValueError: illegal newline value` -- AFTER the
  open had already truncated the file to zero bytes. The patch never ran, the file was gone, and the
  next grep of it returned nothing at all rather than an error. Found because `grep -n` on my own new
  file printed nothing; repaired by rewriting the file whole from the Write tool. **The rule: a patch
  script validates everything it can BEFORE it opens the target for writing -- parse the new text, then
  write -- and it writes through a temporary file it renames, because `open(p, "w")` destroys the
  target at the call, not at the write.** The same lane had already been told by the skill that no text
  with a backslash goes through a heredoc; this is that trap one layer down, where the damage is
  silent.

- 16:20 -- the same file, first version: I "measured" the `.lodl` pyramid over `range(cellsX)` when the
  grid is `cellsX * spc` wide (32 samples a cell edge), so the bijection check walked 192 of 37.7
  million samples and the cell cross-check compared cell (cx,cy) against a sample from cell
  (cx/32, cy/32). It printed three failures that were entirely mine, and one of them -- "16,513 cells
  outside their own height range" -- looks exactly like a writer defect. Found by reading the header
  field I had decoded and ignored (`spc`). **The rule: a header field that the decoder unpacks and the
  checker never uses is a question, not a spare -- ask what it is for before trusting a coordinate.**

'''
assert s.count(anchor) == 1
s = s.replace(anchor, entry + anchor, 1)
io.open(p, 'w', encoding='utf-8', newline='').write(s)
b = open(p, 'rb').read()
print('CR %d LF %d' % (b.count(b'\r'), b.count(b'\n')))
