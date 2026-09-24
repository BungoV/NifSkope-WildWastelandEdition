"""DEFAULTS1: the heredoc trap hit a FIFTH time; the ledger says so.

Written as a file (the rule this entry is about). LF only, CR asserted 0.
"""
ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
LANE = ROOT + '/scratchpad/defaults1_20260912/MISTAKES_ENTRIES.md'
MAIN = ROOT + '/MISTAKES.md'

OLD = """   builds the character as `BS = chr(92)`.
"""
NEW = """   builds the character as `BS = chr(92)`.

   AND A FIFTH, an hour later, in the patch that re-bases the panel self-test:
   `python - <<'PY'` carrying a C++ `"\\n"` literal inside an anchor arrived
   with a real newline, so the anchor count was 0 while the head anchor in the
   same script matched. The script REFUSED and wrote nothing, which is the only
   reason this is a note and not a corrupt `src/nifskope_ui.cpp`. Rewritten as
   `scratchpad/defaults1_20260912/fix_tickedbox.py` with the Write tool, and the
   anchor re-chosen so it quotes no escape sequence at all -- the better fix,
   because an anchor that cannot carry a backslash cannot lose one.
"""

lane = open(LANE, 'rb').read().decode('utf-8')
assert lane.count(OLD) == 1, lane.count(OLD)
new_lane = lane.replace(OLD, NEW)

main = open(MAIN, 'rb').read().decode('utf-8')
assert main.count(OLD) == 1, main.count(OLD)
new_main = main.replace(OLD, NEW)

for path, text in ((LANE, new_lane), (MAIN, new_main)):
    data = text.encode('utf-8')
    assert data.count(b'\r') == 0, path
    open(path, 'wb').write(data)
    print('%-72s %7d bytes  LF %5d  CR %d' % (path, len(data), data.count(b'\n'), data.count(b'\r')))
