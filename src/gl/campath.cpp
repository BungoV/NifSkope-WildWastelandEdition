#include "campath.h"

#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <cmath>

// lane MOTION1: the scripted camera path (campath.h)

bool WwCamPath::load( const QString & path, QString * error )
{
	m_keys.clear();
	m_fps = 30.0;
	m_frames = -1;
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		if ( error )
			*error = QStringLiteral( "cannot open %1" ).arg( path );
		return false;
	}
	QTextStream in( &f );
	int lineNo = 0;
	while ( !in.atEnd() ) {
		QString line = in.readLine();
		lineNo++;
		const int hash = line.indexOf( QLatin1Char( '#' ) );
		if ( hash >= 0 )
			line.truncate( hash );
		const QStringList w = line.simplified().split( QLatin1Char( ' ' ), Qt::SkipEmptyParts );
		if ( w.isEmpty() )
			continue;
		auto bad = [&]( const QString & why ) {
			if ( error )
				*error = QStringLiteral( "%1 line %2: %3" ).arg( path ).arg( lineNo ).arg( why );
			return false;
		};
		bool ok = true;
		if ( w[0] == QLatin1StringView( "fps" ) && w.size() == 2 ) {
			m_fps = w[1].toDouble( &ok );
			if ( !ok || !( m_fps > 0.0 ) )
				return bad( QStringLiteral( "fps must be positive" ) );
		} else if ( w[0] == QLatin1StringView( "frames" ) && w.size() == 2 ) {
			m_frames = w[1].toInt( &ok );
			if ( !ok || m_frames < 1 )
				return bad( QStringLiteral( "frames must be at least 1" ) );
		} else if ( w[0] == QLatin1StringView( "key" ) && ( w.size() == 8 || w.size() == 9 ) ) {
			WwCamPathKey k;
			double v[8];
			for ( int i = 0; i < w.size() - 1 && ok; i++ )
				v[i] = w[i + 1].toDouble( &ok );
			if ( !ok )
				return bad( QStringLiteral( "a key is t ex ey ez ax ay az [fov], all numbers" ) );
			k.t = v[0];
			for ( int i = 0; i < 3; i++ ) {
				k.eye[i] = v[1 + i];
				k.at[i] = v[4 + i];
			}
			if ( w.size() == 9 )
				k.fov = v[7];
			if ( !( k.fov > 0.5 && k.fov < 179.0 ) )
				return bad( QStringLiteral( "fov outside 0.5..179" ) );
			if ( !m_keys.isEmpty() && !( k.t > m_keys.last().t ) )
				return bad( QStringLiteral( "key times must increase" ) );
			m_keys.append( k );
		} else {
			return bad( QStringLiteral( "unknown line '%1'" ).arg( w[0] ) );
		}
	}
	if ( m_keys.isEmpty() ) {
		if ( error )
			*error = QStringLiteral( "%1: no keys" ).arg( path );
		return false;
	}
	return true;
}

int WwCamPath::frameCount() const
{
	if ( m_frames > 0 )
		return m_frames;
	if ( m_keys.size() < 2 )
		return 1;
	return int( std::floor( ( m_keys.last().t - m_keys.first().t ) * m_fps + 1e-9 ) ) + 1;
}

void WwCamPath::sampleAt( double t, double eye[3], double at[3], double & fov ) const
{
	const int n = int( m_keys.size() );
	if ( n == 1 || t <= 0.0 ) {
		const WwCamPathKey & k = m_keys.first();
		std::copy( k.eye, k.eye + 3, eye );
		std::copy( k.at, k.at + 3, at );
		fov = k.fov;
		return;
	}
	const double T = m_keys.first().t + t;
	if ( T >= m_keys.last().t ) {
		const WwCamPathKey & k = m_keys.last();
		std::copy( k.eye, k.eye + 3, eye );
		std::copy( k.at, k.at + 3, at );
		fov = k.fov;
		return;
	}
	int i = 0;
	while ( i + 1 < n - 1 && m_keys[i + 1].t <= T )
		i++;
	const WwCamPathKey & a = m_keys[i];
	const WwCamPathKey & b = m_keys[i + 1];
	const double h = b.t - a.t;
	const double s = ( T - a.t ) / h;
	const double s2 = s * s, s3 = s2 * s;
	const double h00 = 2 * s3 - 3 * s2 + 1, h10 = s3 - 2 * s2 + s, h01 = -2 * s3 + 3 * s2, h11 = s3 - s2;
	// Catmull-Rom tangents in units per second, one-sided at the ends
	auto tangent = [&]( int j, const double WwCamPathKey::*, int which, int c ) {
		const int lo = std::max( j - 1, 0 ), hi = std::min( j + 1, n - 1 );
		const double * pl = which ? m_keys[lo].at : m_keys[lo].eye;
		const double * ph = which ? m_keys[hi].at : m_keys[hi].eye;
		return ( ph[c] - pl[c] ) / ( m_keys[hi].t - m_keys[lo].t );
	};
	for ( int c = 0; c < 3; c++ ) {
		eye[c] = h00 * a.eye[c] + h10 * h * tangent( i, nullptr, 0, c ) + h01 * b.eye[c] + h11 * h * tangent( i + 1, nullptr, 0, c );
		at[c] = h00 * a.at[c] + h10 * h * tangent( i, nullptr, 1, c ) + h01 * b.at[c] + h11 * h * tangent( i + 1, nullptr, 1, c );
	}
	fov = a.fov + ( b.fov - a.fov ) * s;
}

void WwCamPath::sample( int frame, Vector3 & eye, Vector3 & at, float & fov ) const
{
	double e[3], l[3], f = 70.0;
	sampleAt( double( frame ) / m_fps, e, l, f );
	eye = Vector3( float( e[0] ), float( e[1] ), float( e[2] ) );
	at = Vector3( float( l[0] ), float( l[1] ), float( l[2] ) );
	fov = float( f );
}
