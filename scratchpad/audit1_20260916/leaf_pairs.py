"""AUDIT1 thick-leaves row: the NEAR texture our default FO4CS library names
against the LOD texture vanilla's object LOD uses, coverage per mip at 128."""
import sys
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916')
from leaf_alpha import alpha_levels
from leaf_chain import box

T = 'E:/Tools/Fallout 4/DataUnpacked/Data/textures/'
pairs = [
    ('MapleAtlas01_d.DDS',                 'landscape/trees/', 'MaplePostWarSet01LODlv2_d.DDS', 'LOD/Trees/'),
    ('MapleAtlas02_d.DDS',                 'landscape/trees/', 'MaplePostWarSet02LODlv2_d.DDS', 'LOD/Trees/'),
    ('ElmAtlas_D.dds',                     'landscape/trees/', 'ElmSet02LODlv2_d.DDS',          'LOD/Trees/'),
    ('ElmPreWarAtlas_d.dds',               'landscape/trees/', 'ElmSet01LODlv2_d.DDS',          'LOD/Trees/'),
    ('BlastedForestBurntTreeAtlas_d.DDS',  'landscape/trees/', 'BlastedForestSet01LODlv2_d.DDS','LOD/Trees/'),
    ('BlastedForestDestroyedTreeAtlas_d.DDS','landscape/trees/','BlastedForestSet01LODlv2_d.DDS','LOD/Trees/'),
]

def chain(path, th=128, n=8):
    w, h, fcc, lv = alpha_levels(path)
    src = [float((lv[i][2] >= th).mean()) for i in range(min(n, len(lv)))]
    mine, ours = lv[0][2], []
    for i in range(min(n, len(lv))):
        if i:
            mine = box(mine)
        ours.append(float((mine >= th).mean()))
    return w, h, src, ours

print('alpha-test coverage at 128, per mip: NEAR (our default FO4CS rung 0) vs LOD (vanilla)')
for near, nd, lod, ld in pairs:
    try:
        nw, nh, nsrc, nours = chain(T + nd + near)
        lw, lh, lsrc, lours = chain(T + ld + lod)
    except Exception as e:
        print('  %s / %s: %s' % (near, lod, e)); continue
    print('')
    print('  %-38s %dx%d   vs   %-32s %dx%d' % (near, nw, nh, lod, lw, lh))
    print('    mip     near-src  near-ours   lod-src  lod-ours')
    for i in range(min(len(nsrc), 8)):
        l1 = ('%.4f' % lsrc[i]) if i < len(lsrc) else '  --  '
        l2 = ('%.4f' % lours[i]) if i < len(lours) else '  --  '
        print('    %-3d     %.4f    %.4f     %s    %s' % (i, nsrc[i], nours[i], l1, l2))
