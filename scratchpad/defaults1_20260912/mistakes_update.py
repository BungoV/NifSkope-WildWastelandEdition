"""DEFAULTS1: update this lane's MISTAKES block, in the lane file and at the
TOP of root MISTAKES.md, where the section already sits. LF only; CR asserted 0.

Backslashes are built with chr(92); nothing carrying one passes through a shell.
"""
BS = chr(92)
ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
LANE = ROOT + '/scratchpad/defaults1_20260912/MISTAKES_ENTRIES.md'
MAIN = ROOT + '/MISTAKES.md'

ADD1 = """
   OCCURRENCE COUNT, ADDED LATER THE SAME SESSION: a fourth time, in a
   throwaway probe rather than a patch -- `python - <<'PY'` carrying
   `if '@BS@' in s:` arrived as `if '` and died with
   `SyntaxError: EOL while scanning string literal`. The rule above is not
   about PATCHES, it is about any Python that carries a backslash: it goes in
   a file. `scratchpad/defaults1_20260912/btrstrings.py` is that probe, and it
   builds the character as `BS = chr(92)`.
""".replace('@BS@', BS)

ADD_NEW = """
3. **Two renders in a row measured something other than what I said they
   measured, and both times the frame looked plausible.** First, the `.BTR`
   pictures came back as one flat colour because the camera was pinned at the
   `.BTO`'s WORLD centre: a `.BTO` is in world units, a `.BTR`'s Land shape is
   in the FILE's own space (bounding sphere centre 2048,2048), and the `chunk`
   node scales the 4096-unit box by 4, so the camera is 8192,8192,0 at ortho
   half-width 8192. Second -- worse, because the frame was a perfectly good
   picture of the wrong file -- the paint-1 and paint-0 renders came back
   PIXEL-IDENTICAL (difference bounding box None) while the two sheets differ
   by 8,837 of 174,888 bytes, because `WW_LODGEN_RESOURCES="$root;$DATA"` lets
   the game's own `Commonwealth.4.-20.20.DDS` win the lookup; every panel was
   the SHIPPED sheet. Found by a swap test: rendering one bake's `.BTR` against
   the OTHER bake's resource root gave a byte-identical frame, which is
   impossible if the root is being read. THE RULE: a render that is meant to
   show a bake's own texture gets the shim root ALONE, never with the vanilla
   Data appended, and every picture pair is checked with a difference number
   BEFORE it is described -- an identical pair is a broken measurement, not a
   result.

4. **`git show HEAD:` was used as the "before" column of CHANGED_FILES.txt in
   a shared tree that is many lanes ahead of HEAD.** The table claimed
   `src/lodgen.cpp` went 364,264 -> 594,075 bytes, which is every lane's
   uncommitted work since the last commit and not remotely this lane's diff;
   `MISTAKES.md` read 68,834 -> 428,704 the same way. Nobody was misled because
   I read my own table, but a reader of the report would have been. THE RULE:
   in a shared worktree, "before" is a number this lane MEASURED before it
   edited, or the honest words `not snapshotted`; HEAD may appear only in a
   separately labelled block that says it is a landmark, not a baseline.
"""

lane = open(LANE, 'rb').read().decode('utf-8')
assert lane.count('so the cost was time, not a corrupt source.\n') == 1
lane = lane.replace('so the cost was time, not a corrupt source.\n',
                    'so the cost was time, not a corrupt source.\n' + ADD1)
assert lane.rstrip().endswith('cannot be silent again.')
lane = lane.rstrip('\n') + '\n' + ADD_NEW
assert BS + 'n' not in ADD1 or True

main = open(MAIN, 'rb').read().decode('utf-8')
old_block = open(LANE, 'rb').read().decode('utf-8').rstrip('\n')
assert main.count(old_block) == 1, main.count(old_block)
main = main.replace(old_block, lane.rstrip('\n'))

for path, text in ((LANE, lane), (MAIN, main)):
    data = text.encode('utf-8')
    assert data.count(b'\r') == 0, path
    open(path, 'wb').write(data)
    print('%-70s %7d bytes  LF %5d  CR %d' % (path, len(data), data.count(b'\n'), data.count(b'\r')))
