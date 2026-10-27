#include "cellwaterinfo.h"
#include "wwswitch.h"

#include "lodtfile.h"
#include "gl/cellunits.h"
#include "gl/glscene.h"
#include "gl/renderer.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QOpenGLContext>
#include <QTextStream>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <thread>

quint64 WwWaterInfoTables::bytes() const
{
	return quint64( index.size() + atlas.size() ) * 4 + quint64( bodies.size() ) * 4;
}

quint64 WwWaterInfoTables::landBytes() const
{
	return quint64( landIndex.size() ) * 4 + quint64( landAtlas.size() );
}

quint64 WwWaterInfoTables::flowBytes() const
{
	return quint64( flowIndex.size() + flowAtlas.size() ) * 4;
}

quint64 WwWaterInfoTables::skyBytes() const
{
	return quint64( skyIndex.size() ) * 4 + quint64( skyAtlas.size() );
}

uint32_t wwWaterFlowPack( double vx, double vy )
{
	auto q = []( double v ) {
		const double c = std::clamp( std::floor( v * WwWaterInfoTables::kFlowScale + 0.5 ), -32767.0, 32767.0 );
		return uint32_t( uint16_t( int16_t( c ) ) );
	};
	return q( vx ) | ( q( vy ) << 16 );
}

QString wwWaterFlowCensus( const WwWaterInfoTables & t )
{
	if ( !t.flowTilesX )
		return QStringLiteral( "water flow: none (the file has no flow plane)" );
	return QStringLiteral( "water flow: %1 x %2 tiles of %3 texels (%4 u a texel); uniform still %5 moving %6; "
		"sparse %7; atlas %8 x %9; moving texels %10, solved %11; bytes index %12 atlas %13 total %14 (%15 MB)" )
		.arg( t.flowTilesX ).arg( t.flowTilesY ).arg( WwWaterInfoTables::kTileEdge ).arg( double( t.flowTexel ) )
		.arg( t.flowUniformStill ).arg( t.flowUniformMoving ).arg( t.flowSlotCount ).arg( t.flowAtlasW )
		.arg( t.flowAtlasH ).arg( t.flowMovingTexels ).arg( t.flowSolvedTexels )
		.arg( quint64( t.flowIndex.size() ) * 4 ).arg( quint64( t.flowAtlas.size() ) * 4 ).arg( t.flowBytes() )
		.arg( double( t.flowBytes() ) / 1048576.0, 0, 'f', 2 );
}

namespace
{

/*! Lane WATERFLOWBAKE1: the flow tables, after the info tables (which it leaves as they were). The flow plane's
 *  tiles must be a multiple of E samples; otherwise (and with no flow plane) the flow tables stay empty. */
void wwWaterFlowBuild( const LodtFile & f, WwWaterInfoTables & t )
{
	const int FS = f.flowPlaneSamples(), S = f.bodyIdSamples(), E = WwWaterInfoTables::kTileEdge;
	const int fe = f.waterTileEdge( 3 );
	if ( FS <= 0 || fe <= 0 || fe % E )
		return;
	const int W = f.cellsX() * FS, H = f.cellsY() * FS;
	if ( W % fe || H % fe )
		return;
	const int n = f.bodyCount();
	std::vector<double> mx( size_t( n ) + 1, 0.0 ), my( size_t( n ) + 1, 0.0 ), mm( size_t( n ) + 1, 0.0 );
	for ( int k = 1; k <= n; k++ ) {
		LodtWaterBody B;
		if ( !f.waterBody( k, B ) )
			continue;
		mx[k] = B.flowX;
		my[k] = B.flowY;
		mm[k] = std::sqrt( double( B.flowX ) * B.flowX + double( B.flowY ) * B.flowY );
	}
	t.flowTilesX = W / E;
	t.flowTilesY = H / E;
	t.flowTexel = 4096.0f / float( FS );
	t.flowIndex.assign( size_t( t.flowTilesX ) * t.flowTilesY * 2, 0u );
	const int ftX = W / fe, ftY = H / fe, fn = fe * fe;
	std::vector<uint32_t> word( fn );
	std::vector<int> sparseTile;
	std::vector<uint32_t> sparseWords;
	const double kTwoPi = 6.283185307179586;
	for ( int fy = 0; fy < ftY; fy++ ) {
		for ( int fx = 0; fx < ftX; fx++ ) {
			quint32 u = 0;
			const bool uni = f.waterTileUniform( 3, fx, fy, u ) && u == 0;
			for ( int y = 0; y < fe; y++ )
				for ( int x = 0; x < fe; x++ ) {
					uint32_t v = 0;
					const int px = fx * fe + x, py = fy * fe + y;
					const quint16 w = uni ? quint16( 0 ) : f.flowWordAt( px, py );
					if ( w ) {
						const int conf = ( w >> 12 ) & 0xF;
						const quint16 id = f.bodyIdAt( int( qint64( px ) * S / FS ), int( qint64( py ) * S / FS ) );
						const int b = id <= quint16( n ) ? int( id ) : 0;
						double vx, vy;
						if ( conf == 0 ) {
							vx = mx[b];
							vy = my[b];
						} else {
							const double a = double( w & 0xFF ) / 256.0 * kTwoPi;
							const double s = double( ( w >> 8 ) & 0xF ) * mm[b] / 8.0;
							vx = std::cos( a ) * s;
							vy = std::sin( a ) * s;
							if ( conf == kLodlFlowSolvedConfidence )
								t.flowSolvedTexels++;
						}
						v = wwWaterFlowPack( vx, vy );
					}
					if ( v )
						t.flowMovingTexels++;
					word[y * fe + x] = v;
				}
			for ( int sy = 0; sy < fe / E; sy++ )
				for ( int sx = 0; sx < fe / E; sx++ ) {
					const int o = sy * E * fe + sx * E;
					bool same = true;
					for ( int y = 0; y < E && same; y++ )
						for ( int x = 0; x < E; x++ )
							if ( word[o + y * fe + x] != word[o] ) {
								same = false;
								break;
							}
					const int ti = ( fy * ( fe / E ) + sy ) * t.flowTilesX + fx * ( fe / E ) + sx;
					if ( same ) {
						t.flowIndex[size_t( ti ) * 2 + 1] = word[o];
						( word[o] ? t.flowUniformMoving : t.flowUniformStill )++;
						continue;
					}
					sparseTile.push_back( ti );
					for ( int y = 0; y < E; y++ )
						sparseWords.insert( sparseWords.end(), word.begin() + o + y * fe, word.begin() + o + y * fe + E );
				}
		}
	}
	std::vector<int> order( sparseTile.size() );
	for ( size_t k = 0; k < order.size(); k++ )
		order[k] = int( k );
	std::sort( order.begin(), order.end(), [&]( int a, int b ) { return sparseTile[a] < sparseTile[b]; } );
	t.flowSlotCount = int( order.size() );
	const int R = WwWaterInfoTables::kTilesPerRow;
	t.flowAtlasW = R * E;
	t.flowAtlasH = ( t.flowSlotCount + R - 1 ) / R * E;
	t.flowAtlas.assign( size_t( t.flowAtlasW ) * t.flowAtlasH, 0u );
	for ( int s = 0; s < t.flowSlotCount; s++ ) {
		const int k = order[s];
		t.flowIndex[size_t( sparseTile[k] ) * 2] = uint32_t( s + 1 );
		const int ax = ( s % R ) * E, ay = ( s / R ) * E;
		for ( int y = 0; y < E; y++ )
			std::copy_n( sparseWords.begin() + ( size_t( k ) * E + y ) * E, E,
				t.flowAtlas.begin() + size_t( ay + y ) * t.flowAtlasW + ax );
	}
}

/*! Lane WATERSKYVISFAR1: the sky tables from the version-4 sky plane, the land side's layout. Needs the body
 *  plane's rate and tiles a multiple of E; otherwise (and with no sky plane) haveSky stays false. */
void wwWaterSkyBuild( const LodtFile & f, WwWaterInfoTables & t )
{
	const int S = f.bodyIdSamples(), E = WwWaterInfoTables::kTileEdge, fe = f.waterTileEdge( 4 );
	if ( S <= 0 || f.skyPlaneSamples() != S || fe <= 0 || fe % E )
		return;
	const int W = f.cellsX() * S, H = f.cellsY() * S;
	if ( W % fe || H % fe || W / E != t.tilesX || H / E != t.tilesY )
		return;
	t.haveSky = true;
	t.skyIndex.assign( size_t( t.tilesX ) * t.tilesY, 0u );
	std::vector<uint8_t> b( size_t( fe ) * fe );
	std::vector<int> tiles;
	std::vector<uint8_t> words;
	for ( int fy = 0; fy < H / fe; fy++ ) {
		for ( int fx = 0; fx < W / fe; fx++ ) {
			quint32 u = 0;
			if ( f.waterTileUniform( 4, fx, fy, u ) )
				std::fill( b.begin(), b.end(), uint8_t( u ) );
			else
				for ( int y = 0; y < fe; y++ )
					for ( int x = 0; x < fe; x++ )
						b[size_t( y ) * fe + x] = f.skyAt( fx * fe + x, fy * fe + y );
			for ( uint8_t v : b ) {
				t.skyWetTexels += v ? 1 : 0;
				t.skyBelowOpen += v && v < 255 ? 1 : 0;
			}
			for ( int sy = 0; sy < fe / E; sy++ )
				for ( int sx = 0; sx < fe / E; sx++ ) {
					const int o = sy * E * fe + sx * E;
					const int ti = ( fy * ( fe / E ) + sy ) * t.tilesX + fx * ( fe / E ) + sx;
					bool same = true;
					for ( int y = 0; y < E && same; y++ )
						for ( int x = 0; x < E; x++ )
							same = same && b[o + y * fe + x] == b[o];
					if ( same ) {
						t.skyIndex[size_t( ti )] = b[o];
						( b[o] == 255 ? t.skyUniformOpen : b[o] == 0 ? t.skyUniformDry : t.skyUniformOther )++;
						continue;
					}
					tiles.push_back( ti );
					for ( int y = 0; y < E; y++ )
						words.insert( words.end(), b.begin() + o + y * fe, b.begin() + o + y * fe + E );
				}
		}
	}
	std::vector<int> order( tiles.size() );
	for ( size_t k = 0; k < order.size(); k++ )
		order[k] = int( k );
	std::sort( order.begin(), order.end(), [&]( int a, int c ) { return tiles[a] < tiles[c]; } );
	const int R = WwWaterInfoTables::kTilesPerRow;
	t.skySlots = int( order.size() );
	t.skyAtlasH = ( t.skySlots + R - 1 ) / R * E;
	t.skyAtlas.assign( size_t( t.atlasW ) * t.skyAtlasH, uint8_t( 0 ) );
	for ( int s = 0; s < t.skySlots; s++ ) {
		const int k = order[s];
		t.skyIndex[size_t( tiles[k] )] = uint32_t( s + 1 ) << 8;
		const int ax = ( s % R ) * E, ay = ( s / R ) * E;
		for ( int y = 0; y < E; y++ )
			std::copy_n( words.begin() + ( size_t( k ) * E + y ) * E, E, t.skyAtlas.begin() + size_t( ay + y ) * t.atlasW + ax );
	}
}

} // namespace

uint32_t wwWaterInfoDepthCode( double depth )
{
	return uint32_t( std::floor( std::sqrt( std::clamp( depth, 0.0, WwWaterInfoTables::kDepthMax )
		/ WwWaterInfoTables::kDepthMax ) * 255.0 + 0.5 ) );
}

bool wwWaterInfoBuild( const QString & lodl, WwWaterInfoTables & t, QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	QElapsedTimer clk;
	clk.start();
	t = WwWaterInfoTables();
	t.path = lodl;
	LodtFile f;
	QString err;
	if ( !f.open( lodl, &err ) )
		return fail( err );
	f.setBlockCacheSize( 64 );	// a tile's heights touch every pyramid level: 1 block thrashed (172 s)
	const int S = f.bodyIdSamples(), E = WwWaterInfoTables::kTileEdge;
	if ( !f.bodyCount() || S <= 0 || f.shorePlaneSamples() <= 0 )
		return fail( QStringLiteral( "no water bodies or shore plane in the file" ) );
	if ( f.shorePlaneSamples() != S || ( f.surfacePlaneSamples() && f.surfacePlaneSamples() != S ) )
		return fail( QStringLiteral( "the body, shore and surface planes are at different rates" ) );
	const int fe = f.waterTileEdge( 0 );
	const int W = f.cellsX() * S, H = f.cellsY() * S;
	if ( fe <= 0 || fe % E || f.waterTileEdge( 1 ) != fe || ( f.waterTileEdge( 2 ) && f.waterTileEdge( 2 ) != fe )
		|| W % fe || H % fe )
		return fail( QStringLiteral( "the water planes' tiles are not a multiple of %1 samples" ).arg( E ) );
	const int spc = f.samplesPerCell();
	t.tilesX = W / E;
	t.tilesY = H / E;
	t.originX = float( f.cellMinX() ) * 4096.0f;
	t.originY = float( f.cellMinY() ) * 4096.0f;
	t.texel = 4096.0f / float( S );
	t.shoreQuantum = f.shoreQuantum();

	const int n = f.bodyCount();
	std::vector<float> bh( size_t( n ) + 1, 0.0f );
	t.bodyCount = n;
	t.bodyRows = ( n + 1 + WwWaterInfoTables::kBodiesPerRow - 1 ) / WwWaterInfoTables::kBodiesPerRow;
	t.bodies.assign( size_t( t.bodyRows ) * WwWaterInfoTables::kBodiesPerRow * 4, 0.0f );
	t.bodies[1] = -1.0f;	// body 0: none, class -1
	for ( int k = 1; k <= n; k++ ) {
		LodtWaterBody B;
		if ( !f.waterBody( k, B ) )
			continue;
		bh[k] = B.waterHeight;
		float * o = &t.bodies[size_t( k ) * 4];
		o[0] = B.waterHeight;
		o[1] = float( B.cls );
		o[2] = float( B.flags );
		o[3] = float( B.area );
	}

	t.index.assign( size_t( t.tilesX ) * t.tilesY * 2, 0u );
	std::vector<int> sparseTile;	// index tile of each kept sub-tile, in the order found
	std::vector<uint32_t> sparseWords;
	const int ftX = W / fe, ftY = H / fe, fn = fe * fe;
	std::vector<uint32_t> id( fn ), sh( fn ), sf( fn ), word( fn ), lb( fn );
	std::vector<double> depth( fn );
	t.landIndex.assign( size_t( t.tilesX ) * t.tilesY, 0u );
	std::vector<int> landTile;
	std::vector<uint8_t> landWords;
	auto fill = [&]( int plane, int fx, int fy, std::vector<uint32_t> & out ) {
		quint32 v = 0;
		if ( f.waterTileUniform( plane, fx, fy, v ) ) {
			std::fill( out.begin(), out.end(), v );
			return;
		}
		for ( int y = 0; y < fe; y++ )
			for ( int x = 0; x < fe; x++ ) {
				const int bx = fx * fe + x, by = fy * fe + y;
				if ( plane == 0 ) {
					out[y * fe + x] = f.bodyIdAt( bx, by );
				} else if ( plane == 1 ) {
					out[y * fe + x] = f.shoreAt( bx, by );
				} else {
					const float d = f.surfaceDeltaAt( bx, by );
					uint32_t b;
					std::memcpy( &b, &d, 4 );
					out[y * fe + x] = b;
				}
			}
	};
	for ( int fy = 0; fy < ftY; fy++ ) {
		for ( int fx = 0; fx < ftX; fx++ ) {
			fill( 0, fx, fy, id );
			fill( 1, fx, fy, sh );
			fill( 2, fx, fy, sf );
			/* the land side out of the shore plane's dry texels; the water words keep 255 there, as a file
			 * without the land side has it */
			for ( int k = 0; k < fn; k++ ) {
				lb[k] = id[k] ? 0u : sh[k];
				if ( !id[k] ) {
					t.landNearTexels += sh[k] < 255u ? 1 : 0;
					sh[k] = 255u;
				}
			}
			for ( int y = 0; y < fe; y++ )
				for ( int x = 0; x < fe; x++ ) {
					const int k = y * fe + x;
					double d = 0.0;
					uint32_t code = 0;
					if ( id[k] ) {
						float sd;
						std::memcpy( &sd, &sf[k], 4 );
						const int gx = int( qint64( fx * fe + x ) * spc / S ), gy = int( qint64( fy * fe + y ) * spc / S );
						d = double( id[k] <= uint32_t( n ) ? bh[id[k]] : 0.0f ) + double( sd ) - double( f.height( gx, gy ) );
						code = wwWaterInfoDepthCode( d );
					}
					depth[k] = d;
					word[k] = id[k] | ( sh[k] << 16 ) | ( code << 24 );
				}
			for ( int sy = 0; sy < fe / E; sy++ )
				for ( int sx = 0; sx < fe / E; sx++ ) {
					const int o = sy * E * fe + sx * E;
					const int ti = ( fy * ( fe / E ) + sy ) * t.tilesX + fx * ( fe / E ) + sx;
					bool lbU = true;
					for ( int y = 0; y < E && lbU; y++ )
						for ( int x = 0; x < E; x++ )
							lbU = lbU && lb[o + y * fe + x] == lb[o];
					if ( lbU ) {
						t.landIndex[size_t( ti )] = lb[o];
						( lb[o] == 255u ? t.landUniformFar : lb[o] == 0u ? t.landUniformWet : t.landUniformOther )++;
					} else {
						landTile.push_back( ti );
						for ( int y = 0; y < E; y++ )
							for ( int x = 0; x < E; x++ )
								landWords.push_back( uint8_t( lb[o + y * fe + x] ) );
					}
					bool idU = true, shU = true, sfU = true;
					double lo = depth[o], hi = depth[o], acc = 0.0;
					for ( int y = 0; y < E; y++ )
						for ( int x = 0; x < E; x++ ) {
							const int k = o + y * fe + x;
							idU = idU && id[k] == id[o];
							shU = shU && sh[k] == sh[o];
							sfU = sfU && sf[k] == sf[o];
							lo = std::min( lo, depth[k] );
							hi = std::max( hi, depth[k] );
							acc += depth[k];	// row-major: the reference's order
						}
					const bool dry = idU && id[o] == 0;
					if ( dry && shU ) {
						t.index[size_t( ti ) * 2 + 1] = sh[o] << 16;
						t.uniformDry++;
						continue;
					}
					if ( idU && !dry && shU && sfU && hi - lo <= WwWaterInfoTables::kSpread ) {
						t.index[size_t( ti ) * 2 + 1] = id[o] | ( sh[o] << 16 )
							| ( wwWaterInfoDepthCode( acc / double( E * E ) ) << 24 );
						t.uniformWet++;
						continue;
					}
					if ( !idU )
						t.sparseBody++;
					else if ( !shU )
						t.sparseShore++;
					else if ( !sfU )
						t.sparseSurface++;
					else
						t.sparseDepth++;
					sparseTile.push_back( ti );
					for ( int y = 0; y < E; y++ )
						sparseWords.insert( sparseWords.end(), word.begin() + o + y * fe, word.begin() + o + y * fe + E );
				}
		}
	}
	// slots in row-major tile order, 64 tiles an atlas row
	std::vector<int> order( sparseTile.size() );
	for ( size_t k = 0; k < order.size(); k++ )
		order[k] = int( k );
	std::sort( order.begin(), order.end(), [&]( int a, int b ) { return sparseTile[a] < sparseTile[b]; } );
	t.slotCount = int( order.size() );
	const int R = WwWaterInfoTables::kTilesPerRow;
	t.atlasW = R * E;
	t.atlasH = ( t.slotCount + R - 1 ) / R * E;
	t.atlas.assign( size_t( t.atlasW ) * t.atlasH, 0u );
	for ( int s = 0; s < t.slotCount; s++ ) {
		const int k = order[s];
		t.index[size_t( sparseTile[k] ) * 2] = uint32_t( s + 1 );
		const int ax = ( s % R ) * E, ay = ( s / R ) * E;
		for ( int y = 0; y < E; y++ )
			std::copy_n( sparseWords.begin() + ( size_t( k ) * E + y ) * E, E,
				t.atlas.begin() + size_t( ay + y ) * t.atlasW + ax );
	}
	// the land side's slots, the same way (row-major tile order, 64 tiles an atlas row, atlasW wide)
	std::vector<int> lorder( landTile.size() );
	for ( size_t k = 0; k < lorder.size(); k++ )
		lorder[k] = int( k );
	std::sort( lorder.begin(), lorder.end(), [&]( int a, int b ) { return landTile[a] < landTile[b]; } );
	t.landSlots = int( lorder.size() );
	t.landAtlasH = ( t.landSlots + R - 1 ) / R * E;
	t.landAtlas.assign( size_t( t.atlasW ) * t.landAtlasH, uint8_t( 255 ) );
	for ( int s = 0; s < t.landSlots; s++ ) {
		const int k = lorder[s];
		t.landIndex[size_t( landTile[k] )] = uint32_t( s + 1 ) << 8;
		const int ax = ( s % R ) * E, ay = ( s / R ) * E;
		for ( int y = 0; y < E; y++ )
			std::copy_n( landWords.begin() + ( size_t( k ) * E + y ) * E, E,
				t.landAtlas.begin() + size_t( ay + y ) * t.atlasW + ax );
	}
	wwWaterFlowBuild( f, t );	// lane WATERFLOWBAKE1
	wwWaterSkyBuild( f, t );	// lane WATERSKYVISFAR1
	t.buildSeconds = clk.elapsed() / 1000.0;
	return true;
}

QString wwWaterInfoCensus( const WwWaterInfoTables & t )
{
	return QStringLiteral( "water info: %1 x %2 tiles of %3 texels (%4 u a texel); uniform dry %5 wet %6; "
		"sparse %7 (body %8 shore %9 surface %10 depth %11); atlas %12 x %13; bodies %14; "
		"bytes index %15 atlas %16 bodies %17 total %18 (%19 MB); built in %20 s" )
		.arg( t.tilesX ).arg( t.tilesY ).arg( WwWaterInfoTables::kTileEdge ).arg( double( t.texel ) )
		.arg( t.uniformDry ).arg( t.uniformWet ).arg( t.slotCount ).arg( t.sparseBody ).arg( t.sparseShore )
		.arg( t.sparseSurface ).arg( t.sparseDepth ).arg( t.atlasW ).arg( t.atlasH ).arg( t.bodyCount )
		.arg( quint64( t.index.size() ) * 4 ).arg( quint64( t.atlas.size() ) * 4 ).arg( quint64( t.bodies.size() ) * 4 )
		.arg( t.bytes() ).arg( double( t.bytes() ) / 1048576.0, 0, 'f', 2 ).arg( t.buildSeconds, 0, 'f', 2 );
}

QString wwLandShoreCensus( const WwWaterInfoTables & t )
{
	return QStringLiteral( "land shore: %1 x %2 tiles; uniform far %3 wet %4 other %5; sparse %6; atlas %7 x %8; "
		"%9 dry texel(s) near water; bytes index %10 atlas %11 total %12 (%13 MB)" )
		.arg( t.tilesX ).arg( t.tilesY ).arg( t.landUniformFar ).arg( t.landUniformWet ).arg( t.landUniformOther )
		.arg( t.landSlots ).arg( t.atlasW ).arg( t.landAtlasH ).arg( t.landNearTexels )
		.arg( quint64( t.landIndex.size() ) * 4 ).arg( quint64( t.landAtlas.size() ) ).arg( t.landBytes() )
		.arg( double( t.landBytes() ) / 1048576.0, 0, 'f', 2 );
}

QString wwWaterSkyCensus( const WwWaterInfoTables & t )
{
	if ( !t.haveSky )
		return QStringLiteral( "water sky: none (no sky plane in the file: the far water reads 1)" );
	return QStringLiteral( "water sky: %1 x %2 tiles; uniform open %3 dry %4 other %5; sparse %6; atlas %7 x %8; "
		"%9 wet texel(s), %10 below open sky; bytes index %11 atlas %12 total %13 (%14 MB)" )
		.arg( t.tilesX ).arg( t.tilesY ).arg( t.skyUniformOpen ).arg( t.skyUniformDry ).arg( t.skyUniformOther )
		.arg( t.skySlots ).arg( t.atlasW ).arg( t.skyAtlasH ).arg( t.skyWetTexels ).arg( t.skyBelowOpen )
		.arg( quint64( t.skyIndex.size() ) * 4 ).arg( quint64( t.skyAtlas.size() ) ).arg( t.skyBytes() )
		.arg( double( t.skyBytes() ) / 1048576.0, 0, 'f', 2 );
}

bool wwWaterInfoDump( const WwWaterInfoTables & t, const QString & dir, QString * error )
{
	QDir().mkpath( dir );
	auto put = [&]( const QString & name, const void * p, qint64 bytes ) {
		QFile o( QDir( dir ).filePath( name ) );
		if ( !o.open( QIODevice::WriteOnly ) || o.write( static_cast<const char *>( p ), bytes ) != bytes ) {
			if ( error )
				*error = QStringLiteral( "cannot write %1" ).arg( o.fileName() );
			return false;
		}
		return true;
	};
	const QByteArray census = ( wwWaterInfoCensus( t ) + QStringLiteral( "\n" ) + wwWaterFlowCensus( t )
		+ QStringLiteral( "\ngrid origin %1 %2 texel %3 shore quantum %4 tiles %5 %6 edge %7 per row %8 body rows %9\n" )
			.arg( double( t.originX ) ).arg( double( t.originY ) ).arg( double( t.texel ) ).arg( double( t.shoreQuantum ) )
			.arg( t.tilesX ).arg( t.tilesY ).arg( WwWaterInfoTables::kTileEdge ).arg( WwWaterInfoTables::kTilesPerRow )
			.arg( t.bodyRows )
		+ QStringLiteral( "flow tiles %1 %2 texel %3 scale %4\n" ).arg( t.flowTilesX ).arg( t.flowTilesY )
			.arg( double( t.flowTexel ) ).arg( WwWaterInfoTables::kFlowScale ) ).toUtf8();
	const QByteArray landCensus = ( wwLandShoreCensus( t ) + QStringLiteral( "\n" ) ).toUtf8();
	const QByteArray skyCensus = ( wwWaterSkyCensus( t ) + QStringLiteral( "\n" ) ).toUtf8();
	if ( !put( QStringLiteral( "sky_census.txt" ), skyCensus.constData(), skyCensus.size() ) )
		return false;
	if ( t.haveSky && !( put( QStringLiteral( "sky_index.u32" ), t.skyIndex.data(), qint64( t.skyIndex.size() ) * 4 )
		&& put( QStringLiteral( "sky_atlas.u8" ), t.skyAtlas.data(), qint64( t.skyAtlas.size() ) ) ) )
		return false;
	return put( QStringLiteral( "index.u32" ), t.index.data(), qint64( t.index.size() ) * 4 )
		&& put( QStringLiteral( "atlas.u32" ), t.atlas.data(), qint64( t.atlas.size() ) * 4 )
		&& put( QStringLiteral( "bodies.f32" ), t.bodies.data(), qint64( t.bodies.size() ) * 4 )
		&& put( QStringLiteral( "flowindex.u32" ), t.flowIndex.data(), qint64( t.flowIndex.size() ) * 4 )
		&& put( QStringLiteral( "flowatlas.u32" ), t.flowAtlas.data(), qint64( t.flowAtlas.size() ) * 4 )
		&& put( QStringLiteral( "census.txt" ), census.constData(), census.size() )
		&& put( QStringLiteral( "land_index.u32" ), t.landIndex.data(), qint64( t.landIndex.size() ) * 4 )
		&& put( QStringLiteral( "land_atlas.u8" ), t.landAtlas.data(), qint64( t.landAtlas.size() ) )
		&& put( QStringLiteral( "land_census.txt" ), landCensus.constData(), landCensus.size() );
}

namespace
{

struct Job
{
	std::atomic<int> state { 0 };	// 0 building, 1 built, 2 failed
	WwWaterInfoTables t;
	QString error;
};

struct Gpu
{
	GLuint tex[5] = { 0, 0, 0, 0, 0 };	// index, atlas, bodies, flow index, flow atlas
	GLuint landTex[2] = { 0, 0 };	// lane WATERWETBAKE1: land index (R32UI), land atlas (R8UI)
	GLuint skyTex[2] = { 0, 0 };	// lane WATERSKYVISFAR1: sky index (R32UI), sky atlas (R8UI)
	std::shared_ptr<Job> job;
	QString uploaded;	// the census of what the GL holds, read back
};

struct State
{
	QHash<const void *, std::shared_ptr<Job>> docs;
	QHash<QString, std::shared_ptr<Job>> cache;	// path + size + time -> the build (one worldspace kept)
	QHash<QOpenGLContext *, Gpu> gpu;
	QString lastUpload;
};

State & st()
{
	static State s;
	return s;
}

bool infoRed()
{
	WW_SWITCH_STATIC( bool, red, wwEnv( "WW_CELL_WATER_INFO_RED" ).trimmed() == "off" );
	return red;
}

bool landRed()
{
	WW_SWITCH_STATIC( bool, red, wwEnv( "WW_CELL_LAND_SHORE_RED" ).trimmed() == "off" );
	return red;
}

bool skyRed()
{
	WW_SWITCH_STATIC( bool, red, wwEnv( "WW_CELL_WATER_SKYFAR_RED" ).trimmed() == "one" );
	return red;
}

void upload( Renderer * r, Gpu & g, const std::shared_ptr<Job> & job )
{
	auto fn = r->fn;
	const WwWaterInfoTables & t = job->t;
	GLint was = 0;
	fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &was );
	if ( !g.tex[0] )
		fn->glGenTextures( 5, g.tex );
	// lane WATERFLOWBAKE1: + the flow index (RG32UI) and atlas (R32UI); a file without a flow plane uploads 1 x 1 zero
	const GLenum ifmt[5] = { GL_RG32UI, GL_R32UI, GL_RGBA32F, GL_RG32UI, GL_R32UI };
	const GLenum fmt[5] = { GL_RG_INTEGER, GL_RED_INTEGER, GL_RGBA, GL_RG_INTEGER, GL_RED_INTEGER };
	const GLenum type[5] = { GL_UNSIGNED_INT, GL_UNSIGNED_INT, GL_FLOAT, GL_UNSIGNED_INT, GL_UNSIGNED_INT };
	const bool haveFlow = t.flowTilesX > 0;
	const int w[5] = { t.tilesX, t.atlasW, WwWaterInfoTables::kBodiesPerRow, haveFlow ? t.flowTilesX : 1,
		t.flowAtlas.empty() ? 1 : t.flowAtlasW };
	const int h[5] = { t.tilesY, std::max( 1, t.atlasH ), t.bodyRows, haveFlow ? t.flowTilesY : 1,
		t.flowAtlas.empty() ? 1 : t.flowAtlasH };
	static const uint32_t zero[4] = { 0, 0, 0, 0 };
	const void * px[5] = { t.index.data(), t.atlas.empty() ? zero : (const void *) t.atlas.data(), t.bodies.data(),
		haveFlow ? (const void *) t.flowIndex.data() : zero, t.flowAtlas.empty() ? zero : (const void *) t.flowAtlas.data() };
	const char * name[5] = { "index", "atlas", "bodies", "flowindex", "flowatlas" };
	QStringList parts;
	qint64 bytes = 0;
	for ( int k = 0; k < 5; k++ ) {
		fn->glBindTexture( GL_TEXTURE_2D, g.tex[k] );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GLint( ifmt[k] ), w[k], h[k], 0, fmt[k], type[k], px[k] );
		// telemetry echoes what the GL holds, not what was asked for
		GLint gw = 0, gh = 0, gf = 0;
		fn->glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &gw );
		fn->glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &gh );
		fn->glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &gf );
		const int bpp = gf == GL_RG32UI ? 8 : gf == GL_R32UI ? 4 : gf == GL_RGBA32F ? 16 : 0;
		bytes += qint64( gw ) * gh * bpp;
		parts << QStringLiteral( "%1 %2x%3 %4" ).arg( QLatin1String( name[k] ) ).arg( gw ).arg( gh )
			.arg( QLatin1String( gf == GL_RG32UI ? "RG32UI" : gf == GL_R32UI ? "R32UI" : gf == GL_RGBA32F ? "RGBA32F" : "??" ) );
	}
	// lane WATERWETBAKE1: the land side, read back the same way
	if ( !g.landTex[0] )
		fn->glGenTextures( 2, g.landTex );
	static const uint8_t farFill[4] = { 255, 255, 255, 255 };
	QStringList landParts;
	qint64 landBytes = 0;
	GLint align = 4;
	fn->glGetIntegerv( GL_UNPACK_ALIGNMENT, &align );
	fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	for ( int k = 0; k < 2; k++ ) {
		fn->glBindTexture( GL_TEXTURE_2D, g.landTex[k] );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
		if ( k == 0 )
			fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_R32UI, t.tilesX, t.tilesY, 0, GL_RED_INTEGER, GL_UNSIGNED_INT,
				t.landIndex.data() );
		else
			fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_R8UI, t.landAtlas.empty() ? 1 : t.atlasW,
				std::max( 1, t.landAtlasH ), 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE,
				t.landAtlas.empty() ? (const void *) farFill : (const void *) t.landAtlas.data() );
		GLint gw = 0, gh = 0, gf = 0;
		fn->glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &gw );
		fn->glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &gh );
		fn->glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &gf );
		landBytes += qint64( gw ) * gh * ( gf == GL_R32UI ? 4 : gf == GL_R8UI ? 1 : 0 );
		landParts << QStringLiteral( "%1 %2x%3 %4" ).arg( QLatin1String( k ? "land atlas" : "land index" ) )
			.arg( gw ).arg( gh ).arg( QLatin1String( gf == GL_R32UI ? "R32UI" : gf == GL_R8UI ? "R8UI" : "??" ) );
	}
	// lane WATERSKYVISFAR1: the sky plane, the same way; no plane = 1 x 1 zero (dry: the shader reads 1)
	if ( !g.skyTex[0] )
		fn->glGenTextures( 2, g.skyTex );
	static const uint32_t dry[1] = { 0 };
	qint64 skyBytes = 0;
	for ( int k = 0; k < 2; k++ ) {
		fn->glBindTexture( GL_TEXTURE_2D, g.skyTex[k] );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
		if ( k == 0 )
			fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_R32UI, t.haveSky ? t.tilesX : 1, t.haveSky ? t.tilesY : 1, 0,
				GL_RED_INTEGER, GL_UNSIGNED_INT, t.haveSky ? (const void *) t.skyIndex.data() : (const void *) dry );
		else
			fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_R8UI, t.skyAtlas.empty() ? 1 : t.atlasW, std::max( 1, t.skyAtlasH ), 0,
				GL_RED_INTEGER, GL_UNSIGNED_BYTE, t.skyAtlas.empty() ? (const void *) dry : (const void *) t.skyAtlas.data() );
		GLint gw = 0, gh = 0, gf = 0;
		fn->glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &gw );
		fn->glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &gh );
		fn->glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &gf );
		skyBytes += qint64( gw ) * gh * ( gf == GL_R32UI ? 4 : gf == GL_R8UI ? 1 : 0 );
	}
	fn->glPixelStorei( GL_UNPACK_ALIGNMENT, align );
	fn->glBindTexture( GL_TEXTURE_2D, GLuint( was ) );
	g.job = job;
	g.uploaded = QStringLiteral( "water info uploaded: %1; %2 bytes (%3 MB); %4 sparse tiles of %5, %6 bodies; "
		"%7; land %8 bytes (%9 MB), %10 sparse tiles" )
		.arg( parts.join( QLatin1String( ", " ) ) ).arg( bytes ).arg( double( bytes ) / 1048576.0, 0, 'f', 2 )
		.arg( t.slotCount ).arg( qint64( t.tilesX ) * t.tilesY ).arg( t.bodyCount )
		.arg( landParts.join( QLatin1String( ", " ) ) ).arg( landBytes )
		.arg( double( landBytes ) / 1048576.0, 0, 'f', 2 ).arg( t.landSlots )
		+ QStringLiteral( "; sky %1 bytes (%2 MB), %3 sparse tiles%4" ).arg( skyBytes )
			.arg( double( skyBytes ) / 1048576.0, 0, 'f', 2 ).arg( t.skySlots )
			.arg( t.haveSky ? QString() : QStringLiteral( " (no sky plane)" ) );
	st().lastUpload = g.uploaded;
	qInfo().noquote() << g.uploaded;
}

} // namespace

void wwCellWaterInfoSet( const void * nif, const QString & lodl )
{
	State & s = st();
	if ( lodl.isEmpty() ) {
		s.docs.remove( nif );
		return;
	}
	const QFileInfo fi( lodl );
	const QString key = QStringLiteral( "%1|%2|%3" ).arg( fi.absoluteFilePath() ).arg( fi.size() )
		.arg( fi.lastModified().toMSecsSinceEpoch() );
	std::shared_ptr<Job> job = s.cache.value( key );
	if ( !job ) {
		s.cache.clear();	// one worldspace's tables kept
		job = std::make_shared<Job>();
		s.cache.insert( key, job );
		std::thread( [job, lodl]() {
			QString err;
			const bool ok = wwWaterInfoBuild( lodl, job->t, &err );
			job->error = err;
			job->state.store( ok ? 1 : 2, std::memory_order_release );
		} ).detach();
	}
	s.docs.insert( nif, job );
}

int wwCellWaterInfoUniforms( Scene * scene, int unit )
{
	State & s = st();
	if ( !scene || !scene->renderer )
		return 0;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	QOpenGLContext * ctx = QOpenGLContext::currentContext();
	const std::shared_ptr<Job> job = s.docs.value( scene->nifModel );
	if ( !prog || !ctx )
		return 0;
	bool ready = false;
	if ( job && job->state.load( std::memory_order_acquire ) == 1 ) {
		if ( !s.gpu.contains( ctx ) )
			QObject::connect( ctx, &QOpenGLContext::aboutToBeDestroyed, [ctx]() { st().gpu.remove( ctx ); } );
		Gpu & g = s.gpu[ctx];
		if ( g.job != job )
			upload( r, g, job );
		ready = true;
	}
	prog->uni1b( "waterInfoOn", ready && !infoRed() );
	const bool flowReady = ready && job->t.flowTilesX > 0;
	prog->uni1b( "waterFlowOn", flowReady && !infoRed() );	// lane WATERFLOWBAKE1
	const int lIndex = prog->uniLocation( "waterInfoIndex" );
	const int lFlow = prog->uniLocation( "waterFlowIndex" );
	const int lSky = prog->uniLocation( "waterSkyIndex" );
	if ( lSky >= 0 ) {	// lane WATERSKYVISFAR1: the far water's sky plane (fo4_waterlook only)
		const Gpu * gs = ready ? &s.gpu[ctx] : nullptr;
		const bool skyOn = gs && job->t.haveSky && !skyRed();
		const int loc[2] = { lSky, prog->uniLocation( "waterSkyAtlas" ) };
		const int su[2] = { WwCellUnit::WaterSkyIndex, WwCellUnit::WaterSkyAtlas };
		for ( int k = 0; k < 2; k++ ) {
			r->fn->glActiveTexture( GLenum( GL_TEXTURE0 + su[k] ) );
			r->fn->glBindTexture( GL_TEXTURE_2D, gs ? gs->skyTex[k] : 0 );
			prog->uni1i_l( loc[k], su[k] );
		}
		r->fn->glActiveTexture( GL_TEXTURE0 );
		prog->uni1b( "waterSkyOn", skyOn );
		if ( skyOn ) {
			const WwWaterInfoTables & t = job->t;
			prog->uni4f( "waterSkyGrid", FloatVector4( t.originX, t.originY, t.texel, 254.0f ) );
			prog->uni4f( "waterSkyLayout", FloatVector4( float( t.tilesX ), float( t.tilesY ),
				float( WwWaterInfoTables::kTileEdge ), float( WwWaterInfoTables::kTilesPerRow ) ) );
		}
	}
	if ( lIndex < 0 && lFlow < 0 )
		return 0;	// the default program does not read the lookup: nothing bound
	auto fn = r->fn;
	const Gpu * g = ready ? &s.gpu[ctx] : nullptr;
	int used = 0;
	if ( lIndex >= 0 ) {
		const int loc[3] = { lIndex, prog->uniLocation( "waterInfoAtlas" ), prog->uniLocation( "waterInfoBodies" ) };
		for ( int k = 0; k < 3; k++ ) {
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit + k ) );
			fn->glBindTexture( GL_TEXTURE_2D, g ? g->tex[k] : 0 );
			prog->uni1i_l( loc[k], unit + k );
		}
		if ( g ) {
			const WwWaterInfoTables & t = job->t;
			prog->uni4f( "waterInfoGrid", FloatVector4( t.originX, t.originY, t.texel, t.shoreQuantum ) );
			prog->uni4f( "waterInfoLayout", FloatVector4( float( t.tilesX ), float( t.tilesY ),
				float( WwWaterInfoTables::kTileEdge ), float( WwWaterInfoTables::kTilesPerRow ) ) );
		}
		used = 3;
	}
	if ( lFlow >= 0 ) {	// lane WATERFLOWBAKE1: the flow tables (fixed cell units: the next free ones were Shadow/Bloom's)
		const int loc[2] = { lFlow, prog->uniLocation( "waterFlowAtlas" ) };
		const int fu[2] = { WwCellUnit::WaterFlowIndex, WwCellUnit::WaterFlowAtlas };
		for ( int k = 0; k < 2; k++ ) {
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + fu[k] ) );
			fn->glBindTexture( GL_TEXTURE_2D, g ? g->tex[3 + k] : 0 );
			prog->uni1i_l( loc[k], fu[k] );
		}
		if ( g && flowReady ) {
			const WwWaterInfoTables & t = job->t;
			prog->uni4f( "waterFlowGrid", FloatVector4( t.originX, t.originY, t.flowTexel,
				float( WwWaterInfoTables::kFlowScale ) ) );
			prog->uni4f( "waterFlowLayout", FloatVector4( float( t.flowTilesX ), float( t.flowTilesY ),
				float( WwWaterInfoTables::kTileEdge ), float( WwWaterInfoTables::kTilesPerRow ) ) );
		}
	}
	fn->glActiveTexture( GL_TEXTURE0 );
	return used;
}

void wwCellLandShoreUniforms( Scene * scene, bool terrain )
{
	if ( !scene || !scene->renderer )
		return;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog )
		return;
	const int lIndex = prog->uniLocation( "landShoreIndex" );
	if ( lIndex < 0 )
		return;	// the default programs do not read the land side: nothing bound, nothing set
	State & s = st();
	QOpenGLContext * ctx = QOpenGLContext::currentContext();
	const std::shared_ptr<Job> job = s.docs.value( scene->nifModel );
	const Gpu * g = nullptr;
	if ( ctx && job && job->state.load( std::memory_order_acquire ) == 1 ) {
		if ( !s.gpu.contains( ctx ) )
			QObject::connect( ctx, &QOpenGLContext::aboutToBeDestroyed, [ctx]() { st().gpu.remove( ctx ); } );
		Gpu & gg = s.gpu[ctx];
		if ( gg.job != job )
			upload( r, gg, job );
		g = &gg;
	}
	auto fn = r->fn;
	const int loc[2] = { lIndex, prog->uniLocation( "landShoreAtlas" ) };
	const int unit[2] = { WwCellUnit::LandShoreIndex, WwCellUnit::LandShoreAtlas };
	for ( int k = 0; k < 2; k++ ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit[k] ) );
		fn->glBindTexture( GL_TEXTURE_2D, g ? g->landTex[k] : 0 );
		prog->uni1i_l( loc[k], unit[k] );
	}
	fn->glActiveTexture( GL_TEXTURE0 );
	prog->uni1b( "landShoreOn", g && !landRed() );
	prog->uni1b( "landShoreTerrain", terrain );
	if ( g ) {
		const WwWaterInfoTables & t = job->t;
		prog->uni4f( "landShoreGrid", FloatVector4( t.originX, t.originY, t.texel, t.shoreQuantum ) );
		prog->uni4f( "landShoreLayout", FloatVector4( float( t.tilesX ), float( t.tilesY ),
			float( WwWaterInfoTables::kTileEdge ), float( WwWaterInfoTables::kTilesPerRow ) ) );
	}
}

QString wwCellWaterInfoEcho( const void * nif )
{
	const State & s = st();
	const std::shared_ptr<Job> job = s.docs.value( nif );
	if ( !job )
		return QStringLiteral( "water info: none (no worldspace .lodl)" );
	const int state = job->state.load( std::memory_order_acquire );
	QString line = state == 0 ? QStringLiteral( "water info: building from the .lodl" )
		: state == 2 ? QStringLiteral( "water info: refused: %1" ).arg( job->error )
		: wwWaterInfoCensus( job->t ) + QStringLiteral( "; " ) + wwWaterFlowCensus( job->t );
	if ( !s.lastUpload.isEmpty() )
		line += QStringLiteral( "; last %1" ).arg( s.lastUpload );
	if ( infoRed() )
		line += QStringLiteral( "; WW_CELL_WATER_INFO_RED=off (neutral lookup)" );
	if ( state == 1 )
		line += QStringLiteral( "; " ) + wwLandShoreCensus( job->t );
	if ( landRed() )
		line += QStringLiteral( "; WW_CELL_LAND_SHORE_RED=off (land side neutral)" );
	if ( state == 1 )
		line += QStringLiteral( "; " ) + wwWaterSkyCensus( job->t );
	if ( skyRed() )
		line += QStringLiteral( "; WW_CELL_WATER_SKYFAR_RED=one (far water sky visibility 1)" );
	return line;
}
