import sys, struct
sys.path.insert(0, r'E:/Projects/Claude/.claude/skills/fo4-nif-vertex-channel-census/tools')
import ba2lib, nifwind
a = ba2lib.load(r'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4 - Meshes.ba2')
b = ba2lib.load(r'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4 - MeshesExtra.ba2')
BS = chr(92)
for p in sys.argv[1:]:
    n = ('meshes' + BS + p.replace('/', BS)).lower(); d = None
    for ar in (a, b):
        try: d = ba2lib.get(ar, n); break
        except KeyError: pass
    N = nifwind.Nif(d)
    for k, (t, o, s) in enumerate(N.blocks):
        if t == 'BSLightingShaderProperty': print(p, 'mat', N.shader_flags(k)[2], 'flags1 %08x flags2 %08x' % N.shader_flags(k)[:2])
        if t == 'BSShaderTextureSet':
            cnt = struct.unpack_from('<I', d, o)[0]; oo = o + 4; tx = []
            for _ in range(cnt):
                L = struct.unpack_from('<I', d, oo)[0]; tx.append(d[oo + 4:oo + 4 + L].decode('latin-1')); oo += 4 + L
            print('   tex', tx[:2])
