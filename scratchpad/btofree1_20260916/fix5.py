# Lane BTOFREE1, 2026-09-16 -- entry 5 exists, so the report and the file table
# have to say five, with the new byte counts measured rather than typed.
P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/btofree1_20260916/lane_btofree1_report.md'
C = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/btofree1_20260916/CHANGED_FILES.txt'
M = 'E:/Projects/NifskopeWildWastelandEdition/MISTAKES.md'

mb = open(M, 'rb').read()


def splice(path, pairs):
    b = open(path, 'rb').read()
    cr0 = b.count(b'\r')
    s = b.decode('utf-8')
    for old, new in pairs:
        assert s.count(old) == 1, 'anchor count %d in %s' % (s.count(old), path)
        s = s.replace(old, new)
    nb = s.encode('utf-8')
    assert nb.count(b'\r') == cr0
    open(path, 'wb').write(nb)
    print('%s %d -> %d B, CR %d, LF %d' % (path.rsplit('/', 1)[-1], len(b), len(nb),
                                           nb.count(b'\r'), nb.count(b'\n')))


old_head = ('Four, all written into `MISTAKES_ENTRIES.md` the moment each was recognised and spliced by me to\n'
            'the top of the root `MISTAKES.md` (which is now 458,995 B, LF 7,741, CR 0 -- unchanged ending).\n'
            'Short form:\n')
new_head = ('Five, all written into `MISTAKES_ENTRIES.md` the moment each was recognised and spliced by me to\n'
            'the top of the root `MISTAKES.md` (now %s B, LF %s, CR 0 -- the file was LF-only and still is).\n'
            'Short form:\n' % ('{:,}'.format(len(mb)), '{:,}'.format(mb.count(b'\n'))))

entry5 = '''4. **Read 0.8179 out of the brief as a current measurement** and spent the first minutes of the
   diagnosis looking for what had broken. Nothing had; the number was measured on the old library
   default. A discriminator bake reproduced it to four decimals. A number in a brief is a number
   from an earlier exe.
5. **Did number 1 again, forty minutes after writing it down.** One `awk` line into my own chain
   script, through `python -c`, and the escape collapsed: the two characters I meant as a newline
   inside an awk format string arrived as a real newline and were written into the file. `bash -n`
   passed it -- a newline inside single quotes is valid shell -- and `awk` would have refused at run
   time, forty minutes into a chain. The tool's own echo of the changed lines is what caught it, not
   me. The lesson entry 1 did not spell out and entry 5 does: there is **no size exemption**. I had
   read "write it to a file" as advice about long commands, and a one-line edit is exactly where the
   collapse goes unseen. The script was rewritten whole with the Write tool.
'''

splice(P, [(old_head, new_head),
           ('''4. **Read 0.8179 out of the brief as a current measurement** and spent the first minutes of the
   diagnosis looking for what had broken. Nothing had; the number was measured on the old library
   default. A discriminator bake reproduced it to four decimals. A number in a brief is a number
   from an earlier exe.
''', entry5)])

splice(C, [('M MISTAKES.md                                             455055 ->      458995        0 ->     0         7680 ->   7741',
            'M MISTAKES.md                                             455055 ->      %6d        0 ->     0         7680 ->   %4d'
            % (len(mb), mb.count(b'\n')))])
