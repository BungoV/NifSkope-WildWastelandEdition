import sys, glob, numpy as np
ROOT='E:/Projects/NifskopeWildWastelandEdition'
sys.path.insert(0, ROOT+'/tests/spells')
import lodgen_native_decode as ND
from lodgen_horizon_witness import HorizonSheet
BAKE = ROOT + '/scratchpad/horizon2_20260918/dumpbake/nat/FO4CSLOD/Commonwealth/Commonwealth'
T=ND.read_lodi(BAKE+'.lodi'); h=T['header']
print('chunk extent W%d S%d E%d N%d  present=%d'%(h['chunkWest'],h['chunkSouth'],h['chunkEast'],h['chunkNorth'],h['presentChunks']))
for c in T['chunks']: print('  chunk cx=%d cy=%d inst=%d zMin=%.0f zExt=%.0f'%(c['cx'],c['cy'],c['instanceCount'],c['zMin'],c['zExtent']))
for p in sorted(glob.glob(ROOT+'/scratchpad/horizon2_20260918/dumpbake/vt/FO4CSLOD/Commonwealth/*.lodt')):
    try:
        s=HorizonSheet(p)
        v=s.v
        print('%s role7 sheets=%s azimuths=%d upt=%.1f levelDim=%d content=%d stored=%d border=%d tiles=%dx%d west=%d east=%d south=%d north=%d'%(
            p.split('/')[-1], s.hz, s.azimuths, s.upt, v.levelDim, v.content, v.stored, v.border, v.tilesX, v.tilesY, v.west,v.east,v.south,v.north))
    except Exception as e:
        print('%s -> %s'%(p.split('/')[-1], e))
