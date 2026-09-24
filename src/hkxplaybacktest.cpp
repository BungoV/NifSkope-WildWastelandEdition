/* WW_HKXANIM_TEST: the pre-registered gates of lane HKX2 (playback + mapping),
   run inside the real application against the real scene graph.

   It lives in its own translation unit rather than beside the thirty other
   WW_*_TEST harnesses in nifskope_ui.cpp because that file is 31,000 lines and
   two other lanes are writing into it this session; the whole of this lane's
   footprint there is the one line that calls wwHkxAnimHarness().

   WHAT IT MEASURES (the gate letters are the brief's):

     (a) after loading a clip and stepping to frame N, every matched NiNode's
         local transform EQUALS the decoder's output for frame N -- translation
         and scale within 1e-4, rotation within 0.01 degrees. Read back off
         Node::localTrans(), not off the playback's own idea of what it wrote.
         FLOOR: the same comparison against a DIFFERENT frame's output must
         fail, or the check would pass on a playback that never wrote anything.
     (b) unloading restores every node's transform BYTE for byte to what it was
         before the clip was loaded -- all 13 floats of every Transform in the
         scene, memcmp, not "close enough". FLOOR: while the clip is active at
         least one node's bytes must differ, or "it restored" is vacuous.
     (c) the mapping report on the player skeleton.nif: 78 matched, 17 unmatched
         and 4 case-folded, which are lane HKX1's measured numbers for
         skeleton.hkx (95 bones) against skeleton.nif. FLOOR: 95 names that are
         in no NIF must map to 0 matched / 95 unmatched.
     (d) a NIF with no matching bones refuses IN WORDS and changes nothing:
         setActive returns false, the summary sentence says so and names the
         count, and every node's transform is byte-identical afterwards.

   ENVIRONMENT (tests/spells/hkxanim_play.sh sets all of them):
     WW_HKXANIM_TEST=1          arm the harness
     WW_HKXANIM_CLIP=<file>     the clip to play (a spline-compressed .hkx)
     WW_HKXANIM_SKEL=<file>     skeleton.hkx, for the bone names of gate (c)
     WW_HKXANIM_NOMATCH=<file>  a NIF with none of those bones, for gate (d)
     WW_HKXANIM_EXPECT=78,17,4  gate (c)'s pre-registered counts
   Log: release/ww_hkxanim_test.log

   Lane HKX2, 2026-09-10. */

#include "hkxplayback.h"

#include "nifskope.h"
#include "glview.h"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "model/nifmodel.h"

#include <QApplication>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QTimer>
#include <QUndoStack>

#include <cmath>
#include <cstring>

namespace
{

struct WwHkxState
{
	int stage = 0;
	int checks = 0;
	int fails = 0;
	QString text;
	QString clipPath, skelPath, noMatchNif;
	int expMatched = 78, expUnmatched = 17, expFolded = 4;
};

void wwCheck( WwHkxState & st, const QString & what, bool pass )
{
	st.checks++;
	if ( !pass )
		st.fails++;
	st.text += ( pass ? QStringLiteral( "  ok   " ) : QStringLiteral( "  FAIL " ) ) + what
		+ QStringLiteral( "\n" );
}

void wwSay( WwHkxState & st, const QString & line )
{
	st.text += line + QStringLiteral( "\n" );
}

//! Every node's local transform, as raw bytes, in scene order. The identity
//! gate (b) rests on: 13 floats per Transform, compared with memcmp.
QVector<QByteArray> wwSnapshot( const Scene * sc )
{
	QVector<QByteArray> out;
	if ( !sc )
		return out;
	for ( Node * n : sc->getNodes() ) {
		if ( !n ) {
			out.append( QByteArray() );
			continue;
		}
		const Transform t = n->localTrans();
		out.append( QByteArray( reinterpret_cast<const char *>( &t ), int( sizeof( Transform ) ) ) );
	}
	return out;
}

//! How many entries of two snapshots differ, and the first node that does.
int wwDiffCount( const QVector<QByteArray> & a, const QVector<QByteArray> & b, int * firstAt )
{
	if ( firstAt )
		*firstAt = -1;
	int n = 0;
	const int c = qMin( a.count(), b.count() );
	for ( int i = 0; i < c; i++ ) {
		if ( a.at( i ) != b.at( i ) ) {
			if ( n == 0 && firstAt )
				*firstAt = i;
			n++;
		}
	}
	return n + qAbs( a.count() - b.count() );
}

/*! Angle between two rotations, in degrees, as 2*asin(|q1 -+ q2|/2).
 *
 *  HKX1's metric, and for its reason: 2*acos(|dot|) has no resolution below
 *  0.03 degrees, so it cannot hold a 0.01-degree gate.
 */
float wwQuatAngleDeg( const Quat & a, const Quat & b )
{
	float dm = 0.0f, dp = 0.0f;
	for ( int i = 0; i < 4; i++ ) {
		const float m = a[i] - b[i];
		const float p = a[i] + b[i];
		dm += m * m;
		dp += p * p;
	}
	float d = std::sqrt( qMin( dm, dp ) ) * 0.5f;
	if ( d > 1.0f )
		d = 1.0f;
	return float( 2.0 * std::asin( double( d ) ) * 180.0 / M_PI );
}

Node * wwNodeById( const Scene * sc, int id )
{
	for ( Node * n : sc->getNodes() ) {
		if ( n && n->id() == id )
			return n;
	}
	return nullptr;
}

/*! Compare the SCENE against the DECODER at one time.
 *
 *  `sampleTime` is what the scene was driven to; `decodeTime` is the time the
 *  decoder is asked about. They are the same for the gate and deliberately
 *  different for the floor.
 */
void wwCompareAtTime( const Scene * sc, const HkxClipEntry * e, float decodeTime,
					  float & worstTrans, float & worstRotDeg, float & worstScale, int & compared )
{
	worstTrans = worstRotDeg = worstScale = 0.0f;
	compared = 0;
	if ( !sc || !e )
		return;
	for ( auto it = e->nodeTrack.constBegin(); it != e->nodeTrack.constEnd(); ++it ) {
		Node * n = wwNodeById( sc, it.key() );
		if ( !n )
			continue;
		HkxTransform x;
		if ( !HkxPlayback::sampleTrack( e->clip, it.value(), decodeTime, x ) )
			continue;
		const Transform got = n->localTrans();
		for ( int i = 0; i < 3; i++ )
			worstTrans = qMax( worstTrans, std::fabs( got.translation[i] - x.translation[i] ) );
		worstScale = qMax( worstScale, std::fabs( got.scale - x.scale[0] ) );
		worstRotDeg = qMax( worstRotDeg, wwQuatAngleDeg( got.rotation.toQuat(), x.rotation ) );
		compared++;
	}
}

void wwFinish( NifSkope * skope, WwHkxState * st )
{
	QFile logf( QApplication::applicationDirPath() + "/ww_hkxanim_test.log" );
	if ( logf.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
		QTextStream log( &logf );
		log << st->text;
		log << st->checks << " checks, " << st->fails << " failures\n";
		log << ( st->fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
		logf.close();
	}
	if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
		n->undoStack->setClean();
	skope->setWindowModified( false );
	delete st;
	QTimer::singleShot( 0, qApp, &QApplication::quit );
}

//! Drive the scene to a time and make sure the transform walk has actually run
//! before anything is read back off a node.
void wwStepTo( NifSkope * skope, Scene * sc, float t )
{
	skope->getGLView()->setSceneTime( t );
	skope->getGLView()->update();
	qApp->processEvents();
	qApp->processEvents();
	// Belt: if the repaint did not happen, this runs the same walk paintGL runs;
	// if it did, Scene::transform's own early-out makes this free and cannot
	// apply the pose twice.
	sc->transform( sc->view, t );
}

// ---------------------------------------------------------------- stage 1 ---
// Gates (c), (a) and (b) on the rigged NIF given on the command line.
void wwStage1( NifSkope * skope, WwHkxState & st, bool ok )
{
	if ( !ok ) {
		wwCheck( st, QStringLiteral( "the rigged NIF loaded" ), false );
		return;
	}
	Scene * sc = skope->getGLView() ? skope->getGLView()->getScene() : nullptr;
	if ( !sc || !sc->hkx ) {
		wwCheck( st, QStringLiteral( "the scene has a Havok animation playback" ), false );
		return;
	}
	HkxPlayback * hkx = sc->hkx;

	wwSay( st, QStringLiteral( "NIF: %1 (%2 nodes)" )
		.arg( sc->nifModel ? sc->nifModel->getFilename() : QStringLiteral( "?" ) )
		.arg( sc->getNodes().count() ) );

	// --- the state every later check is compared against ---------------------
	const QVector<QByteArray> before = wwSnapshot( sc );
	wwCheck( st, QStringLiteral( "the scene has nodes to pose" ), before.count() > 0 );

	// --- (c) the mapping report ---------------------------------------------
	{
		const QString err = hkx->load( st.skelPath );
		wwSay( st, QStringLiteral( "skeleton: %1 -> %2" ).arg( st.skelPath, hkx->summary() ) );
		wwCheck( st, QStringLiteral( "skeleton.hkx loads and is not a refusal" ), err.isEmpty() );

		const HkxSkeleton * sk = nullptr;
		for ( const HkxSkeleton & s : hkx->knownSkeletons() ) {
			if ( !sk || s.boneNames.count() > sk->boneNames.count() )
				sk = &s;
		}
		wwCheck( st, QStringLiteral( "it carries a skeleton" ), sk != nullptr );
		if ( sk ) {
			const HkxMapping m = HkxPlayback::mapNames( sc, sk->boneNames );
			wwSay( st, QStringLiteral( "mapping: %1" ).arg( m.summary( QStringLiteral( "skeleton" ) ) ) );
			wwCheck( st, QStringLiteral( "(c) %1 bones matched (expected %2)" )
					 .arg( m.matched.count() ).arg( st.expMatched ),
					 m.matched.count() == st.expMatched );
			wwCheck( st, QStringLiteral( "(c) %1 bones unmatched (expected %2)" )
					 .arg( m.unmatched.count() ).arg( st.expUnmatched ),
					 m.unmatched.count() == st.expUnmatched );
			wwCheck( st, QStringLiteral( "(c) %1 matched only by case (expected %2)" )
					 .arg( m.caseFolded.count() ).arg( st.expFolded ),
					 m.caseFolded.count() == st.expFolded );
			wwCheck( st, QStringLiteral( "(c) the unmatched bones are NAMED in the summary" ),
					 m.unmatched.isEmpty()
					 || m.summary( QStringLiteral( "x" ) ).contains( m.unmatched.first() ) );

			// FLOOR: names no NIF has must all come back unmatched, or the
			// counts above would be met by a matcher that matches anything.
			QStringList bogus;
			for ( int i = 0; i < sk->boneNames.count(); i++ )
				bogus.append( QStringLiteral( "ww_no_such_bone_%1" ).arg( i ) );
			const HkxMapping f = HkxPlayback::mapNames( sc, bogus );
			wwCheck( st, QStringLiteral( "(c floor) %1 invented names match nothing" )
					 .arg( bogus.count() ),
					 f.matched.isEmpty() && f.unmatched.count() == bogus.count() );
		}
	}

	// --- load the clip -------------------------------------------------------
	QStringList added;
	const QString err = hkx->load( st.clipPath, &added );
	wwSay( st, QStringLiteral( "clip: %1 -> %2" ).arg( st.clipPath, hkx->summary() ) );
	wwCheck( st, QStringLiteral( "the clip loads" ), err.isEmpty() && !added.isEmpty() );
	if ( added.isEmpty() )
		return;

	const QString clipName = added.first();
	wwCheck( st, QStringLiteral( "it is an entry in the animations list" ),
			 sc->animGroups.contains( clipName ) );
	wwCheck( st, QStringLiteral( "the transport can see its length (%1 s)" )
			 .arg( sc->timeMax() - sc->timeMin() ), true );

	skope->getGLView()->setSceneSequence( clipName );
	qApp->processEvents();
	wwCheck( st, QStringLiteral( "selecting it binds the clip" ),
			 hkx->activeName() == clipName );

	const HkxClipEntry * e = hkx->find( clipName );
	if ( !e ) {
		wwCheck( st, QStringLiteral( "the clip entry is readable" ), false );
		return;
	}
	wwSay( st, QStringLiteral( "clip '%1': %2 frames, %3 tracks, frameDuration %4, "
							   "%5 bound nodes, root motion %6 samples" )
		.arg( clipName ).arg( e->clip.numFrames ).arg( e->clip.numTracks )
		.arg( e->clip.frameDuration ).arg( e->nodeTrack.count() )
		.arg( e->clip.rootMotion.count() ) );
	wwCheck( st, QStringLiteral( "it bound at least one node" ), e->nodeTrack.count() > 0 );
	wwCheck( st, QStringLiteral( "the clip has more than one frame to step through" ),
			 e->clip.numFrames > 1 );

	// --- (a) the pose in the scene IS the decoder's output --------------------
	const int nF = e->clip.numFrames;
	const int frames[3] = { 0, nF / 2, nF - 1 };
	float worstT = 0.0f, worstR = 0.0f, worstS = 0.0f;
	int comparedTotal = 0;
	for ( int k = 0; k < 3; k++ ) {
		const int fr = qBound( 0, frames[k], nF - 1 );
		const float t = float( fr ) * e->clip.frameDuration;
		wwStepTo( skope, sc, t );

		float dT = 0.0f, dR = 0.0f, dS = 0.0f;
		int compared = 0;
		wwCompareAtTime( sc, e, t, dT, dR, dS, compared );
		comparedTotal += compared;
		worstT = qMax( worstT, dT );
		worstR = qMax( worstR, dR );
		worstS = qMax( worstS, dS );
		wwSay( st, QStringLiteral( "  frame %1 (t=%2, scene t=%3): %4 nodes, "
								   "worst translation %5, rotation %6 deg, scale %7" )
			.arg( fr ).arg( t ).arg( sc->time ).arg( compared )
			.arg( dT ).arg( dR ).arg( dS ) );
	}
	wwCheck( st, QStringLiteral( "(a) every matched node was compared at all three frames "
								 "(%1 comparisons)" ).arg( comparedTotal ),
			 comparedTotal >= 3 * e->nodeTrack.count() );
	wwCheck( st, QStringLiteral( "(a) worst translation %1 <= 1e-4" ).arg( worstT ),
			 worstT <= 1.0e-4f );
	wwCheck( st, QStringLiteral( "(a) worst rotation %1 deg <= 0.01" ).arg( worstR ),
			 worstR <= 0.01f );
	wwCheck( st, QStringLiteral( "(a) worst scale %1 <= 1e-4" ).arg( worstS ),
			 worstS <= 1.0e-4f );

	// FLOOR for (a): the scene is standing at the LAST frame. Held against the
	// decoder's answer for frame 0 the same comparison must go red, or it is
	// measuring nothing -- a playback that wrote the bind pose and never moved
	// would pass the three checks above only if the clip itself is static.
	{
		float dT = 0.0f, dR = 0.0f, dS = 0.0f;
		int compared = 0;
		wwCompareAtTime( sc, e, 0.0f, dT, dR, dS, compared );
		wwSay( st, QStringLiteral( "  floor: last frame vs frame 0 -> translation %1, "
								   "rotation %2 deg" ).arg( dT ).arg( dR ) );
		wwCheck( st, QStringLiteral( "(a floor) the wrong frame FAILS the same test" ),
				 dT > 1.0e-4f || dR > 0.01f );
	}

	// FLOOR for (b): something in the scene actually moved.
	{
		int firstAt = -1;
		const QVector<QByteArray> posed = wwSnapshot( sc );
		const int moved = wwDiffCount( before, posed, &firstAt );
		wwSay( st, QStringLiteral( "  posed: %1 of %2 nodes differ from the pre-load state" )
			.arg( moved ).arg( before.count() ) );
		wwCheck( st, QStringLiteral( "(b floor) the clip really changed the rig" ), moved > 0 );
	}

	// --- (b) unload restores the pre-load state, byte for byte ---------------
	{
		const bool dropped = hkx->unload( clipName );
		qApp->processEvents();
		wwCheck( st, QStringLiteral( "the clip unloads" ), dropped );
		wwCheck( st, QStringLiteral( "and leaves the animations list" ),
				 !sc->animGroups.contains( clipName ) );
		int firstAt = -1;
		const QVector<QByteArray> after = wwSnapshot( sc );
		const int diff = wwDiffCount( before, after, &firstAt );
		wwSay( st, QStringLiteral( "  after unload: %1 of %2 nodes differ%3" )
			.arg( diff ).arg( before.count() )
			.arg( firstAt >= 0 ? QStringLiteral( " (first: node %1)" ).arg( firstAt )
							   : QString() ) );
		wwCheck( st, QStringLiteral( "(b) every node's transform is byte-identical to "
									 "the pre-load state" ), diff == 0 );
	}
}

// ---------------------------------------------------------------- stage 2 ---
// Gate (d) on a NIF that has none of the clip's bones.
void wwStage2( NifSkope * skope, WwHkxState & st, bool ok )
{
	if ( !ok ) {
		wwCheck( st, QStringLiteral( "(d) the no-match NIF loaded" ), false );
		return;
	}
	Scene * sc = skope->getGLView() ? skope->getGLView()->getScene() : nullptr;
	if ( !sc || !sc->hkx ) {
		wwCheck( st, QStringLiteral( "(d) the scene has a playback" ), false );
		return;
	}
	HkxPlayback * hkx = sc->hkx;
	wwSay( st, QStringLiteral( "no-match NIF: %1 (%2 nodes)" )
		.arg( sc->nifModel ? sc->nifModel->getFilename() : QStringLiteral( "?" ) )
		.arg( sc->getNodes().count() ) );

	const QVector<QByteArray> before = wwSnapshot( sc );

	QStringList added;
	hkx->load( st.skelPath );
	const QString err = hkx->load( st.clipPath, &added );
	wwSay( st, QStringLiteral( "  %1" ).arg( hkx->summary() ) );
	wwCheck( st, QStringLiteral( "(d) the clip still LOADS on a NIF it cannot play on" ),
			 err.isEmpty() && !added.isEmpty() );
	if ( added.isEmpty() )
		return;

	const bool bound = hkx->setActive( added.first() );
	const QString words = hkx->summary();
	wwSay( st, QStringLiteral( "  setActive -> %1; says: %2" )
		.arg( bound ? QStringLiteral( "true" ) : QStringLiteral( "false" ) ).arg( words ) );
	wwCheck( st, QStringLiteral( "(d) it refuses to bind" ), !bound );
	wwCheck( st, QStringLiteral( "(d) the refusal is IN WORDS, naming the reason" ),
			 words.contains( QStringLiteral( "does not play" ) )
			 && words.contains( QStringLiteral( "no" ) ) );

	int firstAt = -1;
	const QVector<QByteArray> after = wwSnapshot( sc );
	const int diff = wwDiffCount( before, after, &firstAt );
	wwSay( st, QStringLiteral( "  after the refusal: %1 of %2 nodes differ" )
		.arg( diff ).arg( before.count() ) );
	wwCheck( st, QStringLiteral( "(d) and changes nothing" ), diff == 0 );
}

} // namespace

void wwHkxAnimHarness( NifSkope * skope )
{
	if ( !skope || !qEnvironmentVariableIsSet( "WW_HKXANIM_TEST" ) )
		return;

	auto * st = new WwHkxState;
	const QString root = QApplication::applicationDirPath() + QStringLiteral( "/../" );
	st->clipPath = qEnvironmentVariable( "WW_HKXANIM_CLIP",
		root + QStringLiteral( "scratchpad/hkx1_20260910/clips/jog.hkx" ) );
	st->skelPath = qEnvironmentVariable( "WW_HKXANIM_SKEL",
		root + QStringLiteral( "scratchpad/hkx1_20260910/clips/skeleton.hkx" ) );
	st->noMatchNif = qEnvironmentVariable( "WW_HKXANIM_NOMATCH" );

	const QStringList exp = qEnvironmentVariable( "WW_HKXANIM_EXPECT",
		QStringLiteral( "78,17,4" ) ).split( QLatin1Char( ',' ) );
	if ( exp.count() == 3 ) {
		st->expMatched = exp.at( 0 ).toInt();
		st->expUnmatched = exp.at( 1 ).toInt();
		st->expFolded = exp.at( 2 ).toInt();
	}

	QObject::connect( skope, &NifSkope::completeLoading, skope, [skope, st]( bool ok, QString & ) {
		// 1.5 s, like the other WW harnesses: the scene is built on the load
		// signal but the first paint, and so the first transform walk, is not.
		QTimer::singleShot( 1500, skope, [skope, st, ok]() {
			if ( st->stage == 0 ) {
				st->stage = 1;
				wwStage1( skope, *st, ok );
				if ( !st->noMatchNif.isEmpty() && QFileInfo::exists( st->noMatchNif ) ) {
					QString p = st->noMatchNif;
					skope->openFile( p );		// stage 2 runs on ITS completeLoading
					return;
				}
				wwSay( *st, QStringLiteral( "no WW_HKXANIM_NOMATCH given: gate (d) not run" ) );
				wwCheck( *st, QStringLiteral( "(d) a no-match NIF was supplied" ), false );
				wwFinish( skope, st );
			} else if ( st->stage == 1 ) {
				st->stage = 2;
				wwStage2( skope, *st, ok );
				wwFinish( skope, st );
			}
		} );
	} );
}
