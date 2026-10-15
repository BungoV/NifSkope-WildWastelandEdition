/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

/* The release build is -O3 -march=haswell, where GCC fuses a*b+c into one FMA by default. Patch
 * geometry must come out bit for bit what the independent twin (tests/spells/lodp_decode.py)
 * computes, and the 45-degree crease test sits on exact ties, so this unit does plain IEEE
 * arithmetic (lane LODP1, 2026-10-15: FMA noise of 1e-17 split 5 bevelled meshes). */
#pragma GCC optimize( "fp-contract=off" )

#include "lodpplace.h"

#include <QDir>
#include <QFile>

#include <cmath>
#include <cstring>

/* The LOD PRTP known-answer fixture (lane L1). Everything here is authored
 * in this file: four small meshes, six placements across four sectors, and a
 * ground made of a formula. The answers it writes down in
 * `Synthetic.lodp.expect.txt` are derived by hand in the comments below,
 * BEFORE any run; the independent twin (tests/spells/lodp_decode.py) checks
 * them and recomputes every chart, slot and patch from the `.lodo` bytes. */

namespace {

void quad( LodoSrcShape & s, const float a[3], const float b[3], const float c[3], const float d[3] )
{
	// a b c d counter-clockwise seen from the face's front: triangles (a b c) (a c d)
	const float * v[4] = { a, b, c, d };
	quint32 base = quint32( s.pos.size() / 3 );
	float e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
	float n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
	float l = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
	for ( int k = 0; k < 4; k++ ) {
		for ( int q = 0; q < 3; q++ )
			s.pos.push_back( v[k][q] );
		for ( int q = 0; q < 3; q++ )
			s.nrm.push_back( l > 0 ? n[q] / l : ( q == 2 ? 1.0f : 0.0f ) );
		s.tan.push_back( 1.0f ); s.tan.push_back( 0.0f ); s.tan.push_back( 0.0f );
		s.uv.push_back( float( k == 1 || k == 2 ) ); s.uv.push_back( float( k >= 2 ) );
	}
	const quint32 t[6] = { 0, 1, 2, 0, 2, 3 };
	for ( quint32 i : t )
		s.tris.push_back( base + i );
}

//! An axis box [x0,x1] x [y0,y1] x [z0,z1], outward faces; `bottom` false leaves the -Z face out.
void box( LodoSrcShape & s, float x0, float y0, float z0, float x1, float y1, float z1, bool bottom )
{
	const float P[8][3] = { { x0, y0, z0 }, { x1, y0, z0 }, { x1, y1, z0 }, { x0, y1, z0 },
		{ x0, y0, z1 }, { x1, y0, z1 }, { x1, y1, z1 }, { x0, y1, z1 } };
	quad( s, P[4], P[5], P[6], P[7] );          // +Z
	if ( bottom )
		quad( s, P[0], P[3], P[2], P[1] );      // -Z
	quad( s, P[0], P[1], P[5], P[4] );          // -Y
	quad( s, P[1], P[2], P[6], P[5] );          // +X
	quad( s, P[2], P[3], P[7], P[6] );          // +Y
	quad( s, P[3], P[0], P[4], P[7] );          // -X
}

void rotZ( float deg, float r[9] )
{
	const double a = double( deg ) * 3.14159265358979323846 / 180.0;
	const float c = float( std::cos( a ) ), s = float( std::sin( a ) );
	const float m[9] = { c, -s, 0, s, c, 0, 0, 0, 1 };
	std::memcpy( r, m, sizeof m );
}

//! R = Rz(a) * Rx(b): a rotation that tips the model, so the normals' Z moves.
void rotZX( float a, float b, float r[9] )
{
	float z[9];
	rotZ( a, z );
	const double t = double( b ) * 3.14159265358979323846 / 180.0;
	const float c = float( std::cos( t ) ), s = float( std::sin( t ) );
	const float x[9] = { 1, 0, 0, 0, c, -s, 0, s, c };
	for ( int i = 0; i < 3; i++ )
		for ( int j = 0; j < 3; j++ )
			r[i * 3 + j] = z[i * 3 + 0] * x[0 * 3 + j] + z[i * 3 + 1] * x[1 * 3 + j] + z[i * 3 + 2] * x[2 * 3 + j];
}

} // namespace

// The ground formula, shared by the fixture and its twin (tests/spells/lodp_decode.py, `fixture_height`).
float lodpFixtureHeight( int gx, int gy )
{
	return float( 64 + 3 * ( gx % 37 ) + 2 * ( gy % 23 ) );
}

bool lodpFixtureWrite( const QString & dir, int red, QStringList * report, QString * error )
{
	auto fail = [&]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	QDir().mkpath( dir );
	QStringList exp;
	auto E = [&]( const QString & k, const QString & v ) { exp << QString( "expect %1 %2" ).arg( k, v ); };

	LodoLibrary lib;
	lib.worldspaceEdid = QStringLiteral( "Synthetic" );
	lib.pluginCorpusHash = Q_UINT64_C( 0x1111111111111111 );
	lib.objectCorpusHash = Q_UINT64_C( 0x2222222222222222 );
	lib.modelCorpusHash = Q_UINT64_C( 0x3333333333333333 );
	lib.loadOrderHash = Q_UINT64_C( 0x4444444444444444 );
	/* The ladder ON so a coarser level exists to take its nearest chart (the
	 * slab is the only mesh big enough to climb). */
	lib.flags |= LODO_FLAG_LADDER;
	lib.silhouetteMin = 0.0f;
	lib.addString( QString() );
	{
		LodoMaterial m0, m1;
		std::memset( &m0, 0, sizeof( m0 ) );
		std::memset( &m1, 0, sizeof( m1 ) );
		m0.layer = LODO_NO_LAYER; m0.family = LODO_FAMILY_LEGACY;
		m0.lodmStringOffset = lib.addString( QStringLiteral( "textures\\lod\\lodp_a_opaque_d.dds" ) );
		m1.layer = LODO_NO_LAYER; m1.family = LODO_FAMILY_LEGACY; m1.alphaThreshold = 128; m1.flags = LODO_MAT_TREE;
		m1.lodmStringOffset = lib.addString( QStringLiteral( "textures\\lod\\lodp_b_leaves_d.dds" ) );
		lib.materials.push_back( m0 );
		lib.materials.push_back( m1 );
	}
	quint16 mBox = 0, mSlab = 0, mTree = 0, mCrease = 0;
	QString err;
	{
		/* a_box: 400 x 200 x 300, six faces = SIX charts (every pair of faces
		 * meets at 90 degrees, past the 45-degree crease). */
		LodoSrcShape s;
		box( s, -200, -100, 0, 200, 100, 300, true );
		if ( !lodoAppendMesh( lib, { s }, QStringLiteral( "meshes\\lod\\lodp\\a_box_lod.nif" ), &mBox, &err ) )
			return fail( err );
		/* b_slab: a 1024 x 1024 flat plane in 16 x 16 quads (512 triangles):
		 * ONE chart, and big enough for the ladder to build coarser levels,
		 * whose triangles all take chart 0. */
		LodoSrcShape p;
		for ( int j = 0; j < 16; j++ )
			for ( int i = 0; i < 16; i++ ) {
				const float a[3] = { float( i * 64 ), float( j * 64 ), 0 }, b[3] = { float( ( i + 1 ) * 64 ), float( j * 64 ), 0 };
				const float c[3] = { float( ( i + 1 ) * 64 ), float( ( j + 1 ) * 64 ), 0 }, d[3] = { float( i * 64 ), float( ( j + 1 ) * 64 ), 0 };
				quad( p, a, b, c, d );
			}
		if ( !lodoAppendMesh( lib, { p }, QStringLiteral( "meshes\\lod\\lodp\\b_slab_lod.nif" ), &mSlab, &err ) )
			return fail( err );
		/* c_tree: a 40 x 40 x 300 trunk with no bottom (FIVE surface charts)
		 * and two crossed 400 x 400 leaf cards on the tree material (the ONE
		 * tree chart; foliage area 2 x 160,000 x 2 sides? no: each card is one
		 * quad, two triangles, so 2 x 160,000 = 320,000 square units). */
		LodoSrcShape trunk, leaves;
		box( trunk, -20, -20, 0, 20, 20, 300, false );
		trunk.materialId = 0;
		const float c1[4][3] = { { -200, 0, 200 }, { 200, 0, 200 }, { 200, 0, 600 }, { -200, 0, 600 } };
		const float c2[4][3] = { { 0, -200, 200 }, { 0, 200, 200 }, { 0, 200, 600 }, { 0, -200, 600 } };
		quad( leaves, c1[0], c1[1], c1[2], c1[3] );
		quad( leaves, c2[0], c2[1], c2[2], c2[3] );
		leaves.materialId = 1;
		if ( !lodoAppendMesh( lib, { trunk, leaves }, QStringLiteral( "meshes\\lod\\lodp\\c_tree_lod.nif" ), &mTree, &err ) )
			return fail( err );
		/* d_crease: three 200 x 200 panels side by side, each its own quad --
		 * flat (seed), tipped 44 degrees about Y, tipped 46 degrees -- and one
		 * DEGENERATE triangle (three points on a line). The 44-degree panel
		 * joins the seed's chart (cos 44 = 0.719 >= cos 45), the 46-degree one
		 * seeds a second (cos 46 = 0.695): TWO charts, one NONE triangle. */
		LodoSrcShape k;
		auto tipped = [&]( float x0, float deg ) {
			const double t = double( deg ) * 3.14159265358979323846 / 180.0;
			const float cx = float( 200.0 * std::cos( t ) ), cz = float( 200.0 * std::sin( t ) );
			const float a[3] = { x0, 0, 0 }, b[3] = { x0 + cx, 0, cz }, c[3] = { x0 + cx, 200, cz }, d[3] = { x0, 200, 0 };
			quad( k, a, b, c, d );
		};
		tipped( 0, 0.0f );
		tipped( 600, 44.0f );
		tipped( 1200, 46.0f );
		{
			quint32 base = quint32( k.pos.size() / 3 );
			const float L[3][3] = { { 0, 400, 0 }, { 100, 400, 0 }, { 200, 400, 0 } };
			for ( auto & v : L ) {
				for ( int q = 0; q < 3; q++ )
					k.pos.push_back( v[q] );
				k.nrm.push_back( 0 ); k.nrm.push_back( 0 ); k.nrm.push_back( 1 );
				k.tan.push_back( 1 ); k.tan.push_back( 0 ); k.tan.push_back( 0 );
				k.uv.push_back( 0 ); k.uv.push_back( 0 );
			}
			k.tris.push_back( base ); k.tris.push_back( base + 1 ); k.tris.push_back( base + 2 );
		}
		if ( !lodoAppendMesh( lib, { k }, QStringLiteral( "meshes\\lod\\lodp\\d_crease_lod.nif" ), &mCrease, &err ) )
			return fail( err );
	}
	{
		const quint16 reps[4] = { mBox, mSlab, mTree, mCrease };
		for ( int i = 0; i < 4; i++ ) {
			LodoBase b;
			std::memset( &b, 0, sizeof b );
			b.formId = 0x00020001 + quint32( i );
			b.flags = LODO_BASE_ANY_MESH | ( i == 2 ? LODO_BASE_TREE | LODO_BASE_ANY_ALPHA : 0 );
			b.modelStringOffset = lib.addString( QString( "meshes\\lodp\\%1.nif" ).arg( i ) );
			b.rep[0] = reps[i];
			b.rep[1] = b.rep[2] = b.rep[3] = LODO_NO_MESH;
			b.cardLayer = LODO_NO_CARD;
			b.boundRadius = 1000.0f;
			const LodoMesh & bm = lib.meshes[reps[i]];
			for ( quint32 c = bm.clusterFirst; c < bm.clusterFirst + bm.clusterCount; c++ )
				if ( lib.clusterLods[c].level == 0 )
					b.fullTriangles += lib.clusters[c].triangleCount;
			lib.bases.push_back( b );
		}
	}
	std::vector<quint8> meshCharts;
	if ( !lodpChartStream( lib, &lib.chartStream, &meshCharts, &err ) )
		return fail( err );
	LodoHeader lh;
	if ( !lodoWrite( dir + QStringLiteral( "/Synthetic.lodo" ), lib, &lh, &err ) )
		return fail( err );
	E( "lodo.version", QString::number( LODO_VERSION_CHARTS ) );
	E( "charts.mesh0", "6" );       // the box
	E( "charts.mesh1", "1" );       // the slab
	E( "charts.mesh2", "5+tree" );  // the trunk's five faces and the foliage
	E( "charts.mesh3", "2+none1" ); // flat with 44, then 46; one degenerate
	E( "charts.mesh1.coarse", "0" );  // every slab triangle above level 0 is in chart 0

	/* The placements: (lodiIndex, mesh, position, rotation, scale). Cell =
	 * floor(pos / 4096); sector = floor(cell / 16). */
	std::vector<LodpPlacementIn> pl;
	auto add = [&]( quint32 idx, quint32 mesh, float x, float y, float z, const float r[9], float sc ) {
		LodpPlacementIn in;
		in.lodiIndex = idx;
		in.mesh = mesh;
		in.pos[0] = x; in.pos[1] = y; in.pos[2] = z;
		std::memcpy( in.rot, r, sizeof in.rot );
		in.scale = sc;
		pl.push_back( in );
	};
	float I[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 }, R30[9], R90[9], RT[9];
	rotZ( 30, R30 );
	rotZ( 90, R90 );
	rotZX( 25, 60, RT );
	add( 3, mBox, 100, 100, 50, I, 1.0f );                       // cell (0,0)   sector (0,0)
	add( 5, mBox, 15 * 4096 + 4000, 2000, 80, R30, 2.0f );       // cell (15,0)  sector (0,0), reaches into cell 16
	add( 7, mTree, 16 * 4096 + 500, -300, 10, I, 1.5f );        // cell (16,-1) sector (1,-1)
	add( 0, mSlab, -2000, -2000, 0, R90, 1.0f );                // cell (-1,-1) sector (-1,-1)
	add( 1, mCrease, 300, 3000, 20, I, 1.0f );                  // cell (0,0)   sector (0,0)
	add( 2, mBox, 2000, 1000, 120, RT, 1.0f );                   // cell (0,0)   sector (0,0), tipped
	/* Box at scale 2, step 256 mesh units: +-Z 400 x 200 -> 2 x 1, +-Y 400 x
	 * 300 -> 2 x 2, +-X 200 x 300 -> 1 x 2: 2+2+4+4+2+2 = 16 patches. At scale
	 * 1 (step 512) every face is one slot: 6. Slab at scale 1: 1024 / 512 =
	 * 2 x 2 = 4. Trunk at 1.5 (step 341.3): five faces of at most 300 -> 5, plus
	 * the tree patch. */
	E( "patches.instance3", "6" );
	E( "patches.instance5", "16" );
	E( "patches.instance0", "4" );
	E( "patches.instance7", "6" );
	E( "patches.instance2", "6" );
	E( "area.instance5", "4*2*(400*200+400*300+200*300)" );  // six faces, 2,080,000 square units at scale 2
	E( "area.instance7.tree", "2.25*320000" );

	/* The ground: cells x -1..16, y -1..0 (36), 16 samples a cell, height =
	 * 64 + 3 (gx mod 37) + 2 (gy mod 23). Land everywhere but (16, 0); cell
	 * (0, 0) is under water at 1,000 (above every height, 216 at most): all 64
	 * of its patches SUBMERGED; cell (-1, 0) has water at 150, partly. */
	LodpTerrainIn terr;
	terr.minX = -1; terr.maxX = 16; terr.minY = -1; terr.maxY = 0;
	terr.samplesPerCell = 16;
	terr.height = []( int gx, int gy ) { return lodpFixtureHeight( gx, gy ); };
	terr.cell = []( int cx, int cy, bool * land, bool * water, float * wh ) {
		*land = !( cx == 16 && cy == 0 );
		*water = ( cx == 0 && cy == 0 ) || ( cx == -1 && cy == 0 );
		*wh = ( cx == 0 && cy == 0 ) ? 1000.0f : 150.0f;
	};
	E( "terrain.landCells", "35" );
	E( "terrain.patches", QString::number( 35 * 64 ) );
	E( "terrain.submerged.cell0_0", "64" );
	E( "sectors", "-1_-1,-1_0,0_-1,0_0,1_-1" );   // terrain reaches y = 0 in sector -1 x; (1, 0) holds only the no-land cell

	LodpPlaceOptions opt;
	opt.worldspaceHash = Q_UINT64_C( 0x5555555555555555 );
	opt.lodInputHash = Q_UINT64_C( 0x6666666666666666 );
	opt.lodoIdentity = lodoIdentityOf( lh.headerCrc32, lh.modelCorpusHash, lh.objectCorpusHash );
	opt.bound = true;
	opt.red = red;
	std::map<std::pair<int, int>, LodpSector> sectors;
	LodpPlaceStats st;
	if ( !lodpPlace( lib, pl, &terr, opt, &sectors, &st, &err ) )
		return fail( err );
	for ( auto & kv : sectors ) {
		LodpHeader h;
		if ( !lodpWrite( dir + "/" + lodpFileName( kv.first.first, kv.first.second ), kv.second, &h, &err ) )
			return fail( err );
	}
	{
		// the placements as the twin reads them (the fixture has no .lodi)
		QFile f( dir + QStringLiteral( "/placements.tsv" ) );
		if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
			return fail( "cannot write placements.tsv" );
		QByteArray t = "# lodiIndex mesh x y z r0..r8 scale (floats as hex bits)\n";
		for ( const LodpPlacementIn & in : pl ) {
			auto hx = []( float v ) { quint32 b; std::memcpy( &b, &v, 4 ); return QByteArray::number( b, 16 ); };
			t += QByteArray::number( in.lodiIndex ) + "\t" + QByteArray::number( in.mesh );
			for ( float v : in.pos )
				t += "\t" + hx( v );
			for ( float v : in.rot )
				t += "\t" + hx( v );
			t += "\t" + hx( in.scale ) + "\n";
		}
		f.write( t );
	}
	{
		QFile f( dir + QStringLiteral( "/Synthetic.lodp.expect.txt" ) );
		if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
			return fail( "cannot write the expect file" );
		f.write( ( exp.join( "\n" ) + "\n" ).toUtf8() );
	}
	if ( report )
		*report << QString( "lodp-fixture: %1 sectors, %2 instances placed, %3 charts (%4 tree), %5 object + %6 tree + %7 terrain patches, "
			"%8 submerged, %9 centroids moved, %10 bytes" )
			.arg( st.sectors ).arg( st.placedObjects ).arg( st.charts ).arg( st.treeCharts ).arg( st.objectPatches )
			.arg( st.treePatches ).arg( st.terrainPatches ).arg( st.submerged ).arg( st.centroidMoved ).arg( st.bytes );
	return true;
}
