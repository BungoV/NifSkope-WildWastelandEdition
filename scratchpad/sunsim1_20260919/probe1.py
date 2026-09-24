import sys, time, numpy as np
ROOT='E:/Projects/NifskopeWildWastelandEdition'
sys.path.insert(0, ROOT+'/tests/spells')
import lodgen_native_decode as ND
BAKE = ROOT + '/scratchpad/horizon2_20260918/dumpbake/nat/FO4CSLOD/Commonwealth/Commonwealth'
t=time.time(); L=ND.read_lodo(BAKE+'.lodo'); print('lodo %.1fs'%(time.time()-t))
t=time.time(); T=ND.read_lodi(BAKE+'.lodi'); print('lodi %.1fs'%(time.time()-t))
h=T['header']
print('lodi v%d inst=%d chunks=%d A=%d steps=%d reach=%.0f'%(h['version'],h['instanceCount'],h['chunkCount'],h['horizonAzimuths'],h['horizonSteps'],h['horizonReach']))
print('lodo meshes=%d clusters=%d verts=%d bases=%d'%(L['header']['meshCount'],L['header']['clusterCount'],L['header']['vertexCount'],L['header']['baseCount']))
print('slotInstances', h['slotInstances'])
# mesh vertex population candidates
import collections
mv_sum0={}; mv_span={}; mv_sumall={}
for mi,m in enumerate(L['meshes']):
    cs=L['clusters'][m['clusterFirst']:m['clusterFirst']+m['clusterCount']]
    ls=L['clusterLods'][m['clusterFirst']:m['clusterFirst']+m['clusterCount']]
    mv_sum0[mi]=sum(c['vertexCount'] for c,l in zip(cs,ls) if l['level']==0)
    mv_sumall[mi]=sum(c['vertexCount'] for c in cs)
    if cs: mv_span[mi]=max(c['vertexBase']+c['vertexCount'] for c in cs)-min(c['vertexBase'] for c in cs)
    else: mv_span[mi]=0
ao=T['vertexAoFirst']
match=collections.Counter()
for i in range(min(h['instanceCount'], 4000)):
    n=ao[i+1]-ao[i]
    b=L['bases'][T['instances'][i]['baseId']]
    reps=[b['rep%d'%k] for k in range(4)]
    tag=[]
    for k,r in enumerate(reps):
        if r==0xFFFF or r>=len(L['meshes']): continue
        if mv_sum0[r]==n: tag.append('slot%d.sum0'%k)
        if mv_sumall[r]==n: tag.append('slot%d.sumall'%k)
        if mv_span[r]==n: tag.append('slot%d.span'%k)
    match[tuple(tag)]+=1
for k,v in match.most_common(8): print(v,k)
