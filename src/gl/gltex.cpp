/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENCE BLOCK *****/

#include "gltex.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store
#include "cellspeed.h"
#include "celltexahead.h"
#include "cellpasstime.h"	// lane STREAM1
#include "celldrawpacket.h"	// lane GAPPERF-BINDLESS
#include "cellbindless.h"	// lane BINDLESS2

#include "message.h"
#include "gl/glscene.h"
#include "model/nifmodel.h"

#include <QDebug>
#include <QDir>
#include <QListView>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSettings>

#include <algorithm>
#include <cstdio>

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif


//! @file gltex.cpp TexCache management

int TexCache::num_texture_units = 0;
int TexCache::pbrCubeMapResolution = 512;
int TexCache::pbrImportanceSamples = 256;
int TexCache::hdrToneMapLevel = 8;

//! Maximum anisotropy
static float max_anisotropy = 1.0f;

void TexCache::set_max_anisotropy()
{
	static QSettings settings;
	int	tmp = roundFloat( settings.value( "Settings/Render/General/Anisotropic Filtering", 4.0 ).toFloat() );
	tmp = 1 << std::min< int >( std::max< int >( tmp, 0 ), 4 );
	max_anisotropy = std::min( float( tmp ), max_anisotropy );
}

float TexCache::get_max_anisotropy()
{
	return max_anisotropy;
}

void TexCache::setOpenGLContext( NifSkopeOpenGLContext * context )
{
	fn = context->fn;

	GLint	tmp = 0;
	glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &tmp );
	tmp = std::min( std::max( tmp, GLint(1) ), GLint(TexCache::maxTextureUnits) );
	TexCache::num_texture_units = tmp;

	//qDebug() << "texture units" << TexCache::num_texture_units;

	if ( context->cx->hasExtension( "GL_EXT_texture_filter_anisotropic" ) ) {
		glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy );
		set_max_anisotropy();
		//qDebug() << "maximum anisotropy" << max_anisotropy;
	}
}

bool TexCache::activateTextureUnit( int stage )
{
	if ( stage >= TexCache::num_texture_units ) [[unlikely]]
		return false;

	fn->glActiveTexture( GL_TEXTURE0 + stage );
	return true;
}


/*
 *  TexCache
 */

QString TexCache::TexFmt::toString() const
{
	if ( imageFormat != TEXFMT_UNKNOWN ) {
		QString	format;
		switch ( imageFormat ) {
		case TEXFMT_BMP:
			format = "BMP";
			break;
		case TEXFMT_DDS:
			format = "DDS";
			break;
		case TEXFMT_NIF:
			format = "NIF";
			break;
		default:
			format = "TGA";
			break;
		}
		if ( imageEncoding & TEXFMT_DXT1 )
			format.append( " (DXT1)" );
		if ( imageEncoding & TEXFMT_DXT3 )
			format.append( " (DXT3)" );
		if ( imageEncoding & TEXFMT_DXT5 )
			format.append( " (DXT5)" );
		if ( imageEncoding & TEXFMT_GRAYSCALE )
			format.append( " (greyscale)" );
		if ( imageEncoding & TEXFMT_GRAYSCALE_ALPHA )
			format.append( " (greyscale) (alpha)" );
		if ( imageEncoding & TEXFMT_PAL8 )
			format.append( " (PAL8)" );
		if ( imageEncoding & TEXFMT_RGB8 )
			format.append( " (RGB8)" );
		if ( imageEncoding & TEXFMT_RGBA8 )
			format.append( " (RGBA8)" );
		if ( imageEncoding & TEXFMT_RLE )
			format.append( " (RLE)" );
		return format;
	}
	if ( imageEncoding & TEXFMT_DXT1 )
		return "(DXT1)";
	if ( imageEncoding & TEXFMT_DXT3 )
		return "(DXT3)";
	if ( imageEncoding & TEXFMT_DXT5 )
		return "(DXT5)";
	return QString();
}

TexCache::TexCache( QObject * parent ) : QObject( parent )
{
	textures = nullptr;
	textureHashMask = 0;
	textureCount = 0;
	fn = nullptr;
	rehashTextures();
}

TexCache::~TexCache()
{
#if 0
	flush();
#endif
	delete[] textures;
}

QString TexCache::find( const QString & file, const NifModel * nif )
{
	if ( file.isEmpty() )
		return QString();
	if ( file.startsWith( QChar('#') ) && (file.length() == 9 || file.length() == 10) )
		return file;

	QString filename( file );

	static const char *	extensions[6] = {
		".dds", ".tga", ".png", ".bmp", ".nif", ".texcache"
	};

	// attempt to find the texture with one of the extensions
	for ( size_t i = 0; i < 6; i++ ) {
		QString	fullPath;
		if ( !nif )
			fullPath = Game::GameManager::find_file( Game::OTHER, filename, "textures", extensions[i] );
		else
			fullPath = nif->findResourceFile( filename, "textures", extensions[i] );
		if ( !fullPath.isEmpty() )
			return fullPath;
		if ( i == 0 ) {
			QSettings settings;
			if ( !settings.value( "Settings/Resources/Alternate Extensions", false ).toBool() )
				break;
		}
	}

	return filename;
}

/*!
 * Note: all original morrowind nifs use name.ext only for addressing the
 * textures, but most mods use something like textures/[subdir/]name.ext.
 * This is due to a feature in Morrowind resource manager: it loads name.ext,
 * textures/name.ext and textures/subdir/name.ext but NOT subdir/name.ext.
 */
QString TexCache::stripPath( const QString & filepath, const QString & nifFolder )
{
	QString file = filepath;
	file = file.replace( QChar('/'), QChar('\\') ).toLower();
	QDir basePath;

	QSettings settings;

	// TODO: New asset manager support
	QStringList folders = settings.value( "Settings/Resources/Folders", QStringList() ).toStringList();

	for ( QString base : folders ) {
		if ( base.startsWith( QLatin1StringView("./") ) || base.startsWith( QLatin1StringView(".\\") ) ) {
			base = nifFolder + QChar('/') + base;
		}

		basePath.setPath( base );
		base = basePath.absolutePath();
		base = base.replace( QChar('/'), QChar('\\') ).toLower();
		/*
		 * note that basePath.relativeFilePath( file ) here is *not*
		 * what we want - see the above doc comments for this function
		 */

		if ( file.startsWith( base ) ) {
			file.remove( 0, base.length() );
			break;
		}
	}

	if ( file.startsWith( QChar('/') ) || file.startsWith( QChar('\\') ) )
		file.remove( 0, 1 );

	return file;
}

bool TexCache::canLoad( const QString & filePath )
{
	return texCanLoad( filePath );
}

bool TexCache::isSupported( const QString & filePath )
{
	return texIsSupported( filePath );
}

const TexCache::Tex::ImageInfo * TexCache::getTextureInfo( const QStringView & file ) const
{
	if ( file.isEmpty() ) [[unlikely]]
		return nullptr;
	const QChar *	s = file.data();
	size_t	nameLen = size_t( file.size() );
	std::uint32_t	h = hashFunctionUInt32( s, nameLen * sizeof( QChar ) );
	std::uint32_t	m = textureHashMask;
	for ( h = h & m; textures[h].nameLen; h = ( h + 1U ) & m ) {
		const Tex &	p = textures[h];
		if ( p.nameLen == nameLen && std::memcmp( p.nameData, s, nameLen * sizeof( QChar ) ) == 0 )
			return p.imageInfo;
	}
	return nullptr;
}

// lane PRTP5 (the cell census): what the frames since the last flush asked this cache for
void TexCache::wwAskedAndMissing( int & asked, int & missing, QStringList * names ) const
{
	asked = missing = 0;
	for ( size_t i = 0; i <= textureHashMask; i++ ) {
		const Tex &	tx = textures[i];
		if ( !tx.nameLen || !tx.imageInfo )
			continue;
		asked++;
		if ( tx.mipmaps || ( !tx.id[0] && tx.imageInfo->mipmaps ) )   // lane CELLSTREAM1: evicted is not missing
			continue;
		missing++;
		if ( names )
			names->append( tx.imageInfo->filename );
	}
}

static inline const QString & convertToQString( const QString & s )
{
	return s;
}

static inline QString convertToQString( const QStringView & s )
{
	return s.toString();
}

template< typename T > inline TexCache::Tex * TexCache::insertTex( const T & file )
{
	if ( file.isEmpty() ) [[unlikely]]
		return nullptr;
	const QChar *	s = file.data();
	size_t	nameLen = size_t( file.size() );
	std::uint32_t	h = hashFunctionUInt32( s, nameLen * sizeof( QChar ) );
	std::uint32_t	m = textureHashMask;
	for ( h = h & m; textures[h].nameLen; h = ( h + 1U ) & m ) {
		Tex &	p = textures[h];
		if ( p.nameLen == nameLen && std::memcmp( p.nameData, s, nameLen * sizeof( QChar ) ) == 0 )
			return &p;
	}

	if ( std::uint16_t( nameLen ) != nameLen ) [[unlikely]]
		return nullptr;
	Tex &	tx = textures[h];
	tx.imageInfo = new Tex::ImageInfo;
	tx.imageInfo->filename = convertToQString( file );
	tx.nameData = tx.imageInfo->filename.constData();
	tx.nameLen = std::uint16_t( nameLen );

	textureCount++;
	if ( ( std::uint64_t(textureCount) * 3U ) > ( std::uint64_t(textureHashMask) * 2U ) )
		return rehashTextures( &tx );

	return &tx;
}

TexCache::Tex * TexCache::rehashTextures( Tex * p )
{
	std::uint32_t	prvMask = textureHashMask;
	std::uint32_t	m = ( prvMask << 1 ) | 15U;
	size_t	n = size_t( m ) + 1;
	Tex *	newTextures = new Tex[n];
	Tex *	q = nullptr;
	if ( textureCount ) {
		for ( size_t i = 0; i <= prvMask; i++ ) {
			Tex &	tx = textures[i];
			if ( tx.nameLen ) {
				std::uint32_t	h = hashFunctionUInt32( tx.nameData, size_t( tx.nameLen ) * sizeof( QChar ) ) & m;
				while ( newTextures[h].nameLen )
					h = ( h + 1U ) & m;
				newTextures[h] = tx;
				if ( p == &tx ) [[unlikely]]
					q = newTextures + h;
			}
		}
	}
	delete[] textures;
	textures = newTextures;
	textureHashMask = m;
	return q;
}

int TexCache::bind( const QStringView & fname, const NifModel * nif )
{
	paramId = 0;	// lane PERF5
	wwBound = 0;	// lane CELLUNI3
	Tex *	tx = insertTex( fname );
	if ( !tx ) [[unlikely]]
		return 0;
	tx->useTick = useTick;
	if ( !tx->isLoaded() ) [[unlikely]] {
		if ( tx->id[0] )
			return 0;

		return loadTex( *tx, nif );
	}

	if ( !tx->target ) [[unlikely]]
		tx->target = GL_TEXTURE_2D;
	glBindTexture( tx->target, tx->id[0] );
	WWDP( WwDp::BindTex, GLint( tx->target ), GLint( tx->id[0] ) );	// lane GAPPERF-BINDLESS
	wwBound = tx->target == GL_TEXTURE_2D ? tx->id[0] : 0;	// lane CELLUNI3
	// lane PERF5: the shadow follows its own caller's binds; any other caller may set other parameters
	if ( wwParamCaller && tx->target == GL_TEXTURE_2D ) {
		paramId = tx->id[0];
	} else {
		paramForget( tx->id[0] );
		WwDp::abortRecording();
	}

	return tx->mipmaps;
}

void TexCache::paramForget( GLuint id )
{
	if ( id < paramShadow.size() ) {
		paramShadow[id].epoch = 0;
		paramShadow[id].cube = 0;	// lane CELLUNI3: a reload may change the format
	}
}

static int wwTexShadowMode()	// lane PERF5: 0 = the red (always set), 1 = the shadow, 2 = the shadow read back and compared
{
	WW_SWITCH_STATIC( int, m, wwEnvInt( "WW_DRAW_TEXSHADOW_RED" ) != 0 ? 0 : ( wwEnvInt( "WW_DRAW_TEXSHADOW_CHECK" ) != 0 ? 2 : 1 ) );
	return m;
}

bool TexCache::wwParamsNeeded( int wrapS, int wrapT, int minFilter, float aniso )
{
	const GLuint id = paramId;
	paramId = 0;
	const int mode = wwTexShadowMode();
	if ( !mode || !id || id >= ( 1U << 22 ) )
		return true;
	if ( id >= paramShadow.size() )
		paramShadow.resize( size_t( id ) + 1024 );
	ParamShadow & e = paramShadow[id];
	if ( e.epoch == paramEpoch && e.wrapS == wrapS && e.wrapT == wrapT && e.minFilter == minFilter && e.aniso == aniso ) {
		paramSkips++;
		if ( mode == 1 )
			return false;
		// the check: what GL holds must be what the shadow believes; the parameters are set again all the same
		GLint s = 0, t = 0, mn = 0, mg = 0;
		GLfloat a = 0.0f;
		glGetTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &s );
		glGetTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &t );
		glGetTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &mn );
		glGetTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &mg );
		glGetTexParameterfv( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &a );
		if ( s != wrapS || t != wrapT || mn != minFilter || mg != GL_LINEAR || a != aniso ) {
			paramBad++;
			paramBadAll++;
		}
		return true;
	}
	e.epoch = paramEpoch;
	e.wrapS = wrapS;
	e.wrapT = wrapT;
	e.minFilter = minFilter;
	e.aniso = aniso;
	paramSets++;
	return true;
}

void TexCache::wwParamFrame()
{
	WwDp::frame();	// lane GAPPERF-BINDLESS: last frame's draw packets are void
	WwBl::frame();	// lane BINDLESS2: the census line, the pool's stale spans
	if ( ++paramEpoch == 0 )
		paramEpoch = 1;
	if ( paramSkips || paramSets ) {
		extern bool wwFrameLog();	// gl/cellcull.cpp: WW_FRAME_LOG=0 keeps the per-camera lines out
		const QString line = QStringLiteral( "draw texshadow: %1 skips=%2 sets=%3 mismatches=%4 (all frames %5)" )
			.arg( wwTexShadowMode() == 2 ? QStringLiteral( "check" ) : QStringLiteral( "on" ) ).arg( paramSkips )
			.arg( paramSets ).arg( paramBad ).arg( paramBadAll );
		if ( line != paramLine && wwFrameLog() ) {
			paramLine = line;
			std::fprintf( stderr, "%s\n", line.toLocal8Bit().constData() );
		}
	}
	paramSkips = paramSets = paramBad = 0;
}

bool TexCache::bindCube( const QString & fname, const NifModel * nif, bool useSecondTexture )
{
	Tex *	tx = insertTex( fname );
	if ( !tx ) [[unlikely]]
		return false;
	tx->useTick = useTick;

	if ( !tx->isLoaded() ) [[unlikely]] {
		if ( tx->id[0] || !loadTex( *tx, nif ) )
			return false;
		tx = insertTex( fname );	// lane PERF1: loadTex may have grown (rehashed) the table
		if ( !tx ) [[unlikely]]
			return false;
	} else {
		if ( !tx->id[size_t(useSecondTexture)] ) [[unlikely]]
			return false;

		if ( !tx->target ) [[unlikely]]
			tx->target = GL_TEXTURE_CUBE_MAP;
	}

	glBindTexture( tx->target, tx->id[size_t(useSecondTexture)] );
	WWDP( WwDp::BindTex, GLint( tx->target ), GLint( tx->id[size_t(useSecondTexture)] ) );	// lane GAPPERF-BINDLESS
	wwCubeBound = ( tx->target == GL_TEXTURE_CUBE_MAP && !useSecondTexture ) ? tx->id[0] : 0;	// lane CELLUNI3

	if ( !tx->mipmaps ) [[unlikely]]
		return false;

	glEnable( GL_TEXTURE_CUBE_MAP_SEAMLESS );

	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	if ( WwDp::rec ) {
		WWDP( WwDp::Enable, GL_TEXTURE_CUBE_MAP_SEAMLESS );
		const GLint cp[5][2] = { { GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE }, { GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE },
			{ GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE }, { GL_TEXTURE_MAG_FILTER, GL_LINEAR }, { GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR } };
		for ( const auto & q : cp )
			WwDp::put( WwDp::TexParamI, GL_TEXTURE_CUBE_MAP, q[0], &q[1], 1 );
	}

	return true;
}

std::uint16_t TexCache::loadTex( Tex & txIn, const NifModel * nif )
{
	WwDp::abortRecording();	// lane GAPPERF-BINDLESS: a load is never replayed
	Tex *	t = &txIn;
	Tex::ImageInfo *	i = t->imageInfo;
	CellSpeed::Acc speedAcc( "texture loads (find, decode, upload)" );   // lane SPEED1

	if ( !isSupported( i->filename ) ) {
		t->id[0] = GLuint( -1 );
		return 0;
	}

	i->filepath = find( i->filename, nif );

	/* lane PERF1: before this file is read here (one at a time), the files the read-ahead workers already hold go
	 * up first, so their byte bound frees and they read on in parallel; each binds on this unit, and this file's
	 * bind below comes last, as the caller expects. Red WW_TEX_DRAIN_RED=1 keeps the old order. */
	WW_SWITCH_STATIC( bool, drainRed, wwEnvInt( "WW_TEX_DRAIN_RED" ) == 1 );
	if ( !drainRed && !draining && !pumpOwnsDrain && CellTexAhead::armedFor( nif ) ) {
		draining = true;
		const QStringList names = CellTexAhead::ready( nif, 4096 );
		for ( const QString & n : names ) {
			if ( n == i->filename )
				continue;
			Tex * o = insertTex( n );	// may rehash the table: t is found again below
			if ( o && o->imageInfo != i && !o->isLoaded() && !o->id[0] )
				loadTex( *o, nif );
		}
		draining = false;
		t = insertTex( i->filename );
		if ( !t || t->imageInfo != i ) [[unlikely]]
			return 0;
	}

	if ( !t->id[0] )
		glGenTextures( 1, t->id );
	paramForget( t->id[0] );	// lane PERF5: a load sets its own parameters

	if ( t->target )
		glBindTexture( t->target, t->id[0] );

	try
	{
		i->mipmaps = texLoad( nif, i->filepath, i->format, t->target, i->width, i->height, t->id );
		t->mipmaps = std::uint16_t( i->mipmaps );
	}
	catch ( QString & e )
	{
		i->status = e;
	}

	return t->mipmaps;
}

int TexCache::bind( const QModelIndex & iSource )
{
	auto nif = NifModel::fromValidIndex(iSource);
	if ( nif ) {
		if ( nif->get<quint8>( iSource, "Use External" ) == 0 ) {
			QModelIndex iData = nif->getBlockIndex( nif->getLink( iSource, "Pixel Data" ) );

			if ( iData.isValid() ) {
				Tex &	tx = embedTextures[iData];
				Tex::ImageInfo *	i = tx.imageInfo;

				if ( !i ) {
					i = new Tex::ImageInfo;
					tx.imageInfo = i;
					try
					{
						glGenTextures( 1, tx.id );
						paramForget( tx.id[0] );	// lane PERF5
						glBindTexture( GL_TEXTURE_2D, tx.id[0] );
						i->mipmaps = texLoad( iData, i->format, tx.target, i->width, i->height, tx.id );
						tx.mipmaps = std::uint16_t( i->mipmaps );
					}
					catch ( QString & e ) {
						i->status = e;
					}
				} else {
					glBindTexture( GL_TEXTURE_2D, tx.id[0] );
				}
				paramForget( tx.id[0] );	// lane PERF5: not the shadowed bind

				return tx.mipmaps;
			}
		} else if ( !nif->get<QString>( iSource, "File Name" ).isEmpty() ) {
			return bind( nif->get<QString>( iSource, "File Name" ), nif );
		}
	}

	return 0;
}

void TexCache::flush()
{
	paramShadow.clear();	// lane PERF5
	WwDp::invalidate();	// lane GAPPERF-BINDLESS: packets name these textures
	WwBl::forgetAll();	// lane BINDLESS2: no handle may stay resident on a deleted texture
	for ( size_t i = 0; i <= textureHashMask; i++ ) {
		Tex &	tx = textures[i];
		if ( tx.isLoaded() ) {
			PassTime::texResidentDrop( tx.id[0] );	// lane STREAM1
			glDeleteTextures( ( !tx.id[1] ? 1 : 2 ), tx.id );
		}
		if ( tx.imageInfo )
			delete tx.imageInfo;
	}
	textureHashMask = 0;
	textureCount = 0;
	rehashTextures();

	for ( Tex & tx : embedTextures ) {
		if ( tx.id[0] ) {
			PassTime::texResidentDrop( tx.id[0] );	// lane STREAM1
			glDeleteTextures( ( !tx.id[1] ? 1 : 2 ), tx.id );
		}
		if ( tx.imageInfo )
			delete tx.imageInfo;
	}
	embedTextures.clear();
}

int TexCache::evictUnused( std::uint32_t tick, QStringList * names )
{
	int n = 0;
	for ( size_t i = 0; i <= textureHashMask; i++ ) {
		Tex &	tx = textures[i];
		if ( !tx.nameLen || !tx.isLoaded() || !tx.mipmaps || tx.useTick >= tick )
			continue;
		PassTime::texResidentDrop( tx.id[0] );	// lane STREAM1
		paramForget( tx.id[0] );	// lane PERF5
		WwDp::invalidate();	// lane GAPPERF-BINDLESS
		WwBl::forget( tx.id[0] );	// lane BINDLESS2
		WwBl::forget( tx.id[1] );
		glDeleteTextures( ( !tx.id[1] ? 1 : 2 ), tx.id );
		tx.id[0] = tx.id[1] = 0;
		tx.mipmaps = 0;
		tx.target = 0;
		if ( names )
			names->append( tx.filename().toString() );
		n++;
	}
	return n;
}

int TexCache::wwPump( const NifModel * nif, qint64 budgetBytes )
{
	if ( draining || !CellTexAhead::armedFor( nif ) )
		return 0;
	const QStringList names = CellTexAhead::ready( nif, 512 );
	if ( names.isEmpty() )
		return 0;
	GLint	unit = 0, b2d = 0, bCube = 0;	// a load binds on the active unit: put back what was bound there
	glGetIntegerv( GL_ACTIVE_TEXTURE, &unit );
	glGetIntegerv( GL_TEXTURE_BINDING_2D, &b2d );
	glGetIntegerv( GL_TEXTURE_BINDING_CUBE_MAP, &bCube );
	PassTime::CacheCounts &	pc = PassTime::cacheCounts();
	const qint64	bytes0 = pc.texBytes;
	int	n = 0;
	draining = true;
	for ( const QString & name : names ) {
		if ( n > 0 && pc.texBytes - bytes0 >= budgetBytes )
			break;
		Tex *	tx = insertTex( name );
		if ( !tx )
			continue;
		if ( tx->isLoaded() || tx->id[0] ) {
			CellTexAhead::drop( nif, name );	// loaded already (another name of the file): free its bytes
			continue;
		}
		tx->useTick = useTick;
		loadTex( *tx, nif );
		n++;
	}
	draining = false;
	pc.texPumped += n;
	pc.texPumpedBytes += pc.texBytes - bytes0;
	if ( fn )
		fn->glActiveTexture( GLenum( unit ) );
	glBindTexture( GL_TEXTURE_2D, GLuint( b2d ) );
	glBindTexture( GL_TEXTURE_CUBE_MAP, GLuint( bCube ) );
	return n;
}

void TexCache::setNifFolder( const QString & folder )
{
	(void) folder;
	flush();
	emit sigRefresh();
}

QString TexCache::info( const QModelIndex & iSource )
{
	QString temp;

	auto nif = NifModel::fromValidIndex(iSource);
	if ( nif ) {
		if ( nif->get<quint8>( iSource, "Use External" ) == 0 ) {
			QModelIndex iData = nif->getBlockIndex( nif->getLink( iSource, "Pixel Data" ) );

			const Tex::ImageInfo *	i = nullptr;
			if ( iData.isValid() )
				i = embedTextures.value( iData ).imageInfo;
			if ( i ) {
				temp = QString( "Embedded texture: %1\nWidth: %2\nHeight: %3\nMipmaps: %4" )
						.arg( i->format.toString() )
						.arg( i->width )
						.arg( i->height )
						.arg( i->mipmaps );
			} else {
				temp = QString( "Embedded texture invalid" );
			}
		} else {
			QString filename = nif->get<QString>( iSource, "File Name" );
			const Tex::ImageInfo *	i = getTextureInfo( filename );
			if ( i ) {
				temp = QString( "External texture file: %1\nTexture path: %2\nFormat: %3\nWidth: %4\nHeight: %5\nMipmaps: %6" )
						.arg( i->filename )
						.arg( i->filepath )
						.arg( i->format.toString() )
						.arg( i->width )
						.arg( i->height )
						.arg( i->mipmaps );
			} else {
				temp = QString( "External texture file '%1' not found" ).arg( filename );
			}
		}
	}

	return temp;
}

bool TexCache::exportFile( const QModelIndex & iSource, QString & filepath )
{
	Tex &	tx = embedTextures[iSource];

	if ( !tx.imageInfo )
		tx.imageInfo = new Tex::ImageInfo;

	return tx.saveAsFile( *this, iSource, filepath );
}

bool TexCache::importFile( NifModel * nif, const QModelIndex & iSource, QModelIndex & iData )
{
	// auto nif = NifModel::fromIndex(iSource);
	if ( nif && iSource.isValid() ) {
		if ( nif->get<quint8>( iSource, "Use External" ) == 1 ) {
			QString filename = nif->get<QString>( iSource, "File Name" );
			//qDebug() << "TexCache::importFile: Texture has filename (from NIF) " << filename;
			const Tex::ImageInfo *	i = getTextureInfo( filename );
			if ( i )
				return i->savePixelData( *this, nif, iData );
		}
	}

	return false;
}


/*
*  TexCache::Tex
*/

bool TexCache::Tex::saveAsFile( TexCache & t, const QModelIndex & index, QString & savepath )
{
	ImageInfo *	i = imageInfo;
	i->mipmaps = t.texLoad( index, i->format, target, i->width, i->height, id );
	mipmaps = std::uint16_t( i->mipmaps );

	if ( savepath.toLower().endsWith( QLatin1StringView(".tga") ) ) {
		return TexCache::texSaveTGA( index, savepath, i->width, i->height );
	}

	return TexCache::texSaveDDS( index, savepath, i->width, i->height, i->mipmaps );
}

bool TexCache::Tex::ImageInfo::savePixelData( TexCache & t, NifModel * nif, QModelIndex & iData ) const
{
	// gltexloaders function goes here
	//qDebug() << "TexCache::Tex:savePixelData: Packing" << iSource << "from file" << filepath << "to" << iData;
	try {
		return t.texSaveNIF( nif, filepath, iData );
	} catch ( QString & e ) {
		qCWarning( nsIo ) << e;
	}
	return false;
}

bool TexCache::loadSettings( QSettings & settings )
{
	int	tmp = settings.value( "Settings/Render/General/Ibl Cube Map Resolution", 2 ).toInt();
	tmp = 128 << std::clamp< int >( tmp, 0, 4 );
	bool	r = ( tmp != pbrCubeMapResolution );
	pbrCubeMapResolution = tmp;

	tmp = settings.value( "Settings/Render/General/Ibl Importance Sample Cnt", 2 ).toInt();
	tmp = 64 << std::clamp< int >( tmp, 0, 6 );
	r = r | ( tmp != pbrImportanceSamples );
	pbrImportanceSamples = tmp;

	tmp = settings.value( "Settings/Render/General/Hdr Tone Map", 8 ).toInt();
	tmp = std::clamp< int >( tmp, 0, 16 );
	r = r | ( tmp != hdrToneMapLevel );
	hdrToneMapLevel = tmp;

	return r;
}

