import sys, numpy as np
sys.path.insert(0, '../../tests/spells')
import lodgen_native_decode as ND
T = ND.read_lodi(sys.argv[1] + '.lodi'); H = T['header']; w = H['chunkEast'] - H['chunkWest'] + 1
steps = []
for ci, ch in enumerate(T['chunks']):
    if ch['instanceCount']:
        steps.append((ch['zExtent'] / 65535.0, (H['chunkWest'] + ci % w, H['chunkSouth'] + ci // w), ch['zMin'], ch['zExtent']))
steps.sort()
print('chunks with instances', len(steps), 'z step min %.4f median %.4f max %.4f' % (steps[0][0], steps[len(steps)//2][0], steps[-1][0]))
for bi in map(int, sys.argv[2:]):
    o = T['occluders'][bi]; ii = o['instanceIndex']
    for ci, ch in enumerate(T['chunks']):
        if ch['instanceFirst'] <= ii < ch['instanceFirst'] + ch['instanceCount']:
            print('box', bi, 'chunk', (H['chunkWest'] + ci % w, H['chunkSouth'] + ci // w), 'zMin %.1f zExtent %.1f step %.4f u' % (ch['zMin'], ch['zExtent'], ch['zExtent'] / 65535.0))
