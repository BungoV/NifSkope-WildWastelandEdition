"""A2: the grey (flat) dim-4 chunks of a VT.2 pyramid, listed by SW cell -- the FG1 definition (colour SD < 2 over
the chunk's mip-1 content, fill_gate.py), plus vanilla's cover and the .lodl geometry, per chunk.
usage: a2_lists.py <VT.2.lodt> <out.txt>"""
import sys, os, pickle
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import vtread
src = open(HERE + '/fill_gate.py').read()
ns = {'np': __import__('numpy'), 'vtread': vtread}
exec(src[src.index('def chunk_sd'):src.index('sa, sb = chunk_sd')], ns)
sd = ns['chunk_sd'](vtread.Vt(sys.argv[1]))
V = 'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth/Commonwealth.4.%d.%d.DDS'
flat = sorted(k for k, s in sd.items() if s < 2)
noVan = sorted(k for k in sd if not os.path.exists(V % k))
with open(sys.argv[2], 'w') as f:
    f.write('# %s\n# chunks read %d | flat (SD<2) %d | chunks with no vanilla dim-4 sheet %d\n' % (
        sys.argv[1], len(sd), len(flat), len(noVan)))
    f.write('# flat chunks, SW cell x y, SD\n')
    for k in flat:
        f.write('%d %d %.2f\n' % (k[0], k[1], sd[k]))
    f.write('# chunks vanilla does not cover\n')
    for k in noVan:
        f.write('%d %d\n' % k)
print('chunks %d flat %d noVanilla %d -> %s' % (len(sd), len(flat), len(noVan), sys.argv[2]))
