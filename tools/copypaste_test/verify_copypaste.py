"""Verify Block List multi-block copy/paste output at the byte level.

Re-parses the <input>_pasted.nif that the WW_COPYPASTE_TEST harness saved and
checks the paste round-tripped through save/load without corrupting the scene
graph. The harness already proves the invariants on the live model; this guards
the serialised bytes:

  1. block count == expected (origCount + copied-branch-union size), argv[2]
  2. every NiNode-family child link resolves to a real block: none is -1
     (a dangling attach) and none is out of range (a stale, un-remapped index)
  3. prints each node's Children so the re-attached pasted roots are visible

Layout note: children sit immediately after the NiAVObject prefix for every
NiNode subclass (derived fields follow the base's), so one walk covers them all.
The NiAVObject prefix mirrors tools/createskin_test/verify_createskin.py, which
is validated on these FO4 (bsver 130) files.
"""
import struct, sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', 'rigging_prototype'))
import nifparse

NODE_TYPES = ('NiNode', 'BSFadeNode', 'BSLeafAnimNode', 'BSMultiBoundNode',
              'BSOrderedNode', 'BSValueNode', 'NiSwitchNode', 'NiBillboardNode')

def avobject_end(data, start):
    """Offset just past the NiAVObject prefix (FO4 bsver 130)."""
    o = start
    o += 4                                            # Name (string index)
    ne = struct.unpack_from('<I', data, o)[0]
    o += 4 + 4 * ne                                   # Num Extra Data + list
    o += 4                                            # Controller
    o += 4                                            # Flags
    o += 12 + 36 + 4                                  # Translation, Rotation, Scale
    o += 4                                            # Collision Object
    return o

def children_of(data, start):
    o = avobject_end(data, start)
    nc = struct.unpack_from('<I', data, o)[0]; o += 4
    return [struct.unpack_from('<i', data, o + 4 * k)[0] for k in range(nc)]

def name_of(data, strings, start):
    idx = struct.unpack_from('<I', data, start)[0]
    return strings[idx] if 0 <= idx < len(strings) else f'#{idx}'

def main():
    path = sys.argv[1]
    expected = int(sys.argv[2]) if len(sys.argv) > 2 else None
    data, hdr, strings, blocks = nifparse.parse(path)
    n = len(blocks)
    fails = []
    if expected is not None and n != expected:
        fails.append(f'block count {n} != expected {expected}')

    dangling = outrange = 0
    for i, tname, start, size in blocks:
        if tname not in NODE_TYPES:
            continue
        kids = children_of(data, start)
        bad = [c for c in kids if c == -1 or c < 0 or c >= n]
        dangling += sum(1 for c in kids if c == -1)
        outrange += sum(1 for c in kids if c != -1 and (c < 0 or c >= n))
        tag = f"  [{i}] {tname} '{name_of(data, strings, start)}' children {kids}"
        print(tag + ('   <-- BAD' if bad else ''))

    if dangling:
        fails.append(f'{dangling} dangling (-1) child links')
    if outrange:
        fails.append(f'{outrange} out-of-range child links')

    print(f"blocks={n}" + (f" expected={expected}" if expected is not None else ""))
    print(f"node child links: dangling={dangling} out-of-range={outrange}")
    if fails:
        print('FAIL:')
        for f in fails:
            print(f'  - {f}')
        sys.exit(1)
    print('PASS')

if __name__ == '__main__':
    main()
