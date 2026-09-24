#!/usr/bin/env python3
"""Lane UI6 -- the THREE GATE DEFECTS the first run of these checks found.

No application code is touched: the three files below are harnesses. Each
defect and its cost is in the report's section 6.

  D1 src/wateruitest.cpp      group A pinned and unpinned each button in turn,
                              so the toolbar re-laid out 13 times per sweep and
                              the third sweep read one button 1 px narrow.
                              Pin them ALL, sweep, unpin them all.
  D2 src/hkxanimuitest.cpp    the Animation dock rebuilds its list on a 50 ms
                              TIMER (refreshLater), which processEvents() does
                              not fire; every check that reads a row was reading
                              a stale list. And its splitter is HORIZONTAL, so
                              the "find the vertical splitter" line found none.
  D3 src/animworkspacetest.cpp  gate (j) -- the panel-style group, which is what
                              bungo's scrub-field ruling is measured by -- lived
                              inside the sequence-NIF branch and has NEVER RUN
                              on this machine, because the fixture NIF has no
                              NiControllerSequence and the branch skips.
"""

import re


def ed(path, pairs):
    b = open(path, 'rb').read().decode('utf-8')
    cr = b.count('\r')
    for a, t in pairs:
        c = b.count(a)
        assert c == 1, (path, c, a[:90])
        b = b.replace(a, t, 1)
    out = b.encode('utf-8')
    assert out.count(b'\r') == cr
    open(path, 'wb').write(out)
    print('ok %s %d bytes CR %d' % (path, len(out), cr))


# ---------------------------------------------------------------- D1
ed('src/wateruitest.cpp', [
("""	const int w = b->width(), h = b->height();
	const QSize mn = b->minimumSize(), mx = b->maximumSize();
	b->setFixedSize( w, h );
	QApplication::processEvents();""",
 """	const int w = b->width(), h = b->height();
	/* The caller has already pinned every button in the row (see pinAll below).
	 * Pinning and unpinning them ONE AT A TIME makes the toolbar re-lay out
	 * thirteen times per sweep, and the sweep after that reads a button a pixel
	 * narrow -- which is what the first run of this group went red on, in its
	 * restore half, on a window that was correct. */
	const QSize mn = b->minimumSize(), mx = b->maximumSize();
	b->setFixedSize( w, h );
	QApplication::processEvents();"""),
("""//! Does this button carry a menu, so that the style draws an arrow on it?
bool hasMenu( QToolButton * b )
{
	return b && ( b->menu() != nullptr
		|| ( b->defaultAction() && b->defaultAction()->menu() != nullptr ) );
}""",
 """//! Does this button carry a menu, so that the style draws an arrow on it?
bool hasMenu( QToolButton * b )
{
	return b && ( b->menu() != nullptr
		|| ( b->defaultAction() && b->defaultAction()->menu() != nullptr ) );
}

/*! Freeze the whole row's geometry for the length of a sweep, and give it back.
 *
 *  One layout state for thirteen measurements. Without it each arrowGap() call
 *  re-lays out the bar, the next button is measured mid-relayout, and the third
 *  sweep of a correct window reads one button a pixel narrow. Returns the sizes
 *  to hand back to unpinAll().
 */
QList<QPair<QSize, QSize>> pinAll( const QList<QToolButton *> & btns )
{
	QList<QPair<QSize, QSize>> saved;
	for ( QToolButton * b : btns ) {
		saved.append( qMakePair( b->minimumSize(), b->maximumSize() ) );
		b->setFixedSize( b->width(), b->height() );
	}
	for ( int i = 0; i < 4; i++ )
		QApplication::processEvents();
	return saved;
}

void unpinAll( const QList<QToolButton *> & btns, const QList<QPair<QSize, QSize>> & saved )
{
	for ( int i = 0; i < btns.size() && i < saved.size(); i++ ) {
		btns.at( i )->setMinimumSize( saved.at( i ).first );
		btns.at( i )->setMaximumSize( saved.at( i ).second );
	}
	for ( int i = 0; i < 6; i++ )
		QApplication::processEvents();
}"""),
("""				auto sweep = [&]( int * worstOut, QString * worstName, int * readOut,
								  bool print ) {
					int worst = 1 << 20, read = 0;
					QString who;""",
 """				auto sweep = [&]( int * worstOut, QString * worstName, int * readOut,
								  bool print ) {
					int worst = 1 << 20, read = 0;
					QString who;
					// ONE layout state for the whole sweep (see pinAll)
					const QList<QPair<QSize, QSize>> pinned = pinAll( menuBtns );"""),
("""					if ( read == 0 )
						worst = -99;
					if ( worstOut ) *worstOut = worst;""",
 """					unpinAll( menuBtns, pinned );
					if ( read == 0 )
						worst = -99;
					if ( worstOut ) *worstOut = worst;"""),
("""				int rWorst = 0, rRead = 0;
				QString rWho;
				sweep( &rWorst, &rWho, &rRead, false );""",
 """				int rWorst = 0, rRead = 0;
				QString rWho;
				sweep( &rWorst, &rWho, &rRead, true );"""),
])

# ---------------------------------------------------------------- D2
ed('src/hkxanimuitest.cpp', [
("""//! The text of one row of that list ("" when the row is not there).""",
 """/*! Let the dock rebuild its list, and WAIT FOR IT.
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

//! The text of one row of that list ("" when the row is not there)."""),

("""	const QString said = hub->loadFiles( ogl, { st.clipPath }, true );
	qApp->processEvents();""",
 """	const QString said = hub->loadFiles( ogl, { st.clipPath }, true );
	wwSettle( tl );"""),

("""	const bool dropped = hub->unload( ogl, clipName );
	qApp->processEvents();""",
 """	const bool dropped = hub->unload( ogl, clipName );
	wwSettle( tl );"""),

("""		const QString why = hub->loadFiles( ogl, { st.refusePath }, false );
		qApp->processEvents();""",
 """		const QString why = hub->loadFiles( ogl, { st.refusePath }, false );
		wwSettle( tl );"""),

("""	QSplitter * vsplit = nullptr;
	for ( QSplitter * s : tl->findChildren<QSplitter *>() ) {
		if ( s->orientation() == Qt::Vertical )
			vsplit = s;
	}""",
 """	/* The Animation dock's splitter is HORIZONTAL (the list column beside the
	 * dope sheet) where the retired dock's was vertical, so "find the vertical
	 * one" found none and this whole check went red on a correct window. It is
	 * asked for BY NAME now, which is what the rest of this file already does. */
	QSplitter * vsplit = tl->findChild<QSplitter *>( QStringLiteral( "AnimWsSplitter" ) );"""),
])

# ---------------------------------------------------------------- D3
b = open('src/animworkspacetest.cpp', 'rb').read().decode('utf-8')
cr = b.count('\r')
start = b.index('\t\t\t\t\t\t// ---- (j) panel style\n')
end = b.index('\t\t\t\t\t\tfinish();\n', start)
block = b[start:end]
assert block.count('(j) number fields carry the scrub stamp') == 1

lam = ('\t\t\t/* GATE (j) HAS NEVER RUN ON THIS MACHINE (found by lane UI6,\n'
       '\t\t\t * 2026-09-11). It lived inside the sequence-NIF branch below, and\n'
       '\t\t\t * the fixture that branch is given -- 10mmPistol.nif -- has no\n'
       '\t\t\t * NiControllerSequence, so every run since lane HKXEDIT2 has taken\n'
       '\t\t\t * the SKIP and finished without asking a single panel-style\n'
       '\t\t\t * question. The scrub-field count bungo ruled on was among them.\n'
       '\t\t\t * It is a lambda now and BOTH branches call it. */\n'
       '\t\t\tauto panelStyle = [st, ws]() {\n'
       + block +
       '\t\t\t};\n\n')

b = b[:start] + '\t\t\t\t\t\tpanelStyle();\n' + b[end:]

# insert the lambda just before the sequence-NIF branch
anchor = "\t\t\tif ( !st->seqNif.isEmpty() && QFile::exists( st->seqNif ) ) {"
assert b.count(anchor) == 1
b = b.replace(anchor, lam + anchor, 1)

# capture it in the singleShot lambda that runs gate (i)
a2 = "QTimer::singleShot( 2500, skope, [skope, st, ws, ogl, grp, finish]() {"
assert b.count(a2) == 1
b = b.replace(a2, "QTimer::singleShot( 2500, skope, [skope, st, ws, ogl, grp, finish, panelStyle]() {", 1)

# ...and call it on the two paths that skip gate (i)
a3 = """					skip( *st, QStringLiteral( "(i) %1 has no NiControllerSequence to test with" ).arg( st->seqNif ) );"""
assert b.count(a3) == 1
b = b.replace(a3, a3 + "\n\t\t\t\t\tpanelStyle();", 1)
a4 = """				skip( *st, QStringLiteral( "(i) no sequence NIF at '%1'" ).arg( st->seqNif ) );"""
assert b.count(a4) == 1
b = b.replace(a4, a4 + "\n\t\t\t\tpanelStyle();", 1)

out = b.encode('utf-8')
assert out.count(b'\r') == cr
open('src/animworkspacetest.cpp', 'wb').write(out)
print('ok src/animworkspacetest.cpp %d bytes CR %d' % (len(out), cr))
