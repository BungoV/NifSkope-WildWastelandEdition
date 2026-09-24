#!/usr/bin/env python3
"""Append lane FILESTAB's two mistakes to the root MISTAKES.md.

APPEND-ONLY and idempotent: another lane is writing into the same file, so the
bytes are re-read at the moment of writing, nothing already there is touched,
and the run refuses if the heading is present. The file is LF-only (CR 0) and
the assertion below keeps it so.
"""
import io
import os

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
DEST = os.path.join(REPO, 'MISTAKES.md')
HEAD = '## 2026-09-10 -- lane FILESTAB: the heredoc, twice, and two anchors that were not unique'

TEXT = '''

''' + HEAD + '''

**1. A Bash heredoc halved the backslashes, twice, in one lane.**

The first time was the inventory script: `[^"\\\\]` inside a `python - <<'PY'`
heredoc arrived as `[^"\\]` and Python refused with "unterminated character
set". The second time was appending this lane's own report through
`cat >> file << 'EOF'`, which died with "unexpected EOF while looking for
matching quote" and wrote nothing at all.

**What is true instead.** Text that reaches the Bash tool through a heredoc is
not the text that was written. This is recorded in three skills already --
`nifskope-ww-build-verify`, `nifskope-ww-resume-pending` and
`ww-anchored-hookup` all name it -- and lanes HKX1, HKX2 and BUILD6 each paid
for it on 2026-09-10 before this one did.

**Why it happened anyway.** Both times the content "was not a patch script": a
grep, and a block of prose. The rule as written is attached to patch scripts, so
it was read as not applying.

**The rule, restated so it has no exception.** EVERY script and EVERY
multi-line text goes through the Write tool, whatever it is for -- a grep, a
report, a changelog entry, a shell one-liner with a quote in it. There is no
category of heredoc that is safe.

**2. Two hook-up anchors were declared unique and were not.**

`const int source = nameIndex.data( NifBrowserSourceRole ).toInt();` occurs in
THREE functions of `src/nifskope.cpp` (`openArchiveFile`, and two browser
helpers below it), and `"Use as Skeleton for Loaded NIFs"` occurs THREE times,
not the two menu items expected -- the third is a comment at
`src/nifskope.cpp:921` that quotes the menu item by name.

**How it was found.** `hookup.py --check` prints the COUNT for every anchor and
refuses the whole run when one does not match its declared number; it printed
`found 3 REFUSE` twice and wrote nothing. That is precisely what
`ww-anchored-hookup` section 4 says the script must do instead of printing "ok",
so the cost was two minutes rather than a build.

**The rule.** An anchor's expected count is DECLARED and asserted, never
assumed to be 1; and a `--check` that prints "ok" instead of the number is not a
check. When a count comes back high, look at the extra hits before widening the
anchor -- one of them here was a comment that had to be renamed with the label
it quotes, and silently excluding it would have left the comment pointing at a
menu item that no longer exists.
'''


def main():
    with io.open(DEST, 'rb') as f:
        data = f.read()
    if HEAD.encode('utf-8') in data:
        print('already present; nothing written')
        return
    cr_before = data.count(b'\r')
    out = data + TEXT.encode('utf-8')
    assert out.count(b'\r') == cr_before, 'CR count moved'
    with io.open(DEST, 'wb') as f:
        f.write(out)
    print('appended %d bytes; CR %d -> %d' % (len(TEXT), cr_before, out.count(b'\r')))


if __name__ == '__main__':
    main()
