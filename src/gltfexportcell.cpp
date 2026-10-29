/* lane GLTFCELL1 (2026-10-28): a whole exterior cell block as glTF 2.0 (see gltfexportcell.h). */

#include "gltfexportcell.h"

#include "celldecal.h"
#include "cellground.h"
#include "cellvertexalpha.h"
#include "cellview.h"
#include "esmdata.h"
#include "gltfexport.h"
#include "lodgen.h"
#include "nativeemit.h"
#include "wwswitch.h"
#include "io/material.h"
#include "model/nifmodel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <climits>
#include <limits>
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
	QHash<QString, QVector<int>> meshesOf;   // model key + paint -> its glTF meshes (one per drawn shape)
	const EsmWorld * world = nullptr;
	int meshesPainted = 0;
	int placementsSeen = 0, placementsKept = 0, placementsEmpty = 0, shapesSkipped = 0, landQuads = 0;
	int meshesColored = 0, meshesDecal = 0, landLayers = 0, ltexFromMaterial = 0, ltexUnresolved = 0;
	int meshesClamped = 0;   //!< materials whose texture clamps on an axis (the game's tile flags off)
	qint64 trisPlaced = 0;
	float decalOffset = 1.0f;      // game units pushed along the normal: Blender has no depth bias for coplanar decals
	// the opaque surfaces a placed decal may land on, (world - origin), pos + normal interleaved (the cell view's
	// receivers: its welded opaque buckets and the ground)
	struct Receiver
	{
		std::vector<float> pn;
		std::vector<quint32> tris;
	};
	std::vector<Receiver> receivers;
	CellDecalResult decalResult;
	int meshesPlacedDecal = 0;

	void receive( const QVector<Vector3> & pos, const QVector<Vector3> & nrm, const QVector<quint32> & idx )
	{
		if ( pos.isEmpty() || nrm.size() != pos.size() || idx.size() < 3 )
			return;
		Receiver r;
		r.pn.reserve( size_t( pos.size() ) * 6 );
		for ( int v = 0; v < pos.size(); v++ )
			for ( const Vector3 * p : { &pos[v], &nrm[v] } )
				for ( int k = 0; k < 3; k++ )
					r.pn.push_back( ( *p )[k] );
		r.tris.assign( idx.begin(), idx.end() );
		receivers.push_back( std::move( r ) );
	}

	// FO4 vertex colours multiply the gamma-space diffuse; glTF COLOR_n is linear
	static float linearOf( quint8 b ) { return std::pow( b / 255.0f, 2.2f ); }

	// an LTEX's diffuse and normal; a material-backed TXST names a .bgsm, whose own slots answer (as lodgen reads them)
	void ltexResolve( const EsmWorld & world, quint32 ltex, QString & diffuse, QString & normal )
	{
		diffuse.clear();
		normal.clear();
		if ( !ltex )
			return;
		world.ltexTextures( ltex, diffuse, normal );
		const EsmLtexTextureSet & set = world.ltexTextureSet( ltex );
		QString mat = set.material;
		if ( mat.isEmpty() && diffuse.endsWith( QLatin1String( ".bgsm" ), Qt::CaseInsensitive ) )
			mat = diffuse;
		if ( mat.isEmpty() )
			return;
		diffuse.clear();
		normal.clear();
		mat = mat.toLower();
		mat.replace( QLatin1Char( '\\' ), QLatin1Char( '/' ) );
		const int mi = mat.lastIndexOf( QLatin1String( "materials/" ) );
		if ( mi > 0 )
			mat.remove( 0, mi );
		else if ( mi < 0 )
			mat.prepend( QLatin1String( "materials/" ) );
		QByteArray bytes;
		if ( lodgenReadAssetBytes( dataRoot, mat, "materials", ".bgsm", bytes ) ) {
			const ShaderMaterial sm( bytes );
			if ( sm.isValid() ) {
				const QStringList & t = sm.textures();
				if ( t.size() > 0 )
					diffuse = t[0];
				if ( t.size() > 1 )
					normal = t[1];
			}
		}
		if ( diffuse.isEmpty() )
			ltexUnresolved++;
		else
			ltexFromMaterial++;
	}

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
		this->world = &world;
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
				ltexResolve( world, land.baseTex[q], diffuse, normal );
				m.materialName = m.name;   // the paint differs per quadrant, so the material does too
				m.diffuseSourcePath = diffuse;
				m.diffuseUri = tex( diffuse );
				m.normalUri = tex( normal );
				m.doubleSided = false;
				m.materialExtras.append( { QStringLiteral( "ltex" ),
					QStringLiteral( "%1" ).arg( land.baseTex[q], 8, 16, QLatin1Char( '0' ) ) } );
				/* COLOR_0 = the LAND's vertex colours (white without VCLR); COLOR_1.. = the ATXT/VTXT layer opacities,
				 * four layers per set in paint order, layerN* extras name each layer's textures. */
				QVector<float> vc;
				vc.reserve( 17 * 17 * 4 );
				for ( int r = r0; r <= r0 + 16; r++ )
					for ( int k = c0; k <= c0 + 16; k++ )
						for ( int ch = 0; ch < 4; ch++ )
							vc.append( ch < 3 && land.hasColors ? linearOf( land.colors[r][k][ch] ) : 1.0f );
				m.colorSets.append( vc );
				int nl = 0;
				for ( const EsmLandLayer & L : land.layers[q] ) {
					QString ld, ln;
					ltexResolve( world, L.ltex, ld, ln );
					if ( ld.isEmpty() )
						continue;
					if ( nl % 4 == 0 )
						m.colorSets.append( QVector<float>( 17 * 17 * 4, 0.0f ) );
					QVector<float> & w = m.colorSets.last();
					for ( int r = 0; r <= 16; r++ )
						for ( int k = 0; k <= 16; k++ )
							w[( r * 17 + k ) * 4 + nl % 4] = L.opacity[r][k];
					m.materialExtras.append( { QStringLiteral( "layer%1" ).arg( nl ), tex( ld ) } );
					if ( !ln.isEmpty() )
						m.materialExtras.append( { QStringLiteral( "layer%1Normal" ).arg( nl ), tex( ln ) } );
					nl++;
				}
				landLayers += nl;
				m.materialExtras.append( { QStringLiteral( "paintLayers" ), QString::number( nl ) } );
				m.node = node( m.name, landNode );
				receive( m.positions, m.normals, m.indices );
				scene.meshes.append( m );
				landQuads++;
			}
		}
	}

	/* The paint row (cellview.cpp "THE PAINT"): an MSWP row's CNAM replaces the palette row of its material, else the
	 * base's MODC; the game takes either only on a Greyscale_To_PaletteColor material. Keyed by the row's original and
	 * its replacement, since the shapes here are the swapped ones. */
	QHash<QString, float> paintOf( const CellExportPlaced & p, float & modc, bool & hasModc ) const
	{
		QHash<QString, float> out;
		const EsmLodBase & lb = world->lodBase( p.base );
		hasModc = lb.hasColorRemap && lb.colorRemap < std::numeric_limits<float>::max();
		modc = lb.colorRemap;
		if ( !p.paintSwap )
			return out;
		const EsmMaterialSwap & mw = world->materialSwap( p.paintSwap );
		for ( const EsmMaterialSubst & row : mw.rows ) {
			if ( !row.hasColorRemap || row.colorRemap >= std::numeric_limits<float>::max() )
				continue;
			for ( const QString & m : { row.original, row.replacement } ) {
				const QString k = lodgenMaterialSwapKey( m );
				if ( !k.isEmpty() && !out.contains( k ) )
					out.insert( k, row.colorRemap );
			}
		}
		return out;
	}

	QVector<int> meshes( const CellExportPlaced & p, const QString & label )
	{
		float modc = 0.0f;
		bool hasModc = false;
		const QHash<QString, float> cnam = world ? paintOf( p, modc, hasModc ) : QHash<QString, float>();
		QString key = p.key;
		if ( hasModc )
			key += QStringLiteral( "|modc%1" ).arg( modc );
		for ( auto c = cnam.cbegin(); c != cnam.cend(); ++c )
			key += QStringLiteral( "|%1=%2" ).arg( c.key() ).arg( c.value() );
		auto it = meshesOf.find( key );
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
			// filled only when the shape has a colour channel AND the Vertex_Colors flag (lodofile.h)
			const bool coloured = g.rgba.size() == nv * 4;
			// A is opacity only where the game uses it so; a tree's vertex alpha is its wind weight (cellvertexalpha.h)
			const bool alphaKept = coloured
				&& wwCellKeepsVertexAlpha( true, f.effectShader, false, g.vertexAlpha, f.treeAnim, false );
			if ( coloured ) {
				QVector<float> vc( int( nv * 4 ) );
				for ( size_t v = 0; v < nv * 4; v++ )
					vc[int( v )] = v % 4 != 3 ? linearOf( g.rgba[v] ) : alphaKept ? g.rgba[v] / 255.0f : 1.0f;
				m.colorSets.append( vc );
				meshesColored++;
			}
			const bool decal = f.decal;   // the shape's flags OR'd with its .bgsm's (lodgen.cpp)
			if ( decal && decalOffset > 0.0f && m.normals.size() == m.positions.size() ) {
				for ( int v = 0; v < m.positions.size(); v++ )
					m.positions[v] += m.normals[v] * decalOffset;
				meshesDecal++;
			}
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
			m.wrap = f.tile & 3;
			if ( m.wrap != 3 )
				meshesClamped++;
			auto extra = [&m]( const char * k, const QString & v ) {
				if ( !v.isEmpty() )
					m.materialExtras.append( { QString::fromLatin1( k ), v } );
			};
			extra( "material", s.matName );
			extra( "specularTexture", f.effectShader ? QString() : texRel( s.tex7 ) );   // R specular, G gloss
			extra( "glowTexture", texRel( s.glowTex ) );
			extra( "greyscaleTexture", s.g2p ? texRel( s.g2pTex ) : QString() );
			// the game's palette lookup: albedo = palette( diffuse G, paletteScale x vertex R ) (fo4_default.frag)
			extra( "paletteScale", s.g2p && !s.g2pTex.isEmpty() ? QString::number( s.g2pScale ) : QString() );
			// a painted part reads its palette row straight from the CNAM/MODC (the game writes it to fLookupScale)
			if ( s.g2p && !s.g2pTex.isEmpty() && s.g2pScale > 1.0e-6f ) {
				auto c = cnam.constFind( lodgenMaterialSwapKey( s.matName ) );
				if ( c != cnam.constEnd() || hasModc ) {
					extra( "paletteRow", QString::number( c != cnam.constEnd() ? c.value() : modc ) );
					meshesPainted++;
				}
			}
			extra( "smoothness", f.effectShader ? QString() : QString::number( s.smoothness ) );
			extra( "specularMult", f.effectShader ? QString() : QString::number( s.specMult ) );
			if ( s.ownEmit || s.glowFlag )
				extra( "emitColor", QStringLiteral( "%1 %2 %3 x%4" ).arg( s.emitColor[0] ).arg( s.emitColor[1] )
					.arg( s.emitColor[2] ).arg( s.emitMult ) );
			extra( "effect", f.effectShader ? QStringLiteral( "1" ) : QString() );
			extra( "decal", decal ? QString::number( decalOffset ) : QString() );
			extra( "vertexColors", coloured ? QStringLiteral( "COLOR_0" ) : QString() );
			extra( "vertexAlpha", alphaKept ? QStringLiteral( "1" ) : QString() );
			if ( !s.glowTex.isEmpty() )
				tex( s.glowTex );
			if ( s.g2p && !s.g2pTex.isEmpty() )
				tex( s.g2pTex );
			if ( !f.effectShader && !s.tex7.isEmpty() )
				tex( s.tex7 );
			out.append( scene.meshes.size() );
			scene.meshes.append( m );   // its owner node is the first placement's child (placed())
		}
		meshesOf.insert( key, out );
		return out;
	}

	void placed( const CellExportPlaced & p ) override
	{
		placementsSeen++;
		if ( !p.shapes )
			return;
		for ( const NativeSrcShape & s : *p.shapes ) {   // the decal receivers, as the cell view's opaque buckets
			const NearShapeFacts & f = s.nearFacts;
			const LodoSrcShape & g = s.geom;
			const size_t nv = g.pos.size() / 3;
			if ( nv == 0 || g.tris.size() < 3 || g.nrm.size() != nv * 3 || f.alphaTest || f.alphaBlend || s.effectBlend
				|| f.effectShader || f.decal || f.waterShader )
				continue;
			QVector<Vector3> wp( static_cast<int>( nv ) ), wn( static_cast<int>( nv ) );
			for ( size_t v = 0; v < nv; v++ )
				for ( int r = 0; r < 3; r++ ) {
					wp[int( v )][r] = p.rot[r * 3] * g.pos[v * 3] * p.scale + p.rot[r * 3 + 1] * g.pos[v * 3 + 1] * p.scale
						+ p.rot[r * 3 + 2] * g.pos[v * 3 + 2] * p.scale + p.pos[r] - origin[r];
					wn[int( v )][r] = p.rot[r * 3] * g.nrm[v * 3] + p.rot[r * 3 + 1] * g.nrm[v * 3 + 1]
						+ p.rot[r * 3 + 2] * g.nrm[v * 3 + 2];
				}
			receive( wp, wn, QVector<quint32>( g.tris.begin(), g.tris.end() ) );
		}
		if ( !want( p.pos[0], p.pos[1] ) )
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

	/* The placed decals, clipped out of the receivers by the cell view's own rule (celldecal.h) and written in world
	 * space under one node. Blended; COLOR_0 alpha = the angle fade per vertex (the game fades per pixel). No
	 * specular: the game lights a decal like the surface under it, which a material of its own cannot say. */
	void decals( const std::vector<CellDecalRef> & all ) override
	{
		if ( !world )
			return;
		std::vector<CellDecalRef> refs;
		for ( const CellDecalRef & d : all )
			if ( want( d.pos[0], d.pos[1] ) )
				refs.push_back( d );
		if ( refs.empty() )
			return;
		std::vector<CellDecalReceiver> rcv;
		for ( const Receiver & r : receivers ) {
			CellDecalReceiver c;
			c.pos = &r.pn[0];
			c.nrm = &r.pn[3];
			c.stride = 6 * sizeof( float );
			c.numVerts = r.pn.size() / 6;
			c.tris = r.tris.data();
			c.numTris = r.tris.size() / 3;
			rcv.push_back( c );
		}
		cellProjectDecals( *world, dataRoot, refs, rcv, origin, decalResult, QString() );
		int parent = -1;
		for ( const CellDecalMesh & d : decalResult.meshes ) {
			if ( d.tris.empty() )
				continue;
			GltfExportMesh m;
			m.name = d.name;   // "decal <EDID>"
			QVector<float> vc;
			for ( const CellDecalVert & v : d.verts ) {
				const Vector3 n( v.nrm[0], v.nrm[1], v.nrm[2] );
				m.positions.append( Vector3( v.pos[0], v.pos[1], v.pos[2] ) + n * decalOffset );
				m.normals.append( n );
				m.texCoords.append( Vector2( v.uv[0], v.uv[1] ) );
				vc << 1.0f << 1.0f << 1.0f << v.alpha;
			}
			m.colorSets.append( vc );
			for ( quint32 t : d.tris )
				m.indices.append( t );
			m.materialName = d.name;
			m.diffuseSourcePath = d.diffuse;
			m.diffuseUri = tex( d.diffuse );
			m.normalUri = tex( d.normal );
			m.alphaMode = 2;
			m.materialExtras.append( { QStringLiteral( "decal" ), QString::number( decalOffset ) } );
			m.materialExtras.append( { QStringLiteral( "placedDecals" ), QString::number( d.decals ) } );
			m.materialExtras.append( { QStringLiteral( "vertexColors" ), QStringLiteral( "COLOR_0" ) } );
			m.materialExtras.append( { QStringLiteral( "vertexAlpha" ), QStringLiteral( "1" ) } );
			if ( parent < 0 )
				parent = node( QStringLiteral( "placed decals" ), -1 );
			m.node = node( m.name, parent );
			scene.meshes.append( m );
			meshesPlacedDecal++;
		}
	}
};

void usage( const QString & why )
{
	std::fprintf( stderr, "gltf-cell: %s\nusage: gltf-cell \"plugins|world|x,y|n\"|<file.wwcell> --out <abs .gltf> "
		"[--cells x,y ...] [--origin x,y,z] [--textures copy|none] [--terrain on|off] [--decal-offset units]\n", qPrintable( why ) );
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
		} else if ( a == QLatin1String( "--decal-offset" ) && parseTriple( v, &sink.decalOffset, 1 ) ) {
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
		"land_quads=%d land_layers=%d ltex_bgsm=%d ltex_unresolved=%d meshes_coloured=%d meshes_decal=%d "
		"meshes_painted=%d decal_offset=%g shapes_skipped=%d tris_placed=%lld textures=%d copied=%d missing=%d origin=%g,%g,%g "
		"meshes_clamped=%d placed_decals=%d decals_drawn=%d meshes_placed_decal=%d decal_receivers=%d\n",
		sink.placementsSeen, sink.placementsKept, sink.placementsEmpty, int( sink.meshesOf.size() ),
		int( sink.scene.meshes.size() ), int( sink.scene.nodes.size() ), sink.landQuads, sink.landLayers,
		sink.ltexFromMaterial, sink.ltexUnresolved, sink.meshesColored, sink.meshesDecal, sink.meshesPainted,
		double( sink.decalOffset ),
		sink.shapesSkipped,
		sink.trisPlaced, int( sink.textures.size() ), copied, missing, sink.origin[0], sink.origin[1], sink.origin[2],
		sink.meshesClamped, int( sink.decalResult.fates.size() ), sink.decalResult.count[int( CellDecalFate::Drawn )],
		sink.meshesPlacedDecal, int( sink.receivers.size() ) );
	if ( !sink.decalResult.fates.empty() )
		std::printf( "%s\n", qPrintable( cellDecalCensusLine( sink.decalResult ) ) );
	return 0;
}
