"""Make an unskinned twin of a skinned FO4 NIF's first BSTriShape-family shape.

Surgery on a byte level:
  - Skin link -> -1
  - VertexDesc: drop VA_SKINNING, recompute offsets/size (ResetAttributeOffsets
    semantics copied from niftypes.h)
  - Vertex records: remove the skin bytes (weights+indices) from each record
  - Data Size and the header block-size table patched to match
  - BSSkin::Instance / BSSkin::BoneData left in place as orphans (no link
    renumbering needed; the Create Skin spell only checks Skin link < 0)
"""
import struct, sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', 'rigging_prototype'))
import io, contextlib
import nifparse

VA_POSITION, VA_TEXCOORD0, VA_TEXCOORD1, VA_NORMAL, VA_BINORMAL, VA_COLOR, \
    VA_SKINNING, VA_LANDDATA, VA_EYEDATA = range(9)
VA_COUNT = 9
VF_FULLPREC = 0x400

def get_flags(desc):
    return (desc >> 44) & 0xFFFFF

def get_attr_offset(desc, attr):
    return (desc >> (4 * attr + 2)) & 0x3C

def reset_attribute_offsets(desc, stream):
    """Faithful port of BSVertexDesc::ResetAttributeOffsets (niftypes.h)."""
    vf = get_flags(desc)
    desc &= 0xFFFFFF0000000000  # ClearAttributeOffsets keeps flags only
    sizes = [0] * VA_COUNT
    if vf & (1 << VA_POSITION):
        sizes[VA_POSITION] = 4 if (vf & VF_FULLPREC or stream == 100) else 2
    if vf & (1 << VA_TEXCOORD0):
        sizes[VA_TEXCOORD0] = 1
    if vf & (1 << VA_TEXCOORD1):
        sizes[VA_TEXCOORD1] = 1
    if vf & (1 << VA_NORMAL):
        sizes[VA_NORMAL] = 1
        if vf & (1 << VA_BINORMAL):
            sizes[VA_BINORMAL] = 1
    if vf & (1 << VA_COLOR):
        sizes[VA_COLOR] = 1
    if vf & (1 << VA_SKINNING):
        sizes[VA_SKINNING] = 3
    if vf & (1 << VA_EYEDATA):
        sizes[VA_EYEDATA] = 1
    vertex_size = 0
    for va in range(VA_COUNT):
        if sizes[va]:
            if va != VA_POSITION:  # SetAttributeOffset is a no-op for position
                desc = (vertex_size << (4 * va + 2)) | (desc & ~(15 << (4 * va + 4)))
            vertex_size += sizes[va] * 4
    desc = (desc & 0xFFFFFFFFFFFFFFF0) | (vertex_size >> 2)  # SetSize
    return desc

def strip(src, dst):
    with open(src, 'rb') as f:
        data = bytearray(f.read())
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        _, hdr, strings, blocks = nifparse.parse(src)

    # locate the header's block-size table: num_blocks u32s right after the
    # type-index array; recompute its offset by re-walking the header
    nl = bytes(data).index(b'\n')
    off = nl + 1
    off += 4 + 1 + 4          # version, endian, user_version
    num_blocks = struct.unpack_from('<I', data, off)[0]; off += 4
    off += 4                  # bs_version
    bs_version = struct.unpack_from('<I', data, off - 4)[0]
    for _ in range(4 if bs_version >= 130 else 3):  # author/process/export[/maxpath]
        off += 1 + data[off]
    num_types = struct.unpack_from('<H', data, off)[0]; off += 2
    for _ in range(num_types):
        n = struct.unpack_from('<I', data, off)[0]; off += 4 + n
    off += 2 * num_blocks     # block type indices
    size_table = off          # block sizes u32[num_blocks]

    shape = next(b for b in blocks
                 if b[1] in ('BSTriShape', 'BSSubIndexTriShape', 'BSMeshLODTriShape'))
    bidx, btype, bstart, bsize = shape

    # walk the shape header exactly like the scanner
    o = bstart
    o += 4                               # name
    ne = struct.unpack_from('<I', data, o)[0]; o += 4 + 4 * ne
    o += 8                               # controller, flags
    o += 12 + 36 + 4                     # translation, rotation, scale
    o += 4                               # collision
    o += 16                              # bounding sphere
    skin_off = o; o += 4                 # skin link
    o += 8                               # shader, alpha
    desc_off = o
    desc = struct.unpack_from('<Q', data, o)[0]; o += 8
    ntri = struct.unpack_from('<I', data, o)[0]; o += 4
    nv = struct.unpack_from('<H', data, o)[0]; o += 2
    dsize_off = o
    dsize = struct.unpack_from('<I', data, o)[0]; o += 4
    vstart = o

    stride = (desc & 0xF) * 4
    vf = get_flags(desc)
    assert vf & (1 << VA_SKINNING), 'shape is not skinned'
    sk_off = get_attr_offset(desc, VA_SKINNING)
    assert dsize == nv * stride + ntri * 6, (dsize, nv, stride, ntri)

    new_desc = desc & ~(1 << (VA_SKINNING + 44))
    new_desc = reset_attribute_offsets(new_desc, bs_version)
    new_stride = (new_desc & 0xF) * 4
    assert new_stride == stride - 12, (stride, new_stride)
    # non-skin attribute offsets must be unchanged (skin bytes sit at sk_off..+12
    # and everything else in these meshes precedes them)
    for va in range(VA_COUNT):
        if va == VA_SKINNING:
            continue
        a, b = get_attr_offset(desc, va), get_attr_offset(new_desc, va)
        assert a == b, (va, a, b)
    assert sk_off + 12 == stride, 'skin bytes are not the record tail'

    new_verts = bytearray()
    for v in range(nv):
        rec = data[vstart + v * stride: vstart + (v + 1) * stride]
        new_verts += rec[:sk_off] + rec[sk_off + 12:]
    new_dsize = nv * new_stride + ntri * 6

    # rebuild the file
    out = bytearray(data[:bstart])
    piece = bytearray(data[bstart:vstart])           # shape header (to be patched)
    piece[skin_off - bstart:skin_off - bstart + 4] = struct.pack('<i', -1)
    piece[desc_off - bstart:desc_off - bstart + 8] = struct.pack('<Q', new_desc)
    piece[dsize_off - bstart:dsize_off - bstart + 4] = struct.pack('<I', new_dsize)
    out += piece
    out += new_verts
    out += data[vstart + nv * stride: bstart + bsize]  # triangles + segment tail
    out += data[bstart + bsize:]                       # all later blocks
    # patch the header block-size entry
    out[size_table + 4 * bidx: size_table + 4 * bidx + 4] = \
        struct.pack('<I', bsize - nv * 12)

    with open(dst, 'wb') as f:
        f.write(out)
    print(f"stripped {btype}[{bidx}] nv={nv} stride {stride}->{new_stride} "
          f"desc {desc:016x}->{new_desc:016x} -> {dst}")

if __name__ == '__main__':
    strip(sys.argv[1], sys.argv[2])
