import io

p = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/hkx2_20260910/shots.sh'
s = io.open(p, encoding='utf-8', newline='').read()

pairs = [
    # 1. the default NIF is the RIGGED HUMAN, not the bone-only skeleton: a mesh
    #    is what shows that skinning followed the bones.
    ('SRC="${SRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/character/characterassets/skeleton.nif}"\n'
     'CLIP="${CLIP:-$ROOT/scratchpad/hkx1_20260910/clips/jog.hkx}"\n'
     'OUT="$ROOT/scratchpad/hkx2_20260910/images"\n',

     '# The default rig is the RIGGED HUMAN fixture, not the bone-only\n'
     '# skeleton.nif: a mesh is what shows that skinning followed the bones\n'
     '# (bones that move under a mesh that does not is the refuter of the\n'
     '# "Node::transform() is early enough" claim). Run this script ONCE PER\n'
     '# CLIP with PREFIX set, so the two sets do not overwrite each other:\n'
     '#\n'
     '#   PREFIX=jog    CLIP=scratchpad/hkx1_20260910/clips/jog.hkx \\\n'
     '#                 FD=0.0333333 NFRAMES=23 bash scratchpad/hkx2_20260910/shots.sh\n'
     '#   PREFIX=mixamo CLIP=fixtures/Running_To_Slide_And_Back_To_Running.hkx \\\n'
     '#                 FD=0.0166667 NFRAMES=93 bash scratchpad/hkx2_20260910/shots.sh\n'
     'SRC="${SRC:-$ROOT/fixtures/human_male_vanilla.nif}"\n'
     'CLIP="${CLIP:-$ROOT/scratchpad/hkx1_20260910/clips/jog.hkx}"\n'
     'PREFIX="${PREFIX:-hkx}"\n'
     'OUT="${OUT:-$ROOT/scratchpad/hkx2_20260910/images}"\n'),

    # 2. per-clip file names
    ('RC=0\n'
     'shot "$OUT/hkx_bindpose.png"  "$T0"    ""      || RC=1\n'
     'shot "$OUT/hkx_frame0.png"    "$T0"    "$CLIP" || RC=1\n'
     'shot "$OUT/hkx_frame_half.png" "$THALF" "$CLIP" || RC=1\n'
     'shot "$OUT/hkx_frame_last.png" "$TEND"  "$CLIP" || RC=1\n',

     'RC=0\n'
     'shot "$OUT/${PREFIX}_bindpose.png"   "$T0"    ""      || RC=1\n'
     'shot "$OUT/${PREFIX}_frame0.png"     "$T0"    "$CLIP" || RC=1\n'
     'shot "$OUT/${PREFIX}_frame_half.png" "$THALF" "$CLIP" || RC=1\n'
     'shot "$OUT/${PREFIX}_frame_last.png" "$TEND"  "$CLIP" || RC=1\n'),

    ('md5sum "$OUT"/hkx_bindpose.png "$OUT"/hkx_frame0.png "$OUT"/hkx_frame_half.png "$OUT"/hkx_frame_last.png 2>/dev/null\n'
     'U=$(md5sum "$OUT"/hkx_*.png 2>/dev/null | awk \'{print $1}\' | sort -u | wc -l)\n',

     'md5sum "$OUT/${PREFIX}_bindpose.png" "$OUT/${PREFIX}_frame0.png" \\\n'
     '       "$OUT/${PREFIX}_frame_half.png" "$OUT/${PREFIX}_frame_last.png" 2>/dev/null\n'
     'U=$(md5sum "$OUT/${PREFIX}_bindpose.png" "$OUT/${PREFIX}_frame0.png" \\\n'
     '           "$OUT/${PREFIX}_frame_half.png" "$OUT/${PREFIX}_frame_last.png" 2>/dev/null \\\n'
     '     | awk \'{print $1}\' | sort -u | wc -l)\n'),
]

for old, new, in pairs:
    if new in s:
        print('already patched:', new.splitlines()[0][:50])
        continue
    assert s.count(old) == 1, (s.count(old), old.splitlines()[0])
    s = s.replace(old, new, 1)
    print('patched:', old.splitlines()[0][:50])

io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('CR =', open(p, 'rb').read().count(b'\r'))
