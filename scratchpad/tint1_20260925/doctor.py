"""TINT1 refuter for the viewer: copy the installed v5 pair into red/, set EVERY colour row to pure red
(R 255, G 0, B 0, A 255), recompute indexCrc32 + headerCrc32 of the .lodo, then the .lodi's lodoIdentity and
its headerCrc32. If a render of red/ is byte-identical to the installed pair, the viewer never draws the stream.
usage: doctor.py"""
import struct, zlib, sys
sys.path.insert(0, r'E:/Projects/NifskopeWWE-tint1/tests/spells')
import lodgen_native_decode as dec
D = r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/Commonwealth.'
O = r'E:/Projects/NifskopeWWE-tint1/scratchpad/tint1_20260925/red/Commonwealth.'
b = bytearray(open(D + 'lodo', 'rb').read())
ver, n, off = struct.unpack_from('<I', b, 4)[0], struct.unpack_from('<I', b, 0xD4)[0], struct.unpack_from('<Q', b, 0xD8)[0]
assert ver == 5 and n > 0
for i in range(n):
    struct.pack_into('<I', b, off + 4 * i, 0xFF0000FF)
L0 = dec.read_lodo(D + 'lodo'); h = L0['header'] if 'header' in L0 else L0['h']
tabs = [(h['offBases'], h['baseCount'] * 32), (h['offMeshes'], h['meshCount'] * 56),
        (h['offClusters'], h['clusterCount'] * 16), (h['offClusterLods'], h['clusterCount'] * 48),
        (h['offMaterials'], h['materialCount'] * 16), (h['offLocalIndices'], h['clusterCount'] * 48),
        (h['offVertices'], h['vertexCount'] * 16), (h['offStrings'], h['stringBytes']), (off, n * 4)]
crc = 0
for oo, k in tabs:
    crc = zlib.crc32(bytes(b[oo:oo + k]), crc)
struct.pack_into('<I', b, 0xA8, crc)
struct.pack_into('<I', b, 0x0C, zlib.crc32(bytes(b[0x10:0x100])))
open(O + 'lodo', 'wb').write(b)
L = dec.read_lodo(O + 'lodo'); h2 = L['header'] if 'header' in L else L['h']
ident = dec.lodo_identity(h2)
i = bytearray(open(D + 'lodi', 'rb').read())
iv = struct.unpack_from('<I', i, 4)[0]; hdr = 512 if iv >= 7 else 256
struct.pack_into('<Q', i, 0x20, ident)
struct.pack_into('<I', i, 0x0C, zlib.crc32(bytes(i[0x10:hdr])))
open(O + 'lodi', 'wb').write(i)
dec.read_lodi(O + 'lodi')
print('red pair written: %d colour rows set red, lodi v%d identity %016x' % (n, iv, ident))
