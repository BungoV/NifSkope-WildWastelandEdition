#!/usr/bin/env python3
"""Lane UI6 -- re-aim WW_HKXANIM_UI_TEST from the retired Animation MANAGER
dock at the Animation dock. Same questions, same loader, same playback; the
widgets moved."""

p = 'src/hkxanimuitest.cpp'
b = open(p, 'rb').read().decode('utf-8')
cr = b.count('\r')


def rep(a, t):
    global b
    c = b.count(a)
    assert c == 1, (c, a[:90])
    b = b.replace(a, t, 1)


rep("""/* WW_HKXANIM_UI_TEST: the pre-registered gates of lane HKX3 -- a loaded Havok
   animation clip as a row of the Animation Manager dock's own list, the drop
   path, the unload, and the panel-style rules the new controls must obey.""",
    """/* WW_HKXANIM_UI_TEST: the pre-registered gates of lane HKX3 -- a loaded Havok
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
   moved with it.""")

rep("""   Everything here reads the WIDGETS by object name -- TimelineSeqBox,
   TimelineLoadAnim, TimelineUnloadAnim, TimelineRootMotion, TimelineSpeed,
   TimelineFrameReadout, TimelineAnimNote -- and never the dock's private
   members. A gate that reads the private state reads what the code MEANT; a
   gate that reads the combo reads what the user is looking at.""",
    """   Everything here reads the WIDGETS by object name -- AnimWsClipList,
   AnimWsLoadAnim, AnimWsUnloadAnim, AnimWsRootMotion, AnimWsSpeed,
   AnimWsReadout, AnimWsNote -- and never the dock's private members. A gate
   that reads the private state reads what the code MEANT; a gate that reads
   the list reads what the user is looking at.""")

rep('#include "ui/widgets/timeline.h"', '#include "animworkspace.h"')
rep('#include <QLabel>', '#include <QLabel>\n#include <QListWidget>')

rep("""//! The row of the dock's combo that carries `name` as its user data, or -1.
int wwRowOf( QComboBox * box, const QString & name )
{
	if ( !box )
		return -1;
	for ( int i = 0; i < box->count(); i++ ) {
		if ( box->itemData( i ).toString() == name )
			return i;
	}
	return -1;
}""",
    """/*! The row of the dock's LIST that carries `name` as its user data, or -1.
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

//! The text of one row of that list ("" when the row is not there).
QString wwRowText( QListWidget * box, int row )
{
	if ( !box || row < 0 || row >= box->count() )
		return QString();
	return box->item( row )->text();
}""")

rep("""void wwPanelStyle( WwUiState & st, TimelineWidget * tl )
{
	if ( !tl ) {
		wwCheck( st, QStringLiteral( "(g) the Animation Manager dock is there" ), false );
		return;
	}""",
    """void wwPanelStyle( WwUiState & st, AnimWorkspace * tl )
{
	if ( !tl ) {
		wwCheck( st, QStringLiteral( "(g) the Animation dock is there" ), false );
		return;
	}""")

rep("""	const QStringList mine = { QStringLiteral( "TimelineSeqBox" ),
							   QStringLiteral( "TimelineLoadAnim" ),
							   QStringLiteral( "TimelineUnloadAnim" ),
							   QStringLiteral( "TimelineRootMotion" ),
							   QStringLiteral( "TimelineSpeed" ),
							   QStringLiteral( "TimelineFrameReadout" ) };""",
    """	const QStringList mine = { QStringLiteral( "AnimWsClipList" ),
							   QStringLiteral( "AnimWsLoadAnim" ),
							   QStringLiteral( "AnimWsUnloadAnim" ),
							   QStringLiteral( "AnimWsRootMotion" ),
							   QStringLiteral( "AnimWsSpeed" ),
							   QStringLiteral( "AnimWsReadout" ) };""")

rep("""	QWidget * note = tl->findChild<QWidget *>( QStringLiteral( "TimelineAnimNote" ) );
	QWidget * box = tl->findChild<QWidget *>( QStringLiteral( "TimelineSeqBox" ) );""",
    """	QWidget * note = tl->findChild<QWidget *>( QStringLiteral( "AnimWsNote" ) );
	QWidget * box = tl->findChild<QWidget *>( QStringLiteral( "AnimWsActionBar" ) );""")

rep("""		wwCheck( st, QStringLiteral( "(g) the animations list is outside the splitter" ),
				 !vsplit->isAncestorOf( box ) );""",
    """		/* Lane UI6: in the Animation dock the LIST is inside the splitter's
		 * left column by design -- it is a pane, not a strip control -- so the
		 * question the old dock's check asked (can it scroll away from what it
		 * answers for) is asked of the ACTION BAR, which is the thing that
		 * must stay pinned. Same predicate, same splitter, different widget. */
		wwCheck( st, QStringLiteral( "(g) the action bar is outside the splitter" ),
				 !vsplit->isAncestorOf( box ) );""")

rep("""	auto * speed = tl->findChild<QDoubleSpinBox *>( QStringLiteral( "TimelineSpeed" ) );
	if ( !speed ) {""",
    """	auto * speed = tl->findChild<QDoubleSpinBox *>( QStringLiteral( "AnimWsSpeed" ) );
	if ( !speed ) {""")

rep("""	auto * dock = skope->findChild<QDockWidget *>( QStringLiteral( "TimelineDock" ) );
	auto * tl = skope->findChild<TimelineWidget *>();
	if ( !dock || !tl ) {
		wwCheck( st, QStringLiteral( "the Animation Manager dock is there" ), false );
		return;
	}""",
    """	auto * dock = skope->findChild<QDockWidget *>( QStringLiteral( "AnimWorkspaceDock" ) );
	auto * tl = skope->findChild<AnimWorkspace *>();
	if ( !dock || !tl ) {
		wwCheck( st, QStringLiteral( "the Animation dock is there" ), false );
		return;
	}""")

rep("""	auto * box = tl->findChild<QComboBox *>( QStringLiteral( "TimelineSeqBox" ) );
	auto * readout = tl->findChild<QLabel *>( QStringLiteral( "TimelineFrameReadout" ) );
	auto * note = tl->findChild<QLabel *>( QStringLiteral( "TimelineAnimNote" ) );
	auto * speed = tl->findChild<QDoubleSpinBox *>( QStringLiteral( "TimelineSpeed" ) );
	auto * unloadBtn = tl->findChild<QToolButton *>( QStringLiteral( "TimelineUnloadAnim" ) );""",
    """	auto * box = tl->findChild<QListWidget *>( QStringLiteral( "AnimWsClipList" ) );
	auto * readout = tl->findChild<QLabel *>( QStringLiteral( "AnimWsReadout" ) );
	auto * note = tl->findChild<QLabel *>( QStringLiteral( "AnimWsNote" ) );
	auto * speed = tl->findChild<QDoubleSpinBox *>( QStringLiteral( "AnimWsSpeed" ) );
	auto * unloadBtn = tl->findChild<QToolButton *>( QStringLiteral( "AnimWsUnloadAnim" ) );""")

rep("""		.arg( box->count() ).arg( row )
		.arg( row >= 0 ? box->itemText( row ) : QString() ) );""",
    """		.arg( box->count() ).arg( row ).arg( wwRowText( box, row ) ) );""")

rep("""			 row >= 0 && box->itemText( row ).contains( QString::number( st.expFrames ) )
			 && box->itemText( row ).contains( QString::number( st.expFps ) ) );""",
    """			 row >= 0 && wwRowText( box, row ).contains( QString::number( st.expFrames ) )
			 && wwRowText( box, row ).contains( QString::number( st.expFps ) ) );""")

rep("""	// selecting the row is what makes the timeline drive the clip
	if ( row >= 0 && box->currentIndex() != row ) {
		box->setCurrentIndex( row );
		emit box->activated( row );
		qApp->processEvents();
	}""",
    """	// selecting the row is what makes the dock drive the clip
	if ( row >= 0 && box->currentRow() != row ) {
		box->setCurrentRow( row );
		qApp->processEvents();
	}""")

rep("""				 r >= 0 && box->itemText( r ).contains( QLatin1String( "refused" ),
														Qt::CaseInsensitive ) );""",
    """				 r >= 0 && wwRowText( box, r ).contains( QLatin1String( "refused" ),
														Qt::CaseInsensitive ) );""")

rep("""			.arg( why ).arg( rows ).arg( box->count() ).arg( r )
			.arg( r >= 0 ? box->itemText( r ) : QString() ) );""",
    """			.arg( why ).arg( rows ).arg( box->count() ).arg( r )
			.arg( wwRowText( box, r ) ) );""")

rep("""		const int r = wwRowOf( box, clipName );
		if ( r >= 0 ) {
			box->setCurrentIndex( r );
			emit box->activated( r );
			qApp->processEvents();
		}""",
    """		const int r = wwRowOf( box, clipName );
		if ( r >= 0 ) {
			box->setCurrentRow( r );
			qApp->processEvents();
		}""")

out = b.encode('utf-8')
assert out.count(b'\r') == cr
open(p, 'wb').write(out)
print('ok %s %d bytes, CR %d' % (p, len(out), cr))
