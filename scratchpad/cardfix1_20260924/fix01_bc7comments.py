# CARDFIX1 step 1: drop the stale "no BC7 encoder in this tree" comments (IMPOSTORDEPTH2 added
# src/lodgenbc7.h). The _msn cache stays uncompressed; BC7 there is unruled (R9 ruled "No" for the
# card cache for now), so the comments now say THAT instead of claiming there is no encoder.
ROOT = 'E:/Projects/NifskopeWWE-cardfix1/'


def patch(path, edits):
    b = open(ROOT + path, 'rb').read()
    cr = b.count(b'\r')
    s = b.decode('utf-8')
    for old, new in edits:
        c = s.count(old)
        assert c == 1, (path, old[:70], c)
        s = s.replace(old, new)
    out = s.encode('utf-8')
    assert out.count(b'\r') == cr, path
    open(ROOT + path, 'wb').write(out)
    print('patched', path)


patch('src/lodgen.h', [
    (' *  There is no BC7 encoder in this tree, so BC7 is not offered; the size that\n'
     ' *  costs is in the lane report, for bungo to rule on. */\n',
     ' *  A BC7 encoder exists (src/lodgenbc7.h, lane IMPOSTORDEPTH2, used for the\n'
     ' *  card `_n` sheets) but this cache does not use it: BC7 here is unruled, and\n'
     ' *  the size the uncompressed sheet costs is in the lane report. */\n'),
])
patch('src/nifcli.cpp', [
    ('\t\t * thing the cache removed. There is no BC7 encoder in this tree. What\n',
     '\t\t * thing the cache removed. BC7 (src/lodgenbc7.h) is not used here. What\n'),
])
