#!/usr/bin/env python3
"""fix01.py -- repair the usage block a heredoc mangled.

The insert went in with a real newline where a two-character `\\n` escape
belonged, and it landed between the `--info` line and its own continuation.
Both are undone here and the entry is put after that continuation instead.
(nifskope-ww-build-verify: patch with a SCRIPT FILE, never a heredoc -- this is
exactly the failure it warns about.)
"""

import sys

P = 'src/nifcli.cpp'
raw = open(P, 'rb').read()
assert raw.count(b'\r') == 0, 'src is LF-only'
s = raw.decode('utf-8')

BROKEN = (
    '\t\t  << "  lodl <file.lodl> --water-census          the version-3 water body table,\n'
    '"\n'
    '\t\t  << "                                          read back out of the file itself\n'
    '"\n'
)
assert s.count(BROKEN) == 1, 'the broken block is not there exactly once'
s = s.replace(BROKEN, '', 1)

ANCHOR = '\t\t  << "                                          sections present and the plane keys\\n"\n'
assert s.count(ANCHOR) == 1, 'the --info continuation is not there exactly once'
GOOD = (
    '\t\t  << "  lodl <file.lodl> --water-census          the version-3 water body table,\\n"\n'
    '\t\t  << "                                          read back out of the file itself\\n"\n'
    '\t\t  << "  lodl <file.lodl> --water-selftest       the classifier\'s known-answer\\n"\n'
    '\t\t  << "                                          control, with its refuter\\n"\n'
)
s = s.replace(ANCHOR, ANCHOR + GOOD, 1)

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('ok, %d bytes' % len(out))
