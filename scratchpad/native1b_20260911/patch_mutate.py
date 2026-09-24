"""v3 mutations for tests/spells/lodgen_native_mutate.py -- one per NEW field
and per NEW rule, each named so the RULE and not a checksum answers."""
import sys

SRC = "tests/spells/lodgen_native_mutate.py"
EDITS = []


def edit(old, new):
    EDITS.append((old, new))


# ---- re-signing has to know about the two new payloads, or every v3 case is
#      answered by a CRC and proves nothing ----
edit(
    '''def resign_lodo(b):
    """indexCrc32 over the seven payloads in file order, then headerCrc32."""
    (baseCount, meshCount, clusterCount, materialCount, vertexCount) = get(b, 0x50, 'IIIII')
    stringBytes = get(b, 0x6C, 'I')[0]
    offs = get(b, 0x70, 'QQQQQQQ')
    sizes = [baseCount * 32, meshCount * 56, clusterCount * 16, materialCount * 16,
             clusterCount * 48, vertexCount * 16, stringBytes]
    crc = 0
    for off, size in zip(offs, sizes):
        crc = crc32(bytes(b[off:off + size]), crc)
    put(b, 0xA8, 'I', crc)
    put(b, 0x0C, 'I', crc32(bytes(b[0x10:0x100])))''',
    '''def resign_lodo(b):
    """v3: indexCrc32 over the EIGHT payloads in file order, then headerCrc32.
    The ladder table sits between the clusters and the materials."""
    (baseCount, meshCount, clusterCount, materialCount, vertexCount) = get(b, 0x50, 'IIIII')
    stringBytes = get(b, 0x6C, 'I')[0]
    (offBases, offMeshes, offClusters, offMaterials,
     offLocal, offVerts, offStrings) = get(b, 0x70, 'QQQQQQQ')
    offLods = get(b, 0xC0, 'Q')[0]
    table = [(offBases, baseCount * 32), (offMeshes, meshCount * 56),
             (offClusters, clusterCount * 16), (offLods, clusterCount * 48),
             (offMaterials, materialCount * 16), (offLocal, clusterCount * 48),
             (offVerts, vertexCount * 16), (offStrings, stringBytes)]
    crc = 0
    for off, size in table:
        crc = crc32(bytes(b[off:off + size]), crc)
    put(b, 0xA8, 'I', crc)
    put(b, 0x0C, 'I', crc32(bytes(b[0x10:0x100])))''')

edit(
    """    idx = bytes(b[offChunks:offChunks + chunkCount * 32]) + \\
        bytes(b[offCells:offCells + presentChunks * 16 * 8])
    put(b, 0x64, 'I', crc32(idx))""",
    """    offOcc, offOccRange = get(b, 0x98, 'QQ')
    occCount = get(b, 0xA8, 'I')[0]
    idx = bytes(b[offChunks:offChunks + chunkCount * 32]) + \\
        bytes(b[offCells:offCells + presentChunks * 16 * 8]) + \\
        bytes(b[offOcc:offOcc + occCount * 40]) + \\
        bytes(b[offOccRange:offOccRange + presentChunks * 16 * 8])
    put(b, 0x64, 'I', crc32(idx))""")

# ---- the two reserved-byte cases moved with the header room ----
edit(
    """    add('v2 lodo reserved byte at 0xC0', 'lodo', lambda b: put(b, 0xC0, 'B', 1), True, '0xC0')
    add('v2 lodi reserved byte at 0x98', 'lodi', lambda b: put(b, 0x98, 'B', 1), True, '0x98')
    add('v2 lodo unknown header flag bit', 'lodo',
        lambda b: put(b, 0x08, 'I', get(b, 0x08, 'I')[0] | 8), True, 'reserved flag')""",
    """    add('v3 lodo reserved byte at 0xCE', 'lodo', lambda b: put(b, 0xCE, 'B', 1), True, '0xCE')
    add('v3 lodi reserved byte at 0xB0', 'lodi', lambda b: put(b, 0xB0, 'B', 1), True, '0xB0')
    add('v3 lodo unknown header flag bit', 'lodo',
        lambda b: put(b, 0x08, 'I', get(b, 0x08, 'I')[0] | 0x10), True, 'reserved flag')""")

# ---- the v3 cases ----
edit(
    """    if baseCount:
        pass
    return C""",
    """    # ------------------------------------------------------------------ v3
    offLods = get(lodo, 0xC0, 'Q')[0]
    offMeshes = get(lodo, 0x78, 'Q')[0]
    clusterCount = get(lodo, 0x58, 'I')[0]

    def lodrow(i):
        return get(lodo, offLods + i * 48, 'ffffffIHBBHHfII')

    add('v3 lodo version back to 2', 'lodo', lambda b: put(b, 0x04, 'I', 2), True, 'version 2')
    add('v3 lodi version back to 2', 'lodi', lambda b: put(b, 0x04, 'I', 2), True, 'version 2')
    add('v3 lodo clusterLodStride not 48', 'lodo', lambda b: put(b, 0xC8, 'I', 40), True,
        'clusterLodStride')
    add('v3 lodo levelMax above the table', 'lodo',
        lambda b: put(b, 0xCC, 'B', get(b, 0xCC, 'B')[0] + 1), True, 'levelMax')
    add('v3 lodo LADDER flag set with ladderGroup 0', 'lodo', lambda b: put(b, 0xCD, 'B', 0), True,
        'ladderGroup')
    add('v3 lodo ladder row reserved field', 'lodo',
        lambda b: put(b, offLods + 0x1F, 'B', 1), True, 'reserved')
    add('v3 lodo level-0 cluster given an error', 'lodo',
        lambda b: put(b, offLods + 0x10, 'f', 1.0), True, 'level 0')
    add('v3 lodo level-0 sourceTriangles not its own triangleCount', 'lodo',
        lambda b: put(b, offLods + 0x28, 'I', 999), True, 'sourceTriangles')
    add('v3 lodo bounding-sphere radius 0', 'lodo',
        lambda b: put(b, offLods + 0x0C, 'f', 0.0), True, 'radius')
    add('v3 lodo mesh clusterCountL0 wrong', 'lodo',
        lambda b: put(b, offMeshes + 0x34, 'H', 99), True, 'clusterCountL0')
    add('v3 lodo mesh levelCount wrong', 'lodo',
        lambda b: put(b, offMeshes + 0x36, 'B', 9), True, 'levelCount')
    # the CONE that must open: clearing the flag on a cluster that carries no
    # cone leaves a cosine of -1 where a real cone must be in (0, 1]
    coneOpen = None
    for i in range(clusterCount):
        if get(lodo, offClusters + i * 16 + 14, 'H')[0] & 4:
            coneOpen = i
            break
    if coneOpen is not None:
        add('v3 lodo CONE_OPEN cleared on a cluster with no cone', 'lodo',
            lambda b, i=coneOpen: put(b, offClusters + i * 16 + 14, 'H',
                                      get(b, offClusters + i * 16 + 14, 'H')[0] & ~4), True, 'cone')
        add('v3 lodo CONE_OPEN set but an axis is stored', 'lodo',
            lambda b, i=coneOpen: put(b, offLods + i * 48 + 0x20, 'H', 1234), True, 'CONE_OPEN')
    # MONOTONICITY: a child that deviates MORE than the parent replacing it.
    # parentError is lowered (not the parent's own error), so the monotone rule
    # is the one with something to say and the parent-consistency rule is not.
    child = None
    for i in range(clusterCount):
        r = lodrow(i)
        if r[8] >= 1 and r[6] != 0xFFFFFFFF and r[4] > 0.0:
            child = i
            break
    if child is not None:
        add('v3 lodo a child deviates more than its parent', 'lodo',
            lambda b, i=child: put(b, offLods + i * 48 + 0x14, 'f', lodrow(i)[4] * 0.5), True,
            'monotone')

    # ---- the occluders ----
    occCount = get(lodi, 0xA8, 'I')[0]
    offOcc = get(lodi, 0x98, 'Q')[0]
    add('v3 lodi occluderStride not 40', 'lodi', lambda b: put(b, 0xAC, 'H', 32), True,
        'occluderStride')
    add('v3 lodi maxOccludersPerCell 0', 'lodi', lambda b: put(b, 0xAE, 'H', 0), True,
        'maxOccludersPerCell')
    if occCount:
        add('v3 lodi occluder reserved word', 'lodi',
            lambda b: put(b, offOcc + 0x24, 'I', 1), True, 'reserved')
        add('v3 lodi occluder flag bit0 clear', 'lodi',
            lambda b: put(b, offOcc + 0x1E, 'H', 0), True, 'flags')
        add('v3 lodi occluder half extent 0', 'lodi',
            lambda b: put(b, offOcc + 0x0C, 'f', 0.0), True, 'half extent')
        add('v3 lodi occluder names an instance outside its cell', 'lodi',
            lambda b: put(b, offOcc + 0x20, 'I', get(b, 0x58, 'I')[0] - 1), True, 'cell')
    return C""")


def main():
    apply = "--apply" in sys.argv
    raw = open(SRC, "rb").read()
    cr = raw.count(b"\r")
    text = raw.decode("utf-8")
    ok = True
    for old, new in EDITS:
        n = text.count(old)
        print("x%d  %s" % (n, old.strip().split("\n")[0][:74]))
        if n != 1:
            ok = False
    if not ok:
        print("REFUSED: every anchor must match exactly once")
        return 1
    for old, new in EDITS:
        text = text.replace(old, new, 1)
    data = text.encode("utf-8")
    print("%d -> %d bytes, CR %d -> %d" % (len(raw), len(data), cr, data.count(b"\r")))
    if data.count(b"\r") != cr:
        print("REFUSED: CR count moved")
        return 1
    if not apply:
        print("--check only: nothing written")
        return 0
    open(SRC, "wb").write(data)
    print("applied")
    return 0


sys.exit(main())
