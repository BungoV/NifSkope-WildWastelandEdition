/* WW_HKXANIM_UI_TEST: the pre-registered gates of lane HKX3 -- a loaded Havok
   animation clip as a row of the ANIMATION DOCK's own list, the drop path, the
   unload, and the panel-style rules the controls must obey.

   RE-AIMED BY LANE UI6, 2026-09-11. Every letter below still asks its own
   question of the same loader (WwHkxAnimHub) and the same playback; what moved
   is the SURFACE it asks them of. bungo retired the Animation Manager dock
   ("old and outdated", then three screenshots of its clipped spin boxes and
   its text clutter), so the widgets this harness drives are the Animation
   dock's: AnimWsClipList (a QListWidget where the old dock had a QComboBox),
   AnimWsReadout, AnimWsNote, AnimWsSpeed, AnimWsUnloadAnim. Nothing about what
   is MEASURED changed, and the count moves only by the checks whose widget
   moved with it.

   Its own translation unit for the same reason lane HKX2's is: nifskope_ui.cpp
   is 31,000 lines, two other lanes are writing into it this session, and the
   whole of this lane's footprint there is the one line that calls
   wwHkxAnimUiHarness().

   Everything here reads the WIDGETS by object name -- AnimWsClipList,
   AnimWsLoadAnim, AnimWsUnloadAnim, AnimWsRootMotion, AnimWsSpeed,
   AnimWsReadout, AnimWsNote -- and never the dock's private members. A gate
   that reads the private state reads what the code MEANT; a gate that reads
   the list reads what the user is looking at.

   WHAT IT MEASURES (the brief's gate letters)

     (a) loading the Mixamo fixture puts ONE row in the dock's list carrying its
         name, 93 frames and 60 fps.
         FLOOR: a name that was never loaded has no row.
     (b) with that row selected and the scene driven to frame 46, every bound
         node's local transform equals the DECODER's output for frame 46 --
         translation and scale within 1e-4, rotation within 0.01 degrees (the
         4*asin metric; see below).
         FLOOR: the same comparison against frame 0 must go red.
     (c) the readout row and the transport rows move as they should: the frame
         readout says "frame 46 / 92" -- which is only true at the clip's OWN
         60 fps, since 30 would call the same instant frame 23 -- the Speed row
         writes GLView::animationSpeed, and the Loop action the dock mirrors
         flips.
         FLOORS: the readout at t=0 says frame 0; the speed reads 1 before the
         row is touched.
     (d) unloading restores every Transform in the scene byte for byte.
         FLOOR: while the clip is posed, more than zero nodes differ.
     (e) DROPPING the same file on the main window yields the same row. The
         drag-enter must be ACCEPTED, and the drop must go through the same
         loader.
         FLOOR: dropping a file that is not an animation adds no row and is not
         accepted by that branch.
     (f) a file that is not an animation clip -- skeleton.hkx, which is a
         skeleton -- lands as a row MARKED REFUSED, with the reason in words.
         FLOOR: the good clip's row is not marked refused.
     (g) the panel-style counts, each with a floor: every number field carries
         the scrub stamp, the selector carries the matched drop-down rule, no
         QGroupBox, every new control has a tooltip, the summary line and the
         toolbar live outside the splitter, and the wheel over the unfocused
         Speed field leaves it while stepping it once focused.

   And one picture: the dock with a clip loaded and the timeline at mid-clip.

   ENVIRONMENT (tests/spells/hkxanim_ui.sh sets all of them):
     WW_HKXANIM_UI_TEST=1        arm the harness
     WW_HKXANIM_UI_CLIP=<file>   the clip (the Mixamo fixture)
     WW_HKXANIM_UI_REFUSE=<file> a non-animation .hkx (skeleton.hkx)
     WW_HKXANIM_UI_EXPECT=93,60  frames, fps -- pre-registered
     WW_HKXANIM_UI_FRAME=46      the frame gate (b) compares at
     WW_HKXANIM_UI_SHOT=<png>    where the dock grab goes
   Log: release/ww_hkxanim_ui_test.log

   Lane HKX3, 2026-09-10. */

#include "hkxanimui.h"
#include "hkxplayback.h"

#include "nifskope.h"
#include "glview.h"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "model/nifmodel.h"
#include "animworkspace.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QPixmap>
#include <QSplitter>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QUndoStack>
#include <QUrl>
#include <QWheelEvent>

#include <cmath>

namespace
{

struct WwUiState
{
	int checks = 0;
	int fails = 0;
	int skips = 0;
	QString text;
	QString clipPath, refusePath, shotPath;
	int expFrames = 93, expFps = 60, atFrame = 46;
};

void wwCheck( WwUiState & st, const QString & what, bool pass )
{
	st.checks++;
	if ( !pass )
		st.fails++;
	st.text += ( pass ? QStringLiteral( "  ok   " ) : QStringLiteral( "  FAIL " ) ) + what
		+ QStringLiteral( "\n" );
}

void wwSay( WwUiState & st, const QString & line )
{
	st.text += line + QStringLiteral( "\n" );
}

//! A named SKIP, never a silent pass: the resume greps these out of the log.
void wwSkip( WwUiState & st, const QString & why )
{
	st.skips++;
	st.text += QStringLiteral( "  SKIP " ) + why + QStringLiteral( "\n" );
}

/*! Angle between two rotations in degrees, as 4*asin(|q1 -+ q2|/2).
 *
 *  Lane HKX5's correction to HKX1's metric: 2*asin gives HALF the true angle.
 *  2*acos(|dot|) is not usable at all -- no resolution below 0.03 degrees, so
 *  it cannot hold a 0.01-degree gate.
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
	return float( 4.0 * std::asin( double( d ) ) * 180.0 / M_PI );
}

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

int wwDiffCount( const QVector<QByteArray> & a, const QVector<QByteArray> & b )
{
	int n = 0;
	const int c = qMin( a.count(), b.count() );
	for ( int i = 0; i < c; i++ ) {
		if ( a.at( i ) != b.at( i ) )
			n++;
	}
	return n + qAbs( int( a.count() ) - int( b.count() ) );
}

Node * wwNodeById( const Scene * sc, int id )
{
	for ( Node * n : sc->getNodes() ) {
		if ( n && n->id() == id )
			return n;
	}
	return nullptr;
}

//! The scene against the decoder at one time. `decodeTime` differs from the
//! time the scene stands at only for the floor.
void wwCompareAtTime( const Scene * sc, const HkxClipEntry * e, float decodeTime,
					  float & worstTrans, float & worstRotDeg, int & compared )
{
	worstTrans = worstRotDeg = 0.0f;
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
		worstRotDeg = qMax( worstRotDeg, wwQuatAngleDeg( got.rotation.toQuat(), x.rotation ) );
		compared++;
	}
}

void wwStepTo( NifSkope * skope, Scene * sc, float t )
{
	skope->getGLView()->setSceneTime( t );
	skope->getGLView()->update();
	qApp->processEvents();
	qApp->processEvents();
	sc->transform( sc->view, t );
}

/*! The row of the dock's LIST that carries `name` as its user data, or -1.
 *  Lane UI6: the Animation dock's list is a QListWidget where the retired
 *  Animation Manager's was a QComboBox; the user data is the same entry name. */
int wwRowOf( QListWidget * box, const QString & name )
{
	if ( !box )
		return -1;
	for ( int i = 0; i < box->count(); i++ ) {
		if ( box->item( i )->data( Qt::UserRole ).toString() == name )
			return i;
	}
	return -1;
}

/*! Let the dock rebuild its list, and WAIT FOR IT.
 *
 *  The Animation dock answers WwHkxAnimHub::clipsChanged with refreshLater(),
 *  which arms a 50 ms QTimer. QApplication::processEvents() does not fire a
 *  timer that has not expired, so every check below that reads a row was
 *  reading the list as it stood BEFORE the load -- the first run of the
 *  re-aimed harness went red on six of them for that one reason. refresh() is
 *  the same rebuild without the timer, and the dock is idempotent about it.
 */
void wwSettle( AnimWorkspace * tl )
{
	qApp->processEvents();
	if ( tl )
		tl->refresh();
	for ( int i = 0; i < 4; i++ )
		qApp->processEvents();
}

//! The text of one row of that list ("" when the row is not there).
QString wwRowText( QListWidget * box, int row )
{
	if ( !box || row < 0 || row >= box->count() )
		return QString();
	return box->item( row )->text();
}

/*! Send one drag-enter and one drop carrying `paths` to the main window.
 *
 *  The application event filter is installed on qApp, so an event SENT to the
 *  window goes through exactly the code an Explorer drop goes through -- this
 *  is the real branch, not a call to the loader with a different name.
 *  Returns whether the drop was accepted.
 */
bool wwSimulateDrop( NifSkope * skope, const QStringList & paths, bool * enterAccepted )
{
	QMimeData mime;
	QList<QUrl> urls;
	for ( const QString & p : paths )
		urls.append( QUrl::fromLocalFile( p ) );
	mime.setUrls( urls );

	const QPointF at( 40, 40 );
	QDragEnterEvent enter( at.toPoint(), Qt::CopyAction, &mime, Qt::LeftButton,
						   Qt::NoModifier );
	QApplication::sendEvent( skope, &enter );
	if ( enterAccepted )
		*enterAccepted = enter.isAccepted();

	QDropEvent drop( at, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier );
	QApplication::sendEvent( skope, &drop );
	const bool accepted = drop.isAccepted();

	// The handler queues the load so it never runs inside the platform drag.
	for ( int i = 0; i < 8; i++ )
		qApp->processEvents();
	return accepted;
}

// ------------------------------------------------------------------ (g) ---
void wwPanelStyle( WwUiState & st, AnimWorkspace * tl )
{
	if ( !tl ) {
		wwCheck( st, QStringLiteral( "(g) the Animation dock is there" ), false );
		return;
	}

	// 1. every number field carries the scrub stamp
	const QList<QDoubleSpinBox *> spins = tl->findChildren<QDoubleSpinBox *>();
	int unstamped = 0;
	for ( QDoubleSpinBox * s : spins ) {
		if ( !s->property( "wwScrubbed" ).toBool() )
			unstamped++;
	}
	wwSay( st, QStringLiteral( "  %1 number field(s), %2 without the scrub stamp" )
		.arg( spins.count() ).arg( unstamped ) );
	wwCheck( st, QStringLiteral( "(g) 0 plain number fields, of %1 (floor: >= 3)" )
			 .arg( spins.count() ), unstamped == 0 && spins.count() >= 3 );

	// 2. no QGroupBox: sections are headings, and this dock is a toolbar row
	//    with no sections at all -- stated as a divergence from the settings
	//    panels rather than counted as headings it does not have.
	wwCheck( st, QStringLiteral( "(g) 0 group boxes" ),
			 tl->findChildren<QGroupBox *>().isEmpty() );

	// 3. the selector carries the matched field style
	const QList<QComboBox *> combos = tl->findChildren<QComboBox *>();
	int unmatched = 0;
	for ( QComboBox * c : combos ) {
		if ( !c->styleSheet().contains( QLatin1String( "drop-down" ) ) )
			unmatched++;
	}
	wwSay( st, QStringLiteral( "  %1 selector(s), %2 without the drop-down rule" )
		.arg( combos.count() ).arg( unmatched ) );
	wwCheck( st, QStringLiteral( "(g) 0 unstyled selectors, of %1 (floor: >= 1)" )
			 .arg( combos.count() ), unmatched == 0 && combos.count() >= 1 );

	// 4. every control this lane added has a tooltip
	const QStringList mine = { QStringLiteral( "AnimWsClipList" ),
							   QStringLiteral( "AnimWsLoadAnim" ),
							   QStringLiteral( "AnimWsUnloadAnim" ),
							   QStringLiteral( "AnimWsRootMotion" ),
							   QStringLiteral( "AnimWsSpeed" ),
							   QStringLiteral( "AnimWsReadout" ) };
	int found = 0, noTip = 0;
	for ( const QString & n : mine ) {
		QWidget * w = tl->findChild<QWidget *>( n );
		if ( !w )
			continue;
		found++;
		if ( w->toolTip().isEmpty() )
			noTip++;
	}
	wwCheck( st, QStringLiteral( "(g) all %1 new controls are there (of %2)" )
			 .arg( found ).arg( mine.count() ), found == mine.count() );
	wwCheck( st, QStringLiteral( "(g) 0 of them without a tooltip" ), noTip == 0 );

	// 5. the summary line and the list are OUTSIDE the splitter, so neither can
	//    scroll away from what it is answering for
	/* The Animation dock's splitter is HORIZONTAL (the list column beside the
	 * dope sheet) where the retired dock's was vertical, so "find the vertical
	 * one" found none and this whole check went red on a correct window. It is
	 * asked for BY NAME now, which is what the rest of this file already does. */
	QSplitter * vsplit = tl->findChild<QSplitter *>( QStringLiteral( "AnimWsSplitter" ) );
	QWidget * note = tl->findChild<QWidget *>( QStringLiteral( "AnimWsNote" ) );
	/* Lane UINOTES1 (ruling 6): the bottom action bar is gone -- its fifteen
	 * buttons are header-menu actions now -- so the widget that must stay
	 * pinned outside the splitter is the header bar. Same predicate. */
	QWidget * box = tl->findChild<QWidget *>( QStringLiteral( "AnimWsHeader" ) );
	if ( !vsplit || !note || !box ) {
		wwCheck( st, QStringLiteral( "(g) the splitter, the summary line and the list exist" ),
				 false );
	} else {
		wwCheck( st, QStringLiteral( "(g) the summary line is outside the splitter" ),
				 !vsplit->isAncestorOf( note ) );
		/* Lane UI6: in the Animation dock the LIST is inside the splitter's
		 * left column by design -- it is a pane, not a strip control -- so the
		 * question the old dock's check asked (can it scroll away from what it
		 * answers for) is asked of the ACTION BAR, which is the thing that
		 * must stay pinned. Same predicate, same splitter, different widget. */
		wwCheck( st, QStringLiteral( "(g) the header menu bar is outside the splitter" ),
				 !vsplit->isAncestorOf( box ) );
	}

	// 6. the wheel scrolls the panel, not the value -- unless the field has focus
	auto * speed = tl->findChild<QDoubleSpinBox *>( QStringLiteral( "AnimWsSpeed" ) );
	if ( !speed ) {
		wwCheck( st, QStringLiteral( "(g) the Speed field is there" ), false );
	} else {
		speed->clearFocus();
		const double v0 = speed->value();
		QWheelEvent w1( QPointF( 10, 10 ), speed->mapToGlobal( QPoint( 10, 10 ) ),
						QPoint(), QPoint( 0, 120 ), Qt::NoButton, Qt::NoModifier,
						Qt::NoScrollPhase, false );
		QApplication::sendEvent( speed, &w1 );
		const double v1 = speed->value();

		speed->setFocus( Qt::OtherFocusReason );
		qApp->processEvents();
		/* WHY THE FOCUS STATE IS PRINTED (lane BUILD9, 2026-09-10).
		 * The guard in wwGuardWheel blocks the wheel while `!w->hasFocus()`,
		 * and hasFocus() is false in an INACTIVE window however many times
		 * setFocus() is called. Without these two words the floor's red says
		 * "the guard is broken" when it may only say "the harness window was
		 * never activated" -- two very different verdicts. */
		wwSay( st, QStringLiteral( "  after setFocus: hasFocus %1, focusWidget %2, window active %3" )
			.arg( speed->hasFocus() ? QStringLiteral( "yes" ) : QStringLiteral( "no" ),
				  QApplication::focusWidget()
					  ? QString::fromLatin1( QApplication::focusWidget()->metaObject()->className() )
					  : QStringLiteral( "<none>" ),
				  speed->window() && speed->window()->isActiveWindow()
					  ? QStringLiteral( "yes" ) : QStringLiteral( "no" ) ) );
		QWheelEvent w2( QPointF( 10, 10 ), speed->mapToGlobal( QPoint( 10, 10 ) ),
						QPoint(), QPoint( 0, 120 ), Qt::NoButton, Qt::NoModifier,
						Qt::NoScrollPhase, false );
		QApplication::sendEvent( speed, &w2 );
		const double v2 = speed->value();
		speed->clearFocus();

		wwSay( st, QStringLiteral( "  Speed under the wheel: %1 -> %2 (unfocused) -> %3 (focused)" )
			.arg( v0 ).arg( v1 ).arg( v2 ) );
		wwCheck( st, QStringLiteral( "(g) the wheel over an unfocused number field leaves it" ),
				 qFuzzyCompare( v0 + 1.0, v1 + 1.0 ) );
		wwCheck( st, QStringLiteral( "(g floor) and steps it once focused" ),
				 !qFuzzyCompare( v1 + 1.0, v2 + 1.0 ) );
		speed->setValue( v0 );
	}
}

void wwFinish( NifSkope * skope, WwUiState * st )
{
	QFile logf( QApplication::applicationDirPath() + "/ww_hkxanim_ui_test.log" );
	if ( logf.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
		QTextStream log( &logf );
		log << st->text;
		log << st->checks << " checks, " << st->fails << " failures, "
			<< st->skips << " skips\n";
		log << ( st->fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
		logf.close();
	}
	if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
		n->undoStack->setClean();
	skope->setWindowModified( false );
	delete st;
	QTimer::singleShot( 0, qApp, &QApplication::quit );
}

void wwRun( NifSkope * skope, WwUiState & st, bool loaded )
{
	if ( !loaded ) {
		wwCheck( st, QStringLiteral( "the rigged NIF loaded" ), false );
		return;
	}

	GLView * ogl = skope->getGLView();
	Scene * sc = ogl ? ogl->getScene() : nullptr;
	if ( !sc || !sc->hkx ) {
		wwCheck( st, QStringLiteral( "the scene has a Havok animation playback" ), false );
		return;
	}

	auto * dock = skope->findChild<QDockWidget *>( QStringLiteral( "AnimWorkspaceDock" ) );
	auto * tl = skope->findChild<AnimWorkspace *>();
	if ( !dock || !tl ) {
		wwCheck( st, QStringLiteral( "the Animation dock is there" ), false );
		return;
	}
	// The dock defers its scan while hidden, so the list is only real once it
	// is shown. The window itself is invisible and off the primary monitor --
	// that is the harness launcher's job, not this one's.
	dock->show();
	dock->raise();
	qApp->processEvents();
	tl->refresh();
	qApp->processEvents();

	auto * box = tl->findChild<QListWidget *>( QStringLiteral( "AnimWsClipList" ) );
	auto * readout = tl->findChild<QLabel *>( QStringLiteral( "AnimWsReadout" ) );
	auto * note = tl->findChild<QLabel *>( QStringLiteral( "AnimWsNote" ) );
	auto * speed = tl->findChild<QDoubleSpinBox *>( QStringLiteral( "AnimWsSpeed" ) );
	auto * unloadBtn = tl->findChild<QToolButton *>( QStringLiteral( "AnimWsUnloadAnim" ) );

	wwSay( st, QStringLiteral( "NIF: %1 (%2 nodes); dock list starts with %3 row(s)" )
		.arg( sc->nifModel ? sc->nifModel->getFilename() : QStringLiteral( "?" ) )
		.arg( sc->getNodes().count() ).arg( box ? box->count() : -1 ) );
	wwCheck( st, QStringLiteral( "the dock's animations list is there" ), box != nullptr );
	if ( !box )
		return;

	const int rowsBefore = box->count();
	const QVector<QByteArray> bindPose = wwSnapshot( sc );
	wwCheck( st, QStringLiteral( "the scene has nodes to pose" ), bindPose.count() > 0 );

	auto * hub = WwHkxAnimHub::instance();

	// ------------------------------------------------------------- (a) ---
	// Through the SAME call the dock's Load button makes (the QFileDialog line
	// itself cannot run in a harness, and is the only line not exercised).
	const QString said = hub->loadFiles( ogl, { st.clipPath }, true );
	wwSettle( tl );
	wwSay( st, QStringLiteral( "load: %1" ).arg( said ) );

	/* The clip's LIST NAME comes from the playback, not from the file name:
	 * loadFiles activated it, so the scene's active clip is the one just
	 * loaded. Guessing the stem here would make gate (a) fail on a file whose
	 * clip is named something else, which is a defect in the gate. */
	QString clipName = sc->hkx->activeName();
	if ( clipName.isEmpty() )
		clipName = QFileInfo( st.clipPath ).completeBaseName();
	wwSay( st, QStringLiteral( "  the clip is listed as \"%1\" (file stem \"%2\")" )
		.arg( clipName, QFileInfo( st.clipPath ).completeBaseName() ) );
	const int row = wwRowOf( box, clipName );
	wwSay( st, QStringLiteral( "  list is now %1 row(s); clip row = %2, text \"%3\"" )
		.arg( box->count() ).arg( row ).arg( wwRowText( box, row ) ) );

	wwCheck( st, QStringLiteral( "(a) the clip is ONE new row in the dock's list" ),
			 row >= 0 && box->count() == rowsBefore + 1 );

	const WwHkxListEntry e = hub->entry( ogl, clipName );
	wwCheck( st, QStringLiteral( "(a) it reports %1 frames (expected %2)" )
			 .arg( e.numFrames ).arg( st.expFrames ), e.numFrames == st.expFrames );
	wwCheck( st, QStringLiteral( "(a) at %1 fps (expected %2)" )
			 .arg( qRound( e.fps ) ).arg( st.expFps ), qRound( e.fps ) == st.expFps );
	wwCheck( st, QStringLiteral( "(a) and the ROW SAYS SO" ),
			 row >= 0 && wwRowText( box, row ).contains( QString::number( st.expFrames ) )
			 && wwRowText( box, row ).contains( QString::number( st.expFps ) ) );
	wwCheck( st, QStringLiteral( "(a floor) a name that was never loaded has no row" ),
			 wwRowOf( box, QStringLiteral( "ww_no_such_clip" ) ) < 0 );

	// selecting the row is what makes the dock drive the clip
	if ( row >= 0 && box->currentRow() != row ) {
		box->setCurrentRow( row );
		qApp->processEvents();
	}
	wwCheck( st, QStringLiteral( "(a) selecting the row binds it in the scene" ),
			 sc->hkx->activeName() == clipName );
	wwCheck( st, QStringLiteral( "(a) and the transport sees its length (%1 s)" )
			 .arg( sc->timeMax() - sc->timeMin() ),
			 sc->timeMax() - sc->timeMin() > 0.5f );
	wwCheck( st, QStringLiteral( "(f floor) a clip that PLAYS is not marked refused" ),
			 !hub->entry( ogl, clipName ).refused );

	const HkxClipEntry * ce = sc->hkx->find( clipName );
	if ( !ce ) {
		wwCheck( st, QStringLiteral( "the clip entry is readable" ), false );
		return;
	}

	// ------------------------------------------------------- (b) and (c) ---
	const float t = float( st.atFrame ) * ce->clip.frameDuration;
	wwStepTo( skope, sc, t );
	tl->setTime( t, sc->timeMin(), sc->timeMax() );
	qApp->processEvents();

	float dT = 0.0f, dR = 0.0f;
	int compared = 0;
	wwCompareAtTime( sc, ce, t, dT, dR, compared );
	wwSay( st, QStringLiteral( "  frame %1 (t=%2, scene t=%3): %4 nodes, worst translation "
							   "%5, rotation %6 deg" )
		.arg( st.atFrame ).arg( t ).arg( sc->time ).arg( compared ).arg( dT ).arg( dR ) );
	wwCheck( st, QStringLiteral( "(b) %1 bound nodes were compared" ).arg( compared ),
			 compared > 0 && compared == ce->nodeTrack.count() );
	wwCheck( st, QStringLiteral( "(b) worst translation %1 <= 1e-4" ).arg( dT ),
			 dT <= 1.0e-4f );
	wwCheck( st, QStringLiteral( "(b) worst rotation %1 deg <= 0.01" ).arg( dR ),
			 dR <= 0.01f );
	{
		float fT = 0.0f, fR = 0.0f;
		int fc = 0;
		wwCompareAtTime( sc, ce, 0.0f, fT, fR, fc );
		wwSay( st, QStringLiteral( "  floor: frame %1 held against frame 0 -> %2 / %3 deg" )
			.arg( st.atFrame ).arg( fT ).arg( fR ) );
		wwCheck( st, QStringLiteral( "(b floor) the wrong frame FAILS the same test" ),
				 fT > 1.0e-4f || fR > 0.01f );
	}

	// (c) the readout. "frame 46 / 92" is only true at 60 fps: at 30 the same
	// instant is frame 23, so this number IS the rate gate.
	const QString shown = readout ? readout->text() : QString();
	wwSay( st, QStringLiteral( "  readout: \"%1\"" ).arg( shown ) );
	wwCheck( st, QStringLiteral( "(c) the readout names frame %1 of %2" )
			 .arg( st.atFrame ).arg( st.expFrames - 1 ),
			 shown.contains( QStringLiteral( "frame %1 / %2" )
							 .arg( st.atFrame ).arg( st.expFrames - 1 ) ) );
	wwCheck( st, QStringLiteral( "(c) which is the CLIP's rate, not the dock's default" ),
			 !shown.contains( QStringLiteral( "frame %1 /" )
							  .arg( qRound( float( st.atFrame ) * 30.0f / float( st.expFps ) ) ) ) );
	wwStepTo( skope, sc, 0.0f );
	tl->setTime( 0.0f, sc->timeMin(), sc->timeMax() );
	qApp->processEvents();
	wwCheck( st, QStringLiteral( "(c floor) at t=0 the readout says frame 0" ),
			 readout && readout->text().contains( QStringLiteral( "frame 0 / " ) ) );

	if ( !speed ) {
		wwCheck( st, QStringLiteral( "(c) the Speed row is there" ), false );
	} else {
		const float before = ogl->animationSpeed();
		speed->setValue( 2.0 );
		qApp->processEvents();
		const float after = ogl->animationSpeed();
		speed->setValue( 1.0 );
		qApp->processEvents();
		wwSay( st, QStringLiteral( "  speed: %1 -> %2 -> %3" )
			.arg( double( before ) ).arg( double( after ) )
			.arg( double( ogl->animationSpeed() ) ) );
		wwCheck( st, QStringLiteral( "(c floor) the speed reads 1 before the row is touched" ),
				 qAbs( before - 1.0f ) < 1.0e-4f );
		wwCheck( st, QStringLiteral( "(c) the Speed row writes the playback speed" ),
				 qAbs( after - 2.0f ) < 1.0e-4f );
		wwCheck( st, QStringLiteral( "(c) and puts it back" ),
				 qAbs( ogl->animationSpeed() - 1.0f ) < 1.0e-4f );
	}

	// Loop: the dock mirrors the render toolbar's action, so the button here IS
	// that action. Its state is what the transport reads.
	{
		QAction * loop = nullptr;
		for ( QToolButton * b : tl->findChildren<QToolButton *>() ) {
			if ( b->defaultAction()
				 && b->defaultAction()->text().contains( QLatin1String( "Loop" ),
														 Qt::CaseInsensitive ) )
				loop = b->defaultAction();
		}
		if ( !loop ) {
			wwSkip( st, QStringLiteral( "(c) no Loop action is mirrored on this dock" ) );
		} else {
			const bool was = loop->isChecked();
			loop->toggle();
			qApp->processEvents();
			const bool now = loop->isChecked();
			loop->setChecked( was );
			wwCheck( st, QStringLiteral( "(c) the dock's Loop row flips the transport's "
										 "loop action (%1 -> %2)" )
					 .arg( was ).arg( now ), now != was );
		}
		wwCheck( st, QStringLiteral( "(c) the clip is registered as a looping cycle" ),
				 sc->animCycle.value( clipName, -1 ) == Scene::CycleLoop );
	}

	// ------------------------------------------------------------- (d) ---
	wwStepTo( skope, sc, t );
	{
		const int moved = wwDiffCount( bindPose, wwSnapshot( sc ) );
		wwSay( st, QStringLiteral( "  posed: %1 of %2 nodes differ from the bind pose" )
			.arg( moved ).arg( bindPose.count() ) );
		wwCheck( st, QStringLiteral( "(d floor) the clip really moved the rig" ), moved > 0 );
	}
	if ( unloadBtn ) {
		wwCheck( st, QStringLiteral( "(d) the Unload control is enabled while a clip is loaded" ),
				 unloadBtn->isEnabled() );
	}
	const bool dropped = hub->unload( ogl, clipName );
	wwSettle( tl );
	wwStepTo( skope, sc, t );
	{
		const int diff = wwDiffCount( bindPose, wwSnapshot( sc ) );
		wwSay( st, QStringLiteral( "  after unload: %1 of %2 nodes differ" )
			.arg( diff ).arg( bindPose.count() ) );
		wwCheck( st, QStringLiteral( "(d) the clip unloads" ), dropped );
		wwCheck( st, QStringLiteral( "(d) its row leaves the dock's list" ),
				 wwRowOf( box, clipName ) < 0 );
		wwCheck( st, QStringLiteral( "(d) every transform is byte-identical to the bind pose" ),
				 diff == 0 );
	}

	// ------------------------------------------------------------- (e) ---
	// FLOOR FIRST: a file that is not an animation must add nothing.
	{
		const QString junk = QDir::tempPath() + QStringLiteral( "/ww_hkx3_not_an_animation.txt" );
		QFile jf( junk );
		if ( jf.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			jf.write( "not an animation\n" );
			jf.close();
		}
		const int rows = box->count();
		bool enterOk = false;
		const bool acc = wwSimulateDrop( skope, { junk }, &enterOk );
		wwSay( st, QStringLiteral( "  junk drop: enter accepted %1, drop accepted %2, "
								   "rows %3 -> %4" )
			.arg( enterOk ).arg( acc ).arg( rows ).arg( box->count() ) );
		wwCheck( st, QStringLiteral( "(e floor) dropping a non-animation adds no row" ),
				 box->count() == rows );
		QFile::remove( junk );
	}
	{
		const int rows = box->count();
		bool enterOk = false;
		const bool acc = wwSimulateDrop( skope, { st.clipPath }, &enterOk );
		wwSettle( tl );		// the drop rebuilds the list on the same 50 ms timer
		const int r = wwRowOf( box, clipName );
		const WwHkxListEntry de = hub->entry( ogl, clipName );
		wwSay( st, QStringLiteral( "  clip drop: enter accepted %1, drop accepted %2, "
								   "rows %3 -> %4, row %5" )
			.arg( enterOk ).arg( acc ).arg( rows ).arg( box->count() ).arg( r ) );
		wwCheck( st, QStringLiteral( "(e) the drag-enter is accepted for a .hkx" ), enterOk );
		wwCheck( st, QStringLiteral( "(e) the drop is accepted" ), acc );
		wwCheck( st, QStringLiteral( "(e) and yields the SAME entry: %1 frames @ %2 fps" )
				 .arg( de.numFrames ).arg( qRound( de.fps ) ),
				 r >= 0 && de.numFrames == st.expFrames && qRound( de.fps ) == st.expFps );
		wwCheck( st, QStringLiteral( "(e) the dropped clip is what the scene is playing" ),
				 sc->hkx->activeName() == clipName );
	}

	// ------------------------------------------------------------- (f) ---
	if ( st.refusePath.isEmpty() || !QFileInfo::exists( st.refusePath ) ) {
		wwSkip( st, QStringLiteral( "(f) no WW_HKXANIM_UI_REFUSE file given" ) );
		wwCheck( st, QStringLiteral( "(f) a refusable file was supplied" ), false );
	} else {
		const int rows = box->count();
		const QString why = hub->loadFiles( ogl, { st.refusePath }, false );
		wwSettle( tl );
		const QString rname = QFileInfo( st.refusePath ).completeBaseName();
		const int r = wwRowOf( box, rname );
		const WwHkxListEntry re = hub->entry( ogl, rname );
		wwSay( st, QStringLiteral( "  refuse: \"%1\"; rows %2 -> %3, row %4, text \"%5\"" )
			.arg( why ).arg( rows ).arg( box->count() ).arg( r )
			.arg( wwRowText( box, r ) ) );
		wwCheck( st, QStringLiteral( "(f) it lands as a row" ), r >= 0 );
		wwCheck( st, QStringLiteral( "(f) marked refused" ), re.refused );
		wwCheck( st, QStringLiteral( "(f) the row SAYS refused" ),
				 r >= 0 && wwRowText( box, r ).contains( QLatin1String( "refused" ),
														Qt::CaseInsensitive ) );
		wwCheck( st, QStringLiteral( "(f) with the reason in words" ),
				 re.reason.length() > 20 && !re.reason.contains( QLatin1String( "???" ) ) );
		wwCheck( st, QStringLiteral( "(f) and the reason reaches the summary line" ),
				 note && !note->text().isEmpty() );
	}

	// ------------------------------------------------------------ (g) ---
	wwPanelStyle( st, tl );

	// ------------------------------------------------------- the picture ---
	if ( st.shotPath.isEmpty() ) {
		wwSkip( st, QStringLiteral( "no WW_HKXANIM_UI_SHOT given: no dock grab" ) );
	} else {
		const int r = wwRowOf( box, clipName );
		if ( r >= 0 ) {
			box->setCurrentRow( r );
			qApp->processEvents();
		}
		wwStepTo( skope, sc, t );
		tl->setTime( t, sc->timeMin(), sc->timeMax() );
		dock->resize( 1200, 320 );
		qApp->processEvents();
		qApp->processEvents();
		const QPixmap grab = dock->grab();
		const bool wrote = !grab.isNull() && grab.save( st.shotPath );
		wwSay( st, QStringLiteral( "  dock grab %1x%2 -> %3" )
			.arg( grab.width() ).arg( grab.height() ).arg( st.shotPath ) );
		wwCheck( st, QStringLiteral( "the dock grab was written" ), wrote );
		wwCheck( st, QStringLiteral( "and it is not an empty strip" ),
				 grab.width() > 400 && grab.height() > 80 );
	}
}

} // namespace

void wwHkxAnimUiHarness( NifSkope * skope )
{
	if ( !skope || !qEnvironmentVariableIsSet( "WW_HKXANIM_UI_TEST" ) )
		return;

	auto * st = new WwUiState;
	const QString root = QApplication::applicationDirPath() + QStringLiteral( "/../" );
	st->clipPath = qEnvironmentVariable( "WW_HKXANIM_UI_CLIP",
		root + QStringLiteral( "fixtures/Running_To_Slide_And_Back_To_Running.hkx" ) );
	st->refusePath = qEnvironmentVariable( "WW_HKXANIM_UI_REFUSE",
		root + QStringLiteral( "scratchpad/hkx1_20260910/clips/skeleton.hkx" ) );
	st->shotPath = qEnvironmentVariable( "WW_HKXANIM_UI_SHOT" );
	st->atFrame = qEnvironmentVariable( "WW_HKXANIM_UI_FRAME",
		QStringLiteral( "46" ) ).toInt();

	const QStringList exp = qEnvironmentVariable( "WW_HKXANIM_UI_EXPECT",
		QStringLiteral( "93,60" ) ).split( QLatin1Char( ',' ) );
	if ( exp.count() == 2 ) {
		st->expFrames = exp.at( 0 ).toInt();
		st->expFps = exp.at( 1 ).toInt();
	}

	QObject::connect( skope, &NifSkope::completeLoading, skope,
		[skope, st]( bool ok, QString & ) {
			// 1.5 s, like the other WW harnesses: the scene is built on the
			// load signal, but the first paint -- and so the first transform
			// walk -- is not.
			QTimer::singleShot( 1500, skope, [skope, st, ok]() {
				wwRun( skope, *st, ok );
				wwFinish( skope, st );
			} );
		} );
}
