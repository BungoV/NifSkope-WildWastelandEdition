import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dds, lodmean
T = r'E:\Tools\Fallout 4\DataUnpacked\Data\Textures\Terrain\Commonwealth'
names = ['Commonwealth.4.-20.24', 'Commonwealth.4.-20.40', 'Commonwealth.4.-20.60',
         'Commonwealth.4.-20.80', 'Commonwealth.4.0.0', 'Commonwealth.4.-60.60']
for n in [names[0] + '', names[0] + '_msn', 'Commonwealth.32.-96.-96', 'Commonwealth.16.-16.0']:
    d = dds.DDS(os.path.join(T, n + '.DDS'))
    print('%-32s %dx%d %-12s mips=%d fourcc=%r off=%d size=%d expect=%d' % (
        n, d.width, d.height, d.fmt, d.mips, d.fourcc, d.dataOff, len(d.raw), d.dataOff + d.sliceBytes()))
print()
print('%-24s %-38s %-38s' % ('cell', 'FULL mip0 decode', '4x4-mip endpoint shortcut'))
for n in names:
    p = os.path.join(T, n + '.DDS')
    s = dds.stats(p, 0)
    w, h, m, lum, sat = lodmean.mean565(p)
    print('%-24s rgb %5.1f %5.1f %5.1f lum %5.1f sat %.3f | rgb %5.1f %5.1f %5.1f lum %5.1f sat %.3f | meanSat %.3f lumStd %5.1f' % (
        n.replace('Commonwealth.', ''), s['mean'][0], s['mean'][1], s['mean'][2], s['lum'], s['satOfMean'],
        m[0], m[1], m[2], lum, sat, s['meanSat'], s['lumStd']))
