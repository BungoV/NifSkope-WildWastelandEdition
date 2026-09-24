p = 'tests/spells/lodgen_native_decode.py'
s = open(p, encoding='utf-8', newline='').read()


def rep(o, n, c=1):
    global s
    assert s.count(o) == c, (s.count(o), o[:90])
    s = s.replace(o, n)


rep("""    if h['version'] not in (3, 4, 5, 6):
        raise Refusal('version %d; this reader knows 3, 4, 5 and 6' % h['version'])
    v6 = h['version'] == 6          # v6 = v5 + the per-instance vertex-AO stream
    v5 = h['version'] == 5 or v6
    if crc32(b[0x10:0x100]) != h['headerCrc32']:
        raise Refusal('headerCrc32 mismatch')""",
    """    if h['version'] not in (3, 4, 5, 6, 7):
        raise Refusal('version %d; this reader knows 3, 4, 5, 6 and 7' % h['version'])
    # v7 = v6 + the group table and/or the per-vertex sky stream, in a 512-byte header BLOCK
    v7 = h['version'] == 7
    v6 = h['version'] == 6 or v7    # v6 = v5 + the per-instance vertex-AO stream
    v5 = h['version'] == 5 or v6
    hdr = 512 if v7 else 256
    if len(b) < hdr:
        raise Refusal('%s: shorter than the %d-byte version-%d header' % (path, hdr, h['version']))
    if crc32(b[0x10:hdr]) != h['headerCrc32']:
        raise Refusal('headerCrc32 mismatch')""")

rep("""    padFrom = 0x100 if v6 else (0xF1 if v5 else (0xD4 if h['version'] == 4 else 0xB0))
    if any(b[padFrom:0x100]):
        raise Refusal('reserved bytes 0x%02X..0xFF not zero' % padFrom)""",
    """    padFrom = 0x100 if v6 else (0xF1 if v5 else (0xD4 if h['version'] == 4 else 0xB0))
    if any(b[padFrom:0x100]):
        raise Refusal('reserved bytes 0x%02X..0xFF not zero' % padFrom)
    # v7: the group table (0x100/0x108/0x10C) and the vertex-sky stream (0x110/0x118)
    h['offGroup'] = h['groupCount'] = h['groupStride'] = 0
    h['offVertexSky'] = h['vertexSkyBytes'] = 0
    if v7:
        h['offGroup'] = le('Q', b, 0x100)[0]
        h['groupCount'] = le('I', b, 0x108)[0]
        h['groupStride'] = le('H', b, 0x10C)[0]
        h['offVertexSky'] = le('Q', b, 0x110)[0]
        h['vertexSkyBytes'] = le('I', b, 0x118)[0]
        if h['offGroup'] == 0 and h['offVertexSky'] == 0:
            raise Refusal('version 7 carrying neither a group table (header 0x100) nor a vertex-sky '
                          'stream (header 0x110); version 7 IS one of the two')
        if h['offGroup']:
            if h['groupStride'] != 2:
                raise Refusal('groupStride %d; this reader knows 2' % h['groupStride'])
            if h['groupCount'] == 0 and h['instanceCount']:
                raise Refusal('a group table is present but groupCount is 0')
        elif h['groupCount'] or h['groupStride']:
            raise Refusal('no group table but groupCount %d / groupStride %d say otherwise'
                          % (h['groupCount'], h['groupStride']))
        if h['offVertexSky']:
            if h['vertexSkyBytes'] < 4 * (h['instanceCount'] + 1):
                raise Refusal('vertex-sky stream of %d bytes cannot hold its own %d offset words'
                              % (h['vertexSkyBytes'], h['instanceCount'] + 1))
        elif h['vertexSkyBytes']:
            raise Refusal('no vertex-sky stream but vertexSkyBytes is %d' % h['vertexSkyBytes'])
        if any(b[0x11C:0x200]):
            raise Refusal('reserved bytes 0x11C..0x1FF not zero')
    elif any(b[0x100:0x120]) and len(b) >= 0x120:
        raise Refusal('version %d carrying version-7 header words; versions 3 to 6 have a 256-byte '
                      'header and end at 0x100' % h['version'])""")

rep("""    iVao = -1
    if v6:
        iVao = len(tabs)
        tabs.append(('vertexAo', h['offVertexAo'], h['vertexAoBytes']))
    prev = 256""",
    """    iVao = -1
    if v6:
        iVao = len(tabs)
        tabs.append(('vertexAo', h['offVertexAo'], h['vertexAoBytes']))
    iGrp = iVsky = -1
    if v7 and h['offGroup']:
        iGrp = len(tabs)
        tabs.append(('group', h['offGroup'], h['instanceCount'] * 2))
    if v7 and h['offVertexSky']:
        iVsky = len(tabs)
        tabs.append(('vertexSky', h['offVertexSky'], h['vertexSkyBytes']))
    prev = hdr""")

rep("""        f = T['vertexAoFirst']
        if f[0] != 0 or any(f[i] > f[i + 1] for i in range(n1 - 1)) or f[-1] != len(T['vertexAo']):
            raise Refusal('vertex-AO offsets are not a monotone run from 0 to the byte count')""",
    """        f = T['vertexAoFirst']
        if f[0] != 0 or any(f[i] > f[i + 1] for i in range(n1 - 1)) or f[-1] != len(T['vertexAo']):
            raise Refusal('vertex-AO offsets are not a monotone run from 0 to the byte count')
    # v7: one u16 a placement, in instance order, dense per CHUNK from 0
    T['group'] = []
    if iGrp >= 0:
        T['group'] = list(le('%dH' % h['instanceCount'], b, h['offGroup'])) if h['instanceCount'] else []
    # v7: s4.8's layout exactly, for sky
    T['vertexSkyFirst'], T['vertexSky'] = [], []
    if iVsky >= 0:
        n1 = h['instanceCount'] + 1
        o = h['offVertexSky']
        T['vertexSkyFirst'] = list(le('%dI' % n1, b, o))
        T['vertexSky'] = list(b[o + 4 * n1:o + h['vertexSkyBytes']])
        f = T['vertexSkyFirst']
        if f[0] != 0 or any(f[i] > f[i + 1] for i in range(n1 - 1)) or f[-1] != len(T['vertexSky']):
            raise Refusal('vertex-sky offsets are not a monotone run from 0 to the byte count')""")

open(p, 'w', encoding='utf-8', newline='').write(s)
print('decoder spliced')
