#!/usr/bin/env python
"""The REFUSAL half of the standalone writer gate (`ww-standalone-writer-gate`)
for the FO4CS-native pair: take a written `.lodo` + `.lodi`, corrupt exactly one
thing, RE-SIGN every CRC that the corruption would otherwise trip, and require
the independent decoder to refuse it BY NAME -- so the row rule answers and not
the checksum.

A mutation that is refused only by a CRC proves the CRC works and nothing else.
Every case below therefore names the substring the refusal must contain, and a
case whose refusal does not mention it FAILS even though the file was rejected.

    python tests/spells/lodgen_native_mutate.py <dir with Synthetic.lodo/.lodi>

Prints `N checks, M failures` then `RESULT PASS`/`FAIL`; exit 1 on any failure.
Nothing here imports the writers; it works from the byte tables of
docs/LODGEN_NATIVE_LODO_LODI.md, like the decoder beside it.
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
DECODE = os.path.join(HERE, 'lodgen_native_decode.py')


def crc32(b, seed=0):
    return zlib.crc32(b, seed) & 0xFFFFFFFF


def put(buf, off, fmt, *vals):
    struct.pack_into('<' + fmt, buf, off, *vals)


def get(buf, off, fmt):
    return struct.unpack_from('<' + fmt, buf, off)


# --------------------------------------------------------------- re-signing
def resign_header_lodo(b):
    put(b, 0x0C, 'I', crc32(bytes(b[0x10:0x100])))


def resign_header_lodi(b):
    """headerCrc32 covers the header BLOCK, and v7 made that block 512 bytes
    (docs s4.9). Signing 256 of a 512-byte header leaves the file refused for a
    CRC mismatch, which is not the refusal any of these cases is testing for --
    it is a control that goes red for the wrong reason, which is worse than one
    that does not go red at all."""
    hdr = 0x200 if get(b, 0x04, 'I')[0] >= 7 else 0x100
    put(b, 0x0C, 'I', crc32(bytes(b[0x10:hdr])))


def resign_lodo(b):
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
    put(b, 0x0C, 'I', crc32(bytes(b[0x10:0x100])))


def resign_lodi(b):
    """Every present chunk's crc32, then indexCrc32, then headerCrc32."""
    chunkCount, instanceCount, presentChunks = get(b, 0x54, 'III')
    offChunks, offCells, offInst, offCold = get(b, 0x68, 'QQQQ')
    for ci in range(chunkCount):
        base = offChunks + ci * 32
        first, count = get(b, base, 'II')
        if count == 0:
            continue
        rec = bytes(b[offInst + first * 24:offInst + (first + count) * 24])
        cold = bytes(b[offCold + first * 8:offCold + (first + count) * 8])
        put(b, base + 24, 'I', crc32(cold, crc32(rec)))
    offOcc, offOccRange = get(b, 0x98, 'QQ')
    occCount = get(b, 0xA8, 'I')[0]
    idx = bytes(b[offChunks:offChunks + chunkCount * 32]) + \
        bytes(b[offCells:offCells + presentChunks * 16 * 8]) + \
        bytes(b[offOcc:offOcc + occCount * 40]) + \
        bytes(b[offOccRange:offOccRange + presentChunks * 16 * 8])
    # v4 the aggregate table and covered blob join next, v5's AO blob joins LAST
    version = get(b, 0x04, 'I')[0]
    if version >= 4:
        offAgg, offCov = get(b, 0xB0, 'QQ')
        aggCount, covCount = get(b, 0xC0, 'II')
        if version == 4 or aggCount:
            idx += bytes(b[offAgg:offAgg + aggCount * 48])
            idx += bytes(b[offCov:offCov + covCount * 4])
    if version == 5:
        offPao = get(b, 0xE4, 'Q')[0]
        paoCount = get(b, 0xEC, 'I')[0]
        idx += bytes(b[offPao:offPao + paoCount])
    put(b, 0x64, 'I', crc32(idx))
    put(b, 0x0C, 'I', crc32(bytes(b[0x10:0x100])))


# --------------------------------------------------------------- the cases
def cases(lodo, lodi):
    """Each case: (name, which file, a function that edits the bytearray,
    whether to re-sign, the substring the refusal must contain)."""
    offInst = get(lodi, 0x78, 'Q')[0]
    offCold = get(lodi, 0x80, 'Q')[0]
    offChunks = get(lodi, 0x68, 'Q')[0]
    offBases = get(lodo, 0x70, 'Q')[0]
    offClusters, offMaterials = get(lodo, 0x80, 'QQ')
    baseCount = get(lodo, 0x50, 'I')[0]

    # the chunk with more than one instance is the one the order rule needs
    chunkCount = get(lodi, 0x54, 'I')[0]
    multi = None
    for ci in range(chunkCount):
        first, count = get(lodi, offChunks + ci * 32, 'II')
        if count > 1:
            multi = (first, count)
            break

    C = []

    def add(name, which, fn, resign, want):
        C.append((name, which, fn, resign, want))

    # --- plain flips: the CRCs must answer
    add('lodo headerCrc32 flip', 'lodo', lambda b: put(b, 0x0C, 'I', get(b, 0x0C, 'I')[0] ^ 1), False,
        'headerCrc32')
    # a PAYLOAD byte, with only the HEADER crc re-signed, so indexCrc32 answers
    # (flipping the stored indexCrc32 itself is caught by headerCrc32 first,
    # which covers 0x10..0x100 -- lane NATIVE1a, one gate-only correction)
    add('lodo a vertex byte, header re-signed', 'lodo',
        lambda b: b.__setitem__(get(b, 0x98, 'Q')[0], b[get(b, 0x98, 'Q')[0]] ^ 0xFF), 'header',
        'indexCrc32')
    add('lodi headerCrc32 flip', 'lodi', lambda b: put(b, 0x0C, 'I', get(b, 0x0C, 'I')[0] ^ 1), False,
        'headerCrc32')
    add('lodi a chunk crc32 flip', 'lodi',
        lambda b: put(b, offChunks + 24, 'I', get(b, offChunks + 24, 'I')[0] ^ 1), False, 'crc32')

    # --- v1 row rules, re-signed so the RULE answers
    add('lodo base table out of formId order', 'lodo',
        lambda b: put(b, offBases, 'I', 0xFFFFFFFF), True, 'sorted by (formId')
    # --- v6 (lane SWAP1): the material-swap variant rows, row 0 of the base table
    if get(lodo, 0x04, 'I')[0] >= 6:
        add('v6 lodo SWAPPED flag with materialSwap 0', 'lodo',
            lambda b: put(b, offBases + 18, 'H', get(b, offBases + 18, 'H')[0] | 8), True, 'disagree')
        add('v6 lodo materialSwap with the SWAPPED flag clear', 'lodo',
            lambda b: put(b, offBases + 28, 'I', 0x0001ABCD), True, 'disagree')
        add('v6 lodo variant row with no plain row before it', 'lodo',
            lambda b: (put(b, offBases + 28, 'I', 0x0001ABCD),
                       put(b, offBases + 18, 'H', get(b, offBases + 18, 'H')[0] | 8)), True, 'no plain row')
        add('v6 lodo read as v5: a SWAPPED flag has no materialSwap', 'lodo',
            lambda b: (put(b, 0x04, 'I', 5), put(b, offBases + 28, 'I', 0x0001ABCD),
                       put(b, offBases + 18, 'H', get(b, offBases + 18, 'H')[0] | 8)), True, 'disagree')
    add('lodo base boundRadius 0', 'lodo',
        lambda b: put(b, offBases + 20, 'f', 0.0), True, 'boundRadius')
    add('lodo cluster reserved flag bit', 'lodo',
        lambda b: put(b, offClusters + 14, 'H', get(b, offClusters + 14, 'H')[0] | 0x10), True,
        'reserved flags')
    add('lodo material family 2', 'lodo',
        lambda b: put(b, offMaterials + 4, 'B', 2), True, 'family')
    add('lodi ROW_ORDER_NORTH_UP clear', 'lodi',
        lambda b: put(b, 0x08, 'I', get(b, 0x08, 'I')[0] & ~1), True, 'NORTH_UP')
    add('lodi instance reserved flag bit', 'lodi',
        lambda b: put(b, offInst + 20, 'H', get(b, offInst + 20, 'H')[0] | 0x40), True,
        'reserved flags')
    add('lodi NOLIB with a non-zero identity', 'lodi',
        lambda b: put(b, 0x08, 'I', get(b, 0x08, 'I')[0] | 4), True, 'NOLIB')
    add('lodi lodoIdentity does not name the .lodo', 'lodi',
        lambda b: put(b, 0x20, 'Q', get(b, 0x20, 'Q')[0] ^ 1), True, 'lodoIdentity')

    # --- v2: one mutation per NEW field, each refused by its own name
    add('v2 lodo version back to 1', 'lodo', lambda b: put(b, 0x04, 'I', 1), True, 'version 1')
    add('v2 lodi version back to 1', 'lodi', lambda b: put(b, 0x04, 'I', 1), True, 'version 1')
    add('v3 lodo reserved byte at 0xCE', 'lodo', lambda b: put(b, 0xCE, 'B', 1), True, '0xCE')
    add('v3 lodi reserved byte at 0xB0', 'lodi', lambda b: put(b, 0xB0, 'B', 1), True, '0xB0')
    add('v3 lodo unknown header flag bit', 'lodo',
        lambda b: put(b, 0x08, 'I', get(b, 0x08, 'I')[0] | 0x10), True, 'reserved flag')
    add('v2 loadOrderHash in the .lodo only', 'lodo',
        lambda b: put(b, 0xB8, 'Q', get(b, 0xB8, 'Q')[0] ^ 1), True, 'loadOrderHash')
    add('v2 loadOrderHash in the .lodi only', 'lodi',
        lambda b: put(b, 0x90, 'Q', get(b, 0x90, 'Q')[0] ^ 1), True, 'loadOrderHash')
    add('v2 instance scale 0 (the bound radius rule)', 'lodi',
        lambda b: put(b, offInst + 12, 'H', 0), True, 'scale is 0')
    if multi:
        first, count = multi
        # the LAST record of the multi-instance chunk gets a drawKey BELOW its
        # predecessor's, which no other rule can see
        last = first + count - 1
        add('v2 drawKey out of order inside a cell', 'lodi',
            lambda b: put(b, offInst + last * 24 + 22, 'H', 0), True, 'drawKey')
        add('v2 drawKey is not the base rank', 'lodi',
            lambda b: put(b, offInst + first * 24 + 22, 'H', 0xFFFE), True, 'drawKey')
        add('v2 stock identity changed', 'lodi',
            lambda b: put(b, offCold + first * 8 + 6, 'H', 0xBEEF), True, 'identities')
    # ------------------------------------------------------------------ v3
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
        # 0x24 is meshId; the u16 reserved word sits at 0x26
        add('v3 lodi occluder reserved word', 'lodi',
            lambda b: put(b, offOcc + 0x26, 'H', 1), True, 'reserved')
        add('v3 lodi occluder flag bit0 clear', 'lodi',
            lambda b: put(b, offOcc + 0x1E, 'H', 0), True, 'flags')
        add('v3 lodi occluder half extent 0', 'lodi',
            lambda b: put(b, offOcc + 0x0C, 'f', 0.0), True, 'half extent')
        add('v3 lodi occluder names an instance outside its cell', 'lodi',
            lambda b: put(b, offOcc + 0x20, 'I', get(b, 0x58, 'I')[0] - 1), True, 'cell')
    return C


def main():
    if len(sys.argv) < 2:
        print('usage: lodgen_native_mutate.py <dir with Synthetic.lodo/.lodi>')
        return 2
    src = sys.argv[1]
    lodoPath = os.path.join(src, 'Synthetic.lodo')
    lodiPath = os.path.join(src, 'Synthetic.lodi')
    expect = os.path.join(src, 'Synthetic.expect.txt')
    for f in (lodoPath, lodiPath, expect):
        if not os.path.exists(f):
            print('missing %s' % f)
            return 2
    lodo0 = bytearray(open(lodoPath, 'rb').read())
    lodi0 = bytearray(open(lodiPath, 'rb').read())

    ok = 0
    fails = []
    tmp = tempfile.mkdtemp(prefix='lodnative_mut_')
    try:
        # the control: the unmutated pair must PASS, or nothing below means anything
        rc = subprocess.call([sys.executable, DECODE, lodoPath, lodiPath, '--expect', expect],
                             stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        if rc == 0:
            ok += 1
            print('  ok   control: the unmutated pair passes')
        else:
            fails.append('control')
            print('  FAIL control: the unmutated pair does NOT pass (rc=%d)' % rc)

        for name, which, fn, resign, want in cases(lodo0, lodi0):
            o = bytearray(lodo0)
            i = bytearray(lodi0)
            fn(o if which == 'lodo' else i)
            if resign == 'header':
                resign_header_lodo(o)
                resign_header_lodi(i)
            elif resign:
                resign_lodo(o)
                resign_lodi(i)
            po = os.path.join(tmp, 'Synthetic.lodo')
            pi = os.path.join(tmp, 'Synthetic.lodi')
            open(po, 'wb').write(bytes(o))
            open(pi, 'wb').write(bytes(i))
            p = subprocess.Popen([sys.executable, DECODE, po, pi, '--expect', expect],
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            out = p.communicate()[0].decode('utf-8', 'replace')
            refused = p.returncode != 0
            named = want.lower() in out.lower()
            if refused and named:
                ok += 1
                print('  ok   %s -- refused, naming "%s"' % (name, want))
            elif refused:
                fails.append(name)
                print('  FAIL %s -- refused, but nothing named "%s"' % (name, want))
            else:
                fails.append(name)
                print('  FAIL %s -- ACCEPTED' % name)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    print('%d checks, %d failures' % (ok + len(fails), len(fails)))
    print('RESULT %s' % ('PASS' if not fails else 'FAIL'))
    return 0 if not fails else 1


if __name__ == '__main__':
    sys.exit(main())
