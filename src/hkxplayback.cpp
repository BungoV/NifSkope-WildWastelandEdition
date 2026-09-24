/* Playback and bone mapping for loaded Havok animation clips.
   The design, the ruling it implements and the reasons live in hkxplayback.h.
   Lane HKX2, 2026-09-10. */

#include "hkxplayback.h"

#include "gamemanager.h"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "model/nifmodel.h"

#include <QDir>
#include <QFileInfo>
#include <QObject>

#include <string>
#include <string_view>

#include <cmath>

/* HOW CLOSE TO A DECODED FRAME COUNTS AS BEING ON IT.
 *
 * Frame times are built by multiplying a frame number by a frame duration that
 * is not representable in binary (1/30 s), so asking for frame 5 of a 30 fps
 * clip and dividing back gives 4.99999952, not 5. Interpolating there would
 * blend 99.99997% of frame 6 into a pose the caller asked for exactly, and gate
 * (a) -- "at frame N the node's local transform EQUALS the decoder's output" --
 * would fail by the whole distance between two frames.
 *
 * 1e-4 of a frame is 3.3 microseconds at 30 fps: below any time the transport
 * or a scrub bar can express, and eleven orders of magnitude above the float
 * error being cancelled.
 */
static const float HKX_FRAME_EPSILON = 1.0e-4f;

//! Where in the clip a time falls: the frame at or before it, and how far past
//! it we are (0 = exactly on a decoded frame).
static void hkxFrameAt( const HkxAnimClip & clip, int numSamples, float time,
						int & i0, int & i1, float & frac )
{
	i0 = 0;
	i1 = 0;
	frac = 0.0f;
	if ( numSamples <= 1 || !( clip.frameDuration > 0.0f ) )
		return;

	float x = time / clip.frameDuration;
	if ( !( x > 0.0f ) )				// also catches NaN
		x = 0.0f;
	if ( x > float( numSamples - 1 ) )
		x = float( numSamples - 1 );

	const float nearest = std::round( x );
	if ( std::fabs( x - nearest ) < HKX_FRAME_EPSILON )
		x = nearest;

	i0 = int( std::floor( x ) );
	if ( i0 >= numSamples - 1 ) {
		i0 = numSamples - 1;
		i1 = i0;
		frac = 0.0f;
		return;
	}
	i1 = i0 + 1;
	frac = x - float( i0 );
}

//! Shortest-arc normalised lerp, the degree-1 spline's own answer between two
//! control points. NOT Quat::normalize(), which divides by the SQUARED
//! magnitude (niftypes.h:868) and so only normalises quaternions that are
//! already unit.
static Quat hkxNlerp( const Quat & a, const Quat & b, float t )
{
	float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
	const float s = ( d < 0.0f ) ? -1.0f : 1.0f;
	Quat r( a[0] + ( s * b[0] - a[0] ) * t,
			a[1] + ( s * b[1] - a[1] ) * t,
			a[2] + ( s * b[2] - a[2] ) * t,
			a[3] + ( s * b[3] - a[3] ) * t );
	const float m = std::sqrt( r[0] * r[0] + r[1] * r[1] + r[2] * r[2] + r[3] * r[3] );
	if ( m > 0.0f ) {
		r[0] /= m; r[1] /= m; r[2] /= m; r[3] /= m;
	}
	return r;
}

static Vector3 hkxLerp( const Vector3 & a, const Vector3 & b, float t )
{
	return Vector3( a[0] + ( b[0] - a[0] ) * t,
					a[1] + ( b[1] - a[1] ) * t,
					a[2] + ( b[2] - a[2] ) * t );
}

bool HkxPlayback::sampleTrack( const HkxAnimClip & clip, int track, float time,
							   HkxTransform & out )
{
	const int n = clip.frames.count();
	if ( n <= 0 || track < 0 )
		return false;
	if ( track >= clip.frames.at( 0 ).count() )
		return false;

	int i0 = 0, i1 = 0;
	float frac = 0.0f;
	hkxFrameAt( clip, n, time, i0, i1, frac );

	const HkxTransform & a = clip.frames.at( i0 ).at( track );
	if ( frac <= 0.0f || i1 == i0 ) {
		// VERBATIM at a decoded frame: the bytes the decoder produced, with no
		// interpolation and no renormalisation in the way. Gate (a) rests on it.
		out = a;
		return true;
	}

	const HkxTransform & b = clip.frames.at( i1 ).at( track );
	out.translation = hkxLerp( a.translation, b.translation, frac );
	out.scale = hkxLerp( a.scale, b.scale, frac );
	out.rotation = hkxNlerp( a.rotation, b.rotation, frac );
	return true;
}

//! The extracted root motion at a time, same rule as a track.
static bool hkxSampleRootMotion( const HkxAnimClip & clip, float time, HkxRootMotion & out )
{
	const int n = clip.rootMotion.count();
	if ( n <= 0 )
		return false;

	int i0 = 0, i1 = 0;
	float frac = 0.0f;
	hkxFrameAt( clip, n, time, i0, i1, frac );

	const HkxRootMotion & a = clip.rootMotion.at( i0 );
	if ( frac <= 0.0f || i1 == i0 ) {
		out = a;
		return true;
	}
	const HkxRootMotion & b = clip.rootMotion.at( i1 );
	out.translation = hkxLerp( a.translation, b.translation, frac );
	out.yaw = a.yaw + ( b.yaw - a.yaw ) * frac;
	return true;
}


/*
 *  The mapping
 */

QString HkxMapping::summaryShort() const
{
	/* A FEW WORDS, and both numbers (lane UI6, 2026-09-11; bungo, "look at all
	 * this text clutter"). The names of the unmatched bones are the whole of
	 * the length, and they are in summary(), which the label's tooltip carries.
	 * Kept under 40 characters by construction: two numbers and one word. */
	const int total = matched.count() + unmatched.count() + unnamed.count();
	if ( total == 0 )
		return QObject::tr( "no bones named" );
	if ( matched.isEmpty() )
		return QObject::tr( "0 of %1 bones, none play" ).arg( total );
	return QObject::tr( "%1 of %2 bones" ).arg( matched.count() ).arg( total );
}

QString HkxMapping::summary( const QString & what ) const
{
	const int total = matched.count() + unmatched.count() + unnamed.count();

	if ( total == 0 )
		return QObject::tr( "%1: nothing to map -- the file names no bones." ).arg( what );

	if ( matched.isEmpty() ) {
		// bungo's "no match = refuse in words"
		return QObject::tr( "%1 does not play: none of its %2 bones names a node in "
							"this NIF (%3 named nodes). Open the rigged NIF this "
							"animation belongs to." )
			.arg( what ).arg( total ).arg( nodesInNif );
	}

	QString s = QObject::tr( "%1: %2 of %3 bones play" )
		.arg( what ).arg( matched.count() ).arg( total );

	if ( !caseFolded.isEmpty() ) {
		s += QObject::tr( ", %1 matched only by ignoring case (%2)" )
			.arg( caseFolded.count() ).arg( caseFolded.join( QStringLiteral( ", " ) ) );
	}

	if ( !unmatched.isEmpty() ) {
		// NAMED, not counted: the 17 Weapon* bones are the difference between
		// "the body NIF, as expected" and "the wrong skeleton".
		s += QObject::tr( "; %1 have no node in this NIF: %2" )
			.arg( unmatched.count() ).arg( unmatched.join( QStringLiteral( ", " ) ) );
	}

	if ( !unnamed.isEmpty() ) {
		s += QObject::tr( "; %1 tracks have no bone name and cannot be matched" )
			.arg( unnamed.count() );
	}

	return s + QStringLiteral( "." );
}

HkxMapping HkxPlayback::mapNames( const Scene * scene, const QStringList & boneNames )
{
	HkxMapping m;
	if ( !scene )
		return m;

	/* Two tables, because "matched by case" and "matched only after folding
	 * case" are different answers and the summary line reports both. Head and
	 * HEAD are the same bone; saying so silently would hide a skeleton mismatch
	 * that happens to fold onto the right names.
	 *
	 * First name wins on a collision, so the mapping is a function of the file
	 * rather than of hash order.
	 */
	QHash<QString, int> exact;
	QHash<QString, int> folded;
	for ( Node * n : scene->getNodes() ) {
		if ( !n )
			continue;
		const QString nm = n->getName();
		if ( nm.isEmpty() )
			continue;
		m.nodesInNif++;
		if ( !exact.contains( nm ) )
			exact.insert( nm, n->id() );
		const QString lo = nm.toLower();
		if ( !folded.contains( lo ) )
			folded.insert( lo, n->id() );
	}

	for ( const QString & bone : boneNames ) {
		if ( bone.isEmpty() ) {
			m.unnamed.append( bone );
			continue;
		}
		if ( exact.contains( bone ) ) {
			m.matched.append( bone );
			continue;
		}
		if ( folded.contains( bone.toLower() ) ) {
			m.matched.append( bone );
			m.caseFolded.append( bone );
			continue;
		}
		m.unmatched.append( bone );
	}
	return m;
}


/*
 *  Loading
 */

int HkxPlayback::indexOf( const QString & clipName ) const
{
	for ( int i = 0; i < clips.count(); i++ ) {
		if ( clips.at( i ).name == clipName )
			return i;
	}
	return -1;
}

const HkxClipEntry * HkxPlayback::find( const QString & clipName ) const
{
	const int i = indexOf( clipName );
	return ( i >= 0 ) ? &clips.at( i ) : nullptr;
}

QStringList HkxPlayback::names() const
{
	QStringList l;
	for ( const HkxClipEntry & e : clips )
		l.append( e.name );
	return l;
}

//! skeleton.hkx beside a file, or in a CharacterAssets folder above it.
//! FO4 puts clips in <actor>/Animations/<group>/<clip>.hkx and the skeleton in
//! <actor>/CharacterAssets/skeleton.hkx, so the walk goes UP and looks sideways.
static QString hkxFindSkeletonOnDisk( const QString & startPath )
{
	QFileInfo fi( startPath );
	QDir d = fi.isDir() ? QDir( startPath ) : fi.absoluteDir();

	for ( int up = 0; up < 8; up++ ) {
		const QStringList candidates = {
			d.absoluteFilePath( QStringLiteral( "skeleton.hkx" ) ),
			d.absoluteFilePath( QStringLiteral( "CharacterAssets/skeleton.hkx" ) )
		};
		for ( const QString & c : candidates ) {
			// case-insensitive on Windows, which is where this runs
			if ( QFileInfo::exists( c ) )
				return c;
		}
		if ( !d.cdUp() )
			break;
	}
	return QString();
}

//! The same walk through the GAME ARCHIVES, for a clip that was pulled out of a
//! BA2 and sits somewhere with no skeleton beside it.
static QByteArray hkxFindSkeletonInArchives( const NifModel * nif, const QString & clipPath,
											 QString & servedBy )
{
	QByteArray blob;
	if ( !nif )
		return blob;
	const Game::GameMode game = Game::GameManager::get_game( nif );
	if ( game == Game::OTHER )
		return blob;

	QStringList rel;
	// The clip's own place in the game tree, when the path still carries one.
	const QString norm = QDir::fromNativeSeparators( clipPath ).toLower();
	const int meshAt = norm.indexOf( QStringLiteral( "meshes/" ) );
	if ( meshAt >= 0 ) {
		QString r = norm.mid( meshAt );
		while ( true ) {
			const int slash = r.lastIndexOf( QLatin1Char( '/' ) );
			if ( slash <= 0 )
				break;
			r = r.left( slash );
			rel.append( r + QStringLiteral( "/characterassets/skeleton.hkx" ) );
			rel.append( r + QStringLiteral( "/skeleton.hkx" ) );
		}
	}
	// ...and the player's, which is where a loose clip usually belongs.
	rel.append( QStringLiteral( "meshes/actors/character/characterassets/skeleton.hkx" ) );

	for ( const QString & r : rel ) {
		const std::string s = r.toStdString();
		if ( Game::GameManager::get_file( blob, game, std::string_view( s ) ) && !blob.isEmpty() ) {
			servedBy = r;
			return blob;
		}
		blob.clear();
	}
	return blob;
}

bool HkxPlayback::namesFromSkeletons( HkxClipEntry & e )
{
	if ( skeletons.isEmpty() )
		return false;

	/* AN EMPTY BINDING IS THE IDENTITY MAP, not a missing one.
	 *
	 * hkaAnimationBinding::transformTrackToBoneIndices is empty in every clip
	 * Bethesda ships (HKX1's census: identity in 13,322 of 13,514, a permutation
	 * in 192) and in third-party clips exported without a binding at all; track
	 * i then drives bone i. Refusing an empty vector here would have refused
	 * almost the whole archive. Accepted only when the skeleton has at least
	 * numTracks bones -- the same rule the fixture lane wrote into the
	 * ww-hkx-animation skill -- and the loop below reads it as
	 * trackToBone.value(t, t), so a short vector falls through to identity too.
	 */
	int need = e.clip.numTracks;
	for ( int b : e.clip.trackToBone )
		need = qMax( need, b + 1 );

	/* Prefer the skeleton the BINDING names; fall back to any skeleton with
	 * enough bones, newest first. Named and unnamed are different arms and the
	 * summary says which one served.
	 */
	const HkxSkeleton * chosen = nullptr;
	for ( int i = skeletons.count() - 1; i >= 0; i-- ) {
		const HkxSkeleton & sk = skeletons.at( i );
		if ( sk.boneNames.count() < need )
			continue;
		if ( !e.clip.originalSkeletonName.isEmpty() && sk.name == e.clip.originalSkeletonName ) {
			chosen = &sk;
			break;
		}
		if ( !chosen )
			chosen = &sk;
	}
	if ( !chosen )
		return false;

	e.trackBone.clear();
	for ( int t = 0; t < e.clip.numTracks; t++ ) {
		const int b = e.clip.trackToBone.value( t, t );
		e.trackBone.append( ( b >= 0 && b < chosen->boneNames.count() )
							? chosen->boneNames.at( b ) : QString() );
	}
	return true;
}

bool HkxPlayback::loadSkeletonBeside( const QString & clipPath )
{
	const QString path = hkxFindSkeletonOnDisk( clipPath );
	if ( path.isEmpty() )
		return false;
	const HkxAnimFile f = hkxAnimLoad( path );
	if ( !f.ok() || f.skeletons.isEmpty() )
		return false;
	for ( const HkxSkeleton & sk : f.skeletons )
		skeletons.append( sk );
	return true;
}

void HkxPlayback::resolveNames( HkxClipEntry & e )
{
	e.trackBone.clear();

	// ARM 1: a skeleton already in hand -- this file's own, or one loaded earlier
	// in the session (the user opened skeleton.hkx first).
	if ( namesFromSkeletons( e ) ) {
		e.skeletonSource = QObject::tr( "bone names from an hkaSkeleton already loaded" );
		return;
	}

	// ARM 2: skeleton.hkx on disk beside the clip, or in a CharacterAssets
	// folder above it.
	if ( loadSkeletonBeside( e.path ) && namesFromSkeletons( e ) ) {
		e.skeletonSource = QObject::tr( "bone names from skeleton.hkx found beside the clip" );
		return;
	}

	// ARM 3: the same walk from the OPEN NIF, which is usually the rig itself.
	if ( scene && scene->nifModel ) {
		const QString nifPath = scene->nifModel->getFolder();
		if ( !nifPath.isEmpty() && loadSkeletonBeside( nifPath ) && namesFromSkeletons( e ) ) {
			e.skeletonSource = QObject::tr( "bone names from skeleton.hkx beside the open NIF" );
			return;
		}
	}

	// ARM 4: the game archives (zero-authoring: the user's own files).
	{
		QString servedBy;
		const QByteArray blob = hkxFindSkeletonInArchives( scene ? scene->nifModel : nullptr,
														   e.path, servedBy );
		if ( !blob.isEmpty() ) {
			const HkxAnimFile f = hkxAnimLoadPackfile( blob, QStringLiteral( "skeleton" ) );
			if ( f.ok() && !f.skeletons.isEmpty() ) {
				for ( const HkxSkeleton & sk : f.skeletons )
					skeletons.append( sk );
				if ( namesFromSkeletons( e ) ) {
					e.skeletonSource = QObject::tr( "bone names from %1 in the game archives" )
						.arg( servedBy );
					return;
				}
			}
		}
	}

	// FLOOR: no names. The clip is kept and refuses in words rather than
	// binding tracks to nodes by position, which would pose the wrong bones.
	for ( int t = 0; t < e.clip.numTracks; t++ )
		e.trackBone.append( QString() );
	e.skeletonSource = QObject::tr( "no skeleton found for '%1' -- load skeleton.hkx to name its bones" )
		.arg( e.clip.originalSkeletonName.isEmpty()
			  ? QObject::tr( "(unnamed skeleton)" ) : e.clip.originalSkeletonName );
}

QString HkxPlayback::load( const QString & path, QStringList * added )
{
	if ( added )
		added->clear();
	lastSummary.clear();
	lastShort.clear();

	if ( path.isEmpty() )
		return QObject::tr( "No file given." );
	if ( !QFileInfo::exists( path ) )
		return QObject::tr( "There is no file at %1." ).arg( path );

	const HkxAnimFile f = hkxAnimLoad( path );
	if ( !f.ok() ) {
		lastSummary = f.error;
		return f.error;
	}

	for ( const HkxSkeleton & sk : f.skeletons )
		skeletons.append( sk );

	if ( f.clips.isEmpty() ) {
		// skeleton.hkx: not a refusal. Its names are what the CLIPS need.
		if ( f.skeletons.isEmpty() ) {
			lastSummary = QObject::tr( "%1 carries no animation and no skeleton." )
				.arg( QFileInfo( path ).fileName() );
			return lastSummary;
		}
		int bones = 0;
		for ( const HkxSkeleton & sk : f.skeletons )
			bones = qMax( bones, sk.boneNames.count() );
		lastSummary = QObject::tr( "%1: a skeleton of %2 bones, no animation in this file. "
								   "Its names will be used for clips that need them." )
			.arg( QFileInfo( path ).fileName() ).arg( bones );
		return QString();
	}

	QString summaryOut;
	for ( const HkxAnimClip & c : f.clips ) {
		HkxClipEntry e;
		e.clip = c;
		e.path = path;
		e.additive = c.blendHint.startsWith( QStringLiteral( "ADDITIVE" ) );

		// A unique list entry. The clip's own name first; the file stem after;
		// then a counter, so two clips of the same name are two rows.
		QString base = c.name.isEmpty() ? QFileInfo( path ).completeBaseName() : c.name;
		if ( base.isEmpty() )
			base = QStringLiteral( "animation" );
		QString n = base;
		for ( int k = 2; indexOf( n ) >= 0 || ( scene && scene->animGroups.contains( n ) ); k++ )
			n = QStringLiteral( "%1 (%2)" ).arg( base ).arg( k );
		e.name = n;

		resolveNames( e );
		e.mapping = mapNames( scene, e.trackBone );

		clips.append( e );
		registerInScene( clips.last() );
		if ( added )
			added->append( e.name );

		if ( !summaryOut.isEmpty() )
			summaryOut += QStringLiteral( "\n" );
		summaryOut += e.mapping.summary( e.name ) + QStringLiteral( " (" )
			+ e.skeletonSource + QStringLiteral( ")" );
	}

	lastSummary = summaryOut;
	if ( !clips.isEmpty() )
		lastShort = clips.last().mapping.summaryShort();
	return QString();
}

bool HkxPlayback::unload( const QString & clipName )
{
	const int i = indexOf( clipName );
	if ( i < 0 )
		return false;

	if ( activeIndex == i )
		setActive( QString() );		// restores every node it posed

	if ( scene ) {
		scene->animGroups.removeAll( clipName );
		scene->animTags.remove( clipName );
		scene->animCycle.remove( clipName );
		scene->transformDirty = true;
	}
	clips.removeAt( i );
	if ( activeIndex > i )
		activeIndex--;
	lastSummary = QObject::tr( "%1 unloaded." ).arg( clipName );
	return true;
}

/* ---- lane UINOTES1: editing the list itself (bungo's ruling 5, 2026-09-12)
   "For these animations, I should be able to use a shortcut to delete, copy,
   paste, etc. them, same goes with reordering with a drag and drop, same goes
   with right clicking and selecting each such option."

   The scene keeps its own animations list (animGroups) and the pickers read
   it, so every one of these keeps that list in the same order as ours -- a
   reorder the eye can see but the picker cannot would be a bug waiting. */

void HkxPlayback::syncSceneOrder()
{
	if ( !scene )
		return;
	// our names, in our order, at the end of the scene's list; anything else
	// (the NIF's own sequences) keeps its place at the front
	QStringList keep;
	for ( const QString & n : scene->animGroups )
		if ( indexOf( n ) < 0 )
			keep << n;
	for ( const HkxClipEntry & e : clips )
		keep << e.name;
	scene->animGroups = keep;
	scene->transformDirty = true;
}

bool HkxPlayback::setOrder( const QStringList & order )
{
	if ( clips.count() < 2 )
		return false;
	const QString act = activeName();
	const QStringList before = names();
	QVector<HkxClipEntry> out;
	out.reserve( clips.count() );
	QVector<bool> taken( clips.count(), false );
	for ( const QString & n : order ) {
		const int i = indexOf( n );
		if ( i >= 0 && !taken.at( i ) ) {
			taken[i] = true;
			out.append( clips.at( i ) );
		}
	}
	for ( int i = 0; i < clips.count(); i++ )
		if ( !taken.at( i ) )
			out.append( clips.at( i ) );
	clips = out;
	activeIndex = act.isEmpty() ? -1 : indexOf( act );
	syncSceneOrder();
	return names() != before;
}

QString HkxPlayback::insertClip( const HkxClipEntry & src, int atIndex, QString * newName )
{
	HkxClipEntry e = src;
	// a copy is not bound to anything and has posed nothing yet
	e.nodeTrack.clear();
	e.saved.clear();
	e.bound = false;
	e.rootNode = -1;
	e.rootNodeName.clear();
	QString base = src.name.isEmpty() ? QFileInfo( src.path ).completeBaseName() : src.name;
	if ( base.isEmpty() )
		base = QStringLiteral( "animation" );
	QString n = base;
	for ( int k = 2; indexOf( n ) >= 0 || ( scene && scene->animGroups.contains( n ) ); k++ )
		n = QStringLiteral( "%1 (%2)" ).arg( base ).arg( k );
	e.name = n;
	e.mapping = mapNames( scene, e.trackBone );
	const int at = std::min( std::max( 0, atIndex ), int( clips.count() ) );
	clips.insert( at, e );
	if ( activeIndex >= at )
		activeIndex++;
	registerInScene( clips.at( at ) );
	syncSceneOrder();
	if ( newName )
		*newName = e.name;
	lastSummary = QObject::tr( "%1 added as a copy of %2." ).arg( e.name, src.name );
	lastShort = e.name;
	return QString();
}

QString HkxPlayback::duplicateClip( const QString & clipName, QString * newName )
{
	const int i = indexOf( clipName );
	if ( i < 0 )
		return QObject::tr( "%1 is not a loaded animation." ).arg( clipName );
	return insertClip( clips.at( i ), i + 1, newName );
}

QString HkxPlayback::renameClip( const QString & clipName, const QString & newName )
{
	const int i = indexOf( clipName );
	if ( i < 0 )
		return QObject::tr( "%1 is not a loaded animation." ).arg( clipName );
	const QString want = newName.trimmed();
	if ( want.isEmpty() )
		return QObject::tr( "An animation needs a name." );
	if ( want == clipName )
		return QString();
	if ( indexOf( want ) >= 0 || ( scene && scene->animGroups.contains( want ) ) )
		return QObject::tr( "%1 is already the name of another animation." ).arg( want );

	const bool wasActive = ( activeIndex == i );
	if ( scene ) {
		scene->animGroups.removeAll( clipName );
		scene->animTags.remove( clipName );
		scene->animCycle.remove( clipName );
	}
	clips[i].name = want;
	registerInScene( clips.at( i ) );
	syncSceneOrder();
	if ( wasActive )
		setActive( want );		// re-binds under the new name, restoring nothing
	lastSummary = QObject::tr( "%1 is now %2." ).arg( clipName, want );
	lastShort = want;
	return QString();
}

void HkxPlayback::unloadAll()
{
	const QStringList all = names();
	for ( const QString & n : all )
		unload( n );
	skeletons.clear();
}


/*
 *  Binding, and the pose
 */

bool HkxPlayback::bind( HkxClipEntry & e )
{
	restore( e );
	e.nodeTrack.clear();
	e.saved.clear();
	e.rootNode = -1;
	e.rootNodeName.clear();
	e.bound = false;

	if ( !scene )
		return false;

	e.mapping = mapNames( scene, e.trackBone );
	if ( !e.mapping.any() )
		return false;			// bungo's "no match = refuse in words"; nothing written

	QHash<QString, Node *> byLower;
	for ( Node * n : scene->getNodes() ) {
		if ( !n )
			continue;
		const QString nm = n->getName();
		if ( nm.isEmpty() )
			continue;
		const QString lo = nm.toLower();
		if ( !byLower.contains( lo ) )
			byLower.insert( lo, n );
	}

	for ( int t = 0; t < e.trackBone.count(); t++ ) {
		const QString & bone = e.trackBone.at( t );
		if ( bone.isEmpty() )
			continue;
		Node * n = byLower.value( bone.toLower(), nullptr );
		if ( !n )
			continue;
		if ( e.nodeTrack.contains( n->id() ) )
			continue;			// one node, one track: the first that named it
		e.nodeTrack.insert( n->id(), t );
		// The pre-pose local, kept BY VALUE. Unbinding writes these back
		// unchanged, which is gate (b).
		e.saved.insert( n->id(), n->local );
	}

	/* THE ROOT MOTION NODE.
	 *
	 * The clip's extracted motion is the character's displacement, not a track,
	 * so it goes on the skeleton root: the node named by the animation
	 * skeleton's own root bone when there is one, and otherwise the first bound
	 * node with no bound ancestor. Named either way, so a picture that moves
	 * the wrong thing says which thing it moved.
	 */
	if ( !e.clip.rootMotion.isEmpty() ) {
		for ( const HkxSkeleton & sk : skeletons ) {
			for ( int b = 0; b < sk.parents.count() && b < sk.boneNames.count(); b++ ) {
				if ( sk.parents.at( b ) >= 0 )
					continue;
				Node * n = byLower.value( sk.boneNames.at( b ).toLower(), nullptr );
				if ( n && e.nodeTrack.contains( n->id() ) ) {
					e.rootNode = n->id();
					e.rootNodeName = n->getName();
					break;
				}
			}
			if ( e.rootNode >= 0 )
				break;
		}
		if ( e.rootNode < 0 ) {
			for ( Node * n : scene->getNodes() ) {
				if ( !n || !e.nodeTrack.contains( n->id() ) )
					continue;
				Node * p = n->parent;
				bool boundAncestor = false;
				while ( p ) {
					if ( e.nodeTrack.contains( p->id() ) ) {
						boundAncestor = true;
						break;
					}
					p = p->parent;
				}
				if ( !boundAncestor ) {
					e.rootNode = n->id();
					e.rootNodeName = n->getName();
					break;
				}
			}
		}
	}

	e.bound = !e.nodeTrack.isEmpty();
	return e.bound;
}

void HkxPlayback::restore( HkxClipEntry & e )
{
	if ( !e.bound || !scene ) {
		e.bound = false;
		return;
	}
	/* BYTE FOR BYTE (gate (b)).
	 *
	 * A Transform is nine rotation floats, three translation floats and a
	 * scale: assigning the saved copy writes back the same bit patterns that
	 * were read, with no arithmetic anywhere in between. Reconstructing the
	 * transform from the NIF instead would go through Transform(nif, index) and
	 * could not promise that.
	 */
	for ( Node * n : scene->getNodes() ) {
		if ( !n )
			continue;
		auto it = e.saved.constFind( n->id() );
		if ( it != e.saved.constEnd() )
			n->local = it.value();
	}
	e.bound = false;
	scene->transformDirty = true;
}

bool HkxPlayback::setActive( const QString & clipName )
{
	const int want = indexOf( clipName );

	if ( activeIndex >= 0 && activeIndex != want && activeIndex < clips.count() )
		restore( clips[activeIndex] );

	active.clear();
	activeIndex = -1;

	if ( want < 0 )
		return false;

	HkxClipEntry & e = clips[want];
	// The binding is recomputed HERE, every time, so a clip selected after a
	// different NIF was opened can never be posing nodes that are gone.
	if ( !bind( e ) ) {
		lastSummary = e.mapping.summary( e.name );
		lastShort = e.mapping.summaryShort();
		return false;
	}

	active = e.name;
	activeIndex = want;
	lastSummary = e.mapping.summary( e.name );
	lastShort = e.mapping.summaryShort();
	if ( e.additive ) {
		lastSummary += QObject::tr( " Additive clip: composed on the pose the NIF was in." );
	}
	if ( rootMotion && e.rootNode >= 0 ) {
		lastSummary += QObject::tr( " Root motion applied to %1." ).arg( e.rootNodeName );
	}
	if ( scene )
		scene->transformDirty = true;
	return true;
}

void HkxPlayback::setRootMotion( bool on )
{
	if ( rootMotion == on )
		return;
	rootMotion = on;
	if ( scene && activeIndex >= 0 && activeIndex < clips.count() ) {
		HkxClipEntry & e = clips[activeIndex];
		// Turning it OFF must put the root back where the clip's own track puts
		// it, so the pose is rebuilt rather than left carrying the last offset.
		if ( !on && e.rootNode >= 0 ) {
			auto it = e.saved.constFind( e.rootNode );
			if ( it != e.saved.constEnd() ) {
				for ( Node * n : scene->getNodes() ) {
					if ( n && n->id() == e.rootNode )
						n->local = it.value();
				}
			}
		}
	}
	if ( scene )
		scene->transformDirty = true;
}

void HkxPlayback::applyLocal( Node * node ) const
{
	if ( activeIndex < 0 || activeIndex >= clips.count() || !node || !scene )
		return;
	const HkxClipEntry & e = clips.at( activeIndex );
	if ( !e.bound )
		return;

	const int id = node->id();
	if ( id == heldNode )
		return;			// lane HKXEDIT2: the gizmo owns this node's pose right now
	const int track = e.nodeTrack.value( id, -1 );
	const bool doRoot = ( rootMotion && id == e.rootNode && !e.clip.rootMotion.isEmpty() );
	if ( track < 0 && !doRoot )
		return;

	const float t = scene->time;
	Transform out = node->local;

	if ( track >= 0 ) {
		HkxTransform x;
		if ( HkxPlayback::sampleTrack( e.clip, track, t, x ) ) {
			Transform p;
			p.rotation.fromQuat( x.rotation );
			p.translation = x.translation;
			/* A NifSkope Transform carries ONE scale, a Havok transform three.
			 * Every FO4 clip measured by lane HKX1 is uniform, so x is taken
			 * and y/z are dropped; a clip that is not uniform loses the
			 * difference rather than the pose. */
			p.scale = x.scale[0];
			if ( e.additive ) {
				// ADDITIVE_* clips are deltas on the pose the rig was in.
				auto it = e.saved.constFind( id );
				out = ( it != e.saved.constEnd() ) ? ( it.value() * p ) : p;
			} else {
				out = p;
			}
		}
	}

	if ( doRoot ) {
		HkxRootMotion rm;
		if ( hkxSampleRootMotion( e.clip, t, rm ) ) {
			Vector3 axis = e.clip.rootMotionUp;
			const float al = std::sqrt( axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2] );
			if ( al > 0.0f ) {
				axis[0] /= al; axis[1] /= al; axis[2] /= al;
			} else {
				axis = Vector3( 0.0f, 0.0f, 1.0f );
			}
			const float hs = std::sin( rm.yaw * 0.5f );
			Transform m;
			m.rotation.fromQuat( Quat( std::cos( rm.yaw * 0.5f ),
									   axis[0] * hs, axis[1] * hs, axis[2] * hs ) );
			m.translation = rm.translation;
			// OUTSIDE the root bone's own transform: the motion moves the whole
			// skeleton, it does not replace what track 0 does inside it.
			out = m * out;
		}
	}

	node->local = out;
}


/*
 *  The scene's own life cycle
 */

void HkxPlayback::registerInScene( const HkxClipEntry & e )
{
	if ( !scene )
		return;
	/* THE WHOLE OF THE UI, in four lines.
	 *
	 * animGroups is the animations list every picker reads; animTags is what
	 * Scene::timeMin/timeMax answer from, so the scrub bar, the transport's
	 * wrap and the timeline ruler get the clip's length without a line of new
	 * transport code; animCycle makes it loop by default like a NiControllerSequence
	 * that says CYCLE_LOOP.
	 */
	if ( !scene->animGroups.contains( e.name ) )
		scene->animGroups.append( e.name );
	QMap<QString, float> tags;
	tags.insert( QStringLiteral( "start" ), 0.0f );
	tags.insert( QStringLiteral( "end" ), e.clip.duration );
	scene->animTags.insert( e.name, tags );
	scene->animCycle.insert( e.name, int( Scene::CycleLoop ) );
	scene->transformDirty = true;
}

QString HkxPlayback::replaceClip( const QString & clipName, const HkxAnimClip & clip, const QStringList & trackNames )
{
	// lane HKXEDIT2: the animation workspace hands back the clip it edited
	const int i = indexOf( clipName );
	if ( i < 0 )
		return QObject::tr( "%1 is not a loaded clip." ).arg( clipName );
	if ( clip.numFrames < 1 || clip.numTracks < 1 || clip.frames.count() != clip.numFrames )
		return QObject::tr( "The edited clip is malformed: %1 tracks, %2 frames, %3 frame rows." )
			.arg( clip.numTracks ).arg( clip.numFrames ).arg( clip.frames.count() );
	HkxClipEntry & e = clips[i];
	const bool wasActive = ( activeIndex == i );
	if ( wasActive )
		restore( e );
	e.clip = clip;
	e.clip.name = clipName;
	e.additive = clip.blendHint.startsWith( QStringLiteral( "ADDITIVE" ) );
	if ( trackNames.count() == clip.numTracks ) {
		e.trackBone = trackNames;
	} else {
		// the track count moved and the workspace gave no names: re-resolve
		resolveNames( e );
	}
	e.mapping = mapNames( scene, e.trackBone );
	registerInScene( e );		// the range follows a trim or a retime
	if ( wasActive ) {
		if ( !bind( e ) ) {
			active.clear();
			activeIndex = -1;
			lastSummary = e.mapping.summary( e.name );
			return QString();
		}
		if ( scene )
			scene->transformDirty = true;
	}
	return QString();
}

void HkxPlayback::onSceneCleared()
{
	// The nodes are about to go. Nothing may be restored onto them and nothing
	// may be written to them, so every binding is dropped, names and decoded
	// frames kept.
	for ( HkxClipEntry & e : clips ) {
		e.bound = false;
		e.nodeTrack.clear();
		e.saved.clear();
		e.rootNode = -1;
	}
	activeIndex = -1;
}

void HkxPlayback::onSceneRebuilt()
{
	if ( !scene )
		return;
	for ( HkxClipEntry & e : clips ) {
		e.mapping = mapNames( scene, e.trackBone );
		registerInScene( e );
	}
	if ( !active.isEmpty() ) {
		const QString want = active;
		setActive( want );
	}
}
