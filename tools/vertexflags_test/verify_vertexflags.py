"""Verify that a Vertex Desc edit actually rewrote the vertex records.

The stock "Vertex Flags" spell (src/spells/flags.cpp, spEditVertexDesc) changes
a BSTriShape's Vertex Desc and Data Size, then calls updateArraySize() on
"Vertex Data" to rebuild the per-vertex rows in the new layout. When the edit
only changes the vertex SIZE and not the vertex COUNT, updateArraySize()
early-returns (nifmodel.cpp: `if (nNewSize == nOldSize) return true;`) and the
rows can be left in the OLD layout while the desc advertises the new one --
every byte after Vertex Data then shifts, corrupting the mesh.

This checks that at the byte level, without trusting the model that wrote it.
The header's Block Sizes table is an independent record of how many bytes each
block actually occupies, so for each shape:

    expected delta = Num Vertices * (new stride - old stride)
    actual   delta = saved block size - original block size

If the rows were rebuilt, those agree. If the rows were left alone, the actual
delta is 0 while the expected delta is not.

Usage:
    python verify_vertexflags.py <original.nif> <edited.nif>
"""
import struct, sys, os

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', 'rigging_prototype'))
import nifparse

SHAPE_TYPES = ('BSTriShape', 'BSSubIndexTriShape', 'BSMeshLODTriShape',
               'BSDynamicTriShape')


def u32(d, o): return struct.unpack_from('<I', d, o)[0]
def u16(d, o): return struct.unpack_from('<H', d, o)[0]


def avobject_end(d, o):
    o += 4                                   # Name
    ne = u32(d, o); o += 4 + 4 * ne          # Extra Data
    o += 4 + 4                               # Controller, Flags
    o += 12 + 36 + 4                         # Translation, Rotation, Scale
    o += 4                                   # Collision Object
    return o


def shape_fields(data, start):
    """(desc, stride, numVerts, numTris, dataSize) for a BSTriShape block."""
    o = avobject_end(data, start)
    o += 16                                  # Bounding Sphere
    o += 4                                   # Skin
    o += 8                                   # Shader Property, Alpha Property
    desc = struct.unpack_from('<Q', data, o)[0]; o += 8
    numTris = u32(data, o); o += 4
    numVerts = u16(data, o); o += 2
    dataSize = u32(data, o); o += 4
    return desc, (desc & 0xF) * 4, numVerts, numTris, dataSize


def flag_names(desc):
    names = ['Vertex', 'UVs', 'UVs2', 'Normals', 'Tangents', 'Colors',
             'Skinned', 'LandData', 'EyeData']
    vf = (desc >> 44) & 0xFFFF
    on = [n for i, n in enumerate(names) if vf & (1 << i)]
    if vf & (1 << 10):
        on.append('FullPrecision')
    return ','.join(on) or '-'


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    orig_path, new_path = sys.argv[1], sys.argv[2]

    odata, _, _, oblocks = nifparse.parse(orig_path)
    ndata, _, _, nblocks = nifparse.parse(new_path)

    obyidx = {i: (t, s, sz) for i, t, s, sz in oblocks}
    fails, checked = [], 0

    for i, t, start, size in nblocks:
        if t not in SHAPE_TYPES or i not in obyidx:
            continue
        ot, ostart, osize = obyidx[i]
        if ot != t:
            continue

        odesc, ostride, onv, ont, ods = shape_fields(odata, ostart)
        ndesc, nstride, nnv, nnt, nds = shape_fields(ndata, start)
        checked += 1

        note = []
        if onv != nnv or ont != nnt:
            note.append(f"counts changed {onv}/{ont} -> {nnv}/{nnt} "
                        f"(this check assumes a size-only edit)")

        expected_delta = nnv * (nstride - ostride)
        actual_delta = size - osize
        if expected_delta != actual_delta:
            note.append(f"block grew {actual_delta}B, desc implies "
                        f"{expected_delta}B ({nnv} verts x {nstride - ostride}B)")

        want_ds = nnv * nstride + nnt * 6
        if nds != want_ds:
            note.append(f"Data Size {nds} != {nnv}*{nstride}+{nnt}*6 = {want_ds}")

        status = "  <-- " + "; ".join(note) if note else "  OK"
        print(f"[{i}] {t} verts={nnv} tris={nnt} "
              f"stride {ostride}->{nstride} "
              f"blocksize {osize}->{size} "
              f"flags[{flag_names(odesc)} -> {flag_names(ndesc)}]{status}")
        fails += [f"block {i}: {n}" for n in note]

    if not checked:
        print("FAIL: no comparable shape blocks found")
        sys.exit(1)
    if fails:
        print("FAIL:")
        for f in fails:
            print("  -", f)
        sys.exit(1)
    print(f"PASS ({checked} shape(s))")


if __name__ == '__main__':
    main()
