"""Verify a rigging-aware Join (Ctrl+J) output at the byte level.

Reparses the <input>_joined.nif the WW_JOIN_TEST harness saved and checks that
every BSSubIndexTriShape is internally consistent after the merge:

  1. skin: Num Bones (BSSkin::Instance) == len(Bones) == Num Bones (BoneData)
  2. every per-vertex Bone Index is < Num Bones (no stale index into a source's
     old, shorter bone list — the corruption the merge must avoid)
  3. segments cover exactly [0, Num Triangles): Start Index in triangle-index
     units, non-empty segments sorted and non-overlapping, Σ Num Primitives ==
     Num Triangles, Num Segments == Total Segments (no orphaned Segment Data)
  4. Data Size == Num Vertices*stride + Num Triangles*6

Pass an expected merged vertex/triangle count (argv[2],[3]) to also assert the
biggest shape absorbed everything.
"""
import struct, sys
sys.path.insert(0, r'E:\Projects\ClaudeNifskope\tools\rigging_prototype')
import nifparse

VA_SKINNING = 6

def u32(d, o): return struct.unpack_from('<I', d, o)[0]
def u16(d, o): return struct.unpack_from('<H', d, o)[0]

def avobject_end(d, o):
    o += 4                                   # Name
    ne = u32(d, o); o += 4 + 4 * ne          # Extra Data
    o += 4 + 4                                # Controller, Flags
    o += 12 + 36 + 4                          # Translation, Rotation, Scale
    o += 4                                    # Collision Object
    return o

def skin_num_bones(data, blocks, skin_block):
    """(Num Bones on Instance, len Bones array, Num Bones on BoneData)."""
    if skin_block < 0 or skin_block >= len(blocks):
        return None
    i, t, s, z = blocks[skin_block]
    if t != 'BSSkin::Instance':
        return None
    o = s + 4                                # Skeleton Root
    dataRef = struct.unpack_from('<i', data, o)[0]; o += 4
    nbInst = u32(data, o); o += 4
    o += 4 * nbInst                          # Bones ptrs
    nbData = -1
    if 0 <= dataRef < len(blocks) and blocks[dataRef][1] == 'BSSkin::BoneData':
        nbData = u32(data, blocks[dataRef][2])
    return nbInst, nbInst, nbData

def check_shape(data, blocks, i, t, start):
    fails = []
    o = avobject_end(data, start)
    o += 16                                  # Bounding Sphere
    skin = struct.unpack_from('<i', data, o)[0]; o += 4
    o += 8                                    # Shader, Alpha
    desc = struct.unpack_from('<Q', data, o)[0]; o += 8
    numTris = u32(data, o); o += 4
    numVerts = u16(data, o); o += 2
    dataSize = u32(data, o); o += 4
    stride = (desc & 0xF) * 4
    vbase = o
    o += numVerts * stride                   # Vertex Data
    o += numTris * 6                         # Triangles
    numPrim = u32(data, o); o += 4
    numSeg = u32(data, o); o += 4
    totSeg = u32(data, o); o += 4

    # 4. Data Size
    if dataSize != numVerts * stride + numTris * 6:
        fails.append(f"Data Size {dataSize} != {numVerts}*{stride}+{numTris}*6")

    # 1. skin counts
    nb = skin_num_bones(data, blocks, skin)
    numBones = None
    if nb is not None:
        inst, arr, bdata = nb
        numBones = inst
        if not (inst == arr == bdata):
            fails.append(f"skin counts differ: Instance {inst} Bones {arr} BoneData {bdata}")

    # 2. bone-index range
    if numBones is not None and stride:
        sk_off = (desc >> (4 * VA_SKINNING + 2)) & 0x3C
        bad = 0
        for v in range(numVerts):
            rec = vbase + v * stride
            for k in range(4):
                if data[rec + sk_off + 8 + k] >= numBones:
                    bad += 1
        if bad:
            fails.append(f"{bad} bone indices >= Num Bones ({numBones})")

    # 3. segment coverage (top-level segments partition all triangles; sub-
    #    segments live within them and are not counted toward coverage)
    segs = []
    p = o
    nsub_total = 0
    for s in range(numSeg):
        si = u32(data, p); npr = u32(data, p + 4); nsub = u32(data, p + 12); p += 16
        p += 16 * nsub
        nsub_total += nsub
        segs.append((si, npr, nsub))
    covered = sorted((si, npr) for si, npr, _ in segs if npr > 0)
    total_prim = sum(npr for _, npr in covered)
    if total_prim != numTris:
        fails.append(f"segments cover {total_prim} prims != {numTris} triangles")
    expect = 0
    for si, npr in covered:
        if si != expect:
            fails.append(f"segment gap/overlap: Start Index {si} != {expect}")
            break
        expect += npr * 3

    # shared Segment Data (present iff Num < Total) must stay self-consistent
    if numSeg < totSeg:
        sdNum = u32(data, p); sdTot = u32(data, p + 4); p += 8
        p += 4 * sdNum                                   # Segment Starts
        for _ in range(sdTot):                           # Per Segment Data
            nCut = u32(data, p + 8); p += 12 + 4 * nCut
        if sdNum != numSeg or sdTot != totSeg:
            fails.append(f"Segment Data Num/Total {sdNum}/{sdTot} != shape {numSeg}/{totSeg}")
        if totSeg != numSeg + nsub_total:
            fails.append(f"Total {totSeg} != Num {numSeg} + subsegments {nsub_total}")
    elif nsub_total:
        fails.append(f"{nsub_total} subsegments but Num == Total (no Segment Data)")

    print(f"[{i}] {t} verts={numVerts} tris={numTris} bones={numBones} "
          f"segments={numSeg} desc=0x{desc:x}" + ("  <-- " + "; ".join(fails) if fails else "  OK"))
    return fails, numVerts, numTris

def main():
    path = sys.argv[1]
    expV = int(sys.argv[2]) if len(sys.argv) > 2 else None
    expT = int(sys.argv[3]) if len(sys.argv) > 3 else None
    data, hdr, strings, blocks = nifparse.parse(path)
    allf = []
    biggest = (0, 0, 0)     # verts, tris
    for i, t, start, size in blocks:
        if t != 'BSSubIndexTriShape':
            continue
        f, nv, nt = check_shape(data, blocks, i, t, start)
        allf += f
        if nv > biggest[0]:
            biggest = (nv, nt, i)
    if expV is not None and biggest[0] != expV:
        allf.append(f"biggest shape verts {biggest[0]} != expected {expV}")
    if expT is not None and biggest[1] != expT:
        allf.append(f"biggest shape tris {biggest[1]} != expected {expT}")
    if allf:
        print("FAIL:")
        for f in allf:
            print("  -", f)
        sys.exit(1)
    print("PASS")

if __name__ == '__main__':
    main()
