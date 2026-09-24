#!/usr/bin/env python3
"""Lane UI6 -- the edits to the SHARED, high-traffic files, as a refusing script
(skill ww-anchored-hookup).

`src/nifskope_ui.cpp`, `src/nifskope.h`, `src/wateruitest.cpp` and
`tests/spells/water_ui.sh` were touched by lanes UI3, UI4, UI5, WATER7 and
WATER8 in this session alone. Every anchor below is exact, carries the file's
real line ending (all four are LF-only: b.count(b'\\r') == 0), and must match
EXACTLY ONCE. --check is the default and writes nothing; --apply refuses unless
every anchor still matches once, and then writes every file or none.

The lane's other files (hkxplayback, hkxanimui, animworkspace, animworkspacetest,
hkxanimuitest, wateruitest_lod, timeline) have one owner and no lane was alive in
the tree, so they are edited directly and named in the report instead.
"""

import sys

EDITS = []


def E(path, mode, anchor, text, note):
    EDITS.append((path, mode, anchor, text, note))


UI = "src/nifskope_ui.cpp"

# --------------------------------------------------------------- item 1, the strip
E(UI, "replace",
  """	 * A separated segment also has to CLOSE ITS OWN BOX: with a gap beside it,
	 * the joined strip's `border-left: 0` would leave an open-sided rectangle.
	 * The shared seam and the square inner corners are the JOINED look only,
	 * and they come back exactly at air 0.
	 */""",
  """	 * THE SEGMENTS TOUCH (bungo, 2026-09-10 21:0x, over a zoomed screenshot of
	 * the strip lane UI4 had just gapped, verbatim: "Why are they separated?").
	 * A segmented control is ONE element, so the air is measured from its OUTER
	 * box and nowhere else: four pixels from the row's top and bottom, from the
	 * window's left edge and from the toolbar beside it, and NOTHING between the
	 * segments. They share one seam again (`border-left: 0` on every segment,
	 * the first putting its own back), their inner corners are square and the
	 * two outer corners stay rounded as UI4 left them.
	 *
	 * Measured before it was written, scratchpad/ui6_20260910/probe.cpp:
	 *   case 0  (shipped)  top 4 bottom 4 left 4 right 4 between 4 / 4, h 27
	 *   case J4 (this)     top 4 bottom 4 left 4 right 4 between 0 / 0, h 27
	 *   case J0 == case 1  -- the way back at air 0 is the 18:25:20 sheet, and
	 *                         the two strings are byte-identical, not merely
	 *                         equivalent.
	 */""",
  "the joined-strip rationale replaces the separated one"),

E(UI, "replace",
  """		"%1 { min-height: MINHpx; padding: 3px 10px; color: %4; background: %5;"
		" border: 1px solid %6; SEAM border-radius: BRADpx; MARGINALL }"
		"%2 { border-left: 1px solid %6; border-top-left-radius: 3px;"
		" border-bottom-left-radius: 3px; }\"""",
  """		"%1 { min-height: MINHpx; padding: 3px 10px; color: %4; background: %5;"
		" border: 1px solid %6; border-left: 0; border-radius: 0px; MARGINALL }"
		"%2 { border-left: 1px solid %6; border-top-left-radius: 3px;"
		" border-bottom-left-radius: 3px;MARGINFIRST }\"""",
  "the strip is one joined box at every air"),

E(UI, "replace",
  """	sheet.replace( QLatin1String( "MINH" ), QString::number( minH ) );
	sheet.replace( QLatin1String( "BRAD" ), QString::number( air > 0 ? 3 : 0 ) );
	sheet.replace( QLatin1String( "SEAM" ),
		air > 0 ? QString() : QStringLiteral( "border-left: 0;" ) );
	sheet.replace( QLatin1String( "MARGINALL" ), air > 0
		? QStringLiteral( "margin-top: %1px; margin-bottom: %1px; margin-left: %1px;"
			" margin-right: 0px;" ).arg( air )
		: QString() );""",
  """	sheet.replace( QLatin1String( "MINH" ), QString::number( minH ) );
	/* The air is on the OUTER edges only. MARGINFIRST carries its own leading
	 * space, so at air 0 the emitted string is the 18:25:20 sheet byte for
	 * byte rather than one space off it. */
	sheet.replace( QLatin1String( "MARGINALL" ), air > 0
		? QStringLiteral( "margin-top: %1px; margin-bottom: %1px; margin-left: 0px;"
			" margin-right: 0px;" ).arg( air )
		: QString() );
	sheet.replace( QLatin1String( "MARGINFIRST" ), air > 0
		? QStringLiteral( " margin-left: %1px;" ).arg( air ) : QString() );""",
  "outer air only; MARGINFIRST replaces SEAM/BRAD"),

# ------------------------------------------------------- item 5, the menu arrow
E(UI, "after",
  """//! The air above and below the glyph line, the same for every button in the row.
static const int wwBarRowPad = 2;""",
  """
/*! THE MENU ARROW GETS ITS OWN COLUMN (bungo, 2026-09-11 05:4x, over
 *  scratchpad/ui3_20260910/images/cmp_zoom.png, verbatim: "That's fine, as long
 *  as the dropdown arrows do not intersect with the text / icons like on the
 *  screenshots you showed me").
 *
 *  Lane UI3 made the row's buttons the row's height, which finally SHOWED the
 *  dropdown arrow they had always had -- and on the two narrow icon-only
 *  buttons of the viewport header it was drawn on top of the glyph.
 *
 *  Measured, scratchpad/ui6_20260910/probe.cpp, from two renders of the same
 *  button (one with its menu taken away, so the columns that differ ARE the
 *  arrow):
 *
 *    padding-right   4 (res/style.qss's own)   6    8    10
 *    pivot dot                        gap 3    5    7     9
 *    grid / snap                      gap 0    2    4     6
 *
 *  The arrow is 9 columns wide and sits against the padding box's right edge,
 *  so every pixel of padding-right above res/style.qss's 4 is a pixel of clear
 *  background between the glyph and the arrow. 8 leaves FOUR on the tightest
 *  button in the row, which is the middle of the usable range rather than the
 *  2 the gate demands. The button grows by the same 4 px and the row does not
 *  move: this is horizontal, and nothing here touches a height. */
static const int wwBarRowArrowAir = 8;""",
  "the arrow's column, with the probe's map"),

E(UI, "replace",
  """	return QStringLiteral(
		"QMenuBar::item { min-height: %1px; padding-top: %2px; padding-bottom: %2px; }"
		"QToolButton { min-height: %1px; padding-top: %2px; padding-bottom: %2px; }"
		"QToolButton::menu-indicator { subcontrol-position: right center;"
		" subcontrol-origin: padding; }" )
		.arg( contentHeight ).arg( wwBarRowPad );""",
  """	/* The LAST rule is the only horizontal one in this sheet, and it is stated
	 * for the buttons that HAVE a menu and no others. `wwHasMenu` is a property
	 * wwAlignBarRow stamps after LOOKING -- popupMode is not the test, because a
	 * QToolButton's default mode is DelayedPopup and every button in the row
	 * carries it whether it has a menu or not. */
	return QStringLiteral(
		"QMenuBar::item { min-height: %1px; padding-top: %2px; padding-bottom: %2px; }"
		"QToolButton { min-height: %1px; padding-top: %2px; padding-bottom: %2px; }"
		"QToolButton::menu-indicator { subcontrol-position: right center;"
		" subcontrol-origin: padding; }"
		"QToolButton[wwHasMenu=\\"true\\"] { padding-right: %3px; }" )
		.arg( contentHeight ).arg( wwBarRowPad ).arg( wwBarRowArrowAir );""",
  "the arrow's column, in the row's own button sheet"),

E(UI, "replace",
  """			buttons.append( b );
			btnSaved.append( b->styleSheet() );""",
  """			buttons.append( b );
			/* MEASURED, never assumed: whether this button has a menu decides
			 * whether it needs the arrow's column (wwBarRowArrowAir). Set
			 * before the sheet is applied, so the first polish already sees
			 * it. A default action's menu counts -- that is how the render
			 * toolbar's display-toggles button carries one. */
			b->setProperty( "wwHasMenu",
				b->menu() != nullptr
					|| ( b->defaultAction() && b->defaultAction()->menu() != nullptr ) );
			btnSaved.append( b->styleSheet() );""",
  "stamp wwHasMenu on the row's buttons"),

# ------------------------------- item 2, the old Animation Manager is retired
E(UI, "replace",
  """	// Animation Manager
	dTimeline = new QDockWidget( tr( "Animation Manager" ), this );
	dTimeline->setObjectName( "TimelineDock" );
	timeline = new TimelineWidget( dTimeline );
	// timeline buttons must not take keyboard focus, otherwise Tab (edit-mode
	// toggle) cycles the transport buttons after clicking one
	for ( QAbstractButton * b : timeline->findChildren<QAbstractButton *>() )
		b->setFocusPolicy( Qt::NoFocus );
	timeline->setNif( nif );
	// The dock's second list source: the .hkx clips loaded into this
	// view's Scene, which have no block to be a NiControllerSequence row
	// (lane HKX3).
	timeline->setGLView( ogl );
	dTimeline->setWidget( timeline );
	/* Docked, area-restricted and hidden, like its seven siblings.
	 *
	 * It was added with neither setAllowedAreas nor hide(), so a fresh profile
	 * opened with the Animation Manager already spread across the bottom before
	 * any workspace was chosen -- and because it accepted all four areas,
	 * activateWorkspace needed a special case to keep it where it belongs.
	 * Bottom stays allowed because this one really does live there; the point is
	 * that the allowance is now declared rather than assumed.
	 */
	dTimeline->setAllowedAreas( Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea
		| Qt::RightDockWidgetArea );
	addDockWidget( Qt::BottomDockWidgetArea, dTimeline );
	dTimeline->hide();

	connect( timeline, &TimelineWidget::indexSelected, this, &NifSkope::select );
	connect( timeline, &TimelineWidget::timeChanged, ogl, &GLView::setSceneTime );
	connect( ogl, &GLView::sceneTimeChanged, timeline, &TimelineWidget::setTime );
	connect( this, &NifSkope::completeLoading, timeline, &TimelineWidget::refreshLater );

	// Two way sequence sync with the animation toolbar / scene
	connect( timeline, &TimelineWidget::sequenceActivated, ogl, &GLView::setSceneSequence );
	connect( ogl, &GLView::sequenceChanged, timeline, &TimelineWidget::setSequenceByName );

	connect( timeline, &TimelineWidget::isolateBlock, ogl, &GLView::setSoloBlock );

	// Loop / switch-animation toggles mirrored from the render toolbar
	timeline->addAnimActions( ui->aAnimLoop, ui->aAnimSwitch );
""",
  """	/* THE ANIMATION MANAGER DOCK IS GONE (lane UI6, 2026-09-11).
	 *
	 * bungo had already called it "old and outdated"; on 2026-09-10 21:1x he
	 * sent three screenshots of it -- two spin boxes with their steppers
	 * clipped to a sliver ("Do you see it?"), "we have a new standard for
	 * those sliders, don't you remember?" and "look at all this text clutter"
	 * -- and the Animation dock built below, gated 57/0 by animws.sh, is its
	 * replacement. So nothing is constructed here any more: no QDockWidget
	 * named TimelineDock, no TimelineWidget, no connections, and the
	 * Workspaces menu's "Animation" entry (which pointed at this dock) now
	 * opens the one below.
	 *
	 * `NifSkope::timeline` stays declared and stays null. Two old harnesses
	 * (WW_ANIMPLAY_TEST, WW_ROTKEY_TEST) drove this widget and now refuse by
	 * name rather than measuring nothing; deleting TimelineWidget itself is a
	 * separate lane, because src/ui/widgets/timeline.cpp also holds the icon
	 * set (tlMakeIcon / tlIconNames / tlWriteIconSheet) the whole window uses.
	 */
""",
  "the Animation Manager dock is not constructed"),

E(UI, "replace",
  """	connect( animws, &AnimWorkspace::playPauseRequested, timeline, &TimelineWidget::playPauseRequested );
""",
  "",
  "the transport no longer routes through the retired dock"),

E(UI, "replace",
  """	connect( dTimeline->toggleViewAction(), &QAction::triggered, [this]( bool on ) {
		if ( on && dTimeline->isFloating() ) {
			dTimeline->setFloating( false );
			addDockWidget( Qt::BottomDockWidgetArea, dTimeline );
		}
	} );
""",
  "",
  "no toggle action to re-dock: the dock is gone"),

E(UI, "replace",
  """	connect( timeline, &TimelineWidget::playPauseRequested, this, [this]( int dir ) {""",
  """	connect( animws, &AnimWorkspace::playPauseRequested, this, [this]( int dir ) {""",
  "the clock listens to the Animation dock directly"),

E(UI, "replace",
  """		timeline->setPlayingState( true, dir < 0 );
	} );""",
  """		animws->setPlayingState( true, dir < 0 );
	} );""",
  "...and reports back to it"),

E(UI, "replace",
  """	connect( ui->aAnimPlay, &QAction::toggled, timeline, [this]( bool on ) {
		timeline->setPlayingState( on, ogl->animationSpeed() < 0.0f );
	} );
	connect( ogl, &GLView::sequenceStopped, timeline, [this]() {
		timeline->setPlayingState( false, false );
	} );
""",
  "",
  "the Animation dock already has both of these connections"),

E(UI, "replace",
  """	connect( ogl, &GLView::transformCommitted, timeline, &TimelineWidget::keyNodeTransform );
""",
  "",
  "auto-key goes to the Animation dock, which is already connected"),

E(UI, "replace",
  """	aAutoKey->setToolTip( tr( "After a gizmo transform (G/R/S in the viewport), key it on the Animation Manager's transform lane at the playhead" ) );""",
  """	aAutoKey->setToolTip( tr( "After a gizmo transform (G/R/S in the viewport), key the bone on the Animation dock's dope sheet at the playhead" ) );""",
  "the tooltip names the surviving dock"),

E(UI, "replace",
  """			static const char * const dockNames[] = {
				"TimelineDock", "MatTexManagerDock", "CollisionManagerDock",""",
  """			static const char * const dockNames[] = {
				"AnimWorkspaceDock", "MatTexManagerDock", "CollisionManagerDock",""",
  "WW_DOCKS_TEST counts the surviving animation dock"),

E(UI, "replace",
  """			QDockWidget * tl = skope->findChild<QDockWidget *>( QStringLiteral( "TimelineDock" ) );""",
  """			QDockWidget * tl = skope->findChild<QDockWidget *>( QStringLiteral( "AnimWorkspaceDock" ) );""",
  "...and opens it on demand"),

E(UI, "replace",
  """					if ( !nif || !tl ) { log << "no model or timeline\\n"; break; }
					if ( skope->dTimeline ) { skope->dTimeline->show(); skope->dTimeline->raise(); }""",
  """					if ( !nif || !tl ) {
						// lane UI6 retired the Animation Manager dock this harness drove
						log << "WW_ROTKEY_TEST measured the Animation Manager dock's key "
							   "lanes; lane UI6 retired that dock on 2026-09-11, so there "
							   "is no timeline widget to insert a key on. RETIRED.\\n";
						break;
					}""",
  "WW_ROTKEY_TEST refuses in words instead of measuring nothing"),

E(UI, "replace",
  """					if ( !ogl || !tl || !ogl->getScene() ) { log << "no view/timeline\\n"; break; }""",
  """					if ( !ogl || !tl || !ogl->getScene() ) {
						// lane UI6 retired the Animation Manager dock this harness drove
						log << "WW_ANIMPLAY_TEST pressed the Animation MANAGER dock's Play; "
							   "lane UI6 retired that dock on 2026-09-11. The same question "
							   "for the Animation dock is animws.sh's transport. RETIRED.\\n";
						break;
					}""",
  "WW_ANIMPLAY_TEST refuses in words instead of measuring nothing"),

E(UI, "replace",
  """	const QList<QDockWidget *> workspaceManagers = {
		dTimeline, dMatMgr, dCollisionMgr, dRiggingMgr, dVertexPaintMgr, dUVMgr, dPoseMgr,
		dSkeletonMgr, dUnfuckMgr, dLodGen, dAnimWs	// (lane HKXEDIT2) the animation workspace
	};""",
  """	/* lane UI6: the Animation Manager dock that used to be first in this list
	 * is gone, and the Animation WORKSPACE takes its place -- the same position,
	 * so every stored workspace index still points where it did, and the dock
	 * that had no Workspaces entry of its own now has the one that matters. */
	const QList<QDockWidget *> workspaceManagers = {
		dAnimWs, dMatMgr, dCollisionMgr, dRiggingMgr, dVertexPaintMgr, dUVMgr, dPoseMgr,
		dSkeletonMgr, dUnfuckMgr, dLodGen
	};""",
  "the workspace manager list: the new dock takes the old one's seat"),

E(UI, "replace",
  """			const QList<QDockWidget *> managers = {
			dTimeline, dMatMgr, dCollisionMgr, dRiggingMgr, dVertexPaintMgr, dUVMgr, dPoseMgr,
			dSkeletonMgr, dUnfuckMgr, dLodGen, dAnimWs	// (lane HKXEDIT2)
		};""",
  """			const QList<QDockWidget *> managers = {
			dAnimWs, dMatMgr, dCollisionMgr, dRiggingMgr, dVertexPaintMgr, dUVMgr, dPoseMgr,
			dSkeletonMgr, dUnfuckMgr, dLodGen	// lane UI6: index 0 is the Animation dock
		};""",
  "Workspaces > Animation opens the new dock"),

E(UI, "replace",
  """		auto * timelineBtn = new QPushButton( tr( "Timeline dock…" ), animPanel );
		timelineBtn->setStyleSheet( boxQss );
		connect( timelineBtn, &QPushButton::clicked, this, [animMenu, this]() {
			animMenu->close();
			dTimeline->toggleViewAction()->trigger();
		} );""",
  """		auto * timelineBtn = new QPushButton( tr( "Animation dock…" ), animPanel );
		timelineBtn->setStyleSheet( boxQss );
		connect( timelineBtn, &QPushButton::clicked, this, [animMenu, this]() {
			animMenu->close();
			dAnimWs->toggleViewAction()->trigger();
		} );""",
  "the render panel's button opens the surviving dock"),

E(UI, "replace",
  """			{ "NiControllerManager",       "TimelineDock",         QT_TR_NOOP( "Animation" ) },
			{ "NiControllerSequence",      "TimelineDock",         QT_TR_NOOP( "Animation" ) },""",
  """			{ "NiControllerManager",       "AnimWorkspaceDock",    QT_TR_NOOP( "Animation" ) },
			{ "NiControllerSequence",      "AnimWorkspaceDock",    QT_TR_NOOP( "Animation" ) },""",
  "\"Open in Animation\" reaches the surviving dock"),

# --------------------------------------------------------------- nifskope.h
E("src/nifskope.h", "replace",
  """	QDockWidget * dTimeline;
""",
  "",
  "the retired dock's member goes with it (it had no initialiser)"),

E("src/nifskope.h", "replace",
  """	//! Animation timeline
	TimelineWidget * timeline = nullptr;""",
  """	/*! The old Animation Manager's widget. NEVER CONSTRUCTED since lane UI6
	 *  (2026-09-11) retired that dock; it stays declared and null so the
	 *  guarded call sites keep compiling, and two retired harnesses say so in
	 *  their own logs. Deleting TimelineWidget is a separate lane. */
	TimelineWidget * timeline = nullptr;""",
  "say why the member is permanently null"),


def run(apply_it):
    import io
    ok = True
    newfiles = {}
    cache = {}
    for path, mode, anchor, text, note in EDITS:
        if path not in cache:
            with open(path, "rb") as f:
                cache[path] = f.read().decode("utf-8")
        body = newfiles.get(path, cache[path])
        n = body.count(anchor)
        already = body.count(text) if (text and mode == "replace") else 0
        cr = cache[path].count("\r")
        print("%-26s %-8s anchor x%d  already x%d  CR %d  -- %s"
              % (path, mode, n, already, cr, note))
        if n != 1:
            ok = False
            continue
        if mode == "after":
            body = body.replace(anchor, anchor + text, 1)
        else:
            body = body.replace(anchor, text, 1)
        newfiles[path] = body
    print("\n%d of %d edits match once" % (
        sum(1 for p, m, a, t, nn in EDITS
            if (newfiles.get(p, cache[p]) is not None)), len(EDITS)))
    if not ok:
        print("REFUSED: an anchor did not match exactly once; nothing written")
        return 2
    if not apply_it:
        print("--check only, nothing written (pass --apply to write)")
        return 0
    for path, body in newfiles.items():
        before = cache[path]
        b = body.encode("utf-8")
        assert b.count(b"\r") == before.encode("utf-8").count(b"\r"), path
        with open(path, "wb") as f:
            f.write(b)
        print("wrote %s (%d -> %d bytes, CR %d)"
              % (path, len(before.encode("utf-8")), len(b), b.count(b"\r")))
    return 0


if __name__ == "__main__":
    sys.exit(run("--apply" in sys.argv))
