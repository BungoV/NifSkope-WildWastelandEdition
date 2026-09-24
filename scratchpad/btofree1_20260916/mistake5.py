# Lane BTOFREE1, 2026-09-16 -- entry 5, added to the lane's own file and spliced
# into the root ledger in the same run so the two cannot drift.
L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/btofree1_20260916/MISTAKES_ENTRIES.md'
R = 'E:/Projects/NifskopeWildWastelandEdition/MISTAKES.md'

ENTRY = '''
5. **Did mistake 1 again, in the same lane, forty minutes after writing it down
   -- and this time it edited a file instead of failing to start.**
   I wanted one `awk` line added to my own chain script and reached for
   `python -c "... printf ...n ..."` rather than opening the file. The escape
   collapsed on the way through the shell, the newline I meant to be two
   characters inside an awk format string arrived as a REAL newline, and the
   script was written with an awk program split across two lines. `bash -n`
   passed it, because a newline inside single quotes is valid shell; awk would
   have refused the string at run time, in a step forty minutes into a chain.
   **How it was found:** the tool that applied the edit echoed the changed lines
   back and the newline was visible in them. Nothing in my own process caught it.
   **What is different from entry 1:** entry 1 said "if it contains a backslash
   or an apostrophe, write it to a file". I read that as advice about LONG or
   TRICKY commands and made an exception for a one-line edit. There is no size
   exemption. The rule is not "be careful with backslashes", it is "a backslash
   never goes through a shell argument", and the one-line edit is exactly where
   it will not be noticed. The script was rewritten whole with the Write tool.
'''

b = open(L, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')
assert s.count('5. **') == 0
s = s.rstrip('\n') + '\n' + ENTRY
nb = s.encode('utf-8')
assert nb.count(b'\r') == cr0
open(L, 'wb').write(nb)
print('MISTAKES_ENTRIES.md %d -> %d B, CR %d, LF %d' % (len(b), len(nb), nb.count(b'\r'), nb.count(b'\n')))

rb = open(R, 'rb').read()
rcr0 = rb.count(b'\r')
rs = rb.decode('utf-8')
# the lane's block ends where the next lane's heading begins; entry 4's last
# line is the one the splice put in, so anchor on it rather than on a heading.
tail = '   you before you treat a difference as a regression.\n'
assert rs.count(tail) == 1, 'anchor count %d' % rs.count(tail)
rs = rs.replace(tail, tail + ENTRY)
rnb = rs.encode('utf-8')
assert rnb.count(b'\r') == rcr0
open(R, 'wb').write(rnb)
print('MISTAKES.md %d -> %d B, CR %d, LF %d' % (len(rb), len(rnb), rnb.count(b'\r'), rnb.count(b'\n')))
