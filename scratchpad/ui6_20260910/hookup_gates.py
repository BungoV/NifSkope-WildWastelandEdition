#!/usr/bin/env python3
"""Lane UI6 -- the edits to the two SHARED GATE files, as a refusing script
(skill ww-anchored-hookup).

`src/wateruitest.cpp` and `tests/spells/water_ui.sh` were written by lane UI4,
extended by WATER8 and again by UI5, all in this session. Both are LF-only
(b.count(b'\\r') == 0). --check is the default and writes nothing.
"""

import sys

EDITS = []


def E(path, mode, anchor, text, note):
    EDITS.append((path, mode, anchor, text, note))


W = "src/wateruitest.cpp"
S = "tests/spells/water_ui.sh"

# ---------------------------------------------------------- the arrow instrument
E(W, "after",
  """//! A widget's rectangle in MAIN-WINDOW coordinates.""",
  """
/*! WHAT THE MENU ARROW IS CLEAR OF, in the button's own logical pixels.
 *
 *  bungo, 2026-09-11 05:4x, over scratchpad/ui3_20260910/images/cmp_zoom.png:
 *  "That's fine, as long as the dropdown arrows do not intersect with the text
 *  / icons like on the screenshots you showed me".
 *
 *  TWO RENDERS OF THE SAME BUTTON at the same PINNED size: one as it stands,
 *  one with its menu taken away. The columns that DIFFER between them are the
 *  arrow and nothing else -- the geometry cannot move, because the size is
 *  pinned across the pair -- and the glyph's last ink column is read off the
 *  second render, so both numbers come from one instrument.
 *
 *  NOT a grab: an auto-raise QToolButton paints no background of its own and
 *  QWidget::grab() hands back a pixmap Qt filled with WHITE, on which this
 *  theme's near-white glyphs are invisible (measured in the probe: background
 *  #efefef, icon #e6e8eb, seven levels apart). Rendering over a known dark fill
 *  is what makes the ink findable at all.
 *
 *  NOT a rect either: nothing a QToolButton can be asked for says where the
 *  style drew its menu-indicator.
 */
struct WwArrow
{
	int arrowFirst = -1;   //!< first column the arrow occupies
	int inkLast = -1;      //!< last column the icon or label occupies
	int cols = 0;          //!< how many columns the arrow occupies
	int gap = -99;         //!< clear columns between them
	bool ok = false;       //!< both were found, so `gap` means something
};

WwArrow arrowGap( QToolButton * b )
{
	WwArrow a;
	if ( !b || !b->isVisible() || b->width() < 8 || b->height() < 8 )
		return a;
	const int w = b->width(), h = b->height();
	const QSize mn = b->minimumSize(), mx = b->maximumSize();
	b->setFixedSize( w, h );
	QApplication::processEvents();

	auto shot = [&]() {
		QPixmap pm( w, h );
		pm.fill( QColor( wwSkinColor( "bgBar" ) ) );
		b->render( &pm, QPoint(), QRegion(), QWidget::DrawChildren );
		return pm.toImage().convertToFormat( QImage::Format_RGB32 );
	};
	const QImage with = shot();
	QMenu * ownMenu = b->menu();
	QAction * da = b->defaultAction();
	QMenu * actMenu = da ? da->menu() : nullptr;
	b->setMenu( nullptr );
	if ( actMenu )
		da->setMenu( nullptr );
	QApplication::processEvents();
	const QImage without = shot();
	if ( ownMenu )
		b->setMenu( ownMenu );
	if ( actMenu )
		da->setMenu( actMenu );
	b->setMinimumSize( mn );
	b->setMaximumSize( mx );
	QApplication::processEvents();
	if ( with.size() != without.size() || with.width() < 4 )
		return a;

	QHash<QRgb, int> hist;
	for ( int y = 0; y < without.height(); y++ )
		for ( int x = 0; x < without.width(); x++ )
			hist[without.pixel( x, y ) & 0x00ffffff]++;
	QRgb bg = 0;
	int best = -1;
	for ( auto it = hist.cbegin(); it != hist.cend(); ++it )
		if ( it.value() > best ) { best = it.value(); bg = it.key(); }
	auto lum = []( QRgb c ) {
		return 0.299 * qRed( c ) + 0.587 * qGreen( c ) + 0.114 * qBlue( c );
	};
	const double bgL = lum( bg );

	for ( int x = 0; x < with.width(); x++ ) {
		bool differs = false, ink = false;
		for ( int y = 0; y < with.height(); y++ ) {
			if ( ( with.pixel( x, y ) & 0x00ffffff ) != ( without.pixel( x, y ) & 0x00ffffff ) )
				differs = true;
			if ( qAbs( lum( without.pixel( x, y ) & 0x00ffffff ) - bgL ) > 40.0 )
				ink = true;
		}
		if ( differs ) {
			if ( a.arrowFirst < 0 )
				a.arrowFirst = x;
			a.cols++;
		}
		if ( ink )
			a.inkLast = x;
	}
	a.ok = ( a.arrowFirst >= 0 && a.inkLast >= 0 );
	if ( a.ok )
		a.gap = a.arrowFirst - a.inkLast - 1;
	return a;
}

//! Does this button carry a menu, so that the style draws an arrow on it?
bool hasMenu( QToolButton * b )
{
	return b && ( b->menu() != nullptr
		|| ( b->defaultAction() && b->defaultAction()->menu() != nullptr ) );
}
""",
  "the arrow instrument, beside the strip's and the menu bar's"),

# --------------------------------------------------------------- group S: the gap
E(W, "replace",
  """				auto atAir = []( const Five & f, int want ) {
					return f.real && qAbs( f.top - want ) <= 1 && qAbs( f.bottom - want ) <= 1
						&& qAbs( f.left - want ) <= 1 && qAbs( f.right - want ) <= 1
						&& qAbs( f.gapMin - want ) <= 1 && qAbs( f.gapMax - want ) <= 1;
				};""",
  """				/* AMENDED BY LANE UI6. Four of the five distances are the air;
				 * the fifth is ZERO. bungo, 2026-09-10 21:0x, over the zoomed
				 * screenshot of the strip UI4 had just gapped, verbatim: "Why
				 * are they separated?" -- a segmented control is ONE element,
				 * and the air belongs outside its box, never inside it. */
				auto atAir = []( const Five & f, int want ) {
					return f.real && qAbs( f.top - want ) <= 1 && qAbs( f.bottom - want ) <= 1
						&& qAbs( f.left - want ) <= 1 && qAbs( f.right - want ) <= 1
						&& f.gapMin == 0 && f.gapMax == 0;
				};
				auto joined = []( const Five & f ) {
					return f.real && f.gapMin == 0 && f.gapMax == 0;
				};""",
  "the fifth distance is 0, and it has its own predicate"),

E(W, "replace",
  """				check( *st, QStringLiteral( "(S5) every pair of segments is %1..%2 px apart "
					"(want %3)" ).arg( f.gapMin ).arg( f.gapMax ).arg( air ),
					f.real && qAbs( f.gapMin - air ) <= 1 && qAbs( f.gapMax - air ) <= 1 );""",
  """				check( *st, QStringLiteral( "(S5) the segments TOUCH: every pair is %1..%2 px "
					"apart (want 0)" ).arg( f.gapMin ).arg( f.gapMax ), joined( f ) );""",
  "S5 reads 0, not the air"),

E(W, "after",
  """				check( *st, QStringLiteral( "(S floor) ...and taking it away puts all five "
					"back at %1, so the pictures below are the shipped state" ).arg( air ),
					atAir( back, air ) );""",
  """
				/* THE SECOND FLOOR, and the one this lane exists for.
				 *
				 * The floor above puts back the 18:25:20 FLUSH strip, which
				 * moves all five distances at once -- so it would still fire if
				 * the gap alone had been left at UI4's 4. This one puts back
				 * UI4's SEPARATED sheet and nothing else: `margin-left` on every
				 * segment, its own border, its own rounded corners. S5's own
				 * predicate must go RED with the gap back at 4, and green again
				 * when it is taken away. Without it, S5 is a check that has only
				 * ever been seen green. */
				const QString apart = QStringLiteral(
					"QTabBar::tab { margin-left: %1px; border-left: 1px solid %2;"
					" border-radius: 3px; }" ).arg( air ).arg( wwSkinColor( "border" ) );
				tabs->setStyleSheet( saved + apart );
				for ( int i = 0; i < 3; i++ )
					QApplication::processEvents();
				const Five sep = measure( nullptr );
				say( *st, QStringLiteral( "  S floor: with UI4's SEPARATED strip put back, the "
					"gap reads %1..%2 (top %3, left %4)" )
					.arg( sep.gapMin ).arg( sep.gapMax ).arg( sep.top ).arg( sep.left ) );
				check( *st, QStringLiteral( "(S5 floor) the SAME gap test goes red on the strip "
					"bungo asked about (%1..%2 apart, want 0)" )
					.arg( sep.gapMin ).arg( sep.gapMax ), !joined( sep ) );

				tabs->setStyleSheet( saved );
				for ( int i = 0; i < 3; i++ )
					QApplication::processEvents();
				const Five rejoined = measure( nullptr );
				say( *st, QStringLiteral( "  S floor: restored, the gap reads %1..%2 again" )
					.arg( rejoined.gapMin ).arg( rejoined.gapMax ) );
				check( *st, QStringLiteral( "(S5 floor) ...and taking it away joins the segments "
					"again (%1..%2)" ).arg( rejoined.gapMin ).arg( rejoined.gapMax ),
					joined( rejoined ) );""",
  "the gap's OWN floor, live, in the same run"),

# ------------------------------------------------------------------- group R4
E(W, "replace",
  """					.arg( sels ).arg( mins ).arg( padT ).arg( padB ),
					mins >= 2 && padT == mins && padB == mins && sels == mins + 1 );""",
  """					.arg( sels ).arg( mins ).arg( padT ).arg( padB ),
					mins >= 2 && padT == mins && padB == mins && sels == mins + 2 );""",
  "R4: the sheet has one more selector, the arrow's column"),

E(W, "replace",
  """				check( *st, QStringLiteral( "(R4) ...and nothing horizontal, so each button keeps "
					"its own width" ),
					!sheet.contains( QStringLiteral( "padding-left" ) )
						&& !sheet.contains( QStringLiteral( "padding-right" ) )
						&& sheet.count( QStringLiteral( "padding:" ) ) == 0 );""",
  """				/* AMENDED BY LANE UI6. This check used to read "nothing
				 * horizontal". bungo, 2026-09-11 05:4x: "That's fine, as long as
				 * the dropdown arrows do not intersect with the text / icons
				 * like on the screenshots you showed me" -- so the row now
				 * states exactly ONE horizontal rule, the column the menu arrow
				 * needs, and only for the buttons that HAVE a menu. Everything
				 * else horizontal is still forbidden, and the count pins that
				 * there is one such rule and not two. */
				check( *st, QStringLiteral( "(R4) ...and the only horizontal rule is the menu "
					"arrow's own column (%1 padding-right, %2 [wwHasMenu] selector, 0 "
					"padding-left, 0 shorthand padding)" )
					.arg( sheet.count( QStringLiteral( "padding-right:" ) ) )
					.arg( sheet.count( QStringLiteral( "QToolButton[wwHasMenu=\\"true\\"]" ) ) ),
					!sheet.contains( QStringLiteral( "padding-left" ) )
						&& sheet.count( QStringLiteral( "padding:" ) ) == 0
						&& sheet.count( QStringLiteral( "padding-right:" ) ) == 1
						&& sheet.count( QStringLiteral( "QToolButton[wwHasMenu=\\"true\\"]" ) ) == 1 );""",
  "R4: one horizontal rule, the arrow's, and only on menu buttons"),

# -------------------------------------------------------------------- group A
E(W, "after",
  """			// R5: the search row's content top, printed pass or fail""",
  """
			/* =============================================================
			 *  A -- THE DROPDOWN ARROWS DO NOT TOUCH THE GLYPHS
			 *
			 *  bungo, 2026-09-11 05:4x, over
			 *  scratchpad/ui3_20260910/images/cmp_zoom.png, verbatim: "That's
			 *  fine, as long as the dropdown arrows do not intersect with the
			 *  text / icons like on the screenshots you showed me".
			 *
			 *  Every button in the row that HAS a menu is measured, named and
			 *  printed -- a count cannot say which button is the tight one, and
			 *  the two tight ones are the whole complaint.
			 * ============================================================= */
			say( *st, QStringLiteral( "--- A: the menu arrows' air ---" ) );
			if ( !compact ) {
				skip( *st, QStringLiteral( "A1..A2 measure the compact row's button sheet; this "
					"profile has UI/CompactTopBars OFF, so the arrows are drawn against "
					"BUILD9's buttons and these gates do not describe them" ) );
			} else {
				QList<QToolButton *> menuBtns;
				QStringList menuNames;
				QList<QWidget *> barsWithButtons;
				barsWithButtons << header << tFile << tLOD << tView;
				for ( QWidget * w : barsWithButtons ) {
					if ( !w )
						continue;
					for ( QToolButton * b : w->findChildren<QToolButton *>() ) {
						if ( !b->isVisible()
							 || b->objectName() == QLatin1String( "qt_toolbar_ext_button" ) )
							continue;
						if ( !hasMenu( b ) )
							continue;
						menuBtns.append( b );
						QString n = b->objectName();
						if ( n.isEmpty() )
							n = b->text();
						if ( n.isEmpty() )
							n = QStringLiteral( "<icon %1x%2 at x %3>" )
								.arg( b->width() ).arg( b->height() )
								.arg( b->mapTo( skope, QPoint( 0, 0 ) ).x() );
						menuNames.append( n );
					}
				}

				/* ONE measurement function for the table, the verdict and the
				 * floor, so the floor exercises the code the verdict comes from
				 * (the R3 / S pattern). */
				auto sweep = [&]( int * worstOut, QString * worstName, int * readOut,
								  bool print ) {
					int worst = 1 << 20, read = 0;
					QString who;
					for ( int i = 0; i < menuBtns.size(); i++ ) {
						const WwArrow a = arrowGap( menuBtns.at( i ) );
						if ( print )
							say( *st, QStringLiteral( "  A %1: %2x%3; arrow %4 col(s) from %5; "
								"glyph last ink %6; GAP %7%8" )
								.arg( menuNames.at( i ), -26 )
								.arg( menuBtns.at( i )->width() ).arg( menuBtns.at( i )->height() )
								.arg( a.cols ).arg( a.arrowFirst ).arg( a.inkLast ).arg( a.gap )
								.arg( a.ok ? QString()
										   : QStringLiteral( "   (nothing to compare)" ) ) );
						if ( !a.ok )
							continue;
						read++;
						if ( a.gap < worst ) {
							worst = a.gap;
							who = menuNames.at( i );
						}
					}
					if ( read == 0 )
						worst = -99;
					if ( worstOut ) *worstOut = worst;
					if ( worstName ) *worstName = who;
					if ( readOut ) *readOut = read;
				};

				int worst = 0, read = 0;
				QString who;
				sweep( &worst, &who, &read, true );
				say( *st, QStringLiteral( "  A: %1 menu button(s) in the row, %2 readable; the "
					"skin states padding-right for them and nothing else horizontal" )
					.arg( menuBtns.size() ).arg( read ) );
				check( *st, QStringLiteral( "(A floor) the row carries menu buttons to measure "
					"and both renders differed on them (%1 found, %2 readable)" )
					.arg( menuBtns.size() ).arg( read ),
					menuBtns.size() >= 4 && read >= 4 );
				check( *st, QStringLiteral( "(A1) every menu button's arrow is at least 2 px "
					"clear of its glyph (worst %1 on \\"%2\\", of %3 read)" )
					.arg( worst ).arg( who ).arg( read ),
					read >= 4 && worst >= 2 );

				/* THE FLOOR THAT FIRES, both halves in one run. res/style.qss
				 * gives every toolbar button `padding: 2px 4px`, and 4 is
				 * exactly what the arrows had before this lane: putting it back
				 * live is the 05:58:21 window, and the SAME sweep must find a
				 * button whose arrow is under 2 px from its glyph. */
				const QString noAir = QStringLiteral(
					"QToolButton[wwHasMenu=\\"true\\"] { padding-right: 4px; }" );
				QList<QWidget *> victims;
				QStringList victimSheets;
				for ( QWidget * w : barsWithButtons )
					if ( w )
						victims.append( w );
				for ( QToolButton * b : menuBtns )
					victims.append( b );
				for ( QWidget * w : victims )
					victimSheets.append( w->styleSheet() );
				for ( int i = 0; i < victims.size(); i++ )
					victims.at( i )->setStyleSheet( victimSheets.at( i ) + noAir );
				for ( int i = 0; i < 3; i++ )
					QApplication::processEvents();
				int sWorst = 0, sRead = 0;
				QString sWho;
				sweep( &sWorst, &sWho, &sRead, true );
				say( *st, QStringLiteral( "  A floor: with res/style.qss's own 4 px put back "
					"(the 05:58:21 window) the worst is %1 px on \\"%2\\", of %3 read" )
					.arg( sWorst ).arg( sWho ).arg( sRead ) );
				check( *st, QStringLiteral( "(A1 floor) the SAME test goes red on the sheet that "
					"shipped at 05:58:21 (worst %1 on \\"%2\\")" ).arg( sWorst ).arg( sWho ),
					sRead >= 4 && sWorst < 2 );

				for ( int i = 0; i < victims.size(); i++ )
					victims.at( i )->setStyleSheet( victimSheets.at( i ) );
				for ( int i = 0; i < 3; i++ )
					QApplication::processEvents();
				int rWorst = 0, rRead = 0;
				QString rWho;
				sweep( &rWorst, &rWho, &rRead, false );
				say( *st, QStringLiteral( "  A floor: restored, the worst is %1 px again" )
					.arg( rWorst ) );
				check( *st, QStringLiteral( "(A1 floor) ...and taking it away gives every arrow "
					"its air back (worst %1), so the picture below is the shipped state" )
					.arg( rWorst ), rRead >= 4 && rWorst >= 2 );

				/* A2: the rule reached the right buttons and no others. The
				 * property is what the sheet selects on, so a button that has a
				 * menu and does NOT carry it would be missed in silence. */
				int stamped = 0, wrongly = 0, allBtns = 0;
				for ( QWidget * w : barsWithButtons ) {
					if ( !w )
						continue;
					for ( QToolButton * b : w->findChildren<QToolButton *>() ) {
						if ( !b->isVisible()
							 || b->objectName() == QLatin1String( "qt_toolbar_ext_button" ) )
							continue;
						allBtns++;
						const bool prop = b->property( "wwHasMenu" ).toBool();
						if ( hasMenu( b ) && prop )
							stamped++;
						else if ( !hasMenu( b ) && prop )
							wrongly++;
					}
				}
				say( *st, QStringLiteral( "  A2: %1 buttons in the row, %2 with a menu, %3 of "
					"those stamped wwHasMenu, %4 stamped without one" )
					.arg( allBtns ).arg( menuBtns.size() ).arg( stamped ).arg( wrongly ) );
				check( *st, QStringLiteral( "(A2) every button with a menu carries wwHasMenu "
					"(%1 of %2)" ).arg( stamped ).arg( menuBtns.size() ),
					menuBtns.size() >= 4 && stamped == menuBtns.size() );
				check( *st, QStringLiteral( "(A2 floor) ...and no button WITHOUT a menu carries "
					"it (%1 of %2), so the stamp is a measurement and not a blanket" )
					.arg( wrongly ).arg( allBtns - menuBtns.size() ),
					wrongly == 0 && allBtns > menuBtns.size() );
			}
""",
  "group A: the arrows' air, with both halves of its live floor"),

# ----------------------------------------------------------------- the spell
E(S, "replace",
  """	"(L8) the LOD strip's first segment" "(L8 floor)" \\""",
  """	"(L8) the LOD strip's first segment" "(L8 floor) ...and the LEFT strip" \\
	"(L8) the LOD strip's two segments TOUCH" "(L8 floor) the SAME gap test" \\
	"(L8 floor) ...and taking it away" \\""",
  "the spell reads L8's missing half back by name"),

E(S, "replace",
  """	"(R4) the skin states the height" "(R4) ...and nothing horizontal" \\""",
  """	"(R4) the skin states the height" "(R4) ...and the only horizontal rule" \\""",
  "R4's renamed check (its text is an interface, UI5's own mistake)"),

E(S, "replace",
  """	"(S4) the last segment" "(S5) every pair of segments" "(S6) and the ROW did not move" \\
	"(S floor) the SAME five go red" "(S floor) ...and taking it away" \\""",
  """	"(S4) the last segment" "(S5) the segments TOUCH" "(S6) and the ROW did not move" \\
	"(S floor) the SAME five go red" "(S floor) ...and taking it away" \\
	"(S5 floor) the SAME gap test" "(S5 floor) ...and taking it away" \\
	"(A floor) the row carries menu buttons" "(A1) every menu button's arrow" \\
	"(A1 floor) the SAME test goes red" "(A1 floor) ...and taking it away" \\
	"(A2) every button with a menu" "(A2 floor) ...and no button WITHOUT a menu" \\""",
  "the spell reads S5's own floor and the whole of group A by name"),

E(S, "replace",
  """#   S5  every pair of segments is 4 px apart""",
  """#   S5  the segments TOUCH -- 0 px apart (lane UI6; bungo, "Why are they
#       separated?"). The FOUR OUTER distances are still 4.""",
  "the spell's own header says what S5 now means"),

E(S, "replace",
  """echo "checks run: ${COUNT:-none} (floor 62)\"""",
  """# LANE UI6 re-counted it against UI5's MEASURED 76: group S gains BOTH halves
# of its own gap floor (+2), group L gains L8's missing half and its two floor
# halves (+3), and the new group A -- the menu arrows' air -- adds 6 (its
# readable floor, A1, BOTH halves of A1's live floor, A2 and A2's floor). So a
# full run reads 87 and the smallest run that is still WORKING (no pictures
# asked for: 67 + 11) reads 78. 72 is six below that and well above UI5's 76
# only when the pictures are on, so a build that lost group A entirely goes red
# on the count alone however the spell is called. All 6 of group A skip by name
# when UI/CompactTopBars is off, exactly as group R's do.
echo "checks run: ${COUNT:-none} (floor 72)\"""",
  "the count floor rises with the new checks, with the arithmetic"),

E(S, "replace",
  """	*) [ "$COUNT" -ge 62 ] || { echo "FAIL: only $COUNT checks ran, floor is 62"; fails=$((fails+1)); } ;;""",
  """	*) [ "$COUNT" -ge 72 ] || { echo "FAIL: only $COUNT checks ran, floor is 72"; fails=$((fails+1)); } ;;""",
  "...and the assertion that reads it"),


def run(apply_it):
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
    print("\n%d edits" % len(EDITS))
    if not ok:
        print("REFUSED: an anchor did not match exactly once; nothing written")
        return 2
    if not apply_it:
        print("--check only, nothing written (pass --apply to write)")
        return 0
    for path, body in newfiles.items():
        before = cache[path].encode("utf-8")
        b = body.encode("utf-8")
        assert b.count(b"\r") == before.count(b"\r"), path
        with open(path, "wb") as f:
            f.write(b)
        print("wrote %s (%d -> %d bytes, CR %d)" % (path, len(before), len(b), b.count(b"\r")))
    return 0


if __name__ == "__main__":
    sys.exit(run("--apply" in sys.argv))
