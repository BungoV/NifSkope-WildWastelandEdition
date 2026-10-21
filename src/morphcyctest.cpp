/* WW_MORPHCYC_TEST: lane MORPHCYC1's gates for the Rigging Manager's chargen
   preview clips (src/morphcycle.h), run inside the real application on real
   game meshes, driving the dock's own widgets by object name.

   Four NIFs, one stage each, in this order (tests/spells/morphcyc.sh passes
   the first on the command line and the rest in WW_MORPHCYC_NIFS):

     1 BaseMaleHead_faceBones.nif   the Facebones Cycle - Male
       (x) THE CROSS-CHECK: max vertex displacement against rest, read through
           Shape::skinVertex, at +1 of five channels, against Blender's
           numbers for the same formula (2026-09-29, BoSInfantryArmor.blend):
             Nose - Full: pos Y 0.400   Chin: pos Y 0.593   Chin: pos Z 0.678
             Chin: rot X 0.263          Chin: scale 0.275        each 1e-3.
       (x floor) the same five with the whole morph in the PARENT frame must
           miss at least one by more than 1e-3; the rotation sign flipped is
           measured and reported (whether these five can see the sign at all).
       (r) at t = 0 no vertex has moved (< 1e-4): the rest frame is the rest.
       (l) it is an entry of the animations list, it is the active clip, the
           transport's range is its duration, and pressing the button again
           replaces it rather than adding a second copy.
       (e) every export path's sentence refuses it, and replaceClip refuses.
       (u) unloading puts every node's local back byte for byte.
       + the dock grab (WW_MORPHCYC_SHOT)
     2 BaseFemaleHead_faceBones.nif the Facebones Cycle - Female: counts, (r),
       a chin vertex moves at Chin: pos Y +1, (u).
     3 MaleBody.nif / 4 FemaleBody.nif  the Body Shape Cycle:
       (b) at t = 4 s (FAT) every bound *_skin node's basis is the saved basis
           times diag(bodyBuildScale(bone, fat)), computed HERE from the RACE
           table, 1e-5. FLOOR: the same against the MUSCULAR scale fails.
       (v) a vertex moves more than 0.1 between thin and fat; FLOOR: the same
           state read twice moves < 1e-5.
       (u) as above.

   Log: release/ww_morphcyc_test.log. Lane MORPHCYC1, 2026-09-29. */

#include "morphcycle.h"

#include "bodybuild.h"
#include "gamemanager.h"
#include "glview.h"
#include "hkxanimui.h"
#include "hkxplayback.h"
#include "nifskope.h"
#include "wwskin.h"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "model/nifmodel.h"

#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTextStream>
#include <QTimer>
#include <QUndoStack>

#include <cmath>

namespace
{

struct WwMorphState
{
	int stage = 0;
	int checks = 0;
	int fails = 0;
	QString text;
	QStringList nifs;		//!< stages 2.. (stage 1 is the command-line NIF)
	QString shot;
};

void check( WwMorphState & st, const QString & what, bool pass )
{
	st.checks++;
	if ( !pass )
		st.fails++;
	st.text += ( pass ? QStringLiteral( "  ok   " ) : QStringLiteral( "  FAIL " ) ) + what + QStringLiteral( "\n" );
}

void say( WwMorphState & st, const QString & line )
{
	st.text += QStringLiteral( "SAY " ) + line + QStringLiteral( "\n" );
}

QVector<QByteArray> snapshot( const Scene * sc )
{
	QVector<QByteArray> out;
	for ( Node * n : sc->getNodes() ) {
		Transform t = n ? n->localTrans() : Transform();
		out.append( QByteArray( reinterpret_cast<const char *>( &t ), int( sizeof( Transform ) ) ) );
	}
	return out;
}

int diffCount( const QVector<QByteArray> & a, const QVector<QByteArray> & b )
{
	int n = qAbs( a.count() - b.count() );
	for ( int i = 0; i < qMin( a.count(), b.count() ); i++ )
		n += ( a.at( i ) != b.at( i ) ) ? 1 : 0;
	return n;
}

void stepTo( NifSkope * skope, Scene * sc, float t )
{
	skope->getGLView()->setSceneTime( t );
	skope->getGLView()->update();
	qApp->processEvents();
	qApp->processEvents();
	sc->transform( sc->view, t );
}

//! Every shape's every vertex, through the GPU's own skin transform.
QVector<QVector<Vector3>> skinned( const Scene * sc )
{
	QVector<QVector<Vector3>> out;
	for ( Shape * s : sc->shapes ) {
		QVector<Vector3> v;
		if ( s ) {
			for ( int i = 0; i < s->verts.count(); i++ )
				v << s->skinVertex( i, s->verts[i] );
		}
		out << v;
	}
	return out;
}

double maxDisp( const QVector<QVector<Vector3>> & a, const QVector<QVector<Vector3>> & b )
{
	double m = 0.0;
	for ( int s = 0; s < qMin( a.count(), b.count() ); s++ )
		for ( int i = 0; i < qMin( a[s].count(), b[s].count() ); i++ )
			m = std::max( m, double( ( a[s][i] - b[s][i] ).length() ) );
	return m;
}

int vertexCount( const QVector<QVector<Vector3>> & a )
{
	int n = 0;
	for ( const auto & v : a )
		n += v.count();
	return n;
}

struct Ui
{
	QDockWidget * dock = nullptr;
	QComboBox * gender = nullptr;
	QPushButton * body = nullptr;
	QPushButton * face = nullptr;
	QLabel * status = nullptr;
	QWidget * host = nullptr;
	bool ok() const { return dock && gender && body && face && status && host; }
};

Ui findUi( NifSkope * skope )
{
	Ui u;
	u.dock = skope->findChild<QDockWidget *>( QStringLiteral( "RiggingManagerDock" ) );
	if ( !u.dock )
		return u;
	u.dock->setVisible( true );
	u.dock->raise();
	qApp->processEvents();
	u.gender = u.dock->findChild<QComboBox *>( QStringLiteral( "RiggingMorphCycleGender" ) );
	u.body = u.dock->findChild<QPushButton *>( QStringLiteral( "RiggingMorphCycleBodyButton" ) );
	u.face = u.dock->findChild<QPushButton *>( QStringLiteral( "RiggingMorphCycleFaceButton" ) );
	u.status = u.dock->findChild<QLabel *>( QStringLiteral( "RiggingMorphCycleStatus" ) );
	u.host = u.dock->findChild<QWidget *>( QStringLiteral( "RiggingMorphCycleHost" ) );
	return u;
}

//! Press one of the dock's buttons for one gender, as a user would.
void press( Ui & u, int gender, bool face )
{
	u.gender->setCurrentIndex( gender );
	( face ? u.face : u.body )->click();
	qApp->processEvents();
	qApp->processEvents();
}

int countNamed( const Scene * sc, const QString & base )
{
	int n = 0;
	for ( const QString & g : sc->animGroups )
		n += g.startsWith( base ) ? 1 : 0;
	return n;
}

//! (l) + (e): the clip is in the list, playing, over its own range, and refused for export
const HkxClipEntry * listChecks( WwMorphState & st, Scene * sc, const Ui & u, const QString & name )
{
	const QString shown = u.status->text();
	say( st, QStringLiteral( "status line: %1" ).arg( shown ) );
	say( st, QStringLiteral( "status tooltip: %1" ).arg( u.status->toolTip().replace( '\n', ' ' ) ) );
	check( st, QStringLiteral( "(l) the status line is shown and is not the danger colour" ),
		   u.status->isVisible() && !u.status->styleSheet().contains( wwSkinColor( "danger" ) ) );
	check( st, QStringLiteral( "(l) %1 is an entry of the animations list" ).arg( name ),
		   sc->animGroups.contains( name ) );
	check( st, QStringLiteral( "(l) and it is the active clip" ), sc->hkx && sc->hkx->activeName() == name );
	const HkxClipEntry * e = sc->hkx ? sc->hkx->find( name ) : nullptr;
	check( st, QStringLiteral( "(l) the entry is readable and generated" ), e && e->isGenerated() );
	if ( !e )
		return nullptr;
	const float range = sc->timeMax() - sc->timeMin();
	say( st, QStringLiteral( "clip %1: %2 frames, %3 tracks, %4 s, bound %5 nodes; transport range %6 s" )
		 .arg( name ).arg( e->clip.numFrames ).arg( e->clip.numTracks ).arg( e->clip.duration )
		 .arg( e->nodeTrack.count() ).arg( range ) );
	check( st, QStringLiteral( "(l) the transport's range is the clip's duration (%1 vs %2)" )
		   .arg( range ).arg( e->clip.duration ), std::fabs( range - e->clip.duration ) < 1.0e-3f );
	const QString no = HkxPlayback::generatedRefusal( e );
	check( st, QStringLiteral( "(e) the export refusal names skeleton.hkx" ),
		   no.contains( QStringLiteral( "skeleton.hkx" ) ) && no.contains( QStringLiteral( "skin_bone_" ) ) );
	const QString rep = sc->hkx->replaceClip( name, e->clip, e->trackBone );
	check( st, QStringLiteral( "(e) replaceClip refuses an edit: %1" ).arg( rep ), rep == no );
	// FLOOR for (e): a real clip gets no refusal sentence
	HkxClipEntry real;
	real.name = QStringLiteral( "a real clip" );
	check( st, QStringLiteral( "(e floor) a clip that is not generated is not refused" ),
		   HkxPlayback::generatedRefusal( &real ).isEmpty() );
	return e;
}

//! (u): unload through the hub, as the list's Delete does, and compare bytes
void unloadChecks( WwMorphState & st, NifSkope * skope, Scene * sc, const QString & name,
				   const QVector<QByteArray> & before )
{
	const QVector<QByteArray> posed = snapshot( sc );
	const int moved = diffCount( before, posed );
	check( st, QStringLiteral( "(u floor) while posed, %1 node locals differ from before" ).arg( moved ), moved > 0 );
	WwHkxAnimHub::instance()->unload( skope->getGLView(), name );
	qApp->processEvents();
	const int diff = diffCount( before, snapshot( sc ) );
	check( st, QStringLiteral( "(u) after unload %1 node locals differ from before the button (0 wanted)" ).arg( diff ),
		   diff == 0 && !sc->animGroups.contains( name ) );
}

struct CrossRow { const char * channel; double expected; };
const CrossRow kCross[5] = {
	{ "Nose - Full: pos Y", 0.400 },
	{ "Chin: pos Y", 0.593 },
	{ "Chin: pos Z", 0.678 },
	{ "Chin: rot X", 0.263 },
	{ "Chin: scale", 0.275 },
};

//! The five displacements for the clip that is playing now; worst |diff|.
double crossFive( WwMorphState & st, NifSkope * skope, Scene * sc, const HkxClipEntry * e,
				  const QVector<QVector<Vector3>> & rest, const QString & tag, int * misses )
{
	double worst = 0.0;
	int miss = 0;
	for ( const CrossRow & r : kCross ) {
		const float t = morphCycleChannelTime( *e, QString::fromLatin1( r.channel ), +1 );
		if ( t < 0.0f ) {
			say( st, QStringLiteral( "%1 %2: no such channel" ).arg( tag, QString::fromLatin1( r.channel ) ) );
			miss++;
			worst = 1.0e9;
			continue;
		}
		stepTo( skope, sc, t );
		const double d = maxDisp( rest, skinned( sc ) );
		const double diff = std::fabs( d - r.expected );
		worst = std::max( worst, diff );
		miss += diff > 1.0e-3 ? 1 : 0;
		say( st, QStringLiteral( "%1 %2 +1 at t=%3: max displacement %4 (Blender %5, diff %6)" )
			 .arg( tag, QString::fromLatin1( r.channel ) ).arg( t ).arg( d, 0, 'f', 5 )
			 .arg( r.expected, 0, 'f', 3 ).arg( diff, 0, 'f', 5 ) );
	}
	if ( misses )
		*misses = miss;
	return worst;
}

void grabDock( WwMorphState & st, NifSkope * skope, const Ui & u )
{
	if ( st.shot.isEmpty() )
		return;
	// the dock is wherever this scope's layout put it (a first run docks it
	// narrow): widen it for the picture, then crop the Chargen preview section
	const int oldMin = u.dock->minimumWidth();
	u.dock->setMinimumWidth( 440 );
	skope->resizeDocks( { u.dock }, { 440 }, Qt::Horizontal );
	if ( auto * sa = u.dock->findChild<QScrollArea *>( QStringLiteral( "RiggingToolsScrollArea" ) ) )
		sa->ensureWidgetVisible( u.status, 0, 60 );
	qApp->processEvents();
	qApp->processEvents();
	QWidget * panel = u.host->parentWidget();
	QRect r = u.host->geometry().united( u.status->geometry() );
	r.setTop( std::max( 0, r.top() - 40 ) );
	r.setBottom( std::min( panel->height() - 1, r.bottom() + 8 ) );
	r.setLeft( 0 );
	r.setWidth( panel->width() );
	const bool saved = panel->grab( r ).save( st.shot );
	say( st, QStringLiteral( "dock grab: %1 x %2 px of the Rigging Manager, dock %3 px wide" )
		 .arg( r.width() ).arg( r.height() ).arg( u.dock->width() ) );
	check( st, QStringLiteral( "the dock grab was written to %1" ).arg( st.shot ), saved && r.width() >= 300 );
	u.dock->setMinimumWidth( oldMin );
}

// ------------------------------------------------------------ face stages ---
void faceStage( WwMorphState & st, NifSkope * skope, bool ok, int gender )
{
	const QString who = morphCycleGenderWord( gender );
	check( st, QStringLiteral( "the %1 head loaded" ).arg( who ), ok );
	Scene * sc = skope->getGLView() ? skope->getGLView()->getScene() : nullptr;
	Ui u = findUi( skope );
	check( st, QStringLiteral( "the Rigging Manager carries the Gender selector, both buttons and the status line" ), u.ok() );
	if ( !ok || !sc || !sc->hkx || !u.ok() )
		return;
	say( st, QStringLiteral( "NIF %1: %2 nodes, %3 shapes, %4 vertices" )
		 .arg( sc->nifModel ? sc->nifModel->getFilename() : QString() ).arg( sc->getNodes().count() )
		 .arg( sc->shapes.count() ).arg( vertexCount( skinned( sc ) ) ) );

	// the counts, from the generator the button calls
	HkxClipEntry probe;
	QString made;
	MorphCycleStats ms;
	const bool built = morphCycleFaceBones( gender, probe, made, &ms );
	say( st, made );
	say( st, QStringLiteral( "%1 counts: %2 regions, %3 with channels, %4 channels, %5 region bones, %6 found in "
							 "the skeleton, %7 tracks; missing: %8" )
		 .arg( who ).arg( ms.regions ).arg( ms.liveRegions ).arg( ms.channels ).arg( ms.regionBones )
		 .arg( ms.regionBonesFound ).arg( ms.tracks ).arg( ms.missing.join( QStringLiteral( ", " ) ) ) );
	const int wantCh = gender == 1 ? 104 : 103;		// measured 2026-09-29 from the regions JSON
	check( st, QStringLiteral( "the %1 regions file builds (%2 regions, %3 channels; %4 wanted)" )
		   .arg( who ).arg( ms.regions ).arg( ms.channels ).arg( wantCh ),
		   built && ms.regions == 32 && ms.channels == wantCh );
	check( st, QStringLiteral( "every one of the %1 region bones is a skeleton node" ).arg( ms.regionBones ),
		   built && ms.regionBones == 60 && ms.regionBonesFound == 60 && ms.missing.isEmpty() );
	check( st, QStringLiteral( "%1 frames = 1 + 24 x %2" ).arg( probe.clip.numFrames ).arg( ms.channels ),
		   probe.clip.numFrames == 1 + 24 * ms.channels );

	const QVector<QByteArray> before = snapshot( sc );
	stepTo( skope, sc, 0.0f );
	const QVector<QVector<Vector3>> rest = skinned( sc );

	press( u, gender, true );
	const QString name = QStringLiteral( "Facebones Cycle - %1" ).arg( who );
	const HkxClipEntry * e = listChecks( st, sc, u, name );
	if ( !e )
		return;
	say( st, QStringLiteral( "mapping: %1" ).arg( e->mapping.summary( name ) ) );

	stepTo( skope, sc, 0.0f );
	const double d0 = maxDisp( rest, skinned( sc ) );
	check( st, QStringLiteral( "(r) at t = 0 the head is at rest: worst vertex %1 (< 1e-4)" ).arg( d0 ), d0 < 1.0e-4 );

	if ( gender == 0 ) {
		int misses = 0;
		const double worst = crossFive( st, skope, sc, e, rest, QStringLiteral( "RIGHT" ), &misses );
		check( st, QStringLiteral( "(x) all five Blender cross-checks within 1e-3 (worst %1)" ).arg( worst, 0, 'f', 5 ),
			   misses == 0 && worst <= 1.0e-3 );

		// (l) pressing again replaces, never stacks
		press( u, gender, true );
		check( st, QStringLiteral( "(l) pressed twice: %1 entry named %2 (1 wanted)" )
			   .arg( countNamed( sc, name ) ).arg( name ), countNamed( sc, name ) == 1 );
		stepTo( skope, sc, morphCycleChannelTime( *sc->hkx->find( name ), QStringLiteral( "Chin: pos Z" ), +1 ) );
		grabDock( st, skope, u );

		// (x floor) the two wrong readings, played through the same hub
		for ( int w = 0; w < 2; w++ ) {
			MorphCycleOptions bad;
			bad.parentFrame = ( w == 0 );
			bad.flipRotationSign = ( w == 1 );
			HkxClipEntry wrong;
			QString wsay;
			morphCycleFaceBones( gender, wrong, wsay, nullptr, bad );
			WwHkxAnimHub::instance()->addGenerated( skope->getGLView(), wrong );
			qApp->processEvents();
			const HkxClipEntry * we = sc->hkx->find( name );
			int wm = 0;
			const QString tag = w == 0 ? QStringLiteral( "WRONG(morph in the parent frame)" )
									   : QStringLiteral( "SIGN(rotation sign flipped)" );
			const double ww = we ? crossFive( st, skope, sc, we, rest, tag, &wm ) : 0.0;
			if ( w == 0 )
				check( st, QStringLiteral( "(x floor) %1 misses %2 of 5 by more than 1e-3 (worst %3)" )
					   .arg( tag ).arg( wm ).arg( ww, 0, 'f', 5 ), wm >= 1 );
			else
				say( st, QStringLiteral( "%1 misses %2 of 5 by more than 1e-3 (worst %3): %4" )
					 .arg( tag ).arg( wm ).arg( ww, 0, 'f', 5 )
					 .arg( wm >= 1 ? QStringLiteral( "the cross-check sees the sign" )
								   : QStringLiteral( "the cross-check cannot see the sign; it stays UNPROVEN" ) ) );
		}
		// back to the right one before unloading, so (u) is about the real clip
		press( u, gender, true );
	} else {
		const float t = morphCycleChannelTime( *e, QStringLiteral( "Chin: pos Y" ), +1 );
		stepTo( skope, sc, t );
		const double d = maxDisp( rest, skinned( sc ) );
		say( st, QStringLiteral( "Female Chin: pos Y +1 at t=%1: max displacement %2" ).arg( t ).arg( d, 0, 'f', 5 ) );
		check( st, QStringLiteral( "the female chin moves at Chin: pos Y +1 (%1 > 0.1)" ).arg( d ), t >= 0.0f && d > 0.1 );
	}
	unloadChecks( st, skope, sc, name, before );
}

// ------------------------------------------------------------ body stages ---
QString harnessEsm()
{
	// the game path's own Data, not the generator's Game Folder walk: the two
	// arms share no lookup code
	const QString c = Game::GameManager::path( Game::FALLOUT_4 ) + QStringLiteral( "/Data/Fallout4.esm" );
	return QFile::exists( c ) ? c : QString();
}

void bodyStage( WwMorphState & st, NifSkope * skope, bool ok, int gender )
{
	const QString who = morphCycleGenderWord( gender );
	check( st, QStringLiteral( "the %1 body loaded" ).arg( who ), ok );
	Scene * sc = skope->getGLView() ? skope->getGLView()->getScene() : nullptr;
	Ui u = findUi( skope );
	if ( !ok || !sc || !sc->hkx || !u.ok() ) {
		check( st, QStringLiteral( "the body stage has a scene and the dock" ), false );
		return;
	}
	// the table, read HERE, independently of the generator
	BodyBuildTable table;
	QString err;
	const QString esm = harnessEsm();
	const bool haveTable = !esm.isEmpty() && bodyBuildLoadRace( esm, BODYBUILD_HUMAN_RACE, table, err );
	const BodyBuildSet * set = haveTable ? table.set( gender ) : nullptr;
	check( st, QStringLiteral( "the harness read HumanRace's %1 table itself (%2)" ).arg( who, esm ), set != nullptr );

	const QVector<QByteArray> before = snapshot( sc );
	stepTo( skope, sc, 0.0f );
	press( u, gender, false );
	const QString name = QStringLiteral( "Body Shape Cycle - %1" ).arg( who );
	const HkxClipEntry * e = listChecks( st, sc, u, name );
	if ( !e || !set )
		return;
	say( st, QStringLiteral( "mapping: %1" ).arg( e->mapping.summary( name ) ) );
	check( st, QStringLiteral( "181 frames at 30 fps over 6 s (%1 frames, %2 s)" ).arg( e->clip.numFrames ).arg( e->clip.duration ),
		   e->clip.numFrames == 181 && std::fabs( e->clip.duration - 6.0f ) < 1.0e-4f );

	// (b) the fat corner, against the table read here
	stepTo( skope, sc, 4.0f );
	float wf[3], wm[3];
	bodyBuildCornerWeights( 2, wf[0], wf[1], wf[2] );
	bodyBuildCornerWeights( 1, wm[0], wm[1], wm[2] );
	double worstRight = 0.0, worstWrong = 0.0;
	int compared = 0;
	for ( Node * n : sc->getNodes() ) {
		if ( !n || !e->nodeTrack.contains( n->id() ) )
			continue;
		const BodyBuildBone * b = set->find( n->getName() );
		auto sv = e->saved.constFind( n->id() );
		if ( !b || sv == e->saved.constEnd() )
			continue;
		const Vector3 sF = bodyBuildScale( *b, wf[0], wf[1], wf[2] );
		const Vector3 sM = bodyBuildScale( *b, wm[0], wm[1], wm[2] );
		const Matrix got = n->localTrans().rotation;
		for ( int r = 0; r < 3; r++ ) {
			for ( int c = 0; c < 3; c++ ) {
				const double base = double( sv.value().rotation( r, c ) );
				worstRight = std::max( worstRight, std::fabs( double( got( r, c ) ) - base * sF[c] ) );
				worstWrong = std::max( worstWrong, std::fabs( double( got( r, c ) ) - base * sM[c] ) );
			}
		}
		compared++;
	}
	say( st, QStringLiteral( "fat corner: %1 *_skin nodes compared, worst basis diff %2 (FLOOR vs the muscular "
							 "scale: %3)" ).arg( compared ).arg( worstRight ).arg( worstWrong ) );
	check( st, QStringLiteral( "(b) %1 bound *_skin nodes hold saved x diag(fat) within 1e-5" ).arg( compared ),
		   compared >= 10 && worstRight <= 1.0e-5 );
	check( st, QStringLiteral( "(b floor) the same basis against the MUSCULAR scale is off by %1 (> 1e-3)" ).arg( worstWrong ),
		   worstWrong > 1.0e-3 );

	// (v) a vertex moves thin -> fat, and the same state twice does not
	stepTo( skope, sc, 0.0f );
	const auto thin = skinned( sc );
	stepTo( skope, sc, 0.0f );
	const auto thin2 = skinned( sc );
	stepTo( skope, sc, 4.0f );
	const auto fat = skinned( sc );
	const double moved = maxDisp( thin, fat ), still = maxDisp( thin, thin2 );
	say( st, QStringLiteral( "vertex move thin -> fat: worst %1; the same state twice: %2" ).arg( moved ).arg( still ) );
	check( st, QStringLiteral( "(v) the %1 body changes shape thin -> fat (%2 > 0.1)" ).arg( who ).arg( moved ), moved > 0.1 );
	check( st, QStringLiteral( "(v floor) the same state read twice moves %1 (< 1e-5)" ).arg( still ), still < 1.0e-5 );

	unloadChecks( st, skope, sc, name, before );
}

void finish( NifSkope * skope, WwMorphState * st )
{
	QFile logf( QApplication::applicationDirPath() + QStringLiteral( "/ww_morphcyc_test.log" ) );
	if ( logf.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
		QTextStream log( &logf );
		log << st->text;
		log << st->checks << " checks, " << st->fails << " failures\n";
		log << ( st->fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
		log.flush();
		logf.close();
	}
	if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
		n->undoStack->setClean();
	skope->setWindowModified( false );
	delete st;
	QTimer::singleShot( 100, qApp, &QApplication::quit );
}

} // namespace

void wwMorphCycleHarness( NifSkope * skope )
{
	if ( !skope || !qEnvironmentVariableIsSet( "WW_MORPHCYC_TEST" ) )
		return;
	auto * st = new WwMorphState;
	st->shot = qEnvironmentVariable( "WW_MORPHCYC_SHOT" );
	st->nifs = qEnvironmentVariable( "WW_MORPHCYC_NIFS" ).split( QLatin1Char( ';' ), Qt::SkipEmptyParts );

	QObject::connect( skope, &NifSkope::completeLoading, skope, [skope, st]( bool ok, QString & ) {
		QTimer::singleShot( 1500, skope, [skope, st, ok]() {
			const int s = st->stage++;
			switch ( s ) {
			case 0: faceStage( *st, skope, ok, 0 ); break;
			case 1: faceStage( *st, skope, ok, 1 ); break;
			case 2: bodyStage( *st, skope, ok, 0 ); break;
			case 3: bodyStage( *st, skope, ok, 1 ); break;
			default: break;
			}
			if ( s < 3 && s < st->nifs.count() && QFileInfo::exists( st->nifs.at( s ) ) ) {
				// clean, so opening the next NIF asks nothing
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QString p = st->nifs.at( s );
				skope->openFile( p );		// the next stage runs on ITS completeLoading
				return;
			}
			if ( s < 3 )
				check( *st, QStringLiteral( "stage %1's NIF was supplied in WW_MORPHCYC_NIFS" ).arg( s + 2 ), false );
			finish( skope, st );
		} );
	} );
}
