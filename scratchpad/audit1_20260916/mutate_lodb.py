"""AUDIT1 step 3: doctor one field of a BAKEREC1 v2 plain-text record in place.

Two cases, and each one is aimed at a field the reader it refutes ACTUALLY
READS. The first draft of the `hash` case mutated the first 40-hex digest in the
file, which is the `switches` digest; `lodgen_bakerec_gate.py hashes` never
looks at that line, so the mutation proved nothing about the reader and the
refuter reported NOT CAUGHT on a reader that is fine. A refuter has to be aimed.

    hash     one hex digit of `hash objectCorpusHash <16 hex>`, which `hashes`
             compares against the .lodo header word.
    dropout  the first `out` row deleted, which `sections` compares against the
             files on disk.

usage: python mutate_lodb.py <hash|dropout> <file.lodb>
"""
import io
import re
import sys

TAB = chr(9)


def main():
    case, path = sys.argv[1], sys.argv[2]
    s = io.open(path, encoding='utf-8', newline='').read()
    if case == 'hash':
        pat = re.compile('^hash' + TAB + 'objectCorpusHash' + TAB
                         + '([0-9a-f]{16})$', re.M)
        m = pat.search(s)
        if not m:
            print('  ABORT: no objectCorpusHash row')
            return 2
        d = m.group(1)
        bad = ('0' if d[0] != '0' else '1') + d[1:]
        s = s[:m.start(1)] + bad + s[m.end(1):]
        print('  objectCorpusHash %s -> %s' % (d, bad))
    elif case == 'dropout':
        lines = s.split('\n')
        for i, l in enumerate(lines):
            if l.startswith('out' + TAB):
                print('  dropped: %s' % l[:58])
                del lines[i]
                break
        else:
            print('  ABORT: no out row')
            return 2
        s = '\n'.join(lines)
    else:
        print('  ABORT: unknown case %s' % case)
        return 2
    io.open(path, 'w', encoding='utf-8', newline='').write(s)
    return 0


if __name__ == '__main__':
    sys.exit(main())
