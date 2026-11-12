/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2024, NIF File Format Library and Tools
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

#include "glcontext.hpp"
#include "wwloadwatch.h"	// lane PERFLOAD1: the log drain before a hard exit
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store
#include "cellunits.h"	// lane FIX1101A lodch: the far stream's unit
#include "ddstxt16.hpp"
#include "libfo76utils/src/filebuf.hpp"
#include "model/nifmodel.h"
#include "gltex.h"
#include "glproperty.h"
#include "harnesswindow.h"
#include "features/shared/cellpasstime.h"	// lane PERF1
#include "features/near/celldrawpacket.h"	// lane GAPPERF-BINDLESS: the setters record while a draw packet records
#include "features/near/cellbindless.h"	// lane BINDLESS2
#include "progcachekeep.h"	// lane BUGS1102C
#include "gpustage.h"	// MODULES1: static buffers filled by a GPU copy (no driver RAM copy)
#include "wwuploadcap.h"	// lane EMPTY240 R4-1: the paint's upload budget

// lane CELLUNI1: inside the cell-uniform scope an unchanged value is not uploaded again (celluniform.h)
#define WWCU( k, l, v, n ) WwCellUni::want( wwUni, l, WwCellUni::k, v, n, WwDp::rec != nullptr )
// lane PERFFIX3: the CPU mirror of what a location holds (celluniform.h Mirror), kept only on programs that asked for it
#define WWMI( k, l, v, n ) if ( wwMirror.on ) wwMirror.put( l, WwCellUni::k, v, n )
#define WWMF( l, n ) if ( wwMirror.on ) wwMirror.forget( l, n )
bool rtddgiCellShaderDefine();	// lane RTXGI1 M2: src/features/gi/rtddgicell.cpp (the Ray-traced GI row / WW_CELL_RTDDGI)
int rtddgiCellCascades();	// lane RTXCASCADE1: src/features/gi/rtddgicell.h (WW_CELL_RTDDGI_CASCADES)
bool rtddgiRestirShaderDefine();	// lane RESTIRDI1: src/features/gi/rtddgicell.cpp (the RT quality row Medium+ / WW_CELL_RT_PRESET)
bool wwAirZoneShaderDefine();	// lane AIRZONE1: src/features/air/cellairzone.cpp (the Air zones row / WW_CELL_AIRZONE)
bool wwCellViewsShaderDefine();	// lane FO4CELLGI1: src/gl/celllights.cpp (WW_CELL_VIEWS, the probe and pass views)
bool wwDebugViewShaderDefine();	// lane DEBUGVIEW1: src/features/overlays/debugview.cpp (WW_DEBUG_VIEW, Show > Debug view)
bool wwFogLeakShaderDefine();	// lane FOGLEAK1: src/features/air/cellvolair.cpp (WW_CELL_FOGLEAK, the fog leak fix; off by default)
bool wwAirBounceShaderDefine();	// lane AIRBOUNCE1: src/features/air/cellvolair.cpp (WW_CELL_AIR_BOUNCE_LAMPS, the lamps' floor bounce; off by default)
bool wwAirSkyViewShaderDefine();	// lane DARKAIR1: src/features/air/cellvolair.cpp (WW_CELL_AIR_SKYVIEW, the air's sky visibility; on since DARKAIR2, 0 compiles it out)
int wwCellGiVis();	// lane PERF1: src/gl/celllights.cpp (1: the march, WW_CELL_GI_MARCH)
bool wwCellPrtOn();	// lane PRTOFF1: src/gl/celllights.cpp (the old baked probe GI, only under its red)

#include <QElapsedTimer>
#include <QOpenGLContext>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QOpenGLContext>
#include <QTextStream>
#include <QHash>
#include <functional>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>
#include <QThread>
#include <algorithm>
#include <atomic>
#include <vector>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#  include <QOpenGLVersionFunctionsFactory>
#endif
#if defined( __F16C__ ) && defined( __AVX__ )
#  include <immintrin.h>
#endif
#ifndef GL_HALF_FLOAT
#  define GL_HALF_FLOAT 0x140B
#endif


static const QString white = "#FFFFFFFF";
static const QString black = "#FF000000";
static const QString lighting = "#FF00F040";
static const QString reflectivity = "#FF0A0A0A";
static const QString default_n = "#FFFF8080";
static const QString default_ns = "#FFFF8080n";


// Templates

template <typename T> inline bool NifSkopeOpenGLContext::ConditionSingle::compare( T a, T b ) const
{
	switch ( comp ) {
	case EQ:
		return a == b;
	case NE:
		return a != b;
	case LE:
		return a <= b;
	case GE:
		return a >= b;
	case LT:
		return a < b;
	case GT:
		return a > b;
	case AND:
		return a & b;
	case NAND:
		return !(a & b);
	default:
		return true;
	}
}

template <> inline bool NifSkopeOpenGLContext::ConditionSingle::compare( float a, float b ) const
{
	switch ( comp ) {
	case EQ:
		return a == b;
	case NE:
		return a != b;
	case LE:
		return a <= b;
	case GE:
		return a >= b;
	case LT:
		return a < b;
	case GT:
		return a > b;
	default:
		return true;
	}
}

template <> inline bool NifSkopeOpenGLContext::ConditionSingle::compare( QString a, QString b ) const
{
	switch ( comp ) {
	case EQ:
		return a == b;
	case NE:
		return a != b;
	default:
		return false;
	}
}


const QHash<NifSkopeOpenGLContext::ConditionSingle::Type, QString> NifSkopeOpenGLContext::ConditionSingle::compStrs{
	{ EQ,  " == " },
	{ NE,  " != " },
	{ LE,  " <= " },
	{ GE,  " >= " },
	{ LT,  " < " },
	{ GT,  " > " },
	{ AND, " & " },
	{ NAND, " !& " }
};

NifSkopeOpenGLContext::ConditionSingle::ConditionSingle( const QString & line, bool neg ) : invert( neg )
{
	QHashIterator<Type, QString> i( compStrs );
	int pos = -1;

	while ( i.hasNext() ) {
		i.next();
		pos = line.indexOf( i.value() );

		if ( pos > 0 )
			break;
	}

	if ( pos > 0 ) {
		left  = line.left( pos ).trimmed();
		right = line.right( line.length() - pos - i.value().length() ).trimmed();

		if ( right.startsWith( "\"" ) && right.endsWith( "\"" ) )
			right = right.mid( 1, right.length() - 2 );

		comp = i.key();
	} else {
		left = line;
		comp = NONE;
	}
}

QModelIndex NifSkopeOpenGLContext::ConditionSingle::getIndex( const NifModel * nif, const QVector<QModelIndex> & iBlocks, QString blkid ) const
{
	QString childid;

	if ( blkid.startsWith( QLatin1StringView("HEADER/") ) ) {
		auto blk = blkid.remove( "HEADER/" );
		if ( blk.contains("/") ) {
			auto blks = blk.split( "/" );
			return nif->getIndex( nif->getIndex( nif->getHeaderIndex(), blks.at(0) ), blks.at(1) );
		}
		return nif->getIndex( nif->getHeaderIndex(), blk );
	}

	int pos = blkid.indexOf( QChar('/') );

	if ( pos > 0 ) {
		childid = blkid.right( blkid.length() - pos - 1 );
		blkid = blkid.left( pos );
	}

	for ( QModelIndex iBlock : iBlocks ) {
		if ( nif->blockInherits( iBlock, blkid ) ) {
			if ( childid.isEmpty() )
				return iBlock;

			return nif->getIndex( iBlock, childid );
		}
	}
	return QModelIndex();
}

bool NifSkopeOpenGLContext::ConditionSingle::eval( const NifModel * nif, const QVector<QModelIndex> & iBlocks ) const
{
	if ( left == "BSVersion" )
		return compare( nif->getBSVersion(), right.toUInt( nullptr, 0 ) ) ^ invert;

	QModelIndex iLeft = getIndex( nif, iBlocks, left );

	if ( !iLeft.isValid() )
		return invert;

	if ( comp == NONE )
		return !invert;

	const NifItem * item = nif->getItem( iLeft );
	if ( !item )
		return false;

	if ( item->isString() )
		return compare( item->getValueAsString(), right ) ^ invert;
	else if ( item->isCount() )
		return compare( item->getCountValue(), right.toULongLong( nullptr, 0 ) ) ^ invert;
	else if ( item->isFloat() )
		return compare( item->getFloatValue(), (float)right.toDouble() ) ^ invert;
	else if ( item->isFileVersion() )
		return compare( item->getFileVersionValue(), right.toUInt( nullptr, 0 ) ) ^ invert;
	else if ( item->valueType() == NifValue::tBSVertexDesc )
		return compare( (uint) item->get<BSVertexDesc>().GetFlags(), right.toUInt( nullptr, 0 ) ) ^ invert;

	return false;
}

bool NifSkopeOpenGLContext::ConditionGroup::eval( const NifModel * nif, const QVector<QModelIndex> & iBlocks ) const
{
	if ( conditions.isEmpty() )
		return true;

	if ( isOrGroup() ) {
		for ( Condition * cond : conditions ) {
			if ( cond->eval( nif, iBlocks ) )
				return true;
		}
		return false;
	} else {
		for ( Condition * cond : conditions ) {
			if ( !cond->eval( nif, iBlocks ) )
				return false;
		}
		return true;
	}
}

void NifSkopeOpenGLContext::ConditionGroup::addCondition( Condition * c )
{
	conditions.append( c );
}

/* lane STARTSPEED1: a shader's compile waits until a program links from it. The program binary cache key needs only
 * the source hash, so with every program's binary in shadercache/ no shader is compiled at start; a cache miss, or a
 * binary the driver refuses, compiles that program's attached shaders first (wwCompileDeferred). The prepared source
 * waits here, by GL shader id; updateShaders() drops what no link asked for. Red: WW_STARTUP_EAGER=1, every shader
 * compiles at load, as before. */
bool wwStartupEager()
{
	static const bool eager = qgetenv( "WW_STARTUP_EAGER" ).trimmed() == "1";
	return eager;
}

// lane STARTSPEED1: FNV-1a 64 of a cached program binary, checked before the driver sees it
static quint64 wwProgSum( const char * p, qsizetype n )
{
	quint64	h = 14695981039346656037ull;
	for ( qsizetype i = 0; i < n; i++ ) {
		h ^= quint8( p[i] );
		h *= 1099511628211ull;
	}
	return h;
}

namespace {
struct WwDeferredShader {
	std::string	name;
	QByteArray	src;
	std::function< void( const QString & ) >	onError;
};
QHash< unsigned int, WwDeferredShader > &	wwDeferredShaders()
{
	static QHash< unsigned int, WwDeferredShader >	h;
	return h;
}
int	wwShadersCompiled = 0;
int	wwShadersLoaded = 0;
}

// compiles the waiting shaders attached to program prog; false (err set) when one fails to compile
static bool wwCompileDeferred( NifSkopeOpenGLContext::GLFunctions * f, unsigned int prog, QString & err )
{
	auto &	h = wwDeferredShaders();
	if ( h.isEmpty() || !prog )
		return true;
	GLint	n = 0;
	f->glGetProgramiv( prog, GL_ATTACHED_SHADERS, &n );
	if ( n <= 0 )
		return true;
	std::vector< GLuint >	ids( size_t( n ), 0 );
	GLsizei	got = 0;
	f->glGetAttachedShaders( prog, GLsizei( n ), &got, ids.data() );
	bool	ok = true;
	for ( GLsizei i = 0; i < got; i++ ) {
		auto	it = h.find( ids[i] );
		if ( it == h.end() )
			continue;
		WwDeferredShader	d = it.value();
		h.erase( it );
		const char *	src = d.src.constData();
		f->glShaderSource( ids[i], 1, &src, (GLint *) 0 );
		f->glCompileShader( ids[i] );
		wwShadersCompiled++;
		GLint	result = GL_FALSE;
		f->glGetShaderiv( ids[i], GL_COMPILE_STATUS, &result );
		if ( result != GL_TRUE ) {
			GLint	logLen = 0;
			f->glGetShaderiv( ids[i], GL_INFO_LOG_LENGTH, &logLen );
			QByteArray	log( qMax( logLen, 1 ), '\0' );
			f->glGetShaderInfoLog( ids[i], logLen, 0, log.data() );
			if ( d.onError )
				d.onError( QString::fromLatin1( log.constData() ) );
			err = QString( "depends on shader %1 which was not compiled successful" ).arg( QString::fromStdString( d.name ) );
			ok = false;
		}
	}
	return ok;
}

NifSkopeOpenGLContext::Shader::Shader( const std::string_view & n, unsigned int t, GLFunctions * fn )
	: f( fn ), name( n ), id( 0 ), status( false ), isProgram( !t )
{
	if ( t )
		id = f->glCreateShader( t );
}

NifSkopeOpenGLContext::Shader::~Shader()
{
	if ( id && !isProgram )
		f->glDeleteShader( id );
}

void NifSkopeOpenGLContext::Shader::clear()
{
	if ( isProgram ) {
		static_cast< Program * >( this )->clear();
		return;
	}
	if ( id ) {
		wwDeferredShaders().remove( id );
		f->glDeleteShader( id );
		id = 0;
	}
	status = false;
	isProgram = false;
}

void NifSkopeOpenGLContext::Shader::printCompileError( const QString & err )
{
	status = false;
	QString	tmp( QString::fromUtf8( name.data(), qsizetype( name.length() ) ) );
	tmp.append( QLatin1StringView( ":\r\n\r\n" ) );
	tmp.append( err );
	// lane VOLAIR1 (10-15): also into the log, where a headless run's notes see it (the box alone hid a
	// program that failed to link, and every draw that wanted it fell back)
	qWarning().noquote() << "shader compile/link error:" << tmp;
	/* THE SHADER GUARD (lane VOLAIR1, 10-15, coordinator ruling: no silent grey pictures ever again).
	 *
	 * A program that fails to compile or link leaves every draw that wanted it on a fallback: the
	 * picture comes out untextured or grey and still LOOKS like a measurement. In a harness run
	 * (wwHarnessRun(): any WW_* key or -no-gui, the predicate saveUi() already uses) the run stops
	 * here, before any picture can be written: the error text goes to the notes after a fixed
	 * "SHADER REFUSED:" prefix and the exe exits 87. Every gate then fails on the missing picture
	 * and tests/spells/_harness.sh ww_notes_shader_refusal prints why. A user session (no WW_* key)
	 * keeps the message box and goes on. There is no switch to turn this off; the red that proves
	 * it trips is WW_SHADER_RED=<shader file> (Shader::load), run by tests/spells/shader_guard.sh. */
	if ( wwHarnessRun() && wwLiveSessionOn() ) {
		/* fix 10-22: in a live session the guard refuses the ROW, not the process: the driver skips that row's
		 * pictures (so still no grey picture is written) and the next row's reset reloads the programs */
		QString why;
		for ( const QString & l : err.split( QLatin1Char( '\n' ) ) )
			if ( l.contains( QLatin1StringView( "error" ), Qt::CaseInsensitive ) ) {
				why = l.simplified();
				break;
			}
		const QString shortText = QString::fromUtf8( name.data(), qsizetype( name.length() ) ) + QStringLiteral( ": " )
			+ ( why.isEmpty() ? QString( err ).simplified().left( 200 ) : why );
		std::fprintf( stderr, "SHADER REFUSED (live row): %s\n", qPrintable( shortText ) );
		std::fflush( stderr );
		wwLiveShaderRefused( shortText );
		return;
	}
	if ( wwHarnessRun() ) {
		const QByteArray line = ( QStringLiteral( "SHADER REFUSED: " ) + QString( tmp ).simplified() ).toUtf8();
		std::fprintf( stderr, "%s\n", line.constData() );
		std::fflush( stderr );
		( wwStderrPumpDrain(), std::_Exit( 87 ) );
	}
	Message::append( QObject::tr( "There were errors during shader compilation" ), tmp );
}

/* MODULES1: a module's shaders sit in shaders/features/<module>/ under their own names (a .prog lists names, an
 * #include names a file). A name missing beside the file that asks for it is looked up in the shaders folder and its
 * modules' folders, from the asking file's folder up two levels (tests/spells/shader_link_check.py shader_path). */
QString wwShaderPath( const QString & filepath )
{
	if ( QFileInfo::exists( filepath ) )
		return filepath;
	const QFileInfo	fi( filepath );
	QDir	root( fi.absolutePath() );
	for ( int up = 0; up < 3; up++ ) {
		if ( root.exists( fi.fileName() ) )
			return root.filePath( fi.fileName() );
		const QDir	feat( root.filePath( "features" ) );
		for ( const QString & m : feat.entryList( QDir::Dirs | QDir::NoDotAndDotDot ) ) {
			const QString	p = feat.filePath( m + QChar( '/' ) + fi.fileName() );
			if ( QFileInfo::exists( p ) )
				return p;
		}
		if ( !root.cdUp() )
			break;
	}
	return filepath;
}

static QByteArray loadShaderFile( const QString & asked, int includeDepth = 0 )
{
	const QString	filepath = wwShaderPath( asked );
	QFile file( filepath );

	if ( !file.open( QIODevice::ReadOnly ) )
		throw QString( "couldn't open %1 for read access" ).arg( filepath );

	QByteArray	data = file.readAll();
	/* An INCLUDED file loses its own #version line (lane FOG1), so a variant can
	 * be `#version`, a `#define`, then `#include` of a whole shader: fo4_fog.frag
	 * is fo4_default.frag with WW_FOG defined. No shipped include starts with one. */
	if ( includeDepth > 0 && data.startsWith( "#version" ) ) {
		const qsizetype	eol = data.indexOf( '\n' );
		data.remove( 0, eol < 0 ? data.size() : eol + 1 );
	}
	qsizetype	n = 0;
	while ( n < data.size() && ( n = data.indexOf( '#', n ) ) >= 0 ) {
		qsizetype	includePos = n;
		bool	isInclude = true;
		while ( includePos > 0 ) {
			char	c = data.at( includePos - 1 );
			if ( c == '\n' )
				break;
			includePos--;
			if ( c == ' ' || c == '\t' )
				continue;
			isInclude = false;
			break;
		}
		n++;
		if ( !isInclude )
			continue;
		while ( n < data.size() && ( data.at( n ) == ' ' || data.at( n ) == '\t' ) )
			n++;
		if ( !( ( n + 7 ) <= data.size() && std::string_view( data.constData() + n, 7 ) == "include" ) )
			continue;
		n = n + 7;

		QString	includeFileName( QDir::cleanPath( filepath ) );
		includeFileName.truncate( includeFileName.lastIndexOf( QChar('/') ) + 1 );
		int	includeState = 0;
		for ( ; n < data.size(); n++ ) {
			char	c = data.at( n );
			if ( c == '"' ) {
				if ( includeState > 1 )
					break;
				includeState++;
				continue;
			} else if ( c == '\n' ) {
				includeState++;
				break;
			} else if ( !( c == ' ' || c == '\t' || c == '\r' ) && includeState != 1 ) {
				break;
			}
			if ( includeState == 1 )
				includeFileName += QChar( c );
		}
		if ( includeState != 3 || includeFileName.isEmpty() || includeFileName.endsWith( QChar('/') ) )
			throw QString( "invalid #include syntax in %1" ).arg( filepath );
		if ( includeDepth >= 16 )
			throw QString( "%1: #include recursion depth is too high" ).arg( filepath );
		qsizetype	oldSize = data.size();
		data.remove( includePos, n - includePos );
		data.insert( includePos, loadShaderFile( includeFileName, includeDepth + 1 ) );
		n = n + ( data.size() - oldSize );
	}

	return data;
}

bool NifSkopeOpenGLContext::Shader::load( const QString & filepath )
{
	try
	{
		QByteArray	data = loadShaderFile( filepath );
		// red for the shader guard (printCompileError): WW_SHADER_RED=<file name> breaks that one shader
		if ( const QString red = wwEnvStr( "WW_SHADER_RED" );
			!red.isEmpty() && QFileInfo( filepath ).fileName().compare( red, Qt::CaseInsensitive ) == 0 ) {
			const qsizetype eol = data.startsWith( "#version" ) ? data.indexOf( '\n' ) + 1 : 0;
			data.insert( eol, "#error WW_SHADER_RED broke this shader on purpose\n" );
		}
		// lane PERF1: WW_CELL_GI_VIS=march (red), or a march-only WW_CELL_PV_RED: the per-pixel chain in every shader (cell_lights.glsl)
		const bool giMarch = wwCellGiVis() == 1;	// not static: a live set reloads the shaders (stage shaders)
		if ( giMarch && data.startsWith( "#version" ) )
			data.insert( data.indexOf( '\n' ) + 1, "#define WW_CELL_GI_MARCH 1\n" );
		/* lane PRTOFF1: the old baked probe GI's reads (cell_lights.glsl), compiled only under its red WW_CELL_PRT_RED=1;
		 * without the define no program holds a grid read (not a "WW_" literal: the switch survey would list it) */
		if ( wwCellPrtOn() && data.startsWith( "#version" ) && data.contains( "_CELL_PRT" ) )
			data.insert( data.indexOf( '\n' ) + 1, "#define WW_CELL_PRT 1\n" );
		// lane RTXGI1 M2: the ray-traced GI volume's read (cell_lights.glsl + cell_ddgi.glsl); not static: a set reloads the shaders
		if ( !giMarch && rtddgiCellShaderDefine() && data.startsWith( "#version" ) && data.contains( "_CELL_RTDDGI" ) ) {
			data.insert( data.indexOf( '\n' ) + 1, "#define WW_CELL_RTDDGI 1\n" );
			// lane RTXCASCADE1: 2..4 nested volumes (cell_ddgi.glsl's cascades' read); 1 = M3's program, not a line added
			if ( rtddgiCellCascades() > 1 )
				data.insert( data.indexOf( '\n' ) + 1, "#define WW_CELL_RTDDGI_CASCADES 1\n" );
			/* lane DDGIREADPERF1: fo4_cell / pbrm_cell read the volume once (cellGiFirst); WW_CELL_RTDDGI_ONEREAD=0 is the red
			 * (a read inlined at every cellGiSample site, main before the lane). Not static: a set reloads the shaders */
			if ( qgetenv( "WW_CELL_RTDDGI_ONEREAD" ).trimmed() == "0" && data.contains( "_CELL_GI_ONEREAD" ) )
				data.insert( data.indexOf( '\n' ) + 1, "#define WW_CELL_GI_ONEREAD_RED 1\n" );
			/* lane DDGIREADPERF2: cellGiFirst also takes the screen texel once; WW_CELL_RTDDGI_ONEHALF=0 is the red (the
			 * upsample inlined at every cellGiSample site and cellSpecSky). Not static: a set reloads the shaders */
			if ( qgetenv( "WW_CELL_RTDDGI_ONEHALF" ).trimmed() == "0" && data.contains( "_CELL_GI_ONEREAD" ) )
				data.insert( data.indexOf( '\n' ) + 1, "#define WW_CELL_GI_ONEHALF_RED 1\n" );
		}
		// lane RESTIRDI1: the RT quality preset's ray-traced shadow ratio (cell_lights.glsl cellShadowF); not static: the row reloads
		if ( !giMarch && rtddgiRestirShaderDefine() && data.startsWith( "#version" ) && data.contains( "_CELL_RESTIR" ) ) {
			data.insert( data.indexOf( '\n' ) + 1, "#define WW_CELL_RESTIR 1\n" );
			// lane PRESETPERF1 (PRESETPERM1): the red keeps the dead cube taps in the Medium+ programs (cellShadowF); per load
			if ( qEnvironmentVariableIntValue( "WW_PRESETPERM1_RED" ) != 0 )
				data.insert( data.indexOf( '\n' ) + 1, "#define WW_CELL_RESTIR_CUBES_RED 1\n" );
		}
		// lane AIRZONE1: the hand-placed volumes' read (cell_airzone.glsl, via cell_volair.frag and cell_volpart.glsl); not static:
		// the row's tick reloads the shaders
		if ( wwAirZoneShaderDefine() && data.startsWith( "#version" ) && data.contains( "_CELL_AIRZONE" ) )
			data.insert( data.indexOf( '\n' ) + 1, "#define WW_CELL_AIRZONE 1\n" );
		// lane FO4CELLGI1: fo4_default.frag's probe and pass views only while one is asked; not static: a set reloads the shaders
		if ( wwCellViewsShaderDefine() && data.startsWith( "#version" ) && data.contains( "_CELL_VIEWS" ) )
			data.insert( data.indexOf( '\n' ) + 1, "#define WW_CELL_VIEWS 1\n" );
		// lane DEBUGVIEW1: the debug views (debug_view.glsl) only while one is on; not static: a set reloads the shaders
		if ( wwDebugViewShaderDefine() && data.startsWith( "#version" ) && data.contains( "_DEBUG_VIEWS" ) )
			data.insert( data.indexOf( '\n' ) + 1, "#define WW_DEBUG_VIEWS 1\n" );
		// lane FOGLEAK1: the fog leak fix (cell_volair.frag WW_AIR_LEAK, lookdev_fog.glsl's far tail); not static: read per load
		if ( wwFogLeakShaderDefine() && data.startsWith( "#version" ) && data.contains( "_CELL_FOGLEAK" ) )
			data.insert( data.indexOf( '\n' ) + 1, "#define WW_CELL_FOGLEAK 1\n" );
		// lane AIRBOUNCE1: the lamps' floor bounce in the air (cell_volair.frag); not static: read per load
		if ( wwAirBounceShaderDefine() && data.startsWith( "#version" ) && data.contains( "_CELL_AIR_BOUNCE" ) )
			data.insert( data.indexOf( '\n' ) + 1, "#define WW_CELL_AIR_BOUNCE 1\n" );
		// lane DARKAIR1: the air's ambient takes the volume's sky visibility (cell_volair.frag airSkyView); read per load
		if ( wwAirSkyViewShaderDefine() && data.startsWith( "#version" ) && data.contains( "_CELL_AIR_SKYVIEW" ) )
			data.insert( data.indexOf( '\n' ) + 1, "#define WW_CELL_AIR_SKYVIEW 1\n" );
		// lane PERF3: the cascades from one per-frame block, in the sources that ask for it (WW_FRAME_UBO_RED=1: plain uniforms)
		static const bool frameUbo = qgetenv( "WW_FRAME_UBO" ).trimmed() != "0" && qgetenv( "WW_FRAME_UBO_RED" ).trimmed() != "1"
			&& qgetenv( "WW_PERF3_RED" ).trimmed() != "1";
		if ( frameUbo && data.startsWith( "#version" ) && data.contains( "WW_FRAME_UBO" ) )
			data.insert( data.indexOf( '\n' ) + 1, "#define WW_FRAME_UBO 1\n" );
		// lane WATERDATA1: the water-info probes' build (probes 28..31), harness only
		const bool infoProbe = !qgetenv( "WW_CELL_WATER_INFO_PROBE" ).trimmed().isEmpty();
		if ( infoProbe && data.startsWith( "#version" ) && data.contains( "_WATER_INFO_PROBE" ) )	// not a "WW_" literal: the switch survey would list it
			data.insert( data.indexOf( '\n' ) + 1, "#define WW_WATER_INFO_PROBE 1\n" );

		if ( name.ends_with( "frag" ) ) {
			if ( qsizetype n = data.indexOf( "NUM_TEXTURE_UNITS" ); n >= 0 )
				data.replace( n, 17, QByteArray::number( TexCache::num_texture_units - 2 ) );
		}

		const char * src = data.constData();
		srcHash = 0xCBF29CE484222325ULL;
		for ( char c : std::string( name ) + '\n' + data.toStdString() )
			srcHash = ( srcHash ^ (unsigned char) c ) * 0x100000001B3ULL;

		wwShadersLoaded++;
		if ( !wwStartupEager() ) {
			wwDeferredShaders().insert( id, WwDeferredShader{ std::string( name ), data, [this]( const QString & e ) { printCompileError( e ); } } );
			status = true;
			return true;
		}
		wwShadersCompiled++;
		f->glShaderSource( id, 1, &src, (GLint *) 0 );
		f->glCompileShader( id );

		GLint result;
		f->glGetShaderiv( id, GL_COMPILE_STATUS, &result );

		if ( result != GL_TRUE ) {
			GLint logLen;
			f->glGetShaderiv( id, GL_INFO_LOG_LENGTH, &logLen );
			char * log = new char[ logLen ];
			f->glGetShaderInfoLog( id, logLen, 0, log );
			QString errlog( log );
			delete[] log;
			throw errlog;
		}
	}
	catch ( QString & err )
	{
		printCompileError( err );
		return false;
	}
	status = true;
	return true;
}


inline size_t NifSkopeOpenGLContext::Program::getUniLocationMapAllocSize( unsigned int m )
{
	size_t	n = size_t( m ) + 1;
	return ( n << 1 ) - ( n >> 2 );
}

NifSkopeOpenGLContext::Program::Program( const std::string_view & n, GLFunctions * fn )
	: Shader( n, 0, fn ), nextProgram( nullptr )
{
	uniLocationsMap = new UniformLocationMapItem[getUniLocationMapAllocSize( 63 )];
	uniLocationsMapMask = 63;
	uniLocationsMapSize = 0;
	id = f->glCreateProgram();
}

NifSkopeOpenGLContext::Program::~Program()
{
	if ( id ) {
		f->glDeleteProgram( id );
		id = 0;
	}
	delete[] uniLocationsMap;
}

void NifSkopeOpenGLContext::Program::clear()
{
	wwUni.reset();
	wwMirror.reset();
	if ( id ) {
		f->glDeleteProgram( id );
		id = 0;
	}
	status = false;
	isProgram = true;
	uniLocationsMapSize = 0;
	nextProgram = nullptr;
	size_t	n = getUniLocationMapAllocSize( uniLocationsMapMask );
	for ( size_t i = 0; i < n; i++ )
		(void) new( &(uniLocationsMap[i]) ) UniformLocationMapItem();
}

bool NifSkopeOpenGLContext::Program::load( const QString & filepath, NifSkopeOpenGLContext * context )
{
	wwUni.reset();
	wwMirror.reset();
	linkFrom = 0;
	cacheFile.clear();
	try
	{
		QFile file( filepath );

		if ( !file.open( QIODevice::ReadOnly ) )
			throw QString( "couldn't open %1 for read access" ).arg( filepath );

		std::uint64_t	key = 0xCBF29CE484222325ULL;
		const auto mix = [&key]( const QByteArray & b ) {
			for ( char c : b )
				key = ( key ^ (unsigned char) c ) * 0x100000001B3ULL;
			key = ( key ^ 0xFF ) * 0x100000001B3ULL;
		};
		mix( file.readAll() );
		file.seek( 0 );
		QTextStream stream( &file );

		QStack<ConditionGroup *> chkgrps;
		chkgrps.push( &conditions );
		patches = false;

		while ( !stream.atEnd() ) {
			QString line = stream.readLine().trimmed();

			if ( line.startsWith( "shaders" ) ) {
				QStringList list = line.simplified().split( " " );

				for ( qsizetype i = 1; i < list.size(); i++ ) {
					const QString &	name = list.at( i );
					std::string	nameStr( name.toStdString() );
					std::uint32_t	m = context->shaderHashMask;
					std::uint32_t	h = hashFunctionUInt32( nameStr.c_str(), nameStr.length() ) & m;
					Shader * shader;
					for ( ; ( shader = context->shadersAndPrograms[h] ) != nullptr; h = ( h + 1 ) & m ) {
						if ( !shader->isProgram && shader->name == nameStr )
							break;
					}

					if ( shader && shader->id ) {
						if ( shader->status ) {
							if ( nameStr.ends_with( "tese" ) )
								patches = true;
							f->glAttachShader( id, shader->id );
							mix( QByteArray::number( qulonglong( shader->srcHash ), 16 ) );
						}
						else
							throw QString( "depends on shader %1 which was not compiled successful" ).arg( name );
					} else {
						throw QString( "shader %1 not found" ).arg( name );
					}
				}
			} else if ( line.startsWith( "checkgroup" ) ) {
				QStringList list = line.simplified().split( " " );

				if ( list.value( 1 ) == "begin" ) {
					ConditionGroup * group = new ConditionGroup( list.value( 2 ) == "or" );
					chkgrps.top()->addCondition( group );
					chkgrps.push( group );
				} else if ( list.value( 1 ) == "end" ) {
					if ( chkgrps.count() > 1 )
						chkgrps.pop();
					else
						throw QString( "mismatching checkgroup end tag" );
				} else {
					throw QString( "expected begin or end after checkgroup" );
				}
			} else if ( line.startsWith( "check" ) ) {
				line = line.remove( 0, 5 ).trimmed();

				bool invert = false;

				if ( line.startsWith( "not " ) ) {
					invert = true;
					line = line.remove( 0, 4 ).trimmed();
				}

				chkgrps.top()->addCondition( new ConditionSingle( line, invert ) );
			}
		}

		/* lane PERF1: the driver compiles at link time, about 2.4 s a program here; the links are started all
		 * together and finishLink() waits for them, and a linked program's binary is kept in shadercache/ next
		 * to the exe (keyed on the sources and the driver), so the next launch loads it instead */
		if ( !context->programCacheDir.isEmpty() ) {
			for ( GLenum k : { GL_VENDOR, GL_RENDERER, GL_VERSION } )
				mix( QByteArray( (const char *) f->glGetString( k ) ) );
			cacheFile = context->programCacheDir + '/' + QString::fromLatin1( name.data(), qsizetype( name.size() ) )
				+ '-' + QString::number( qulonglong( key ), 16 ) + ".bin";
			QFile	cf( cacheFile );
			if ( cf.open( QIODevice::ReadOnly ) ) {
				QByteArray	b = cf.readAll();
				/* lane STARTSPEED1: "WWPC" files carry a checksum of the binary, and a binary that does not match
				 * never reaches the driver (a damaged one crashed the NV driver); old "WWPB" files load unchecked.
				 * red WW_PROGCACHE_RED=1 damages a byte after the read (refused here, compiled from source),
				 * =2 hands the driver a wrong format (refused by the driver, the finishLink() fallback) */
				const int	red = wwEnvInt( "WW_PROGCACHE_RED" );
				const bool	sum = b.size() > 16 && b.startsWith( "WWPC" );
				if ( sum || ( b.size() > 8 && b.startsWith( "WWPB" ) && red != 1 ) ) {
					const int	at = sum ? 16 : 8;
					if ( red == 1 )
						b[at + ( b.size() - at ) / 2] = char( b.at( at + ( b.size() - at ) / 2 ) ^ 0x5A );
					quint64	want = 0;
					if ( sum )
						std::memcpy( &want, b.constData() + 8, 8 );
					if ( !sum || wwProgSum( b.constData() + at, b.size() - at ) == want ) {
						GLenum	fmt;
						std::memcpy( &fmt, b.constData() + 4, 4 );
						if ( red == 2 )
							fmt ^= 1;
						f->glProgramBinary( id, fmt, b.constData() + at, GLsizei( b.size() - at ) );
						linkFrom = 2;
						return true;
					}
					context->programCacheRejected++;
				}
			}
			f->glProgramParameteri( id, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE );
		}
		if ( QString err; !wwCompileDeferred( f, id, err ) )
			throw err;
		// lane MERGEFIX1102: STARTSPEED1's deferred compile runs here, on the GUI thread (the waiting-source table
		// is not shared with the cold-link workers); the workers only link the compiled shaders
		if ( context->coldLinkWorkers > 0 ) {
			linkFrom = 3;	// lane COLDLINK1: updateShaders links it on a worker thread
		} else {
			f->glLinkProgram( id );
			linkFrom = 1;
		}
	}
	catch ( QString & x )
	{
		printCompileError( x );
		return false;
	}
	return true;
}

bool NifSkopeOpenGLContext::Program::finishLink( NifSkopeOpenGLContext * context )
{
	wwUni.reset();
	wwMirror.reset();
	try
	{
		GLint result;

		f->glGetProgramiv( id, GL_LINK_STATUS, &result );

		if ( result != GL_TRUE && linkFrom == 2 ) {
			// the driver refused the cached binary (another driver, or damaged): link from the attached sources
			context->programCacheRejected++;
			f->glProgramParameteri( id, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE );
			if ( QString err; !wwCompileDeferred( f, id, err ) ) {
				f->glDeleteProgram( id );
				id = 0;
				throw err;
			}
			f->glLinkProgram( id );
			linkFrom = 1;
			f->glGetProgramiv( id, GL_LINK_STATUS, &result );
		}

		if ( result != GL_TRUE ) {
			GLint logLen = 0;
			f->glGetProgramiv( id, GL_INFO_LOG_LENGTH, &logLen );

			if ( logLen != 0 ) {
				char * log = new char[ logLen ];
				f->glGetProgramInfoLog( id, logLen, 0, log );
				QString errlog( log );
				delete[] log;
				f->glDeleteProgram( id );
				id = 0;
				throw errlog;
			}
		} else {
			if ( unsigned int l = f->glGetUniformBlockIndex( id, "globalUniforms" ); l != GL_INVALID_INDEX ) {
				f->glUniformBlockBinding( id, l, 0 );
				f->glBindBufferBase( GL_UNIFORM_BUFFER, 0, context->globalUniformsBufferObject );
			}
			if ( unsigned int l = f->glGetUniformBlockIndex( id, "skinningUniforms" ); l != GL_INVALID_INDEX ) {
				f->glUniformBlockBinding( id, l, 1 );
				f->glBindBufferBase( GL_UNIFORM_BUFFER, 1, context->boneTransformsBufferObject );
			}
			wwShadowFrame = false;	// lane PERF3: gl/sunshadow.cpp fills and binds the buffer
			wwBindless = name.size() > 7 && name.substr( name.size() - 7 ) == std::string_view( "bl.prog" );	// lane BINDLESS2
			wwDrawIdLoc = wwBindless ? f->glGetUniformLocation( id, "wwDrawBase" ) : -1;	// lane BINDLESSMDI1: + base instance
			if ( unsigned int l = f->glGetUniformBlockIndex( id, "wwShadowFrame" ); l != GL_INVALID_INDEX ) {
				f->glUniformBlockBinding( id, l, 2 );
				wwShadowFrame = true;
			}
			/* lane FIX1101A lodch: fo4_default.vert's far stream (samplerBuffer) sits on its own unit in every program
			 * that has it: left on unit 0 beside the material's sampler2D, every draw of the program would be invalid */
			if ( GLint l = f->glGetUniformLocation( id, "farStream" ); l >= 0 )
				f->glProgramUniform1i( id, l, WwCellUnit::FarStream );
			if ( linkFrom == 2 ) {
				context->programCacheHits++;
				// a hit stamps its binary, so the prune keeps the most recently used (gl/progcachekeep.h; WW_PROGCACHE_TOUCH_RED=1 = no stamp)
				if ( !cacheFile.isEmpty() && wwProgCacheTouchOn( qgetenv( "WW_PROGCACHE_TOUCH_RED" ) ) )
					wwProgCacheTouch( cacheFile );
			} else if ( !cacheFile.isEmpty() ) {
				GLint	len = 0;
				f->glGetProgramiv( id, GL_PROGRAM_BINARY_LENGTH, &len );
				if ( len > 0 ) {
					QByteArray	b( 16 + len, '\0' );
					GLenum	fmt = 0;
					GLsizei	got = 0;
					f->glGetProgramBinary( id, len, &got, &fmt, b.data() + 16 );
					b.resize( 16 + qMax( got, 0 ) );
					const quint64	sum = wwProgSum( b.constData() + 16, b.size() - 16 );
					std::memcpy( b.data(), "WWPC", 4 );
					std::memcpy( b.data() + 4, &fmt, 4 );
					std::memcpy( b.data() + 8, &sum, 8 );
					// one binary a define set, the newest few kept (gl/progcachekeep.h; WW_PROGCACHE_KEEP=1 = the red, one file)
					const QFileInfo	fi( cacheFile );
					QFile	cf( cacheFile );
					if ( got > 0 && cf.open( QIODevice::WriteOnly ) && cf.write( b ) == b.size() )
						context->programCacheWritten++;
					cf.close();
					wwProgCachePrune( fi.absolutePath(), QString::fromLatin1( name.data(), qsizetype( name.size() ) ), fi.fileName(),
						wwProgCacheKeep( qgetenv( "WW_PROGCACHE_KEEP" ) ) );
				}
			}
		}
	}
	catch ( QString & x )
	{
		printCompileError( x );
		return false;
	}
	status = true;
	nextProgram = context->programsLinked;
	context->programsLinked = this;
	return true;
}

inline NifSkopeOpenGLContext::Program::UniformLocationMapItem::UniformLocationMapItem( const char *s, int argsX16Y16 )
	: fmt( s ), args( std::uint32_t(argsX16Y16) ), l( -1 )
{
}

inline bool NifSkopeOpenGLContext::Program::UniformLocationMapItem::operator==( const UniformLocationMapItem & r ) const
{
	return ( fmt == r.fmt && args == r.args );
}

inline std::uint32_t NifSkopeOpenGLContext::Program::UniformLocationMapItem::hashFunction() const
{
	// note: this requires fmt to point to a string literal
	std::uint64_t	tmp = reinterpret_cast< std::uintptr_t >( fmt ) ^ ( std::uint64_t( args ) << 32 );
	std::uint32_t	h = 0xFFFFFFFFU;
	hashFunctionCRC32C< std::uint64_t >( h, tmp );
	return h;
}

int NifSkopeOpenGLContext::Program::storeUniformLocation( UniformLocationMapItem & o )
{
	const char *	fmt = o.fmt;
	int	arg1 = int( o.args & 0xFFFF );
	int	arg2 = int( o.args >> 16 );

	char	varNameBuf[256];
	char *	sp = varNameBuf;
	char *	endp = sp + 254;
	while ( sp < endp ) [[likely]] {
		char	c = *( fmt++ );
		if ( (unsigned char) c > (unsigned char) '%' ) [[likely]] {
			*( sp++ ) = c;
			continue;
		}
		if ( !c )
			break;
		if ( c == '%' ) [[likely]] {
			c = *( fmt++ );
			if ( c == 'd' ) {
				int	n = arg1;
				arg1 = arg2;
				if ( n >= 10 ) {
					c = char( (n / 10) & 15 ) | '0';
					*( sp++ ) = c;
					n = n % 10;
				}
				c = char( n & 15 ) | '0';
			} else if ( c != '%' ) {
				break;
			}
		}
		*( sp++ ) = c;
	}
	*sp = '\0';
	int	l = f->glGetUniformLocation( id, varNameBuf );
	o.l = l;
#ifndef QT_NO_DEBUG
	if ( l < 0 )
		std::fprintf( stderr, "[Warning] Uniform '%s' not found\n", varNameBuf );
#endif

	uniLocationsMapSize++;
	if ( ( uniLocationsMapSize * size_t(3) ) > ( uniLocationsMapMask * size_t(2) ) ) [[unlikely]] {
		unsigned int	m = ( uniLocationsMapMask << 1 ) | 0xFFU;
		size_t	n = getUniLocationMapAllocSize( uniLocationsMapMask );
		UniformLocationMapItem *	tmpBuf = new UniformLocationMapItem[getUniLocationMapAllocSize( m )];
		for ( size_t j = 0; j < n; j++ ) {
			size_t	k = uniLocationsMap[j].hashFunction() & m;
			while ( tmpBuf[k].fmt )
				k++;
			tmpBuf[k] = uniLocationsMap[j];
		}
		delete[] uniLocationsMap;
		uniLocationsMap = tmpBuf;
		uniLocationsMapMask = m;
	}

	return l;
}

inline int NifSkopeOpenGLContext::Program::uniLocation( const UniformLocationMapItem & key )
{
	UniformLocationMapItem *	p = uniLocationsMap + ( key.hashFunction() & uniLocationsMapMask );
	if ( *p == key )
		return p->l;
	for ( ; p->fmt; p++ ) {
		if ( *( p + 1 ) == key ) {
			std::swap( *p, *( p + 1 ) );
			return p->l;
		}
	}
	*p = key;
	return storeUniformLocation( *p );
}

int NifSkopeOpenGLContext::Program::uniLocation( const char * fmt )
{
	UniformLocationMapItem	key( fmt, 0 );

	return uniLocation( key );
}

int NifSkopeOpenGLContext::Program::uniLocation( const char * fmt, int argsX16Y16 )
{
	UniformLocationMapItem	key( fmt, argsX16Y16 );

	return uniLocation( key );
}

void NifSkopeOpenGLContext::Program::uni1i( const char * name, int x )
{
	UniformLocationMapItem	key( name, 0 );

	const int l = uniLocation( key );
	if ( WWCU( K1i, l, &x, 1 ) )
		f->glUniform1i( l, x );
	WWMI( K1i, l, &x, 1 );
	WWDP( WwDp::U1i, l, 0, &x, 1 );
}

void NifSkopeOpenGLContext::Program::uni1f( const char * name, float x )
{
	UniformLocationMapItem	key( name, 0 );

	const int l = uniLocation( key );
	if ( WWCU( K1f, l, &x, 1 ) )
		f->glUniform1f( l, x );
	WWMI( K1f, l, &x, 1 );
	WWDP( WwDp::U1f, l, 0, &x, 1 );
}

void NifSkopeOpenGLContext::Program::uni1b_l( int l, bool x )
{
	const int v = int( x );
	if ( WWCU( K1i, l, &v, 1 ) )
		f->glUniform1i( l, v );
	WWMI( K1i, l, &v, 1 );
	WWDP( WwDp::U1i, l, 0, &v, 1 );
}

void NifSkopeOpenGLContext::Program::uni1i_l( int l, int x )
{
	if ( WWCU( K1i, l, &x, 1 ) )
		f->glUniform1i( l, x );
	WWMI( K1i, l, &x, 1 );
	WWDP( WwDp::U1i, l, 0, &x, 1 );
}

void NifSkopeOpenGLContext::Program::uni1f_l( int l, float x )
{
	if ( WWCU( K1f, l, &x, 1 ) )
		f->glUniform1f( l, x );
	WWMI( K1f, l, &x, 1 );
	WWDP( WwDp::U1f, l, 0, &x, 1 );
}

void NifSkopeOpenGLContext::Program::uni2f_l( int l, float x, float y )
{
	const float v[2] = { x, y };
	if ( WWCU( K2f, l, v, 2 ) )
		f->glUniform2f( l, x, y );
	WWMI( K2f, l, v, 2 );
	WWDP( WwDp::U2f, l, 0, v, 2 );
}

void NifSkopeOpenGLContext::Program::uni3f_l( int l, float x, float y, float z )
{
	const float v[3] = { x, y, z };
	if ( WWCU( K3f, l, v, 3 ) )
		f->glUniform3f( l, x, y, z );
	WWMI( K3f, l, v, 3 );
	WWDP( WwDp::U3f, l, 0, v, 3 );
}

void NifSkopeOpenGLContext::Program::uni4f_l( int l, FloatVector4 x )
{
	const float v[4] = { x[0], x[1], x[2], x[3] };
	if ( WWCU( K4f, l, v, 4 ) )
		f->glUniform4f( l, x[0], x[1], x[2], x[3] );
	WWMI( K4f, l, v, 4 );
	WWDP( WwDp::U4f, l, 0, v, 4 );
}

void NifSkopeOpenGLContext::Program::uni4srgb_l( int l, FloatVector4 x )
{
	x = DDSTexture16::srgbExpand( x );
	const float v[4] = { x[0], x[1], x[2], x[3] };
	if ( WWCU( K4f, l, v, 4 ) )
		f->glUniform4f( l, x[0], x[1], x[2], x[3] );
	WWMI( K4f, l, v, 4 );
	WWDP( WwDp::U4f, l, 0, v, 4 );
}

void NifSkopeOpenGLContext::Program::uni4c_l( int l, std::uint32_t c, bool isSRGB )
{
	FloatVector4	x(c);
	x *= 1.0f / 255.0f;
	if ( isSRGB )
		x = DDSTexture16::srgbExpand( x );
	const float v[4] = { x[0], x[1], x[2], x[3] };
	if ( WWCU( K4f, l, v, 4 ) )
		f->glUniform4f( l, x[0], x[1], x[2], x[3] );
	WWMI( K4f, l, v, 4 );
	WWDP( WwDp::U4f, l, 0, v, 4 );
}

void NifSkopeOpenGLContext::Program::uni1bv_l( int l, const bool * x, size_t n )
{
	n = std::min< size_t >( n, 64 );
	GLint	tmp[64];
	for ( size_t i = 0; i < n; i++ )
		tmp[i] = GLint( x[i] );
	f->glUniform1iv( l, GLsizei(n), tmp );
	WwCellUni::wrote( wwUni, l, int( n ) );
	WWMF( l, int( n ) );
	WWDP( WwDp::U1iv, l, int( n ), tmp, int( n ) );
}

void NifSkopeOpenGLContext::Program::uni1iv_l( int l, const int * x, size_t n )
{
	f->glUniform1iv( l, GLsizei(n), x );
	WwCellUni::wrote( wwUni, l, int( n ) );
	WWMF( l, int( n ) );
	WWDP( WwDp::U1iv, l, int( n ), x, int( n ) );
}

void NifSkopeOpenGLContext::Program::uni1fv_l( int l, const float * x, size_t n )
{
	f->glUniform1fv( l, GLsizei(n), x );
	WwCellUni::wrote( wwUni, l, int( n ) );
	WWMF( l, int( n ) );
	WWDP( WwDp::U1fv, l, int( n ), x, int( n ) );
}

void NifSkopeOpenGLContext::Program::uni4fv_l( int l, const FloatVector4 * x, size_t n )
{
	f->glUniform4fv( l, GLsizei(n), &(x[0][0]) );
	WwCellUni::wrote( wwUni, l, int( n ) );
	WWMF( l, int( n ) );
	WWDP( WwDp::U4fv, l, int( n ), &(x[0][0]), int( n ) * 4 );
}

void NifSkopeOpenGLContext::Program::uni3m_l( int l, const Matrix & val )
{
	f->glUniformMatrix3fv( l, 1, 0, val.data() );
	WwCellUni::wrote( wwUni, l, 1 );
	WWMF( l, 1 );
	WWDP( WwDp::UM3, l, 1, val.data(), 9 );
}

void NifSkopeOpenGLContext::Program::uni4m_l( int l, const Matrix4 & val )
{
	f->glUniformMatrix4fv( l, 1, 0, val.data() );
	WwCellUni::wrote( wwUni, l, 1 );
	WWMF( l, 1 );
	WWDP( WwDp::UM4, l, 1, val.data(), 16 );
}

void NifSkopeOpenGLContext::Program::uniSampler_l( int l, int firstTextureUnit, int textureCnt, int arraySize )
{
	arraySize = std::min< int >( arraySize, TexCache::maxTextureUnits );
	textureCnt = std::min< int >( textureCnt, arraySize );
	GLint	tmp[TexCache::maxTextureUnits];
	int	i;
	for ( i = 0; i < textureCnt; i++ )
		tmp[i] = firstTextureUnit + i;
	for ( ; i < arraySize; i++ )
		tmp[i] = firstTextureUnit;
	f->glUniform1iv( l, arraySize, tmp );
	WwCellUni::wrote( wwUni, l, arraySize );
	WWMF( l, arraySize );
	WWDP( WwDp::U1iv, l, arraySize, tmp, arraySize );
}

bool NifSkopeOpenGLContext::Program::uniSampler( BSShaderLightingProperty * bsprop, const char * var,
									int textureSlot, int & texunit, const QString & alternate,
									uint clamp, const QString & forced )
{
	int	uniSamp = uniLocation( var );
	/* lane BINDLESS2: a map the bindless twin reads from its row: bound as the twin would bind it, its handle taken and
	 * the unit given back; a map the twin does not read stays 0 */
	if ( uniSamp < 0 && wwBindless && wwTwin ) {
		const int slot = WwBl::slotOf( var );
		if ( slot >= 0 ) {
			std::uint64_t h = 0;
			if ( wwTwin->uniLocation( var ) >= 0 ) {
				if ( texunit >= TexCache::num_texture_units ) [[unlikely]]
					return false;
				f->glActiveTexture( GL_TEXTURE0 + texunit );
				WWDP( WwDp::ActiveTex, GL_TEXTURE0 + texunit );
				/* lane BINDLESSPERF1: the unit is given back only for the packet check's state compare (the twin never
				 * samples it) and in the slow red; a query waits for the driver's thread */
				const bool giveBack = WwBl::red() == 5 || WwDp::mode() == 2;
				GLint prev = 0;
				if ( giveBack )
					f->glGetIntegerv( GL_TEXTURE_BINDING_2D, &prev );
				const size_t recOps = WwDp::rec ? WwDp::rec->ops.size() : 0, recData = WwDp::rec ? WwDp::rec->data.size() : 0;
				WwBl::resolving = true;
				WwBl::resolvedTex = 0;
				const bool ok = wwBindChain( bsprop, textureSlot, alternate, clamp, forced );
				WwBl::resolving = false;
				if ( WwDp::rec ) {	// the replay sets the handle, never the bind
					WwDp::rec->ops.resize( recOps );
					WwDp::rec->data.resize( recData );
				}
				if ( ok )
					h = WwBl::handleOfBound( f );
				WwBl::resolvedTex = 0;
				if ( giveBack )
					f->glBindTexture( GL_TEXTURE_2D, GLuint( prev ) );
				if ( !ok )
					return false;
				texunit++;
			}
			WwBl::setSlot( slot, h );
			WWDP( WwDp::BlTex, slot, 0, &h, 2 );
			return true;
		}
	}
	if ( uniSamp < 0 )
		return true;
	if ( texunit >= TexCache::num_texture_units ) [[unlikely]]
		return false;
	f->glActiveTexture( GL_TEXTURE0 + texunit );
	WWDP( WwDp::ActiveTex, GL_TEXTURE0 + texunit );
	if ( !wwBindChain( bsprop, textureSlot, alternate, clamp, forced ) )
		return false;

	WWDP( WwDp::U1i, uniSamp, 0, &texunit, 1 );
	f->glUniform1i( uniSamp, texunit++ );
	return true;
}

//! the material texture bind of uniSampler: forced, the slot's file, the alternate, then the slot's default
bool NifSkopeOpenGLContext::Program::wwBindChain( BSShaderLightingProperty * bsprop, int textureSlot, const QString & alternate,
									uint clamp, const QString & forced )
{

	// TODO: On stream 155 bsprop->fileName can reference incorrect strings because
	// the BSSTS is not filled out nor linked from the BSSP
	do {
		if ( !forced.isEmpty() && bsprop->bind( forced, true, TexClampMode(clamp) ) )
			break;
		if ( textureSlot >= 0 ) {
			QString	fname = bsprop->fileName( textureSlot );
			if ( !fname.isEmpty() && bsprop->bind( fname, false, TexClampMode(clamp) ) )
				break;
		}
		if ( !alternate.isEmpty() && bsprop->bind( alternate, false, TexClampMode::WRAP_S_WRAP_T ) )
			break;
		const QString *	fname = &black;
		if ( textureSlot == 0 )
			fname = &white;
		else if ( textureSlot == 1 )
			fname = ( bsprop->bsVersion < 151 ? &default_n : &default_ns );
		else if ( textureSlot >= 8 && bsprop->bsVersion >= 151 )
			fname = ( textureSlot == 8 ? &reflectivity : &lighting );
		if ( bsprop->bind( *fname, true, TexClampMode::WRAP_S_WRAP_T ) )
			break;

		return false;
	} while ( false );
	return true;
}


NifSkopeOpenGLContext::NifSkopeOpenGLContext( QOpenGLContext * context )
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	:	fn( context->versionFunctions< NifSkopeOpenGLContext::GLFunctions >() ), cx( context )
#else
	:	fn( QOpenGLVersionFunctionsFactory::get< NifSkopeOpenGLContext::GLFunctions >( context ) ), cx( context )
#endif
{
	vertexAttrib1f = reinterpret_cast< void (*)( unsigned int, float ) >( cx->getProcAddress( "glVertexAttrib1f" ) );
	vertexAttrib2fv =
		reinterpret_cast< void (*)( unsigned int, const float * ) >( cx->getProcAddress( "glVertexAttrib2fv" ) );
	vertexAttrib3fv =
		reinterpret_cast< void (*)( unsigned int, const float * ) >( cx->getProcAddress( "glVertexAttrib3fv" ) );
	vertexAttrib4fv =
		reinterpret_cast< void (*)( unsigned int, const float * ) >( cx->getProcAddress( "glVertexAttrib4fv" ) );
	if ( !( fn && vertexAttrib1f && vertexAttrib2fv && vertexAttrib3fv && vertexAttrib4fv ) )
		throw NifSkopeError( "failed to initialize OpenGL functions" );
	rehashShaders();

	globalUniforms = new GlobalUniforms();
	globalUniforms->lightSourcePosition[0][2] = 1.0f;
	globalUniforms->lightSourceDiffuse[0] = FloatVector4( 1.0f );
	globalUniforms->lightSourceAmbient = FloatVector4( 1.0f );
	globalUniforms->toneMapScale = 1.0f;
	globalUniforms->brightnessScale = 1.0f;
	globalUniforms->glowScale = 1.0f;
	globalUniforms->glowScaleSRGB = 1.0f;

	fn->glGenBuffers( 1, &globalUniformsBufferObject );
	setGlobalUniforms();
	fn->glGenBuffers( 1, &boneTransformsBufferObject );
	updateBoneTransforms( nullptr, 0 );
}

NifSkopeOpenGLContext::~NifSkopeOpenGLContext()
{
	flushCache();
	stopProgram();
	for ( size_t i = 0; i <= shaderHashMask; i++ ) {
		Shader *	s = shadersAndPrograms[i];
		if ( !s )
			continue;
		if ( s->isProgram )
			delete static_cast< Program * >( s );
		else
			delete s;
	}
	fn->glDeleteBuffers( 1, &boneTransformsBufferObject );
	fn->glDeleteBuffers( 1, &globalUniformsBufferObject );
	delete globalUniforms;
}

void NifSkopeOpenGLContext::setViewport( int x, int y, int width, int height )
{
	globalUniforms->viewportDimensions[0] = x;
	globalUniforms->viewportDimensions[1] = y;
	globalUniforms->viewportDimensions[2] = width;
	globalUniforms->viewportDimensions[3] = height;

	fn->glViewport( GLint( x ), GLint( y ), GLsizei( width ), GLsizei( height ) );
}

FloatVector4 NifSkopeOpenGLContext::getViewport() const
{
	if ( globalUniforms->viewportDimensions[2] > 0 ) [[likely]]
		return FloatVector4::convertInt32( globalUniforms->viewportDimensions );

	GLint	viewportDims[4];
	fn->glGetIntegerv( GL_VIEWPORT, viewportDims );
	return FloatVector4( float( viewportDims[0] ), float( viewportDims[1] ),
							float( viewportDims[2] ), float( viewportDims[3] ) );
}

NifSkopeOpenGLContext::Shader * NifSkopeOpenGLContext::createShader( const QString & name )
{
	std::string	nameStr( name.toLower().toStdString() );
	const char *	nameData = nameStr.c_str();
	size_t	nameLen = nameStr.length();
	if ( !( nameLen >= 5 && nameData[nameLen - 5] == '.' ) )
		return nullptr;

	unsigned int	t = 0;
	if ( nameStr.ends_with( "frag" ) )
		t = GL_FRAGMENT_SHADER;
	else if ( nameStr.ends_with( "geom" ) )
		t = GL_GEOMETRY_SHADER;
	else if ( nameStr.ends_with( "vert" ) )
		t = GL_VERTEX_SHADER;
	else if ( nameStr.ends_with( "tesc" ) )	// lane WATERLOOK1 step 2: the displaced water's split (fo4_waterdisp.*)
		t = GL_TESS_CONTROL_SHADER;
	else if ( nameStr.ends_with( "tese" ) )
		t = GL_TESS_EVALUATION_SHADER;
	else if ( !nameStr.ends_with( "prog" ) )
		return nullptr;

	std::uint32_t	m = shaderHashMask;
	std::uint32_t	h = hashFunctionUInt32( nameData, nameLen ) & m;
	Shader *	p;
	for ( ; ( p = shadersAndPrograms[h] ) != nullptr; h = ( h + 1 ) & m ) {
		if ( p->name == nameStr )
			break;
	}

	if ( p ) {
		if ( !p->id ) {
			if ( t )
				p->id = fn->glCreateShader( t );
			else
				p->id = fn->glCreateProgram();
		}
		return p;
	}

	std::string_view *	storedName = shaderDataBuf.constructObject< std::string_view >();
	char *	s = shaderDataBuf.allocateObjects< char >( nameLen + 1 );
	std::memcpy( s, nameData, nameLen );
	*storedName = std::string_view( s, nameLen );
	if ( t )
		p = new Shader( *storedName, t, fn );
	else
		p = new Program( *storedName, fn );
	shadersAndPrograms[h] = p;

	shaderCnt++;
	if ( ( shaderCnt * 2U ) > shaderHashMask )
		rehashShaders();

	return p;
}

void NifSkopeOpenGLContext::rehashShaders()
{
	size_t	n = size_t( 128 ) << std::bit_width( std::uint64_t( shaderCnt >> 6 ) );
	std::uint32_t	m = std::uint32_t( n - 1 );
	Shader **	tmp = shaderDataBuf.allocateObjects< Shader * >( n );
	for ( size_t i = 0; i < n; i++ )
		tmp[i] = nullptr;
	if ( shadersAndPrograms ) {
		for ( size_t i = 0; i <= shaderHashMask; i++ ) {
			Shader *	p = shadersAndPrograms[i];
			if ( !p )
				continue;
			std::uint32_t	h = hashFunctionUInt32( p->name.data(), p->name.length() ) & m;
			while ( tmp[h] )
				h = ( h + 1 ) & m;
			tmp[h] = p;
		}
	}
	shadersAndPrograms = tmp;
	shaderHashMask = m;
}

static int wwColdLinkRun( const QVector< NifSkopeOpenGLContext::Program * > & started, int workers, qint64 & ms );

/* lane EMPTY240 R4-1: a program's first draw makes the driver finish it (build 76: the first cell paint and a live set's
 * first paint after a shader reload ran hundreds of ms in their zones). Each linked program draws one point here, at
 * the load, into a 1x1 target with the rasterizer off (no attributes; the per-draw row a zeroed buffer). Programs with a
 * geometry, tessellation or compute stage are left out. Red WW_PROGWARM_RED=1: none (finished at the first draw). */
static void wwWarmPrograms( NifSkopeOpenGLContext::GLFunctions * f, NifSkopeOpenGLContext::Program * list )
{
	if ( wwEnvInt( "WW_PROGWARM_RED" ) == 1 ) {
		std::fprintf( stderr, "shader warm: off (WW_PROGWARM_RED=1: the driver finishes each program at its first draw)\n" );
		return;
	}
	QElapsedTimer	clock;
	clock.start();
	constexpr GLenum	kSsbo = 0x90D2, kSsboBinding = 0x90D3;	// GL_SHADER_STORAGE_BUFFER(_BINDING)
	GLint	prevProg = 0, prevVao = 0, prevFbo = 0, prevRb = 0, prevSsbo = 0;
	f->glGetIntegerv( GL_CURRENT_PROGRAM, &prevProg );
	f->glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &prevVao );
	f->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevFbo );
	f->glGetIntegerv( GL_RENDERBUFFER_BINDING, &prevRb );
	f->glGetIntegeri_v( kSsboBinding, 12, &prevSsbo );
	const GLboolean	prevDiscard = f->glIsEnabled( GL_RASTERIZER_DISCARD );
	GLuint	vao = 0, fbo = 0, rb = 0, ssbo = 0;
	f->glGenVertexArrays( 1, &vao );
	f->glGenRenderbuffers( 1, &rb );
	f->glBindRenderbuffer( GL_RENDERBUFFER, rb );
	f->glRenderbufferStorage( GL_RENDERBUFFER, GL_RGBA8, 1, 1 );
	f->glGenFramebuffers( 1, &fbo );
	f->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, fbo );
	f->glFramebufferRenderbuffer( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb );
	f->glGenBuffers( 1, &ssbo );
	f->glBindBuffer( kSsbo, ssbo );
	const std::vector< char >	zero( 176 * 64, 0 );
	f->glBufferData( kSsbo, GLsizeiptr( zero.size() ), zero.data(), GL_STATIC_DRAW );
	f->glBindBufferBase( kSsbo, 12, ssbo );
	f->glBindVertexArray( vao );
	f->glEnable( GL_RASTERIZER_DISCARD );
	while ( f->glGetError() != GL_NO_ERROR ) {
	}
	// a stage this point draw does not fit: its query answers without an error only when the program has it
	auto hasStage = [f]( GLuint id, GLenum q ) {
		GLint	v[3] = { 0, 0, 0 };
		f->glGetProgramiv( id, q, v );
		return f->glGetError() == GL_NO_ERROR;
	};
	int	warmed = 0, skipped = 0;
	for ( NifSkopeOpenGLContext::Program * p = list; p; p = p->nextProgram ) {
		if ( !p->status || !p->id || p->patches || hasStage( p->id, 0x8916 /* GL_GEOMETRY_VERTICES_OUT */ )
			|| hasStage( p->id, 0x8E75 /* GL_TESS_CONTROL_OUTPUT_VERTICES */ )
			|| hasStage( p->id, 0x8267 /* GL_COMPUTE_WORK_GROUP_SIZE */ ) ) {
			skipped++;
			continue;
		}
		f->glUseProgram( p->id );
		f->glDrawArrays( GL_POINTS, 0, 1 );
		warmed++;
	}
	while ( f->glGetError() != GL_NO_ERROR ) {
	}
	if ( !prevDiscard )
		f->glDisable( GL_RASTERIZER_DISCARD );
	f->glUseProgram( GLuint( prevProg ) );
	f->glBindVertexArray( GLuint( prevVao ) );
	f->glBindBufferBase( kSsbo, 12, GLuint( prevSsbo ) );
	f->glBindBuffer( kSsbo, 0 );
	f->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevFbo ) );
	f->glBindRenderbuffer( GL_RENDERBUFFER, GLuint( prevRb ) );
	f->glDeleteFramebuffers( 1, &fbo );
	f->glDeleteRenderbuffers( 1, &rb );
	f->glDeleteBuffers( 1, &ssbo );
	f->glDeleteVertexArrays( 1, &vao );
	std::fprintf( stderr, "shader warm: %d programs drawn once at the load (%d left out: geometry, tessellation or compute "
		"stage, or not linked) in %lld ms (red WW_PROGWARM_RED=1: the driver finishes each at its first draw)\n",
		warmed, skipped, (long long) clock.elapsed() );
}

void NifSkopeOpenGLContext::updateShaders()
{
	releaseShaders();

	QDir dir( QCoreApplication::applicationDirPath() );

	if ( dir.exists( "shaders" ) )
		dir.cd( "shaders" );

#ifdef Q_OS_LINUX
	else if ( dir.exists( "/usr/share/nifskope/shaders" ) )
		dir.cd( "/usr/share/nifskope/shaders" );
#endif

	QElapsedTimer	clock;
	clock.start();
	programCacheHits = programCacheRejected = programCacheWritten = 0;
	programCacheDir.clear();
	if ( wwEnvStr( "WW_PROGCACHE" ) != "0" ) {
		QDir	app( QCoreApplication::applicationDirPath() );
		if ( app.mkpath( "shadercache" ) )
			programCacheDir = app.filePath( "shadercache" );
	}
	/* lane PERF1: the driver's own link threads, opt-in (WW_PARALLEL_LINK=1): measured on a cold cache they made the
	 * 62 links slower here (370 s against 132 s serial); the binary cache is what makes the second launch fast */
	const char *	threads = "off";
	if ( QOpenGLContext * ctx = QOpenGLContext::currentContext(); ctx && wwEnvStr( "WW_PARALLEL_LINK" ) == "1" ) {
		using MaxThreads = void ( QOPENGLF_APIENTRY * )( GLuint );
		for ( const char * ext : { "GL_KHR_parallel_shader_compile", "GL_ARB_parallel_shader_compile" } ) {
			if ( !ctx->hasExtension( ext ) )
				continue;
			const bool	khr = ext[3] == 'K';
			if ( auto fn = reinterpret_cast< MaxThreads >( ctx->getProcAddress( khr ? "glMaxShaderCompilerThreadsKHR" : "glMaxShaderCompilerThreadsARB" ) ) ) {
				fn( 0xFFFFFFFFU );
				threads = khr ? "KHR" : "ARB";
				break;
			}
		}
	}

	coldLinkWorkers = wwEnvInt( "WW_COLDLINK_RED" ) == 1 ? 0 : std::clamp( wwEnvInt( "WW_COLDLINK" ), 0, 16 );
	// MODULES1: the shaders folder in its own order, then each module's folder (features/<module>/)
	QStringList	entryList = dir.entryList();
	QStringList	entryPath;
	for ( const QString & name : entryList )
		entryPath.append( dir.filePath( name ) );
	if ( const QDir feat( dir.filePath( "features" ) ); feat.exists() ) {
		for ( const QString & m : feat.entryList( QDir::Dirs | QDir::NoDotAndDotDot ) ) {
			const QDir	md( feat.filePath( m ) );
			for ( const QString & name : md.entryList( QDir::Files ) ) {
				if ( entryList.contains( name, Qt::CaseInsensitive ) ) {
					std::fprintf( stderr, "shader name twice: %s (features/%s kept out)\n", qPrintable( name ), qPrintable( m ) );
					continue;
				}
				entryList.append( name );
				entryPath.append( md.filePath( name ) );
			}
		}
	}
	QVector< Program * >	started;
	wwShadersCompiled = wwShadersLoaded = 0;
	for ( int i = 0; i < 2; i++ ) {
		for ( qsizetype k = 0; k < entryList.size(); k++ ) {
			const QString &	name = entryList.at( k );
			bool	isProgram = name.endsWith( QLatin1StringView( ".prog" ), Qt::CaseInsensitive );
			if ( isProgram != bool( i ) )
				continue;
			Shader *	shader = createShader( name );
			if ( !shader )
				continue;
			const QString &	fullPath = entryPath.at( k );
			if ( !shader->isProgram )
				shader->load( fullPath );
			else if ( static_cast< Program * >( shader )->load( fullPath, this ) )
				started.append( static_cast< Program * >( shader ) );
		}
	}
	qint64	coldMs = 0;
	const int	coldLinked = coldLinkWorkers > 0 ? wwColdLinkRun( started, coldLinkWorkers, coldMs ) : 0;
	const qint64	issued = clock.elapsed();
	int	ok = 0;
	for ( Program * p : started )	// in the load order: programsLinked keeps the order the renderer picks by
		ok += p->finishLink( this ) ? 1 : 0;
	std::fprintf( stderr, "shader programs: %d of %d linked (cache: %d loaded, %d refused, %d written; %s) parallel=%s issue %lld ms total %lld ms\n",
		ok, int( started.size() ), programCacheHits, programCacheRejected, programCacheWritten,
		programCacheDir.isEmpty() ? "off" : "on", threads, (long long) issued, (long long) clock.elapsed() );
	// lane STARTSPEED1: what the deferred compile saved (the waiting sources no link asked for go now)
	std::fprintf( stderr, "shader sources: %d of %d compiled (%s)\n", wwShadersCompiled, wwShadersLoaded,
		wwStartupEager() ? "eager, WW_STARTUP_EAGER=1" : "on demand" );
	wwDeferredShaders().clear();
	std::fprintf( stderr, "shader coldlink: workers %d, %d linked on them in %lld ms\n", coldLinkWorkers, coldLinked,
		(long long) coldMs );
	wwWarmPrograms( fn, programsLinked );	// lane EMPTY240 R4-1: finished now, not at a paint's first draw
}

/* lane COLDLINK1: a cold start links every program the binary cache missed (~2 s each on the NV driver). With
 * WW_COLDLINK=<n> they link on n threads, each with its own context shared with the current one (a program object
 * is shared), and the GUI thread waits for them; finishLink then reads each result as before. A worker that gets no
 * context leaves its programs to the serial link below. Red WW_COLDLINK_RED=1: the serial link. */
static int wwColdLinkRun( const QVector< NifSkopeOpenGLContext::Program * > & started, int workers, qint64 & ms )
{
	QVector< NifSkopeOpenGLContext::Program * >	todo;
	for ( NifSkopeOpenGLContext::Program * p : started )
		if ( p->linkFrom == 3 )
			todo.append( p );
	QElapsedTimer	clock;
	clock.start();
	std::atomic< int >	next{ 0 }, linked{ 0 };
	struct Worker { QOffscreenSurface * s; QOpenGLContext * c; QThread * t; };
	std::vector< Worker >	ws;
	QOpenGLContext *	cur = QOpenGLContext::currentContext();
	QThread *	gui = QThread::currentThread();
	const int	n = cur && QOpenGLContext::supportsThreadedOpenGL() ? std::min( workers, int( todo.size() ) ) : 0;
	// lane COLDLINK2: the shaders and programs were made on this context; a shared context sees them only after a
	// flush (GL 4.6 appendix D.3). Red WW_COLDLINK_RED=2 skips it.
	if ( n > 0 && wwEnvInt( "WW_COLDLINK_RED" ) != 2 )
		cur->functions()->glFinish();
	for ( int i = 0; i < n; i++ ) {
		QOffscreenSurface *	s = new QOffscreenSurface;
		s->setFormat( cur->format() );
		s->create();
		QOpenGLContext *	c = new QOpenGLContext;
		c->setFormat( cur->format() );
		c->setShareContext( cur );
		if ( !s->isValid() || !c->create() ) {
			delete c;
			delete s;
			break;
		}
		QThread *	t = QThread::create( [&todo, &next, &linked, s, c, gui]() {
			if ( c->makeCurrent( s ) ) {
				QOpenGLFunctions *	g = c->functions();
				for ( int k; ( k = next++ ) < int( todo.size() ); ) {
					GLint	r = 0;
					g->glLinkProgram( todo[k]->id );
					g->glGetProgramiv( todo[k]->id, GL_LINK_STATUS, &r );	// waits for the link
					todo[k]->linkFrom = 1;
					linked++;
				}
				g->glFinish();
				c->doneCurrent();
			}
			c->moveToThread( gui );
		} );
		c->moveToThread( t );
		ws.push_back( { s, c, t } );
		t->start();
	}
	for ( Worker & w : ws ) {
		w.t->wait();
		delete w.t;
		delete w.c;
		delete w.s;
	}
	for ( NifSkopeOpenGLContext::Program * p : todo ) {
		if ( p->linkFrom == 3 ) {
			p->f->glLinkProgram( p->id );
			p->linkFrom = 1;
		}
	}
	ms = clock.elapsed();
	return linked;
}

void NifSkopeOpenGLContext::releaseShaders()
{
	stopProgram();
	programsLinked = nullptr;
	for ( size_t i = 0; i <= shaderHashMask; i++ ) {
		Shader *	s = shadersAndPrograms[i];
		if ( !s )
			continue;
		if ( s->isProgram )
			static_cast< Program * >( s )->clear();
		else
			s->clear();
	}
}

NifSkopeOpenGLContext::Program * NifSkopeOpenGLContext::useProgram( const std::string_view & name )
{
	std::uint32_t	m = shaderHashMask;
	std::uint32_t	h = hashFunctionUInt32( name.data(), name.length() ) & m;
	std::uint32_t	prvH = h;
	Shader *	s;
	for ( ; ( s = shadersAndPrograms[h] ) != nullptr; h = ( h + 1 ) & m ) {
		if ( s->isProgram && s->name == name )
			break;
		prvH = h;
	}
	if ( s && s->status ) [[likely]] {
		if ( h != prvH )
			std::swap( shadersAndPrograms[prvH], shadersAndPrograms[h] );
		Program *	prog = static_cast< Program * >( s );
		fn->glUseProgram( prog->id );
		currentProgram = prog;
		return prog;
	}
	stopProgram();
	return nullptr;
}

void NifSkopeOpenGLContext::stopProgram()
{
	currentProgram = nullptr;
	fn->glUseProgram( 0 );
}

void NifSkopeOpenGLContext::setViewTransform( const Transform & t, int upAxis, float envMapRotation )
{
	const float *	r = t.rotation.data();
	globalUniforms->viewMatrix[0] = FloatVector4( r[0], r[3], r[6], 0.0f );
	globalUniforms->viewMatrix[1] = FloatVector4( r[1], r[4], r[7], 0.0f );
	globalUniforms->viewMatrix[2] = FloatVector4( r[2], r[5], r[8], 0.0f );

	FloatVector4	m[3];
	m[0] = FloatVector4::convertVector3( r );
	m[1] = FloatVector4::convertVector3( r + 3 );
	m[2] = FloatVector4::convertVector3( r + 6 );
	if ( upAxis == 0 ) {
		m[0].shuffleValues( 0xC9 );		// up axis = X: XYZ -> YZX
		m[1].shuffleValues( 0xC9 );
		m[2].shuffleValues( 0xC9 );
	} else if ( upAxis == 1 ) {
		m[0].shuffleValues( 0xD2 );		// up axis = Y: XYZ -> ZXY
		m[1].shuffleValues( 0xD2 );
		m[2].shuffleValues( 0xD2 );
	}
	float	r_c = float( std::cos( envMapRotation * float( 3.1415926536 / 180.0 ) ) );
	float	r_s = float( std::sin( envMapRotation * float( 3.1415926536 / 180.0 ) ) );
	m[0] = FloatVector4( m[0][0] * r_c - m[0][1] * r_s, m[0][0] * r_s + m[0][1] * r_c, m[0][2], m[0][3] );
	m[1] = FloatVector4( m[1][0] * r_c - m[1][1] * r_s, m[1][0] * r_s + m[1][1] * r_c, m[1][2], m[1][3] );
	m[2] = FloatVector4( m[2][0] * r_c - m[2][1] * r_s, m[2][0] * r_s + m[2][1] * r_c, m[2][2], m[2][3] );
	globalUniforms->envMapRotation[0] = m[0];
	globalUniforms->envMapRotation[1] = m[1];
	globalUniforms->envMapRotation[2] = m[2];
}

quint64 wwGlPaintCount();	// glview.cpp

void NifSkopeOpenGLContext::setGlobalUniforms()
{
	/* Never upload a zero viewport — it makes the line geometry shader emit NaN.
	 *
	 * lines.geom divides by vpScale, which is viewportDimensions.zw * 0.5, and
	 * normalizes (p1_ss - p0_ss) where both are scaled by it. At zero that is
	 * normalize(0,0) and a divide by zero, so every emitted gl_Position is NaN
	 * and the rasteriser drops the primitive. The geometry IS emitted — a capture
	 * of the grid draw shows 22 triangles out of the GS and no pixels — which is
	 * why forcing the colour red also produced nothing, and why POINTS from the
	 * same streaming path still drew: they never touch vpScale.
	 *
	 * setViewport() is the only writer, and it is called from just resizeGL() and
	 * indexAt(). resizeGL() returns early when the context is not valid yet
	 * WITHOUT setting it, and if no later resize arrives the value stays zero for
	 * the life of the window — until a click runs indexAt(). That is exactly the
	 * "grid and axes do not appear until I click in the viewport" defect (07-17).
	 *
	 * getViewport() above already carries this fallback; the upload path simply
	 * never got it. Read-only, so it cannot move the viewport the way forcing
	 * setViewport() per frame did — that attempt shifted every render-regression
	 * baseline and was reverted.
	 *
	 * CORRECTION (2026-08-11): "only when it was never set" was too narrow, and
	 * the missing half is a second, worse defect — a STALE nonzero viewport,
	 * which no fallback gated on zero can ever repair.
	 *
	 * Measured on X01_Torso_Tesla_VFX.nif, first frames after open:
	 *     resizeGL 1485x925 valid=0            <- the ONLY resize for the final
	 *                                             size, and isValid() is false,
	 *                                             so setViewport() never runs
	 *     paint 4  uniVP=1434x730  liveVP=1485x925
	 *     paint 5..7 identical
	 *     paint 9  uniVP=1280x741  liveVP=1280x741   (after a later resize)
	 * Qt sizes the default framebuffer and its viewport itself; GLView::resizeGL
	 * is the only thing that would have told the uniform, and it returned early
	 * before the context was valid. The zero-fallback then latched whatever
	 * transient viewport frame 1 happened to have (1434x730) and, being nonzero,
	 * never fired again. The value stayed wrong until a resize or a click ran
	 * indexAt() -> setViewport().
	 *
	 * fo4_default.frag reads the same uniform for screen-space refraction:
	 *     suv = (gl_FragCoord.xy - viewportDimensions.xy) / viewportDimensions.zw
	 * so a 1434x730 uniform over a 1485x925 framebuffer fetches the background
	 * at up to 1.04x in u and 1.27x in v. The refracting shape then draws a
	 * rescaled, displaced copy of the scene behind it — heavy visible warping
	 * that has nothing to do with Refraction Strength — and it healed the moment
	 * the user clicked in the viewport. That is bungo's report verbatim: "at the
	 * beginning it starts broken ... I have to click somewhere in the nif file
	 * for it to stabilize".
	 *
	 * So the value is reconciled with the live viewport on EVERY upload, not
	 * just while it is zero. This is still read-only — it cannot move the
	 * viewport, and it cannot disagree with the framebuffer being drawn, because
	 * the live viewport IS what this frame rasterises with. setViewport() writes
	 * both sides at once, so every call that goes through it reconciles to
	 * itself and nothing changes: the settled render is bit-identical (measured,
	 * paints 9-12 above already agreed).
	 */
	{
		GLint	viewportDims[4] = { 0, 0, 0, 0 };
		// lane CELLUNI6: glview's upload after the jittered projection takes the viewport the jitter asked (this paint,
		// no viewport write between); any other upload asks. WW_CELL_UNI6_RED=jitter: asked (the jitter keeps none).
		WwCellUni::viewportFrom( fn, WwCellUni::jitterVp(), wwGlPaintCount(), viewportDims, wwSetupMemoChecking() );
		if ( viewportDims[2] > 0 && viewportDims[3] > 0 ) {
			globalUniforms->viewportDimensions[0] = viewportDims[0];
			globalUniforms->viewportDimensions[1] = viewportDims[1];
			globalUniforms->viewportDimensions[2] = viewportDims[2];
			globalUniforms->viewportDimensions[3] = viewportDims[3];
		}
	}

	fn->glBindBuffer( GL_UNIFORM_BUFFER, globalUniformsBufferObject );
	fn->glBufferData( GL_UNIFORM_BUFFER, GLsizeiptr( sizeof(GlobalUniforms) ), globalUniforms, GL_DYNAMIC_DRAW );
	fn->glBindBuffer( GL_UNIFORM_BUFFER, 0 );
}

void NifSkopeOpenGLContext::setDefaultVertexAttribs( std::uint64_t attrMask, const float * const * attrData )
{
	fn->glBindVertexArray( 0 );
	for ( size_t i = 0; attrMask; i++, attrMask = attrMask >> 4 ) {
		size_t	n = attrMask & 15;
		if ( !n )
			continue;
		if ( n >= 4 )
			vertexAttrib4fv( GLuint( i ), attrData[i] );
		else if ( n == 3 )
			vertexAttrib3fv( GLuint( i ), attrData[i] );
		else if ( n == 2 )
			vertexAttrib2fv( GLuint( i ), attrData[i] );
		else
			vertexAttrib1f( GLuint( i ), attrData[i][0] );
	}
}

// RESUME 15: the last skinning upload's contents (FNV-1a), part of a skinned draw's vertex key (ao.cpp aoVertexKey)
static quint64 wwBoneHash = 0;
quint64 wwBoneUploadHash()
{
	return wwBoneHash;
}

void NifSkopeOpenGLContext::updateBoneTransforms( const FloatVector4 * boneTransforms, size_t numBones )
{
	FloatVector4	tmp[768];
	size_t	n = std::min< size_t >( numBones, 256 ) * 3;
	{
		quint64 h = 1469598103934665603ull;
		const unsigned char * b = reinterpret_cast<const unsigned char *>( boneTransforms );
		for ( size_t i = 0; boneTransforms && i < n * sizeof( FloatVector4 ); i++ ) {
			h ^= b[i];
			h *= 1099511628211ull;
		}
		wwBoneHash = h ^ quint64( n );
	}
	if ( n > 0 )
		std::memcpy( &( tmp[0][0] ), &( boneTransforms[0][0] ), n * sizeof( FloatVector4 ) );
	for ( ; n < 768; n = n + 3 ) {
		tmp[n] = FloatVector4( 1.0f, 0.0f, 0.0f, 0.0f );
		tmp[n + 1] = FloatVector4( 0.0f, 1.0f, 0.0f, 0.0f );
		tmp[n + 2] = FloatVector4( 0.0f, 0.0f, 1.0f, 0.0f );
	}
	fn->glBindBuffer( GL_UNIFORM_BUFFER, boneTransformsBufferObject );
	fn->glBufferData( GL_UNIFORM_BUFFER, GLsizeiptr( sizeof(FloatVector4) * 768 ), tmp, GL_DYNAMIC_DRAW );
	fn->glBindBuffer( GL_UNIFORM_BUFFER, 0 );
}

void NifSkopeOpenGLContext::bindShape(
	unsigned int numVerts, std::uint64_t attrMask, size_t elementDataSize,
	const float * const * attrData, const void * elementData )
{
	bindShape( ShapeDataHash( numVerts, attrMask, elementDataSize, attrData, elementData ), attrData, elementData );
}

void NifSkopeOpenGLContext::bindShape(
	const ShapeDataHash & h, const float * const * attrData, const void * elementData )
{
	if ( ( cacheShapeCnt * 3U ) >= ( geometryCache.size() * 2U ) ) [[unlikely]]
		rehashCache();

	GLFunctions &	f = *fn;

	std::uint32_t	m = std::uint32_t( geometryCache.size() - 1 );
	std::uint32_t	i = h.hashFunction() & m;
	std::uint32_t	j = i;
	for ( ; geometryCache[i]; i = ( i + 1 ) & m ) {
		if ( geometryCache[i]->h == h ) {
			ShapeData *	d = geometryCache[i];
			if ( i != j )
				std::swap( geometryCache[j], geometryCache[i] );
			if ( d != cacheLastItem ) {
				d->prev->next = d->next;
				d->next->prev = d->prev;
				d->prev = cacheLastItem;
				d->next = cacheLastItem->next;
				d->prev->next = d;
				d->next->prev = d;
			}
			cacheLastItem = d;
			d->usedEpoch = cacheEpoch;
			if ( !wwVaoDefer ) [[likely]]
				f.glBindVertexArray( d->vao );
			return;
		}
		j = i;
	}

	// lane EMPTY240 R4-1: over this paint's upload budget: a draw that can wait leaves its mesh for the next paint
	if ( WwUp::st().meshMayDefer && WwUp::spent() ) {
		WwUp::deferMesh();
		WwDp::abortRecording();
		return;
	}
	ShapeData *	d;
	{
		WwUp::Charge wwUpCharge;
		d = new ShapeData( *this, h, attrData, elementData );
		WwUp::Charge::addBytes( qint64( d->h.getBufferDataSize() ) );
	}
	if ( !cacheLastItem ) {
		d->prev = d;
		d->next = d;
	} else {
		d->prev = cacheLastItem;
		d->next = cacheLastItem->next;
		d->prev->next = d;
		d->next->prev = d;
	}
	d->usedEpoch = cacheEpoch;
	cacheLastItem = d;
	geometryCache[i] = d;
	cacheShapeCnt++;
	cacheBytesUsed += d->h.getBufferDataSize() + 24576;
	PassTime::CacheCounts & pc = PassTime::cacheCounts();	// lane PERF1: the cache's traffic for the frame timers
	pc.made++;
	pc.madeBytes += qint64( d->h.getBufferDataSize() );
	pc.usedBytes = qint64( cacheBytesUsed );
	pc.maxBytes = qint64( effectiveCacheBytes() );
}

/*! lane STREAM1: the geometry budget a game sizes from the card (an eighth of its dedicated memory, 256 MB to
 *  1.5 GB), never below the renderer's setting: a 3x3 set held 308 MB against 128 MB and re-uploaded shapes every
 *  frame. WW_GEOCACHE_MB=<n> pins it; WW_STREAM1_RED=geocache keeps the setting alone (the old budget). */
size_t NifSkopeOpenGLContext::effectiveCacheBytes()
{
	if ( PassTime::stream1Red( "geocache" ) )
		return cacheMaxBytes;
	if ( !cacheBudgetBytes ) {
		qint64 mb = qEnvironmentVariableIntValue( "WW_GEOCACHE_MB" );
		if ( mb <= 0 ) {
			const qint64 vram = PassTime::videoMemoryMb( nullptr );
			mb = vram > 0 ? std::clamp< qint64 >( vram / 8, 256, 1536 ) : 256;
		}
		cacheBudgetBytes = size_t( mb ) << 20;
	}
	return std::max( cacheMaxBytes, cacheBudgetBytes );
}

//! WW_GEOCACHE_RED=thrash: the old strict size limit, evicting shapes the frame still draws (the gate's red)
static bool geoCacheThrashRed()
{
	WW_SWITCH_STATIC( bool, v, wwEnv( "WW_GEOCACHE_RED" ) == "thrash" );
	return v;
}

void NifSkopeOpenGLContext::setCacheSize( size_t maxBytes )
{
	cacheMaxBytes = std::uint32_t( maxBytes );
}

void NifSkopeOpenGLContext::shrinkCache( bool deleteAll )
{
	bool	rehashNeeded = false;
	// lane STREAM1: between the setting and the budget a shape goes only after ~30 s (60 fps) undrawn, so a
	// streamed cell that left frees its geometry and one that comes back into view is not uploaded again
	const bool	budgetOn = !deleteAll && !geoCacheThrashRed() && !PassTime::stream1Red( "geocache" );
	const size_t	budget = budgetOn ? effectiveCacheBytes() : 0;
	constexpr std::uint32_t	keepEpochs = 1800;

	while ( cacheLastItem ) {
		ShapeData *	d = cacheLastItem->next;

		if ( deleteAll ) {
			cacheShapeCnt = 0;
			cacheBytesUsed = 0;
		} else {
			if ( cacheBytesUsed < cacheMaxBytes )
				break;
			// lane PERF1: the oldest shape was drawn this frame, so all are: evicting it would re-upload
			// the frame's whole working set every frame once it outgrows the size (a cell frame did)
			if ( d->usedEpoch == cacheEpoch && !geoCacheThrashRed() )
				break;
			if ( budgetOn && cacheBytesUsed < budget && cacheEpoch - d->usedEpoch < keepEpochs )
				break;
			cacheShapeCnt--;
			cacheBytesUsed -= d->h.getBufferDataSize() + 24576;
		}

		if ( !rehashNeeded ) {
			geometryCache.clear();
			rehashNeeded = true;
		}
		if ( d->prev == d ) {
			cacheLastItem = nullptr;
		} else {
			d->prev->next = d->next;
			d->next->prev = d->prev;
		}
		delete d;
		PassTime::cacheCounts().evicted++;	// lane PERF1
	}
	PassTime::cacheCounts().usedBytes = qint64( cacheBytesUsed );

	if ( rehashNeeded )
		rehashCache();
	cacheEpoch++;
}

/*! lane STREAMMEM1: while cells stream, shapes no paint since `epoch` drew (cells that left, old far masks) go now;
 *  shrinkCache alone kept them ~1800 paints under the 1.5 GB budget (+657 MB, private x1.19 in 20 crossings) */
int NifSkopeOpenGLContext::evictUndrawnSince( std::uint32_t epoch )
{
	int	n = 0;
	while ( cacheLastItem ) {
		ShapeData *	d = cacheLastItem->next;	// the oldest: the list is in draw order
		if ( d->usedEpoch == cacheEpoch || std::int32_t( d->usedEpoch - epoch ) >= 0 )
			break;
		cacheShapeCnt--;
		cacheBytesUsed -= d->h.getBufferDataSize() + 24576;
		if ( d->prev == d ) {
			cacheLastItem = nullptr;
		} else {
			d->prev->next = d->next;
			d->next->prev = d->prev;
		}
		delete d;
		n++;
	}
	if ( n ) {
		PassTime::cacheCounts().evicted += n;
		PassTime::cacheCounts().usedBytes = qint64( cacheBytesUsed );
		geometryCache.clear();
		rehashCache();
	}
	return n;
}

void NifSkopeOpenGLContext::rehashCache()
{
	size_t	n = 256UL << std::bit_width( ( cacheShapeCnt * 3U ) >> 9U );
	if ( geometryCache.size() == n )
		return;

	std::uint32_t	m = std::uint32_t( n - 1 );
	geometryCache.clear();
	geometryCache.resize( n, nullptr );
	ShapeData *	d = cacheLastItem;
	if ( !d )
		return;
	do {
		std::uint32_t	i = d->h.hashFunction() & m;
		while ( geometryCache[i] )
			i = ( i + 1 ) & m;
		geometryCache[i] = d;
		d = d->prev;
	} while ( d != cacheLastItem );
}


inline std::uint32_t NifSkopeOpenGLContext::ShapeDataHash::hashFunction() const
{
	std::uint64_t	tmp1 = attrMask;
	std::uint64_t	tmp2 = ( std::uint64_t( elementBytes ) << 32 ) | numVerts;
	std::uint64_t	r = h[0];
	hashFunctionUInt64( r, tmp1 );
	hashFunctionUInt64( r, tmp2 );
	return std::uint32_t( r );
}

//! lane VTXPACK1: WW_STREAM1_RED=vtxpack uploads every array as fp32 (the old buffers and their size)
static bool vtxPackOn()
{
	static const bool v = !PassTime::stream1Red( "vtxpack" );
	return v;
}

/*! lane VTXPACK1: the array as half floats only when EVERY value comes back bit-exact from fp16 and is a normal
 *  half or zero (FO4 stores UVs, weights and most positions as halves): the vertex fetch widens each to the very
 *  float the fp32 array held, so the shader reads the same numbers. n = components a vertex (2 to 4); 3 is padded
 *  to 4 so each vertex stays 4-byte aligned. False = keep fp32. */
static bool wwPackHalfExact( const float * src, size_t n, size_t verts, std::vector< std::uint16_t > & out )
{
#if defined( __F16C__ ) && defined( __AVX__ )
	const size_t	total = n * verts;
	std::vector< std::uint16_t >	tmp( ( total + 7 ) & ~size_t( 7 ) );
	const __m256	absMask = _mm256_castsi256_ps( _mm256_set1_epi32( 0x7FFFFFFF ) );
	const __m256	maxHalf = _mm256_set1_ps( 65504.0f );
	const __m256	minNormal = _mm256_set1_ps( 6.103515625e-05f );
	const __m256	zero = _mm256_setzero_ps();
	for ( size_t i = 0; i < total; i += 8 ) {
		alignas( 32 ) float	buf[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
		const float *	p = src + i;
		if ( i + 8 > total ) {
			std::memcpy( buf, p, ( total - i ) * sizeof( float ) );
			p = buf;
		}
		const __m256	v = _mm256_loadu_ps( p );
		const __m128i	h = _mm256_cvtps_ph( v, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC );
		const __m256	a = _mm256_and_ps( v, absMask );
		__m256	ok = _mm256_cmp_ps( _mm256_cvtph_ps( h ), v, _CMP_EQ_OQ );	// NaN fails here
		ok = _mm256_and_ps( ok, _mm256_cmp_ps( a, maxHalf, _CMP_LE_OQ ) );	// infinity
		ok = _mm256_and_ps( ok, _mm256_or_ps( _mm256_cmp_ps( a, minNormal, _CMP_GE_OQ ), _mm256_cmp_ps( a, zero, _CMP_EQ_OQ ) ) );
		if ( _mm256_movemask_ps( ok ) != 0xFF )
			return false;
		_mm_storeu_si128( reinterpret_cast< __m128i * >( tmp.data() + i ), h );
	}
	if ( n != 3 ) {
		tmp.resize( total );
		out.swap( tmp );
	} else {
		out.assign( verts * 4, 0 );
		for ( size_t v = 0; v < verts; v++ )
			std::memcpy( out.data() + v * 4, tmp.data() + v * 3, 3 * sizeof( std::uint16_t ) );
	}
	return true;
#else
	(void) src; (void) n; (void) verts; (void) out;
	return false;
#endif
}

size_t NifSkopeOpenGLContext::ShapeDataHash::getBufferDataSize() const
{
	std::uint64_t	tmp = ( attrMask & 0x0F0F0F0F0F0F0F0FULL ) + ( ( attrMask >> 4 ) & 0x0F0F0F0F0F0F0F0FULL );
	tmp = tmp + ( tmp >> 8 );
	tmp = tmp + ( tmp >> 16 );
	tmp = tmp + ( tmp >> 32 );
	size_t	totalDataSize = size_t( tmp & 0xFFU ) * sizeof( float ) * numVerts + elementBytes;
	return totalDataSize;
}


NifSkopeOpenGLContext::ShapeData::ShapeData(
	NifSkopeOpenGLContext & context, const ShapeDataHash & dataHash,
	const float * const * attrData, const void * elementData )
	: h( dataHash ), prev( nullptr ), next( nullptr ), fn( context.fn ), vao( 0 ), ebo( 0 )
{
	std::uint64_t	attrMask = dataHash.attrMask;
	std::uint32_t	vertCnt = dataHash.numVerts;
	std::uint32_t	elementDataSize = dataHash.elementBytes;
	std::uint32_t	attrDataSize = dataHash.getBufferDataSize() - elementDataSize;
	std::uintptr_t	attrOffset = 0;

	// lane VTXPACK1: which arrays go up as exact halves (WW_STREAM1_RED=vtxpack: none, the old buffer)
	std::vector< std::uint16_t >	halves[16];
	if ( vtxPackOn() ) {
		attrDataSize = 0;
		std::uint64_t	m = attrMask;
		for ( size_t i = 0; m && i < 16; i++, m = m >> 4 ) {
			size_t	n = m & 15;
			if ( !n )
				continue;
			if ( n >= 2 && n <= 4 && attrData[i] && wwPackHalfExact( attrData[i], n, vertCnt, halves[i] ) )
				attrDataSize += std::uint32_t( halves[i].size() * sizeof( std::uint16_t ) );
			else
				attrDataSize += std::uint32_t( n * sizeof( float ) * vertCnt );
		}
	}
	gpuBytes = attrDataSize + elementDataSize;
	PassTime::CacheCounts &	pc = PassTime::cacheCounts();
	pc.geoShapes++;
	pc.geoGpuBytes += qint64( gpuBytes );
	pc.geoFp32Bytes += qint64( dataHash.getBufferDataSize() );

	GLFunctions &	f = *( context.fn );
	f.glGenVertexArrays( 1, &vao );
	f.glBindVertexArray( vao );
	f.glGenBuffers( 1, &vbo );
	f.glBindBuffer( GL_ARRAY_BUFFER, vbo );
	f.glBufferData( GL_ARRAY_BUFFER, GLsizeiptr( attrDataSize ), (void *) 0, GL_STATIC_DRAW );
	for ( size_t i = 0; attrMask; i++, attrMask = attrMask >> 4 ) {
		size_t	n = attrMask & 15;
		if ( !n )
			continue;
		if ( i < 16 && !halves[i].empty() ) {
			size_t	nBytes = halves[i].size() * sizeof( std::uint16_t );
			wwStageUpload( GL_ARRAY_BUFFER, GLintptr( attrOffset ), GLsizeiptr( nBytes ), halves[i].data() );
			f.glVertexAttribPointer( GLuint( i ), GLint( n ), GL_HALF_FLOAT, GL_FALSE,
										GLsizei( ( n == 3 ? 4 : n ) * sizeof( std::uint16_t ) ), (void *) attrOffset );
			f.glEnableVertexAttribArray( GLuint( i ) );
			attrOffset = attrOffset + nBytes;
			continue;
		}
		size_t	nBytes = n * sizeof( float ) * vertCnt;
		wwStageUpload( GL_ARRAY_BUFFER, GLintptr( attrOffset ), GLsizeiptr( nBytes ), attrData[i] );
		f.glVertexAttribPointer( GLuint( i ), GLint( n ), GL_FLOAT, GL_FALSE, 0, (void *) attrOffset );
		f.glEnableVertexAttribArray( GLuint( i ) );
		attrOffset = attrOffset + nBytes;
	}

	if ( elementDataSize ) [[likely]] {
		f.glGenBuffers( 1, &ebo );
		f.glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ebo );
		wwStageBufferData( GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr( elementDataSize ), elementData, GL_STATIC_DRAW );
	}

	// TEMP DIAGNOSTIC (WW_GRID_TRACE): trace small streaming uploads (grid/axes).
	// Its own variable for the same reason as drawGrid's: a file write per upload
	// has no business under the flag reached for when something is slow.
	if ( wwEnvIsSet( "WW_GRID_TRACE" ) && dataHash.numVerts <= 100
		&& !dataHash.elementBytes ) {
		QFile df( QCoreApplication::applicationDirPath() + "/ww_perf_test.log" );
		if ( df.open( QIODevice::Append | QIODevice::Text ) )
			QTextStream( &df ) << "    [ShapeData create: verts=" << dataHash.numVerts
				<< " mask=0x" << QString::number( dataHash.attrMask, 16 )
				<< " vao=" << vao << " vbo=" << vbo
				<< " ctx=" << quintptr( QOpenGLContext::currentContext() )
				<< " err=0x" << QString::number( f.glGetError(), 16 )
				<< " data0=" << ( attrData && attrData[0] ? attrData[0][0] : -9999.0f )
				<< "]\n";
	}
}

NifSkopeOpenGLContext::ShapeData::~ShapeData()
{
	PassTime::CacheCounts &	pc = PassTime::cacheCounts();	// lane STREAM1: what stays resident
	pc.geoShapes--;
	pc.geoGpuBytes -= qint64( gpuBytes );
	pc.geoFp32Bytes -= qint64( h.getBufferDataSize() );
	GLFunctions &	f = *fn;
	f.glBindVertexArray( 0 );
	f.glDeleteVertexArrays( 1, &vao );
	if ( ebo ) [[likely]]
		f.glDeleteBuffers( 1, &ebo );
	f.glDeleteBuffers( 1, &vbo );
}


#define XXH_INLINE_ALL 1
#include "xxhash.h"

struct alignas( 64 ) ShapeDataHashSecret {
	unsigned char	buf[XXH3_SECRET_DEFAULT_SIZE];
	ShapeDataHashSecret();
};

ShapeDataHashSecret::ShapeDataHashSecret()
{
	unsigned long long	s[2];
#if ENABLE_X86_64_SIMD >= 4
	__builtin_ia32_rdrand64_step( &(s[0]) );
	__builtin_ia32_rdrand64_step( &(s[1]) );
#else
	auto	t = std::chrono::steady_clock::now().time_since_epoch();
	s[0] = (unsigned long long) std::chrono::duration_cast< std::chrono::microseconds >( t ).count();
	s[1] = timerFunctionRDTSC();
#endif
	XXH3_generateSecret( buf, sizeof( buf ), s, sizeof( s ) );
}

static ShapeDataHashSecret	shapeDataHashSecret;


NifSkopeOpenGLContext::ShapeDataHash::ShapeDataHash(
	std::uint32_t vertCnt, std::uint64_t attrModeMask, size_t elementDataSize,
	const float * const * attrData, const void * elementData )
	: attrMask( attrModeMask ), numVerts( vertCnt ), elementBytes( std::uint32_t(elementDataSize) )
{
	XXH3_state_t *	xxhState;
#if defined(_WIN32) || defined(_WIN64)
	// work around stack alignment issues on Windows
	unsigned char	xxhStateBuf[sizeof( XXH3_state_t ) + 64];
	{
		std::uintptr_t	p = reinterpret_cast< std::uintptr_t >( &(xxhStateBuf[0]) );
		p = ( p + 63U ) & std::uintptr_t( -64 );
		xxhState = reinterpret_cast< XXH3_state_t * >( reinterpret_cast< unsigned char * >( p ) );
	}
#else
	XXH3_state_t	xxhStateBuf;
	xxhState = &xxhStateBuf;
#endif
	XXH3_128bits_reset_withSecret( xxhState, shapeDataHashSecret.buf, sizeof( shapeDataHashSecret.buf ) );
	if ( !vertCnt ) [[unlikely]]
		attrModeMask = 0;
	for ( std::uint32_t i = 0; attrModeMask; i++, attrModeMask = attrModeMask >> 4 ) {
		size_t	nBytes = attrModeMask & 15;
		if ( !nBytes )
			continue;
		nBytes = nBytes * sizeof( float ) * vertCnt;
		const void *	p = attrData[i];
		XXH3_128bits_update( xxhState, p, nBytes );
	}
	if ( elementDataSize > 0 ) [[likely]]
		XXH3_128bits_update( xxhState, elementData, elementDataSize );
	XXH128_hash_t	xxhResult = XXH3_128bits_digest( xxhState );
	h[0] = xxhResult.low64;
	h[1] = xxhResult.high64;
}

