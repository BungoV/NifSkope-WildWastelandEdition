#include "cellwaterinfo.h"
#include "wwswitch.h"

#include "lodtfile.h"
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

quint64 WwWaterInfoTables::flowBytes() const
{
	return quint64( flowIndex.size() + flowAtlas.size() ) * 4;
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
	std::vector<uint32_t> id( fn ), sh( fn ), sf( fn ), word( fn );
	std::vector<double> depth( fn );
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
					const int ti = ( fy * ( fe / E ) + sy ) * t.tilesX + fx * ( fe / E ) + sx;
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
	wwWaterFlowBuild( f, t );	// lane WATERFLOWBAKE1
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
	return put( QStringLiteral( "index.u32" ), t.index.data(), qint64( t.index.size() ) * 4 )
		&& put( QStringLiteral( "atlas.u32" ), t.atlas.data(), qint64( t.atlas.size() ) * 4 )
		&& put( QStringLiteral( "bodies.f32" ), t.bodies.data(), qint64( t.bodies.size() ) * 4 )
		&& put( QStringLiteral( "flowindex.u32" ), t.flowIndex.data(), qint64( t.flowIndex.size() ) * 4 )
		&& put( QStringLiteral( "flowatlas.u32" ), t.flowAtlas.data(), qint64( t.flowAtlas.size() ) * 4 )
		&& put( QStringLiteral( "census.txt" ), census.constData(), census.size() );
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
	fn->glBindTexture( GL_TEXTURE_2D, GLuint( was ) );
	g.job = job;
	g.uploaded = QStringLiteral( "water info uploaded: %1; %2 bytes (%3 MB); %4 sparse tiles of %5, %6 bodies" )
		.arg( parts.join( QLatin1String( ", " ) ) ).arg( bytes ).arg( double( bytes ) / 1048576.0, 0, 'f', 2 )
		.arg( t.slotCount ).arg( qint64( t.tilesX ) * t.tilesY ).arg( t.bodyCount );
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
	if ( lFlow >= 0 ) {	// lane WATERFLOWBAKE1: the flow tables at the next two units
		const int loc[2] = { lFlow, prog->uniLocation( "waterFlowAtlas" ) };
		for ( int k = 0; k < 2; k++ ) {
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit + used + k ) );
			fn->glBindTexture( GL_TEXTURE_2D, g ? g->tex[3 + k] : 0 );
			prog->uni1i_l( loc[k], unit + used + k );
		}
		if ( g && flowReady ) {
			const WwWaterInfoTables & t = job->t;
			prog->uni4f( "waterFlowGrid", FloatVector4( t.originX, t.originY, t.flowTexel,
				float( WwWaterInfoTables::kFlowScale ) ) );
			prog->uni4f( "waterFlowLayout", FloatVector4( float( t.flowTilesX ), float( t.flowTilesY ),
				float( WwWaterInfoTables::kTileEdge ), float( WwWaterInfoTables::kTilesPerRow ) ) );
		}
		used += 2;
	}
	fn->glActiveTexture( GL_TEXTURE0 );
	return used;
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
	return line;
}
