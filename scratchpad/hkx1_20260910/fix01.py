"""Move the QRegularExpression include to the top of hkxanim.cpp; let a file
with skeletons and no animation (skeleton.hkx) load; insert the two .pro lines."""
import re

def patch(path, pairs):
    b = open(path, 'rb').read()
    cr = b.count(b'\r')
    s = b.decode('utf-8')
    for a, c in pairs:
        assert s.count(a) == 1, (path, a[:60], s.count(a))
        s = s.replace(a, c)
    out = s.encode('utf-8')
    assert out.count(b'\r') == cr, (path, cr, out.count(b'\r'))
    open(path, 'wb').write(out)
    print('patched', path, len(b), '->', len(out), 'CR', cr)

patch('src/hkxanim.cpp', [
    ('#include <QHash>\n#include <QXmlStreamReader>\n',
     '#include <QHash>\n#include <QRegularExpression>\n#include <QXmlStreamReader>\n'),
    ('} // namespace\n\n#include <QRegularExpression>\n\nHkxAnimFile hkxAnimLoadXml',
     '} // namespace\n\nHkxAnimFile hkxAnimLoadXml'),
    ('\tif ( r.anims.isEmpty() )\n\t\trefuse( "the container holds no animation" );\n',
     '\tif ( r.anims.isEmpty() && r.skeletons.isEmpty() )\n\t\trefuse( "the container holds no animation and no skeleton" );\n'),
])

patch('tests/spells/hkxanim_decode.py', [
    ('    if not result["animations"]:\n        raise Refusal("%s: the container holds no animation" % result["source"])\n',
     '    if not result["animations"] and not result["skeletons"]:\n        raise Refusal("%s: the container holds no animation and no skeleton" % result["source"])\n'),
    ('        r = load(path)\n        a = r["animations"][0]\n        bind = r["bindings"][0] if r["bindings"] else None\n        frames, ov = decode(a, overlap)\n',
     '        r = load(path)\n        if not r["animations"]:\n            for sk in r["skeletons"]:\n                print("skeleton %s bones %d" % (sk["name"], len(sk["boneNames"])))\n            return 0\n        a = r["animations"][0]\n        bind = r["bindings"][0] if r["bindings"] else None\n        frames, ov = decode(a, overlap)\n'),
])

patch('NifSkope.pro', [
    ('\tsrc/gl/hknpdecode.h \\\n', '\tsrc/gl/hknpdecode.h \\\n\tsrc/hkxanim.h \\\n'),
    ('\tsrc/gl/hknpdecode.cpp \\\n', '\tsrc/gl/hknpdecode.cpp \\\n\tsrc/hkxanim.cpp \\\n'),
])
