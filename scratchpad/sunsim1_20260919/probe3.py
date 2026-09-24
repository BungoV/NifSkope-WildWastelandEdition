import sys, time, numpy as np
ROOT='E:/Projects/NifskopeWildWastelandEdition'
sys.path.insert(0, ROOT+'/tests/spells')
import lodgen_native_decode as ND
BAKE = ROOT + '/scratchpad/horizon2_20260918/dumpbake/nat/FO4CSLOD/Commonwealth/Commonwealth'
L=ND.read_lodo(BAKE+'.lodo'); T=ND.read_lodi(BAKE+'.lodi')
lv=np.array([(v['px'],v['py'],v['pz'],v['n0'],v['n1'],v['n2']) for v in L['vertices']],dtype=np.float64)
n=(lv[:,3:6]/255.0)*2.0-1.0
ln=np.linalg.norm(n,axis=1)
print('vertex normal |n| mean %.4f p1 %.3f p99 %.3f'%(ln.mean(),np.percentile(ln,1),np.percentile(ln,99)))
# triangles per instance
tot=0; totv=0; big=0
mtris={}
for mi,m in enumerate(L['meshes']):
    cs=L['clusters'][m['clusterFirst']:m['clusterFirst']+m['clusterCount']]
    ls=L['clusterLods'][m['clusterFirst']:m['clusterFirst']+m['clusterCount']]
    mtris[mi]=sum(c['triangleCount'] for c,l in zip(cs,ls) if l['level']==0)
print('levelMax', L['header']['levelMax'])
for i in range(T['header']['instanceCount']):
    b=L['bases'][T['instances'][i]['baseId']]
    mid=b['rep0']
    tot+=mtris.get(mid,0)
print('instanced level0 triangles total: %d'%tot)
xs=np.array([r['x'] for r in T['instances']]); ys=np.array([r['y'] for r in T['instances']]); zs=np.array([r['z'] for r in T['instances']])
print('instance world x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f'%(xs.min(),xs.max(),ys.min(),ys.max(),zs.min(),zs.max()))
