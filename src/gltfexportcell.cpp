/* lane GLTFCELL1 (2026-10-28): a whole exterior cell block as glTF 2.0 (see gltfexportcell.h). */

#include "gltfexportcell.h"

#include "cellground.h"
#include "cellview.h"
#include "esmdata.h"
#include "gltfexport.h"
#include "lodgen.h"
#include "nativeemit.h"
#include "wwswitch.h"
#include "model/nifmodel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>

namespace
{

//! Row-major rotation (v' = R v) -> Quat (w, x, y, z), Shepperd's method as src/gltfexportnif.cpp's matrixToQuat.
Quat rotToQuat( const float * r )
{
	const double r00 = r[0], r01 = r[1], r02 = r[2], r10 = r[3], r11 = r[4], r12 = r[5], r20 = r[6], r21 = r[7], r22 = r[8];
	const double tr = r00 + r11 + r22;
	double w, x, y, z;
	if ( tr > 0.0 ) {
		const double s = std::sqrt( tr + 1.0 ) * 2.0;
		w = 0.25 * s; x = ( r21 - r12 ) / s; y = ( r02 - r20 ) / s; z = ( r10 - r01 ) / s;
	} else if ( r00 > r11 && r00 > r22 ) {
		const double s = std::sqrt( 1.0 + r00 - r11 - r22 ) * 2.0;
		w = ( r21 - r12 ) / s; x = 0.25 * s; y = ( r01 + r10 ) / s; z = ( r02 + r20 ) / s;
	} else if ( r11 > r22 ) {
		const double s = std::sqrt( 1.0 + r11 - r00 - r22 ) * 2.0;
		w = ( r02 - r20 ) / s; x = ( r01 + r10 ) / s; y = 0.25 * s; z = ( r12 + r21 ) / s;
	} else {
		const double s = std::sqrt( 1.0 + r22 - r00 - r11 ) * 2.0;
		w = ( r10 - r01 ) / s; x = ( r02 + r20 ) / s; y = ( r12 + r21 ) / s; z = 0.25 * s;
	}
	const double n = std::sqrt( w * w + x * x + y * y + z * z );
	return Quat( float( w / n ), float( x / n ), float( y / n ), float( z / n ) );
}

constexpr float CELL_UNITS = 4096.0f;

QString texRel( const QString & raw )
{
	if ( raw.isEmpty() )
		return raw;
	QString p = raw.toLower();
	p.replace( QLatin1Char( '\\' ), QLatin1Char( '/' ) );
	while ( p.startsWith( QLatin1Char( '/' ) ) )
		p.remove( 0, 1 );
	if ( !p.startsWith( QLatin1String( "textures/" ) ) )
		p.prepend( QLatin1String( "textures/" ) );
	return p;
}

class CellGltfSink : public CellExportSink
{
public:
	GltfExportScene scene;
	QSet<QPair<int, int>> cells;   // empty = every cell the build placed
	float origin[3] = { 0, 0, 0 };
	bool terrain = true;
	QString dataRoot;
	QSet<QString> textures;        // relative "textures/..." paths the materials name
	QHash<QString, QVector<int>> meshesOf;   // model key -> its glTF meshes (one per drawn shape)
	int placementsSeen = 0, placementsKept = 0, placementsEmpty = 0, shapesSkipped = 0, landQuads = 0;
	qint64 trisPlaced = 0;

	bool want( float x, float y ) const
	{
		return cells.isEmpty()
			|| cells.contains( { int( std::floor( x / CELL_UNITS ) ), int( std::floor( y / CELL_UNITS ) ) } );
	}

	int node( const QString & name, int parent )
	{
		GltfExportNode n;
		n.name = name;
		n.parent = parent;
		scene.nodes.append( n );
		return scene.nodes.size() - 1;
	}

	QString tex( const QString & raw )
	{
		const QString rel = texRel( raw );
		if ( !rel.isEmpty() )
			textures.insert( rel );
		return rel;
	}

	void begin( const EsmWorld & world, const QString & root ) override
	{
		dataRoot = root;
		if ( !terrain )
			return;
		const int landNode = node( QStringLiteral( "terrain" ), -1 );
		QSet<QPair<int, int>> todo = cells;
		if ( todo.isEmpty() )
			return;   // the CLI always names cells when terrain is on
		for ( const auto & c : todo ) {
			EsmLand land;
			if ( !world.land( c.first, c.second, land ) || !land.valid )
				continue;
			for ( int q = 0; q < 4; q++ ) {   // 0 BL, 1 BR, 2 TL, 3 TR
				const int c0 = ( q & 1 ) ? 16 : 0, r0 = ( q & 2 ) ? 16 : 0;
				GltfExportMesh m;
				m.name = QStringLiteral( "land %1,%2 q%3" ).arg( c.first ).arg( c.second ).arg( q );
				for ( int r = r0; r <= r0 + 16; r++ )
					for ( int k = c0; k <= c0 + 16; k++ ) {
						const float x = c.first * CELL_UNITS + k * 128.0f, y = c.second * CELL_UNITS + r * 128.0f;
						m.positions.append( Vector3( x - origin[0], y - origin[1], land.heights[r][k] - origin[2] ) );
						Vector3 nr;
						if ( land.hasNormals )
							nr = Vector3( land.normals[r][k][0], land.normals[r][k][1], land.normals[r][k][2] );
						else {
							const int kl = std::max( k - 1, 0 ), kr = std::min( k + 1, 32 );
							const int rd = std::max( r - 1, 0 ), ru = std::min( r + 1, 32 );
							nr = Vector3( -( land.heights[r][kr] - land.heights[r][kl] ) / ( ( kr - kl ) * 128.0f ),
								-( land.heights[ru][k] - land.heights[rd][k] ) / ( ( ru - rd ) * 128.0f ), 1.0f );
							nr.normalize();
						}
						m.normals.append( nr );
						m.texCoords.append( Vector2( x / CELL_GROUND_TILING, -y / CELL_GROUND_TILING ) );
					}
				for ( int r = 0; r < 16; r++ )
					for ( int k = 0; k < 16; k++ ) {
						const quint32 a = r * 17 + k, b = a + 1, d = a + 17, e = d + 1;
						m.indices << a << b << e << a << e << d;
					}
				QString diffuse, normal;
				if ( land.baseTex[q] )
					world.ltexTextures( land.baseTex[q], diffuse, normal );
				m.materialName = QStringLiteral( "land_%1" ).arg( land.baseTex[q], 8, 16, QLatin1Char( '0' ) );
				m.diffuseSourcePath = diffuse;
				m.diffuseUri = tex( diffuse );
				m.normalUri = tex( normal );
				m.doubleSided = false;
				m.materialExtras.append( { QStringLiteral( "ltex" ),
					QStringLiteral( "%1" ).arg( land.baseTex[q], 8, 16, QLatin1Char( '0' ) ) } );
				m.materialExtras.append( { QStringLiteral( "paintLayers" ), QString::number( land.layers[q].size() ) } );
				m.node = node( m.name, landNode );
				scene.meshes.append( m );
				landQuads++;
			}
		}
	}

	QVector<int> meshes( const CellExportPlaced & p, const QString & label )
	{
		auto it = meshesOf.find( p.key );
		if ( it != meshesOf.end() )
			return it.value();
		QVector<int> out;
		int k = 0;
		for ( const NativeSrcShape & s : *p.shapes ) {
			const LodoSrcShape & g = s.geom;
			const size_t nv = g.pos.size() / 3;
			if ( nv == 0 || g.tris.empty() || s.nearFacts.waterShader ) {
				shapesSkipped++;
				continue;
			}
			GltfExportMesh m;
			m.name = QStringLiteral( "%1 #%2" ).arg( label ).arg( k++ );
			m.positions.reserve( int( nv ) );
			for ( size_t v = 0; v < nv; v++ )
				m.positions.append( Vector3( g.pos[v * 3], g.pos[v * 3 + 1], g.pos[v * 3 + 2] ) );
			if ( g.nrm.size() == nv * 3 )
				for ( size_t v = 0; v < nv; v++ )
					m.normals.append( Vector3( g.nrm[v * 3], g.nrm[v * 3 + 1], g.nrm[v * 3 + 2] ) );
			if ( g.uv.size() == nv * 2 )
				for ( size_t v = 0; v < nv; v++ )
					m.texCoords.append( Vector2( g.uv[v * 2], g.uv[v * 2 + 1] ) );
			for ( quint32 t : g.tris )
				m.indices.append( t );
			const NearShapeFacts & f = s.nearFacts;
			const QString diffuse = s.tex0.isEmpty() ? s.effectTex0 : s.tex0;
			m.materialName = s.matName.isEmpty() ? m.name : s.matName;
			m.diffuseSourcePath = diffuse;
			m.diffuseUri = tex( diffuse );
			if ( !f.effectShader && !f.modelSpaceNormals )
				m.normalUri = tex( s.tex1 );
			if ( f.alphaTest ) {
				m.alphaMode = 1;
				m.alphaCutoff = f.alphaRef / 255.0f;
			} else if ( f.alphaBlend || s.effectBlend ) {
				m.alphaMode = 2;
			}
			m.doubleSided = f.twoSided;
			auto extra = [&m]( const char * k, const QString & v ) {
				if ( !v.isEmpty() )
					m.materialExtras.append( { QString::fromLatin1( k ), v } );
			};
			extra( "material", s.matName );
			extra( "specularTexture", f.effectShader ? QString() : texRel( s.tex7 ) );   // R specular, G gloss
			extra( "glowTexture", texRel( s.glowTex ) );
			extra( "greyscaleTexture", s.g2p ? texRel( s.g2pTex ) : QString() );
			extra( "smoothness", f.effectShader ? QString() : QString::number( s.smoothness ) );
			extra( "specularMult", f.effectShader ? QString() : QString::number( s.specMult ) );
			if ( s.ownEmit || s.glowFlag )
				extra( "emitColor", QStringLiteral( "%1 %2 %3 x%4" ).arg( s.emitColor[0] ).arg( s.emitColor[1] )
					.arg( s.emitColor[2] ).arg( s.emitMult ) );
			extra( "effect", f.effectShader ? QStringLiteral( "1" ) : QString() );
			extra( "decal", f.decal ? QStringLiteral( "1" ) : QString() );
			extra( "vertexColors", f.vertexColour ? QStringLiteral( "not written" ) : QString() );
			if ( !s.glowTex.isEmpty() )
				tex( s.glowTex );
			if ( !f.effectShader && !s.tex7.isEmpty() )
				tex( s.tex7 );
			out.append( scene.meshes.size() );
			scene.meshes.append( m );   // its owner node is the first placement's child (placed())
		}
		meshesOf.insert( p.key, out );
		return out;
	}

	void placed( const CellExportPlaced & p ) override
	{
		placementsSeen++;
		if ( !p.shapes || !want( p.pos[0], p.pos[1] ) )
			return;
		const QString label = QFileInfo( p.model ).completeBaseName();
		const QVector<int> ms = meshes( p, label );
		if ( ms.isEmpty() ) {
			placementsEmpty++;
			return;
		}
		placementsKept++;
		QString name = QStringLiteral( "%1 %2" ).arg( p.ref, 8, 16, QLatin1Char( '0' ) ).arg( label );
		if ( p.part >= 0 )
			name += QStringLiteral( " part%1" ).arg( p.part );
		GltfExportNode n;
		n.name = name;
		n.translation = Vector3( p.pos[0] - origin[0], p.pos[1] - origin[1], p.pos[2] - origin[2] );
		n.rotation = rotToQuat( p.rot );
		n.scale = Vector3( p.scale, p.scale, p.scale );
		scene.nodes.append( n );
		const int parent = scene.nodes.size() - 1;
		for ( int m : ms ) {
			const int c = node( scene.meshes[m].name, parent );
			if ( scene.meshes[m].node < 0 )
				scene.meshes[m].node = c;
			else
				scene.nodes[c].mesh = m;
			trisPlaced += scene.meshes[m].indices.size() / 3;
		}
	}
};

void usage( const QString & why )
{
	std::fprintf( stderr, "gltf-cell: %s\nusage: gltf-cell \"plugins|world|x,y|n\"|<file.wwcell> --out <abs .gltf> "
		"[--cells x,y ...] [--origin x,y,z] [--textures copy|none] [--terrain on|off]\n", qPrintable( why ) );
}

bool parseTriple( const QString & s, float * v, int n )
{
	const QStringList f = s.split( QLatin1Char( ',' ) );
	if ( f.size() != n )
		return false;
	for ( int i = 0; i < n; i++ ) {
		bool ok = false;
		v[i] = f[i].trimmed().toFloat( &ok );
		if ( !ok )
			return false;
	}
	return true;
}

} // namespace

int gltfCellCli( const QStringList & args )
{
	if ( args.isEmpty() ) {
		usage( QStringLiteral( "no cell given" ) );
		return 2;
	}
	CellGltfSink sink;
	QString outPath, texMode = QStringLiteral( "copy" );
	bool originSet = false;
	for ( int i = 1; i < args.size(); i++ ) {
		const QString & a = args[i];
		const QString v = i + 1 < args.size() ? args[i + 1] : QString();
		if ( a == QLatin1String( "--out" ) && !v.isEmpty() ) {
			outPath = v;
			i++;
		} else if ( a == QLatin1String( "--origin" ) && parseTriple( v, sink.origin, 3 ) ) {
			originSet = true;
			i++;
		} else if ( a == QLatin1String( "--textures" ) && ( v == QLatin1String( "copy" ) || v == QLatin1String( "none" ) ) ) {
			texMode = v;
			i++;
		} else if ( a == QLatin1String( "--terrain" ) && ( v == QLatin1String( "on" ) || v == QLatin1String( "off" ) ) ) {
			sink.terrain = v == QLatin1String( "on" );
			i++;
		} else if ( a == QLatin1String( "--cells" ) ) {
			float xy[2];
			while ( i + 1 < args.size() && parseTriple( args[i + 1], xy, 2 ) ) {
				sink.cells.insert( { int( xy[0] ), int( xy[1] ) } );
				i++;
			}
		} else {
			usage( QStringLiteral( "unknown or incomplete option %1" ).arg( a ) );
			return 2;
		}
	}
	if ( outPath.isEmpty() || QFileInfo( outPath ).isRelative() ) {
		usage( QStringLiteral( "--out must be an absolute .gltf path" ) );
		return 2;
	}
	CellSceneSpec spec;
	QString err, notes;
	const QString arg = args.first();
	const bool file = arg.endsWith( QLatin1String( ".wwcell" ), Qt::CaseInsensitive );
	if ( !( file ? cellSpecFromFile( arg, spec, &err ) : cellSpecFromLine( arg, spec, &err ) ) || !spec.valid ) {
		usage( err.isEmpty() ? QStringLiteral( "bad cell spec" ) : err );
		return 2;
	}
	if ( spec.interior ) {
		usage( QStringLiteral( "interiors are not exported yet" ) );
		return 2;
	}
	int x0, y0, x1, y1;
	spec.rect( x0, y0, x1, y1 );
	if ( sink.cells.isEmpty() )
		for ( int y = y0; y <= y1; y++ )
			for ( int x = x0; x <= x1; x++ )
				sink.cells.insert( { x, y } );
	if ( !originSet ) {   // the cells' south-west corner, at z 0
		int mx = INT_MAX, my = INT_MAX;
		for ( const auto & c : sink.cells ) {
			mx = std::min( mx, c.first );
			my = std::min( my, c.second );
		}
		sink.origin[0] = mx * CELL_UNITS;
		sink.origin[1] = my * CELL_UNITS;
	}
	// a -no-gui run never applies the resource stack (main.cpp's GUI branch does)
	const QString res = wwEnvStr( "WW_LODGEN_RESOURCES" );
	if ( !res.isEmpty() )
		lodgenSetResources( res.split( QLatin1Char( ';' ), Qt::SkipEmptyParts ) );
	qputenv( "WW_CELL_STREAM", "0" );   // the whole block in one build, no streamer
	spec.water = false;
	spec.grid = false;   // spec.terrain stays: the LAND is read with it; the build's ground mesh is never reached
	cellExportSetSink( &sink );
	NifModel nif;
	const bool built = nifCreateCellScene( &nif, spec, &err, &notes );
	cellExportSetSink( nullptr );
	if ( !built ) {
		std::fprintf( stderr, "gltf-cell: the cell build failed: %s\n", qPrintable( err ) );
		return 2;
	}
	if ( sink.placementsSeen == 0 ) {
		std::fprintf( stderr, "gltf-cell: the build placed nothing\n" );
		return 2;
	}
	// textures: copied out of the stack (archives or loose) beside the file, relative URIs
	int copied = 0, missing = 0;
	QStringList missed;
	const QDir outDir = QFileInfo( outPath ).absoluteDir();
	if ( texMode == QLatin1String( "copy" ) ) {
		QStringList list = sink.textures.values();
		list.sort();
		for ( const QString & rel : list ) {
			QByteArray bytes;
			if ( !lodgenReadAssetBytes( sink.dataRoot, rel, "textures", ".dds", bytes ) || bytes.isEmpty() ) {
				missing++;
				missed << rel;
				continue;
			}
			const QString dst = outDir.filePath( rel );
			QDir().mkpath( QFileInfo( dst ).absolutePath() );
			QFile f( dst );
			if ( !f.open( QIODevice::WriteOnly ) || f.write( bytes ) != bytes.size() ) {
				std::fprintf( stderr, "gltf-cell: cannot write %s\n", qPrintable( dst ) );
				return 2;
			}
			copied++;
		}
	}
	sink.scene.shareImages = true;
	sink.scene.unitScale = 0.9144f / 64.0f;
	sink.scene.generator = QStringLiteral( "NifSkope WW gltf-cell" );
	QString werr;
	if ( !gltfExportWrite( sink.scene, outPath, werr ) ) {
		std::fprintf( stderr, "gltf-cell: %s\n", qPrintable( werr ) );
		return 2;
	}
	for ( const QString & m : missed )
		std::printf( "gltf-cell: texture missing %s\n", qPrintable( m ) );
	std::printf( "MEASURED placements_seen=%d placements=%d placements_empty=%d models=%d meshes=%d nodes=%d "
		"land_quads=%d shapes_skipped=%d tris_placed=%lld textures=%d copied=%d missing=%d origin=%g,%g,%g\n",
		sink.placementsSeen, sink.placementsKept, sink.placementsEmpty, int( sink.meshesOf.size() ),
		int( sink.scene.meshes.size() ), int( sink.scene.nodes.size() ), sink.landQuads, sink.shapesSkipped,
		sink.trisPlaced, int( sink.textures.size() ), copied, missing, sink.origin[0], sink.origin[1], sink.origin[2] );
	return 0;
}
