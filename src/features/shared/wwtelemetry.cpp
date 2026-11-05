#include "wwtelemetry.h"

#include "wwswitch.h"

#include <QElapsedTimer>
#include <QProcessEnvironment>
#include <QStringList>

#include <cstdio>
#include <cstring>
#include <mutex>

// lane MODULES1: see wwtelemetry.h

namespace WwTelemetry
{
namespace
{
struct State
{
	std::mutex mutex;
	std::FILE * f = nullptr;
	bool tried = false;
	qint64 frame = -1;
	QByteArray row;
	int rate = 1;
	QElapsedTimer clock;
};

State & st()
{
	static State s;
	return s;
}

void writeLine( State & s, const char * module, const char * event, const QByteArray & fields )
{
	QByteArray line = QByteArray( "{\"t\":" ) + num( double( s.clock.nsecsElapsed() ) * 1e-9 ) + ",\"frame\":"
		+ QByteArray::number( s.frame ) + ",\"row\":" + str( s.row ) + ",\"gen\":" + QByteArray::number( wwSwitchGen() )
		+ ",\"module\":" + str( module ) + ",\"event\":" + str( event );
	if ( !fields.isEmpty() )
		line += "," + fields;
	line += "}\n";
	std::fwrite( line.constData(), 1, size_t( line.size() ), s.f );
	std::fflush( s.f );
}

//! Opens the file on the first use; the start record lists every WW_* switch the process launched with
bool open( State & s )
{
	if ( s.tried )
		return s.f != nullptr;
	s.tried = true;
	const QByteArray path = qgetenv( "WW_TELEMETRY" );
	if ( path.isEmpty() )
		return false;
	s.f = std::fopen( path.constData(), "ab" );
	if ( !s.f )
		return false;
	s.clock.start();
	bool ok = false;
	const int r = qEnvironmentVariableIntValue( "WW_TELEMETRY_RATE", &ok );
	s.rate = ok && r > 0 ? r : 1;
	const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	QStringList keys = env.keys();
	keys.sort();
	QByteArray sw;
	for ( const QString & k : keys ) {
		if ( !k.startsWith( QLatin1String( "WW_" ) ) )
			continue;
		if ( !sw.isEmpty() )
			sw += ",";
		sw += str( k.toLatin1() ) + ":" + str( env.value( k ).toLocal8Bit() );
	}
	writeLine( s, "telemetry", "start", "\"read\":\"launch\",\"switches\":{" + sw + "}" );
	return true;
}
} // namespace

bool on()
{
	static const bool v = !qgetenv( "WW_TELEMETRY" ).isEmpty();
	return v;
}

void record( const char * module, const char * event, const QByteArray & fields )
{
	if ( !on() )
		return;
	State & s = st();
	std::lock_guard<std::mutex> l( s.mutex );
	if ( open( s ) )
		writeLine( s, module, event, fields );
}

void setFrame( qint64 frame )
{
	if ( !on() )
		return;
	State & s = st();
	std::lock_guard<std::mutex> l( s.mutex );
	s.frame = frame;
}

void setRow( const QByteArray & row )
{
	if ( !on() )
		return;
	{
		State & s = st();
		std::lock_guard<std::mutex> l( s.mutex );
		s.row = row;
	}
	record( "telemetry", "row", QByteArray() );
}

bool costDue( qint64 frame )
{
	if ( !on() )
		return false;
	State & s = st();
	std::lock_guard<std::mutex> l( s.mutex );
	open( s );
	return s.f && ( frame % s.rate ) == 0;
}

const char * zoneModule( const char * zone )
{
	// one owner per area: the module each pass-time zone and GPU sub-stage reports under
	static const char * const map[][2] = {
		{ "sun-cascades", "shadows" }, { "contact", "shadows" }, { "sun-casters", "shadows" }, { "sun-terrstep", "shadows" },
		{ "light-cubes", "lights" },
		{ "sky", "sky" },
		{ "obscurance", "ao" },
		{ "reflections", "reflections" }, { "refl-probes", "reflections" }, { "ssr-donor", "reflections" },
		{ "air", "air" },
		{ "measure", "exposure" }, { "tone-map", "exposure" },
		{ "umbra-depth", "near" }, { "opaque", "near" }, { "sorted", "near" },
		{ "impostors", "far-lod" }, { "far-cards", "far-lod" },
		{ "effects", "particles" },
		{ "post", "post" }, { "taa", "post" },
		{ "overlays", "overlays" },
		{ "ssgi", "gi" },
		{ "water", "water" }, { "ssr-water", "water" }, { "underwater", "water" },
		{ "cloud-shadows", "clouds" }, { "clouds", "clouds" },
		{ "far-ground", "far-ground" },
		{ "precip", "weather" },
		{ "cull-camera", "" }, { "cull-cascades", "" }, { "shape-setup", "" },	// CPU timers that hold other zones' work
	};
	for ( const auto & m : map )
		if ( std::strcmp( m[0], zone ) == 0 )
			return m[1];
	return "other";
}

QByteArray str( const QByteArray & s )
{
	QByteArray o = "\"";
	for ( char c : s ) {
		if ( c == '"' || c == '\\' )
			o += '\\';
		if ( c == '\n' )
			o += "\\n";
		else if ( c == '\r' )
			o += "\\r";
		else if ( c == '\t' )
			o += "\\t";
		else if ( static_cast<unsigned char>( c ) >= 0x20 )
			o += c;
	}
	return o + "\"";
}

QByteArray num( double v )
{
	return QByteArray::number( v, 'f', 3 );
}

} // namespace WwTelemetry
