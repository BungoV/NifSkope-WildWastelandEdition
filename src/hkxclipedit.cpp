/* The editable clip: sparse keys over a dense per-frame clip. Lane HKXEDIT2,
   2026-09-10. The key model is stated in hkxclipedit.h; every operation here
   regenerates only the tracks it touched, and says in words what it did. */

#include "hkxclipedit.h"
#include "hkxwrite.h"
#ifdef WW_HKXCLIP_CANON
#include "hkxfile.h"
#endif

#include <QFile>
#include <QFileInfo>
#include <QObject>

#include <cmath>
#include <cstring>


/*
 *  The law between keys and frames
 */

static float hkxLen3( const Vector3 & v )
{
	return std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
}

static Vector3 hkxLerp3( const Vector3 & a, const Vector3 & b, float t )
{
	return Vector3( a[0] + ( b[0] - a[0] ) * t, a[1] + ( b[1] - a[1] ) * t, a[2] + ( b[2] - a[2] ) * t );
}

/*! Shortest-arc nlerp with an EXACT normalise.
 *
 *  Quat::normalize() in data/niftypes.h divides by the squared magnitude
 *  (lane HKX2's finding), so the normalise is done here. The same law as
 *  HkxPlayback::hkxNlerp, so the dope sheet and the viewport agree between
 *  keys; degree-1 splines are what every FO4 clip carries, and a degree-1
 *  spline between two unit quaternions is exactly this. */
static Quat hkxNlerp( const Quat & a, const Quat & b, float t )
{
	float dot = 0.0f;
	for ( int i = 0; i < 4; i++ )
		dot += a[i] * b[i];
	const float s = ( dot < 0.0f ) ? -1.0f : 1.0f;
	Quat q;
	float len2 = 0.0f;
	for ( int i = 0; i < 4; i++ ) {
		q[i] = a[i] + ( s * b[i] - a[i] ) * t;
		len2 += q[i] * q[i];
	}
	const float len = std::sqrt( len2 );
	if ( len > 0.0f ) {
		for ( int i = 0; i < 4; i++ )
			q[i] /= len;
	} else {
		q = Quat( 1.0f, 0.0f, 0.0f, 0.0f );
	}
	return q;
}

HkxTransform HkxClipDocument::interpolate( const HkxTransform & a, const HkxTransform & b, float t )
{
	if ( t <= 0.0f )
		return a;
	if ( t >= 1.0f )
		return b;
	HkxTransform out;
	out.translation = hkxLerp3( a.translation, b.translation, t );
	out.rotation = hkxNlerp( a.rotation, b.rotation, t );
	out.scale = hkxLerp3( a.scale, b.scale, t );
	return out;
}

float HkxClipDocument::angleDeg( const Quat & a, const Quat & b )
{
	// 4*asin(|q1 -+ q2|/2): lane HKX5's correction (2*asin is half the angle;
	// acos(dot) has no resolution below 0.03 deg).
	float dm = 0.0f, dp = 0.0f;
	for ( int i = 0; i < 4; i++ ) {
		dm += ( a[i] - b[i] ) * ( a[i] - b[i] );
		dp += ( a[i] + b[i] ) * ( a[i] + b[i] );
	}
	const float d = std::sqrt( std::min( dm, dp ) );
	const float x = std::min( 1.0f, d * 0.5f );
	return 4.0f * std::asin( x ) * 180.0f / float( M_PI );
}

bool HkxClipDocument::transformsEqual( const HkxTransform & a, const HkxTransform & b )
{
	// bit patterns, never a tolerance: this is the gate's instrument
	for ( int i = 0; i < 3; i++ ) {
		if ( std::memcmp( &a.translation[i], &b.translation[i], sizeof( float ) ) != 0 )
			return false;
		if ( std::memcmp( &a.scale[i], &b.scale[i], sizeof( float ) ) != 0 )
			return false;
	}
	for ( int i = 0; i < 4; i++ ) {
		if ( std::memcmp( &a.rotation[i], &b.rotation[i], sizeof( float ) ) != 0 )
			return false;
	}
	return true;
}

bool HkxClipDocument::framesEqual( const HkxAnimClip & a, const HkxAnimClip & b, int * frame, int * track )
{
	if ( frame ) *frame = -1;
	if ( track ) *track = -1;
	if ( a.frames.count() != b.frames.count() ) {
		if ( frame ) *frame = std::min( a.frames.count(), b.frames.count() );
		return false;
	}
	for ( int f = 0; f < a.frames.count(); f++ ) {
		const QVector<HkxTransform> & fa = a.frames.at( f );
		const QVector<HkxTransform> & fb = b.frames.at( f );
		if ( fa.count() != fb.count() ) {
			if ( frame ) *frame = f;
			if ( track ) *track = std::min( fa.count(), fb.count() );
			return false;
		}
		for ( int t = 0; t < fa.count(); t++ ) {
			if ( !transformsEqual( fa.at( t ), fb.at( t ) ) ) {
				if ( frame ) *frame = f;
				if ( track ) *track = t;
				return false;
			}
		}
	}
	return true;
}


/*
 *  Construction and lookups
 */

HkxClipDocument HkxClipDocument::fromClip( const HkxAnimClip & c, const QStringList & names )
{
	HkxClipDocument d;
	d.clip = c;
	d.keys.resize( c.numTracks );
	for ( int t = 0; t < c.numTracks; t++ ) {
		QVector<HkxKey> & ks = d.keys[t];
		ks.reserve( c.numFrames );
		for ( int f = 0; f < c.numFrames && f < c.frames.count(); f++ ) {
			HkxKey k;
			k.frame = f;
			if ( t < c.frames.at( f ).count() )
				k.xf = c.frames.at( f ).at( t );
			ks.append( k );
		}
	}
	d.trackNames = names;
	while ( d.trackNames.count() < c.numTracks )
		d.trackNames.append( QString() );
	while ( d.trackNames.count() > c.numTracks )
		d.trackNames.removeLast();
	if ( d.clip.annotations.count() < c.numTracks )
		d.clip.annotations.resize( c.numTracks );
	return d;
}

int HkxClipDocument::frameOfTime( float seconds ) const
{
	if ( clip.frameDuration <= 0.0f )
		return 0;
	return int( std::lround( double( seconds ) / double( clip.frameDuration ) ) );
}

int HkxClipDocument::findTrack( const QString & name ) const
{
	for ( int t = 0; t < trackNames.count(); t++ ) {
		if ( trackNames.at( t ).compare( name, Qt::CaseInsensitive ) == 0 )
			return t;
	}
	return -1;
}

const HkxKey * HkxClipDocument::keyAt( int track, int frame ) const
{
	if ( track < 0 || track >= keys.count() )
		return nullptr;
	for ( const HkxKey & k : keys.at( track ) ) {
		if ( k.frame == frame )
			return &k;
		if ( k.frame > frame )
			break;
	}
	return nullptr;
}

void HkxClipDocument::sortKeys( int track )
{
	QVector<HkxKey> & ks = keys[track];
	std::sort( ks.begin(), ks.end(), []( const HkxKey & a, const HkxKey & b ) { return a.frame < b.frame; } );
	// unique frames: the later entry wins (a moved key over an unmoved one)
	for ( int i = ks.count() - 2; i >= 0; i-- ) {
		if ( ks.at( i ).frame == ks.at( i + 1 ).frame )
			ks.removeAt( i );
	}
}


/*
 *  Regeneration
 */

void HkxClipDocument::regenerateTrack( int track )
{
	if ( track < 0 || track >= keys.count() )
		return;
	const QVector<HkxKey> & ks = keys.at( track );
	if ( ks.isEmpty() )
		return;
	if ( clip.frames.count() < clip.numFrames )
		clip.frames.resize( clip.numFrames );
	int k = 0;
	for ( int f = 0; f < clip.numFrames; f++ ) {
		QVector<HkxTransform> & row = clip.frames[f];
		if ( row.count() < clip.numTracks )
			row.resize( clip.numTracks );
		while ( k + 1 < ks.count() && ks.at( k + 1 ).frame <= f )
			k++;
		const HkxKey & a = ks.at( k );
		if ( f <= a.frame || k + 1 >= ks.count() ) {
			row[track] = a.xf;              // at or before the first key / after the last: verbatim
			continue;
		}
		const HkxKey & b = ks.at( k + 1 );
		if ( f == a.frame ) {
			row[track] = a.xf;
		} else {
			const float t = float( f - a.frame ) / float( b.frame - a.frame );
			row[track] = interpolate( a.xf, b.xf, t );
		}
	}
}

void HkxClipDocument::regenerateAll()
{
	for ( int t = 0; t < keys.count(); t++ )
		regenerateTrack( t );
}

bool HkxClipDocument::consistent( int * firstTrack, int * firstFrame ) const
{
	HkxClipDocument copy = *this;
	copy.regenerateAll();
	int f = -1, t = -1;
	const bool same = framesEqual( clip, copy.clip, &f, &t );
	if ( firstTrack ) *firstTrack = t;
	if ( firstFrame ) *firstFrame = f;
	return same;
}


/*
 *  Keys
 */

HkxEditResult HkxClipDocument::insertKey( int track, int frame, const HkxTransform & xf )
{
	if ( track < 0 || track >= keys.count() )
		return HkxEditResult::refused( QObject::tr( "Track %1 does not exist (the clip has %2)." ).arg( track ).arg( keys.count() ) );
	if ( frame < 0 || frame >= clip.numFrames )
		return HkxEditResult::refused( QObject::tr( "Frame %1 is outside the clip (0..%2)." ).arg( frame ).arg( clip.numFrames - 1 ) );
	QVector<HkxKey> & ks = keys[track];
	bool replaced = false;
	for ( HkxKey & k : ks ) {
		if ( k.frame == frame ) {
			k.xf = xf;
			replaced = true;
			break;
		}
	}
	if ( !replaced ) {
		HkxKey k;
		k.frame = frame;
		k.xf = xf;
		ks.append( k );
		sortKeys( track );
	}
	regenerateTrack( track );
	return HkxEditResult::done( QObject::tr( "%1 key on %2 at frame %3." )
		.arg( replaced ? QObject::tr( "Replaced the" ) : QObject::tr( "Inserted a" ) )
		.arg( trackNames.value( track ).isEmpty() ? QObject::tr( "track %1" ).arg( track ) : trackNames.value( track ) )
		.arg( frame ) );
}

HkxEditResult HkxClipDocument::deleteKeys( const QVector<HkxKeyRef> & which )
{
	if ( which.isEmpty() )
		return HkxEditResult::refused( QObject::tr( "No key is selected." ) );
	QVector<int> touched;
	int deleted = 0;
	for ( const HkxKeyRef & r : which ) {
		if ( r.track < 0 || r.track >= keys.count() )
			continue;
		QVector<HkxKey> & ks = keys[r.track];
		if ( ks.count() <= 1 )
			return HkxEditResult::refused( QObject::tr( "%1 would be left with no key; a track needs at least one." )
				.arg( trackNames.value( r.track ).isEmpty() ? QObject::tr( "Track %1" ).arg( r.track ) : trackNames.value( r.track ) ) );
		for ( int i = 0; i < ks.count(); i++ ) {
			if ( ks.at( i ).frame == r.frame ) {
				ks.removeAt( i );
				deleted++;
				if ( !touched.contains( r.track ) )
					touched.append( r.track );
				break;
			}
		}
	}
	for ( int t : touched )
		regenerateTrack( t );
	if ( deleted == 0 )
		return HkxEditResult::refused( QObject::tr( "None of the selected keys exists." ) );
	return HkxEditResult::done( QObject::tr( "Deleted %1 key(s) on %2 track(s)." ).arg( deleted ).arg( touched.count() ) );
}

HkxEditResult HkxClipDocument::moveKeys( const QVector<HkxKeyRef> & which, int deltaFrames, bool copy )
{
	if ( which.isEmpty() )
		return HkxEditResult::refused( QObject::tr( "No key is selected." ) );
	if ( deltaFrames == 0 && !copy )
		return HkxEditResult::done( QObject::tr( "Nothing moved." ) );
	// clamp so the whole selection stays inside the clip
	int lo = clip.numFrames, hi = -1;
	for ( const HkxKeyRef & r : which ) {
		lo = std::min( lo, r.frame );
		hi = std::max( hi, r.frame );
	}
	int d = deltaFrames;
	if ( lo + d < 0 )
		d = -lo;
	if ( hi + d > clip.numFrames - 1 )
		d = clip.numFrames - 1 - hi;
	if ( d == 0 && !copy )
		return HkxEditResult::done( QObject::tr( "The selection is already against the clip's edge." ) );

	QVector<int> touched;
	for ( int t = 0; t < keys.count(); t++ ) {
		QVector<HkxKey> moved;
		QVector<HkxKey> & ks = keys[t];
		for ( int i = ks.count() - 1; i >= 0; i-- ) {
			if ( !which.contains( HkxKeyRef{ t, ks.at( i ).frame } ) )
				continue;
			HkxKey k = ks.at( i );
			k.frame += d;
			moved.append( k );
			if ( !copy )
				ks.removeAt( i );
		}
		if ( moved.isEmpty() )
			continue;
		// the moved keys go in LAST so sortKeys' "later wins" gives them the frame
		for ( const HkxKey & k : moved ) {
			for ( int i = ks.count() - 1; i >= 0; i-- ) {
				if ( ks.at( i ).frame == k.frame )
					ks.removeAt( i );
			}
			ks.append( k );
		}
		sortKeys( t );
		touched.append( t );
	}
	for ( int t : touched )
		regenerateTrack( t );
	return HkxEditResult::done( QObject::tr( "%1 %2 key(s) by %3 frame(s) on %4 track(s)." )
		.arg( copy ? QObject::tr( "Copied" ) : QObject::tr( "Moved" ) ).arg( which.count() ).arg( d ).arg( touched.count() ) );
}

HkxEditResult HkxClipDocument::reduce( float tolTranslation, float tolDegrees, float tolScale, int * removed )
{
	int before = 0, after = 0;
	for ( int t = 0; t < keys.count(); t++ ) {
		before += keys.at( t ).count();
		if ( clip.numFrames < 3 || keys.at( t ).count() < 3 ) {
			after += keys.at( t ).count();
			continue;
		}
		/* Greedy refinement against the DENSE frames (the truth): start with the
		 * first and last frame, find the frame the interpolation misses by the
		 * most, key it, repeat until every frame is within tolerance. Every
		 * kept key reads the dense frame verbatim, so a reduced track still
		 * hits its keyed frames exactly. */
		QVector<int> kept;
		kept.append( 0 );
		kept.append( clip.numFrames - 1 );
		for ( ;; ) {
			int worstFrame = -1;
			float worstScore = 0.0f;
			for ( int i = 0; i + 1 < kept.count(); i++ ) {
				const int f0 = kept.at( i ), f1 = kept.at( i + 1 );
				const HkxTransform & a = clip.frames.at( f0 ).at( t );
				const HkxTransform & b = clip.frames.at( f1 ).at( t );
				for ( int f = f0 + 1; f < f1; f++ ) {
					const HkxTransform x = interpolate( a, b, float( f - f0 ) / float( f1 - f0 ) );
					const HkxTransform & want = clip.frames.at( f ).at( t );
					const float et = hkxLen3( want.translation - x.translation );
					const float er = angleDeg( want.rotation, x.rotation );
					const float es = hkxLen3( want.scale - x.scale );
					// each error in units of its own tolerance; > 1 = out
					float score = 0.0f;
					if ( tolTranslation > 0.0f ) score = std::max( score, et / tolTranslation );
					else if ( et > 0.0f ) score = std::max( score, 2.0f );
					if ( tolDegrees > 0.0f ) score = std::max( score, er / tolDegrees );
					else if ( er > 0.0f ) score = std::max( score, 2.0f );
					if ( tolScale > 0.0f ) score = std::max( score, es / tolScale );
					else if ( es > 0.0f ) score = std::max( score, 2.0f );
					if ( score > worstScore ) {
						worstScore = score;
						worstFrame = f;
					}
				}
			}
			if ( worstFrame < 0 || worstScore <= 1.0f )
				break;
			kept.append( worstFrame );
			std::sort( kept.begin(), kept.end() );
		}
		QVector<HkxKey> ks;
		for ( int f : kept ) {
			HkxKey k;
			k.frame = f;
			k.xf = clip.frames.at( f ).at( t );
			ks.append( k );
		}
		keys[t] = ks;
		after += ks.count();
		// NOT regenerated: the dense frames are the truth the keys were fitted
		// to, and regenerating would replace them by the fit. The invariant
		// consistent() is then within tolerance, not bit-exact -- by design.
	}
	if ( removed )
		*removed = before - after;
	return HkxEditResult::done( QObject::tr( "Reduced %1 keys to %2 (tolerance %3 units, %4 deg, %5 scale)." )
		.arg( before ).arg( after ).arg( tolTranslation ).arg( tolDegrees ).arg( tolScale ) );
}


/*
 *  Annotations
 */

int HkxClipDocument::annotationCount() const
{
	int n = 0;
	for ( const QVector<HkxAnnotation> & a : clip.annotations )
		n += a.count();
	return n;
}

HkxEditResult HkxClipDocument::addAnnotation( int track, float time, const QString & text, int * index )
{
	if ( clip.annotations.count() < clip.numTracks )
		clip.annotations.resize( clip.numTracks );
	if ( track < 0 || track >= clip.annotations.count() )
		return HkxEditResult::refused( QObject::tr( "Track %1 does not exist." ).arg( track ) );
	if ( text.trimmed().isEmpty() )
		return HkxEditResult::refused( QObject::tr( "An annotation needs a name." ) );
	if ( time < 0.0f || time > clip.duration + clip.frameDuration )
		return HkxEditResult::refused( QObject::tr( "%1 s is outside the clip (0..%2 s)." ).arg( time ).arg( clip.duration ) );
	QVector<HkxAnnotation> & list = clip.annotations[track];
	HkxAnnotation a;
	a.time = time;
	a.text = text.trimmed();
	// kept in time order, as every shipped clip is
	int at = list.count();
	for ( int i = 0; i < list.count(); i++ ) {
		if ( list.at( i ).time > time ) {
			at = i;
			break;
		}
	}
	list.insert( at, a );
	if ( index )
		*index = at;
	return HkxEditResult::done( QObject::tr( "Added annotation '%1' at frame %2 (%3 s) on track %4." )
		.arg( a.text ).arg( frameOfTime( time ) ).arg( time ).arg( track ) );
}

HkxEditResult HkxClipDocument::renameAnnotation( int track, int index, const QString & text )
{
	if ( track < 0 || track >= clip.annotations.count() || index < 0 || index >= clip.annotations.at( track ).count() )
		return HkxEditResult::refused( QObject::tr( "There is no annotation %1 on track %2." ).arg( index ).arg( track ) );
	if ( text.trimmed().isEmpty() )
		return HkxEditResult::refused( QObject::tr( "An annotation needs a name." ) );
	const QString old = clip.annotations.at( track ).at( index ).text;
	clip.annotations[track][index].text = text.trimmed();
	return HkxEditResult::done( QObject::tr( "Renamed annotation '%1' to '%2'." ).arg( old, text.trimmed() ) );
}

HkxEditResult HkxClipDocument::moveAnnotation( int track, int index, float time )
{
	if ( track < 0 || track >= clip.annotations.count() || index < 0 || index >= clip.annotations.at( track ).count() )
		return HkxEditResult::refused( QObject::tr( "There is no annotation %1 on track %2." ).arg( index ).arg( track ) );
	if ( time < 0.0f || time > clip.duration + clip.frameDuration )
		return HkxEditResult::refused( QObject::tr( "%1 s is outside the clip (0..%2 s)." ).arg( time ).arg( clip.duration ) );
	HkxAnnotation a = clip.annotations.at( track ).at( index );
	clip.annotations[track].removeAt( index );
	a.time = time;
	QVector<HkxAnnotation> & list = clip.annotations[track];
	int at = list.count();
	for ( int i = 0; i < list.count(); i++ ) {
		if ( list.at( i ).time > time ) {
			at = i;
			break;
		}
	}
	list.insert( at, a );
	return HkxEditResult::done( QObject::tr( "Moved annotation '%1' to frame %2." ).arg( a.text ).arg( frameOfTime( time ) ) );
}

HkxEditResult HkxClipDocument::deleteAnnotation( int track, int index )
{
	if ( track < 0 || track >= clip.annotations.count() || index < 0 || index >= clip.annotations.at( track ).count() )
		return HkxEditResult::refused( QObject::tr( "There is no annotation %1 on track %2." ).arg( index ).arg( track ) );
	const QString old = clip.annotations.at( track ).at( index ).text;
	clip.annotations[track].removeAt( index );
	return HkxEditResult::done( QObject::tr( "Deleted annotation '%1'." ).arg( old ) );
}


/*
 *  Float tracks
 */

HkxEditResult HkxClipDocument::addFloatTrack( const QString & name, int * index )
{
	HkxFloatTrackDoc ft;
	ft.name = name.trimmed().isEmpty() ? QObject::tr( "float %1" ).arg( floatTracks.count() ) : name.trimmed();
	HkxFloatKey k;
	k.frame = 0;
	k.value = 0.0f;
	ft.keys.append( k );
	floatTracks.append( ft );
	regenerateFloatTrack( floatTracks.count() - 1 );
	if ( index )
		*index = floatTracks.count() - 1;
	return HkxEditResult::done( QObject::tr( "Added float track '%1' (a row of this document; not yet written to .hkx)." ).arg( ft.name ) );
}

HkxEditResult HkxClipDocument::removeFloatTrack( int index )
{
	if ( index < 0 || index >= floatTracks.count() )
		return HkxEditResult::refused( QObject::tr( "There is no float track %1." ).arg( index ) );
	const QString n = floatTracks.at( index ).name;
	floatTracks.removeAt( index );
	return HkxEditResult::done( QObject::tr( "Removed float track '%1'." ).arg( n ) );
}

HkxEditResult HkxClipDocument::setFloatKey( int index, int frame, float value )
{
	if ( index < 0 || index >= floatTracks.count() )
		return HkxEditResult::refused( QObject::tr( "There is no float track %1." ).arg( index ) );
	if ( frame < 0 || frame >= clip.numFrames )
		return HkxEditResult::refused( QObject::tr( "Frame %1 is outside the clip (0..%2)." ).arg( frame ).arg( clip.numFrames - 1 ) );
	QVector<HkxFloatKey> & ks = floatTracks[index].keys;
	bool replaced = false;
	for ( HkxFloatKey & k : ks ) {
		if ( k.frame == frame ) {
			k.value = value;
			replaced = true;
		}
	}
	if ( !replaced ) {
		HkxFloatKey k;
		k.frame = frame;
		k.value = value;
		ks.append( k );
		std::sort( ks.begin(), ks.end(), []( const HkxFloatKey & a, const HkxFloatKey & b ) { return a.frame < b.frame; } );
	}
	regenerateFloatTrack( index );
	return HkxEditResult::done( QObject::tr( "%1 float key %2 at frame %3 on '%4'." )
		.arg( replaced ? QObject::tr( "Set" ) : QObject::tr( "Added" ) ).arg( value ).arg( frame ).arg( floatTracks.at( index ).name ) );
}

HkxEditResult HkxClipDocument::deleteFloatKey( int index, int frame )
{
	if ( index < 0 || index >= floatTracks.count() )
		return HkxEditResult::refused( QObject::tr( "There is no float track %1." ).arg( index ) );
	QVector<HkxFloatKey> & ks = floatTracks[index].keys;
	if ( ks.count() <= 1 )
		return HkxEditResult::refused( QObject::tr( "'%1' would be left with no key." ).arg( floatTracks.at( index ).name ) );
	for ( int i = 0; i < ks.count(); i++ ) {
		if ( ks.at( i ).frame == frame ) {
			ks.removeAt( i );
			regenerateFloatTrack( index );
			return HkxEditResult::done( QObject::tr( "Deleted the float key at frame %1." ).arg( frame ) );
		}
	}
	return HkxEditResult::refused( QObject::tr( "There is no float key at frame %1." ).arg( frame ) );
}

void HkxClipDocument::regenerateFloatTrack( int index )
{
	if ( index < 0 || index >= floatTracks.count() )
		return;
	HkxFloatTrackDoc & ft = floatTracks[index];
	ft.values.resize( clip.numFrames );
	if ( ft.keys.isEmpty() )
		return;
	int k = 0;
	for ( int f = 0; f < clip.numFrames; f++ ) {
		while ( k + 1 < ft.keys.count() && ft.keys.at( k + 1 ).frame <= f )
			k++;
		const HkxFloatKey & a = ft.keys.at( k );
		if ( f <= a.frame || k + 1 >= ft.keys.count() ) {
			ft.values[f] = a.value;
			continue;
		}
		const HkxFloatKey & b = ft.keys.at( k + 1 );
		const float t = float( f - a.frame ) / float( b.frame - a.frame );
		ft.values[f] = a.value + ( b.value - a.value ) * t;
	}
}


/*
 *  The clip as a whole
 */

void HkxClipDocument::addBoundaryKeys( int track )
{
	if ( track < 0 || track >= keys.count() || clip.numFrames < 1 )
		return;
	QVector<HkxKey> & ks = keys[track];
	const int last = clip.numFrames - 1;
	bool has0 = false, hasN = false;
	for ( const HkxKey & k : ks ) {
		if ( k.frame == 0 ) has0 = true;
		if ( k.frame == last ) hasN = true;
	}
	if ( !has0 ) {
		HkxKey k;
		k.frame = 0;
		k.xf = clip.frames.at( 0 ).at( track );
		ks.append( k );
	}
	if ( !hasN && last > 0 ) {
		HkxKey k;
		k.frame = last;
		k.xf = clip.frames.at( last ).at( track );
		ks.append( k );
	}
	sortKeys( track );
}

int HkxClipDocument::rangeFirstFrame() const
{
	return std::max( 0, std::min( clip.numFrames - 1, rangeFirst ) );
}

int HkxClipDocument::rangeLastFrame() const
{
	if ( rangeLast < 0 )
		return std::max( 0, clip.numFrames - 1 );
	return std::max( rangeFirstFrame(), std::min( clip.numFrames - 1, rangeLast ) );
}

bool HkxClipDocument::rangeIsPartial() const
{
	return rangeFirstFrame() > 0 || rangeLastFrame() < clip.numFrames - 1;
}

HkxEditResult HkxClipDocument::setPlayRange( int first, int last )
{
	if ( clip.numFrames < 2 )
		return HkxEditResult::refused( QObject::tr( "A clip of %1 frame(s) has no range to set." ).arg( clip.numFrames ) );
	first = std::max( 0, std::min( clip.numFrames - 1, first ) );
	last = std::max( 0, std::min( clip.numFrames - 1, last ) );
	if ( last <= first )
		return HkxEditResult::refused( QObject::tr( "The play range must keep at least two frames: %1..%2 of 0..%3." )
			.arg( first ).arg( last ).arg( clip.numFrames - 1 ) );
	rangeFirst = first;
	rangeLast = last;
	return HkxEditResult::done( QObject::tr( "Play range %1..%2: %3 frames, %4 s of the clip's %5 s." )
		.arg( first ).arg( last ).arg( last - first + 1 )
		.arg( float( last - first ) * clip.frameDuration ).arg( clip.duration ) );
}

HkxEditResult HkxClipDocument::trim( int first, int last )
{
	if ( first < 0 || last >= clip.numFrames || first >= last )
		return HkxEditResult::refused( QObject::tr( "Trim range %1..%2 must lie inside 0..%3 and keep at least two frames." )
			.arg( first ).arg( last ).arg( clip.numFrames - 1 ) );
	const int oldFrames = clip.numFrames;
	const int n = last - first + 1;
	// dense frames: the slice, verbatim
	QVector<QVector<HkxTransform>> fr;
	fr.reserve( n );
	for ( int f = first; f <= last; f++ )
		fr.append( clip.frames.at( f ) );
	clip.frames = fr;
	clip.numFrames = n;
	clip.duration = float( n - 1 ) * clip.frameDuration;
	// keys: re-based; outside dropped; boundaries keyed verbatim
	for ( int t = 0; t < keys.count(); t++ ) {
		QVector<HkxKey> ks;
		for ( const HkxKey & k : keys.at( t ) ) {
			if ( k.frame < first || k.frame > last )
				continue;
			HkxKey m = k;
			m.frame -= first;
			ks.append( m );
		}
		keys[t] = ks;
		addBoundaryKeys( t );
	}
	// annotations: shifted; outside dropped
	const float t0 = float( first ) * clip.frameDuration;
	for ( QVector<HkxAnnotation> & list : clip.annotations ) {
		QVector<HkxAnnotation> kept;
		for ( HkxAnnotation a : list ) {
			const int f = frameOfTime( a.time );
			if ( f < first || f > last )
				continue;
			a.time -= t0;
			if ( a.time < 0.0f )
				a.time = 0.0f;
			kept.append( a );
		}
		list = kept;
	}
	// root motion: the slice, verbatim (NOT re-based to the origin: the
	// engine's reference frame is sampled by time, and a clip that starts
	// mid-travel starts mid-travel)
	if ( !clip.rootMotion.isEmpty() ) {
		QVector<HkxRootMotion> rm;
		for ( int f = first; f <= last && f < clip.rootMotion.count(); f++ )
			rm.append( clip.rootMotion.at( f ) );
		clip.rootMotion = rm;
	}
	if ( !bakedTranslations.isEmpty() ) {
		bakedTranslations = bakedTranslations.mid( first, n );
		rootMotionBeforeBake = rootMotionBeforeBake.mid( first, n );
	}
	// float tracks: keys re-based, values regenerated
	for ( int i = 0; i < floatTracks.count(); i++ ) {
		QVector<HkxFloatKey> ks;
		for ( const HkxFloatKey & k : floatTracks.at( i ).keys ) {
			if ( k.frame < first || k.frame > last )
				continue;
			HkxFloatKey m = k;
			m.frame -= first;
			ks.append( m );
		}
		if ( ks.isEmpty() ) {
			HkxFloatKey m;
			m.frame = 0;
			m.value = floatTracks.at( i ).values.value( first, 0.0f );
			ks.append( m );
		}
		floatTracks[i].keys = ks;
		regenerateFloatTrack( i );
	}
	// the range was frame numbers on the OLD grid: the cut retires it
	rangeFirst = 0;
	rangeLast = -1;
	return HkxEditResult::done( QObject::tr( "Trimmed to frames %1..%2: %3 of %4 frames kept, %5 s." )
		.arg( first ).arg( last ).arg( n ).arg( oldFrames ).arg( clip.duration ) );
}

HkxEditResult HkxClipDocument::retime( float newFps )
{
	if ( !( newFps > 0.0f ) || newFps > 1000.0f )
		return HkxEditResult::refused( QObject::tr( "%1 is not a usable frame rate." ).arg( newFps ) );
	if ( clip.frameDuration <= 0.0f || clip.numFrames < 2 )
		return HkxEditResult::refused( QObject::tr( "The clip has no frame rate to retime from." ) );
	const double oldFps = 1.0 / double( clip.frameDuration );
	const double duration = double( clip.numFrames - 1 ) * double( clip.frameDuration );
	const int n = std::max( 2, int( std::lround( duration * double( newFps ) ) ) + 1 );
	const float newDur = 1.0f / newFps;
	const int oldN = clip.numFrames;

	auto oldFrameAt = [&]( int i, int & k0, int & k1, float & t, bool & exact ) {
		const double time = double( i ) / double( newFps );
		const double of = time * oldFps;
		const int k = int( std::lround( of ) );
		if ( std::fabs( of - double( k ) ) < 1.0e-4 && k >= 0 && k < oldN ) {
			k0 = k1 = k;
			t = 0.0f;
			exact = true;
			return;
		}
		exact = false;
		k0 = std::max( 0, std::min( oldN - 1, int( std::floor( of ) ) ) );
		k1 = std::min( oldN - 1, k0 + 1 );
		t = float( of - double( k0 ) );
		if ( t < 0.0f ) t = 0.0f;
		if ( t > 1.0f ) t = 1.0f;
	};

	QVector<QVector<HkxTransform>> fr( n );
	QVector<HkxRootMotion> rm;
	QVector<QVector<float>> fv( floatTracks.count() );
	int exactFrames = 0;
	for ( int i = 0; i < n; i++ ) {
		int k0, k1;
		float t;
		bool exact;
		oldFrameAt( i, k0, k1, t, exact );
		if ( exact )
			exactFrames++;
		QVector<HkxTransform> row( clip.numTracks );
		for ( int tr = 0; tr < clip.numTracks; tr++ ) {
			const HkxTransform & a = clip.frames.at( k0 ).at( tr );
			if ( exact ) {
				row[tr] = a;
			} else {
				row[tr] = interpolate( a, clip.frames.at( k1 ).at( tr ), t );
			}
		}
		fr[i] = row;
		if ( !clip.rootMotion.isEmpty() ) {
			HkxRootMotion r;
			const HkxRootMotion & a = clip.rootMotion.at( std::min( k0, int( clip.rootMotion.count() ) - 1 ) );
			if ( exact ) {
				r = a;
			} else {
				const HkxRootMotion & b = clip.rootMotion.at( std::min( k1, int( clip.rootMotion.count() ) - 1 ) );
				r.translation = hkxLerp3( a.translation, b.translation, t );
				r.yaw = a.yaw + ( b.yaw - a.yaw ) * t;
			}
			rm.append( r );
		}
		for ( int ft = 0; ft < floatTracks.count(); ft++ ) {
			const QVector<float> & v = floatTracks.at( ft ).values;
			const float a = v.value( k0, 0.0f );
			fv[ft].append( exact ? a : a + ( v.value( k1, a ) - a ) * t );
		}
	}
	clip.frames = fr;
	clip.rootMotion = rm;
	clip.numFrames = n;
	clip.frameDuration = newDur;
	clip.duration = float( n - 1 ) * newDur;
	// keys: every frame again (the reduction cannot be carried across a grid change)
	keys.clear();
	keys.resize( clip.numTracks );
	for ( int tr = 0; tr < clip.numTracks; tr++ ) {
		for ( int f = 0; f < n; f++ ) {
			HkxKey k;
			k.frame = f;
			k.xf = clip.frames.at( f ).at( tr );
			keys[tr].append( k );
		}
	}
	for ( int ft = 0; ft < floatTracks.count(); ft++ ) {
		floatTracks[ft].values = fv.at( ft );
		floatTracks[ft].keys.clear();
		for ( int f = 0; f < n; f++ ) {
			HkxFloatKey k;
			k.frame = f;
			k.value = fv.at( ft ).at( f );
			floatTracks[ft].keys.append( k );
		}
	}
	// a bake's memory is on the old grid; it cannot be undone exactly any more
	bakedTranslations.clear();
	rootMotionBeforeBake.clear();
	// same reason as the trim: the frame numbers the range named are gone
	rangeFirst = 0;
	rangeLast = -1;
	return HkxEditResult::done( QObject::tr( "Retimed %1 -> %2 fps: %3 frames (%4 coincident with the old grid, copied exactly; the rest interpolated); keys at every frame again." )
		.arg( oldFps ).arg( newFps ).arg( n ).arg( exactFrames ) );
}

HkxEditResult HkxClipDocument::removeTrack( int track )
{
	if ( track < 0 || track >= clip.numTracks )
		return HkxEditResult::refused( QObject::tr( "Track %1 does not exist." ).arg( track ) );
	if ( clip.numTracks <= 1 )
		return HkxEditResult::refused( QObject::tr( "The clip's last track cannot be removed." ) );
	const QString name = trackNames.value( track );
	for ( QVector<HkxTransform> & row : clip.frames )
		row.removeAt( track );
	keys.removeAt( track );
	trackNames.removeAt( track );
	if ( track < clip.annotations.count() )
		clip.annotations.removeAt( track );
	if ( track < clip.trackToBone.count() )
		clip.trackToBone.removeAt( track );
	clip.numTracks--;
	if ( bakedTrack == track ) {
		bakedTrack = -1;
		bakedTranslations.clear();
		rootMotionBeforeBake.clear();
	} else if ( bakedTrack > track ) {
		bakedTrack--;
	}
	return HkxEditResult::done( QObject::tr( "Removed track %1%2; %3 tracks remain." )
		.arg( track ).arg( name.isEmpty() ? QString() : QStringLiteral( " (" ) + name + QStringLiteral( ")" ) ).arg( clip.numTracks ) );
}

HkxEditResult HkxClipDocument::removeTransformAxes( int track, const HkxAxisMask & mask )
{
	/* bungo's ruling 3, 2026-09-12: hold the ticked directions still so "the
	   slide stays in the center, but the COM still moves downward when the
	   player crouches".

	   THE REFERENCE IS FRAME 0, not zero. Zeroing a translation would teleport
	   the bone to the skeleton's origin; frame 0 is where the animation starts,
	   so a stripped axis simply stops moving. The rotation triple is
	   NifSkope's own Matrix Euler (toEuler / fromEuler), about the bone's local
	   axes; a key whose rotation is left alone is not converted at all, so
	   nothing is lost to a round trip it did not need. */
	if ( track < 0 || track >= clip.numTracks || track >= keys.count() )
		return HkxEditResult::refused( QObject::tr( "Track %1 does not exist." ).arg( track ) );
	if ( !mask.any() )
		return HkxEditResult::refused( QObject::tr( "Nothing was ticked, so nothing was removed." ) );
	if ( keys.at( track ).isEmpty() || clip.frames.isEmpty() )
		return HkxEditResult::refused( QObject::tr( "Track %1 has no keys to change." ).arg( track ) );

	const HkxTransform ref = clip.frames.at( 0 ).value( track );
	float rx0 = 0.0f, ry0 = 0.0f, rz0 = 0.0f;
	bool refOk = true;
	if ( mask.rx || mask.ry || mask.rz ) {
		Matrix m0;
		m0.fromQuat( ref.rotation );
		refOk = m0.toEuler( rx0, ry0, rz0 );
	}

	int changed = 0, locked = 0;
	float maxMove = 0.0f, maxDeg = 0.0f;
	for ( HkxKey & k : keys[track] ) {
		const HkxTransform before = k.xf;
		if ( mask.tx )
			k.xf.translation[0] = ref.translation[0];
		if ( mask.ty )
			k.xf.translation[1] = ref.translation[1];
		if ( mask.tz )
			k.xf.translation[2] = ref.translation[2];
		if ( mask.rx || mask.ry || mask.rz ) {
			float ex = 0.0f, ey = 0.0f, ez = 0.0f;
			Matrix mk;
			mk.fromQuat( k.xf.rotation );
			if ( !mk.toEuler( ex, ey, ez ) )
				locked++;
			if ( mask.rx )
				ex = rx0;
			if ( mask.ry )
				ey = ry0;
			if ( mask.rz )
				ez = rz0;
			Matrix mn;
			mn.fromEuler( ex, ey, ez );
			k.xf.rotation = mn.toQuat();
		}
		if ( !transformsEqual( before, k.xf ) ) {
			changed++;
			maxMove = std::max( maxMove, ( k.xf.translation - before.translation ).length() );
			maxDeg = std::max( maxDeg, angleDeg( before.rotation, k.xf.rotation ) );
		}
	}
	regenerateTrack( track );

	QStringList held;
	if ( mask.tx ) held << QObject::tr( "translation X" );
	if ( mask.ty ) held << QObject::tr( "translation Y" );
	if ( mask.tz ) held << QObject::tr( "translation Z" );
	if ( mask.rx ) held << QObject::tr( "rotation X" );
	if ( mask.ry ) held << QObject::tr( "rotation Y" );
	if ( mask.rz ) held << QObject::tr( "rotation Z" );
	QString msg = QObject::tr( "Held %1 at their frame-0 value on track %2%3: %4 of %5 keys changed, by up to %6 units and %7 deg." )
		.arg( held.join( QStringLiteral( ", " ) ) ).arg( track )
		.arg( trackNames.value( track ).isEmpty() ? QString() : QStringLiteral( " (" ) + trackNames.value( track ) + QStringLiteral( ")" ) )
		.arg( changed ).arg( keys.at( track ).count() ).arg( maxMove, 0, 'f', 3 ).arg( maxDeg, 0, 'f', 2 );
	if ( locked > 0 || !refOk )
		msg += QObject::tr( " %1 key(s) were at gimbal lock, where the Euler triple is not unique; their Z was read as 0." ).arg( locked + ( refOk ? 0 : 1 ) );
	return HkxEditResult::done( msg );
}

HkxEditResult HkxClipDocument::renameTrack( int track, const QString & name, const HkxSkeleton * skeleton )
{
	if ( track < 0 || track >= clip.numTracks )
		return HkxEditResult::refused( QObject::tr( "Track %1 does not exist." ).arg( track ) );
	const QString n = name.trimmed();
	if ( n.isEmpty() )
		return HkxEditResult::refused( QObject::tr( "A track needs a bone name." ) );
	const int dup = findTrack( n );
	if ( dup >= 0 && dup != track )
		return HkxEditResult::refused( QObject::tr( "Track %1 is already '%2'." ).arg( dup ).arg( n ) );
	const QString old = trackNames.value( track );
	trackNames[track] = n;
	if ( skeleton ) {
		for ( int b = 0; b < skeleton->boneNames.count(); b++ ) {
			if ( skeleton->boneNames.at( b ).compare( n, Qt::CaseInsensitive ) == 0 ) {
				if ( clip.trackToBone.count() < clip.numTracks )
					clip.trackToBone.resize( clip.numTracks );
				clip.trackToBone[track] = b;
				clip.trackToBoneIsIdentity = false;
				return HkxEditResult::done( QObject::tr( "Renamed track %1 '%2' -> '%3'; its binding now points at bone %4 of %5." )
					.arg( track ).arg( old, n ).arg( b ).arg( skeleton->name ) );
			}
		}
		return HkxEditResult::done( QObject::tr( "Renamed track %1 '%2' -> '%3'; skeleton %4 has no bone of that name, so the binding index %5 is kept." )
			.arg( track ).arg( old, n ).arg( skeleton->name ).arg( clip.trackToBone.value( track, track ) ) );
	}
	return HkxEditResult::done( QObject::tr( "Renamed track %1 '%2' -> '%3'; no skeleton is loaded, so the binding index %4 is kept." )
		.arg( track ).arg( old, n ).arg( clip.trackToBone.value( track, track ) ) );
}

float HkxClipDocument::rootMotionTravel() const
{
	if ( clip.rootMotion.isEmpty() )
		return 0.0f;
	return hkxLen3( clip.rootMotion.last().translation - clip.rootMotion.first().translation );
}

float HkxClipDocument::trackTravel( int track ) const
{
	if ( track < 0 || track >= clip.numTracks || clip.frames.isEmpty() )
		return 0.0f;
	const Vector3 base = clip.frames.at( 0 ).at( track ).translation;
	float worst = 0.0f;
	for ( const QVector<HkxTransform> & row : clip.frames )
		worst = std::max( worst, hkxLen3( row.at( track ).translation - base ) );
	return worst;
}

HkxEditResult HkxClipDocument::bakeRootMotion( int track )
{
	if ( track < 0 || track >= clip.numTracks )
		return HkxEditResult::refused( QObject::tr( "Track %1 does not exist." ).arg( track ) );
	if ( bakedTrack >= 0 )
		return HkxEditResult::refused( QObject::tr( "Track %1 is already baked; unbake it first." ).arg( bakedTrack ) );
	const int n = clip.numFrames;
	rootMotionBeforeBake = clip.rootMotion;
	if ( clip.rootMotion.count() != n ) {
		clip.rootMotion.clear();
		clip.rootMotion.resize( n );	// all zero, up (0,0,1), forward (0,1,0) as the clip carries
	}
	bakedTranslations.resize( n );
	const Vector3 base = clip.frames.at( 0 ).at( track ).translation;
	for ( int f = 0; f < n; f++ ) {
		const Vector3 t = clip.frames.at( f ).at( track ).translation;
		bakedTranslations[f] = t;
		clip.rootMotion[f].translation = clip.rootMotion.at( f ).translation + ( t - base );
		clip.frames[f][track].translation = base;
	}
	for ( HkxKey & k : keys[track] )
		k.xf.translation = base;
	bakedTrack = track;
	return HkxEditResult::done( QObject::tr( "Baked %1's translation into the extracted motion: the track now holds %2, the motion travels %3 units." )
		.arg( trackNames.value( track ).isEmpty() ? QObject::tr( "track %1" ).arg( track ) : trackNames.value( track ) )
		.arg( QStringLiteral( "(%1, %2, %3)" ).arg( base[0] ).arg( base[1] ).arg( base[2] ) )
		.arg( rootMotionTravel() ) );
}

HkxEditResult HkxClipDocument::unbakeRootMotion()
{
	if ( bakedTrack < 0 )
		return HkxEditResult::refused( QObject::tr( "No track is baked." ) );
	const int track = bakedTrack;
	const int n = clip.numFrames;
	// Has the track stayed exactly where the bake left it? Then the bytes it
	// had before go back verbatim (gate (f): unbake reverses byte-identically).
	bool untouched = ( bakedTranslations.count() == n );
	if ( untouched ) {
		const Vector3 base = clip.frames.at( 0 ).at( track ).translation;
		for ( int f = 0; f < n && untouched; f++ ) {
			const Vector3 & t = clip.frames.at( f ).at( track ).translation;
			for ( int i = 0; i < 3; i++ ) {
				if ( std::memcmp( &t[i], &base[i], sizeof( float ) ) != 0 )
					untouched = false;
			}
		}
	}
	QString how;
	if ( untouched ) {
		for ( int f = 0; f < n; f++ )
			clip.frames[f][track].translation = bakedTranslations.at( f );
		for ( HkxKey & k : keys[track] )
			k.xf.translation = bakedTranslations.at( k.frame );
		clip.rootMotion = rootMotionBeforeBake;
		how = QObject::tr( "restored the bytes the bake removed" );
	} else {
		// the track was edited after the bake: add the motion back arithmetically
		if ( clip.rootMotion.count() == n ) {
			const Vector3 rm0 = clip.rootMotion.at( 0 ).translation;
			for ( int f = 0; f < n; f++ ) {
				const Vector3 d = clip.rootMotion.at( f ).translation - rm0;
				clip.frames[f][track].translation = clip.frames.at( f ).at( track ).translation + d;
			}
			for ( HkxKey & k : keys[track] )
				k.xf.translation = clip.frames.at( k.frame ).at( track ).translation;
			clip.rootMotion = rootMotionBeforeBake;
		}
		how = QObject::tr( "the track was edited after the bake, so the motion was added back arithmetically" );
	}
	bakedTrack = -1;
	bakedTranslations.clear();
	rootMotionBeforeBake.clear();
	return HkxEditResult::done( QObject::tr( "Unbaked track %1: %2." ).arg( track ).arg( how ) );
}


/*
 *  Out
 */

QByteArray HkxClipDocument::toPackfile( QString & error ) const
{
	if ( !floatTracks.isEmpty() ) {
		error = QObject::tr( "The document carries %1 float track(s), which the .hkx writer does not carry yet (CHANGE_NEEDED: hkxwrite floats + binding slots). Remove them to save." )
			.arg( floatTracks.count() );
		return QByteArray();
	}
	HkxWriteOptions opt;
	QByteArray bytes = hkxWritePackfile( clip, opt, error );
#ifdef WW_HKXCLIP_CANON
	/* THE CANONICAL LAYOUT (lane HKXEDIT2, measured 2026-09-10). HKX5's writer
	 * orders the local fixups its own way, and HKXPACK 0.1.6 then prints an
	 * EMPTY text for every annotation (the bytes are there; HKX1's reader and
	 * HKXEDIT1's oracle both read them). Passed through HKXEDIT1's generic
	 * packfile model -- read, then write with the layout every one of the
	 * 15,278 shipped files has -- the same objects come out with the fixups in
	 * file order, and HKXPACK reads the names. 16 bytes differ, all in the
	 * fixup tables. When the class database cannot be found the writer's own
	 * bytes are kept and the message says so. */
	if ( !bytes.isEmpty() ) {
		QString dbErr;
		const Hkx::ClassDb * db = Hkx::ClassDb::instance( &dbErr );
		if ( db ) {
			Hkx::File f;
			if ( f.read( bytes, *db ) ) {
				const QByteArray canon = f.write( *db );
				if ( !canon.isEmpty() )
					bytes = canon;
			}
		}
	}
#endif
	return bytes;
}

bool HkxClipDocument::save( const QString & path, HkxWriteReport & report, QString & error ) const
{
	/* THE PLAY RANGE IS WHAT GOES IN THE FILE (bungo's ruling 9, 2026-09-12).
	   The document keeps every frame so the grips can be dragged back, but the
	   .hkx carries the range alone -- its duration is the number the grips
	   show. Done on a COPY, so saving changes nothing in the open document. */
	if ( rangeIsPartial() ) {
		HkxClipDocument out = *this;
		out.rangeFirst = 0;
		out.rangeLast = -1;
		const HkxEditResult r = out.trim( rangeFirstFrame(), rangeLastFrame() );
		if ( !r.ok ) {
			error = r.message;
			report.error = error;
			return false;
		}
		return out.save( path, report, error );
	}
	const QByteArray bytes = toPackfile( error );
	if ( bytes.isEmpty() ) {
		if ( error.isEmpty() )
			error = QObject::tr( "The writer produced no bytes." );
		report.error = error;
		return false;
	}
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly ) ) {
		error = QObject::tr( "Cannot write %1: %2" ).arg( path, f.errorString() );
		report.error = error;
		return false;
	}
	const qint64 n = f.write( bytes );
	f.close();
	if ( n != bytes.size() ) {
		error = QObject::tr( "%1: wrote %2 of %3 bytes." ).arg( path ).arg( n ).arg( bytes.size() );
		report.error = error;
		return false;
	}
	report.error.clear();
#ifdef WW_HKXCLIP_CANON
	report.routeUsed = QStringLiteral( "direct packfile, canonical layout" );
#else
	report.routeUsed = QStringLiteral( "direct packfile" );
#endif
	report.tracks = clip.numTracks;
	report.frames = clip.numFrames;
	report.transformBytes = qint64( 48 ) * clip.numTracks * clip.numFrames;
	report.fileBytes = bytes.size();
	report.objects = clip.rootMotion.isEmpty() ? 5 : 6;
	return true;
}

QString HkxClipDocument::summary() const
{
	int nkeys = 0;
	for ( const QVector<HkxKey> & ks : keys )
		nkeys += ks.count();
	return QObject::tr( "%1: %2 tracks, %3 frames at %4 fps (%5 s), %6 keys, %7 annotation(s)%8%9." )
		.arg( clip.name.isEmpty() ? QObject::tr( "clip" ) : clip.name )
		.arg( clip.numTracks ).arg( clip.numFrames ).arg( fps() ).arg( clip.duration )
		.arg( nkeys ).arg( annotationCount() )
		.arg( floatTracks.isEmpty() ? QString() : QObject::tr( ", %1 float track(s)" ).arg( floatTracks.count() ) )
		.arg( bakedTrack >= 0 ? QObject::tr( ", root motion baked from track %1" ).arg( bakedTrack ) : QString() )
		+ ( rangeIsPartial() ? QObject::tr( "  Play range %1..%2 (%3 frames); a save writes the range." )
			.arg( rangeFirstFrame() ).arg( rangeLastFrame() ).arg( rangeLastFrame() - rangeFirstFrame() + 1 ) : QString() );
}
