"""FLAT1: splice the flat-ground-object pass into src/lodgen.cpp (LodgenRoadSet). Run once."""
import sys

P = 'E:/Projects/NifskopeWWE-flat1/src/lodgen.cpp'
s = open(P, newline='').read()


def _dedent1(t):
    return '\n'.join(l[1:] if l.startswith('\t') else l for l in t.split('\n'))


def rep(a, b, n=1):
    global s
    c = s.count(a)
    if c != n:
        a2, b2 = _dedent1(a), _dedent1(b)
        if s.count(a2) == n:
            print('anchor matched one tab shallower:', repr(a.strip()[:50]))
            a, b = a2, b2
        else:
            sys.exit('anchor count %d != %d: %r' % (c, n, a[:80]))
    s = s.replace(a, b)


# --- includes -----------------------------------------------------------------
rep('#include <QMutex>\n', '#include <QMutex>\n#include <QCoreApplication>\n#include <QCryptographicHash>\n')

# --- the override file + helpers, after LodgenRoadCensus::line() ----------------
HELPERS = r'''
/* ---------------------------------------------------------------------------
 * FLAT GROUND OBJECTS (lane FLAT1, 2026-09-26) -- the override file.
 *
 * Plain text, one rule a line: `bake <path>` or `nobake <path>`, `#` starts a
 * comment. A path ending in `.nif` names one model; anything else is a folder
 * prefix, matched at a folder boundary. Paths are relative to Meshes, either
 * slash, any case. The LONGEST matching path wins, and a later line wins a tie.
 * The bake reads the file every time and never writes it; the DEFAULT file is
 * created, header only, when it is missing.
 * --------------------------------------------------------------------------- */
namespace
{
struct LodgenFlatOverride
{
	QString key;        //!< normalised: lower case, forward slashes, no leading meshes/
	bool bake = false;
	bool folder = false;
	int line = 0;
};

QString lodgenFlatModelKey( const QString & model )
{
	QString p = model.trimmed().toLower();
	p.replace( QChar( 92 ), QChar( '/' ) );
	while ( p.startsWith( QChar( '/' ) ) )
		p.remove( 0, 1 );
	if ( p.startsWith( QStringLiteral( "data/" ) ) )
		p.remove( 0, 5 );
	if ( p.startsWith( QStringLiteral( "meshes/" ) ) )
		p.remove( 0, 7 );
	return p;
}

const char * const kLodgenFlatHeader =
	"# NifSkope LOD generation: flat ground objects -- YOUR overrides.\n"
	"#\n"
	"# The terrain bake paints objects that lie flat on the ground (slabs, railway\n"
	"# tracks, paths, decals, debris) into the far terrain's colour. Which ones is\n"
	"# measured from each placed mesh. A line here beats that measurement:\n"
	"#\n"
	"#   nobake  SetDressing/Example/SomeFlatThing.nif    never paint this model\n"
	"#   bake    MyMod/GroundBits/                        paint everything in this folder\n"
	"#\n"
	"# A path ending in .nif is one model; anything else is a folder. Paths are under\n"
	"# Meshes, either slash, any case. The longest match wins. The bake reads this\n"
	"# file every time and never changes it. Each bake writes a report beside its\n"
	"# output listing every model the rule looked at and what it decided.\n";

/*! Resolve, create when it is the missing default, and parse. `problems` gets
 *  one entry per line that is not a rule. */
bool lodgenFlatLoadOverrides( const QString & file, QVector<LodgenFlatOverride> & out,
	QString * resolved, QStringList * problems )
{
	out.clear();
	const QString path = file.isEmpty() ? lodgenFlatObjectsDefaultFile() : file;
	if ( resolved )
		*resolved = path;
	QFile f( path );
	if ( !f.exists() ) {
		if ( !file.isEmpty() ) {
			if ( problems )
				problems->append( QStringLiteral( "file not found" ) );
			return false;
		}
		QFile w( path );
		if ( w.open( QIODevice::WriteOnly | QIODevice::NewOnly ) )
			w.write( kLodgenFlatHeader );
		return true;
	}
	if ( !f.open( QIODevice::ReadOnly ) ) {
		if ( problems )
			problems->append( QStringLiteral( "file will not open" ) );
		return false;
	}
	const QStringList lines = QString::fromUtf8( f.readAll() ).split( QChar( '\n' ) );
	for ( int i = 0; i < lines.size(); i++ ) {
		QString l = lines[i];
		const int h = l.indexOf( QChar( '#' ) );
		if ( h >= 0 )
			l.truncate( h );
		l = l.trimmed();
		if ( l.isEmpty() )
			continue;
		int sp = 0;
		while ( sp < l.size() && !l[sp].isSpace() )
			sp++;
		const QString word = l.left( sp ).toLower();
		const QString rest = l.mid( sp ).trimmed();
		if ( ( word != QLatin1String( "bake" ) && word != QLatin1String( "nobake" ) ) || rest.isEmpty() ) {
			if ( problems )
				problems->append( QString( "line %1 is not `bake <path>` or `nobake <path>`" ).arg( i + 1 ) );
			continue;
		}
		LodgenFlatOverride o;
		o.bake = ( word == QLatin1String( "bake" ) );
		o.key = lodgenFlatModelKey( rest );
		o.folder = !o.key.endsWith( QStringLiteral( ".nif" ) );
		if ( o.folder && !o.key.endsWith( QChar( '/' ) ) )
			o.key += QChar( '/' );
		o.line = i + 1;
		out.append( o );
	}
	return true;
}

//! The override that decides a model: -1 none, else its index in `rules`.
int lodgenFlatOverrideFor( const QVector<LodgenFlatOverride> & rules, const QString & model )
{
	if ( rules.isEmpty() )
		return -1;
	const QString k = lodgenFlatModelKey( model );
	int best = -1, bestLen = -1;
	for ( int i = 0; i < rules.size(); i++ ) {
		const LodgenFlatOverride & o = rules[i];
		const bool hit = o.folder ? k.startsWith( o.key ) : ( k == o.key );
		if ( hit && o.key.size() >= bestLen ) {
			best = i;
			bestLen = o.key.size();
		}
	}
	return best;
}

/*! A KIND LABEL for the report, from the path: rail, decal, pad, path or debris.
 *  The rule never reads it -- it only makes the report readable. */
const char * lodgenFlatKindLabel( const QString & model )
{
	const QString p = lodgenFlatModelKey( model );
	const QString f = p.mid( p.lastIndexOf( QChar( '/' ) ) + 1 );
	auto in = []( const QString & t, std::initializer_list<const char *> ks ) {
		for ( const char * k : ks )
			if ( t.contains( QLatin1String( k ) ) )
				return true;
		return false;
	};
	if ( in( p, { "rrtrack", "railroad/rrtie", "traintrack", "trackbed" } ) )
		return "rail";
	if ( in( f, { "decal", "crack", "stain", "manhole", "drain", "grate" } ) )
		return "decal";
	if ( in( p, { "parking", "slab", "foundation", "floor", "pad0", "platform", "lot0" } ) )
		return "pad";
	if ( in( p, { "path", "trail", "dirtroad", "dirtpatch", "mudpatch" } ) && !p.contains( QLatin1String( "trailer" ) ) )
		return "path";
	return "debris";
}

//! numpy's linear percentile over an unsorted list (the census's own statistic).
double lodgenFlatPercentile( std::vector<double> v, double q )
{
	if ( v.empty() )
		return 0.0;
	std::sort( v.begin(), v.end() );
	const double pos = q / 100.0 * double( v.size() - 1 );
	const size_t lo = size_t( std::floor( pos ) );
	const size_t hi = qMin( lo + 1, v.size() - 1 );
	return v[lo] + ( v[hi] - v[lo] ) * ( pos - double( lo ) );
}
} // namespace

QString lodgenFlatObjectsDefaultFile()
{
	QString d = QCoreApplication::applicationDirPath();
	if ( d.isEmpty() )
		d = QDir::currentPath();
	return d + QStringLiteral( "/lodgen_flat_objects.txt" );
}

QString lodgenFlatObjectsRulesDigest( const QString & file )
{
	QVector<LodgenFlatOverride> rules;
	QStringList problems;
	lodgenFlatLoadOverrides( file, rules, nullptr, &problems );
	QByteArray b;
	for ( const LodgenFlatOverride & o : rules ) {
		b += o.bake ? "bake " : "nobake ";
		b += o.key.toUtf8();
		b += '\n';
	}
	return QString::number( rules.size() ) + QChar( ':' )
		+ QString::fromLatin1( QCryptographicHash::hash( b, QCryptographicHash::Sha1 ).toHex().left( 16 ) );
}
'''

anchor = 'void LodgenObjectAoCensus::addRefusal( const char * why, const QString & name )\n'
rep(anchor, HELPERS.lstrip('\n') + '\n' + anchor)

# --- LodgenRoadShape: the raised flag --------------------------------------------
rep('''	//! The shape's material is one of the LANDSCAPE'S ground materials.
	bool groundMat = false;
};
''', '''	//! The shape's material is one of the LANDSCAPE'S ground materials.
	bool groundMat = false;
	//! Lane FLAT1: the shape's model is in a raised road folder (a deck, not ground).
	bool raisedModel = false;
};

/*! Lane FLAT1: one flat object's shape, in world space, with what the game's
 *  material adds on top of a road shape's operands. */
struct LodgenFlatShape : LodgenRoadShape
{
	QVector<float> rise;            //!< per vertex: height above LAND (0 where no LAND)
	int rec = -1;                   //!< the placement's row in the report
	float uOff = 0.0f, vOff = 0.0f, uScale = 1.0f, vScale = 1.0f;  //!< the material's UV transform
	float matAlpha = 1.0f;          //!< the material's own opacity (blended shapes)
	float tint[3] = { 1.0f, 1.0f, 1.0f };   //!< BGEM base colour x scale; 1 for a BGSM
	bool vertexAlpha = false;       //!< SLSF1 Vertex_Alpha: the vertex alpha scales a blended shape
};

//! Lane FLAT1: what the flat pass reads out of one material (BGSM or BGEM).
struct LodgenFlatMat
{
	QString tex0;
	bool read = false, effect = false, decal = false, alphaTest = false, alphaBlend = false;
	bool g2p = false;
	float alphaRef = 1.0f, alpha = 1.0f;
	float uOff = 0.0f, vOff = 0.0f, uScale = 1.0f, vScale = 1.0f;
	float tint[3] = { 1.0f, 1.0f, 1.0f };
};

//! Lane FLAT1: one placement the flat rule looked at -- a row of the report.
struct LodgenFlatRec
{
	QString model, plugin;
	quint32 refr = 0, base = 0;
	double top = 0.0, under = 0.0, ratio = 0.0;
	int squares = 0;
	bool measured = false, painted = false, rulePaint = false, hasLod = false;
	int override = -1;              //!< index into the override rules, -1 none
	const char * why = "";          //!< the rule's refusal reason; empty = the rule paints
};

//! Lane FLAT1: a non-road STAT placement, kept until the road shapes exist.
struct LodgenFlatCand
{
	quint32 refr = 0, base = 0, nameBase = 0, mswp = 0;
	Vector3 pos;
	Matrix rot;
	float scale = 1.0f;
};
''')

# --- gather: flat switch, pass refr/nameBase/mswp, evaluate at the end -----------
rep('''		const int margin = 2;
		raised = includeRaised;
		sidewalks = includeSidewalks;
''', '''		const int margin = 2;
		raised = includeRaised;
		sidewalks = includeSidewalks;
		flatCands.clear();
''')
rep('''						if ( std::memcmp( &r.baseType, "SCOL", 4 ) == 0 ) {
							for ( const EsmScolPart & part : world.scolParts( r.base ) ) {
								const LodgenMaterialSubst * sw = swapFor( world,
									lodgenRoadEffectiveSwap( world, r.materialSwap, r.base, part.base ) );
								for ( const EsmScolPlacement & pl : part.placements ) {
									Matrix pm;
									pm.fromEuler( -pl.rot[0], -pl.rot[1], -pl.rot[2] );
									addPlacement( world, dataRoot, loaded, part.base,
										rp + rm * ( Vector3( pl.pos[0], pl.pos[1], pl.pos[2] )
											* r.scale ),
										rm * pm, r.scale * pl.scale, sw );
								}
							}
							continue;
						}
						addPlacement( world, dataRoot, loaded, r.base, rp, rm, r.scale,
							swapFor( world, lodgenRoadEffectiveSwap( world, r.materialSwap, r.base, 0 ) ) );
					}
				}
			}
		}
''', '''						if ( std::memcmp( &r.baseType, "SCOL", 4 ) == 0 ) {
							for ( const EsmScolPart & part : world.scolParts( r.base ) ) {
								const quint32 mswp =
									lodgenRoadEffectiveSwap( world, r.materialSwap, r.base, part.base );
								const LodgenMaterialSubst * sw = swapFor( world, mswp );
								for ( const EsmScolPlacement & pl : part.placements ) {
									Matrix pm;
									pm.fromEuler( -pl.rot[0], -pl.rot[1], -pl.rot[2] );
									addPlacement( world, dataRoot, loaded, part.base,
										rp + rm * ( Vector3( pl.pos[0], pl.pos[1], pl.pos[2] )
											* r.scale ),
										rm * pm, r.scale * pl.scale, sw, r.formID, r.base, mswp );
								}
							}
							continue;
						}
						const quint32 mswp = lodgenRoadEffectiveSwap( world, r.materialSwap, r.base, 0 );
						addPlacement( world, dataRoot, loaded, r.base, rp, rm, r.scale,
							swapFor( world, mswp ), r.formID, r.base, mswp );
					}
				}
			}
			if ( flatOn )
				evaluateFlat( world, dataRoot );
		}

		/*! Lane FLAT1: paint flat ground objects too (before `gather`), with the
		 *  override file `file` (empty = the default beside the executable). */
		void setFlat( bool on, const QString & file )
		{
			flatOn = on;
			flatFile = file;
		}

		/*! Lane FLAT1: the per-bake report -- one row per base model the flat rule
		 *  looked at. Written beside the bake output by the caller. */
		QString flatReport( const QString & title ) const;
''')

# --- rasterise: signature, empty test, zbuf in blend mode, flat pass ------------
rep('''			int composite = LodgenCoverOptions::RoadMaxZ, float detail = 1.0f,
			float groundPaint = 1.0f ) const
		{
			colour.assign( size_t( S ) * S, 0U );
			if ( shapes.isEmpty() )
				return;''', '''			int composite = LodgenCoverOptions::RoadMaxZ, float detail = 1.0f,
			float groundPaint = 1.0f, int inset = 0 ) const
		{
			colour.assign( size_t( S ) * S, 0U );
			if ( shapes.isEmpty() && flatShapes.isEmpty() )
				return;''')
rep('''							} else {
								float * a = &acc[o * 4];
								const float k = 1.0f - cov;
								for ( int ch = 0; ch < 3; ch++ )
									a[ch] = c[ch] * cov + a[ch] * k;
								a[3] = cov + a[3] * k;
								if ( cov < 1.0f )
									partial[o] = 1;
							}''', '''							} else {
								float * a = &acc[o * 4];
								const float k = 1.0f - cov;
								for ( int ch = 0; ch < 3; ch++ )
									a[ch] = c[ch] * cov + a[ch] * k;
								a[3] = cov + a[3] * k;
								if ( cov < 1.0f )
									partial[o] = 1;
								/* Lane FLAT1: the road's top, so a flat object under
								 * a road stays under it. Read by nothing else. */
								if ( z > zbuf[o] )
									zbuf[o] = z;
							}''')
rep('''		out.texels += wrote;
		out.decalTexels += dwrote;
		decalHere.clear();
	}
''', '''		out.texels += wrote;
		out.decalTexels += dwrote;
		decalHere.clear();
		if ( !flatShapes.isEmpty() )
			rasteriseFlat( wx0, wyTop, upt, S, colour, zbuf, bc, dataRoot, out, inset );
	}

	/*! THE FLAT PASS (lane FLAT1), after the roads, into the same plane.
	 *
	 *  THE ORDER, stated: first every OPAQUE flat shape, max-z against the road
	 *  surface's own z (a slab under a road stays under it) and REPLACING what is
	 *  there; then every DECAL, alpha-BLENDED or alpha-TESTED flat shape, in
	 *  ascending mean world Z (gather order at equal Z), composited OVER:
	 *
	 *      A   = a + A0 (1 - a)
	 *      rgb = ( src a + rgb0 A0 (1 - a) ) / A
	 *
	 *  with `a` the fragment's coverage. An over-shape is not painted where the
	 *  surface already there stands more than 4 units above it, so a decal under
	 *  a road or a slab does not print through it. A fragment more than 8 units
	 *  below LAND is not painted at all: the terrain hides it in game.
	 *  Only texels a flat fragment reaches are rewritten, so every other byte of
	 *  the plane is the road pass's own. */
	void rasteriseFlat( float wx0, float wyTop, float upt, int S,
		std::vector<quint32> & colour, std::vector<float> & zbuf, LodgenBakeCaches & bc,
		const QString & dataRoot, LodgenRoadCensus & out, int inset ) const
	{
		const float wy0 = wyTop - float( S ) * upt;
		std::vector<int> opaque, over;
		for ( int k = 0; k < flatShapes.size(); k++ ) {
			const LodgenFlatShape & sh = flatShapes[k];
			if ( sh.bx1 < wx0 || sh.bx0 > wx0 + float( S ) * upt )
				continue;
			if ( sh.by1 < wy0 || sh.by0 > wyTop )
				continue;
			( ( sh.decal || sh.alphaBlend || sh.alphaTest ) ? over : opaque ).push_back( k );
		}
		if ( opaque.empty() && over.empty() )
			return;
		const QVector<LodgenFlatShape> & fv = flatShapes;
		std::stable_sort( over.begin(), over.end(),
			[&fv]( int a, int b ) { return fv[a].meanZ < fv[b].meanZ; } );
		std::vector<int> owner( size_t( S ) * S, -1 );
		auto paint = [&]( int k, bool composite ) {
			const LodgenFlatShape & sh = flatShapes[k];
			const DDSTexture16 * tex = sh.tex0.isEmpty() ? nullptr
				: lodgenCachedTexture( bc, dataRoot, sh.tex0 );
			if ( !tex ) {
				out.flatRefusedNoTexture++;
				out.addRefusal( "flat-no-diffuse", sh.tex0.isEmpty()
					? QStringLiteral( "(shape names none)" ) : sh.tex0 );
				return;
			}
			out.flatShapes++;
			const int texW = int( tex->getWidth() ), texH = int( tex->getHeight() );
			const bool vc = !sh.col.isEmpty();
			for ( const Triangle & t : sh.tris ) {
				float px[3], py[3], pz[3];
				for ( int q = 0; q < 3; q++ ) {
					const Vector3 & v = sh.pos[t[q]];
					px[q] = ( v[0] - wx0 ) / upt;
					py[q] = ( wyTop - v[1] ) / upt;
					pz[q] = v[2];
				}
				int i0 = int( std::floor( qMin( px[0], qMin( px[1], px[2] ) ) ) );
				int i1 = int( std::ceil( qMax( px[0], qMax( px[1], px[2] ) ) ) );
				int j0 = int( std::floor( qMin( py[0], qMin( py[1], py[2] ) ) ) );
				int j1 = int( std::ceil( qMax( py[0], qMax( py[1], py[2] ) ) ) );
				i0 = qMax( i0, 0 ); j0 = qMax( j0, 0 );
				i1 = qMin( i1, S - 1 ); j1 = qMin( j1, S - 1 );
				if ( i1 < i0 || j1 < j0 )
					continue;
				const float d = ( py[1] - py[2] ) * ( px[0] - px[2] )
					+ ( px[2] - px[1] ) * ( py[0] - py[2] );
				if ( std::fabs( d ) < 1e-9f )
					continue;
				// the mip, as the road pass: texture area (the material's UV scale in) over footprint
				float mip = 0.0f;
				if ( !sh.uv.isEmpty() ) {
					const Vector2 & a = sh.uv[t[0]];
					const Vector2 & b = sh.uv[t[1]];
					const Vector2 & c = sh.uv[t[2]];
					const float uvA = std::fabs( ( b[0] - a[0] ) * ( c[1] - a[1] )
						- ( c[0] - a[0] ) * ( b[1] - a[1] ) ) * float( texW ) * float( texH )
						* std::fabs( sh.uScale * sh.vScale );
					const float pxA = std::fabs( d );
					if ( uvA > 0.0f && pxA > 0.0f )
						mip = qBound( 0.0f, 0.5f * std::log2( uvA / pxA ),
							float( tex->getMaxMipLevel() ) );
				}
				for ( int j = j0; j <= j1; j++ ) {
					for ( int i = i0; i <= i1; i++ ) {
						const float x = float( i ) + 0.5f, y = float( j ) + 0.5f;
						const float w0 = ( ( py[1] - py[2] ) * ( x - px[2] )
							+ ( px[2] - px[1] ) * ( y - py[2] ) ) / d;
						const float w1 = ( ( py[2] - py[0] ) * ( x - px[2] )
							+ ( px[0] - px[2] ) * ( y - py[2] ) ) / d;
						const float w2 = 1.0f - w0 - w1;
						if ( w0 < 0.0f || w1 < 0.0f || w2 < 0.0f )
							continue;
						const float z = w0 * pz[0] + w1 * pz[1] + w2 * pz[2];
						const size_t o = size_t( j ) * S + i;
						if ( composite ? ( z + 4.0f < zbuf[o] ) : ( z <= zbuf[o] ) )
							continue;
						// buried: the terrain covers this fragment in game
						if ( w0 * sh.rise[t[0]] + w1 * sh.rise[t[1]] + w2 * sh.rise[t[2]] < -8.0f )
							continue;
						float u = 0.0f, v = 0.0f;
						if ( !sh.uv.isEmpty() ) {
							u = w0 * sh.uv[t[0]][0] + w1 * sh.uv[t[1]][0] + w2 * sh.uv[t[2]][0];
							v = w0 * sh.uv[t[0]][1] + w1 * sh.uv[t[1]][1] + w2 * sh.uv[t[2]][1];
						}
						u = u * sh.uScale + sh.uOff;
						v = v * sh.vScale + sh.vOff;
						u = u - std::floor( u );
						v = v - std::floor( v );
						FloatVector4 c = tex->getPixelT( u, v, mip );
						float cov = 1.0f;
						if ( sh.alphaTest )
							cov = ( c[3] >= sh.alphaRef ) ? 1.0f : 0.0f;
						else if ( sh.alphaBlend )
							cov = qBound( 0.0f, c[3] * sh.matAlpha, 1.0f );
						for ( int q = 0; q < 3; q++ )
							c[q] *= sh.tint[q];
						if ( vc ) {
							const Color4 & ca = sh.col[t[0]];
							const Color4 & cb = sh.col[t[1]];
							const Color4 & cc = sh.col[t[2]];
							for ( int q = 0; q < 3; q++ )
								c[q] *= w0 * ca[q] + w1 * cb[q] + w2 * cc[q];
							if ( sh.alphaBlend && sh.vertexAlpha )
								cov *= qBound( 0.0f, w0 * ca[3] + w1 * cb[3] + w2 * cc[3], 1.0f );
						}
						if ( cov <= 0.0f )
							continue;
						float r = qBound( 0.0f, c[0], 1.0f ), g = qBound( 0.0f, c[1], 1.0f ),
							b = qBound( 0.0f, c[2], 1.0f ), A = 1.0f;
						if ( !composite ) {
							zbuf[o] = z;
						} else if ( cov < 1.0f ) {
							const quint32 d0 = colour[o];
							const float A0 = float( d0 >> 24 ) / 255.0f;
							A = cov + A0 * ( 1.0f - cov );
							if ( A > 0.0f ) {
								const float k0 = A0 * ( 1.0f - cov );
								r = ( r * cov + float( ( d0 >> 16 ) & 255U ) / 255.0f * k0 ) / A;
								g = ( g * cov + float( ( d0 >> 8 ) & 255U ) / 255.0f * k0 ) / A;
								b = ( b * cov + float( d0 & 255U ) / 255.0f * k0 ) / A;
							}
						}
						const quint32 a8 = quint32( qBound( 0.0f, A * 255.0f + 0.5f, 255.0f ) );
						if ( a8 == 0U )
							continue;
						colour[o] = ( a8 << 24 )
							| ( quint32( qBound( 0, int( r * 255.0f + 0.5f ), 255 ) ) << 16 )
							| ( quint32( qBound( 0, int( g * 255.0f + 0.5f ), 255 ) ) << 8 )
							| quint32( qBound( 0, int( b * 255.0f + 0.5f ), 255 ) );
						owner[o] = k;
					}
				}
			}
		};
		for ( int k : opaque )
			paint( k, false );
		for ( int k : over )
			paint( k, true );
		const int e = S - inset;
		for ( int j = inset; j < e; j++ )
			for ( int i = inset; i < e; i++ ) {
				const int k = owner[size_t( j ) * S + i];
				if ( k < 0 )
					continue;
				const LodgenFlatShape & sh = flatShapes[k];
				out.flatTexels++;
				if ( sh.decal || sh.alphaBlend || sh.alphaTest )
					out.flatDecalTexels++;
				if ( sh.rec >= 0 && size_t( sh.rec ) < flatRecTexels.size() )
					flatRecTexels[size_t( sh.rec )]++;
			}
	}
''')

# --- addPlacement: new params, flat candidates, raised flag ---------------------
rep('''	void addPlacement( const EsmWorld & world, const QString & dataRoot,
		QSet<QString> & loaded, quint32 base, const Vector3 & pos,
		const Matrix & rot, float scale, const LodgenMaterialSubst * swap )
	{
		const EsmLodBase & lb = world.lodBase( base );
		if ( std::memcmp( &lb.type, "STAT", 4 ) != 0 )
			return;
		if ( !lodgenIsRoadModel( lb.model ) )
			return;''', '''	void addPlacement( const EsmWorld & world, const QString & dataRoot,
		QSet<QString> & loaded, quint32 base, const Vector3 & pos,
		const Matrix & rot, float scale, const LodgenMaterialSubst * swap,
		quint32 refr = 0, quint32 nameBase = 0, quint32 mswp = 0 )
	{
		const EsmLodBase & lb = world.lodBase( base );
		if ( std::memcmp( &lb.type, "STAT", 4 ) != 0 )
			return;
		if ( !lodgenIsRoadModel( lb.model ) ) {
			/* Lane FLAT1: every other STAT is a flat-object CANDIDATE, measured
			 * once the road shapes (part of the ground it is measured on) exist. */
			if ( flatOn && !lb.model.isEmpty() ) {
				LodgenFlatCand c;
				c.refr = refr;
				c.base = base;
				c.nameBase = nameBase ? nameBase : base;
				c.mswp = mswp;
				c.pos = pos;
				c.rot = rot;
				c.scale = scale;
				flatCands.append( c );
			}
			return;
		}''')
rep('''				out.meanZ = float( zsum / double( out.pos.size() ) );
				shapes.append( out );
			}
		}
''', '''				out.meanZ = float( zsum / double( out.pos.size() ) );
				out.raisedModel = lodgenIsRaisedRoadModel( lb.model );
				shapes.append( out );
			}
		}

		/* ---- lane FLAT1: the measurement ------------------------------------ */

		//! LAND, bilinear inside the cell's 33x33 grid; NaN where the cell has none.
		double landAt( const EsmWorld & world, double x, double y )
		{
			const int cx = int( std::floor( x / 4096.0 ) ), cy = int( std::floor( y / 4096.0 ) );
			const qint64 key = ( qint64( cx ) << 32 ) ^ qint64( quint32( cy ) );
			auto it = landCache.constFind( key );
			if ( it == landCache.constEnd() ) {
				QVector<float> h;
				std::unique_ptr<EsmLand> L( new EsmLand );
				if ( world.land( cx, cy, *L ) ) {
					h.resize( 33 * 33 );
					for ( int j = 0; j < 33; j++ )
						for ( int i = 0; i < 33; i++ )
							h[j * 33 + i] = L->heights[j][i];
				}
				it = landCache.insert( key, h );
			}
			if ( it->isEmpty() )
				return std::numeric_limits<double>::quiet_NaN();
			const QVector<float> & h = *it;
			const double fx = ( x - double( cx ) * 4096.0 ) / 128.0;
			const double fy = ( y - double( cy ) * 4096.0 ) / 128.0;
			const int i0 = qBound( 0, int( std::floor( fx ) ), 31 );
			const int j0 = qBound( 0, int( std::floor( fy ) ), 31 );
			const double tx = fx - i0, ty = fy - j0;
			return h[j0 * 33 + i0] * ( 1 - tx ) * ( 1 - ty ) + h[j0 * 33 + i0 + 1] * tx * ( 1 - ty )
				+ h[( j0 + 1 ) * 33 + i0] * ( 1 - tx ) * ty + h[( j0 + 1 ) * 33 + i0 + 1] * tx * ty;
		}

		//! The cell's water height, NaN for none.
		double waterAt( const EsmWorld & world, double x, double y )
		{
			const int cx = int( std::floor( x / 4096.0 ) ), cy = int( std::floor( y / 4096.0 ) );
			const qint64 key = ( qint64( cx ) << 32 ) ^ qint64( quint32( cy ) );
			auto it = waterCache.constFind( key );
			if ( it == waterCache.constEnd() ) {
				float h = 0.0f;
				const double v = world.cellWater( cx, cy, h )
					? double( h ) : std::numeric_limits<double>::quiet_NaN();
				it = waterCache.insert( key, v );
			}
			return *it;
		}

		//! The ground roads make: the highest non-raised road triangle over the
		//! centre of 16-unit square (bx,by); NaN where none.
		double roadZSquare( qint64 bx, qint64 by )
		{
			const qint64 key = ( bx << 32 ) ^ qint64( quint32( by ) );
			auto it = roadZCache.constFind( key );
			if ( it != roadZCache.constEnd() )
				return *it;
			const double x = ( double( bx ) + 0.5 ) * 16.0, y = ( double( by ) + 0.5 ) * 16.0;
			double best = std::numeric_limits<double>::quiet_NaN();
			const qint64 gx = qint64( std::floor( x / 256.0 ) ), gy = qint64( std::floor( y / 256.0 ) );
			auto g = roadGrid.constFind( ( gx << 32 ) ^ qint64( quint32( gy ) ) );
			if ( g != roadGrid.constEnd() ) {
				for ( const QPair<int, int> & st : *g ) {
					const LodgenRoadShape & sh = shapes[st.first];
					const Triangle & t = sh.tris[st.second];
					const Vector3 & a = sh.pos[t[0]];
					const Vector3 & b = sh.pos[t[1]];
					const Vector3 & c = sh.pos[t[2]];
					const double d = ( double( b[1] ) - c[1] ) * ( double( a[0] ) - c[0] )
						+ ( double( c[0] ) - b[0] ) * ( double( a[1] ) - c[1] );
					if ( std::fabs( d ) < 1e-9 )
						continue;
					const double w0 = ( ( double( b[1] ) - c[1] ) * ( x - c[0] ) + ( double( c[0] ) - b[0] ) * ( y - c[1] ) ) / d;
					const double w1 = ( ( double( c[1] ) - a[1] ) * ( x - c[0] ) + ( double( a[0] ) - c[0] ) * ( y - c[1] ) ) / d;
					const double w2 = 1.0 - w0 - w1;
					if ( w0 < 0.0 || w1 < 0.0 || w2 < 0.0 )
						continue;
					const double z = w0 * a[2] + w1 * b[2] + w2 * c[2];
					if ( std::isnan( best ) || z > best )
						best = z;
				}
			}
			roadZCache.insert( key, best );
			return best;
		}

		static double groundOf( double land, double road )
		{
			if ( std::isnan( road ) )
				return land;
			if ( std::isnan( land ) )
				return road;
			return qMax( land, road );
		}

		void buildRoadGrid()
		{
			roadGrid.clear();
			roadZCache.clear();
			for ( int si = 0; si < shapes.size(); si++ ) {
				const LodgenRoadShape & sh = shapes[si];
				if ( sh.raisedModel )
					continue;
				for ( int ti = 0; ti < sh.tris.size(); ti++ ) {
					const Triangle & t = sh.tris[ti];
					float x0 = sh.pos[t[0]][0], x1 = x0, y0 = sh.pos[t[0]][1], y1 = y0;
					for ( int q = 1; q < 3; q++ ) {
						x0 = qMin( x0, sh.pos[t[q]][0] ); x1 = qMax( x1, sh.pos[t[q]][0] );
						y0 = qMin( y0, sh.pos[t[q]][1] ); y1 = qMax( y1, sh.pos[t[q]][1] );
					}
					const qint64 gx0 = qint64( std::floor( x0 / 256.0f ) ), gx1 = qint64( std::floor( x1 / 256.0f ) );
					const qint64 gy0 = qint64( std::floor( y0 / 256.0f ) ), gy1 = qint64( std::floor( y1 / 256.0f ) );
					for ( qint64 gy = gy0; gy <= gy1; gy++ )
						for ( qint64 gx = gx0; gx <= gx1; gx++ )
							roadGrid[( gx << 32 ) ^ qint64( quint32( gy ) )].append( qMakePair( si, ti ) );
				}
			}
		}

		/*! THE RULE (lane FLAT1). Kind-agnostic and path-agnostic: every number is
		 *  read off the placed mesh against the ground under it, so a mod's flat
		 *  object with no entry anywhere is measured like a vanilla one. The
		 *  thresholds come from the Boston-box census (DONE.md section 2):
		 *
		 *    under water      the placement's origin is below its cell's water
		 *    not on the ground  median over its 16-unit squares of (lowest point
		 *                     - ground) above 16
		 *    under the ground 90th percentile of (highest point - ground) below -8
		 *    too tall         that 90th percentile above 64
		 *    stands up        (steep area rising more than 8 above the ground) /
		 *                     (up-facing area not buried more than 16) above 0.35
		 *    no top surface   no such up-facing area
		 *
		 *  "Ground" is LAND raised to the stamped road surface where a road lies
		 *  above it (a raised deck is not ground, so a thing on a bridge floats). */
		void evaluateFlat( const EsmWorld & world, const QString & dataRoot )
		{
			flatRules.clear();
			flatProblems.clear();
			lodgenFlatLoadOverrides( flatFile, flatRules, &flatFileResolved, &flatProblems );
			buildRoadGrid();
			for ( int ci = 0; ci < flatCands.size(); ci++ ) {
				const LodgenFlatCand & c = flatCands[ci];
				const EsmLodBase lb = world.lodBase( c.base );
				LodgenFlatRec rec;
				rec.model = lb.model;
				rec.refr = c.refr;
				rec.base = c.base;
				rec.plugin = world.recordPlugin( c.refr );
				rec.hasLod = lb.hasLod
					|| ( c.nameBase != c.base && world.lodBase( c.nameBase ).hasLod );
				rec.override = lodgenFlatOverrideFor( flatRules, lb.model );
				cen.flatExamined++;
				const LodgenMaterialSubst * sw = swapFor( world, c.mswp );
				const QVector<LodSrcShape> src = lodgenLoadModel( dataRoot, lb.model, modelCache, sw );
				const int recIndex = flatRecs.size();
				if ( src.isEmpty() ) {
					rec.why = "would not load";
					flatRecs.append( rec );
					continue;
				}
				// world space, and the highest / lowest point per 16-unit square
				QVector<QVector<Vector3>> wv( src.size() );
				QHash<qint64, QPair<double, double>> sq;
				auto bin = [&sq]( double x, double y, double z ) {
					const qint64 bx = qint64( std::floor( x / 16.0 ) ), by = qint64( std::floor( y / 16.0 ) );
					const qint64 k = ( bx << 32 ) ^ qint64( quint32( by ) );
					auto it = sq.find( k );
					if ( it == sq.end() )
						sq.insert( k, qMakePair( z, z ) );
					else {
						it->first = qMax( it->first, z );
						it->second = qMin( it->second, z );
					}
				};
				for ( int si = 0; si < src.size(); si++ ) {
					const LodSrcShape & s = src[si];
					QVector<Vector3> & w = wv[si];
					w.resize( s.pos.size() );
					for ( int k = 0; k < s.pos.size(); k++ ) {
						w[k] = c.pos + c.rot * ( s.pos[k] * c.scale );
						bin( w[k][0], w[k][1], w[k][2] );
					}
					for ( const Triangle & t : s.tris ) {
						const Vector3 m = ( w[t[0]] + w[t[1]] + w[t[2]] ) / 3.0f;
						bin( m[0], m[1], m[2] );
					}
				}
@@TAIL@@
''')

open(P, 'w', newline='').write(s)
print('patched')
