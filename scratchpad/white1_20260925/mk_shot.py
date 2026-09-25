s = open(r'E:/Projects/NifskopeWWE-extent1/scratchpad/extent1_20260925/shot.sh', 'rb').read().decode()
BS = chr(92)
rep = [('ME=/e/Projects/NifskopeWWE-extent1\n', 'ME=/e/Projects/NifskopeWWE-white1\n'),
       ('"$ME/scratchpad/extent1_20260925/frame_check.py"', '"/e/Projects/NifskopeWWE-extent1/scratchpad/extent1_20260925/frame_check.py"'),
       ('sheetcache_extent1_', 'sheetcache_white1_'),
       ('env "${OBJ[@]}" ' + BS, 'env "${OBJ[@]}" ${CHAN:+WW_LOD_CHANNEL=$CHAN} ' + BS),
       ('# EXTENT1 whole-map picture', '# WHITE1 copy of EXTENT1 shot.sh; env CHAN=n adds WW_LOD_CHANNEL=n (12 = base colour x vertex colour, unlit).\n# EXTENT1 whole-map picture')]
for a, b in rep:
    assert s.count(a) == 1, a
    s = s.replace(a, b)
open(r'E:/Projects/NifskopeWWE-white1/scratchpad/white1_20260925/shot.sh', 'wb').write(s.encode())
