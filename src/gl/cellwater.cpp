#include "cellwater.h"

#include "cellhdr.h"
#include "celllights.h"
#include "cellssr.h"
#include "lookdevstage.h"
#include "esmwater.h"
#include "gl/glscene.h"
#include "gl/gltex.h"
#include "gl/renderer.h"

#include <QFile>
#include <QHash>
#include <QSet>
#include <QSettings>
#include <QTextStream>
#include <QVector>

#include <algorithm>

namespace
{

struct WaterBody
{
	WwWaterRecord rec;
	WwWaterMaterial mat;
	QString noise[3];	// as the texture cache takes them: "textures\water\..."
};

struct WaterDoc
{
	QVector<WaterBody> bodies;
	QHash<quint32, int> byForm;
	QHash<int, int> shapeBody;	// written shape's block -> index into bodies
};

struct WaterState
{
	QHash<const void *, WaterDoc> docs;
	bool pinOff = false;	// lane CELLALL1: off (the Water row, or the pin)
	bool pinned = false;
	int red = 0;		// 1 norefl, 2 nofresnel, 4 nosilt, 8 nospec, 16 noshore, 32 nonormal,
				// 64 nodepthfog, 128 nofar, 256 nossr, 512 fogunits (lane WATER2)
				// 1024 hdrraw (WATERHDR1: the water's linear light written back untone-mapped, the old defect)
	int probe = 0;
	bool linear = false;	// WATERHDR1: the last water draw wrote linear light into the HDR frame
	QString dump;
	QSet<QString> missing;	// noise textures that did not bind (the census says so)
	quint64 dumped = 0;
};

WaterState & st()
{
	static WaterState * s = nullptr;
	if ( !s ) {
		s = new WaterState;
		const QByteArray pin = qgetenv( "WW_CELL_WATER" ).trimmed();
		s->pinned = !pin.isEmpty();
		s->pinOff = s->pinned ? pin == "0" : !QSettings().value( QStringLiteral( "WW/CellWater" ), true ).toBool();
		const QByteArray r = qgetenv( "WW_CELL_WATER_RED" );
		s->red = r == "norefl" ? 1 : r == "nofresnel" ? 2 : r == "nosilt" ? 4 : r == "nospec" ? 8
			: r == "noshore" ? 16 : r == "nonormal" ? 32 : r == "nodepthfog" ? 64 : r == "nofar" ? 128
			: r == "nossr" ? 256 : r == "fogunits" ? 512 : r == "hdrraw" ? 1024 : 0;
		s->probe = qEnvironmentVariableIntValue( "WW_CELL_WATER_PROBE" );
		s->dump = QString::fromLocal8Bit( qgetenv( "WW_CELL_WATER_DUMP" ) );
	}
	return *s;
}

//! NAM2-4 store "data\Textures\Water\X.dds"; the cache resolves paths under the data folder
QString noisePath( const QString & stored )
{
	QString p = QString( stored ).replace( '/', '\\' );
	if ( p.startsWith( QLatin1String( "data\\" ), Qt::CaseInsensitive ) )
		p = p.mid( 5 );
	return p;
}

void writeDump( const WaterDoc & d, const float sky[3][3], bool skyFromWeather, const float sun[3] )
{
	WaterState & s = st();
	QFile f( s.dump );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Text ) )
		return;
	QTextStream o( &f );
	o << "water bodies=" << d.bodies.size() << " shapes=" << d.shapeBody.size() << " red=" << s.red
	  << " probe=" << s.probe << "\n";
	o << "sky " << ( skyFromWeather ? "weather" : "reflection" );
	for ( int r = 0; r < 3; r++ )
		for ( int c = 0; c < 3; c++ )
			o << " " << QString::number( double( sky[r][c] ), 'g', 9 );
	o << "\nsun";
	for ( int c = 0; c < 3; c++ )
		o << " " << QString::number( double( sun[c] ), 'g', 9 );
	o << "\n";
	for ( const WaterBody & b : d.bodies )
		o << wwWaterDescribe( b.rec ) << "\n";
	for ( auto it = d.shapeBody.constBegin(); it != d.shapeBody.constEnd(); ++it )
		o << "shape " << it.key() << " body " << it.value() << " form "
		  << QString::number( d.bodies[it.value()].rec.form, 16 ) << "\n";
}

} // namespace

void wwCellWaterBegin( const void * nif )
{
	st().docs.remove( nif );
}

void wwCellWaterShape( const void * nif, int block, const WwWaterRecord & rec )
{
	WaterDoc & d = st().docs[nif];
	auto it = d.byForm.constFind( rec.form );
	int i;
	if ( it == d.byForm.constEnd() ) {
		WaterBody b;
		b.rec = rec;
		b.mat = wwWaterMaterial( rec );
		for ( int k = 0; k < 3; k++ )
			b.noise[k] = noisePath( rec.noise[k] );
		i = d.bodies.size();
		d.bodies.append( b );
		d.byForm.insert( rec.form, i );
	} else {
		i = it.value();
	}
	d.shapeBody.insert( block, i );
}

bool wwCellWaterOn()
{
	return !st().pinOff;
}

void wwCellWaterSetOn( bool on )
{
	WaterState & s = st();
	if ( s.pinned )
		return;
	s.pinOff = !on;
	QSettings().setValue( QStringLiteral( "WW/CellWater" ), on );
}

bool wwCellWaterWanted( Scene * scene, int block )
{
	const WaterState & s = st();
	if ( s.pinOff || !scene || scene->selecting || !wwCellLightsWanted( scene ) || wwCellProbePass( scene ) )
		return false;
	const auto it = s.docs.constFind( scene->nifModel );
	return it != s.docs.constEnd() && it->shapeBody.contains( block );
}

bool wwCellWaterSsrWanted( Scene * scene )
{
	const WaterState & s = st();
	if ( s.pinOff || !scene || scene->selecting || !wwCellLightsWanted( scene ) || wwCellProbePass( scene ) )
		return false;
	const auto it = s.docs.constFind( scene->nifModel );
	if ( it == s.docs.constEnd() || it->shapeBody.isEmpty() )
		return false;
	for ( const WaterBody & b : it->bodies )
		if ( b.rec.ssr )
			return true;
	return false;
}

bool wwCellWaterIsShape( const void * nif, int block )
{
	const WaterState & s = st();
	const auto it = s.docs.constFind( nif );
	return it != s.docs.constEnd() && it->shapeBody.contains( block );
}

bool wwCellWaterUniforms( Scene * scene, int block, int unit )
{
	WaterState & s = st();
	if ( !scene || !scene->renderer )
		return false;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog || prog->uniLocation( "waterMat" ) < 0 )
		return false;
	const auto dit = s.docs.constFind( scene->nifModel );
	if ( dit == s.docs.constEnd() )
		return false;
	const auto sit = dit->shapeBody.constFind( block );
	if ( sit == dit->shapeBody.constEnd() )
		return false;
	const WaterBody & b = dit->bodies[sit.value()];
	auto fn = r->fn;

	// the record's constant block, in the order the game's shader reads it (src/esmwater.h)
	const float * rows[13] = { b.mat.shallow, b.mat.deep, b.mat.reflection, b.mat.underwater, b.mat.lightSilt,
		b.mat.darkSilt, b.mat.varAmounts, b.mat.params1, b.mat.params2, b.mat.params3, b.mat.params4,
		b.mat.amplitude, b.mat.uvScale };
	FloatVector4 m[13];
	for ( int k = 0; k < 13; k++ )
		m[k] = FloatVector4( rows[k][0], rows[k][1], rows[k][2], rows[k][3] );
	prog->uni4fv( "waterMat", m, 13 );

	// the water's sky: the weather's horizon, lower and upper rows at the hour the dome uses (linear)
	float sky[3][3];
	const bool fromWeather = wwLookdevWaterSky( sky[0], sky[1], sky[2] );
	if ( !fromWeather )
		for ( int k = 0; k < 3; k++ )
			for ( int c = 0; c < 3; c++ )
				sky[k][c] = b.mat.reflection[c];
	FloatVector4 sk[3];
	for ( int k = 0; k < 3; k++ )
		sk[k] = FloatVector4( sky[k][0], sky[k][1], sky[k][2], 0.0f );
	prog->uni4fv( "waterSky", sk, 3 );
	prog->uni1i( "waterRed", s.red );
	prog->uni1i( "waterProbe", s.probe );
	// the normals' scroll: wind direction x speed x time in the game; the cell view stands at time 0 (INFERRED)
	prog->uni4f( "waterScroll0", FloatVector4( 0.0f ) );
	prog->uni4f( "waterScroll1", FloatVector4( 0.0f ) );

	static const char * const noiseUni[3] = { "waterNoise0", "waterNoise1", "waterNoise2" };
	for ( int k = 0; k < 3; k++ ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
		if ( b.noise[k].isEmpty() || !scene->textures->bind( QStringView( b.noise[k] ), scene->nifModel ) ) {
			if ( !b.noise[k].isEmpty() )
				s.missing.insert( b.noise[k] );
			scene->textures->bind( QStringView( u"#FF8080FF" ), scene->nifModel );	// a flat normal
		}
		/* lane WATER2: the game's sampler for the noise slots 4..6 (SetupGeometry): WRAP_S_WRAP_T, filter
		 * TEXTURE_FILTER_MODE_ANISO at the INI's iMaxAnisotropy (16). The cache leaves its default (nearest
		 * within the mip, no anisotropy): the distant ripples average flat and the far glints die. */
		static GLfloat maxAniso = -1.0f;
		if ( maxAniso < 0.0f ) {
			maxAniso = 1.0f;
			glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso );
		}
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, ( s.red & 128 ) ? 1.0f : std::min( 16.0f, maxAniso ) );
		prog->uni1i_l( prog->uniLocation( noiseUni[k] ), unit++ );
	}
	// the scene behind the water (the second pass's copy) and the opaque frame's depth; in the reflections' scene
	// passes (lane WATER2) no copy: the ray pass reads the obscurance pass's linear depth instead
	unsigned int gbuf = 0;
	const bool rayPass = wwCellSsrWaterRayPass( &gbuf );
	const bool inSsr = wwCellSsrInPass();
	const bool haveScene = !inSsr && scene->grabRefractionSource();
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glBindTexture( GL_TEXTURE_2D, haveScene ? scene->refractionTexId : 0 );
	prog->uni1i_l( prog->uniLocation( "waterScene" ), unit++ );
	const bool haveDepth = !inSsr && scene->grabEffectDepth();
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glBindTexture( GL_TEXTURE_2D, rayPass ? gbuf : haveDepth ? scene->fxDepthTexId : 0 );
	prog->uni1i_l( prog->uniLocation( "waterDepth" ), unit++ );
	prog->uni1b( "waterHaveDepth", haveDepth );
	prog->uni1i( "waterSslr", rayPass ? 1 : 0 );
	/* WATERHDR1: in the linear frame (cellhdr.h) the water writes linear light (fo4_water.frag, cellIsLinear) and
	 * must be sorted with the cell programs, tone-mapped once at the resolve; its probes and its reflection-ray
	 * pass write raw data and are written back as drawn */
	s.linear = wwCellHdrActive() && s.probe == 0 && !rayPass && !( s.red & 1024 );
	// lane WATER2: this frame's reflections (the game reads them only when its WATR's SSR flag is set)
	unsigned int ssrRaw = 0, ssrFin = 0;
	const bool ssrOn = !inSsr && b.rec.ssr && wwCellSsrWaterTextures( scene, ssrRaw, ssrFin );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glBindTexture( GL_TEXTURE_2D, ssrOn ? ssrRaw : 0 );
	prog->uni1i_l( prog->uniLocation( "waterSsrRaw" ), unit++ );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glBindTexture( GL_TEXTURE_2D, ssrOn ? ssrFin : 0 );
	prog->uni1i_l( prog->uniLocation( "waterSsrFin" ), unit++ );
	prog->uni1b( "waterSsrOn", ssrOn );
	fn->glActiveTexture( GL_TEXTURE0 );

	// an opaque surface: it composites the scene behind it itself
	glDisable( GL_BLEND );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glDepthMask( GL_TRUE );

	if ( !s.dump.isEmpty() && s.dumped != quint64( quintptr( scene->nifModel ) ) ) {
		s.dumped = quint64( quintptr( scene->nifModel ) );
		const float sun[3] = { 0.0f, 0.0f, 0.0f };	// the shader reads light 0 (the frame's sun); the probes echo it
		writeDump( *dit, sky, fromWeather, sun );
	}
	return true;
}

bool wwCellWaterWritesLinear()
{
	return st().linear;
}

QString wwCellWaterEcho( const void * nif )
{
	const WaterState & s = st();
	const auto it = s.docs.constFind( nif );
	if ( it == s.docs.constEnd() )
		return QStringLiteral( "water: no water surfaces registered" );
	QStringList names;
	for ( const WaterBody & b : it->bodies )
		names << QStringLiteral( "%1 %2" ).arg( b.rec.form, 8, 16, QLatin1Char( '0' ) ).arg( b.rec.editorId );
	return QStringLiteral( "water: %1 shape(s), %2 record(s) [%3]%4" ).arg( it->shapeBody.size() )
		.arg( it->bodies.size() ).arg( names.join( QLatin1String( ", " ) ) )
		.arg( s.pinOff ? ( s.pinned ? QStringLiteral( ", pinned off (WW_CELL_WATER=0)" ) : QStringLiteral( ", off (row)" ) )
			: QString() );
}
