/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

See the accompanying LICENSE file for the full text.

***** END LICENSE BLOCK *****/

#include "harnesswindow.h"

#include "nifskope.h"

#include <QCoreApplication>
#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QPointer>
#include <QRect>
#include <QScreen>
#include <QTextStream>
#include <QWidget>
#include <QWindow>

/* ---------------------------------------------------------------------------
 * A HARNESS RUN'S WINDOW IS FORCED, NEVER INHERITED.  Lane HARNESSWIN1,
 * 2026-09-19.  This is a REPAIR: no INI key, no menu row, no master switch.
 *
 * THE DEFECT, AND THE MEASUREMENT THAT IS ALREADY ON DISK
 *
 * tests/spells/native_open.sh row (c) -- "the .lodi scene draws everything the
 * .BTO draws (covered 0.8978 >= 0.90)" -- has been RED, and it is not a
 * rendering defect.  Lane HORIZONOUT ran the same script minutes apart on the
 * shipped exe and on release/NifSkope.before_horizonout.exe and got the same
 * number to four decimals, so no code change moved it
 * (scratchpad/horizonout_20260919/lane_horizonout_report.md, s "native_open.sh
 * (c): the one red").  What moved was the WINDOW:
 *
 *     2026-09-18 22:38   vp=1024x989   upp=16.000000   COVER 0.9331
 *     2026-09-19         vp=1822x989   upp= 8.992316   COVER 0.8978
 *
 * HKCU\Software\NifTools\NifSkope 2.0\UI\Window Geometry decodes to frame
 * 1920x1048 on screen 1, MAXIMIZED, normal 1280x800 -- bungo's last window.
 *
 * Lane IMPOSTORSHOW then reproduced it exactly and named the mechanism
 * (src/impostorpreviewtest.cpp, forceWindow()): showNormal() ALONE DOES NOT
 * CLEAR A WindowMaximized BIT that arrived with the restored geometry, and a
 * resize() applied to a still-maximized window is discarded WITHOUT A WORD.
 * "asked 1024x1024, got 1822x989".  The height survives the discard because the
 * grab reads the viewport rather than the frame; the width does not.  At upp
 * 8.99 instead of 16.0 a sub-pixel disagreement between a .lodi scene and a
 * .BTO resolves into whole pixels, and the row goes red.
 *
 * So the harness measured bungo's last window instead of the code.
 *
 * WHAT THIS FILE DOES ABOUT IT, IN THREE PARTS
 *
 *   1. The restore is SUPPRESSED.  restoreUi() does not replay UI/Window
 *      Geometry on a harness run, so the maximized bit never arrives in the
 *      first place.  Belt and braces: wwApplyHarnessWindow() clears the state
 *      bits BY HAND as well, because the window manager can re-apply a state
 *      across show() and clearing a bit on a widget whose platform window does
 *      not exist yet has been observed not to stick (src/nifskope_ui.cpp:1609,
 *      the 2026-09-09 note).
 *
 *   2. The size is EXPLICIT.  WW_WINDOW_SIZE=WxH, else WW_RENDER_SIZE=WxH so
 *      the eleven spells that already set that variable need no edit, else a
 *      default MEASURED off the screen the window is placed on.
 *
 *   3. The size obtained is WRITTEN DOWN, and a floor REFUSES.  Every harness
 *      can put wwHarnessWindowLine() in its log, and wwHarnessSizeRefusal()
 *      hands back a sentence carrying BOTH numbers.  A gate that gets a
 *      non-empty string is entitled to refuse rather than measure.  This is the
 *      part that matters most: the floor above was silent for a day, and a
 *      silent floor turns a harness into a measurement of the machine.
 *
 * THE WRITE-BACK HALF IS ALREADY DONE, AND IS NOT DUPLICATED HERE.
 * NifSkope::saveUi() (src/nifskope_ui.cpp:31398) returns early when ANY
 * environment key starts with "WW_", added 2026-07-27 after a WW_RENDER_SHOT
 * run left its hidden-dock layout as the user's saved layout.  The settings
 * this lane must leave byte-identical are therefore already untouched by the
 * main window:
 *
 *     UI/Window Geometry      UI/Window State
 *     UI/LeftColumn/LayoutSchema   /Mode   /BlockSplitter   /NifSplitter
 *     UI/List Header/list     UI/List Header/hierarchy
 *     UI/Tree Header          UI/Header Header    UI/Kfmtree Header
 *     UI/List Mode            UI/Show Non-applicable Rows
 *     Theme                   File/Auto Sanitize
 *     GLView/Enable Animations
 *
 * Row (b) of tests/spells/harness_window.sh proves that byte-for-byte rather
 * than trusting the paragraph above.
 *
 * TWO WRITE PATHS ARE *NOT* COVERED BY THAT GUARD, and are named rather than
 * fixed here because no harness currently drives them:
 * AnimWorkspace/splitter and AnimWorkspace/sidePanelWidth
 * (src/animworkspace.cpp:649-654) and BodyBuild/splitter
 * (src/bodybuildpanel.cpp:560) both write from a splitterMoved handler, outside
 * saveUi().  See CHANGE_NEEDED in the lane's handoff.
 *
 * THE PREDICATE, AND WHY IT IS THIS ONE
 *
 * NifSkope::wwHeadlessRun() (src/nifskope.cpp:7584): any WW_* environment key,
 * or -no-gui, cached.  Chosen over "WW_WINDOW_AT is set" and over a list of
 * WW_*_TEST names because IT IS THE SAME PREDICATE saveUi() ALREADY USES.  With
 * any other predicate the read side and the write side can disagree -- a run
 * that suppresses the restore but not the save, or the reverse, and the second
 * of those is exactly the 2026-07-27 incident.  One predicate, both directions.
 * WW_WINDOW_AT is in any case a subset: tests/spells/_harness.sh:29 exports it
 * for every spell.
 *
 * CAN A 1024x1024 VIEWPORT FIT ON THE SECOND MONITOR AT 1960,40?  NO, NOT
 * WHOLLY ON SCREEN -- SAID PLAINLY BECAUSE THE BRIEF ASKED.
 *
 * Arithmetic, from the one measurement this tree actually has (a client height
 * of 1024 produced a viewport of 989, so the window's own vertical chrome --
 * menu bar, tool bars, status bar, with every dock and the viewport header
 * hidden as the WW_RENDER_SHOT path hides them -- is 35 px), plus a Windows 10
 * frame of about 39 px of title bar and border:
 *
 *     viewport 1024 tall  ->  client 1059  ->  frame 1098
 *     at origin y=40 on a 1080-tall screen the frame bottom lands at 1138
 *     at origin y=0                                              at 1098
 *
 * A 1080-tall screen cannot hold it at ANY origin.  The honest wholly-on-screen
 * maximum on 1920x1080 is about a 1024x966 viewport at 1960,40, or 1024x1006 at
 * 1960,0.  The 35 and the 39 are INFERRED from one logged run and from the
 * platform's usual metrics, NOT measured by this lane, which is why
 * wwHarnessDefaultWindowSize() re-derives both at run time off the real screen
 * and the real frameGeometry() instead of hard-coding them.
 *
 * It does not follow that a bigger request must be refused.  Windows does not
 * clamp an oversized top-level, and a window hanging off the bottom of the
 * screen is still EXPOSED, so it still has a GL context and still renders at
 * its full widget size -- which is why the 2026-09-19 run produced a 989-tall
 * viewport from a window whose frame bottom sat below 1080.  So an EXPLICIT
 * WW_WINDOW_SIZE / WW_RENDER_SIZE is honoured as asked, oversized or not; only
 * the DEFAULT is the wholly-on-screen figure.  If the request is not obtained,
 * the refusal says so with both numbers, which is the whole point.
 *
 * The supported route to a genuinely larger capture than the screen allows is
 * WW_RENDER_SS, which supersamples through an offscreen FBO
 * (src/nifskope_ui.cpp:22316-22320).
 *
 * THE REFUTER FOR ALL OF THE ABOVE is row (c) of
 * tests/spells/harness_window.sh: plant a maximized geometry in an ISOLATED
 * settings scope, run the OLD path, and watch it floor.  Nothing here is called
 * fixed until that row has been executed and bungo has seen a green
 * native_open.sh.
 * ------------------------------------------------------------------------- */

namespace {

//! Cached across the process: the environment does not change under us, and the
//! placement runs on paths that must not walk the environment per window.
QSize & askedSizeCache()
{
	static QSize s;
	return s;
}

bool & askedSizeExplicit()
{
	static bool b = false;
	return b;
}

/*! THE WINDOW IS TRACKED, AND EVERY ANSWER IS RECOMPUTED FROM IT.
 *
 *  Not a cached sentence.  The WW_RENDER_SHOT path resizes the window on a
 *  2500 ms timer, long after the window was shown, so a line built at show time
 *  would be a truthful record of the wrong moment -- and its place in the grab
 *  lambda (src/nifskope_ui.cpp ~22324) is inside the block lane IMPOSTORSHOW
 *  owns, which this lane may not touch.  Recomputing on demand means a harness
 *  gets the truth at the instant IT asks, with no further hook-up anywhere.
 *
 *  QPointer, because a harness may outlive the window it measured.
 */
QPointer<QWidget> & trackedWindow()
{
	static QPointer<QWidget> w;
	return w;
}

//! The viewport last handed to wwHarnessRecordViewport, so the watcher below
//! can keep naming it without the caller passing it again.
QPointer<QObject> & trackedViewport()
{
	static QPointer<QObject> v;
	return v;
}

//! The size of the thing being measured, whether it is a widget or a window.
QSize viewportSize( const QObject * v )
{
	if ( const QWidget * w = qobject_cast<const QWidget *>( v ) )
		return w->size();
	if ( const QWindow * n = qobject_cast<const QWindow *>( v ) )
		return n->size();
	return QSize();
}

//! WxH out of an environment variable, or an invalid QSize.
//! Floors at 320x240 exactly as the two existing readers do
//! (src/nifskope_ui.cpp:22091, src/impostorpreviewtest.cpp:224), so a harness
//! that moves onto this path sees no change of behaviour at the small end.
QSize parseSizeEnv( const char * name )
{
	const QString v = qEnvironmentVariable( name );
	if ( !v.contains( QLatin1Char( 'x' ) ) )
		return QSize();
	const QStringList parts = v.split( QLatin1Char( 'x' ) );
	if ( parts.size() != 2 )
		return QSize();
	bool okW = false, okH = false;
	const int w = parts.at( 0 ).toInt( &okW );
	const int h = parts.at( 1 ).trimmed().toInt( &okH );
	if ( !okW || !okH || w <= 0 || h <= 0 )
		return QSize();
	return QSize( qMax( w, 320 ), qMax( h, 240 ) );
}

//! The screen a point sits on, or the first non-primary one, or the primary.
//! Mirrors wwHeadlessWindowOrigin()'s preference order without duplicating its
//! arms: this only has to know WHICH SCREEN, not which corner.
const QScreen * screenForPoint( const QPoint & p )
{
	for ( const QScreen * s : QGuiApplication::screens() )
		if ( s->geometry().contains( p ) )
			return s;
	const QScreen * primary = QGuiApplication::primaryScreen();
	for ( const QScreen * s : QGuiApplication::screens() )
		if ( s != primary )
			return s;
	return primary;
}

} // namespace


QString wwHarnessSettingsSuffix()
{
	/* A HARNESS GETS ITS OWN SETTINGS SCOPE, BY CONSTRUCTION.
	 *
	 * The memory rule is "a GUI harness forces the state it measures, never
	 * inherits QSettings", and a gate for THIS repair has to PLANT a maximized
	 * geometry and then prove the planted bytes are unchanged.  Planting it in
	 * bungo's own key and restoring afterwards is what
	 * tests/spells/window_state_roundtrip.sh has to do, and that script's own
	 * comments record two separate occasions on which the restore failed and
	 * test-only values were left in his profile.  There is no reason to keep
	 * paying that.
	 *
	 * WW_SETTINGS_SCOPE=<name> appends " <name>" to applicationName, so the
	 * WHOLE QSettings tree moves to HKCU\Software\NifTools\NifSkope 2.0 <name>
	 * and his key cannot be reached at all -- not by the plant, not by
	 * saveUi(), not by a splitter handler that saveUi()'s guard does not cover.
	 * Nothing is restored afterwards because nothing was borrowed.
	 *
	 * Deliberately NOT gated on wwHeadlessRun(): setting the variable makes the
	 * run a WW_ run anyway, and this is called from main.cpp while the
	 * application object is still being configured, where depending on
	 * QCoreApplication::arguments() would be an ordering question nobody should
	 * have to think about.
	 *
	 * The charset is restricted because this string becomes a registry key
	 * name; an unusable value is IGNORED and the run uses the normal scope,
	 * which the gate detects by reading the line this file writes rather than
	 * by trusting the variable.
	 */
	const QString scope = qEnvironmentVariable( "WW_SETTINGS_SCOPE" ).trimmed();
	if ( scope.isEmpty() || scope.size() > 40 )
		return QString();
	for ( const QChar c : scope )
		if ( !c.isLetterOrNumber() && c != QLatin1Char( '-' ) && c != QLatin1Char( '_' ) )
			return QString();
	return QLatin1Char( ' ' ) + scope;
}


bool wwHarnessRun()
{
	return NifSkope::wwHeadlessRun();
}


bool wwHarnessGeometryRestoreSuppressed()
{
	return NifSkope::wwHeadlessRun();
}


bool wwHarnessWindowSizeWasAsked()
{
	if ( !askedSizeCache().isValid() )
		(void) wwHarnessAskedWindowSize();
	return askedSizeExplicit();
}


QSize wwHarnessDefaultWindowSize( const QWidget * w )
{
	/* Measured, not assumed, and it degrades honestly.
	 *
	 * frameGeometry() equals geometry() until the platform window exists, so
	 * before show() the frame margins read as zero.  A zero there would make
	 * the default too TALL by the height of a title bar, which is the failure
	 * that puts a window off the bottom of the screen -- so when the margins
	 * are not yet real, a conservative allowance is used and the window is
	 * re-sized after show() by wwApplyHarnessWindow() being called again from
	 * the QEvent::Show filter, at which point the margins ARE real.
	 */
	const QPoint origin = w ? w->pos() : QPoint( 1960, 40 );
	const QScreen * screen = screenForPoint( origin );
	const QRect avail = screen ? screen->availableGeometry() : QRect( 0, 0, 1920, 1080 );

	int frameW = 16, frameH = 39;           // the conservative allowance
	if ( w ) {
		const QRect fg = w->frameGeometry();
		const QRect g = w->geometry();
		if ( fg.isValid() && g.isValid() && fg != g ) {
			frameW = qMax( 0, fg.width() - g.width() );
			frameH = qMax( 0, fg.height() - g.height() );
		}
	}

	const int maxW = avail.right() - origin.x() + 1 - frameW;
	const int maxH = avail.bottom() - origin.y() + 1 - frameH;

	/* 1024 wide is what native_open.sh, impostor_draw.sh and native_lighting.sh
	 * all ask for, so it is the width a default should hand them when they do
	 * not ask; the height is whatever the screen honestly has left. */
	return QSize( qBound( 320, 1024, qMax( 320, maxW ) ),
	              qBound( 240, maxH, qMax( 240, maxH ) ) );
}


QSize wwHarnessAskedWindowSize()
{
	if ( askedSizeCache().isValid() )
		return askedSizeCache();

	QSize s = parseSizeEnv( "WW_WINDOW_SIZE" );
	if ( !s.isValid() )
		s = parseSizeEnv( "WW_RENDER_SIZE" );

	askedSizeExplicit() = s.isValid();
	if ( !s.isValid() )
		s = wwHarnessDefaultWindowSize( nullptr );

	askedSizeCache() = s;
	return s;
}


void wwApplyHarnessWindow( QWidget * w )
{
	if ( !w || !NifSkope::wwHeadlessRun() )
		return;

	/* BY HAND, AND BEFORE showNormal().  showNormal() sets the state to
	 * WindowNoState through the platform, and on Windows a window that the WM
	 * still considers maximized swallows the resize that follows without an
	 * error.  Lane IMPOSTORSHOW measured exactly that: asked 1024x1024, got
	 * 1822x989.  Clearing the bits on the widget first is what makes the resize
	 * land. */
	w->setWindowState( w->windowState()
		& ~( Qt::WindowMaximized | Qt::WindowFullScreen | Qt::WindowMinimized ) );
	if ( w->isVisible() )
		w->showNormal();

	QSize want = wwHarnessAskedWindowSize();
	if ( !askedSizeExplicit() ) {
		// Re-derive off THIS window now that its frame margins may be real.
		const QSize measured = wwHarnessDefaultWindowSize( w );
		if ( measured.isValid() ) {
			want = measured;
			askedSizeCache() = measured;
		}
	}
	w->resize( want );
	trackedWindow() = w;
}


//! Build the line from the window as it is AT THIS INSTANT.
static QString wwHarnessLineFor( QWidget * window, QObject * viewport, const char * when )
{
	const QSize asked = wwHarnessAskedWindowSize();
	const QSize got = window ? window->size() : QSize();

	const QSize vp = viewportSize( viewport );
	const QPoint at = window ? window->pos() : QPoint();
	const bool maximised = window
		&& ( window->windowState() & ( Qt::WindowMaximized | Qt::WindowFullScreen ) );

	/* ONE LINE, AND IT CARRIES BOTH NUMBERS.  A harness that prints only what it
	 * GOT cannot be read six weeks later: "vp=1822x989" is a fact about a
	 * machine until the request is beside it. */
	QString line = QStringLiteral(
		"harness-window %1 asked=%2x%3%4 window=%5x%6 viewport=%7 origin=%8,%9"
		" maximised=%10 settings=%11" )
		.arg( QLatin1String( when ? when : "?" ) )
		.arg( asked.width() ).arg( asked.height() )
		.arg( askedSizeExplicit() ? QString() : QStringLiteral( "(default)" ) )
		.arg( got.width() ).arg( got.height() )
		.arg( vp.isValid() ? QStringLiteral( "%1x%2" ).arg( vp.width() ).arg( vp.height() )
		                   : QStringLiteral( "-" ) )
		.arg( at.x() ).arg( at.y() )
		.arg( maximised ? 1 : 0 )
		.arg( wwHarnessGeometryRestoreSuppressed()
			? QStringLiteral( "not-restored" ) : QStringLiteral( "RESTORED" ) );

	if ( !wwHarnessSizeRefusal( asked, got ).isEmpty() )
		line += QLatin1String( " FLOORED" );
	return line;
}


/*! THE LAST LINE IN THE LOG IS THE SETTLED ONE, AND NO TIMER GUESSES WHEN THAT IS.
 *
 *  The size a harness cares about is the size at GRAB time, and the grab is a
 *  2500 ms lambda inside the WW_RENDER_SHOT block (src/nifskope_ui.cpp ~22300)
 *  that belongs to lane IMPOSTORSHOW -- this lane may not put a call there.
 *  A single record at show time would therefore be a true statement about a
 *  moment nobody measures: the run above recorded window=1020x480 at "shown"
 *  and the window was 1024x480 by the time the picture was taken.
 *
 *  So instead of adding a second hook-up, or picking a delay and hoping, the
 *  window is WATCHED.  Every resize and every window-state change re-records,
 *  the writer drops a line identical to the one before it, and whatever the
 *  window last did is the last line in the file.  A harness reads `tail -1`.
 *
 *  Q_OBJECT is deliberately absent: eventFilter is virtual, there are no
 *  signals or slots, so this needs no moc pass and cannot go stale when qmake
 *  is not re-run.
 */
namespace {

void wwHarnessWrite( const QString & line, const QString & refusal );

class WwHarnessWatcher final : public QObject
{
public:
	explicit WwHarnessWatcher( QObject * parent ) : QObject( parent ) {}

protected:
	bool eventFilter( QObject * o, QEvent * e ) override
	{
		switch ( e->type() ) {
		case QEvent::Resize:
		case QEvent::Show:
		case QEvent::WindowStateChange:
		case QEvent::Move:
			break;
		default:
			return QObject::eventFilter( o, e );
		}
		QWidget * w = trackedWindow().data();
		if ( w ) {
			const QString line = wwHarnessLineFor( w, trackedViewport().data(), "settled" );
			wwHarnessWrite( line, wwHarnessSizeRefusal( wwHarnessAskedWindowSize(), w->size() ) );
		}
		return QObject::eventFilter( o, e );
	}
};

//! Append, and never twice the same sentence -- a resize storm is one line.
void wwHarnessWrite( const QString & line, const QString & refusal )
{
	static QString last;
	static bool opened = false;
	if ( line == last )
		return;
	last = line;

	QFile f( QCoreApplication::applicationDirPath()
		+ QStringLiteral( "/ww_harness_window.log" ) );
	if ( !f.open( ( opened ? QIODevice::Append : QIODevice::WriteOnly ) | QIODevice::Text ) )
		return;
	opened = true;
	QTextStream ts( &f );
	ts << line << "\n";
	if ( !refusal.isEmpty() )
		ts << refusal << "\n";
	ts.flush();
	f.close();
}

} // namespace


void wwHarnessRecordViewport( QWidget * window, QObject * viewport, const char * when )
{
	if ( !NifSkope::wwHeadlessRun() )
		return;
	if ( window )
		trackedWindow() = window;
	if ( viewport )
		trackedViewport() = viewport;

	const QString line = wwHarnessLineFor( window, viewport, when );
	const QString refusal = wwHarnessSizeRefusal(
		wwHarnessAskedWindowSize(), window ? window->size() : QSize() );
	wwHarnessWrite( line, refusal );

	// Once, on the window itself; the watcher dies with it.
	static QPointer<WwHarnessWatcher> watcher;
	if ( window && !watcher ) {
		watcher = new WwHarnessWatcher( window );
		window->installEventFilter( watcher );
		if ( viewport )
			viewport->installEventFilter( watcher );
	}
}


QString wwHarnessWindowLine( QObject * viewport )
{
	if ( !NifSkope::wwHeadlessRun() )
		return QString();
	QWidget * w = trackedWindow().data();
	if ( !w )
		return QStringLiteral( "harness-window not-recorded (no window tracked)" );
	return wwHarnessLineFor( w, viewport, "asked" );
}


QString wwHarnessSizeRefusal( const QSize & asked, const QSize & got )
{
	if ( !asked.isValid() || !got.isValid() )
		return QString();
	if ( asked == got )
		return QString();

	/* ONLY AN EXPLICIT REQUEST CAN BE REFUSED.  Caught by running
	 * tests/spells/render_shot.sh, 2026-09-19, which does not set
	 * WW_RENDER_SIZE at all: this file then supplies its own screen-derived
	 * default (1024x904 on the second monitor), while the WW_RENDER_SHOT grab
	 * lambda separately falls back to ITS default of 1280x800
	 * (src/nifskope_ui.cpp:22101).  Two different defaults, neither of them
	 * asked for by anybody -- and the line accused the run of flooring a
	 * request that was never made ("asked for 1024x904 ... came out 1822x560").
	 *
	 * A floor that fires on a correct input is not a floor, it is a second
	 * defect (ww-test-harness-add 5b).  So a run that named no size gets the
	 * size written down and no accusation; the word REFUSED is reserved for a
	 * WW_WINDOW_SIZE or WW_RENDER_SIZE that was explicitly asked for and not
	 * obtained, which is the only case where a harness is entitled to stop. */
	if ( !askedSizeExplicit() )
		return QString();
	return QStringLiteral(
		"REFUSED: the window size asked for was %1x%2 and the window came out"
		" %3x%4 -- the request was floored, so this run measures the machine"
		" and not the code" )
		.arg( asked.width() ).arg( asked.height() )
		.arg( got.width() ).arg( got.height() );
}


QString wwHarnessSizeRefusal()
{
	if ( !NifSkope::wwHeadlessRun() )
		return QString();
	QWidget * w = trackedWindow().data();
	if ( !w )
		return QString();
	return wwHarnessSizeRefusal( wwHarnessAskedWindowSize(), w->size() );
}
