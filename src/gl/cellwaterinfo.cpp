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
	const QByteArray census = ( wwWaterInfoCensus( t )
		+ QStringLiteral( "\ngrid origin %1 %2 texel %3 shore quantum %4 tiles %5 %6 edge %7 per row %8 body rows %9\n" )
			.arg( double( t.originX ) ).arg( double( t.originY ) ).arg( double( t.texel ) ).arg( double( t.shoreQuantum ) )
			.arg( t.tilesX ).arg( t.tilesY ).arg( WwWaterInfoTables::kTileEdge ).arg( WwWaterInfoTables::kTilesPerRow )
			.arg( t.bodyRows ) ).toUtf8();
	const QByteArray landCensus = ( wwLandShoreCensus( t ) + QStringLiteral( "\n" ) ).toUtf8();
	return put( QStringLiteral( "index.u32" ), t.index.data(), qint64( t.index.size() ) * 4 )
		&& put( QStringLiteral( "atlas.u32" ), t.atlas.data(), qint64( t.atlas.size() ) * 4 )
		&& put( QStringLiteral( "bodies.f32" ), t.bodies.data(), qint64( t.bodies.size() ) * 4 )
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
	GLuint tex[3] = { 0, 0, 0 };
	GLuint landTex[2] = { 0, 0 };	// lane WATERWETBAKE1: land index (R32UI), land atlas (R8UI)
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

void upload( Renderer * r, Gpu & g, const std::shared_ptr<Job> & job )
{
	auto fn = r->fn;
	const WwWaterInfoTables & t = job->t;
	GLint was = 0;
	fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &was );
	if ( !g.tex[0] )
		fn->glGenTextures( 3, g.tex );
	const GLenum ifmt[3] = { GL_RG32UI, GL_R32UI, GL_RGBA32F };
	const GLenum fmt[3] = { GL_RG_INTEGER, GL_RED_INTEGER, GL_RGBA };
	const GLenum type[3] = { GL_UNSIGNED_INT, GL_UNSIGNED_INT, GL_FLOAT };
	const int w[3] = { t.tilesX, t.atlasW, WwWaterInfoTables::kBodiesPerRow };
	const int h[3] = { t.tilesY, std::max( 1, t.atlasH ), t.bodyRows };
	static const uint32_t zero[4] = { 0, 0, 0, 0 };
	const void * px[3] = { t.index.data(), t.atlas.empty() ? zero : (const void *) t.atlas.data(), t.bodies.data() };
	const char * name[3] = { "index", "atlas", "bodies" };
	QStringList parts;
	qint64 bytes = 0;
	for ( int k = 0; k < 3; k++ ) {
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
	fn->glPixelStorei( GL_UNPACK_ALIGNMENT, align );
	fn->glBindTexture( GL_TEXTURE_2D, GLuint( was ) );
	g.job = job;
	g.uploaded = QStringLiteral( "water info uploaded: %1; %2 bytes (%3 MB); %4 sparse tiles of %5, %6 bodies; "
		"%7; land %8 bytes (%9 MB), %10 sparse tiles" )
		.arg( parts.join( QLatin1String( ", " ) ) ).arg( bytes ).arg( double( bytes ) / 1048576.0, 0, 'f', 2 )
		.arg( t.slotCount ).arg( qint64( t.tilesX ) * t.tilesY ).arg( t.bodyCount )
		.arg( landParts.join( QLatin1String( ", " ) ) ).arg( landBytes )
		.arg( double( landBytes ) / 1048576.0, 0, 'f', 2 ).arg( t.landSlots );
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
	const int lIndex = prog->uniLocation( "waterInfoIndex" );
	if ( lIndex < 0 )
		return 0;	// the default program does not read the lookup: nothing bound
	auto fn = r->fn;
	const Gpu * g = ready ? &s.gpu[ctx] : nullptr;
	const int loc[3] = { lIndex, prog->uniLocation( "waterInfoAtlas" ), prog->uniLocation( "waterInfoBodies" ) };
	for ( int k = 0; k < 3; k++ ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit + k ) );
		fn->glBindTexture( GL_TEXTURE_2D, g ? g->tex[k] : 0 );
		prog->uni1i_l( loc[k], unit + k );
	}
	fn->glActiveTexture( GL_TEXTURE0 );
	if ( g ) {
		const WwWaterInfoTables & t = job->t;
		prog->uni4f( "waterInfoGrid", FloatVector4( t.originX, t.originY, t.texel, t.shoreQuantum ) );
		prog->uni4f( "waterInfoLayout", FloatVector4( float( t.tilesX ), float( t.tilesY ),
			float( WwWaterInfoTables::kTileEdge ), float( WwWaterInfoTables::kTilesPerRow ) ) );
	}
	return 3;
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
		: wwWaterInfoCensus( job->t );
	if ( !s.lastUpload.isEmpty() )
		line += QStringLiteral( "; last %1" ).arg( s.lastUpload );
	if ( infoRed() )
		line += QStringLiteral( "; WW_CELL_WATER_INFO_RED=off (neutral lookup)" );
	if ( state == 1 )
		line += QStringLiteral( "; " ) + wwLandShoreCensus( job->t );
	if ( landRed() )
		line += QStringLiteral( "; WW_CELL_LAND_SHORE_RED=off (land side neutral)" );
	return line;
}
