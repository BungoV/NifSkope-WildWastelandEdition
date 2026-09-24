"""CARDLINK1 patch 2: src/lodofile.cpp (cardCount recount, version strings) + src/lodofile.h comments."""
import sys
WT = 'E:/Projects/NifskopeWWE-cardlink1'
P = WT + '/scratchpad/cardlink1_20260924/patch/'

def snip(n):
    with open(P + n, 'rb') as f:
        return f.read().replace(b'\r\n', b'\n')

def patch(path, reps):
    with open(path, 'rb') as f:
        b = f.read()
    cr0 = b.count(b'\r')
    for old, new in reps:
        c = b.count(old)
        if c != 1:
            sys.exit('%s: anchor count %d for %r' % (path, c, old[:80]))
        b = b.replace(old, new)
    if b.count(b'\r') != cr0:
        sys.exit('%s: CR count moved' % path)
    with open(path, 'wb') as f:
        f.write(b)
    print('patched', path, len(reps), 'anchors')

patch(WT + '/src/lodofile.cpp', [
    (b'Re-bake; this reader knows version 3 only" ) );',
     b'Re-bake; this reader knows version 4 only" ) );'),
    (b'Re-bake; this reader knows version 3" ) );',
     b'Re-bake; this reader knows version 4" ) );'),
    (b'\tif ( h.baseCount ) std::memcpy( L.bases.data(), p + h.offBases, tabs[0].bytes );\n',
     snip('s_recount.txt')),
])

patch(WT + '/src/lodofile.h', [
    (b'\t *  and a consumer sizes its card draw list from the header alone. It is\n'
     b'\t *  0 on a bake that assigns no card, which is still every bake the emitter\n'
     b'\t *  makes today (docs 11, deviation 5) -- and the field is proved to MOVE\n'
     b'\t *  on a synthetic library that assigns some, not on the region alone. */\n',
     b'\t *  and a consumer sizes its card draw list from the header alone. It is\n'
     b'\t *  0 on a bake that links no card arrays; a `--native --impostors --arrays`\n'
     b'\t *  bake links them (lane CARDLINK1, `lodgenNativeLinkCards`). The reader\n'
     b'\t *  RECOUNTS it from the base rows and refuses a mismatch by name. */\n'),
])
