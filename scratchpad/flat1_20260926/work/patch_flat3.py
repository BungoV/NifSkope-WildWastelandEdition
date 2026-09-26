import sys
P = 'E:/Projects/NifskopeWWE-flat1/src/lodgen.cpp'
s = open(P, newline='').read()
def rep(a, b, n=1):
    global s
    c = s.count(a)
    if c != n:
        sys.exit('anchor count %d != %d: %r' % (c, n, a[:80]))
    s = s.replace(a, b)
rep('''		LodgenRoadSet roads;
		roads.gather( world, dataRoot, chunkX, chunkY,''', '''		LodgenRoadSet roads;
		roads.setFlat( coverOpts.flatObjects, coverOpts.flatObjectsFile );
		roads.gather( world, dataRoot, chunkX, chunkY,''')
rep('''		roads->rasterise( wx0, wyTop, upt, S, roadPlane, bc, dataRoot, local,
			coverOpts.roadComposite, coverOpts.roadDetail,
			coverOpts.roadGroundPaint );''', '''		roads->rasterise( wx0, wyTop, upt, S, roadPlane, bc, dataRoot, local,
			coverOpts.roadComposite, coverOpts.roadDetail,
			coverOpts.roadGroundPaint, border );''')
rep('''		roadSet.reset( new LodgenRoadSet );
		roadSet->gather(''', '''		roadSet.reset( new LodgenRoadSet );
		roadSet->setFlat( opts.cover.flatObjects, opts.cover.flatObjectsFile );
		roadSet->gather(''')
open(P, 'w', newline='').write(s)
print('ok')
