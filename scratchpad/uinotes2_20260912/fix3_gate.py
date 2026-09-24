"""UINOTES2 step 3 -- the gate for the two icons, for Ctrl+A, and the six owed
pictures.

Refusing script: exact-once anchors, pure LF, all-or-nothing.
"""
import os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "src", "animworkspacetest.cpp")

with open(SRC, "rb") as f:
    orig = f.read()
assert orig.count(b"\r") == 0
text = orig.decode("utf-8")

subs = []


def sub(a, r):
    subs.append((a, r))


# =====================================================================
# (m) / (7) / (7a) -- openMenu learns to photograph the menu it opened
# =====================================================================
sub(
    "\t\t\t\tQStringList menuTexts;\n"
    "\t\t\t\tQString triggeredText;\n"
    "\t\t\t\tauto openMenu = [&]( const QPoint & pos, const QString & wantPrefix ) {\n"
    "\t\t\t\t\tmenuTexts.clear();\n"
    "\t\t\t\t\ttriggeredText.clear();\n"
    "\t\t\t\t\tQTimer::singleShot( 0, qApp, [&menuTexts, &triggeredText, wantPrefix]() {\n"
    "\t\t\t\t\t\tauto * m = qobject_cast<QMenu *>( QApplication::activePopupWidget() );\n"
    "\t\t\t\t\t\tif ( !m )\n"
    "\t\t\t\t\t\t\treturn;\n"
    "\t\t\t\t\t\tQAction * hit = nullptr;\n",
    "\t\t\t\tQStringList menuTexts;\n"
    "\t\t\t\tQString triggeredText;\n"
    "\t\t\t\t/* The PICTURE of the menu, when one is owed (bungo's rulings 7 and\n"
    "\t\t\t\t   7a). It has to be taken while the popup is alive, which is inside\n"
    "\t\t\t\t   the same timer that reads the entries out of it -- so the shot\n"
    "\t\t\t\t   path is set just before the openMenu call that owes one and the\n"
    "\t\t\t\t   lambda takes a copy. Empty = no picture. */\n"
    "\t\t\t\tQString menuShot;\n"
    "\t\t\t\tQStringList menuShotsTaken;\n"
    "\t\t\t\tauto openMenu = [&]( const QPoint & pos, const QString & wantPrefix ) {\n"
    "\t\t\t\t\tmenuTexts.clear();\n"
    "\t\t\t\t\ttriggeredText.clear();\n"
    "\t\t\t\t\tQTimer::singleShot( 0, qApp, [&menuTexts, &triggeredText, &menuShotsTaken, wantPrefix, shot = menuShot]() {\n"
    "\t\t\t\t\t\tauto * m = qobject_cast<QMenu *>( QApplication::activePopupWidget() );\n"
    "\t\t\t\t\t\tif ( !m )\n"
    "\t\t\t\t\t\t\treturn;\n"
    "\t\t\t\t\t\tif ( !shot.isEmpty() ) {\n"
    "\t\t\t\t\t\t\tconst QPixmap pm = m->grab();\n"
    "\t\t\t\t\t\t\tif ( pm.width() > 40 && pm.save( shot ) )\n"
    "\t\t\t\t\t\t\t\tmenuShotsTaken << QStringLiteral( \"%1 (%2x%3)\" ).arg( shot ).arg( pm.width() ).arg( pm.height() );\n"
    "\t\t\t\t\t\t}\n"
    "\t\t\t\t\t\tQAction * hit = nullptr;\n",
)

# the ruler menu at frame 37 -> ruling 7's picture
sub(
    "\t\t\t\t// ---- the RULER had NO menu at all before this lane\n"
    "\t\t\t\topenMenu( QPoint( int( sheet->frameToX( 37.0f ) ), 4 ), QString() );\n",
    "\t\t\t\t// ---- the RULER had NO menu at all before this lane\n"
    "\t\t\t\tmenuShot = st->outDir.isEmpty() ? QString() : st->outDir + QStringLiteral( \"/r7_sheet_menu_frame37.png\" );\n"
    "\t\t\t\topenMenu( QPoint( int( sheet->frameToX( 37.0f ) ), 4 ), QString() );\n"
    "\t\t\t\tmenuShot.clear();\n",
)

# the marker menu -> ruling 7a's picture
sub(
    "\t\t\t\topenMenu( QPoint( int( sheet->frameToX( float( footFrame ) ) ), markerY ), QString() );\n",
    "\t\t\t\tmenuShot = st->outDir.isEmpty() ? QString() : st->outDir + QStringLiteral( \"/r7a_annotation_menu.png\" );\n"
    "\t\t\t\topenMenu( QPoint( int( sheet->frameToX( float( footFrame ) ) ), markerY ), QString() );\n"
    "\t\t\t\tmenuShot.clear();\n",
)

# report the two pictures at the end of the (m7a) block
sub(
    "\t\t\t\t// THE FLOOR for the zoom check: a real reset must fail it\n"
    "\t\t\t\tsheet->frameAll();\n",
    "\t\t\t\t/* THE PICTURE for ruling 7a's second half: the sheet AFTER the\n"
    "\t\t\t\t   drag, still at the zoom it was set to. The check above says the\n"
    "\t\t\t\t   window is unchanged as a number; this is the same fact as a\n"
    "\t\t\t\t   picture, taken before frameAll() throws the zoom away. */\n"
    "\t\t\t\tif ( !st->outDir.isEmpty() ) {\n"
    "\t\t\t\t\tconst QPixmap zp = sheet->grab();\n"
    "\t\t\t\t\tconst QString zs = st->outDir + QStringLiteral( \"/r7a_zoom_after_drag.png\" );\n"
    "\t\t\t\t\tcheck( *st, QStringLiteral( \"picture: (7a) the sheet still at %1..%2 after the marker drag, %3x%4 -> %5\" )\n"
    "\t\t\t\t\t\t   .arg( sheet->viewFirst() ).arg( sheet->viewLast() ).arg( zp.width() ).arg( zp.height() ).arg( zs ),\n"
    "\t\t\t\t\t\t   zp.width() > 200 && zp.save( zs ) );\n"
    "\t\t\t\t}\n"
    "\t\t\t\tcheck( *st, QStringLiteral( \"picture: (7) and (7a) the two context menus: %1\" ).arg( menuShotsTaken.join( QStringLiteral( \" | \" ) ) ),\n"
    "\t\t\t\t\t   st->outDir.isEmpty() || menuShotsTaken.count() == 2 );\n"
    "\t\t\t\t// THE FLOOR for the zoom check: a real reset must fail it\n"
    "\t\t\t\tsheet->frameAll();\n",
)

# =====================================================================
# (l) -- ruling 9's picture: the ruler with both grips
# =====================================================================
sub(
    "\t\t\t\tcheck( *st, QStringLiteral( \"(l) three gestures put three commands on the stack: %1 -> %2\" ).arg( undo0 ).arg( ws->undoStack()->index() ),\n",
    "\t\t\t\tif ( !st->outDir.isEmpty() ) {\n"
    "\t\t\t\t\t/* RULING 9's picture: the ruler carrying both range grips, with\n"
    "\t\t\t\t\t   the out-of-range frames darkened on either side of them. */\n"
    "\t\t\t\t\tconst QPixmap rp = sheet->grab();\n"
    "\t\t\t\t\tconst QString rs = st->outDir + QStringLiteral( \"/r9_range_grips.png\" );\n"
    "\t\t\t\t\tcheck( *st, QStringLiteral( \"picture: (9) the ruler with both grips, range %1..%2, %3x%4 -> %5\" )\n"
    "\t\t\t\t\t\t   .arg( ws->document()->rangeFirstFrame() ).arg( ws->document()->rangeLastFrame() )\n"
    "\t\t\t\t\t\t   .arg( rp.width() ).arg( rp.height() ).arg( rs ),\n"
    "\t\t\t\t\t\t   rp.width() > 200 && rp.save( rs ) );\n"
    "\t\t\t\t}\n"
    "\t\t\t\tcheck( *st, QStringLiteral( \"(l) three gestures put three commands on the stack: %1 -> %2\" ).arg( undo0 ).arg( ws->undoStack()->index() ),\n",
)

# =====================================================================
# (n) -- ruling 3's picture: the Remove transform axes dialog
# =====================================================================
sub(
    "\t\t\t\tbool dlgSeen = false, boxesSeen = false;\n"
    "\t\t\t\tQTimer::singleShot( 0, qApp, [&dlgSeen, &boxesSeen]() {\n",
    "\t\t\t\tbool dlgSeen = false, boxesSeen = false;\n"
    "\t\t\t\tQString dlgShotLine;\n"
    "\t\t\t\tQTimer::singleShot( 0, qApp, [&dlgSeen, &boxesSeen, &dlgShotLine, out = st->outDir]() {\n",
)

sub(
    "\t\t\t\t\tif ( by )\n"
    "\t\t\t\t\t\tby->setChecked( true );\n"
    "\t\t\t\t\tif ( auto * ok = dw->findChild<QPushButton *>( QStringLiteral( \"AnimWsAxisOk\" ) ) )\n",
    "\t\t\t\t\tif ( by )\n"
    "\t\t\t\t\t\tby->setChecked( true );\n"
    "\t\t\t\t\t/* RULING 3's picture, taken with the two boxes ticked -- the\n"
    "\t\t\t\t\t   dialog only exists inside this timer, so this is the one\n"
    "\t\t\t\t\t   moment it can be photographed. */\n"
    "\t\t\t\t\tif ( !out.isEmpty() ) {\n"
    "\t\t\t\t\t\tconst QPixmap dp = dw->grab();\n"
    "\t\t\t\t\t\tconst QString ds = out + QStringLiteral( \"/r3_axis_dialog.png\" );\n"
    "\t\t\t\t\t\tif ( dp.width() > 80 && dp.save( ds ) )\n"
    "\t\t\t\t\t\t\tdlgShotLine = QStringLiteral( \"%1 (%2x%3)\" ).arg( ds ).arg( dp.width() ).arg( dp.height() );\n"
    "\t\t\t\t\t}\n"
    "\t\t\t\t\tif ( auto * ok = dw->findChild<QPushButton *>( QStringLiteral( \"AnimWsAxisOk\" ) ) )\n",
)

sub(
    "\t\t\t\tcheck( *st, \"(n) the dialog opened with six axis boxes and the line naming the rotation convention\",\n"
    "\t\t\t\t\t   dlgSeen && boxesSeen );\n",
    "\t\t\t\tcheck( *st, \"(n) the dialog opened with six axis boxes and the line naming the rotation convention\",\n"
    "\t\t\t\t\t   dlgSeen && boxesSeen );\n"
    "\t\t\t\tcheck( *st, QStringLiteral( \"picture: (3) the Remove transform axes dialog with translation X and Y ticked: %1\" ).arg( dlgShotLine ),\n"
    "\t\t\t\t\t   st->outDir.isEmpty() || !dlgShotLine.isEmpty() );\n",
)

# =====================================================================
# (p) -- rulings 6 and 6a: the header menu open, and the side panel
# =====================================================================
sub(
    "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(p) the Track section's Bone field reads the selected track: '%1'\" ).arg( boneField ? boneField->text() : QString() ),\n"
    "\t\t\t\t\t\t\t   boneField && boneField->text() == QStringLiteral( \"COM\" ) );\n",
    "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(p) the Track section's Bone field reads the selected track: '%1'\" ).arg( boneField ? boneField->text() : QString() ),\n"
    "\t\t\t\t\t\t\t   boneField && boneField->text() == QStringLiteral( \"COM\" ) );\n"
    "\t\t\t\t\t\tif ( !st->outDir.isEmpty() && panel ) {\n"
    "\t\t\t\t\t\t\t// RULING 6a's picture: the right-side panel, on a bone row\n"
    "\t\t\t\t\t\t\tconst QPixmap pp = panel->grab();\n"
    "\t\t\t\t\t\t\tconst QString ps = st->outDir + QStringLiteral( \"/r6a_panel_track.png\" );\n"
    "\t\t\t\t\t\t\tcheck( *st, QStringLiteral( \"picture: (6a) the side panel with a bone row selected (%1), %2x%3 -> %4\" )\n"
    "\t\t\t\t\t\t\t\t   .arg( shown().join( QStringLiteral( \"+\" ) ) ).arg( pp.width() ).arg( pp.height() ).arg( ps ),\n"
    "\t\t\t\t\t\t\t\t   pp.width() > 80 && pp.save( ps ) );\n"
    "\t\t\t\t\t\t}\n",
)

sub(
    "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(p) an annotation selected shows Clip and Annotation only: %1\" ).arg( shown().join( QStringLiteral( \"+\" ) ) ),\n",
    "\t\t\t\t\t\tif ( !st->outDir.isEmpty() && panel ) {\n"
    "\t\t\t\t\t\t\t// RULING 6a's second picture: the same panel, another kind\n"
    "\t\t\t\t\t\t\tconst QPixmap pp = panel->grab();\n"
    "\t\t\t\t\t\t\tconst QString ps = st->outDir + QStringLiteral( \"/r6a_panel_annotation.png\" );\n"
    "\t\t\t\t\t\t\tcheck( *st, QStringLiteral( \"picture: (6a) the side panel with an annotation selected (%1), %2x%3 -> %4\" )\n"
    "\t\t\t\t\t\t\t\t   .arg( shown().join( QStringLiteral( \"+\" ) ) ).arg( pp.width() ).arg( pp.height() ).arg( ps ),\n"
    "\t\t\t\t\t\t\t\t   pp.width() > 80 && pp.save( ps ) );\n"
    "\t\t\t\t\t\t}\n"
    "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(p) an annotation selected shows Clip and Annotation only: %1\" ).arg( shown().join( QStringLiteral( \"+\" ) ) ),\n",
)

sub(
    "\t\t\t\tcheck( *st, QStringLiteral( \"(p) no push buttons are left in the dock: %1\" ).arg( pushLeft ), pushLeft == 0 );\n",
    "\t\t\t\tcheck( *st, QStringLiteral( \"(p) no push buttons are left in the dock: %1\" ).arg( pushLeft ), pushLeft == 0 );\n"
    "\t\t\t\t/* RULING 6's picture: the header menu bar that took the fifteen\n"
    "\t\t\t\t   buttons' place, with one of its menus actually open -- a picture\n"
    "\t\t\t\t   of a closed menu bar would not show that the actions are in it.\n"
    "\t\t\t\t   popup() is not modal, so the picture can be taken in line. */\n"
    "\t\t\t\tif ( !st->outDir.isEmpty() ) {\n"
    "\t\t\t\t\tQString line;\n"
    "\t\t\t\t\tif ( auto * clipMenu = ws->findChild<QMenu *>( QStringLiteral( \"AnimWsMenuClip\" ) ) ) {\n"
    "\t\t\t\t\t\tclipMenu->popup( ws->mapToGlobal( QPoint( 8, 40 ) ) );\n"
    "\t\t\t\t\t\tqApp->processEvents();\n"
    "\t\t\t\t\t\tconst QPixmap mp = clipMenu->grab();\n"
    "\t\t\t\t\t\tconst QString ms = st->outDir + QStringLiteral( \"/r6_menu_clip.png\" );\n"
    "\t\t\t\t\t\tif ( mp.width() > 40 && mp.save( ms ) )\n"
    "\t\t\t\t\t\t\tline = QStringLiteral( \"%1 (%2x%3)\" ).arg( ms ).arg( mp.width() ).arg( mp.height() );\n"
    "\t\t\t\t\t\tclipMenu->close();\n"
    "\t\t\t\t\t\tqApp->processEvents();\n"
    "\t\t\t\t\t}\n"
    "\t\t\t\t\tif ( auto * hdr = widget<QWidget>( ws, \"AnimWsHeader\" ) ) {\n"
    "\t\t\t\t\t\tconst QPixmap hp = hdr->grab();\n"
    "\t\t\t\t\t\thp.save( st->outDir + QStringLiteral( \"/r6_header_bar.png\" ) );\n"
    "\t\t\t\t\t}\n"
    "\t\t\t\t\tcheck( *st, QStringLiteral( \"picture: (6) the Clip menu open on the header menu bar: %1\" ).arg( line ), !line.isEmpty() );\n"
    "\t\t\t\t}\n",
)

# =====================================================================
# (q) -- the two toggles are icons, one size, and LIT when on
# =====================================================================
sub(
    "\t\t\t\t\t{ \"AnimWsAutoKey\",  QString(), true },\n"
    "\t\t\t\t\t{ \"AnimWsLoop\",     QString(), true },\n"
    "\t\t\t\t};\n",
    "\t\t\t\t\t{ \"AnimWsAutoKey\",  QStringLiteral( \"Auto-key gizmo transforms: after every committed gizmo transform, key the bone at the playhead\" ), true },\n"
    "\t\t\t\t\t/* bungo, 2026-09-12 06:1x: \"Both icons\". The pose switch was a\n"
    "\t\t\t\t\t   WORD in a row of drawings until this lane, so it was not in\n"
    "\t\t\t\t\t   this table at all and nothing held it to the set's rules. */\n"
    "\t\t\t\t\t{ \"AnimWsPose\",     QStringLiteral( \"Pose with the gizmo: hold the selected bone out of the clip's pose so the viewport shows what the transform gizmo (G/R/S) does to it; Insert key then keys that pose at the playhead\" ), true },\n"
    "\t\t\t\t\t/* Loop's tip comes from the shared QAction when the Animation\n"
    "\t\t\t\t\t   menu has been wired up and from the button when it has not, so\n"
    "\t\t\t\t\t   it is the one tip this table does not pin. */\n"
    "\t\t\t\t\t{ \"AnimWsLoop\",     QString(), true },\n"
    "\t\t\t\t};\n",
)

sub(
    "\t\t\t\tcheck( *st, QStringLiteral( \"(q) every tooltip is the sentence it should be, with its shortcut in it (%1 wrong:%2)\" ).arg( wrongTip ).arg( tipTrouble ),\n"
    "\t\t\t\t\t   wrongTip == 0 );\n",
    "\t\t\t\tcheck( *st, QStringLiteral( \"(q) every tooltip is the sentence it should be, with its shortcut in it (%1 wrong:%2)\" ).arg( wrongTip ).arg( tipTrouble ),\n"
    "\t\t\t\t\t   wrongTip == 0 );\n"
    "\t\t\t\t/* ---- THE TWO GIZMO TOGGLES, bungo 2026-09-12 06:1x (\"Both\n"
    "\t\t\t\t * icons\"): each carries a drawing and no text, at the PLAY\n"
    "\t\t\t\t * button's icon size, and the drawing is LIT when the mode is on.\n"
    "\t\t\t\t *\n"
    "\t\t\t\t * \"Lit\" is measured, not asserted: the mean colour of the icon's\n"
    "\t\t\t\t * own ink (the pixels with alpha over half) is taken in both\n"
    "\t\t\t\t * states and compared, and the ON mean is compared to the palette's\n"
    "\t\t\t\t * `accent`, so a button that merely changed its plate colour\n"
    "\t\t\t\t * cannot pass. THE FLOOR is on the same line: Stop has no On\n"
    "\t\t\t\t * pixmap at all, and the same arithmetic must report it unchanged.\n"
    "\t\t\t\t */\n"
    "\t\t\t\t{\n"
    "\t\t\t\t\tauto * play = widget<QToolButton>( ws, \"AnimWsPlay\" );\n"
    "\t\t\t\t\tauto inkMean = []( const QIcon & ic, const QSize & sz, QIcon::State s ) {\n"
    "\t\t\t\t\t\tconst QImage im = ic.pixmap( sz, QIcon::Normal, s ).toImage().convertToFormat( QImage::Format_ARGB32 );\n"
    "\t\t\t\t\t\tqint64 r = 0, g = 0, b = 0, n = 0;\n"
    "\t\t\t\t\t\tfor ( int y = 0; y < im.height(); y++ ) {\n"
    "\t\t\t\t\t\t\tfor ( int x = 0; x < im.width(); x++ ) {\n"
    "\t\t\t\t\t\t\t\tconst QRgb px = im.pixel( x, y );\n"
    "\t\t\t\t\t\t\t\tif ( qAlpha( px ) > 128 ) { r += qRed( px ); g += qGreen( px ); b += qBlue( px ); n++; }\n"
    "\t\t\t\t\t\t\t}\n"
    "\t\t\t\t\t\t}\n"
    "\t\t\t\t\t\treturn n ? QColor( int( r / n ), int( g / n ), int( b / n ) ) : QColor();\n"
    "\t\t\t\t\t};\n"
    "\t\t\t\t\tauto dist = []( const QColor & a, const QColor & b ) {\n"
    "\t\t\t\t\t\tif ( !a.isValid() || !b.isValid() )\n"
    "\t\t\t\t\t\t\treturn -1;\n"
    "\t\t\t\t\t\treturn std::abs( a.red() - b.red() ) + std::abs( a.green() - b.green() ) + std::abs( a.blue() - b.blue() );\n"
    "\t\t\t\t\t};\n"
    "\t\t\t\t\tconst QColor accent( wwSkinColor( \"accent\" ) );\n"
    "\t\t\t\t\tconst char * toggles[2] = { \"AnimWsPose\", \"AnimWsAutoKey\" };\n"
    "\t\t\t\t\tint notIcon = 0, wrongSize = 0, notLit = 0, notAccent = 0;\n"
    "\t\t\t\t\tQString litLine;\n"
    "\t\t\t\t\tfor ( const char * nm : toggles ) {\n"
    "\t\t\t\t\t\tauto * b = widget<QToolButton>( ws, nm );\n"
    "\t\t\t\t\t\tif ( !b || b->icon().isNull() || b->toolButtonStyle() != Qt::ToolButtonIconOnly ) {\n"
    "\t\t\t\t\t\t\tnotIcon++;\n"
    "\t\t\t\t\t\t\tcontinue;\n"
    "\t\t\t\t\t\t}\n"
    "\t\t\t\t\t\tif ( !play || b->iconSize() != play->iconSize() )\n"
    "\t\t\t\t\t\t\twrongSize++;\n"
    "\t\t\t\t\t\tconst QColor off = inkMean( b->icon(), b->iconSize(), QIcon::Off );\n"
    "\t\t\t\t\t\tconst QColor on = inkMean( b->icon(), b->iconSize(), QIcon::On );\n"
    "\t\t\t\t\t\tconst int moved = dist( off, on ), toAccent = dist( on, accent );\n"
    "\t\t\t\t\t\tlitLine += QStringLiteral( \" %1 off=%2 on=%3 moved=%4 fromAccent=%5\" )\n"
    "\t\t\t\t\t\t\t.arg( QString::fromLatin1( nm ).mid( 6 ), off.name(), on.name() ).arg( moved ).arg( toAccent );\n"
    "\t\t\t\t\t\tif ( moved < 60 )\n"
    "\t\t\t\t\t\t\tnotLit++;\n"
    "\t\t\t\t\t\tif ( toAccent < 0 || toAccent > 40 )\n"
    "\t\t\t\t\t\t\tnotAccent++;\n"
    "\t\t\t\t\t}\n"
    "\t\t\t\t\tsay( *st, QStringLiteral( \"  (q) the gizmo toggles' ink:%1 (accent %2)\" ).arg( litLine, accent.name() ) );\n"
    "\t\t\t\t\tcheck( *st, QStringLiteral( \"(q) both gizmo toggles are drawings with no text (%1 are not)\" ).arg( notIcon ), notIcon == 0 );\n"
    "\t\t\t\t\tcheck( *st, QStringLiteral( \"(q) both are the PLAY button's icon size, %1 px (%2 the odd one out)\" )\n"
    "\t\t\t\t\t\t   .arg( play ? play->iconSize().width() : -1 ).arg( wrongSize ), play && wrongSize == 0 );\n"
    "\t\t\t\t\tcheck( *st, QStringLiteral( \"(q) both LIGHT UP when the mode is on (%1 did not move)\" ).arg( notLit ), notLit == 0 );\n"
    "\t\t\t\t\tcheck( *st, QStringLiteral( \"(q) and the lit ink is the palette's accent, not some other colour (%1 off it)\" ).arg( notAccent ), notAccent == 0 );\n"
    "\t\t\t\t\tif ( auto * stop = widget<QToolButton>( ws, \"AnimWsStop\" ) ) {\n"
    "\t\t\t\t\t\tconst int stopMoved = dist( inkMean( stop->icon(), stop->iconSize(), QIcon::Off ),\n"
    "\t\t\t\t\t\t\t\t\t\t\t\t   inkMean( stop->icon(), stop->iconSize(), QIcon::On ) );\n"
    "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(q floor) the same arithmetic calls Stop, which has no lit state, unchanged: moved %1\" ).arg( stopMoved ),\n"
    "\t\t\t\t\t\t\t   stopMoved == 0 );\n"
    "\t\t\t\t\t}\n"
    "\t\t\t\t}\n",
)

# the transport pictures: add an OFF and an ON shot of the two toggles
sub(
    "\t\t\t\t\tcheck( *st, QStringLiteral( \"picture: (q) the transport bar %1x%2 at 1:1 -> %3, and at 2:1 -> %4\" )\n"
    "\t\t\t\t\t\t   .arg( one.width() ).arg( one.height() ).arg( p1, p2 ),\n"
    "\t\t\t\t\t\t   one.width() > 200 && one.save( p1 ) && two.save( p2 ) );\n",
    "\t\t\t\t\tcheck( *st, QStringLiteral( \"picture: (q) the transport bar %1x%2 at 1:1 -> %3, and at 2:1 -> %4\" )\n"
    "\t\t\t\t\t\t   .arg( one.width() ).arg( one.height() ).arg( p1, p2 ),\n"
    "\t\t\t\t\t\t   one.width() > 200 && one.save( p1 ) && two.save( p2 ) );\n"
    "\t\t\t\t\t/* The two toggles OFF and ON, side by side in time: the same bar\n"
    "\t\t\t\t\t   photographed twice at 2:1, with nothing else changed, so the\n"
    "\t\t\t\t\t   lit state is visible and not merely a number above. The\n"
    "\t\t\t\t\t   switches are put back exactly as they were found. */\n"
    "\t\t\t\t\t{\n"
    "\t\t\t\t\t\tauto * pose = widget<QToolButton>( ws, \"AnimWsPose\" );\n"
    "\t\t\t\t\t\tauto * ak = widget<QToolButton>( ws, \"AnimWsAutoKey\" );\n"
    "\t\t\t\t\t\tconst bool pose0 = pose && pose->isChecked(), ak0 = ak && ak->isChecked();\n"
    "\t\t\t\t\t\tauto shotAt = [&]( bool on, const char * leaf ) {\n"
    "\t\t\t\t\t\t\tif ( pose ) pose->setChecked( on );\n"
    "\t\t\t\t\t\t\tif ( ak ) ak->setChecked( on );\n"
    "\t\t\t\t\t\t\tqApp->processEvents();\n"
    "\t\t\t\t\t\t\tconst QPixmap g = transport->grab();\n"
    "\t\t\t\t\t\t\tconst QPixmap g2 = g.scaled( g.width() * 2, g.height() * 2, Qt::IgnoreAspectRatio, Qt::FastTransformation );\n"
    "\t\t\t\t\t\t\tconst QString path = st->outDir + QLatin1String( \"/\" ) + QLatin1String( leaf );\n"
    "\t\t\t\t\t\t\treturn g2.width() > 400 && g2.save( path ) ? path : QString();\n"
    "\t\t\t\t\t\t};\n"
    "\t\t\t\t\t\tconst QString offShot = shotAt( false, \"transport_2x_off.png\" );\n"
    "\t\t\t\t\t\tconst QString onShot = shotAt( true, \"transport_2x_on.png\" );\n"
    "\t\t\t\t\t\tif ( pose ) pose->setChecked( pose0 );\n"
    "\t\t\t\t\t\tif ( ak ) ak->setChecked( ak0 );\n"
    "\t\t\t\t\t\tqApp->processEvents();\n"
    "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"picture: (q) the two toggles off -> %1 and on -> %2, and both switches put back (%3, %4)\" )\n"
    "\t\t\t\t\t\t\t   .arg( offShot, onShot ).arg( pose && pose->isChecked() == pose0 ).arg( ak && ak->isChecked() == ak0 ),\n"
    "\t\t\t\t\t\t\t   !offShot.isEmpty() && !onShot.isEmpty()\n"
    "\t\t\t\t\t\t\t   && pose && pose->isChecked() == pose0 && ak && ak->isChecked() == ak0 );\n"
    "\t\t\t\t\t}\n",
)

# =====================================================================
# (o) -- Ctrl+A: the settled number counts too
# =====================================================================
sub(
    "\t\t\t\t\tcheck( *st, QStringLiteral( \"(o) Ctrl+A selects every row: %1 of %2 (one pump later %3, once the list rebuilds %4)\" )\n"
    "\t\t\t\t\t       .arg( selNow ).arg( list->count() ).arg( selPumped ).arg( selSettled ),\n"
    "\t\t\t\t\t       sa && selNow == list->count() && list->count() >= 3 );\n",
    "\t\t\t\t\t/* All THREE numbers are the check now (lane UINOTES2,\n"
    "\t\t\t\t\t * 2026-09-12). It used to check the first alone and print the\n"
    "\t\t\t\t\t * other two, which was right while the defect was being\n"
    "\t\t\t\t\t * measured: the key's own effect is one fact and what eats the\n"
    "\t\t\t\t\t * selection afterwards is another. Now that both places are\n"
    "\t\t\t\t\t * fixed -- the driven re-entry AND the rebuild -- a selection\n"
    "\t\t\t\t\t * that survives the key and dies 50 ms later is a failure, not a\n"
    "\t\t\t\t\t * printed number. */\n"
    "\t\t\t\t\tcheck( *st, QStringLiteral( \"(o) Ctrl+A selects every row: %1 of %2 (one pump later %3, once the list rebuilds %4)\" )\n"
    "\t\t\t\t\t       .arg( selNow ).arg( list->count() ).arg( selPumped ).arg( selSettled ),\n"
    "\t\t\t\t\t       sa && selNow == list->count() && selPumped == list->count()\n"
    "\t\t\t\t\t       && selSettled == list->count() && list->count() >= 3 );\n",
)

bad = 0
for anchor, _ in subs:
    n = text.count(anchor)
    if n != 1:
        bad += 1
        print("ANCHOR x%d (want 1): %s" % (n, anchor.splitlines()[0][:100]))
if bad:
    sys.exit("refused: %d anchor(s) did not match exactly once; nothing written" % bad)

for anchor, rep in subs:
    text = text.replace(anchor, rep, 1)

out = text.encode("utf-8")
assert out.count(b"\r") == 0
with open(SRC, "wb") as f:
    f.write(out)
print("written %s: CR %d LF %d bytes %d (was %d), %d substitutions"
      % (SRC, out.count(b"\r"), out.count(b"\n"), len(out), len(orig), len(subs)))
