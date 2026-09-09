import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dds
T = r'E:\Tools\Fallout 4\DataUnpacked\Data\Textures\Terrain\Commonwealth'

for a0, a1 in ((255, 255), (255, 0), (0, 255), (200, 100), (100, 200)):
    if a0 > a1:
        t = [a0, a1] + [((6 - i) * a0 + (i + 1) * a1) // 7 for i in range(6)]
    else:
        t = [a0, a1] + [((4 - i) * a0 + (i + 1) * a1) // 5 for i in range(4)] + [0, 255]
    assert all(0 <= v <= 255 for v in t), (a0, a1, t)
    print('BC4 palette a0=%3d a1=%3d -> %s' % (a0, a1, t))
print()
for n in ['Commonwealth.4.-20.24', 'Commonwealth.4.-20.60', 'Commonwealth.4.-60.60',
          'Commonwealth.4.0.0', 'Commonwealth.16.-16.0', 'Commonwealth.32.-96.-96']:
    d = dds.DDS(os.path.join(T, n + '.DDS')); w, h, px = d.decode(2)
    a = [px[i * 4 + 3] for i in range(w * h)]
    dn = dds.DDS(os.path.join(T, n + '_msn.DDS')); w2, h2, p2 = dn.decode(2)
    a2 = [p2[i * 4 + 3] for i in range(w2 * h2)]
    print('%-22s diffuse alpha min %3d max %3d | _msn alpha min %3d max %3d'
          % (n.replace('Commonwealth.', ''), min(a), max(a), min(a2), max(a2)))
