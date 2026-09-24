"""Mutation gate (e) run 1 found three holes: the C++ XML route ignored the
`numelements` attribute on `data`; the Python route crashed (traceback, rc 1)
on a data byte out of range and on malformed XML instead of refusing."""

def patch(path, pairs):
    b = open(path, 'rb').read()
    cr = b.count(b'\r')
    s = b.decode('utf-8')
    for a, c in pairs:
        assert s.count(a) == 1, (path, a[:70], s.count(a))
        s = s.replace(a, c)
    out = s.encode('utf-8')
    assert out.count(b'\r') == cr
    open(path, 'wb').write(out)
    print('patched', path)

patch('src/hkxanim.cpp', [
    ('\tQHash<QString, QString> text;              //!< hkparam name -> text\n',
     '\tQHash<QString, QString> text;              //!< hkparam name -> text\n\tQHash<QString, int> numelements;           //!< hkparam name -> its numelements attribute, -1 when absent\n'),
    ('\t\t\tconst QString pname = x.attributes().value( "name" ).toString();\n\t\t\tQString text;\n',
     '\t\t\tconst QString pname = x.attributes().value( "name" ).toString();\n\t\t\tbool hasN = false;\n\t\t\tconst int ne = x.attributes().value( "numelements" ).toInt( &hasN );\n\t\t\to.numelements.insert( pname, hasN ? ne : -1 );\n\t\t\tQString text;\n'),
    ('\t\t\tfor ( int v : intsOf( o, "data" ) ) {\n\t\t\t\tif ( v < 0 || v > 255 )\n\t\t\t\t\trefuse( QString( "data byte %1 out of range" ).arg( v ) );\n\t\t\t\ta.data.append( char( v ) );\n\t\t\t}\n',
     '\t\t\tfor ( int v : intsOf( o, "data" ) ) {\n\t\t\t\tif ( v < 0 || v > 255 )\n\t\t\t\t\trefuse( QString( "data byte %1 out of range" ).arg( v ) );\n\t\t\t\ta.data.append( char( v ) );\n\t\t\t}\n\t\t\tconst int ne = o.numelements.value( "data", -1 );\n\t\t\tif ( ne >= 0 && ne != a.data.size() )\n\t\t\t\trefuse( QString( "data holds %1 bytes, numelements says %2" ).arg( a.data.size() ).arg( ne ) );\n'),
])

patch('tests/spells/hkxanim_decode.py', [
    ('def parse_xml(path):\n    root = ET.parse(path).getroot()\n',
     'def parse_xml(path):\n    try:\n        root = ET.parse(path).getroot()\n    except ET.ParseError as e:\n        raise Refusal("%s: not well-formed XML: %s" % (path, e))\n'),
    ('        dp = _param(o, "data")\n        a["data"] = bytes(int(t) for t in (dp.text or "").split())\n',
     '        dp = _param(o, "data")\n        if dp is None:\n            raise Refusal("%s: animation %s has no data" % (path, ref))\n        vals = [int(t) for t in (dp.text or "").split()]\n        bad = [v for v in vals if v < 0 or v > 255]\n        if bad:\n            raise Refusal("%s: data byte %d out of range" % (path, bad[0]))\n        a["data"] = bytes(vals)\n'),
])
