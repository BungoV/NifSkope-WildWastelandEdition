import sys

def patch(path, subs):
    b = open(path, 'rb').read()
    cr = b.count(b'\r')
    t = b.decode()
    for old, new in subs:
        n = t.count(old)
        if n != 1:
            sys.stderr.write('%s: anchor %d matches: %r\n' % (path, n, old[:70]))
            sys.exit(1)
        t = t.replace(old, new)
    out = t.encode()
    if out.count(b'\r') != cr:
        sys.stderr.write('%s: CR count moved %d -> %d\n' % (path, cr, out.count(b'\r')))
        sys.exit(1)
    open(path, 'wb').write(out)
    print(path, 'CR', out.count(b'\r'), 'LF', out.count(b'\n'), 'bytes', len(out))


# ---------------------------------------------------------------- nifskope.h
patch('src/nifskope.h', [
    ("	static bool wwHeadlessRun();",
     """	static bool wwHeadlessRun();

	//! Put one top-level window where a headless run's windows go: off the
	//! primary monitor, un-maximised, and at opacity 0 unless WW_WINDOW_VISIBLE=1.
	//! Called from the constructor (before any native window can exist), from
	//! createWindow after restoreUi has had its say, and from the application
	//! event filter for every other top-level window this process shows.
	static void wwPlaceHeadlessWindow( QWidget * w );"""),
])

# ------------------------------------------------------------- nifskope.cpp
patch('src/nifskope.cpp', [
    ("""	backgroundWorkspaceDocument = background;
	// Init UI
	ui->setupUi( this );""",
     """	backgroundWorkspaceDocument = background;

	/* PLACE A HEADLESS WINDOW BEFORE ANYTHING CAN CREATE A NATIVE ONE
	 * (bungo 2026-09-09: "Agent is launching nifskope on my main monitor,
	 * which is a no no").
	 *
	 * Doing it in createWindow alone was too late, and it was MEASURED too
	 * late: an EnumWindows probe at 25 ms saw a 426x306 OPAQUE window of this
	 * process on the PRIMARY monitor at t=371 ms -- Qt's default geometry,
	 * class Qt6111QWindowIcon, the application title and no filename -- which
	 * was gone by t=1403 ms and replaced by the real window, class
	 * Qt6111QWindowOwnDCIcon, at the asked-for place with layered alpha 0. The
	 * class change is the tell: the Windows plugin picks the window class by
	 * whether the surface needs its own DC, so realising the GL container
	 * (createWindowContainer, below) destroys the first native window and
	 * creates a second. The first one was created and shown while this widget
	 * still had its default geometry, because createWindow does not touch it
	 * until this constructor has returned.
	 *
	 * Setting the geometry and the opacity HERE means whatever native window
	 * is created carries them from birth. createWindow applies them again,
	 * because restoreUi()'s restoreGeometry() overwrites both. */
	if ( !background )
		wwPlaceHeadlessWindow( this );

	// Init UI
	ui->setupUi( this );"""),
])

# ---------------------------------------------------------- nifskope_ui.cpp
patch('src/nifskope_ui.cpp', [
    # The placement, factored out of createWindow so the constructor and the
    # event filter can use the same code.
    ("""NifSkope * NifSkope::createWindow( const QString & fname, bool background )""",
     """/*! Put one top-level window where a headless run's windows go.
 *
 *  Un-maximised (move() on a maximised window is a no-op on Windows), on a
 *  non-primary screen, shown without activating, and at opacity 0 unless
 *  WW_WINDOW_VISIBLE=1 asks for it back. Does nothing at all in an interactive
 *  session, so restart-free normal use is untouched.
 */
void NifSkope::wwPlaceHeadlessWindow( QWidget * w )
{
	if ( !w || !NifSkope::wwHeadlessRun() )
		return;
	w->setWindowState( w->windowState() & ~( Qt::WindowMaximized | Qt::WindowFullScreen ) );
	QString arm;
	w->move( wwHeadlessWindowOrigin( &arm ) );
	w->setAttribute( Qt::WA_ShowWithoutActivating, true );
	if ( qEnvironmentVariableIntValue( "WW_WINDOW_VISIBLE" ) != 1 )
		w->setWindowOpacity( 0.0 );
	wwHeadlessPlacementArm() = arm;
}

NifSkope * NifSkope::createWindow( const QString & fname, bool background )"""),

    # createWindow's own branch now goes through the same helper.
    ("""		if ( headlessRun ) {
			/* UN-MAXIMISE FIRST. restoreUi() restored whatever state the person
			 * last left, which is maximised, and move() on a maximised window
			 * is a no-op on Windows except for choosing the monitor. This one
			 * line is what makes every placement below mean anything -- the
			 * 2026-09-09 attempt that omitted it changed nothing measurable. */
			skope->setWindowState( skope->windowState()
				& ~( Qt::WindowMaximized | Qt::WindowFullScreen ) );
			QString arm;
			const QPoint origin = wwHeadlessWindowOrigin( &arm );
			skope->move( origin );
			skope->setAttribute( Qt::WA_ShowWithoutActivating, true );
			if ( headlessHidden ) {
				/* INVISIBLE, NOT ABSENT. A layered window at alpha 0 is still
				 * exposed, still gets paint events, and still has a GL context
				 * -- which is exactly what an off-screen window does not. */
				skope->setWindowOpacity( 0.0 );
			}
			skope->show();
			/* And again after show(): a platform window that did not exist when
			 * move() was called can be placed by the window manager instead.
			 * Cheap, and the log below reads back what actually took. */
			skope->move( origin );
			wwHeadlessPlacementArm() = arm;
		} else {""",
     """		if ( headlessRun ) {
			/* AGAIN, because restoreUi() above just overwrote both: its
			 * restoreGeometry() restores the position AND the maximised state
			 * the person last left, and move() on a maximised window is a
			 * no-op on Windows except for choosing the monitor. The 2026-09-09
			 * attempt that omitted the un-maximise changed nothing measurable.
			 *
			 * INVISIBLE, NOT ABSENT: a layered window at alpha 0 is still
			 * exposed, still gets paint events and still has a GL context --
			 * which is exactly what an off-screen window does not. */
			NifSkope::wwPlaceHeadlessWindow( skope );
			const QPoint origin = skope->pos();
			skope->show();
			/* And once more after show(): a platform window that did not exist
			 * when move() was called can be placed by the window manager
			 * instead. Cheap, and the log below reads back what actually took. */
			skope->move( origin );
			(void) headlessHidden;
		} else {"""),

    # The backstop: every OTHER top-level window this process shows.
    ("""bool NifSkope::eventFilter( QObject * o, QEvent * e )
{""",
     """bool NifSkope::eventFilter( QObject * o, QEvent * e )
{
	/* EVERY top-level window of a headless run, not just the main one
	 * (bungo 2026-09-09, "Agent is launching nifskope on my main monitor").
	 *
	 * createWindow places the document window and the constructor places it
	 * earlier still, but a headless route can raise a dialog, a floating dock
	 * or a tool window, and each of those is a top-level window of its own that
	 * neither of those two places ever sees. This clamps them all at the moment
	 * they are shown: same origin, same opacity, same rule. It is a BACKSTOP,
	 * not the mechanism -- a window clamped here has already been created, so
	 * anything that must never be created wrong is placed at its source. */
	if ( e->type() == QEvent::Show && NifSkope::wwHeadlessRun() ) {
		if ( QWidget * w = qobject_cast<QWidget *>( o ) ) {
			if ( w->isWindow() )
				NifSkope::wwPlaceHeadlessWindow( w );
		}
	}
"""),
])
