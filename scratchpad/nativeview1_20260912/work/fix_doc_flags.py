# Repair: the bullet was inserted through `python -c "..."` inside a
# DOUBLE-QUOTED bash string, so every backticked span was command-substituted
# away by the shell before python ever saw it.  Written with the Write tool this
# time, per ww-anchored-hookup.
P = 'E:/Projects/NifskopeWildWastelandEdition/docs/LODGEN_BTD_FORMAT.md'
s = open(P, 'rb').read().decode('utf-8')

bad = """  * **The shader flags say the normal map is MODEL-SPACE**, because the sheet's
    is an . Read as a tangent-space map it lights the land about 40 percent
    too dark (measured: mean luma 70.6 against the  of the same cells at
    121.1, mean absolute colour difference 50.457). The bake's own  of the
    same chunk is the authority for what those flags should be -- Shader Flags 1
    , which is  set with  clear (the
    two are documented as incompatible), and Shader Flags 2 , which is
     with . The sheet branch sets exactly those
    three bits and leaves every other bit of the block alone; the no-sheets arm
    never reaches this code, which is why the module-off identity holds. After
    the fix the same comparison reads 35.821 with the lumas correlating at
    +0.6008, against +0.2023 for a different chunk and -0.0242 for the same
    chunk mirrored in Y.
"""

good = """  * **The shader flags say the normal map is MODEL-SPACE**, because the sheet's
    is an `_msn`. Read as a tangent-space map it lights the land about 40 percent
    too dark (measured: mean luma 70.6 against the `.BTR` of the same cells at
    121.1, mean absolute colour difference 50.457). The bake's own `.BTR` of the
    same chunk is the authority for what those flags should be -- Shader Flags 1
    `0x80401000`, which is `Model_Space_Normals` set with `Specular` clear (the
    two are documented as incompatible), and Shader Flags 2 `3`, which is
    `ZBuffer_Write` with `LOD_Landscape`. The sheet branch sets exactly those
    three bits and leaves every other bit of the block alone; the no-sheets arm
    never reaches this code, which is why the module-off identity holds. After
    the fix the same comparison reads 35.821 with the lumas correlating at
    +0.6008, against +0.2023 for a different chunk and -0.0242 for the same
    chunk mirrored in Y.
"""

assert s.count(bad) == 1, 'damaged bullet count %d' % s.count(bad)
s = s.replace(bad, good)
d = s.encode('utf-8')
open(P, 'wb').write(d)
print('doc %d B  CR %d  LF %d' % (len(d), d.count(13), d.count(10)))
