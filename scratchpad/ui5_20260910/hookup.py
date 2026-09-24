"""Lane UI5 -- the hook-up, as a REFUSING script (skill ww-anchored-hookup).

Every file this lane changes is a file another lane could be reading or writing
while this lane is code-only, so nothing is edited by hand. One table, exact
anchors that must match ONCE in the file's real bytes, the line ending inside
the anchor, a CR byte count asserted unchanged, and --check as the default so
the script can be run and quoted without writing anything.

WHY IT MATTERS HERE, specifically. Lane WATER8-GATE is alive in this tree while
this lane is written. It touches no source (its own brief: "Touch NO source, QSS
or .pro file"), but its gate G4 is an exe-newer sweep over
`git status --porcelain -- src res tools tests` that must print no STALE line --
so touching ANY tracked file before its DONE marker appears would put a false
red in another lane's verdict. That is why this exists as a script and why
--apply is not run until scratchpad/water8_20260910/DONE is on disk.

    python scratchpad/ui5_20260910/hookup.py            # check, writes nothing
    python scratchpad/ui5_20260910/hookup.py --apply    # all five edits at once
"""

import sys
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


# ----------------------------------------------------------------- wwskin.h
SKIN_ANCHOR = (
    "QString wwBarRowButtonQss( int contentHeight );\n"
    "QString wwBarRowBoxQss();\n"
    "int wwBarRowButtonContent();\n"
    "int wwBarRowButtonOverhead();\n"
)

SKIN_ADD = """
/*! ...AND THE TITLES IN THE MENU BAR SIT ON THE ROW'S CENTRE LINE (lane UI5,
 * 2026-09-10). bungo, verbatim: "Also, please center file / view / spells /
 * options / help buttons, top left".
 *
 * Lane UI3 put the menu BAR in the row and said in its own handoff that the
 * ITEMS were not made the row's height, only the bar. They were not: measured
 * on the shipped 20:45:47 window (its own in-application grab, read by
 * scratchpad/ui5_20260910/measure_before.py), the five titles' text sat at
 * y 6..16 in a 35 px row whose centre line is 17.0 -- six pixels high, which is
 * what his screenshot shows.
 *
 * `wwBarRowMenuItemQss( rowHeight, itemContent )` states the air above and below
 * one menu title and nothing else: `itemContent` is the item's own height with
 * no vertical padding, and the row's spare pixels are split evenly over and
 * under it. Nothing horizontal, so the 8 px `res/style.qss` gives a title on
 * either side is left alone -- the same rule `wwBarRowButtonQss` follows.
 *
 * WHY PADDING AND NOT min-height AND NOT A MARGIN, measured outside the
 * application over 30 cases (scratchpad/ui5_20260910/probe.cpp, probe_out2.txt):
 *
 *   - `QMenuBar::item { min-height }` IS NOT CONSULTED ON THIS PATH. Fifteen
 *     cases from 22 to 36 px leave the item exactly 20 px tall. The rule has
 *     been in the row's sheet since lane WATER7 and has never moved a pixel.
 *   - a `::item` MARGIN does move the item, but `actionGeometry()` grows with
 *     it, so the rect lies about where the item is drawn -- exactly as
 *     `QTabBar::tabRect()` did for lane UI4. A margin therefore cannot be gated
 *     on geometry and is not used.
 *   - the two vertical PADDINGS are consulted and leave the rect honest. The
 *     item's painted box becomes the whole row (offset 0.0 from the centre
 *     line) and the title's text lands within half a pixel of it (+0.5). No
 *     integer split does better: the ink sits one pixel below its own content's
 *     centre, so 8/11 reads -0.5 and 9/10 reads +0.5.
 *
 * `wwAlignBarRow` MEASURES `itemContent` rather than typing 16: it applies the
 * row's sheet once with both vertical paddings zeroed and reads the item's
 * height back, which needs `ensurePolished()` and no event loop (probe case E)
 * -- and that is what makes it usable, because the row is aligned during
 * construction where no events can be pumped.
 * `wwBarRowMenuItemContent()`, `wwBarRowMenuItemPadTop()` and
 * `wwBarRowMenuItemPadBottom()` read the three numbers back, for the gate and
 * for the log; the two paddings are -1 until the row has been aligned.
 *
 * THE WAY BACK is the same single key. `UI/CompactTopBars` false returns an
 * empty string here too -- one reader, `wwCompactTopBars()`, shared with both
 * other sheets -- so the menu bar keeps `res/style.qss` alone and the titles sit
 * where they did before lane UI3. A row height or a content of zero also
 * returns an empty string: no row, no rule. */
QString wwBarRowMenuItemQss( int rowHeight, int itemContent );
int wwBarRowMenuItemContent();
int wwBarRowMenuItemPadTop();
int wwBarRowMenuItemPadBottom();
"""


# ----------------------------------------------------- nifskope_ui.cpp (1/3)
UI_HELPERS_ANCHOR = (
    "int wwBarRowButtonOverhead()\n"
    "{\n"
    "\treturn wwBarRowOverhead;\n"
    "}\n"
)

UI_HELPERS_ADD = """
/* THE TITLES IN THE MENU BAR, lane UI5. The measurement this rests on is in
 * wwskin.h; the short of it is that the menu-item `min-height` the row has
 * stated since lane WATER7 is not consulted at all, and the two vertical
 * paddings are. Stated from the row's own height and the item's own content, so
 * the only literal here is the even split itself. */
static int wwBarRowMenuContent = 0;
static int wwBarRowMenuPadTop = -1;
static int wwBarRowMenuPadBottom = -1;

QString wwBarRowMenuItemQss( int rowHeight, int itemContent )
{
	if ( !wwCompactTopBars() || rowHeight <= 0 || itemContent <= 0
		 || itemContent > rowHeight )
		return QString();
	const int top = ( rowHeight - itemContent ) / 2;
	const int bottom = rowHeight - itemContent - top;
	/* Nothing horizontal: res/style.qss gives a title 8 px on either side and
	 * that is its own business. */
	return QStringLiteral(
		"QMenuBar::item { padding-top: %1px; padding-bottom: %2px; }" )
		.arg( top ).arg( bottom );
}

int wwBarRowMenuItemContent()
{
	return wwBarRowMenuContent;
}

int wwBarRowMenuItemPadTop()
{
	return wwBarRowMenuPadTop;
}

int wwBarRowMenuItemPadBottom()
{
	return wwBarRowMenuPadBottom;
}
"""


# ----------------------------------------------------- nifskope_ui.cpp (2/3)
UI_APPLY_OLD = (
	"\tauto applyRow = [&]( int content ) {\n"
	"\t\tconst QString btn = wwBarRowButtonQss( content );\n"
	"\t\tconst QString bar = wwBarRowBoxQss() + btn;\n"
)

UI_APPLY_NEW = (
	"\t/* `menuItem` is appended LAST so it outranks the menu-item rule\n"
	"\t * wwBarRowButtonQss states: same selector, same specificity, and the later\n"
	"\t * declaration is the one Qt keeps. Empty by default, which is what lets the\n"
	"\t * calibrating pass below measure an item with no vertical padding at all. */\n"
	"\tauto applyRow = [&]( int content, const QString & menuItem = QString() ) {\n"
	"\t\tconst QString btn = wwBarRowButtonQss( content );\n"
	"\t\tconst QString bar = wwBarRowBoxQss() + btn + menuItem;\n"
)


# ----------------------------------------------------- nifskope_ui.cpp (3/3)
UI_TAIL_OLD = (
	"\twwBarRowContent = ( overhead >= 0 && h - overhead >= 8 ) ? h - overhead : fallback;\n"
	"\tapplyRow( wwBarRowContent );\n"
	"}\n"
)

UI_TAIL_NEW = (
	"\twwBarRowContent = ( overhead >= 0 && h - overhead >= 8 ) ? h - overhead : fallback;\n"
	"\tapplyRow( wwBarRowContent );\n"
	"\n"
	"\t/* AND THE TITLES IN THE MENU BAR SIT ON THE ROW'S CENTRE LINE (lane UI5;\n"
	"\t * bungo, 2026-09-10: \"Also, please center file / view / spells / options /\n"
	"\t * help buttons, top left\").\n"
	"\t *\n"
	"\t * The sheet above already reaches QMenuBar::item, but what it states there\n"
	"\t * is a min-height, and a min-height is not consulted on that path at all --\n"
	"\t * fifteen probe cases from 22 to 36 px leave the item 20 px tall -- so the\n"
	"\t * titles stayed at the top of a 35 px row. The two vertical paddings are\n"
	"\t * consulted, and unlike a margin they leave actionGeometry() honest.\n"
	"\t *\n"
	"\t * The item's own content height is MEASURED, never typed: the row's sheet is\n"
	"\t * applied once more with both vertical paddings taken away and the item's\n"
	"\t * height read back. ensurePolished() is enough and no event loop is needed\n"
	"\t * (probe case E), which is what makes this possible here at all -- the row\n"
	"\t * is aligned during construction, where nothing can be pumped. */\n"
	"\twwBarRowMenuContent = 0;\n"
	"\twwBarRowMenuPadTop = -1;\n"
	"\twwBarRowMenuPadBottom = -1;\n"
	"\tQMenuBar * rowMenu = nullptr;\n"
	"\tfor ( QWidget * w : barTargets ) {\n"
	"\t\tif ( auto * m = qobject_cast<QMenuBar *>( w ) ) {\n"
	"\t\t\trowMenu = m;\n"
	"\t\t\tbreak;\n"
	"\t\t}\n"
	"\t}\n"
	"\tif ( rowMenu ) {\n"
	"\t\tapplyRow( wwBarRowContent, QStringLiteral(\n"
	"\t\t\t\"QMenuBar::item { padding-top: 0px; padding-bottom: 0px; }\" ) );\n"
	"\t\trowMenu->ensurePolished();\n"
	"\t\tint itemContent = 0;\n"
	"\t\tfor ( QAction * a : rowMenu->actions() )\n"
	"\t\t\titemContent = qMax( itemContent, rowMenu->actionGeometry( a ).height() );\n"
	"\t\t/* FALLBACK, and it is named by the number it lands on (CONSTITUTION 10):\n"
	"\t\t * a menu bar with no titles to measure, or one whose items read back\n"
	"\t\t * taller than the row itself, takes the font's own line height rather\n"
	"\t\t * than shipping no rule and leaving the titles at the top. */\n"
	"\t\tif ( itemContent < 8 || itemContent > h )\n"
	"\t\t\titemContent = qBound( 8, rowMenu->fontMetrics().height(), h );\n"
	"\t\twwBarRowMenuContent = itemContent;\n"
	"\t\twwBarRowMenuPadTop = ( h - itemContent ) / 2;\n"
	"\t\twwBarRowMenuPadBottom = h - itemContent - wwBarRowMenuPadTop;\n"
	"\t\tapplyRow( wwBarRowContent, wwBarRowMenuItemQss( h, itemContent ) );\n"
	"\t}\n"
	"}\n"
)


# ----------------------------------------------------- wateruitest.cpp (1/3)
TEST_INC_OLD = "#include <QFile>\n#include <QImage>\n"
TEST_INC_NEW = "#include <QFile>\n#include <QHash>\n#include <QImage>\n"


# ----------------------------------------------------- wateruitest.cpp (2/3)
TEST_HELPER_ANCHOR = "//! A widget's rectangle in MAIN-WINDOW coordinates.\n"

TEST_HELPER_ADD = """/*! THE TEXT INK of one menu title: the rows of pixels much brighter than the
 *  bar's own background, inside [x0,x1] of the bar's own grab, in the bar's own
 *  logical pixels.
 *
 *  WHY PIXELS AND NOT actionGeometry(). Lane UI5 measured it outside the
 *  application (scratchpad/ui5_20260910/probe.cpp, 30 cases): once the row
 *  states the item's vertical padding, the item's RECT and the item's PAINTED
 *  BOX are both exactly the row -- and what bungo looked at is the TEXT inside
 *  them, which no rect the menu bar can be asked for describes. It is also the
 *  same method scratchpad/ui5_20260910/measure_before.py used on the shipped
 *  grab, so the harness's number and the report's number are one number.
 *
 *  `lift` is added to the threshold, so the caller can ask the SAME scan for a
 *  brightness no pixel in the bar carries and watch it find nothing -- a scan
 *  that has stopped finding anything cannot then pass for a centred title.
 */
QRect menuInk( QMenuBar * bar, int x0, int x1, double lift )
{
	if ( !bar || x1 < x0 )
		return QRect();
	const QPixmap pm = bar->grab();
	if ( pm.isNull() )
		return QRect();
	const qreal dpr = pm.devicePixelRatio() > 0 ? pm.devicePixelRatio() : 1.0;
	const QImage img = pm.toImage().convertToFormat( QImage::Format_RGB32 );
	// the bar's own background is the commonest colour in the strip
	QHash<QRgb, int> hist;
	for ( int y = 0; y < img.height(); y++ )
		for ( int x = 0; x < img.width(); x++ )
			hist[img.pixel( x, y ) & 0x00ffffff]++;
	QRgb bg = 0;
	int best = -1;
	for ( auto it = hist.cbegin(); it != hist.cend(); ++it )
		if ( it.value() > best ) {
			best = it.value();
			bg = it.key();
		}
	auto lum = []( QRgb c ) {
		return 0.299 * qRed( c ) + 0.587 * qGreen( c ) + 0.114 * qBlue( c );
	};
	const double thresh = lum( bg ) + 40.0 + lift;
	int y0 = 1 << 20, y1 = -1, ix0 = 1 << 20, ix1 = -1;
	for ( int y = 0; y < img.height(); y++ ) {
		for ( int x = 0; x < img.width(); x++ ) {
			const int lx = qRound( x / dpr );
			if ( lx < x0 || lx > x1 )
				continue;
			if ( lum( img.pixel( x, y ) ) <= thresh )
				continue;
			y0 = qMin( y0, qRound( y / dpr ) );
			y1 = qMax( y1, qRound( y / dpr ) );
			ix0 = qMin( ix0, lx );
			ix1 = qMax( ix1, lx );
		}
	}
	if ( y1 < 0 )
		return QRect();
	return QRect( QPoint( ix0, y0 ), QPoint( ix1, y1 ) );
}

"""


# ----------------------------------------------------- wateruitest.cpp (3/3)
TEST_M_ANCHOR = "\t\t\t// the top-strip picture, for bungo\n"

TEST_M_ADD = """\t\t\t/* =============================================================
\t\t\t *  M -- THE TITLES IN THE MENU BAR SIT ON THE ROW'S CENTRE LINE
\t\t\t *
\t\t\t *  bungo, 2026-09-10 20:2x, verbatim: "Also, please center file /
\t\t\t *  view / spells / options / help buttons, top left".
\t\t\t *
\t\t\t *  Measured on the shipped 20:45:47 window by
\t\t\t *  scratchpad/ui5_20260910/measure_before.py: the five titles' text
\t\t\t *  sat at y 6..16 in a 35 px row whose centre line is 17.0 -- six
\t\t\t *  pixels high. This group reads the same thing out of the LIVE menu
\t\t\t *  bar, in PIXELS. The rect cannot answer it: once the row states the
\t\t\t *  item's padding, the rect and the painted box are both the whole
\t\t\t *  row, and it is the text inside them that moved.
\t\t\t *
\t\t\t *  It runs BEFORE the picture below, and both halves of its floor are
\t\t\t *  checks, so whatever it appends is proved taken away again and the
\t\t\t *  picture is the shipped state.
\t\t\t * ============================================================= */
\t\t\tsay( *st, QStringLiteral( "--- M: the menu bar's titles in the row ---" ) );
\t\t\tif ( !compact )
\t\t\t\tskip( *st, QStringLiteral( "M1..M6 measure the compact bar row's menu items; this "
\t\t\t\t\t"profile has UI/CompactTopBars OFF, so the titles are where they were before "
\t\t\t\t\t"lane UI3 and these gates do not describe them" ) );
\t\t\tif ( compact && menubar && row > 0 ) {
\t\t\t\tconst double mCentre = ( menubar->height() - 1 ) / 2.0;
\t\t\t\tconst QList<QAction *> titles = menubar->actions();
\t\t\t\t/* ONE predicate for the verdict, the way back and the floor, so the
\t\t\t\t * floor exercises the code the verdict comes from. */
\t\t\t\tauto readInk = [&]( double lift, QString * detail, int * found,
\t\t\t\t\t\t\t\t\tdouble * worst, double * spread ) {
\t\t\t\t\t*found = 0;
\t\t\t\t\t*worst = 0.0;
\t\t\t\t\tdouble lo = 1e9, hi = -1e9;
\t\t\t\t\tQString d;
\t\t\t\t\tfor ( QAction * a : titles ) {
\t\t\t\t\t\tconst QRect g = menubar->actionGeometry( a );
\t\t\t\t\t\tif ( g.isNull() || g.width() <= 0 )
\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\tconst QRect ink = menuInk( menubar, g.left(), g.right(), lift );
\t\t\t\t\t\tif ( ink.isNull() )
\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\t( *found )++;
\t\t\t\t\t\tconst double c = ( ink.top() + ink.bottom() ) / 2.0;
\t\t\t\t\t\tif ( qAbs( c - mCentre ) > qAbs( *worst ) )
\t\t\t\t\t\t\t*worst = c - mCentre;
\t\t\t\t\t\tlo = qMin( lo, c );
\t\t\t\t\t\thi = qMax( hi, c );
\t\t\t\t\t\td += QStringLiteral( " %1[y %2..%3 c %4]" )
\t\t\t\t\t\t\t.arg( a->text().remove( QLatin1Char( '&' ) ) )
\t\t\t\t\t\t\t.arg( ink.top() ).arg( ink.bottom() ).arg( c, 0, 'f', 1 );
\t\t\t\t\t}
\t\t\t\t\t*spread = ( *found > 0 ) ? hi - lo : -1.0;
\t\t\t\t\tif ( detail )
\t\t\t\t\t\t*detail = d;
\t\t\t\t};

\t\t\t\tQString detail;
\t\t\t\tint found = 0;
\t\t\t\tdouble worst = 0.0, spread = 0.0;
\t\t\t\treadInk( 0.0, &detail, &found, &worst, &spread );
\t\t\t\tsay( *st, QStringLiteral( "  M: menu bar %1x%2 at y %3, row %4, centre line %5; the "
\t\t\t\t\t"skin measured item content %6 -> padding-top %7 / padding-bottom %8" )
\t\t\t\t\t.arg( rMenu.width() ).arg( rMenu.height() ).arg( rMenu.top() ).arg( row )
\t\t\t\t\t.arg( mCentre, 0, 'f', 1 ).arg( wwBarRowMenuItemContent() )
\t\t\t\t\t.arg( wwBarRowMenuItemPadTop() ).arg( wwBarRowMenuItemPadBottom() ) );
\t\t\t\tsay( *st, QStringLiteral( "  M: %1 titles read:%2  worst %3, spread %4" )
\t\t\t\t\t.arg( found ).arg( detail ).arg( worst, 0, 'f', 1 ).arg( spread, 0, 'f', 1 ) );

\t\t\t\tcheck( *st, QStringLiteral( "(M floor) every title in the menu bar was found as real "
\t\t\t\t\t"text ink (%1 of %2)" ).arg( found ).arg( titles.size() ),
\t\t\t\t\ttitles.size() >= 5 && found == titles.size() );
\t\t\t\t{
\t\t\t\t\tint none = 0;
\t\t\t\t\tdouble w2 = 0.0, s2 = 0.0;
\t\t\t\t\tQString d2;
\t\t\t\t\treadInk( 255.0, &d2, &none, &w2, &s2 );
\t\t\t\t\tcheck( *st, QStringLiteral( "(M floor) ...and the SAME scan finds nothing for a "
\t\t\t\t\t\t"brightness the bar does not carry (%1 titles)" ).arg( none ), none == 0 );
\t\t\t\t}
\t\t\t\tcheck( *st, QStringLiteral( "(M1) the menu bar is still exactly the row's height "
\t\t\t\t\t"(%1 px at y %2, row %3) -- centring the titles did not grow it" )
\t\t\t\t\t.arg( rMenu.height() ).arg( rMenu.top() ).arg( row ),
\t\t\t\t\t!rMenu.isNull() && rMenu.height() == row && rMenu.top() == 0 );
\t\t\t\tcheck( *st, QStringLiteral( "(M2) every title's text is on the row's centre line "
\t\t\t\t\t"within 1 px (worst %1)" ).arg( worst, 0, 'f', 1 ),
\t\t\t\t\tfound >= 5 && qAbs( worst ) <= 1.0 );
\t\t\t\tcheck( *st, QStringLiteral( "(M2) ...and the five agree with each other within 1 px "
\t\t\t\t\t"(spread %1)" ).arg( spread, 0, 'f', 1 ),
\t\t\t\t\tfound >= 5 && spread >= 0.0 && spread <= 1.0 );

\t\t\t\t/* M3 IS THE REFUTER this lane registered before it wrote a line:
\t\t\t\t * wwAlignBarRow takes the TALLEST bar's own hint as the row, so a
\t\t\t\t * menu bar whose hint had grown past 35 would take the whole row
\t\t\t\t * with it the next time the row is aligned. */
\t\t\t\tconst int mHint = qMax( menubar->sizeHint().height(),
\t\t\t\t\tmenubar->minimumSizeHint().height() );
\t\t\t\tsay( *st, QStringLiteral( "  M3: the menu bar's own hint is %1 against a row of %2" )
\t\t\t\t\t.arg( mHint ).arg( row ) );
\t\t\t\tcheck( *st, QStringLiteral( "(M3) the menu bar's own size hint does not exceed the row "
\t\t\t\t\t"(%1 <= %2), so aligning the row again cannot grow it" ).arg( mHint ).arg( row ),
\t\t\t\t\tmHint <= row );
\t\t\t\t{
\t\t\t\t\tint real = 0, agree = 0;
\t\t\t\t\tfor ( const QRect & r : { rFile, rLOD, rView, rHeader, rTabs } ) {
\t\t\t\t\t\tif ( r.isNull() || r.height() <= 0 )
\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\treal++;
\t\t\t\t\t\tif ( qAbs( r.height() - row ) <= 1 )
\t\t\t\t\t\t\tagree++;
\t\t\t\t\t}
\t\t\t\t\tcheck( *st, QStringLiteral( "(M3) ...and every other bar in the row is still the "
\t\t\t\t\t\t"row's height (%1 of %2)" ).arg( agree ).arg( real ),
\t\t\t\t\t\treal >= 4 && agree == real );
\t\t\t\t}

\t\t\t\t/* M4: THE WAY BACK, live. UI/CompactTopBars false appends no row
\t\t\t\t * sheet at all, so the menu bar carries res/style.qss alone -- which
\t\t\t\t * is this, and it must read the PRE-UI3 position, not the centre
\t\t\t\t * line. The setting itself is not written: this harness runs against
\t\t\t\t * the user's own QSettings and never changes a state it did not make.
\t\t\t\t * The sheet is the whole of the off value, so this is the whole of
\t\t\t\t * the way back for the titles. */
\t\t\t\tconst QString menuSaved = menubar->styleSheet();
\t\t\t\tconst QString rowRule = wwBarRowMenuItemQss( row, wwBarRowMenuItemContent() );
\t\t\t\tcheck( *st, QStringLiteral( "(M4) the row's menu-item rule is the one the skin states, "
\t\t\t\t\t"and the menu bar actually carries it (\\"%1\\")" ).arg( rowRule ),
\t\t\t\t\t!rowRule.isEmpty() && menuSaved.contains( rowRule ) );
\t\t\t\tcheck( *st, QStringLiteral( "(M4 floor) ...and the skin states no rule at all for a row "
\t\t\t\t\t"of 0 or a content of 0" ),
\t\t\t\t\twwBarRowMenuItemQss( 0, 16 ).isEmpty()
\t\t\t\t\t\t&& wwBarRowMenuItemQss( row, 0 ).isEmpty() );
\t\t\t\tmenubar->setStyleSheet( QString() );
\t\t\t\tfor ( int i = 0; i < 3; i++ )
\t\t\t\t\tQApplication::processEvents();
\t\t\t\tint offFound = 0;
\t\t\t\tdouble offWorst = 0.0, offSpread = 0.0;
\t\t\t\tQString offDetail;
\t\t\t\treadInk( 0.0, &offDetail, &offFound, &offWorst, &offSpread );
\t\t\t\tsay( *st, QStringLiteral( "  M4: with the row's sheet taken off (the off value of "
\t\t\t\t\t"UI/CompactTopBars) the titles read:%1  worst %2" )
\t\t\t\t\t.arg( offDetail ).arg( offWorst, 0, 'f', 1 ) );
\t\t\t\tcheck( *st, QStringLiteral( "(M4) the way back puts the titles back high in the row, "
\t\t\t\t\t"where they were before this lane (worst %1, against %2 with the row's sheet on)" )
\t\t\t\t\t.arg( offWorst, 0, 'f', 1 ).arg( worst, 0, 'f', 1 ),
\t\t\t\t\toffFound >= 5 && offWorst < -2.0 );

\t\t\t\t/* M5: THE FLOOR THAT FIRES, live, in this same log. The arithmetic
\t\t\t\t * that SHIPPED at 20:45:47 -- the row's own two pixels of air, which
\t\t\t\t * is what wwBarRowButtonQss states and what left the titles six
\t\t\t\t * pixels high -- appended over the live sheet, and the SAME M2
\t\t\t\t * predicate asked again. */
\t\t\t\tmenubar->setStyleSheet( menuSaved + QStringLiteral(
\t\t\t\t\t"QMenuBar::item { padding-top: 2px; padding-bottom: 2px; }" ) );
\t\t\t\tfor ( int i = 0; i < 3; i++ )
\t\t\t\t\tQApplication::processEvents();
\t\t\t\tint sabFound = 0;
\t\t\t\tdouble sabWorst = 0.0, sabSpread = 0.0;
\t\t\t\tQString sabDetail;
\t\t\t\treadInk( 0.0, &sabDetail, &sabFound, &sabWorst, &sabSpread );
\t\t\t\tsay( *st, QStringLiteral( "  M5 floor: with the 20:45:47 arithmetic put back (padding "
\t\t\t\t\t"2 / 2) the titles read:%1  worst %2" )
\t\t\t\t\t.arg( sabDetail ).arg( sabWorst, 0, 'f', 1 ) );
\t\t\t\tcheck( *st, QStringLiteral( "(M5 floor) the SAME test goes red on the sheet that "
\t\t\t\t\t"shipped with the titles six pixels high (worst %1)" ).arg( sabWorst, 0, 'f', 1 ),
\t\t\t\t\tsabFound >= 5 && qAbs( sabWorst ) > 1.0 );

\t\t\t\tmenubar->setStyleSheet( menuSaved );
\t\t\t\tfor ( int i = 0; i < 3; i++ )
\t\t\t\t\tQApplication::processEvents();
\t\t\t\tint backFound = 0;
\t\t\t\tdouble backWorst = 0.0, backSpread = 0.0;
\t\t\t\tQString backDetail;
\t\t\t\treadInk( 0.0, &backDetail, &backFound, &backWorst, &backSpread );
\t\t\t\tsay( *st, QStringLiteral( "  M5 floor: restored, the titles read:%1  worst %2" )
\t\t\t\t\t.arg( backDetail ).arg( backWorst, 0, 'f', 1 ) );
\t\t\t\tcheck( *st, QStringLiteral( "(M5 floor) ...and taking it away puts every title back on "
\t\t\t\t\t"the centre line (worst %1), so the picture below is the shipped state" )
\t\t\t\t\t.arg( backWorst, 0, 'f', 1 ),
\t\t\t\t\tbackFound >= 5 && qAbs( backWorst ) <= 1.0 );

\t\t\t\t/* M6: the rule is stated ONCE, by the skin, derived from the row --
\t\t\t\t * not a literal anybody typed -- and nothing horizontal. */
\t\t\t\tsay( *st, QStringLiteral( "  M6: the skin's menu-item rule is \\"%1\\"" ).arg( rowRule ) );
\t\t\t\tcheck( *st, QStringLiteral( "(M6) the skin states one top and one bottom padding for "
\t\t\t\t\t"the menu item, derived from the row (%1 + %2 + %3 = %4)" )
\t\t\t\t\t.arg( wwBarRowMenuItemPadTop() ).arg( wwBarRowMenuItemContent() )
\t\t\t\t\t.arg( wwBarRowMenuItemPadBottom() ).arg( row ),
\t\t\t\t\trowRule.count( QStringLiteral( "padding-top:" ) ) == 1
\t\t\t\t\t\t&& rowRule.count( QStringLiteral( "padding-bottom:" ) ) == 1
\t\t\t\t\t\t&& rowRule.count( QLatin1Char( '{' ) ) == 1
\t\t\t\t\t\t&& wwBarRowMenuItemPadTop() + wwBarRowMenuItemContent()
\t\t\t\t\t\t\t+ wwBarRowMenuItemPadBottom() == row );
\t\t\t\tcheck( *st, QStringLiteral( "(M6) ...and nothing horizontal, so a title keeps the 8 px "
\t\t\t\t\t"res/style.qss gives it on either side" ),
\t\t\t\t\t!rowRule.contains( QStringLiteral( "padding-left" ) )
\t\t\t\t\t\t&& !rowRule.contains( QStringLiteral( "padding-right" ) )
\t\t\t\t\t\t&& rowRule.count( QStringLiteral( "padding:" ) ) == 0 );
\t\t\t}

\t\t\t// the top-strip picture, for bungo
"""


# ------------------------------------------------------ tests/spells/water_ui.sh
SPELL_NAMES_OLD = "\t\"(S floor) the SAME five go red\" \"(S floor) ...and taking it away\"; do\n"
SPELL_NAMES_NEW = (
	"\t\"(S floor) the SAME five go red\" \"(S floor) ...and taking it away\" \\\n"
	"\t\"(M floor) every painted title in the menu bar\" \"(M floor) ...and the SAME scan finds nothing\" \\\n"
	"\t\"(M1) the menu bar is still exactly\" \"(M2) every title's text is on the row's centre\" \\\n"
	"\t\"(M2) ...and the five agree\" \"(M3) the menu bar's own size hint\" \\\n"
	"\t\"(M3) ...and every other bar in the row\" \"(M4) the row's menu-item rule\" \\\n"
	"\t\"(M4 floor)\" \"(M4) the way back puts the titles back high\" \\\n"
	"\t\"(M5 floor) the SAME test goes red\" \"(M5 floor) ...and taking it away\" \\\n"
	"\t\"(M6) the skin states one top\" \"(M6) ...and nothing horizontal\"; do\n"
)

SPELL_FLOOR_OLD = (
	"echo \"checks run: ${COUNT:-none} (floor 48)\"\n"
	"case \"${COUNT:-}\" in\n"
	"\t''|*[!0-9]*) echo \"FAIL: the log carries no check count\"; fails=$((fails+1)) ;;\n"
	"\t*) [ \"$COUNT\" -ge 48 ] || { echo \"FAIL: only $COUNT checks ran, floor is 48\"; fails=$((fails+1)); } ;;\n"
	"esac\n"
)

SPELL_FLOOR_NEW = (
	"# LANE UI5 re-counted it against the MEASURED run, not the model: lane\n"
	"# WATER8-GATE's own chain read 59 checks on the 21:02:12 exe (its DONE line),\n"
	"# where the arithmetic above had predicted 53.  Group M -- the menu bar's\n"
	"# titles on the row's centre line -- adds 14: 2 floors, M1, 2 for M2, 2 for\n"
	"# M3, 2 for M4 plus its own floor, BOTH halves of the live M5 floor, and 2\n"
	"# for M6.  So a full run reads 73 and the smallest run that is still WORKING\n"
	"# (no pictures asked for, 53 + 14) reads 67.  62 is five below that, and it\n"
	"# is also above 59 -- so a build that lost group M entirely goes red on the\n"
	"# count alone, however the spell is called.  All 14 skip by name when\n"
	"# UI/CompactTopBars is off.\n"
	"echo \"checks run: ${COUNT:-none} (floor 62)\"\n"
	"case \"${COUNT:-}\" in\n"
	"\t''|*[!0-9]*) echo \"FAIL: the log carries no check count\"; fails=$((fails+1)) ;;\n"
	"\t*) [ \"$COUNT\" -ge 62 ] || { echo \"FAIL: only $COUNT checks ran, floor is 62\"; fails=$((fails+1)); } ;;\n"
	"esac\n"
)


EDITS = [
	("src/wwskin.h",            "after",   SKIN_ANCHOR,        SKIN_ADD,          "wwBarRowMenuItemQss declared"),
	("src/nifskope_ui.cpp",     "after",   UI_HELPERS_ANCHOR,  UI_HELPERS_ADD,    "wwBarRowMenuItemQss defined"),
	("src/nifskope_ui.cpp",     "replace", UI_APPLY_OLD,       UI_APPLY_NEW,      "applyRow takes the menu rule"),
	("src/nifskope_ui.cpp",     "replace", UI_TAIL_OLD,        UI_TAIL_NEW,       "wwAlignBarRow calibrates the item"),
	("src/wateruitest.cpp",     "replace", TEST_INC_OLD,       TEST_INC_NEW,      "QHash for the ink histogram"),
	("src/wateruitest.cpp",     "after",   TEST_HELPER_ANCHOR, TEST_HELPER_ADD,   "menuInk() helper"),
	("src/wateruitest.cpp",     "replace", TEST_M_ANCHOR,      TEST_M_ADD,        "group M"),
	("tests/spells/water_ui.sh", "replace", SPELL_NAMES_OLD,   SPELL_NAMES_NEW,   "group M gate names"),
	("tests/spells/water_ui.sh", "replace", SPELL_FLOOR_OLD,   SPELL_FLOOR_NEW,   "floor 48 -> 62"),
]


def main():
	apply = "--apply" in sys.argv
	ok = True
	staged = {}
	for path, mode, anchor, text, why in EDITS:
		full = os.path.join(ROOT, path)
		raw = staged.get(path)
		if raw is None:
			with open(full, "rb") as f:
				raw = f.read()
		s = raw.decode("utf-8")
		# the file's real line ending, carried in the anchor
		cr = raw.count(b"\r")
		if cr:
			anchor_use = anchor.replace("\n", "\r\n")
			text_use = text.replace("\n", "\r\n")
		else:
			anchor_use = anchor
			text_use = text
		n = s.count(anchor_use)
		mark = text_use.strip().splitlines()[0][:48] if text_use.strip() else ""
		already = s.count(text_use)
		print("%-26s %-8s anchor x%d  already x%d  CR %d  -- %s"
			  % (path, mode, n, already, cr, why))
		if already >= 1:
			print("    REFUSED: this edit is already present")
			ok = False
			continue
		if n != 1:
			print("    REFUSED: anchor must match exactly once (matched %d)  [%s]"
				  % (n, mark))
			ok = False
			continue
		if mode == "after":
			s = s.replace(anchor_use, anchor_use + text_use, 1)
		elif mode == "replace":
			s = s.replace(anchor_use, text_use, 1)
		else:
			print("    REFUSED: unknown mode")
			ok = False
			continue
		staged[path] = s.encode("utf-8")

	if not ok:
		print("\nCHECK FAILED -- nothing written")
		return 1
	print("\n%d of %d edits match once" % (len(EDITS), len(EDITS)))
	if not apply:
		print("--check only, nothing written (pass --apply to write)")
		return 0

	for path, data in staged.items():
		full = os.path.join(ROOT, path)
		with open(full, "rb") as f:
			before = f.read()
		if before.count(b"\r") != data.count(b"\r"):
			print("REFUSED %s: CR count would change %d -> %d"
				  % (path, before.count(b"\r"), data.count(b"\r")))
			return 1
	for path, data in staged.items():
		full = os.path.join(ROOT, path)
		with open(full, "wb") as f:
			f.write(data)
		print("wrote %-26s %d bytes, CR %d" % (path, len(data), data.count(b"\r")))
	return 0


if __name__ == "__main__":
	sys.exit(main())
