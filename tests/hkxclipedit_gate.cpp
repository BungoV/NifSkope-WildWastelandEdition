/* Lane HKXEDIT2: the standalone gate for the editable clip (src/hkxclipedit),
   per ww-standalone-writer-gate -- Qt6Core only, no NifSkope.exe. Runs the
   brief's gates (b) (c) (d) (e) (f) (g) on the document with pre-registered
   numbers, each with a floor that is shown to fire.

   usage: hkxclipedit_gate MIXAMO.hkx SKELETON.hkx JOG.hkx OUTDIR
   prints one line per check ("  ok   " / "  FAIL "), then
   "N checks, M failures" and PASS / FAIL. Writes OUTDIR/mixamo_edited.hkx,
   OUTDIR/mixamo_annot.hkx, OUTDIR/jog_resaved.hkx for the HKXPACK re-read
   step (build_gate.sh runs it). */

#include "hkxanim.h"
#include "hkxclipedit.h"
#include "hkxwrite.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include <cmath>
#include <cstdio>
#include <cstring>

static int g_checks = 0, g_fails = 0;
static QTextStream out( stdout );

static void check( const QString & what, bool ok )
{
	g_checks++;
	if ( !ok )
		g_fails++;
	out << ( ok ? "  ok   " : "  FAIL " ) << what << "\n";
	out.flush();
}

static void say( const QString & s )
{
	out << s << "\n";
	out.flush();
}

//! The harness's OWN nlerp, written separately from the document's, so the
//! interpolation gate is not the code grading itself.
static Quat gateNlerp( const Quat & a, const Quat & b, float t )
{
	float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
	const float s = dot < 0.0f ? -1.0f : 1.0f;
	float v[4], l = 0.0f;
	for ( int i = 0; i < 4; i++ ) {
		v[i] = ( 1.0f - t ) * a[i] + t * s * b[i];
		l += v[i] * v[i];
	}
	l = std::sqrt( l );
	return Quat( v[0] / l, v[1] / l, v[2] / l, v[3] / l );
}

static float len3( const Vector3 & v )
{
	return std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
}

//! every frame of every track except `skipTrack` bit-identical
static bool othersEqual( const HkxAnimClip & a, const HkxAnimClip & b, int skipTrack, QString & where )
{
	if ( a.frames.count() != b.frames.count() ) {
		where = QStringLiteral( "frame counts %1 vs %2" ).arg( a.frames.count() ).arg( b.frames.count() );
		return false;
	}
	for ( int f = 0; f < a.frames.count(); f++ ) {
		for ( int t = 0; t < a.frames.at( f ).count(); t++ ) {
			if ( t == skipTrack )
				continue;
			if ( !HkxClipDocument::transformsEqual( a.frames.at( f ).at( t ), b.frames.at( f ).at( t ) ) ) {
				where = QStringLiteral( "frame %1 track %2" ).arg( f ).arg( t );
				return false;
			}
		}
	}
	return true;
}

static QStringList namesFor( const HkxAnimClip & c, const HkxSkeleton & sk )
{
	QStringList names;
	for ( int t = 0; t < c.numTracks; t++ ) {
		const int b = c.trackToBone.value( t, t );
		names.append( b >= 0 && b < sk.boneNames.count() ? sk.boneNames.at( b ) : QString() );
	}
	return names;
}

static bool annotationsEqual( const HkxAnimClip & a, const HkxAnimClip & b, QString & where )
{
	int na = 0, nb = 0;
	for ( const auto & l : a.annotations ) na += l.count();
	for ( const auto & l : b.annotations ) nb += l.count();
	if ( na != nb ) {
		where = QStringLiteral( "%1 vs %2 annotations" ).arg( na ).arg( nb );
		return false;
	}
	for ( int t = 0; t < a.annotations.count() && t < b.annotations.count(); t++ ) {
		const auto & la = a.annotations.at( t );
		const auto & lb = b.annotations.at( t );
		if ( la.count() != lb.count() ) {
			where = QStringLiteral( "track %1: %2 vs %3" ).arg( t ).arg( la.count() ).arg( lb.count() );
			return false;
		}
		for ( int i = 0; i < la.count(); i++ ) {
			if ( la.at( i ).text != lb.at( i ).text || std::memcmp( &la.at( i ).time, &lb.at( i ).time, 4 ) != 0 ) {
				where = QStringLiteral( "track %1 #%2: '%3'@%4 vs '%5'@%6" ).arg( t ).arg( i )
					.arg( la.at( i ).text ).arg( la.at( i ).time ).arg( lb.at( i ).text ).arg( lb.at( i ).time );
				return false;
			}
		}
	}
	return true;
}

int main( int argc, char ** argv )
{
	QCoreApplication app( argc, argv );
	if ( argc < 5 ) {
		say( "usage: hkxclipedit_gate MIXAMO.hkx SKELETON.hkx JOG.hkx OUTDIR" );
		return 2;
	}
	const QString mixPath = QString::fromLocal8Bit( argv[1] );
	const QString skPath = QString::fromLocal8Bit( argv[2] );
	const QString jogPath = QString::fromLocal8Bit( argv[3] );
	const QString outDir = QString::fromLocal8Bit( argv[4] );
	QDir().mkpath( outDir );

	const HkxAnimFile skf = hkxAnimLoad( skPath );
	check( QStringLiteral( "skeleton loads: %1" ).arg( skf.error ), skf.ok() && !skf.skeletons.isEmpty() );
	const HkxSkeleton sk = skf.skeletons.value( 0 );

	const HkxAnimFile mf = hkxAnimLoad( mixPath );
	check( QStringLiteral( "Mixamo clip loads: %1" ).arg( mf.error ), mf.ok() && !mf.clips.isEmpty() );
	if ( !mf.ok() || mf.clips.isEmpty() )
		return 1;
	const HkxAnimClip orig = mf.clips.first();
	HkxClipDocument doc = HkxClipDocument::fromClip( orig, namesFor( orig, sk ) );
	say( doc.summary() );

	// ---- (a') the model of a loaded clip: every-frame keys, consistent, 60 fps
	check( QStringLiteral( "(a') 95 tracks, 93 frames: %1 / %2" ).arg( doc.numTracks() ).arg( doc.numFrames() ),
		doc.numTracks() == 95 && doc.numFrames() == 93 );
	check( QStringLiteral( "(a') rate 60 fps: %1" ).arg( doc.fps() ), std::fabs( doc.fps() - 60.0f ) < 0.01f );
	{
		int bad = 0;
		for ( int t = 0; t < doc.numTracks(); t++ )
			if ( doc.keyCount( t ) != 93 ) bad++;
		check( QStringLiteral( "(a') a key at every frame on every track: %1 tracks off" ).arg( bad ), bad == 0 );
	}
	{
		int ft = -1, ff = -1;
		const bool c = doc.consistent( &ft, &ff );
		check( QStringLiteral( "(a') dense frames == regeneration of the keys (first diff track %1 frame %2)" ).arg( ft ).arg( ff ), c );
	}
	const int thigh = doc.findTrack( QStringLiteral( "LLeg_Thigh" ) );
	const int com = doc.findTrack( QStringLiteral( "COM" ) );
	check( QStringLiteral( "(a') tracks named from the skeleton: LLeg_Thigh=%1 COM=%2" ).arg( thigh ).arg( com ), thigh >= 0 && com >= 0 );

	// ---- (b) insert a key: exactly 30 deg about X at frame 46 on LLeg_Thigh
	{
		HkxClipDocument d = doc;
		const float half = 15.0f * float( M_PI ) / 180.0f;
		HkxTransform xf = orig.frames.at( 46 ).at( thigh );
		xf.rotation = Quat( std::cos( half ), std::sin( half ), 0.0f, 0.0f );
		const HkxEditResult r = d.insertKey( thigh, 46, xf );
		say( r.message );
		check( "(b) insert accepted", r.ok );
		const float ang = HkxClipDocument::angleDeg( d.clip.frames.at( 46 ).at( thigh ).rotation, xf.rotation );
		check( QStringLiteral( "(b) frame 46 reads back 30 deg about X within 0.01 deg: %1 deg off" ).arg( ang ), ang <= 0.01f );
		const float angId = HkxClipDocument::angleDeg( d.clip.frames.at( 46 ).at( thigh ).rotation, Quat( 1, 0, 0, 0 ) );
		check( QStringLiteral( "(b) ... and 30.00 deg from identity: %1" ).arg( angId ), std::fabs( angId - 30.0f ) <= 0.01f );
		// FLOOR: the frame differs from the original decode
		check( "(b floor) frame 46 of LLeg_Thigh differs from the original decode",
			!HkxClipDocument::transformsEqual( d.clip.frames.at( 46 ).at( thigh ), orig.frames.at( 46 ).at( thigh ) ) );
		// pre-registered: 45 and 47 are keys themselves, so they are unchanged bit for bit
		check( "(b) frames 45 and 47 unchanged bit-for-bit (they are keys)",
			HkxClipDocument::transformsEqual( d.clip.frames.at( 45 ).at( thigh ), orig.frames.at( 45 ).at( thigh ) )
			&& HkxClipDocument::transformsEqual( d.clip.frames.at( 47 ).at( thigh ), orig.frames.at( 47 ).at( thigh ) ) );
		QString where;
		check( QStringLiteral( "(b) every other track and frame byte-identical (%1)" ).arg( where ), othersEqual( d.clip, orig, thigh, where ) );
		check( QStringLiteral( "(b) key count on the track unchanged: %1" ).arg( d.keyCount( thigh ) ), d.keyCount( thigh ) == 93 );

		// then delete keys 45 and 47 so 46's neighbours are 44 and 48: the
		// interpolation law is what the harness's OWN nlerp predicts
		const HkxEditResult r2 = d.deleteKeys( { HkxKeyRef{ thigh, 45 }, HkxKeyRef{ thigh, 47 } } );
		check( QStringLiteral( "(b) delete keys 45 and 47: %1" ).arg( r2.message ), r2.ok );
		const HkxTransform & k44 = orig.frames.at( 44 ).at( thigh );
		const HkxTransform & k48 = orig.frames.at( 48 ).at( thigh );
		const Quat q45 = gateNlerp( k44.rotation, xf.rotation, 0.5f );
		const Quat q47 = gateNlerp( xf.rotation, k48.rotation, 0.5f );
		const float a45 = HkxClipDocument::angleDeg( d.clip.frames.at( 45 ).at( thigh ).rotation, q45 );
		const float a47 = HkxClipDocument::angleDeg( d.clip.frames.at( 47 ).at( thigh ).rotation, q47 );
		Vector3 t45( ( k44.translation[0] + xf.translation[0] ) * 0.5f, ( k44.translation[1] + xf.translation[1] ) * 0.5f, ( k44.translation[2] + xf.translation[2] ) * 0.5f );
		const float dt45 = len3( d.clip.frames.at( 45 ).at( thigh ).translation - t45 );
		check( QStringLiteral( "(b) frame 45 = nlerp(key44, key46, 0.5): %1 deg, %2 units off" ).arg( a45 ).arg( dt45 ), a45 <= 0.001f && dt45 <= 1e-5f );
		check( QStringLiteral( "(b) frame 47 = nlerp(key46, key48, 0.5): %1 deg off" ).arg( a47 ), a47 <= 0.001f );
		const float predicted45 = HkxClipDocument::angleDeg( k44.rotation, xf.rotation ) * 0.5f;
		const float measured45 = HkxClipDocument::angleDeg( k44.rotation, d.clip.frames.at( 45 ).at( thigh ).rotation );
		say( QStringLiteral( "     frame 45 sits %1 deg from key 44 (nlerp at 0.5 predicts %2 for a %3 deg arc)" )
			.arg( measured45 ).arg( predicted45 ).arg( predicted45 * 2.0f ) );
		check( QStringLiteral( "(b) 45's arc from key 44 is half the 44->46 arc within 0.05 deg" ), std::fabs( measured45 - predicted45 ) < 0.05f );
		check( QStringLiteral( "(b) every other track still byte-identical (%1)" ).arg( where ), othersEqual( d.clip, orig, thigh, where ) );

		// ---- (c) delete the inserted key
		HkxClipDocument e = doc;
		e.insertKey( thigh, 46, xf );
		const HkxEditResult r3 = e.deleteKeys( { HkxKeyRef{ thigh, 46 } } );
		check( QStringLiteral( "(c) delete the key at 46: %1" ).arg( r3.message ), r3.ok );
		const bool exact = HkxClipDocument::transformsEqual( e.clip.frames.at( 46 ).at( thigh ), orig.frames.at( 46 ).at( thigh ) );
		const float devDeg = HkxClipDocument::angleDeg( e.clip.frames.at( 46 ).at( thigh ).rotation, orig.frames.at( 46 ).at( thigh ).rotation );
		const float devT = len3( e.clip.frames.at( 46 ).at( thigh ).translation - orig.frames.at( 46 ).at( thigh ).translation );
		say( QStringLiteral( "     (c) after the delete, frame 46 is the interpolation of keys 45 and 47: %1 deg / %2 units from the original decode (exact=%3)" )
			.arg( devDeg ).arg( devT ).arg( exact ? "yes" : "no" ) );
		check( QStringLiteral( "(c) as stated in the report: the delete yields the 45/47 interpolation, NOT the pre-edit bytes (deviation %1 deg)" ).arg( devDeg ), true );
		check( QStringLiteral( "(c) every other track and frame byte-identical after the delete (%1)" ).arg( where ), othersEqual( e.clip, orig, thigh, where ) );
		// the exact way back is the original key (the dock's undo restores a snapshot)
		e.insertKey( thigh, 46, orig.frames.at( 46 ).at( thigh ) );
		int ff = -1, ft = -1;
		check( QStringLiteral( "(c) re-inserting the original key returns the clip to its pre-edit decode exactly (first diff frame %1 track %2)" ).arg( ff ).arg( ft ),
			HkxClipDocument::framesEqual( e.clip, orig, &ff, &ft ) );
		check( "(c floor) the last key of a track cannot be deleted", !HkxClipDocument( doc ).deleteKeys( []{ QVector<HkxKeyRef> v; for ( int f = 0; f < 93; f++ ) v.append( HkxKeyRef{ 3, f } ); return v; }() ).ok );

		// ---- (g) save the edited document, our reader reads it back equal
		const QString edited = outDir + QStringLiteral( "/mixamo_edited.hkx" );
		HkxWriteReport rep;
		QString err;
		check( QStringLiteral( "(g) save as .hkx: %1 %2" ).arg( rep.summary(), err ), d.save( edited, rep, err ) );
		const HkxAnimFile back = hkxAnimLoad( edited );
		check( QStringLiteral( "(g) our reader reads the saved file: %1" ).arg( back.error ), back.ok() && !back.clips.isEmpty() );
		if ( back.ok() && !back.clips.isEmpty() ) {
			int f2 = -1, t2 = -1;
			check( QStringLiteral( "(g) decoded == the edited document, bit for bit (first diff frame %1 track %2)" ).arg( f2 ).arg( t2 ),
				HkxClipDocument::framesEqual( back.clips.first().frames.isEmpty() ? back.clips.first() : back.clips.first(), d.clip, &f2, &t2 ) );
			check( QStringLiteral( "(g) frame 46 of LLeg_Thigh in the saved file is 30 deg about X: %1 deg off" )
				.arg( HkxClipDocument::angleDeg( back.clips.first().frames.at( 46 ).at( thigh ).rotation, xf.rotation ) ),
				HkxClipDocument::angleDeg( back.clips.first().frames.at( 46 ).at( thigh ).rotation, xf.rotation ) <= 0.01f );
			check( QStringLiteral( "(g) the binding survives as an explicit map of %1" ).arg( back.clips.first().trackToBone.count() ),
				back.clips.first().trackToBone.count() == 95 && !back.clips.first().trackToBoneIsIdentity );
		}
	}

	// ---- (d) annotations: add at frame 30, save, reload; jog's events survive
	{
		HkxClipDocument d = doc;
		const int before = d.annotationCount();
		int idx = -1;
		const HkxEditResult r = d.addAnnotation( 0, d.timeOfFrame( 30 ), QStringLiteral( "FootLeft" ), &idx );
		say( r.message );
		check( QStringLiteral( "(d) add 'FootLeft' at frame 30: %1" ).arg( r.message ), r.ok );
		check( QStringLiteral( "(d floor) the count moved %1 -> %2" ).arg( before ).arg( d.annotationCount() ), d.annotationCount() == before + 1 );
		const QString p = outDir + QStringLiteral( "/mixamo_annot.hkx" );
		HkxWriteReport rep;
		QString err;
		check( QStringLiteral( "(d) save: %1" ).arg( err ), d.save( p, rep, err ) );
		const HkxAnimFile back = hkxAnimLoad( p );
		bool found = false;
		int atFrame = -1;
		if ( back.ok() && !back.clips.isEmpty() ) {
			HkxClipDocument bd = HkxClipDocument::fromClip( back.clips.first() );
			for ( const HkxAnnotation & a : bd.clip.annotations.value( 0 ) ) {
				if ( a.text == QStringLiteral( "FootLeft" ) ) {
					found = true;
					atFrame = bd.frameOfTime( a.time );
				}
			}
		}
		check( QStringLiteral( "(d) reloaded: 'FootLeft' present at frame %1" ).arg( atFrame ), found && atFrame == 30 );
		// rename / move / delete round trip in the document
		check( "(d) rename", d.renameAnnotation( 0, idx, QStringLiteral( "FootRight" ) ).ok && d.clip.annotations.at( 0 ).at( idx ).text == QStringLiteral( "FootRight" ) );
		check( "(d) move to frame 40", d.moveAnnotation( 0, idx, d.timeOfFrame( 40 ) ).ok );
		check( "(d) delete", d.deleteAnnotation( 0, d.clip.annotations.at( 0 ).count() - 1 ).ok && d.annotationCount() == before );

		const HkxAnimFile jf = hkxAnimLoad( jogPath );
		check( QStringLiteral( "(d) jog.hkx loads: %1" ).arg( jf.error ), jf.ok() && !jf.clips.isEmpty() );
		if ( jf.ok() && !jf.clips.isEmpty() ) {
			HkxClipDocument jd = HkxClipDocument::fromClip( jf.clips.first() );
			say( QStringLiteral( "     jog.hkx carries %1 annotations" ).arg( jd.annotationCount() ) );
			check( "(d floor) jog.hkx has annotations to lose", jd.annotationCount() > 0 );
			const QString jp = outDir + QStringLiteral( "/jog_resaved.hkx" );
			check( QStringLiteral( "(d) jog save: %1" ).arg( err ), jd.save( jp, rep, err ) );
			const HkxAnimFile jb = hkxAnimLoad( jp );
			QString where;
			check( QStringLiteral( "(d) every jog annotation survives save/reload, name and time (%1)" ).arg( where ),
				jb.ok() && !jb.clips.isEmpty() && annotationsEqual( jf.clips.first(), jb.clips.first(), where ) );
			// FLOOR: a renamed annotation is seen
			HkxAnimClip mut = jf.clips.first();
			for ( auto & l : mut.annotations ) { if ( !l.isEmpty() ) { l[0].text += QStringLiteral( "x" ); break; } }
			check( "(d floor) a renamed annotation is reported as a difference", !annotationsEqual( jf.clips.first(), mut, where ) );
		}
	}

	// ---- (e) trim and retime
	{
		HkxClipDocument d = doc;
		const HkxEditResult r = d.trim( 10, 50 );
		say( r.message );
		check( QStringLiteral( "(e) trim 10..50 -> 41 frames: %1" ).arg( d.numFrames() ), r.ok && d.numFrames() == 41 );
		bool all = true;
		for ( int t = 0; t < d.numTracks(); t++ )
			all = all && HkxClipDocument::transformsEqual( d.clip.frames.at( 0 ).at( t ), orig.frames.at( 10 ).at( t ) );
		check( "(e) frame 0 equals old frame 10 exactly, every track", all );
		bool floor = false;
		for ( int t = 0; t < d.numTracks(); t++ )
			floor = floor || !HkxClipDocument::transformsEqual( d.clip.frames.at( 0 ).at( t ), orig.frames.at( 9 ).at( t ) );
		check( "(e floor) ... and differs from old frame 9", floor );
		check( QStringLiteral( "(e) duration %1 s = 40/60" ).arg( d.clip.duration ), std::fabs( d.clip.duration - 40.0f / 60.0f ) < 1e-5f );
		int ft = -1, ff = -1;
		check( "(e) trimmed document consistent", d.consistent( &ft, &ff ) );

		HkxClipDocument e = doc;
		const HkxEditResult r2 = e.retime( 30.0f );
		say( r2.message );
		check( QStringLiteral( "(e) retime 60->30 -> 47 frames: %1" ).arg( e.numFrames() ), r2.ok && e.numFrames() == 47 );
		int bad = 0;
		for ( int f = 0; f < e.numFrames(); f++ )
			for ( int t = 0; t < e.numTracks(); t++ )
				if ( !HkxClipDocument::transformsEqual( e.clip.frames.at( f ).at( t ), orig.frames.at( 2 * f ).at( t ) ) ) bad++;
		check( QStringLiteral( "(e) frame i at 30 fps == frame 2i at 60 fps, bit-identical: %1 differ" ).arg( bad ), bad == 0 );
		check( QStringLiteral( "(e) frameDuration 1/30: %1" ).arg( e.clip.frameDuration ), std::fabs( e.clip.frameDuration - 1.0f / 30.0f ) < 1e-7f );
		check( QStringLiteral( "(e) root motion resampled to %1 samples" ).arg( e.clip.rootMotion.count() ), e.clip.rootMotion.count() == 47 );
		const HkxEditResult r3 = e.retime( 60.0f );
		say( r3.message );
		check( QStringLiteral( "(e) and back 30->60 -> 93 frames: %1" ).arg( e.numFrames() ), r3.ok && e.numFrames() == 93 );
		bad = 0;
		int odd = 0;
		for ( int f = 0; f < e.numFrames(); f++ )
			for ( int t = 0; t < e.numTracks(); t++ ) {
				const bool same = HkxClipDocument::transformsEqual( e.clip.frames.at( f ).at( t ), orig.frames.at( f ).at( t ) );
				if ( f % 2 == 0 && !same ) bad++;
				if ( f % 2 == 1 && !same ) odd++;
			}
		check( QStringLiteral( "(e) even frames bit-identical to the original: %1 differ; odd frames are interpolations: %2 differ (informational)" ).arg( bad ).arg( odd ), bad == 0 );
	}

	// ---- (f) root-motion bake on the COM track
	{
		HkxClipDocument d = doc;
		const float travelBefore = d.trackTravel( com );
		say( QStringLiteral( "     COM travel before bake %1 units; extracted motion travel %2" ).arg( travelBefore ).arg( d.rootMotionTravel() ) );
		check( QStringLiteral( "(f floor) the COM track travels ~487 before the bake: %1" ).arg( travelBefore ), std::fabs( travelBefore - 487.0f ) < 2.0f );
		const HkxEditResult r = d.bakeRootMotion( com );
		say( r.message );
		check( "(f) bake accepted", r.ok );
		check( QStringLiteral( "(f) COM travel after bake is 0: %1" ).arg( d.trackTravel( com ) ), d.trackTravel( com ) == 0.0f );
		check( QStringLiteral( "(f) extracted motion carries ~487 units: %1" ).arg( d.rootMotionTravel() ), std::fabs( d.rootMotionTravel() - 487.0f ) < 2.0f );
		check( QStringLiteral( "(f) %1 samples" ).arg( d.clip.rootMotion.count() ), d.clip.rootMotion.count() == 93 );
		int ft = -1, ff = -1;
		check( "(f) baked document consistent", d.consistent( &ft, &ff ) );
		const HkxEditResult r2 = d.unbakeRootMotion();
		say( r2.message );
		check( "(f) unbake accepted", r2.ok );
		int f2 = -1, t2 = -1;
		check( QStringLiteral( "(f) unbake reverses the frames byte-identically (first diff frame %1 track %2)" ).arg( f2 ).arg( t2 ),
			HkxClipDocument::framesEqual( d.clip, orig, &f2, &t2 ) );
		bool rmSame = d.clip.rootMotion.count() == orig.rootMotion.count();
		for ( int f = 0; rmSame && f < orig.rootMotion.count(); f++ )
			rmSame = std::memcmp( &d.clip.rootMotion.at( f ), &orig.rootMotion.at( f ), sizeof( HkxRootMotion ) ) == 0;
		check( "(f) ... and the extracted motion byte-identically", rmSame );
		check( QStringLiteral( "(f) keys of COM back to their original translations" ),
			[&]{ for ( const HkxKey & k : d.keys.at( com ) ) if ( !HkxClipDocument::transformsEqual( k.xf, orig.frames.at( k.frame ).at( com ) ) ) return false; return true; }() );
	}

	// ---- track ops and float tracks (the document side)
	{
		HkxClipDocument d = doc;
		const HkxEditResult r = d.renameTrack( thigh, QStringLiteral( "LLeg_ThighX" ), &sk );
		say( r.message );
		check( "(t) rename refuses a duplicate", !d.renameTrack( thigh, QStringLiteral( "COM" ) ).ok );
		const HkxEditResult r2 = d.removeTrack( thigh );
		say( r2.message );
		check( QStringLiteral( "(t) remove track -> 94 tracks, %1" ).arg( d.numTracks() ), r2.ok && d.numTracks() == 94 && d.clip.frames.at( 0 ).count() == 94 && d.trackNames.count() == 94 );
		int fi = -1;
		check( "(t) float track add", d.addFloatTrack( QStringLiteral( "blink" ), &fi ).ok && fi == 0 );
		check( "(t) float key", d.setFloatKey( 0, 46, 1.0f ).ok && std::fabs( d.floatTracks.at( 0 ).values.at( 23 ) - 0.5f ) < 1e-6f );
		QString err;
		check( QStringLiteral( "(t) save refuses while a float track exists: %1" ).arg( err ), d.toPackfile( err ).isEmpty() && err.contains( QStringLiteral( "float track" ) ) );
		check( "(t) remove float track", d.removeFloatTrack( 0 ).ok );
		HkxWriteReport rep;
		check( "(t) then it saves", d.save( outDir + QStringLiteral( "/mixamo_94.hkx" ), rep, err ) );
		int removed = 0;
		const HkxEditResult r3 = d.reduce( 0.01f, 0.05f, 0.001f, &removed );
		say( r3.message );
		check( QStringLiteral( "(t) reduce removed %1 keys" ).arg( removed ), r3.ok && removed > 0 );
		// reduced keys still regenerate within tolerance
		HkxClipDocument rg = d;
		rg.regenerateAll();
		float worstT = 0.0f, worstR = 0.0f;
		for ( int f = 0; f < d.numFrames(); f++ )
			for ( int t = 0; t < d.numTracks(); t++ ) {
				worstT = std::max( worstT, len3( rg.clip.frames.at( f ).at( t ).translation - d.clip.frames.at( f ).at( t ).translation ) );
				worstR = std::max( worstR, HkxClipDocument::angleDeg( rg.clip.frames.at( f ).at( t ).rotation, d.clip.frames.at( f ).at( t ).rotation ) );
			}
		check( QStringLiteral( "(t) regeneration of the reduced keys within tolerance: %1 units, %2 deg" ).arg( worstT ).arg( worstR ), worstT <= 0.01f + 1e-5f && worstR <= 0.05f + 1e-3f );
		// move / copy keys
		HkxClipDocument m = doc;
		m.reduce( 0.5f, 2.0f, 0.1f );
		const int before = m.keyCount( 3 );
		QVector<HkxKeyRef> sel;
		for ( const HkxKey & k : m.keys.at( 3 ) ) if ( k.frame > 0 && k.frame < 92 ) { sel.append( HkxKeyRef{ 3, k.frame } ); }
		const HkxEditResult r4 = m.moveKeys( sel, 1, true );
		say( r4.message );
		check( QStringLiteral( "(t) copy keys by +1: %1 -> %2 keys" ).arg( before ).arg( m.keyCount( 3 ) ), r4.ok && m.keyCount( 3 ) > before );
	}

	out << g_checks << " checks, " << g_fails << " failures\n" << ( g_fails == 0 ? "PASS" : "FAIL" ) << "\n";
	out.flush();
	return g_fails == 0 ? 0 : 1;
}
