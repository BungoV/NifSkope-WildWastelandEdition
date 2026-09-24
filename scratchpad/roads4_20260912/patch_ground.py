"""ROADS4 item 2 -- the knob the measurement asked for.

A road NIF carries shapes whose MATERIAL is a landscape GROUND material --
`materials/Landscape/Ground/CommonwealthDefault01.bgsm`, `DirtGravel01.BGSM` --
modelled as the verge beside the asphalt.  They are opaque, they take coverage
1, and the bake prints them into the road plane as road paint.  Measured on the
2026-09-12 04:10:38 exe's own sheets, before any code moved:

  chunk (-20,20)  8,337 of 23,116 road texels (36.1%) are won by such a shape
  chunk  (-8,8)   2,756 of 11,069 (24.9%)

and the boundary where such a patch meets the asphalt inside the road plane
reads a mean luminance gradient of 15.387 and 8.010 against Bethesda's 5.362
and 5.138 at the same texels, with displaced-mask floors of 6.077 and 4.805.

This patch adds ONE knob, `LodgenCoverOptions::roadGroundPaint`, the coverage
multiplier for exactly those shapes.  1.0 is today's bake and is BRANCHED OVER,
so the way back is the previous bake's bytes and not a float argument about
1.0f.  0.0 is a drop: `rasterise` already refuses a zero-coverage fragment
before it touches the z buffer, so a ground shape at 0 neither paints nor
occludes the road shape beneath it -- the same result as never gathering it,
with no second code path to keep honest.
"""
import io
import sys


def edit(path, pairs):
    s = io.open(path, encoding='utf-8').read()
    cr0 = io.open(path, 'rb').read().count(b'\r')
    for old, new in pairs:
        n = s.count(old)
        if n != 1:
            sys.exit('ANCHOR %d times in %s: %r' % (n, path, old[:70]))
        s = s.replace(old, new)
    io.open(path, 'w', encoding='utf-8', newline='\n').write(s)
    cr1 = io.open(path, 'rb').read().count(b'\r')
    print('%s ok  CR %d -> %d' % (path, cr0, cr1))


# ------------------------------------------------------------------ lodgen.h
H_OLD = '\tfloat roadDetail = 1.0f;\n'
H_NEW = H_OLD + '''
\t/*! THE COVERAGE MULTIPLIER for a road shape whose MATERIAL IS A LANDSCAPE
\t *  GROUND MATERIAL -- one that lives under `materials/Landscape/Ground/`,
\t *  the same folder the landscape's own painted textures come from.
\t *
\t *  WHAT IT IS FOR. A Fallout 4 road model is not only asphalt. Inside
\t *  `Landscape/Roads/Sanctuary/SancRoadStr01.nif` and its siblings there are
\t *  shapes carrying `CommonwealthDefault01.bgsm` and `DirtGravel01.BGSM` --
\t *  the verge, modelled as terrain and materialled as terrain. The road pass
\t *  cannot tell them from the road surface: they are opaque, so the coverage
\t *  clause gives them 1, and the max-z composite prints them over the ground
\t *  the landscape pass already painted from the cell's own LAND record.
\t *
\t *  MEASURED (lane ROADS4, 2026-09-12, on the 04:10:38 exe's sheets, the
\t *  classification by the material's own FOLDER and not by its name):
\t *
\t *    chunk (-20,20): 8,337 of 23,116 painted road texels (36.1%) are won by
\t *    a ground-material shape; chunk (-8,8): 2,756 of 11,069 (24.9%).
\t *
\t *    where such a patch meets the road surface INSIDE the road plane, the
\t *    mean luminance gradient is 15.387 on (-20,20) and 8.010 on (-8,8);
\t *    Bethesda's shipped sheet reads 5.362 and 5.138 at the same texels, and
\t *    the same boundary set displaced five ways reads 6.077 and 4.805 for
\t *    ours. So the step is roughly three times vanilla's on (-20,20) and
\t *    well clear of its own floor on both.
\t *
\t *    the two classes are 25.3 levels apart in our bake on (-20,20) (road
\t *    surface 110.83, ground-material 85.53) where vanilla's are 2.5 apart
\t *    (93.52 and 91.01). Vanilla's far road is nearly one tone; ours is two.
\t *
\t *  bungo, 2026-09-12, verbatim: *"the issue with the roads is, these meshes
\t *  have some terrain included there, you can see the sharp mesh terrain
\t *  being included into the chunk's bake"*. That is this, and the numbers
\t *  above are its size.
\t *
\t *  WHAT THE VALUE MEANS. The multiply happens on COVERAGE, so it moves the
\t *  paint and the ground-cover suppression together: at 0 a ground-material
\t *  shape neither paints the sheet nor keeps grass off the verge, and the
\t *  landscape's own colour is what stays. 1.0 is branched over entirely, so
\t *  a bake at the old default is the old bake's BYTES. */
\tfloat roadGroundPaint = 1.0f;
'''

CENSUS_OLD = ('\tint sidewalkBases = 0;      //!< distinct sidewalk bases behind'
              ' those refusals\n')
CENSUS_NEW = CENSUS_OLD + (
    '\tint groundShapes = 0;       //!< road shapes whose material is under'
    ' materials/Landscape/Ground/\n'
    '\tint groundTexels = 0;       //!< texels such a shape wrote (0 when'
    ' roadGroundPaint is 0)\n')

edit('src/lodgen.h', [(H_OLD, H_NEW), (CENSUS_OLD, CENSUS_NEW)])

# ---------------------------------------------------------------- lodgen.cpp
A_OLD = '\tsidewalkBases += o.sidewalkBases;\n'
A_NEW = A_OLD + '\tgroundShapes += o.groundShapes;\n\tgroundTexels += o.groundTexels;\n'

L_OLD = '\tkv( "sidewalk_bases", sidewalkBases );\n'
L_NEW = L_OLD + ('\tkv( "ground_shapes", groundShapes );\n'
                 '\tkv( "ground_texels", groundTexels );\n')

PATH_OLD = '''	if ( i < 0 )
		p.prepend( QStringLiteral( "materials/" ) );
	return p;
}
'''
PATH_NEW = PATH_OLD + '''
/*! Is this material one of the LANDSCAPE'S OWN GROUND materials?
 *
 *  The discriminator is the material's FOLDER, which is data and not a guess:
 *  Bethesda files `materials/Landscape/Ground/*` for the textures the terrain
 *  itself is painted with and `materials/Landscape/Roads/*` for the road
 *  surface, its kerbs and its decals. A name-stem list was tried first and
 *  mis-read two of the biggest winners on chunk (-20,20) -- 7,558 texels of
 *  `CommonwealthDefault01.bgsm` and 5,317 of `SancSW01.BGSM` -- because
 *  neither name carries a stem that says which it is.
 *
 *  The path is normalised by `lodgenRoadMaterialPath` first, so the four
 *  prefixes the shipped NIFs actually carry (a bare `materials/...`, a
 *  `Data/materials/...`, and Bethesda's absolute
 *  `C:/Projects/Fallout4/Build/PC/Data/Materials/...`, in any case) all reduce
 *  to the same test. */
bool lodgenRoadMaterialIsGround( const QString & matName )
{
	if ( matName.isEmpty() )
		return false;
	return lodgenRoadMaterialPath( matName ).toLower()
		.contains( QStringLiteral( "materials/landscape/ground/" ) );
}
'''

MAT_OLD = '''	bool alphaBlend = false;
	float alphaRef = 1.0f;
	bool read = false;
};
'''
MAT_NEW = '''	bool alphaBlend = false;
	float alphaRef = 1.0f;
	bool read = false;
	//! The material lives under materials/Landscape/Ground/: it is terrain.
	bool ground = false;
};
'''

SHAPE_OLD = '''	//! Mean world Z of the shape's vertices: the composite's painting order.
	float meanZ = 0.0f;
};
'''
SHAPE_NEW = '''	//! Mean world Z of the shape's vertices: the composite's painting order.
	float meanZ = 0.0f;
	//! The shape's material is one of the LANDSCAPE'S ground materials.
	bool groundMat = false;
};
'''

MATFN_OLD = '''		LodgenRoadMat m;
		QByteArray bytes;
'''
MATFN_NEW = '''		LodgenRoadMat m;
		/* Set from the NAME, before the file is opened, so a ground material
		 * that will not load is still classified as ground rather than
		 * silently falling into the road surface. */
		m.ground = lodgenRoadMaterialIsGround( s.matName );
		QByteArray bytes;
'''

GATHER_OLD = '''			const LodgenRoadMat & m = material( dataRoot, s );
			out.tex0 = m.read && !m.tex0.isEmpty() ? m.tex0 : s.tex0;
'''
GATHER_NEW = '''			const LodgenRoadMat & m = material( dataRoot, s );
			out.tex0 = m.read && !m.tex0.isEmpty() ? m.tex0 : s.tex0;
			out.groundMat = m.ground;
			if ( out.groundMat )
				cen.groundShapes++;
'''

SIG_OLD = '''		int composite = LodgenCoverOptions::RoadMaxZ, float detail = 1.0f ) const
'''
SIG_NEW = '''		int composite = LodgenCoverOptions::RoadMaxZ, float detail = 1.0f,
		float groundPaint = 1.0f ) const
'''

COV_OLD = '''						if ( cov <= 0.0f ) {
							out.alphaRejected++;
							continue;
						}
'''
COV_NEW = '''						/* THE GROUND-MATERIAL SHAPES (lane ROADS4). The
						 * multiply is on COVERAGE, so it moves the paint and
						 * the cover suppression together, and it is branched
						 * over at 1.0 so the old default is the old bytes.
						 * At 0 the fragment falls out at the test below
						 * WITHOUT touching the z buffer, so a ground shape
						 * does not occlude the road shape under it either --
						 * a drop, with no second code path. */
						if ( sh.groundMat ) {
							if ( groundPaint < 1.0f )
								cov *= groundPaint;
							if ( cov > 0.0f )
								out.groundTexels++;
						}
						if ( cov <= 0.0f ) {
							out.alphaRejected++;
							continue;
						}
'''

CALL1 = '''			coverOpts.roadComposite, coverOpts.roadDetail );
'''
CALLN = '''			coverOpts.roadComposite, coverOpts.roadDetail,
			coverOpts.roadGroundPaint );
'''

R_OLD = ('''		r << QString( "roadDetail %1" ).arg( double( opts.cover.roadDetail )'''
         ''', 0, 'f', 3 );\n''')
R_NEW = R_OLD + '''		r << QString( "roadGroundPaint %1" )
			.arg( double( opts.cover.roadGroundPaint ), 0, 'f', 3 );
		r << QString( "roadGroundShapes %1" ).arg( roadCensus.groundShapes );
		r << QString( "roadGroundTexels %1" ).arg( roadCensus.groundTexels );
'''

s = io.open('src/lodgen.cpp', encoding='utf-8').read()
if s.count(CALL1) != 2:
    sys.exit('rasterise call sites: %d' % s.count(CALL1))
s = s.replace(CALL1, CALLN)
io.open('src/lodgen.cpp', 'w', encoding='utf-8', newline='\n').write(s)

edit('src/lodgen.cpp', [(A_OLD, A_NEW), (L_OLD, L_NEW), (PATH_OLD, PATH_NEW),
                        (MAT_OLD, MAT_NEW), (SHAPE_OLD, SHAPE_NEW),
                        (MATFN_OLD, MATFN_NEW), (GATHER_OLD, GATHER_NEW),
                        (SIG_OLD, SIG_NEW), (COV_OLD, COV_NEW),
                        (R_OLD, R_NEW)])

# ---------------------------------------------------------------- nifcli.cpp
C_OLD = '''		else if ( t == QLatin1String( "--road-detail" ) ) {
			lgCover.roadDetail = qBound( 0.0f, next().toFloat(), 1.0f );
			lgRoadDetailSet = true;
		}
'''
C_NEW = C_OLD + '''		/* THE GROUND-MATERIAL SHAPES inside a road model (lane ROADS4):
		 * the coverage multiplier for a shape whose material lives under
		 * materials/Landscape/Ground/. 1 paints them as road, 0 leaves the
		 * landscape's own colour there. The numbers are in
		 * LodgenCoverOptions::roadGroundPaint. */
		else if ( t == QLatin1String( "--road-ground-paint" ) ) {
			lgCover.roadGroundPaint = qBound( 0.0f, next().toFloat(), 1.0f );
			lgRoadGroundPaintSet = true;
		}
'''

D_OLD = '''	bool lgRoadDetailSet = false, lgRoadRaisedSet = false,
'''
D_NEW = '''	bool lgRoadGroundPaintSet = false;
	bool lgRoadDetailSet = false, lgRoadRaisedSet = false,
'''

LEG_OLD = '''		if ( !lgRoadDetailSet )
			lgCover.roadDetail = 1.0f;
'''
LEG_NEW = '''		if ( !lgRoadDetailSet )
			lgCover.roadDetail = 1.0f;
		/* ROADS1 painted every shape in a road model, the verge included. */
		if ( !lgRoadGroundPaintSet )
			lgCover.roadGroundPaint = 1.0f;
'''

edit('src/nifcli.cpp', [(C_OLD, C_NEW), (D_OLD, D_NEW), (LEG_OLD, LEG_NEW)])
print('PATCH DONE')
