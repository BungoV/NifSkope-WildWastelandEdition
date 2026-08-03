#define _USE_MATH_DEFINES
/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENCE BLOCK *****/

#include "nifskope.h"
#include "starterscene.h"
#include "ui_nifskope.h"

#include "bakegeom.h"
#include "glview.h"
#include "nifmerge.h"
#include "gl/gltools.h"
#include "message.h"
#include "nifsnapshot.h"
#include "shortcutregistry.h"
#include "spellbook.h"
#include "ui/widgets/spellpalette.h"
#include "spells/blocks.h"
#include "spells/normaltransfer.h"
#include "wwskin.h"
#include "skeletontools.h"

#include <functional>

#include <QProcessEnvironment>
#include <QScopeGuard>
#include "version.h"
#include "gl/controllers.h"
#include "gl/glparticles.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/renderer.h"
#include "model/kfmmodel.h"
#include "model/nifmodel.h"
#include "model/nifproxymodel.h"
#include "ui/widgets/filebrowser.h"
#include "ui/widgets/physicspanel.h"
#include "ui/widgets/fileselect.h"
#include "ui/widgets/floatslider.h"
#include "ui/widgets/floatedit.h"
#include "ui/widgets/lightingwidget.h"
#include "ui/widgets/nifview.h"
#include "ui/widgets/refrbrowser.h"
#include "ui/widgets/inspect.h"
#include "ui/widgets/timeline.h"
#include "ui/widgets/valueedit.h"
#include "ui/widgets/wwnumberfield.h"
#include "ui/widgets/xmlcheck.h"
#include "ui/about_dialog.h"
#include "ui/settingsdialog.h"
#include "ui/settingspane.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBuffer>
#include <QButtonGroup>
#include <QByteArray>
#include <QCheckBox>
#include <QStandardItemModel>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QMimeData>
#include <QDebug>
#include <QElapsedTimer>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDialog>
#include <QFrame>
#include <QGridLayout>
#include <QScreen>
#include <QInputDialog>
#include <QGroupBox>
#include <QLabel>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QProgressBar>
#include <QMessageBox>
#include <QProgressDialog>
#include "spells/animationsetup.h"

#include <QListWidget>
#include <QPushButton>
#include <QScrollBar>
#include <QScrollArea>
#include <QSlider>
#include <QTabWidget>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSettings>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QWidgetAction>

#include <QProcess>
#include <QStyleFactory>
#include <QStyle>
#include <QRegularExpression>
#include <QPainter>
#include <QLineEdit>

#include <memory>

namespace {
//! QMenu normally closes after every QAction trigger. Contribution checkboxes
//! are more useful as a small mixer, so actions explicitly marked keepMenuOpen
//! can be toggled repeatedly without reopening the shading popover.
class PersistentActionMenu final : public QMenu
{
public:
	explicit PersistentActionMenu( QWidget * parent = nullptr ) : QMenu( parent ) {}

protected:
	void mouseReleaseEvent( QMouseEvent * event ) override
	{
		QAction * action = actionAt( event->position().toPoint() );
		if ( event->button() == Qt::LeftButton && action && action->isEnabled()
			&& action->property( "keepMenuOpen" ).toBool() ) {
			action->trigger();
			event->accept();
			return;
		}
		QMenu::mouseReleaseEvent( event );
	}

	void keyPressEvent( QKeyEvent * event ) override
	{
		QAction * action = activeAction();
		if ( action && action->isEnabled() && action->property( "keepMenuOpen" ).toBool()
			&& ( event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space ) ) {
			action->trigger();
			event->accept();
			return;
		}
		QMenu::keyPressEvent( event );
	}
};

/* The Blender number field lived here, as WwNumberField.
 *
 * It is now ui/widgets/wwnumberfield.h. It was the ORIGINAL and the best of
 * the five copies, and it was copied four times for one reason: it sat in the
 * anonymous namespace above, so no other panel could reach it. Moving it out
 * is the whole fix; everything else is deleting the copies.
 */

//! Set a redo panel's title, keeping the ˅ / ˃ collapse marker in sync
static void tlSetPanelTitle( QFrame * panel, QToolButton * title, const QString & text )
{
	panel->setProperty( "titleText", text );
	bool col = panel->property( "collapsed" ).toBool();
	title->setText( ( col ? QStringLiteral( "˃  " ) : QStringLiteral( "˅  " ) ) + text );
}

//! Blender: clicking the panel header collapses it to just the title bar
static void tlTogglePanelCollapse( QFrame * panel, QToolButton * title, QWidget * body )
{
	bool col = !panel->property( "collapsed" ).toBool();
	panel->setProperty( "collapsed", col );
	body->setVisible( !col );
	tlSetPanelTitle( panel, title, panel->property( "titleText" ).toString() );
	panel->adjustSize();
	panel->resize( panel->sizeHint() );	// tool windows don't shrink on their own
}
}

QString nstypes::operator""_uip( const char * str, size_t )
{
	QString u;

#ifndef QT_NO_DEBUG
	u = "UI/Debug/";
#else
	u = "UI/";
#endif

	return u + QString( str );
}

using namespace nstypes;
using namespace nstheme;


// The dark palette is the PBR Material Editor's (PBRMaterialEditorQt,
// resources/style.qss) so the two tools read as one product. Bump
// themePaletteVersion in loadTheme() when these change, or existing installs
// keep the colours already written to their settings.
QColor NifSkope::defaultsDark[6] = {
	QColor( 0x30, 0x32, 0x36 ), /// nstheme::Base
	QColor( 0x2d, 0x30, 0x34 ), /// nstheme::BaseAlt
	QColor( 0xe6, 0xe8, 0xeb ), /// nstheme::Text
	QColor( 0x3d, 0x6f, 0x9f ), /// nstheme::Highlight
	QColor( 0xff, 0xff, 0xff ), /// nstheme::HighlightText
	QColor( 0xf0, 0xa5, 0x4a )  /// nstheme::BrightText
};

QColor NifSkope::defaultsLight[6] = {
	QColor( 245, 245, 245 ), /// nstheme::Base
	QColor( 255, 255, 255 ), /// nstheme::BaseAlt
	Qt::black,               /// nstheme::Text
	QColor( 42, 130, 218 ),  /// nstheme::Highlight
	Qt::white,               /// nstheme::HighlightText
	Qt::red                  /// nstheme::BrightText
};


/*! Skin surfaces — the single source for every colour in `res/style.qss` (as
 * `${name}`) AND for the per-widget stylesheets in the docks (via
 * wwSkinColor(), declared in wwskin.h).
 *
 * The dark column is the PBR Material Editor's palette, so the two tools read
 * as one product ahead of the merge. Both columns exist because ONE stylesheet
 * serves both themes: hardcoding the dark values would leave the light theme as
 * dark widgets on a light window.
 *
 * Add a colour here rather than writing a literal anywhere else.
 */
static const struct WwSkinVar { const char * name; const char * dark; const char * light; } skinVars[] = {
	{ "bg",           "#303236", "#f0f0f0" },  // content background
	{ "bgWin",        "#292b2f", "#e4e4e4" },  // window / dialog plate
	{ "bgBar",        "#25272a", "#dcdcdc" },  // menu, tool and status bars
	{ "bgPanel",      "#27292d", "#e8e8e8" },  // docks, views, panels
	{ "bgAlt",        "#2d3034", "#f6f6f6" },  // alternating rows
	{ "bgCard",       "#2b2d31", "#ffffff" },  // framed sections, floating panels
	{ "bgInput",      "#3c3f44", "#ffffff" },  // line edits, combos, spins
	{ "bgBtn",        "#3a3d42", "#e9e9e9" },
	{ "bgBtnHover",   "#484c52", "#dadada" },
	{ "bgBtnDown",    "#355f86", "#c4d6e6" },
	{ "bgHeader",     "#2c2f33", "#e0e0e0" },  // header view sections
	{ "border",       "#4d5056", "#b4b4b4" },
	{ "borderDim",    "#3a3d42", "#c8c8c8" },
	{ "borderStrong", "#1b1c1f", "#9a9a9a" },  // panel outlines, splitters
	{ "focus",        "#5d92c5", "#3d78ae" },
	{ "scroll",       "#62666c", "#b0b0b0" },  // scrollbar handle
	{ "scrollHover",  "#777c83", "#909090" },
	{ "text",         "#e6e8eb", "#202020" },
	{ "textMuted",    "#aeb3ba", "#5a5f66" },
	{ "textBright",   "#f2f3f5", "#000000" },
	{ "accent",       "#f0a54a", "#c07000" },  // selection accent, active toggles
	{ "accentText",   "#ffb54a", "#a05a00" },  // text on an accented toggle
	{ "accentBg",     "#40331f", "#f6e6c8" },  // amber toggle plate
	{ "danger",       "#ff8484", "#c0392b" },  // invalid / error text
	{ "viewport",     "#2b2d31", "#c8ccd0" },  // GL clear colour (Render settings default)
	/* Tree/list selection, shared by every view that highlights rows.
	 *
	 * These four values were hardcoded identically in six files -- the Block
	 * List model, the Loaded NIFs delegate, the Collision and Materials tree
	 * sheets, the Materials recolor pass and the Rigging bone delegate -- with
	 * zero drift between them, which is what made them worth naming rather than
	 * reconciling. Two more views had already drifted: Pose to #2b3b5c, and
	 * Skeleton to wwSkinColor("danger"), so a multi-selected bone rendered in
	 * the same red that means "missing texture" elsewhere.
	 *
	 * "Active" is the primary of a multi-selection, not Qt window focus.
	 */
	{ "selBgActive",   "#4a7ab0", "#4a7ab0" },  // primary selected row
	{ "selBgInactive", "#2b425f", "#b9cbe0" },  // other selected rows
	{ "selTextActive", "#ff9d00", "#a05a00" },  // text on the primary row
	{ "selTextInactive", "#ff7200", "#8a4a00" },  // text on the others
};

//! Which column wwSkinColor() serves. loadTheme() keeps it current; defaults to
//! dark for anything built before the first theme load.
static bool wwSkinLightTheme = false;

QString wwSkinColor( const char * name )
{
	for ( const WwSkinVar & v : skinVars ) {
		if ( qstrcmp( v.name, name ) == 0 )
			return QString::fromLatin1( wwSkinLightTheme ? v.light : v.dark );
	}

	qWarning() << "wwSkinColor: unknown skin colour" << name;
	return QString();
}

/*! The shared selection look for a QTreeWidget / QTreeView / QListWidget.
 *
 *  Two views wrote this sheet out byte-for-byte identically (Collision and
 *  Materials); four more wrote the same four colours as C++ brushes.
 *
 *  A NOTE ON WHAT ":!active" MEANS HERE, because it is not what the C++ sites
 *  mean. Qt's `:!active` is "this view does not have window focus". The C++
 *  sites use the same two colours for something different -- "this row is
 *  selected but is not the ACTIVE object of a multi-selection". A stylesheet
 *  cannot express that second idea, so those sites read the four variables
 *  directly rather than taking this sheet. Both readings share one palette; only
 *  the trigger differs, and conflating them is part of why the values ended up
 *  repeated by hand in six files.
 */
QString wwSelectionTreeQss()
{
	return QStringLiteral(
		"QTreeWidget::item:selected, QTreeView::item:selected, QListWidget::item:selected"
		" { background: %1; color: %2; }"
		"QTreeWidget::item:selected:!active, QTreeView::item:selected:!active,"
		" QListWidget::item:selected:!active { background: %3; color: %4; }" )
		.arg( wwSkinColor( "selBgActive" ), wwSkinColor( "selTextActive" ),
			  wwSkinColor( "selBgInactive" ), wwSkinColor( "selTextInactive" ) );
}

QLabel * wwHeading( const QString & text, QWidget * parent )
{
	auto * l = new QLabel( text, parent );
	l->setStyleSheet( QStringLiteral( "QLabel { font-weight: 600; }" ) );
	return l;
}

/*! The toolbar-selector look: the viewport mode dropdown, the mode row's menu
 * buttons, and the Panels / Workspaces selectors. One definition so the sites
 * cannot drift apart — they were three copies of the same greys.
 *
 * FLAT, in the style of Blender's header: no border and no background plate
 * until the pointer is on it. A top bar with ~15 controls, each drawn in its own
 * bordered box, spends most of its width on chrome and reads as fifteen separate
 * things competing for attention; without the boxes the glyphs and labels carry
 * it, and the row shrinks by roughly a third. Hover and checked still give plain
 * feedback, which is all the boxes were really providing.
 *
 * Not weight 600 either — bold labels on every control is the same problem in
 * type.
 */
/*! Desaturate a resource icon onto the toolbar's grey.
 *
 *  The drawn icons (tlMakeIcon) are already one greyscale family; the ones that
 *  come out of res/ are not, so a handful of full-colour PNGs sit among them
 *  looking like the only things worth clicking. This is luminance followed by a
 *  lift toward the bar's light-grey tone, so the result matches the drawn glyphs
 *  rather than merely being colourless - a straight qGray leaves the darker art
 *  almost invisible on a dark toolbar.
 *
 *  Was a lambda inside the shading-contributions menu, which is where the
 *  approach was proven. Promoted to file scope so the Render menu can use the
 *  same pass rather than a second one that drifts from it.
 *
 *  Kept at 22 px because that is the size the menus ask for; a QIcon built from
 *  one pixmap scales down cleanly and these are never drawn larger.
 */
QIcon wwGreyscaleIcon( const QIcon & icon )
{
	QPixmap pm = icon.pixmap( 22, 22 );
	if ( pm.isNull() )
		return icon;
	QImage img = pm.toImage().convertToFormat( QImage::Format_ARGB32 );
	for ( int y = 0; y < img.height(); y++ ) {
		for ( int x = 0; x < img.width(); x++ ) {
			QColor c = img.pixelColor( x, y );
			if ( c.alpha() == 0 )
				continue;
			// luminance, then lift toward the toolbar's light-grey tone
			int g = qGray( c.red(), c.green(), c.blue() );
			g = 70 + g * 160 / 255;
			img.setPixelColor( x, y, QColor( g, g, g, c.alpha() ) );
		}
	}
	return QIcon( QPixmap::fromImage( img ) );
}

/*! A group boundary on the top row, spaced the way Blender spaces one.
 *
 *  Blender's header separates groups with WHITESPACE and no rule at all; this
 *  fork keeps the rule, so the job here is to give it the room Blender gives a
 *  gap. A bare addSeparator() draws a hairline hard against the button on either
 *  side, which reads as a divider between two adjacent things rather than the
 *  edge of a group - and with eight of them in a row the effect is a picket
 *  fence, where every boundary looks equally important.
 *
 *  Padding on both sides, symmetric, one number everywhere. The rule then has to
 *  be earned: it goes only where a genuine group ends, which is why the trailing
 *  Animation / Collision / Panels run has none inside it - those three are one
 *  group, and ruling between them said they were three.
 */
void wwGroupBreak( QToolBar * bar )
{
	if ( !bar )
		return;
	// Named so the duplicate-separator cleanup in initMenu can see past it: two
	// groups meeting would otherwise contribute a rule each, with a pad between
	// them stopping the collapse, and the row draws a double bar.
	auto pad = [bar]() {
		QWidget * gap = new QWidget( bar );
		gap->setObjectName( QStringLiteral( "wwGroupPad" ) );
		gap->setFixedWidth( 7 );
		bar->addWidget( gap );
	};
	pad();
	bar->addSeparator();
	pad();
}

/*! An action's own icon with a tick beside it.
 *
 *  QMenu paints the icon and the check indicator in the SAME column, so a
 *  checkable item that has an icon silently loses its checkmark — which is
 *  every overlay toggle in the Overlays menu. You could see the glyph and not
 *  whether the thing was on.
 *
 *  Compositing the tick into the icon is the only way to show both without
 *  writing a QStyle. The base icon is captured once and never re-composited, so
 *  toggling cannot stack ticks.
 */
static QIcon wwTickedIcon( const QIcon & base, bool checked )
{
	/* SAME canvas as the source icon - the tick goes in the free space at its
	 * right, it does not make the icon wider.
	 *
	 * The first version built a 30x16 pixmap for a 16x16 slot, so Qt scaled the
	 * whole thing down to fit and every glyph in the menu came out shrunken.
	 * The glyphs in here are drawn well inside their box, which is the free
	 * space this uses.
	 */
	const int px = 16;
	QPixmap pm( px, px );
	pm.fill( Qt::transparent );
	QPainter p( &pm );
	p.setRenderHint( QPainter::Antialiasing );
	base.paint( &p, QRect( 0, 0, px, px ) );
	if ( checked ) {
		QPen pen( QColor( wwSkinColor( "accent" ) ) );
		pen.setWidthF( 1.6 );
		pen.setCapStyle( Qt::RoundCap );
		pen.setJoinStyle( Qt::RoundJoin );
		p.setPen( pen );
		// bottom-right corner, clear of a centred glyph
		p.drawLine( QPointF( px - 6.5, px - 5.0 ), QPointF( px - 4.5, px - 2.8 ) );
		p.drawLine( QPointF( px - 4.5, px - 2.8 ), QPointF( px - 0.8, px - 8.0 ) );
	}
	p.end();
	return QIcon( pm );
}

//! Make a checkable action in `m` show its state even though it has an icon.
static void wwShowCheckBesideIcon( QAction * a, QMenu * m )
{
	if ( !a || !a->isCheckable() || a->icon().isNull() )
		return;		// no icon: Qt's own checkmark already shows
	const QIcon base = a->icon();
	auto sync = [a, base]() { a->setIcon( wwTickedIcon( base, a->isChecked() ) ); };
	sync();
	QObject::connect( a, &QAction::toggled, a, [sync]( bool ) { sync(); } );
	// also on open: some of these are driven from elsewhere (settings restore,
	// the visibility menu) and toggled() is not the only way they change
	QObject::connect( m, &QMenu::aboutToShow, a, sync );
}

QString wwBoxedButtonQss( const QString & padding )
{
	return QStringLiteral(
		"QToolButton { padding: %1; border: 1px solid transparent; border-radius: 3px;"
		" background: transparent; color: %2; }"
		"QToolButton:hover { background: %3; color: %4; }"
		"QToolButton:pressed, QToolButton::menu-button:pressed { background: %5; }"
		"QToolButton:checked { background: %5; }"
		"QToolButton:disabled { color: %6; }"
		"QToolButton::menu-indicator { subcontrol-position: right center;"
		" subcontrol-origin: padding; }" )
		.arg( padding, wwSkinColor( "text" ), wwSkinColor( "bgBtnHover" ),
			  wwSkinColor( "textBright" ), wwSkinColor( "bgBtnDown" ),
			  wwSkinColor( "textMuted" ) );
}



//! @file nifskope_ui.cpp UI logic for %NifSkope's main window.

/*! Should a window with no file open on the starter cube?
 *
 * Off for any harness run: every WW_* test expects the empty document it always
 * got, and a cube would change block numbers under all of them. Same guard
 * saveUi() uses, for the same reason.
 */
static bool startupCubeWanted()
{
	// WW_STARTER_SHOT is the harness FOR this path, so it is the one WW_ variable
	// that must not switch it off.
	if ( !qEnvironmentVariableIsSet( "WW_STARTER_SHOT" )
		&& !qEnvironmentVariableIsSet( "WW_UI_SHOT" ) ) {
		const QStringList envKeys = QProcessEnvironment::systemEnvironment().keys();
		for ( const QString & key : envKeys ) {
			if ( key.startsWith( QLatin1String( "WW_" ) ) )
				return false;
		}
	}
	QSettings settings;
	return settings.value( QStringLiteral( "Settings/Nif/Startup Defaults/New Document Cube" ),
						   true ).toBool();
}

NifSkope * NifSkope::createWindow( const QString & fname, bool background )
{
	NifSkope * primary = nullptr;
	for ( NifSkope * document : NifSkope::openDocuments() )
		if ( document && document->isVisible() ) {
			primary = document;
			if ( document->isActiveWindow() ) break;
		}
	NifSkope * skope = new NifSkope( background );
	skope->setAttribute( Qt::WA_DeleteOnClose );
	skope->loadTheme();
	skope->workspaceRoot = background && primary
		? ( primary->workspaceRoot ? primary->workspaceRoot : primary ) : skope;
	if ( !background ) {
		skope->restoreUi();
		/* WW_WINDOW_AT=x,y: place the window BEFORE showing it, and do not raise.
		 *
		 * For running a GUI harness on a second monitor while someone is working on
		 * the first. Moving it after show() is not the same thing -- the window
		 * appears on the primary monitor for a frame and then jumps, which is
		 * exactly the interruption this exists to avoid -- and raise() would take
		 * focus even once it is out of the way.
		 */
		const QString at = qEnvironmentVariable( "WW_WINDOW_AT" );
		const QStringList xy = at.split( QLatin1Char( ',' ) );
		if ( xy.size() == 2 ) {
			skope->move( xy.at( 0 ).toInt(), xy.at( 1 ).toInt() );
			skope->show();
		} else {
			skope->show();
			skope->raise();
		}
	}

	// TEMP DIAGNOSTIC (WW_EXTRUDE_TEST=1, remove when the append-row condition
	// bug is fixed): after the file loads, grow "Vertex Data" on the first
	// skinned BSTriShape by one row, probe the new row's structure/conditions,
	// dump to ww_extrude_test.log next to the exe, then quit.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_EXTRUDE_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QFile logf( QApplication::applicationDirPath() + "/ww_extrude_test.log" );
			if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
				return;
			QTextStream log( &logf );
			NifModel * nif = skope->getNifModel();
			do {
				if ( !ok || !nif ) { log << "load failed\n"; break; }
				int sb = -1;
				for ( int b = 0; b < nif->getBlockCount(); b++ ) {
					QModelIndex iB = nif->getBlockIndex( b );
					if ( nif->blockInherits( iB, "BSTriShape" )
						&& nif->get<int>( iB, "Num Vertices" ) > 100 ) {
						sb = b;
						break;
					}
				}
				if ( sb < 0 ) { log << "no BSTriShape\n"; break; }
				QModelIndex iShape = nif->getBlockIndex( sb );
				QModelIndex iVD = nif->getIndex( iShape, "Vertex Data" );
				const int oldNV = nif->get<int>( iShape, "Num Vertices" );
				log << "block " << sb << " oldNV " << oldNV << "\n";
				auto dumpRow = [&]( int r ) {
					QModelIndex row = nif->getIndex( iVD, r );
					log << "row " << r << " children " << nif->rowCount( row ) << ":";
					for ( int c = 0; c < nif->rowCount( row ); c++ ) {
						QModelIndex ch = nif->getIndex( row, c, 0 );
						log << " [" << nif->itemName( ch );
						log << ( nif->getIndex( row, nif->itemName( ch ) ).isValid() ? "+" : "-" ) << "]";
					}
					log << "\n";
				};
				dumpRow( 0 );
				// faithful replica of tlExtrudeApplyPlan's spur path, wrapped in
				// nifSnapshotOp like the real op, run twice (extrude the extruded)
				std::function<void( const QModelIndex &, const QModelIndex & )> copyVals =
					[&]( const QModelIndex & src, const QModelIndex & dst ) {
						int rc = nif->rowCount( src );
						if ( rc > 0 && rc == nif->rowCount( dst ) ) {
							for ( int r = 0; r < rc; r++ )
								copyVals( nif->getIndex( src, r ), nif->getIndex( dst, r ) );
						} else {
							nif->setIndexValue( dst, nif->getValue( src ) );
						}
					};
				auto spurExtrude = [&]( int srcVert ) {
					QModelIndex iTris = nif->getIndex( iShape, "Triangles" );
					const int nv = nif->get<int>( iShape, "Num Vertices" );
					const int nt = nif->get<int>( iShape, "Num Triangles" );
					const int ds = nif->get<int>( iShape, "Data Size" );
					const int stride = ( nv > 0 ) ? ( ds - nt * 6 ) / nv : 0;
					nif->set<int>( iShape, "Num Vertices", nv + 1 );
					nif->updateArraySize( iVD );
					copyVals( nif->getIndex( iVD, srcVert ), nif->getIndex( iVD, nv ) );
					nif->set<int>( iShape, "Num Triangles", nt + 1 );
					nif->updateArraySize( iTris );
					nif->set<Triangle>( nif->getIndex( iTris, nt ),
						Triangle( quint16( srcVert ), quint16( nv ), quint16( nv ) ) );
					nif->set<int>( iShape, "Data Size", ( nv + 1 ) * stride + ( nt + 1 ) * 6 );
					return nv;	// the new vert
				};
				// replicate the user's failing sequence: DELETE a vertex first
				// (forward-compact + shrink, like tlDeleteGeometry), then extrude
				auto deleteVert = [&]( int victim ) {
					nif->setState( BaseModel::Processing );	// same fix as deleteGeometry
					QModelIndex iTris = nif->getIndex( iShape, "Triangles" );
					const int nv = nif->get<int>( iShape, "Num Vertices" );
					const int nt = nif->get<int>( iShape, "Num Triangles" );
					const int ds = nif->get<int>( iShape, "Data Size" );
					const int stride = ( nv > 0 ) ? ( ds - nt * 6 ) / nv : 0;
					QVector<int> remap( nv, -1 );
					int j = 0;
					for ( int v = 0; v < nv; v++ ) {
						if ( v == victim )
							continue;
						if ( j != v )
							copyVals( nif->getIndex( iVD, v ), nif->getIndex( iVD, j ) );
						remap[v] = j++;
					}
					QVector<Triangle> kept;
					for ( int t = 0; t < nt; t++ ) {
						Triangle tri = nif->get<Triangle>( nif->getIndex( iTris, t ) );
						int a = remap[tri[0]], b = remap[tri[1]], c = remap[tri[2]];
						if ( a < 0 || b < 0 || c < 0 )
							continue;
						kept.append( Triangle( quint16( a ), quint16( b ), quint16( c ) ) );
					}
					nif->set<int>( iShape, "Num Vertices", j );
					nif->updateArraySize( iVD );
					nif->set<int>( iShape, "Num Triangles", kept.size() );
					nif->updateArraySize( iTris );
					for ( int t = 0; t < kept.size(); t++ )
						nif->set<Triangle>( nif->getIndex( iTris, t ), kept[t] );
					nif->set<int>( iShape, "Data Size", j * stride + kept.size() * 6 );
					nif->restoreState();
					nif->dataChanged( iShape, iShape );
				};
				// row-hiding check: Skyrim-only fields must be hidden on a FO4 NIF
				auto hideMark = []( const QString & m ) {
					QFile f( QApplication::applicationDirPath() + "/ww_hide.log" );
					if ( f.open( QIODevice::Append | QIODevice::Text ) )
						QTextStream( &f ) << m << "\n";
				};
				// sweep EVERY shader property block: click it like a user would,
				// then count version-mismatched rows the view still shows
				auto sweepHiding = [&]( const char * when ) {
					for ( int b = 0; b < nif->getBlockCount(); b++ ) {
						QModelIndex iB = nif->getBlockIndex( b );
						if ( !nif->isNiBlock( iB, "BSLightingShaderProperty" ) )
							continue;
						if ( skope->list->model() == skope->proxy )
							skope->list->setCurrentIndex( skope->proxy->mapFrom( iB, QModelIndex() ) );
						else
							skope->list->setCurrentIndex( iB );
						qApp->processEvents();
						auto countHidden = [&]( const QModelIndex & parent ) {
							int n = 0;
							for ( int r = 0; r < nif->rowCount( parent ); r++ )
								if ( skope->tree->QTreeView::isRowHidden( r, parent ) )
									n++;
							return n;
						};
						const bool rootMatches = ( skope->tree->rootIndex() == iB );
						const int hidViaRoot = countHidden( skope->tree->rootIndex() );
						const int hidViaBlock = countHidden( iB );
						skope->tree->refreshRowHiding();
						const int hidAfterManual = countHidden( iB );
						log << when << ": block " << b << " rootMatches=" << rootMatches
							<< " hidViaRoot=" << hidViaRoot << " hidViaBlock=" << hidViaBlock
							<< " afterManualRefresh=" << hidAfterManual << "\n";
					}
				};
				sweepHiding( "sweep" );
				auto dumpHiding = [&]( const char * when ) {
					hideMark( QString( "=== %1: select block ===" ).arg( when ) );
					int pb = -1;
					for ( int b = 0; b < nif->getBlockCount(); b++ )
						if ( nif->isNiBlock( nif->getBlockIndex( b ), "BSLightingShaderProperty" ) ) {
							pb = b;
							break;
						}
					if ( pb < 0 ) {
						log << when << ": no BSLightingShaderProperty\n";
						return;
					}
					QModelIndex iP = nif->getBlockIndex( pb );
					// mimic the user's actual interaction: a click in the Block
					// List (proxy) — not a programmatic select
					if ( skope->list->model() == skope->proxy ) {
						QModelIndex pIdx = skope->proxy->mapFrom( iP, QModelIndex() );
						log << when << ": proxy click, proxyValid=" << pIdx.isValid() << "\n";
						skope->list->setCurrentIndex( pIdx );
					} else {
						skope->list->setCurrentIndex( iP );
					}
					qApp->processEvents();
					log << when << ": state=" << int( nif->getState() )
						<< " treeModelIsNif=" << ( skope->tree->model() == nif )
						<< " rootValid=" << skope->tree->rootIndex().isValid()
						<< " rootIsBlock=" << ( skope->tree->rootIndex() == iP ) << "\n";
					log << when << ": block " << pb << " rows visible in the VIEW"
						" (PRED! marks rows the predicate says to hide):";
					for ( int r = 0; r < nif->rowCount( iP ); r++ ) {
						QModelIndex ch = nif->getIndex( iP, r, 0 );
						// NifTreeView::isRowHidden takes the ROW'S OWN index
						const bool predHidden = skope->tree->isRowHidden( r, ch );
						const bool viewHidden = skope->tree->QTreeView::isRowHidden( r, iP );
						if ( !viewHidden )
							log << " [" << nif->itemName( ch )
								<< "/" << nif->itemStrType( ch )
								<< ( predHidden ? "|PRED!" : "" ) << "]";
					}
					log << "\n";
					hideMark( QString( "=== %1: dump done ===" ).arg( when ) );
					// mechanism tests: does a direct setRowHidden stick, and does a
					// dataChanged-triggered updateConditions fix the whole block?
					int predRow = -1;
					for ( int r = 0; r < nif->rowCount( iP ) && predRow < 0; r++ )
						if ( skope->tree->isRowHidden( r, nif->getIndex( iP, r, 0 ) ) )
							predRow = r;
					if ( predRow >= 0 ) {
						skope->tree->setRowHidden( predRow, iP, true );
						log << when << ": direct setRowHidden stick = "
							<< skope->tree->QTreeView::isRowHidden( predRow, iP ) << "\n";
						QModelIndex ch = nif->getIndex( iP, predRow, 0 );
						nif->dataChanged( ch, ch );
						qApp->processEvents();
						int visiblePred = 0;
						for ( int r = 0; r < nif->rowCount( iP ); r++ )
							if ( skope->tree->isRowHidden( r, nif->getIndex( iP, r, 0 ) )
								&& !skope->tree->QTreeView::isRowHidden( r, iP ) )
								visiblePred++;
						log << when << ": PRED-hidden rows still visible after dataChanged refresh: "
							<< visiblePred << "\n";
					}
				};
				dumpHiding( "hiding before ops" );

				// the user's environment: shape selected (Block Details showing its
				// rows, views reacting to every dataChanged) and the event loop
				// running between operations
				skope->select( iShape );
				qApp->processEvents();
				QElapsedTimer timer;
				timer.start();
				nifSnapshotOp( nif, "probe delete", [&]() { deleteVert( 0 ); } );
				log << "delete of one vert took " << timer.elapsed() << " ms\n";
				qApp->processEvents();
				log << "after delete: numVerts " << nif->get<int>( iShape, "Num Vertices" ) << "\n";
				dumpRow( nif->get<int>( iShape, "Num Vertices" ) - 1 );

				int v1 = -1, v2 = -1;
				nifSnapshotOp( nif, "probe extrude 1", [&]() { v1 = spurExtrude( 0 ); } );
				qApp->processEvents();
				dumpRow( v1 );
				log << "after ex1: new row Normal valid: "
					<< nif->getIndex( nif->getIndex( iVD, v1 ), "Normal" ).isValid() << "\n";
				log.flush();
				nifSnapshotOp( nif, "probe extrude 2", [&]() { v2 = spurExtrude( v1 ); } );
				qApp->processEvents();
				dumpRow( v1 );
				dumpRow( v2 );
				log << "after ex2: row v1 Normal valid: "
					<< nif->getIndex( nif->getIndex( iVD, v1 ), "Normal" ).isValid()
					<< ", row v2 Normal valid: "
					<< nif->getIndex( nif->getIndex( iVD, v2 ), "Normal" ).isValid() << "\n";
				// snapshot undo/redo round-trip (reload from serialized bytes),
				// then probe again — the suspected poisoning path
				if ( nif->undoStack ) {
					nif->undoStack->undo();
					nif->undoStack->redo();
					iShape = nif->getBlockIndex( sb );
					iVD = nif->getIndex( iShape, "Vertex Data" );
					dumpRow( v1 );
					dumpRow( v2 );
					log << "after undo+redo reload: row v1 Normal valid: "
						<< nif->getIndex( nif->getIndex( iVD, v1 ), "Normal" ).isValid()
						<< ", row v2 Normal valid: "
						<< nif->getIndex( nif->getIndex( iVD, v2 ), "Normal" ).isValid() << "\n";
					log << "after reload: numVerts " << nif->get<int>( iShape, "Num Vertices" )
						<< " dataSize " << nif->get<int>( iShape, "Data Size" ) << "\n";
				}
				dumpHiding( "hiding after ops" );
			} while ( false );
			logf.close();
			QTimer::singleShot( 0, qApp, &QApplication::quit );
		} );
	}

	// TEMP TEST HARNESS (WW_CREATESKIN_TEST=1): after the file loads, cast the
	// real "Create Skin (bind to node)" spell on the first unskinned BSTriShape
	// through NifSkope::castSpell — the exact path the menu and workspace
	// button share — auto-driving its modal dialogs from a timer (QInputDialog
	// accepted, QMessageBox answered Yes/Ok, QProgressDialog left alone).
	// Saves <input>_skinned.nif; with WW_TEST_DONOR set, then runs the atomic
	// "Transfer Bones and Weights" on the same shape and saves
	// <input>_transferred.nif. WW_TEST_PICKNODE=<substring> picks a
	// non-default bind node (the mesh must not move for ANY choice). Dumps
	// ww_createskin_test.log next to the exe and quits.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_CREATESKIN_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope, fname]( bool ok, QString & ) {
			QTimer::singleShot( 500, skope, [skope, ok, fname]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_createskin_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				int stage = 1;	// 1 = create-skin dialogs, 2 = transfer dialogs
				QTimer driver;
				QObject::connect( &driver, &QTimer::timeout, skope, [&log, &stage]() {
					QWidget * w = QApplication::activeModalWidget();
					if ( !w || qobject_cast<QProgressDialog *>( w ) )
						return;
					if ( auto * in = qobject_cast<QInputDialog *>( w ) ) {
						const QByteArray pick = qgetenv( "WW_TEST_PICKNODE" );
						auto * combo = in->findChild<QComboBox *>();
						if ( stage == 1 && combo && !pick.isEmpty() ) {
							for ( int i = 0; i < combo->count(); i++ ) {
								if ( combo->itemText( i ).contains( QString::fromLocal8Bit( pick ) ) ) {
									combo->setCurrentIndex( i );
									break;
								}
							}
						}
						log << "  dialog [" << in->windowTitle() << "] accepted"
							<< ( combo ? QString( ": %1" ).arg( combo->currentText() ) : QString() )
							<< "\n";
						log.flush();
						in->accept();
						return;
					}
					if ( auto * mb = qobject_cast<QMessageBox *>( w ) ) {
						// the transfer confirm defaults to Cancel; click Yes/Ok
						QAbstractButton * btn = mb->button( QMessageBox::Yes );
						if ( !btn )
							btn = mb->button( QMessageBox::Ok );
						if ( !btn && !mb->buttons().isEmpty() )
							btn = mb->buttons().first();
						log << "  messagebox [" << mb->windowTitle() << "]: "
							<< QString( mb->text() ).replace( '\n', ' ' ).left( 300 ) << "\n";
						log.flush();
						if ( btn )
							btn->click();
						return;
					}
				} );
				driver.start( 250 );
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }
					int sb = -1;
					for ( int b = 0; b < nif->getBlockCount(); b++ ) {
						QModelIndex iB = nif->getBlockIndex( b );
						if ( nif->blockInherits( iB, "BSTriShape" )
							&& nif->getIndex( iB, "Vertex Data" ).isValid()
							&& nif->getLink( iB, "Skin" ) < 0 ) {
							sb = b;
							break;
						}
					}
					if ( sb < 0 ) { log << "no unskinned BSTriShape\n"; break; }
					QPersistentModelIndex pShape( nif->getBlockIndex( sb ) );
					log << "target: block " << sb << " '"
						<< nif->get<QString>( nif->getBlockIndex( sb ), "Name" ) << "' verts "
						<< nif->get<int>( nif->getBlockIndex( sb ), "Num Vertices" ) << "\n";
					log.flush();
					// deterministic transfer options
					QSettings cfg;
					cfg.setValue( "Rigging/MappingMode", 1 );	// Nearest Vertex
					cfg.setValue( "Rigging/MaxInfluences", 4 );

					auto dumpRow0 = [&log, nif]( const QModelIndex & iShape, const char * tag ) {
						QModelIndex iVD0 = nif->getIndex( iShape, "Vertex Data" );
						QModelIndex row0 = nif->getIndex( iVD0, 0 );
						log << tag << " desc 0x" << QString::number(
							nif->get<BSVertexDesc>( iShape, "Vertex Desc" ).Value(), 16 )
							<< " row0 children " << nif->rowCount( row0 ) << ":\n";
						for ( int c = 0; c < nif->rowCount( row0 ); c++ ) {
							QModelIndex raw = nif->getIndex( row0, c, 0 );
							QModelIndex cond = nif->getIndex( row0, c );	// condition-checked
							const NifItem * it = nif->getItem( raw, false );
							log << "  [" << c << "] " << nif->itemName( raw )
								<< " <" << nif->itemStrType( raw ) << ">"
								<< " cond=" << cond.isValid()
								<< " condStr='" << ( it ? it->cond() : QString( "?" ) ) << "'"
								<< " cless=" << ( it ? it->isConditionless() : -1 )
								<< " kids=" << nif->rowCount( raw )
								<< " val='" << nif->getValue( raw ).toString().left( 32 ) << "'\n";
						}
						log.flush();
					};
					dumpRow0( pShape, "PRE-SPELL:" );
					skope->castSpell( QLatin1String( "Rigging/Create Skin (bind to node)..." ), pShape );
					// the spell reloads the model; block numbering is unchanged
					pShape = QPersistentModelIndex( nif->getBlockIndex( sb ) );
					const int skinLink = nif->getLink( pShape, "Skin" );
					log << "after create-skin: Skin link " << skinLink << "\n";
					dumpRow0( pShape, "POST-SPELL:" );
					if ( skinLink < 0 ) { log << "FAIL: no skin created\n"; break; }
					QString outSkinned = fname;
					if ( outSkinned.endsWith( QLatin1String( ".nif" ), Qt::CaseInsensitive ) )
						outSkinned.chop( 4 );
					outSkinned += QLatin1String( "_skinned.nif" );
					log << "save skinned ok " << nif->saveToFile( outSkinned )
						<< ": " << outSkinned << "\n";
					log.flush();

					if ( !qEnvironmentVariableIsSet( "WW_TEST_DONOR" ) )
						break;
					stage = 2;
					skope->castSpell( QLatin1String( "Rigging/Transfer Bones and Weights..." ), pShape );
					QModelIndex iInst = nif->getBlockIndex( nif->getLink( pShape, "Skin" ) );
					log << "after transfer: instance bones "
						<< nif->get<int>( iInst, "Num Bones" ) << "\n";
					QString outTransferred = fname;
					if ( outTransferred.endsWith( QLatin1String( ".nif" ), Qt::CaseInsensitive ) )
						outTransferred.chop( 4 );
					outTransferred += QLatin1String( "_transferred.nif" );
					log << "save transferred ok " << nif->saveToFile( outTransferred )
						<< ": " << outTransferred << "\n";
				} while ( false );
				driver.stop();
				log << "done\n";
				logf.close();
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEST HARNESS (WW_ISOLATE_TEST=1): drive the visibility menu's isolate /
	// restore paths headlessly and verify BOTH the state (hiddenNodes,
	// per-shape isHidden) and the pixels (framebuffer diff) actually change.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_ISOLATE_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 800, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_isolate_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				auto pixDiff = []( const QImage & a, const QImage & b ) {
					if ( a.size() != b.size() || a.isNull() )
						return -1;
					int d = 0;
					for ( int y = 0; y < a.height(); y += 4 )
						for ( int x = 0; x < a.width(); x += 4 )
							if ( a.pixel( x, y ) != b.pixel( x, y ) )
								d++;
					return d;
				};
				// grabFramebuffer() reads the CURRENT buffer without repainting;
				// a fresh paint must be pumped through the event loop first
				auto freshGrab = [skope]() {
					skope->ogl->update();
					QCoreApplication::processEvents( QEventLoop::AllEvents, 250 );
					QCoreApplication::processEvents( QEventLoop::AllEvents, 250 );
					return skope->ogl->grabFramebuffer();
				};
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }
					QVector<int> shapes;
					for ( int b = 0; b < nif->getBlockCount(); b++ )
						if ( nif->blockInherits( nif->getBlockIndex( b ), "BSTriShape" ) )
							shapes.append( b );
					if ( shapes.size() < 2 ) { log << "need 2+ shapes\n"; break; }
					const QImage before = freshGrab();
					skope->ogl->getScene()->currentBlock = nif->getBlockIndex( shapes.at( 0 ) );
					skope->ogl->syncObjectSelection( shapes.at( 0 ) );
					skope->ogl->isolateSelected();
					log << "hiddenNodes after isolate: "
						<< skope->ogl->getScene()->hiddenNodes.size() << "\n";
					for ( Shape * s : skope->ogl->getScene()->shapes ) {
						if ( s )
							log << "  shape block " << s->id() << " isHidden="
								<< s->isHidden() << "\n";
					}
					const QImage isolated = freshGrab();
					log << "pixel diff before->isolated: " << pixDiff( before, isolated )
						<< " (0 means the viewport did NOT change)\n";
					skope->ogl->restoreAllVisibility();
					const QImage restored = freshGrab();
					log << "hiddenNodes after restore: "
						<< skope->ogl->getScene()->hiddenNodes.size() << "\n";
					log << "pixel diff isolated->restored: " << pixDiff( isolated, restored ) << "\n";
					log << ( pixDiff( before, isolated ) > 0 && pixDiff( isolated, restored ) > 0
						? "PASS\n" : "FAIL\n" );
				} while ( false );
				log << "DONE\n";
				QTimer::singleShot( 300, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEST HARNESS (WW_DUPFREEZE_TEST=1 edit-mode / =2 object-mode): reproduce
	// "duplicate most of a high-poly mesh" headlessly and TIME it — regression
	// guard for the tlCloneBlock freeze (loadIndex populated a 38k-vert clone
	// without Loading state, so every write emitted a signal the live scene
	// reacted to → quadratic). =1 enters edit mode, selects all faces (forces
	// the over-cap new-shape path, confirm auto-answered Yes), times
	// duplicateElements(); =2 times object-mode duplicateSelection(). Logs
	// block-count delta + elapsed to ww_dupfreeze_test.log, quits.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_DUPFREEZE_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 800, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_dupfreeze_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				// answer the over-cap confirmation Yes as soon as it appears
				QTimer driver;
				QObject::connect( &driver, &QTimer::timeout, skope, [&log]() {
					QWidget * w = QApplication::activeModalWidget();
					if ( auto * mb = qobject_cast<QMessageBox *>( w ) ) {
						QAbstractButton * btn = mb->button( QMessageBox::Yes );
						log << "  confirm: " << QString( mb->text() ).replace( '\n', ' ' ).left( 200 ) << "\n";
						log.flush();
						if ( btn )
							btn->click();
					}
				} );
				driver.start( 30 );
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }
					int sb = -1, best = -1;
					for ( int b = 0; b < nif->getBlockCount(); b++ ) {
						QModelIndex iB = nif->getBlockIndex( b );
						if ( !nif->blockInherits( iB, "BSTriShape" ) )
							continue;
						int nv = nif->get<int>( iB, "Num Vertices" );
						if ( nv > best ) { best = nv; sb = b; }
					}
					if ( sb < 0 ) { log << "no BSTriShape\n"; break; }
					log << "shape: block " << sb << " '"
						<< nif->get<QString>( nif->getBlockIndex( sb ), "Name" ) << "' verts "
						<< best << " tris " << nif->get<int>( nif->getBlockIndex( sb ), "Num Triangles" ) << "\n";
					// build the scene so shapeForBlock() has geometry
					skope->ogl->grabFramebuffer();
					skope->ogl->getScene()->currentBlock = nif->getBlockIndex( sb );
					skope->ogl->syncObjectSelection( sb );
					// WW_DUPFREEZE_TEST=2 → object-mode duplicate (same clone,
					// no edit overlay) to isolate whether the clone or the
					// edit-mode machinery is the freeze
					if ( qgetenv( "WW_DUPFREEZE_TEST" ) == "2" ) {
						const int before = nif->getBlockCount();
						log << "object mode; blocks before " << before << "\n";
						log.flush();
						QElapsedTimer t2; t2.start();
						skope->ogl->duplicateSelection();
						log << "object duplicateSelection returned in " << t2.elapsed()
							<< " ms; blocks after " << nif->getBlockCount() << "\n";
						break;
					}
					skope->ogl->setEditMode( true );
					skope->ogl->setPickMode( 4 );	// faces
					skope->ogl->selectAll( 1 );
					const int before = nif->getBlockCount();
					log << "in edit mode; selected all faces; blocks before " << before << "\n";
					log.flush();
					QElapsedTimer t; t.start();
					skope->ogl->duplicateElements();
					const qint64 ms = t.elapsed();
					const int after = nif->getBlockCount();
					log << "duplicateElements returned in " << ms << " ms; blocks after " << after
						<< " (delta " << ( after - before ) << ")\n";
					// find the new shape (highest-numbered BSTriShape sharing the stem)
					int newBlk = -1;
					for ( int b = nif->getBlockCount() - 1; b >= before; b-- )
						if ( nif->blockInherits( nif->getBlockIndex( b ), "BSTriShape" ) ) { newBlk = b; break; }
					if ( newBlk >= 0 )
						log << "new shape: block " << newBlk << " '"
							<< nif->get<QString>( nif->getBlockIndex( newBlk ), "Name" ) << "' verts "
							<< nif->get<int>( nif->getBlockIndex( newBlk ), "Num Vertices" ) << " tris "
							<< nif->get<int>( nif->getBlockIndex( newBlk ), "Num Triangles" ) << "\n";
					if ( qEnvironmentVariableIsSet( "WW_TEST_SAVE" ) ) {
						QString out = qEnvironmentVariable( "WW_TEST_SAVE" );
						log << "save ok " << nif->saveToFile( out ) << ": " << out << "\n";
					}
				} while ( false );
				driver.stop();
				log << "done\n";
				logf.close();
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEST HARNESS (WW_DELETE_TEST=1): exercise object-mode multi-delete
	// headlessly. Object-selects the two smallest BSTriShapes plus (if present)
	// one whole NiNode branch, runs deleteBlocksWithConfirm with the confirm
	// auto-clicked, verifies the branch closure was removed and no dangling -1
	// child link remains, saves (WW_TEST_SAVE) and quits.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_DELETE_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 600, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_delete_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				QTimer driver;
				QObject::connect( &driver, &QTimer::timeout, skope, [&log]() {
					if ( auto * mb = qobject_cast<QMessageBox *>( QApplication::activeModalWidget() ) ) {
						const QPoint c = QCursor::pos();
						for ( QAbstractButton * b : mb->buttons() )
							if ( mb->buttonRole( b ) == QMessageBox::AcceptRole ) {
								const QPoint btnC = b->mapToGlobal( b->rect().center() );
								log << "  action button '" << b->text() << "' centre ("
									<< btnC.x() << "," << btnC.y() << ") vs cursor ("
									<< c.x() << "," << c.y() << "); offset "
									<< ( btnC - c ).manhattanLength() << " px\n";
								log << "  confirm '" << mb->text().left( 40 ) << "'\n";
								log.flush();
								b->click();
								break;
							}
					}
				} );
				driver.start( 30 );
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }
					// pick two smallest BSTriShapes as leaf targets
					QVector<QPair<int,int>> shapes;	// (verts, block)
					for ( int b = 0; b < nif->getBlockCount(); b++ )
						if ( nif->blockInherits( nif->getBlockIndex( b ), "BSTriShape" ) )
							shapes.append( { nif->get<int>( nif->getBlockIndex( b ), "Num Vertices" ), b } );
					std::sort( shapes.begin(), shapes.end() );
					QVector<int> targets;
					QStringList names;
					for ( int i = 0; i < shapes.size() && targets.size() < 2; i++ ) {
						targets.append( shapes[i].second );
						names << nif->get<QString>( nif->getBlockIndex( shapes[i].second ), "Name" );
					}
					if ( targets.isEmpty() ) { log << "no shapes\n"; break; }
					// record parents + expected survivor count
					QSet<int> expectRemoved;
					std::function<void(int)> closure = [&]( int bl ) {
						if ( bl < 0 || expectRemoved.contains( bl ) ) return;
						expectRemoved.insert( bl );
						for ( int l : nif->getChildLinks( bl ) )
							if ( l >= 0 && nif->getParent( l ) == bl ) closure( l );
					};
					for ( int t : targets ) closure( t );
					const int before = nif->getBlockCount();
					QStringList targetStrs;
					for ( int t : targets ) targetStrs << QString::number( t );
					log << "targets [" << targetStrs.join( "," ) << "] names " << names.join( ", " )
						<< "; branch-closure " << expectRemoved.size()
						<< "; blocks before " << before << "\n";
					log.flush();
					skope->ogl->setObjectSelection( QSet<int>( targets.constBegin(), targets.constEnd() ),
						targets.first() );
					// park the cursor at a known point so the popup-placement
					// check has a reference (Blender-style at-cursor confirm)
					QCursor::setPos( 900, 500 );
					const int removed = skope->ogl->deleteBlocksWithConfirm( targets );
					const int after = nif->getBlockCount();
					log << "deleteBlocksWithConfirm returned " << removed
						<< "; blocks after " << after << " (delta " << ( before - after ) << ")\n";
					// verify no dangling -1 child link anywhere
					int dangling = 0;
					for ( int b = 0; b < nif->getBlockCount(); b++ ) {
						QModelIndex iCh = nif->getIndex( nif->getBlockIndex( b ), "Children" );
						if ( !iCh.isValid() ) continue;
						for ( int c = 0; c < nif->rowCount( iCh ); c++ )
							if ( nif->getLink( nif->getIndex( iCh, c ) ) == -1 ) dangling++;
					}
					log << "dangling -1 child links after delete: " << dangling
						<< ( ( before - after == expectRemoved.size() && dangling == 0 )
							? "  PASS\n" : "  CHECK\n" );
					if ( qEnvironmentVariableIsSet( "WW_TEST_SAVE" ) ) {
						QString out = qEnvironmentVariable( "WW_TEST_SAVE" );
						log << "save ok " << nif->saveToFile( out ) << ": " << out << "\n";
					}
				} while ( false );
				driver.stop();
				log << "done\n";
				logf.close();
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEST HARNESS (WW_COPYPASTE_TEST=1): exercise the multi-block Copy Branch /
	// Paste Branch spells end to end, through the SAME path Ctrl+C / Ctrl+V take
	// (NifSkope::castSpell -> SpellBook::cast -> spell->cast). Selects the two
	// smallest BSTriShapes in the block list (which publishes the selection Copy
	// Branch reads), casts Copy Branch (union of both branches), then casts Paste
	// Branch WITH A SHAPE as the target — the realistic Ctrl+V case where the
	// current block is not a node — so the "slot into the nearest NiNode"
	// fallback is exercised. Verifies:
	//   - the clipboard holds a nibranch payload (copy succeeded),
	//   - block-count delta == the copied branch union size (the union really was
	//     copied, not just one branch),
	//   - the nearest NiNode gained exactly roots.count() children whose types
	//     match the copied roots in order (every root slotted in, not just the
	//     first), and
	//   - no pasted block has a child link pointing back into the original
	//     blocks (internal links were remapped, not left dangling).
	// Saves (WW_TEST_SAVE) and quits. Log: release/ww_copypaste_test.log.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_COPYPASTE_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 600, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_copypaste_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }

					// roots = the two smallest BSTriShapes (independent branches,
					// each a scene object that belongs under a NiNode)
					QVector<QPair<int,int>> shapes;	// (verts, block)
					for ( int b = 0; b < nif->getBlockCount(); b++ )
						if ( nif->blockInherits( nif->getBlockIndex( b ), "BSTriShape" ) )
							shapes.append( { nif->get<int>( nif->getBlockIndex( b ), "Num Vertices" ), b } );
					std::sort( shapes.begin(), shapes.end() );
					QVector<int> roots;
					QStringList names, rootTypes;
					for ( int i = 0; i < shapes.size() && roots.size() < 2; i++ ) {
						roots.append( shapes[i].second );
						names << nif->get<QString>( nif->getBlockIndex( shapes[i].second ), "Name" );
						rootTypes << nif->itemName( nif->getBlockIndex( shapes[i].second ) );
					}
					if ( roots.isEmpty() ) { log << "no shapes\n"; break; }
					if ( roots.size() < 2 )
						log << "note: only one shape found; multi-root path degenerates to single\n";

					// expected copy union == populateBlocks (DFS over child links,
					// dedup); mirror it exactly so the count delta is predictable
					QSet<int> unionSet;
					std::function<void(int)> dfs = [&]( int b ) {
						if ( b < 0 || unionSet.contains( b ) ) return;
						unionSet.insert( b );
						for ( int l : nif->getChildLinks( b ) ) dfs( l );
					};
					for ( int r : roots ) dfs( r );

					// where the fallback should slot the roots: nearest NiNode
					// ancestor of the shape we will paste onto
					int tb = roots.first();
					while ( tb >= 0 && !nif->blockInherits( nif->getBlockIndex( tb ), "NiNode" ) )
						tb = nif->getParent( tb );
					if ( tb < 0 ) tb = 0;
					QModelIndex iTarget = nif->getBlockIndex( tb );
					const int childrenBefore = nif->rowCount( nif->getIndex( iTarget, "Children" ) );

					QStringList rootStrs;
					for ( int r : roots ) rootStrs << QString::number( r );
					log << "roots [" << rootStrs.join( "," ) << "] (" << rootTypes.join( ", " )
						<< ") names " << names.join( ", " ) << "\n";
					log << "copy union " << unionSet.size() << " blocks; nearest NiNode " << tb
						<< " '" << nif->get<QString>( iTarget, "Name" ) << "' children before "
						<< childrenBefore << "\n";
					log.flush();

					// --- COPY: select both shapes (publishes the selection Copy
					// Branch reads), then cast Copy Branch on one of them ---
					skope->list->selectionModel()->clearSelection();
					for ( int r : roots ) {
						QModelIndex p = skope->proxy->mapFrom( nif->getBlockIndex( r ), QModelIndex() );
						skope->list->selectionModel()->select( p,
							QItemSelectionModel::Select | QItemSelectionModel::Rows );
					}
					skope->castSpell( QStringLiteral( "Block/Copy Branch" ),
						nif->getBlockIndex( roots.first() ) );
					bool copied = false;
					if ( const QMimeData * md = QApplication::clipboard()->mimeData() )
						for ( const QString & f : md->formats() )
							if ( f.contains( QLatin1String( "nibranch" ) ) ) { copied = true; break; }
					log << "after Copy Branch: clipboard holds nibranch payload: " << copied << "\n";
					log.flush();
					if ( !copied ) { log << "FAIL: copy produced no branch payload\n"; break; }

					// --- PASTE: cast Paste Branch onto a SHAPE (not a node), so the
					// fallback must slot each root into the nearest NiNode. ---
					const int origCount = nif->getBlockCount();
					skope->castSpell( QStringLiteral( "Block/Paste Branch" ),
						nif->getBlockIndex( roots.first() ) );
					const int newCount = nif->getBlockCount();
					const int delta = newCount - origCount;
					log << "after Paste Branch: blocks " << origCount << " -> " << newCount
						<< " (delta " << delta << ", expected " << unionSet.size() << ")\n";

					// the nearest NiNode's Children must have grown by roots.count()
					iTarget = nif->getBlockIndex( tb );
					QModelIndex iChildren = nif->getIndex( iTarget, "Children" );
					const int childrenAfter = nif->rowCount( iChildren );
					log << "node children " << childrenBefore << " -> " << childrenAfter
						<< " (delta " << ( childrenAfter - childrenBefore )
						<< ", expected " << roots.size() << ")\n";

					// the last roots.count() children must be pasted blocks
					// (>= origCount) whose types match the copied roots, in order
					int typeMatches = 0, attached = 0;
					for ( int k = 0; k < roots.size(); k++ ) {
						int row = childrenAfter - roots.size() + k;
						if ( row < 0 ) continue;
						int child = nif->getLink( nif->getIndex( iChildren, row ) );
						if ( child >= origCount ) attached++;
						if ( child >= 0 && nif->itemName( nif->getBlockIndex( child ) ) == rootTypes[k] )
							typeMatches++;
					}
					log << "slotted-in roots " << attached << "/" << roots.size()
						<< "; type order matches " << typeMatches << "/" << roots.size() << "\n";

					// remap invariant: no pasted block links a child back into the
					// original range [0, origCount) — that would be the classic
					// paste bug (only the first root remapped / dangling links)
					int backLinks = 0;
					for ( int b = origCount; b < newCount; b++ )
						for ( int l : nif->getChildLinks( b ) )
							if ( l >= 0 && l < origCount ) backLinks++;
					log << "pasted-block child links into original range: " << backLinks
						<< " (expected 0)\n";

					const bool pass = delta == unionSet.size()
						&& ( childrenAfter - childrenBefore ) == roots.size()
						&& attached == roots.size()
						&& typeMatches == roots.size()
						&& backLinks == 0;
					log << ( pass ? "PASS\n" : "CHECK: one or more invariants failed\n" );

					if ( qEnvironmentVariableIsSet( "WW_TEST_SAVE" ) ) {
						QString out = qEnvironmentVariable( "WW_TEST_SAVE" );
						log << "save ok " << nif->saveToFile( out ) << ": " << out << "\n";
					}
				} while ( false );
				log << "done\n";
				logf.close();
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEST HARNESS (WW_JOIN_TEST=1): exercise rigging-aware Ctrl+J on FO4 skinned
	// segmented meshes. Object-selects the largest BSSubIndexTriShape plus every
	// other BSSubIndexTriShape sharing its vertex desc, runs GLView::
	// joinSelectedObjects(), and verifies on the merged shape:
	//   - Num Vertices / Num Triangles == the sums of the participants,
	//   - the skin bone list == the UNION of the participants' bone NiNodes
	//     (Num Bones consistent across Instance/Bones/BoneData),
	//   - every per-vertex Bone Index < merged Num Bones (indices were remapped,
	//     not left pointing into a source's old shorter list),
	//   - segments cover [0, Num Triangles) with Sum(Num Primitives) == tris.
	// Saves (WW_TEST_SAVE) and quits. Log: release/ww_join_test.log.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_JOIN_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 800, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_join_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				auto boneNodes = [nif]( int shapeBlock ) {
					QSet<int> nodes;
					QModelIndex iInst = nif->getBlockIndex( nif->getLink( nif->getBlockIndex( shapeBlock ), "Skin" ) );
					QModelIndex iBones = nif->getIndex( iInst, "Bones" );
					for ( int i = 0; i < nif->rowCount( iBones ); i++ )
						nodes.insert( nif->getLink( nif->getIndex( iBones, i ) ) );
					return nodes;
				};
				auto flagsOf = [nif]( int b ) {
					return quint16( ( nif->get<BSVertexDesc>( nif->getBlockIndex( b ), "Vertex Desc" ).Value() >> 44 ) & 0xFFFF );
				};
				const int mode = QString::fromLocal8Bit( qgetenv( "WW_JOIN_TEST" ) ).toInt();
				const quint16 fillable = quint16( VF_COLORS | VF_SKINNED | VF_EYEDATA );
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }
					// active: mode 2 = richest vertex format (tie-break biggest);
					// otherwise the biggest shape (Phase A path)
					int active = -1;
					long long best = -1;
					for ( int b = 0; b < nif->getBlockCount(); b++ )
						if ( nif->blockInherits( nif->getBlockIndex( b ), "BSSubIndexTriShape" ) ) {
							int nv = nif->get<int>( nif->getBlockIndex( b ), "Num Vertices" );
							int bits = 0; for ( quint16 f = flagsOf( b ); f; f >>= 1 ) bits += f & 1;
							long long key = ( mode == 2 ) ? ( qint64( bits ) << 24 ) + nv : nv;
							if ( key > best ) { best = key; active = b; }
						}
					if ( active < 0 ) { log << "no BSSubIndexTriShape\n"; break; }
					quint64 desc = nif->get<BSVertexDesc>( nif->getBlockIndex( active ), "Vertex Desc" ).Value();
					const quint16 aFlags = flagsOf( active );
					const int activeOrigV = nif->get<int>( nif->getBlockIndex( active ), "Num Vertices" );
					// sources: same structural layout, nothing the active lacks
					// (mirrors GLView::joinSelectedObjects compatibility)
					QVector<int> parts{ active };
					bool anySourceLacksColor = false;
					for ( int b = 0; b < nif->getBlockCount(); b++ ) {
						if ( b == active || !nif->blockInherits( nif->getBlockIndex( b ), "BSSubIndexTriShape" ) )
							continue;
						quint16 sf = flagsOf( b );
						if ( ( aFlags & ~fillable ) != ( sf & ~fillable ) || ( sf & ~aFlags ) )
							continue;
						if ( ( aFlags & VF_COLORS ) && !( sf & VF_COLORS ) )
							anySourceLacksColor = true;
						parts.append( b );
					}
					if ( parts.size() < 2 ) { log << "need >=2 compatible BSSubIndexTriShapes\n"; break; }

					int expV = 0, expT = 0;
					QSet<int> expBones;
					QStringList partStr;
					for ( int b : parts ) {
						expV += nif->get<int>( nif->getBlockIndex( b ), "Num Vertices" );
						expT += nif->get<int>( nif->getBlockIndex( b ), "Num Triangles" );
						expBones |= boneNodes( b );
						partStr << QString( "%1(v%2,t%3)" ).arg( b )
							.arg( nif->get<int>( nif->getBlockIndex( b ), "Num Vertices" ) )
							.arg( nif->get<int>( nif->getBlockIndex( b ), "Num Triangles" ) );
					}
					QList<int> eb = expBones.values(); std::sort( eb.begin(), eb.end() );
					log << "active " << active << " desc 0x" << QString::number( desc, 16 )
						<< " participants [" << partStr.join( ", " ) << "]\n";
					{
						QStringList s; for ( int n : eb ) s << QString::number( n );
						log << "expect verts " << expV << " tris " << expT
							<< " bone-node union " << eb.size() << " {" << s.join( "," ) << "}\n";
					}
					log.flush();

					// build the scene (world transforms) then object-select + join
					skope->ogl->grabFramebuffer();
					QSet<int> sel( parts.constBegin(), parts.constEnd() );
					skope->ogl->setObjectSelection( sel, active );
					skope->ogl->joinSelectedObjects();

					// merged shape = the biggest BSSubIndexTriShape now
					int merged = -1, mv = -1;
					for ( int b = 0; b < nif->getBlockCount(); b++ )
						if ( nif->blockInherits( nif->getBlockIndex( b ), "BSSubIndexTriShape" ) ) {
							int nv = nif->get<int>( nif->getBlockIndex( b ), "Num Vertices" );
							if ( nv > mv ) { mv = nv; merged = b; }
						}
					if ( merged < 0 ) { log << "no merged shape\n"; break; }
					QModelIndex iM = nif->getBlockIndex( merged );
					int gotV = nif->get<int>( iM, "Num Vertices" );
					int gotT = nif->get<int>( iM, "Num Triangles" );
					log << "merged block " << merged << " verts " << gotV
						<< " (expect " << expV << ") tris " << gotT << " (expect " << expT << ")\n";

					// skin: counts + bone-node union
					QModelIndex iInst = nif->getBlockIndex( nif->getLink( iM, "Skin" ) );
					QModelIndex iData = nif->getBlockIndex( nif->getLink( iInst, "Data" ) );
					int nbInst = nif->get<int>( iInst, "Num Bones" );
					int nbArr = nif->rowCount( nif->getIndex( iInst, "Bones" ) );
					int nbData = nif->get<int>( iData, "Num Bones" );
					QSet<int> gotBones = boneNodes( merged );
					QList<int> gb = gotBones.values(); std::sort( gb.begin(), gb.end() );
					{ QStringList s; for ( int n : gb ) s << QString::number( n );
						log << "merged bones Num=" << nbInst << " arr=" << nbArr << " data=" << nbData
							<< " nodes{" << s.join( "," ) << "}\n"; }

					// every per-vertex bone index < Num Bones, and the appended donor
					// verts must keep non-zero skin weight (the merge must not drop
					// the skin when it copies each vertex record)
					int badIdx = 0, zeroW = 0;
					QModelIndex iVD = nif->getIndex( iM, "Vertex Data" );
					for ( int v = 0; v < gotV; v++ ) {
						QModelIndex iRec = nif->getIndex( iVD, v );
						QModelIndex iI = nif->getIndex( iRec, "Bone Indices" );
						for ( int k = 0; k < nif->rowCount( iI ); k++ )
							if ( nif->get<quint8>( nif->getIndex( iI, k ) ) >= nbInst ) badIdx++;
						if ( v >= activeOrigV ) {
							QModelIndex iW = nif->getIndex( iRec, "Bone Weights" );
							float sum = 0;
							for ( int k = 0; k < nif->rowCount( iW ); k++ )
								sum += nif->get<float>( nif->getIndex( iW, k ) );
							if ( sum < 1e-4f ) zeroW++;
						}
					}
					log << "vertex bone indices >= Num Bones: " << badIdx << " (expect 0)\n";
					log << "appended verts [" << activeOrigV << "," << gotV << ") zero-weight: "
						<< zeroW << " (expect 0)\n";

					// segment coverage
					int nSeg = nif->get<int>( iM, "Num Segments" );
					int nTot = nif->get<int>( iM, "Total Segments" );
					QModelIndex iSeg = nif->getIndex( iM, "Segment" );
					int sumPrim = 0; bool contiguous = true; int expectStart = 0;
					QVector<QPair<int,int>> segList;
					for ( int s = 0; s < nif->rowCount( iSeg ); s++ ) {
						int si = nif->get<quint32>( nif->getIndex( iSeg, s ), "Start Index" );
						int np = nif->get<quint32>( nif->getIndex( iSeg, s ), "Num Primitives" );
						segList.append( { si, np } );
					}
					std::sort( segList.begin(), segList.end() );
					for ( const auto & pr : segList ) {
						if ( pr.second == 0 ) continue;
						if ( pr.first != expectStart ) contiguous = false;
						expectStart += pr.second * 3; sumPrim += pr.second;
					}
					// shared Segment Data (subsegments) consistency when Num<Total
					bool segDataOk = true;
					if ( nSeg < nTot ) {
						QModelIndex iSD = nif->getIndex( iM, "Segment Data" );
						int sdNum = nif->get<int>( iSD, "Num Segments" );
						int sdTot = nif->get<int>( iSD, "Total Segments" );
						int nStarts = nif->rowCount( nif->getIndex( iSD, "Segment Starts" ) );
						int nPsd = nif->rowCount( nif->getIndex( iSD, "Per Segment Data" ) );
						segDataOk = iSD.isValid() && sdNum == nSeg && sdTot == nTot
							&& nStarts == nSeg && nPsd == nTot;
						log << "  segData Num=" << sdNum << " Total=" << sdTot
							<< " starts=" << nStarts << " psd=" << nPsd
							<< " SSF='" << nif->get<QString>( iSD, "SSF File" ) << "' ok=" << segDataOk << "\n";
					}
					log << "segments Num=" << nSeg << " Total=" << nTot << " sumPrim=" << sumPrim
						<< " (expect " << gotT << ") contiguous=" << contiguous << "\n";

					// superset color fill: when the active has vertex colors and a
					// merged source lacked them, the appended verts must be opaque
					// white (the active's own [0, activeOrigV) verts keep colours).
					bool colorOk = true;
					if ( ( aFlags & VF_COLORS ) && anySourceLacksColor ) {
						int nonWhite = 0;
						for ( int v = activeOrigV; v < gotV; v++ ) {
							ByteColor4 c = nif->get<ByteColor4>( nif->getIndex( iVD, v ), "Vertex Colors" );
							if ( c.red() < 0.99f || c.green() < 0.99f || c.blue() < 0.99f || c.alpha() < 0.99f )
								nonWhite++;
						}
						colorOk = ( nonWhite == 0 );
						log << "appended verts [" << activeOrigV << "," << gotV << ") non-white colors: "
							<< nonWhite << " (expect 0)\n";
					}

					const bool pass = gotV == expV && gotT == expT && gb == eb
						&& nbInst == nbArr && nbInst == nbData && badIdx == 0 && zeroW == 0
						&& segDataOk && sumPrim == gotT && contiguous && colorOk;
					log << ( pass ? "PASS\n" : "CHECK: one or more invariants failed\n" );

					if ( qEnvironmentVariableIsSet( "WW_TEST_SAVE" ) ) {
						QString out = qEnvironmentVariable( "WW_TEST_SAVE" );
						log << "save ok " << nif->saveToFile( out ) << ": " << out << "\n";
					}
				} while ( false );
				log << "done\n";
				logf.close();
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEST HARNESS (WW_VERTEXFLAGS_TEST): probe the "Vertex Flags" landmine
	// recorded in WW_CHANGES 2026-07-18b — the note flags every spell that does
	// set<BSVertexDesc> + updateArraySize on an UNCHANGED vertex count as
	// suspect, the stock spEditVertexDesc included, because updateArraySize
	// early-returns when the row count does not move (nifmodel.cpp:626).
	//
	// Casts the REAL spell through NifSkope::castSpell (the path the Block
	// Details context menu uses) with a timer ticking its modal checkbox dialog,
	// so the whole shipping code path runs — including the getVertexPositions /
	// setVertexPositions round trip that has to carry positions across a
	// precision change.
	//
	// Two independent observables:
	//   - in-model: row 0 of "Vertex Data" must expose exactly the fields the
	//     new desc claims, and every vertex position must survive the edit.
	//   - on disk: WW_TEST_SAVE writes the result and verify_vertexflags.py
	//     compares each shape's declared stride against the bytes the header's
	//     block-size table says were actually written.
	//
	// =1 toggles Colors (stride +/-4B, one variant per field); =2 toggles Full
	// Precision (stride +/-8B), the sharper case: "Vertex" and "Bitangent X"
	// each appear TWICE in BSVertexData (Vector3/float vs HalfVector3/hfloat),
	// so a by-name lookup cannot tell the variants apart.
	// Log: release/ww_vertexflags_test.log.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_VERTEXFLAGS_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 800, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_vertexflags_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				const int mode = QString::fromLocal8Bit( qgetenv( "WW_VERTEXFLAGS_TEST" ) ).toInt();
				const bool fullPrec = ( mode == 2 );
				// checkbox order matches spEditVertexDesc's flagNames list
				const int chkIndex = fullPrec ? 10 : 5;

				// tick the spell's modal dialog: flip the one checkbox, accept
				QTimer driver;
				QObject::connect( &driver, &QTimer::timeout, skope, [&log, chkIndex]() {
					QWidget * w = QApplication::activeModalWidget();
					if ( !w || qobject_cast<QProgressDialog *>( w ) )
						return;
					if ( auto * mb = qobject_cast<QMessageBox *>( w ) ) {
						QAbstractButton * btn = mb->button( QMessageBox::Yes );
						if ( !btn )
							btn = mb->button( QMessageBox::Ok );
						if ( !btn && !mb->buttons().isEmpty() )
							btn = mb->buttons().first();
						if ( btn )
							btn->click();
						return;
					}
					if ( auto * dlg = qobject_cast<QDialog *>( w ) ) {
						QList<QCheckBox *> chks = dlg->findChildren<QCheckBox *>();
						if ( chkIndex < 0 || chkIndex >= chks.size() ) {
							log << "  dialog has " << chks.size()
								<< " checkboxes, index " << chkIndex << " out of range\n";
							dlg->reject();
							return;
						}
						QCheckBox * c = chks.at( chkIndex );
						c->setChecked( !c->isChecked() );
						log << "  dialog: toggled checkbox[" << chkIndex << "] '"
							<< c->text() << "' -> " << c->isChecked() << ", accepting\n";
						log.flush();
						dlg->accept();
					}
				} );
				driver.start( 200 );

				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }
					int shape = -1;
					for ( int b = 0; b < nif->getBlockCount(); b++ )
						if ( nif->blockInherits( nif->getBlockIndex( b ), "BSTriShape" ) ) { shape = b; break; }
					if ( shape < 0 ) { log << "no BSTriShape\n"; break; }

					QModelIndex iShape = nif->getBlockIndex( shape );
					QModelIndex iDesc  = nif->getIndex( iShape, "Vertex Desc" );
					QModelIndex iVD    = nif->getIndex( iShape, "Vertex Data" );
					if ( !iDesc.isValid() || !iVD.isValid() || !nif->getIndex( iVD, 0 ).isValid() ) {
						log << "shape " << shape << " has no Vertex Desc / Vertex Data rows\n";
						break;
					}

					const VertexAttribute att = fullPrec ? VertexAttribute( 10 ) : VA_COLOR;
					const char * probe = fullPrec ? "Vertex" : "Vertex Colors";

					// Reading the row layout back is itself subject to the hazard
					// this harness exists to check: "Vertex" names BOTH precision
					// variants, so mere existence proves nothing. For Colors the
					// name is unique and existence is the answer; for Full
					// Precision we have to ask the LIVE item for its value type.
					auto rowMatchesDesc = [nif, fullPrec]( const QModelIndex & iArr, bool want ) {
						const NifItem * row0 = nif->getItem( iArr, 0, false );
						if ( !row0 )
							return false;
						if ( !fullPrec )
							return nif->getItem( row0, "Vertex Colors" ) != nullptr ? want : !want;
						const NifItem * v = nif->getItem( row0, "Vertex" );
						if ( !v )
							return false;
						const bool isFull = ( v->valueType() == NifValue::tVector3 );
						return isFull == want;
					};
					BSVertexDesc desc = nif->get<BSVertexDesc>( iDesc );
					const int numVerts = nif->get<int>( iShape, "Num Vertices" );
					const int numTris  = nif->get<int>( iShape, "Num Triangles" );
					const bool hadFlag = desc.HasFlag( att );
					const uint oldSize = desc.GetVertexSize();

					// positions before, to prove the spell carried them across the
					// layout change rather than reinterpreting the old bytes
					QVector<Vector3> before( numVerts );
					for ( int v = 0; v < numVerts; v++ )
						before[v] = nif->get<Vector3>( nif->getIndex( iVD, v ), "Vertex" );

					log << "shape block " << shape << " '" << nif->get<QString>( iShape, "Name" ) << "'\n";
					log << "mode " << mode << ": toggling "
						<< ( fullPrec ? "Full Precision" : "Colors" ) << " via the real spell\n";
					log << "before: desc 0x" << QString::number( desc.Value(), 16 )
						<< " flags 0x" << QString::number( ( desc.Value() >> 44 ) & 0xFFFF, 16 )
						<< " vertexSize " << oldSize
						<< " numVerts " << numVerts << " numTris " << numTris
						<< " DataSize " << nif->get<uint>( iShape, "Data Size" )
						<< " rows " << nif->rowCount( iVD ) << "\n";
					log << "before: row0 layout matches desc for '" << probe << "' = "
						<< rowMatchesDesc( iVD, hadFlag )
						<< " (desc flag " << hadFlag << ")\n";
					log.flush();

					skope->castSpell( QLatin1String( "Vertex Flags" ), iDesc );

					// re-resolve: the spell may rebuild the block's items
					iShape = nif->getBlockIndex( shape );
					iVD    = nif->getIndex( iShape, "Vertex Data" );
					desc   = nif->get<BSVertexDesc>( iShape, "Vertex Desc" );
					const bool wantFlag = !hadFlag;
					const bool gotFlag  = desc.HasFlag( att );
					const bool exposes  = rowMatchesDesc( iVD, wantFlag );

					log << "after:  desc 0x" << QString::number( desc.Value(), 16 )
						<< " flags 0x" << QString::number( ( desc.Value() >> 44 ) & 0xFFFF, 16 )
						<< " vertexSize " << oldSize << " -> " << desc.GetVertexSize()
						<< " DataSize " << nif->get<uint>( iShape, "Data Size" )
						<< " rows " << nif->rowCount( iVD ) << "\n";
					log << "after:  desc flag " << gotFlag << " (want " << wantFlag << ")"
						<< ( gotFlag == wantFlag ? "  OK" : "  <-- SPELL DID NOT APPLY" ) << "\n";
					log << "after:  row0 layout matches desc for '" << probe << "' = " << exposes
						<< ( exposes ? "  OK" : "  <-- ROW LAYOUT DISAGREES WITH DESC" ) << "\n";

					// positions must survive: half->full is exact, full->half costs
					// at most half-precision rounding, so scale tolerance by magnitude
					int moved = 0;
					float worst = 0.0f;
					for ( int v = 0; v < numVerts; v++ ) {
						Vector3 a = before[v];
						Vector3 b = nif->get<Vector3>( nif->getIndex( iVD, v ), "Vertex" );
						float d = ( a - b ).length();
						if ( d > 0.002f * qMax( 1.0f, a.length() ) )
							moved++;
						worst = qMax( worst, d );
					}
					log << "positions: " << moved << " of " << numVerts
						<< " moved beyond tolerance (expect 0), worst delta " << worst << "\n";

					const bool pass = gotFlag == wantFlag && exposes && moved == 0;
					log << ( pass ? "in-model: PASS\n"
						: "in-model: FAIL — desc, row layout or vertex values diverged\n" );

					if ( qEnvironmentVariableIsSet( "WW_TEST_SAVE" ) ) {
						QString out = qEnvironmentVariable( "WW_TEST_SAVE" );
						log << "save ok " << nif->saveToFile( out ) << ": " << out << "\n";
					}
				} while ( false );
				driver.stop();
				log << "done\n";
				logf.close();
				// The spell dirties the document, so quitting can raise a
				// save-changes prompt after this scope (and its driver) is gone.
				// Leave an app-owned answerer running so the process cannot strand.
				QTimer * quitDriver = new QTimer( qApp );
				QObject::connect( quitDriver, &QTimer::timeout, qApp, []() {
					auto * mb = qobject_cast<QMessageBox *>( QApplication::activeModalWidget() );
					if ( !mb )
						return;
					QAbstractButton * btn = mb->button( QMessageBox::Discard );
					if ( !btn )
						btn = mb->button( QMessageBox::No );
					if ( !btn )
						btn = mb->button( QMessageBox::Ok );
					if ( !btn && !mb->buttons().isEmpty() )
						btn = mb->buttons().first();
					if ( btn )
						btn->click();
				} );
				quitDriver->start( 100 );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEST HARNESS (WW_POSEDOCK_TEST=1): the Pose Manager dock is wired to the
	// shared pose API. Finds the dock, shows it, checks the bone list populated,
	// drives Save current (auto-answering the name dialog) and confirms a pose
	// sequence appeared, then drives Apply and confirms it ran. UI over the API
	// that WW_POSE_TEST / the CLI already prove numerically.
	// Log: release/ww_posedock_test.log.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_POSEDOCK_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1000, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_posedock_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				QStringList fails;

				// auto-answer the "Pose name" QInputDialog the Save button opens
				QTimer dlgDriver;
				QObject::connect( &dlgDriver, &QTimer::timeout, skope, [&log]() {
					if ( auto * in = qobject_cast<QInputDialog *>( QApplication::activeModalWidget() ) ) {
						in->setTextValue( QStringLiteral( "DockPose" ) );
						log << "  answered pose-name dialog\n"; log.flush();
						in->accept();
					}
				} );
				dlgDriver.start( 150 );

				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }
					QDockWidget * dock = skope->findChild<QDockWidget *>( QStringLiteral( "PoseManagerDock" ) );
					if ( !dock ) { fails << "PoseManagerDock not found"; break; }
					dock->show();
					dock->raise();
					qApp->processEvents();

					auto * boneList = dock->findChild<QListWidget *>( QStringLiteral( "PoseBoneList" ) );
					auto * poseListW = dock->findChild<QListWidget *>( QStringLiteral( "PosePoseList" ) );
					auto * slider = dock->findChild<QSlider *>( QStringLiteral( "PoseBlendSlider" ) );
					if ( !boneList || !poseListW || !slider ) { fails << "dock widgets missing"; break; }
					log << "bone list rows: " << boneList->count() << "\n";
					if ( boneList->count() < 1 )
						fails << "bone list did not populate";

					// find and click Save current
					QPushButton * saveBtn = nullptr, * applyBtn = nullptr;
					for ( QPushButton * b : dock->findChildren<QPushButton *>() ) {
						if ( b->text().contains( "Save" ) ) saveBtn = b;
						if ( b->text() == QStringLiteral( "Apply" ) ) applyBtn = b;
					}
					if ( !saveBtn || !applyBtn ) { fails << "Save/Apply buttons missing"; break; }

					const int posesBefore = poseListW->count();
					saveBtn->click();
					qApp->processEvents();
					log << "poses after Save: " << poseListW->count() << " (was " << posesBefore << ")\n";
					if ( poseListW->count() != posesBefore + 1 )
						fails << "Save current did not add a pose to the library";

					const bool inFile = AnimSetup::sequenceNames( nif ).contains( QStringLiteral( "DockPose" ) );
					log << "'DockPose' sequence in the file: " << inFile << "\n";
					if ( !inFile )
						fails << "the saved pose is not a sequence in the model";

					// select it, set blend 50%, Apply
					for ( int r = 0; r < poseListW->count(); r++ )
						if ( poseListW->item( r )->text() == QStringLiteral( "DockPose" ) )
							poseListW->setCurrentRow( r );
					slider->setValue( 50 );
					qApp->processEvents();
					applyBtn->click();
					qApp->processEvents();
					log << "Apply at 50% ran without error\n";
				} while ( false );

				dlgDriver.stop();
				for ( const QString & f : fails )
					log << "  FAIL: " << f << "\n";
				log << ( fails.isEmpty() ? "PASS — Pose Manager dock drives the pose API\n" : "FAILED\n" );
				log << "done\n";
				logf.close();
				QTimer * quitDriver = new QTimer( qApp );
				QObject::connect( quitDriver, &QTimer::timeout, qApp, []() {
					auto * mb = qobject_cast<QMessageBox *>( QApplication::activeModalWidget() );
					if ( !mb ) return;
					QAbstractButton * btn = mb->button( QMessageBox::Discard );
					if ( !btn ) btn = mb->button( QMessageBox::No );
					if ( !btn && !mb->buttons().isEmpty() ) btn = mb->buttons().first();
					if ( btn ) btn->click();
				} );
				quitDriver->start( 100 );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEST HARNESS (WW_SKELETON_TEST=1): the Skeleton Manager dock renders what
	// skeletonAnalyse() reports. The numbers themselves are proven separately and
	// independently by the CLI (`skeleton <file>`), whose summed weight matches the
	// vertex count to rounding; this checks the UI layer on top of them - that the
	// tree populates, that each filter narrows the way its tooltip claims, and that
	// search filters. Read-only, so it cannot damage the file it opens.
	// Log: release/ww_skeleton_test.log.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_SKELETON_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 800, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_skeleton_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				QStringList fails;

				auto countRows = []( QTreeWidget * t ) {
					int n = 0;
					std::function<void( QTreeWidgetItem * )> walk = [&]( QTreeWidgetItem * it ) {
						n++;
						for ( int i = 0; i < it->childCount(); i++ )
							walk( it->child( i ) );
					};
					for ( int i = 0; i < t->topLevelItemCount(); i++ )
						walk( t->topLevelItem( i ) );
					return n;
				};

				do {
					if ( !ok || !nif ) { fails << "load failed"; break; }
					QDockWidget * dock = skope->findChild<QDockWidget *>( QStringLiteral( "SkeletonManagerDock" ) );
					if ( !dock ) { fails << "SkeletonManagerDock not found"; break; }
					dock->show();
					dock->raise();
					qApp->processEvents();

					auto * tree = dock->findChild<QTreeWidget *>( QStringLiteral( "SkeletonTree" ) );
					auto * search = dock->findChild<QLineEdit *>( QStringLiteral( "SkeletonSearch" ) );
					auto * footer = dock->findChild<QLabel *>( QStringLiteral( "SkeletonFooter" ) );
					if ( !tree || !search || !footer ) { fails << "dock widgets missing"; break; }

					// ground truth, straight from the shared analysis
					const SkeletonReport report = skeletonAnalyse( nif );
					const int bones = report.deformingCount() + report.unusedCount();
					log << "analyse: " << report.bones.size() << " nodes, " << bones << " bones, "
						<< report.deformingCount() << " deforming, " << report.unusedCount()
						<< " unused, " << report.skinnedShapes << " skinned shape(s)\n";

					const int allRows = countRows( tree );
					log << "All filter rows: " << allRows << "\n";
					if ( allRows != report.bones.size() )
						fails << QString( "All filter shows %1 rows, analyse reports %2 nodes" )
							.arg( allRows ).arg( report.bones.size() );

					// each filter button, in the order they were created
					auto clickFilter = [dock]( int i ) {
						auto * b = dock->findChild<QToolButton *>( QStringLiteral( "SkeletonFilter%1" ).arg( i ) );
						if ( b )
							b->click();
						qApp->processEvents();
						return b != nullptr;
					};

					if ( !clickFilter( 1 ) ) { fails << "Bones filter button missing"; break; }
					const int boneRows = countRows( tree );
					log << "Bones filter rows: " << boneRows << "\n";
					if ( boneRows != bones )
						fails << QString( "Bones filter shows %1, expected %2" ).arg( boneRows ).arg( bones );

					if ( !clickFilter( 2 ) ) { fails << "Deforming filter button missing"; break; }
					const int deformRows = countRows( tree );
					log << "Deforming filter rows: " << deformRows << "\n";
					if ( deformRows != report.deformingCount() )
						fails << QString( "Deforming filter shows %1, expected %2" )
							.arg( deformRows ).arg( report.deformingCount() );

					if ( !clickFilter( 3 ) ) { fails << "Unused filter button missing"; break; }
					const int unusedRows = countRows( tree );
					log << "Unused filter rows: " << unusedRows << "\n";
					if ( unusedRows != report.unusedCount() )
						fails << QString( "Unused filter shows %1, expected %2" )
							.arg( unusedRows ).arg( report.unusedCount() );

					// search, back on the All filter
					clickFilter( 0 );
					QString needle;
					for ( const SkeletonBoneInfo & b : report.bones ) {
						if ( b.name.size() >= 4 ) { needle = b.name.left( 4 ); break; }
					}
					if ( !needle.isEmpty() ) {
						int expected = 0;
						for ( const SkeletonBoneInfo & b : report.bones )
							if ( b.name.contains( needle, Qt::CaseInsensitive ) )
								expected++;
						search->setText( needle );
						qApp->processEvents();
						const int found = countRows( tree );
						log << "search '" << needle << "': " << found << " rows, expected " << expected << "\n";
						if ( found != expected )
							fails << QString( "search '%1' shows %2, expected %3" )
								.arg( needle ).arg( found ).arg( expected );
						search->clear();
						qApp->processEvents();
					}

					// Screenshot of the armature itself. The render-regression harness
					// cannot serve here: it hides every dock, which switches the skeleton
					// view back off. WW_SKELETON_SHOT=<png> grabs the viewport with the
					// dock up, so the octahedral bones, the names and the relationship
					// lines can actually be looked at rather than assumed.
					const QString shot = qEnvironmentVariable( "WW_SKELETON_SHOT" );
					if ( !shot.isEmpty() && skope->ogl ) {
						skope->ogl->setSkeletonView( true );
						int viewIdx = qEnvironmentVariableIntValue( "WW_SKELETON_VIEW" );
						if ( viewIdx <= 0 || viewIdx > int( GLView::ViewWalk ) )
							viewIdx = int( GLView::ViewFront );
						skope->ogl->setOrientation( GLView::ViewState( viewIdx ), true );
						// Warm up the line path: before the first pick render, streaming
						// line geometry draws nothing (the open 07-17 defect) while points
						// still render, so the bones would be missing and only their joint
						// dots would show. indexAt() runs the pick render, which is what
						// heals it. Harness-only: this is not a fix for that bug.
						skope->ogl->indexAt( QPointF( skope->ogl->width() * 0.5,
													   skope->ogl->height() * 0.5 ) );
						qApp->processEvents();
						for ( int i = 0; i < 3; i++ ) {
							skope->ogl->update();
							qApp->processEvents();
						}
						const int drawn = skope->ogl->skeletonDrawnBones().size();
						log << "skeletonView=" << int( skope->ogl->skeletonViewActive() )
							<< " drawnBones=" << drawn << "\n";
						if ( drawn < 1 )
							fails << "skeleton view drew no bones";
						skope->ogl->grabFramebuffer().save( shot );
					}

					log << "footer: " << footer->text() << "\n";
					if ( footer->text().isEmpty() )
						fails << "footer empty";
				} while ( false );

				log << ( fails.isEmpty() ? "PASS" : "FAIL: " + fails.join( "; " ) ) << "\n";
				log.flush();
				logf.close();
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEST HARNESS (WW_POSEDRAW_TEST=1): Pose Mode draws the skeleton and picks
	// bones. grab() can't see the native GL child (airspace), so this uses
	// grabFramebuffer + a pixel diff to confirm the skeleton overlay appears,
	// checks the bone list built, and confirms poseBoneAt() resolves a bone at
	// a drawn bone's screen position.
	// Log: release/ww_posedraw_test.log.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_POSEDRAW_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1200, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_posedraw_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				QStringList fails;
				auto pump = [skope]() {
					skope->ogl->update(); qApp->processEvents();
					skope->ogl->update(); qApp->processEvents();
				};
				auto pixDelta = []( const QImage & a, const QImage & b ) {
					if ( a.isNull() || b.isNull() || a.size() != b.size() ) return -1;
					int n = 0;
					for ( int y = 0; y < a.height(); y += 2 )
						for ( int x = 0; x < a.width(); x += 2 )
							if ( a.pixel( x, y ) != b.pixel( x, y ) ) n++;
					return n;
				};
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }
					// the Pose dock may already have entered pose mode during
					// init; force a clean off->on so the pixel diff is meaningful
					skope->ogl->setPoseMode( false );
					pump();
					QImage before = skope->ogl->grabFramebuffer();
					skope->ogl->setPoseMode( true );
					pump();
					log << "pose mode active: " << skope->ogl->poseModeActive() << "\n";
					log << "bones drawn/pickable: " << skope->ogl->poseBoneCountForTest() << "\n";
					if ( !skope->ogl->poseModeActive() )
						fails << "pose mode did not activate";
					if ( skope->ogl->poseBoneCountForTest() < 1 )
						fails << "no bones in the skeleton";

					QImage after = skope->ogl->grabFramebuffer();
					after.save( QApplication::applicationDirPath() + "/ww_posedraw.png" );
					const int dPix = pixDelta( before, after );

					// also capture a labelled, face-filtered view for eyeballing
					skope->ogl->setPoseShowBoneNames( true );
					skope->ogl->setPoseBoneFilter( 2 );   // face sculpt
					pump();
					skope->ogl->grabFramebuffer().save(
						QApplication::applicationDirPath() + "/ww_posedraw_labelled.png" );
					skope->ogl->setPoseBoneFilter( 0 );
					skope->ogl->setPoseShowBoneNames( false );
					log << "framebuffer pixels changed by the skeleton overlay: " << dPix << "\n";
					if ( dPix == 0 )
						fails << "skeleton overlay did not draw (no pixels changed)";

					// pick: the screen position of a drawn bone must resolve to it
					int probe = skope->ogl->poseBoneProbeForTest();
					log << "poseBoneAt(a bone's screen pos) -> " << probe << "\n";
					if ( probe < 0 )
						fails << "poseBoneAt did not resolve a bone at its own drawn position";

					// RESET: pose a bone away from rest, then reset restores it
					if ( probe >= 0 ) {
						QModelIndex iB = nif->getBlockIndex( probe );
						const Vector3 rest = nif->get<Vector3>( iB, "Translation" );
						nif->set<Vector3>( iB, "Translation", rest + Vector3( 50, 0, 0 ) );
						pump();
						skope->ogl->poseResetBone( probe, 7 );   // all channels
						pump();
						const Vector3 back = nif->get<Vector3>( nif->getBlockIndex( probe ), "Translation" );
						const float d = ( back - rest ).length();
						log << "reset: bone moved +50 then reset, delta from rest = " << d << " (expect ~0)\n";
						if ( d > 1e-2f )
							fails << "reset did not restore the bone to its rest transform";
						// re-resolve probe: the snapshot reload may renumber picks,
						// but block numbers are stable across value edits
					}

					// MULTI-SELECT: two Shift+clicks accumulate into objSelection
					{
						QList<int> distinct;
						for ( int b = 0; b < nif->getBlockCount() && distinct.size() < 2; b++ )
							if ( nif->get<QString>( nif->getBlockIndex( b ), "Name" )
									.startsWith( QLatin1String( "skin_bone_" ) ) )
								distinct << b;
						if ( distinct.size() == 2 ) {
							auto clickBone = [&]( int blk, Qt::KeyboardModifiers mods ) {
								Node * n = skope->ogl->getScene()->getNode( nif, nif->getBlockIndex( blk ) );
								QPointF sp;
								if ( !n || !skope->ogl->worldToScreenForTest( n->worldTrans().translation, sp ) )
									return;
								QPoint lp( int( sp.x() ), int( sp.y() ) );
								QMouseEvent pr( QEvent::MouseButtonPress, lp, skope->ogl->mapToGlobal( lp ),
									Qt::LeftButton, Qt::LeftButton, mods );
								QMouseEvent rl( QEvent::MouseButtonRelease, lp, skope->ogl->mapToGlobal( lp ),
									Qt::LeftButton, Qt::LeftButton, mods );
								qApp->sendEvent( skope->ogl, &pr );
								qApp->sendEvent( skope->ogl, &rl );
							};
							clickBone( distinct[0], Qt::NoModifier );
							clickBone( distinct[1], Qt::ShiftModifier );   // add
							pump();
							const int selCount = skope->ogl->objectSelectionCountForTest();
							log << "multi-select: 2 Shift-clicks -> " << selCount << " selected (expect 2)\n";
							if ( selCount != 2 )
								fails << "Shift-click did not accumulate a multi-bone selection";
						}
					}

					// MIRROR: pose a left bone, mirror to the right, check the
					// counterpart moved and its X translation is the mirror image
					{
						int left = -1, right = -1;
						for ( int b = 0; b < nif->getBlockCount(); b++ ) {
							QString nm = nif->get<QString>( nif->getBlockIndex( b ), "Name" );
							if ( nm.startsWith( QLatin1String( "skin_bone_L_" ) ) ) {
								QString r = QStringLiteral( "skin_bone_R_" ) + nm.mid( 12 );
								for ( int c = 0; c < nif->getBlockCount(); c++ )
									if ( nif->get<QString>( nif->getBlockIndex( c ), "Name" ) == r ) {
										left = b; right = c; break;
									}
							}
							if ( left >= 0 ) break;
						}
						if ( left >= 0 ) {
							QModelIndex iL = nif->getBlockIndex( left );
							const Vector3 rBefore = nif->get<Vector3>( nif->getBlockIndex( right ), "Translation" );
							Vector3 lt = nif->get<Vector3>( iL, "Translation" );
							nif->set<Vector3>( iL, "Translation", lt + Vector3( 0, 5, 0 ) );  // pose left
							pump();
							int got = skope->ogl->poseMirrorBone( left );
							pump();
							const Vector3 rAfter = nif->get<Vector3>( nif->getBlockIndex( right ), "Translation" );
							const float moved = ( rAfter - rBefore ).length();
							log << "mirror: L='" << nif->get<QString>( iL, "Name" ) << "' -> counterpart block "
								<< got << ", right bone moved " << moved << " (expect > 0)\n";
							if ( got != right )
								fails << "mirror did not resolve the correct R counterpart";
							if ( moved < 1e-3f )
								fails << "mirror did not move the counterpart bone";
						} else {
							log << "mirror: no L/R bone pair in this rig; skipped\n";
						}
					}

					// a real click at that bone's screen position must SELECT it
					if ( probe >= 0 ) {
						Node * n = skope->ogl->getScene()->getNode( nif, nif->getBlockIndex( probe ) );
						QPointF sp;
						if ( n && skope->ogl->worldToScreenForTest( n->worldTrans().translation, sp ) ) {
							int pickedBlock = -1;
							QMetaObject::Connection c = QObject::connect( skope->ogl,
								&GLView::poseBonePicked, skope, [&pickedBlock]( int b ) { pickedBlock = b; } );
							// worldToScreen returns LOGICAL pixels, same space the
							// event position uses — no dpr conversion.
							QPoint local( int( sp.x() ), int( sp.y() ) );
							QMouseEvent press( QEvent::MouseButtonPress, local, skope->ogl->mapToGlobal( local ),
								Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
							QMouseEvent release( QEvent::MouseButtonRelease, local, skope->ogl->mapToGlobal( local ),
								Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
							qApp->sendEvent( skope->ogl, &press );
							qApp->sendEvent( skope->ogl, &release );
							pump();
							QObject::disconnect( c );
							log << "click at bone " << probe << "'s screen pos selected block "
								<< pickedBlock << " (active=" << skope->ogl->activeObjectBlock() << ")\n";
							if ( skope->ogl->activeObjectBlock() != probe )
								fails << "clicking a bone did not make it the active object";
						}
					}
				} while ( false );

				for ( const QString & f : fails )
					log << "  FAIL: " << f << "\n";
				log << ( fails.isEmpty() ? "PASS — skeleton draws and bones are pickable\n" : "FAILED\n" );
				log << "done\n";
				logf.close();
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEST HARNESS (WW_POSEEXTRAS_TEST=1): weight-influence overlay + proportional
	// editing. (1) select a skinned bone, turn Weights on, confirm the overlay
	// found vertices it drives. (2) proportional self-test: pick a central vertex,
	// enable proportional editing, begin a move, confirm neighbours were gathered
	// with a distance-decreasing falloff.
	// Log: release/ww_poseextras_test.log.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_POSEEXTRAS_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1000, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_poseextras_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				QStringList fails;
				auto pump = [skope]() { skope->ogl->update(); qApp->processEvents(); skope->ogl->update(); qApp->processEvents(); };
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }

					// (1) weight overlay
					skope->ogl->setPoseMode( true );
					pump();
					int bone = skope->ogl->poseBoneProbeForTest();
					if ( bone >= 0 ) {
						skope->ogl->getScene();
						skope->ogl->setObjectSelection( QSet<int>{ bone }, bone );
						skope->ogl->setPoseShowWeights( true );
						pump();
						const int wp = skope->ogl->poseWeightPointCountForTest();
						log << "weight overlay: bone " << bone << " drives " << wp << " vertex point(s)\n";
						if ( wp < 1 )
							fails << "weight overlay found no influenced vertices";
						skope->ogl->setPoseShowWeights( false );
					} else {
						log << "no bone for weight test\n";
					}
					skope->ogl->setPoseMode( false );
					pump();

					// (2) proportional editing gather
					bool monotonic = false;
					const int neigh = skope->ogl->proportionalSelfTestForTest( monotonic );
					log << "proportional editing: " << neigh << " neighbour(s) gathered, falloff decreases with distance: "
						<< ( monotonic ? "yes" : "no" ) << "\n";
					if ( neigh < 1 )
						fails << "proportional editing gathered no neighbours";
					if ( !monotonic )
						fails << "proportional falloff did not decrease with distance";
				} while ( false );
				for ( const QString & f : fails )
					log << "  FAIL: " << f << "\n";
				log << ( fails.isEmpty() ? "PASS — weight overlay + proportional editing work\n" : "FAILED\n" );
				log << "done\n";
				logf.close();
				QTimer * qd = new QTimer( qApp );
				QObject::connect( qd, &QTimer::timeout, qApp, []() {
					auto * mb = qobject_cast<QMessageBox *>( QApplication::activeModalWidget() );
					if ( !mb ) return;
					QAbstractButton * bn = mb->button( QMessageBox::Discard );
					if ( !bn ) bn = mb->button( QMessageBox::No );
					if ( !bn && !mb->buttons().isEmpty() ) bn = mb->buttons().first();
					if ( bn ) bn->click();
				} );
				qd->start( 100 );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEST HARNESS (WW_OSPOSE_TEST=1): Outfit Studio pose XML round-trip.
	// Enters pose mode (captures rest), poses a bone by a known Euler delta,
	// exports an OS pose XML, resets the bone, re-imports, and checks the bone
	// returns to the posed orientation — proving export/import are exact
	// inverses. Also parse-imports a real PA pose file if WW_OSPOSE_FILE is set.
	// Log: release/ww_ospose_test.log.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_OSPOSE_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1000, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_ospose_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				QStringList fails;
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }
					skope->ogl->setPoseMode( true );
					qApp->processEvents();
					const int b = skope->ogl->poseBoneProbeForTest();
					if ( b < 0 ) { fails << "no bone to pose"; break; }
					QModelIndex iB = nif->getBlockIndex( b );

					// pose the bone by an arbitrary multi-axis rotation, then
					// verify export->reset->import reproduces the SAME rotation
					// matrix (representation-independent — the OS rot vector math
					// only has to be a faithful inverse of itself)
					auto matDiff = []( const Matrix & a, const Matrix & b2 ) {
						float d = 0;
						for ( int r = 0; r < 3; r++ )
							for ( int c = 0; c < 3; c++ )
								d += std::fabs( a( r, c ) - b2( r, c ) );
						return d;
					};
					const Matrix restRot = nif->get<Matrix>( iB, "Rotation" );
					Matrix dr; dr.fromEuler( 0.3f, -0.2f, 0.5f );
					const Matrix posed = restRot * dr;
					nif->set<Matrix>( iB, "Rotation", posed );
					qApp->processEvents();

					const QString xml = QApplication::applicationDirPath() + "/ww_ospose_out.xml";
					QString err;
					if ( !skope->ogl->poseExportOutfitStudio( xml, QStringLiteral( "RoundTrip" ), &err ) ) {
						fails << ( "export failed: " + err );
						break;
					}

					skope->ogl->poseResetBone( b, 7 );
					qApp->processEvents();
					const int n = skope->ogl->poseImportOutfitStudio( xml, 1.0f, &err );
					log << "import applied " << n << " bone(s)\n";
					if ( n <= 0 ) { fails << ( "import failed: " + err ); break; }

					const Matrix back = nif->get<Matrix>( nif->getBlockIndex( b ), "Rotation" );
					const float dMat = matDiff( posed, back );
					log << "round-trip rotation-matrix diff = " << dMat << " (expect ~0)\n";
					if ( dMat > 1e-3f )
						fails << "OS pose export->import did not reproduce the bone rotation";

					// optional: parse a real PA pose file, count matched bones
					const QByteArray real = qgetenv( "WW_OSPOSE_FILE" );
					if ( !real.isEmpty() ) {
						int applied = skope->ogl->poseImportOutfitStudio(
							QString::fromLocal8Bit( real ), 1.0f, &err );
						log << "real file '" << QString::fromLocal8Bit( real ) << "': "
							<< applied << " bone(s) matched this skeleton (" << err << ")\n";
					}
				} while ( false );
				for ( const QString & f : fails )
					log << "  FAIL: " << f << "\n";
				log << ( fails.isEmpty() ? "PASS — OS pose XML round-trips exactly\n" : "FAILED\n" );
				log << "done\n";
				logf.close();
				QTimer * qd = new QTimer( qApp );
				QObject::connect( qd, &QTimer::timeout, qApp, []() {
					auto * mb = qobject_cast<QMessageBox *>( QApplication::activeModalWidget() );
					if ( !mb ) return;
					QAbstractButton * bn = mb->button( QMessageBox::Discard );
					if ( !bn ) bn = mb->button( QMessageBox::No );
					if ( !bn && !mb->buttons().isEmpty() ) bn = mb->buttons().first();
					if ( bn ) bn->click();
				} );
				qd->start( 100 );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEST HARNESS (WW_POSELIB_TEST=1): the folder-based pose library, driven
	// through the actual dock. Points the library at a throwaway folder using
	// the same QSettings key poseLibraryFolder() reads (proving the override
	// persists), poses a bone, saves a pose file into the folder the way "Save
	// current" does, nudges the dock's bone-search box so refresh() re-lists the
	// folder, confirms the saved pose shows up in the library list, then selects
	// it and clicks the dock's real Apply button — proving the list->select->
	// apply path is wired, not just the underlying OS import/export. Finally
	// clicks Delete (auto-answering the confirm) and checks the file is gone.
	// Log: release/ww_poselib_test.log.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_POSELIB_TEST" ) ) {
		// A persistent modal answerer: Delete pops a Yes/No confirm and any error
		// pops a warning; keep one running so a click that spins a nested event
		// loop never hangs the harness. Prefers Yes, then Ok/Discard/No/first.
		QTimer * modal = new QTimer( skope );
		QObject::connect( modal, &QTimer::timeout, skope, []() {
			auto * mb = qobject_cast<QMessageBox *>( QApplication::activeModalWidget() );
			if ( !mb ) return;
			// the save-on-quit prompt must be DECLINED — clicking Yes there opens a
			// native save dialog we can't drive (that was the earlier hang). The
			// Delete confirm ("Delete pose") must be ACCEPTED with Yes.
			QAbstractButton * bn = nullptr;
			if ( mb->windowTitle().contains( QStringLiteral( "Save Confirmation" ) ) ) {
				bn = mb->button( QMessageBox::No );
			} else {
				bn = mb->button( QMessageBox::Yes );
				if ( !bn ) bn = mb->button( QMessageBox::Ok );
				if ( !bn ) bn = mb->button( QMessageBox::Discard );
				if ( !bn && !mb->buttons().isEmpty() ) bn = mb->buttons().first();
			}
			if ( bn ) bn->click();
		} );
		modal->start( 50 );

		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1000, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_poselib_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				QStringList fails;
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }

					// point the library ROOT at a throwaway folder via the exact key
					// the library reads, and prove the override persists. Poses live
					// in <root>/Poses.
					const QString folder = QApplication::applicationDirPath() + "/ww_poselib";
					const QString poseFolder = QDir( folder ).filePath( "Poses" );
					QDir().mkpath( poseFolder );
					for ( const QFileInfo & fi : QDir( poseFolder ).entryInfoList( { "*.xml" }, QDir::Files ) )
						QFile::remove( fi.absoluteFilePath() );
					{ QSettings s; s.setValue( "Settings/Library/Library Folder", folder ); s.sync(); }
					{ QSettings s2; if ( s2.value( "Settings/Library/Library Folder" ).toString() != folder )
						fails << "library folder QSettings override did not persist"; }

					skope->ogl->setPoseMode( true );
					qApp->processEvents();
					const int b = skope->ogl->poseBoneProbeForTest();
					if ( b < 0 ) { fails << "no bone to pose"; break; }
					QModelIndex iB = nif->getBlockIndex( b );

					// pose the bone, then save into <root>/Poses the way the dock's
					// "Save current" does (OS xml -> poseFolder/name.xml)
					const Matrix restRot = nif->get<Matrix>( iB, "Rotation" );
					Matrix dr; dr.fromEuler( 0.25f, -0.15f, 0.4f );
					const Matrix posed = restRot * dr;
					nif->set<Matrix>( iB, "Rotation", posed );
					qApp->processEvents();
					const QString saved = QDir( poseFolder ).filePath( "HarnessPose.xml" );
					QString err;
					if ( !skope->ogl->poseExportOutfitStudio( saved, "HarnessPose", &err ) ) {
						fails << ( "save to library failed: " + err ); break;
					}
					log << "saved pose file exists = " << QFile::exists( saved ) << "\n";

					// drive the dock's refresh via the bone-search signal and
					// confirm the saved pose is now listed in the library
					QDockWidget * dock = skope->findChild<QDockWidget *>( "PoseManagerDock" );
					auto * search = dock ? dock->findChild<QLineEdit *>( "PoseBoneSearch" ) : nullptr;
					auto * poseList = dock ? dock->findChild<QListWidget *>( "PosePoseList" ) : nullptr;
					auto * folderLbl = dock ? dock->findChild<QLabel *>( "PoseLibraryFolderLabel" ) : nullptr;
					if ( !dock || !search || !poseList ) { fails << "dock widgets not found"; break; }
					search->setText( "zzzrefresh" ); qApp->processEvents();
					search->clear(); qApp->processEvents();
					log << "folder label = '" << ( folderLbl ? folderLbl->text() : QString() ) << "'\n";
					log << "pose list count after refresh = " << poseList->count() << "\n";
					// re-find the list item on demand: every model edit fires a
					// queued refresh() that CLEARS and rebuilds the list, so any
					// cached QListWidgetItem* is dangling after processEvents.
					auto findPose = [&]() -> QListWidgetItem * {
						for ( int r = 0; r < poseList->count(); r++ )
							if ( poseList->item( r )->text() == "HarnessPose" )
								return poseList->item( r );
						return nullptr;
					};
					if ( !findPose() ) { fails << "saved pose not listed in the library"; break; }

					auto matDiff = []( const Matrix & a, const Matrix & b2 ) {
						float d = 0;
						for ( int r = 0; r < 3; r++ )
							for ( int c = 0; c < 3; c++ )
								d += std::fabs( a( r, c ) - b2( r, c ) );
						return d;
					};

					// (a) isolate the core: reset, then import the saved file
					// DIRECTLY at full strength. This is exactly what WW_OSPOSE
					// proves; if it fails HERE the harness state differs, if only
					// the button fails it's the dock wiring.
					skope->ogl->poseResetBone( b, 7 );
					qApp->processEvents();
					const Matrix atRest = nif->get<Matrix>( nif->getBlockIndex( b ), "Rotation" );
					log << "posed-vs-rest displacement = " << matDiff( posed, atRest ) << " (bone did reset)\n";
					const int nd = skope->ogl->poseImportOutfitStudio( saved, 1.0f, &err );
					qApp->processEvents();
					const Matrix backDirect = nif->get<Matrix>( nif->getBlockIndex( b ), "Rotation" );
					log << "DIRECT import n=" << nd << " diff = " << matDiff( posed, backDirect ) << " (expect ~0)\n";

					// (b) the dock button path: reset FIRST (this fires the queued
					// refresh), let it settle, THEN re-find + select the pose and
					// click the real Apply button.
					skope->ogl->poseResetBone( b, 7 );
					qApp->processEvents();
					auto * slider = dock->findChild<QSlider *>( "PoseBlendSlider" );
					log << "blend slider value = " << ( slider ? slider->value() : -1 ) << "\n";
					QListWidgetItem * item = findPose();
					if ( !item ) { fails << "saved pose vanished from list after reset"; break; }
					poseList->setCurrentItem( item );
					log << "poseList currentItem = '"
						<< ( poseList->currentItem() ? poseList->currentItem()->text() : QString() ) << "'\n";
					QPushButton * applyBtn = nullptr;
					for ( QPushButton * pb : dock->findChildren<QPushButton *>() )
						if ( pb->text() == "Apply" ) applyBtn = pb;
					if ( !applyBtn ) { fails << "Apply button not found"; break; }
					applyBtn->click();
					qApp->processEvents();
					const Matrix back = nif->get<Matrix>( nif->getBlockIndex( b ), "Rotation" );
					const float dMat = matDiff( posed, back );
					log << "BUTTON apply-from-library rotation diff = " << dMat << " (expect ~0)\n";
					if ( dMat > 1e-3f )
						fails << "applying the library pose did not reproduce the bone rotation";

					// Delete via the dock button (confirm auto-answered) removes it.
					// Apply also fired a refresh, so re-find the item again.
					QPushButton * delBtn = nullptr;
					for ( QPushButton * pb : dock->findChildren<QPushButton *>() )
						if ( pb->text() == "Delete" ) delBtn = pb;
					QListWidgetItem * delItem = findPose();
					if ( !delBtn || !delItem ) {
						fails << "Delete precondition missing (button or list item)";
					} else {
						poseList->setCurrentItem( delItem );
						log << "before Delete: currentItem = '"
							<< ( poseList->currentItem() ? poseList->currentItem()->text() : QString() )
							<< "', file exists = " << QFile::exists( saved ) << "\n";
						delBtn->click();
						qApp->processEvents();
						log << "pose file exists after Delete = " << QFile::exists( saved ) << "\n";
						if ( QFile::exists( saved ) )
							fails << "Delete did not remove the pose file";
					}
				} while ( false );
				for ( const QString & f : fails )
					log << "  FAIL: " << f << "\n";
				log << ( fails.isEmpty() ? "PASS — folder pose library saves, lists, applies, deletes\n" : "FAILED\n" );
				log << "done\n";
				logf.close();
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEST HARNESS (WW_POSESETTINGS_TEST=1): the General -> Poses settings tab.
	// Opens the Settings dialog (loadSettings populates the field), types a test
	// folder into the pose-library line edit, marks the pane modified, and hits
	// Apply (saveSettings). Then it (1) confirms the value persisted to EXACTLY
	// the key the dock reads — Settings/Poses/Pose Library Folder, the humanized
	// form of the "poseLibraryFolder" object name — and (2) shows the Pose
	// Manager dock and checks its folder label updates to the same folder,
	// proving the settings tab and the dock's Folder... button share one key.
	// Log: release/ww_posesettings_test.log.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_POSESETTINGS_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 800, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_posesettings_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				QStringList fails;
				do {
					if ( !ok ) { log << "load failed\n"; break; }
					SettingsDialog * opt = skope->getOptions();
					if ( !opt ) { fails << "no settings dialog"; break; }
					opt->show();               // showEvent emits loadSettings -> panes read()
					qApp->processEvents();

					auto * tab = opt->findChild<QWidget *>( "library" );
					auto * edt = opt->findChild<QLineEdit *>( "libraryFolder" );
					auto * btn = opt->findChild<QPushButton *>( "btnLibraryBrowse" );
					log << "library tab = " << ( tab ? 1 : 0 ) << ", line edit = " << ( edt ? 1 : 0 )
						<< ", browse button = " << ( btn ? 1 : 0 ) << "\n";
					if ( !tab || !edt || !btn ) { fails << "NifSkope Library settings widgets missing"; break; }

					const QString testDir = QApplication::applicationDirPath() + "/ww_posesettings";
					QDir().mkpath( testDir );
					edt->setText( testDir );
					// setText() doesn't mark the pane dirty; do it so write() persists
					for ( SettingsPane * p : opt->findChildren<SettingsPane *>() )
						if ( p->findChild<QLineEdit *>( "libraryFolder" ) )
							p->modifyPane();
					opt->apply();              // emits saveSettings -> panes write()
					qApp->processEvents();

					QSettings s;
					const QString stored = s.value( "Settings/Library/Library Folder" ).toString();
					log << "stored key value = '" << stored << "'\n";
					if ( stored != testDir )
						fails << "settings did not persist to key Settings/Library/Library Folder";

					// the dock must read the SAME key: show it, check the folder
					// label (which shows the library root's name)
					QDockWidget * dock = skope->findChild<QDockWidget *>( "PoseManagerDock" );
					if ( !dock ) { fails << "PoseManagerDock not found"; break; }
					dock->show();
					qApp->processEvents();
					auto * lbl = dock->findChild<QLabel *>( "PoseLibraryFolderLabel" );
					const QString labelText = lbl ? lbl->text() : QString();
					log << "dock folder label = '" << labelText << "' (expect 'ww_posesettings')\n";
					if ( labelText != QStringLiteral( "ww_posesettings" ) )
						fails << "dock did not pick up the folder set in Settings";

					// grab the NifSkope Library settings tab for visual review
					if ( auto * tabs = opt->findChild<QTabWidget *>( "general" ) ) {
						tabs->setCurrentWidget( tab );
						qApp->processEvents();
						opt->grab().save( QApplication::applicationDirPath() + "/ww_settings_poses.png" );
					}
				} while ( false );
				for ( const QString & f : fails )
					log << "  FAIL: " << f << "\n";
				log << ( fails.isEmpty()
					? "PASS — NifSkope Library settings tab writes the shared key and the dock reads it\n"
					: "FAILED\n" );
				log << "done\n";
				logf.close();
				// dismiss the save-on-quit prompt (decline), then quit
				QTimer * qd = new QTimer( qApp );
				QObject::connect( qd, &QTimer::timeout, qApp, []() {
					auto * mb = qobject_cast<QMessageBox *>( QApplication::activeModalWidget() );
					if ( !mb ) return;
					QAbstractButton * bn = mb->button( QMessageBox::No );
					if ( !bn ) bn = mb->button( QMessageBox::Discard );
					if ( !bn && !mb->buttons().isEmpty() ) bn = mb->buttons().first();
					if ( bn ) bn->click();
				} );
				qd->start( 50 );
				QTimer::singleShot( 300, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEST HARNESS (WW_MERGEARCH_TEST=1): load-skeleton-from-archive plumbing.
	// (1) the dock's "From game archive..." menu action exists; (2) the new
	// in-memory merge nifMergeData() produces the SAME result as nifMergeFile()
	// — merge the current NIF into itself both ways, undoing between, and compare
	// the block/node counts; (3) pickNifFromBrowser() opens and cancels cleanly
	// (a timer rejects whatever modal it shows). The archive EXTRACTION itself is
	// the NIF Browser's own proven path (extractConfiguredNifBytes), and needs
	// configured game archives to exercise for real. Log: ww_mergearch_test.log.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_MERGEARCH_TEST" ) ) {
		// auto-answer any modal the picker raises (its QDialog, or a "no archives"
		// info box), and decline the save-on-quit prompt so nothing hangs
		QTimer * mergeArchModal = new QTimer( skope );
		QObject::connect( mergeArchModal, &QTimer::timeout, skope, []() {
			QWidget * w = QApplication::activeModalWidget();
			if ( !w ) return;
			if ( auto * mb = qobject_cast<QMessageBox *>( w ) ) {
				if ( mb->windowTitle().contains( QStringLiteral( "Save Confirmation" ) ) ) {
					if ( auto * no = mb->button( QMessageBox::No ) ) no->click();
				} else if ( !mb->buttons().isEmpty() ) {
					mb->buttons().first()->click();
				}
			} else if ( auto * dlg = qobject_cast<QDialog *>( w ) ) {
				dlg->reject();
			}
		} );
		mergeArchModal->start( 50 );

		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 900, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_mergearch_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				QStringList fails;
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }

					// (1) the dock's archive menu action exists
					QDockWidget * dock = skope->findChild<QDockWidget *>( "PoseManagerDock" );
					QAction * arch = dock ? dock->findChild<QAction *>( "PoseLoadSkeletonArchiveAction" ) : nullptr;
					log << "archive menu action present = " << ( arch ? 1 : 0 ) << "\n";
					if ( !arch ) fails << "Load-skeleton 'From game archive' action missing";

					// (2) nifMergeData(bytes) == nifMergeFile(path). Merge the
					// current NIF into itself both ways, undoing between.
					const QString src = nif->getFileInfo().absoluteFilePath();
					if ( src.isEmpty() || !QFile::exists( src ) ) { fails << "no source path to merge"; break; }
					const int base = nif->getBlockCount();

					QByteArray bytes;
					{ QFile f( src ); if ( f.open( QIODevice::ReadOnly ) ) { bytes = f.readAll(); f.close(); } }
					NifMergeResult rd;
					const bool okD = nifMergeData( nif, bytes, QStringLiteral( "self.nif" ), true, rd );
					const int afterData = nif->getBlockCount();
					log << "nifMergeData ok=" << okD << " blocksAdded=" << rd.blocksAdded
						<< " reused=" << rd.nodesReused << " added=" << rd.nodesAdded
						<< " count " << base << "->" << afterData << "\n";
					if ( nif->undoStack ) nif->undoStack->undo();
					qApp->processEvents();
					const int afterUndo = nif->getBlockCount();
					log << "block count after undo = " << afterUndo << " (expect " << base << ")\n";
					if ( afterUndo != base ) fails << "merge did not undo cleanly";

					NifMergeResult rf;
					const bool okF = nifMergeFile( nif, src, true, rf );
					log << "nifMergeFile ok=" << okF << " blocksAdded=" << rf.blocksAdded
						<< " reused=" << rf.nodesReused << " added=" << rf.nodesAdded << "\n";
					if ( nif->undoStack ) nif->undoStack->undo();
					qApp->processEvents();

					if ( !okD || !okF )
						fails << "a merge failed";
					else if ( rd.blocksAdded != rf.blocksAdded || rd.nodesReused != rf.nodesReused
						|| rd.nodesAdded != rf.nodesAdded )
						fails << "nifMergeData and nifMergeFile produced different results";

					// (3) the picker opens and cancels cleanly (timer rejects it)
					QByteArray pb; QString pl;
					const bool picked = skope->pickNifFromBrowser( skope, pb, pl );
					log << "pickNifFromBrowser (auto-cancelled) returned " << picked << " (expect 0)\n";
					if ( picked ) fails << "picker returned true when cancelled";
				} while ( false );
				for ( const QString & f : fails )
					log << "  FAIL: " << f << "\n";
				log << ( fails.isEmpty()
					? "PASS — archive merge plumbing wired; nifMergeData matches nifMergeFile\n"
					: "FAILED\n" );
				log << "done\n";
				logf.close();
				QTimer::singleShot( 200, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEST HARNESS (WW_POSEHIER_TEST=1): is bone-by-bone posing actually usable?
	// Two properties decide that, beyond "a bone moves the mesh" (WW_POSE_TEST):
	//   1. HIERARCHY — rotating a PARENT bone must move its child bones and the
	//      armour weighted to them, or you would have to transform all ~68 bones
	//      one at a time. Proven by composing each bone's world transform from
	//      the model (walk parents) and checking a child bone's world position
	//      moves when only its ancestor is rotated.
	//   2. CUMULATIVE — transforming several bones must stack into one pose, not
	//      overwrite. Proven by rotating three separate bones and watching the
	//      skinned bounds keep changing.
	// Log: release/ww_posehier_test.log.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_POSEHIER_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1200, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_posehier_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				QStringList fails;

				// world transform of a block, composed straight from the model
				auto worldTrans = [nif]( int block ) {
					Transform t;
					int b = block;
					QList<int> chain;
					while ( b >= 0 ) { chain.prepend( b ); b = nif->getParent( b ); }
					for ( int n : chain )
						t = t * Transform( nif, nif->getBlockIndex( n ) );
					return t;
				};
				auto pump = [skope]() {
					skope->ogl->update(); qApp->processEvents();
					skope->ogl->update(); qApp->processEvents();
				};

				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }
					Scene * scene = skope->ogl->getScene();
					if ( !scene ) { fails << "no scene"; break; }
					pump();

					// the skin bones of the first skinned shape
					Shape * shape = nullptr;
					for ( Shape * s : scene->shapes ) {
						if ( s && nif->getLink( nif->getBlockIndex( s->id() ), "Skin" ) >= 0 ) { shape = s; break; }
					}
					if ( !shape ) { fails << "no skinned shape"; break; }
					QModelIndex iSkin = nif->getBlockIndex( nif->getLink( nif->getBlockIndex( shape->id() ), "Skin" ) );
					QModelIndex iBones = nif->getIndex( iSkin, "Bones" );
					QSet<int> skinBones;
					for ( int r = 0; r < nif->rowCount( iBones ); r++ )
						skinBones.insert( nif->getLink( nif->getIndex( iBones, r ) ) );

					// Hierarchy: rotating a bone's ANCESTOR must carry the bone
					// (and the armour weighted to it) along — the shoulder→arm→
					// hand behaviour. Prefer an ancestor that is itself a skin
					// bone (a true "pose the shoulder" case); fall back to any
					// non-root ancestor node, which still proves propagation.
					int parent = -1, child = -1;
					QList<int> sortedBones = skinBones.values();
					std::sort( sortedBones.begin(), sortedBones.end() );
					for ( int c : sortedBones ) {
						int p = nif->getParent( c );
						while ( p >= 0 ) {
							if ( skinBones.contains( p ) ) { parent = p; child = c; break; }
							p = nif->getParent( p );
						}
						if ( parent >= 0 ) break;
					}
					if ( parent < 0 ) {
						// no skin-bone-to-skin-bone nesting (e.g. a flat facial
						// rig); prove propagation with any bone's parent node
						for ( int c : sortedBones ) {
							int p = nif->getParent( c );
							if ( p >= 0 ) { parent = p; child = c; break; }
						}
						log << "(no skin-bone chain in this skeleton; using a bone's parent node)\n";
					}
					if ( parent < 0 ) {
						log << "every skin bone is a root; hierarchy check not applicable\n";
					} else {
						log << "parent bone " << parent << " '" << nif->get<QString>( nif->getBlockIndex( parent ), "Name" )
							<< "'  child bone " << child << " '" << nif->get<QString>( nif->getBlockIndex( child ), "Name" ) << "'\n";
						const Vector3 childBefore = worldTrans( child ).translation;
						const Matrix rBefore = nif->get<Matrix>( nif->getBlockIndex( parent ), "Rotation" );
						Matrix rot; rot.fromEuler( 0, 0, float( 40.0 * 3.14159265 / 180.0 ) );
						nif->set<Matrix>( nif->getBlockIndex( parent ), "Rotation", rBefore * rot );
						const Vector3 childAfter = worldTrans( child ).translation;
						const float d = ( childAfter - childBefore ).length();
						log << "  rotating ONLY the parent moved the child bone's world position by " << d << " (expect > 0)\n";
						if ( d <= 1e-3f )
							fails << "child bone did not follow its parent — hierarchy is not propagating";
						nif->set<Matrix>( nif->getBlockIndex( parent ), "Rotation", rBefore );	// restore
					}

					// CUMULATIVE: rotate three bones in turn, bounds must keep moving
					QList<int> some = skinBones.values();
					std::sort( some.begin(), some.end() );
					pump();
					BoundSphere prev = shape->bounds();
					int stacked = 0;
					for ( int i = 0; i < some.size() && stacked < 3; i++ ) {
						int b = some.at( i );
						QModelIndex iB = nif->getBlockIndex( b );
						Matrix r0 = nif->get<Matrix>( iB, "Rotation" );
						Matrix rot; rot.fromEuler( 0, 0, float( 20.0 * 3.14159265 / 180.0 ) );
						nif->set<Matrix>( iB, "Rotation", r0 * rot );
						scene->transformDirty = true;
						pump();
						BoundSphere now = shape->bounds();
						const float moved = ( now.center - prev.center ).length() + std::fabs( now.radius - prev.radius );
						log << "  posed bone " << b << " -> bounds moved " << moved << "\n";
						if ( moved > 1e-4f ) stacked++;
						prev = now;
					}
					log << "bones that each further changed the pose: " << stacked << " of 3\n";
					if ( stacked < 2 )
						fails << "posing multiple bones did not accumulate";
				} while ( false );

				for ( const QString & f : fails )
					log << "  FAIL: " << f << "\n";
				log << ( fails.isEmpty()
					? "PASS — parent bones carry their children, and poses stack bone by bone\n" : "FAILED\n" );
				log << "done\n";
				logf.close();
				QTimer * quitDriver = new QTimer( qApp );
				QObject::connect( quitDriver, &QTimer::timeout, qApp, []() {
					auto * mb = qobject_cast<QMessageBox *>( QApplication::activeModalWidget() );
					if ( !mb ) return;
					QAbstractButton * btn = mb->button( QMessageBox::Discard );
					if ( !btn ) btn = mb->button( QMessageBox::No );
					if ( !btn && !mb->buttons().isEmpty() ) btn = mb->buttons().first();
					if ( btn ) btn->click();
				} );
				quitDriver->start( 100 );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEST HARNESS (WW_POSE_TEST=1): does moving a bone NiNode actually pose a
	// skinned mesh? Shape::updateBoneTransforms() derives each bone matrix from
	// bone->localTrans(skeletonRoot) — the LIVE node transform — so posing
	// should already work with no new feature. This proves it, or disproves it.
	//
	// Rotates the first bone that some shape is skinned to, then checks:
	//   - the shape's CPU-side boneTransforms changed (the skinning maths saw it),
	//   - its bounding sphere moved (the deformation is geometric, not cosmetic),
	//   - the rendered framebuffer changed (it reaches the screen).
	// Restores the bone afterwards and re-checks, so a false positive from some
	// unrelated per-frame jitter is caught.
	// Log: release/ww_pose_test.log.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_POSE_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1200, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_pose_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				QStringList fails;
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }
					Scene * scene = skope->ogl->getScene();
					if ( !scene ) { fails << "no scene"; break; }

					// force a full draw so shapes and bone transforms exist
					auto pump = [skope]() {
						skope->ogl->update();
						qApp->processEvents();
						skope->ogl->update();
						qApp->processEvents();
					};
					pump();

					// every skinned shape in the scene (Shape's own skin members
					// are protected, so judge from the model)
					QVector<Shape *> skinned;
					for ( Shape * s : scene->shapes ) {
						if ( !s )
							continue;
						QModelIndex iS = nif->getBlockIndex( s->id() );
						if ( nif->getLink( iS, "Skin" ) >= 0 )
							skinned.append( s );
					}
					if ( skinned.isEmpty() ) { fails << "no skinned shape in the scene"; break; }
					Shape * shape = skinned.first();
					log << skinned.size() << " skinned shape(s) in the scene\n";

					const int shapeBlock = shape->id();
					QModelIndex iShape = nif->getBlockIndex( shapeBlock );
					QModelIndex iSkin = nif->getBlockIndex( nif->getLink( iShape, "Skin" ) );
					QModelIndex iBones = nif->getIndex( iSkin, "Bones" );
					if ( !iBones.isValid() || nif->rowCount( iBones ) < 1 ) {
						fails << "shape has no Bones array";
						break;
					}
					const int boneBlock = nif->getLink( nif->getIndex( iBones, 0 ) );
					QModelIndex iBone = nif->getBlockIndex( boneBlock );
					if ( !iBone.isValid() ) { fails << "bone link is invalid"; break; }

					log << "shape block " << shapeBlock << " '" << nif->get<QString>( iShape, "Name" ) << "'"
						<< " skinned to " << nif->rowCount( iBones ) << " bone(s)\n";
					log << "posing bone block " << boneBlock << " '"
						<< nif->get<QString>( iBone, "Name" ) << "'\n";

					// bounds() is public and is recomputed by updateBoneTransforms()
					// straight from the live bone matrices, so it is a faithful
					// numeric proxy for "the skinning saw the bone move".
					auto bounds = [shape]() { return shape->bounds(); };

					const BoundSphere bsBefore = bounds();
					QHash<int, BoundSphere> shapeBoundsBefore;
					for ( Shape * s : skinned )
						shapeBoundsBefore.insert( s->id(), s->bounds() );
					QImage imgBefore = skope->ogl->grabFramebuffer();

					// rotate the bone 30 degrees about Z
					const Matrix rotBefore = nif->get<Matrix>( iBone, "Rotation" );
					Matrix rot;
					rot.fromEuler( 0.0f, 0.0f, float( 30.0 * 3.14159265 / 180.0 ) );
					nif->set<Matrix>( iBone, "Rotation", rotBefore * rot );
					scene->transformDirty = true;
					pump();

					const BoundSphere bsAfter = bounds();
					QImage imgAfter = skope->ogl->grabFramebuffer();

					auto pixDelta = []( const QImage & a, const QImage & b ) {
						if ( a.isNull() || b.isNull() || a.size() != b.size() )
							return -1;
						int n = 0;
						for ( int y = 0; y < a.height(); y += 2 )
							for ( int x = 0; x < a.width(); x += 2 )
								if ( a.pixel( x, y ) != b.pixel( x, y ) )
									n++;
						return n;
					};

					const float dBounds = ( bsAfter.center - bsBefore.center ).length()
						+ std::fabs( bsAfter.radius - bsBefore.radius );
					const int dPix = pixDelta( imgBefore, imgAfter );

					log << "skinned bounds delta: " << dBounds << " (expect > 0)\n";
					log << "  before c(" << bsBefore.center[0] << "," << bsBefore.center[1]
						<< "," << bsBefore.center[2] << ") r" << bsBefore.radius << "\n";
					log << "  after  c(" << bsAfter.center[0] << "," << bsAfter.center[1]
						<< "," << bsAfter.center[2] << ") r" << bsAfter.radius << "\n";
					log << "framebuffer pixels changed: " << dPix << "\n";

					if ( dBounds <= 1e-4f )
						fails << "skinned bounds unchanged — the mesh does NOT follow the bone";

					// The real claim for a merged armour set: EVERY piece whose
					// skin references the posed bone must follow it. If merge's
					// node de-duplication failed, some piece would be bound to a
					// private copy of the bone and would stay put.
					int followed = 0, shouldFollow = 0;
					for ( Shape * s : skinned ) {
						QModelIndex iS = nif->getBlockIndex( s->id() );
						QModelIndex iSk = nif->getBlockIndex( nif->getLink( iS, "Skin" ) );
						QModelIndex iBl = nif->getIndex( iSk, "Bones" );
						bool uses = false;
						for ( int r = 0; r < nif->rowCount( iBl ); r++ )
							if ( nif->getLink( nif->getIndex( iBl, r ) ) == boneBlock ) { uses = true; break; }
						if ( !uses )
							continue;
						shouldFollow++;
						const float d = ( s->bounds().center - shapeBoundsBefore.value( s->id() ).center ).length()
							+ std::fabs( s->bounds().radius - shapeBoundsBefore.value( s->id() ).radius );
						log << "  shape " << s->id() << " '" << nif->get<QString>( iS, "Name" )
							<< "' uses the bone, moved " << d << "\n";
						if ( d > 1e-4f )
							followed++;
					}
					log << "pieces bound to that bone: " << shouldFollow
						<< ", pieces that moved: " << followed << "\n";
					if ( shouldFollow > 0 && followed != shouldFollow )
						fails << QString( "%1 of %2 piece(s) bound to the bone did NOT move — "
							"they are on a private copy of the skeleton" )
							.arg( shouldFollow - followed ).arg( shouldFollow );
					if ( dPix == 0 )
						fails << "framebuffer identical — the pose does not reach the screen";
					if ( dPix < 0 )
						log << "  (framebuffer compare unavailable; relying on the numeric check)\n";

					// restore and confirm it goes back, so the deltas above were
					// caused by the edit and not by frame-to-frame noise
					nif->set<Matrix>( iBone, "Rotation", rotBefore );
					scene->transformDirty = true;
					pump();
					const BoundSphere bsBack = bounds();
					const float dBack = ( bsBack.center - bsBefore.center ).length()
						+ std::fabs( bsBack.radius - bsBefore.radius );
					log << "after restore, bounds delta vs original: " << dBack << " (expect ~0)\n";
					if ( dBack > 1e-2f )
						fails << "restoring the bone did not restore the mesh";
				} while ( false );

				for ( const QString & f : fails )
					log << "  FAIL: " << f << "\n";
				log << ( fails.isEmpty()
					? "PASS — posing a bone deforms the skinned mesh live\n" : "FAILED\n" );
				log << "done\n";
				logf.close();
				QTimer * quitDriver = new QTimer( qApp );
				QObject::connect( quitDriver, &QTimer::timeout, qApp, []() {
					auto * mb = qobject_cast<QMessageBox *>( QApplication::activeModalWidget() );
					if ( !mb )
						return;
					QAbstractButton * btn = mb->button( QMessageBox::Discard );
					if ( !btn )
						btn = mb->button( QMessageBox::No );
					if ( !btn && !mb->buttons().isEmpty() )
						btn = mb->buttons().first();
					if ( btn )
						btn->click();
				} );
				quitDriver->start( 100 );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEST HARNESS (WW_PINNED_TEST=1): Block Details pinned fields. Pins a field
	// on one block, then checks the pin follows to ANOTHER block of the same
	// type (the whole point — pins are per block TYPE, stored as a field path,
	// not as an item pointer or row number), that the star reaches the Name
	// column's display text, that the ★ pinned-only filter hides exactly the
	// unpinned rows, and that unpinning undoes all of it.
	// Log: release/ww_pinned_test.log.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_PINNED_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 800, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_pinned_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				QStringList fails;
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }

					// two blocks of the same type, with a common named field
					const QString field = QStringLiteral( "Flags" );
					QHash<QString, QVector<int>> byType;
					for ( int b = 0; b < nif->getBlockCount(); b++ )
						byType[nif->itemName( nif->getBlockIndex( b ) )].append( b );
					int blockA = -1, blockB = -1;
					QString type;
					for ( auto it = byType.constBegin(); it != byType.constEnd(); ++it ) {
						if ( it.value().size() < 2 )
							continue;
						if ( !nif->getIndex( nif->getBlockIndex( it.value().at( 0 ) ), field ).isValid() )
							continue;
						type = it.key(); blockA = it.value().at( 0 ); blockB = it.value().at( 1 );
						break;
					}
					if ( blockA < 0 ) { log << "no type with 2 blocks carrying a '" << field << "' field\n"; break; }
					log << "type '" << type << "' blocks A=" << blockA << " B=" << blockB
						<< " field '" << field << "'\n";

					// start clean so a previous run's settings cannot mask a failure
					skope->wwPinnedFields.clear();
					skope->wwSavePinnedFields();

					// --- pin on block A ---
					skope->select( nif->getBlockIndex( blockA ) );
					QModelIndex iA = nif->getIndex( nif->getBlockIndex( blockA ), field );
					const QString path = skope->wwFieldPath( iA );
					log << "field path '" << path << "'\n";
					if ( path.isEmpty() ) { fails << "wwFieldPath returned empty"; break; }
					skope->wwTogglePinField( iA );

					const NifItem * itemA = nif->getItem( iA );
					if ( !nif->pinnedItems.contains( itemA ) )
						fails << "after pin, block A's item is not in pinnedItems";
					if ( !skope->wwIsFieldPinned( iA ) )
						fails << "after pin, wwIsFieldPinned(A) is false";

					// the star must reach the Name column's display text
					const QString shownA = nif->index( iA.row(), NifModel::NameCol,
						iA.parent() ).data( Qt::DisplayRole ).toString();
					log << "block A name cell: '" << shownA << "'\n";
					if ( !shownA.contains( QString::fromUtf8( "\xe2\x98\x85" ) ) )
						fails << "pinned field's Name cell has no star";

					// --- the point: switch to block B, same type ---
					skope->select( nif->getBlockIndex( blockB ) );
					QModelIndex iB = nif->getIndex( nif->getBlockIndex( blockB ), field );
					const NifItem * itemB = nif->getItem( iB );
					if ( !nif->pinnedItems.contains( itemB ) )
						fails << "pin did NOT follow to block B (per-type resolution broken)";
					if ( !skope->wwIsFieldPinned( iB ) )
						fails << "wwIsFieldPinned(B) is false";
					log << "pin followed to block B: "
						<< ( nif->pinnedItems.contains( itemB ) ? "yes" : "NO" ) << "\n";

					// --- pinned-only filter hides exactly the unpinned rows ---
					// NOTE: read the visibility the view ACTUALLY applied, via
					// QTreeView. NifTreeView's same-named overload marks its row
					// argument [[maybe_unused]] and reads index.internalPointer(),
					// so passing (row, parent) asks about the PARENT row instead.
					QModelIndex rootB = nif->getBlockIndex( blockB );
					int visibleBefore = 0;
					for ( int r = 0; r < nif->rowCount( rootB ); r++ )
						if ( !skope->tree->QTreeView::isRowHidden( r, rootB ) )
							visibleBefore++;
					skope->blockDetailsPinFilter->setChecked( true );
					int visibleAfter = 0, pinnedVisible = 0;
					for ( int r = 0; r < nif->rowCount( rootB ); r++ ) {
						if ( skope->tree->QTreeView::isRowHidden( r, rootB ) )
							continue;
						visibleAfter++;
						if ( nif->getItem( nif->index( r, 0, rootB ) ) == itemB )
							pinnedVisible++;
					}
					log << "top-level rows visible: " << visibleBefore
						<< " -> " << visibleAfter << " with the pin filter on"
						<< " (pinned row visible: " << pinnedVisible << ")\n";
					if ( visibleAfter != 1 || pinnedVisible != 1 )
						fails << QString( "pin filter should leave exactly the 1 pinned row, left %1" )
							.arg( visibleAfter );

					// --- unpin undoes everything ---
					skope->blockDetailsPinFilter->setChecked( false );
					skope->wwTogglePinField( iB );
					if ( nif->pinnedItems.contains( itemB ) )
						fails << "after unpin, item still in pinnedItems";
					if ( skope->wwPinnedFields.contains( type ) )
						fails << "after unpin, the type still has a pin set";
					const QString shownAfter = nif->index( iB.row(), NifModel::NameCol,
						iB.parent() ).data( Qt::DisplayRole ).toString();
					if ( shownAfter.contains( QString::fromUtf8( "\xe2\x98\x85" ) ) )
						fails << "after unpin, the star is still on the Name cell";

					// --- persistence round trip ---
					skope->wwTogglePinField( iB );
					skope->wwPinnedFields.clear();
					skope->wwLoadPinnedFields();
					if ( !skope->wwPinnedFields.value( type ).contains( path ) )
						fails << "pin did not survive a settings save/load round trip";
					log << "persistence round trip: "
						<< ( skope->wwPinnedFields.value( type ).contains( path ) ? "ok" : "LOST" ) << "\n";
					skope->wwPinnedFields.clear();
					skope->wwSavePinnedFields();
				} while ( false );

				for ( const QString & f : fails )
					log << "  FAIL: " << f << "\n";
				log << ( fails.isEmpty() ? "PASS\n" : "FAILED\n" );
				log << "done\n";
				logf.close();
				QTimer * quitDriver = new QTimer( qApp );
				QObject::connect( quitDriver, &QTimer::timeout, qApp, []() {
					auto * mb = qobject_cast<QMessageBox *>( QApplication::activeModalWidget() );
					if ( !mb )
						return;
					QAbstractButton * btn = mb->button( QMessageBox::Discard );
					if ( !btn )
						btn = mb->button( QMessageBox::No );
					if ( !btn && !mb->buttons().isEmpty() )
						btn = mb->buttons().first();
					if ( btn )
						btn->click();
				} );
				quitDriver->start( 100 );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_WORKSPACE_TEST=a.nif;b.nif): the Loaded NIFs display
	 * toggles. Adds the given files as workspace documents, sets the first solid
	 * and the second semi-transparent, and saves two pictures: the list (do the
	 * row buttons draw, and does the lit one match the state?) and the viewport
	 * (does the ghost actually render translucent?).
	 * Log: release/ww_workspace_test.log, ww_workspace_list.png / _view.png.
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_WORKSPACE_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1200, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_workspace_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass )
						fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				auto settle = []( int ms ) {
					QEventLoop loop;
					QTimer::singleShot( ms, &loop, &QEventLoop::quit );
					loop.exec();
					QApplication::processEvents();
				};
				do {
					if ( !ok ) { log << "load failed\n"; break; }
					const QStringList files =
						qEnvironmentVariable( "WW_WORKSPACE_TEST" ).split( QLatin1Char( ';' ),
							Qt::SkipEmptyParts );
					int added = 0;
					for ( const QString & f : files )
						if ( skope->addWorkspaceDocumentFromFile( f ) )
							added++;
					log << "added " << added << " of " << files.size() << " workspace document(s)\n";
					check( "every file joined the workspace", added == files.size() && added > 0 );
					check( "...and the workspace counts them",
						skope->workspaceDocumentCount() == added );

					/* A document that has just joined must be VISIBLE.
					 *
					 * It used to depend on the tree selection, so a file added
					 * without being clicked was invisible and looked like it had
					 * failed to load.
					 */
					{
						int hidden = 0;
						for ( int i = 0; i < added; i++ )
							if ( skope->workspaceDisplayMode( i ) == 0 )
								hidden++;
						check( "every new document starts visible (eye open)", hidden == 0 );
					}

					// WW_WORKSPACE_MODES=1;2 sets each document's mode explicitly
					// (0 hidden, 1 solid, 2 ghost); default is first solid, rest ghost
					const QStringList modes =
						qEnvironmentVariable( "WW_WORKSPACE_MODES" ).split( QLatin1Char( ';' ),
							Qt::SkipEmptyParts );
					for ( int i = 0; i < added; i++ ) {
						const int m = ( i < modes.size() ) ? modes.at( i ).toInt()
							: ( i == 0 ? 1 : 2 );
						check( QStringLiteral( "document %1 set to mode %2" ).arg( i ).arg( m ),
							skope->setWorkspaceDisplayMode( i, m ) );
					}
					settle( 1200 );

					/* Selecting rows must not change what is drawn.
					 *
					 * Selection used to WRITE sessionPreviewVisible, and
					 * rebuildLoadedNifsBrowserGroup selected rows back from it, so
					 * the two were the same thing: clicking a row to aim a menu at it
					 * hid every other document, and there was no way to say "this
					 * one" without also changing the viewport.
					 *
					 * Record the modes, move the selection about, require them
					 * unchanged. This is the check that would have caught it.
					 */
					QVector<int> before;
					for ( int i = 0; i < added; i++ )
						before.append( skope->workspaceDisplayMode( i ) );
					log << "modes after setup:";
					for ( const int m : before )
						log << " " << m;
					log << "\n";

					auto * view = skope->findChild<QTreeView *>( QStringLiteral( "LoadedNifsView" ) );
					check( "the Loaded NIFs view is reachable", view != nullptr );
					if ( view && view->selectionModel() && view->model() ) {
						auto modesNow = [&]() {
							QVector<int> now;
							for ( int i = 0; i < added; i++ )
								now.append( skope->workspaceDisplayMode( i ) );
							return now;
						};
						const int rows = view->model()->rowCount();
						auto pick = [&]( int row ) {
							view->selectionModel()->clearSelection();
							if ( row >= 0 && row < rows )
								view->selectionModel()->select( view->model()->index( row, 0 ),
									QItemSelectionModel::Select | QItemSelectionModel::Rows );
							settle( 150 );
						};

						pick( 0 );
						check( "selecting the first row leaves visibility alone",
							modesNow() == before );
						pick( rows - 1 );
						check( "selecting the last row leaves visibility alone",
							modesNow() == before );
						view->selectAll();
						settle( 150 );
						check( "selecting every row leaves visibility alone",
							modesNow() == before );
						view->selectionModel()->clearSelection();
						settle( 150 );
						check( "clearing the selection leaves visibility alone",
							modesNow() == before );
						/* Leave TWO rows selected for the shot.
						 *
						 * One selected row cannot show the difference between the
						 * active member of a selection and the rest of it, which is
						 * the Block List convention this list now follows: primary
						 * #FF9D00 on light blue, secondary #FF7200 on dark blue. It
						 * also shows a selected row that is not the only visible one —
						 * the state that was unreachable while selection meant
						 * visibility.
						 */
						view->selectionModel()->clearSelection();
						if ( rows > 1 ) {
							view->selectionModel()->select(
								QItemSelection( view->model()->index( rows - 2, 0 ),
									view->model()->index( rows - 1, 0 ) ),
								QItemSelectionModel::Select | QItemSelectionModel::Rows );
							view->selectionModel()->setCurrentIndex(
								view->model()->index( rows - 2, 0 ),
								QItemSelectionModel::NoUpdate );
						}
						settle( 150 );
						check( "two rows can be selected at once",
							view->selectionModel()->selectedRows().size() == 2 );
					}

					/* WW_WORKSPACE_MERGE=1: "Merge Into", the in-place path.
					 *
					 * The other merge writes a new file and leaves every loaded
					 * document alone. This one changes a document you have open, so
					 * what needs proving is that the target GREW by the donor and the
					 * donor was left untouched — a merge that silently emptied the
					 * donor, or edited the wrong one of the two, would look the same
					 * from the dialog.
					 *
					 * It ends in a modal summary, so a driver dismisses it; that also
					 * means the real code path runs, dialog included.
					 */
					if ( added >= 2 && qEnvironmentVariableIsSet( "WW_WORKSPACE_MERGE" ) ) {
						const int targetBefore = skope->workspaceBlockCount( 0 );
						const int donorBefore = skope->workspaceBlockCount( 1 );
						auto * dismiss = new QTimer( skope );
						QObject::connect( dismiss, &QTimer::timeout, skope, []() {
							if ( auto * mb = qobject_cast<QMessageBox *>(
									QApplication::activeModalWidget() ) ) {
								if ( !mb->buttons().isEmpty() )
									mb->buttons().first()->click();
							}
						} );
						dismiss->start( 100 );
						const bool merged = skope->mergeWorkspaceDocumentsInto( 0, { 1 } );
						dismiss->stop();
						settle( 400 );
						const int targetAfter = skope->workspaceBlockCount( 0 );
						const int donorAfter = skope->workspaceBlockCount( 1 );
						log << "merge into: target " << targetBefore << " -> " << targetAfter
							<< ", donor " << donorBefore << " -> " << donorAfter << "\n";
						check( "the in-place merge reported success", merged );
						check( "the target absorbed the donor", targetAfter > targetBefore );
						check( "the donor was left alone", donorAfter == donorBefore );
						// Dedupe means the sum is an upper bound, never a target.
						check( "the target did not exceed target + donor",
							targetAfter <= targetBefore + donorBefore );
						check( "merging into a document that is not there fails",
							!skope->mergeWorkspaceDocumentsInto( 0, { 99 } ) );
						check( "merging a document into itself fails",
							!skope->mergeWorkspaceDocumentsInto( 0, { 0 } ) );
					}

					const QString listShot =
						QApplication::applicationDirPath() + "/ww_workspace_list.png";
					const QString viewShot =
						QApplication::applicationDirPath() + "/ww_workspace_view.png";
					check( "the Loaded NIFs list renders", skope->grabLoadedNifsView( listShot ) );
					check( "the viewport renders", skope->ogl->grabFramebuffer().save( viewShot ) );
					log << "  " << listShot << "\n  " << viewShot << "\n";

					/* Removal, last because it empties the workspace.
					 *
					 * Same call the X key makes. Selecting every row and removing must
					 * take the count to zero; a per-row implementation would leave the
					 * unclicked ones behind, which is what this catches.
					 */
					if ( view && view->selectionModel() ) {
						view->selectAll();
						settle( 150 );
						const int before2 = skope->workspaceDocumentCount();
						const int gone = skope->removeSelectedWorkspaceDocuments();
						settle( 200 );
						log << "removal: " << before2 << " documents, " << gone
							<< " removed, " << skope->workspaceDocumentCount() << " left\n";
						check( "removing the selection removes every one of them",
							gone == before2 && skope->workspaceDocumentCount() == 0 );
					}
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS\n" : "CHECK: failures above\n" );
				log << "done\n";
				logf.close();
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_REVERT_TEST=<donor.nif>): can a loaded NIF be modified and
	 * put back exactly, without saving?
	 *
	 * This gates the whole workspace idea: if several loaded documents are to be
	 * posed, synced and frozen as a scratch space, then "undo everything" has to
	 * mean the bytes are what they were, not merely that the model looks right.
	 * So every check here compares the SERIALIZED file, not the model state — a
	 * revert that leaves an extra header string or a grown array is not a revert.
	 *
	 * Three edit paths, because they do not share a mechanism: a pose import (the
	 * thing the user will actually do), a merge (which routes through
	 * nifSnapshotOp), and a direct set<> (which does not).
	 * Log: release/ww_revert_test.log.
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_REVERT_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 900, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_revert_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass )
						fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				auto bytes = [nif]() {
					QByteArray b;
					QBuffer buf( &b );
					if ( buf.open( QIODevice::WriteOnly ) )
						nif->save( buf );
					return b;
				};
				auto undoAll = [nif]() {
					if ( !nif->undoStack )
						return;
					while ( nif->undoStack->canUndo() )
						nif->undoStack->undo();
				};
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }
					const QByteArray loaded = bytes();
					log << "loaded: " << loaded.size() << " bytes, "
						<< nif->getBlockCount() << " blocks\n";
					check( "the primary document has an undo stack", nif->undoStack != nullptr );

					// --- path 1: a pose import, the operation this workflow is for
					const QString donor = qEnvironmentVariable( "WW_REVERT_TEST" );
					if ( QFileInfo::exists( donor ) && donor.endsWith( QStringLiteral( ".xml" ) ) ) {
						QString err;
						const int n = skope->ogl->poseImportOutfitStudio( donor, 1.0f, &err );
						log << "pose import: " << n << " bone(s)\n";
						check( "a pose import changes the file", n > 0 && bytes() != loaded );
						undoAll();
						check( "...and undo puts every byte back", bytes() == loaded );
					}

					// --- path 2: a merge, which routes through nifSnapshotOp
					if ( QFileInfo::exists( donor ) && donor.endsWith( QStringLiteral( ".nif" ) ) ) {
						NifMergeResult r;
						const bool merged = nifMergeFile( nif, donor, true, r );
						log << "merge: " << ( merged ? "ok" : "failed" ) << ", +"
							<< r.blocksAdded << " blocks\n";
						check( "a merge changes the file", merged && bytes() != loaded );
						undoAll();
						const QByteArray after = bytes();
						check( "...and undo puts every byte back", after == loaded );
						if ( after != loaded )
							log << "     " << after.size() << " bytes vs " << loaded.size()
								<< ", " << nif->getBlockCount() << " blocks\n";
					}

					// --- path 3: a bare set<>, the path with no command behind it
					{
						QModelIndex iRoot = nif->getBlockIndex( 0 );
						const QString was = nif->get<QString>( iRoot, "Name" );
						nif->set<QString>( iRoot, "Name", was + QStringLiteral( "_scratch" ) );
						const bool changed = ( bytes() != loaded );
						undoAll();
						const bool restored = ( bytes() == loaded );
						log << "direct set<>: changed=" << changed << " restoredByUndo=" << restored << "\n";
						check( "a direct set<> is NOT undoable — it needs a command",
							changed && !restored );
						/* And putting the value back by hand does NOT restore the
						 * file. Setting a name allocates a string in the header
						 * table, and that table only ever GROWS — writing the old
						 * name back reuses its entry and leaves the new one behind
						 * as an orphan, so the bytes differ even though every
						 * value is correct. Revert therefore means undo, never
						 * manual re-entry; that is the rule this workspace needs.
						 */
						nif->set<QString>( iRoot, "Name", was );
						const bool handRestored = ( bytes() == loaded );
						log << "     re-entering the old value by hand restored the bytes: "
							<< handRestored << " (" << bytes().size() << " vs "
							<< loaded.size() << ")\n";
						check( "...and re-entering the value by hand does NOT restore them "
							"(the header string table only grows)", !handRestored );
					}
					check( "the document is clean at the end",
						nif->undoStack && nif->undoStack->isClean() );
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS\n" : "CHECK: failures above\n" );
				log << "done\n";
				logf.close();
				if ( nif && nif->undoStack )
					nif->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_SHOT=<out.png>): load, let the scene settle, save the
	 * viewport, quit. No assertions — some things are only checkable by looking,
	 * and grabFramebuffer renders offscreen, so this costs no focus and can run
	 * while someone else is using the machine. WW_SHOT_VIEW=front|left|top picks
	 * the camera; the default is whatever the file opens with.
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_SHOT" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1500, skope, [skope, ok]() {
				if ( ok ) {
					const QString view = qEnvironmentVariable( "WW_SHOT_VIEW" );
					if ( view == QLatin1String( "front" ) )
						skope->ui->aViewFront->trigger();
					else if ( view == QLatin1String( "left" ) )
						skope->ui->aViewLeft->trigger();
					else if ( view == QLatin1String( "top" ) )
						skope->ui->aViewTop->trigger();
					QEventLoop loop;
					QTimer::singleShot( 700, &loop, &QEventLoop::quit );
					loop.exec();
					/* WW_SHOT_TIME=<seconds>: park the scene clock before grabbing.
					 * Effects open over time — a Fallout 4 ArtObject at t=0 is
					 * usually scaled or faded to nothing, so a screenshot of the
					 * first frame is a screenshot of an empty effect. Stepped in
					 * small increments rather than jumped, because particle systems
					 * integrate frame to frame and cannot be evaluated at an
					 * arbitrary t the way a keyframe controller can.
					 */
					if ( qEnvironmentVariableIsSet( "WW_SHOT_TIME" ) ) {
						const float want = qEnvironmentVariable( "WW_SHOT_TIME" ).toFloat();
						const float step = 1.0f / 30.0f;
						for ( float t = 0.0f; t < want; t += step ) {
							skope->ogl->setSceneTime( std::min( t + step, want ) );
							QApplication::processEvents();
						}
						QEventLoop settle;
						QTimer::singleShot( 300, &settle, &QEventLoop::quit );
						settle.exec();
					}
					skope->ogl->grabFramebuffer().save( qEnvironmentVariable( "WW_SHOT" ) );
				}
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_BOLTBAKE_TEST=1, WW_BOLTBAKE_TIME=<seconds>): does
	 * snapshotting the procedural-lightning arc produce real geometry, in a
	 * sensible place, and the SAME geometry twice for one instant?
	 *
	 * The arcs are the visual signature of the Tesla armour, and 0 of the 173
	 * vanilla loading screens contain a BSProceduralLightningController — a bake
	 * is the only way to keep them. What makes it possible is that regenerate() is
	 * seeded from quantised time; but "the generator is deterministic" and "the
	 * bake reproduces it" are different claims, and this checks the second.
	 * Log: release/ww_boltbake_test.log
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_BOLTBAKE_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1500, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_boltbake_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				do {
					if ( !ok ) { log << "load failed\n"; break; }

					// Not inherited from the registry: with GLView/Enable Animations
					// left false by a GUI session, no controller updates and the
					// capture legitimately finds nothing. See WW_EFFECTBAKE_TEST.
					skope->ogl->setAnimationEnabled( true );
					QApplication::processEvents();

					// Stepped, not jumped: particle systems integrate frame to frame,
					// so the scene has to be walked to the instant.
					const float want = qEnvironmentVariableIsSet( "WW_BOLTBAKE_TIME" )
						? qEnvironmentVariable( "WW_BOLTBAKE_TIME" ).toFloat() : 2.5f;
					for ( float t = 0.0f; t < want; t += 1.0f / 30.0f ) {
						skope->ogl->setSceneTime( std::min( t + 1.0f / 30.0f, want ) );
						QApplication::processEvents();
					}
					QEventLoop settle;
					QTimer::singleShot( 300, &settle, &QEventLoop::quit );
					settle.exec();

					// -Y: a loading screen looks at the figure's front, so that is the
					// direction the ribbons should present their width to.
					const Vector3 facing( 0.0f, -1.0f, 0.0f );
					const auto arcs = skope->ogl->bakeLightningArcs( facing );
					log << "baked " << arcs.size() << " arc(s) at t=" << want << "\n";
					check( "at least one arc was captured", !arcs.isEmpty() );

					int totalTris = 0;
					Vector3 bbLo, bbHi;
					bool haveBB = false;
					for ( const auto & a : arcs ) {
						totalTris += a.tris.size() / 3;
						check( QStringLiteral( "arc '%1' has whole triangles" ).arg( a.name ),
							a.tris.size() % 3 == 0 && !a.tris.isEmpty() );
						check( QStringLiteral( "arc '%1' has one UV per vertex" ).arg( a.name ),
							a.uvs.size() == a.tris.size() );
						check( QStringLiteral( "arc '%1' names a shader property" ).arg( a.name ),
							a.shaderProperty.isValid() );
						for ( const Vector3 & p : a.tris ) {
							if ( !haveBB ) { bbLo = bbHi = p; haveBB = true; continue; }
							for ( int i = 0; i < 3; i++ ) {
								bbLo[i] = std::min( bbLo[i], p[i] );
								bbHi[i] = std::max( bbHi[i], p[i] );
							}
						}
					}
					log << "total triangles " << totalTris << "\n";
					if ( haveBB )
						log << "world bounds (" << bbLo[0] << ", " << bbLo[1] << ", " << bbLo[2]
							<< ") .. (" << bbHi[0] << ", " << bbHi[1] << ", " << bbHi[2] << ")\n";
					// Finite and not collapsed: a degenerate ribbon is the failure mode
					// that still "produces geometry".
					const float span = haveBB ? ( bbHi - bbLo ).length() : 0.0f;
					check( "the baked geometry has real extent",
						haveBB && span > 1.0f && std::isfinite( span ) );

					// Same instant, second call: must match exactly.
					const auto again = skope->ogl->bakeLightningArcs( facing );
					bool same = ( again.size() == arcs.size() );
					for ( int i = 0; same && i < arcs.size(); i++ )
						same = ( again.at( i ).tris == arcs.at( i ).tris );
					check( "baking one instant twice gives identical geometry", same );
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_EFFECTBAKE_TEST=<out.nif>, WW_EFFECTBAKE_TIME=<seconds>):
	 * does a captured effect survive being written into a NIF and read back?
	 *
	 * Capture was proved separately (WW_BOLTBAKE_TEST). This is the other half,
	 * and it is where the interesting failures live, because writing FO4 geometry
	 * has three ways to look successful and be wrong:
	 *
	 *   1. Vertex Desc says half precision while the writer sets Vector3. nif.xml
	 *      names both variants "Vertex", so set<Vector3> does nothing and returns
	 *      false. The loading-screen converter shipped with exactly this and wrote
	 *      a file with no vertices in it.
	 *   2. Data Size and the desc disagree. The model holds the arrays in memory
	 *      either way, so everything looks right until the file is re-read.
	 *   3. The arrays never get sized, because a conditional array has no children
	 *      until the deferred cascade runs.
	 *
	 * All three are invisible in the live model and all three are caught by the
	 * same invariant: SAVE, re-read from disk into a fresh model, and check that
	 * translation + vertex reproduces the world position that was captured. That
	 * is the only claim that matters — "the effect is where it looked".
	 * Log: release/ww_effectbake_test.log
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_EFFECTBAKE_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1500, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_effectbake_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				do {
					if ( !ok ) { log << "load failed\n"; break; }

					/* Animation is a PERSISTED user setting, and with it off no
					 * controller ever updates: there are no bolts and no live
					 * sprites, so a capture correctly returns nothing. This suite
					 * went from green to "captured 0" with no code change at all,
					 * because a GUI session had left GLView/Enable Animations at
					 * false in the registry. A harness that inherits the user's
					 * settings is not measuring the code. */
					skope->ogl->setAnimationEnabled( true );
					QApplication::processEvents();

					const float want = qEnvironmentVariableIsSet( "WW_EFFECTBAKE_TIME" )
						? qEnvironmentVariable( "WW_EFFECTBAKE_TIME" ).toFloat() : 2.5f;
					// Stepped, not jumped: sprite positions integrate frame to frame.
					for ( float t = 0.0f; t < want; t += 1.0f / 30.0f ) {
						skope->ogl->setSceneTime( std::min( t + 1.0f / 30.0f, want ) );
						QApplication::processEvents();
					}
					QEventLoop settle;
					QTimer::singleShot( 300, &settle, &QEventLoop::quit );
					settle.exec();

					const Vector3 facing( 0.0f, -1.0f, 0.0f );
					const auto caught = skope->ogl->bakeEffects( skope->getNifModel(), facing );
					log << "captured " << caught.size() << " effect(s) at t=" << want << "\n";
					check( "at least one effect was captured", !caught.isEmpty() );
					if ( caught.isEmpty() )
						break;

					// What the capture claims, kept for the round-trip comparison.
					QVector<BakeGeom::Capture> caps;
					for ( const auto & e : caught ) {
						BakeGeom::Capture c;
						c.name = e.name;
						c.tris = e.tris;
						c.uvs = e.uvs;
						c.cols = e.cols;
						c.tint = e.tint;
						c.facing = facing;
						c.shaderProperty = e.shaderProperty;
						c.alphaProperty = e.alphaProperty;
						c.fromParticles = e.fromParticles;
						caps.append( c );
						log << "  '" << e.name << "' " << ( e.fromParticles ? "sprites" : "arc" )
							<< ": " << ( e.tris.size() / 3 ) << " tri, "
							<< ( e.shaderProperty.isValid() ? "has shader" : "NO SHADER" ) << "\n";
					}

					NifModel * nif = skope->getNifModel();
					QString err;
					const BakeGeom::Result r = BakeGeom::write( nif, caps, true, &err );
					log << "wrote " << r.shapes << " shape(s), " << r.vertices << " vert, "
						<< r.triangles << " tri, removed " << r.emittersRemoved << " emitter block(s)\n";
					for ( const QString & n : r.notes )
						log << "  note: " << n << "\n";
					check( "every capture became a shape", r.ok && r.shapes == caps.size() );
					check( "the emitters it replaced were removed", r.emittersRemoved > 0 );

					// Nothing live must be left behind, or a loading screen would
					// carry both the bake and the emitter it replaces.
					int leftover = 0;
					for ( int b = 0; b < nif->getBlockCount(); b++ ) {
						const QString t = nif->itemName( nif->getBlockIndex( b ) );
						if ( t == QLatin1String( "NiParticleSystem" )
							|| t == QLatin1String( "BSProceduralLightningController" ) )
							leftover++;
					}
					check( "no emitter blocks remain", leftover == 0 );

					// --- the round trip ---------------------------------------
					const QString out = qEnvironmentVariable( "WW_EFFECTBAKE_TEST" );
					check( "the model saved", nif->saveToFile( out ) );

					NifModel back;
					check( "the saved file re-reads", back.loadFromFile( out ) );

					/* Matched by INDEX, through the names write() reports back.
					 *
					 * Matching on the capture's own name silently compared the wrong
					 * pair on an assembled rig, which has four nodes called
					 * BoltGeo_01 and six called LightningArcs_VFX: it reported a
					 * 115-unit round-trip error that was really a helmet arc being
					 * measured against a leg arc. write() uniquifies and tells the
					 * caller what each capture ended up called.
					 */
					QHash<QString, QModelIndex> byName;
					for ( int b = 0; b < back.getBlockCount(); b++ ) {
						const QModelIndex idx = back.getBlockIndex( b );
						if ( back.blockInherits( idx, "BSTriShape" ) )
							byName.insert( back.get<QString>( idx, "Name" ), idx );
					}
					QVector<QModelIndex> baked;
					QVector<const BakeGeom::Capture *> bakedSrc;
					for ( int i = 0; i < caps.size() && i < r.shapeNames.size(); i++ ) {
						const QString nm = r.shapeNames.at( i );
						if ( nm.isEmpty() || !byName.contains( nm ) )
							continue;
						baked.append( byName.value( nm ) );
						bakedSrc.append( &caps.at( i ) );
					}
					check( QStringLiteral( "all %1 baked shape(s) are in the re-read file" )
						.arg( caps.size() ), baked.size() == caps.size() );

					/* Distinct names, because a rig's effect files share a template.
					 * An assembled X-01 has four BoltGeo_01 and six
					 * LightningArcs_VFX; writing four shapes under one name made
					 * this very suite compare a helmet arc against a leg arc and
					 * report a 115-unit error that did not exist. Uniqueness is
					 * what makes matching by name safe at all.
					 */
					QSet<QString> distinct;
					for ( const QString & nm : r.shapeNames )
						if ( !nm.isEmpty() )
							distinct.insert( nm );
					check( QStringLiteral( "the %1 baked shape names are all distinct" )
						.arg( r.shapes ), distinct.size() == r.shapes );

					float worstPos = 0.0f;
					int sized = 0, shaded = 0;
					for ( int k = 0; k < baked.size(); k++ ) {
						const QModelIndex idx = baked.at( k );
						const QString nm = back.get<QString>( idx, "Name" );
						const BakeGeom::Capture * src = bakedSrc.at( k );

						const quint32 nv = back.get<quint32>( idx, "Num Vertices" );
						const quint32 nt = back.get<quint32>( idx, "Num Triangles" );
						const quint32 ds = back.get<quint32>( idx, "Data Size" );
						const bool hasCol = ( src->cols.size() == src->tris.size() );
						const quint32 want = nv * ( hasCol ? 32u : 28u ) + nt * 6u;
						if ( nv == quint32( src->tris.size() ) && nt == nv / 3 && ds == want )
							sized++;
						else
							log << "  '" << nm << "' sizes: nv " << nv << " nt " << nt
								<< " dataSize " << ds << " (expected " << want << ")\n";

						if ( back.getLink( idx, "Shader Property" ) >= 0 )
							shaded++;

						// translation + vertex must reproduce the captured world point
						const Vector3 tr = back.get<Vector3>( idx, "Translation" );
						QModelIndex iVerts = back.getIndex( idx, "Vertex Data" );
						const int n = std::min<int>( int( nv ), int( src->tris.size() ) );
						for ( int i = 0; i < n; i++ ) {
							QModelIndex iV = back.getIndex( iVerts, i );
							if ( !iV.isValid() )
								continue;
							const Vector3 v = back.get<Vector3>( iV, "Vertex" );
							worstPos = std::max( worstPos, ( ( tr + v ) - src->tris.at( i ) ).length() );
						}
					}
					log << "worst round-trip position error " << worstPos << " units\n";
					check( "every shape's desc and Data Size agree", sized == baked.size() );
					check( "every shape kept a shader property", shaded == baked.size() );
					// Full precision, so this is exact bar float noise. A half-float
					// write would land near 0.008; a failed write near the bolt length.
					check( "the geometry round-trips to where it was captured",
						baked.size() == caps.size() && worstPos < 0.01f );
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				// This harness MODIFIES the model, and quitting with a modified
				// document raises a save prompt that blocks the process — a
				// -Wait run then wedges until something kills it.
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_NORMALS_TEST=1): does Ctrl+N actually turn the faces the
	 * right way round?
	 *
	 * The question has an exact answer, so the test does not look at anything.
	 * A closed mesh whose faces all wind counter-clockwise seen from outside has
	 * POSITIVE signed volume; wind them the other way and the same number comes
	 * out negative. So: vandalise a known fraction of the triangles, run the
	 * operator, and require the volume to come back positive for Outside and
	 * negative for Inside — with the mesh otherwise untouched, which the vertex
	 * count and the triangle multiset check for separately.
	 *
	 * "It renders correctly now" would not do: a mesh with every face flipped
	 * renders perfectly from the inside, and a partially flipped one looks wrong
	 * only from the angles you did not check.
	 * Log: release/ww_normals_test.log
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_NORMALS_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1200, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_normals_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				NifModel * nif = skope->getNifModel();
				/* Every operator here reports what it did — or why it declined —
				 * through gizmoStatus, which a harness cannot see. Without this
				 * the first run of this suite said "nothing changed" four times
				 * and gave no hint that the operator had refused the selection. */
				QStringList said;
				QObject::connect( skope->ogl, &GLView::gizmoStatus, skope,
					[&said]( const QString & s ) { said << s; } );
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }
					int sb = -1;
					for ( int b = 0; b < nif->getBlockCount() && sb < 0; b++ )
						if ( nif->blockInherits( nif->getBlockIndex( b ), "BSTriShape" )
							 && nif->get<int>( nif->getBlockIndex( b ), "Num Triangles" ) > 50 )
							sb = b;
					if ( sb < 0 ) { log << "no BSTriShape\n"; break; }
					QModelIndex iShape = nif->getBlockIndex( sb );
					QModelIndex iTris = nif->getIndex( iShape, "Triangles" );
					QModelIndex iVD = nif->getIndex( iShape, "Vertex Data" );
					const int nT = nif->get<int>( iShape, "Num Triangles" );
					const int nV = nif->get<int>( iShape, "Num Vertices" );
					log << "block " << sb << " '" << nif->get<QString>( iShape, "Name" )
						<< "' " << nV << " verts, " << nT << " tris\n";

					QVector<Vector3> pos( nV );
					for ( int v = 0; v < nV; v++ )
						pos[v] = nif->get<Vector3>( nif->getIndex( iVD, v ), "Vertex" );
					Vector3 centre;
					for ( const Vector3 & p : std::as_const( pos ) )
						centre += p;
					if ( nV )
						centre /= float( nV );

					auto volume = [&]() {
						double vol = 0.0;
						for ( int t = 0; t < nT; t++ ) {
							const Triangle x = nif->get<Triangle>( nif->getIndex( iTris, t ) );
							const Vector3 a = pos.at( x[0] ) - centre;
							const Vector3 b = pos.at( x[1] ) - centre;
							const Vector3 c = pos.at( x[2] ) - centre;
							vol += double( Vector3::dotproduct( a, Vector3::crossproduct( b, c ) ) ) / 6.0;
						}
						return vol;
					};
					// the triangle SET, order and winding ignored — what must not
					// change no matter how often the operator runs
					auto triSignature = [&]() {
						QStringList sig;
						for ( int t = 0; t < nT; t++ ) {
							const Triangle x = nif->get<Triangle>( nif->getIndex( iTris, t ) );
							QList<int> c{ x[0], x[1], x[2] };
							std::sort( c.begin(), c.end() );
							sig << QStringLiteral( "%1/%2/%3" ).arg( c[0] ).arg( c[1] ).arg( c[2] );
						}
						sig.sort();
						return sig.join( QLatin1Char( ',' ) );
					};

					const double vol0 = volume();
					const QString sig0 = triSignature();
					log << "as loaded: volume " << vol0 << "\n";

					// --- vandalise: turn over every third triangle -------------
					nif->undoStack->beginMacro( QStringLiteral( "vandalise" ) );
					int flipped = 0;
					for ( int t = 0; t < nT; t += 3 ) {
						QModelIndex ti = nif->getIndex( iTris, t );
						Triangle x = nif->get<Triangle>( ti );
						std::swap( x[1], x[2] );
						nif->set<Triangle>( ti, x );
						flipped++;
					}
					nif->undoStack->endMacro();
					const double volBad = volume();
					log << "after flipping " << flipped << " of " << nT << ": volume " << volBad << "\n";
					check( "the vandalised mesh really is inconsistent",
						std::fabs( volBad ) < std::fabs( vol0 ) * 0.9 );

					// --- Recalculate Outside ----------------------------------
					// The scene has to exist and the shape has to be the current
					// block before edit mode will take: without this the operator
					// declines every call with "needs a selection in edit mode",
					// which is a refusal, not a result.
					skope->ogl->grabFramebuffer();
					skope->ogl->getScene()->currentBlock = iShape;
					skope->ogl->syncObjectSelection( sb );
					skope->ogl->setEditMode( true );
					skope->ogl->setPickMode( 4 );		// faces
					skope->ogl->selectAll( 1 );
					log << "edit mode: " << skope->ogl->pickedElems.size() << " element(s) picked\n";

					/* Control: the SHIPPED Flip, through the same write path. If
					 * this does not move the volume either, the fault is in the
					 * shared idiom or in this harness, not in the new operator —
					 * a distinction worth one call to find out. */
					/* The trap this suite exists to catch, asserted directly.
					 *
					 * getIndex(parent, row) hands back a COLUMN 0 index, and
					 * NifModel::setData switches on the column: on the name column it
					 * renames the item, RETURNS TRUE, and leaves the value alone. So a
					 * ChangeValueCommand built that way pushes, undoes and redoes
					 * perfectly while writing nothing — which is how Flip Normals and
					 * every post-edit normal recompute came to be silently dead.
					 * If this ever starts writing, the ValueCol dance below is
					 * unnecessary and someone should be told rather than left guessing.
					 */
					{
						QModelIndex probe = nif->getIndex( iTris, 0 );
						log << "index column for a triangle row: " << probe.column()
							<< " (ValueCol is " << int( NifModel::ValueCol ) << ")\n";
						Triangle before = nif->get<Triangle>( probe );
						Triangle test = before;
						std::swap( test[1], test[2] );
						NifValue nv = nif->getValue( probe );
						nv.set<Triangle>( test, nif, nif->getItem( probe ) );
						const bool okSet = nif->setData( probe, nv.toVariant(), Qt::EditRole );
						Triangle after = nif->get<Triangle>( probe );
						const bool wrote = ( before[1] != after[1] );
						log << "direct setData on the name column returned " << int( okSet )
							<< ", value " << ( wrote ? "changed" : "UNCHANGED" ) << "\n";
						check( "setData on the name column reports success and writes nothing",
							okSet && !wrote );
						if ( wrote )
							nif->set<Triangle>( probe, before );
					}
					const double volBeforeFlip = volume();
					const int uc0 = nif->undoStack ? nif->undoStack->count() : -1;
					const int ui0 = nif->undoStack ? nif->undoStack->index() : -1;
					skope->ogl->flipSelectedFaces();
					QApplication::processEvents();
					log << "undo stack: count " << uc0 << " -> "
						<< ( nif->undoStack ? nif->undoStack->count() : -1 )
						<< ", index " << ui0 << " -> "
						<< ( nif->undoStack ? nif->undoStack->index() : -1 )
						<< ", enabled " << ( nif->undoStack ? int( nif->undoStack->isActive() ) : -1 ) << "\n";
					const double volAfterFlip = volume();
					log << "control: Flip took volume " << volBeforeFlip << " -> " << volAfterFlip << "\n";
					check( "the existing Flip writes through this path",
						std::fabs( volAfterFlip + volBeforeFlip ) < std::fabs( volBeforeFlip ) * 0.02 );
					skope->ogl->flipSelectedFaces();		// back
					QApplication::processEvents();
					const Triangle t0Before = nif->get<Triangle>( nif->getIndex( iTris, 0 ) );
					skope->ogl->recalcNormalsSelection( false, false );
					QApplication::processEvents();
					const Triangle t0After = nif->get<Triangle>( nif->getIndex( iTris, 0 ) );
					log << "triangle 0: before (" << t0Before[0] << "," << t0Before[1] << "," << t0Before[2]
						<< ") after (" << t0After[0] << "," << t0After[1] << "," << t0After[2] << ")\n";
					const double volOut = volume();
					log << "after Recalculate Outside: volume " << volOut << "\n";
					check( "Outside gives a positive volume", volOut > 0.0 );
					check( "Outside restores the original volume",
						std::fabs( volOut - std::fabs( vol0 ) ) < std::fabs( vol0 ) * 0.02 );
					check( "no triangle gained, lost or changed corners", triSignature() == sig0 );

					// running it again must change nothing at all
					skope->ogl->recalcNormalsSelection( false, false );
					QApplication::processEvents();
					check( "running Outside twice is a no-op",
						std::fabs( volume() - volOut ) < std::fabs( vol0 ) * 1.0e-4 );

					// --- Recalculate Inside -----------------------------------
					skope->ogl->recalcNormalsSelection( true, false );
					QApplication::processEvents();
					const double volIn = volume();
					log << "after Recalculate Inside: volume " << volIn << "\n";
					check( "Inside gives a negative volume", volIn < 0.0 );
					check( "Inside is Outside turned over",
						std::fabs( volIn + volOut ) < std::fabs( vol0 ) * 0.02 );
					check( "still no triangle gained or lost", triSignature() == sig0 );

					// --- and the normals followed the winding ------------------
					skope->ogl->recalcNormalsSelection( false, false );
					QApplication::processEvents();
					int agree = 0, tested = 0;
					for ( int t = 0; t < nT; t += 7 ) {
						const Triangle x = nif->get<Triangle>( nif->getIndex( iTris, t ) );
						const Vector3 fn = Vector3::crossproduct( pos.at( x[1] ) - pos.at( x[0] ),
																  pos.at( x[2] ) - pos.at( x[0] ) );
						if ( fn.squaredLength() < 1.0e-12f )
							continue;
						for ( int c = 0; c < 3; c++ ) {
							const Vector3 vn = nif->get<Vector3>( nif->getIndex( iVD, x[c] ), "Normal" );
							tested++;
							if ( Vector3::dotproduct( fn, vn ) > 0.0f )
								agree++;
						}
					}
					log << "vertex normals agreeing with their face: " << agree << " of " << tested << "\n";
					check( "the stored normals point the same way as the winding",
						tested > 0 && agree > tested * 9 / 10 );
				} while ( false );
				log << "status line said:\n";
				for ( const QString & s : std::as_const( said ) )
					log << "  \"" << s << "\"\n";
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				if ( nif && nif->undoStack )
					nif->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_SHOT_UI=<out.png>): photograph the CHROME, not the scene.
	 *
	 * grabFramebuffer returns the GL viewport alone, so it cannot show a toolbar,
	 * a menu or a panel — the things a UI change is actually about. QWidget::grab
	 * renders any widget's own tree, whether or not it is on screen or focused,
	 * which is what makes this usable while someone else is working on the machine.
	 *
	 * Writes three files: <out> for the popup of the named button (WW_SHOT_UI_BTN,
	 * default ViewAnimationButton), <out>.toolbar.png for the strip it lives on,
	 * and nothing at all if the button is not found — which is itself the failure
	 * this is here to catch.
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_SHOT_UI" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool, QString & ) {
			QTimer::singleShot( 1200, skope, [skope]() {
				const QString out = qEnvironmentVariable( "WW_SHOT_UI" );
				const QString btnName = qEnvironmentVariableIsSet( "WW_SHOT_UI_BTN" )
					? qEnvironmentVariable( "WW_SHOT_UI_BTN" )
					: QStringLiteral( "ViewAnimationButton" );

				if ( QToolBar * tb = skope->findChild<QToolBar *>( QStringLiteral( "tView" ) ) )
					tb->grab().save( out + QStringLiteral( ".toolbar.png" ) );

				if ( auto * btn = skope->findChild<QToolButton *>( btnName ) ) {
					if ( QMenu * m = btn->menu() ) {
						m->popup( btn->mapToGlobal( QPoint( 0, btn->height() ) ) );
						QApplication::processEvents();
						QEventLoop settle;
						QTimer::singleShot( 300, &settle, &QEventLoop::quit );
						settle.exec();
						m->grab().save( out );

						/* ...and what the picture cannot be read for. Enabled-ness is
						 * a few percent of grey either way, which is exactly the kind
						 * of thing a screenshot invites you to guess at. */
						QFile sf( out + QStringLiteral( ".state.txt" ) );
						if ( sf.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
							QTextStream ss( &sf );
							ss << btnName << " enabled=" << int( btn->isEnabled() ) << "\n";
							for ( QWidget * w : m->findChildren<QWidget *>() ) {
								if ( w->objectName().isEmpty() && !qobject_cast<QComboBox *>( w )
									 && !qobject_cast<QCheckBox *>( w ) && !qobject_cast<QAbstractButton *>( w )
									 && !qobject_cast<QSlider *>( w ) )
									continue;
								QString what = w->metaObject()->className();
								if ( auto * c = qobject_cast<QComboBox *>( w ) )
									what += QStringLiteral( " '%1' (%2 items)" )
										.arg( c->currentText() ).arg( c->count() );
								else if ( auto * cb = qobject_cast<QCheckBox *>( w ) )
									what += QStringLiteral( " '%1' checked=%2" )
										.arg( cb->text() ).arg( int( cb->isChecked() ) );
								else if ( auto * ab = qobject_cast<QAbstractButton *>( w ) )
									what += QStringLiteral( " '%1'" ).arg( ab->text().isEmpty()
										? ab->toolTip() : ab->text() );
								ss << "  " << what << " enabled=" << int( w->isEnabled() ) << "\n";
							}
						}
						m->close();
					}
				}
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_SHOT_TEST=<out.png>, WW_SHOT_VIEW=front|back|left|right|top,
	 * WW_SHOT_TIME=<seconds>): render the file from a named side and save it.
	 *
	 * The numbers say where an effect IS; they cannot say whether it looks right,
	 * and the arcs on this armour are on its BACK, which is the one side a default
	 * view never shows. So: pick a side, step the animation to an instant, paint,
	 * save. Sized from WW_SHOT_SIZE=<w>x<h> so the image is worth looking at.
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_SHOT_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1200, skope, [skope, ok]() {
				const QString out = qEnvironmentVariable( "WW_SHOT_TEST" );
				if ( ok ) {
					skope->ogl->setAnimationEnabled( true );
					QApplication::processEvents();

					// The camera FIRST. setOrientation recenters and repaints, and a
					// repaint steps the scene — so moving it after the stepping loop
					// walks the animation on past the instant that was asked for, and
					// the first version of this harness saved a frame with no arcs in
					// it at all.
					const QString v = qEnvironmentVariable( "WW_SHOT_VIEW" ).toLower();
					GLView::ViewState vs = GLView::ViewFront;
					if ( v == QLatin1String( "back" ) )       vs = GLView::ViewBack;
					else if ( v == QLatin1String( "left" ) )  vs = GLView::ViewLeft;
					else if ( v == QLatin1String( "right" ) ) vs = GLView::ViewRight;
					else if ( v == QLatin1String( "top" ) )   vs = GLView::ViewTop;
					skope->ogl->setOrientation( vs, true );
					QApplication::processEvents();

					/* Framing, because scale decides what is legible. A bolt is 4
					 * units wide; on a 160-unit figure that is a hairline, and the
					 * same effect photographed in its own file — where the camera
					 * frames 40 units — looks ten times thicker. Two shots of the
					 * same geometry can therefore disagree about whether anything is
					 * there. WW_SHOT_AT=x,y,z centres, WW_SHOT_DIST=<units> zooms.
					 */
					if ( qEnvironmentVariableIsSet( "WW_SHOT_AT" ) ) {
						const QStringList xyz = qEnvironmentVariable( "WW_SHOT_AT" ).split( QLatin1Char( ',' ) );
						if ( xyz.size() == 3 )
							skope->ogl->setPosition( Vector3( -xyz.at( 0 ).toFloat(),
							                                 -xyz.at( 1 ).toFloat(),
							                                 -xyz.at( 2 ).toFloat() ) );
					}
					if ( qEnvironmentVariableIsSet( "WW_SHOT_DIST" ) )
						skope->ogl->setDistance( qEnvironmentVariable( "WW_SHOT_DIST" ).toFloat() );
					QApplication::processEvents();

					const float want = qEnvironmentVariableIsSet( "WW_SHOT_TIME" )
						? qEnvironmentVariable( "WW_SHOT_TIME" ).toFloat() : 2.5f;
					// Stepped, not jumped: sprite positions integrate frame to frame.
					for ( float t = 0.0f; t < want; t += 1.0f / 30.0f ) {
						skope->ogl->setSceneTime( std::min( t + 1.0f / 30.0f, want ) );
						QApplication::processEvents();
					}

					// grabFramebuffer does NOT repaint: without pumping first it
					// saves whatever was on screen before the last step.
					for ( int i = 0; i < 3; i++ ) {
						skope->ogl->update();
						QApplication::processEvents();
					}
					QEventLoop settle;
					QTimer::singleShot( 250, &settle, &QEventLoop::quit );
					settle.exec();
					// ...and the settle above let the clock run, so the instant is
					// re-asserted before the grab.
					skope->ogl->setSceneTime( want );
					skope->ogl->update();
					QApplication::processEvents();
					skope->ogl->update();
					QApplication::processEvents();

					skope->ogl->grabFramebuffer().save( out );
				}
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_LIVEFX_TEST=1, WW_LIVEFX_TIME=<seconds>): does a loading
	 * screen converted with --keep-effects still RUN, and in the right place?
	 *
	 * Every other check on that conversion reads the file: block counts, palette
	 * entries, world transforms. All of them can be perfect on a file that draws
	 * nothing, because a particle system's geometry and a procedural arc's are not
	 * in the file at all — they exist only once something steps the controllers.
	 * So this one loads the converted screen, steps time, and asks the RENDERER
	 * what it produced.
	 *
	 * Placement is checked in the same breath, because the failure it guards is
	 * silent: the convert deletes the skeleton, and an effect branch whose attach
	 * bone was not carried over collapses to the origin — geometry still generates,
	 * still has extent, and sits in a heap at the actor's feet. Chest-height and
	 * leg-height captures in the same file is what says the per-limb stubs worked.
	 * Log: release/ww_livefx_test.log
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_LIVEFX_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1500, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_livefx_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				do {
					if ( !ok ) { log << "load failed\n"; break; }
					NifModel * nif = skope->getNifModel();
					if ( !nif ) { log << "no model\n"; break; }

					// --- what the file carries -------------------------------
					int psys = 0, lightning = 0, managers = 0, sequences = 0, bones = 0;
					for ( int b = 0; b < nif->getBlockCount(); b++ ) {
						const QString t = nif->itemName( nif->getBlockIndex( b ) );
						if ( t == QLatin1String( "NiParticleSystem" ) ) psys++;
						else if ( t == QLatin1String( "BSProceduralLightningController" ) ) lightning++;
						else if ( t == QLatin1String( "NiControllerManager" ) ) managers++;
						else if ( t == QLatin1String( "NiControllerSequence" ) ) sequences++;
						if ( !nif->getLinkArray( nif->getBlockIndex( b ), "Bones" ).isEmpty() )
							bones++;
					}
					log << psys << " particle system(s), " << lightning << " lightning controller(s), "
						<< managers << " manager(s), " << sequences << " sequence(s)\n";
					check( "the converted screen kept its particle systems", psys > 0 );
					check( "...and its procedural lightning", lightning > 0 );
					// One graph, on the root. Six merged ArtObjects bring six.
					check( "exactly one controller manager", managers == 1 );
					check( "its sequences came with it", sequences > 0 );
					// The skeleton is what a loading screen does NOT have; keeping
					// effects must not smuggle it back in.
					check( "no skin survived the convert", bones == 0 );

					/* Animation is a persisted user setting and the capture reads the
					 * rendered scene: with it off there are no bolts and no live
					 * sprites, and every check below would fail for a reason that has
					 * nothing to do with the code. */
					skope->ogl->setAnimationEnabled( true );
					QApplication::processEvents();

					const float want = qEnvironmentVariableIsSet( "WW_LIVEFX_TIME" )
						? qEnvironmentVariable( "WW_LIVEFX_TIME" ).toFloat() : 2.5f;
					// Stepped, not jumped: sprite positions integrate frame to frame.
					for ( float t = 0.0f; t < want; t += 1.0f / 30.0f ) {
						skope->ogl->setSceneTime( std::min( t + 1.0f / 30.0f, want ) );
						QApplication::processEvents();
					}
					QEventLoop settle;
					QTimer::singleShot( 300, &settle, &QEventLoop::quit );
					settle.exec();

					// --- what it actually generated --------------------------
					const Vector3 facing( 0.0f, -1.0f, 0.0f );
					const auto caught = skope->ogl->bakeEffects( nif, facing );
					int arcs = 0, sprites = 0;
					float loZ = 0.0f, hiZ = 0.0f, maxX = 0.0f;
					bool haveZ = false;
					/* Per effect, not just in aggregate. Overall bounds are a weak
					 * witness: a leg effect emitting from the wrong node still lands
					 * inside the figure's bounding box, and the first version of this
					 * harness passed on a file whose leg particles were visibly wrong.
					 * WW_LIVEFX_DUMP writes these to a file so the SAME scene can be
					 * captured before and after the convert and compared per effect. */
					QStringList fx;
					for ( const auto & e : caught ) {
						( e.fromParticles ? sprites : arcs )++;
						Vector3 centre, lo, hi;
						bool have = false;
						for ( const Vector3 & p : e.tris ) {
							centre += p;
							if ( !have ) { lo = hi = p; have = true; continue; }
							for ( int i = 0; i < 3; i++ ) {
								lo[i] = std::min( lo[i], p[i] );
								hi[i] = std::max( hi[i], p[i] );
							}
						}
						if ( !e.tris.isEmpty() )
							centre /= float( e.tris.size() );
						auto f = []( float v ) { return QString::number( v, 'f', 3 ); };
						fx << QStringLiteral( "%1\t%2\t%3\t%4 %5 %6\t%7 %8 %9\t%10 %11 %12" )
							.arg( e.name, e.fromParticles ? QStringLiteral( "sprites" ) : QStringLiteral( "arc" ) )
							.arg( e.tris.size() / 3 )
							.arg( f( centre[0] ), f( centre[1] ), f( centre[2] ) )
							.arg( f( lo[0] ), f( lo[1] ), f( lo[2] ) )
							.arg( f( hi[0] ), f( hi[1] ), f( hi[2] ) );
						for ( const Vector3 & p : e.tris ) {
							if ( !haveZ ) { loZ = hiZ = p[2]; haveZ = true; }
							loZ = std::min( loZ, p[2] );
							hiZ = std::max( hiZ, p[2] );
							maxX = std::max( maxX, std::fabs( p[0] ) );
						}
					}
					fx.sort();
					for ( const QString & line : std::as_const( fx ) )
						log << "  fx " << line << "\n";
					if ( qEnvironmentVariableIsSet( "WW_LIVEFX_DUMP" ) ) {
						QFile df( qEnvironmentVariable( "WW_LIVEFX_DUMP" ) );
						if ( df.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
							QTextStream ds( &df );
							for ( const QString & line : std::as_const( fx ) )
								ds << line << "\n";
							df.close();
						}
					}
					log << "generated " << caught.size() << " effect(s) at t=" << want
						<< ": " << arcs << " arc(s), " << sprites << " sprite cloud(s)\n";
					if ( haveZ )
						log << "world Z " << loZ << " .. " << hiZ << ", widest |X| " << maxX << "\n";

					check( "the arcs generate in the converted file", arcs > 0 );
					check( "the particles emit in the converted file", sprites > 0 );

					/* Placement. A figure is ~160 units tall; an effect branch that
					 * lost its attach bone lands at the origin, so the tell is not
					 * "is there geometry" but "is it spread up the body". The X-01
					 * has arcs at the calves and at the chest, so a file where every
					 * capture sits below 60 has lost the upper limbs' placement and
					 * one where none does has lost the legs'. */
					check( "nothing collapsed to the origin", haveZ && hiZ > 60.0f );
					check( "the leg effects are at leg height", haveZ && loZ < 60.0f );
					check( "the chest and helmet effects are up the body", haveZ && hiZ > 100.0f );
					check( "nothing was flung sideways", haveZ && maxX < 60.0f );

					/* "(no sequence)" has to still ANIMATE.
					 *
					 * It is not a stop button: it drops the sequence's bindings and
					 * plays what the file does on its own, which for these effects
					 * is where the particles and arcs live in the first place. A
					 * version of it that quietly froze everything would look like a
					 * working feature right up until someone used it.
					 */
					skope->ogl->clearSceneSequence();
					QApplication::processEvents();
					for ( float t = 0.0f; t < want; t += 1.0f / 30.0f ) {
						skope->ogl->setSceneTime( std::min( t + 1.0f / 30.0f, want ) );
						QApplication::processEvents();
					}
					const auto standalone = skope->ogl->bakeEffects( nif, facing );
					int saArcs = 0, saSprites = 0;
					for ( const auto & e : standalone )
						( e.fromParticles ? saSprites : saArcs )++;
					log << "with no sequence: " << saArcs << " arc(s), "
						<< saSprites << " sprite cloud(s)\n";
					check( "the scene reports no sequence selected",
						skope->ogl->getScene()->animGroup.isEmpty() );
					check( "arcs still generate with no sequence", saArcs > 0 );
					check( "particles still emit with no sequence", saSprites > 0 );
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_WSFIX_TEST=1): does the Skeleton Manager keep its hands to
	 * itself while its dock is hidden — and still do its job when shown?
	 *
	 * Its selection handler redirects: select a collision object and it follows
	 * the Target link to the bone that owns it, so the two docks track each
	 * other. With no visibility guard that ran in EVERY workspace, so clicking a
	 * body in the Collision Manager was yanked away to a bone, and a full
	 * skeletonAnalyse() walked the file on every block-list click app-wide.
	 *
	 * Both directions have to hold, which is why this does not simply assert
	 * "nothing happened". A guard that is really a removal would pass the first
	 * half and silently break the feature; the second half is what tells them
	 * apart.
	 * Log: release/ww_wsfix_test.log
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_WSFIX_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1200, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_wsfix_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				do {
					if ( !ok ) { log << "load failed\n"; break; }
					NifModel * nif = skope->getNifModel();
					if ( !nif ) { log << "no model\n"; break; }

					QDockWidget * skel = skope->findChild<QDockWidget *>(
						QStringLiteral( "SkeletonManagerDock" ) );
					if ( !skel ) { log << "no Skeleton Manager dock\n"; break; }

					// A collision object that actually targets a node — without a
					// Target there is no redirect to observe either way.
					int body = -1, target = -1;
					for ( int b = 0; b < nif->getBlockCount() && body < 0; b++ ) {
						QModelIndex idx = nif->getBlockIndex( b );
						if ( !nif->blockInherits( idx, "bhkNiCollisionObject" )
							&& !nif->blockInherits( idx, "bhkNPCollisionObject" ) )
							continue;
						const int t = nif->getLink( idx, "Target" );
						if ( nif->isValidBlockNumber( t ) ) { body = b; target = t; }
					}
					log << "collision block " << body << " targets node " << target << "\n";
					if ( body < 0 ) { log << "no collision object with a Target\n"; break; }

					auto blockOfCurrent = [nif, skope]() {
						return nif->getBlockNumber( nif->getBlockIndex( skope->currentNifIndex() ) );
					};

					// --- hidden: hands off ---------------------------------
					skel->hide();
					QApplication::processEvents();
					skope->select( nif->getBlockIndex( body ) );
					QApplication::processEvents();
					const int whenHidden = blockOfCurrent();
					log << "dock hidden, selected " << body << " -> now on " << whenHidden << "\n";
					check( "a hidden Skeleton dock does not steal the selection",
						whenHidden == body );

					// --- shown: still follows ------------------------------
					skel->show();
					QApplication::processEvents();
					QEventLoop settle;
					QTimer::singleShot( 250, &settle, &QEventLoop::quit );
					settle.exec();
					skope->select( nif->getBlockIndex( body ) );
					QApplication::processEvents();
					const int whenShown = blockOfCurrent();
					log << "dock shown, selected " << body << " -> now on " << whenShown << "\n";
					check( "a visible Skeleton dock still follows to the target bone",
						whenShown == target );
					skel->hide();
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_UNFUCKPANEL_TEST=1): does the Unfuck workspace actually
	 * find, colour and locate the problems in a file?
	 *
	 * Four things can each be quietly wrong and none is visible from a
	 * screenshot: the scan may find nothing, the severity may be flattened (it
	 * WAS -- BaseModel::testMsg discarded the level until this change, so
	 * everything would arrive as a warning), the block number may not be parsed
	 * out of the message, and Go to may not move the selection. All four are
	 * checked here against a file with a deliberately broken texture path.
	 * Log: release/ww_unfuckpanel_test.log
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_UNFUCKPANEL_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1200, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_unfuckpanel_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				do {
					if ( !ok ) { log << "load failed\n"; break; }
					QDockWidget * dock = skope->findChild<QDockWidget *>(
						QStringLiteral( "UnfuckManagerDock" ) );
					if ( !dock ) { log << "no Unfuck dock\n"; break; }
					dock->show();
					QApplication::processEvents();
					QEventLoop settle;
					QTimer::singleShot( 400, &settle, &QEventLoop::quit );
					settle.exec();

					auto * tree = dock->findChild<QTreeWidget *>( QStringLiteral( "UnfuckIssueTree" ) );
					auto * repairs = dock->findChild<QTreeWidget *>( QStringLiteral( "UnfuckRepairList" ) );
					auto * status = dock->findChild<QLabel *>( QStringLiteral( "UnfuckStatus" ) );
					if ( !tree || !repairs ) { log << "panel widgets missing\n"; break; }

					log << "status: " << ( status ? status->text() : QString() ) << "\n";
					int groups = tree->topLevelItemCount(), findings = 0, located = 0, coloured = 0;
					const QColor plain( wwSkinColor( "text" ) );
					for ( int g = 0; g < groups; g++ ) {
						QTreeWidgetItem * gi = tree->topLevelItem( g );
						log << "  group " << gi->text( 0 ) << "\n";
						for ( int c = 0; c < gi->childCount(); c++ ) {
							QTreeWidgetItem * it = gi->child( c );
							log << "    " << it->text( 0 ) << "  [" << it->text( 1 ) << "]\n";
							if ( it->data( 0, int( Qt::UserRole ) + 1 /* unfucktools.cpp BlockRole */ ).toInt() >= 0 ) { findings++; located++; }
							else if ( !it->text( 1 ).isEmpty() ) findings++;
							if ( it->foreground( 0 ).color() != plain ) coloured++;
						}
					}
					log << groups << " group(s), " << located << " finding(s) with a block number, "
						<< coloured << " coloured\n";

					check( "the scan found something", groups > 0 );
					check( "findings carry the block they point at", located > 0 );
					check( "severity reached the UI as colour", coloured > 0 );
					check( "the repair list was built", repairs->topLevelItemCount() > 0 );

					/* The four repairs that came across from the retired dialog.
					 *
					 * Update All Bounds, Update All Tangent Spaces, Remove Unused
					 * Strings and Make All Skin Partitions live on the Batch and
					 * Optimize pages and none claims sanity(), so the panel's
					 * `page()=="Sanitize" || sanity()` rule cannot see them. They
					 * were reachable from the dialog and nowhere else, so deleting
					 * it without this would have quietly removed four repairs.
					 */
					static const char * const migrated[] = {
						"Update All Bounds", "Update All Tangent Spaces",
						"Remove Unused Strings", "Make All Skin Partitions",
					};
					int found = 0;
					QStringList repairNames;
					for ( int i = 0; i < repairs->topLevelItemCount(); i++ ) {
						const QString n = repairs->topLevelItem( i )
							->data( 0, int( Qt::UserRole ) + 2 /* SpellRole */ ).toString();
						repairNames << n;
						for ( const char * want : migrated )
							if ( n == QLatin1String( want ) )
								found++;
					}
					log << "repairs: " << repairNames.join( QStringLiteral( ", " ) ) << "\n";
					check( "the four Batch/Optimize repairs survived the dialog's removal",
						found == 4 );

					// Go to: click the action column of the first located finding.
					int before = -1, after = -1;
					for ( int g = 0; g < groups && after < 0; g++ ) {
						QTreeWidgetItem * gi = tree->topLevelItem( g );
						for ( int c = 0; c < gi->childCount(); c++ ) {
							QTreeWidgetItem * it = gi->child( c );
							const int b = it->data( 0, int( Qt::UserRole ) + 1 /* unfucktools.cpp BlockRole */ ).toInt();
							if ( b < 0 )
								continue;
							NifModel * m = skope->getNifModel();
							before = m->getBlockNumber( m->getBlockIndex( skope->currentNifIndex() ) );
							/* Double-click, not the action column.
							 *
							 * A row whose action column offers a fix has no "Go to"
							 * cell to click -- one column, one primary action -- so
							 * Go to has to remain reachable some other way, and
							 * double-click anywhere on the row is that way. Testing
							 * the cell would have quietly stopped covering exactly
							 * the rows that gained a fix.
							 */
							emit tree->itemDoubleClicked( it, 0 );
							QApplication::processEvents();
							after = m->getBlockNumber( m->getBlockIndex( skope->currentNifIndex() ) );
							log << "Go to: block " << before << " -> " << after
								<< " (wanted " << b << ")\n";
							check( "Go to selects the offending block", after == b );
							break;
						}
					}
					/* The per-row fix, end to end.
					 *
					 * This is the one action in the panel that claims to repair a
					 * single finding, so "it is offered" is not the question --
					 * "does the finding go away" is. Click it, then re-read the
					 * tree the re-scan rebuilt and confirm that exact row is gone.
					 */
					int fixable = 0;
					QTreeWidgetItem * target = nullptr;
					QString targetText;
					for ( int g = 0; g < tree->topLevelItemCount() && !target; g++ ) {
						QTreeWidgetItem * gi = tree->topLevelItem( g );
						for ( int c = 0; c < gi->childCount(); c++ ) {
							QTreeWidgetItem * it = gi->child( c );
							if ( it->text( 1 ) == QLatin1String( "Fix this one" ) ) {
								fixable++;
								if ( !target ) { target = it; targetText = it->text( 0 ); }
							}
						}
					}
					if ( fixable > 0 ) {
						log << "fixing: " << targetText << "\n";
						emit tree->itemClicked( target, 1 );
						QApplication::processEvents();
						bool stillThere = false;
						for ( int g = 0; g < tree->topLevelItemCount(); g++ ) {
							QTreeWidgetItem * gi = tree->topLevelItem( g );
							for ( int c = 0; c < gi->childCount(); c++ )
								if ( gi->child( c )->text( 0 ) == targetText )
									stillThere = true;
						}
						log << "after: " << ( status ? status->text() : QString() ) << "\n";
						check( "the per-row fix resolved the finding it named", !stillThere );
					} else {
						log << "no per-row-fixable finding in this file\n";
					}

					/* SEVERAL REPAIRS, ONE UNDO STEP.
					 *
					 * The retired dialog ran its whole selection inside a single
					 * snapshot; the panel ran runFix in a loop, so five ticked
					 * boxes left five undo commands and Ctrl+Z walked back through
					 * half-repaired intermediate states one at a time. Deleting
					 * the dialog is only safe if that property came across with
					 * it, and "there is now exactly one command" is not something
					 * the UI can show.
					 */
					if ( NifModel * m = skope->getNifModel(); m && m->undoStack ) {
						/* Arm everything applicable EXCEPT the one that stops for
						 * input. `Fill Blank NiControllerSequence Types` opens a
						 * QInputDialog inside the run, with nobody to answer it —
						 * the first version of this block ticked it and hung the
						 * whole harness until it was killed. Its own hint() says
						 * "it stops and waits for input", which is exactly why it
						 * is offered unticked to real users too.
						 */
						static const char * const stopsForInput[] = {
							"Fill Blank NiControllerSequence Types",
						};
						int armed = 0;
						for ( int i = 0; i < repairs->topLevelItemCount(); i++ ) {
							QTreeWidgetItem * it = repairs->topLevelItem( i );
							if ( it->isDisabled() )
								continue;
							bool interactive = false;
							for ( const char * n : stopsForInput )
								if ( it->data( 0, int( Qt::UserRole ) + 2 /* SpellRole */ )
										.toString() == QLatin1String( n ) )
									interactive = true;
							if ( interactive ) {
								it->setCheckState( 0, Qt::Unchecked );
								continue;
							}
							it->setCheckState( 0, Qt::Checked );
							armed++;
						}
						auto * runBtn = dock->findChild<QPushButton *>(
							QStringLiteral( "UnfuckRunRepairsButton" ) );
						const int cmdsBefore = m->undoStack->count();
						const int blocksBefore = m->getBlockCount();
						log << "arming " << armed << " repair(s), undo commands "
							<< cmdsBefore << "\n";
						if ( runBtn && armed >= 2 ) {
							runBtn->click();
							QApplication::processEvents();
							const int pushed = m->undoStack->count() - cmdsBefore;
							log << "after run: undo commands " << m->undoStack->count()
								<< " (pushed " << pushed << "), blocks "
								<< blocksBefore << " -> " << m->getBlockCount() << "\n";
							check( "a multi-repair run is a single undo step", pushed <= 1 );
							if ( pushed == 1 ) {
								m->undoStack->undo();
								QApplication::processEvents();
								log << "after undo: blocks " << m->getBlockCount()
									<< " (wanted " << blocksBefore << ")\n";
								check( "one Ctrl+Z puts the whole run back",
									m->getBlockCount() == blocksBefore );
							}
						} else {
							log << "not enough applicable repairs to test batching\n";
						}
					}

					dock->hide();
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				/* Mark the document clean before quitting.
				 *
				 * This harness now RUNS a repair, so the model really is modified
				 * and quitting really does raise the unsaved-changes prompt -- with
				 * nobody to answer it, which hung the run. That prompt is correct
				 * behaviour; it is the harness that has to decline it.
				 */
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_ANIMPLAY_TEST=1): does the Animation dock's Play actually
	 * animate the viewport?
	 *
	 * The dock ran a private 16 ms QTimer that advanced its own playhead and
	 * emitted timeChanged -> GLView::setSceneTime. setSceneTime never touches
	 * scene->animate, and IControllable::transform() skips controller evaluation
	 * when !scene->animate -- so with View > Animations OFF, Play scrubbed the
	 * playhead across a frozen viewport. The signal added to fix exactly that,
	 * playPauseRequested, was declared AND connected, and nothing emitted it.
	 *
	 * THE DISCRIMINATING CHECK is scene->animate, not the playhead. The playhead
	 * moved on the broken code too -- that was the whole illusion. So the test
	 * turns animation OFF first, presses the dock's Play, and requires that the
	 * renderer is now actually animating.
	 * Log: release/ww_animplay_test.log
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_ANIMPLAY_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1800, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_animplay_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				do {
					if ( !ok ) { log << "load failed\n"; break; }
					GLView * ogl = skope->getGLView();
					TimelineWidget * tl = skope->timeline;
					if ( !ogl || !tl || !ogl->getScene() ) { log << "no view/timeline\n"; break; }

					log << "scene time range " << ogl->getScene()->timeMin()
						<< " .. " << ogl->getScene()->timeMax() << "\n";
					if ( ogl->getScene()->timeMin() == ogl->getScene()->timeMax() ) {
						log << "this file has no animation to play\n";
						check( "the fixture is animated", false );
						break;
					}

					// View > Animations OFF -- the exact state the bug needed
					if ( skope->ui->aAnimate->isChecked() )
						skope->ui->aAnimate->trigger();
					if ( skope->ui->aAnimPlay->isChecked() )
						skope->ui->aAnimPlay->trigger();
					QApplication::processEvents();
					log << "before: aAnimate " << skope->ui->aAnimate->isChecked()
						<< ", scene->animate " << ogl->getScene()->animate << "\n";
					check( "animation starts switched off", !ogl->getScene()->animate );

					// press the DOCK's play button, through the real signal
					tl->transportToggle( 1 );
					QApplication::processEvents();
					QEventLoop settle;
					QTimer::singleShot( 500, &settle, &QEventLoop::quit );
					settle.exec();

					log << "after Play: aAnimPlay " << skope->ui->aAnimPlay->isChecked()
						<< ", scene->animate " << ogl->getScene()->animate
						<< ", anim speed " << ogl->animationSpeed() << "\n";
					check( "the dock's Play actually animates the viewport",
						ogl->getScene()->animate );
					check( "and the application knows it is playing",
						skope->ui->aAnimPlay->isChecked() );

					// reverse must be the SIGN OF THE SPEED, which is what GLView reads
					tl->transportToggle( -1 );
					QApplication::processEvents();
					log << "after Reverse: anim speed " << ogl->animationSpeed() << "\n";
					check( "reverse is a negative animation speed", ogl->animationSpeed() < 0.0f );

					tl->transportToggle( 0 );
					QApplication::processEvents();
					log << "after Stop: aAnimPlay " << skope->ui->aAnimPlay->isChecked() << "\n";
					check( "stop stops the application, not just the dock",
						!skope->ui->aAnimPlay->isChecked() );
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_TOPBAR_TEST=1): the top bar after the View menu was folded
	 * into the Panels button.
	 *
	 * The dangerous part of that merge is not the duplicate. `View > Show` listed
	 * the same seven dock toggles the Panels button already had, so dropping it
	 * costs nothing. The other three submenus had no home anywhere else, and one
	 * of them - Toolbars - is the ONLY way to bring back a hidden toolbar, because
	 * QMainWindow's built-in toggle popup is unreachable here (nifskope.cpp sets
	 * Qt::NoContextMenu and nothing overrides createPopupMenu). Lose it and hiding
	 * a toolbar is permanent, with no error and nothing to discover.
	 *
	 * The last check is unrelated to the merge and guards the bug this pass
	 * started from: three of the seven viewport modes were drawing the SAME icon,
	 * because Pose and Physics Sim had no glyph and were handed Object Mode's. It
	 * compares the rendered pixels, not the icon names - two QIcons built from
	 * different names still collide if the drawings are the same, which is exactly
	 * how `mode_weightpaint` came to be byte-identical to `brush`.
	 * Log: release/ww_topbar_test.log
	 */
	if ( qEnvironmentVariableIsSet( "WW_TOPBAR_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool, QString & ) {
			QTimer::singleShot( 1500, skope, [skope]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_topbar_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				auto textsOf = []( const QMenu * mn ) {
					QStringList out;
					if ( mn )
						for ( const QAction * a : mn->actions() )
							if ( !a->isSeparator() )
								out << a->text().remove( QLatin1Char( '&' ) );
					return out;
				};
				do {
										// --- LOD is a dropdown, always there, greyed when unusable --
					/* bungo: "LOD will function as a dropdown menu button, just
					 * like animation and collision buttons function. It will be
					 * visible all the time, but greyed out if there's no LODs in
					 * a nif file."
					 *
					 * The old slider hid its own value behind a handle position
					 * and disappeared entirely on a file without LOD meshes - a
					 * control that is absent reads as a missing feature, not as
					 * "not applicable here".
					 */
					{
						auto * lodBtn = skope->findChild<QToolButton *>(
							QStringLiteral( "ViewLodButton" ) );
						auto * animBtn = skope->findChild<QToolButton *>(
							QStringLiteral( "ViewAnimationButton" ) );
						check( "the LOD button exists", lodBtn != nullptr );
						check( "the LOD slider toolbar is gone from the row",
							!skope->findChild<QToolBar *>( QStringLiteral( "tLOD" ) )
							|| !skope->findChild<QToolBar *>( QStringLiteral( "tLOD" ) )->isVisible() );
						if ( lodBtn && animBtn ) {
							log << "LOD button text '" << lodBtn->text()
								<< "', enabled " << lodBtn->isEnabled()
								<< ", menu entries "
								<< ( lodBtn->menu() ? lodBtn->menu()->actions().size() : -1 ) << "\n";
							check( "the button names the current level",
								lodBtn->text().contains( QRegularExpression(
									QStringLiteral( "LOD\\s*\\d" ) ) ) );
							check( "it is visible even with no LODs in the file",
								lodBtn->isVisibleTo( skope ) );
							check( "...but greyed, because this fixture has none",
								!lodBtn->isEnabled() );
							check( "it offers the four levels",
								lodBtn->menu() && lodBtn->menu()->actions().size() >= 4 );
							// it sits with Animation and Collision, after the rule
							check( "it stands beside Animation",
								lodBtn->parentWidget() == animBtn->parentWidget() );
						}
					}


					// --- the View menu is gone from the menu bar ----------------
					/* ui->menubar, NOT QMainWindow::menuBar().
					 *
					 * This fork moves the QMenuBar into the first toolbar so the
					 * menus share the top row, and calls setMenuWidget() with an
					 * empty placeholder. QMainWindow::menuBar() therefore builds a
					 * brand-new EMPTY menu bar and hands that back - against which
					 * "the View menu is gone" passes for the wrong reason. That is
					 * what the File check below is guarding, and it is the reason
					 * it exists.
					 */
					QStringList bar;
					for ( const QAction * a : skope->ui->menubar->actions() )
						if ( !a->isSeparator() )
							bar << a->text().remove( QLatin1Char( '&' ) );
					log << "menu bar: " << bar.join( QStringLiteral( ", " ) ) << "\n";
					check( "the menu bar still has File", bar.contains( QStringLiteral( "File" ) ) );
					/* There IS a View menu again, and it is a different animal.
					 *
					 * The old one held four submenus of dock and display toggles
					 * and is gone into the Panels button. The Render menu was then
					 * renamed View, because that is what it always was - Top,
					 * Front, Left, Flip, Perspective, Walk, Frame Selected, and
					 * nothing that renders anything.
					 *
					 * So a name check alone would pass on either menu. This looks
					 * at the CONTENTS: the viewport one has the view directions,
					 * the retired one had Toolbars and Show.
					 */
					check( "the menu bar has a View menu", bar.contains( QStringLiteral( "View" ) ) );
					QStringList viewItems;
					for ( const QAction * a : skope->ui->mRender->actions() )
						if ( !a->isSeparator() )
							viewItems << a->text().remove( QLatin1Char( '&' ) );
					log << "View menu: " << viewItems.join( QStringLiteral( ", " ) ) << "\n";
					/* It is BOTH now, and that is the point.
					 *
					 * The discriminator used to be "View does not contain
					 * Toolbars", which separated the viewport menu from the
					 * retired dock menu. Then the Panels dropdown was folded in
					 * here, so View legitimately holds Toolbars and the negative
					 * became false for the right reason. Asserting both halves
					 * instead: the view directions it was renamed for, and the
					 * panel toggles it absorbed.
					 */
					check( "and it holds the view directions",
						viewItems.contains( QStringLiteral( "Top" ) )
						&& viewItems.contains( QStringLiteral( "Front" ) ) );
					check( "and the panel toggles it absorbed",
						viewItems.contains( QStringLiteral( "Toolbars" ) )
						&& viewItems.contains( QStringLiteral( "NIF Browser" ) ) );
					check( "Frame Selected came across from the toolbar glyph",
						std::any_of( viewItems.cbegin(), viewItems.cend(),
							[]( const QString & t ) { return t.startsWith( QStringLiteral( "Frame Selected" ) ); } ) );

					/* --- the panel toggles live in the View menu ---------------
					 *
					 * The Panels dropdown is gone: panel toggles are a View-menu
					 * subject, which is where Blender keeps Sidebar and Tool
					 * Settings, and a dedicated button for them was one more
					 * label on a row that had just been trimmed. Everything it
					 * carried has to still be reachable, which is what the rest
					 * of this section checks.
					 */
					check( "the Panels button is gone from the row",
						skope->findChild<QToolButton *>(
							QStringLiteral( "ViewPanelsButton" ) ) == nullptr );
					QMenu * panels = skope->ui->mRender;
					check( "the View menu exists to hold them", panels != nullptr );
					if ( !panels ) break;

					const QStringList top = textsOf( panels );
					log << "Panels: " << top.join( QStringLiteral( ", " ) ) << "\n";

					// --- everything View used to hold is now in there ------------
					for ( const char * dock : { "Block List", "Block Details", "Header",
											    "NIF Browser", "Inspect", "KFM" } )
						check( QStringLiteral( "Panels lists the %1 dock" ).arg( dock ),
							top.contains( QLatin1String( dock ) ) );

					QMenu * toolbars = nullptr, * blockList = nullptr, * blockDetails = nullptr;
					for ( QAction * a : panels->actions() ) {
						if ( !a->menu() )
							continue;
						const QString t = a->text().remove( QLatin1Char( '&' ) );
						if ( t == QLatin1String( "Toolbars" ) )              toolbars = a->menu();
						if ( t == QLatin1String( "Block List Display" ) )    blockList = a->menu();
						if ( t == QLatin1String( "Block Details Display" ) ) blockDetails = a->menu();
					}
					check( "Panels carries the Toolbars submenu", toolbars != nullptr );
					check( "Panels carries the Block List submenu", blockList != nullptr );
					check( "Panels carries the Block Details submenu", blockDetails != nullptr );

					/* A hidden toolbar must still be recoverable. Not "the submenu
					 * exists" - the submenu is filled later, in initMenu(), so an
					 * empty one would pass that and still strand the user.
					 */
					const QStringList tbs = textsOf( toolbars );
					log << "Toolbars: " << tbs.join( QStringLiteral( ", " ) ) << "\n";
					check( "and it can actually bring a hidden toolbar back", tbs.size() >= 3 );
					int checkable = 0;
					if ( toolbars )
						for ( const QAction * a : toolbars->actions() )
							if ( a->isCheckable() )
								checkable++;
					check( "every toolbar entry is a toggle", toolbars && checkable == tbs.size() );

					log << "Block List: " << textsOf( blockList ).join( QStringLiteral( ", " ) )
						<< " | Block Details: "
						<< textsOf( blockDetails ).join( QStringLiteral( ", " ) ) << "\n";
					check( "the block-view display options came across",
						textsOf( blockList ).size() >= 2 && textsOf( blockDetails ).size() >= 2 );

						/* --- the Render menu is greyscale, in BOTH icon states -----
					 *
					 * Measured on the pixels, because "I called the conversion"
					 * is not evidence that the conversion reached the icon the
					 * menu draws. It checks the On state too: aViewUser and
					 * aViewPerspective carry a second, more saturated pixmap, and
					 * converting only the default state leaves the colour to come
					 * back the moment the toggle is used.
					 */
					int coloured = 0, examined = 0;
					QStringList colouredNames;
					for ( QAction * a : skope->ui->mRender->actions() ) {
						if ( a->isSeparator() || a->icon().isNull() )
							continue;
						for ( QIcon::State st : { QIcon::Off, QIcon::On } ) {
							const QPixmap pm = a->icon().pixmap( 22, 22, QIcon::Normal, st );
							if ( pm.isNull() )
								continue;
							examined++;
							const QImage im = pm.toImage().convertToFormat( QImage::Format_ARGB32 );
							int worst = 0;
							for ( int y = 0; y < im.height(); y++ )
								for ( int x = 0; x < im.width(); x++ ) {
									const QColor c = im.pixelColor( x, y );
									if ( c.alpha() == 0 )
										continue;
									worst = std::max( { worst,
										std::abs( c.red() - c.green() ),
										std::abs( c.green() - c.blue() ) } );
								}
							if ( worst > 2 ) {			// 2 to forgive rescaling
								coloured++;
								colouredNames << QStringLiteral( "%1(%2) d=%3" )
									.arg( a->text().remove( QLatin1Char( '&' ) ),
										  st == QIcon::On ? QStringLiteral( "on" )
														  : QStringLiteral( "off" ) )
									.arg( worst );
							}
						}
					}
					log << "Render icons examined: " << examined << ", still coloured: "
						<< ( colouredNames.isEmpty() ? QStringLiteral( "none" )
													 : colouredNames.join( QStringLiteral( ", " ) ) )
						<< "\n";
					check( "the Render menu actually has icons to check", examined >= 10 );
					check( "every Render menu icon is greyscale", coloured == 0 );

					/* --- lighting moved into Viewport Shading -----------------
					 *
					 * Both halves are asserted. The bulb is off the toolbar AND
					 * the sliders are reachable somewhere else - checking only
					 * the removal would pass just as well if the lighting
					 * controls had been deleted outright.
					 */
					bool bulbOnBar = false;
					for ( const QAction * a : skope->ui->tRender->actions() )
						if ( a == skope->ui->aLightMenu )
							bulbOnBar = true;
					check( "the unlabelled Lighting bulb is off the toolbar", !bulbOnBar );

					auto * lightPanel = skope->findChild<LightingWidget *>(
						QStringLiteral( "ShadingLightingPanel" ) );
					check( "the lighting sliders live in Viewport Shading now",
						lightPanel != nullptr );
					if ( lightPanel ) {
						const int sliders = lightPanel->findChildren<QSlider *>().size();
						log << "lighting panel sliders: " << sliders << "\n";
						check( "and all of them came across", sliders >= 7 );
						// "Do Lighting" is this menu's own Unlit/Shaded choice
						auto * dup = lightPanel->findChild<QToolButton *>(
							QStringLiteral( "btnLighting" ) );
						check( "without a second control for Unlit/Shaded",
							!dup || dup->isHidden() );
					}

					/* --- no double rules anywhere on the row -------------------
					 *
					 * wwGroupBreak wraps each rule in two spacer widgets, and
					 * that silently defeated the duplicate-separator cleanup: it
					 * only ever saw two rules as adjacent when NOTHING sat between
					 * them, and now a pad always does. Two groups meeting each
					 * contributed a boundary, and the row drew a double bar
					 * between Object and Global.
					 *
					 * Checked across the WHOLE ROW rather than per toolbar,
					 * because that is the unit anyone looks at - and because the
					 * first fix over-corrected, stripping both sides of the
					 * tMode/tRender boundary and leaving no rule there at all.
					 * Leading and trailing are properties of the row.
					 */
					{
						QStringList rowMarks;
						/* PER ROW, and there are two rows now.
						 *
						 * This used to concatenate five toolbars into one shape
						 * string, on the belief that they were a single row. The
						 * viewport toolbars moved into the footer under the 3D
						 * view, so that belief is false and the concatenation
						 * would be nonsense: a legitimate rule at the right end
						 * of the footer would read as interior, and a legitimate
						 * boundary at the true end of a row would trip the
						 * ends-with check. It would have gone green or red for
						 * reasons unrelated to what it measures.
						 */
						for ( QToolBar * tb : { skope->ui->tFile, skope->ui->tLOD,
												skope->ui->tView } ) {
							if ( !tb || !tb->isVisible() )
								continue;
							for ( QAction * a : tb->actions() ) {
								auto * wa = qobject_cast<QWidgetAction *>( a );
								QWidget * w = wa ? wa->defaultWidget() : nullptr;
								if ( w && w->objectName() == QLatin1String( "wwGroupPad" ) )
									continue;			// padding is not a mark
								if ( !a->isVisible() )
									continue;
								rowMarks << ( a->isSeparator() ? QStringLiteral( "|" )
															   : QStringLiteral( "x" ) );
							}
						}
						const QString shape = rowMarks.join( QString() );
						log << "row shape: " << shape << "\n";
						check( "the row has controls on it", shape.contains( QLatin1Char( 'x' ) ) );
						check( "no two group rules are adjacent",
							!shape.contains( QStringLiteral( "||" ) ) );
						check( "the row does not start with a rule",
							!shape.startsWith( QLatin1Char( '|' ) ) );
						check( "the row does not end with a rule",
							!shape.endsWith( QLatin1Char( '|' ) ) );

						QStringList footMarks;
						for ( QToolBar * tb : { skope->ui->tMode, skope->ui->tRender } ) {
							if ( !tb || !tb->isVisible() )
								continue;
							for ( QAction * a : tb->actions() ) {
								auto * wa = qobject_cast<QWidgetAction *>( a );
								QWidget * w = wa ? wa->defaultWidget() : nullptr;
								if ( w && w->objectName() == QLatin1String( "wwGroupPad" ) )
									continue;
								if ( !a->isVisible() )
									continue;
								footMarks << ( a->isSeparator() ? QStringLiteral( "|" )
																: QStringLiteral( "x" ) );
							}
						}
						const QString foot = footMarks.join( QString() );
						log << "header shape: " << foot << "\n";
						check( "the viewport header has controls on it",
							foot.contains( QLatin1Char( 'x' ) ) );
						check( "no two footer rules are adjacent",
							!foot.contains( QStringLiteral( "||" ) ) );
						check( "the footer does not start with a rule",
							!foot.startsWith( QLatin1Char( '|' ) ) );
						check( "the footer does not end with a rule",
							!foot.endsWith( QLatin1Char( '|' ) ) );
					}

					// --- every viewport mode draws a DIFFERENT icon --------------
					auto * modeBtn = skope->findChild<QToolButton *>(
						QStringLiteral( "ViewportModeButton" ) );
					check( "the mode button can be found by name", modeBtn != nullptr );
					if ( modeBtn && modeBtn->menu() ) {
						const QList<QAction *> modes = modeBtn->menu()->actions();
						QList<QByteArray> seen;
						QStringList dupes;
						for ( const QAction * a : modes ) {
							if ( a->isSeparator() )
								continue;
							QByteArray raw;
							QBuffer buf( &raw );
							buf.open( QIODevice::WriteOnly );
							a->icon().pixmap( 32, 32 ).toImage().save( &buf, "PNG" );
							if ( seen.contains( raw ) )
								dupes << a->text().remove( QLatin1Char( '&' ) );
							seen.append( raw );
						}
						log << "mode entries: " << modes.size() << ", duplicate icons: "
							<< ( dupes.isEmpty() ? QStringLiteral( "none" )
											     : dupes.join( QStringLiteral( ", " ) ) ) << "\n";
						check( "the fixture has every mode to compare", seen.size() >= 7 );
						check( "no two viewport modes draw the same icon", dupes.isEmpty() );
					}
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_SUBTEX_TEST=1): does a texture-atlas flipbook actually
	 * play?
	 *
	 * BSPSysSubTexModifier — "similar to a Flip Controller, this handles particle
	 * texture animation on a single texture atlas" — was not implemented at all.
	 * Every particle picked ONE random cell of the sheet at birth and kept it for
	 * life, so all 302 stock Fallout 4 meshes carrying the modifier showed a
	 * frozen frame of an animation. A 64-cell explosion sheet exists to be played.
	 *
	 * THE DISCRIMINATING MEASUREMENT is how many sprites change cell at once.
	 * Cells changed on the old code too — particles die and are reborn with a new
	 * random cell — so "the cells changed" proves nothing. What cannot happen
	 * without a flipbook is MOST OF THE POPULATION changing cell between two
	 * samples 20 ms apart while the population size holds steady: that needs half
	 * the sprites replaced in 20 ms. With the modifier running it happens on
	 * essentially every sample, because every sprite advances its own playhead.
	 *
	 * Cells are compared as a SORTED multiset, not index by index: a particle
	 * dying removes its slot and shifts every later one, so index-wise comparison
	 * would report a change for the whole tail on any death.
	 * Log: release/ww_subtex_test.log
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_SUBTEX_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1800, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_subtex_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				do {
					if ( !ok ) { log << "load failed\n"; check( "the file loaded", false ); break; }
					GLView * ogl = skope->getGLView();
					NifModel * nif = skope->getNifModel();
					Scene * sc = ogl ? ogl->getScene() : nullptr;
					if ( !sc || !nif ) { log << "no scene\n"; check( "there is a scene", false ); break; }

					int mods = 0, cells = 0;
					for ( int b = 0; b < nif->getBlockCount(); b++ ) {
						if ( nif->isNiBlock( nif->getBlockIndex( b ), "BSPSysSubTexModifier" ) )
							mods++;
						if ( nif->isNiBlock( nif->getBlockIndex( b ), "NiPSysData" ) )
							cells = std::max( cells,
								nif->get<int>( nif->getBlockIndex( b ), "Num Subtexture Offsets" ) );
					}
					log << "  BSPSysSubTexModifier blocks: " << mods
						<< ", largest atlas: " << cells << " cells\n";
					check( "the fixture has a sub-texture modifier", mods > 0 );
					check( "and an atlas with more than one cell to play", cells > 1 );
					if ( mods < 1 || cells < 2 ) break;

					if ( !skope->ui->aAnimate->isChecked() )
						skope->ui->aAnimate->trigger();
					if ( !skope->ui->aAnimPlay->isChecked() )
						skope->ui->aAnimPlay->trigger();

					int bigMoves = 0, comparable = 0, mostMoved = 0;
					QHash<qint32, QVector<float>> prev;
					QEventLoop loop;
					QTimer sampler;
					sampler.setInterval( 20 );
					QObject::connect( &sampler, &QTimer::timeout, &loop, [&]() {
						for ( Node * n : sc->getNodes() ) {
							auto * p = dynamic_cast<Particles *>( n );
							if ( !p || p->liveCount() < 4 )
								continue;
							QVector<float> now;
							const QVector<Vector2> & cs = p->spriteCells();
							for ( int i = 0; i < p->liveCount() && i < cs.size(); i++ )
								now.append( cs.at( i )[0] );
							std::sort( now.begin(), now.end() );
							const qint32 b = nif->getBlockNumber( p->index() );
							const QVector<float> & was = prev[b];
							if ( was.size() == now.size() && !now.isEmpty() ) {
								comparable++;
								int moved = 0;
								for ( int i = 0; i < now.size(); i++ )
									if ( std::fabs( now.at( i ) - was.at( i ) ) > 1.0e-6f )
										moved++;
								mostMoved = std::max( mostMoved, int( moved * 100 / now.size() ) );
								if ( moved * 2 > now.size() )
									bigMoves++;
							}
							prev[b] = now;
						}
					} );
					sampler.start();
					QTimer::singleShot( 3000, &loop, &QEventLoop::quit );
					loop.exec();
					if ( skope->ui->aAnimPlay->isChecked() )
						skope->ui->aAnimPlay->trigger();

					log << "  " << comparable << " comparable samples, " << bigMoves
						<< " where most of the population changed cell; biggest single move "
						<< mostMoved << "%\n";
					check( "there was a steady population to watch", comparable > 10 );
					check( "the sprites advance through the atlas together", bigMoves > 0 );
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_PARTICLECAP_TEST=1): does a particle system simulate the
	 * number of particles the FILE says it may?
	 *
	 * PSysSimController asked the NiParticleSystem for "Num Vertices", and no
	 * version of that block has ever had one — the count is a NiGeometryData row
	 * and lives on the DATA block, renamed "BS Max Vertices" for NiPSysData on
	 * Bethesda 20.2. So the read returned 0 on every file in every game and the
	 * 512 fallback always won. Measured over the stock Fallout 4 mesh tree, 1,345
	 * NiPSysData blocks: NOT ONE is 512. 1,287 authorise fewer, 58 more, and the
	 * smallest is 3.
	 *
	 * WHAT MAKES THIS DISCRIMINATING is saturation, not the bound. "Live <= cap"
	 * would pass on the old code too whenever the emitter is quiet. So the check
	 * is that the population sits exactly ON the cap: below means the emitter is
	 * too slow to prove anything, above means the cap is not being read.
	 * Log: release/ww_particlecap_test.log
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_PARTICLECAP_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1800, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_particlecap_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				do {
					if ( !ok ) { log << "load failed\n"; check( "the file loaded", false ); break; }
					GLView * ogl = skope->getGLView();
					NifModel * nif = skope->getNifModel();
					Scene * sc = ogl ? ogl->getScene() : nullptr;
					if ( !sc || !nif ) { log << "no scene\n"; check( "there is a scene", false ); break; }

					// the cap each particle system authored, straight from the model
					QHash<qint32, int> capOf;			// NiParticleSystem block -> cap
					for ( int b = 0; b < nif->getBlockCount(); b++ ) {
						const QModelIndex iPS = nif->getBlockIndex( b );
						if ( !nif->blockInherits( iPS, "NiParticleSystem" ) )
							continue;
						const QModelIndex iPD = nif->getBlockIndex( nif->getLink( iPS, "Data" ) );
						int cap = nif->get<int>( iPD, "BS Max Vertices" );
						if ( cap < 1 )
							cap = nif->get<int>( iPD, "Num Vertices" );
						capOf[b] = cap;
						log << "  particle system [" << b << "] authored cap " << cap << "\n";
					}
					check( "the fixture has a particle system", !capOf.isEmpty() );
					if ( capOf.isEmpty() ) break;
					const int smallest = *std::min_element( capOf.constBegin(), capOf.constEnd() );
					check( "and it authorises fewer than the 512 fallback",
						smallest > 0 && smallest < 512 );

					if ( !skope->ui->aAnimate->isChecked() )
						skope->ui->aAnimate->trigger();
					if ( !skope->ui->aAnimPlay->isChecked() )
						skope->ui->aAnimPlay->trigger();

					// let it fill up, sampling the live population as it goes
					QHash<qint32, int> peak;
					QEventLoop loop;
					QTimer sampler;
					sampler.setInterval( 20 );
					QObject::connect( &sampler, &QTimer::timeout, &loop, [&]() {
						for ( Node * n : sc->getNodes() ) {
							auto * p = dynamic_cast<Particles *>( n );
							if ( !p )
								continue;
							const qint32 b = nif->getBlockNumber( p->index() );
							peak[b] = std::max( peak.value( b, 0 ), p->liveCount() );
						}
					} );
					sampler.start();
					QTimer::singleShot( 3000, &loop, &QEventLoop::quit );
					loop.exec();
					if ( skope->ui->aAnimPlay->isChecked() )
						skope->ui->aAnimPlay->trigger();

					int over = 0, saturated = 0, seen = 0;
					for ( auto it = capOf.constBegin(); it != capOf.constEnd(); ++it ) {
						const int live = peak.value( it.key(), -1 );
						if ( live < 0 )
							continue;			// no scene node for it
						seen++;
						log << "  [" << it.key() << "] cap " << it.value()
							<< ", peak live " << live << "\n";
						if ( live > it.value() ) over++;
						if ( live == it.value() ) saturated++;
					}
					check( "the particle systems are in the scene", seen > 0 );
					check( "none simulated more particles than the file allows", over == 0 );
					check( "and at least one filled its cap, so the emitter really wanted more",
						saturated > 0 );
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_CYCLETYPE_TEST=1): does the preview do what the SEQUENCE
	 * says it does at its end?
	 *
	 * NiControllerSequence carries a Cycle Type — CYCLE_LOOP, CYCLE_REVERSE or
	 * CYCLE_CLAMP — and nothing read it. One session-wide Loop checkbox decided
	 * for every sequence in every file, it starts unchecked, and saveUi/restoreUi
	 * have the line that would persist it commented out. So the default preview
	 * played every clip exactly once whatever it was authored to do, and
	 * CYCLE_REVERSE had no implementation at all.
	 *
	 * THE FIXTURE IS THE POINT. Bloatfly.nif carries CharFXOn (CYCLE_CLAMP) and
	 * CharFXOnLoop (CYCLE_LOOP) side by side, so one file distinguishes "reads
	 * the field" from "has a new default": no single setting of one checkbox is
	 * right for both, and the first two checks below fail on the old code in
	 * OPPOSITE directions.
	 *
	 * CYCLE_REVERSE is injected into Scene::animCycle rather than edited into the
	 * model, because no stock Fallout 4 mesh uses it and a fixture nobody ships is
	 * not worth manufacturing when the map IS the interface the transport reads.
	 * That the map gets its values from the file is what the two checks above it
	 * measure, on real authored data.
	 * Log: release/ww_cycletype_test.log
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_CYCLETYPE_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1800, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_cycletype_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				do {
					if ( !ok ) { log << "load failed\n"; check( "the file loaded", false ); break; }
					GLView * ogl = skope->getGLView();
					Scene * sc = ogl ? ogl->getScene() : nullptr;
					if ( !sc ) { log << "no scene\n"; check( "there is a scene", false ); break; }

					// --- the fixture must carry both kinds ----------------------
					QString clampSeq, loopSeq;
					for ( const QString & g : sc->animGroups ) {
						const int ct = sc->animCycle.value( g, -1 );
						log << "  sequence '" << g << "' cycle type " << ct << "\n";
						if ( ct == Scene::CycleClamp && clampSeq.isEmpty() ) clampSeq = g;
						if ( ct == Scene::CycleLoop && loopSeq.isEmpty() )   loopSeq = g;
					}
					check( "the fixture has a CYCLE_CLAMP sequence", !clampSeq.isEmpty() );
					check( "the fixture has a CYCLE_LOOP sequence too", !loopSeq.isEmpty() );
					if ( clampSeq.isEmpty() || loopSeq.isEmpty() ) break;

					if ( !skope->ui->aAnimate->isChecked() )
						skope->ui->aAnimate->trigger();
					if ( skope->ui->aAnimSwitch->isChecked() )
						skope->ui->aAnimSwitch->trigger();		// or it walks to the next clip

					// --- the toggle follows the file ---------------------------
					ogl->setSceneSequence( clampSeq );
					QApplication::processEvents();
					log << "after selecting '" << clampSeq << "': Loop "
						<< skope->ui->aAnimLoop->isChecked() << "\n";
					check( "a CYCLE_CLAMP sequence does not loop",
						!skope->ui->aAnimLoop->isChecked() );

					ogl->setSceneSequence( loopSeq );
					QApplication::processEvents();
					log << "after selecting '" << loopSeq << "': Loop "
						<< skope->ui->aAnimLoop->isChecked() << "\n";
					check( "a CYCLE_LOOP sequence loops",
						skope->ui->aAnimLoop->isChecked() );

					/* Run the real transport and watch it, rather than asking the
					 * state what it intends: everything above is a checkbox, and a
					 * checkbox that nothing reads would pass all of it.
					 */
					auto play = [&]( int ms, bool * sawFlip, float * lo, float * hi ) {
						const float d0 = ogl->animationDirection();
						if ( sawFlip ) *sawFlip = false;
						if ( lo ) *lo = 1.0e9f;
						if ( hi ) *hi = -1.0e9f;
						if ( !skope->ui->aAnimPlay->isChecked() )
							skope->ui->aAnimPlay->trigger();
						QEventLoop loop;
						QTimer sampler;
						sampler.setInterval( 10 );
						QObject::connect( &sampler, &QTimer::timeout, &loop, [&]() {
							if ( sawFlip && ogl->animationDirection() * d0 < 0.0f )
								*sawFlip = true;
							if ( lo ) *lo = std::min( *lo, ogl->sceneTime() );
							if ( hi ) *hi = std::max( *hi, ogl->sceneTime() );
						} );
						sampler.start();
						QTimer::singleShot( ms, &loop, &QEventLoop::quit );
						loop.exec();
					};

					// --- CYCLE_LOOP really keeps playing ------------------------
					float lo = 0, hi = 0;
					bool flipped = false;
					play( 600, &flipped, &lo, &hi );
					log << "loop seq after 600 ms: still playing "
						<< skope->ui->aAnimPlay->isChecked() << ", time seen "
						<< lo << " .. " << hi << ", direction "
						<< ogl->animationDirection() << "\n";
					check( "a CYCLE_LOOP sequence is still running after its end",
						skope->ui->aAnimPlay->isChecked() );
					check( "and it never runs backwards", !flipped );

					// --- CYCLE_CLAMP stops -------------------------------------
					if ( skope->ui->aAnimPlay->isChecked() )
						skope->ui->aAnimPlay->trigger();
					ogl->setSceneSequence( clampSeq );
					QApplication::processEvents();
					play( 600, nullptr, nullptr, nullptr );
					log << "clamp seq after 600 ms: still playing "
						<< skope->ui->aAnimPlay->isChecked() << ", time "
						<< ogl->sceneTime() << " (range " << sc->timeMin()
						<< " .. " << sc->timeMax() << ")\n";
					check( "a CYCLE_CLAMP sequence has stopped by itself",
						!skope->ui->aAnimPlay->isChecked() );

					// --- ticking Loop by hand still overrides it ---------------
					if ( !skope->ui->aAnimLoop->isChecked() )
						skope->ui->aAnimLoop->trigger();
					play( 600, nullptr, nullptr, nullptr );
					log << "clamp seq with Loop forced on: still playing "
						<< skope->ui->aAnimPlay->isChecked() << "\n";
					check( "forcing Loop on overrides the file",
						skope->ui->aAnimPlay->isChecked() );

					// --- CYCLE_REVERSE ping-pongs ------------------------------
					if ( skope->ui->aAnimPlay->isChecked() )
						skope->ui->aAnimPlay->trigger();
					sc->animCycle[loopSeq] = Scene::CycleReverse;
					ogl->setSceneSequence( loopSeq );
					QApplication::processEvents();
					check( "a CYCLE_REVERSE sequence loops", skope->ui->aAnimLoop->isChecked() );
					check( "and starts forwards", ogl->animationDirection() > 0.0f );

					flipped = false;
					play( 600, &flipped, &lo, &hi );
					log << "reverse seq after 600 ms: flipped " << flipped
						<< ", time seen " << lo << " .. " << hi
						<< " (range " << sc->timeMin() << " .. " << sc->timeMax()
						<< "), direction " << ogl->animationDirection() << "\n";
					check( "a CYCLE_REVERSE sequence turns round at the end", flipped );
					check( "and stays inside the sequence while doing it",
						lo >= sc->timeMin() - 0.001f && hi <= sc->timeMax() + 0.001f );

					if ( skope->ui->aAnimPlay->isChecked() )
						skope->ui->aAnimPlay->trigger();
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_SELCOLOUR_TEST=1): do the views agree on what a selected
	 * row looks like?
	 *
	 * Audit item 5. Four colours were hardcoded identically in six files, and two
	 * more views had drifted off them -- Pose to #2b3b5c, and Skeleton to
	 * wwSkinColor("danger"), the app's ERROR colour, so a multi-selected bone
	 * rendered in the same red that means "missing texture" in Materials and "key
	 * out of range" in the Timeline.
	 *
	 * The check is not "the literals are gone" -- a grep does that, and would pass
	 * on a conversion that resolved to the wrong colour. It is that the sheet the
	 * shared helper emits actually CONTAINS the four skin values, and that the
	 * error colour is not among them. That last clause is what fails if anyone
	 * points a selection colour back at "danger".
	 * Log: release/ww_selcolour_test.log
	 */
	if ( qEnvironmentVariableIsSet( "WW_SELCOLOUR_TEST" ) ) {
		QTimer::singleShot( 1200, skope, [skope]() {
			QFile logf( QApplication::applicationDirPath() + "/ww_selcolour_test.log" );
			if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) ) { qApp->quit(); return; }
			QTextStream log( &logf );
			int checks = 0, fails = 0;
			auto check = [&]( const QString & what, bool pass ) {
				checks++;
				if ( !pass ) fails++;
				log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
			};

			const QString bgA = wwSkinColor( "selBgActive" );
			const QString bgI = wwSkinColor( "selBgInactive" );
			const QString fgA = wwSkinColor( "selTextActive" );
			const QString fgI = wwSkinColor( "selTextInactive" );
			const QString danger = wwSkinColor( "danger" );
			log << "selBgActive " << bgA << ", selBgInactive " << bgI
				<< ", selTextActive " << fgA << ", selTextInactive " << fgI
				<< ", danger " << danger << "\n";

			check( "all four selection colours are defined",
				!bgA.isEmpty() && !bgI.isEmpty() && !fgA.isEmpty() && !fgI.isEmpty() );
			check( "active and inactive are actually different",
				bgA != bgI && fgA != fgI );
			check( "no selection colour is the error colour",
				bgA != danger && bgI != danger && fgA != danger && fgI != danger );

			const QString qss = wwSelectionTreeQss();
			log << "sheet: " << qss << "\n";
			check( "the shared sheet carries all four",
				qss.contains( bgA ) && qss.contains( bgI )
				&& qss.contains( fgA ) && qss.contains( fgI ) );

			/* And a live view really took it. The Collision tree is the one that
			 * wrote the sheet out by hand, so it is the honest sample.
			 */
			QDockWidget * cd = skope->findChild<QDockWidget *>(
				QStringLiteral( "CollisionManagerDock" ) );
			QTreeWidget * ctree = cd ? cd->findChild<QTreeWidget *>() : nullptr;
			if ( ctree ) {
				const QString sheet = ctree->styleSheet();
				log << "collision tree sheet: " << sheet.left( 80 ) << "...\n";
				check( "a real view is using the shared sheet",
					sheet.contains( bgA ) && !sheet.contains( QLatin1String( "74,122,176" ) ) );
			} else {
				log << "no collision tree to sample\n";
			}
			log << checks << " checks, " << fails << " failures\n";
			log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
			logf.close();
			QTimer::singleShot( 0, qApp, &QApplication::quit );
		} );
	}

	/* TEST HARNESS (WW_DOCKS_TEST=1): does every manager dock start docked and
	 * hidden, and does choosing a workspace still open the right one?
	 *
	 * Seven of the eight ended with addDockWidget + hide(). The Animation Manager
	 * had NEITHER, so a fresh profile opened with it already spread across the
	 * bottom before any workspace was chosen; the Pose Manager never called
	 * addDockWidget at all and was returned floating. Both are now like their
	 * siblings -- and hiding a dock that used to show itself is exactly the kind
	 * of change that could quietly make a workspace unreachable, which is the
	 * second half of this test.
	 * Log: release/ww_docks_test.log
	 */
	if ( qEnvironmentVariableIsSet( "WW_DOCKS_TEST" ) ) {
		QTimer::singleShot( 1500, skope, [skope]() {
			QFile logf( QApplication::applicationDirPath() + "/ww_docks_test.log" );
			if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) ) { qApp->quit(); return; }
			QTextStream log( &logf );
			int checks = 0, fails = 0;
			auto check = [&]( const QString & what, bool pass ) {
				checks++;
				if ( !pass ) fails++;
				log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
			};
			static const char * const dockNames[] = {
				"TimelineDock", "MatTexManagerDock", "CollisionManagerDock",
				"RiggingManagerDock", "UVManagerDock", "PoseManagerDock",
				"SkeletonManagerDock", "UnfuckManagerDock",
			};
			int found = 0, visible = 0, floatingOrOrphan = 0;
			for ( const char * n : dockNames ) {
				QDockWidget * d = skope->findChild<QDockWidget *>( QLatin1String( n ) );
				if ( !d ) { log << "  (no dock named " << n << ")\n"; continue; }
				found++;
				const bool docked = !d->isFloating() && d->parentWidget() != nullptr;
				if ( d->isVisible() ) { visible++; log << "  VISIBLE at startup: " << n << "\n"; }
				if ( !docked ) { floatingOrOrphan++; log << "  NOT DOCKED: " << n << "\n"; }
			}
			log << found << " manager dock(s); " << visible << " visible, "
				<< floatingOrOrphan << " floating/undocked\n";
			check( "the manager docks were found", found >= 6 );
			/* Visibility at startup is NOT this fix's to assert.
			 *
			 * Two versions of this check were wrong before that was clear. The
			 * defect being fixed is at CONSTRUCTION -- a dock that never calls
			 * addDockWidget or hide() and so arrives floating or already open.
			 * What the user sees on the next launch is then decided by
			 * QMainWindow::restoreState replaying the saved window layout, which
			 * is correct behaviour and will legitimately reopen whatever was open
			 * last time. Asserting "nothing is visible" fails on a perfectly good
			 * build the moment someone leaves a panel open.
			 *
			 * So the count is logged, and what is CHECKED is the pair of
			 * properties the constructors actually own: every dock is parented
			 * into the main window rather than floating, and a hidden one can
			 * still be opened.
			 */
			log << "saved workspace: "
				<< QSettings().value( QStringLiteral( "UI/Workspace" ), 0 ).toInt() << "\n";
			check( "all of them are docked, not floating", floatingOrOrphan == 0 );

			// ...and a workspace still opens its dock. Hiding at construction
			// would be a regression if it made one unreachable.
			QDockWidget * tl = skope->findChild<QDockWidget *>( QStringLiteral( "TimelineDock" ) );
			if ( tl ) {
				tl->setFloating( false );
				tl->show();
				QApplication::processEvents();
				log << "Animation dock shows on demand: " << tl->isVisible() << "\n";
				check( "a hidden manager dock can still be opened", tl->isVisible() );
				tl->hide();
			}
			log << checks << " checks, " << fails << " failures\n";
			log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
			logf.close();
			QTimer::singleShot( 0, qApp, &QApplication::quit );
		} );
	}

	/* TEST HARNESS (WW_MULTISEL_TEST=1): do the branch spells honour a
	 * multi-selection, or only the one block they were handed?
	 *
	 * A spell receives ONE index from the menu, so a multi-selection reaches it
	 * only through blockListSelection. Copy Branch consulted it; Remove Branch
	 * and Duplicate Branch did not -- so selecting five nodes and pressing
	 * Ctrl+Delete removed one and left the other four selected and untouched,
	 * under a menu entry that said "Remove Branch" either way.
	 *
	 * Two halves, and the second is what stops the fix being a one-way ratchet:
	 * a multi-selection removes every branch in it, AND a click outside the
	 * selection still removes only the block clicked. An implementation that
	 * always used the selection passes the first and destroys work on the second.
	 * Log: release/ww_multisel_test.log
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_MULTISEL_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1200, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_multisel_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				do {
					if ( !ok ) { log << "load failed\n"; break; }
					NifModel * nif = skope->getNifModel();
					if ( !nif ) { log << "no model\n"; break; }

					// childless blocks, so each removal takes exactly one block
					// and the arithmetic below is exact
					QList<qint32> leaves;
					for ( int b = 1; b < nif->getBlockCount() && leaves.size() < 3; b++ )
						if ( nif->isNiBlock( nif->getBlockIndex( b ) )
							&& nif->getChildLinks( b ).isEmpty() )
							leaves.append( b );
					log << "leaf blocks found: " << leaves.size() << "\n";
					check( "the file has leaf blocks to remove", leaves.size() >= 3 );
					if ( leaves.size() < 3 ) break;

					// --- a selection of two: BOTH must go ---------------------
					const int before = nif->getBlockCount();
					QList<qint32> pick{ leaves.at( 0 ), leaves.at( 1 ) };
					setBlockListSelection( pick );
					spRemoveBranch rm;
					rm.cast( nif, nif->getBlockIndex( pick.first() ) );
					QApplication::processEvents();
					const int afterMulti = nif->getBlockCount();
					log << "2-block selection removed: " << before << " -> " << afterMulti
						<< " (wanted " << before - 2 << ")\n";
					check( "a multi-selection removes every branch in it",
						afterMulti == before - 2 );

					// --- a click OUTSIDE the selection: only that one ---------
					QList<qint32> fresh;		// renumbered by the removal above
					for ( int b = 1; b < nif->getBlockCount() && fresh.size() < 3; b++ )
						if ( nif->isNiBlock( nif->getBlockIndex( b ) )
							&& nif->getChildLinks( b ).isEmpty() )
							fresh.append( b );
					if ( fresh.size() < 3 ) { log << "not enough leaves left\n"; break; }
					setBlockListSelection( QList<qint32>{ fresh.at( 0 ), fresh.at( 1 ) } );
					const int before2 = nif->getBlockCount();
					rm.cast( nif, nif->getBlockIndex( fresh.at( 2 ) ) );	// not selected
					QApplication::processEvents();
					log << "one unselected block removed: " << before2 << " -> "
						<< nif->getBlockCount() << " (wanted " << before2 - 1 << ")\n";
					check( "clicking outside the selection removes only that block",
						nif->getBlockCount() == before2 - 1 );

					setBlockListSelection( QList<qint32>() );
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_MENUTREE_TEST=1): dump the spell menu a block row builds,
	 * and hold the new taxonomy to what it claims.
	 *
	 * Submenus are created the first time a spell on that group registers, so
	 * before this the top-level order was an artefact of the order object files
	 * appear in NifSkope.pro -- and the CONTENTS were an artefact of page(),
	 * which is also the CLI namespace, the Unfuck membership test and the
	 * batch() model-signal switch. Reading the diff cannot tell you whether 106
	 * group() overrides landed on the right spells; only building the menu can.
	 *
	 * The three things a reviewer cannot see by reading:
	 *   - the declared order actually holds (link order no longer decides)
	 *   - the retired page names are GONE as submenu titles, and the whole-file
	 *     pages that legitimately survive are HIDDEN on a block row rather than
	 *     merely inapplicable
	 *   - no submenu shows two enabled entries with the same label, which is the
	 *     failure mode of merging pages: four spells are called "Choose" and
	 *     three "Update", and until now they were kept apart only by living on
	 *     different pages
	 * Log: release/ww_menutree_test.log
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_MENUTREE_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1200, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_menutree_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) ) { qApp->quit(); return; }
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				do {
					if ( !ok ) { log << "load failed\n"; fails++; checks++; break; }
					NifModel * nif = skope->getNifModel();
					if ( !nif || nif->getBlockCount() < 1 ) { log << "no model\n"; fails++; checks++; break; }

					// the widest menu in the file: prefer a geometry block
					int target = 0;
					for ( int b = 0; b < nif->getBlockCount(); b++ ) {
						if ( nif->blockInherits( nif->getBlockIndex( b ),
								{ "BSGeometry", "BSTriShape", "NiTriBasedGeom" } ) ) { target = b; break; }
					}
					const QModelIndex iTarget = nif->getBlockIndex( target );
					log << "menu built on block [" << target << "] "
						<< nif->itemName( iTarget ) << "\n\n";

					SpellBook book( nif, iTarget );

					// ---- dump, and collect what the assertions need ----------
					QStringList topOrder;			// submenu titles, in menu order
					QStringList hiddenTop;			// ...of those, the ones a block row hides
					QStringList everyLabel;			// every leaf label anywhere, visible or not
					QStringList dupes;				// same label twice, both enabled, one menu
					bool sawTexturesUnderMaterial = false;

					std::function<void( QMenu *, int, const QString & )> walk =
						[&]( QMenu * m, int depth, const QString & path ) {
							QHash<QString, int> enabledHere;
							for ( QAction * a : m->actions() ) {
								const QString pad( depth * 2, QLatin1Char( ' ' ) );
								if ( a->menu() ) {
									/* SpellBook doubles '&' so Qt renders the character
									 * instead of eating it as a mnemonic marker. Undo
									 * that here, or every comparison below is against a
									 * string no spell ever declared -- which is how
									 * "Import & Export" first went unnoticed.
									 */
									QString title = a->menu()->title();
									title.replace( QLatin1String( "&&" ), QLatin1String( "&" ) );
									log << pad << "[" << title << "]"
										<< ( a->menu()->isEnabled() ? "" : "   (hidden here)" ) << "\n";
									if ( depth == 0 ) {
										topOrder << title;
										if ( !a->menu()->isEnabled() )
											hiddenTop << title;
									}
									if ( path == QLatin1String( "Material" )
										&& title == QLatin1String( "Textures" ) )
										sawTexturesUnderMaterial = true;
									walk( a->menu(), depth + 1,
										path.isEmpty() ? title : path + QLatin1Char( '/' ) + title );
								} else if ( !a->isSeparator() ) {
									everyLabel << a->text();
									log << pad << ( a->isEnabled() ? "  " : "- " ) << a->text() << "\n";
									if ( a->isEnabled() && ++enabledHere[a->text()] == 2 )
										dupes << ( path.isEmpty() ? a->text()
																  : path + QLatin1String( " / " ) + a->text() );
								}
							}
						};
					log << "---- menu ----\n";
					walk( &book, 0, QString() );
					log << "--------------\n\n";

					// ---- 1. the declared order holds ------------------------
					static const char * const wanted[] = {
						"Block", "Add", "Transform", "Select & View", "Geometry", "Recompute",
						"Skinning", "Material", "Collision", "Animation", "Flags",
						"Import & Export", "Fix", "Info",
					};
					QStringList seenOfWanted;
					for ( const QString & t : topOrder )
						for ( const char * w : wanted )
							if ( t == QLatin1String( w ) ) { seenOfWanted << t; break; }
					QStringList wantedOrder;
					for ( const char * w : wanted )
						if ( seenOfWanted.contains( QLatin1String( w ) ) )
							wantedOrder << QLatin1String( w );
					log << "declared groups present: " << seenOfWanted.join( ", " ) << "\n";
					check( "the submenus are in the declared order, not link order",
						seenOfWanted == wantedOrder );
					check( "most of the new groups exist", seenOfWanted.size() >= 10 );

					// ---- 2. retired page names are gone as titles -----------
					static const char * const retired[] = {
						"Havok", "Node", "Rigging", "Skeleton", "Texture", "Shader",
						"Color", "Mesh", "Bounds", "Morph", "Header", "Footer",
						"String Palette", "Array",
					};
					QStringList survivors;
					for ( const char * r : retired )
						if ( topOrder.contains( QLatin1String( r ) ) )
							survivors << QLatin1String( r );
					log << "retired page names still shown as submenus: "
						<< ( survivors.isEmpty() ? QStringLiteral( "(none)" ) : survivors.join( ", " ) ) << "\n";
					check( "no retired page name is still a submenu", survivors.isEmpty() );

					/* '/' is the group PATH separator, so a group whose own name
					 * contains one is silently split in two. "Import / Export"
					 * built a submenu called "Import " holding a submenu called
					 * " Export", and every check above still passed, because a
					 * title nobody is looking for is a title nobody notices.
					 * A stray space at either end is the visible symptom.
					 */
					QStringList ragged;
					for ( const QString & t : topOrder )
						if ( t != t.trimmed() )
							ragged << QStringLiteral( "'%1'" ).arg( t );
					log << "submenu titles with stray whitespace: "
						<< ( ragged.isEmpty() ? QStringLiteral( "(none)" ) : ragged.join( ", " ) ) << "\n";
					check( "no group name was split by the path separator", ragged.isEmpty() );

					/* ---- 3. the whole-file pages survive, but HIDDEN --------
					 * Batch, Sanitize, Error Checking and Optimize keep their
					 * page() on purpose -- it is what the CLI addresses them by
					 * and what the Unfuck dialog selects on. What they must not
					 * do is appear on a block row, since every one of them casts
					 * against an invalid index and edits the whole file.
					 */
					for ( const char * w : { "Batch", "Sanitize", "Error Checking", "Optimize" } ) {
						const QString t = QLatin1String( w );
						if ( !topOrder.contains( t ) )
							continue;		// nothing left on that page at all
						check( QStringLiteral( "%1 is hidden on a block row" ).arg( t ),
							hiddenTop.contains( t ) );
					}

					// ---- 4. no menu offers the same enabled label twice -----
					log << "duplicate enabled labels: "
						<< ( dupes.isEmpty() ? QStringLiteral( "(none)" ) : dupes.join( "; " ) ) << "\n";
					check( "no submenu offers the same label twice", dupes.isEmpty() );

					// ---- 5. the deletions really happened -------------------
					static const char * const deleted[] = {
						"Create Convex Hull Collision", "Create Convex Decomposition Collision",
						"Create Accurate Mesh Collision", "Fix Bip01", "Scan Bip01",
					};
					QStringList undead;
					for ( const char * d : deleted )
						if ( everyLabel.contains( QLatin1String( d ) ) )
							undead << QLatin1String( d );
					log << "entries that should be gone but are not: "
						<< ( undead.isEmpty() ? QStringLiteral( "(none)" ) : undead.join( ", " ) ) << "\n";
					check( "the retired Create/Bip01 entries are gone", undead.isEmpty() );

					// ---- 6. the two whole-file spells stood down ------------
					for ( const char * n : { "Enforce Node Name Authority",
											 "Decompile All Compiled Collision" } ) {
						SpellPtr sp;
						for ( SpellPtr s : SpellBook::spells() )
							if ( s->name() == QLatin1String( n ) ) { sp = s; break; }
						if ( !sp ) { log << "  (no spell named " << n << ")\n"; continue; }
						// every block, not just the geometry one the menu was built
						// on -- these two used to answer true on all of them
						int stillOffers = 0;
						for ( int b = 0; b < nif->getBlockCount(); b++ )
							if ( sp->isApplicable( nif, nif->getBlockIndex( b ) ) )
								stillOffers++;
						log << "  " << n << ": applicable on " << stillOffers
							<< " of " << nif->getBlockCount() << " block rows\n";
						check( QStringLiteral( "%1 offers itself on no block row" ).arg( QLatin1String( n ) ),
							stillOffers == 0 );
					}
					/* Not checked here: that they still fire from the Spells menu
					 * with an invalid index. tests/spells/unfuck_panel.sh already
					 * casts both that way and asserts they change the file, which
					 * is the half a misplaced guard would break.
					 */

					// ---- 7. Material nests its legacy texture spells --------
					check( "Material has a nested Textures submenu", sawTexturesUnderMaterial );

					/* ---- 8. the verb row ------------------------------------
					 * Copy/Paste/Duplicate Branch are HOISTED out of Block, not
					 * copied into the top level, so exactly one of each must
					 * exist afterwards. A duplicate would look identical in a
					 * screenshot and leave two entries whose enabled state is
					 * then checked independently.
					 */
					SpellBook verbBook( nif, iTarget );
					skope->buildBlockListMenuExtras( verbBook, iTarget );
					int copyBranchSeen = 0;
					std::function<void( QMenu * )> countAll = [&]( QMenu * m ) {
						for ( QAction * a : m->actions() ) {
							if ( a->menu() ) countAll( a->menu() );
							else if ( a->text() == QLatin1String( "Copy Branch" ) ) copyBranchSeen++;
						}
					};
					countAll( &verbBook );
					QStringList topFlat;			// leaf labels above the first separator
					for ( QAction * a : verbBook.actions() ) {
						if ( a->isSeparator() || a->menu() )
							break;
						topFlat << a->text();
					}
					log << "verb row: " << ( topFlat.isEmpty() ? QStringLiteral( "(empty)" )
															   : topFlat.join( ", " ) ) << "\n";
					log << "'Copy Branch' entries anywhere in the menu: " << copyBranchSeen << "\n";
					check( "the verb row leads the menu", topFlat.size() >= 2 );
					check( "Delete is one of the verbs",
						std::any_of( topFlat.cbegin(), topFlat.cend(), []( const QString & t ) {
							return t.startsWith( QLatin1String( "Delete " ) ); } ) );
					check( "Rename is one of the verbs",
						std::any_of( topFlat.cbegin(), topFlat.cend(), []( const QString & t ) {
							return t.startsWith( QLatin1String( "Rename" ) ); } ) );
					check( "Copy Branch was hoisted, not duplicated", copyBranchSeen == 1 );

					// ---- 9. Select & View, and where it sits ----------------
					QStringList verbTops;
					QStringList selectViewEntries;
					for ( QAction * a : verbBook.actions() ) {
						if ( !a->menu() )
							continue;
						QString t = a->menu()->title();
						t.replace( QLatin1String( "&&" ), QLatin1String( "&" ) );
						verbTops << t;
						if ( t == QLatin1String( "Select & View" ) )
							for ( QAction * s : a->menu()->actions() )
								if ( !s->isSeparator() )
									selectViewEntries << s->text();
					}
					log << "Select & View entries: "
						<< ( selectViewEntries.isEmpty() ? QStringLiteral( "(absent)" )
														 : selectViewEntries.join( ", " ) ) << "\n";
					check( "Select & View exists", !selectViewEntries.isEmpty() );
					check( "...with all seven entries", selectViewEntries.size() == 7 );
					const int tIdx = verbTops.indexOf( QLatin1String( "Transform" ) );
					const int svIdx = verbTops.indexOf( QLatin1String( "Select & View" ) );
					log << "Transform at " << tIdx << ", Select & View at " << svIdx << "\n";
					check( "Select & View follows Transform", tIdx >= 0 && svIdx == tIdx + 1 );

					/* ---- 10. Open in <Manager>, on a block that has one -----
					 * One dynamically-titled row, not three fixed ones. On a
					 * geometry block there is nothing to open, so the row must
					 * be ABSENT rather than present-and-disabled — checked both
					 * ways so "never appears" cannot pass as a success.
					 */
					int openRows = 0;
					QString openTitle;
					for ( QAction * a : verbBook.actions() )
						if ( !a->menu() && a->text().startsWith( QLatin1String( "Open in " ) ) ) {
							openRows++;
							openTitle = a->text();
						}
					log << "Open in <Manager> on the geometry block: " << openRows << "\n";
					check( "no Open in row on a block no dock edits", openRows == 0 );

					int shaderBlock = -1;
					for ( int b = 0; b < nif->getBlockCount(); b++ )
						if ( nif->blockInherits( nif->getBlockIndex( b ), "BSShaderProperty" ) ) { shaderBlock = b; break; }
					if ( shaderBlock >= 0 ) {
						SpellBook shaderBook( nif, nif->getBlockIndex( shaderBlock ) );
						skope->buildBlockListMenuExtras( shaderBook, nif->getBlockIndex( shaderBlock ) );
						openRows = 0;
						for ( QAction * a : shaderBook.actions() )
							if ( !a->menu() && a->text().startsWith( QLatin1String( "Open in " ) ) ) {
								openRows++;
								openTitle = a->text();
							}
						log << "shader block [" << shaderBlock << "] offers: '" << openTitle
							<< "' (" << openRows << " row(s))\n";
						check( "a shader block offers exactly one Open in row", openRows == 1 );
						check( "...and it names the Material Manager",
							openTitle == QLatin1String( "Open in Material Manager" ) );
					} else {
						log << "(no shader block in this file to test Open in against)\n";
					}

					/* ---- 11. one rename propagation, not two ----------------
					 * The F2 editor and the Rename (sync animation)… spell had
					 * a copy each. They were identical line for line, so the
					 * merge is a no-op — but "identical" is the claim being
					 * tested, and the thing that must keep working is that a
					 * rename follows the node into the object palette AND into
					 * every controller sequence that addresses it by name.
					 */
					int animNode = -1;
					QString animOld;
					for ( int b = 0; b < nif->getBlockCount() && animNode < 0; b++ ) {
						const QModelIndex iSeq = nif->getBlockIndex( b );
						if ( !nif->blockInherits( iSeq, "NiControllerSequence" ) )
							continue;
						const QModelIndex iCB = nif->getIndex( iSeq, "Controlled Blocks" );
						for ( int r = 0; r < nif->rowCount( iCB ); r++ ) {
							const QString n = nif->resolveString( nif->index( r, 0, iCB ), "Node Name" );
							if ( n.isEmpty() )
								continue;
							for ( int c = 0; c < nif->getBlockCount(); c++ )
								if ( nif->blockInherits( nif->getBlockIndex( c ), "NiAVObject" )
									&& nif->resolveString( nif->getBlockIndex( c ), "Name" ) == n ) {
									animNode = c;
									animOld = n;
									break;
								}
							if ( animNode >= 0 )
								break;
						}
					}
					log << "animated node for the rename check: [" << animNode << "] '" << animOld << "'\n";
					check( "the file has a node addressed by name from a sequence", animNode >= 0 );
					if ( animNode >= 0 ) {
						const QString fresh = animOld + QLatin1String( "_wwRenamed" );
						const int fixed = wwPropagateNodeName( nif, animNode, animOld, fresh );
						int seqHits = 0;
						for ( int b = 0; b < nif->getBlockCount(); b++ ) {
							const QModelIndex iSeq = nif->getBlockIndex( b );
							if ( !nif->blockInherits( iSeq, "NiControllerSequence" ) )
								continue;
							const QModelIndex iCB = nif->getIndex( iSeq, "Controlled Blocks" );
							for ( int r = 0; r < nif->rowCount( iCB ); r++ )
								if ( nif->resolveString( nif->index( r, 0, iCB ), "Node Name" ) == fresh )
									seqHits++;
						}
						log << "wwPropagateNodeName rewrote " << fixed << " reference(s); "
							<< seqHits << " controlled block(s) now name the new node\n";
						check( "the shared propagation rewrites references", fixed > 0 );
						check( "...including the controller sequence bindings", seqHits > 0 );
						// put it back: this harness must not leave the model edited
						wwPropagateNodeName( nif, animNode, fresh, animOld );
					}
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_SPELLSEARCH_TEST=1): the command palette.
	 *
	 * Its whole justification over a filtered menu is that it can show what is
	 * NOT available — Batch, Sanitize, Optimize and Error Checking vanish on a
	 * block row BY DESIGN, and "Update All Tangent Spaces — Batch — not
	 * applicable here" answers the question a "no results" row cannot. That, and
	 * the rule that two keystrokes and Return must never reach Crop To Branch,
	 * are behaviours no diff can show.
	 *
	 * Driven from inside its own modal loop: the driver is scheduled BEFORE
	 * wwSpellPalette is called, so it fires once exec() is already running.
	 * Log: release/ww_spellsearch_test.log
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_SPELLSEARCH_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1200, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_spellsearch_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) ) { qApp->quit(); return; }
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				do {
					if ( !ok ) { log << "load failed\n"; fails++; checks++; break; }
					NifModel * nif = skope->getNifModel();
					if ( !nif || nif->getBlockCount() < 1 ) { log << "no model\n"; fails++; checks++; break; }

					int target = 0;
					for ( int b = 0; b < nif->getBlockCount(); b++ )
						if ( nif->blockInherits( nif->getBlockIndex( b ),
								{ "BSGeometry", "BSTriShape", "NiTriBasedGeom" } ) ) { target = b; break; }
					const QModelIndex iTarget = nif->getBlockIndex( target );

					struct Seen {
						QStringList labels, groups;
						QVector<bool> enabled;
						int current = -1;
					};
					// one run: type the query, record what it found, then Enter or Esc
					auto runPalette = [&]( const QString & query, bool accept, Seen & seen ) -> QAction * {
						SpellBook book( nif, iTarget );
						skope->buildBlockListMenuExtras( book, iTarget );
						QTimer::singleShot( 250, skope, [skope, query, accept, &seen]() {
							QDialog * dlg = skope->findChild<QDialog *>( QStringLiteral( "WWSpellPalette" ) );
							if ( !dlg )
								return;
							auto * edit = dlg->findChild<QLineEdit *>( QStringLiteral( "SpellPaletteSearch" ) );
							auto * view = dlg->findChild<QTreeView *>( QStringLiteral( "SpellPaletteList" ) );
							if ( !edit || !view ) { dlg->reject(); return; }
							edit->setText( query );
							QApplication::processEvents();
							QAbstractItemModel * m = view->model();
							for ( int r = 0; r < m->rowCount() && r < 12; r++ ) {
								seen.labels << m->index( r, 0 ).data().toString();
								seen.groups << m->index( r, 1 ).data().toString();
								seen.enabled << m->index( r, 0 ).data( Qt::UserRole + 2 ).toBool();
							}
							seen.current = view->currentIndex().isValid() ? view->currentIndex().row() : -1;
							if ( accept )
								dlg->accept();
							else
								dlg->reject();
						} );
						return wwSpellPalette( skope, book, QString() );
					};

					// --- 1. a plain query finds its entry and pre-selects it ---
					Seen s1;
					QAction * got = runPalette( QStringLiteral( "copy branch" ), true, s1 );
					log << "query 'copy branch' -> " << s1.labels.join( " | " )
						<< "   (highlighted row " << s1.current << ")\n";
					check( "the query finds Copy Branch",
						!s1.labels.isEmpty() && s1.labels.first() == QLatin1String( "Copy Branch" ) );
					check( "the top hit is pre-selected", s1.current == 0 );
					check( "Enter returns the action", got != nullptr );

					// --- 2. the group is searchable, which is what tells the
					//        four spells called "Copy" apart ------------------
					Seen s2;
					runPalette( QStringLiteral( "transform copy" ), false, s2 );
					log << "query 'transform copy' -> " << s2.labels.join( " | " )
						<< "   groups: " << s2.groups.join( " | " ) << "\n";
					check( "a group term narrows the query",
						!s2.groups.isEmpty() && s2.groups.first() == QLatin1String( "Transform" ) );

					// --- 3. inapplicable entries are SHOWN, and cannot run ----
					Seen s3;
					QAction * dead = runPalette( QStringLiteral( "update all tangent" ), true, s3 );
					log << "query 'update all tangent' -> " << s3.labels.join( " | " )
						<< "   enabled: " << ( s3.enabled.isEmpty() ? QStringLiteral( "-" )
							: QString::number( int( s3.enabled.first() ) ) )
						<< "   (highlighted row " << s3.current << ")\n";
					check( "a whole-file spell is still findable on a block row", !s3.labels.isEmpty() );
					check( "...shown as not applicable", !s3.enabled.isEmpty() && !s3.enabled.first() );
					check( "...and Enter cannot run it", s3.current == -1 && dead == nullptr );

					/* --- 4. a destructive top hit is NOT auto-highlighted -----
					 * The rule that makes typing into this thing safe. Crop To
					 * Branch deletes every block outside the clicked branch, and
					 * two keystrokes plus a reflexive Return must not reach it.
					 */
					Seen s4;
					runPalette( QStringLiteral( "crop" ), false, s4 );
					log << "query 'crop' -> " << s4.labels.join( " | " )
						<< "   (highlighted row " << s4.current << ")\n";
					check( "the destructive entry is found", !s4.labels.isEmpty() );
					check( "...but nothing is auto-highlighted", s4.current == -1 );
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_COLLCOMPILED_TEST=1): editing a COMPILED body in place.
	 *
	 * hknpEncodeSystem reproduces 810 of 822 stock FO4 packfiles byte for byte
	 * and was reachable only from the CLI's own round-trip self-test. The one
	 * production write went through hknpEncodeCompressedMesh, which flattens a
	 * system to a single static body with one triangle mesh — so changing a
	 * compiled body's friction meant Decompile, edit, Compile, and losing the
	 * other bodies, the compounds, the primitives, the constraints and the
	 * ragdoll skeleton on the way.
	 *
	 * The check that matters is the LAST one: one Ctrl+Z has to give back a
	 * byte-identical packfile. Anything less means the edit path is rewriting
	 * more than it was asked to, which is exactly the failure that cannot be
	 * seen by looking at the file in the viewport.
	 * Log: release/ww_collcompiled_test.log
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_COLLCOMPILED_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1500, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_collcompiled_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) ) { qApp->quit(); return; }
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				do {
					if ( !ok ) { log << "load failed\n"; fails++; checks++; break; }
					NifModel * nif = skope->getNifModel();
					if ( !nif || !nif->undoStack ) { log << "no model\n"; fails++; checks++; break; }

					int sysBlock = -1;
					for ( int b = 0; b < nif->getBlockCount(); b++ )
						if ( nif->blockInherits( nif->getBlockIndex( b ), "bhkPhysicsSystem" ) ) { sysBlock = b; break; }
					log << "compiled system at block: " << sysBlock << "\n";
					check( "the fixture has a compiled collision system", sysBlock >= 0 );
					if ( sysBlock < 0 ) break;
					const QModelIndex iSys = nif->getBlockIndex( sysBlock );
					const QByteArray before = nif->get<QByteArray>( iSys, "Binary Data" );
					log << "packfile is " << before.size() << " bytes\n";

					QDockWidget * dock = skope->findChild<QDockWidget *>(
						QStringLiteral( "CollisionManagerDock" ) );
					if ( !dock ) { log << "no dock\n"; fails++; checks++; break; }
					dock->setFloating( false );
					dock->show();
					QApplication::processEvents();
					auto * tree = dock->findChild<QTreeWidget *>( QStringLiteral( "CollisionInventoryTree" ) );
					auto * frictionSpin = dock->findChild<QDoubleSpinBox *>( QStringLiteral( "CollisionFrictionSpin" ) );
					auto * massSpin = dock->findChild<QDoubleSpinBox *>( QStringLiteral( "CollisionMassSpin" ) );
					if ( !tree || !frictionSpin || !massSpin ) {
						log << "panel widgets not found\n"; fails++; checks++; break;
					}

					/* A compiled row is the one where friction is editable and
					 * mass is not — mass lives in the motion/inertia arrays and
					 * is not offered, so that pair identifies it without reading
					 * the panel's private item roles.
					 */
					bool found = false;
					QList<QTreeWidgetItem *> stack;
					for ( int i = 0; i < tree->topLevelItemCount(); i++ )
						stack << tree->topLevelItem( i );
					while ( !stack.isEmpty() && !found ) {
						QTreeWidgetItem * it = stack.takeFirst();
						for ( int c = 0; c < it->childCount(); c++ )
							stack << it->child( c );
						tree->setCurrentItem( it );
						QApplication::processEvents();
						if ( frictionSpin->isEnabled() && !massSpin->isEnabled() )
							found = true;
					}
					log << "a compiled row offering an editable friction: " << found << "\n";
					check( "a compiled body can be edited without decompiling", found );
					if ( !found ) break;

					nif->undoStack->setClean();
					const int base = nif->undoStack->index();
					const double was = frictionSpin->value();
					const double want = ( was > 0.6 ) ? 0.25 : 0.75;
					frictionSpin->setValue( want );
					QMetaObject::invokeMethod( frictionSpin, "editingFinished" );
					QApplication::processEvents();

					const QByteArray after = nif->get<QByteArray>( nif->getBlockIndex( sysBlock ), "Binary Data" );
					log << "friction " << was << " -> " << want << "; packfile "
						<< before.size() << " -> " << after.size() << " bytes, "
						<< ( after == before ? "UNCHANGED" : "rewritten" )
						<< "; undo depth " << base << " -> " << nif->undoStack->index() << "\n";
					check( "the packfile was rewritten", after != before );
					check( "...to the same size, so nothing structural moved", after.size() == before.size() );
					check( "...in exactly one undo step", nif->undoStack->index() == base + 1 );

					const HknpSystem back = hknpDecode( after );
					float got = -1.0f;
					if ( back.valid && !back.bodyPhys.isEmpty() ) {
						// whichever body now carries it; the row's index is private
						for ( const HknpBodyPhys & p : back.bodyPhys )
							if ( std::fabs( p.friction - float( want ) ) < 1.0e-2f ) { got = p.friction; break; }
					}
					log << "re-decoded friction on some body: " << got << "\n";
					check( "the new value survives a re-read", got >= 0.0f );

					/* The one that matters. An edit path that rewrites more than
					 * it was asked to still passes everything above.
					 */
					nif->undoStack->undo();
					QApplication::processEvents();
					const QByteArray undone = nif->get<QByteArray>( nif->getBlockIndex( sysBlock ), "Binary Data" );
					int firstDiff = -1;
					for ( int i = 0; i < std::min( undone.size(), before.size() ); i++ )
						if ( undone.at( i ) != before.at( i ) ) { firstDiff = i; break; }
					log << "after undo: " << undone.size() << " bytes, first difference at "
						<< firstDiff << " (-1 means none)\n";
					check( "one undo restores the byte-identical packfile", undone == before );
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_COLLUNDO_TEST=1): does one edit in the Collision Manager
	 * make exactly one undo step, and does one Ctrl+Z take it back?
	 *
	 * applyPhysics, applyLayerSelection and applyMaterialSelection were fixed in
	 * 2ff7457 to write through nifSnapshotOp, and shipped verified only by reading
	 * the diff. NifModel::set pushes no undo command by itself, so before that fix
	 * these live editors wrote straight through the model while Compile and Apply
	 * Safe Fixes in the same panel went through nifSnapshotOp -- undo appeared to
	 * work and silently reverted whichever of THOSE ran last, keeping the edit
	 * made here. That is invisible in a diff and invisible on screen.
	 *
	 * THE FIXTURE PROBLEM, AND WHY IT IS GONE.  A previous attempt was abandoned
	 * with the note that this needs "a fixture from a game that authors editable
	 * rigid bodies -- Skyrim or Oblivion". FO4 ships compiled collision
	 * (bhkNPCollisionObject) throughout; ~300 meshes were sampled without finding
	 * one editable bhkRigidBody, and manufacturing one via Create Box Collision
	 * needs the GL scene graph and blocks on a modal.
	 *
	 * But the file already contains the body -- compiled. Decompile Compiled
	 * Collision is a pure data transform over the hknp packfile, it needs no
	 * scene, and it runs headless: one CLI cast turns block [2] bhkNPCollisionObject
	 * into a real bhkRigidBody + bhkCollisionObject. tests/spells/collision_undo.sh
	 * builds the fixture that way. No second game required.
	 *
	 * Log: release/ww_collundo_test.log
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_COLLUNDO_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1500, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_collundo_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) ) { qApp->quit(); return; }
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				do {
					if ( !ok ) { log << "load failed\n"; fails++; checks++; break; }
					NifModel * nif = skope->getNifModel();
					if ( !nif || !nif->undoStack ) { log << "no model\n"; fails++; checks++; break; }

					// --- the fixture must really be editable ------------------
					int bodyBlock = -1;
					for ( int b = 0; b < nif->getBlockCount(); b++ )
						if ( nif->blockInherits( nif->getBlockIndex( b ), "bhkRigidBody" ) ) { bodyBlock = b; break; }
					log << "editable bhkRigidBody at block: " << bodyBlock << "\n";
					check( "the fixture has an editable rigid body to edit", bodyBlock >= 0 );
					if ( bodyBlock < 0 ) break;

					auto bodyInfo = [&]() {
						return nif->getIndex( nif->getBlockIndex( bodyBlock ), "Rigid Body Info" );
					};
					auto readLayer = [&]() {
						return nif->get<quint32>( bhkGetHavokFilter( nif, bodyInfo() ), "Layer" );
					};
					auto readMass = [&]() { return nif->get<float>( bodyInfo(), "Mass" ); };

					QDockWidget * dock = skope->findChild<QDockWidget *>(
						QStringLiteral( "CollisionManagerDock" ) );
					if ( !dock ) { log << "no Collision Manager dock\n"; fails++; checks++; break; }
					dock->setFloating( false );
					dock->show();
					QApplication::processEvents();

					auto * tree = dock->findChild<QTreeWidget *>( QStringLiteral( "CollisionInventoryTree" ) );
					auto * layerCombo = dock->findChild<QComboBox *>( QStringLiteral( "CollisionLayerCombo" ) );
					auto * massSpin = dock->findChild<QDoubleSpinBox *>( QStringLiteral( "CollisionMassSpin" ) );
					if ( !tree || !layerCombo || !massSpin ) {
						log << "panel widgets not found (tree/layer/mass)\n"; fails++; checks++; break;
					}

					/* Select the row that drives this body, and be able to do it
					 * again: an undo resets the model, the panel rebuilds its tree
					 * from scratch, and the selection does not survive. The panel's
					 * item roles are private to collisiontools.cpp, so the row is
					 * identified by what it DOES — after selecting it, the physics
					 * editor becomes live.
					 */
					auto selectLive = [&]() -> bool {
						QList<QTreeWidgetItem *> stack;
						for ( int i = 0; i < tree->topLevelItemCount(); i++ )
							stack << tree->topLevelItem( i );
						while ( !stack.isEmpty() ) {
							QTreeWidgetItem * it = stack.takeFirst();
							for ( int c = 0; c < it->childCount(); c++ )
								stack << it->child( c );
							tree->setCurrentItem( it );
							QApplication::processEvents();
							if ( layerCombo->isEnabled() && layerCombo->count() > 0
								&& massSpin->isEnabled() )
								return true;
						}
						return false;
					};
					const bool live = selectLive();
					log << "inventory rows offering the physics editor: "
						<< ( live ? "found one" : "none" ) << "\n";
					check( "an inventory row makes the physics editor live", live );
					if ( !live ) break;

					/* Depth is undoStack->index(), NOT count(). A push after an undo
					 * TRUNCATES the redo tail before adding, so count() can stay the
					 * same across a push that really happened — which is how the
					 * first version of this harness reported a failure on working
					 * code. index() is the position, and one edit must advance it by
					 * exactly one.
					 */
					auto depth = [&]() { return nif->undoStack->index(); };

					// --- 1. the layer combo ----------------------------------
					const quint32 layerBefore = readLayer();
					/* Pick a row that is not the one already showing. The combo does
					 * not display the stored layer when it is Unidentified — the
					 * panel substitutes a guess — so choosing by the MODEL's value
					 * can land on the row already current, and setCurrentIndex then
					 * emits nothing and writes nothing. That is what this harness
					 * first measured, and it looked exactly like a broken editor.
					 */
					int wantRow = -1;
					for ( int r = 0; r < layerCombo->count(); r++ )
						if ( r != layerCombo->currentIndex() && layerCombo->itemData( r ).isValid()
							&& layerCombo->itemData( r ).toUInt() != layerBefore ) { wantRow = r; break; }
					if ( wantRow < 0 ) { log << "no other layer to pick\n"; fails++; checks++; break; }
					const quint32 layerWanted = layerCombo->itemData( wantRow ).toUInt();
					const int base = depth();
					layerCombo->setCurrentIndex( wantRow );
					QApplication::processEvents();
					log << "layer " << layerBefore << " -> " << readLayer()
						<< " (asked for " << layerWanted << "); undo depth "
						<< base << " -> " << depth() << "\n";
					check( "changing the layer writes the model", readLayer() == layerWanted );
					check( "...in exactly one undo step", depth() == base + 1 );
					nif->undoStack->undo();
					QApplication::processEvents();
					log << "after one undo, layer is " << readLayer() << "\n";
					check( "one undo puts the layer back", readLayer() == layerBefore );

					// --- 2. the mass spin, which goes through applyPhysics ----
					if ( !selectLive() ) { log << "lost the editable row\n"; fails++; checks++; break; }
					const float massBefore = readMass();
					const float massWanted = massBefore + 3.5f;
					const int base2 = depth();
					massSpin->setValue( double( massWanted ) );
					// setValue is not "editing", so the signal the panel listens to
					// has to be raised explicitly
					QMetaObject::invokeMethod( massSpin, "editingFinished" );
					QApplication::processEvents();
					log << "mass " << massBefore << " -> " << readMass()
						<< " (asked for " << massWanted << "); undo depth "
						<< base2 << " -> " << depth() << "\n";
					check( "editing mass writes the model",
						std::fabs( readMass() - massWanted ) < 1.0e-3f );
					check( "...in exactly one undo step", depth() == base2 + 1 );
					nif->undoStack->undo();
					QApplication::processEvents();
					check( "one undo puts the mass back",
						std::fabs( readMass() - massBefore ) < 1.0e-3f );

					/* --- 3. the repair spell, on the same body ---------------
					 * Set Collision Layer from Motion is the Unfuck panel's
					 * per-row fix. It could not fire on any Skyrim-or-later file
					 * at all until the Havok Filter mixin was handled, so this
					 * also guards that: an inapplicable spell fails the first
					 * check here rather than passing quietly.
					 */
					nif->set<quint32>( bhkGetHavokFilter( nif, bodyInfo() ), "Layer", 0u );
					const int base3 = depth();
					SpellPtr fix = SpellBook::lookup( QStringLiteral( "Havok/Set Collision Layer from Motion" ) );
					if ( !fix ) { log << "repair spell not found\n"; fails++; checks++; break; }
					check( "the layer repair applies to a zeroed rigid body",
						fix->isApplicable( nif, nif->getBlockIndex( bodyBlock ) ) );
					fix->cast( nif, nif->getBlockIndex( bodyBlock ) );
					QApplication::processEvents();
					log << "repair set layer to " << readLayer() << "; undo depth "
						<< base3 << " -> " << depth() << "\n";
					check( "the repair chose a real layer", readLayer() != 0 );
					check( "...in exactly one undo step", depth() == base3 + 1 );
					nif->undoStack->undo();
					QApplication::processEvents();
					check( "one undo puts the zeroed layer back", readLayer() == 0 );
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_ROTKEY_TEST=1): can a key be inserted on a rotation lane?
	 *
	 * One of the two fixes in 2ff7457 that shipped verified only by reading the
	 * diff, and the changelog said so at the time: "the rotation fix compiles and
	 * the sampler is straightforward, but nothing has yet inserted a rotation key
	 * and read it back."
	 *
	 * insertKeyAtTime used to skip QuatVal outright, because Controller::interpolate
	 * has no Quat specialisation — so I or a double-click on a rotation lane
	 * produced no key and no message, on the most-keyed channel there is. The fix
	 * SLERPs between the bracketing keys off the list the function already reads.
	 *
	 * The discriminating assertion is the key COUNT: the old code silently did
	 * nothing, so "one more key exists afterwards" fails on it. The value check is
	 * secondary and deliberately loose — a SLERP result between two keys must be a
	 * unit quaternion, which catches an uninitialised or zeroed sample without
	 * pinning the interpolation to one implementation.
	 * Log: release/ww_rotkey_test.log
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_ROTKEY_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1800, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_rotkey_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				do {
					if ( !ok ) { log << "load failed\n"; break; }
					NifModel * nif = skope->getNifModel();
					TimelineWidget * tl = skope->timeline;
					if ( !nif || !tl ) { log << "no model or timeline\n"; break; }
					if ( skope->dTimeline ) { skope->dTimeline->show(); skope->dTimeline->raise(); }
					QApplication::processEvents();
					QEventLoop settle;
					QTimer::singleShot( 600, &settle, &QEventLoop::quit );
					settle.exec();

					// a lane carrying a quaternion channel with at least two keys
					int lane = -1, chIdx = -1;
					for ( int l = 0; l < tl->lanes.size() && lane < 0; l++ ) {
						const TimelineLane & L = tl->lanes.at( l );
						for ( int c = 0; c < L.channels.size(); c++ ) {
							const TimelineChannel & ch = L.channels.at( c );
							if ( ch.type == TimelineChannel::QuatVal && ch.iKeysArray.isValid()
								&& nif->rowCount( QModelIndex( ch.iKeysArray ) ) >= 2 ) {
								lane = l; chIdx = c; break;
							}
						}
					}
					log << tl->lanes.size() << " lane(s); quaternion lane = " << lane << "\n";
					check( "the file has a rotation lane to test", lane >= 0 );
					if ( lane < 0 ) {
						log << "no NiTransformData rotation keys in this file\n";
						break;
					}

					const TimelineChannel & ch = tl->lanes.at( lane ).channels.at( chIdx );
					QModelIndex keysArr( ch.iKeysArray );
					const int before = nif->rowCount( keysArr );
					// midway between the first two keys, so the insert lands
					// strictly inside the range and has two keys to blend
					const float t0 = nif->get<float>( nif->getIndex( keysArr, 0 ), "Time" );
					const float t1 = nif->get<float>( nif->getIndex( keysArr, 1 ), "Time" );
					const float at = ( t0 + t1 ) * 0.5f;
					log << "keys " << before << ", inserting at t=" << at
						<< " (between " << t0 << " and " << t1 << ")\n";

					tl->insertKeyAtTime( lane, at );
					QApplication::processEvents();

					keysArr = QModelIndex( ch.iKeysArray );
					const int after = nif->rowCount( keysArr );
					log << "keys after insert: " << after << "\n";
					check( "inserting on a rotation lane adds a key", after == before + 1 );

					if ( after == before + 1 ) {
						// find the row at the inserted time and read its quaternion
						int row = -1;
						for ( int r = 0; r < after; r++ ) {
							const float t = nif->get<float>( nif->getIndex( keysArr, r ), "Time" );
							if ( std::fabs( t - at ) < 1.0e-4f ) { row = r; break; }
						}
						log << "inserted row = " << row << "\n";
						check( "the new key sits at the requested time", row >= 0 );
						if ( row >= 0 ) {
							const Quat q = nif->get<Quat>( nif->getIndex( keysArr, row ), "Value" );
							const float len = std::sqrt( q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3] );
							log << "value = (" << q[0] << ", " << q[1] << ", " << q[2] << ", "
								<< q[3] << "), length " << len << "\n";
							check( "the sampled rotation is a unit quaternion",
								std::fabs( len - 1.0f ) < 1.0e-3f );
						}
					}
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_SCRUB_TEST=1): do all number fields scrub the same way?
	 *
	 * bungo asked for the Move field's behaviour -- hover arrows, press-drag the
	 * number -- on every type-in field in the program. An audit found FIVE
	 * separate implementations of that gesture, none reachable from the others
	 * because the original lives in an anonymous namespace:
	 *
	 *   WwNumberField           nifskope_ui.cpp:183      the reference (Move X/Y/Z)
	 *   UVWwNumberField         uvtools.cpp:96
	 *   CollisionWwNumberField  spells/collisiontools.cpp:345
	 *   ColorWwNumberField      ui/widgets/colorwheel.cpp:105
	 *   WwScrubFilter         ui/widgets/valueedit.cpp:72
	 *
	 * THIS FILE IS WRITTEN BEFORE THE FIX, DELIBERATELY. Every check below was
	 * run against the unfixed binary first and its failure recorded, because a
	 * consistency harness written after the fact cannot tell you it would have
	 * caught anything. The recorded "before" numbers are in the commit message.
	 *
	 * The widgets are constructed here rather than driven through their real
	 * dialogs on purpose: Transform Edit and Light are modal (exec() blocks the
	 * harness), and what is being measured is the WIDGET's behaviour, which is
	 * the same object either way. The two construction paths below are exactly
	 * the two the app uses -- ui/widgets/nifeditors.cpp:311 builds a bare
	 * VectorEdit, ui/widgets/valueedit.cpp:489 builds one and then attaches the
	 * scrub filter -- so the divergence they expose is the real one.
	 *
	 * MOUSE EVENTS GO THROUGH QApplication::sendEvent TO THE LINE EDIT, with
	 * globalPosition set. The gesture is implemented as an event filter on the
	 * spin box's internal QLineEdit (nifskope_ui.cpp:196) and reads only
	 * globalPosition (:249), so synthetic globals are sufficient and no real
	 * pointer is moved -- this must never take the cursor away from whoever is
	 * working. Never QTest::mouseMove, never QCursor::setPos.
	 *
	 * Log: release/ww_scrub_test.log
	 */
	if ( qEnvironmentVariableIsSet( "WW_SCRUB_TEST" ) ) {
		QTimer::singleShot( 1200, skope, [skope]() {
			QFile logf( QApplication::applicationDirPath() + "/ww_scrub_test.log" );
			if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) ) { qApp->quit(); return; }
			QTextStream log( &logf );
			int checks = 0, fails = 0;
			auto check = [&]( const QString & what, bool pass ) {
				checks++;
				if ( !pass ) fails++;
				log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
			};

			/* One scrub gesture on a widget's internal line edit.
			 *
			 * Returns false if the widget had no line edit to drive, so a check
			 * can tell "the gesture did nothing" from "there was nothing to
			 * drive" -- the precondition that stops a missing widget reading as
			 * a passing test.
			 */
			auto scrub = []( QWidget * host, int dx, Qt::KeyboardModifiers mods = Qt::NoModifier ) -> bool {
				QLineEdit * le = host ? host->findChild<QLineEdit *>() : nullptr;
				if ( !le )
					le = qobject_cast<QLineEdit *>( host );
				if ( !le )
					return false;
				const QPointF start( 20, 6 );
				const QPointF gStart( 500, 500 );
				QMouseEvent press( QEvent::MouseButtonPress, start, gStart,
					Qt::LeftButton, Qt::LeftButton, mods );
				QApplication::sendEvent( le, &press );
				// several moves, as a real drag delivers: the threshold latches
				// on the first one past 2 px and the rest map absolutely
				for ( int step = 1; step <= 4; step++ ) {
					const qreal f = qreal( step ) / 4.0;
					QMouseEvent move( QEvent::MouseMove, start + QPointF( dx * f, 0 ),
						gStart + QPointF( dx * f, 0 ), Qt::NoButton, Qt::LeftButton, mods );
					QApplication::sendEvent( le, &move );
				}
				QMouseEvent rel( QEvent::MouseButtonRelease, start + QPointF( dx, 0 ),
					gStart + QPointF( dx, 0 ), Qt::LeftButton, Qt::NoButton, mods );
				QApplication::sendEvent( le, &rel );
				return true;
			};

			// ---- A. the same widget, built two ways -------------------------
			//
			// VectorEdit is what X/Y/Z rows are made of. Block Details builds it
			// through ValueEdit, which attaches the scrub; every spell dialog
			// builds it directly (nifeditors.cpp:311) and gets nothing. Same
			// class, same row, different feel, depending only on who called new.
			{
				VectorEdit * direct = new VectorEdit( skope );
				direct->setVector3( Vector3( 0, 0, 0 ) );
				QDoubleSpinBox * dsb = direct->findChild<QDoubleSpinBox *>();
				check( "A: the directly-built VectorEdit has a spin box to drive", dsb != nullptr );
				double before = dsb ? dsb->value() : 0.0;
				bool driven = scrub( dsb, 50 );
				double after = dsb ? dsb->value() : 0.0;
				check( "A: ...and a line edit to send the gesture to", driven );
				log << "A: direct VectorEdit  " << before << " -> " << after
					<< "  (delta " << ( after - before ) << ")\n";
				check( "A: dragging a spell-dialog X field changes it", driven && after != before );
				direct->deleteLater();

				ValueEdit * ve = new ValueEdit( skope );
				NifValue nv( NifValue::tVector3 );
				nv.set<Vector3>( Vector3( 0, 0, 0 ), nullptr, nullptr );
				ve->setValue( nv );
				QDoubleSpinBox * vdsb = ve->findChild<QDoubleSpinBox *>();
				double vbefore = vdsb ? vdsb->value() : 0.0;
				scrub( vdsb, 50 );
				double vafter = vdsb ? vdsb->value() : 0.0;
				log << "A: ValueEdit VectorEdit  " << vbefore << " -> " << vafter
					<< "  (delta " << ( vafter - vbefore ) << ")\n";
				check( "A: the Block Details X field does change (control)",
					vdsb && vafter != vbefore );
				// the point of the check: identical widget, opposite behaviour
				check( "A: both construction paths behave the same",
					dsb && vdsb && ( after != before ) == ( vafter != vbefore ) );
				ve->deleteLater();
			}

			// ---- B. the reference field's step law ---------------------------
			//
			// Move X/Y/Z never calls setSingleStep, so it inherits Qt's default
			// 1.0 and scrubs at 0.1 units/px. This is the one number in the
			// whole exercise that must not move; it is asserted numerically so
			// a shared widget that "helpfully" sets a step is caught.
			{
				QDoubleSpinBox * ref = nullptr;
				const QList<QDoubleSpinBox *> all = skope->findChildren<QDoubleSpinBox *>();
				for ( QDoubleSpinBox * s : all ) {
					if ( s->decimals() == 4 && s->maximum() >= 1.0e6 ) { ref = s; break; }
				}
				if ( !ref ) {
					log << "B: skip - the gizmo panel is not built until a transform starts\n";
				} else {
					log << "B: reference field singleStep " << ref->singleStep() << "\n";
					check( "B: the Move field's step is still Qt's default 1.0",
						qAbs( ref->singleStep() - 1.0 ) < 1e-9 );
					const double b0 = ref->value();
					scrub( ref, 50 );
					log << "B: 50 px -> delta " << ( ref->value() - b0 ) << " (want 5)\n";
					check( "B: 50 px moves the Move field by 5.0",
						qAbs( ( ref->value() - b0 ) - 5.0 ) < 1e-6 );
				}
			}

			// ---- C. the sentinel ---------------------------------------------
			//
			// FloatEdit spells +-FLT_MAX as "<float_max>" (floatedit.cpp:123).
			// The Block Details step law scales per-pixel step by magnitude
			// (valueedit.cpp:105), so on that value it is 3.4e38 * 0.005 =
			// 1.7e36 PER PIXEL. Three pixels of accidental drag replaces a
			// meaningful sentinel with garbage.
			{
				ValueEdit * ve = new ValueEdit( skope );
				NifValue nv( NifValue::tFloat );
				nv.set<float>( FLT_MAX, nullptr, nullptr );
				ve->setValue( nv );
				FloatEdit * fe = ve->findChild<FloatEdit *>();
				if ( !fe )
					fe = qobject_cast<FloatEdit *>( ve->focusProxy() );
				if ( !fe ) {
					log << "C: skip - no FloatEdit built for tFloat\n";
				} else {
					const QString shown = fe->text();
					log << "C: field shows '" << shown << "'\n";
					scrub( fe, 3 );
					log << "C: after a 3 px drag it shows '" << fe->text() << "'\n";
					check( "C: a 3 px drag does not destroy the float_max sentinel",
						fe->text() == shown );
				}
				ve->deleteLater();
			}

			// ---- E. the number must not sit on top of the arrows -------------
			//
			// The gutters only exist if the internal line edit is held out of
			// them. Without the inset the value paints over the glyphs AND
			// covers the two click zones, so the arrows become decorative: a
			// press in the margin lands on the line edit and scrubs instead.
			{
				auto * nf = new WwNumberField( skope );
				// show() and polish first: an unshown widget has never had its
				// internal layout run, so its editor geometry is meaningless
				// and the check would be measuring nothing
				nf->show();
				nf->ensurePolished();
				nf->resize( 200, 24 );
				QApplication::processEvents();
				QLineEdit * le = nf->findChild<QLineEdit *>();
				const QRect g = le ? le->geometry() : QRect();
				log << "E: host is " << nf->width() << "x" << nf->height()
					<< ", chrome " << nf->findChildren<WwScrubChrome *>().size()
					<< ", editor " << g.x() << "," << g.y()
					<< " " << g.width() << "x" << g.height() << "\n";
				check( "E: the field has its chrome",
					!nf->findChildren<WwScrubChrome *>().isEmpty() );
				check( "E: the number is held clear of the left gutter", g.x() == 16 );
				check( "E: ...and of the right one", g.width() == nf->width() - 32 );

				// and the functional half: a gutter click must STEP, not scrub.
				// Range and start value are set deliberately: a default
				// QDoubleSpinBox starts at 0 with minimum 0, so a step DOWN
				// clamps and the check would fail on correct code.
				nf->setRange( -100.0, 100.0 );
				nf->setValue( 10.0 );
				const double before = nf->value();
				QMouseEvent gp( QEvent::MouseButtonPress, QPointF( 8, 12 ), QPointF( 508, 512 ),
					Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
				QApplication::sendEvent( nf, &gp );
				log << "E: left-gutter click " << before << " -> " << nf->value() << "\n";
				/* Weaker than it looks, and worth saying so: the event is sent
				 * straight to the field, so this proves mousePressEvent's
				 * arithmetic, not that a real click REACHES it. What guarantees
				 * that is the inset asserted above - without it the line edit
				 * covers the gutters and swallows the press.
				 */
				check( "E: a click in the gutter steps the value down",
					nf->value() == before - nf->singleStep() );
				nf->deleteLater();
			}

			// ---- F. a locked field must look locked --------------------------
			//
			// Setting `color` in a stylesheet overrides the palette's Disabled
			// role, so a read-only field kept painting its number in the
			// ordinary colour and read as editable - which is what happened in
			// the Collision Manager, where a compiled body greys Motion and
			// Quality but the numbers stayed bright. Rendered and compared as
			// pixels, not asserted about the sheet text: what matters is what
			// the user sees.
			{
				auto * nf = new WwNumberField( skope );
				nf->resize( 120, 24 );
				nf->setValue( 42.0 );
				QApplication::processEvents();
				const QImage on = nf->grab().toImage();
				nf->setEnabled( false );
				QApplication::processEvents();
				const QImage off = nf->grab().toImage();
				int differing = 0;
				if ( !on.isNull() && on.size() == off.size() ) {
					for ( int y = 0; y < on.height(); y++ )
						for ( int x = 0; x < on.width(); x++ )
							if ( on.pixel( x, y ) != off.pixel( x, y ) )
								differing++;
				}
				log << "F: pixels differing, enabled vs disabled: " << differing << "\n";
				check( "F: a disabled field renders differently from an enabled one",
					differing > 0 );
				nf->deleteLater();
			}

			// ---- G. a field you have already clicked must still scrub --------
			//
			// bungo: "For move and other type of numeric input boxes, I can no
			// longer left click and hold to move the number down or up."
			//
			// A plain click focuses the field. A version of this widget passed
			// focused presses through to the line edit so drag-select would work
			// while typing - which silently killed scrubbing on every field the
			// user had ever clicked, including the Move field the whole feature
			// is named after. Scrubbing wins; caret placement is preserved
			// instead, and only drag-select inside a focused field is given up.
			{
				auto * nf = new WwNumberField( skope );
				nf->show();
				nf->ensurePolished();
				nf->resize( 200, 24 );
				nf->setRange( -1000.0, 1000.0 );
				nf->setValue( 0.0 );
				QApplication::processEvents();

				// first gesture: click, which focuses and selects
				scrub( nf, 0 );
				QApplication::processEvents();
				QLineEdit * le = nf->findChild<QLineEdit *>();
				log << "G: after a plain click, editor focus = "
					<< ( le && le->hasFocus() ? "yes" : "no" ) << "\n";

				// second gesture: the drag that used to stop working
				const double before = nf->value();
				scrub( nf, 50 );
				log << "G: drag on the focused field " << before
					<< " -> " << nf->value() << "\n";
				check( "G: a field that already has focus still scrubs",
					nf->value() != before );
				check( "G: ...by the same amount as an unfocused one",
					qAbs( ( nf->value() - before ) - 5.0 ) < 1e-6 );

				// and the caret lands where the click did rather than the whole
				// number being re-selected under the user
				scrub( nf, 0 );
				QApplication::processEvents();
				log << "G: after a click on the focused field, selection length "
					<< ( le ? le->selectedText().length() : -1 ) << "\n";
				check( "G: a click on an already-focused field places the caret",
					le && le->selectedText().isEmpty() );
				nf->deleteLater();
			}

			// ---- H. Esc and RMB cancel a drag --------------------------------
			//
			// Blender: "Press Esc or RMB to cancel." Until this landed a drag
			// could only be committed - once you started pulling, the value you
			// started from was gone and Ctrl+Z was the only way back.
			{
				auto * nf = new WwNumberField( skope );
				nf->show(); nf->ensurePolished();
				nf->resize( 200, 24 );
				nf->setRange( -1000.0, 1000.0 );
				nf->setValue( 7.0 );
				QApplication::processEvents();
				QLineEdit * le = nf->findChild<QLineEdit *>();

				// drag partway, then Esc without releasing
				const QPointF p0( 60, 12 ), g0( 500, 512 );
				QMouseEvent press( QEvent::MouseButtonPress, p0, g0,
					Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
				QApplication::sendEvent( le, &press );
				QMouseEvent mv( QEvent::MouseMove, p0 + QPointF( 40, 0 ), g0 + QPointF( 40, 0 ),
					Qt::NoButton, Qt::LeftButton, Qt::NoModifier );
				QApplication::sendEvent( le, &mv );
				log << "H: mid-drag the value reads " << nf->value() << " (was 7)\n";
				check( "H: the drag is actually moving the value", nf->value() != 7.0 );
				QKeyEvent esc( QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier );
				QApplication::sendEvent( le, &esc );
				log << "H: after Esc it reads " << nf->value() << "\n";
				check( "H: Esc puts the value back where the drag started",
					qAbs( nf->value() - 7.0 ) < 1e-9 );

				// and RMB does the same
				nf->setValue( 3.0 );
				QApplication::sendEvent( le, &press );
				QApplication::sendEvent( le, &mv );
				QMouseEvent rmb( QEvent::MouseButtonPress, p0, g0,
					Qt::RightButton, Qt::RightButton, Qt::NoModifier );
				QApplication::sendEvent( le, &rmb );
				log << "H: after RMB it reads " << nf->value() << " (was 3)\n";
				check( "H: RMB cancels a drag too", qAbs( nf->value() - 3.0 ) < 1e-9 );
				nf->deleteLater();
			}

			// ---- I. Ctrl snaps to whole steps --------------------------------
			//
			// "Hold Ctrl to snap to the discrete steps while dragging or Shift
			// for precision input." Only Shift was ever implemented, so half the
			// documented pair was missing.
			{
				auto * nf = new WwNumberField( skope );
				nf->show(); nf->ensurePolished();
				nf->resize( 200, 24 );
				nf->setRange( -1000.0, 1000.0 );
				nf->setDecimals( 4 );
				nf->setValue( 0.0 );
				QApplication::processEvents();

				scrub( nf, 37 );                       // 37 px -> 3.7 unsnapped
				const double free = nf->value();
				nf->setValue( 0.0 );
				scrub( nf, 37, Qt::ControlModifier );  // -> 4.0 snapped
				const double snapped = nf->value();
				log << "I: 37 px free = " << free << ", with Ctrl = " << snapped << "\n";
				check( "I: an unsnapped drag lands off a step", qAbs( free - qRound( free ) ) > 1e-6 );
				check( "I: Ctrl snaps it to a whole step", qAbs( snapped - qRound( snapped ) ) < 1e-9 );
				nf->deleteLater();
			}

			// ---- J. typed expressions ----------------------------------------
			//
			// "You can enter mathematical expressions into any number field...
			// Even constants like pi or functions like sqrt(2) may be used."
			{
				struct { const char * in; double want; bool valid; } cases[] = {
					{ "3*2",        6.0,                true  },
					{ "10/5+4",     6.0,                true  },
					{ "1024/3",     341.3333333333,     true  },
					{ "2^10",       1024.0,             true  },
					{ "pi",         M_PI,               true  },
					{ "sqrt(2)",    1.4142135624,       true  },
					{ "rad(90)",    M_PI / 2.0,         true  },
					{ "-(3+4)*2",  -14.0,               true  },
					{ "42",         0.0,                false },  // plain number: host handles it
					{ "1/0",        0.0,                false },  // refused, not inf
					{ "system(rm)", 0.0,                false },  // no identifiers beyond the named set
					{ "3+",         0.0,                false },
				};
				int good = 0, total = 0;
				for ( const auto & c : cases ) {
					total++;
					double got = 0.0;
					const bool did = wwEvalExpression( QString::fromLatin1( c.in ), got );
					const bool pass = ( did == c.valid )
						&& ( !c.valid || qAbs( got - c.want ) < 1e-6 );
					if ( pass ) good++;
					else log << "J: '" << c.in << "' -> evaluated=" << did
							 << " value=" << got << " (wanted " << c.valid << " / " << c.want << ")\n";
				}
				log << "J: expression cases " << good << "/" << total << "\n";
				check( "J: expressions, constants and refusals all behave", good == total );
			}

			// ---- K. one drag can set a whole X/Y/Z row -----------------------
			//
			// Blender: "You can edit multiple number fields at once by pressing
			// down LMB on the first field, and then dragging vertically over the
			// fields you want to edit." Recruitment is by SIBLING, so a drag that
			// wanders off the form cannot grab unrelated fields.
			{
				auto * row = new QWidget( skope );
				row->setGeometry( 40, 300, 240, 90 );
				auto * fx = new WwNumberField( row );
				auto * fy = new WwNumberField( row );
				for ( WwNumberField * f : { fx, fy } ) {
					f->setRange( -1000.0, 1000.0 );
					f->setValue( 0.0 );
				}
				fx->setGeometry( 0, 0, 200, 24 );
				fy->setGeometry( 0, 30, 200, 24 );
				row->show();
				QApplication::processEvents();

				QLineEdit * le = fx->findChild<QLineEdit *>();
				const QPoint gx = fx->mapToGlobal( QPoint( 100, 12 ) );
				const QPoint gy = fy->mapToGlobal( QPoint( 100, 12 ) );
				log << "K: X at global " << gx.x() << "," << gx.y()
					<< "   Y at " << gy.x() << "," << gy.y() << "\n";

				QMouseEvent press( QEvent::MouseButtonPress, QPointF( 100, 12 ), QPointF( gx ),
					Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
				QApplication::sendEvent( le, &press );
				// vertical first: recruit Y
				QMouseEvent down( QEvent::MouseMove, QPointF( 100, 42 ), QPointF( gy ),
					Qt::NoButton, Qt::LeftButton, Qt::NoModifier );
				QApplication::sendEvent( le, &down );
				// then horizontal: move everything recruited
				QMouseEvent side( QEvent::MouseMove, QPointF( 150, 42 ),
					QPointF( gy + QPoint( 50, 0 ) ),
					Qt::NoButton, Qt::LeftButton, Qt::NoModifier );
				QApplication::sendEvent( le, &side );
				QMouseEvent rel( QEvent::MouseButtonRelease, QPointF( 150, 42 ),
					QPointF( gy + QPoint( 50, 0 ) ),
					Qt::LeftButton, Qt::NoButton, Qt::NoModifier );
				QApplication::sendEvent( le, &rel );

				log << "K: after the drag X = " << fx->value() << ", Y = " << fy->value() << "\n";
				check( "K: the field under the press moved", fx->value() != 0.0 );
				check( "K: ...and so did the one dragged over", fy->value() != 0.0 );
				check( "K: ...by the same amount",
					qAbs( fx->value() - fy->value() ) < 1e-6 );
				row->deleteLater();
			}

			// ---- L. a theme reload restyles the fields ------------------------
			//
			// The field's stylesheet is built from wwSkinColor at CONSTRUCTION,
			// which happens before loadTheme runs. wwRestyleScrubFields existed
			// to fix that and was never called by anything - so this checks the
			// WIRING, not the function: clear a field's sheet, ask the app to
			// reload its theme, and see whether the field got it back.
			{
				auto * nf = new WwNumberField( skope );
				nf->show(); nf->ensurePolished();
				nf->resize( 120, 24 );
				QApplication::processEvents();
				check( "L: the field carries a skin-built stylesheet",
					!nf->styleSheet().isEmpty() );

				nf->setStyleSheet( QString() );		// as if it had never been styled
				NifSkope::reloadTheme();
				QApplication::processEvents();
				log << "L: after reloadTheme the sheet is "
					<< ( nf->styleSheet().isEmpty() ? "still empty" : "restored" ) << "\n";
				check( "L: a theme reload reaches the number fields",
					!nf->styleSheet().isEmpty() );
				nf->deleteLater();
			}

			log << checks << " checks, " << fails << " failures\n";
			log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
			logf.close();
			QTimer::singleShot( 0, qApp, &QApplication::quit );
		} );
	}

	/* TEST HARNESS (WW_GIZMONUM_TEST=1): can a decimal point be typed into a
	 * modal transform?
	 *
	 * bungo: "Cannot type in 11.25 in transform, move, rotate or scale, instead
	 * 1125 gets typed in." The digits arrived and the '.' did not, because THREE
	 * separate handlers upstream of GLView's numeric buffer claim a bare period:
	 * the app-wide filter's frame_selection branch, the same filter's numpad
	 * branch via handleBlenderNumpad, and physicsKeyPress inside GLView itself.
	 *
	 * THE KEYS MUST GO THROUGH QApplication::sendEvent, NOT ogl->keyPressEvent.
	 * Calling the handler directly skips the application event filter, which is
	 * where two of the three thefts happen — a harness written that way passes
	 * on the broken code and proves nothing. That is the single thing this test
	 * depends on getting right.
	 *
	 * Log: release/ww_gizmonum_test.log
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_GIZMONUM_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1500, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_gizmonum_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				do {
					if ( !ok ) { log << "load failed\n"; break; }
					NifModel * nif = skope->getNifModel();
					GLView * ogl = skope->getGLView();
					if ( !nif || !ogl ) { log << "no model or view\n"; break; }

					// a node to transform
					int node = -1;
					for ( int b = 0; b < nif->getBlockCount() && node < 0; b++ )
						if ( nif->blockInherits( nif->getBlockIndex( b ), "NiAVObject" ) )
							node = b;
					if ( node < 0 ) { log << "no NiAVObject\n"; break; }
					skope->select( nif->getBlockIndex( node ) );
					QApplication::processEvents();
					log << "selected block " << node << "\n";

					/* The pointer HAS to be inside the viewport, because that is
					 * the condition the buggy branches test — and it is tested
					 * against the CONTAINER widget, not the QOpenGLWindow, which
					 * is what the filter measures with
					 * graphicsView->rect().contains( mapFromGlobal( QCursor::pos() ) ).
					 * Park it in the middle of that, or the bug cannot reproduce
					 * and the test goes green for the wrong reason.
					 */
					QWidget * vp = ogl->graphicsView;
					if ( !vp ) { log << "no viewport container\n"; break; }
					/* Retry, and FAIL if it never lands.
					 *
					 * A single setPos is not reliable: under load the window may
					 * not be laid out yet, so mapToGlobal returns a position that
					 * is not actually over the widget. When that happened the four
					 * checks below still passed — because with the pointer outside,
					 * the branch that steals '.' never fires, so even the BROKEN
					 * build looks fixed. Caught by the shell script's precondition
					 * grep, which is exactly the kind of green-for-the-wrong-reason
					 * this run has produced twice now, so the harness asserts it
					 * itself rather than leaving it to the caller.
					 */
					// the window has to be on screen before its coordinates mean
					// anything: a hidden or unmapped widget maps to a point setPos
					// then clamps away, and the check silently reads false
					skope->show();
					skope->raise();
					QApplication::processEvents();

					/* MOVE THE WINDOW TO THE POINTER, not the pointer to the window.
					 *
					 * QCursor::setPos is a no-op on Windows when the calling
					 * process is not the foreground window, and a headless test run
					 * never is. Measured: setPos(2719,536) left the cursor at
					 * (611,707) on all ten attempts, so the harness reported
					 * "pointer over viewport: 0" and — before the check above
					 * existed — passed all four of its real assertions anyway,
					 * because with the pointer outside, the branch that steals '.'
					 * never fires and even a broken build looks fixed.
					 *
					 * Repositioning the window needs no foreground rights and is
					 * exact.
					 */
					bool overViewport = false;
					for ( int attempt = 0; attempt < 10 && !overViewport; attempt++ ) {
						const QPoint cursor = QCursor::pos();
						const QPoint centre = vp->mapToGlobal( vp->rect().center() );
						if ( centre != cursor )
							skope->move( skope->pos() + ( cursor - centre ) );
						QApplication::processEvents();
						overViewport = vp->rect().contains( vp->mapFromGlobal( QCursor::pos() ) );
						if ( attempt == 0 || !overViewport )
							log << "  attempt " << attempt << ": viewport " << vp->width() << "x"
								<< vp->height() << ", cursor (" << cursor.x() << "," << cursor.y()
								<< "), viewport centre (" << centre.x() << "," << centre.y()
								<< ") -> over = " << overViewport << "\n";
						if ( !overViewport ) {
							QEventLoop wait;
							QTimer::singleShot( 150, &wait, &QEventLoop::quit );
							wait.exec();
						}
					}
					log << "pointer over viewport: " << overViewport << "\n";
					check( "the pointer is over the viewport (or nothing below is tested)",
						overViewport );
					if ( !overViewport )
						break;

					if ( !ogl->startModalTransform( 1 ) ) {
						log << "startModalTransform refused\n"; break;
					}
					QApplication::processEvents();
					log << "gizmoMode = " << ogl->gizmoMode << "\n";
					check( "a modal move transform started", ogl->gizmoMode != 0 );

					auto typeKey = [ogl]( int key, const QString & text ) {
						QKeyEvent press( QEvent::KeyPress, key, Qt::NoModifier, text );
						QApplication::sendEvent( ogl, &press );
						QApplication::processEvents();
					};

					// "11.25", one key at a time, through the real event path
					typeKey( Qt::Key_1, QStringLiteral( "1" ) );
					typeKey( Qt::Key_1, QStringLiteral( "1" ) );
					typeKey( Qt::Key_Period, QStringLiteral( "." ) );
					typeKey( Qt::Key_2, QStringLiteral( "2" ) );
					typeKey( Qt::Key_5, QStringLiteral( "5" ) );

					const QString typed = ogl->gizmoNum.value( 0 );
					log << "gizmoNum[0] = '" << typed << "' (wanted '11.25')\n";
					check( "the digits reached the transform", typed.contains( QLatin1String( "11" ) ) );
					check( "the decimal point survived", typed.contains( QLatin1Char( '.' ) ) );
					check( "the whole number arrived", typed == QLatin1String( "11.25" ) );

					// abandon the transform rather than committing it: this
					// harness is about the keystrokes, not about moving anything
					ogl->gizmoEnd( false );
					QApplication::processEvents();
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_DESTRUCTIVE_TEST=1): is the confirmation on the right
	 * spells, does it say what is lost, and can it still be turned off?
	 *
	 * The prompt this replaces fired for nearly every write and carried a "Do not
	 * ask me again" box, so one tick disarmed it for Crop To Branch too. Reading
	 * the code cannot tell you the new one is any better -- a `destructive()` that
	 * returns true but is never consulted, or a dialog whose Cancel still lets the
	 * cast through, both look correct on the page. So all four properties are
	 * measured against a real cast:
	 *
	 *   1. Cancel means cancel        the file is unchanged afterwards
	 *   2. it names the loss          the text carries the actual block counts,
	 *                                 which is the whole difference from
	 *                                 "this action cannot currently be undone"
	 *   3. it cannot be suppressed    set the old suppression key and it still asks
	 *   4. and it is not on everything  Move Up, non-destructive, is not to ask at
	 *                                 all -- the old code prompted for it, so this
	 *                                 check is what fails on the previous version
	 *
	 * The dialog is modal and exec() blocks, so it is answered by a timer started
	 * before the cast: it polls for the active modal window from inside the nested
	 * loop, records the text and clicks a button. `sawDialog` staying false is a
	 * real result, not an absence of evidence -- check 4 depends on it.
	 * Log: release/ww_destructive_test.log
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_DESTRUCTIVE_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1200, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_destructive_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				do {
					if ( !ok ) { log << "load failed\n"; break; }
					NifModel * nif = skope->getNifModel();
					if ( !nif || nif->getBlockCount() < 3 ) { log << "no usable model\n"; break; }

					/* NOT the root. Cropping to the root keeps the whole file, and
					 * the first version of this harness did exactly that: the
					 * dialog truthfully said "Delete 0 of the 268 blocks" and the
					 * count check failed on a fixture problem rather than a bug.
					 */
					int target = -1;
					const QList<int> roots = nif->getRootLinks();
					for ( int b = 0; b < nif->getBlockCount() && target < 0; b++ ) {
						if ( nif->blockInherits( nif->getBlockIndex( b ), "NiAVObject" )
							&& !roots.contains( b ) )
							target = b;
					}
					if ( target < 0 ) { log << "no non-root NiAVObject\n"; break; }
					log << "target: block " << target << " "
						<< nif->itemName( nif->getBlockIndex( target ) ) << "\n";

					/* Answer whatever modal appears while a cast runs.
					 *
					 * Deliberately ANY modal dialog, not just a QMessageBox. The
					 * prompt this change removes was a CheckableMessageBox, which
					 * is a plain QDialog — a poller that only recognised
					 * QMessageBox would leave it unanswered, so the old code would
					 * hang here instead of failing, and "a non-destructive spell
					 * runs without a prompt" would read as passing right up until
					 * the run timed out. A check that cannot fail cleanly on the
					 * code it exists to reject is not a check.
					 *
					 * `accept` picks the button by role rather than by text: the
					 * go-ahead is labelled with the operation ("Crop To Branch"),
					 * deliberately, so there is no fixed string to match on.
					 */
					QString seen, seenClass;
					bool sawDialog = false;
					auto answerNextDialog = [&seen, &seenClass, &sawDialog]( bool accept ) {
						auto * poll = new QTimer;
						poll->setInterval( 40 );
						QObject::connect( poll, &QTimer::timeout, poll,
							[poll, &seen, &seenClass, &sawDialog, accept]() {
							auto * dlg = qobject_cast<QDialog *>( QApplication::activeModalWidget() );
							if ( !dlg )
								return;
							sawDialog = true;
							seenClass = QString::fromLatin1( dlg->metaObject()->className() );
							poll->stop();

							if ( auto * box = qobject_cast<QMessageBox *>( dlg ) ) {
								seen = box->text();
								for ( QAbstractButton * b : box->buttons() ) {
									if ( ( box->buttonRole( b ) == QMessageBox::AcceptRole ) == accept ) {
										b->click();
										return;
									}
								}
								box->reject();
								return;
							}

							// anything else: read its labels, then take the
							// button box if it has one
							QStringList text;
							for ( QLabel * l : dlg->findChildren<QLabel *>() )
								if ( !l->text().isEmpty() )
									text << l->text();
							seen = text.join( QLatin1Char( ' ' ) );
							if ( auto * bb = dlg->findChild<QDialogButtonBox *>() ) {
								if ( QPushButton * b = bb->button( accept
										? QDialogButtonBox::Yes : QDialogButtonBox::No ) ) {
									b->click();
									return;
								}
							}
							if ( accept ) dlg->accept(); else dlg->reject();
						} );
						poll->start();
						return poll;
					};

					/* Move Up FIRST, because it renumbers.
					 *
					 * It ran last in the first version of this harness and shifted
					 * the block the crop checks had already chosen, so the final
					 * cast cropped to a different, childless block and deleted 267
					 * where the dialog had said 0. Both numbers were right about
					 * the block they were describing; they were describing
					 * different blocks.
					 */
					int mover = -1;
					for ( int b = nif->getBlockCount() - 1; b > target && mover < 0; b-- )
						if ( nif->isNiBlock( nif->getBlockIndex( b ) ) )
							mover = b;
					if ( mover > 0 ) {
						const QString wasAt = nif->itemName( nif->getBlockIndex( mover ) );
						seen.clear(); sawDialog = false;
						QTimer * p3 = answerNextDialog( true );
						skope->castSpell( QStringLiteral( "Block/Move Up" ),
							nif->getBlockIndex( mover ) );
						QApplication::processEvents();
						p3->stop(); delete p3;
						const bool moved = nif->itemName( nif->getBlockIndex( mover - 1 ) ) == wasAt;
						log << "Move Up on block " << mover << ": dialog seen " << sawDialog
							<< " " << seenClass << ", block moved " << moved << "\n";
						check( "a non-destructive spell runs without a prompt", !sawDialog );
						check( "and it still ran", moved );
					} else {
						log << "no block to Move Up\n";
					}

					const int before = nif->getBlockCount();

					// --- Cancel, and what the text says -------------------------
					seen.clear(); sawDialog = false;
					QTimer * p1 = answerNextDialog( false );
					skope->castSpell( QStringLiteral( "Block/Crop To Branch" ),
						nif->getBlockIndex( target ) );
					QApplication::processEvents();
					p1->stop(); delete p1;
					log << "cancel path: dialog seen " << sawDialog << ", blocks "
						<< before << " -> " << nif->getBlockCount() << "\n";
					log << "  text: " << seen << "\n";
					check( "Crop To Branch asks before it runs", sawDialog );
					check( "Cancel leaves the file alone", nif->getBlockCount() == before );

					// the counts are the point: "Delete 214 of the 217 blocks..."
					static const QRegularExpression counts(
						QStringLiteral( "Delete (\\d+) of the (\\d+) blocks" ) );
					const auto m = counts.match( seen );
					bool named = m.hasMatch() && m.captured( 2 ).toInt() == before
						&& m.captured( 1 ).toInt() > 0;
					check( "the question names how much of the file goes", named );

					// --- the old suppression key must not disarm it -------------
					{
						QSettings cfg;
						cfg.setValue( "Settings/Suppress Undoable Confirmation", true );
					}
					seen.clear(); sawDialog = false;
					QTimer * p2 = answerNextDialog( false );
					skope->castSpell( QStringLiteral( "Block/Crop To Branch" ),
						nif->getBlockIndex( target ) );
					QApplication::processEvents();
					p2->stop(); delete p2;
					log << "with Suppress Undoable Confirmation set: dialog seen "
						<< sawDialog << "\n";
					check( "the confirmation cannot be switched off", sawDialog );
					{
						QSettings cfg;
						cfg.remove( QStringLiteral( "Settings/Suppress Undoable Confirmation" ) );
					}

					/* The go-ahead, and the strongest check here: the file must
					 * lose EXACTLY the number the dialog quoted.
					 *
					 * "it deleted something" would pass on a warning whose counts
					 * are off by any amount, and a count that is merely plausible
					 * is worse than none — it is the number the user weighed the
					 * decision on.
					 */
					seen.clear(); sawDialog = false;
					const int wasCount = nif->getBlockCount();
					QTimer * p4 = answerNextDialog( true );
					skope->castSpell( QStringLiteral( "Block/Crop To Branch" ),
						nif->getBlockIndex( target ) );
					QApplication::processEvents();
					p4->stop(); delete p4;
					const auto m2 = counts.match( seen );
					const int promised = m2.hasMatch() ? m2.captured( 1 ).toInt() : -1;
					const int lost = wasCount - nif->getBlockCount();
					log << "accept path: blocks " << wasCount << " -> "
						<< nif->getBlockCount() << " (lost " << lost
						<< ", dialog promised " << promised << ")\n";
					check( "the go-ahead button actually casts", lost > 0 );
					check( "and the file loses exactly what the dialog quoted", lost == promised );
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				// this harness really does modify the file; decline the save prompt
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_SEQBIND_TEST=1): does a sequence bind through the file's
	 * object palette, and does that ever differ from a name search?
	 *
	 * The engine resolves every Controlled Block through
	 * `NiDefaultAVObjectPalette::GetAVObject` on the palette the FILE supplies.
	 * NifSkope used to search the scene graph by name and take the first hit.
	 * On a file with unique node names the two agree and there is nothing to see,
	 * which is exactly why this has to run on a MERGED file: merging is what
	 * produces two nodes with one name, and a name search cannot tell them apart.
	 *
	 * `differs` is the measurement. It is the count of rows where the palette and
	 * `findChild` pick different nodes — so it is zero for the old behaviour by
	 * construction, and a non-zero reading is proof the palette decided something
	 * the search would have got wrong. Nothing else here would fail if the palette
	 * were ignored: the same blocks bind, the same animations play, just onto the
	 * wrong limb.
	 *
	 * Log: release/ww_seqbind_test.log, plus release/ww_seqbind_debug.log with
	 * WW_SEQBIND_DEBUG=1 for the per-row detail.
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_SEQBIND_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1200, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_seqbind_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				do {
					if ( !ok ) { log << "load failed\n"; break; }
					NifModel * nif = skope->getNifModel();
					Scene * scene = skope->ogl ? skope->ogl->getScene() : nullptr;
					if ( !nif || !scene ) { log << "no model or scene\n"; break; }

					int palettes = 0, entries = 0;
					for ( int b = 0; b < nif->getBlockCount(); b++ ) {
						QModelIndex idx = nif->getBlockIndex( b );
						if ( !nif->blockInherits( idx, "NiDefaultAVObjectPalette" ) )
							continue;
						palettes++;
						entries += nif->rowCount( nif->getIndex( idx, "Objs" ) );
					}
					const QStringList groups = scene->animGroups;
					log << palettes << " palette(s), " << entries << " entries, "
						<< groups.size() << " sequence(s)\n";
					check( "the file has an object palette to bind through", palettes > 0 );
					check( "it has sequences to bind", !groups.isEmpty() );

					SeqBind::reset();
					for ( const QString & g : groups ) {
						skope->ogl->setSceneSequence( g );
						QApplication::processEvents();
					}
					const SeqBind::Stats s = SeqBind::stats();
					/* Machine-readable, because what counts as a pass is a property
					 * of the FILE, not of the code: a file with unique node names
					 * must read differs=0 and a file whose palette points somewhere
					 * else must read differs>0, and only the caller knows which it
					 * handed over. tests/anim/sequence_binding.sh asserts both.
					 */
					log << "stats rows=" << s.rows << " palette=" << s.viaPalette
						<< " differs=" << s.differs << " unresolved=" << s.unresolved << "\n";

					check( "controlled blocks were bound at all", s.rows > 0 );
					// Every name the palette carries must come from the palette. Rows
					// left to the name search are rows the palette does not mention,
					// which is a fact about the file — reported, not judged.
					check( "no row resolved through the palette to nothing",
						s.viaPalette == 0 || s.unresolved < s.rows );
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_GROUPSKEL_TEST=<skeleton.nif>): marking one loaded NIF as
	 * THE skeleton makes the others snap to it — and marking nothing changes nothing.
	 *
	 * The measurement is the evaluated skin, Shape::skinVertex for every vertex of
	 * the primary's largest shape, which is what the viewport actually draws. Block
	 * counts and transforms would not do: bones are addressed by block number inside
	 * each file, so "snapping" is entirely a question of what the skin evaluates to.
	 *
	 * Marking a skeleton that is in the same bind pose as the armour legitimately
	 * moves almost nothing, so a "did it move" check against a bind-pose skeleton
	 * would prove nothing either way. The skeleton is therefore POSED — every
	 * non-root node translated — and the question becomes whether the armour follows.
	 *
	 * Both halves matter, and the second is the one the user asked for explicitly:
	 * with no skeleton marked, a loaded file that happens to contain a skeleton must
	 * have no effect at all, even while it is being posed.
	 * Log: release/ww_groupskel_test.log
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_GROUPSKEL_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1200, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_groupskel_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass ) fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				auto settle = []( int ms ) {
					QEventLoop loop;
					QTimer::singleShot( ms, &loop, &QEventLoop::quit );
					loop.exec();
					QApplication::processEvents();
				};
				do {
					if ( !ok ) { log << "load failed\n"; break; }
					NifModel * nif = skope->getNifModel();
					if ( !nif ) { log << "no primary model\n"; break; }

					// the primary's biggest skinned shape stands in for the armour
					int sb = -1, best = 0;
					for ( int b = 0; b < nif->getBlockCount(); b++ )
						if ( nif->blockInherits( nif->getBlockIndex( b ), "BSTriShape" ) ) {
							const int nv = nif->get<int>( nif->getBlockIndex( b ), "Num Vertices" );
							if ( nv > best ) { best = nv; sb = b; }
						}
					if ( sb < 0 ) { log << "no BSTriShape in the primary\n"; break; }

					// A paint is what runs applyWorkspaceSkeleton, so every
					// measurement has to be taken after one.
					auto repaint = [skope, &settle]() {
						skope->ogl->update();
						QApplication::processEvents();
						skope->ogl->grabFramebuffer();
						settle( 120 );
					};
					auto skinned = [skope, nif, sb]() {
						QVector<Vector3> out;
						Shape * s = skope->ogl->shapeForBlock( sb );
						if ( !s )
							return out;
						s->updateBoneTransforms();
						QModelIndex iVD = nif->getIndex( nif->getBlockIndex( sb ), "Vertex Data" );
						for ( int v = 0; v < nif->rowCount( iVD ); v++ )
							out.append( s->skinVertex( v,
								nif->get<Vector3>( nif->getIndex( iVD, v ), "Vertex" ) ) );
						return out;
					};
					auto movedCount = []( const QVector<Vector3> & a, const QVector<Vector3> & b ) {
						int n = 0;
						for ( int i = 0; i < a.size() && i < b.size(); i++ )
							if ( ( a[i] - b[i] ).length() > 0.01f )
								n++;
						return n;
					};

					const QString skelFile = qEnvironmentVariable( "WW_GROUPSKEL_TEST" );
					check( "the skeleton joined the workspace",
						skope->addWorkspaceDocumentFromFile( skelFile ) );
					NifModel * skel = skope->workspaceDocumentModel( 0 );
					check( "its model is reachable", skel != nullptr );
					if ( !skel )
						break;

					// Move every non-root bone, so any weighted vertex must follow.
					auto poseSkeleton = [skel]( float dz ) {
						int n = 0;
						const QList<int> roots = skel->getRootLinks();
						const int skelRoot = roots.isEmpty() ? -1 : roots.first();
						for ( int b = 0; b < skel->getBlockCount(); b++ ) {
							if ( b == skelRoot )
								continue;
							QModelIndex idx = skel->getBlockIndex( b );
							if ( !skel->blockInherits( idx, "NiNode" ) )
								continue;
							QModelIndex iT = skel->getIndex( idx, "Translation" );
							if ( !iT.isValid() )
								continue;
							Vector3 t = skel->get<Vector3>( iT );
							t[2] += dz;
							if ( skel->set<Vector3>( iT, t ) )
								n++;
						}
						return n;
					};
					// What the primary scene believes one bone's height is.
					auto overrideZ = [skope]( const QString & bone ) {
						Scene * ps = skope->ogl->getScene();
						if ( !ps )
							return -9999.0f;
						auto it = ps->skeletonOverride.constFind( bone );
						return ( it != ps->skeletonOverride.constEnd() )
							? it.value().translation[2] : -9999.0f;
					};

					repaint();
					const QVector<Vector3> before = skinned();
					log << "measuring " << before.size() << " evaluated vertices\n";
					check( "the primary has evaluated skin to measure", before.size() > 0 );

					// Mark the skeleton while it is still in its own bind pose. This
					// SHOULD move almost nothing — the armour's bones and a bind-pose
					// skeleton agree — so it is logged, not asserted. Asserting
					// movement here would be asserting that two identical poses differ.
					check( "the skeleton can be marked", skope->setWorkspaceSkeletonDocument( 0 ) );
					repaint();
					const QVector<Vector3> markedBind = skinned();
					log << "marked, bind pose: " << movedCount( before, markedBind )
						<< " vertices moved; override has "
						<< ( skope->ogl->getScene() ? skope->ogl->getScene()->skeletonOverride.size() : 0 )
						<< " entries, Chest at Z " << overrideZ( QStringLiteral( "Chest" ) ) << "\n";
					if ( Shape * s = skope->ogl->shapeForBlock( sb ) ) {
						QStringList hit, miss;
						Scene * ps = skope->ogl->getScene();
						for ( int i = 0; i < s->boneCount(); i++ ) {
							const QString bn = s->boneNameAt( i );
							( ( ps && ps->skeletonOverride.contains( bn ) ) ? hit : miss )
								<< ( bn.isEmpty() ? QStringLiteral( "<unnamed>" ) : bn );
						}
						log << "  shape bones matched [" << hit.join( QStringLiteral( ", " ) )
							<< "] missed [" << miss.join( QStringLiteral( ", " ) ) << "]\n";
						check( "every bone of the shape is present in the marked skeleton",
							miss.isEmpty() && !hit.isEmpty() );
					}

					/* Now POSE the marked skeleton. This is the real question: does the
					 * armour follow a skeleton that moves? A bind-pose comparison
					 * cannot answer it, because bind and bind are the same.
					 */
					/* Timed, because posing a MARKED skeleton is a continuous-edit
					 * workflow and the secondary-scene refresh sits on that path.
					 * Ten separate edit-and-repaint cycles, which is what dragging a
					 * bone looks like to the refresh code.
					 */
					/* No settle() in the timed loop.
					 *
					 * repaint() sleeps 120 ms to let things quiesce, so timing it
					 * measured that sleep and nothing else — the first attempt read
					 * "123 ms per edit" of which ~120 was the harness. An edit cycle
					 * is: change the model, let the deferred refresh run, paint.
					 */
					auto editCycle = [skope, &poseSkeleton]() {
						poseSkeleton( 3.0f );
						QApplication::processEvents();	// runs the coalesced flush
						skope->ogl->update();
						QApplication::processEvents();	// and the paint it scheduled
					};
					editCycle();		// warm: first touch builds caches
					QElapsedTimer poseClock;
					poseClock.start();
					for ( int k = 0; k < 10; k++ )
						editCycle();
					const qint64 tenCycles = poseClock.elapsed();
					repaint();
					log << "10 pose+repaint cycles: " << tenCycles << " ms ("
						<< ( tenCycles / 10 ) << " ms each)\n";
					/* 25 ms, not "generous".
					 *
					 * Rebuilding the secondary Scene on every edit measured 54 ms here
					 * and refreshing it in place measures 7. A loose threshold would
					 * pass both, so it would not be guarding anything — this one fails
					 * if the structural/value split is ever lost.
					 */
					check( "posing a marked skeleton keeps up (< 25 ms per edit)",
						tenCycles / 10 < 25 );
					const QVector<Vector3> markedPosed = skinned();
					const int followed = movedCount( markedBind, markedPosed );
					log << "marked, posed: " << followed << " of " << before.size()
						<< " vertices followed; Chest now at Z "
						<< overrideZ( QStringLiteral( "Chest" ) ) << "\n";
					check( "the armour follows the marked skeleton when it moves",
						followed > before.size() / 2 );

					// The row has to SAY it is the skeleton, or nothing explains why
					// every other file moved.
					const QString skelShot =
						QApplication::applicationDirPath() + "/ww_groupskel_list.png";
					check( "the Loaded NIFs list renders with the skeleton marked",
						skope->grabLoadedNifsView( skelShot ) );
					log << "  " << skelShot << "\n";

					// Unmark: everything goes back exactly, posed skeleton or not.
					check( "the skeleton can be unmarked",
						skope->setWorkspaceSkeletonDocument( -1 ) );
					repaint();
					const QVector<Vector3> restored = skinned();
					const int residue = movedCount( before, restored );
					log << "unmarked: " << residue << " vertex/vertices differ from the start\n";
					check( "unmarking restores the original evaluation exactly", residue == 0 );

					/* And with nothing marked, moving the skeleton again must do
					 * nothing at all — the requirement that a loaded file which merely
					 * contains a skeleton has no effect until it is marked.
					 */
					log << "posing " << poseSkeleton( 25.0f ) << " node(s) again, unmarked\n";
					repaint();
					const int strayMoved = movedCount( restored, skinned() );
					log << "unmarked and posed: " << strayMoved << " vertex/vertices moved\n";
					check( "posing an UNMARKED skeleton moves nothing", strayMoved == 0 );
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
				logf.close();
				if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
					n->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_SKELMERGE_TEST=<donor.nif>): does Load skeleton break the
	 * mesh, or only the posing that follows it?
	 *
	 * The string-index fix (07-30f) proved the merge was renaming imported bones.
	 * What it did NOT establish is which half of the reported symptom that caused,
	 * and a byte-diff argument ("existing blocks are untouched, so the skin cannot
	 * move") is a proof about the FILE, not about what NifSkope evaluates. This
	 * measures the evaluated skin instead: Shape::skinVertex for every vertex,
	 * before and after the merge, which is the thing the user actually looks at.
	 *
	 * Then it poses. A bone is resolved by name exactly as the Pose Manager's
	 * blockForName does — first NiAVObject with that name — rotated, and the set
	 * of vertices that moved is compared with the same rotation performed before
	 * the merge. Duplicate bone names would send the rotation elsewhere, so the
	 * two sets would differ; that is the failure this is looking for.
	 * Log: release/ww_skelmerge_test.log, screenshot ww_skelmerge_test.png.
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_SKELMERGE_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 1200, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_skelmerge_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				int checks = 0, fails = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass )
						fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }
					const QString donor = qEnvironmentVariable( "WW_SKELMERGE_TEST" );
					if ( !QFileInfo::exists( donor ) ) { log << "no donor: " << donor << "\n"; break; }

					int sb = -1, best = -1;
					for ( int b = 0; b < nif->getBlockCount(); b++ )
						if ( nif->blockInherits( nif->getBlockIndex( b ), "BSTriShape" ) ) {
							const int nv = nif->get<int>( nif->getBlockIndex( b ), "Num Vertices" );
							if ( nv > best ) { best = nv; sb = b; }
						}
					if ( sb < 0 ) { log << "no BSTriShape\n"; break; }

					// evaluated skin, exactly as the viewport draws it
					auto skinned = [skope, nif, sb]() {
						QVector<Vector3> out;
						Shape * s = skope->ogl->shapeForBlock( sb );
						if ( !s )
							return out;
						s->updateBoneTransforms();
						QModelIndex iVD = nif->getIndex( nif->getBlockIndex( sb ), "Vertex Data" );
						for ( int v = 0; v < nif->rowCount( iVD ); v++ )
							out.append( s->skinVertex( v,
								nif->get<Vector3>( nif->getIndex( iVD, v ), "Vertex" ) ) );
						return out;
					};
					auto movedSet = []( const QVector<Vector3> & a, const QVector<Vector3> & b ) {
						QSet<int> moved;
						for ( int i = 0; i < a.size() && i < b.size(); i++ )
							if ( ( a[i] - b[i] ).length() > 0.01f )
								moved.insert( i );
						return moved;
					};
					// the Pose Manager's own rule: first NiAVObject with that name
					auto blockForName = [nif]( const QString & name ) {
						for ( int b = 0; b < nif->getBlockCount(); b++ ) {
							QModelIndex i = nif->getBlockIndex( b );
							if ( nif->blockInherits( i, "NiAVObject" )
								&& nif->get<QString>( i, "Name" ) == name )
								return b;
						}
						return -1;
					};
					// rotate a bone 30 degrees about X and report what moved
					auto poseAndMeasure = [&]( const QString & bone, QSet<int> & moved, float & maxD ) {
						const int nb = blockForName( bone );
						moved.clear();
						maxD = 0.0f;
						if ( nb < 0 )
							return -1;
						const QVector<Vector3> before = skinned();
						QModelIndex iRot = nif->getIndex( nif->getBlockIndex( nb ), "Rotation" );
						const Matrix orig = nif->get<Matrix>( iRot );
						Matrix spin;
						spin.fromEuler( float( M_PI ) / 6.0f, 0.0f, 0.0f );
						nif->set<Matrix>( iRot, orig * spin );
						skope->ogl->update();
						QApplication::processEvents();
						const QVector<Vector3> after = skinned();
						moved = movedSet( before, after );
						for ( int i : std::as_const( moved ) )
							maxD = std::max( maxD, ( before[i] - after[i] ).length() );
						nif->set<Matrix>( iRot, orig );		// put it back
						skope->ogl->update();
						QApplication::processEvents();
						return nb;
					};

					const QStringList bones = { QStringLiteral( "LArm_ForeArm1" ),
						QStringLiteral( "LArm_Hand" ), QStringLiteral( "Chest" ) };
					QHash<QString, QSet<int>> movedBefore;
					QHash<QString, float> distBefore;
					for ( const QString & b : bones ) {
						QSet<int> m; float d = 0.0f;
						if ( poseAndMeasure( b, m, d ) >= 0 ) {
							movedBefore.insert( b, m );
							distBefore.insert( b, d );
							log << "  pre-merge  " << b << ": " << m.size() << " verts move, max "
								<< QString::number( d, 'f', 3 ) << "\n";
						} else {
							log << "  pre-merge  " << b << ": not in this file\n";
						}
					}

					const QVector<Vector3> bindBefore = skinned();
					log << "shape block " << sb << ", " << bindBefore.size() << " vertices\n";

					NifMergeResult r;
					const bool merged = nifMergeFile( nif, donor, true, r );
					log << "merge " << ( merged ? "ok" : "FAILED: " + r.error ) << ": +"
						<< r.blocksAdded << " blocks, " << r.nodesReused << " reused, "
						<< r.nodesAdded << " added, " << r.rebased << " rebased, "
						<< r.duplicateNames.size() << " dupes\n";
					if ( !merged )
						break;
					check( "the merge introduces no duplicate bone names", r.duplicateNames.isEmpty() );

					/* Let the viewport actually rebuild before measuring. A merge is
					 * a model edit; GLView reacts to it through queued signals and
					 * GLView::update() only schedules a repaint. Measuring straight
					 * after the call reads a Shape whose bone list belongs to the
					 * PREVIOUS scene, which moves vertices for a reason that has
					 * nothing to do with the merge. Pump until the scene settles.
					 */
					auto settle = []( int ms ) {
						QEventLoop loop;
						QTimer::singleShot( ms, &loop, &QEventLoop::quit );
						loop.exec();
						QApplication::processEvents();
					};
					skope->ogl->update();
					settle( 900 );
					const QVector<Vector3> bindAfter = skinned();
					const QSet<int> movedByMerge = movedSet( bindBefore, bindAfter );
					log << "  vertices moved by the merge alone: " << movedByMerge.size() << "\n";
					check( "loading a skeleton does not move the mesh", movedByMerge.isEmpty()
						&& bindAfter.size() == bindBefore.size() );

					for ( const QString & b : bones ) {
						if ( !movedBefore.contains( b ) )
							continue;
						QSet<int> m; float d = 0.0f;
						poseAndMeasure( b, m, d );
						log << "  post-merge " << b << ": " << m.size() << " verts move, max "
							<< QString::number( d, 'f', 3 ) << " (was " << movedBefore.value( b ).size()
							<< ", max " << QString::number( distBefore.value( b ), 'f', 3 ) << ")\n";
						/* A SUPERSET, not the same set. Before the merge every bone is
						 * a flat root child, so rotating one moves only the vertices
						 * weighted to it. After it, the bone has the skeleton's
						 * children, and rotating it drags the rest of the limb — which
						 * is the entire reason for loading a skeleton. What must still
						 * hold is that the bone drives its OWN vertices, and that it
						 * has not turned into a transform on the whole mesh.
						 */
						check( QStringLiteral( "posing %1 still drives its own vertices" ).arg( b ),
							m.contains( movedBefore.value( b ) ) );
						check( QStringLiteral( "...and not the entire mesh" ).arg( b ),
							m.size() < bindAfter.size() );
					}

					skope->ogl->update();
					QApplication::processEvents();
					const QString shot = QApplication::applicationDirPath() + "/ww_skelmerge_test.png";
					skope->ogl->grabFramebuffer().save( shot );
					log << "  screenshot: " << shot << "\n";
				} while ( false );
				log << checks << " checks, " << fails << " failures\n";
				log << ( fails == 0 ? "PASS\n" : "CHECK: failures above\n" );
				log << "done\n";
				logf.close();
				if ( nif && nif->undoStack )
					nif->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_SUMMARY_TEST=1): the Block List's Summary column.
	 *
	 * Read back through data() on the real columns, and through the hierarchy
	 * proxy as well, because the proxy translates its own column numbers into the
	 * model's and a summary that only works in one of the two list modes is a
	 * summary that is broken half the time.
	 *
	 * The status markers are checked in both directions: a texture path is
	 * mangled so its OWNING block must go red, then restored so it must go back.
	 * "unreferenced" is checked as an invariant over the whole file — a vanilla
	 * NIF has no orphan blocks, so any hit means the root-link exclusion is wrong,
	 * which is the one way this marker can be spectacularly noisy.
	 * Log: release/ww_summary_test.log.
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_SUMMARY_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 800, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_summary_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				int checks = 0, fails = 0, skips = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass )
						fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				auto skip = [&]( const QString & what, const QString & why ) {
					skips++;
					log << "  skip " << what << " — " << why << "\n";
				};
				auto summaryOf = [nif]( int b ) {
					return nif->data( nif->getBlockIndex( b ).sibling(
						nif->getBlockIndex( b ).row(), NifModel::WwSummaryCol ), Qt::DisplayRole ).toString();
				};
				auto isRed = [nif]( int b ) {
					const QVariant fg = nif->data( nif->getBlockIndex( b ).sibling(
						nif->getBlockIndex( b ).row(), NifModel::WwSummaryCol ), Qt::ForegroundRole );
					return fg.canConvert<QColor>() && fg.value<QColor>().red() > 200
						&& fg.value<QColor>().green() < 160;
				};
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }

					// ---- the column is where it should be ----
					const bool listIsProxy = ( skope->list->model() != nif );
					log << "  tree: " << skope->tree->header()->count() << " sections, summary hidden "
						<< skope->tree->isColumnHidden( NifModel::WwSummaryCol ) << "\n";
					log << "  list: " << ( listIsProxy ? "proxy" : "nif" ) << ", "
						<< skope->list->header()->count() << " sections, summary hidden "
						<< skope->list->isColumnHidden( listIsProxy ? 2 : int( NifModel::WwSummaryCol ) ) << "\n";
					check( "Block Details hides the Summary column",
						skope->tree->isColumnHidden( NifModel::WwSummaryCol ) );
					check( "the Block List shows it",
						!skope->list->isColumnHidden( listIsProxy ? 2 : int( NifModel::WwSummaryCol ) ) );
					check( "the header reads Summary",
						nif->headerData( NifModel::WwSummaryCol, Qt::Horizontal,
							Qt::DisplayRole ).toString() == QStringLiteral( "Summary" ) );

					// ---- fields have no summary; blocks do ----
					{
						QModelIndex root = nif->getBlockIndex( 0 );
						QModelIndex field = nif->getIndex( root, 0 );
						check( "a FIELD row has no summary",
							!field.isValid() || nif->data( field.sibling( field.row(),
								NifModel::WwSummaryCol ), Qt::DisplayRole ).toString().isEmpty() );
					}

					// ---- per-type content ----
					int shape = -1, texSet = -1, ctrl = -1;
					for ( int b = 0; b < nif->getBlockCount(); b++ ) {
						QModelIndex ib = nif->getBlockIndex( b );
						if ( shape < 0 && nif->blockInherits( ib, "BSTriShape" ) )
							shape = b;
						if ( texSet < 0 && nif->isNiBlock( ib, "BSShaderTextureSet" ) )
							texSet = b;
						if ( ctrl < 0 && nif->blockInherits( ib, "NiTimeController" ) )
							ctrl = b;
					}
					if ( shape < 0 ) {
						skip( "a shape summarises its counts", "no BSTriShape" );
					} else {
						const QString s = summaryOf( shape );
						log << "  shape #" << shape << ": " << s << "\n";
						check( "a shape summarises its counts",
							s.contains( QStringLiteral( "tris" ) ) && s.contains( QStringLiteral( "verts" ) ) );
					}
					if ( ctrl < 0 ) {
						skip( "a controller names its target", "no NiTimeController" );
					} else {
						const QString s = summaryOf( ctrl );
						log << "  controller #" << ctrl << ": " << s << "\n";
						check( "a controller names its target", s.contains( QStringLiteral( "→" ) ) );
					}

					// ---- status markers, both directions ----
					if ( texSet < 0 ) {
						skip( "a mangled path marks its block", "no BSShaderTextureSet" );
					} else {
						QModelIndex iTex = nif->getIndex( nif->getBlockIndex( texSet ), "Textures" );
						int slot = -1;
						for ( int s = 0; s < nif->rowCount( iTex ) && slot < 0; s++ )
							if ( !nif->get<QString>( nif->getIndex( iTex, s ) ).trimmed().isEmpty() )
								slot = s;
						const QString before = summaryOf( texSet );
						const bool redBefore = isRed( texSet );
						log << "  texture set #" << texSet << ": " << before
							<< ( redBefore ? "   [red]" : "" ) << "\n";
						if ( slot < 0 ) {
							skip( "a mangled path marks its block", "every slot is empty" );
						} else {
							// both separators, because NIFs mix them and the summary
							// strips whichever one the file happens to use
							check( "a texture set names its diffuse",
								before.contains( nif->get<QString>( nif->getIndex( iTex, slot ) )
									.section( QLatin1Char( '\\' ), -1 ).section( QLatin1Char( '/' ), -1 ) ) );
							const QString orig = nif->get<QString>( nif->getIndex( iTex, slot ) );
							nif->set<QString>( nif->getIndex( iTex, slot ),
								QStringLiteral( "textures\\ww_no_such_texture_9f3a.dds" ) );
							check( "a mangled path marks its block red", isRed( texSet ) );
							check( "...and says which fault it is",
								summaryOf( texSet ).contains( QStringLiteral( "missing texture" ) ) );
							nif->set<QString>( nif->getIndex( iTex, slot ), orig );
							check( "restoring it clears the marker", isRed( texSet ) == redBefore );
							check( "...and the summary", summaryOf( texSet ) == before );
						}
					}

					// ---- unreferenced must be silent on a well-formed file ----
					{
						QStringList orphans;
						for ( int b = 0; b < nif->getBlockCount(); b++ )
							if ( summaryOf( b ).contains( QStringLiteral( "unreferenced" ) ) )
								orphans << QString::number( b );
						if ( !orphans.isEmpty() )
							log << "  orphans: " << orphans.join( QStringLiteral( ", " ) ) << "\n";
						check( "no block in a vanilla file reads as unreferenced", orphans.isEmpty() );
					}

					// ---- the hierarchy proxy maps its third column ----
					if ( !skope->proxy ) {
						skip( "the hierarchy view maps column 2", "no proxy model" );
					} else {
						check( "the proxy offers three columns",
							skope->proxy->columnCount( QModelIndex() ) == 3 );
						check( "...and its third header is Summary",
							skope->proxy->headerData( 2, Qt::Horizontal, Qt::DisplayRole )
								.toString() == QStringLiteral( "Summary" ) );
						QModelIndex proot = skope->proxy->index( 0, 2, QModelIndex() );
						check( "...and its third column is the model's summary",
							proot.isValid()
							&& skope->proxy->mapTo( proot ).column() == NifModel::WwSummaryCol );
					}

					// A column that reads correctly through data() can still be two
					// pixels wide, or off the right edge, on screen. grab() renders
					// the widget offscreen, so the picture costs nothing and steals
					// no focus. The sections are squeezed to fit the dock's real
					// width first — a screenshot of the columns you cannot see is
					// not evidence about the one you are checking.
					skope->list->expandAll();
					skope->list->scrollToTop();
					{
						QHeaderView * h = skope->list->header();
						const int w = skope->list->viewport()->width();
						h->resizeSection( 0, int( w * 0.34 ) );
						h->resizeSection( 1, int( w * 0.22 ) );
						h->resizeSection( 2, w - int( w * 0.34 ) - int( w * 0.22 ) - 4 );
					}
					const QString shot = QApplication::applicationDirPath() + "/ww_summary_test.png";
					check( "the block list renders", skope->list->grab().save( shot ) );
					log << "  screenshot: " << shot << "\n";
				} while ( false );
				log << checks << " checks, " << fails << " failures, " << skips << " skips\n";
				log << ( fails == 0 ? "PASS\n" : "CHECK: failures above\n" );
				log << "done\n";
				logf.close();
				if ( nif && nif->undoStack )
					nif->undoStack->setClean();
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	/* TEST HARNESS (WW_TEXCOLOR_TEST=1): the two Block Details typed editors.
	 *
	 * Texture paths: NifModel::texturePathInfo drives both the Value column's
	 * colour and its tooltip, so the two cannot disagree — the harness reads them
	 * back through data() exactly as the delegate does, rather than calling the
	 * helper directly. A path is then MANGLED in place: whatever the machine's
	 * configured resources happen to contain, a garbage path must resolve nowhere
	 * and must go red, which makes the negative case machine-independent. Whether
	 * the ORIGINAL paths resolve depends on whether FO4's data is configured here,
	 * so that check reports a skip rather than a failure when nothing resolves.
	 *
	 * Colour: a real ColorEdit is built and driven. The swatch's job is to open
	 * the picker, so the click is made with a timer waiting to dismiss the modal
	 * dialog — that proves the wiring end to end. What it cannot prove is the HDR
	 * scale round trip, which needs a colour CHANGED inside the dialog.
	 * Log: release/ww_texcolor_test.log.
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_TEXCOLOR_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 800, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_texcolor_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				int checks = 0, fails = 0, skips = 0;
				auto check = [&]( const QString & what, bool pass ) {
					checks++;
					if ( !pass )
						fails++;
					log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
				};
				auto skip = [&]( const QString & what, const QString & why ) {
					skips++;
					log << "  skip " << what << " — " << why << "\n";
				};
				auto isRed = [nif]( const QModelIndex & idx ) {
					const QVariant fg = nif->data( idx, Qt::ForegroundRole );
					return fg.canConvert<QColor>() && fg.value<QColor>().red() > 200
						&& fg.value<QColor>().green() < 160;
				};
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }

					// ---- texture paths ----
					QModelIndex iSet;
					for ( int b = 0; b < nif->getBlockCount() && !iSet.isValid(); b++ )
						if ( nif->isNiBlock( nif->getBlockIndex( b ), "BSShaderTextureSet" ) )
							iSet = nif->getBlockIndex( b );
					if ( !iSet.isValid() ) {
						log << "no BSShaderTextureSet in this file\n";
					} else {
						QModelIndex iTex = nif->getIndex( iSet, "Textures" );
						const int n = nif->rowCount( iTex );
						int filled = 0, resolved = 0, firstFilled = -1;
						for ( int s = 0; s < n; s++ ) {
							const QString path = nif->get<QString>( nif->getIndex( iTex, s ) );
							if ( path.trimmed().isEmpty() )
								continue;
							filled++;
							if ( firstFilled < 0 )
								firstFilled = s;
							QString p, r;
							if ( nif->texturePathInfo( nif->getItem( nif->getIndex( iTex, s ) ), p, r ) && !r.isEmpty() )
								resolved++;
							log << "  slot " << s << ": " << path << ( r.isEmpty() ? "  [missing]" : "  [found]" ) << "\n";
						}
						log << "texture set block " << nif->getBlockNumber( iSet ) << ": "
							<< n << " slots, " << filled << " filled, " << resolved << " resolve\n";
						check( "every slot is recognised as a texture path", [&]() {
							for ( int s = 0; s < n; s++ ) {
								QString p, r;
								if ( !nif->texturePathInfo( nif->getItem( nif->getIndex( iTex, s ) ), p, r ) )
									return false;
							}
							return n > 0;
						}() );
						if ( firstFilled < 0 ) {
							skip( "mangled path goes red", "every slot is empty" );
						} else {
							const QModelIndex iVal = nif->getIndex( iTex, firstFilled ).sibling( firstFilled, NifModel::ValueCol );
							const QString orig = nif->get<QString>( nif->getIndex( iTex, firstFilled ) );
							const bool origRed = isRed( iVal );
							const QString origTip = nif->data( iVal, Qt::ToolTipRole ).toString();

							nif->set<QString>( nif->getIndex( iTex, firstFilled ),
								QStringLiteral( "textures\\ww_no_such_texture_9f3a.dds" ) );
							check( "a path that resolves nowhere is drawn red", isRed( iVal ) );
							check( "...and says so in its tooltip",
								nif->data( iVal, Qt::ToolTipRole ).toString().contains( QStringLiteral( "Not found" ) ) );

							nif->set<QString>( nif->getIndex( iTex, firstFilled ),
								QStringLiteral( "" ) );
							check( "an EMPTY slot is not marked broken", !isRed( iVal ) );
							check( "...and reads as an empty slot",
								nif->data( iVal, Qt::ToolTipRole ).toString().contains( QStringLiteral( "Empty" ) ) );

							nif->set<QString>( nif->getIndex( iTex, firstFilled ), orig );
							if ( resolved > 0 && !origRed ) {
								check( "a resolvable path is left alone", !isRed( iVal ) );
								check( "...and its tooltip names the file",
									nif->data( iVal, Qt::ToolTipRole ).toString().contains( orig.section( '\\', -1 ) ) );
							} else {
								skip( "resolvable path is left alone",
									QStringLiteral( "no configured resources on this machine" ) );
							}
							check( "restoring the path restores the marking",
								isRed( iVal ) == origRed );
							check( "...and the tooltip", nif->data( iVal, Qt::ToolTipRole ).toString() == origTip );
						}
					}
					// a plain string field must NOT be treated as a texture path —
					// and, retyped as one, must read its value out of the HEADER
					// STRING TABLE rather than off the item. That second branch is
					// what a Fallout 3 / Skyrim NiSourceTexture "File Name" takes,
					// and no file on this machine has one, so it is reached here by
					// retyping a NiFixedString field in place and putting it back.
					{
						QModelIndex iNode;
						for ( int b = 0; b < nif->getBlockCount() && !iNode.isValid(); b++ )
							if ( nif->getIndex( nif->getBlockIndex( b ), "Name" ).isValid() )
								iNode = nif->getIndex( nif->getBlockIndex( b ), "Name" );
						if ( !iNode.isValid() ) {
							skip( "a plain string is not a texture path", "no named block" );
						} else {
							QString p, r;
							check( "a block Name is not a texture path",
								!nif->texturePathInfo( nif->getItem( iNode ), p, r ) );
							// the Value column: NifModel::data only ever returns the
							// broken-path red for ValueCol, so asking the Name index
							// (column 0) could never fail whatever the path said
							check( "...so it is never marked red",
								!isRed( iNode.sibling( iNode.row(), NifModel::ValueCol ) ) );

							NifItem * nameItem = nif->getItem( iNode );
							const QString wasType = nameItem->strType();
							const QString expect = nif->get<QString>( iNode );
							if ( nameItem->valueType() != NifValue::tStringIndex || expect.isEmpty() ) {
								skip( "FilePath reads the header string table",
									QStringLiteral( "no string-indexed name in this file" ) );
							} else {
								nameItem->setStrType( QStringLiteral( "FilePath" ) );
								QString fp, fr;
								const bool got = nif->texturePathInfo( nameItem, fp, fr );
								nameItem->setStrType( wasType );
								check( "a FilePath field is a texture path", got );
								check( "...and its value comes from the header string table", fp == expect );
								check( "...and a node name resolves to no texture", fr.isEmpty() );
							}
						}
					}

					// ---- colour swatch ----
					{
						ColorEdit * ce = new ColorEdit( skope );
						ce->setColor4( Color4( 0.25f, 0.5f, 0.75f, 0.5f ) );
						const Color4 got = ce->getColor4();
						check( "ColorEdit round-trips a Color4",
							std::abs( got[0] - 0.25f ) < 1e-3f && std::abs( got[1] - 0.5f ) < 1e-3f
							&& std::abs( got[2] - 0.75f ) < 1e-3f && std::abs( got[3] - 0.5f ) < 1e-3f );

						QList<QAbstractButton *> btns = ce->findChildren<QAbstractButton *>();
						check( "the editor has exactly one swatch button", btns.size() == 1 );
						if ( !btns.isEmpty() ) {
							QAbstractButton * sw = btns.first();
							// HDR: the swatch must accept a channel above 1.0 without
							// clamping the VALUE (only its drawn colour is normalised)
							ce->setColor4( Color4( 4.0f, 2.0f, 1.0f, 1.0f ) );
							const Color4 hdr = ce->getColor4();
							check( "an HDR colour survives the swatch",
								std::abs( hdr[0] - 4.0f ) < 1e-3f && std::abs( hdr[1] - 2.0f ) < 1e-3f );

							ce->setColor3( Color3( 1.0f, 0.0f, 0.0f ) );
							check( "setColor3 hides the alpha box",
								ce->findChildren<QDoubleSpinBox *>().size() == 4
								&& ce->findChildren<QDoubleSpinBox *>().at( 3 )->isHidden() );

							// clicking must open the picker: dismiss whatever modal
							// dialog appears and confirm one appeared at all
							auto sawDialog = std::make_shared<bool>( false );
							QTimer driver;
							QObject::connect( &driver, &QTimer::timeout, skope, [sawDialog]() {
								if ( QWidget * w = QApplication::activeModalWidget() ) {
									*sawDialog = true;
									if ( auto * d = qobject_cast<QDialog *>( w ) )
										d->reject();
									else
										w->close();
								}
							} );
							driver.start( 120 );
							sw->click();
							driver.stop();
							check( "clicking the swatch opens the colour picker", *sawDialog );
							const Color3 after = ce->getColor3();
							check( "dismissing it leaves the colour alone",
								std::abs( after[0] - 1.0f ) < 1e-3f && std::abs( after[1] ) < 1e-3f );
						}
						delete ce;
					}
				} while ( false );
				log << checks << " checks, " << fails << " failures, " << skips << " skips\n";
				log << ( fails == 0 ? "PASS\n" : "CHECK: failures above\n" );
				log << "done\n";
				logf.close();
				// leave the file as it was found, so no save prompt blocks the quit
				if ( nif && nif->undoStack ) {
					nif->undoStack->setClean();
				}
				skope->setWindowModified( false );
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEST HARNESS (WW_SEP_TEST=1): exercise skin/segment-aware Separate (P) on an
	// FO4 skinned segmented mesh. Edit-selects the first half of the largest
	// BSSubIndexTriShape's faces, runs GLView::separateSelection(), and verifies on
	// BOTH resulting shapes:
	//   - they reference DIFFERENT BSSkin::Instance and BoneData blocks (the clone
	//     was given its OWN skin, not left sharing the original's), while the source
	//     keeps its original skin,
	//   - each shape's segments cover [0, Num Triangles) with Sum(Num Primitives)
	//     == its Num Triangles (ranges rebuilt for the new subset),
	//   - source tris + new tris == the original triangle count (nothing lost),
	//   - all verts retained on both, no per-vertex weight zeroed.
	// Saves (WW_SEP_SAVE) and quits. Log: release/ww_sep_test.log.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_SEP_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 800, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_sep_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				auto skinData = [nif]( int block ) {
					int inst = nif->getLink( nif->getBlockIndex( block ), "Skin" );
					int data = inst >= 0 ? nif->getLink( nif->getBlockIndex( inst ), "Data" ) : -1;
					return QPair<int, int>( inst, data );
				};
				// numSeg = -1 means "this shape has no segment array at all" (a plain
				// BSTriShape), which is not a failure — the caller skips the coverage
				// check rather than treating an absent structure as a broken one.
				auto segCoverage = [nif]( int block, int & sumPrim, bool & contiguous, int & numSeg ) {
					QModelIndex iB = nif->getBlockIndex( block );
					QModelIndex iSeg = nif->getIndex( iB, "Segment" );
					if ( !iSeg.isValid() ) { numSeg = -1; sumPrim = -1; contiguous = true; return; }
					numSeg = nif->get<int>( iB, "Num Segments" );
					QVector<QPair<int, int>> segList;
					for ( int s = 0; s < nif->rowCount( iSeg ); s++ )
						segList.append( { int( nif->get<quint32>( nif->getIndex( iSeg, s ), "Start Index" ) ),
						                  int( nif->get<quint32>( nif->getIndex( iSeg, s ), "Num Primitives" ) ) } );
					std::sort( segList.begin(), segList.end() );
					sumPrim = 0; contiguous = true; int expectStart = 0;
					for ( const auto & pr : segList ) {
						if ( pr.second == 0 ) continue;
						if ( pr.first != expectStart ) contiguous = false;
						expectStart += pr.second * 3; sumPrim += pr.second;
					}
				};
				// -1 means "unskinned", which is not zero weights — an unskinned shape
				// has no Bone Weights array and would otherwise count every vertex.
				auto zeroWeights = [nif]( int block ) {
					int zero = 0;
					if ( nif->getLink( nif->getBlockIndex( block ), "Skin" ) < 0 )
						return -1;
					QModelIndex iVD = nif->getIndex( nif->getBlockIndex( block ), "Vertex Data" );
					for ( int v = 0; v < nif->rowCount( iVD ); v++ ) {
						QModelIndex iW = nif->getIndex( nif->getIndex( iVD, v ), "Bone Weights" );
						float s = 0;
						for ( int k = 0; k < nif->rowCount( iW ); k++ )
							s += nif->get<float>( nif->getIndex( iW, k ) );
						if ( s < 1e-4f ) zero++;
					}
					return zero;
				};
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }
					// WW_SEP_BLOCK targets a specific shape (e.g. a coloured one to
					// exercise the vertex-colour path); otherwise the largest.
					// WW_SEP_ANY widens the search from skinned/segmented FO4 meshes to
					// any BSTriShape — most files people run Separate on are plain ones,
					// and the skin and segment checks below now skip themselves rather
					// than failing when the structures are absent.
					const char * wantType = qEnvironmentVariableIsSet( "WW_SEP_ANY" )
						? "BSTriShape" : "BSSubIndexTriShape";
					int sb = -1, sbVerts = -1;
					if ( qEnvironmentVariableIsSet( "WW_SEP_BLOCK" ) ) {
						int req = QString::fromLocal8Bit( qgetenv( "WW_SEP_BLOCK" ) ).toInt();
						if ( req >= 0 && req < nif->getBlockCount()
							&& nif->blockInherits( nif->getBlockIndex( req ), wantType ) )
							sb = req;
						else
							log << "WW_SEP_BLOCK " << req << " is not a " << wantType << "; using largest\n";
					}
					if ( sb < 0 )
						for ( int b = 0; b < nif->getBlockCount(); b++ )
							if ( nif->blockInherits( nif->getBlockIndex( b ), wantType ) ) {
								int nv = nif->get<int>( nif->getBlockIndex( b ), "Num Vertices" );
								if ( nv > sbVerts ) { sbVerts = nv; sb = b; }
							}
					if ( sb < 0 ) { log << "no " << wantType << "\n"; break; }
					const int origTris = nif->get<int>( nif->getBlockIndex( sb ), "Num Triangles" );
					const int origVerts = nif->get<int>( nif->getBlockIndex( sb ), "Num Vertices" );
					const int origBlocks = nif->getBlockCount();
					const QPair<int, int> srcSkinBefore = skinData( sb );
					// snapshot original geometry + colours so we can prove the split
					// preserved them after the orphan-vertex compaction reindexes
					// everything: canonical triangle position-triples (catches a bad
					// remap that scrambles the mesh) and (position, RGBA) pairs (vertex
					// alpha is the A channel).
					const bool srcHasColor =
						( ( nif->get<BSVertexDesc>( nif->getBlockIndex( sb ), "Vertex Desc" ).Value() >> 44 ) & 0x20 ) != 0;
					auto posKey = []( const Vector3 & p ) {
						return QString::asprintf( "%.4f_%.4f_%.4f", p[0], p[1], p[2] );
					};
					auto colorKey = []( const ByteColor4 & c ) {
						return QString::asprintf( "%.3f,%.3f,%.3f,%.3f", c.red(), c.green(), c.blue(), c.alpha() );
					};
					QVector<Vector3> origPos;
					QSet<QString> origTriPos, origPosColor;
					{
						QModelIndex iVD = nif->getIndex( nif->getBlockIndex( sb ), "Vertex Data" );
						QModelIndex iT = nif->getIndex( nif->getBlockIndex( sb ), "Triangles" );
						for ( int v = 0; v < nif->rowCount( iVD ); v++ ) {
							Vector3 p = nif->get<Vector3>( nif->getIndex( iVD, v ), "Vertex" );
							origPos.append( p );
							if ( srcHasColor )
								origPosColor.insert( posKey( p ) + "#"
									+ colorKey( nif->get<ByteColor4>( nif->getIndex( iVD, v ), "Vertex Colors" ) ) );
						}
						for ( int t = 0; t < nif->rowCount( iT ); t++ ) {
							Triangle tri = nif->get<Triangle>( nif->getIndex( iT, t ) );
							QStringList k;
							for ( int c = 0; c < 3; c++ )
								k << ( tri[c] < origPos.size() ? posKey( origPos[tri[c]] ) : QString() );
							k.sort();
							origTriPos.insert( k.join( QLatin1Char( '|' ) ) );
						}
					}
					// validate a resulting shape: geometry (every triangle's position-
					// triple exists in the original), no orphan verts remain, and every
					// vertex's (position, colour) is an original pair.
					auto validateShape = [&]( int block, bool & geomOk, bool & noOrphan, bool & colorOk, int & vc ) {
						QModelIndex iVD = nif->getIndex( nif->getBlockIndex( block ), "Vertex Data" );
						QModelIndex iT = nif->getIndex( nif->getBlockIndex( block ), "Triangles" );
						vc = nif->rowCount( iVD );
						QVector<Vector3> pos;
						for ( int v = 0; v < vc; v++ )
							pos.append( nif->get<Vector3>( nif->getIndex( iVD, v ), "Vertex" ) );
						QVector<bool> used( vc, false );
						geomOk = true;
						for ( int t = 0; t < nif->rowCount( iT ); t++ ) {
							Triangle tri = nif->get<Triangle>( nif->getIndex( iT, t ) );
							QStringList k;
							bool inRange = true;
							for ( int c = 0; c < 3; c++ ) {
								if ( tri[c] >= vc ) { inRange = false; continue; }
								used[tri[c]] = true;
								k << posKey( pos[tri[c]] );
							}
							k.sort();
							if ( !inRange || !origTriPos.contains( k.join( QLatin1Char( '|' ) ) ) )
								geomOk = false;
						}
						noOrphan = true;
						for ( int v = 0; v < vc; v++ )
							if ( !used[v] ) noOrphan = false;
						colorOk = true;
						if ( srcHasColor )
							for ( int v = 0; v < vc; v++ ) {
								QString key = posKey( pos[v] ) + "#"
									+ colorKey( nif->get<ByteColor4>( nif->getIndex( iVD, v ), "Vertex Colors" ) );
								if ( !origPosColor.contains( key ) ) colorOk = false;
							}
					};
					log << "source block " << sb << " verts " << origVerts << " tris " << origTris
						<< " colors " << ( srcHasColor ? "Y" : "n" )
						<< " skin(inst " << srcSkinBefore.first << ", data " << srcSkinBefore.second << ")\n";
					log.flush();

					// WW_SEP_MODE: 0 = Selection (default), 1 = By Loose Parts, 2 = By
					// Segment. The last two split N ways, so everything below validates
					// a LIST of outputs rather than a source/new pair.
					const int sepMode = qEnvironmentVariableIsSet( "WW_SEP_MODE" )
						? QString::fromLocal8Bit( qgetenv( "WW_SEP_MODE" ) ).toInt() : 0;

					// build the scene, enter edit mode, select the first half of faces
					skope->ogl->grabFramebuffer();
					QSet<int> objSel; objSel.insert( sb );
					skope->ogl->setObjectSelection( objSel, sb );
					skope->ogl->setEditMode( true );
					log << "editMode=" << skope->ogl->editModeActive() << " sepMode=" << sepMode << "\n";
					int preview = -1;
					if ( sepMode == 0 ) {
						QVector<GLView::PickedElement> faces;
						const int half = origTris / 2;
						for ( int t = 0; t < half; t++ ) {
							GLView::PickedElement pe;
							if ( skope->ogl->buildFacePick( sb, t, pe ) )
								faces.append( pe );
						}
						log << "selected " << faces.size() << " of " << origTris << " faces\n";
						log.flush();
						skope->ogl->setElementSelectionExternal( sb, faces, 3 );
						skope->ogl->separateSelection();
					} else {
						// the P menu shows this count before the click; it must match what
						// the operator then produces, or the label is lying
						preview = skope->ogl->separateGroupsPreview( sepMode == 1 );
						log << "menu preview: +" << preview << " new object(s)\n";
						log.flush();
						skope->ogl->separateByGroups( sepMode == 1 );
					}

					QVector<int> outs;			// [0] is the source, then every new shape
					outs.append( sb );
					for ( int b = origBlocks; b < nif->getBlockCount(); b++ )
						if ( nif->blockInherits( nif->getBlockIndex( b ), wantType ) )
							outs.append( b );
					if ( outs.size() < 2 ) { log << "no separated shape produced\n"; break; }
					log << "outputs: " << outs.size() << " shape(s), source + "
						<< ( outs.size() - 1 ) << " new\n";

					const bool srcSkinned = ( srcSkinBefore.first >= 0 );
					int sumTris = 0;
					bool skinDistinct = ( skinData( sb ).first == srcSkinBefore.first );
					bool segOk = true, weightsOk = true, geomOk = true, colorOk = true, trimmed = true;
					QSet<int> skinInsts, skinDatas;
					QVector<QSet<QString>> shapePos( outs.size() );	// for the loose-part check
					for ( int i = 0; i < outs.size(); i++ ) {
						const int b = outs[i];
						const int tris = nif->get<int>( nif->getBlockIndex( b ), "Num Triangles" );
						const QPair<int, int> skin = skinData( b );
						sumTris += tris;
						if ( i > 0 && srcSkinned ) {
							// each split-off shape needs its OWN skin, not a shared one
							if ( skin.first < 0 || skin.second < 0
								|| skinInsts.contains( skin.first ) || skinDatas.contains( skin.second ) )
								skinDistinct = false;
						}
						skinInsts.insert( skin.first );
						skinDatas.insert( skin.second );

						int sum, nseg; bool cont;
						segCoverage( b, sum, cont, nseg );
						if ( nseg >= 0 && ( sum != tris || !cont || nseg == 0 ) )
							segOk = false;
						const int zero = zeroWeights( b );
						if ( zero > 0 )
							weightsOk = false;
						bool g, noOrphan, c; int vc = 0;
						validateShape( b, g, noOrphan, c, vc );
						if ( !g || !noOrphan )
							geomOk = false;
						if ( !c )
							colorOk = false;
						if ( vc >= origVerts )
							trimmed = false;	// orphan verts should have been dropped
						{
							QModelIndex iVD = nif->getIndex( nif->getBlockIndex( b ), "Vertex Data" );
							for ( int v = 0; v < nif->rowCount( iVD ); v++ )
								shapePos[i].insert( posKey( nif->get<Vector3>( nif->getIndex( iVD, v ), "Vertex" ) ) );
						}
						log << ( i ? "  new  " : "  src  " ) << "block " << b << ": tris " << tris
							<< " verts " << vc << " skin(inst " << skin.first << ", data " << skin.second
							<< ") segs " << nseg << " sumPrim " << sum << " contiguous " << cont
							<< " zeroW " << zero << " geomOk " << g << " noOrphan " << noOrphan
							<< " colorOk " << c << "\n";
					}
					const bool trisConserved = ( sumTris == origTris );
					log << "triangles " << sumTris << " of " << origTris
						<< ", colours " << ( srcHasColor ? "present" : "n/a" ) << "\n";

					// mode-specific invariant. By Loose Parts means exactly this: no two
					// output shapes touch, i.e. they share no vertex POSITION. By Segment
					// means each output carries exactly one non-empty segment.
					bool modeOk = true;
					if ( sepMode == 1 ) {
						for ( int i = 0; i < outs.size() && modeOk; i++ )
							for ( int j = i + 1; j < outs.size() && modeOk; j++ )
								for ( const QString & p : std::as_const( shapePos[i] ) )
									if ( shapePos[j].contains( p ) ) {
										log << "shapes " << outs[i] << " and " << outs[j]
											<< " share position " << p << "\n";
										modeOk = false;
										break;
									}
						log << "disjoint pieces=" << modeOk << " previewMatched="
							<< ( preview == outs.size() - 1 ) << "\n";
					} else if ( sepMode == 2 ) {
						for ( int b : std::as_const( outs ) ) {
							QModelIndex iSeg = nif->getIndex( nif->getBlockIndex( b ), "Segment" );
							if ( !iSeg.isValid() )
								continue;
							int nonEmpty = 0;
							for ( int s = 0; s < nif->rowCount( iSeg ); s++ )
								if ( nif->get<quint32>( nif->getIndex( iSeg, s ), "Num Primitives" ) > 0 )
									nonEmpty++;
							if ( nonEmpty != 1 ) {
								log << "shape " << b << " has " << nonEmpty << " non-empty segments (expect 1)\n";
								modeOk = false;
							}
						}
						log << "onePerSegment=" << modeOk << " previewMatched="
							<< ( preview == outs.size() - 1 ) << "\n";
					}
					if ( sepMode != 0 && preview != outs.size() - 1 )
						modeOk = false;

					const bool pass = skinDistinct && trisConserved && segOk && weightsOk
						&& geomOk && colorOk && trimmed && modeOk;
					log << "skinDistinct=" << skinDistinct << " trisConserved=" << trisConserved
						<< " segOk=" << segOk << " weightsOk=" << weightsOk << " geomOk=" << geomOk
						<< " colorOk=" << colorOk << " trimmed=" << trimmed << " modeOk=" << modeOk << "\n";
					log << ( pass ? "PASS\n" : "CHECK: one or more invariants failed\n" );

					if ( qEnvironmentVariableIsSet( "WW_SEP_SAVE" ) ) {
						QString out = qEnvironmentVariable( "WW_SEP_SAVE" );
						log << "save ok " << nif->saveToFile( out ) << ": " << out << "\n";
					}

					// Undo (Ctrl+Z) must fully restore the source: verts, triangles,
					// segment ranges, AND drop the appended clone/skin blocks.
					if ( nif->undoStack ) {
						nif->undoStack->undo();
						const int uTris = nif->get<int>( nif->getBlockIndex( sb ), "Num Triangles" );
						const int uVerts = nif->get<int>( nif->getBlockIndex( sb ), "Num Vertices" );
						int uSum, uSeg; bool uCont;
						segCoverage( sb, uSum, uCont, uSeg );
						bool uGeom, uNoOrphan, uColor; int uVC;
						validateShape( sb, uGeom, uNoOrphan, uColor, uVC );
						const int uZero = zeroWeights( sb );	// catches the grow-back skin-array landmine
						const int uBlocks = nif->getBlockCount();
						const bool undoOk = uTris == origTris && uVerts == origVerts
							&& ( uSeg < 0 || ( uSum == origTris && uCont ) ) && uBlocks == origBlocks
							&& uGeom && uColor && uZero <= 0;
						log << "after undo: tris " << uTris << " (expect " << origTris << ") verts " << uVerts
							<< " (expect " << origVerts << ") segSum " << uSum << " contiguous " << uCont
							<< " blocks " << uBlocks << " geomOk " << uGeom << " colorOk " << uColor
							<< " zeroW " << uZero << "\n";
						log << ( undoOk ? "UNDO PASS\n" : "UNDO CHECK: source not fully restored\n" );
					}
				} while ( false );
				log << "done\n";
				logf.close();
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEST HARNESS (WW_WP_TEST=1): verify the viewport Weight Paint selector
	// auto-acquires a target. Loads with NOTHING selected, triggers the
	// ViewportWeightPaintAction, and checks the paint mode actually engaged (the
	// old handler silently reverted to Object Mode when no mesh was pre-selected).
	// Log: release/ww_wp_test.log.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_WP_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			QTimer::singleShot( 800, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_wp_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }
					skope->ogl->grabFramebuffer();	// build the scene; leave nothing selected
					log << "objActive before=" << skope->ogl->activeObjectBlock() << "\n";
					QAction * wp = skope->findChild<QAction *>( QStringLiteral( "ViewportWeightPaintAction" ) );
					if ( !wp ) { log << "no ViewportWeightPaintAction\n"; break; }
					wp->trigger();
					const bool active = skope->ogl->riggingWeightPaintModeActive();
					log << "weightPaintModeActive=" << active << "\n";
					log << ( active ? "PASS\n" : "CHECK: weight paint did not engage on auto-acquire\n" );
				} while ( false );
				log << "done\n";
				logf.close();
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	// TEMP DIAGNOSTIC (WW_UI_SHOT=1): after the file loads and layout settles,
	// grab the whole main window (toolbars + docks) to ww_ui_shot.png and quit
	// — used to eyeball toolbar/menu changes offscreen (run with
	// -platform offscreen so nothing pops onto the desktop).
	// No fname requirement: the starter document emits completeLoading too, so
	// this now works on the startup scene, which is the only way to eyeball the
	// docks in that state.
	if ( qEnvironmentVariableIsSet( "WW_UI_SHOT" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool, QString & ) {
			QTimer::singleShot( 2500, skope, [skope]() {
				// WW_UI_SHOT_DOCK=<objectName> opens that dock before the grab.
				const QByteArray dockName = qgetenv( "WW_UI_SHOT_DOCK" );
				if ( !dockName.isEmpty() ) {
					if ( QDockWidget * d = skope->findChild<QDockWidget *>( QString::fromLocal8Bit( dockName ) ) ) {
						d->show();
						d->raise();
						qApp->processEvents();
					}
				}
				// WW_UI_SHOT_POSE=1 flips the viewport into Pose Mode first.
				if ( qEnvironmentVariableIsSet( "WW_UI_SHOT_POSE" ) ) {
					skope->ogl->setPoseMode( true );
					qApp->processEvents();
					skope->ogl->update();
					qApp->processEvents();
				}
				skope->grab().save( QApplication::applicationDirPath() + "/ww_ui_shot.png" );
				qApp->quit();
			} );
		} );
	}

	/* PHYSICS SIM HARNESS (WW_PHYSICS_TEST=<report.txt>): drive the viewport's
	 * Physics Sim mode through synthesized mouse and key events and report what
	 * happened, so the mode is verified rather than eyeballed.
	 *
	 * It posts REAL QMouseEvents at the GLView rather than calling the preview
	 * directly. The controller underneath is already covered headlessly by
	 * `simulate --drag-spring` and the pick self-test; what only a window can
	 * exercise is the event plumbing -- that a press reaches the picker, that a
	 * move reaches the drag, that Space and R are not swallowed by another
	 * binding first. Calling the controller would test the part that is already
	 * tested and skip the part that is not.
	 *
	 * WW_WINDOW_AT=x,y places the window before it is shown, so a run can be put
	 * on a second monitor and never take focus from what the user is doing.
	 */
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_PHYSICS_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool, QString & ) {
			QTimer::singleShot( 2000, skope, [skope]() {
				QString outPath = qEnvironmentVariable( "WW_PHYSICS_TEST" );
				if ( outPath.isEmpty() || outPath == QStringLiteral( "1" ) )
					outPath = QApplication::applicationDirPath() + "/ww_physics_test.txt";
				QStringList log;
				int failures = 0;
				auto check = [&log, &failures]( bool ok, const QString & what ) {
					log << ( ok ? QStringLiteral( "ok    " ) : QStringLiteral( "FAIL  " ) ) + what;
					if ( !ok )
						failures++;
				};

				GLView * gl = skope->getGLView();
				PhysicsPreview & pv = gl->physicsSim();

				/* The Collision dropdown, checked BEFORE the sim is entered so a
				 * file with nothing to simulate still exercises it -- that is the
				 * case where the button has to be GREY, and skipping out early
				 * would have left the disabled path untested.
				 */
				bool haveCollision = false;
				if ( NifModel * m = skope->nif ) {
					for ( qint32 b = 0; b < m->getBlockCount() && !haveCollision; b++ ) {
						const QModelIndex i = m->getBlockIndex( b );
						haveCollision = m->blockInherits( i, "bhkCollisionObject" )
							|| m->blockInherits( i, "bhkNiCollisionObject" )
							|| m->blockInherits( i, "bhkPhysicsSystem" )
							|| m->blockInherits( i, "bhkRagdollSystem" );
					}
				}
				if ( QToolButton * cb = skope->findChild<QToolButton *>(
						QStringLiteral( "ViewCollisionButton" ) ) ) {
					check( cb->isEnabled() == haveCollision,
						QStringLiteral( "Collision button %1 (file %2 collision)" )
							.arg( cb->isEnabled() ? QStringLiteral( "enabled" ) : QStringLiteral( "greyed" ) )
							.arg( haveCollision ? QStringLiteral( "has" ) : QStringLiteral( "has no" ) ) );
					/* The toolbar itself, photographed. Icons are drawn in code and
					 * nothing else here looks at one -- the previous collision icon
					 * was a wireframe cube indistinguishable at 16 px from the x-ray
					 * icon beside it, and that survived because no test ever saw them
					 * next to each other.
					 */
					if ( QWidget * bar = cb->parentWidget() )
						bar->grab().save( outPath + QStringLiteral( ".toolbar.png" ) );
					/* popup(), not showMenu(): showMenu spins its own event loop and
					 * does not return until the menu is dismissed, so a harness that
					 * calls it simply hangs.
					 */
					if ( QMenu * cm = cb->menu(); cm && cb->isEnabled() ) {
						cm->popup( cb->mapToGlobal( QPoint( 0, cb->height() ) ) );
						QApplication::processEvents();
						check( cm->isVisible(), QStringLiteral( "Collision panel opened" ) );
						cm->grab().save( outPath + QStringLiteral( ".panel.png" ) );

						/* The visibility box and View > Show Collision are one setting.
						 * Checked in both directions: a panel that only wrote to the
						 * action would go stale the moment the toolbar was used.
						 */
						if ( QCheckBox * vis = cm->findChild<QCheckBox *>() ) {
							const bool was = skope->ui->aShowCollision->isChecked();
							vis->setChecked( !was );
							QApplication::processEvents();
							check( skope->ui->aShowCollision->isChecked() != was,
								QStringLiteral( "visibility box drives Show Collision" ) );
							skope->ui->aShowCollision->trigger();
							QApplication::processEvents();
							check( vis->isChecked() == skope->ui->aShowCollision->isChecked(),
								QStringLiteral( "and follows it back" ) );
							if ( skope->ui->aShowCollision->isChecked() != was )
								skope->ui->aShowCollision->trigger();
						}
						cm->close();
						QApplication::processEvents();
					}
				} else {
					check( false, QStringLiteral( "Collision button found" ) );
				}


				/* A file with no jointed collision is a SKIP, not a failure.
				 *
				 * Refusing is the correct behaviour there -- most NIFs are not ragdolls,
				 * and CreateABot's skeleton carries two single-body systems and no
				 * constraints at all. Counting it as failed cascaded one honest refusal
				 * into nine red lines and would have made a corpus run unreadable.
				 */
				if ( !gl->setPhysicsSimMode( true ) ) {
					log << QStringLiteral( "skip  nothing to simulate in this file" );
					QFile sf( outPath );
					if ( sf.open( QIODevice::WriteOnly | QIODevice::Text ) )
						sf.write( log.join( QChar( '\n' ) ).toUtf8() + '\n' );
					qApp->quit();
					return;
				}
				check( pv.active(), QStringLiteral( "preview active" ) );
				check( pv.bodyCount() > 1, QStringLiteral( "bodies: %1" ).arg( pv.bodyCount() ) );
				check( pv.jointCount() > 0, QStringLiteral( "joints: %1" ).arg( pv.jointCount() ) );
				const QVector<Vector3> atEntry = pv.soup();
				check( !atEntry.isEmpty(), QStringLiteral( "geometry: %1 triangles" ).arg( atEntry.size() / 3 ) );

				/* Grab FIRST, in the authored pose.
				 *
				 * Testing the grab after letting it fall meant clicking at where the
				 * bodies used to be: a brahmin drops 4.5 m in the first second, which
				 * put every one of them out of frame and failed the check for a reason
				 * that had nothing to do with picking.
				 */
				gl->setOrientation( GLView::ViewFront, true );
				QApplication::processEvents();

				/* Grab a body through the real event path. The target is a body's
				 * own centre projected to screen, so the click lands on something
				 * whatever the camera happens to be framing.
				 */
				/* Counted in its own pass. Counting inside the grab loop stops at the
				 * first success, so it reported "1 body on screen" for a ragdoll filling
				 * the viewport -- a diagnostic that misleads is worse than none.
				 */
				int onScreen = 0, rayHits = 0;
				for ( int b = 0; b < pv.sim().bodies().size(); b++ ) {
					QPointF sp;
					if ( !gl->worldToScreen( pv.sim().toWorld( b, Vector3() ) * PhysicsPreview::SCALE, sp )
						|| sp.x() < 0 || sp.y() < 0 || sp.x() >= gl->width() || sp.y() >= gl->height() )
						continue;
					onScreen++;
					Vector3 ro, rd;
					gl->mouseRayWorld( sp, ro, rd );
					if ( pv.pick( ro, rd ).hit() )
						rayHits++;
				}
				log << QStringLiteral( "      %1 of %2 body centres on screen, the picker hits %3" )
					.arg( onScreen ).arg( pv.sim().bodies().size() ).arg( rayHits );

				/* Aim at a point that is definitely ON the drawn surface -- the
				 * centroid of one of the body's own triangles -- and see whether the
				 * picker returns that body. This is what a user does: they click the
				 * shape they can see.
				 */
				{
					int aimed = 0, correct = 0, wrongBody = 0, missed = 0, degenerate = 0;
					for ( int b = 0; b < pv.sim().bodies().size(); b++ ) {
						const QVector<Vector3> tris = pv.bodySoup( b );
						if ( tris.size() < 3 )
							continue;
						// a few triangles spread through the shape, not just the first
						for ( int k = 0; k < 5; k++ ) {
							const int t = ( tris.size() / 3 ) * k / 5;
							/* DEGENERATE triangles are not clickable, by anyone.
							 *
							 * PowerArmor's skeleton_female_faceBones carries a body whose
							 * first triangle has zero area -- both edges 0.000 game units
							 * -- and Moller-Trumbore rejects it on the |det| test, exactly
							 * as it should: there is no surface there to hit. Aiming at it
							 * and then demanding a hit tested the file, not the picker.
							 */
							const Vector3 e1 = tris.at( t * 3 + 1 ) - tris.at( t * 3 );
							const Vector3 e2 = tris.at( t * 3 + 2 ) - tris.at( t * 3 );
							if ( Vector3::crossproduct( e1, e2 ).length() < 1.0e-6f ) {
								degenerate++;
								continue;
							}
							const Vector3 c = ( tris.at( t * 3 ) + tris.at( t * 3 + 1 )
								+ tris.at( t * 3 + 2 ) ) / 3.0f;
							QPointF sp;
							if ( !gl->worldToScreen( c, sp ) || sp.x() < 0 || sp.y() < 0
								|| sp.x() >= gl->width() || sp.y() >= gl->height() )
								continue;
							Vector3 ro, rd;
							gl->mouseRayWorld( sp, ro, rd );
							const SimPick hit = pv.pick( ro, rd );
							aimed++;
							if ( !hit.hit() )
								missed++;
							else if ( hit.body == b )
								correct++;
							else
								wrongBody++;
						}
					}
					/* "Aimed at body b, got body c" cannot be the measure: a rig's
					 * shapes overlap heavily, so a neighbour genuinely in front is
					 * the RIGHT answer, and counting it as wrong made both pickers
					 * look equally bad and hid the difference between them.
					 *
					 * The property that was actually broken is agreement with what
					 * is DRAWN: whatever body comes back, the point reported must
					 * lie on that body's surface. The sphere set cannot promise it,
					 * since its hit is a point on a capsule end-cap that may be
					 * nowhere near the shape; a triangle pick gives it by
					 * construction. Both are measured so the difference is shown
					 * rather than asserted.
					 */
					/* Distance to the triangle ITSELF, not to its centroid.
					 *
					 * Measuring to centroids was a third failed attempt: collision
					 * hulls have large triangles, so a point exactly on one sits
					 * several units from its middle, and the metric reported
					 * triangle size rather than picker accuracy.
					 */
					auto pointToTri = []( const Vector3 & p, const Vector3 & a,
						const Vector3 & b, const Vector3 & c ) -> float {
						const Vector3 ab = b - a, ac = c - a, ap = p - a;
						const float d1 = Vector3::dotproduct( ab, ap );
						const float d2 = Vector3::dotproduct( ac, ap );
						if ( d1 <= 0.0f && d2 <= 0.0f )
							return ( p - a ).length();
						const Vector3 bp = p - b;
						const float d3 = Vector3::dotproduct( ab, bp );
						const float d4 = Vector3::dotproduct( ac, bp );
						if ( d3 >= 0.0f && d4 <= d3 )
							return ( p - b ).length();
						const Vector3 cp = p - c;
						const float d5 = Vector3::dotproduct( ab, cp );
						const float d6 = Vector3::dotproduct( ac, cp );
						if ( d6 >= 0.0f && d5 <= d6 )
							return ( p - c ).length();
						const float vc = d1 * d4 - d3 * d2;
						if ( vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f )
							return ( p - ( a + ab * ( d1 / ( d1 - d3 ) ) ) ).length();
						const float vb = d5 * d2 - d1 * d6;
						if ( vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f )
							return ( p - ( a + ac * ( d2 / ( d2 - d6 ) ) ) ).length();
						const float va = d3 * d6 - d5 * d4;
						if ( va <= 0.0f && ( d4 - d3 ) >= 0.0f && ( d5 - d6 ) >= 0.0f ) {
							const float w2 = ( d4 - d3 ) / ( ( d4 - d3 ) + ( d5 - d6 ) );
							return ( p - ( b + ( c - b ) * w2 ) ).length();
						}
						const float denom = 1.0f / ( va + vb + vc );
						return ( p - ( a + ab * ( vb * denom ) + ac * ( vc * denom ) ) ).length();
					};
					auto offSurface = [&]( const SimPick & hit ) -> float {
						if ( !hit.hit() )
							return -1.0f;
						const QVector<Vector3> tri = pv.bodySoup( hit.body );
						if ( tri.size() < 3 )
							return -1.0f;
						float best = 1.0e9f;
						const Vector3 w = hit.worldPoint * PhysicsPreview::SCALE;
						for ( int t = 0; t + 2 < tri.size(); t += 3 )
							best = std::min( best, pointToTri( w, tri.at( t ),
								tri.at( t + 1 ), tri.at( t + 2 ) ) );
						return best;
					};
					float worstNew = 0.0f, worstOld = 0.0f;
					int compared = 0;
					for ( int b = 0; b < pv.sim().bodies().size(); b++ ) {
						const QVector<Vector3> tris = pv.bodySoup( b );
						if ( tris.size() < 3 )
							continue;
						const Vector3 c = ( tris.at( 0 ) + tris.at( 1 ) + tris.at( 2 ) ) / 3.0f;
						QPointF sp;
						if ( !gl->worldToScreen( c, sp ) || sp.x() < 0 || sp.y() < 0
							|| sp.x() >= gl->width() || sp.y() >= gl->height() )
							continue;
						Vector3 ro, rd;
						gl->mouseRayWorld( sp, ro, rd );
						const float dNew = offSurface( pv.pick( ro, rd ) );
						const float dOld = offSurface(
							pv.sim().pick( ro / PhysicsPreview::SCALE, rd ) );
						if ( dNew >= 0.0f ) {
							worstNew = std::max( worstNew, dNew );
							compared++;
						}
						if ( dOld >= 0.0f )
							worstOld = std::max( worstOld, dOld );
					}
					log << QStringLiteral( "      clicking ON a shape: %1 aimed, %2 returned that "
										   "body, %3 a nearer one, %4 nothing"
										   "%5" )
						.arg( aimed ).arg( correct ).arg( wrongBody ).arg( missed )
						.arg( degenerate ? QStringLiteral( " (%1 zero-area triangles skipped)" )
							.arg( degenerate ) : QString() );
					log << QStringLiteral( "      hit point off the reported body: %1 game units "
										   "(sphere set: %2)" )
						.arg( double( worstNew ), 0, 'f', 2 )
						.arg( double( worstOld ), 0, 'f', 2 );
					check( compared > 0 && worstNew < worstOld,
						QStringLiteral( "the pick lands on the body it reports" ) );
					check( missed == 0,
						QStringLiteral( "clicking a shape always hits something" ) );
				}

				int grabbed = -1;
				QPointF grabAt;
				for ( int b = 0; b < pv.sim().bodies().size() && grabbed < 0; b++ ) {
					QPointF sp;
					if ( !gl->worldToScreen( pv.sim().toWorld( b, Vector3() ) * PhysicsPreview::SCALE, sp ) )
						continue;
					if ( sp.x() < 0 || sp.y() < 0
						|| sp.x() >= gl->width() || sp.y() >= gl->height() )
						continue;

					QMouseEvent press( QEvent::MouseButtonPress, sp, gl->mapToGlobal( sp ),
						gl->selectMouseButton(), gl->selectMouseButton(), Qt::NoModifier );
					QApplication::sendEvent( gl, &press );
					if ( pv.grabbing() ) {
						grabbed = pv.grabbedBody();
						grabAt = sp;
					}
				}
				check( grabbed >= 0, QStringLiteral( "a click grabbed body %1" ).arg( grabbed ) );

				if ( grabbed >= 0 ) {
					const Vector3 before = pv.sim().toWorld( grabbed, Vector3() );
					// drag 150 px right and up, then let the solver chase it
					for ( int i = 1; i <= 15; i++ ) {
						const QPointF p = grabAt + QPointF( 10.0 * i, -6.0 * i );
						QMouseEvent move( QEvent::MouseMove, p, gl->mapToGlobal( p ),
							Qt::NoButton, gl->selectMouseButton(), Qt::NoModifier );
						QApplication::sendEvent( gl, &move );
						for ( int k = 0; k < 4; k++ )
							gl->physicsTick( 1.0f / 60.0f );
					}
					/* Right-click while holding freezes the bone IN HAND, not
					 * whatever the ray finds -- a dragged limb rarely stays under
					 * the pointer, so aiming would freeze a neighbour.
					 */
					{
						const QPointF away( 5.0, 5.0 );   // deliberately off the rig
						QMouseEvent rc( QEvent::MouseButtonPress, away, gl->mapToGlobal( away ),
							gl->cursorPlaceButton(), gl->cursorPlaceButton(), Qt::NoModifier );
						QApplication::sendEvent( gl, &rc );
						check( pv.sim().bodies().at( grabbed ).pinned,
							QStringLiteral( "right-click mid-drag froze the held bone" ) );
						check( pv.grabbing(), QStringLiteral( "and kept hold of it" ) );
						QApplication::sendEvent( gl, &rc );
						check( !pv.sim().bodies().at( grabbed ).pinned,
							QStringLiteral( "right-click again unfroze it" ) );
						check( pv.grabbedBody() == grabbed,
							QStringLiteral( "still the same bone in hand" ) );
					}

					// while still held: the rig black, the bone in hand orange
					check( !pv.grabbedSoup().isEmpty(),
						QStringLiteral( "the held bone is drawn apart from the rest" ) );
					// the name comes from the NIF, since the packfile carries none
					check( !pv.bodyName( grabbed ).isEmpty(),
						QStringLiteral( "the held bone is named: %1" )
							.arg( pv.bodyName( grabbed ) ) );
					gl->update();
					QApplication::processEvents();
					gl->grabFramebuffer().save( outPath + QStringLiteral( ".grab.png" ) );

					const float pulled = ( pv.sim().toWorld( grabbed, Vector3() ) - before ).length();
					check( pulled > 0.005f, QStringLiteral( "the drag moved it %1 m" )
						.arg( double( pulled ), 0, 'f', 4 ) );

					QMouseEvent rel( QEvent::MouseButtonRelease, grabAt, gl->mapToGlobal( grabAt ),
						gl->selectMouseButton(), Qt::NoButton, Qt::NoModifier );
					QApplication::sendEvent( gl, &rel );
					check( !pv.grabbing(), QStringLiteral( "release let go" ) );
				}

				/* A second of simulated time through the REAL per-frame path.
				 *
				 * The measurement is on what the viewport DRAWS, not on the solver's
				 * own pose. Stepping the solver directly, which is what this did at
				 * first, passed every check while the drawn geometry never moved -- the
				 * preview was pushed once at mode entry and never again, and only
				 * looking at two identical screenshots caught it.
				 */
				const QVector<Vector3> drawnBefore = gl->collisionPreview();
				for ( int i = 0; i < 60; i++ )
					gl->physicsTick( 1.0f / 60.0f );
				const QVector<Vector3> drawnAfter = gl->collisionPreview();
				float moved = 0.0f;
				for ( int i = 0; i < std::min( drawnBefore.size(), drawnAfter.size() ); i++ )
					moved = std::max( moved, ( drawnBefore.at( i ) - drawnAfter.at( i ) ).length() );
				check( !drawnBefore.isEmpty(), QStringLiteral( "the viewport is drawing it" ) );
				check( moved > 0.01f, QStringLiteral( "what is DRAWN moves: %1 game units in 1 s" )
					.arg( double( moved ), 0, 'f', 3 ) );
				check( moved < 10000.0f, QStringLiteral( "it stayed bounded" ) );
				// mid-fall, so the images show the viewport actually animating rather
				// than only the pose it resets to
				gl->update();
				QApplication::processEvents();
				gl->grabFramebuffer().save( outPath + QStringLiteral( ".falling.png" ) );

				// Space pauses: the pose must then stop changing
				QKeyEvent space( QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier );
				QApplication::sendEvent( gl, &space );
				check( pv.paused(), QStringLiteral( "Space paused it" ) );
				const QVector<Vector3> paused1 = gl->collisionPreview();
				for ( int i = 0; i < 30; i++ )
					gl->physicsTick( 1.0f / 60.0f );
				check( gl->collisionPreview() == paused1, QStringLiteral( "paused really is frozen" ) );
				QApplication::sendEvent( gl, &space );
				check( !pv.paused(), QStringLiteral( "Space resumed it" ) );

				/* Every tool, through the real event path.
				 *
				 * Each is checked by its EFFECT, not by whether the call returned
				 * true: a tool that consumed the click and did nothing is exactly
				 * the failure worth catching.
				 */
				/* Returns the body the PICKER lands on, not the one aimed at.
				 *
				 * A ray toward body b's centre often hits a different body first --
				 * limbs overlap. Returning b meant the Pin test clicked one body and
				 * then asserted about another, and reported a toggle that had genuinely
				 * happened as a failure.
				 */
				auto aimAtSomething = [&]( QPointF & sp ) -> int {
					for ( int b = 0; b < pv.sim().bodies().size(); b++ ) {
						QPointF p;
						if ( !gl->worldToScreen( pv.sim().toWorld( b, Vector3() ) * PhysicsPreview::SCALE, p ) )
							continue;
						Vector3 ro, rd;
						gl->mouseRayWorld( p, ro, rd );
						const SimPick hit = pv.pick( ro, rd );
						if ( hit.hit() ) {
							sp = p;
							return hit.body;
						}
					}
					return -1;
				};
				auto clickAt = [&]( const QPointF & sp ) {
					QMouseEvent press( QEvent::MouseButtonPress, sp, gl->mapToGlobal( sp ),
						gl->selectMouseButton(), gl->selectMouseButton(), Qt::NoModifier );
					QApplication::sendEvent( gl, &press );
					QMouseEvent rel( QEvent::MouseButtonRelease, sp, gl->mapToGlobal( sp ),
						gl->selectMouseButton(), Qt::NoButton, Qt::NoModifier );
					QApplication::sendEvent( gl, &rel );
				};

				struct ToolCase { PhysicsPreview::Tool tool; const char * name; };
				static const ToolCase cases[] = {
					{ PhysicsPreview::Tool::Shoot, "Shoot" },
					{ PhysicsPreview::Tool::Blast, "Blast" },
				};
				for ( const ToolCase & tc : cases ) {
					pv.reset();
					pv.freeze();
					pv.setTool( tc.tool );
					QPointF sp;
					const int target = aimAtSomething( sp );
					if ( target < 0 ) {
						check( false, QStringLiteral( "%1: nothing to aim at" ).arg( tc.name ) );
						continue;
					}
					const QVector<Vector3> before = pv.soup();
					clickAt( sp );
					// an impulse is velocity, so it only shows up after a step
					for ( int k = 0; k < 6; k++ )
						gl->physicsTick( 1.0f / 60.0f );
					float moved = 0.0f;
					const QVector<Vector3> after = pv.soup();
					for ( int i = 0; i < std::min( before.size(), after.size() ); i++ )
						moved = std::max( moved, ( before.at( i ) - after.at( i ) ).length() );
					check( moved > 0.001f, QStringLiteral( "%1 moved it %2 game units" )
						.arg( QLatin1String( tc.name ) ).arg( double( moved ), 0, 'f', 4 ) );
				}

				/* Shooting, both kinds.
				 *
				 * The hitscan shot has to leave a visible trace and the physical
				 * one has to actually travel: a projectile that teleported to its
				 * target would pass every impulse check while being the thing the
				 * feature exists to avoid.
				 */
				{
					pv.reset();
					pv.freeze();
					pv.setTool( PhysicsPreview::Tool::Shoot );
					pv.settings().shootProjectile = false;
					QPointF sp;
					const int target = aimAtSomething( sp );
					if ( target >= 0 ) {
						clickAt( sp );
						check( !pv.shots().isEmpty(), QStringLiteral( "hitscan leaves a trace" ) );
						if ( !pv.shots().isEmpty() )
							check( !pv.shots().first().flying && pv.shots().first().hit,
								QStringLiteral( "the trace records a hit" ) );
						// and it fades rather than accumulating for ever
						for ( int i = 0; i < 60; i++ )
							gl->physicsTick( 1.0f / 60.0f );
						check( pv.shots().isEmpty(), QStringLiteral( "the trace fades away" ) );

						pv.reset();
						pv.freeze();
						pv.settings().shootProjectile = true;
						pv.settings().projectileSpeed = 20.0f;
						pv.settings().projectileGravity = false;
						const QVector<Vector3> before = pv.soup();
						clickAt( sp );
						check( !pv.shots().isEmpty() && pv.shots().first().flying,
							QStringLiteral( "projectile is in flight" ) );
						// it should take several frames to arrive, not arrive at once
						int framesInFlight = 0;
						for ( int i = 0; i < 120 && !pv.shots().isEmpty()
								&& pv.shots().first().flying; i++ ) {
							gl->physicsTick( 1.0f / 60.0f );
							framesInFlight++;
						}
						check( framesInFlight > 1, QStringLiteral( "it travelled (%1 frames)" )
							.arg( framesInFlight ) );
						check( !pv.shots().isEmpty() && pv.shots().first().hit,
							QStringLiteral( "and connected" ) );
						for ( int i = 0; i < 12; i++ )
							gl->physicsTick( 1.0f / 60.0f );
						float moved = 0.0f;
						const QVector<Vector3> after = pv.soup();
						for ( int i = 0; i < std::min( before.size(), after.size() ); i++ )
							moved = std::max( moved, ( before.at( i ) - after.at( i ) ).length() );
						check( moved > 0.001f, QStringLiteral( "the round moved it %1 game units" )
							.arg( double( moved ), 0, 'f', 4 ) );
						pv.settings().shootProjectile = false;
					} else {
						check( false, QStringLiteral( "Shoot: nothing to aim at" ) );
					}
				}

				// the ground surface, and the limit highlight
				pv.setGroundVisible( true );
				check( !pv.groundSoup().isEmpty(), QStringLiteral( "ground draws as a surface" ) );
				pv.setGroundVisible( false );
				check( pv.groundSoup().isEmpty(), QStringLiteral( "and can be hidden" ) );
				pv.setGroundHeight( pv.groundHeight() + 40.0f );
				pv.resetGroundHeight();
				check( std::fabs( pv.groundHeight() - pv.defaultGroundHeight() ) < 0.01f,
					QStringLiteral( "ground resets to under the rig" ) );
				pv.setHighlightLimits( true );
				check( pv.highlightLimits(), QStringLiteral( "limit highlight can be turned on" ) );
				pv.setHighlightLimits( false );

				/* The physics gun: push/pull, turn, and the beam that shows both.
				 *
				 * Driven through the real event path -- a synthesized wheel event on
				 * the widget -- because what was missing was never the arithmetic but
				 * the WIRING: the depth existed as a member and nothing ever changed
				 * it, so a test that called adjustGrabDepth directly would have passed
				 * against the broken build.
				 */
				{
					pv.reset();
					pv.freeze();
					pv.setPaused( true );
					pv.setTool( PhysicsPreview::Tool::Grab );
					QPointF sp;
					const int target = aimAtSomething( sp );
					if ( target >= 0 ) {
						QMouseEvent press( QEvent::MouseButtonPress, sp, gl->mapToGlobal( sp ),
							gl->selectMouseButton(), gl->selectMouseButton(), Qt::NoModifier );
						QApplication::sendEvent( gl, &press );
						check( pv.grabbing(), QStringLiteral( "grabbed body %1" ).arg( pv.grabbedBody() ) );

						Vector3 grip, hand;
						check( pv.grabBeam( grip, hand ),
							QStringLiteral( "the beam has two ends while holding" ) );
						const float gripOff = ( grip / PhysicsPreview::SCALE
							- pv.sim().toWorld( pv.grabbedBody(), Vector3() ) ).length();
						log << QStringLiteral( "      grip sits %1 m from the body centre" )
							.arg( double( gripOff ), 0, 'f', 3 );

						const float before = pv.grabDepth();
						QWheelEvent out( sp, gl->mapToGlobal( sp ), QPoint(), QPoint( 0, 120 ),
							gl->selectMouseButton(), Qt::NoModifier, Qt::NoScrollPhase, false );
						QApplication::sendEvent( gl, &out );
						const float pushed = pv.grabDepth();

						/* The hand's TARGET has to follow the depth with the mouse
						 * standing still, or the wheel appears to do nothing until the
						 * cursor twitches.
						 *
						 * Sampled HERE, between the push and the pull-back. Sampled after
						 * both, as the first version did, it compares the hand with where
						 * it started -- and a notch out followed by a notch in is a round
						 * trip that lands exactly there, so a working push/pull failed the
						 * check for being reversible.
						 */
						Vector3 g2, h2;
						pv.grabBeam( g2, h2 );
						check( ( h2 - hand ).length() > 0.5f,
							QStringLiteral( "the hand moved without the mouse moving" ) );
						QWheelEvent in( sp, gl->mapToGlobal( sp ), QPoint(), QPoint( 0, -120 ),
							gl->selectMouseButton(), Qt::NoModifier, Qt::NoScrollPhase, false );
						QApplication::sendEvent( gl, &in );
						const float pulled = pv.grabDepth();
						log << QStringLiteral( "      hand depth %1 -> %2 -> %3 units" )
							.arg( double( before ), 0, 'f', 1 ).arg( double( pushed ), 0, 'f', 1 )
							.arg( double( pulled ), 0, 'f', 1 );
						check( pushed > before * 1.01f,
							QStringLiteral( "the wheel pushes the hand away" ) );
						check( pulled < pushed * 0.99f,
							QStringLiteral( "and pulls it back in" ) );

						// turning: a quarter turn about a fixed axis
						const Quat q0 = pv.sim().bodies().at( pv.grabbedBody() ).q;
						pv.beginGrabRotate( Vector3( 1, 0, 0 ), Vector3( 0, 0, 1 ),
							Vector3( 0, 1, 0 ) );
						pv.addGrabRotate( 1.5708f, 0.0f, 0.0f, false );
						check( pv.grabRotating(), QStringLiteral( "the hand is turning" ) );
						check( pv.sim().draggingOrientation(),
							QStringLiteral( "and the solver has an orientation target" ) );
						pv.setPaused( false );
						for ( int i = 0; i < 40; i++ )
							gl->physicsTick( 1.0f / 60.0f );
						const Quat q1 = pv.sim().bodies().at( pv.grabbedBody() ).q;
						/* |dot| of two unit quaternions is 1 only for the same
						 * orientation, so this says "it actually turned" without caring
						 * which of the two equivalent signs each was stored with.
						 */
						const float dot = std::fabs( q0[0] * q1[0] + q0[1] * q1[1]
							+ q0[2] * q1[2] + q0[3] * q1[3] );
						log << QStringLiteral( "      orientation dot after turning: %1" )
							.arg( double( dot ), 0, 'f', 4 );
						check( dot < 0.999f, QStringLiteral( "the held bone turned" ) );

						// snapping quantises the TOTAL, so 20 degrees asked lands on 15
						pv.beginGrabRotate( Vector3( 1, 0, 0 ), Vector3( 0, 0, 1 ),
							Vector3( 0, 1, 0 ) );
						const Quat base = pv.sim().dragOrientation();
						pv.addGrabRotate( 0.3491f, 0.0f, 0.0f, true );
						const Quat snapped = pv.sim().dragOrientation();
						const float sdot = std::min( 1.0f, std::fabs(
							base[0] * snapped[0] + base[1] * snapped[1]
							+ base[2] * snapped[2] + base[3] * snapped[3] ) );
						const float snapDeg = 2.0f * std::acos( sdot ) * 180.0f / float( M_PI );
						log << QStringLiteral( "      20 degrees snapped to %1" )
							.arg( double( snapDeg ), 0, 'f', 1 );
						check( std::fabs( snapDeg - 15.0f ) < 1.0f,
							QStringLiteral( "Shift snaps the turn to 15 degrees" ) );

						/* No-collide, measured where it acts: the broad phase. With it
						 * on, no body-on-body pair may involve the held body.
						 */
						pv.setDragNoCollide( true );
						pv.sim().selfCollision = true;
						gl->physicsTick( 1.0f / 60.0f );
						int touching = 0;
						for ( const SimPair & pr : pv.sim().pairs() )
							if ( pr.b >= 0 && ( pr.a == pv.grabbedBody() || pr.b == pv.grabbedBody() ) )
								touching++;
						check( touching == 0,
							QStringLiteral( "the held bone is excused from self-collision" ) );
						pv.setDragNoCollide( false );

						QMouseEvent rel( QEvent::MouseButtonRelease, sp, gl->mapToGlobal( sp ),
							gl->selectMouseButton(), Qt::NoButton, Qt::NoModifier );
						QApplication::sendEvent( gl, &rel );
						check( !pv.grabbing(), QStringLiteral( "and lets go" ) );
						check( !pv.grabBeam( grip, hand ),
							QStringLiteral( "the beam goes out with the grab" ) );
					} else {
						check( false, QStringLiteral( "physics gun: nothing to aim at" ) );
					}
					pv.reset();
				}

				/* The file's own friction and bounce, now that the solver reads them.
				 *
				 * Checked as a RANGE, not a value: the point is that the bodies
				 * differ from each other, which a single global could never produce.
				 * The corpus authors friction at 0.30 through 3.00 and restitution
				 * from 0 to 0.80, so a rig whose bodies all report 0.5 and 0.4 is one
				 * where build() quietly fell back on the struct defaults.
				 */
				{
					float fLo = 1.0e9f, fHi = -1.0e9f, rLo = 1.0e9f, rHi = -1.0e9f;
					for ( const SimBody & b : pv.sim().bodies() ) {
						fLo = std::min( fLo, b.friction );
						fHi = std::max( fHi, b.friction );
						rLo = std::min( rLo, b.restitution );
						rHi = std::max( rHi, b.restitution );
					}
					log << QStringLiteral( "      authored friction %1..%2, bounce %3..%4" )
						.arg( double( fLo ), 0, 'f', 2 ).arg( double( fHi ), 0, 'f', 2 )
						.arg( double( rLo ), 0, 'f', 2 ).arg( double( rHi ), 0, 'f', 2 );
					check( fLo > 0.0f, QStringLiteral( "every body has a friction" ) );
					check( fHi >= fLo && rHi >= rLo,
						QStringLiteral( "and a bounce, read from the file" ) );

					/* ...and the rig still SETTLES with them.
					 *
					 * Bounce used to default to 0 and now defaults to whatever the
					 * file says, which on this corpus is mostly 0.2 to 0.3 and
					 * sometimes 0.8. That is more faithful and it is also energy
					 * coming back out of every landing, so the thing worth checking
					 * is that a dropped rig still comes to rest.
					 */
					pv.reset();
					pv.setPaused( false );
					for ( int i = 0; i < 300; i++ )
						gl->physicsTick( 1.0f / 60.0f );
					const float rest = pv.stats().maxSpeed;
					log << QStringLiteral( "      after five seconds the fastest body is %1 m/s" )
						.arg( double( rest ), 0, 'f', 2 );
					check( rest < 2.0f, QStringLiteral( "the rig settles with authored bounce" ) );
					check( !pv.stats().diverged, QStringLiteral( "and nothing diverged" ) );
					pv.reset();
				}

				// Unpin all: the way back from a handful of pins
				{
					pv.reset();
					const int n = std::min( 3, pv.bodyCount() );
					for ( int i = 0; i < n; i++ )
						pv.sim().setPinned( i, true );
					check( pv.pinnedCount() >= n,
						QStringLiteral( "%1 bodies pinned" ).arg( pv.pinnedCount() ) );
					pv.unpinAll();
					check( pv.pinnedCount() == 0, QStringLiteral( "unpin all releases them" ) );
				}

				/* Balls: thrown, simulated, drawn, picked and cleared.
				 *
				 * The last two matter as much as the physics. A prop joins the same
				 * body list the rig uses precisely so drawing, picking, grabbing and
				 * pinning need no special case, and a ball that could not be clicked
				 * would mean that had not actually happened.
				 */
				{
					pv.reset();
					pv.setTool( PhysicsPreview::Tool::Prop );
					pv.settings().propSpeed = 0.0f;     // dropped, so its path is predictable
					pv.settings().propRadius = 0.12f;
					pv.settings().propMass = 4.0f;
					const int before = pv.bodyCount();
					const int soupBefore = int( pv.soup().size() );
					QPointF sp;
					if ( aimAtSomething( sp ) >= 0 ) {
						clickAt( sp );
						check( pv.bodyCount() == before + 1,
							QStringLiteral( "a ball joins the simulation" ) );
						check( pv.propCount() == 1, QStringLiteral( "and is counted as a prop" ) );
						check( int( pv.soup().size() ) > soupBefore,
							QStringLiteral( "and is drawn" ) );

						const int ball = pv.bodyCount() - 1;
						check( !pv.bodyName( ball ).isEmpty(),
							QStringLiteral( "it is named \"%1\"" ).arg( pv.bodyName( ball ) ) );
						QPointF bp;
						if ( gl->worldToScreen( pv.sim().toWorld( ball, Vector3() )
								* PhysicsPreview::SCALE, bp ) ) {
							Vector3 ro, rd;
							gl->mouseRayWorld( bp, ro, rd );
							check( pv.pick( ro, rd ).body == ball,
								QStringLiteral( "and clicking it picks it" ) );
						}

						const float z0 = pv.sim().bodies().at( ball ).x[2];
						pv.setPaused( false );
						for ( int i = 0; i < 30; i++ )
							gl->physicsTick( 1.0f / 60.0f );
						const float z1 = pv.sim().bodies().at( ball ).x[2];
						log << QStringLiteral( "      the ball fell %1 m in half a second" )
							.arg( double( z0 - z1 ), 0, 'f', 3 );
						check( z1 < z0, QStringLiteral( "and it falls" ) );

						pv.clearProps();
						check( pv.propCount() == 0 && pv.bodyCount() == before,
							QStringLiteral( "clear balls puts the rig back to %1 bodies" )
								.arg( before ) );
					} else {
						check( false, QStringLiteral( "Ball: nothing to aim at" ) );
					}
					pv.reset();
				}

				/* Bounce, measured as a rebound height.
				 *
				 * Placed by hand rather than thrown, because the question is whether
				 * restitution does anything and that needs a known drop. Dead and
				 * lively are compared with each other rather than with an absolute, so
				 * the test does not depend on the drop height or on the rig.
				 */
				{
					auto dropFrom = [&]( float e ) {
						pv.reset();
						pv.clearProps();
						pv.setRestitution( e );
						pv.sim().ground = true;
						/* Dropped well to one side of the rig.
						 *
						 * Over the origin, as the first version had it, the ball landed on
						 * the ragdoll rather than on the floor -- so it never reached the
						 * height the test was watching for and both runs measured a
						 * rebound of exactly zero, which reads as "restitution does
						 * nothing" when the ball had simply never hit the ground.
						 */
						const float z = pv.sim().groundZ + 1.0f;
						const int b = pv.sim().addProp( Vector3( 50.0f, 50.0f, z ), Vector3(),
							0.1f, 1.0f );
						if ( b < 0 )
							return 0.0f;
						pv.setPaused( false );
						/* Measured from the ball's OWN lowest point rather than from an
						 * assumed contact height, so it does not depend on where the floor
						 * is or on the ball's radius.
						 */
						float lowest = z, rebound = 0.0f;
						bool past = false;
						for ( int i = 0; i < 240; i++ ) {
							pv.step( 1.0f / 60.0f );
							const float h = pv.sim().bodies().at( b ).x[2];
							if ( !past ) {
								if ( h < lowest )
									lowest = h;
								else if ( i > 2 )
									past = true;
							}
							if ( past )
								rebound = std::max( rebound, h - lowest );
						}
						pv.clearProps();
						return rebound;
					};
					const float dead = dropFrom( 0.0f );
					const float lively = dropFrom( 0.8f );
					log << QStringLiteral( "      rebound: dead %1 m, lively %2 m" )
						.arg( double( dead ), 0, 'f', 3 ).arg( double( lively ), 0, 'f', 3 );
					check( lively > dead + 0.05f,
						QStringLiteral( "bounce makes a dropped ball come back up" ) );
					pv.setRestitution( 0.0f );
					pv.reset();
				}

				// Punt sends a body along the view, and pulling reverses it
				{
					pv.reset();
					pv.freeze();
					pv.setTool( PhysicsPreview::Tool::Punt );
					pv.settings().puntStrength = 80.0f;
					pv.settings().puntPull = false;
					QPointF sp;
					const int target = aimAtSomething( sp );
					if ( target >= 0 ) {
						Vector3 ro, rd;
						gl->mouseRayWorld( sp, ro, rd );
						rd.normalize();
						clickAt( sp );
						const float away = Vector3::dotproduct( pv.sim().bodies().at( target ).v, rd );
						pv.reset();
						pv.freeze();
						pv.settings().puntPull = true;
						clickAt( sp );
						const float toward = Vector3::dotproduct( pv.sim().bodies().at( target ).v, rd );
						log << QStringLiteral( "      punt %1 m/s along the view, pull %2" )
							.arg( double( away ), 0, 'f', 2 ).arg( double( toward ), 0, 'f', 2 );
						check( away > 0.01f, QStringLiteral( "punt sends it away" ) );
						check( toward < -0.01f, QStringLiteral( "and pull brings it back" ) );
						pv.settings().puntPull = false;
					} else {
						check( false, QStringLiteral( "Punt: nothing to aim at" ) );
					}
					pv.reset();
				}

				/* Record and scrub.
				 *
				 * Checked by POSE, not by frame count: a recording that stored the
				 * right number of empty frames would pass a count check and be
				 * useless. Seeking back to frame 0 has to put the rig where it was.
				 */
				{
					pv.reset();
					pv.setPaused( false );
					pv.setRecording( true );
					const QVector<Vector3> first = pv.soup();
					for ( int i = 0; i < 45; i++ )
						gl->physicsTick( 1.0f / 60.0f );
					check( pv.frameCount() > 30,
						QStringLiteral( "recorded %1 frames" ).arg( pv.frameCount() ) );
					const QVector<Vector3> moved = pv.soup();
					float drift = 0.0f;
					for ( int i = 0; i < std::min( first.size(), moved.size() ); i++ )
						drift = std::max( drift, ( first.at( i ) - moved.at( i ) ).length() );
					check( drift > 0.01f,
						QStringLiteral( "the rig moved while recording (%1 units)" )
							.arg( double( drift ), 0, 'f', 3 ) );

					/* The TIMELINE scrubs it, not a slider of the panel's own.
					 *
					 * Driven through GLView::setSceneTime, which is the slot the
					 * timeline dock's playhead is connected to -- so this exercises
					 * the same path a drag on the ruler takes, rather than calling
					 * seek() and hoping the two are wired together.
					 */
					const float span = gl->physicsRecordingRange();
					log << QStringLiteral( "      the recording offers the timeline %1 s" )
						.arg( double( span ), 0, 'f', 2 );
					check( span > 0.5f,
						QStringLiteral( "the recording hands the timeline a range" ) );
					int rangeSeen = -1;
					float lo = -1.0f, hi = -1.0f;
					auto conn = QObject::connect( gl, &GLView::sceneTimeChanged, gl,
						[&rangeSeen, &lo, &hi]( float, float mn, float mx ) {
							rangeSeen = 1;
							lo = mn;
							hi = mx;
						} );
					gl->setSceneTime( 0.0f );
					QObject::disconnect( conn );
					check( rangeSeen > 0 && lo == 0.0f && std::fabs( hi - span ) < 0.01f,
						QStringLiteral( "and reports it back as the scene range" ) );
					check( pv.frameIndex() == 0,
						QStringLiteral( "scrubbing the timeline seeks the recording" ) );

					pv.seek( 0 );
					check( pv.paused(), QStringLiteral( "scrubbing pauses" ) );
					const QVector<Vector3> back = pv.soup();
					float err = 0.0f;
					for ( int i = 0; i < std::min( first.size(), back.size() ); i++ )
						err = std::max( err, ( first.at( i ) - back.at( i ) ).length() );
					log << QStringLiteral( "      frame 0 restored to within %1 units" )
						.arg( double( err ), 0, 'f', 4 );
					check( err < 0.01f, QStringLiteral( "scrubbing back restores the pose" ) );

					pv.resumeLive();
					check( pv.frameIndex() < 0, QStringLiteral( "and Live leaves the recording" ) );
					pv.setRecording( false );
					pv.clearRecording();
					check( pv.frameCount() == 0, QStringLiteral( "the recording can be cleared" ) );
					// ...and the scene gets its own ruler back
					check( gl->physicsRecordingRange() == 0.0f,
						QStringLiteral( "and the timeline goes back to the scene" ) );
					pv.setPaused( false );
					pv.reset();
				}

				/* Capture pose: the one control here that writes to the file.
				 *
				 * Checked by reading the node back rather than by trusting the return
				 * count, since a function that wrote nothing and returned a number
				 * would pass the weaker test.
				 */
				{
					pv.reset();
					int probeBlock = -1;
					for ( int i = 0; i < pv.bodyCount() && probeBlock < 0; i++ )
						if ( pv.bodyNode( i ) >= 0 && !pv.sim().bodies().at( i ).pinned )
							probeBlock = pv.bodyNode( i );
					if ( probeBlock >= 0 && skope->nif ) {
						const Vector3 wasAt = skope->nif->get<Vector3>(
							skope->nif->getBlockIndex( probeBlock ), "Translation" );
						pv.setPaused( false );
						for ( int i = 0; i < 90; i++ )
							gl->physicsTick( 1.0f / 60.0f );
						const int moved = gl->physicsCapturePose();
						check( moved > 0, QStringLiteral( "captured the pose onto %1 nodes" )
							.arg( moved ) );
						const Vector3 nowAt = skope->nif->get<Vector3>(
							skope->nif->getBlockIndex( probeBlock ), "Translation" );
						log << QStringLiteral( "      node %1 moved %2 units on capture" )
							.arg( probeBlock ).arg( double( ( nowAt - wasAt ).length() ), 0, 'f', 3 );
						check( ( nowAt - wasAt ).length() > 1.0e-4f,
							QStringLiteral( "and the node really moved" ) );

						/* Undone again, which is both a check and a necessity.
						 *
						 * A check because the button's tooltip promises the capture is
						 * undoable and nothing else here confirms it. A necessity because
						 * the harness quits when it is done, and a MODIFIED file makes
						 * that quit raise a save prompt -- a modal dialog with nobody to
						 * answer it, which hangs the run and leaves an empty report.
						 */
						if ( skope->nif->undoStack ) {
							skope->nif->undoStack->undo();
							const Vector3 undone = skope->nif->get<Vector3>(
								skope->nif->getBlockIndex( probeBlock ), "Translation" );
							check( ( undone - wasAt ).length() < 1.0e-4f,
								QStringLiteral( "and the capture undoes cleanly" ) );
							/* ...and the window told, because undoing is not the same as
							 * being clean: NifSkope prompts on close when EITHER the
							 * window modified flag or the undo stack's clean index says
							 * there is something outstanding, and undo() moves the stack
							 * without touching either. Both, or the quit at the end of
							 * this run raises a save prompt nobody can answer.
							 */
							skope->nif->undoStack->setClean();
							skope->setWindowModified( false );
						}
					} else {
						log << QStringLiteral( "skip  capture pose: no body is bound to a node" );
					}
					pv.reset();
				}

				/* Pull against a pin and see whether the chain goes TAUT.
				 *
				 * This is the complaint: pin one bone, drag another away, and the
				 * joints stretch instead of the rig reaching the end of its chain
				 * and following. The measurement is the worst ball-socket
				 * separation while the drag is at full stretch -- a joint is a
				 * point constraint, so any separation at all is the rig coming
				 * apart, and it should stay at solver noise rather than growing
				 * with how hard you pull.
				 */
				{
					pv.reset();
					pv.setTool( PhysicsPreview::Tool::Grab );
					// pin the root, which is what a user pins to hold the rig down
					pv.sim().setPinned( 0, true );
					QPointF sp;
					const int target = aimAtSomething( sp );
					if ( target >= 0 && target != 0 ) {
						QMouseEvent press( QEvent::MouseButtonPress, sp, gl->mapToGlobal( sp ),
							gl->selectMouseButton(), gl->selectMouseButton(), Qt::NoModifier );
						QApplication::sendEvent( gl, &press );
						// drag a long way: far past anything the rig can reach
						float worstSep = 0.0f;
						for ( int i = 1; i <= 40; i++ ) {
							const QPointF p = sp + QPointF( 26.0 * i, -10.0 * i );
							QMouseEvent mv( QEvent::MouseMove, p, gl->mapToGlobal( p ),
								Qt::NoButton, gl->selectMouseButton(), Qt::NoModifier );
							QApplication::sendEvent( gl, &mv );
							gl->physicsTick( 1.0f / 60.0f );
							worstSep = std::max( worstSep, pv.stats().maxJointError );
						}
						log << QStringLiteral( "      worst joint separation while hauling: %1 m" )
							.arg( double( worstSep ), 0, 'f', 4 );
						// 2 cm: a joint is a point constraint, so this is generous
						check( worstSep < 0.02f,
							QStringLiteral( "the chain goes taut instead of stretching" ) );
						QMouseEvent rel( QEvent::MouseButtonRelease, sp, gl->mapToGlobal( sp ),
							gl->selectMouseButton(), Qt::NoButton, Qt::NoModifier );
						QApplication::sendEvent( gl, &rel );
					}
					pv.sim().setPinned( 0, false );
					pv.reset();
				}

				// Pinning is the RIGHT button now, and works with any tool active
				{
					pv.reset();
					pv.setTool( PhysicsPreview::Tool::Grab );
					QPointF sp;
					const int target = aimAtSomething( sp );
					if ( target >= 0 ) {
						/* Asserted as a TOGGLE, not as "becomes pinned". The first body
						 * a ray finds is often the root, which build() has already
						 * pinned, so clicking it correctly UNpins it -- and a test that
						 * demanded `pinned == true` failed the tool for behaving right.
						 */
						const bool was = pv.sim().bodies().at( target ).pinned;
						auto rightClick = [&]() {
							QMouseEvent pr( QEvent::MouseButtonPress, sp, gl->mapToGlobal( sp ),
								gl->cursorPlaceButton(), gl->cursorPlaceButton(), Qt::NoModifier );
							QApplication::sendEvent( gl, &pr );
						};
						/* The secondary button must do nothing BUT pin. It normally
						 * places the 3D cursor on release and zooms on drag, and both
						 * fought with the pin.
						 */
						const float distBefore = gl->cameraDistance();
						rightClick();
						{
							const QPointF p = sp + QPointF( 60, 40 );
							QMouseEvent mv( QEvent::MouseMove, p, gl->mapToGlobal( p ),
								gl->cursorPlaceButton(), gl->cursorPlaceButton(), Qt::NoModifier );
							QApplication::sendEvent( gl, &mv );
							QMouseEvent rl( QEvent::MouseButtonRelease, p, gl->mapToGlobal( p ),
								gl->cursorPlaceButton(), Qt::NoButton, Qt::NoModifier );
							QApplication::sendEvent( gl, &rl );
						}
						check( std::fabs( gl->cameraDistance() - distBefore ) < 1.0e-3f,
							QStringLiteral( "a right-drag does not zoom" ) );
						check( pv.sim().bodies().at( target ).pinned != was,
							QStringLiteral( "right-click pinned body %1 (%2 -> %3)" ).arg( target )
								.arg( was ? QStringLiteral( "pinned" ) : QStringLiteral( "free" ) )
								.arg( was ? QStringLiteral( "free" ) : QStringLiteral( "pinned" ) ) );
						check( !pv.pinnedSoup().isEmpty(),
							QStringLiteral( "pinned bodies draw apart from the rest" ) );
						gl->update();
						QApplication::processEvents();
						gl->grabFramebuffer().save( outPath + QStringLiteral( ".pinned.png" ) );
						rightClick();
						check( pv.sim().bodies().at( target ).pinned == was,
							QStringLiteral( "right-click again put it back" ) );
					} else {
						check( false, QStringLiteral( "pin: nothing to aim at" ) );
					}
				}

				// Wind is a held force: it acts while the button is down, not after
				{
					pv.reset();
					pv.setTool( PhysicsPreview::Tool::Wind );
					const QPointF mid( gl->width() / 2.0, gl->height() / 2.0 );
					QMouseEvent press( QEvent::MouseButtonPress, mid, gl->mapToGlobal( mid ),
						gl->selectMouseButton(), gl->selectMouseButton(), Qt::NoModifier );
					QApplication::sendEvent( gl, &press );
					check( pv.wind().length() > 0.0f, QStringLiteral( "Wind blows while held" ) );
					QMouseEvent rel( QEvent::MouseButtonRelease, mid, gl->mapToGlobal( mid ),
						gl->selectMouseButton(), Qt::NoButton, Qt::NoModifier );
					QApplication::sendEvent( gl, &rel );
					check( pv.wind().length() == 0.0f, QStringLiteral( "Wind stops on release" ) );
				}

				/* Throw against Drag on the identical motion.
				 *
				 * Comparing the two is the only way to show the throw does
				 * something a plain release does not: a dragged body is already
				 * moving at the hand's speed when it is let go, and what Throw
				 * adds is that the velocity survives the next constraint solve.
				 */
				auto dragAndCoast = [&]( PhysicsPreview::Tool t, bool movingAtRelease ) -> float {
					pv.reset();
					pv.setTool( t );
					QPointF sp;
					const int target = aimAtSomething( sp );
					if ( target < 0 )
						return -1.0f;
					const Vector3 startAt = pv.sim().toWorld( target, Vector3() );
					QMouseEvent press( QEvent::MouseButtonPress, sp, gl->mapToGlobal( sp ),
						gl->selectMouseButton(), gl->selectMouseButton(), Qt::NoModifier );
					QApplication::sendEvent( gl, &press );
					for ( int i = 1; i <= 12; i++ ) {
						const QPointF p = sp + QPointF( 14.0 * i, 0.0 );
						QMouseEvent mv( QEvent::MouseMove, p, gl->mapToGlobal( p ),
							Qt::NoButton, gl->selectMouseButton(), Qt::NoModifier );
						QApplication::sendEvent( gl, &mv );
						gl->physicsTick( 1.0f / 60.0f );
					}
					// the direction the hand travelled, so the measurement can ignore
					// everything gravity is doing at the same time
					Vector3 axis = pv.sim().toWorld( target, Vector3() ) - startAt;
					const float axisLen = axis.length();
					axis = ( axisLen > 1.0e-6f ) ? axis / axisLen : Vector3( 1, 0, 0 );
					if ( !movingAtRelease ) {
						// hold still first, so the hand has no velocity to hand over
						for ( int i = 0; i < 20; i++ )
							gl->physicsTick( 1.0f / 60.0f );
					}
					QMouseEvent rel( QEvent::MouseButtonRelease, sp, gl->mapToGlobal( sp ),
						gl->selectMouseButton(), Qt::NoButton, Qt::NoModifier );
					QApplication::sendEvent( gl, &rel );
					/* The speed handed over AT the moment of release.
					 *
					 * Measuring how far it then coasts conflates the throw with gravity:
					 * on the feral ghoul the still-release case travelled further,
					 * because the bone was left swinging and kept falling. What the tool
					 * promises is that the hand's velocity survives the release, so that
					 * is what is measured, before a single step can muddy it.
					 */
					/* Along the DRAG axis, not the total speed.
					 *
					 * Holding still for 20 frames lets the rig fall, so the "still"
					 * case leaves the bone at 1 m/s of honest gravity -- on the feral
					 * ghoul that was most of the thrown speed and the comparison said
					 * nothing. Gravity contributes nothing along the hand's direction
					 * of travel, so that component isolates what the throw added.
					 */
					return Vector3::dotproduct( pv.sim().bodies().at( target ).v, axis );
				};
				/* A release while MOVING carries; the same tool released while still
				 * does not. That is the whole of what merging Drag and Throw means, so
				 * it is what gets measured.
				 */
				const float speedMoving = dragAndCoast( PhysicsPreview::Tool::Grab, true );
				const float speedStill = dragAndCoast( PhysicsPreview::Tool::Grab, false );
				check( speedMoving > speedStill + 0.25f,
					QStringLiteral( "released moving it is thrown, released still it drops "
									"(%1 m/s vs %2 m/s along the drag)" )
						.arg( double( speedMoving ), 0, 'f', 3 ).arg( double( speedStill ), 0, 'f', 3 ) );

				// Options
				pv.setTool( PhysicsPreview::Tool::Grab );
				/* Gravity measured as FALL, not as total movement.
				 *
				 * Two wrong versions came before this one. "With gravity off it barely
				 * moves" is false because the authored pose has bodies overlapping --
				 * the brahmin has 18 pairs touching at rest -- and the contact solve
				 * pushes them apart regardless. Comparing total drift is also false:
				 * power armour drifts MORE with gravity off (193 against 149), because
				 * gravity holds it against the floor while the push-apart has nothing
				 * to settle it.
				 *
				 * What the option actually promises is that nothing accelerates
				 * downward, so the measurement is the drop in mean height and nothing
				 * else.
				 */
				auto dropOverASecond = [&]() -> float {
					pv.reset();
					pv.freeze();
					auto meanZ = [&]() {
						const QVector<Vector3> s = pv.soup();
						if ( s.isEmpty() )
							return 0.0;
						double z = 0.0;
						for ( const Vector3 & v : s )
							z += double( v[2] );
						return z / double( s.size() );
					};
					const double before = meanZ();
					for ( int i = 0; i < 60; i++ )
						gl->physicsTick( 1.0f / 60.0f );
					return float( before - meanZ() );
				};
				pv.setGravityEnabled( true );
				const float dropOn = dropOverASecond();
				pv.setGravityEnabled( false );
				const float dropOff = dropOverASecond();
				check( dropOff < dropOn * 0.5f,
					QStringLiteral( "gravity off: it does not fall (dropped %1 vs %2 with gravity)" )
						.arg( double( dropOff ), 0, 'f', 2 ).arg( double( dropOn ), 0, 'f', 2 ) );
				pv.setGravityEnabled( true );

				pv.reset();
				pv.freeze();
				{
					float speed = 0.0f;
					for ( const SimBody & sb : pv.sim().bodies() )
						speed = std::max( speed, sb.v.length() );
					check( speed == 0.0f, QStringLiteral( "freeze stopped everything" ) );
				}

				pv.setTimeScale( 0.25f );
				check( pv.timeScale() > 0.24f && pv.timeScale() < 0.26f,
					QStringLiteral( "time scale set" ) );
				pv.setTimeScale( 1.0f );
				pv.setGroundEnabled( false );
				check( !pv.groundEnabled(), QStringLiteral( "ground can be turned off" ) );
				pv.setGroundEnabled( true );
				pv.setSelfCollision( false );
				check( !pv.selfCollision(), QStringLiteral( "self-collision can be turned off" ) );
				pv.setSelfCollision( true );
				pv.setAngularLimits( false );
				check( !pv.angularLimits(), QStringLiteral( "angular limits can be turned off" ) );
				pv.setAngularLimits( true );
				pv.reset();

				// R resets to the stored pose
				QKeyEvent rKey( QEvent::KeyPress, Qt::Key_R, Qt::NoModifier );
				QApplication::sendEvent( gl, &rKey );
				const QVector<Vector3> afterReset = pv.soup();
				float back = 0.0f;
				for ( int i = 0; i < std::min( atEntry.size(), afterReset.size() ); i++ )
					back = std::max( back, ( atEntry.at( i ) - afterReset.at( i ) ).length() );
				check( back < 1.0e-3f, QStringLiteral( "R restored the start pose (worst %1)" )
					.arg( double( back ), 0, 'f', 6 ) );

				gl->update();
				QApplication::processEvents();
				gl->grabFramebuffer().save( outPath + QStringLiteral( ".png" ) );

				// the new drawing: floor, a round in flight, and the trace behind it
				{
					pv.reset();
					pv.setGroundVisible( true );
					pv.setTool( PhysicsPreview::Tool::Shoot );
					pv.settings().shootProjectile = true;
					pv.settings().projectileSpeed = 8.0f;
					pv.settings().projectileRadius = 0.06f;
					QPointF sp;
					if ( aimAtSomething( sp ) >= 0 ) {
						clickAt( sp );
						// far enough along that the round is not in the camera's face:
						// a shot 3 frames old is 0.4 m away and fills the viewport
						for ( int i = 0; i < 60 && !pv.shots().isEmpty()
								&& pv.shots().first().flying; i++ )
							gl->physicsTick( 1.0f / 60.0f );
						gl->update();
						QApplication::processEvents();
						gl->grabFramebuffer().save( outPath + QStringLiteral( ".shot.png" ) );
					}
					pv.settings().shootProjectile = false;
				}

				// the panel again, this time with the sim running, so the enabled
				// state of every control is looked at and not only the idle one
				if ( QToolButton * cb2 = skope->findChild<QToolButton *>(
						QStringLiteral( "ViewCollisionButton" ) ) ) {
					if ( QMenu * cm2 = cb2->menu() ) {
						cm2->popup( cb2->mapToGlobal( QPoint( 0, cb2->height() ) ) );
						QApplication::processEvents();
						cm2->grab().save( outPath + QStringLiteral( ".panel-running.png" ) );
						cm2->close();
						QApplication::processEvents();
					}
				}


				/* Every other mode has to be able to take over.
				 *
				 * This is the bug bungo hit: Physics Sim was added without the
				 * change signal every other mode has, so Pose and the paint modes
				 * never left it and Object/Edit left it without the button noticing.
				 * Each one is entered from a RUNNING sim, which is the case that broke.
				 */
				struct ModeCase { const char * name; std::function<void()> enter; };
				const ModeCase modes[] = {
					{ "Edit", [gl]() { gl->setEditMode( true ); } },
					{ "Pose", [gl]() { gl->setPoseMode( true ); } },
					{ "Vertex Paint", [gl]() { gl->setVertexPaintMode( true ); } },
					{ "Segment Paint", [gl]() { gl->setSegmentPaintMode( true ); } },
				};
				for ( const ModeCase & mc : modes ) {
					gl->setPhysicsSimMode( true );
					if ( !pv.active() )
						continue;
					mc.enter();
					check( !pv.active(), QStringLiteral( "%1 Mode leaves Physics Sim" )
						.arg( QLatin1String( mc.name ) ) );
					gl->setEditMode( false );
					gl->setPoseMode( false );
					gl->setVertexPaintMode( false );
					gl->setSegmentPaintMode( false );
				}
				// and the signal that keeps the mode button honest
				{
					int fired = 0;
					auto conn = QObject::connect( gl, &GLView::physicsSimModeChanged,
						gl, [&fired]( bool ) { fired++; } );
					gl->setPhysicsSimMode( true );
					gl->setPhysicsSimMode( false );
					QObject::disconnect( conn );
					check( fired == 2, QStringLiteral( "entering and leaving each announce it" ) );
				}

				/* Two homes for one panel, and the split between them.
				 *
				 * The dropdown must stay SHORT -- that is the whole reason the rest
				 * moved out, so a check that both panels merely exist would miss the
				 * regression that matters. The manager's copy must carry the controls
				 * the dropdown dropped, and its Create/Test switch must show exactly
				 * one of the two at a time.
				 */
				{
					// the mode-signal block above left the sim stopped, and a panel
					// syncing against nothing exercises none of what it does
					gl->setPhysicsSimMode( true );
					PhysicsSimPanel * quick = skope->findChild<PhysicsSimPanel *>(
						QStringLiteral( "CollisionQuickPanel" ) );
					PhysicsSimPanel * full = skope->findChild<PhysicsSimPanel *>(
						QStringLiteral( "CollisionTestPanel" ) );
					check( quick && full,
						QStringLiteral( "the toolbar and the manager each have a panel" ) );
					if ( quick && full ) {
						/* Counted by what is actually SHOWN, not by what exists: the
						 * essentials panel builds every control so one sync path can
						 * refresh them all, and hides the ones it does not offer. A
						 * count of children would report the two as identical.
						 */
						auto shown = []( QWidget * root ) {
							int n = 0;
							for ( QWidget * c : root->findChildren<QWidget *>() ) {
								/* Walk UP to the panel: isHidden() is per-widget, so the
								 * children of a hidden row still report themselves visible
								 * and the two panels came out 93 against 97 -- a metric
								 * that would have passed whatever the split did.
								 */
								bool visible = true;
								for ( QWidget * p = c; p && p != root; p = p->parentWidget() )
									if ( p->isHidden() ) {
										visible = false;
										break;
									}
								if ( visible )
									n++;
							}
							return n;
						};
						const int q = shown( quick ), f = shown( full );
						log << QStringLiteral( "      dropdown offers %1 controls, the manager %2" )
							.arg( q ).arg( f );

						/* Checked by WHICH sections each carries, not by a ratio.
						 *
						 * A ratio was the first attempt and it stopped meaning anything
						 * the moment World moved into the dropdown and the body list
						 * left the manager: the two counts converged to 43 against 50
						 * while the split itself was working exactly as intended. The
						 * invariant is that each panel carries the sections it is for.
						 */
						auto carries = []( QWidget * root, const QString & label ) {
							for ( QWidget * c : root->findChildren<QWidget *>() ) {
								const QLabel * l = qobject_cast<QLabel *>( c );
								const QAbstractButton * b = qobject_cast<QAbstractButton *>( c );
								const QString text = l ? l->text() : ( b ? b->text() : QString() );
								if ( text == label && c->isVisibleTo( root ) )
									return true;
							}
							return false;
						};
						// both: the world is watched while a rig is running
						check( carries( quick, QStringLiteral( "World" ) )
							&& carries( full, QStringLiteral( "World" ) ),
							QStringLiteral( "both panels carry the world controls" ) );
						// manager only: set once per file, and never mid-drag
						check( !carries( quick, QStringLiteral( "Solver" ) )
							&& carries( full, QStringLiteral( "Solver" ) ),
							QStringLiteral( "only the manager carries the solver knobs" ) );
						check( !carries( quick, QStringLiteral( "Advanced" ) )
							&& carries( full, QStringLiteral( "Advanced" ) ),
							QStringLiteral( "and the advanced ones, collapsed" ) );
						// the things that moved out, named rather than counted
						/* The pins moved OUT of the panel and into the manager's tree,
						 * so the panel must no longer carry a list of its own.
						 */
						check( full->findChildren<QListWidget *>().isEmpty(),
							QStringLiteral( "the panel has no body list of its own" ) );
						bool checkable = false;
						for ( QTreeWidget * tw : skope->findChildren<QTreeWidget *>() )
							for ( int r = 0; r < tw->topLevelItemCount(); r++ )
								if ( tw->topLevelItem( r )->data( 0, Qt::CheckStateRole ).isValid() )
									checkable = true;
						check( checkable,
							QStringLiteral( "and the manager tree carries the pins instead" ) );
						/* ...and NOT the tool row, which belongs to the toolbar. Checked
						 * by what is shown rather than by what exists: the manager's
						 * copy still builds the buttons so one sync path can drive both.
						 */
						bool toolShown = false;
						for ( QPushButton * b : full->findChildren<QPushButton *>() )
							// isVisibleTo, not isHidden: the ROW is what gets hidden, and
							// a button inside a hidden row still reports itself shown
							if ( b->text() == QStringLiteral( "Grab" ) && b->isVisibleTo( full ) )
								toolShown = true;
						check( !toolShown,
							QStringLiteral( "and does not repeat the tool row" ) );

						PhysicsPreview & pvq = gl->physicsSim();
						quick->sync();
						full->sync();
						check( pvq.active(), QStringLiteral( "syncing neither panel disturbs the sim" ) );
					}

					// the Create/Test switch shows one section at a time
					QToolButton * createTab = nullptr;
					QToolButton * testTab = nullptr;
					for ( QToolButton * b : skope->findChildren<QToolButton *>() ) {
						if ( b->text() == QStringLiteral( "Collision Creation" ) )
							createTab = b;
						else if ( b->text() == QStringLiteral( "Collision Simulation" ) )
							testTab = b;
					}
					check( createTab && testTab,
						QStringLiteral( "the manager has a Create/Test switch" ) );
					if ( createTab && testTab && full ) {
						QGroupBox * createBox = nullptr;
						for ( QGroupBox * g : skope->findChildren<QGroupBox *>() )
							if ( g->title() == QStringLiteral( "Collision Creation" ) )
								createBox = g;
						testTab->click();
						QApplication::processEvents();
						check( !full->isHidden() && ( !createBox || createBox->isHidden() ),
							QStringLiteral( "Test shows the simulator and hides Create" ) );
						// the manager's own copy, photographed: the checks above say the
						// right widgets are up, not that they are laid out legibly
						if ( QDockWidget * cmd = skope->findChild<QDockWidget *>(
								QStringLiteral( "CollisionManagerDock" ) ) ) {
							cmd->show();
							QApplication::processEvents();
							cmd->grab().save( outPath + QStringLiteral( ".manager-test.png" ) );
						}
						createTab->click();
						QApplication::processEvents();
						check( full->isHidden() && ( !createBox || !createBox->isHidden() ),
							QStringLiteral( "and Create puts it back" ) );
					}
				}

				check( gl->setPhysicsSimMode( false ) == false, QStringLiteral( "mode left" ) );
				check( !pv.active(), QStringLiteral( "preview stopped" ) );

				log << QStringLiteral( "%1 of %2 checks passed" )
					.arg( log.size() - failures ).arg( log.size() );
				QFile f( outPath );
				if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) )
					f.write( log.join( QStringLiteral( "\n" ) ).toUtf8() + "\n" );
				qApp->quit();
			} );
		} );
	}

	// RENDER REGRESSION BASELINE (WW_RENDER_SHOT=<out.png>): grab the GL
	// framebuffer for one NIF and quit, so a driver script can walk a corpus and
	// pixel-diff before/after a shader change. This is the guard for the particle
	// simulation and screen-space refraction — both are easy to break from the
	// lighting path and neither has any other automated check.
	//
	// Determinism is the whole point, so two things are pinned rather than left
	// to whatever was persisted or to wall-clock:
	//   * camera — setOrientation( recenter ) instead of the per-file stored
	//     camera, or every baseline is framed differently;
	//   * scene time — the particle sim is time-driven (CPU NiPSysUpdateCtlr),
	//     so a wall-clock grab can never reproduce the same pixels.
	// WW_RENDER_TIME (seconds, default 1.0) and WW_RENDER_VIEW (ViewState index,
	// default ViewFront) override them.
	//
	// Uses ogl->grabFramebuffer(), NOT skope->grab(): the viewport is a native
	// window, so the widget grab returns white where the 3D content should be.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_RENDER_SHOT" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool, QString & ) {
			QTimer::singleShot( 2500, skope, [skope]() {
				QString out = qEnvironmentVariable( "WW_RENDER_SHOT" );
				if ( out.isEmpty() || out == QStringLiteral( "1" ) )
					out = QApplication::applicationDirPath() + "/ww_render_shot.png";

				// Pin the window size. The framebuffer follows the restored
				// window geometry, so baselines captured in one session compared
				// as "size mismatch" against another — the guard silently
				// stopped guarding. WW_RENDER_SIZE=WxH overrides.
				{
					int rw = 1280, rh = 800;
					const QString szEnv = qEnvironmentVariable( "WW_RENDER_SIZE" );
					if ( szEnv.contains( QLatin1Char( 'x' ) ) ) {
						const QStringList parts = szEnv.split( QLatin1Char( 'x' ) );
						if ( parts.size() == 2 ) {
							rw = qMax( parts.at( 0 ).toInt(), 320 );
							rh = qMax( parts.at( 1 ).toInt(), 240 );
						}
					}
					skope->showNormal();
					skope->resize( rw, rh );
					// Hide every dock. Pinning the WINDOW size is not enough: the GL
					// viewport gets whatever the docks leave it, so any change to the
					// persisted dock layout silently resizes the framebuffer and every
					// baseline turns into "size mismatch". Adding the Skeleton Manager
					// dock did exactly that (517 -> 695 px tall). Hiding them makes the
					// framebuffer depend only on WW_RENDER_SIZE, and hands the guard the
					// whole window for scene pixels.
					for ( QDockWidget * dw : skope->findChildren<QDockWidget *>() )
						dw->hide();
					/* And the viewport header, which is NOT a dock.
					 *
					 * It sits inside the central widget, directly under the GL
					 * surface, so it takes its height out of the framebuffer for
					 * the same reason the docks do - and the loop above cannot
					 * see it. Left visible it shifts every baseline by the bar's
					 * height and all seven turn into "size mismatch", which reads
					 * as a rendering regression rather than a layout change.
					 */
					if ( skope->viewportHeader )
						skope->viewportHeader->hide();
					qApp->processEvents();
				}

				if ( skope->ogl ) {
					int	viewIdx = qEnvironmentVariableIntValue( "WW_RENDER_VIEW" );
					if ( viewIdx <= 0 || viewIdx > int( GLView::ViewWalk ) )
						viewIdx = int( GLView::ViewFront );
					skope->ogl->setOrientation( GLView::ViewState( viewIdx ), true );

					// Force the two Viewport Effects toggles ON. They default to
					// true in Scene but are overwritten from the persisted menu
					// state at startup, so with them unchecked the refraction and
					// particle cases render identically to a plain mesh and the
					// regression set silently guards nothing. A harness must
					// exercise the code path, not the user's preferences.
					if ( Scene * sc = skope->ogl->getScene() ) {
						sc->showRefraction = true;
						sc->showParticles = true;
					}

					// WW_RENDER_SEQ=<name> selects a sequence. Without it the
					// scene takes animGroups.first(), which on FO4 VFX files is
					// the one-shot "autoPlay" — sequence-gated effects (the
					// procedural lightning's Generation keys) never fire there.
					const QString seq = qEnvironmentVariable( "WW_RENDER_SEQ" );
					if ( !seq.isEmpty() )
						skope->ogl->setSceneSequence( seq );

					bool	timeOk = false;
					float	t = qEnvironmentVariable( "WW_RENDER_TIME" ).toFloat( &timeOk );
					skope->ogl->setSceneTime( timeOk ? t : 1.0f );

					// grabFramebuffer() reads the CURRENT buffer without
					// repainting — pump twice or the grab is a stale frame.
					// Warm the line path. Streaming LINE geometry (grid, origin axes, 3D
					// cursor) draws nothing until a pick render has run - the open 07-17
					// defect. Whether that had happened varied per run, so the grid was in
					// some captures and absent from others and the guard could not be
					// trusted. indexAt() runs the pick render, making it deterministically
					// on. A workaround for that defect, not a fix - remove when it is solved.
					skope->ogl->indexAt( QPointF( skope->ogl->width() * 0.5,
												  skope->ogl->height() * 0.5 ) );
					qApp->processEvents();
					for ( int i = 0; i < 2; i++ ) {
						skope->ogl->update();
						qApp->processEvents();
					}
					skope->ogl->grabFramebuffer().save( out );
					// TEMP DIAGNOSTIC (WW_GRID_PROBE): bracket the grab so the log
					// shows which paintGL frames precede it, and whether any grid
					// draw belongs to the frame actually captured.
					if ( qEnvironmentVariableIsSet( "WW_GRID_PROBE" ) ) {
						QFile pf( QApplication::applicationDirPath() + "/ww_grid_probe.log" );
						if ( pf.open( QIODevice::Append | QIODevice::Text ) )
							QTextStream( &pf ) << ">> GRAB done (image saved) <<\n";
					}
				}
				qApp->quit();
			} );
		} );
	}

	/* TEST HARNESS (WW_ARCHIVEBROWSE_TEST=<folder>): does loading a nif destroy a
	 * browsed archive tree?
	 *
	 * bungo: "Loading any nif from the nif browser resets the NIF folder tree."
	 *
	 * WW_BROWSER_TEST below already covers the CONFIGURED tree, and it passes:
	 * an unchanged (game, resource paths) signature takes a fast path that reuses
	 * the tree entirely. That is why this needed a second harness rather than an
	 * extra assertion — the reported symptom lives in the other mode, which the
	 * first one never enters.
	 *
	 * Browse mode sets currentArchivePath, which makes `configuredIndexLive`
	 * false forever, so the fast path can never be taken and every load fell
	 * through to the full teardown. Two things are asserted, and the second is
	 * the one that matters more: the tree survives, AND currentArchiveNames is
	 * still populated. The teardown cleared that too, and
	 * openArchiveFileString() returns immediately when it is empty — so the
	 * first load from a browsed archive also made every later file in it
	 * unopenable, which no amount of looking at the tree would have shown.
	 *
	 * Log: release/ww_archivebrowse_test.log
	 */
	if ( qEnvironmentVariableIsSet( "WW_ARCHIVEBROWSE_TEST" ) ) {
		const QString browseTarget = qEnvironmentVariable( "WW_ARCHIVEBROWSE_TEST" );
		QTimer::singleShot( 1500, skope, [skope, browseTarget]() {
			QFile logf( QApplication::applicationDirPath() + "/ww_archivebrowse_test.log" );
			if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) ) { qApp->quit(); return; }
			QTextStream log( &logf );
			int checks = 0, fails = 0;
			auto check = [&]( const QString & what, bool pass ) {
				checks++;
				if ( !pass ) fails++;
				log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
			};
			do {
				if ( skope->dBrowser ) { skope->dBrowser->show(); skope->dBrowser->raise(); }
				QApplication::processEvents();

				log << "browsing: " << browseTarget << "\n";
				skope->openArchive( browseTarget );
				QApplication::processEvents();

				QTreeView * view = skope->bsaView;
				if ( !view || !view->model() ) { log << "no browser view\n"; break; }
				const int rowsBefore = view->model()->rowCount();
				const int namesBefore = skope->currentArchiveNames.size();
				log << "archive path set: " << !skope->currentArchivePath.isEmpty()
					<< ", top-level rows " << rowsBefore
					<< ", archive names " << namesBefore << "\n";
				/* A run that never entered browse mode proves nothing, so it is a
				 * FAILURE and not a skip. The first version of this harness
				 * `break`-ed here and reported "0 checks, 0 failures / PASS" --
				 * a green result from a test that had not run.
				 */
				check( "the browse target opened in archive mode",
					!skope->currentArchivePath.isEmpty() && rowsBefore > 0 );
				if ( skope->currentArchivePath.isEmpty() || rowsBefore == 0 ) {
					log << "nothing to test: point WW_ARCHIVEBROWSE_TEST at a Data "
						   "folder or a .ba2\n";
					break;
				}

				// a live handle into the tree: a real rebuild invalidates it
				QPersistentModelIndex anchor( view->model()->index( 0, 0 ) );

				/* Fire the exact signal a finished load fires. The bug is in the
				 * handler connected to it, so this is the real path -- and it
				 * avoids depending on which file happens to be in the archive.
				 */
				QString dummy;
				emit skope->completeLoading( true, dummy );
				QApplication::processEvents();
				QEventLoop settle;
				QTimer::singleShot( 400, &settle, &QEventLoop::quit );
				settle.exec();

				log << "after load: archive path set: " << !skope->currentArchivePath.isEmpty()
					<< ", top-level rows " << view->model()->rowCount()
					<< ", archive names " << skope->currentArchiveNames.size()
					<< ", anchor still valid " << anchor.isValid() << "\n";

				check( "the browsed tree survives a load", anchor.isValid() );
				check( "the browser is still in archive mode",
					!skope->currentArchivePath.isEmpty() );
				check( "the archive can still be opened from",
					skope->currentArchiveNames.size() == namesBefore
					&& !skope->currentArchiveNames.isEmpty() );
			} while ( false );
			log << checks << " checks, " << fails << " failures\n";
			log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
			logf.close();
			QTimer::singleShot( 0, qApp, &QApplication::quit );
		} );
	}

	// TEMP DIAGNOSTIC (WW_BROWSER_TEST=1): confirm the Available-NIFs tree keeps
	// its expanded folders across the load-triggered repopulate. Expands the first
	// folder, repopulates (exactly what completeLoading does), and checks the
	// folder is still open. -> ww_browser_test.log, quit.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_BROWSER_TEST" ) ) {
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool, QString & ) {
			QTimer::singleShot( 2500, skope, [skope]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_browser_test.log" );
				if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) ) { qApp->quit(); return; }
				QTextStream log( &logf );
				// The browser dock starts tabified behind the Header dock, so the
				// load-time populate is deferred by the visibility gate. Show the
				// dock and populate directly to build the tree under test.
				if ( skope->dBrowser ) { skope->dBrowser->show(); skope->dBrowser->raise(); }
				skope->populateConfiguredNifBrowser();
				QTreeView * view = skope->bsaView;
				QAbstractItemModel * m = view ? view->model() : nullptr;
				if ( !view || !m ) { log << "no browser view/model\n"; qApp->quit(); return; }
				auto findRoot = [&]() -> QModelIndex {
					for ( int r = 0; r < m->rowCount(); ++r ) {
						QModelIndex idx = m->index( r, 0 );
						if ( m->data( idx ).toString() == QLatin1String( "Available NIFs" ) )
							return idx;
					}
					return QModelIndex();
				};
				QModelIndex root = findRoot();
				if ( !root.isValid() ) { log << "no Available NIFs root\n"; qApp->quit(); return; }
				view->expand( root );
				QModelIndex folder;
				for ( int r = 0; r < m->rowCount( root ); ++r ) {
					QModelIndex idx = m->index( r, 0, root );
					if ( m->hasChildren( idx ) ) { folder = idx; break; }
				}
				if ( !folder.isValid() ) { log << "no folder under Available NIFs (browser empty?)\n"; qApp->quit(); return; }
				const QString folderText = m->data( folder ).toString();
				view->expand( folder );
				const bool before = view->isExpanded( folder );
				log << "folder='" << folderText << "' expanded before repopulate=" << before << "\n";
				// Full-rebuild path: clear the cache signatures so this populate
				// really re-scans and rebuilds (what Refresh / changed settings do).
				skope->nifBrowserIndexSignature.clear();
				skope->nifBrowserTreeSignature.clear();
				skope->populateConfiguredNifBrowser();
				QModelIndex root2 = findRoot();
				bool after = false;
				if ( root2.isValid() ) {
					for ( int r = 0; r < m->rowCount( root2 ); ++r ) {
						QModelIndex idx = m->index( r, 0, root2 );
						if ( m->data( idx ).toString() == folderText ) { after = view->isExpanded( idx ); break; }
					}
				}
				log << "folder='" << folderText << "' expanded after full rebuild=" << after << "\n";
				// Fast path: an unchanged-signature populate (what loading a nif now
				// triggers) must reuse the tree — a persistent index into it
				// survives; a real rebuild (removeRows + insertRow) kills it.
				QPersistentModelIndex rootBefore( findRoot() );
				skope->populateConfiguredNifBrowser();
				const bool fastPath = rootBefore.isValid();
				log << "unchanged-signature populate reused tree=" << fastPath << "\n";
				log << ( ( before && after && fastPath ) ? "PASS\n" : "FAIL\n" );
				qApp->quit();
			} );
		} );
	}

	// TEMP DIAGNOSTIC (WW_PERF_TEST=1, remove when the slow click-select is
	// fixed): after the file loads, time every stage of the click-select
	// pipeline on the largest BSTriShape, dump to ww_perf_test.log, quit.
	if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_PERF_TEST" ) ) {
		// launch -> loaded wall time (includes window construction and the
		// synchronous UI-thread NIF parse)
		auto launchTimer = std::make_shared<QElapsedTimer>();
		launchTimer->start();
		QObject::connect( skope, &NifSkope::beginLoading, skope, [launchTimer]() {
			QFile f( QApplication::applicationDirPath() + "/ww_perf_test.log" );
			if ( f.open( QIODevice::Append | QIODevice::Text ) )
				QTextStream( &f ) << "[launch->beginLoading: " << launchTimer->elapsed() << " ms]\n";
		} );
		QObject::connect( skope, &NifSkope::completeLoading, skope, [launchTimer]( bool, QString & ) {
			QFile f( QApplication::applicationDirPath() + "/ww_perf_test.log" );
			if ( f.open( QIODevice::Append | QIODevice::Text ) )
				QTextStream( &f ) << "[launch->completeLoading: " << launchTimer->elapsed() << " ms]\n";
		} );
		QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
			// let startup painting/shader compiles settle first
			QTimer::singleShot( 2000, skope, [skope, ok]() {
				QFile logf( QApplication::applicationDirPath() + "/ww_perf_test.log" );
				if ( !logf.open( QIODevice::Append | QIODevice::Text ) )
					return;
				QTextStream log( &logf );
				NifModel * nif = skope->getNifModel();
				do {
					if ( !ok || !nif ) { log << "load failed\n"; break; }
					int sb = -1, sbVerts = -1;
					for ( int b = 0; b < nif->getBlockCount(); b++ ) {
						QModelIndex iB = nif->getBlockIndex( b );
						if ( !nif->blockInherits( iB, "BSTriShape" ) )
							continue;
						int nv = nif->get<int>( iB, "Num Vertices" );
						if ( nv > sbVerts ) { sbVerts = nv; sb = b; }
					}
					if ( sb < 0 ) { log << "no BSTriShape\n"; break; }
					log << "largest shape: block " << sb << " verts " << sbVerts << "\n";
					QElapsedTimer t;
					auto stamp = [&]( const char * name ) {
						log << name << ": " << t.elapsed() << " ms\n";
						log.flush();
						t.restart();
					};
					t.start();
					skope->ogl->grabFramebuffer().save(
						QApplication::applicationDirPath() + "/ww_perf_baseline.png" );
					stamp( "baseline paint (grabFramebuffer)" );
					// grid/axis rendering in the axis-aligned ortho views
					skope->ogl->handleBlenderNumpad( Qt::Key_7, Qt::KeypadModifier, true );
					qApp->processEvents();
					skope->ogl->grabFramebuffer().save(
						QApplication::applicationDirPath() + "/ww_perf_orthotop.png" );
					skope->ogl->handleBlenderNumpad( Qt::Key_1, Qt::KeypadModifier, true );
					qApp->processEvents();
					skope->ogl->grabFramebuffer().save(
						QApplication::applicationDirPath() + "/ww_perf_orthofront.png" );
					skope->ogl->handleBlenderNumpad( Qt::Key_7,
						Qt::KeypadModifier | Qt::ControlModifier, true );
					qApp->processEvents();
					log << "  bottom: Rot=" << skope->ogl->Rot[0] << "," << skope->ogl->Rot[1]
						<< "," << skope->ogl->Rot[2]
						<< " view=" << int( skope->ogl->view )
						<< " persp=" << int( skope->ogl->perspectiveMode )
						<< " axisState=" << int( skope->ogl->axisAlignedViewState() ) << "\n";
					log.flush();
					skope->ogl->grabFramebuffer().save(
						QApplication::applicationDirPath() + "/ww_perf_orthobottom.png" );
					skope->ogl->handleBlenderNumpad( Qt::Key_5, Qt::KeypadModifier, true );
					qApp->processEvents();
					stamp( "ortho view grabs" );
					QPointF center( skope->ogl->width() / 2.0, skope->ogl->height() / 2.0 );
					QModelIndex pickIdx = skope->ogl->indexAt( center );
					stamp( "GLView::indexAt(center) pick render" );
					skope->ogl->grabFramebuffer().save(
						QApplication::applicationDirPath() + "/ww_perf_afterpick.png" );
					stamp( "paint after pick render only" );
					log << "  picked block: "
						<< ( pickIdx.isValid() ? nif->getBlockNumber( pickIdx ) : -1 ) << "\n";
					// does flushing the geometry cache alone unlock the grid?
					// (tests the "broken first upload cached forever" theory)
					if ( skope->ogl->getScene()->renderer ) {
						auto prvCx = skope->ogl->pushGLContext();
						skope->ogl->getScene()->renderer->flushCache();
						skope->ogl->popGLContext( prvCx );
					}
					skope->ogl->update();
					qApp->processEvents();
					skope->ogl->grabFramebuffer().save(
						QApplication::applicationDirPath() + "/ww_perf_afterflush.png" );
					stamp( "paint after flushCache only" );
					// does merely making scene->currentBlock valid unlock the grid?
					skope->ogl->getScene()->currentBlock = nif->getBlockIndex( sb );
					skope->ogl->update();
					qApp->processEvents();
					skope->ogl->grabFramebuffer().save(
						QApplication::applicationDirPath() + "/ww_perf_curblockonly.png" );
					stamp( "paint with only scene->currentBlock set" );
					skope->ogl->getScene()->currentBlock = QPersistentModelIndex();
					skope->ogl->update();
					qApp->processEvents();
					skope->ogl->grabFramebuffer().save(
						QApplication::applicationDirPath() + "/ww_perf_curblockcleared.png" );
					stamp( "paint after clearing currentBlock again" );
					skope->ogl->objectSelectClick( sb, false );
					stamp( "objectSelectClick(largest)" );
					qApp->processEvents();
					stamp( "processEvents after objectSelectClick" );
					skope->ogl->grabFramebuffer().save(
						QApplication::applicationDirPath() + "/ww_perf_afterosc.png" );
					stamp( "paint after objectSelectClick only" );
					skope->select( nif->getBlockIndex( sb ) );
					stamp( "NifSkope::select(largest)" );
					qApp->processEvents();
					stamp( "processEvents after select" );
					skope->ogl->grabFramebuffer().save(
						QApplication::applicationDirPath() + "/ww_perf_withsel.png" );
					stamp( "paint with selection" );
					// second round: switch away and back, like a user clicking around
					skope->select( nif->getBlockIndex( 0 ) );
					qApp->processEvents();
					stamp( "select(block 0) + events" );
					skope->ogl->objectSelectClick( sb, false );
					stamp( "objectSelectClick #2" );
					skope->select( nif->getBlockIndex( sb ) );
					qApp->processEvents();
					stamp( "select(largest) #2 + events" );
					skope->ogl->grabFramebuffer();
					stamp( "paint #2" );
					// does the grid survive a full deselect? (distinguishes a
					// per-frame selection-gated pass from one-time lazy init)
					skope->ogl->objectSelectClick( -1, false );
					qApp->processEvents();
					skope->ogl->grabFramebuffer().save(
						QApplication::applicationDirPath() + "/ww_perf_deselected.png" );
					stamp( "paint after deselect" );
					// the Block List click path (what the sweep probe used)
					if ( skope->list->model() == skope->proxy )
						skope->list->setCurrentIndex( skope->proxy->mapFrom( nif->getBlockIndex( sb ), QModelIndex() ) );
					else
						skope->list->setCurrentIndex( nif->getBlockIndex( sb ) );
					qApp->processEvents();
					stamp( "Block List setCurrentIndex(largest) + events" );

					// ---- frame-time benchmark: object mode, then edit mode ----
					// slight camera rotation between frames defeats any
					// content-identical caching, like a user orbiting
					auto benchFrames = [&]( const char * name, int frames ) {
						skope->ogl->grabFramebuffer();	// warm
						QElapsedTimer bt;
						bt.start();
						for ( int i = 0; i < frames; i++ ) {
							skope->ogl->rotate( 0.0f, 0.0f, 0.7f );
							skope->ogl->grabFramebuffer();
						}
						log << name << ": " << ( double( bt.elapsed() ) / frames )
							<< " ms/frame over " << frames << "\n";
						log.flush();
					};
					// pure parse cost without any attached views: the delta to
					// launch->completeLoading is Qt model/view reaction overhead
					{
						NifModel bare( skope );
						bare.setMessageMode( BaseModel::MSG_TEST );
						QElapsedTimer pt;
						pt.start();
						const bool ok2 = bare.loadFromFile( skope->currentFile );
						log << "bare NifModel parse: " << pt.elapsed()
							<< " ms ok=" << int( ok2 ) << "\n";
						log.flush();
					}
					// attribute the overhead: reload attached, then peel the
					// consumers off one by one (probe quits after, no restore)
					{
						QElapsedTimer rt;
						rt.start();
						skope->nif->loadFromFile( skope->currentFile );
						log << "reload fully attached: " << rt.elapsed() << " ms\n";
						log.flush();
						skope->tree->setModel( skope->nifEmpty );
						skope->header->setModel( skope->nifEmpty );
						rt.restart();
						skope->nif->loadFromFile( skope->currentFile );
						log << "reload w/o details+header trees: " << rt.elapsed() << " ms\n";
						log.flush();
						skope->list->setModel( skope->proxyEmpty );
						rt.restart();
						skope->nif->loadFromFile( skope->currentFile );
						log << "reload w/o block list view too: " << rt.elapsed() << " ms\n";
						log.flush();
						skope->proxy->setModel( skope->nifEmpty );
						rt.restart();
						skope->nif->loadFromFile( skope->currentFile );
						log << "reload w/o proxy model too: " << rt.elapsed() << " ms\n";
						log.flush();
					}
					benchFrames( "bench object mode", 30 );
					skope->select( nif->getBlockIndex( sb ) );
					qApp->processEvents();
					skope->ogl->setEditMode( true );
					qApp->processEvents();
					if ( skope->ogl->editMode ) {
						benchFrames( "bench edit mode (no selection)", 30 );
						skope->ogl->selectAll( 1 );
						qApp->processEvents();
						benchFrames( "bench edit mode (all selected)", 30 );
						skope->ogl->selectAll( 2 );
						skope->ogl->setEditMode( false );
						qApp->processEvents();
					} else {
						log << "edit mode did not engage\n";
					}
				} while ( false );
				logf.close();
				QTimer::singleShot( 0, qApp, &QApplication::quit );
			} );
		} );
	}

	if ( !fname.isEmpty() ) {
		skope->loadFile( fname );
	} else if ( !background && startupCubeWanted() ) {
		// Blender opens on a cube rather than on nothing, and this editor has the
		// same problem an empty document has: nowhere to click. Add Primitive
		// itself refuses without an existing BSTriShape, since it clones one for
		// its vertex layout and material.
		NifModel * nif = skope->getNifModel();
		QString error;
		/* Bracket the build in beginLoading/completeLoading, exactly as loadFile()
		 * does, and in that order.
		 *
		 * Everything that reacts to a document arriving hangs off these, and
		 * building the model directly skips all of it. But they MUST be paired:
		 * onLoadBegin() and onLoadComplete() each call swapModels(), which TOGGLES
		 * the views between the real models and the empty ones used to keep them
		 * quiet during a load. Emitting only completeLoading — which is what this
		 * did at first — runs that toggle an odd number of times and leaves the
		 * views bound to nifEmpty, so Block Details showed a header with
		 * "Num Blocks 0" and a stale version instead of the selected block.
		 */
		QString noFile;
		emit skope->beginLoading();
		const bool built = nif && nifCreateStarterScene( nif, STARTER_CUBE_SIZE, &error );
		if ( built && nif->undoStack ) {
			// the starting state, not an edit: an untouched window must not ask
			// about saving on close
			nif->undoStack->clear();
			nif->undoStack->setClean();
		}
		emit skope->completeLoading( built, noFile );
		if ( built ) {
			if ( skope->ogl ) {
				skope->ogl->updateScene();
				skope->ogl->frameAll();
			}
			// WW_STARTER_SHOT=<png>: prove the document startup builds actually
			// renders, then quit. The startup path cannot be checked any other way
			// without leaving a window up.
			const QString starterShot = qEnvironmentVariable( "WW_STARTER_SHOT" );
			if ( !starterShot.isEmpty() && skope->ogl ) {
				QTimer::singleShot( 0, skope, [skope, starterShot, nif]() {
					QFile logf( QApplication::applicationDirPath() + "/ww_starter_test.log" );
					if ( logf.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
						QTextStream log( &logf );
						log << "blocks " << nif->getBlockCount()
							<< "  version " << nif->getVersion()
							<< "  user " << nif->getUserVersion()
							<< "  bs " << nif->getBSVersion() << "\n";
						for ( int b = 0; b < nif->getBlockCount(); b++ )
							log << "  [" << b << "] " << nif->itemName( nif->getBlockIndex( b ) )
								<< " '" << nif->get<QString>( nif->getBlockIndex( b ), "Name" ) << "'\n";
						log << "clean " << ( nif->undoStack ? int( nif->undoStack->isClean() ) : -1 ) << "\n";
						logf.close();
					}
					skope->ogl->setOrientation( GLView::ViewFront, true );
					// same line-path warm-up the skeleton shot needs (07-17 defect)
					skope->ogl->indexAt( QPointF( skope->ogl->width() * 0.5,
												   skope->ogl->height() * 0.5 ) );
					qApp->processEvents();
					for ( int i = 0; i < 3; i++ ) {
						skope->ogl->update();
						qApp->processEvents();
					}
					skope->ogl->grabFramebuffer().save( starterShot );
					QTimer::singleShot( 0, qApp, &QApplication::quit );
				} );
			}
		} else if ( !error.isEmpty() ) {
			qWarning() << "starter scene:" << error;
		}
	}
	if ( primary ) {
		skope->setGeometry( primary->geometry() );
		if ( !background ) {
			// Ordinary Open/New commands create an actual foreground window. The
			// old hidden-tab session model used to conceal this window until the
			// current one closed, which made a successful browser open look broken.
			skope->show();
			skope->raise();
			skope->activateWindow();
		}
	}
	refreshAllDocumentSessions();

	return skope;
}

void NifSkope::initActions()
{
	aSanitize = ui->aSanitize;
	aList = ui->aList;
	aHierarchy = ui->aHierarchy;
	aCondition = ui->aCondition;
	aRCondition = ui->aRCondition;
	aSelectFont = ui->aSelectFont;

	// Build all actions list
	allActions = QSet<QAction *>();
	for ( auto i : ui->tFile->actions() )
		allActions.insert( i );
	for ( auto i : ui->mRender->actions() )
		allActions.insert( i );
	for ( auto i : ui->tRender->actions() )
		allActions.insert( i );

	// Undo/Redo
	undoAction = nif->undoStack->createUndoAction( this, tr( "&Undo" ) );
	undoAction->setShortcut( QKeySequence::Undo );
	undoAction->setObjectName( "aUndo" );
	undoAction->setIcon( QIcon( ":btn/undo" ) );
	allActions << undoAction;
	redoAction = nif->undoStack->createRedoAction( this, tr( "&Redo" ) );
	redoAction->setShortcut( QKeySequence::Redo );
	redoAction->setObjectName( "aRedo" );
	redoAction->setIcon( QIcon( ":btn/redo" ) );
	allActions << redoAction;

	/* Ctrl+Shift+P anywhere in the window.
	 *
	 * The menu's own Search… row is discoverability; THIS is the speed win — no
	 * right-click at all, against whichever block the list is on. It builds its
	 * own SpellBook rather than borrowing the context menu's, because there is no
	 * context menu open when this fires.
	 */
	QAction * paletteAction = new QAction( tr( "Search Spells…" ), this );
	paletteAction->setObjectName( QStringLiteral( "aSpellPalette" ) );
	paletteAction->setShortcut( QKeySequence( QStringLiteral( "Ctrl+Shift+P" ) ) );
	paletteAction->setShortcutContext( Qt::WindowShortcut );
	connect( paletteAction, &QAction::triggered, this, [this]() {
		if ( !nif )
			return;
		QModelIndex idx = currentNifIndex();
		SpellBook book( nif, idx, this, SLOT( select( const QModelIndex & ) ) );
		buildBlockListMenuExtras( book, idx );
		const int bn = nif->getBlockNumber( idx );
		const QString what = bn >= 0
			? QStringLiteral( "[%1] %2" ).arg( bn ).arg( nif->itemName( nif->getBlockIndex( bn ) ) )
			: QString();
		if ( QAction * run = wwSpellPalette( this, book, what ) )
			run->trigger();
	} );
	addAction( paletteAction );
	allActions << paletteAction;

	// TODO: Back/Forward button in Block List
	//idxForwardAction = indexStack->createRedoAction( this );
	//idxBackAction = indexStack->createUndoAction( this );

	/* Undo and Redo go in a MENU, not on the bar.
	 *
	 * Blender has no undo buttons in any header - it is Edit > Undo and Ctrl+Z -
	 * and these two were holding the most valuable spot on the row, immediately
	 * right of the menus, for a pair nobody clicks twice. The shortcuts are
	 * unchanged and are how anyone actually undoes anything.
	 *
	 * Options is where they go because Options is this program's Edit menu:
	 * Settings, Theme, Font. Put at the TOP, above a separator, which is where
	 * Blender's Edit menu keeps them.
	 */
	if ( ui->mOptions ) {
		QAction * first = ui->mOptions->actions().value( 0 );
		ui->mOptions->insertAction( first, undoAction );
		ui->mOptions->insertAction( first, redoAction );
		if ( first )
			ui->mOptions->insertSeparator( first );
	}

	connect( undoAction, &QAction::triggered, [this]( bool ) {
		ogl->update();
	} );

	connect( redoAction, &QAction::triggered, [this]( bool ) {
		ogl->update();
	} );

	ui->aSave->setShortcut( QKeySequence::Save );
	ui->aSaveAs->setShortcut( { "Ctrl+Alt+S" } );
	ui->aWindow->setShortcut( QKeySequence::New );

	// Blender-compatible numpad view navigation. Keep the keypad modifier so
	// the top-row number keys remain available for edit-mode element selection.
	ui->aViewTop->setShortcut( QKeySequence( QKeyCombination( Qt::KeypadModifier, Qt::Key_7 ) ) );
	ui->aViewFront->setShortcut( QKeySequence( QKeyCombination( Qt::KeypadModifier, Qt::Key_1 ) ) );
	ui->aViewLeft->setShortcut( QKeySequence( QKeyCombination(
		Qt::ControlModifier | Qt::KeypadModifier, Qt::Key_3 ) ) );
	ui->aViewFlip->setShortcut( QKeySequence( QKeyCombination( Qt::KeypadModifier, Qt::Key_9 ) ) );
	ui->aViewPerspective->setShortcut( QKeySequence( QKeyCombination( Qt::KeypadModifier, Qt::Key_5 ) ) );

	connect( ui->aBrowseArchive, &QAction::triggered, this, &NifSkope::archiveDlg );
	connect( ui->aBrowseGameFolder, &QAction::triggered, this, &NifSkope::archiveFolderDlg );
	connect( ui->aOpen, &QAction::triggered, this, &NifSkope::openDlg );
	connect( ui->aSave, &QAction::triggered, this, &NifSkope::save );
	connect( ui->aSaveAs, &QAction::triggered, this, &NifSkope::saveAsDlg );

	ui->aReload->setDisabled(true);

	// TODO: Assure Actions and Scene state are synced
	// Set Data for Actions to pass onto Scene when clicking
	/*
		ShowAxes = 0x1,
		ShowGrid = 0x2,
		ShowNodes = 0x4,
		ShowCollision = 0x8,
		ShowConstraints = 0x10,
		ShowMarkers = 0x20,
		DoDoubleSided = 0x40,       // Not implemented
		DoVertexColors = 0x80,
		DoSpecular = 0x100,
		DoGlow = 0x200,
		DoTexturing = 0x400,
		DoBlending = 0x800,         // Not implemented
		DoMultisampling = 0x1000,   // Not implemented
		DoLighting = 0x2000,
		DoCubeMapping = 0x4000,
		DisableShaders = 0x8000,    // Not implemented
		ShowHidden = 0x10000
	*/

	ui->aShowAxes->setData( Scene::ShowAxes );
	ui->aShowGrid->setData( Scene::ShowGrid );
	ui->aShowNodes->setData( Scene::ShowNodes );
	ui->aShowCollision->setData( Scene::ShowCollision );
	ui->aShowConstraints->setData( Scene::ShowConstraints );
	ui->aShowMarkers->setData( Scene::ShowMarkers );
	ui->aShowHidden->setData( Scene::ShowHidden );
	ui->aDoSkinning->setData( Scene::DoSkinning );

	ui->aTextures->setData( Scene::DoTexturing );
	ui->aVertexColors->setData( Scene::DoVertexColors );
	ui->aSpecular->setData( Scene::DoSpecular );
	ui->aGlow->setData( Scene::DoGlow );
	ui->aCubeMapping->setData( Scene::DoCubeMapping );
	ui->aLighting->setData( Scene::DoLighting );

	ui->aSelectObject->setData( Scene::SelObject );
	ui->aSelectVertex->setData( Scene::SelVertex );

	auto agroup = [this]( QVector<QAction *> actions, bool exclusive ) {
		QActionGroup * ag = new QActionGroup( this );
		for ( auto a : actions ) {
			ag->addAction( a );
		}

		ag->setExclusive( exclusive );

		return ag;
	};

	selectActions = agroup( { ui->aSelectObject, ui->aSelectVertex }, true );
	connect( selectActions, &QActionGroup::triggered, ogl->getScene(), &Scene::updateSelectMode );

	showActions = agroup( { ui->aShowAxes, ui->aShowGrid, ui->aShowNodes, ui->aShowCollision,
						  ui->aShowConstraints, ui->aShowMarkers, ui->aShowHidden, ui->aDoSkinning
	}, false );
	connect( showActions, &QActionGroup::triggered, ogl->getScene(), &Scene::updateSceneOptionsGroup );
	connect( showActions, &QActionGroup::triggered, ogl, &GLView::updateScene );

	shadingActions = agroup( { ui->aTextures, ui->aVertexColors, ui->aSpecular, ui->aGlow, ui->aCubeMapping, ui->aLighting }, false );
	connect( shadingActions, &QActionGroup::triggered, ogl->getScene(), &Scene::updateSceneOptionsGroup );
	connect( shadingActions, &QActionGroup::triggered, ogl, &GLView::updateScene );

	// Sync actions to Scene state
	for ( auto a : showActions->actions() ) {
		a->setChecked( ogl->scene->options & a->data().toInt() );
	}

	// Sync actions to Scene state
	for ( auto a : shadingActions->actions() ) {
		a->setChecked( ogl->scene->options & a->data().toInt() );
	}

	// Setup blank QActions for Recent Files menus
	for ( int i = 0; i < NumRecentFiles; ++i ) {
		recentFileActs[i] = new QAction( this );
		recentArchiveActs[i] = new QAction( this );
		recentArchiveFileActs[i] = new QAction( this );

		recentFileActs[i]->setVisible( false );
		recentArchiveActs[i]->setVisible( false );
		recentArchiveFileActs[i]->setVisible( false );

		connect( recentFileActs[i], &QAction::triggered, this, &NifSkope::openRecentFile );
		connect( recentArchiveActs[i], &QAction::triggered, this, &NifSkope::openRecentArchive );
		connect( recentArchiveFileActs[i], &QAction::triggered, this, &NifSkope::openRecentArchiveFile );
	}

	aList->setChecked( list->model() == nif );
	aHierarchy->setChecked( list->model() == proxy );

	// Allow only List or Tree view to be selected at once
	gListMode = new QActionGroup( this );
	gListMode->addAction( aList );
	gListMode->addAction( aHierarchy );
	gListMode->setExclusive( true );
	connect( gListMode, &QActionGroup::triggered, this, &NifSkope::setListMode );

	connect( aCondition, &QAction::toggled, tree, &NifTreeView::setRowHiding );
	connect( aCondition, &QAction::toggled, kfmtree, &NifTreeView::setRowHiding );

	connect( ui->aAboutNifSkope, &QAction::triggered, []() {
		auto aboutDialog = new AboutDialog();
		aboutDialog->show();
	} );
	connect( ui->aAboutQt, &QAction::triggered, qApp, &QApplication::aboutQt );

	connect( ui->aPrintView, &QAction::triggered, ogl, &GLView::saveImage );

#ifdef QT_NO_DEBUG
	ui->aColorKeyDebug->setDisabled( true );
	ui->aColorKeyDebug->setVisible( false );
	ui->aBoundsDebug->setDisabled( true );
	ui->aBoundsDebug->setVisible( false );
#else
	QAction * debugNone = new QAction( this );

	QActionGroup * debugActions = agroup( { debugNone, ui->aColorKeyDebug, ui->aBoundsDebug }, false );
	connect( ui->aColorKeyDebug, &QAction::triggered, [this]( bool checked ) {
		if ( checked )
			ogl->setDebugMode( GLView::DbgColorPicker );
		else
			ogl->setDebugMode( GLView::DbgNone );

		ogl->update();
	} );

	connect( ui->aBoundsDebug, &QAction::triggered, [this]( bool checked ) {
		if ( checked )
			ogl->setDebugMode( GLView::DbgBounds );
		else
			ogl->setDebugMode( GLView::DbgNone );

		ogl->update();
	} );

	connect( debugActions, &QActionGroup::triggered, [=]( QAction * action ) {
		for ( auto a : debugActions->actions() ) {
			if ( a == action )
				continue;

			a->setChecked( false );
		}
	} );
#endif

	connect( ui->aSilhouette, &QAction::triggered, [this]( bool checked ) {
		ogl->setVisMode( Scene::VisSilhouette, checked );
		ogl->updateScene();
	} );

	connect( ui->aVisNormals, &QAction::triggered, [this]( bool checked ) {
		ogl->setVisMode( Scene::VisNormalsOnly, checked );
	} );

	connect( ogl, &GLView::clicked, this, &NifSkope::select );
	// viewport-hidden nodes (H) -> repaint the greyed block-list rows
	connect( ogl, &GLView::hiddenNodesChanged, [this]() {
		if ( list )
			list->viewport()->update();
		if ( tree )
			tree->viewport()->update();
	} );
	// object-mode multi-selection -> colour the matching block-list rows
	connect( ogl, &GLView::objectSelectionChanged, [this]() {
		QElapsedTimer perfT;	// TEMP DIAGNOSTIC (WW_PERF_TEST)
		perfT.start();
		auto perfMark = [&perfT]() {
			if ( qEnvironmentVariableIsSet( "WW_PERF_TEST" ) ) {
				QFile f( QApplication::applicationDirPath() + "/ww_perf_test.log" );
				if ( f.open( QIODevice::Append | QIODevice::Text ) )
					QTextStream( &f ) << "    [list mirror: " << perfT.elapsed() << " ms]\n";
			}
		};
		Q_UNUSED( perfMark );
		auto perfGuard = qScopeGuard( perfMark );
		if ( !nif )
			return;
		nif->selHighlight = ogl->objSelection;
		// the active node must always be highlighted, even if a selection-path
		// race left it out of the set
		if ( ogl->objActive >= 0 )
			nif->selHighlight.insert( ogl->objActive );
		nif->selHighlightActive = ogl->objActive;
		list->viewport()->update();
		tree->viewport()->update();

		// Mirror the object selection into the block list so the coloured rows
		// are actually visible (and the list tracks viewport clicks the way
		// Blender's outliner tracks the 3D view). Skip when the change already
		// came from the list (the list is correct, and re-driving it would jump
		// the current index to the block's palette copy and break Ctrl+click).
		if ( ogl->editMode || syncingObjToList || updatingObjFromList )
			return;
		syncingObjToList = true;
		QItemSelection selRows;
		QModelIndex activeProxy;
		for ( int b : ogl->objSelection ) {
			QModelIndex src = nif->getBlockIndex( b );
			if ( !src.isValid() )
				continue;
			QModelIndex p = proxy->mapFromPrimary( src );
			if ( !p.isValid() )
				continue;
			selRows.select( p, p );
			if ( b == ogl->objActive )
				activeProxy = p;
		}
		QItemSelectionModel * sm = list->selectionModel();
		sm->select( selRows, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows );
		if ( activeProxy.isValid() ) {
			sm->setCurrentIndex( activeProxy, QItemSelectionModel::NoUpdate );
			list->scrollTo( activeProxy );
		}
		syncingObjToList = false;
	} );
	connect( ogl, &GLView::sceneTimeChanged, inspect, &InspectView::updateTime );
	connect( ogl, &GLView::paintUpdate, inspect, &InspectView::refresh );
	connect( ogl, &GLView::projectionChanged, [this]( bool perspective ) {
		QSignalBlocker blocker( ui->aViewPerspective );
		ui->aViewPerspective->setChecked( perspective );
	} );
	connect( ogl, &GLView::viewpointChanged, [this]() {
		ui->aViewTop->setChecked( false );
		ui->aViewFront->setChecked( false );
		ui->aViewLeft->setChecked( false );
		ui->aViewUser->setChecked( false );

		ogl->setOrientation( GLView::ViewDefault, false );
	} );

	// The viewport has no context menu (right-click places the gizmo); the
	// Blender-style header menu buttons and W cover the viewport operations.

	// Update Inspector widget with current index
	connect( tree, &NifTreeView::sigCurrentIndexChanged, inspect, &InspectView::updateSelection );
}

void NifSkope::initDockWidgets()
{
	dRefr = ui->RefrDock;
	dList = ui->ListDock;
	dTree = ui->TreeDock;
	dHeader = ui->HeaderDock;
	dInsp = ui->InspectDock;
	dKfm = ui->KfmDock;
	dBrowser = ui->BrowserDock;

	// A populate requested while the NIF Browser was hidden (closed dock or a
	// background tab) was deferred; replay it when the dock actually shows.
	connect( dBrowser, &QDockWidget::visibilityChanged, this, [this]( bool visible ) {
		if ( visible && nifBrowserPopulatePending )
			QTimer::singleShot( 0, this, &NifSkope::populateConfiguredNifBrowser );
	} );

	// Animation Manager
	dTimeline = new QDockWidget( tr( "Animation Manager" ), this );
	dTimeline->setObjectName( "TimelineDock" );
	timeline = new TimelineWidget( dTimeline );
	// timeline buttons must not take keyboard focus, otherwise Tab (edit-mode
	// toggle) cycles the transport buttons after clicking one
	for ( QAbstractButton * b : timeline->findChildren<QAbstractButton *>() )
		b->setFocusPolicy( Qt::NoFocus );
	timeline->setNif( nif );
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

	// Re-dock at the bottom when reopened from the menu: a floating dock that
	// was closed can come back with its title bar off screen and become unmovable.
	// Only on the menu action's triggered — toggled also fires during the
	// drag-to-float transition and would make the dock impossible to detach.
	connect( dTimeline->toggleViewAction(), &QAction::triggered, [this]( bool on ) {
		if ( on && dTimeline->isFloating() ) {
			dTimeline->setFloating( false );
			addDockWidget( Qt::BottomDockWidgetArea, dTimeline );
		}
	} );

	/* The Animation dock's transport, routed into the ONE playback engine.
	 *
	 * No aAnimate gate: it made the dock's play button do nothing, silently,
	 * whenever View > Animations was off. aAnimPlay enables it itself.
	 *
	 * Direction is the SIGN OF THE ANIMATION SPEED, which is what GLView's loop
	 * already understands -- it wraps at whichever end the sign heads for, so Loop
	 * and Switch behave identically forwards and backwards. The dock used to keep
	 * a private playDir the renderer never saw.
	 */
	connect( timeline, &TimelineWidget::playPauseRequested, this, [this]( int dir ) {
		if ( dir == 0 ) {
			if ( ui->aAnimPlay->isChecked() )
				ui->aAnimPlay->trigger();
			return;
		}
		const float speed = std::fabs( ogl->animationSpeed() );
		ogl->setAnimSpeed( dir < 0 ? -speed : speed );
		if ( !ui->aAnimPlay->isChecked() )
			ui->aAnimPlay->trigger();
		timeline->setPlayingState( true, dir < 0 );
	} );

	/* ...and the buttons follow the APPLICATION, not their own clicks.
	 *
	 * Space, the menubar's Play, and a sequence ending without Loop all change
	 * playback without going through this dock. btnPlay's checked state used to
	 * track only its own clicks, so it drifted out of step with everything else.
	 */
	connect( ui->aAnimPlay, &QAction::toggled, timeline, [this]( bool on ) {
		timeline->setPlayingState( on, ogl->animationSpeed() < 0.0f );
	} );
	connect( ogl, &GLView::sequenceStopped, timeline, [this]() {
		timeline->setPlayingState( false, false );
	} );

	// Solo / preview-only rendering of the selected node
	QAction * aSolo = new QAction( tr( "Solo Selected" ), this );
	aSolo->setCheckable( true );
	aSolo->setShortcut( QKeySequence( Qt::ALT | Qt::Key_Q ) );
	aSolo->setToolTip( tr( "Render only the selected node's subtree, hiding all other geometry (Alt+Q)" ) );
	connect( aSolo, &QAction::toggled, ogl, &GLView::setSoloMode );
	ui->tRender->addAction( aSolo );
	// deliberately NOT in the Render menu: Solo lives in the display-toggles
	// dropdown; window-scope registration keeps Alt+Q working regardless
	addAction( aSolo );

	// Transform gizmo companions
	connect( ogl, &GLView::gizmoStatus, [this]( const QString & s ) {
		if ( s.isEmpty() )
			ui->statusbar->clearMessage();
		else
			ui->statusbar->showMessage( s );
	} );

	QAction * aAutoKey = new QAction( tr( "Auto-Key Transforms" ), this );
	aAutoKey->setCheckable( true );
	aAutoKey->setToolTip( tr( "After a gizmo transform (G/R/S in the viewport), key it on the Animation Manager's transform lane at the playhead" ) );
	connect( aAutoKey, &QAction::toggled, [this]( bool on ) { ogl->gizmoAutoKey = on; } );
	ui->mRender->addAction( aAutoKey );

	QAction * aGizmoSnap = new QAction( tr( "Gizmo Snap Distance..." ), this );
	connect( aGizmoSnap, &QAction::triggered, [this]() {
		bool ok = false;
		double v = QInputDialog::getDouble( this, tr( "Gizmo snap" ),
			tr( "Grid snap step for Ctrl-dragging the transform gizmo:" ), GLView::gizmoSnapStep, 0.001, 4096.0, 3, &ok );
		if ( ok )
			GLView::gizmoSnapStep = (float)v;
	} );
	ui->mRender->addAction( aGizmoSnap );

	// shared Blender-dark styling for the floating redo panels
	const QString redoPanelQss = QStringLiteral(
		"QFrame#GizmoRedoPanel, QFrame#OperatorRedoPanel, QFrame#BoxSelectRedoPanel, QFrame#OperatorExRedoPanel {"
		" background: %1; border: 1px solid %2; }"
		"QCheckBox { color: %3; background: transparent; }"
		"QLabel { color: %3; background: transparent; }"
		"QToolButton { color: %3; background: transparent; border: none; }"
		"QToolButton:hover { color: %4; }"
		"QPushButton { background: %5; color: %3; border: none; border-radius: 3px; padding: 3px 14px; }"
		"QPushButton:hover { background: %6; }"
		"QPushButton:pressed { background: %7; }"
		"QComboBox { background: %8; color: %3; border: none; border-radius: 3px; padding: 2px 6px; }" )
		.arg( wwSkinColor( "bgCard" ), wwSkinColor( "borderStrong" ), wwSkinColor( "text" ),
			  wwSkinColor( "textBright" ), wwSkinColor( "bgBtn" ), wwSkinColor( "bgBtnHover" ),
			  wwSkinColor( "bgBtnDown" ), wwSkinColor( "bgPanel" ) );

	// Blender-style redo panel: tweak the parameters of the last transform.
	// The GL viewport is a native window (createWindowContainer) that paints
	// over any child-widget overlay, so the panel must be a floating frameless
	// tool window positioned over the viewport instead of a child widget.
	{
		QFrame * rp = new QFrame( this, Qt::Tool | Qt::FramelessWindowHint );
		rp->setObjectName( QStringLiteral( "GizmoRedoPanel" ) );
		rp->setFrameShape( QFrame::StyledPanel );
		rp->setAutoFillBackground( true );
		rp->setStyleSheet( redoPanelQss );
		// pop up after a gesture without pulling focus off the viewport,
		// so chained G/R/S shortcuts keep working
		rp->setAttribute( Qt::WA_ShowWithoutActivating );
		rp->hide();
		gizmoRedoPanel = rp;

		// Blender operator panel layout: clickable "˅ Move" header that collapses
		// the body, one value row per component with a right-aligned label,
		// combos underneath
		QVBoxLayout * rpOuter = new QVBoxLayout( rp );
		rpOuter->setContentsMargins( 10, 8, 10, 8 );
		rpOuter->setSpacing( 4 );
		QHBoxLayout * rpHdr = new QHBoxLayout();
		QToolButton * rpTitle = new QToolButton( rp );
		rpTitle->setObjectName( QStringLiteral( "GizmoRedoTitle" ) );
		rpTitle->setAutoRaise( true );
		QFont tf = rpTitle->font();
		tf.setBold( true );
		rpTitle->setFont( tf );
		QToolButton * rpClose = new QToolButton( rp );
		rpClose->setObjectName( QStringLiteral( "GizmoRedoClose" ) );
		rpClose->setText( QStringLiteral( "✕" ) );
		rpClose->setAutoRaise( true );
		rpHdr->addWidget( rpTitle );
		rpHdr->addStretch( 1 );
		rpHdr->addWidget( rpClose );
		QWidget * rpBody = new QWidget( rp );
		QGridLayout * rpl = new QGridLayout( rpBody );
		rpl->setContentsMargins( 0, 0, 0, 0 );
		rpl->setHorizontalSpacing( 8 );
		rpl->setVerticalSpacing( 3 );
		rpOuter->addLayout( rpHdr );
		rpOuter->addWidget( rpBody );
		connect( rpClose, &QToolButton::clicked, rp, &QWidget::hide );
		connect( rpTitle, &QToolButton::clicked, [this, rp, rpTitle, rpBody]() {
			tlTogglePanelCollapse( rp, rpTitle, rpBody );
			positionRedoPanel();
		} );

		QLabel * rpLbl0 = new QLabel( rpBody ), * rpLbl1 = new QLabel( rpBody ), * rpLbl2 = new QLabel( rpBody );
		QDoubleSpinBox * rpVal0 = new WwNumberField( rpBody ), * rpVal1 = new WwNumberField( rpBody ), * rpVal2 = new WwNumberField( rpBody );
		QLabel * rpLbls[3] = { rpLbl0, rpLbl1, rpLbl2 };
		QDoubleSpinBox * rpVals[3] = { rpVal0, rpVal1, rpVal2 };
		for ( int i = 0; i < 3; i++ ) {
			rpLbls[i]->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
			rpVals[i]->setRange( -1.0e6, 1.0e6 );
			rpVals[i]->setDecimals( 4 );
			rpVals[i]->setKeyboardTracking( false );
			rpVals[i]->setMinimumWidth( 150 );
			rpl->addWidget( rpLbls[i], i, 0 );
			rpl->addWidget( rpVals[i], i, 1, 1, 2 );
		}

		// Blender operator panel extras: rotation axis + transform orientation
		QLabel * rpAxisLbl = new QLabel( tr( "Axis" ), rpBody );
		rpAxisLbl->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
		QComboBox * rpAxis = new QComboBox( rpBody );
		rpAxis->addItems( { tr( "View" ), QStringLiteral( "X" ), QStringLiteral( "Y" ), QStringLiteral( "Z" ) } );
		QLabel * rpOrientLbl = new QLabel( tr( "Orientation" ), rpBody );
		rpOrientLbl->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
		QComboBox * rpOrient = new QComboBox( rpBody );
		rpOrient->addItems( { tr( "Global" ), tr( "Local" ), tr( "Parent" ), tr( "View" ) } );
		rpl->addWidget( rpAxisLbl, 3, 0 );
		rpl->addWidget( rpAxis, 3, 1, 1, 2 );
		rpl->addWidget( rpOrientLbl, 4, 0 );
		rpl->addWidget( rpOrient, 4, 1, 1, 2 );

		auto applyEdit = [this, rp, rpVal0, rpVal1, rpVal2, rpAxis, rpOrient]() {
			if ( !rp->isVisible() )
				return;
			int mode = rp->property( "gestureMode" ).toInt();
			int axis = ( mode == 2 && rpAxis->isVisible() ) ? rpAxis->currentIndex() : -1;
			int orient = ( mode == 1 || mode == 2 ) ? rpOrient->currentIndex() : -1;
			if ( !ogl->gizmoReapply( Vector3( (float)rpVal0->value(), (float)rpVal1->value(), (float)rpVal2->value() ),
					axis, orient ) ) {
				// gesture went stale (something else touched the undo stack):
				// keep the panel visible but freeze its inputs (the title and
				// close buttons stay usable)
				for ( QWidget * w : rp->findChildren<QWidget *>() )
					if ( !w->inherits( "QToolButton" ) )
						w->setEnabled( false );
			}
		};
		for ( auto sb : { rpVal0, rpVal1, rpVal2 } )
			connect( sb, qOverload<double>( &QDoubleSpinBox::valueChanged ), applyEdit );
		for ( auto cb : { rpAxis, rpOrient } )
			connect( cb, qOverload<int>( &QComboBox::currentIndexChanged ), applyEdit );

		connect( ogl, &GLView::transformGesture,
			[this, rp, rpTitle, rpLbl0, rpLbl1, rpLbl2, rpVal0, rpVal1, rpVal2,
				rpAxisLbl, rpAxis, rpOrientLbl, rpOrient]( int mode, int axis, const Vector3 & p ) {
			QLabel * lbls[3] = { rpLbl0, rpLbl1, rpLbl2 };
			QDoubleSpinBox * vals[3] = { rpVal0, rpVal1, rpVal2 };

			for ( QWidget * w : rp->findChildren<QWidget *>() )
				w->setEnabled( true );	// a stale gesture froze them
			for ( auto sb : vals )
				sb->blockSignals( true );
			rpAxis->blockSignals( true );
			rpOrient->blockSignals( true );

			rp->setProperty( "gestureMode", mode );
			rp->setProperty( "gestureBlock", ogl->objActive );
			rpAxis->setCurrentIndex( std::min( std::max( axis, 0 ), 3 ) );
			rpOrient->setCurrentIndex( std::min( std::max( ogl->gizmoOrient, 0 ), 3 ) );

			bool three = ( mode == 1 );
			if ( mode == 1 ) {
				tlSetPanelTitle( rp, rpTitle, tr( "Move" ) );
				const char * comps[3] = { QT_TR_NOOP( "Move X" ), "Y", "Z" };
				for ( int i = 0; i < 3; i++ ) {
					lbls[i]->setText( tr( comps[i] ) );
					vals[i]->setValue( p[i] );
				}
			} else if ( mode == 2 ) {
				tlSetPanelTitle( rp, rpTitle, tr( "Rotate" ) );
				lbls[0]->setText( tr( "Angle°" ) );
				vals[0]->setValue( p[0] );
			} else {
				tlSetPanelTitle( rp, rpTitle, tr( "Scale (uniform)" ) );
				lbls[0]->setText( tr( "Factor" ) );
				vals[0]->setValue( p[0] );
			}

			for ( int i = 1; i < 3; i++ ) {
				lbls[i]->setVisible( three );
				vals[i]->setVisible( three );
			}
			// axis dropdown only applies to rotations; orientation to move + rotate
			rpAxisLbl->setVisible( mode == 2 );
			rpAxis->setVisible( mode == 2 );
			rpOrientLbl->setVisible( mode == 1 || mode == 2 );
			rpOrient->setVisible( mode == 1 || mode == 2 );

			for ( auto sb : vals )
				sb->blockSignals( false );
			rpAxis->blockSignals( false );
			rpOrient->blockSignals( false );

			rp->adjustSize();
			positionRedoPanel();
			rp->show();
			rp->raise();
		} );

		// Blender keeps the operator panel up until the selection moves on:
		// hide only when a different object becomes active (or none)
		connect( ogl, &GLView::objectSelectionChanged, [this, rp]() {
			if ( rp->isVisible() && ogl->objActive != rp->property( "gestureBlock" ).toInt() )
				rp->hide();
		} );

		connect( this, &NifSkope::completeLoading, rp, &QWidget::hide );
	}

	// Operator redo panel: readjust the last Merge by Distance, Select Linked
	// by Angle, or Floating Decal offset. Same floating-tool-window rules as above.
	{
		QFrame * op = new QFrame( this, Qt::Tool | Qt::FramelessWindowHint );
		op->setObjectName( QStringLiteral( "OperatorRedoPanel" ) );
		op->setFrameShape( QFrame::StyledPanel );
		op->setAutoFillBackground( true );
		op->setStyleSheet( redoPanelQss );
		op->setAttribute( Qt::WA_ShowWithoutActivating );
		op->hide();
		operatorRedoPanel = op;

		QVBoxLayout * opOuter = new QVBoxLayout( op );
		opOuter->setContentsMargins( 10, 8, 10, 8 );
		opOuter->setSpacing( 4 );
		QHBoxLayout * opHdr = new QHBoxLayout();
		QToolButton * opTitle = new QToolButton( op );
		opTitle->setObjectName( QStringLiteral( "OperatorRedoTitle" ) );
		opTitle->setAutoRaise( true );
		QFont otf = opTitle->font();
		otf.setBold( true );
		opTitle->setFont( otf );
		QToolButton * opClose = new QToolButton( op );
		opClose->setObjectName( QStringLiteral( "OperatorRedoClose" ) );
		opClose->setText( QStringLiteral( "✕" ) );
		opClose->setAutoRaise( true );
		opHdr->addWidget( opTitle );
		opHdr->addStretch( 1 );
		opHdr->addWidget( opClose );
		QWidget * opBody = new QWidget( op );
		QGridLayout * opl = new QGridLayout( opBody );
		opl->setContentsMargins( 0, 0, 0, 0 );
		opl->setHorizontalSpacing( 8 );
		opOuter->addLayout( opHdr );
		opOuter->addWidget( opBody );
		connect( opClose, &QToolButton::clicked, op, &QWidget::hide );
		connect( opTitle, &QToolButton::clicked, [this, op, opTitle, opBody]() {
			tlTogglePanelCollapse( op, opTitle, opBody );
			positionRedoPanel();
		} );

		QLabel * opLbl = new QLabel( opBody );
		opLbl->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
		QDoubleSpinBox * opVal = new WwNumberField( opBody );
		opVal->setKeyboardTracking( false );
		opVal->setMinimumWidth( 150 );
		opl->addWidget( opLbl, 0, 0 );
		opl->addWidget( opVal, 0, 1 );

		connect( opVal, qOverload<double>( &QDoubleSpinBox::valueChanged ), [this, op]( double v ) {
			if ( !op->isVisible() )
				return;
			if ( !ogl->reapplyOperator( (float)v ) ) {
				// gesture went stale (something else touched the undo stack):
				// keep the panel visible but freeze its inputs (the title and
				// close buttons stay usable)
				for ( QWidget * w : op->findChildren<QWidget *>() )
					if ( !w->inherits( "QToolButton" ) )
						w->setEnabled( false );
			}
		} );
		connect( opVal, &QDoubleSpinBox::editingFinished, ogl, &GLView::commitOperatorPreview );

		connect( ogl, &GLView::operatorPanel, [this, op, opTitle, opLbl, opVal]( int kind, float param ) {
			for ( QWidget * w : op->findChildren<QWidget *>() )
				w->setEnabled( true );	// a stale gesture froze them
			opVal->blockSignals( true );
			op->setProperty( "operatorKind", kind );
			op->setProperty( "operatorBlock", ogl->objActive );
			if ( kind == 1 ) {
				tlSetPanelTitle( op, opTitle, tr( "Merge by Distance" ) );
				opLbl->setText( tr( "Distance" ) );
				opVal->setSuffix( QString() );
				opVal->setDecimals( 4 );
				opVal->setRange( 0.0, 1.0e6 );
				opVal->setSingleStep( 0.01 );
			} else if ( kind == 2 ) {
				tlSetPanelTitle( op, opTitle, tr( "Select Linked" ) );
				opLbl->setText( tr( "Sharpness" ) );
				opVal->setSuffix( QStringLiteral( "°" ) );
				opVal->setDecimals( 1 );
				opVal->setRange( 0.0, 180.0 );
				opVal->setSingleStep( 1.0 );
			} else {
				tlSetPanelTitle( op, opTitle, tr( "Floating Decal" ) );
				opLbl->setText( tr( "Offset" ) );
				opVal->setSuffix( QString() );
				opVal->setDecimals( 4 );
				opVal->setRange( -1.0e6, 1.0e6 );
				opVal->setSingleStep( 0.01 );
			}
			opVal->setValue( param );
			opVal->blockSignals( false );
			if ( gizmoRedoPanel )
				gizmoRedoPanel->hide();	// one panel at a time, Blender-style
			op->adjustSize();
			positionRedoPanel();
			op->show();
			op->raise();
		} );

		// a transform gesture supersedes the operator panel (and its own show
		// handler above hides the transform panel, so only one is ever up)
		connect( ogl, &GLView::transformGesture, op, [op]() { op->hide(); } );
		// Merge and Select Linked live in edit mode. Floating Decal finishes in
		// object mode, but re-entering edit mode invalidates its generated target.
		connect( ogl, &GLView::editModeChanged, op, [op]( bool editing ) {
			if ( !editing || op->property( "operatorKind" ).toInt() == 3 )
				op->hide();
		} );
		connect( ogl, &GLView::objectSelectionChanged, op, [this, op]() {
			if ( op->isVisible() && op->property( "operatorKind" ).toInt() == 3
				&& ogl->objActive != op->property( "operatorBlock" ).toInt() )
				op->hide();
		} );
		connect( this, &NifSkope::completeLoading, op, &QWidget::hide );
	}

	// Box-select redo panel: after a box select (which only ever adds), offer
	// to Deselect the same rectangle's contents instead
	{
		QFrame * bp = new QFrame( this, Qt::Tool | Qt::FramelessWindowHint );
		bp->setObjectName( QStringLiteral( "BoxSelectRedoPanel" ) );
		bp->setFrameShape( QFrame::StyledPanel );
		bp->setAutoFillBackground( true );
		bp->setStyleSheet( redoPanelQss );
		bp->setAttribute( Qt::WA_ShowWithoutActivating );
		bp->hide();
		boxRedoPanel = bp;

		QVBoxLayout * bpOuter = new QVBoxLayout( bp );
		bpOuter->setContentsMargins( 10, 8, 10, 8 );
		bpOuter->setSpacing( 4 );
		QHBoxLayout * bpHdr = new QHBoxLayout();
		QToolButton * bpTitle = new QToolButton( bp );
		bpTitle->setObjectName( QStringLiteral( "BoxRedoTitle" ) );
		bpTitle->setAutoRaise( true );
		QFont btf = bpTitle->font();
		btf.setBold( true );
		bpTitle->setFont( btf );
		tlSetPanelTitle( bp, bpTitle, tr( "Box Select" ) );
		QToolButton * bpClose = new QToolButton( bp );
		bpClose->setText( QStringLiteral( "✕" ) );
		bpClose->setAutoRaise( true );
		bpHdr->addWidget( bpTitle );
		bpHdr->addStretch( 1 );
		bpHdr->addWidget( bpClose );
		QWidget * bpBody = new QWidget( bp );
		QVBoxLayout * bpl = new QVBoxLayout( bpBody );
		bpl->setContentsMargins( 0, 0, 0, 0 );
		bpOuter->addLayout( bpHdr );
		bpOuter->addWidget( bpBody );
		connect( bpClose, &QToolButton::clicked, bp, &QWidget::hide );
		connect( bpTitle, &QToolButton::clicked, [this, bp, bpTitle, bpBody]() {
			tlTogglePanelCollapse( bp, bpTitle, bpBody );
			positionRedoPanel();
		} );

		QPushButton * bpDeselect = new QPushButton( tr( "Deselect" ), bpBody );
		bpDeselect->setToolTip( tr( "Deselect what the box / brush stroke just selected instead" ) );
		bpl->addWidget( bpDeselect );
		connect( bpDeselect, &QPushButton::clicked, [this]() { ogl->deselectLastGesture(); } );

		auto showGesturePanel = [this, bp, bpTitle]( const QString & title ) {
			tlSetPanelTitle( bp, bpTitle, title );
			if ( gizmoRedoPanel )
				gizmoRedoPanel->hide();
			if ( operatorRedoPanel )
				operatorRedoPanel->hide();
			bp->adjustSize();
			positionRedoPanel();
			bp->show();
			bp->raise();
		};
		connect( ogl, &GLView::boxSelectApplied, [showGesturePanel]() {
			showGesturePanel( tr( "Box Select" ) );
		} );
		connect( ogl, &GLView::circleSelectApplied, [showGesturePanel]() {
			showGesturePanel( tr( "Circle Select" ) );
		} );
		// superseded by the other panels, and stale once the mode changes
		connect( ogl, &GLView::transformGesture, bp, [bp]() { bp->hide(); } );
		connect( ogl, &GLView::operatorPanel, bp, [bp]() { bp->hide(); } );
		connect( ogl, &GLView::operatorPanelEx, bp, [bp]() { bp->hide(); } );
		connect( ogl, &GLView::editModeChanged, bp, [bp]() { bp->hide(); } );
		connect( this, &NifSkope::completeLoading, bp, &QWidget::hide );
	}

	// Generalized operator redo panel (Redo Panel v2, MODELING_TOOLS_PLAN
	// F0.a): a typed parameter list — floats/ints as WwNumberFieldes, bools as
	// checkboxes, enums as dropdowns — driving GLView::reapplyOperatorEx.
	// First consumer: Extrude Region and Move.
	{
		constexpr int MAXP = 8;
		QFrame * xp = new QFrame( this, Qt::Tool | Qt::FramelessWindowHint );
		xp->setObjectName( QStringLiteral( "OperatorExRedoPanel" ) );
		xp->setFrameShape( QFrame::StyledPanel );
		xp->setAutoFillBackground( true );
		xp->setStyleSheet( redoPanelQss );
		xp->setAttribute( Qt::WA_ShowWithoutActivating );
		xp->hide();
		operatorExRedoPanel = xp;

		QVBoxLayout * xpOuter = new QVBoxLayout( xp );
		xpOuter->setContentsMargins( 10, 8, 10, 8 );
		xpOuter->setSpacing( 4 );
		QHBoxLayout * xpHdr = new QHBoxLayout();
		QToolButton * xpTitle = new QToolButton( xp );
		xpTitle->setObjectName( QStringLiteral( "OperatorExRedoTitle" ) );
		xpTitle->setAutoRaise( true );
		QFont xtf = xpTitle->font();
		xtf.setBold( true );
		xpTitle->setFont( xtf );
		QToolButton * xpClose = new QToolButton( xp );
		xpClose->setText( QStringLiteral( "✕" ) );
		xpClose->setAutoRaise( true );
		xpHdr->addWidget( xpTitle );
		xpHdr->addStretch( 1 );
		xpHdr->addWidget( xpClose );
		QWidget * xpBody = new QWidget( xp );
		QGridLayout * xpl = new QGridLayout( xpBody );
		xpl->setContentsMargins( 0, 0, 0, 0 );
		xpl->setHorizontalSpacing( 8 );
		xpl->setVerticalSpacing( 3 );
		xpOuter->addLayout( xpHdr );
		xpOuter->addWidget( xpBody );
		connect( xpClose, &QToolButton::clicked, xp, &QWidget::hide );
		connect( xpTitle, &QToolButton::clicked, [this, xp, xpTitle, xpBody]() {
			tlTogglePanelCollapse( xp, xpTitle, xpBody );
			positionRedoPanel();
		} );

		// pre-created rows, one control of each type per row (shown per param)
		QVector<QLabel *> xLbls;
		QVector<QDoubleSpinBox *> xSpins;
		QVector<QCheckBox *> xChecks;
		QVector<QComboBox *> xCombos;
		for ( int i = 0; i < MAXP; i++ ) {
			QLabel * lb = new QLabel( xpBody );
			lb->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
			QDoubleSpinBox * sp = new WwNumberField( xpBody );
			sp->setKeyboardTracking( false );
			sp->setMinimumWidth( 150 );
			QCheckBox * cb = new QCheckBox( xpBody );
			QComboBox * cx = new QComboBox( xpBody );
			xpl->addWidget( lb, i, 0 );
			xpl->addWidget( sp, i, 1, 1, 2 );
			xpl->addWidget( cb, i, 1, 1, 2 );
			xpl->addWidget( cx, i, 1, 1, 2 );
			lb->hide(); sp->hide(); cb->hide(); cx->hide();
			xLbls << lb; xSpins << sp; xChecks << cb; xCombos << cx;
		}

		// the armed params (labels/types/ranges); values re-read from widgets
		auto exParams = std::make_shared<QVector<GLView::TlOpParam>>();
		auto collect = [exParams, xSpins, xChecks, xCombos]() {
			QVector<GLView::TlOpParam> ps = *exParams;
			for ( int i = 0; i < ps.size() && i < xSpins.size(); i++ ) {
				switch ( ps[i].type ) {
				case GLView::TlOpParam::Bool:
					ps[i].value = xChecks[i]->isChecked() ? 1.0 : 0.0;
					break;
				case GLView::TlOpParam::Enum:
					ps[i].value = double( xCombos[i]->currentIndex() );
					break;
				default:
					ps[i].value = xSpins[i]->value();
					break;
				}
			}
			return ps;
		};
		auto applyEx = [this, xp, collect]() {
			if ( !xp->isVisible() )
				return;
			if ( !ogl->reapplyOperatorEx( collect() ) ) {
				// stale gesture: freeze the inputs, keep title/close usable
				for ( QWidget * w : xp->findChildren<QWidget *>() )
					if ( !w->inherits( "QToolButton" ) )
						w->setEnabled( false );
			}
		};
		for ( QDoubleSpinBox * sp : std::as_const( xSpins ) )
			connect( sp, qOverload<double>( &QDoubleSpinBox::valueChanged ), applyEx );
		for ( QCheckBox * cb : std::as_const( xChecks ) )
			connect( cb, &QCheckBox::toggled, applyEx );
		for ( QComboBox * cx : std::as_const( xCombos ) )
			connect( cx, qOverload<int>( &QComboBox::currentIndexChanged ), applyEx );

		connect( ogl, &GLView::operatorPanelEx,
			[this, xp, xpTitle, xLbls, xSpins, xChecks, xCombos, exParams](
				const QString & title, const QVector<GLView::TlOpParam> & params ) {
			*exParams = params;
			for ( QWidget * w : xp->findChildren<QWidget *>() )
				w->setEnabled( true );	// a stale gesture froze them
			for ( int i = 0; i < MAXP; i++ ) {
				const bool on = ( i < params.size() );
				xLbls[i]->setVisible( on );
				xSpins[i]->hide();
				xChecks[i]->hide();
				xCombos[i]->hide();
				if ( !on )
					continue;
				const GLView::TlOpParam & p = params.at( i );
				xLbls[i]->setText( p.label );
				if ( p.type == GLView::TlOpParam::Bool ) {
					QSignalBlocker blocker( xChecks[i] );
					xChecks[i]->setChecked( p.value != 0.0 );
					xChecks[i]->show();
				} else if ( p.type == GLView::TlOpParam::Enum ) {
					QSignalBlocker blocker( xCombos[i] );
					xCombos[i]->clear();
					xCombos[i]->addItems( p.enumNames );
					xCombos[i]->setCurrentIndex( int( p.value ) );
					xCombos[i]->show();
				} else {
					QSignalBlocker blocker( xSpins[i] );
					xSpins[i]->setDecimals( p.type == GLView::TlOpParam::Int ? 0 : p.decimals );
					xSpins[i]->setRange( p.mn, p.mx );
					xSpins[i]->setSingleStep( p.step );
					xSpins[i]->setValue( p.value );
					xSpins[i]->show();
				}
			}
			tlSetPanelTitle( xp, xpTitle, title );
			if ( gizmoRedoPanel )
				gizmoRedoPanel->hide();	// one panel at a time, Blender-style
			if ( operatorRedoPanel )
				operatorRedoPanel->hide();
			if ( boxRedoPanel )
				boxRedoPanel->hide();
			xp->adjustSize();
			xp->resize( xp->sizeHint() );
			positionRedoPanel();
			xp->show();
			xp->raise();
		} );

		// superseded by the other panels, and stale once the mode changes
		connect( ogl, &GLView::transformGesture, xp, [xp]() { xp->hide(); } );
		connect( ogl, &GLView::operatorPanel, xp, [xp]() { xp->hide(); } );
		connect( ogl, &GLView::boxSelectApplied, xp, [xp]() { xp->hide(); } );
		connect( ogl, &GLView::circleSelectApplied, xp, [xp]() { xp->hide(); } );
		connect( ogl, &GLView::editModeChanged, xp, [xp]() { xp->hide(); } );
		connect( this, &NifSkope::completeLoading, xp, &QWidget::hide );
	}

	// F9 (Blender): move the visible adjust panel next to the mouse cursor
	connect( ogl, &GLView::redoPanelToCursor, this, [this]() {
		for ( QFrame * p : { gizmoRedoPanel, operatorRedoPanel, boxRedoPanel, operatorExRedoPanel } ) {
			if ( !p || !p->isVisible() )
				continue;
			QPoint pos = QCursor::pos() - QPoint( 16, p->height() + 12 );
			if ( QScreen * sc = QGuiApplication::screenAt( QCursor::pos() ) ) {
				const QRect avail = sc->availableGeometry();
				pos.setX( std::clamp( pos.x(), avail.left(), avail.right() - p->width() ) );
				pos.setY( std::clamp( pos.y(), avail.top(), avail.bottom() - p->height() ) );
			}
			p->move( pos );
			return;
		}
	} );

	// per-object edit-mode selections are remembered only until a new file loads.
	// The hide state and quad-diagonal marks are keyed by block number, so they
	// MUST go too: a shape in the next file can land on the same block number
	// (hiddenTris is consulted by the normal render — stale entries silently
	// punched holes in freshly loaded meshes).
	connect( this, &NifSkope::completeLoading, [this]() {
		ogl->savedElemSelections.clear();
		ogl->editHiddenTris.clear();
		ogl->quadDiagonals.clear();
		ogl->quadMarkVerts.clear();
		ogl->quadPartnerCache.clear();
		ogl->mirrorPairCache.clear();
		if ( ogl->scene )
			ogl->scene->hiddenTris.clear();
	} );

	// A selection restored while the file was still parsing bails out of the
	// row-hiding pass (model state != Default) and a later select() of the
	// same block skips setRootIndex — re-apply once the load has completed so
	// version-mismatched rows (Skyrim fields on FO4 NIFs) never stay stranded.
	connect( this, &NifSkope::completeLoading, [this]() {
		// Hidden views can skip this: NifTreeView::doItemsLayout re-derives the
		// row hiding before the first layout when the dock is next shown.
		if ( tree && tree->isVisible() )
			tree->refreshRowHiding();
		if ( header && header->isVisible() )
			header->refreshRowHiding();
	} );

	// Gizmo/cursor scale, wireframe thickness, vertex point size and selection
	// line width now live in Options > Settings > Render > Viewport Display;
	// GLView::updateSettings() reads them on apply.

	QAction * aGizmoHandles = new QAction( tr( "Show Transform Gizmo" ), this );
	aGizmoHandles->setCheckable( true );
	aGizmoHandles->setChecked( true );
	aGizmoHandles->setToolTip( tr( "Draw draggable move/rotate/scale handles on the selected node" ) );
	connect( aGizmoHandles, &QAction::toggled, [this]( bool on ) {
		ogl->gizmoHandlesOn = on;
		ogl->update();
	} );
	// not in the Render menu: Show Transform Gizmo lives in the display-
	// toggles dropdown (duplicate entries confused more than they helped)

	// Blender-style transform orientation / pivot point selectors
	const QColor icoColHdr( wwSkinColor( "text" ) );
	QMenu * mOrient = new QMenu( tr( "Transform Orientation" ), this );
	QActionGroup * grpOrient = new QActionGroup( this );
	const char * orientNames[4] = {
		QT_TR_NOOP( "Global" ), QT_TR_NOOP( "Local" ), QT_TR_NOOP( "Parent" ), QT_TR_NOOP( "View" )
	};
	const char * orientIcons[4] = { "orient_global", "orient_local", "orient_parent", "orient_view" };
	for ( int i = 0; i < 4; i++ ) {
		QAction * a = mOrient->addAction( tlMakeIcon( QLatin1String( orientIcons[i] ), icoColHdr ), tr( orientNames[i] ) );
		a->setCheckable( true );
		a->setChecked( i == 0 );
		grpOrient->addAction( a );
		connect( a, &QAction::triggered, [this, i]() {
			ogl->gizmoOrient = i;
			ogl->update();
		} );
	}
	// (orientation lives on the toolbar; not duplicated in the Render menu)

	QMenu * mPivot = new QMenu( tr( "Transform Pivot Point" ), this );
	QActionGroup * grpPivot = new QActionGroup( this );
	const char * pivotNames[5] = {
		QT_TR_NOOP( "Node Origin" ), QT_TR_NOOP( "Bounding Box Center" ),
		QT_TR_NOOP( "Median Point" ), QT_TR_NOOP( "3D Cursor" ),
		QT_TR_NOOP( "Active Element (Last Selected)" )
	};
	const char * pivotIcons[5] = { "pivot_origin", "pivot_bounds", "pivot_median", "pivot_cursor", "pivot_bounds" };
	for ( int i = 0; i < 5; i++ ) {
		QAction * a = mPivot->addAction( tlMakeIcon( QLatin1String( pivotIcons[i] ), icoColHdr ), tr( pivotNames[i] ) );
		a->setCheckable( true );
		a->setChecked( i == 0 );
		grpPivot->addAction( a );
		connect( a, &QAction::triggered, [this, i]() {
			ogl->gizmoPivot = i;
			ogl->update();
		} );
	}
	// (pivot lives on the toolbar)

	// Snapping (Blender-style snap target for Ctrl-dragging). Settings persist.
	{
		QSettings s;
		ogl->snapTargetMode = s.value( "GLView/Snap/TargetMode", ogl->snapTargetMode ).toInt();
		ogl->snapBase = s.value( "GLView/Snap/Base", ogl->snapBase ).toInt();
		ogl->snapAffect = s.value( "GLView/Snap/Affect", ogl->snapAffect ).toInt();
		ogl->snapAlignRot = s.value( "GLView/Snap/AlignRot", ogl->snapAlignRot ).toBool();
		ogl->snapDefaultOn = s.value( "GLView/Snap/DefaultOn", ogl->snapDefaultOn ).toBool();
		GLView::gizmoSnapStep = s.value( "GLView/Snap/GridStep", GLView::gizmoSnapStep ).toFloat();
		GLView::gizmoRotSnapDeg = s.value( "GLView/Snap/RotStep", GLView::gizmoRotSnapDeg ).toFloat();
	}
	QMenu * mSnapTgt = new QMenu( tr( "Snap Target (Ctrl)" ), this );
	QActionGroup * grpSnapTgt = new QActionGroup( this );
	const char * snapNames[4] = {
		QT_TR_NOOP( "Grid Step" ), QT_TR_NOOP( "Vertex" ), QT_TR_NOOP( "Edge" ), QT_TR_NOOP( "Face" )
	};
	const char * snapIcons[4] = { "snap_increment", "vert", "edge", "face" };
	for ( int i = 0; i < 4; i++ ) {
		QAction * a = mSnapTgt->addAction( tlMakeIcon( QLatin1String( snapIcons[i] ), icoColHdr ), tr( snapNames[i] ) );
		a->setCheckable( true );
		a->setChecked( i == ogl->snapTargetMode );
		grpSnapTgt->addAction( a );
		connect( a, &QAction::triggered, [this, i]() {
			ogl->snapTargetMode = i;
			QSettings().setValue( "GLView/Snap/TargetMode", i );
		} );
	}
	mSnapTgt->addSeparator();
	QAction * aAlignRot = mSnapTgt->addAction( tr( "Align Rotation to Target" ) );
	aAlignRot->setCheckable( true );
	aAlignRot->setChecked( ogl->snapAlignRot );
	aAlignRot->setToolTip( tr( "When face snapping, orient the node's +Z to the surface normal" ) );
	connect( aAlignRot, &QAction::toggled, [this]( bool on ) {
		ogl->snapAlignRot = on;
		QSettings().setValue( "GLView/Snap/AlignRot", on );
	} );
	QAction * aRotSnap = mSnapTgt->addAction( tr( "Rotation Snap Angle..." ) );
	connect( aRotSnap, &QAction::triggered, [this]() {
		bool ok = false;
		double v = QInputDialog::getDouble( this, tr( "Rotation snap" ),
			tr( "Snap increment for Ctrl-rotating (degrees):" ), GLView::gizmoRotSnapDeg, 0.1, 180.0, 1, &ok );
		if ( ok ) {
			GLView::gizmoRotSnapDeg = (float)v;
			QSettings().setValue( "GLView/Snap/RotStep", (float)v );
		}
	} );
	// (snap target lives on the toolbar)

	// 3D cursor + element utilities
	QMenu * mCursor = new QMenu( tr( "3D Cursor && Elements" ), this );
	QAction * aShowCursor = mCursor->addAction( tr( "Show 3D Cursor" ) );
	aShowCursor->setCheckable( true );
	aShowCursor->setChecked( true );
	connect( aShowCursor, &QAction::toggled, [this]( bool on ) {
		ogl->showCursor = on;
		ogl->update();
	} );
	mCursor->addSeparator();
	mCursor->addAction( tr( "Snap Cursor to Picked" ), [this]() {
		if ( !ogl->pickedElems.isEmpty() ) {
			ogl->cursorPos = ogl->pickedMedian();
			ogl->update();
		}
	} );
	mCursor->addAction( tr( "Snap Cursor to World Origin" ), [this]() {
		ogl->cursorPos = Vector3();
		ogl->update();
	} );
	mCursor->addAction( tr( "Snap Node to Cursor" ), [this]() { ogl->snapNodeToCursor(); } );
	mCursor->addAction( tr( "Move Picked Vertices to Cursor" ), [this]() { ogl->movePickedVertsToCursor(); } );
	mCursor->addSeparator();
	mCursor->addAction( tr( "Select Linked (Ctrl+L)" ), [this]() { ogl->selectLinked( false ); } );
	mCursor->addAction( tr( "Select Linked Flat Faces" ), [this]() { ogl->selectLinked( true ); } );
	mCursor->addAction( tr( "Select Linked by Angle..." ), [this]() {
		// the operator redo panel that pops up lets you readjust the angle live
		ogl->selectLinked( true, ( ogl->lastOpKind == 2 ) ? ogl->lastOpParam : 30.0f );
	} );
	// deliberately NOT added to the Render menu: every entry here is also in
	// the viewport right-click menus (snap/select-linked) or the display
	// dropdown (Show 3D Cursor), and the duplication confused more than it
	// helped. The menu object stays alive as the owner of its actions.

	connect( ogl, &GLView::transformCommitted, timeline, &TimelineWidget::keyNodeTransform );

	// Space in the viewport toggles animation playback
	QAction * aPlaySpace = new QAction( this );
	aPlaySpace->setShortcut( QKeySequence( Qt::Key_Space ) );
	aPlaySpace->setShortcutContext( Qt::WidgetWithChildrenShortcut );
	graphicsView->addAction( aPlaySpace );
	connect( aPlaySpace, &QAction::triggered, [this]() {
		// see above: Space must not be a silent no-op either
		ui->aAnimPlay->trigger();
	} );

	// Blender Set Origin menu (Shift+Ctrl+Alt+C). A window-level QAction so it
	// fires from anywhere in the window (block list, viewport, ...), unlike a
	// viewport keyPressEvent which needs the GL view to hold keyboard focus.
	QAction * aSetOrigin = new QAction( this );
	aSetOrigin->setShortcut( QKeySequence( Qt::CTRL | Qt::SHIFT | Qt::ALT | Qt::Key_C ) );
	aSetOrigin->setShortcutContext( Qt::WindowShortcut );
	addAction( aSetOrigin );
	connect( aSetOrigin, &QAction::triggered, [this]() {
		ogl->showSetOriginMenu();
	} );

	// Tabify List and Header
	tabifyDockWidget( dList, dHeader );
	tabifyDockWidget( dHeader, dBrowser );

	// Raise List above Header
	dList->raise();

	// Hide certain docks by default
	dRefr->toggleViewAction()->setChecked( false );
	dInsp->toggleViewAction()->setChecked( false );
	dKfm->toggleViewAction()->setChecked( false );

	dRefr->setVisible( false );
	dInsp->setVisible( false );
	dKfm->setVisible( false );

	/* The View menu is gone; the Panels button is where all of it lives now.
	 *
	 * `View > Show` listed these same seven dock toggles - the SAME QAction
	 * objects the Panels menu adds below, not equivalents - so the two could not
	 * disagree, and neither could tell you which one you were supposed to use.
	 * Populating it here as well would only rebuild the door that was removed.
	 *
	 * The actions themselves are untouched: each belongs to its dock, so nothing
	 * is lost by one menu no longer listing them.
	 *
	 * Manager docks are selected from the Workspaces button, not the generic
	 * panel list.
	 */
	ui->menubar->removeAction( ui->mView->menuAction() );

	// ---- main toolbar overhaul: smaller icons, merged dropdowns, transform header ----

	// smaller icons everywhere (~75%); toolbars are fixed in place — the
	// dotted drag grips read as sliders and wasted row width, so group
	// boundaries are drawn by the thin 2px separator line instead
	for ( QToolBar * tb : { ui->tFile, ui->tRender, ui->tMode, ui->tView, ui->tLOD } ) {
		// no setIconSize here: setToolbarSize() runs later from restoreUi() and
		// overwrites whatever this set, so a second opinion here only confuses
		tb->setMovable( false );
		tb->setStyleSheet( QStringLiteral(
			"QToolBar::separator { background: %1; width: 2px; height: 2px; margin: 4px 6px; }" )
			.arg( wwSkinColor( "border" ) ) );
	}

	// open/save already live in the File menu; drop them from the toolbar
	ui->tFile->removeAction( ui->aOpenMenu );
	ui->tFile->removeAction( ui->aSaveMenu );

	// Viewport mode selector replaces the two select-mode buttons. Keep
	// this visually in the same family as the Panels / Workspaces selectors.
	{
		const QColor iconColor( wwSkinColor( "text" ) );
		// All viewport-mode glyphs are greyscale tlMakeIcon designs so the whole
		// selector reads as one consistent family (object cube, edit triangle,
		// vertex-paint dots, weight-paint brush, segment split).
		const QIcon objectIcon = tlMakeIcon( QStringLiteral( "mode_object" ), iconColor );
		const QIcon editIcon = tlMakeIcon( QStringLiteral( "mode_edit" ), iconColor );
		const QIcon poseIcon = tlMakeIcon( QStringLiteral( "mode_pose" ), iconColor );
		const QIcon vertexPaintIcon = tlMakeIcon( QStringLiteral( "mode_vertexpaint" ), iconColor );
		const QIcon weightPaintIcon = tlMakeIcon( QStringLiteral( "mode_weightpaint" ), iconColor );
		const QIcon segmentPaintIcon = tlMakeIcon( QStringLiteral( "mode_segment" ), iconColor );
		const QIcon physicsIcon = tlMakeIcon( QStringLiteral( "mode_physics" ), iconColor );
		QToolButton * modeButton = new QToolButton( this );
		modeButton->setObjectName( QStringLiteral( "ViewportModeButton" ) );
		modeButton->setPopupMode( QToolButton::InstantPopup );
		modeButton->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
		modeButton->setAutoRaise( false );
		modeButton->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "3px 6px" ) ) );
		modeButton->setToolTip( tr( "Viewport interaction mode. Tab toggles Object Mode and the last non-object mode." ) );

		QMenu * modeMenu = new QMenu( modeButton );
		QActionGroup * modeGroup = new QActionGroup( modeMenu );
		modeGroup->setExclusive( true );
		QAction * objectMode = modeMenu->addAction( objectIcon, tr( "Object Mode" ) );
		QAction * editMode = modeMenu->addAction( editIcon, tr( "Edit Mode" ) );
		QAction * poseMode = modeMenu->addAction( poseIcon, tr( "Pose Mode" ) );
		QAction * vertexPaintMode = modeMenu->addAction( vertexPaintIcon, tr( "Vertex Paint" ) );
		QAction * weightPaintMode = modeMenu->addAction( weightPaintIcon, tr( "Weight Paint" ) );
		QAction * segmentPaintMode = modeMenu->addAction( segmentPaintIcon, tr( "Segment Paint" ) );
		QAction * physicsMode = modeMenu->addAction( physicsIcon, tr( "Physics Sim" ) );
		objectMode->setObjectName( QStringLiteral( "ViewportObjectModeAction" ) );
		editMode->setObjectName( QStringLiteral( "ViewportEditModeAction" ) );
		poseMode->setObjectName( QStringLiteral( "ViewportPoseModeAction" ) );
		vertexPaintMode->setObjectName( QStringLiteral( "ViewportVertexPaintAction" ) );
		weightPaintMode->setObjectName( QStringLiteral( "ViewportWeightPaintAction" ) );
		segmentPaintMode->setObjectName( QStringLiteral( "ViewportSegmentPaintAction" ) );
		physicsMode->setObjectName( QStringLiteral( "ViewportPhysicsSimAction" ) );
		physicsMode->setToolTip( tr( "Run this file's ragdoll live. Drag a bone to pull it, "
			"Space pauses, R resets. Nothing is written back." ) );
		poseMode->setToolTip( tr( "Pose the skeleton: bones are drawn and clickable; click one then G/R/S to pose it." ) );
		vertexPaintMode->setToolTip( tr( "Paint per-vertex RGB colour or alpha on the active mesh." ) );
		segmentPaintMode->setToolTip( tr( "Paint binary face membership for FO4 segments and subsegments." ) );
		for ( QAction * action : { objectMode, editMode, poseMode, vertexPaintMode, weightPaintMode, segmentPaintMode, physicsMode } ) {
			action->setCheckable( true );
			modeGroup->addAction( action );
		}
		modeButton->setMenu( modeMenu );

		// constant width across every mode label, so the toolbar row doesn't
		// shift when the mode changes
		{
			/* Wide enough for the longest label and not a pixel more.
			 *
			 * The old form measured the TEXT with QFontMetrics and then added a
			 * hand-tuned 46 px for icon, padding, border and the menu
			 * indicator - which overshot, leaving a visible gap between the end
			 * of the text and the arrow. Setting each label in turn and asking
			 * the button for its own sizeHint gets the real figure from the
			 * style, including the indicator, with no magic number to drift.
			 */
			const QString saved = modeButton->text();
			modeButton->ensurePolished();
			const QFontMetrics fm( modeButton->font() );
			int wHint = 0, wText = 0;
			for ( const QString & s : { tr( "Object Mode" ), tr( "Edit Mode" ), tr( "Pose Mode" ),
					tr( "Vertex Paint" ), tr( "Weight Paint" ), tr( "Segment Paint" ), tr( "Physics Sim" ) } ) {
				modeButton->setText( s );
				wHint = std::max( wHint, modeButton->sizeHint().width() );
				wText = std::max( wText, fm.horizontalAdvance( s ) );
			}
			modeButton->setText( saved );
			/* The LARGER of the two estimates.
			 *
			 * sizeHint alone elided the label to "Obje...Mode": QToolButton does
			 * not fold stylesheet padding into its hint, so on a QSS-styled
			 * button it under-reports. Font metrics alone was the old form and
			 * over-reported, leaving a gap before the arrow. Taking the max
			 * cannot clip, and lands on the tighter number wherever the hint is
			 * honest.
			 *
			 * The 32 covers what the text measurement leaves out: 12 px of QSS
			 * padding, ~4 px icon-to-text spacing, the 2 px border and the menu
			 * indicator.
			 */
			modeButton->setFixedWidth( std::max( wHint,
				wText + modeButton->iconSize().width() + 32 ) );
		}

		auto syncModeButton = [this, modeButton, objectMode, editMode, poseMode, vertexPaintMode,
			weightPaintMode, segmentPaintMode, physicsMode, objectIcon, editIcon, poseIcon,
			vertexPaintIcon, weightPaintIcon, segmentPaintIcon, physicsIcon]() {
			bool paintingWeights = ogl->riggingWeightPaintModeActive();
			bool paintingVertices = ogl->vertexPaintModeActive();
			bool paintingSegments = ogl->segmentPaintModeActive();
			bool editing = ogl->editModeActive();
			bool posing = ogl->poseModeActive();
			QSignalBlocker objectBlocker( objectMode );
			QSignalBlocker editBlocker( editMode );
			QSignalBlocker poseBlocker( poseMode );
			QSignalBlocker vertexBlocker( vertexPaintMode );
			QSignalBlocker weightBlocker( weightPaintMode );
			QSignalBlocker segmentBlocker( segmentPaintMode );
			const bool simulating = ogl->physicsSimActive();
			QSignalBlocker physicsBlocker( physicsMode );
			physicsMode->setChecked( simulating );
			bool anyOther = paintingWeights || paintingVertices || paintingSegments || editing || posing || simulating;
			objectMode->setChecked( !anyOther );
			editMode->setChecked( !paintingWeights && !paintingVertices && !paintingSegments && !posing && editing );
			poseMode->setChecked( posing );
			vertexPaintMode->setChecked( paintingVertices );
			weightPaintMode->setChecked( paintingWeights );
			segmentPaintMode->setChecked( paintingSegments );
			modeButton->setText( simulating ? QObject::tr( "Physics Sim" )
				: paintingWeights ? QObject::tr( "Weight Paint" )
				: paintingVertices ? QObject::tr( "Vertex Paint" )
				: paintingSegments ? QObject::tr( "Segment Paint" )
				: posing ? QObject::tr( "Pose Mode" )
				: ( editing ? QObject::tr( "Edit Mode" ) : QObject::tr( "Object Mode" ) ) );
			/* The same ladder as the label above, in the same order.
			 *
			 * It was missing the simulating and posing rungs, so the button said
			 * "Pose Mode" or "Physics Sim" beside the Object Mode cube. That was
			 * invisible while Pose and Physics had no glyph of their own and the
			 * menu handed them the cube as well - three of the seven entries were
			 * the same picture, so the button was never wrong in a way anyone
			 * could see.
			 */
			modeButton->setIcon( simulating ? physicsIcon
				: paintingWeights ? weightPaintIcon
				: paintingVertices ? vertexPaintIcon
				: paintingSegments ? segmentPaintIcon
				: posing ? poseIcon
				: ( editing ? editIcon : objectIcon ) );
		};
		connect( objectMode, &QAction::triggered, this, [this]() {
			ogl->setPhysicsSimMode( false );
			ogl->setRiggingWeightPaintMode( false );
			ogl->setVertexPaintMode( false );
			ogl->setSegmentPaintMode( false );
			ogl->setPoseMode( false );
			ogl->setEditMode( false );
		} );
		connect( editMode, &QAction::triggered, this, [this]() {
			ogl->setPhysicsSimMode( false );
			ogl->setRiggingWeightPaintMode( false );
			ogl->setVertexPaintMode( false );
			ogl->setSegmentPaintMode( false );
			ogl->setPoseMode( false );
			ogl->setEditMode( true );
		} );
		connect( poseMode, &QAction::triggered, this, [this]() {
			ogl->setRiggingWeightPaintMode( false );
			ogl->setVertexPaintMode( false );
			ogl->setSegmentPaintMode( false );
			ogl->setEditMode( false );
			ogl->setPoseMode( true );
		} );
		connect( physicsMode, &QAction::triggered, this, [this, syncModeButton]() {
			ogl->setRiggingWeightPaintMode( false );
			ogl->setVertexPaintMode( false );
			ogl->setSegmentPaintMode( false );
			ogl->setPoseMode( false );
			ogl->setEditMode( false );
			if ( !ogl->setPhysicsSimMode( true ) ) {
				// nothing to simulate: say so rather than sitting in a mode that does
				// nothing, which reads as the feature being broken
				QMessageBox::information( this, tr( "Physics Sim" ),
					tr( "This file has no jointed collision to simulate.\n\n"
						"Physics Sim runs a ragdoll: it needs a bhkRagdollSystem, or a "
						"physics system whose bodies are joined by constraints." ) );
			}
			syncModeButton();
		} );
		connect( ogl, &GLView::physicsSimModeChanged, this,
			[this, syncModeButton]( bool simulating ) {
				if ( simulating )
					lastViewportNonObjectMode = 6;
				syncModeButton();
			} );
		connect( ogl, &GLView::editModeChanged, this, [this, syncModeButton]( bool enabled ) {
			if ( enabled && !ogl->riggingWeightPaintModeActive() && !ogl->vertexPaintModeActive()
				&& !ogl->segmentPaintModeActive() )
				lastViewportNonObjectMode = 1;
			syncModeButton();
		} );
		connect( ogl, &GLView::vertexPaintModeChanged, this,
			[this, syncModeButton]( bool enabled ) {
				if ( enabled )
					lastViewportNonObjectMode = 2;
				syncModeButton();
			} );
		connect( ogl, &GLView::riggingWeightPaintModeChanged, this,
			[this, syncModeButton]( bool enabled ) {
				if ( enabled )
					lastViewportNonObjectMode = 3;
				syncModeButton();
			} );
		connect( ogl, &GLView::segmentPaintModeChanged, this,
			[this, syncModeButton]( bool enabled ) {
				if ( enabled ) lastViewportNonObjectMode = 4;
				syncModeButton();
			} );
		connect( ogl, &GLView::poseModeChanged, this,
			[this, syncModeButton]( bool enabled ) {
				if ( enabled ) lastViewportNonObjectMode = 5;
				syncModeButton();
			} );
		// GLView always starts in object mode; subsequent changes arrive through
		// the mode signals (including Tab, Esc, RMB, and manager-button changes).
		syncModeButton();
		/* The mode selector heads the MODE toolbar, and that toolbar comes first.
		 *
		 * Blender's viewport header is mode selector, then the header menus that
		 * the mode governs - View, Select, Add, Object - and ours had the mode at
		 * one end of the row and Select/Add/Object at the other, with every
		 * transform widget in between. The position is what teaches you the verbs
		 * follow the mode, and it was saying the opposite.
		 *
		 * insertToolBar moves tMode ahead of tRender in the row; the mode button
		 * then leads tMode, and the separator that tMode adds next divides it
		 * from the verbs.
		 */
		ui->tMode->addWidget( modeButton );
		insertToolBar( ui->tRender, ui->tMode );

		// Blender-style viewport header menus: Select · Add · Object in object
		// mode, Select · Mesh · Vertex · Edge · Face in edit mode, Select ·
		// Weights/Paint/Segments while painting. These now live in a docked
		// toolbar (tMode) in the top toolbar row, in place of the old animation
		// bar, rather than a floating overlay — as ordinary window chrome they
		// need none of the overlay's focus/positioning workarounds. The menus
		// rebuild on aboutToShow from GLView's populate functions (shared with
		// the W quick menu), so enabled states are always current and the entry
		// points cannot drift apart.
		QToolBar * modeBar = ui->tMode;
		// thin line where the drag grip used to sit, delimiting the mode menus
		// from the previous toolbar group
		wwGroupBreak( modeBar );

		// Each button is added with addWidget; the returned QAction is what we
		// show/hide to collapse the toolbar slot per mode (the same idiom the
		// old anim-group combo used — hiding the QToolButton alone leaves a gap).
		auto makeMenuButton = [this, modeBar]( const QString & title,
			void (GLView::*populate)( QMenu * ), QAction ** outAct ) -> QToolButton * {
			QToolButton * btn = new QToolButton( modeBar );
			btn->setText( title );
			btn->setToolButtonStyle( Qt::ToolButtonTextOnly );
			btn->setPopupMode( QToolButton::InstantPopup );
			btn->setFocusPolicy( Qt::NoFocus );
			btn->setAutoRaise( false );
			// boxed like the Panels / Workspaces selectors (slimmer padding:
			// up to eight of these share the row)
			btn->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "3px 6px" ) ) );
			QMenu * menu = new QMenu( btn );
			connect( menu, &QMenu::aboutToShow, this, [this, menu, populate]() {
				menu->clear();
				( ogl->*populate )( menu );
			} );
			btn->setMenu( menu );
			QAction * act = modeBar->addWidget( btn );
			if ( outAct )
				*outAct = act;
			return btn;
		};
		QAction * aSelect = nullptr, * aAdd = nullptr, * aObject = nullptr, * aMesh = nullptr,
			* aVertex = nullptr, * aEdge = nullptr, * aFace = nullptr, * aPaint = nullptr;
		makeMenuButton( tr( "Select" ), &GLView::populateSelectMenu, &aSelect );
		makeMenuButton( tr( "Add" ), &GLView::populateAddMenu, &aAdd );
		QToolButton * mbObject = makeMenuButton( tr( "Object" ), &GLView::populateObjectMenu, &aObject );

		/* Show/Hide joins the Object menu, as it does in Blender.
		 *
		 * Connected SECOND, after makeMenuButton's own aboutToShow: Qt runs slots
		 * in connection order, and the first one clears the menu and repopulates
		 * it, so anything appended earlier would be wiped before it was drawn.
		 *
		 * mViewportVisibility->aboutToShow() is emitted by hand because that is
		 * what retexts these entries for the current mode and selection count -
		 * "Isolate Selected Objects" against "Isolate Selected Geometry", and the
		 * live count in "Hide Secondary Selected Objects (3)". Borrowing the
		 * actions without firing it would show whatever the last mode left.
		 */
		if ( mbObject && mbObject->menu() ) {
			connect( mbObject->menu(), &QMenu::aboutToShow, this, [this, mbObject]() {
				if ( !mViewportVisibility )
					return;
				emit mViewportVisibility->aboutToShow();
				mbObject->menu()->addSeparator();
				mbObject->menu()->addActions( mViewportVisibility->actions() );
			} );
		}
		makeMenuButton( tr( "Mesh" ), &GLView::populateMeshMenu, &aMesh );
		makeMenuButton( tr( "Vertex" ), &GLView::populateVertexMenu, &aVertex );
		makeMenuButton( tr( "Edge" ), &GLView::populateEdgeMenu, &aEdge );
		makeMenuButton( tr( "Face" ), &GLView::populateFaceMenu, &aFace );
		QToolButton * mbPaint = makeMenuButton( tr( "Paint" ), &GLView::populatePaintMenu, &aPaint );
		auto syncViewportMenus = [this, aSelect, aAdd, aObject, aMesh, aVertex,
			aEdge, aFace, aPaint, mbPaint]() {
			const bool weightPaint = ogl->riggingWeightPaintModeActive();
			const bool segmentPaint = ogl->segmentPaintModeActive();
			const bool paint = weightPaint || segmentPaint || ogl->vertexPaintModeActive();
			const bool edit = ogl->editModeActive() && !paint;
			const bool object = !ogl->editModeActive() && !paint;
			// Select stays in every mode: masking selections while painting
			aSelect->setVisible( true );
			aAdd->setVisible( object );
			aObject->setVisible( object );
			aMesh->setVisible( edit );
			aVertex->setVisible( edit );
			aEdge->setVisible( edit );
			aFace->setVisible( edit );
			aPaint->setVisible( paint );
			if ( paint )
				mbPaint->setText( weightPaint ? tr( "Weights" )
					: segmentPaint ? tr( "Segments" ) : tr( "Paint" ) );
		};
		connect( ogl, &GLView::editModeChanged, this, [syncViewportMenus]( bool ) { syncViewportMenus(); } );
		connect( ogl, &GLView::vertexPaintModeChanged, this, [syncViewportMenus]( bool ) { syncViewportMenus(); } );
		connect( ogl, &GLView::riggingWeightPaintModeChanged, this, [syncViewportMenus]( bool ) { syncViewportMenus(); } );
		connect( ogl, &GLView::segmentPaintModeChanged, this, [syncViewportMenus]( bool ) { syncViewportMenus(); } );
		syncViewportMenus();

		ui->tRender->removeAction( ui->aSelectObject );
		ui->tRender->removeAction( ui->aSelectVertex );
	}

	// Universal visibility controls. These intentionally live in the viewport
	// header rather than a workspace panel: object isolation is useful in every
	// workspace, while the same command becomes geometry isolation in Edit and
	// Weight Paint modes.
	{
		QToolButton * visibilityButton = new QToolButton( this );
		visibilityButton->setObjectName( QStringLiteral( "ViewportVisibilityButton" ) );
		visibilityButton->setPopupMode( QToolButton::InstantPopup );
		visibilityButton->setAutoRaise( true );
		visibilityButton->setIcon( tlMakeIcon( QStringLiteral( "eye_hidden" ),
			QColor( wwSkinColor( "text" ) ) ) );
		visibilityButton->setToolTip( tr( "Isolate, hide, or restore viewport geometry" ) );
		QMenu * visibilityMenu = new QMenu( visibilityButton );
		QAction * isolateSelected = visibilityMenu->addAction( tr( "Isolate Selected Objects" ) );
		QAction * isolatePrimary = visibilityMenu->addAction( tr( "Isolate Primary Object" ) );
		QAction * hideSecondary = visibilityMenu->addAction( tr( "Hide Secondary Selected Objects" ) );
		visibilityMenu->addSeparator();
		QAction * restoreAll = visibilityMenu->addAction(
			tlMakeIcon( QStringLiteral( "eye" ), QColor( wwSkinColor( "text" ) ) ),
			tr( "Restore All Hidden" ) );
		visibilityButton->setMenu( visibilityMenu );

		connect( visibilityMenu, &QMenu::aboutToShow, this,
			[this, isolateSelected, isolatePrimary, hideSecondary]() {
			const bool editing = ogl->editModeActive();
			isolateSelected->setText( editing ? tr( "Isolate Selected Geometry" )
				: tr( "Isolate Selected Objects" ) );
			isolateSelected->setEnabled( editing ? !ogl->pickedElems.isEmpty()
				: !ogl->selectedObjectBlocks().isEmpty() );
			const int primary = editing ? ogl->editShapeBlock : ogl->activeObjectBlock();
			isolatePrimary->setEnabled( primary >= 0 || ogl->getScene()->currentBlock.isValid() );
			int secondaryCount = 0;
			for ( int block : ogl->selectedObjectBlocks() )
				if ( block != primary ) secondaryCount++;
			hideSecondary->setEnabled( secondaryCount > 0 );
			hideSecondary->setText( secondaryCount > 0
				? tr( "Hide Secondary Selected Objects (%1)" ).arg( secondaryCount )
				: tr( "Hide Secondary Selected Objects" ) );
		} );
		connect( isolateSelected, &QAction::triggered, this, [this, aSolo]() {
			if ( !ogl->editModeActive() )
				aSolo->setChecked( false );
			ogl->isolateSelected();
		} );
		connect( isolatePrimary, &QAction::triggered, this, [this, aSolo]() {
			aSolo->setChecked( false );
			ogl->isolatePrimary();
		} );
		connect( hideSecondary, &QAction::triggered, this, [this]() {
			ogl->hideSecondarySelection();
		} );
		connect( restoreAll, &QAction::triggered, this, [this, aSolo]() {
			aSolo->setChecked( false );
			ogl->restoreAllVisibility();
		} );
		/* Isolate/Hide/Restore live in the OBJECT menu, not on a glyph.
		 *
		 * Blender has no header button for this: it is Object > Show/Hide, on H
		 * and Alt+H. Ours was an unlabelled struck-through eye - and it was
		 * already behaving like an Object-menu entry, retexting itself between
		 * "Objects" and "Geometry" with the mode. It just was not in the menu.
		 *
		 * Appended after the menu's own populate step. The verb buttons rebuild
		 * their menus on aboutToShow, and Qt runs slots in connection order, so
		 * connecting second is what puts these at the bottom rather than having
		 * them wiped by the clear().
		 */
		visibilityButton->hide();
		mViewportVisibility = visibilityMenu;
	}

	// View directions are now driven by the Blender numpad bindings and the 3D
	// orientation gizmo. Remove the redundant toolbar dropdown; the actions stay
	// available in Render for discoverability and non-numpad keyboards.
	{
		const QList<QAction *> vs = { ui->aViewTop, ui->aViewFront, ui->aViewLeft, ui->aViewFlip,
			ui->aViewPerspective, ui->aViewWalk, ui->aViewUser, ui->aViewUserSave };
		for ( QAction * a : vs )
			ui->tRender->removeAction( a );
	}


	// display toggles collapse into one dropdown button
	{
		QToolButton * btn = new QToolButton( this );
		btn->setObjectName( QStringLiteral( "ViewportOverlaysButton" ) );
		btn->setPopupMode( QToolButton::InstantPopup );
		/* "Overlays", which is what Blender calls this exact dropdown.
		 *
		 * Its contents are nearly Blender's list already - grid, axes, nodes,
		 * markers, origins, 3D cursor, gizmo - so "Display options" was a vaguer
		 * word for a thing that has an established name. Labelled as well as
		 * drawn: it is one of the two dropdowns that belong at the right-hand end
		 * beside Shading, and that end is where a name is affordable.
		 */
		btn->setText( tr( "Overlays" ) );
		btn->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
		btn->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "3px 6px" ) ) );
		btn->setToolTip( tr( "What the viewport draws on top of the model" ) );
		btn->setIcon( tlMakeIcon( QStringLiteral( "nodes" ), QColor( wwSkinColor( "text" ) ) ) );
		QMenu * m = new QMenu( btn );
		const QList<QAction *> ds = { ui->aShowCollision, ui->aShowAxes, ui->aShowNodes, ui->aDoSkinning,
			ui->aShowConstraints, ui->aShowMarkers, ui->aShowHidden };
		for ( QAction * a : ds )
			m->addAction( a );
		m->addSeparator();
		m->addAction( aSolo );

		// toggle NiBillboardNode camera-facing (show geometry in place)
		QAction * aBillboard = new QAction( tr( "Billboards Face Camera" ), this );
		aBillboard->setCheckable( true );
		aBillboard->setChecked( true );
		aBillboard->setToolTip( tr( "When off, NiBillboardNode geometry renders at its authored orientation instead of turning to face the camera" ) );
		connect( aBillboard, &QAction::toggled, [this]( bool on ) {
			ogl->getScene()->billboardFacing = on;
			ogl->update();
		} );
		m->addAction( aBillboard );

		// Blender preference: viewport rotation pivots around the selection
		QAction * aOrbitSel = new QAction( tr( "Orbit Around Selection" ), this );
		aOrbitSel->setCheckable( true );
		aOrbitSel->setToolTip( tr( "Rotate the view around the selected objects instead of the view center" ) );
		connect( aOrbitSel, &QAction::toggled, [this]( bool on ) { ogl->orbitSelection = on; } );
		m->addAction( aOrbitSel );

		// Blender-style viewport toggles, moved in from the toolbar
		m->addSeparator();
		aShowCursor->setIcon( tlMakeIcon( QStringLiteral( "cursor3d" ), icoColHdr, tlAccentColor() ) );
		mCursor->removeAction( aShowCursor );
		m->addAction( aShowCursor );
		aGizmoHandles->setIcon( tlMakeIcon( QStringLiteral( "gizmo" ), icoColHdr, tlAccentColor() ) );
		m->addAction( aGizmoHandles );
		QAction * aOrigins = new QAction( tlMakeIcon( QStringLiteral( "origins" ), icoColHdr, tlAccentColor() ), tr( "Show Origins" ), this );
		aOrigins->setCheckable( true );
		aOrigins->setChecked( true );
		aOrigins->setToolTip( tr( "Show origin points of selected nodes and dashed lines to their parents" ) );
		connect( aOrigins, &QAction::toggled, [this]( bool on ) {
			ogl->showOrigins = on;
			ogl->update();
		} );
		m->addAction( aOrigins );

		/* Show the state of every icon-bearing toggle in here.
		 * Done after the menu is fully populated so nothing is missed, and
		 * before the settings restore below, whose setChecked calls then drive
		 * the tick through the toggled() connection.
		 */
		for ( QAction * a : m->actions() )
			wwShowCheckBesideIcon( a, m );

		// persist these viewport display toggles between sessions (apply the
		// saved state now - the apply connections above fire on setChecked).
		// Particle rendering lives in the lighting menu, persisted there.
		{
			QSettings settings;
			auto persist = [&settings, this]( QAction * a, const QString & key ) {
				a->setChecked( settings.value( key, a->isChecked() ).toBool() );
				connect( a, &QAction::toggled, this, [key]( bool on ) {
					QSettings s;
					s.setValue( key, on );
				} );
			};
			persist( aShowCursor,   QStringLiteral( "GLView/Display/ShowCursor" ) );
			persist( aGizmoHandles, QStringLiteral( "GLView/Display/ShowGizmo" ) );
			persist( aOrigins,      QStringLiteral( "GLView/Display/ShowOrigins" ) );
			persist( aBillboard,    QStringLiteral( "GLView/Display/Billboards" ) );
			persist( aOrbitSel,     QStringLiteral( "GLView/Display/OrbitSelection" ) );
		}

		/* Everything else in here loses its colour.
		 *
		 * The .ui contributes resource icons to the Show Collision / Axes /
		 * Nodes / Markers entries, and colour in an icon is a claim that the
		 * thing matters more than its neighbours - a menu where several make
		 * that claim makes none. The drawn glyphs above keep ONE red element
		 * each, the one the glyph is actually about, which is the treatment
		 * cursor3d already had and the only exception the set allows.
		 */
		for ( QAction * a : m->actions() ) {
			if ( a->isSeparator() || a->icon().isNull() )
				continue;
			if ( a == aShowCursor || a == aGizmoHandles || a == aOrigins )
				continue;						// these carry the accent on purpose
			a->setIcon( wwGreyscaleIcon( a->icon() ) );
		}

		btn->setMenu( m );
		/* NOT added to the row here.
		 *
		 * It belongs at the right-hand end beside Shading - Blender puts
		 * Overlays, X-ray and Shading together as the last group of the header -
		 * and that group is built further down. It is picked up there by
		 * objectName rather than threaded through as a variable, because these
		 * two blocks are separate scopes and a member would outlive the need.
		 */
		for ( QAction * a : ds )
			ui->tRender->removeAction( a );
		ui->tRender->removeAction( aSolo );
	}

	// Camera focus commands share one compact dropdown. It sits between the
	// display menu and transform controls: Center Viewpoint resets the orbit
	// pivot, while Frame Selected also fits the active selection in the view.
	{
		/* Removing the legacy viewpoint actions can leave their old separator
		 * stranded at the end of the toolbar, so it goes — and then one is added
		 * back deliberately.
		 *
		 * This used to run the display menu and this focus control together on
		 * the grounds that they are related. bungo wants them divided, and he is
		 * right: everything else on this row is spaced in equal groups, so one
		 * pair sitting flush was the odd one out whatever the reasoning.
		 */
		while ( !ui->tRender->actions().isEmpty()
			&& ui->tRender->actions().last()->isSeparator() )
			ui->tRender->removeAction( ui->tRender->actions().last() );
		wwGroupBreak( ui->tRender );
		ui->aViewCenter->setText( tr( "Center Viewpoint" ) );
		ui->aViewCenter->setIcon( tlMakeIcon( QStringLiteral( "view_center" ), QColor( wwSkinColor( "text" ) ) ) );
		QToolButton * focusButton = new QToolButton( this );
		focusButton->setObjectName( QStringLiteral( "ViewportFocusButton" ) );
		focusButton->setPopupMode( QToolButton::InstantPopup );
		focusButton->setAutoRaise( true );
		focusButton->setIcon( ui->aViewCenter->icon() );
		focusButton->setToolTip( tr( "Center or frame the viewport" ) );
		QMenu * focusMenu = new QMenu( focusButton );
		focusMenu->addAction( ui->aViewCenter );
		QAction * frameSelectedAction = focusMenu->addAction(
			tr( "Frame Selected\tNum ." ), this, [this]() { ogl->frameSelected(); } );
		frameSelectedAction->setToolTip( tr( "Center and zoom the view to fit the active selection" ) );
		focusButton->setMenu( focusMenu );

		/* Center and Frame belong in the VIEW menu, not on a glyph.
		 *
		 * Blender puts them there - View > Frame Selected / Frame All - and this
		 * menu is already Blender's View menu in everything but name: Top, Front,
		 * Left, Flip, Perspective, Walk, Load View, Save View. Nothing in it
		 * renders anything, so it is renamed to match what it holds; "Render"
		 * described this menu before the view directions moved into it.
		 */
		focusButton->hide();
		if ( ui->mRender ) {
			ui->mRender->setTitle( tr( "&View" ) );
			ui->mRender->addSeparator();
			ui->mRender->addAction( frameSelectedAction );
		}
	}

	// Blender-style transform header on the freed toolbar space
	{
		wwGroupBreak( ui->tRender );

		// Blender-style dropdowns: a flat button showing the current choice's
		// icon (and text, where Blender shows text), opening the checkable
		// menu with a section title
		auto makeDropdown = [this]( QMenu * menu, QActionGroup * grp, const QString & section,
			const QString & tip, bool showText ) {
			menu->insertSection( menu->actions().value( 0 ), section );
			QToolButton * btn = new QToolButton( this );
			btn->setPopupMode( QToolButton::InstantPopup );
			btn->setAutoRaise( true );
			btn->setToolTip( tip );
			btn->setMenu( menu );
			btn->setToolButtonStyle( showText ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly );
			const auto acts = grp->actions();
			for ( QAction * a : acts ) {
				QObject::connect( a, &QAction::triggered, [btn, a, showText]() {
					btn->setIcon( a->icon() );
					if ( showText )
						btn->setText( a->text() );
				} );
				if ( a->isChecked() ) {
					btn->setIcon( a->icon() );
					if ( showText )
						btn->setText( a->text() );
				}
			}
			ui->tRender->addWidget( btn );
			return btn;
		};
		// Blender shows text on the orientation selector, icon-only on pivot
		makeDropdown( mOrient, grpOrient, tr( "Transform Orientations" ), tr( "Transform orientation" ), true );
		makeDropdown( mPivot, grpPivot, tr( "Transform Pivot Point" ), tr( "Transform pivot point" ), false );

		QToolButton * btnMagnet = new QToolButton( this );
		btnMagnet->setCheckable( true );
		btnMagnet->setAutoRaise( true );
		btnMagnet->setIcon( tlMakeIcon( QStringLiteral( "magnet" ), QColor( wwSkinColor( "text" ) ) ) );
		btnMagnet->setToolTip( tr( "Snap during transforms without holding Ctrl (Ctrl inverts)" ) );
		btnMagnet->setChecked( ogl->snapDefaultOn );
		connect( btnMagnet, &QToolButton::toggled, [this]( bool on ) {
			ogl->snapDefaultOn = on;
			QSettings().setValue( "GLView/Snap/DefaultOn", on );
		} );
		ui->tRender->addWidget( btnMagnet );

		// Blender-style snapping panel popup next to the magnet
		{
			QToolButton * btnSnap = new QToolButton( this );
			btnSnap->setPopupMode( QToolButton::InstantPopup );
			btnSnap->setAutoRaise( true );
			btnSnap->setToolTip( tr( "Snapping options" ) );
			// the button shows the active snap target's icon (Blender)
			{
				const auto tgtActsIco = grpSnapTgt->actions();
				for ( QAction * ta : tgtActsIco ) {
					connect( ta, &QAction::triggered, [btnSnap, ta]() { btnSnap->setIcon( ta->icon() ); } );
					if ( ta->isChecked() )
						btnSnap->setIcon( ta->icon() );
				}
			}

			QMenu * snapMenu = new QMenu( btnSnap );
			QWidget * panel = new QWidget( snapMenu );
			QVBoxLayout * pl = new QVBoxLayout( panel );
			pl->setContentsMargins( 10, 8, 10, 8 );
			pl->setSpacing( 4 );

			auto addHeader = [panel, pl]( const QString & text ) {
				QLabel * l = new QLabel( text, panel );
				QFont f = l->font();
				f.setBold( true );
				l->setFont( f );
				pl->addWidget( l );
			};

			// Blender Snap Base: which part of the selection lands on target
			addHeader( tr( "Snap Base" ) );
			{
				QWidget * brow = new QWidget( panel );
				QHBoxLayout * brl = new QHBoxLayout( brow );
				brl->setContentsMargins( 0, 0, 0, 0 );
				brl->setSpacing( 2 );
				static const char * baseNames[4] = {
					QT_TR_NOOP( "Closest" ), QT_TR_NOOP( "Center" ), QT_TR_NOOP( "Median" ), QT_TR_NOOP( "Active" )
				};
				QButtonGroup * bgrp = new QButtonGroup( brow );
				bgrp->setExclusive( true );
				for ( int i = 0; i < 4; i++ ) {
					QPushButton * b = new QPushButton( tr( baseNames[i] ), brow );
					b->setCheckable( true );
					b->setChecked( i == ogl->snapBase );
					bgrp->addButton( b );
					connect( b, &QPushButton::clicked, [this, i]() { ogl->snapBase = i; QSettings().setValue( "GLView/Snap/Base", i ); } );
					brl->addWidget( b );
				}
				pl->addWidget( brow );
			}

			// snap target: mirrors the exclusive action group
			addHeader( tr( "Snap Target" ) );
			const auto tgtActs = grpSnapTgt->actions();
			for ( QAction * ta : tgtActs ) {
				QPushButton * b = new QPushButton( ta->icon(), ta->text(), panel );
				b->setCheckable( true );
				b->setChecked( ta->isChecked() );
				connect( b, &QPushButton::clicked, ta, &QAction::trigger );
				connect( ta, &QAction::toggled, b, &QPushButton::setChecked );
				pl->addWidget( b );
			}

			// which transforms snapping applies to
			addHeader( tr( "Affect" ) );
			QWidget * arow = new QWidget( panel );
			QHBoxLayout * arl = new QHBoxLayout( arow );
			arl->setContentsMargins( 0, 0, 0, 0 );
			arl->setSpacing( 2 );
			const char * affNames[3] = { QT_TR_NOOP( "Move" ), QT_TR_NOOP( "Rotate" ), QT_TR_NOOP( "Scale" ) };
			for ( int i = 0; i < 3; i++ ) {
				QPushButton * b = new QPushButton( tr( affNames[i] ), arow );
				b->setCheckable( true );
				b->setChecked( ogl->snapAffect & ( 1 << i ) );
				connect( b, &QPushButton::toggled, [this, i]( bool on ) {
					if ( on )
						ogl->snapAffect |= ( 1 << i );
					else
						ogl->snapAffect &= ~( 1 << i );
					QSettings().setValue( "GLView/Snap/Affect", ogl->snapAffect );
				} );
				arl->addWidget( b );
			}
			pl->addWidget( arow );

			QCheckBox * cbAlign = new QCheckBox( tr( "Align Rotation to Target" ), panel );
			cbAlign->setChecked( aAlignRot->isChecked() );
			connect( cbAlign, &QCheckBox::toggled, aAlignRot, &QAction::setChecked );
			connect( aAlignRot, &QAction::toggled, cbAlign, &QCheckBox::setChecked );
			pl->addWidget( cbAlign );

			// grid snap distance: the step used by the Grid Step target (and
			// Ctrl-drag grid snapping). Same value as GLView::gizmoSnapStep.
			addHeader( tr( "Grid Snap Distance" ) );
			{
				QWidget * grow = new QWidget( panel );
				QHBoxLayout * grl = new QHBoxLayout( grow );
				grl->setContentsMargins( 0, 0, 0, 0 );
				grl->setSpacing( 2 );
				QDoubleSpinBox * spGrid = new WwNumberField( grow );
				spGrid->setRange( 0.001, 4096.0 );
				spGrid->setDecimals( 3 );
				spGrid->setValue( GLView::gizmoSnapStep );
				connect( spGrid, qOverload<double>( &QDoubleSpinBox::valueChanged ),
					[]( double v ) { GLView::gizmoSnapStep = (float)v; QSettings().setValue( "GLView/Snap/GridStep", (float)v ); } );
				QPushButton * bg1 = new QPushButton( QStringLiteral( "1" ), grow );
				QPushButton * bg10 = new QPushButton( QStringLiteral( "10" ), grow );
				connect( bg1, &QPushButton::clicked, [spGrid]() { spGrid->setValue( 1.0 ); } );
				connect( bg10, &QPushButton::clicked, [spGrid]() { spGrid->setValue( 10.0 ); } );
				grl->addWidget( bg1 );
				grl->addWidget( bg10 );
				grl->addWidget( spGrid );
				pl->addWidget( grow );
			}

			addHeader( tr( "Rotation Increment" ) );
			QWidget * rrow = new QWidget( panel );
			QHBoxLayout * rrl = new QHBoxLayout( rrow );
			rrl->setContentsMargins( 0, 0, 0, 0 );
			rrl->setSpacing( 2 );
			QDoubleSpinBox * spRot = new WwNumberField( rrow );
			spRot->setRange( 0.1, 180.0 );
			spRot->setDecimals( 1 );
			spRot->setSuffix( QStringLiteral( "°" ) );
			spRot->setValue( GLView::gizmoRotSnapDeg );
			connect( spRot, qOverload<double>( &QDoubleSpinBox::valueChanged ),
				[]( double v ) { GLView::gizmoRotSnapDeg = (float)v; QSettings().setValue( "GLView/Snap/RotStep", (float)v ); } );
			QPushButton * b5 = new QPushButton( QStringLiteral( "5°" ), rrow );
			QPushButton * b1 = new QPushButton( QStringLiteral( "1°" ), rrow );
			connect( b5, &QPushButton::clicked, [spRot]() { spRot->setValue( 5.0 ); } );
			connect( b1, &QPushButton::clicked, [spRot]() { spRot->setValue( 1.0 ); } );
			rrl->addWidget( b5 );
			rrl->addWidget( b1 );
			rrl->addWidget( spRot );
			pl->addWidget( rrow );

			QWidgetAction * wa = new QWidgetAction( snapMenu );
			wa->setDefaultWidget( panel );
			snapMenu->addAction( wa );
			btnSnap->setMenu( snapMenu );
			ui->tRender->addWidget( btnSnap );
		}

		// transform-settings cluster ends here (Blender groups these too)
		wwGroupBreak( ui->tRender );

		// Blender-style vertex / edge / face select buttons (edit mode)
		{
			QColor icoCol( wwSkinColor( "text" ) );
			QToolButton * bVert = new QToolButton( this );
			QToolButton * bEdge = new QToolButton( this );
			QToolButton * bFace = new QToolButton( this );
			QToolButton * elemBtns[3] = { bVert, bEdge, bFace };
			const char * elemIcons[3] = { "vert", "edge", "face" };
			const char * elemTips[3] = { "Vertex select (1)", "Edge select (2)", "Face select (3)" };
			// bits: vertex=1, edge=2, face=4 - independently toggleable (Blender-style)
			for ( int i = 0; i < 3; i++ ) {
				elemBtns[i]->setIcon( tlMakeIcon( elemIcons[i], icoCol ) );
				elemBtns[i]->setToolTip( tr( elemTips[i] ) + tr( "  (Shift+click to combine)" ) );
				elemBtns[i]->setCheckable( true );
				elemBtns[i]->setAutoRaise( true );
				int bit = 1 << i;
				connect( elemBtns[i], &QToolButton::clicked, [this, bit]() {
					if ( ogl->riggingWeightPaintModeActive() )
						ogl->setRiggingWeightPaintBrushEnabled( false );
					else if ( ogl->vertexPaintModeActive() )
						ogl->setVertexPaintBrushEnabled( false );
					else if ( !ogl->editMode )
						ogl->setEditMode( true );
					if ( !ogl->editMode )
						return;
					bool shift = ( QApplication::keyboardModifiers() & Qt::ShiftModifier );
					ogl->setPickMode( shift ? ( ogl->pickMode ^ bit ) : bit );
				} );
				ui->tRender->addWidget( elemBtns[i] );
			}
			connect( ogl, &GLView::pickModeChanged, [elemBtns]( int mask ) {
				for ( int i = 0; i < 3; i++ ) {
					QSignalBlocker sb( elemBtns[i] );
					elemBtns[i]->setChecked( mask & ( 1 << i ) );
				}
			} );

			// Evaluated-cage toggle: checked edits the same skinned position shown
			// in Object Mode; unchecked exposes the authored raw bind vertices.
			QToolButton * bDeformedCage = new QToolButton( this );
			bDeformedCage->setObjectName( QStringLiteral( "ViewportDeformedCageButton" ) );
			// A greyscale deformation-lattice glyph, distinct from the weight-paint
			// brush (the old :/btn/skinned icon read as a brush next to "Deformed").
			bDeformedCage->setIcon( tlMakeIcon( QStringLiteral( "mode_deform" ), QColor( wwSkinColor( "text" ) ) ) );
			/* Icon only. The label cost a lot of header width for a toggle whose
			 * glyph already says what it is, and the header is the row that runs
			 * out of room first. The tooltip carries the full explanation.
			 */
			bDeformedCage->setToolButtonStyle( Qt::ToolButtonIconOnly );
			bDeformedCage->setToolTip( tr( "Deformed Cage: edit the evaluated game/skinned position. Disable for raw bind vertices." ) );
			bDeformedCage->setCheckable( true );
			bDeformedCage->setAutoRaise( true );
			bDeformedCage->setChecked( ogl->editDeformedCageEnabled() );
			QAction * deformedCageAction = ui->tRender->addWidget( bDeformedCage );
			deformedCageAction->setVisible( false );
			connect( bDeformedCage, &QToolButton::toggled, ogl, &GLView::setEditDeformedCage );
			connect( ogl, &GLView::editDeformedCageChanged, this, [bDeformedCage]( bool enabled ) {
				QSignalBlocker blocker( bDeformedCage );
				bDeformedCage->setChecked( enabled );
			} );
			auto syncDeformedCageVisibility = [this, deformedCageAction]() {
				deformedCageAction->setVisible( ogl->editModeActive()
					&& !ogl->riggingWeightPaintModeActive() && !ogl->vertexPaintModeActive()
					&& !ogl->segmentPaintModeActive() );
			};
			connect( ogl, &GLView::editModeChanged, this,
				[syncDeformedCageVisibility]( bool ) { syncDeformedCageVisibility(); } );
			connect( ogl, &GLView::riggingWeightPaintModeChanged, this,
				[syncDeformedCageVisibility]( bool ) { syncDeformedCageVisibility(); } );
			connect( ogl, &GLView::vertexPaintModeChanged, this,
				[syncDeformedCageVisibility]( bool ) { syncDeformedCageVisibility(); } );
			connect( ogl, &GLView::segmentPaintModeChanged, this,
				[syncDeformedCageVisibility]( bool ) { syncDeformedCageVisibility(); } );

			// Weight Paint stays one viewport mode, while this Blender-style tool
			// toggle decides whether LMB paints or uses the edit selection engine.
			QToolButton * bWeightBrush = new QToolButton( this );
			bWeightBrush->setObjectName( QStringLiteral( "ViewportWeightPaintBrushButton" ) );
			// The actual paint/select toggle: give it the greyscale brush glyph so
			// it reads as a brush (was the :/btn/skinned icon).
			bWeightBrush->setIcon( tlMakeIcon( QStringLiteral( "mode_weightpaint" ), QColor( wwSkinColor( "text" ) ) ) );
			// icon only, same reasoning as the Deformed toggle above
			bWeightBrush->setToolButtonStyle( Qt::ToolButtonIconOnly );
			bWeightBrush->setToolTip( tr( "Weight Paint brush. Disable to select vertices, edges, or faces." ) );
			bWeightBrush->setCheckable( true );
			bWeightBrush->setAutoRaise( true );
			// Place the Brush toggle between the element-select buttons and the
			// Deformed toggle (Blender order), not after Deformed where it could
			// slip into the toolbar overflow.
			QAction * weightBrushAction = ui->tRender->insertWidget( deformedCageAction, bWeightBrush );
			weightBrushAction->setVisible( false );
			// The brush toggle should be present whenever the user is in a paint
			// context — either a paint mode is active, or the Vertex Paint /
			// Rigging (weight/segment) manager workspace is open — so it doesn't
			// vanish just because painting hasn't been started yet.
			auto refreshBrushVisible = [this, weightBrushAction]() {
				bool active = ogl->riggingWeightPaintModeActive() || ogl->vertexPaintModeActive()
					|| ogl->segmentPaintModeActive();
				bool workspace = false;
				for ( const char * name : { "VertexPaintManagerDock", "RiggingManagerDock" } )
					if ( QDockWidget * d = findChild<QDockWidget *>( QString::fromLatin1( name ) ) )
						workspace = workspace || d->isVisible();
				weightBrushAction->setVisible( active || workspace );
			};
			// The manager docks are created after this toolbar; connect to their
			// visibility once they exist.
			QTimer::singleShot( 0, this, [this, refreshBrushVisible]() {
				for ( const char * name : { "VertexPaintManagerDock", "RiggingManagerDock" } )
					if ( QDockWidget * d = findChild<QDockWidget *>( QString::fromLatin1( name ) ) )
						connect( d, &QDockWidget::visibilityChanged, this,
							[refreshBrushVisible]( bool ) { refreshBrushVisible(); } );
				refreshBrushVisible();
			} );
			connect( bWeightBrush, &QToolButton::clicked, this, [this, bWeightBrush]( bool enabled ) {
				if ( ogl->riggingWeightPaintModeActive() ) {
					ogl->setRiggingWeightPaintBrushEnabled( enabled );
					return;
				}
				if ( ogl->vertexPaintModeActive() ) {
					ogl->setVertexPaintBrushEnabled( enabled );
					return;
				}
				if ( ogl->segmentPaintModeActive() ) {
					ogl->setSegmentPaintBrushEnabled( enabled );
					return;
				}
				// No paint mode active yet: start one from the open manager so the
				// brush actually begins painting instead of doing nothing.
				QPushButton * start = nullptr;
				if ( QDockWidget * d = findChild<QDockWidget *>( QStringLiteral( "VertexPaintManagerDock" ) );
					d && d->isVisible() )
					start = d->findChild<QPushButton *>( QStringLiteral( "VertexPaintButton" ) );
				else if ( QDockWidget * d = findChild<QDockWidget *>( QStringLiteral( "RiggingManagerDock" ) );
					d && d->isVisible() )
					start = d->findChild<QPushButton *>( QStringLiteral( "RiggingWeightPaintButton" ) );
				if ( start && start->isEnabled() && !start->isChecked() )
					start->click();
				const bool started = ogl->riggingWeightPaintModeActive()
					|| ogl->vertexPaintModeActive() || ogl->segmentPaintModeActive();
				if ( !started ) {
					QSignalBlocker blocker( bWeightBrush );
					bWeightBrush->setChecked( false );
					statusBar()->showMessage(
						tr( "Select a mesh (and a bone for Weight Paint) in the manager, then click the brush." ), 5000 );
				}
			} );
			connect( ogl, &GLView::riggingWeightPaintBrushChanged, this,
				[bWeightBrush]( bool enabled ) {
					QSignalBlocker blocker( bWeightBrush );
					bWeightBrush->setChecked( enabled );
				} );
			connect( ogl, &GLView::vertexPaintBrushChanged, this,
				[bWeightBrush]( bool enabled ) {
					QSignalBlocker blocker( bWeightBrush );
					bWeightBrush->setChecked( enabled );
				} );
			connect( ogl, &GLView::segmentPaintBrushChanged, this,
				[bWeightBrush]( bool enabled ) {
					QSignalBlocker blocker( bWeightBrush );
					bWeightBrush->setChecked( enabled );
				} );
			connect( ogl, &GLView::riggingWeightPaintModeChanged, this,
				[this, bWeightBrush, refreshBrushVisible]( bool enabled ) {
					refreshBrushVisible();
					QSignalBlocker blocker( bWeightBrush );
					bWeightBrush->setChecked( enabled ? ogl->riggingWeightPaintBrushActive()
						: ogl->vertexPaintBrushActive() );
				} );
			connect( ogl, &GLView::vertexPaintModeChanged, this,
				[this, bWeightBrush, refreshBrushVisible]( bool enabled ) {
					refreshBrushVisible();
					bWeightBrush->setToolTip( enabled
						? tr( "Vertex Paint brush. Disable to select vertices, edges, or faces." )
						: tr( "Weight Paint brush. Disable to select vertices, edges, or faces." ) );
					QSignalBlocker blocker( bWeightBrush );
					bWeightBrush->setChecked( enabled ? ogl->vertexPaintBrushActive()
						: ogl->segmentPaintModeActive() ? ogl->segmentPaintBrushActive()
						: ogl->riggingWeightPaintBrushActive() );
				} );
			connect( ogl, &GLView::segmentPaintModeChanged, this,
				[this, bWeightBrush, refreshBrushVisible]( bool enabled ) {
					refreshBrushVisible();
					if ( enabled ) bWeightBrush->setToolTip( tr( "Segment Paint brush. Disable to select geometry as a paint mask." ) );
					QSignalBlocker blocker( bWeightBrush );
					bWeightBrush->setChecked( enabled ? ogl->segmentPaintBrushActive()
						: ogl->vertexPaintModeActive() ? ogl->vertexPaintBrushActive()
						: ogl->riggingWeightPaintBrushActive() );
				} );
		}

		/* THE DISPLAY GROUP, at the right-hand end: Overlays, X-ray, Wire,
		 * Shading - Blender's trailing header group, in Blender's order.
		 *
		 * Blender right-aligns this group against the far edge of the header, and
		 * an expanding spacer here does NOT achieve that: these controls are
		 * spread over five sibling QToolBars in one QMainWindow row, and the
		 * main-window layout hands each toolbar its size hint rather than
		 * distributing the slack, so an Expanding widget inside one of them
		 * collapses to nothing. Tried, photographed, removed - it was dead weight
		 * that read as intent. Real right-alignment would mean merging all five
		 * toolbars into one, which is a bigger change than this ordering pass.
		 */
		wwGroupBreak( ui->tRender );
		if ( auto * overlays = findChild<QToolButton *>( QStringLiteral( "ViewportOverlaysButton" ) ) )
			ui->tRender->addWidget( overlays );

		// Blender-style X-ray (Alt+Z): combines with any shading mode; sits
		// left of the shading buttons like Blender's header
		QToolButton * btnXRay = new QToolButton( this );
		btnXRay->setCheckable( true );
		btnXRay->setAutoRaise( true );
		btnXRay->setIcon( tlMakeIcon( QStringLiteral( "xray" ), icoColHdr ) );
		btnXRay->setToolTip( tr( "Toggle X-ray: see through geometry (half-transparent when shaded, see-through edges with the wireframe). Alt+Z" ) );
		btnXRay->setShortcut( QKeySequence( Qt::ALT | Qt::Key_Z ) );
		connect( btnXRay, &QToolButton::toggled, [this]( bool on ) {
			ogl->getScene()->xRay = on;
			ogl->update();
		} );
		ui->tRender->addWidget( btnXRay );

		// Wireframe is an independent overlay: it draws black wires on top of
		// whichever shading (Solid / Shaded) is active, so the texture still
		// shows. Solid / Shaded remain an exclusive pair (lighting off / on).
		QToolButton * btnWire = new QToolButton( this );
		btnWire->setIcon( tlMakeIcon( QStringLiteral( "shade_wire" ), icoColHdr ) );
		btnWire->setCheckable( true );
		btnWire->setAutoRaise( true );
		btnWire->setToolTip( tr( "Wireframe overlay: black wireframe over the shaded/solid render (combine with X-ray to see through)." ) );
		ogl->wireframeOverlay = false;
		connect( btnWire, &QToolButton::toggled, [this]( bool on ) {
			ogl->wireframeOverlay = on;
			ogl->update();
		} );
		ui->tRender->addWidget( btnWire );

		// Viewport shading is split into an exclusive display mode, an exclusive
		// material workflow, and independently combinable material contributions.
		QToolButton * shadeButton = new QToolButton( this );
		shadeButton->setPopupMode( QToolButton::InstantPopup );
		shadeButton->setAutoRaise( true );
		shadeButton->setToolTip( tr( "Viewport shading" ) );
		QMenu * shadeMenu = new PersistentActionMenu( shadeButton );
		shadeMenu->addSection( tr( "Viewport Mode" ) );
		QActionGroup * shadeGrp = new QActionGroup( shadeMenu );
		shadeGrp->setExclusive( true );
		const QStringList shadeNames = { tr( "Flat" ), tr( "Unlit" ), tr( "Shaded" ), tr( "Game Lighting" ) };
		const QStringList shadeTips = {
			tr( "Uniform dark grey faces without material shading" ),
			tr( "Material contributions without viewport lighting" ),
			tr( "Material contributions with viewport lighting" ),
			tr( "Future game-accurate lighting preview (not implemented yet)" )
		};
		const QStringList shadeIcons = { QStringLiteral( "shade_flat" ), QStringLiteral( "shade_solid" ),
			QStringLiteral( "shade_material" ), QStringLiteral( "shade_material" ) };
		QList<QAction *> shadeActions;
		for ( int i = 0; i < shadeNames.size(); i++ ) {
			QAction * action = shadeMenu->addAction( tlMakeIcon( shadeIcons.at( i ), icoColHdr ), shadeNames.at( i ) );
			action->setCheckable( true );
			action->setToolTip( shadeTips.at( i ) );
			action->setData( i );
			shadeGrp->addAction( action );
			shadeActions.append( action );
		}
		shadeActions.at( 3 )->setEnabled( false );
		shadeActions.at( 3 )->setText( tr( "Game Lighting (Planned)" ) );

		shadeMenu->addSeparator();
		shadeMenu->addSection( tr( "Material Workflow" ) );
		QActionGroup * workflowGrp = new QActionGroup( shadeMenu );
		workflowGrp->setExclusive( true );
		// Three exclusive lighting modes. Only the program choice changes between
		// them, so switching needs a repaint, not a scene rebuild.
		struct LightingModeDef { int mode; QString name; QString tip; };
		const LightingModeDef lightingModes[] = {
			{ PbrmModeLegacy, tr( "Legacy" ),
			  tr( "Render only spec/gloss lighting. A resolved .pbrm is ignored." ) },
			{ PbrmModePBR, tr( "PBR" ),
			  tr( "Render only PBR lighting. Shapes without a .pbrm are driven from their "
			      "legacy material, so the whole scene sits under one BRDF." ) },
			{ PbrmModeLegacyAndPBR, tr( "Legacy and PBR" ),
			  tr( "Render both: PBR wherever a .pbrm is available, spec/gloss everywhere else. "
			      "PBR always overrides a legacy material when one is available." ) },
		};
		// The PBR modes are UNFINISHED — they bind and draw but render nothing for
		// lighting-shader shapes (WW_CHANGES 2026-07-27e). Greyed out and forced
		// to Legacy until that is fixed; `pbrmFeatureEnabled` in glproperty.cpp
		// gates the runtime side so a stored setting cannot re-enable them.
		const int activeMode = PbrmModeLegacy;
		setPbrmMode( activeMode );
		for ( const LightingModeDef & def : lightingModes ) {
			const bool finished = ( def.mode == PbrmModeLegacy );
			QAction * a = shadeMenu->addAction( def.mode == PbrmModeLegacy ? ui->aSpecular->icon() : QIcon(),
				finished ? def.name : tr( "%1 (unfinished)" ).arg( def.name ) );
			a->setCheckable( true );
			a->setChecked( def.mode == activeMode );
			a->setEnabled( finished );
			a->setToolTip( finished ? def.tip
				: tr( "Not finished: the PBR path issues draws but renders nothing for "
				      "lighting-shader shapes. Disabled until that is fixed." ) );
			workflowGrp->addAction( a );
			const int mode = def.mode;
			connect( a, &QAction::triggered, this, [this, mode]( bool on ) {
				if ( !on )
					return;		// exclusive group: only act on the one switched ON
				QSettings settings;
				settings.setValue( QStringLiteral( "Settings/Render/PBRM Mode" ), mode );
				setPbrmMode( mode );
				if ( ogl )
					ogl->update();
			} );
		}
		shadeMenu->addSeparator();

		// Same-name .pbrm discovery. NOT part of the workflow radio group: it is
		// an independent toggle, and it governs discovery only — a material whose
		// name already ends in .pbrm is a direct link and is always honoured.
		// Greyed out alongside the PBR modes: substituting a material that nothing
		// can draw would only hide the legacy one.
		QAction * pbrmAuto = shadeMenu->addAction( tr( "Auto-replace BGSM/BGEM with .pbrm (unfinished)" ) );
		pbrmAuto->setCheckable( true );
		pbrmAuto->setChecked( false );
		pbrmAuto->setEnabled( false );
		pbrmAuto->setToolTip( tr( "When a material has a same-name .pbrm beside it, use the .pbrm instead. "
			"Disabled until the PBR render path works." ) );
		connect( pbrmAuto, &QAction::toggled, this, [this]( bool on ) {
			QSettings settings;
			settings.setValue( QStringLiteral( "Settings/Render/PBRM Auto Replace" ), on );
			setPbrmAutoReplace( on );
			// Materials are resolved when a property is built, so the scene has
			// to rebuild for the change to take effect.
			if ( ogl ) {
				ogl->updateSettings();
				ogl->getScene()->clear();
				ogl->setNif( nif );
				ogl->update();
			}
		} );

		shadeMenu->addSeparator();
		shadeMenu->addSection( tr( "Material Contributions" ) );
		struct ContributionDef {
			QString group;
			QString name;
			QString tip;
			Scene::SceneOption option;
			QIcon icon;
			bool needsLighting;
		};
		// The channel mixer reads as one greyscale system; desaturate the few
		// colorful resource icons so none of them stand out (Blender-like).
		auto greyscaleIcon = []( const QIcon & icon ) { return wwGreyscaleIcon( icon ); };
		const QList<ContributionDef> contributionDefs = {
			{ tr( "Color" ), tr( "Diffuse" ), tr( "Base colour textures" ), Scene::DoDiffuse, greyscaleIcon( ui->aTextures->icon() ), false },
			{ tr( "Color" ), tr( "Tint / Greyscale" ), tr( "Greyscale palette and material tint colour" ), Scene::DoMaterialTint, QIcon(), false },
			{ tr( "Color" ), tr( "Vertex Color" ), tr( "Multiply the material by per-vertex RGB" ), Scene::DoVertexColors, greyscaleIcon( ui->aVertexColors->icon() ), false },
			{ tr( "Surface" ), tr( "Normal" ), tr( "Normal-map surface detail" ), Scene::DoNormalMap, QIcon(), true },
			{ tr( "Surface" ), tr( "Height" ), tr( "Parallax and height-map displacement preview" ), Scene::DoParallax, QIcon(), true },
			{ tr( "Surface" ), tr( "Detail / Multilayer" ), tr( "Detail masks and multilayer material contribution" ), Scene::DoDetailTextures, QIcon(), false },
			{ tr( "Lighting" ), tr( "Specular" ), tr( "Specular colour, strength, and mask" ), Scene::DoSpecular, greyscaleIcon( ui->aSpecular->icon() ), true },
			{ tr( "Lighting" ), tr( "Gloss" ), tr( "Authored glossiness and highlight sharpness" ), Scene::DoGloss, QIcon(), true },
			{ tr( "Lighting" ), tr( "Glow" ), tr( "Glow maps and emissive material response" ), Scene::DoGlow, greyscaleIcon( ui->aGlow->icon() ), true },
			{ tr( "Lighting" ), tr( "Reflections" ), tr( "Environment and cube-map reflections" ), Scene::DoCubeMapping, greyscaleIcon( ui->aCubeMapping->icon() ), true },
			{ tr( "Alpha" ), tr( "Texture Alpha" ), tr( "Texture alpha, alpha testing, and material blending" ), Scene::DoBlending, QIcon(), false },
			{ tr( "Alpha" ), tr( "Vertex Alpha" ), tr( "Use per-vertex alpha independently of vertex RGB" ), Scene::DoVertexAlpha, QIcon(), false }
		};
		// Contribution / effect toggles read as plain menu text; the active state
		// is the highlight: blue fill + orange text (matches the edit-mode accent)
		const QString channelToggleQss = QStringLiteral(
			"QToolButton { text-align: left; padding: 3px 6px; border-radius: 2px; background: transparent; }"
			"QToolButton:hover { background: %1; }"
			"QToolButton:checked { background: %2; color: %3; }"
			"QToolButton:disabled { color: %4; }" )
			.arg( wwSkinColor( "bgBtnHover" ), wwSkinColor( "bgBtnDown" ),
				  wwSkinColor( "accentText" ), wwSkinColor( "textMuted" ) );
		QWidget * contributionPanel = new QWidget( shadeMenu );
		contributionPanel->setObjectName( QStringLiteral( "ShadingChannelMixer" ) );
		QGridLayout * contributionLayout = new QGridLayout( contributionPanel );
		contributionLayout->setContentsMargins( 6, 2, 6, 4 );
		contributionLayout->setHorizontalSpacing( 4 );
		contributionLayout->setVerticalSpacing( 1 );
		QList<QAction *> contributionActions;
		quint32 defaultContributionMask = 0;
		quint32 lightingContributionMask = 0;
		QString currentContributionGroup;
		int channelRow = 0;
		int channelColumn = 0;
		for ( const ContributionDef & def : contributionDefs ) {
			if ( def.group != currentContributionGroup ) {
				if ( channelColumn != 0 ) {
					channelRow++;
					channelColumn = 0;
				}
				QLabel * groupLabel = new QLabel( def.group, contributionPanel );
				QFont groupFont = groupLabel->font();
				groupFont.setBold( true );
				groupLabel->setFont( groupFont );
				groupLabel->setStyleSheet( QStringLiteral( "color: %1; padding: 3px 2px 1px 2px;" ).arg( wwSkinColor( "textMuted" ) ) );
				contributionLayout->addWidget( groupLabel, channelRow++, 0, 1, 2 );
				currentContributionGroup = def.group;
			}
			QAction * action = new QAction( def.icon, def.name, shadeMenu );
			action->setCheckable( true );
			action->setChecked( true );
			action->setToolTip( def.tip + ( def.needsLighting ? tr( " (visible in Shaded mode)" ) : QString() ) );
			action->setData( int(def.option) );
			action->setProperty( "keepMenuOpen", true );
			QToolButton * channelButton = new QToolButton( contributionPanel );
			channelButton->setDefaultAction( action );
			channelButton->setToolButtonStyle( Qt::ToolButtonTextOnly );
			channelButton->setAutoRaise( true );
			channelButton->setMinimumWidth( 138 );
			channelButton->setStyleSheet( channelToggleQss );
			contributionLayout->addWidget( channelButton, channelRow, channelColumn );
			channelColumn++;
			if ( channelColumn == 2 ) {
				channelColumn = 0;
				channelRow++;
			}
			contributionActions.append( action );
			defaultContributionMask |= quint32( def.option );
			if ( def.needsLighting )
				lightingContributionMask |= quint32( def.option );
		}
		QWidgetAction * contributionWidgetAction = new QWidgetAction( shadeMenu );
		contributionWidgetAction->setDefaultWidget( contributionPanel );
		shadeMenu->addAction( contributionWidgetAction );
		shadeMenu->addSeparator();
		QAction * resetContributions = shadeMenu->addAction( tr( "Reset Contributions" ) );
		resetContributions->setToolTip( tr( "Enable every supported contribution for the current viewport mode" ) );
		resetContributions->setProperty( "keepMenuOpen", true );

		shadeMenu->addSeparator();
		shadeMenu->addSection( tr( "Viewport Effects" ) );
		QSettings effectSettings;
		QAction * refractionAction = new QAction( tr( "Refraction" ), shadeMenu );
		refractionAction->setCheckable( true );
		refractionAction->setChecked( effectSettings.value(
			QStringLiteral( "GLView/Display/ShowRefraction" ), true ).toBool() );
		refractionAction->setToolTip( tr( "Preview refractive materials by distorting the scene behind them" ) );
		refractionAction->setProperty( "keepMenuOpen", true );
		ogl->getScene()->showRefraction = refractionAction->isChecked();
		connect( refractionAction, &QAction::toggled, this, [this]( bool on ) {
			ogl->getScene()->showRefraction = on;
			QSettings().setValue( QStringLiteral( "GLView/Display/ShowRefraction" ), on );
			ogl->update();
		} );

		QAction * particlesAction = new QAction( tr( "Particles" ), shadeMenu );
		particlesAction->setCheckable( true );
		particlesAction->setChecked( effectSettings.value(
			QStringLiteral( "GLView/Display/ShowParticles" ), true ).toBool() );
		particlesAction->setToolTip( tr( "Render particle systems in the viewport" ) );
		particlesAction->setProperty( "keepMenuOpen", true );
		ogl->getScene()->showParticles = particlesAction->isChecked();
		connect( particlesAction, &QAction::toggled, this, [this]( bool on ) {
			ogl->getScene()->showParticles = on;
			QSettings().setValue( QStringLiteral( "GLView/Display/ShowParticles" ), on );
			ogl->update();
		} );

		// same text-toggle presentation as the contribution mixer above: plain
		// text, blue + orange highlight while active (no checkmark column)
		QWidget * effectsPanel = new QWidget( shadeMenu );
		QGridLayout * effectsLayout = new QGridLayout( effectsPanel );
		effectsLayout->setContentsMargins( 6, 2, 6, 4 );
		effectsLayout->setHorizontalSpacing( 4 );
		effectsLayout->setVerticalSpacing( 1 );
		int effectRow = 0;
		for ( QAction * effectAction : { refractionAction, particlesAction } ) {
			QToolButton * effectButton = new QToolButton( effectsPanel );
			effectButton->setDefaultAction( effectAction );
			effectButton->setToolButtonStyle( Qt::ToolButtonTextOnly );
			effectButton->setAutoRaise( true );
			effectButton->setMinimumWidth( 138 );
			effectButton->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
			effectButton->setStyleSheet( channelToggleQss );
			effectsLayout->addWidget( effectButton, effectRow++, 0 );
		}
		QWidgetAction * effectsWidgetAction = new QWidgetAction( shadeMenu );
		effectsWidgetAction->setDefaultWidget( effectsPanel );
		shadeMenu->addAction( effectsWidgetAction );

		/* LIGHTING MOVES IN HERE, AND THE BULB BUTTON GOES.
		 *
		 * The bulb was the one control on the whole top row with no label AND no
		 * tooltip - the .ui sets both to empty strings and leaves only a
		 * status-bar tip nobody reads - so the way to find the lighting sliders
		 * was to click an unexplained glyph and see what happened. It also sat
		 * next to a Shading dropdown that already owned half the same subject.
		 *
		 * The WIDGET is moved rather than rebuilt. LightingWidget is already
		 * built, already wired to GLView's setters, and already persists every
		 * slider under Settings/Render/Lighting/*; re-authoring seven sliders
		 * here would mean re-authoring those keys too, and any drift between the
		 * two would silently lose someone's saved lighting. This popup already
		 * hosts two arbitrary QWidgets through QWidgetActions, and mLight was
		 * itself a plain QMenu hosting this exact widget, so nothing new is being
		 * asked of either side.
		 *
		 * "Do Lighting" is NOT carried across: it is the same bit as this menu's
		 * own Unlit-vs-Shaded choice - applyShadeMode writes Scene::DoLighting
		 * and force-syncs ui->aLighting - so keeping both would put two controls
		 * on one state, in one popup, a few rows apart.
		 */
		shadeMenu->addSection( tr( "Lighting" ) );
		{
			auto * lightPanel = new LightingWidget( ogl, shadeMenu );
			lightPanel->setObjectName( QStringLiteral( "ShadingLightingPanel" ) );
			// no setActions(): that is what binds the "Do Lighting" toggle, and
			// without it the button stays inert, so it is hidden outright below
			if ( auto * doLighting = lightPanel->findChild<QToolButton *>(
					QStringLiteral( "btnLighting" ) ) )
				doLighting->hide();
			auto * lightWidgetAction = new QWidgetAction( shadeMenu );
			lightWidgetAction->setDefaultWidget( lightPanel );
			shadeMenu->addAction( lightWidgetAction );
			connect( ui->aSaveLighting, &QAction::triggered,
					 lightPanel, &LightingWidget::saveSettings );
		}

		shadeButton->setMenu( shadeMenu );
		ui->tRender->addWidget( shadeButton );

		auto currentShadeMode = std::make_shared<int>( 2 );
		auto soloContribution = std::make_shared<int>( -1 );
		auto soloBackupMask = std::make_shared<quint32>( defaultContributionMask );
		auto contributionMask = [contributionActions]() {
			quint32 mask = 0;
			for ( QAction * action : contributionActions ) {
				if ( action->isChecked() )
					mask |= action->data().toUInt();
			}
			return mask;
		};
		auto setContributionChecks = [contributionActions]( quint32 mask ) {
			for ( QAction * action : contributionActions ) {
				QSignalBlocker blocker( action );
				action->setChecked( ( mask & action->data().toUInt() ) != 0 );
			}
		};
		auto applyContributions = [this, currentShadeMode, contributionActions, contributionMask]() {
			Scene * scene = ogl->getScene();
			for ( QAction * action : contributionActions ) {
				Scene::SceneOption option = Scene::SceneOption( action->data().toInt() );
				scene->options.setFlag( option, action->isChecked() );
			}
			const auto syncLegacy = []( QAction * action, bool checked ) {
				QSignalBlocker blocker( action );
				action->setChecked( checked );
			};
			syncLegacy( ui->aSpecular, scene->hasOption(Scene::DoSpecular) );
			syncLegacy( ui->aGlow, scene->hasOption(Scene::DoGlow) );
			syncLegacy( ui->aCubeMapping, scene->hasOption(Scene::DoCubeMapping) );
			syncLegacy( ui->aVertexColors, scene->hasOption(Scene::DoVertexColors) );
			QSettings().setValue( QStringLiteral( "GLView/Display/Contributions/%1" ).arg( *currentShadeMode ),
							  contributionMask() );
			ogl->update();
		};

		auto applyShadeMode = [this, shadeButton, shadeActions, shadeIcons, shadeTips, icoColHdr,
			currentShadeMode, soloContribution, contributionActions, setContributionChecks,
			applyContributions, defaultContributionMask, lightingContributionMask]( int id ) {
			id = std::clamp( id, 0, 2 );
			*currentShadeMode = id;
			*soloContribution = -1;
			QSettings settings;
			const QString contributionKey = QStringLiteral( "GLView/Display/Contributions/%1" ).arg( id );
			setContributionChecks( settings.value( contributionKey, defaultContributionMask ).toUInt() );
			ogl->getScene()->flatGrey = ( id == 0 );
			ogl->setVisMode( Scene::VisNormalsOnly, false );
			ogl->setVisMode( Scene::VisVertexColors, false );
			{
				QSignalBlocker blocker( ui->aVisNormals );
				ui->aVisNormals->setChecked( false );
			}
			const bool wantsLighting = ( id == 2 );
			ogl->getScene()->options.setFlag( Scene::DoLighting, wantsLighting );
			{
				QSignalBlocker blocker( ui->aLighting );
				ui->aLighting->setChecked( wantsLighting );
			}
			for ( QAction * action : contributionActions ) {
				const quint32 bit = action->data().toUInt();
				const bool meaningful = ( id != 0 ) && ( wantsLighting || !( lightingContributionMask & bit ) );
				action->setEnabled( meaningful );
			}
			shadeActions.at( id )->setChecked( true );
			shadeButton->setIcon( tlMakeIcon( shadeIcons.at( id ), icoColHdr ) );
			shadeButton->setToolTip( tr( "Viewport shading: %1\n%2" ).arg( shadeActions.at( id )->text(), shadeTips.at( id ) ) );
			settings.setValue( QStringLiteral( "GLView/Display/ShadeMode" ), id );
			applyContributions();
		};
		connect( shadeGrp, &QActionGroup::triggered, this, [applyShadeMode]( QAction * action ) {
			applyShadeMode( action->data().toInt() );
		} );
		for ( QAction * action : contributionActions ) {
			connect( action, &QAction::triggered, this,
				[action, contributionMask, setContributionChecks, applyContributions,
				 soloContribution, soloBackupMask]( bool ) {
					const quint32 bit = action->data().toUInt();
					if ( QApplication::keyboardModifiers() & Qt::ShiftModifier ) {
						if ( *soloContribution == int(bit) ) {
							setContributionChecks( *soloBackupMask );
							*soloContribution = -1;
						} else {
							// QAction has already toggled itself; reconstruct the pre-click combination.
							*soloBackupMask = contributionMask() ^ bit;
							setContributionChecks( bit );
							*soloContribution = int(bit);
						}
					} else {
						*soloContribution = -1;
					}
					applyContributions();
				} );
		}
		connect( resetContributions, &QAction::triggered, this,
			[setContributionChecks, applyContributions, soloContribution, defaultContributionMask]() {
				*soloContribution = -1;
				setContributionChecks( defaultContributionMask );
				applyContributions();
			} );
		// Keep the dropdown in sync when Lighting is changed through Render.
		connect( ui->aLighting, &QAction::triggered, this, [this, shadeActions, applyShadeMode]( bool on ) {
			if ( shadeActions.at( 0 )->isChecked() ) return;
			QTimer::singleShot( 0, this, [applyShadeMode, on]() { applyShadeMode( on ? 2 : 1 ); } );
		} );
		auto contributionActionFor = [contributionActions]( Scene::SceneOption option ) {
			for ( QAction * action : contributionActions )
				if ( action->data().toInt() == int(option) ) return action;
			return static_cast<QAction *>( nullptr );
		};
		const QList<QPair<QAction *, QAction *>> legacyContributionActions = {
			{ ui->aSpecular, contributionActionFor( Scene::DoSpecular ) },
			{ ui->aGlow, contributionActionFor( Scene::DoGlow ) },
			{ ui->aCubeMapping, contributionActionFor( Scene::DoCubeMapping ) },
			{ ui->aVertexColors, contributionActionFor( Scene::DoVertexColors ) }
		};
		for ( const auto & pair : legacyContributionActions ) {
			if ( !pair.second ) continue;
			connect( pair.first, &QAction::triggered, this, [this, pair, applyContributions, soloContribution]( bool ) {
				QTimer::singleShot( 0, this, [pair, applyContributions, soloContribution]() {
					QSignalBlocker blocker( pair.second );
					pair.second->setChecked( pair.first->isChecked() );
					*soloContribution = -1;
					applyContributions();
				} );
			} );
		}

		// persist the wireframe overlay, X-ray and base shading mode
		{
			QSettings settings;
			if ( settings.value( QStringLiteral( "GLView/Display/ContributionsVersion" ), 1 ).toInt() < 2 ) {
				settings.remove( QStringLiteral( "GLView/Display/Contributions" ) );
				settings.setValue( QStringLiteral( "GLView/Display/ContributionsVersion" ), 2 );
			}
			btnWire->setChecked( settings.value( QStringLiteral( "GLView/Display/Wireframe" ), false ).toBool() );
			ogl->wireframeOverlay = btnWire->isChecked();
			connect( btnWire, &QToolButton::toggled, this, []( bool on ) {
				QSettings s;
				s.setValue( QStringLiteral( "GLView/Display/Wireframe" ), on );
			} );

			btnXRay->setChecked( settings.value( QStringLiteral( "GLView/Display/XRay" ), false ).toBool() );
			ogl->getScene()->xRay = btnXRay->isChecked();
			connect( btnXRay, &QToolButton::toggled, this, []( bool on ) {
				QSettings s;
				s.setValue( QStringLiteral( "GLView/Display/XRay" ), on );
			} );

			applyShadeMode( settings.value( QStringLiteral( "GLView/Display/ShadeMode" ), 2 ).toInt() );
		}

		// make the toolbar separators clearly visible (they are near-invisible
		// with the default flat theme)
		ui->tRender->setStyleSheet( QStringLiteral(
			"QToolBar::separator { background: %1; width: 2px; height: 2px; margin: 4px 6px; }" )
			.arg( wwSkinColor( "border" ) ) );
	}

	// Material Manager workspace; texture preview is its own movable window.
	extern QDockWidget * tlCreateMatTexManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl );
	QDockWidget * dMatMgr = tlCreateMatTexManagerDock( nif, this, ogl );
	dMatMgr->toggleViewAction()->setText( tr( "Material Manager" ) );

	// Collision Manager panel (browse, create, tune, decode/compile workflow)
	extern QDockWidget * tlCreateCollisionManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl );
	QDockWidget * dCollisionMgr = tlCreateCollisionManagerDock( nif, this, ogl );
	dCollisionMgr->toggleViewAction()->setText( tr( "Collision Manager" ) );

	// Rigging Manager panel; its primary command runs the complete donor transfer
	// and its advanced section exposes the same individual Rigging spells.
	extern QDockWidget * tlCreateRiggingManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl );
	QDockWidget * dRiggingMgr = tlCreateRiggingManagerDock( nif, this, ogl );
	dRiggingMgr->toggleViewAction()->setText( tr( "Rigging Manager" ) );

	// Vertex Paint is its own task workspace. It shares viewport selection and
	// navigation mechanics with Weight Paint, but has no dependency on bones.
	extern QDockWidget * tlCreateVertexPaintManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl );
	QDockWidget * dVertexPaintMgr = tlCreateVertexPaintManagerDock( nif, this, ogl );
	dVertexPaintMgr->toggleViewAction()->setText( tr( "Vertex Paint Manager" ) );

	// UV Editing workspace: Blender-style 2D UV editor working on the meshes
	// currently in Edit Mode, with two-way selection sync.
	extern QDockWidget * tlCreateUVManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl );
	QDockWidget * dUVMgr = tlCreateUVManagerDock( nif, this, ogl );
	dUVMgr->toggleViewAction()->setText( tr( "UV Editor" ) );

	// Pose Manager: bone selection for posing + a save/apply/blend pose library.
	// The posing engine is the live skinning path; this dock is the library and
	// a bone list that drives selection so G/R/S is one click away.
	extern QDockWidget * tlCreatePoseManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl );
	QDockWidget * dPoseMgr = tlCreatePoseManagerDock( nif, this, ogl );
	dPoseMgr->toggleViewAction()->setText( tr( "Pose Manager" ) );

	// Skeleton Manager: the skeleton tree, which nodes are actually bones, how
	// much of the skin each one drives, and the rest-pose toggle. Read-only —
	// phase 1 of SKELETON_AND_POSE_PLAN.md §A.7. Appended LAST on purpose: the
	// persisted UI/Workspace index maps positionally onto the managers list, so
	// inserting anywhere else would silently reopen a different workspace for
	// every existing user.
	extern QDockWidget * tlCreateSkeletonManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl );
	QDockWidget * dSkeletonMgr = tlCreateSkeletonManagerDock( nif, this, ogl );
	dSkeletonMgr->toggleViewAction()->setText( tr( "Skeleton Manager" ) );

	// These docks occupy one manager/workspace slot. Keep the policy on
	// the docks themselves so the planned Blender-style workspace selector can
	// later activate a role instead of knowing about each concrete manager.
	/* Unfuck is a workspace, not a dialog.
	 *
	 * It began as a menu of checkboxes and then a modal dialog, and both had the
	 * same fault: they asked which repairs to run before saying what was wrong.
	 * A panel can stay open while you work, which is what makes "go to the block"
	 * worth having at all — click a finding, look at it, fix it, watch the list
	 * shrink. Appended last so the stored workspace index of every existing user
	 * keeps pointing at the same workspace it did before.
	 */
	extern QDockWidget * tlCreateUnfuckManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl );
	QDockWidget * dUnfuckMgr = tlCreateUnfuckManagerDock( nif, this, ogl );
	/* The workspace is called Issue Manager; "unfuck" stays a VERB.
	 *
	 * It is a list of everything wrong with the file, so the noun should say what
	 * it holds - and it sits in a dropdown beside Animation, Collision and
	 * Rigging, which all name their subject. The button inside it still reads
	 * "Unfuck checked", because that is the action, and no QSettings key anywhere
	 * in src/ contains the old word, so nothing saved is disturbed by the change.
	 */
	dUnfuckMgr->toggleViewAction()->setText( tr( "Issue Manager" ) );

	const QList<QDockWidget *> workspaceManagers = {
		dTimeline, dMatMgr, dCollisionMgr, dRiggingMgr, dVertexPaintMgr, dUVMgr, dPoseMgr,
		dSkeletonMgr, dUnfuckMgr
	};
	for ( QDockWidget * manager : workspaceManagers )
		manager->setProperty( "workspaceRole", QStringLiteral( "manager" ) );
	for ( QDockWidget * manager : workspaceManagers ) {
		connect( manager, &QDockWidget::visibilityChanged, this, [this, manager]( bool visible ) {
			if ( !visible || manager->property( "workspaceRole" ).toString() != QLatin1String( "manager" ) )
				return;
			for ( QDockWidget * other : findChildren<QDockWidget *>() ) {
				if ( other != manager && other->isVisible()
					&& other->property( "workspaceRole" ).toString() == QLatin1String( "manager" ) )
					other->hide();
			}
		} );
	}

	/* Animation: one button, everything inside it.
	 *
	 * This was five widgets in a row — play, sequence, loop, settings, scrub —
	 * plus View ▸ Animations in the menu bar, which is six places to look for one
	 * subject and four of them icon-only. They are one panel now, behind one
	 * labelled button, in the same shape as the Collision button beside it: a
	 * QToolButton that unfolds a widget, not a stack of glyphs.
	 *
	 * It drives the EXISTING actions (aAnimate / aAnimPlay / aAnimLoop /
	 * aAnimSwitch) and Scene::animGroups rather than keeping its own state, so
	 * this, the Timeline dock's transport and Space in the viewport can never
	 * disagree. Everything is re-read on aboutToShow for the same reason: the
	 * keyboard changes the same state behind the panel's back.
	 *
	 * Always present, disabled when the file has nothing to animate. A control
	 * that vanishes between files is worse than one that is visibly unavailable:
	 * you cannot learn where it lives, and its absence is indistinguishable from
	 * not having found it yet.
	 */
	{
		const QColor icoCol( wwSkinColor( "text" ) );
		const QColor icoColOff( wwSkinColor( "textMuted" ) );
		const QString boxQss = wwBoxedButtonQss( QStringLiteral( "3px 6px" ) )
			+ QStringLiteral( "QToolButton:disabled { color:%1; border-color:%2; }" )
				.arg( wwSkinColor( "textMuted" ), wwSkinColor( "borderDim" ) );

		/* LOD: a dropdown beside Animation and Collision, not a slider.
		 *
		 * The slider was the wrong control twice over. It hid its own value -
		 * you read a handle position and guessed - and it vanished entirely on a
		 * file with no LOD meshes, so the control you were looking for was not
		 * missing, it was absent, which is worse. It also sat in the Workspaces
		 * group, among the application menus, when it is a viewport display
		 * setting like the two buttons it now stands beside.
		 *
		 * Always present, greyed when the file has nothing to switch between,
		 * and the button itself says which level you are on.
		 */
		QToolButton * lodBtn = new QToolButton( this );
		lodBtn->setPopupMode( QToolButton::InstantPopup );
		lodBtn->setText( tr( "LOD 0" ) );
		lodBtn->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
		lodBtn->setAutoRaise( false );
		/* wwBoxedButtonQss alone, NOT boxQss.
		 *
		 * boxQss appends "QToolButton:disabled { border-color: ... }" on top of a
		 * base rule of "border: 1px solid transparent" - so a DISABLED button
		 * grows a visible box. LOD is disabled whenever the file has no LODs,
		 * which is exactly when bungo saw one; Animation and Collision are
		 * enabled, so theirs never showed. The plain sheet greys the text and
		 * leaves the border transparent.
		 *
		 * Extra right padding: the others carry an icon, which pushes their text
		 * left and leaves the menu indicator room. A text-only button has the
		 * arrow sitting against the last character.
		 */
		lodBtn->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "3px 18px 3px 8px" ) ) );
		lodBtn->setObjectName( QStringLiteral( "ViewLodButton" ) );
		lodBtn->setEnabled( false );		// until a file with LOD meshes loads
		lodBtn->setToolTip( tr( "This file has no LOD meshes" ) );

		QMenu * lodMenu = new QMenu( lodBtn );
		lodBtn->setMenu( lodMenu );

		auto * lodGroup = new QActionGroup( lodMenu );
		lodGroup->setExclusive( true );
		/* Four entries, but level 3 only means anything in Starfield:
		 * Scene::updateLodLevel clamps to 2 for every other game, so on a
		 * Fallout 4 file a level-3 row would look like a choice and silently do
		 * what level 2 does. It is listed and disabled instead of hidden, so the
		 * ceiling is visible rather than mysterious.
		 */
		for ( int lvl = 0; lvl <= 3; lvl++ ) {
			QAction * a = lodMenu->addAction( tr( "LOD %1" ).arg( lvl ) );
			a->setCheckable( true );
			a->setChecked( lvl == 0 );
			a->setData( lvl );
			lodGroup->addAction( a );
			connect( a, &QAction::triggered, this, [this, lodBtn, lvl]() {
				ogl->getScene()->updateLodLevel( lvl );
				ogl->update();
				lodBtn->setText( tr( "LOD %1" ).arg( lvl ) );
			} );
		}
		lodMenu->addSeparator();
		QAction * lodNote = lodMenu->addAction( tr( "Level 3 is Starfield only" ) );
		lodNote->setEnabled( false );

		/* The enable signal fires from the shape classes as they build, once per
		 * LOD shape found, so it arrives repeatedly and only ever with true. The
		 * false case has to come from somewhere else - a fresh load - which is
		 * why the button is reset in enableUi rather than trusted to a signal.
		 */
		connect( nif, &NifModel::lodSliderChanged, lodBtn, [this, lodBtn, lodMenu]( bool enabled ) {
			lodBtn->setEnabled( enabled );
			lodBtn->setToolTip( enabled
				? tr( "Which level of detail the viewport draws" )
				: tr( "This file has no LOD meshes" ) );
			const bool starfield =
				Game::GameManager::get_game( nif ) == Game::STARFIELD;
			const QList<QAction *> acts = lodMenu->actions();
			for ( QAction * a : acts ) {
				if ( a->data().isValid() && a->data().toInt() == 3 )
					a->setEnabled( starfield );
			}
		} );

		// the rule that closes the Workspaces group, then LOD directly against
		// Animation - one group of three viewport dropdowns, not two
		wwGroupBreak( ui->tView );
		ui->tView->addWidget( lodBtn );

		auto * animBtn = new QToolButton( this );
		animBtn->setPopupMode( QToolButton::InstantPopup );
		animBtn->setText( tr( "Animation" ) );
		animBtn->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
		animBtn->setAutoRaise( false );
		animBtn->setStyleSheet( boxQss );
		animBtn->setObjectName( QStringLiteral( "ViewAnimationButton" ) );

		QMenu * animMenu = new QMenu( animBtn );
		animBtn->setMenu( animMenu );

		QWidget * animPanel = new QWidget( animMenu );
		animPanel->setObjectName( QStringLiteral( "AnimationQuickPanel" ) );
		auto * grid = new QGridLayout( animPanel );
		grid->setContentsMargins( 10, 8, 10, 8 );
		grid->setHorizontalSpacing( 8 );
		grid->setVerticalSpacing( 6 );

		auto rowLabel = [animPanel]( const QString & text ) {
			auto * l = new QLabel( text, animPanel );
			l->setStyleSheet( QStringLiteral( "color:%1;" ).arg( wwSkinColor( "textMuted" ) ) );
			return l;
		};

		// --- transport ------------------------------------------------------
		auto * playBtn = new QToolButton( animPanel );
		playBtn->setDefaultAction( ui->aAnimPlay );
		playBtn->setToolButtonStyle( Qt::ToolButtonIconOnly );
		playBtn->setAutoRaise( false );
		playBtn->setStyleSheet( boxQss );

		auto * loopBtn = new QToolButton( animPanel );
		loopBtn->setDefaultAction( ui->aAnimLoop );
		loopBtn->setToolButtonStyle( Qt::ToolButtonIconOnly );
		loopBtn->setAutoRaise( false );
		loopBtn->setStyleSheet( boxQss );

		// Reverse is a sign on the playback rate, so it and the speed feed one
		// setter and cannot contradict each other.
		auto * revBtn = new QToolButton( animPanel );
		revBtn->setCheckable( true );
		revBtn->setText( tr( "Reverse" ) );
		revBtn->setToolButtonStyle( Qt::ToolButtonTextOnly );
		revBtn->setAutoRaise( false );
		revBtn->setStyleSheet( boxQss );
		revBtn->setToolTip( tr( "Play the sequence backwards" ) );

		const int animTicks = 1000;
		auto * animScrub = new QSlider( Qt::Horizontal, animPanel );
		animScrub->setRange( 0, animTicks );
		animScrub->setMinimumWidth( 150 );
		animScrub->setToolTip( tr( "Scrub the current sequence" ) );
		animScrub->setFocusPolicy( Qt::NoFocus );

		auto * transport = new QHBoxLayout;
		transport->setSpacing( 6 );
		transport->addWidget( playBtn );
		transport->addWidget( loopBtn );
		transport->addWidget( revBtn );
		transport->addStretch( 1 );
		grid->addLayout( transport, 0, 0, 1, 2 );
		grid->addWidget( rowLabel( tr( "Time" ) ), 1, 0 );
		grid->addWidget( animScrub, 1, 1 );

		// --- sequence and speed ---------------------------------------------
		auto * seqCombo = new QComboBox( animPanel );
		seqCombo->setToolTip( tr( "Which sequence plays" ) );
		grid->addWidget( rowLabel( tr( "Sequence" ) ), 2, 0 );
		grid->addWidget( seqCombo, 2, 1 );

		auto * speedCombo = new QComboBox( animPanel );
		const float speeds[] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f };
		for ( float sp : speeds )
			speedCombo->addItem( tr( "%1x" ).arg( sp ), sp );
		speedCombo->setCurrentIndex( 2 );			// 1x
		speedCombo->setToolTip( tr( "Playback rate" ) );
		grid->addWidget( rowLabel( tr( "Speed" ) ), 3, 0 );
		grid->addWidget( speedCombo, 3, 1 );

		auto applySpeed = [this, speedCombo, revBtn]() {
			const float sp = speedCombo->currentData().toFloat();
			ogl->setAnimSpeed( revBtn->isChecked() ? -sp : sp );
		};
		connect( speedCombo, QOverload<int>::of( &QComboBox::currentIndexChanged ),
			this, [applySpeed]( int ) { applySpeed(); } );
		connect( revBtn, &QToolButton::toggled, this, [applySpeed]( bool ) { applySpeed(); } );

		// --- the switches that used to live in the menu bar ------------------
		auto * line = new QFrame( animPanel );
		line->setFrameShape( QFrame::HLine );
		line->setStyleSheet( QStringLiteral( "color:%1;" ).arg( wwSkinColor( "borderDim" ) ) );
		grid->addWidget( line, 4, 0, 1, 2 );

		// Checkboxes over the actions, both ways: the actions are what everything
		// else in the program reads, and the keyboard can change them from under us.
		auto * enableChk = new QCheckBox( ui->aAnimate->text().remove( QLatin1Char( '&' ) ), animPanel );
		enableChk->setToolTip( ui->aAnimate->toolTip() );
		connect( enableChk, &QCheckBox::toggled, ui->aAnimate, &QAction::setChecked );
		connect( ui->aAnimate, &QAction::toggled, enableChk, &QCheckBox::setChecked );
		grid->addWidget( enableChk, 5, 0, 1, 2 );

		auto * cycleChk = new QCheckBox( tr( "Cycle through sequences" ), animPanel );
		cycleChk->setToolTip( ui->aAnimSwitch->toolTip() );
		connect( cycleChk, &QCheckBox::toggled, ui->aAnimSwitch, &QAction::setChecked );
		connect( ui->aAnimSwitch, &QAction::toggled, cycleChk, &QCheckBox::setChecked );
		grid->addWidget( cycleChk, 6, 0, 1, 2 );

		auto * timelineBtn = new QPushButton( tr( "Timeline dock…" ), animPanel );
		timelineBtn->setStyleSheet( boxQss );
		connect( timelineBtn, &QPushButton::clicked, this, [animMenu, this]() {
			animMenu->close();
			dTimeline->toggleViewAction()->trigger();
		} );
		grid->addWidget( timelineBtn, 7, 0, 1, 2 );

		QWidgetAction * animWa = new QWidgetAction( animMenu );
		animWa->setDefaultWidget( animPanel );
		animMenu->addAction( animWa );

		// One guard for the obvious feedback loop: scrubbing sets the scene time,
		// which emits sceneTimeChanged, which would move the slider under the
		// cursor mid-drag.
		auto * scrubbing = new bool( false );
		connect( animScrub, &QSlider::valueChanged, this, [this, scrubbing, animTicks]( int v ) {
			if ( *scrubbing )
				return;
			Scene * sc = ogl->getScene();
			const float mn = sc->timeMin(), mx = sc->timeMax();
			if ( mx > mn )
				ogl->setSceneTime( mn + ( mx - mn ) * float( v ) / float( animTicks ) );
		} );
		connect( ogl, &GLView::sceneTimeChanged, this,
			[animScrub, scrubbing, animTicks]( float t, float mn, float mx ) {
				if ( !( mx > mn ) )
					return;
				const int v = int( ( t - mn ) / ( mx - mn ) * float( animTicks ) + 0.5f );
				*scrubbing = true;
				animScrub->setValue( std::clamp( v, 0, animTicks ) );
				*scrubbing = false;
			} );

		/* Everything that follows the file, in one place: the sequence list, what
		 * is enabled, and the collapsed button's own glyph and tooltip — which is
		 * the only thing left to say "playing" while the panel is shut.
		 */
		auto * seqGuard = new bool( false );
		auto refreshAnimPanel = [this, animBtn, seqCombo, seqGuard, animScrub, playBtn,
								 loopBtn, revBtn, speedCombo, enableChk, cycleChk,
								 icoCol, icoColOff, boxQss]() {
			Scene * sc = ogl->getScene();
			const QStringList groups = sc ? sc->animGroups : QStringList();
			/* Animatable is a TIME RANGE, not a sequence list.
			 *
			 * Plenty of files animate through standalone controllers with no named
			 * NiControllerSequence at all - every NiPSys effect in Meshes/Effects is
			 * like this, and they are exactly what someone opens to watch something
			 * move. Gating on animGroups greyed the transport out on all of them.
			 * The sequence PICKER still needs a non-empty list, since with nothing
			 * to choose from there is nothing to pick.
			 */
			const bool have = sc && sc->timeMax() > sc->timeMin();
			const bool haveGroups = !groups.isEmpty();

			/* "No sequence" is a choice, not an absence.
			 *
			 * Plenty of files animate entirely through controllers that no
			 * NiControllerSequence names — every NiPSys effect in Meshes/Effects
			 * is like that — and a file can have both, where picking a sequence
			 * binds its interpolators over the top and hides what the file does on
			 * its own. So it is the first entry, always, and it is what a file
			 * with no named sequences offers instead of a greyed-out label.
			 *
			 * Carried as an INT, not an empty string: a sequence is allowed to
			 * have an empty name (the "(unnamed)" rows below), so an empty string
			 * cannot mean "none" without also meaning one of those.
			 */
			*seqGuard = true;
			seqCombo->clear();
			seqCombo->addItem( tr( "(no sequence)" ), -1 );
			for ( const QString & g : groups )
				seqCombo->addItem( g.isEmpty() ? tr( "(unnamed)" ) : g, g );
			if ( sc ) {
				// row 0 is "(no sequence)", so the groups start at 1
				const int ix = groups.indexOf( sc->animGroup );
				seqCombo->setCurrentIndex( ( ix >= 0 && !sc->animGroup.isNull() ) ? ix + 1 : 0 );
			}
			*seqGuard = false;

			// The action-backed buttons take their enabled state from the ACTION,
			// not the button, so disabling the widget alone left them live.
			ui->aAnimPlay->setEnabled( have );
			ui->aAnimLoop->setEnabled( have );
			// enabled whenever there is anything to animate: "(no sequence)" is a
			// real choice on a file with no named sequences, which is most effects
			seqCombo->setEnabled( have );
			animScrub->setEnabled( have );
			revBtn->setEnabled( have );
			speedCombo->setEnabled( have );
			cycleChk->setEnabled( haveGroups );

			enableChk->setChecked( ui->aAnimate->isChecked() );
			cycleChk->setChecked( ui->aAnimSwitch->isChecked() );

			// the glyph follows the action's checked state: showing "pause" while
			// paused would say what the button IS rather than what it DOES
			const bool playing = ui->aAnimPlay->isChecked();
			ui->aAnimPlay->setIcon( tlMakeIcon(
				playing ? QStringLiteral( "pause" ) : QStringLiteral( "play" ),
				have ? icoCol : icoColOff ) );
			playBtn->setToolTip( playing ? tr( "Pause animation" ) : tr( "Play animation" ) );
			ui->aAnimLoop->setIcon( tlMakeIcon( QStringLiteral( "loop" ),
												have ? icoCol : icoColOff ) );
			loopBtn->setToolTip( ui->aAnimLoop->toolTip() );

			/* The BUTTON stays enabled even with nothing to animate, and only the
			 * per-file controls inside go grey.
			 *
			 * Disabling it disabled the whole popup with it — including two things
			 * that are not about this file at all: "Animations", which is a stored
			 * preference (`GLView/Enable Animations`), and the Timeline dock. A
			 * harness once went from green to "captured 0 arcs" with no code change
			 * because that preference had been left off; a version of this panel you
			 * cannot open to look at it would make that worse, not better.
			 */
			// "play", not "playback" — that glyph is the Timeline dock's PLAY
			// BACKWARD button and points left on purpose, which on a button that
			// just means "animation" reads as rewind.
			animBtn->setIcon( tlMakeIcon( playing ? QStringLiteral( "pause" )
													: QStringLiteral( "play" ),
										  have ? icoCol : icoColOff ) );
			QString label = sc ? sc->animGroup : QString();
			if ( !haveGroups )
				label = tr( "no named sequence" );
			else if ( label.isEmpty() )
				label = tr( "(unnamed)" );
			animBtn->setToolTip( have ? tr( "Animation — %1%2" )
											.arg( label, playing ? tr( ", playing" ) : QString() )
									  : tr( "This file has nothing to animate" ) );
			/* Grey the TEXT too, not just the glyph.
			 *
			 * The button deliberately stays ENABLED with nothing to animate,
			 * because its popup holds two things that are not about this file -
			 * the stored "Animations" preference and the Timeline dock - and
			 * disabling the button would take those with it. But it was still
			 * painting its label at full strength beside a greyed-out LOD and
			 * Collision, so it read as the one live control in the group.
			 * Matching their colour costs nothing and the tooltip already says
			 * why it is dim.
			 */
			animBtn->setStyleSheet( have ? boxQss
				: boxQss + QStringLiteral( "QToolButton { color:%1; }" )
					.arg( wwSkinColor( "textMuted" ) ) );
		};
		connect( seqCombo, QOverload<int>::of( &QComboBox::currentIndexChanged ), this,
			[this, seqCombo, seqGuard]( int ix ) {
				if ( *seqGuard || ix < 0 )
					return;
				const QVariant g = seqCombo->itemData( ix );
				if ( !g.isValid() )
					return;
				// int marker = the "(no sequence)" row; a string is a group name,
				// and an EMPTY string is a legitimately unnamed sequence
				if ( g.typeId() == QMetaType::Int )
					ogl->clearSceneSequence();
				else
					ogl->setSceneSequence( g.toString() );
			} );
		connect( ogl, &GLView::sequencesUpdated, this, refreshAnimPanel );
		connect( ui->aAnimPlay, &QAction::toggled, this, [refreshAnimPanel]( bool ) { refreshAnimPanel(); } );
		connect( this, &NifSkope::completeLoading, this,
			[refreshAnimPanel]( bool, QString & ) { refreshAnimPanel(); } );
		// the keyboard changes the same state, so the panel re-reads it every time
		// it opens rather than trusting what it last wrote
		connect( animMenu, &QMenu::aboutToShow, this, refreshAnimPanel );
		refreshAnimPanel();

		// No rule here: LOD now leads this group, and the boundary it needs is
		// the one before LOD. A second rule made LOD look like its own group.
		ui->tView->addWidget( animBtn );
	}

	/* Collision: the essentials, in the toolbar. The rest is in the manager.
	 *
	 * This used to be everything -- world settings, solver knobs, the body list,
	 * recording, capture -- and grew past what a dropdown can hold: forty controls
	 * behind a scrollbar, in a popup that closes the moment you click the viewport
	 * you are trying to adjust. Two homes now, split by how often a thing is
	 * touched. Run, tool and playback are used every few seconds and stay here;
	 * everything set once per file lives in the Collision Manager's Test section,
	 * which is a dock and stays open while you work.
	 *
	 * Both are the same PhysicsSimPanel class in two modes, so the two cannot
	 * drift apart the way a second implementation would.
	 *
	 * Greyed out when the file has no collision, because a panel of controls that
	 * cannot do anything is worse than one that is plainly unavailable.
	 */
	{
		/* A GAP, not a rule. Animation, Collision and Panels are one group -
		 * the controls with no Blender counterpart, gathered at the trailing
		 * end - and ruling between them said they were three separate things.
		 */
		{ QWidget * g = new QWidget( ui->tView ); g->setFixedWidth( 8 );
		  ui->tView->addWidget( g ); }
		QToolButton * colBtn = new QToolButton( this );
		colBtn->setPopupMode( QToolButton::InstantPopup );
		colBtn->setText( tr( "Collision" ) );
		colBtn->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
		colBtn->setAutoRaise( false );
		colBtn->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "3px 6px" ) ) );
		colBtn->setObjectName( QStringLiteral( "ViewCollisionButton" ) );

		QMenu * colMenu = new QMenu( colBtn );
		PhysicsSimPanel * quick = new PhysicsSimPanel( ogl, nif,
			PhysicsSimPanel::Mode::Essentials, ui->aShowCollision, colMenu );
		quick->setObjectName( QStringLiteral( "CollisionQuickPanel" ) );
		/* In a scroll area, with a floor under it.
		 *
		 * A QMenu measures a widget action once and recomputes its own size after
		 * aboutToShow, so a panel that resizes itself in response to a tool change
		 * loses the argument and gets drawn a row short. A scroll area with a
		 * minimum height settles it: the popup is that tall whatever happens, and
		 * a tool whose parameters need more than that scrolls instead of being
		 * quietly cut off at the bottom.
		 */
		QScrollArea * quickScroll = new QScrollArea( colMenu );
		quickScroll->setWidget( quick );
		quickScroll->setWidgetResizable( true );
		quickScroll->setFrameShape( QFrame::NoFrame );
		quickScroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
		quickScroll->setMinimumWidth( quick->minimumSizeHint().width() + 8 );
		quickScroll->setMinimumHeight( 470 );
		quickScroll->setMaximumHeight( 640 );

		QWidgetAction * wa = new QWidgetAction( colMenu );
		wa->setDefaultWidget( quickScroll );
		colMenu->addAction( wa );
		colBtn->setMenu( colMenu );

		// "More collision tools..." opens the manager, which is where the rest went
		connect( quick, &PhysicsSimPanel::openManagerRequested, this,
			[colMenu, dCollisionMgr]() {
				colMenu->close();
				if ( dCollisionMgr ) {
					dCollisionMgr->show();
					dCollisionMgr->raise();
				}
			} );

		// the keyboard shortcuts change the same state, so the panel re-reads it
		// every time it is opened rather than trusting what it last wrote
		connect( colMenu, &QMenu::aboutToShow, quick, &PhysicsSimPanel::sync );

		/* Enabled only when the file HAS collision.
		 *
		 * Any bhk collision object counts, not only a jointed one: the button is
		 * about the file having collision at all, and Run explains for itself
		 * when there is nothing to simulate.
		 */
		auto refreshCollisionBar = [this, colBtn, quick]() {
			bool have = false;
			if ( nif ) {
				for ( qint32 b = 0; b < nif->getBlockCount() && !have; b++ ) {
					const QModelIndex i = nif->getBlockIndex( b );
					have = nif->blockInherits( i, "bhkCollisionObject" )
						|| nif->blockInherits( i, "bhkNiCollisionObject" )
						|| nif->blockInherits( i, "bhkPhysicsSystem" )
						|| nif->blockInherits( i, "bhkRagdollSystem" );
				}
			}
			colBtn->setEnabled( have );
			colBtn->setToolTip( have
				? tr( "Collision tools and the live physics preview" )
				: tr( "This file has no collision" ) );
			colBtn->setIcon( tlMakeIcon( QStringLiteral( "collision" ),
				have ? QColor( wwSkinColor( "text" ) ) : QColor( 128, 128, 136 ) ) );
			quick->sync();
		};
		connect( this, &NifSkope::completeLoading, this,
			[refreshCollisionBar]( bool, QString & ) { refreshCollisionBar(); } );
		refreshCollisionBar();

		ui->tView->addWidget( colBtn );
	}


	// Regular dock toggles collapse into one dropdown on the View toolbar.
	// Manager docks live in the adjacent Workspaces menu instead.
	{
		// thin line where this toolbar's drag grip used to sit
		/* A GAP, not a rule. Animation, Collision and Panels are one group -
		 * the controls with no Blender counterpart, gathered at the trailing
		 * end - and ruling between them said they were three separate things.
		 */
		/* THE PANELS BUTTON IS GONE; all of it is in the View menu.
		 *
		 * Panel toggles are a View-menu subject - Blender keeps Sidebar, Tool
		 * Settings and Adjust Last Operation there - and a dedicated dropdown for
		 * them was one more labelled button on a row that had just been trimmed
		 * to the things a viewport actually needs.
		 *
		 * The menu it filled is now the View menu itself, so `m` is that menu and
		 * everything below adds to it unchanged.
		 */
		QMenu * m = ui->mRender;
		m->addSeparator();
		for ( QDockWidget * dw : { dList, dTree, dHeader, dBrowser, dInsp, dKfm, dRefr } ) {
			m->addAction( dw->toggleViewAction() );
			ui->tView->removeAction( dw->toggleViewAction() );
		}

		/* THE VIEW MENU MOVES IN HERE, AND THEN THE VIEW MENU GOES.
		 *
		 * View held four submenus and nothing else. `Show` was the same seven
		 * dock toggles this button already lists - not equivalent actions, the
		 * SAME QAction objects - so it was a second door onto one room. The other
		 * three had no home anywhere else, which is why View could not simply be
		 * deleted:
		 *
		 *   Toolbars       the ONLY way to bring back a hidden toolbar.
		 *                  QMainWindow's built-in toggle popup is unreachable -
		 *                  nifskope.cpp sets Qt::NoContextMenu on the window and
		 *                  nothing overrides createPopupMenu() - so losing this
		 *                  makes hiding a toolbar permanent.
		 *   Block List     tree-vs-list display mode for that dock.
		 *   Block Details  non-applicable rows, and jump to Header.
		 *
		 * All three are settings that belong TO a panel, which is what makes this
		 * button their natural owner rather than a menu-bar entry of their own.
		 *
		 * Added as the existing QMenu objects, not copies: a QMenu's menuAction()
		 * is an ordinary QAction and may appear in more than one menu, so these
		 * keep working and keep filling themselves wherever else they are
		 * referenced. It also sidesteps an ordering problem - mToolbars is not
		 * populated until initMenu() runs, which is after this - because the
		 * submenu shows whatever it holds at the moment it is opened.
		 */
		m->addSeparator();
		m->addMenu( ui->mToolbars );
		m->addSeparator();
		/* Retitled, because in THIS menu the old names are ambiguous.
		 *
		 * Under View they read fine - the menu held nothing else. Here they sit
		 * directly beneath the dock toggles called "Block List" and "Block
		 * Details", so the same two words appear twice in one popup meaning two
		 * different things: once "show me that panel", once "how that panel
		 * displays". The suffix is the smallest thing that separates them.
		 */
		ui->menuBlock_List->setTitle( tr( "Block List Display" ) );
		ui->menuBlock_Details->setTitle( tr( "Block Details Display" ) );
		m->addMenu( ui->menuBlock_List );
		m->addMenu( ui->menuBlock_Details );

		QToolButton * workspaces = new QToolButton( this );
		workspaces->setPopupMode( QToolButton::InstantPopup );
		workspaces->setText( tr( "Workspaces" ) );
		workspaces->setIcon( tlMakeIcon( QStringLiteral( "workspace" ), QColor( wwSkinColor( "text" ) ) ) );
		workspaces->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
		workspaces->setAutoRaise( false );
		workspaces->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "3px 6px" ) ) );
		workspaces->setToolTip( tr( "Switch the active task workspace" ) );
		// Named for the same reason Panels now is: it was the other button on the
		// row with no objectName, so nothing could find it.
		workspaces->setObjectName( QStringLiteral( "ViewWorkspacesButton" ) );
		QMenu * workspaceMenu = new QMenu( workspaces );
		workspaceMenu->setObjectName( QStringLiteral( "ViewWorkspacesMenu" ) );
		QActionGroup * workspaceGroup = new QActionGroup( workspaceMenu );
		workspaceGroup->setExclusive( true );
		const QStringList workspaceNames = {
			tr( "Default" ), tr( "Animation" ), tr( "Materials" ), tr( "Collision" ),
			tr( "Rigging" ), tr( "Vertex Paint" ), tr( "UV Editing" ), tr( "Pose" ),
				tr( "Skeleton" ), tr( "Issue Manager" )
		};
		QList<QAction *> workspaceActions;
		for ( const QString & name : workspaceNames ) {
			QAction * action = workspaceMenu->addAction( name );
			action->setCheckable( true );
			workspaceGroup->addAction( action );
			workspaceActions.append( action );
		}
		// The "Skeleton Manager (Planned)" placeholder that used to sit below a
			// separator here is gone: the workspace exists now (read-only phase 1 of
			// SKELETON_AND_POSE_PLAN.md A.7). No planned entries remain, so the
			// separator went with it.
			const QList<QDockWidget *> managers = {
			dTimeline, dMatMgr, dCollisionMgr, dRiggingMgr, dVertexPaintMgr, dUVMgr, dPoseMgr,
			dSkeletonMgr, dUnfuckMgr
		};
		auto activateWorkspace = [this, managers, workspaceActions]( int workspace ) {
			workspace = std::clamp( workspace, 0, int( managers.size() ) );
			for ( QDockWidget * manager : managers )
				manager->hide();
			QDockWidget * target = workspace > 0 ? managers.at( workspace - 1 ) : nullptr;
			if ( target ) {
				target->setFloating( false );
				// Animation benefits from horizontal working room; the inspector
				// workspaces share the right dock area.
				addDockWidget( ( workspace == 1 )
					? Qt::BottomDockWidgetArea : Qt::RightDockWidgetArea, target );
				target->show();
				target->raise();
			}
			workspaceActions.at( workspace )->setChecked( true );
			QSettings().setValue( QStringLiteral( "UI/Workspace" ), workspace );
		};
		for ( int i = 0; i < workspaceActions.size(); i++ )
			connect( workspaceActions.at( i ), &QAction::triggered, this,
				[activateWorkspace, i]( bool ) { activateWorkspace( i ); } );
		// Weight Paint is already implemented by the Rigging Manager. Entering it
		// from the viewport selector opens that workspace, chooses the first bone
		// when necessary, and delegates to the same Start Painting button so there
		// remains one source of truth for target/bone validation and brush settings.
		QAction * weightPaintMode = findChild<QAction *>( QStringLiteral( "ViewportWeightPaintAction" ) );
		QAction * vertexPaintMode = findChild<QAction *>( QStringLiteral( "ViewportVertexPaintAction" ) );
		QAction * segmentPaintMode = findChild<QAction *>( QStringLiteral( "ViewportSegmentPaintAction" ) );
		QAction * objectMode = findChild<QAction *>( QStringLiteral( "ViewportObjectModeAction" ) );
		QAction * editMode = findChild<QAction *>( QStringLiteral( "ViewportEditModeAction" ) );
		// Auto-acquire a paint target so entering a paint mode from the viewport
		// selector works even when nothing is selected yet (e.g. right after a
		// Join clears the selection). Prefers the object the user is looking at
		// (the active object); otherwise the file's sole qualifying shape — it
		// never guesses between several. requireSkin gates Weight / Segment paint
		// (which need a skin) against Vertex paint (any tri-shape). Selecting the
		// block drives NifSkope::select(), whose synchronous currentNifIndexChanged
		// repopulates the manager before the caller re-checks it. Returns the
		// acquired block, or AcquireNone / AcquireAmbiguous for the caller's hint.
		enum { AcquireNone = -1, AcquireAmbiguous = -2 };
		auto acquirePaintTarget = [this]( bool requireSkin ) -> int {
			NifModel * nif = getNifModel();
			if ( !nif )
				return AcquireNone;
			auto qualifies = [nif, requireSkin]( int b ) {
				QModelIndex idx = nif->getBlockIndex( b );
				if ( !idx.isValid()
					|| ( !nif->blockInherits( idx, "NiTriBasedGeom" )
						&& !nif->blockInherits( idx, "BSTriShape" ) ) )
					return false;
				if ( !requireSkin )
					return true;
				return nif->getLink( idx, "Skin" ) >= 0 || nif->getLink( idx, "Skin Instance" ) >= 0;
			};
			const int active = ogl ? ogl->activeObjectBlock() : -1;
			if ( active >= 0 && qualifies( active ) ) {
				select( nif->getBlockIndex( active ) );
				return active;
			}
			int found = AcquireNone;
			for ( int b = 0, n = nif->getBlockCount(); b < n; b++ ) {
				if ( !qualifies( b ) )
					continue;
				if ( found >= 0 )
					return AcquireAmbiguous;
				found = b;
			}
			if ( found >= 0 )
				select( nif->getBlockIndex( found ) );
			return found;
		};
		if ( weightPaintMode )
			connect( weightPaintMode, &QAction::triggered, this,
				[this, activateWorkspace, acquirePaintTarget, dRiggingMgr, weightPaintMode, objectMode, editMode]() {
				activateWorkspace( 4 );
				QTreeWidget * bones = dRiggingMgr->findChild<QTreeWidget *>( QStringLiteral( "RiggingBoneTree" ) );
				int acquired = AcquireNone;
				if ( bones && bones->topLevelItemCount() == 0 )
					acquired = acquirePaintTarget( true ); // no target yet — try to select one
				if ( bones && !bones->currentItem() && bones->topLevelItemCount() > 0 )
					bones->setCurrentItem( bones->topLevelItem( 0 ) );
				QPushButton * paint = dRiggingMgr->findChild<QPushButton *>( QStringLiteral( "RiggingWeightPaintButton" ) );
				if ( paint && paint->isEnabled() && !paint->isChecked() )
					paint->click();
				if ( !ogl->riggingWeightPaintModeActive() ) {
					weightPaintMode->setChecked( false );
					if ( ogl->editModeActive() && editMode ) editMode->setChecked( true );
					else if ( objectMode ) objectMode->setChecked( true );
					statusBar()->showMessage(
						( bones && bones->topLevelItemCount() == 0 )
							? ( acquired == AcquireAmbiguous
								? tr( "Weight Paint: select which skinned mesh to paint (this file has several)." )
								: tr( "Weight Paint: no skinned mesh to paint — select a skinned BSTriShape first." ) )
							: tr( "Weight Paint: select a bone in the Rigging Manager, then Start Painting." ),
						6000 );
				}
				} );
		if ( vertexPaintMode )
			connect( vertexPaintMode, &QAction::triggered, this,
				[this, activateWorkspace, acquirePaintTarget, dVertexPaintMgr, vertexPaintMode, objectMode, editMode]() {
				activateWorkspace( 5 );
				QPushButton * paint = dVertexPaintMgr->findChild<QPushButton *>( QStringLiteral( "VertexPaintButton" ) );
				int acquired = AcquireNone;
				if ( paint && !paint->isEnabled() )
					acquired = acquirePaintTarget( false ); // any tri-shape can take vertex colors
				if ( paint && paint->isEnabled() && !paint->isChecked() )
					paint->click();
				if ( !ogl->vertexPaintModeActive() ) {
					vertexPaintMode->setChecked( false );
					if ( ogl->editModeActive() && editMode ) editMode->setChecked( true );
					else if ( objectMode ) objectMode->setChecked( true );
					statusBar()->showMessage(
						acquired == AcquireAmbiguous
							? tr( "Vertex Paint: select which mesh to paint (this file has several)." )
							: tr( "Vertex Paint: no paintable mesh — select a tri-shape first." ),
						6000 );
				}
				} );
		if ( segmentPaintMode )
			connect( segmentPaintMode, &QAction::triggered, this,
				[this, activateWorkspace, acquirePaintTarget, dRiggingMgr, segmentPaintMode, objectMode, editMode]() {
				activateWorkspace( 4 );
				QTreeWidget * bones = dRiggingMgr->findChild<QTreeWidget *>( QStringLiteral( "RiggingBoneTree" ) );
				int acquired = AcquireNone;
				if ( bones && bones->topLevelItemCount() == 0 )
					acquired = acquirePaintTarget( true ); // populate the segment list
				QPushButton * paint = dRiggingMgr->findChild<QPushButton *>( QStringLiteral( "RiggingSegmentPaintButton" ) );
				if ( paint && paint->isEnabled() && !paint->isChecked() ) paint->click();
				if ( !ogl->segmentPaintModeActive() ) {
					segmentPaintMode->setChecked( false );
					if ( ogl->editModeActive() && editMode ) editMode->setChecked( true );
					else if ( objectMode ) objectMode->setChecked( true );
					statusBar()->showMessage(
						( bones && bones->topLevelItemCount() == 0 )
							? ( acquired == AcquireAmbiguous
								? tr( "Segment Paint: select which skinned mesh to paint (this file has several)." )
								: tr( "Segment Paint: no skinned mesh with segments — select a BSSubIndexTriShape first." ) )
							: tr( "Segment Paint: select a segment or subsegment row, then Start Painting." ),
						6000 );
				}
				} );
		workspaces->setMenu( workspaceMenu );
		/* Workspaces joins the MENUS; Panels stays with the trailing group.
		 *
		 * Blender keeps its workspace tabs in the app-level topbar beside File
		 * and Edit, not in the viewport header, and the reason is what they do:
		 * a workspace switches the whole window layout, so it is not a viewport
		 * control and should not sit among viewport controls.
		 *
		 * Panels is not the same kind of thing - it toggles individual docks -
		 * so it stays at the trailing end with Animation and Collision, which is
		 * where the controls with no Blender counterpart are gathered.
		 */
		ui->tFile->addWidget( workspaces );


		// Keep the menu's active-workspace marker truthful: derive it from which
		// manager dock is actually visible (a dock closed via its own X, or the
		// exclusive-visibility logic, would otherwise leave a stale radio dot on
		// the wrong entry). The active entry is also bolded for a clearer
		// highlight than the small radio dot alone.
		auto syncWorkspaceIndicators = [workspaceActions, managers]() {
			int active = 0;
			for ( int i = 0; i < managers.size(); i++ )
				if ( managers.at( i )->isVisible() ) { active = i + 1; break; }
			for ( int i = 0; i < workspaceActions.size(); i++ ) {
				QAction * a = workspaceActions.at( i );
				QSignalBlocker blocker( a );
				a->setChecked( i == active );
				QFont f = a->font();
				f.setBold( i == active );
				a->setFont( f );
			}
		};
		connect( workspaceMenu, &QMenu::aboutToShow, this, syncWorkspaceIndicators );

		// Always open a NIF in the Default workspace, regardless of the last
		// session's workspace.
		activateWorkspace( 0 );
		syncWorkspaceIndicators();
	}

	// Set Inspect widget
	dInsp->setWidget( inspect );

	connect( dList->toggleViewAction(), &QAction::triggered, tree, &NifTreeView::clearRootIndex );

}

void NifSkope::initMenu()
{
	// Disable without NIF loaded
	ui->mRender->setEnabled( false );

	/* Populate Toolbars with every toolbar, WHEREVER IT LIVES.
	 *
	 * findChildren, not children(): the latter walks direct children of the main
	 * window only, and the mode and render toolbars are reparented into the
	 * viewport footer, so they would silently drop off this list. That matters
	 * more than it sounds - this submenu is the only way to bring a hidden
	 * toolbar back, because QMainWindow's own toggle popup is unreachable here
	 * (nifskope.cpp sets Qt::NoContextMenu and nothing overrides
	 * createPopupMenu). Losing the entry makes hiding that bar permanent.
	 */
	for ( QToolBar * tb : findChildren<QToolBar *>() ) {
		if ( tb->objectName() != "tFile" )		// tFile is the menu row itself
			ui->mToolbars->addAction( tb->toggleViewAction() );
	}

	/* The Spells menu, kept — with Unfuck at the top of it.
	 *
	 * bungo, in two passes: "remove from it whatever is in the select / add /
	 * object or other top bar buttons, or anything in the right click menu",
	 * then "keep the spells menu as it was ... with only stuff that's unique to
	 * the spell menu".
	 *
	 * What is unique turns out to be a sharp line rather than a judgement call.
	 * Right-clicking a block in the Block List or Block Details opens a full
	 * SpellBook AT THAT INDEX, so every spell that applies to a block is already
	 * one click away there. The spells that right-click can never reach are the
	 * ones that answer `isApplicable` only for an INVALID index, because they act
	 * on the whole file — and those are exactly the ones the menubar copy used to
	 * hide the moment you selected anything, since `SpellBook::checkActions`
	 * drops whatever does not apply to the current selection.
	 *
	 * So this menu is built by hand from the whole-file spells and never filtered
	 * against the selection. Everything else stays on right-click, where it was.
	 */
	{
		QMenu * spellsMenu = new QMenu( tr( "Spells" ), this );
		spellsMenu->setToolTipsVisible( true );
		/* Before Options, by POINTER rather than by index 3.
		 *
		 * The index was an artefact of how many menus happened to precede it, so
		 * removing the old View menu silently slid Spells to the far side of
		 * Options and the bar read File, View, Options, Spells, Help. Options is
		 * this program's Edit menu - settings, theme, font, and now undo - and
		 * belongs next to Help at the end, with the subject menus before it.
		 */
		ui->menubar->insertMenu( ui->mOptions ? ui->mOptions->menuAction()
											  : ui->menubar->actions().value( 2 ), spellsMenu );

		/* Unfuck is NOT in this menu. It is a workspace.
		 *
		 * It was a bolded entry at the top here, from when it was a modal dialog
		 * this menu owned. It stopped being a dialog and became the Issue Manager
		 * workspace, reachable from the Workspaces dropdown beside Animation,
		 * Collision and Rigging - so a menu entry that opens a workspace is the
		 * same duplication this menu was rebuilt to remove, just pointing the
		 * other way.
		 *
		 * Nothing is lost with it: the workspace is the only Unfuck, and the
		 * dialog's assets were folded into it long before this - the curated run
		 * order (repairOrder in unfucktools.cpp), the single-snapshot undo across
		 * a whole run, the four Batch/Optimize repairs the panel's sanity() rule
		 * could not see, the per-spell blurbs as Spell::hint(), and the "why is
		 * this row greyed" text as whyNotApplicable().
		 */

		/* TEST HARNESS (WW_UNFUCK_TEST=1): is the Spells menu what it claims, and
		 * does the dialog open?
		 *
		 * Two things can go wrong here and neither is visible from the code. The
		 * menu can be structurally right and still EMPTY, because the spells it
		 * lists are filtered by applicability; and it can quietly fill up with
		 * block spells again, which is the duplication this was meant to remove.
		 * So it opens the menu with a BLOCK SELECTED — the state that used to
		 * empty it — and checks both directions: something is listed, and nothing
		 * listed wants an index.
		 *
		 * The dialog is grabbed rather than driven. It is modal, so exec() would
		 * block the harness; a timer takes the picture and closes it.
		 * Log: release/ww_unfuck_test.log, shot: release/ww_unfuck_dialog.png
		 */
		if ( qEnvironmentVariableIsSet( "WW_UNFUCK_TEST" ) ) {
			QObject::connect( this, &NifSkope::completeLoading, this,
				[this, spellsMenu]( bool ok, QString & ) {
				QTimer::singleShot( 800, this, [this, spellsMenu, ok]() {
					QFile logf( QApplication::applicationDirPath() + "/ww_unfuck_test.log" );
					if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
						return;
					QTextStream log( &logf );
					int checksRun = 0, fails = 0;
					auto check = [&]( const QString & what, bool pass ) {
						checksRun++;
						if ( !pass ) fails++;
						log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
					};
					do {
						if ( !ok || !nif ) { log << "load failed\n"; break; }

						// Select a block. This is the whole point: with an index
						// selected the old menubar SpellBook hid every whole-file
						// spell, because they answer isApplicable only for an
						// invalid index.
						if ( nif->getBlockCount() > 1 )
							select( nif->getBlockIndex( 1 ) );
						QApplication::processEvents();

						emit spellsMenu->aboutToShow();
						const QList<QAction *> acts = spellsMenu->actions();
						/* Unfuck used to be the bolded first entry here. It is the
						 * Issue Manager workspace now, and a menu entry that opens
						 * a workspace is the same duplication this menu exists to
						 * avoid. Paired with a not-empty check so the assertion
						 * cannot pass by the menu having nothing in it at all.
						 */
						bool hasUnfuck = false;
						for ( const QAction * a : acts )
							if ( a->text().remove( QLatin1Char( '&' ) ) == QLatin1String( "Unfuck" ) )
								hasUnfuck = true;
						check( "Spells no longer carries an Unfuck entry", !hasUnfuck );
						check( "and it is not simply empty", !acts.isEmpty() );

						int pages = 0, listed = 0, wantsIndex = 0, unresolved = 0;
						for ( QAction * a : acts ) {
							if ( !a->menu() )
								continue;
							pages++;
							log << "  page " << a->menu()->title() << ":";
							for ( QAction * sa : a->menu()->actions() ) {
								log << " " << sa->text();
								listed++;
								/* Nothing here may be a block spell: those are on
								 * right-click, which is why they were taken out.
								 *
								 * The SpellPtr comes off the action's data, not
								 * from SpellBook::lookup( sa->text() ). That call
								 * returned NULL for every entry - a bare name has
								 * no "/", so lookup wants a spell whose page() is
								 * empty, and every spell here has one - which made
								 * this check incapable of failing.
								 */
								const SpellPtr sp = sa->data().value<SpellPtr>();
								if ( !sp )
									unresolved++;
								else if ( !sp->isApplicable( nif, QModelIndex() ) )
									wantsIndex++;
							}
							log << "\n";
						}
						log << pages << " page(s), " << listed << " whole-file spell(s)\n";

						log << "unresolved actions: " << unresolved << "\n";
						check( "the menu still lists whole-file spells", listed > 0 );
						/* Not vacuous any more. Every action must carry its
						 * SpellPtr, or the applicability check below is measuring
						 * an empty set - which is exactly what it did while the
						 * pointer was recovered by name.
						 */
						check( "every entry resolves to a real spell", unresolved == 0 );
						check( "none of them needs a block selected", wantsIndex == 0 );


						/* Spell::hint() reaches a real menu action.
						 *
						 * hint() had zero overrides tree-wide and nothing read it,
						 * while the same sentences sat in a private table in this
						 * file. Two things can now break independently: a spell can
						 * lose its hint, or SpellBook can stop copying it onto the
						 * QAction — and the second is invisible, because a missing
						 * tooltip looks exactly like a tooltip nobody hovered. So
						 * this checks the wiring, not only the data.
						 */
						int withHints = 0;
						for ( SpellPtr sp : SpellBook::spells() )
							if ( sp && !sp->hint().isEmpty() )
								withHints++;
						log << withHints << " spell(s) carry a hint\n";
						check( "spells carry their one-line description", withHints >= 16 );

						SpellBook probe( nif );
						QString wired;
						std::function<void( QMenu * )> walk = [&]( QMenu * m ) {
							for ( QAction * a : m->actions() ) {
								if ( a->menu() ) { walk( a->menu() ); continue; }
								if ( a->text() == QLatin1String( "Collapse Link Arrays" ) )
									wired = a->toolTip();
							}
						};
						walk( &probe );
						log << "Collapse Link Arrays tooltip: '" << wired << "'\n";
						SpellPtr collapse =
							SpellBook::lookup( QStringLiteral( "Sanitize/Collapse Link Arrays" ) );
						check( "SpellBook puts the hint on the menu action",
							collapse && !wired.isEmpty() && wired == collapse->hint() );

						/* The submenu order is DECLARED, not inherited from link order.
						 *
						 * A page's submenu is created the first time a spell on it
						 * registers, so this menu's top-level order used to be an
						 * artefact of the order object files appear in NifSkope.pro:
						 * reordering SOURCES silently reordered the user's context
						 * menu. Asserting the declared sequence is what stops that
						 * being reintroduced without a red test.
						 */
						QStringList pageTitles;
						for ( QAction * a : probe.actions() )
							if ( a->menu() )
								pageTitles << a->menu()->title();
						log << "page order: " << pageTitles.join( QStringLiteral( ", " ) ) << "\n";
						auto ordered = [&]( const char * x, const char * y ) {
							const int i = pageTitles.indexOf( QLatin1String( x ) );
							const int j = pageTitles.indexOf( QLatin1String( y ) );
							return i >= 0 && j >= 0 && i < j;
						};
						/* STALE ASSERTIONS, CORRECTED - these named groups that no
						 * longer exist.
						 *
						 * They were written against the page taxonomy and never
						 * updated when Spell::group() replaced it: "Node" was
						 * folded away and "Mesh" became "Geometry", so both
						 * ordered() calls were comparing against an index of -1
						 * and failing for a reason that had nothing to do with
						 * ordering. Two permanently-red checks nobody could act
						 * on, which is worse than no check - a suite with known
						 * failures in it stops being read.
						 *
						 * Mirrors groupOrder[] in SpellBook::orderGroups. The last
						 * check is the one with teeth: Error Checking must be the
						 * final entry, which fails if anything is appended after
						 * the declared run rather than merely reordered inside it.
						 */
						check( "the structural groups lead, in declared order",
							ordered( "Block", "Add" ) && ordered( "Add", "Transform" )
							&& ordered( "Transform", "Geometry" ) );
						check( "the checking groups come last",
							ordered( "Optimize", "Sanitize" )
							&& ordered( "Sanitize", "Error Checking" ) );
						check( "and nothing is appended after them",
							!pageTitles.isEmpty()
							&& pageTitles.last() == QLatin1String( "Error Checking" ) );

						/* It is reached from WORKSPACES now, as "Issue Manager".
						 *
						 * This used to trigger the Spells menu's bolded Unfuck entry and
						 * assert the dock appeared. That entry is gone - it opened a
						 * workspace from a spell menu, which is the duplication this menu
						 * exists to avoid - so the assertion moves to the door that is left.
						 * Removing one door without checking the remaining one is how a
						 * feature becomes unreachable in silence.
						 */
						QDockWidget * udock =
							findChild<QDockWidget *>( QStringLiteral( "UnfuckManagerDock" ) );
						if ( udock )
							udock->hide();
						QApplication::processEvents();
						QAction * issueMgr = nullptr;
						if ( auto * wsBtn = findChild<QToolButton *>(
								QStringLiteral( "ViewWorkspacesButton" ) ) ) {
							if ( QMenu * wsMenu = wsBtn->menu() ) {
								for ( QAction * a : wsMenu->actions() )
									if ( a->text().remove( QLatin1Char( '&' ) )
											== QLatin1String( "Issue Manager" ) )
										issueMgr = a;
							}
						}
						check( "Workspaces offers Issue Manager", issueMgr != nullptr );
						if ( issueMgr )
							issueMgr->trigger();
						QApplication::processEvents();
						log << "Issue Manager dock visible after trigger: "
							<< ( udock && udock->isVisible() ) << "\n";
						check( "the Issue Manager workspace opens its dock",
							udock && udock->isVisible() );
						check( "no modal dialog was raised",
							QApplication::activeModalWidget() == nullptr );
					} while ( false );
					log << checksRun << " checks, " << fails << " failures\n";
					log << ( fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
					logf.close();
					QTimer::singleShot( 300, qApp, &QApplication::quit );
				} );
			} );
		}

		/* ...and the rest of the menu: the whole-file spells, by page.
		 *
		 * Rebuilt every time the menu opens rather than filled once, because
		 * which spells qualify depends on the FILE — a spell offers itself for an
		 * invalid index or it does not, and that answer changes with the version
		 * and the contents of whatever is loaded. Rebuilding is also what keeps
		 * this menu from being filtered against the current selection, which is
		 * the behaviour that used to empty it.
		 *
		 * The repair spells are skipped: they are the Issue Manager workspace's
		 * content, and listing them twice is the duplication this was meant to
		 * remove.
		 */
		connect( spellsMenu, &QMenu::aboutToShow, this,
			[this, spellsMenu]() {
			/* Clear the whole menu, not "everything after the Unfuck entry".
			 *
			 * That anchor was the bolded Unfuck action; with it gone the menu has
			 * no permanent head, so every rebuild starts from empty. Written as a
			 * plain clear rather than a search that can no longer find anything -
			 * a stale anchor search would have quietly kept the first rebuild's
			 * contents forever.
			 */
			for ( QAction * a : spellsMenu->actions() )
				spellsMenu->removeAction( a );
			if ( !nif )
				return;

			/* The repair and check spells are skipped: they are the Unfuck
			 * workspace's content, and listing them twice is the duplication this
			 * menu was rebuilt to remove. The rule matches the panel's own -- see
			 * rebuildRepairs and checkSpells in unfucktools.cpp -- rather than a
			 * captured copy of a list the dialog used to build, so the two cannot
			 * drift apart.
			 */
			static const char * const unfuckExtras[] = {
				"Update All Bounds", "Update All Tangent Spaces",
				"Remove Unused Strings", "Make All Skin Partitions",
			};
			auto belongsToUnfuck = []( const SpellPtr & s ) {
				if ( s->checker() || s->page() == Spell::tr( "Sanitize" ) || s->sanity() )
					return true;
				for ( const char * e : unfuckExtras )
					if ( s->name() == QLatin1String( e ) )
						return true;
				return false;
			};
			QMap<QString, QList<SpellPtr>> byPage;
			for ( SpellPtr s : SpellBook::spells() ) {
				if ( !s || belongsToUnfuck( s ) )
					continue;
				/* Which heading it files under.
				 *
				 * page() first, because that is the frozen id the CLI's -s,
				 * SpellBook::lookup and the QSettings keys all use, and it must
				 * not be changed to fix a menu. group() is the newer, purely
				 * presentational axis, so it is the right fallback for a spell
				 * whose page() is empty.
				 *
				 * Without the fallback, Extract Resource Files was invisible
				 * here: a whole-file, no-selection export - exactly the profile
				 * this menu was rebuilt to gather - dropped for no reason beyond
				 * an empty page() string, and reachable only by right-clicking
				 * the blank space BELOW the last row of the Block List, which is
				 * not a discoverable place for a feature to live.
				 */
				const QString heading = !s->page().isEmpty() ? s->page() : s->group();
				if ( heading.isEmpty() )
					continue;
				// The whole test: does it offer itself with nothing selected? If
				// it wants a block, right-clicking that block already has it.
				if ( s->isApplicable( nif, QModelIndex() ) )
					byPage[heading].append( s );
			}
			for ( auto it = byPage.constBegin(); it != byPage.constEnd(); ++it ) {
				QMenu * page = spellsMenu->addMenu( it.key() );
				QList<SpellPtr> list = it.value();
				std::sort( list.begin(), list.end(),
					[]( const SpellPtr & x, const SpellPtr & y ) { return x->label() < y->label(); } );
				for ( SpellPtr sp : list ) {
					/* label() and hint(), the same as every other menu.
					 *
					 * This used name() and set no tooltip, while SpellBook builds
					 * its menus from label() and hint() - so a spell whose menu
					 * text differs from its id read differently here than
					 * everywhere else (Block/Sort By Name against "Sort Children
					 * By Name"), and setToolTipsVisible on this menu was
					 * decorative because nothing ever set a tooltip.
					 */
					QAction * a = page->addAction( sp->icon(), sp->label() );
					a->setShortcut( sp->hotkey() );
					if ( !sp->hint().isEmpty() )
						a->setToolTip( sp->hint() );
					/* The SpellPtr, carried on the action.
					 *
					 * The harness used to recover it with SpellBook::lookup on the
					 * action's text, which cannot work: a bare name has no "/", so
					 * lookup searches for a spell whose page() is EMPTY, and every
					 * spell in this menu has a page. It returned null every time,
					 * so the "none of them needs a block selected" assertion could
					 * never fail - a green check measuring nothing. Attaching the
					 * pointer removes the string round-trip entirely.
					 */
					a->setData( QVariant::fromValue( sp ) );
					connect( a, &QAction::triggered, this, [this, sp]() {
						if ( nif && book )
							book->cast( nif, QModelIndex(), sp );
					} );
				}
			}
		} );
	}

	// Insert Import/Export menus
	mExport = ui->menuExport;
	mImport = ui->menuImport;

	fillImportExportMenus();
	connect( mExport, &QMenu::triggered, this, &NifSkope::sltExport );
	connect( mImport, &QMenu::triggered, this, &NifSkope::sltImport );

	// BSA Recent Files
	mRecentArchiveFiles = new QMenu( this );
	mRecentArchiveFiles->setObjectName( "mRecentArchiveFiles" );

	for ( int i = 0; i < NumRecentFiles; ++i ) {
		ui->mRecentFiles->addAction( recentFileActs[i] );
		ui->mRecentArchives->addAction( recentArchiveActs[i] );
		mRecentArchiveFiles->addAction( recentArchiveFileActs[i] );
	}

	// right-clicking a recent entry offers "Open in New Window" (a plain
	// click opens in this window, replacing the current document) — handled
	// in eventFilter, scoped to exactly these menus
	ui->mRecentFiles->installEventFilter( this );
	ui->mRecentArchives->installEventFilter( this );
	mRecentArchiveFiles->installEventFilter( this );

	// Load & Save
	QMenu * mSave = new QMenu( this );
	mSave->setObjectName( "mSave" );

	mSave->addAction( ui->aSave );
	mSave->addAction( ui->aSaveAs );

	QMenu * mOpen = new QMenu( this );
	mOpen->setObjectName( "mOpen" );

	mOpen->addAction( ui->aOpen );
	mOpen->addAction( ui->aBrowseArchive );

	aRecentFilesSeparator = mOpen->addSeparator();
	for ( int i = 0; i < NumRecentFiles; ++i )
		mOpen->addAction( recentFileActs[i] );
	mOpenFlyout = mOpen;
	mOpen->installEventFilter( this );	// right-click recents: Open in New Window

	auto setFlyout = []( QToolButton * btn, QMenu * m ) {
		btn->setObjectName( "btnFlyoutMenu" );
		btn->setMenu( m );
		btn->setPopupMode( QToolButton::InstantPopup );
	};

	// Append Menu to tFile actions
	for ( auto child : ui->tFile->findChildren<QToolButton *>() ) {
		if ( child->defaultAction() == ui->aSaveMenu ) {
			setFlyout( child, mSave );
		} else if ( child->defaultAction() == ui->aOpenMenu ) {
			setFlyout( child, mOpen );
		}
	}

	updateRecentFileActions();
	updateRecentArchiveActions();
	updateRecentArchiveFileActions();

	/* The Lighting Options bulb is gone from the toolbar.
	 *
	 * Its sliders live in the Viewport Shading dropdown now, under a "Lighting"
	 * section - see the block in initDockWidgets that hosts a LightingWidget
	 * there. The bulb was the only control on the row carrying no label and no
	 * tooltip at all, and it duplicated the shading dropdown's subject.
	 *
	 * Removed from the toolbar rather than hidden, so the separators either side
	 * of it collapse with it and the row does not draw a double rule. aLightMenu
	 * itself is left in the .ui and simply unused: it is a designer object and
	 * deleting it would churn a generated file for nothing.
	 */
	ui->tRender->removeAction( ui->aLightMenu );

	/* Screenshot / Save View, off the bar.
	 *
	 * bungo: "we won't use that feature, so hide it from the top bar." Hidden,
	 * not deleted — `GLView::saveImage` and the Render menu entry both still
	 * work, so putting it back is one line rather than a resurrection.
	 */
	ui->tRender->removeAction( ui->aPrintView );

	/* Menu bar and tool bar share one row.
	 *
	 * bungo wants the vertical space back at the top: three stacked rows (title,
	 * menus, tools) become two. The menu bar is reparented INTO the first
	 * toolbar as an ordinary widget, so the top toolbar area lays it out on the
	 * same line as everything else — those toolbars already share a row with
	 * each other, this just adds one more member to it.
	 *
	 * Order matters. The bar is put into the toolbar first, so it belongs to the
	 * toolbar before the main window is told to stop reserving a row for it;
	 * doing it the other way round hands QMainWindow a widget it still owns and
	 * is about to replace, which is how you get a double free.
	 *
	 * setMenuWidget takes an empty placeholder rather than nullptr: nullptr
	 * leaves the previous menu widget in place on some styles, while a widget
	 * whose sizeHint is zero collapses the row outright.
	 */
	if ( ui->menubar ) {
		QMenuBar * mb = ui->menubar;
		mb->setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Preferred );
		// Corner-to-corner background would otherwise paint a plate the width of
		// the window behind the whole toolbar row.
		mb->setStyleSheet( QStringLiteral( "QMenuBar { background: transparent; border: 0; }" ) );
		const QList<QAction *> firstActions = ui->tFile->actions();
		QAction * before = firstActions.isEmpty() ? nullptr : firstActions.first();
		if ( before )
			ui->tFile->insertWidget( before, mb );
		else
			ui->tFile->addWidget( mb );
		setMenuWidget( new QWidget( this ) );
		// A vertical rule between the menus and the first tool, matching every
		// other group boundary on the row.
		if ( before )
			ui->tFile->insertSeparator( before );
	}

	/* One bar between groups, not two.
	 *
	 * Every toolbar here is assembled by deleting actions out of the designer
	 * file and inserting widgets between what is left, so removing the last
	 * button of a group strands that group's separator against the next one and
	 * the row draws a double rule with no space in it. Taking Screenshot out,
	 * just above, is exactly that case. Collapse runs of separators and drop any
	 * that end up at either end, on every toolbar, so no future removal has to
	 * remember to tidy up after itself.
	 */
	for ( QToolBar * tb : { ui->tFile, ui->tRender, ui->tMode, ui->tView, ui->tLOD } ) {
		if ( !tb )
			continue;
		/* The group-break PADDING is transparent to this.
		 *
		 * wwGroupBreak wraps every rule in two spacer widgets, and that broke
		 * this cleanup the moment it was introduced: the loop only ever saw two
		 * separators as adjacent when nothing sat between them, and now a pad
		 * always does. The result was a visible double rule between Object and
		 * Global - two groups meeting, each contributing its own boundary, with
		 * nothing left to collapse them. Photographed, not deduced.
		 *
		 * So padding is skipped when deciding adjacency, and removed along with
		 * the separator it belonged to; otherwise deleting the rule would leave
		 * its two gaps behind as an unexplained hole.
		 */
		auto isPad = []( const QAction * a ) {
			const QWidget * w = qobject_cast<const QWidgetAction *>( a )
				? static_cast<const QWidgetAction *>( a )->defaultWidget() : nullptr;
			return w && w->objectName() == QLatin1String( "wwGroupPad" );
		};
		/* Leading and trailing rules are only stripped at the ENDS OF THE ROW.
		 *
		 * These five toolbars sit side by side in one row, so "leading" and
		 * "trailing" are properties of the row, not of each toolbar. Stripping
		 * them per-toolbar deleted the boundary between tMode and tRender
		 * entirely - Object and Global ended up with no rule between them at all,
		 * because tMode's trailing one and tRender's leading one are the SAME
		 * boundary and both were treated as an edge.
		 *
		 * tFile is first in the row and tView last; only those two have real
		 * edges. Hardcoded because the row order is set deliberately (see the
		 * insertToolBar in restoreUi) rather than discovered.
		 */
		const bool rowStart = ( tb == ui->tFile );
		const bool rowEnd = ( tb == ui->tView );

		bool lastWasSeparator = rowStart;		// leading rule goes only at the row's start
		QList<QAction *> pending;				// pads seen since the last real item
		const QList<QAction *> acts = tb->actions();
		for ( QAction * a : acts ) {
			if ( isPad( a ) ) {
				pending << a;
				continue;
			}
			if ( !a->isSeparator() ) {
				lastWasSeparator = false;
				pending.clear();
				continue;
			}
			if ( lastWasSeparator ) {
				for ( QAction * p : pending )
					tb->removeAction( p );
				tb->removeAction( a );
			}
			pending.clear();
			lastWasSeparator = true;
		}
		// trailing run: a rule at the end of the ROW divides it from nothing
		while ( rowEnd ) {
			QList<QAction *> tail = tb->actions();
			int i = tail.size() - 1;
			while ( i >= 0 && isPad( tail.at( i ) ) )
				i--;
			if ( i < 0 || !tail.at( i )->isSeparator() )
				break;
			for ( int k = tail.size() - 1; k >= i; k-- )
				tb->removeAction( tail.at( k ) );
		}
	}

	// Append Menu to tRender actions
	for ( auto child : ui->tRender->findChildren<QToolButton *>() ) {

		// The bulb's flyout branch is gone with the bulb: aLightMenu is no longer
		// on this toolbar, so no child here can match it and every button takes
		// the style tag below.
		{
			/* A STYLE tag, not a name.
			 *
			 * This used to assign the objectName "btnRender", which is what
			 * res/style.qss matched on - and it ran over every tool button in
			 * tRender, so it silently destroyed the names those buttons had just
			 * been given: ViewportModeButton, ViewportVisibilityButton,
			 * ViewportFocusButton and the rest all became "btnRender", six
			 * widgets sharing one name. Nothing looked them up yet, so nothing
			 * was broken; the next thing to try findChild<QToolButton *>(
			 * "ViewportModeButton" ) would have got a null and no explanation.
			 *
			 * A dynamic property carries the styling and leaves the name alone.
			 * The objectName is still set as a FALLBACK for buttons that had none,
			 * so res/style.qss keeps matching them by either route and no button
			 * can end up unstyled if the property selector ever misses.
			 *
			 * unpolish/polish because a dynamic property set after the widget has
			 * already been polished does not re-run the style: that is the classic
			 * way a property selector silently does nothing. Cheap here, and it
			 * makes the change independent of whether the app-wide stylesheet has
			 * been applied yet.
			 */
			child->setProperty( "wwRenderBtn", true );
			if ( child->objectName().isEmpty() )
				child->setObjectName( "btnRender" );
			child->style()->unpolish( child );
			child->style()->polish( child );
		}
	}


	// Theme Menu

	QActionGroup * grpTheme = new QActionGroup( this );

	// Fill the action data with the integer correlating to
	// their position in WindowTheme and add to the action group.
	int i = 0;
	auto themes = ui->mTheme->actions();
	for ( auto a : themes ) {
		a->setData( i++ );
		grpTheme->addAction( a );
	}

	/* GREYSCALE THE RENDER MENU.
	 *
	 * Every drawn icon in this application is one greyscale family, and the
	 * Render menu was the last place still showing full-colour resource PNGs -
	 * the orange/blue view cubes, the perspective toggle, the active Load View
	 * marker, Save View, Lighting Only. Sat in a list beside a dozen grey glyphs
	 * they read as the important entries, which is a claim about them that
	 * nothing in the menu intends.
	 *
	 * Done here because initMenu() runs LAST of the four init passes
	 * (initActions -> initDockWidgets -> initToolBars -> initMenu, nifskope.cpp),
	 * so every icon these actions will ever carry is already set - including
	 * aViewCenter's, which is assigned from tlMakeIcon during the toolbar pass.
	 * It is also where the precedent sits: the bulb above is greyscaled in this
	 * same function for the same reason.
	 *
	 * Both icon STATES are converted where an action has two. aViewUser and
	 * aViewPerspective carry separate Off and On pixmaps, and the On ones are the
	 * most saturated art in the set - converting only the default state would
	 * have left the colour to reappear the moment the toggle was used, which is
	 * the sort of thing that survives review and then shows up in a screenshot.
	 *
	 * Actions with no icon are skipped by isNull() rather than listed, so this
	 * does not have to be kept in step with the menu's contents.
	 */
	if ( ui->mRender ) {
		for ( QAction * a : ui->mRender->actions() ) {
			if ( a->isSeparator() || a->icon().isNull() )
				continue;
			const QIcon src = a->icon();
			QIcon grey;
			// A single-state icon answers the On query with its Off pixmap, so
			// this asks for both unconditionally and a one-state action simply
			// gets the same art in both - which is what it had already.
			for ( QIcon::State st : { QIcon::Off, QIcon::On } ) {
				const QPixmap pm = src.pixmap( 22, 22, QIcon::Normal, st );
				if ( pm.isNull() )
					continue;
				grey.addPixmap( wwGreyscaleIcon( QIcon( pm ) ).pixmap( 22, 22 ),
								QIcon::Normal, st );
			}
			if ( !grey.isNull() )
				a->setIcon( grey );
		}
	}
}


void NifSkope::initToolBars()
{
	// Disable without NIF loaded
	ui->tRender->setEnabled( false );
	ui->tRender->setContextMenuPolicy( Qt::ActionsContextMenu );

	// Status Bar
	ui->statusbar->setContentsMargins( 0, 0, 0, 0 );
	ui->statusbar->addPermanentWidget( progress );

	// TODO: Split off into own widget
	ui->statusbar->addPermanentWidget( filePathWidget( this ) );


	// Render

	QActionGroup * grpView = new QActionGroup( this );
	grpView->addAction( ui->aViewTop );
	grpView->addAction( ui->aViewFront );
	grpView->addAction( ui->aViewLeft );
	grpView->addAction( ui->aViewWalk );
	grpView->setExclusive( true );


	// Animate — the visible transport/timeline now lives entirely in the
	// animation workspace (Timeline dock); the actions below still drive the
	// GLView animation state machine and are mirrored by the dock.

	/*enum AnimationStates
	{
		AnimDisabled = 0x0,
		AnimEnabled = 0x1,
		AnimPlay = 0x2,
		AnimLoop = 0x4,
		AnimSwitch = 0x8
	};*/

	ui->aAnimate->setData( GLView::AnimEnabled );
	ui->aAnimPlay->setData( GLView::AnimPlay );
	ui->aAnimLoop->setData( GLView::AnimLoop );
	ui->aAnimSwitch->setData( GLView::AnimSwitch );

	connect( ui->aAnimate, &QAction::toggled, ogl, &GLView::updateAnimationState );

	// Play implies animation enabled. advanceGears() requires BOTH AnimEnabled
	// and AnimPlay, and View > Animations is persisted ("GLView/Enable
	// Animations"), so one stray click there disabled playback permanently and
	// every play path — this action, the Timeline dock's transport, Space in the
	// viewport — became a silent no-op. That reads exactly like "animation is
	// broken" with nothing to discover. Connected BEFORE the state slot below so
	// AnimEnabled is already set when AnimPlay arrives.
	connect( ui->aAnimPlay, &QAction::triggered, this, [this]( bool checked ) {
		if ( checked && !ui->aAnimate->isChecked() )
			ui->aAnimate->setChecked( true );	// emits toggled -> sets AnimEnabled
	} );
	connect( ui->aAnimPlay, &QAction::triggered, ogl, &GLView::updateAnimationState );
	connect( ui->aAnimLoop, &QAction::toggled, ogl, &GLView::updateAnimationState );
	connect( ui->aAnimSwitch, &QAction::toggled, ogl, &GLView::updateAnimationState );

	// The timeline slider and animation-group combo formerly shown here have
	// moved to the animation workspace (Timeline dock: playhead scrub +
	// sequence selector). Only the wiring the dock and state machine rely on
	// remains: aAnimPlay's pressed state tracks playback, and aAnimSwitch — a
	// toggle the dock exposes — is only meaningful with more than one sequence.
	connect( ogl, &GLView::sequenceStopped, ui->aAnimPlay, &QAction::toggle );
	connect( ogl, &GLView::sequencesUpdated, [this]() {
		ui->aAnimSwitch->setVisible( ogl->getScene()->animGroups.count() > 1 );
	} );

	/* The Loop toggle starts from what the sequence says it does.
	 *
	 * NiControllerSequence carries a Cycle Type — LOOP, REVERSE or CLAMP — and
	 * nothing read it. One session-wide checkbox decided for every sequence in
	 * every file instead, and it starts UNCHECKED and is not persisted (see
	 * saveUi/restoreUi, where the line is commented out), so the default preview
	 * played everything exactly once. Measured over the stock Fallout 4 mesh
	 * tree — 1,124 files, 3,337 sequences — 2,664 are CYCLE_CLAMP and 673 are
	 * CYCLE_LOOP, so that default was wrong for 673 of them and right for the
	 * rest by accident. (None are CYCLE_REVERSE, and none predate the field.)
	 *
	 * Set on sequence CHANGE only, never on a refresh, so it follows the file
	 * without fighting a deliberate tick: override it and the override stands
	 * for as long as you stay on that sequence.
	 */
	connect( ogl, &GLView::sequenceCycleChanged, this, [this]( int cycleType, bool repeats ) {
		ui->aAnimLoop->setChecked( repeats );		// -> updateAnimationState
		static const char * const names[] = { "CYCLE_LOOP", "CYCLE_REVERSE", "CYCLE_CLAMP" };
		const QString ct = ( cycleType >= 0 && cycleType <= 2 )
			? QString::fromLatin1( names[cycleType] ) : tr( "unknown" );
		ui->aAnimLoop->setToolTip( tr( "Loop Animation\nThis sequence is authored %1" ).arg( ct ) );
	} );

	connect ( ogl->scene, &Scene::disableSave, [this]() {
		ui->aSave->setDisabled(true);
		ui->aSaveAs->setDisabled(true);
		ui->aReload->setDisabled(true);
	} );

	/* LOD toolbar.
	 *
	 * It used to sit inside the Workspaces group, before the row's separator,
	 * which grouped it with the application menus it has nothing to do with -
	 * LOD is a viewport display control, so it belongs with Animation and
	 * Collision on the other side of the rule. And its label read a bare "LOD"
	 * whatever the slider was set to, so the one thing you wanted from it - the
	 * level you are looking at - was the one thing it did not say.
	 */
	QToolBar * tLOD = ui->tLOD;
	/* The old LOD slider lived here.
	 *
	 * It is now a dropdown button beside Animation and Collision, built in
	 * initViewMenus with them. The toolbar itself is kept but emptied and
	 * hidden: it is named in four other places (the toolbar loops, the
	 * visibility menu, restoreUi) and removing it would touch all of them for
	 * no gain.
	 */
	ui->aLODDummy->setVisible( false );
	tLOD->setVisible( false );
	tLOD->setEnabled( false );
}

void NifSkope::initConnections()
{
	connect( nif, &NifModel::beginUpdateHeader, this, &NifSkope::enableUi );

	connect( this, &NifSkope::beginLoading, this, &NifSkope::onLoadBegin );
	connect( this, &NifSkope::beginSave, this, &NifSkope::onSaveBegin );

	connect( this, &NifSkope::completeLoading, this, &NifSkope::onLoadComplete );
	connect( this, &NifSkope::completeSave, this, &NifSkope::onSaveComplete );
}


/* NifSkope::lightingWidget() is gone.
 *
 * It built a QMenu wrapping a LightingWidget for the toolbar bulb. The bulb has
 * been removed and the widget is now hosted directly in the Viewport Shading
 * popup, so this was a second construction path for the same panel - and two
 * LightingWidgets would mean two sets of sliders writing the same
 * Settings/Render/Lighting/* keys, disagreeing whenever one was moved.
 */


QWidget * NifSkope::filePathWidget( QWidget * parent )
{
	// Show Filepath of loaded NIF
	auto filepathWidget = new QWidget( parent );
	filepathWidget->setObjectName( "filepathStatusbarWidget" );
	auto filepathLayout = new QHBoxLayout( filepathWidget );
	filepathWidget->setLayout( filepathLayout );
	filepathLayout->setContentsMargins( 0, 0, 0, 0 );
	auto labelFilepath = new QLabel( "", filepathWidget );
	labelFilepath->setMinimumHeight( 16 );

	filepathLayout->addWidget( labelFilepath );

	// Navigate to Filepath
	auto navigateToFilepath = new QPushButton( "", filepathWidget );
	navigateToFilepath->setFlat( true );
	navigateToFilepath->setIcon( QIcon( ":btn/load" ) );
	navigateToFilepath->setIconSize( QSize( 16, 16 ) );
	navigateToFilepath->setStatusTip( tr( "Show in Explorer" ) );

	filepathLayout->addWidget( navigateToFilepath );

	filepathWidget->setVisible( false );

	// Show Filepath on successful NIF load
	connect( this, &NifSkope::completeLoading, [filepathWidget, labelFilepath, navigateToFilepath]( bool success, QString & fname ) {
		filepathWidget->setVisible( success );
		labelFilepath->setText( fname );

		if ( QFileInfo( fname ).exists() ) {
			navigateToFilepath->show();
		} else {
			navigateToFilepath->hide();
		}
	} );

	// Change Filepath on successful NIF save
	connect( this, &NifSkope::completeSave, [filepathWidget, labelFilepath, navigateToFilepath]( bool success, QString & fname ) {
		filepathWidget->setVisible( success );
		labelFilepath->setText( fname );

		if ( QFileInfo( fname ).exists() ) {
			navigateToFilepath->show();
		} else {
			navigateToFilepath->hide();
		}
	} );

	// Navigate to NIF in Explorer (TODO: implement this for macOS)
#if defined( Q_OS_WIN ) || defined( Q_OS_LINUX )
	connect( navigateToFilepath, &QPushButton::clicked, [this]() {
		QStringList args;
#  ifdef Q_OS_WIN
		args << "/select," << QDir::toNativeSeparators( currentFile );
		QProcess::startDetached( "explorer", args );
#  else
		args << "--select" << QDir::toNativeSeparators( currentFile );
		QProcess::startDetached( "dolphin", args );
#  endif
	} );
#endif


	return filepathWidget;
}


void NifSkope::archiveDlg()
{
	QString file = QFileDialog::getOpenFileName( this, tr( "Open Archive" ), "", "Archives (*.bsa *.ba2)" );
	if ( !file.isEmpty() )
		openArchive( file );
}

void NifSkope::archiveFolderDlg()
{
	QString path = QFileDialog::getExistingDirectory( this, tr( "Open Game or Archive Folder" ), "" );
	if ( path.isEmpty() )
		return;
	if ( path.endsWith( "/Data", Qt::CaseInsensitive ) || path.endsWith( "\\Data", Qt::CaseInsensitive ) ) {
		QString	parentDir( path );
		parentDir.truncate( parentDir.length() - 5 );
		if ( !parentDir.isEmpty() && QFileInfo( parentDir ).isDir() )
			path = parentDir;
	}
	openArchive( path );
}

void NifSkope::openDlg()
{
	// Grab most recent filepath if blank window
	auto path = nif->getFileInfo().absolutePath();
	path = (path.isEmpty()) ? recentFileActs[0]->data().toString() : path;

	QStringList files = QFileDialog::getOpenFileNames( this, tr( "Open File" ), path, fileFilters() );
	if ( !files.isEmpty() )
		openFiles( files );
}

void NifSkope::onLoadBegin()
{
	// Disconnect the models from the views
	swapModels();

	ogl->setDisabled( true );
	setEnabled( false );
	ui->tMode->setEnabled( false );

	ui->tLOD->setEnabled( false );
	ui->tLOD->setVisible( false );

	progress->setVisible( true );
	progress->reset();
}

void NifSkope::onLoadComplete( bool success, QString & fname )
{
	QApplication::restoreOverrideCursor();

	updateImportExportMenu(mExport);
	updateImportExportMenu(mImport);

	// Reconnect the models to the views
	swapModels();
	// Set List vs Tree
	setListMode();

	// Re-enable window
	ogl->setDisabled( false );
	setEnabled( true ); // IMPORTANT!

	ui->aSave->setDisabled(false);
	ui->aSaveAs->setDisabled(false);
	ui->aReload->setDisabled(false);

	int timeout = 2500;
	if ( success ) {
		// Scroll panel back to top
		tree->scrollTo( nif->index( 0, 0 ) );

		select( nif->getHeaderIndex() );

		header->setRootIndex( nif->getHeaderIndex() );
		// Refresh the header rows
		header->updateConditions( nif->getIndex( nif->getHeaderIndex(), 0 ), nif->getIndex( nif->getHeaderIndex(), 20 ) );

		enableUi();

	} else {
		// File failed to load
		Message::append( this, NifModel::tr( readFail ),
						 NifModel::tr( readFailFinal ).arg( fname ), QMessageBox::Critical );

		nif->clear();
		kfm->clear();
		timeout = 0;

		// Remove from Current Files
		clearCurrentFile();

		// Reset
		currentFile.clear();
		setWindowFilePath( "" );
		progress->reset();
	}

	// Mark window as unmodified
	setWindowModified( false );
	nif->undoStack->clear();
	indexStack->clear();

	// A new tab gets a useful initial framing. Reloading/replacing a document in
	// an existing tab preserves that tab's live camera for the current session.
	if ( !hasLoadedDocument )
		ogl->center();
	hasLoadedDocument = success;
	refreshAllDocumentSessions();

	// Only in tree view mode: expand the top level of Block List tree
	if ( ui->list->model() != nif )
		ui->list->expandToDepth(0);

	// Hide Progress Bar
	QTimer::singleShot( timeout, progress, SLOT( hide() ) );
}


void NifSkope::saveAsDlg()
{
	QString filename = QFileDialog::getSaveFileName( this, tr( "Save File" ), nif->getFileInfo().absoluteFilePath(),
		fileFilters( false ),
		new QString( fileFilter( nif->getFileInfo().suffix() ) )
	);

	if ( filename.isEmpty() )
		return;

	saveFile( filename );
}

void NifSkope::onSaveBegin()
{
	setEnabled( false );
}

void NifSkope::onSaveComplete( bool success, QString & fname )
{
	setEnabled( true );

	if ( success ) {
		// Update if Save As results in filename change
		setWindowFilePath( fname );
		// Mark window as unmodified
		nif->undoStack->setClean();
		setWindowModified( false );
		refreshAllDocumentSessions();
	}
}

bool NifSkope::saveConfirm()
{
	if ( !cfg.suppressSaveConfirm && (isWindowModified() || !nif->undoStack->isClean()) ) {
		QMessageBox::StandardButton response;
		response = QMessageBox::question( this,
			tr( "Save Confirmation" ),
			tr( "<h3><b>You have unsaved changes to %1.</b></h3>Would you like to save them now?" ).arg( nif->getFileInfo().completeBaseName() ),
			QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No );

		if ( response == QMessageBox::Yes ) {
			saveAsDlg();
			return true;
		} else if ( response == QMessageBox::No ) {
			return true;
		} else if ( response == QMessageBox::Cancel ) {
			return false;
		}
	}

	return true;
}

void NifSkope::enableUi()
{
	// Re-enable toolbars, actions, and menus
	ui->aSaveMenu->setEnabled( true );
	ui->aSave->setEnabled( true );
	ui->aSaveAs->setEnabled( true );
	ui->aReload->setEnabled( true );
	ui->aHeader->setEnabled( true );

	ui->mRender->setEnabled( true );
	ui->tMode->setEnabled( true );

	ui->tRender->setEnabled( true );

	// We only need to enable the UI once, disconnect
	disconnect( nif, &NifModel::beginUpdateHeader, this, &NifSkope::enableUi );
}

void NifSkope::saveUi() const
{
	// Never persist the layout from a harness run.
	//
	// The WW_RENDER_SHOT harness hides every dock so the framebuffer size depends
	// only on WW_RENDER_SIZE (07-27j). It then quit, saveUi() ran, and that
	// hidden-dock layout BECAME the user's saved layout — NifSkope started with
	// Block List, Block Details, Header and NIF Browser all gone. Reported
	// 2026-07-27.
	//
	// The guard is deliberately generic rather than a test for that one variable:
	// this session alone made roughly thirty harness launches across a dozen WW_*
	// harnesses, several of which hide docks, switch modes or resize the window.
	// None of them should ever leave a trace in the user's UI settings.
	const QStringList envKeys = QProcessEnvironment::systemEnvironment().keys();
	for ( const QString & key : envKeys ) {
		if ( key.startsWith( QLatin1String( "WW_" ) ) )
			return;
	}

	QSettings settings;
	// TODO: saveState takes a version number which can be incremented between releases if necessary
	settings.setValue( "Window State"_uip, saveState( 0x073 ) );
	settings.setValue( "Window Geometry"_uip, saveGeometry() );

	settings.setValue( "Theme", theme );

	settings.setValue( "File/Auto Sanitize", aSanitize->isChecked() );

	settings.setValue( "List Mode"_uip, (gListMode->checkedAction() == aList ? "list" : "hierarchy") );
	settings.setValue( "Show Non-applicable Rows"_uip, aCondition->isChecked() );

	settings.setValue( "List Header"_uip, list->header()->saveState() );
	settings.setValue( "Tree Header"_uip, tree->header()->saveState() );
	settings.setValue( "Header Header"_uip, header->header()->saveState() );
	settings.setValue( "Kfmtree Header"_uip, kfmtree->header()->saveState() );

	settings.setValue( "GLView/Enable Animations", ui->aAnimate->isChecked() );
	//settings.setValue( "GLView/Play Animation", ui->aAnimPlay->isChecked() );
	//settings.setValue( "GLView/Loop Animation", ui->aAnimLoop->isChecked() );
	//settings.setValue( "GLView/Switch Animation", ui->aAnimSwitch->isChecked() );
	// Camera projection and transforms are intentionally session-only. They
	// remain intact while another NIF is opened in this window, but a new
	// NifSkope session starts from the configured startup view in perspective.
	settings.remove( "GLView/Perspective" );
	settings.remove( "GLView/User View" );
}


void NifSkope::restoreUi()
{
	uiRestored = true;
	QSettings settings;
	restoreGeometry( settings.value( "Window Geometry"_uip ).toByteArray() );
	if ( isMaximized() )
		QApplication::processEvents();
	restoreState( settings.value( "Window State"_uip ).toByteArray(), 0x073 );

	/* Re-assert the header order AFTER restoreState.
	 *
	 * The mode toolbar is put ahead of the render toolbar during construction,
	 * so the row reads mode, then the verbs the mode governs, then the transform
	 * widgets - Blender's header order. restoreState replays a layout saved by an
	 * older build and silently undoes it, which is why the reorder appeared to do
	 * nothing: the constructor was right and the restore overwrote it a moment
	 * later, with no error and nothing in the diff to look at.
	 *
	 * Applied here rather than bumping the state version, because the version is
	 * also what preserves everyone's dock arrangement; throwing that away to fix
	 * the order of two toolbars is the wrong trade.
	 */
	/* MOVE THE VIEWPORT TOOLBARS INTO THE FOOTER, under the 3D view.
	 *
	 * Done here, after restoreState, and not at construction: restoreState
	 * replays a layout saved by an older build and would drag these two straight
	 * back into the toolbar area. That is not a hypothesis - the toolbar REORDER
	 * that used to live on this line failed silently for exactly that reason, the
	 * constructor being right and the restore overwriting it a moment later.
	 *
	 * The QToolBar objects themselves are reused rather than replaced, so every
	 * addWidget/addAction call site that built them stays valid and, more
	 * importantly, so does syncViewportMenus - the mode-contextual show/hide
	 * table works on the QActions those addWidget calls returned, and rebuilding
	 * the bars would have invalidated all of them.
	 *
	 * removeToolBar first: it detaches from the toolbar area AND hides, so the
	 * show() afterwards is required, not decorative.
	 */
	if ( viewportHeader && ui->tMode && ui->tRender ) {
		auto * headerRow = qobject_cast<QHBoxLayout *>( viewportHeader->layout() );
		if ( headerRow ) {
			for ( QToolBar * tb : { ui->tMode, ui->tRender } ) {
				removeToolBar( tb );
				tb->setMovable( false );
				tb->setFloatable( false );
				tb->setIconSize( QSize( 16, 16 ) );
				/* A QToolBar reports a tiny minimum because it is BUILT to
				 * overflow into an extension menu, so a layout will happily
				 * squeeze it and hide half its buttons behind a chevron. That is
				 * exactly what happened on the first attempt: Add and Object
				 * vanished off the end of the mode bar and the whole display
				 * group off the render bar, silently, with the buttons still
				 * "there" as far as any code could tell.
				 *
				 * Minimum = sizeHint stops the squeeze. Recomputed whenever the
				 * mode changes the button set, below, because these bars grow and
				 * shrink by design.
				 */
				tb->setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Fixed );
				tb->show();
			}
			/* The mode bar takes its natural width; the render bar takes the
			 * slack. A stretch ITEM after both was the first attempt and it is
			 * what caused the squeeze - it claimed all the spare width, leaving
			 * the toolbars on their minimum, which for a QToolBar is "as small as
			 * you like, I have a chevron". Giving the slack to a real widget
			 * instead means neither is starved, and the render bar's own trailing
			 * space is where the display group would sit if it were right
			 * aligned.
			 */
			headerRow->addWidget( ui->tMode, 0 );
			headerRow->addWidget( ui->tRender, 1 );
		}
	}

	aSanitize->setChecked( settings.value( "File/Auto Sanitize", true ).toBool() );

	if ( settings.value( "List Mode"_uip, "hierarchy" ).toString() == "list" )
		aList->setChecked( true );
	else
		aHierarchy->setChecked( true );

	setListMode();

	aCondition->setChecked( settings.value( "Show Non-applicable Rows"_uip, false ).toBool() );

	list->header()->restoreState( settings.value( "List Header"_uip ).toByteArray() );
	tree->header()->restoreState( settings.value( "Tree Header"_uip ).toByteArray() );
	// after restoreState, which carries its own column visibility and would
	// otherwise win over the default set at construction
	tree->setColumnHidden( NifModel::TypeCol,
		!settings.value( QStringLiteral( "BlockDetails/Show Type Column" ), false ).toBool() );
	header->header()->restoreState( settings.value( "Header Header"_uip ).toByteArray() );
	kfmtree->header()->restoreState( settings.value( "Kfmtree Header"_uip ).toByteArray() );

	// header restoreState from a pre-Reference-column layout leaves the new
	// section visible; it must start hidden everywhere (diff state shows it)
	tree->setColumnHidden( NifModel::WwRefCol, true );
	header->setColumnHidden( NifModel::WwRefCol, true );
	kfmtree->setColumnHidden( NifModel::WwRefCol, true );
	// Same for Summary, in the other direction: a restored pre-Summary layout
	// leaves it visible in the field views, where a per-BLOCK line has nothing to
	// say, and hidden in the Block List, where it is the entire point. Set after
	// restoreState (and after setListMode above) because restoreState carries its
	// own visibility and wins over anything set before it.
	tree->setColumnHidden( NifModel::WwSummaryCol, true );
	header->setColumnHidden( NifModel::WwSummaryCol, true );
	kfmtree->setColumnHidden( NifModel::WwSummaryCol, true );
	// list mode drives the NifModel directly; hierarchy mode goes through the
	// 3-column proxy, where the summary is column 2
	list->setColumnHidden( list->model() == nif ? int( NifModel::WwSummaryCol ) : 2, false );

	auto hideSections = []( NifTreeView * tree, bool hidden ) {
		tree->header()->setSectionHidden( NifModel::ArgCol, hidden );
		tree->header()->setSectionHidden( NifModel::Arr1Col, hidden );
		tree->header()->setSectionHidden( NifModel::Arr2Col, hidden );
		tree->header()->setSectionHidden( NifModel::CondCol, hidden );
		tree->header()->setSectionHidden( NifModel::Ver1Col, hidden );
		tree->header()->setSectionHidden( NifModel::Ver2Col, hidden );
		tree->header()->setSectionHidden( NifModel::VerCondCol, hidden );
	};

	// Hide advanced metadata loaded from nif.xml as it's not useful or necessary for editing
	if ( settings.value( "Settings/Nif/Hide metadata columns", true ).toBool() ) {
		hideSections( tree, true );
		hideSections( header, true );
	} else {
		// Unhide here, or header()->restoreState() will keep them perpetually hidden
		hideSections( tree, false );
		hideSections( header, false );
	}

	ui->aAnimate->setChecked( settings.value( "GLView/Enable Animations", true ).toBool() );
	//ui->aAnimPlay->setChecked( settings.value( "GLView/Play Animation", true ).toBool() );
	//ui->aAnimLoop->setChecked( settings.value( "GLView/Loop Animation", true ).toBool() );
	//ui->aAnimSwitch->setChecked( settings.value( "GLView/Switch Animation", true ).toBool() );

	const bool isPersp = true;
	ui->aViewPerspective->setChecked( isPersp );
	int viewDir = settings.value( "Settings/Render/General/Camera/Startup Direction", 1 ).toInt();
	if ( viewDir == 0 )
		ui->aViewLeft->setChecked( true );
	else if ( viewDir == 1 )
		ui->aViewFront->setChecked( true );
	else if ( viewDir == 2 )
		ui->aViewTop->setChecked( true );

	ogl->setProjection( isPersp );

	QVariant fontVar = settings.value( "UI/View Font" );

	if ( fontVar.canConvert<QFont>() )
		setViewFont( fontVar.value<QFont>() );

	// Modify UI settings that cannot be set in Designer
	tabifyDockWidget( ui->InspectDock, ui->KfmDock );
}

void NifSkope::setViewFont( const QFont & font )
{
	list->setFont( font );
	QFontMetrics metrics( list->font() );
	list->setIconSize( QSize( metrics.horizontalAdvance( "000" ), metrics.lineSpacing() ) );
	tree->setFont( font );
	tree->setIconSize( QSize( metrics.horizontalAdvance( "000" ), metrics.lineSpacing() ) );
	header->setFont( font );
	header->setIconSize( QSize( metrics.horizontalAdvance( "000" ), metrics.lineSpacing() ) );
	kfmtree->setFont( font );
	kfmtree->setIconSize( QSize( metrics.horizontalAdvance( "000" ), metrics.lineSpacing() ) );
//	ogl->setFont( font );
}

void NifSkope::reloadTheme()
{
	for ( QWidget * widget : QApplication::topLevelWidgets() ) {
		NifSkope * win = qobject_cast<NifSkope *>(widget);
		if ( win ) {
			win->loadTheme();
			/* Number fields and the selectors beside them carry their own
			 * stylesheet, built from wwSkinColor at construction - which is
			 * BEFORE loadTheme runs. Without this they keep whichever theme they
			 * were born under while the rest of the window switches, so a light
			 * theme comes up with dark wells in every panel.
			 */
			wwRestyleScrubFields( win );
		}
	}
}

void NifSkope::loadTheme()
{
	QSettings settings;
	auto	a = ui->mTheme->actions();
	{
		int	n = settings.value( "Theme", ThemeDark ).toInt();
		if ( n < 0 || n >= a.size() ) {
			n = 0;
			settings.setValue( "Theme", n );
		}
		theme = WindowTheme( n );
		a[theme]->setChecked( true );
	}

	//setThemeActions();
	setToolbarSize();

	switch ( theme )
	{
	default:
		{
			qsizetype	n = a.size();
			qsizetype	i = std::min< qsizetype >( std::max< qsizetype >( qsizetype( theme ), 0 ), n - 1 );
			if ( i != qsizetype( theme ) ) {
				theme = nstheme::WindowTheme( i );
				settings.setValue( "Theme", theme );
			}
			if ( theme != ThemeDark && theme != ThemeLight ) {
				QApplication::setStyle( QStyleFactory::create( a.at( i )->text() ) );
				qApp->setStyleSheet("");
				qApp->setPalette( style()->standardPalette() );
				return;
			}
		}
		[[fallthrough]];
	case ThemeDark:
	case ThemeLight:
		QApplication::setStyle( QStyleFactory::create( "Fusion" ) );
		break;
	}

	// The six colour keys below are persisted (setTheme writes them, and the
	// General settings pane round-trips them), so they OUTRANK defaultsDark[]
	// on any install that has run before — a new default palette would never
	// show up. Refresh them once per palette revision instead.
	constexpr int themePaletteVersion = 3;	// 3: viewport clear colour joined the skin
	if ( settings.value( "Settings/Theme/Palette Version", 1 ).toInt() != themePaletteVersion ) {
		const QColor * defaults = ( theme == ThemeLight ) ? defaultsLight : defaultsDark;
		settings.setValue( "Settings/Theme/Base Color", defaults[Base] );
		settings.setValue( "Settings/Theme/Base Color Alt", defaults[BaseAlt] );
		settings.setValue( "Settings/Theme/Text", defaults[Text] );
		settings.setValue( "Settings/Theme/Highlight", defaults[Highlight] );
		settings.setValue( "Settings/Theme/Highlight Text", defaults[HighlightText] );
		settings.setValue( "Settings/Theme/Bright Text", defaults[BrightText] );
		// The viewport clear colour is a Render setting with its own stored key,
		// so it survives a palette change too and leaves the largest surface in
		// the window off-skin. Drop the override and let GLView's default (the
		// "viewport" skin colour) apply.
		settings.remove( "Settings/Render/Colors/Background" );
		settings.setValue( "Settings/Theme/Palette Version", themePaletteVersion );
	}

	QPalette pal;
	auto baseC = settings.value( "Settings/Theme/Base Color", defaultsDark[Base] ).value<QColor>();
	auto baseCAlt = settings.value( "Settings/Theme/Base Color Alt", defaultsDark[BaseAlt] ).value<QColor>();
	auto baseCTxt = settings.value( "Settings/Theme/Text", defaultsDark[Text] ).value<QColor>();
	auto baseCHighlight = settings.value( "Settings/Theme/Highlight", defaultsDark[Highlight] ).value<QColor>();
	auto baseCTxtHighlight = settings.value( "Settings/Theme/Highlight Text", defaultsDark[HighlightText] ).value<QColor>();
	auto baseCBrightTxt = settings.value( "Settings/Theme/Bright Text", defaultsDark[BrightText] ).value<QColor>();

	// Fill the standard palette
	pal.setColor( QPalette::Window, baseC );
	pal.setColor( QPalette::WindowText, baseCTxt );
	pal.setColor( QPalette::Base, baseC );
	pal.setColor( QPalette::AlternateBase, baseCAlt );
	pal.setColor( QPalette::ToolTipBase, baseC );
	pal.setColor( QPalette::ToolTipText, baseCTxt );
	pal.setColor( QPalette::Text, baseCTxt );
	pal.setColor( QPalette::Button, baseC );
	pal.setColor( QPalette::ButtonText, baseCTxt );
	pal.setColor( QPalette::BrightText, baseCBrightTxt );
	pal.setColor( QPalette::Link, baseCBrightTxt );
	pal.setColor( QPalette::Highlight, baseCHighlight );
	pal.setColor( QPalette::HighlightedText, baseCTxtHighlight );

	// Mute the disabled palette
	auto baseCDark = baseC.darker( 150 );
	auto baseCAltDark = baseCAlt.darker( 150 );
	auto baseCHighlightDark = QColor( 128, 128, 128 );
	auto baseCTxtDark = Qt::darkGray;
	auto baseCTxtHighlightDark = Qt::darkGray;
	auto baseCBrightTxtDark = Qt::darkGray;

	pal.setColor( QPalette::ColorGroup::Disabled, QPalette::Window, baseC ); // Leave base color the same
	pal.setColor( QPalette::ColorGroup::Disabled, QPalette::WindowText, baseCTxtDark );
	pal.setColor( QPalette::ColorGroup::Disabled, QPalette::Base, baseCDark );
	pal.setColor( QPalette::ColorGroup::Disabled, QPalette::AlternateBase, baseCAltDark );
	pal.setColor( QPalette::ColorGroup::Disabled, QPalette::ToolTipBase, baseCDark );
	pal.setColor( QPalette::ColorGroup::Disabled, QPalette::ToolTipText, baseCTxtDark );
	pal.setColor( QPalette::ColorGroup::Disabled, QPalette::Text, baseCTxtDark );
	pal.setColor( QPalette::ColorGroup::Disabled, QPalette::Button, baseCDark );
	pal.setColor( QPalette::ColorGroup::Disabled, QPalette::ButtonText, baseCTxtDark );
	pal.setColor( QPalette::ColorGroup::Disabled, QPalette::BrightText, baseCBrightTxtDark );
	pal.setColor( QPalette::ColorGroup::Disabled, QPalette::Link, baseCBrightTxtDark );
	pal.setColor( QPalette::ColorGroup::Disabled, QPalette::Highlight, baseCHighlightDark );
	pal.setColor( QPalette::ColorGroup::Disabled, QPalette::HighlightedText, baseCTxtHighlightDark );

	// Set Palette and Stylesheet

	QDir qssDir( QApplication::applicationDirPath() );
	QStringList qssList( QStringList()
						 << "style.qss"
#ifdef Q_OS_LINUX
						 << "/usr/share/nifskope/style.qss"
#endif
	);
	QString qssName;
	for ( const QString& str : qssList ) {
		if ( qssDir.exists( str ) ) {
			qssName = qssDir.filePath( str );
			break;
		}
	}

	// Load stylesheet
	QString styleData;
	if ( !qssName.isEmpty() ) {
		QFile style( qssName );
		if ( style.open( QFile::ReadOnly ) ) {
			styleData = style.readAll();
			style.close();
		}
	}

	// Remove comments first
	QRegularExpression cssComment( R"regex(\/\*[^*]*\*+([^/*][^*]*\*+)*\/)regex" );
	styleData.replace( cssComment, "" );

	// Theme name for icon path customization
	styleData.replace( "${theme}", (theme == ThemeDark) ? "dark" : "light" );

	// Skin surfaces — table at the top of this file, shared with wwSkinColor().
	wwSkinLightTheme = ( theme == ThemeLight );
	for ( const WwSkinVar & v : skinVars )
		styleData.replace( QLatin1String( "${" ) + QLatin1String( v.name ) + QLatin1String( "}" ),
						   QLatin1String( wwSkinLightTheme ? v.light : v.dark ) );

	// Highlight colors in an "R, G, B" string to combine with opacity in rgba()
	auto rgb = QString("%1, %2, %3").arg(baseCHighlight.red())
									.arg(baseCHighlight.green())
									.arg(baseCHighlight.blue());
	styleData.replace( "${rgb}", rgb );

	qApp->setPalette( pal );
	qApp->setStyleSheet( styleData );
}

void NifSkope::setThemeActions()
{
	// Map of QAction object names to QRC alias
	QMap<QString, QString> names = {
		//{"aTextures", "textures"}
	};

	QString themeString = (theme == ThemeDark) ? "dark" : "light";
	for ( auto a : allActions ) {
		auto obj = a->objectName();
		if ( names.contains( obj ) ) {
			a->setIcon( QIcon( QString(":btn/%1/%2").arg(themeString).arg(names[obj]) ) );
		}
	}
}

void NifSkope::setToolbarSize()
{
	/* One size: 16px, matching Blender's header.
	 *
	 * There used to be a "large" alternative at 36px, and it was the default,
	 * which is why flattening the buttons barely changed how big the top bar
	 * looked - the chrome went but the glyphs stayed more than twice Blender's
	 * size. The option is gone rather than merely re-defaulted: its Settings
	 * checkbox was never wired to anything (nothing read or wrote `largeIcons`),
	 * so it was a control that appeared to offer a choice and did not.
	 */
	const QSize size = {16, 16};

	// findChildren for the same reason as the Toolbars menu above: the viewport
	// footer's toolbars are not direct children and would keep the default icon
	// size, so the bottom row would draw at a different scale from the top one.
	for ( QToolBar * tb : findChildren<QToolBar *>() )
		tb->setIconSize( size );
}

void NifSkope::setTheme( nstheme::WindowTheme t )
{
	theme = t;

	QSettings settings;
	settings.setValue( "Theme", theme );

	QColor * defaults = nullptr;
	QString iconPrefix;

	// If Dark reset to dark colors and icons
	// If Light reset to light colors and icons
	switch ( t ) {
	case ThemeDark:
		defaults = defaultsDark;
		break;
	case ThemeLight:
		defaults = defaultsLight;
		break;
	default:
		break;
	}

	if ( defaults ) {
		settings.setValue( "Settings/Theme/Base Color", defaults[Base] );
		settings.setValue( "Settings/Theme/Base Color Alt", defaults[BaseAlt] );
		settings.setValue( "Settings/Theme/Text", defaults[Text] );
		settings.setValue( "Settings/Theme/Highlight", defaults[Highlight] );
		settings.setValue( "Settings/Theme/Highlight Text", defaults[HighlightText] );
		settings.setValue( "Settings/Theme/Bright Text", defaults[BrightText] );
	}

	loadTheme();
}


void NifSkope::positionRedoPanel()
{
	if ( !graphicsView )
		return;
	// only one of the panels is ever visible (they hide each other on show)
	for ( QFrame * p : { gizmoRedoPanel, operatorRedoPanel, boxRedoPanel, operatorExRedoPanel } ) {
		if ( p )
			p->move( graphicsView->mapToGlobal(
				QPoint( 10, graphicsView->height() - p->height() - 10 ) ) );
	}
}

void NifSkope::applyShortcutOverrides()
{
	auto & reg = ShortcutRegistry::get();
	QSettings settings;
	settings.beginGroup( QStringLiteral( "Shortcuts" ) );
	const auto acts = findChildren<QAction *>();
	for ( QAction * a : acts ) {
		const QString name = a->objectName();
		if ( name.isEmpty() )
			continue;
		// remember the factory default before any override touches it
		reg.noteActionDefault( name, a->shortcut() );
		const QVariant v = settings.value( QStringLiteral( "action." ) + name );
		if ( v.isValid() )
			a->setShortcut( QKeySequence::fromString( v.toString(), QKeySequence::PortableText ) );
		else if ( a->shortcut() != reg.actionDefault( name ) )
			a->setShortcut( reg.actionDefault( name ) );	// override was removed
	}
	// Blender-style mouse mapping (select vs place-gizmo click)
	if ( ogl )
		ogl->setSelectWithRightMouse(
			settings.value( QStringLiteral( "MouseSelect" ) ).toString() == QLatin1String( "right" ) );
	settings.endGroup();
}

bool NifSkope::eventFilter( QObject * o, QEvent * e )
{
	// TODO: This doesn't seem to be doing anything extra
	//if ( e->type() == QEvent::Polish ) {
	//	QTimer::singleShot( 0, this, SLOT( overrideViewFont() ) );
	//}

	switch ( e->type() ) {
	case QEvent::Enter:
		// Focus-follows-mouse: hovering the 3D viewport gives its embedded GL
		// window keyboard focus, so A / G / R / S and other keyPressEvent-based
		// shortcuts work without a prior click (the UV editor does the same in
		// its own enterEvent). Skipped while a text field is being edited.
		if ( ogl && graphicsView && ( o == ogl || o == graphicsView ) && isActiveWindow() ) {
			QWidget * fw = QApplication::focusWidget();
			const bool editingText = fw && ( fw->inherits( "QLineEdit" )
				|| fw->inherits( "QTextEdit" ) || fw->inherits( "QPlainTextEdit" )
				|| fw->inherits( "QAbstractSpinBox" ) );
			if ( !editingText ) {
				graphicsView->setFocus( Qt::MouseFocusReason );
				ogl->requestActivate();   // the same call the free camera uses to take keys
			}
		}
		break;

	case QEvent::Move:
	case QEvent::Resize:
		// keep the floating redo panels glued to the viewport
		if ( o == this || o == graphicsView ) {
			for ( QFrame * p : { gizmoRedoPanel, operatorRedoPanel, boxRedoPanel, operatorExRedoPanel } ) {
				if ( p && p->isVisible() ) {
					positionRedoPanel();
					break;
				}
			}
		}
		break;

	case QEvent::WindowStateChange:
		if ( o == this && isMinimized() ) {
			for ( QFrame * p : { gizmoRedoPanel, operatorRedoPanel, boxRedoPanel, operatorExRedoPanel } ) {
				if ( p && p->isVisible() )
					p->hide();
			}
		}
		break;

	case QEvent::ShortcutOverride:
	case QEvent::KeyPress: {
		// Shift+F toggles the free camera whenever the pointer is over the
		// viewport, no matter which widget currently has key focus. Handle both
		// entry and exit here: a QOpenGLWindow embedded through
		// createWindowContainer cannot be relied on as the sole recipient of the
		// second shortcut. Text-input widgets keep their keystrokes.
		auto ke = static_cast<QKeyEvent *>( e );
		// Block List: X or Delete removes the selected block(s) and their
		// branches (Blender-style, with the same "Delete selected objects?"
		// confirmation as the viewport). Works on a multi-selection.
		if ( o == list && ogl && nif
			&& ( ke->key() == Qt::Key_X || ke->key() == Qt::Key_Delete )
			&& !( ke->modifiers() & ( Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier ) ) ) {
			if ( e->type() == QEvent::KeyPress && !ke->isAutoRepeat() ) {
				QSet<int> sel;
				for ( const QModelIndex & pidx : list->selectionModel()->selectedIndexes() ) {
					if ( pidx.column() != 0 )
						continue;
					QModelIndex idx = pidx;
					if ( idx.model() == proxy )
						idx = proxy->mapTo( idx );
					int b = nif->getBlockNumber( idx );
					if ( b >= 0 )
						sel.insert( b );
				}
				if ( !sel.isEmpty() )
					ogl->deleteBlocksWithConfirm( QVector<int>( sel.constBegin(), sel.constEnd() ) );
			}
			return true;
		}
		/* Loaded NIFs: X or Delete removes the selected documents.
		 *
		 * Same key as the Block List, on a list that behaves like it — multi-select,
		 * same highlight colours, right-click acting on the selection. Removing a row
		 * used to be buried in a per-row menu.
		 */
		if ( o == loadedNifsView
			&& ( ke->key() == Qt::Key_X || ke->key() == Qt::Key_Delete )
			&& !( ke->modifiers() & ( Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier ) ) ) {
			if ( e->type() == QEvent::KeyPress && !ke->isAutoRepeat() )
				removeSelectedWorkspaceDocuments();
			return true;
		}
		// Block List Ctrl+C / Ctrl+V are the Copy Branch / Paste Branch spells
		// (QKeySequence::Copy/Paste hotkeys). They are multi-selection aware in
		// the spells themselves — Copy Branch unions every selected block's
		// branch (via the published block-list selection), Paste Branch slots
		// each pasted root back in — so no dedicated event-filter handler here.
		const bool pointerOverViewport = ogl && graphicsView && isActiveWindow()
			&& graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) );
		QWidget * keyFocus = QApplication::focusWidget();
		const bool keyFocusIsTextInput = keyFocus && ( keyFocus->inherits( "QLineEdit" )
			|| keyFocus->inherits( "QTextEdit" ) || keyFocus->inherits( "QPlainTextEdit" )
			|| keyFocus->inherits( "QAbstractSpinBox" ) || keyFocus->inherits( "QComboBox" ) );
		// rebindable viewport shortcuts (see tlRegisterViewportShortcuts)
		const auto & vpKeys = ShortcutRegistry::get();
		/* A modal transform is TYPING, and this filter must not eat its keys.
		 *
		 * G/R/S start a Blender-style transform whose numeric entry collects
		 * digits and a decimal point into GLView's gizmoNum buffer. That buffer
		 * is behind `if ( gizmoMode )` in GLView::keyPressEvent, which is
		 * downstream of this application-wide filter — so any branch here that
		 * claims a bare key claims it first.
		 *
		 * "viewport.frame_selection" is bound to a bare period. During a
		 * transform the pointer is over the viewport by definition and focus is
		 * not a text widget, so both of its guards pass and '.' was consumed as
		 * Frame Selection: typing 11.25 produced 1125, with the decimal point and
		 * nothing else silently dropped. Digits were unaffected, which is exactly
		 * why it read as "the decimal key does not work".
		 */
		const bool modalTransform = ogl && ( ogl->gizmoMode || ogl->elemTransform );
		// loop cut is modal: Esc cancels, Enter confirms, digits/+/-/Backspace
		// set the cut count; every other single-key viewport shortcut stays
		// inert while it is armed
		if ( ogl && ogl->loopCutActive && !keyFocusIsTextInput ) {
			if ( ke->key() == Qt::Key_Escape || ke->key() == Qt::Key_Return
				|| ke->key() == Qt::Key_Enter ) {
				if ( e->type() == QEvent::KeyPress ) {
					if ( ke->key() == Qt::Key_Escape )
						ogl->cancelLoopCut();
					else
						ogl->loopCutConfirmRing();
				}
				e->accept();
				return true;
			}
			if ( e->type() == QEvent::KeyPress && ogl->loopCutModalKey( ke->key() ) ) {
				e->accept();
				return true;
			}
			if ( pointerOverViewport ) {
				e->accept();
				return true;
			}
		}
		// the knife is modal: the viewport handles Enter/Esc itself; every
		// other single-key viewport shortcut stays inert while it is armed
		if ( ogl && ogl->knifeActive && !keyFocusIsTextInput ) {
			if ( ke->key() == Qt::Key_Escape || ke->key() == Qt::Key_Return
				|| ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Z ) {
				if ( e->type() == QEvent::KeyPress ) {
					if ( ke->key() == Qt::Key_Escape )
						ogl->cancelKnife();
					else if ( ke->key() == Qt::Key_Z )
						ogl->knifeToggleCutThrough();
					else
						ogl->knifeApply();
				}
				e->accept();
				return true;
			}
			if ( pointerOverViewport ) {
				e->accept();
				return true;
			}
		}
		// Blender numpad navigation is viewport-scoped. Reserve recognized keys
		// during ShortcutOverride so the legacy QAction shortcuts cannot recenter
		// the model before the viewport-preserving handler sees the KeyPress.
		/* Also skipped during a modal transform. ShortcutRegistry::matches masks
		 * KeypadModifier off, so the numpad decimal key reaches the same
		 * frame_selection binding as the main-row one — and handleBlenderNumpad
		 * maps keypad Period/Comma to frameSelected() on its own account. Without
		 * this guard the numpad decimal is eaten twice over, and the numpad digits
		 * go to view navigation instead of the number being typed.
		 */
		if ( pointerOverViewport && !keyFocusIsTextInput && !modalTransform
			&& ogl->handleBlenderNumpad( ke->key(), ke->modifiers(), e->type() == QEvent::KeyPress ) ) {
			if ( e->type() == QEvent::KeyPress ) {
				QSignalBlocker blocker( ui->aViewPerspective );
				ui->aViewPerspective->setChecked( ogl->isPerspectiveProjection() );
			}
			e->accept();
			return true;
		}
		// Tab toggles Object Mode against the last non-object mode. The mode
		// memory is shared with selector-driven changes, so Object <-> Edit and
		// Object <-> Weight Paint behave consistently. Future paint modes can
		// join by enabling their existing selector actions.
		if ( pointerOverViewport && !keyFocusIsTextInput
			&& vpKeys.matches( "viewport.toggle_edit_mode", ke->key(), ke->modifiers() ) ) {
			if ( e->type() == QEvent::KeyPress && !ke->isAutoRepeat() ) {
				if ( ogl->vertexPaintModeActive() ) {
					lastViewportNonObjectMode = 2;
					ogl->setVertexPaintMode( false );
				} else if ( ogl->riggingWeightPaintModeActive() ) {
					lastViewportNonObjectMode = 3;
					ogl->setRiggingWeightPaintMode( false );
				} else if ( ogl->segmentPaintModeActive() ) {
					lastViewportNonObjectMode = 4;
					ogl->setSegmentPaintMode( false );
				} else if ( ogl->editModeActive() ) {
					lastViewportNonObjectMode = 1;
					ogl->setEditMode( false );
				} else {
					const char * actionName = lastViewportNonObjectMode == 2
						? "ViewportVertexPaintAction" : lastViewportNonObjectMode == 3
						? "ViewportWeightPaintAction" : lastViewportNonObjectMode == 4
						? "ViewportSegmentPaintAction" : "ViewportEditModeAction";
					QAction * previousMode = findChild<QAction *>( QString::fromLatin1( actionName ) );
					if ( previousMode && previousMode->isEnabled() )
						previousMode->trigger();
					else
						ogl->setEditMode( true );
				}
			}
			e->accept();
			return true;
		}
		if ( pointerOverViewport && ogl->riggingWeightPaintModeActive()
			&& !keyFocusIsTextInput
			&& vpKeys.matches( "viewport.paint_fill", ke->key(), ke->modifiers() ) ) {
			if ( e->type() == QEvent::KeyPress && !ke->isAutoRepeat() )
				ogl->fillRiggingWeightSelection();
			e->accept();
			return true;
		}
		if ( pointerOverViewport && ogl->segmentPaintModeActive()
			&& !keyFocusIsTextInput
			&& vpKeys.matches( "viewport.paint_fill", ke->key(), ke->modifiers() ) ) {
			if ( e->type() == QEvent::KeyPress && !ke->isAutoRepeat() )
				ogl->fillSegmentPaintSelection();
			e->accept();
			return true;
		}
		// Alt+H (unhide, Blender) would otherwise be eaten by the Help menu
		// mnemonic before the viewport ever sees the key
		if ( pointerOverViewport
			&& vpKeys.matches( "viewport.unhide_all", ke->key(), ke->modifiers() ) ) {
			if ( e->type() == QEvent::KeyPress ) {
				if ( ogl->editMode )
					ogl->unhideAllElements();
				else
					ogl->unhideAll();
			}
			e->accept();
			return true;
		}
		// Ctrl+P / Alt+P are Blender-style Set/Clear Parent in object mode.
		// They work over both the viewport and the Block List; the latter already
		// mirrors its row selection into GLView's object selection.
		if ( ogl && graphicsView && !ogl->editMode
			&& isActiveWindow()
			&& ( graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) )
				|| ( list && list->rect().contains( list->mapFromGlobal( QCursor::pos() ) ) ) ) ) {
			bool setParent = vpKeys.matches( "viewport.parent_set", ke->key(), ke->modifiers() );
			bool clearParent = vpKeys.matches( "viewport.parent_clear", ke->key(), ke->modifiers() );
			QWidget * fw = QApplication::focusWidget();
			bool textInput = fw && ( fw->inherits( "QLineEdit" ) || fw->inherits( "QTextEdit" )
				|| fw->inherits( "QPlainTextEdit" ) || fw->inherits( "QAbstractSpinBox" )
				|| fw->inherits( "QComboBox" ) );
			if ( ( setParent || clearParent ) && !textInput ) {
				if ( e->type() == QEvent::KeyPress ) {
					if ( setParent ) ogl->showParentMenu(); else ogl->showClearParentMenu();
				}
				e->accept();
				return true;
			}
		}
		// P opens the edit-mode Separate menu (Blender). The View > Perspective
		// action also uses P, so only steal it while editing with the pointer
		// over the viewport; object mode keeps P = perspective toggle.
		if ( pointerOverViewport && ogl->editMode && !ogl->riggingWeightPaintModeActive()
			&& !ogl->vertexPaintModeActive()
			&& vpKeys.matches( "viewport.separate", ke->key(), ke->modifiers() ) ) {
			if ( e->type() == QEvent::KeyPress )
				ogl->showSeparateMenu();
			e->accept();
			return true;
		}
		// the edit-mode Merge menu (Blender M) with the pointer over the view
		if ( pointerOverViewport && ogl->editMode && !ogl->riggingWeightPaintModeActive()
			&& !ogl->vertexPaintModeActive()
			&& vpKeys.matches( "viewport.merge", ke->key(), ke->modifiers() ) ) {
			if ( e->type() == QEvent::KeyPress )
				ogl->showMergeMenu();
			e->accept();
			return true;
		}
		// E extrudes the selection (Blender) with the pointer over the view;
		// in free-camera / walk mode E stays camera-up
		if ( pointerOverViewport && ogl->editMode && !ogl->freeCamera
			&& !ogl->riggingWeightPaintModeActive() && !ogl->vertexPaintModeActive()
			&& vpKeys.matches( "viewport.extrude", ke->key(), ke->modifiers() ) ) {
			if ( e->type() == QEvent::KeyPress && !ke->isAutoRepeat() )
				ogl->extrudeRegion();
			e->accept();
			return true;
		}
		// Blender F: form a quad from two adjacent picked triangles, else
		// fill a hole / bridge two rims; F stays Front View everywhere else
		if ( pointerOverViewport && ogl->editMode && !ogl->freeCamera
			&& !ogl->riggingWeightPaintModeActive() && !ogl->vertexPaintModeActive()
			&& vpKeys.matches( "viewport.fill", ke->key(), ke->modifiers() ) ) {
			if ( e->type() == QEvent::KeyPress && !ke->isAutoRepeat() )
				ogl->makeFace();
			e->accept();
			return true;
		}
		// the Blender modeling operators (edit mode, pointer over the view):
		// inset, loop cut, edge slide, dissolve
		if ( pointerOverViewport && ogl->editMode && !ogl->freeCamera
			&& !ogl->riggingWeightPaintModeActive() && !ogl->vertexPaintModeActive()
			&& !ogl->segmentPaintModeActive() ) {
			int op = 0;
			if ( vpKeys.matches( "viewport.inset", ke->key(), ke->modifiers() ) )
				op = 1;
			else if ( vpKeys.matches( "viewport.loop_cut", ke->key(), ke->modifiers() ) )
				op = 2;
			else if ( vpKeys.matches( "viewport.edge_slide", ke->key(), ke->modifiers() ) )
				op = 3;
			else if ( vpKeys.matches( "viewport.dissolve", ke->key(), ke->modifiers() ) )
				op = 4;
			else if ( vpKeys.matches( "viewport.tris_to_quads", ke->key(), ke->modifiers() ) )
				op = 5;
			else if ( vpKeys.matches( "viewport.triangulate", ke->key(), ke->modifiers() ) )
				op = 6;
			else if ( vpKeys.matches( "viewport.knife", ke->key(), ke->modifiers() ) )
				op = 7;
			else if ( vpKeys.matches( "viewport.bevel", ke->key(), ke->modifiers() ) )
				op = 8;
			if ( op ) {
				if ( e->type() == QEvent::KeyPress && !ke->isAutoRepeat() ) {
					if ( op == 1 )
						ogl->insetRegion();
					else if ( op == 2 )
						ogl->loopCut();
					else if ( op == 3 )
						ogl->edgeSlide();
					else if ( op == 4 )
						ogl->dissolveVerts();
					else if ( op == 5 )
						ogl->trisToQuads();
					else if ( op == 6 )
						ogl->triangulateSelection( 0 );
					else if ( op == 7 )
						ogl->beginKnife();
					else
						ogl->bevelSelection();
				}
				e->accept();
				return true;
			}
		}
		// W opens the Blender-style Specials quick menu (edit + object mode);
		// in free-camera / walk mode W stays camera-forward
		if ( pointerOverViewport && !ogl->freeCamera && ogl->view != GLView::ViewWalk
			&& !ogl->riggingWeightPaintModeActive() && !ogl->vertexPaintModeActive()
			&& !ogl->segmentPaintModeActive()
			&& vpKeys.matches( "viewport.quick_menu", ke->key(), ke->modifiers() ) ) {
			QWidget * fw = QApplication::focusWidget();
			const bool textInput = fw && ( fw->inherits( "QLineEdit" ) || fw->inherits( "QTextEdit" )
				|| fw->inherits( "QPlainTextEdit" ) || fw->inherits( "QAbstractSpinBox" ) );
			if ( !textInput ) {
				if ( e->type() == QEvent::KeyPress && !ke->isAutoRepeat() )
					ogl->showSpecialsMenu();
				e->accept();
				return true;
			}
		}
		// add a primitive (object mode, pointer over the view, Blender Shift+A)
		if ( pointerOverViewport && !ogl->editMode && !ogl->freeCamera
			&& vpKeys.matches( "viewport.add_primitive", ke->key(), ke->modifiers() ) ) {
			if ( e->type() == QEvent::KeyPress && !ke->isAutoRepeat() )
				ogl->showAddPrimitiveMenu();
			e->accept();
			return true;
		}
		if ( pointerOverViewport
			&& vpKeys.matches( "viewport.free_camera", ke->key(), ke->modifiers() ) ) {
			QWidget * fw = QApplication::focusWidget();
			bool textInput = fw && ( fw->inherits( "QLineEdit" ) || fw->inherits( "QTextEdit" )
				|| fw->inherits( "QPlainTextEdit" ) || fw->inherits( "QAbstractSpinBox" )
				|| fw->inherits( "QComboBox" ) );
			if ( !textInput ) {
				if ( e->type() == QEvent::KeyPress && !ke->isAutoRepeat() )
					ogl->setFreeCamera( !ogl->freeCamera );
				e->accept();
				return true;
			}
		}
		// arm Blender box select (edit or object mode) with the pointer over
		// the viewport, whatever widget has key focus
		if ( pointerOverViewport
			&& vpKeys.matches( "viewport.select.box", ke->key(), ke->modifiers() ) ) {
			QWidget * fw = QApplication::focusWidget();
			bool textInput = fw && ( fw->inherits( "QLineEdit" ) || fw->inherits( "QTextEdit" )
				|| fw->inherits( "QPlainTextEdit" ) || fw->inherits( "QAbstractSpinBox" )
				|| fw->inherits( "QComboBox" ) );
			if ( !textInput ) {
				if ( e->type() == QEvent::KeyPress )
					ogl->beginBoxSelect();
				e->accept();
				return true;
			}
		}
		// invert the selection (edit: elements, object: objects)
		if ( pointerOverViewport
			&& vpKeys.matches( "viewport.select.invert", ke->key(), ke->modifiers() ) ) {
			if ( e->type() == QEvent::KeyPress )
				ogl->invertSelection();
			e->accept();
			return true;
		}
		// arm the circle-select brush (edit or object mode) with the pointer
		// over the viewport, whatever widget has key focus
		if ( pointerOverViewport
			&& vpKeys.matches( "viewport.select.circle", ke->key(), ke->modifiers() ) ) {
			QWidget * fw = QApplication::focusWidget();
			bool textInput = fw && ( fw->inherits( "QLineEdit" ) || fw->inherits( "QTextEdit" )
				|| fw->inherits( "QPlainTextEdit" ) || fw->inherits( "QAbstractSpinBox" )
				|| fw->inherits( "QComboBox" ) );
			if ( !textInput ) {
				if ( e->type() == QEvent::KeyPress )
					ogl->beginCircleSelect();
				e->accept();
				return true;
			}
		}
		// grow / shrink the edit-mode selection (Select More/Less); the keyboard
		// produces either Plus or Equal for the same physical key
		if ( pointerOverViewport && ogl->editMode ) {
			const int mlKey = ( ke->key() == Qt::Key_Plus ) ? Qt::Key_Equal : ke->key();
			const bool more = vpKeys.matches( "viewport.select.more", mlKey, ke->modifiers() );
			const bool less = !more && vpKeys.matches( "viewport.select.less", mlKey, ke->modifiers() );
			if ( more || less ) {
				if ( e->type() == QEvent::KeyPress )
					ogl->selectMoreLess( more );
				e->accept();
				return true;
			}
		}
		// frame the selection with the pointer over the view (Blender Numpad-.)
		// -- but not while a modal transform is collecting a number; see
		// modalTransform above
		if ( pointerOverViewport && !modalTransform
			&& vpKeys.matches( "viewport.frame_selection", ke->key(), ke->modifiers() ) ) {
			QWidget * fw = QApplication::focusWidget();
			bool textInput = fw && ( fw->inherits( "QLineEdit" ) || fw->inherits( "QTextEdit" )
				|| fw->inherits( "QPlainTextEdit" ) || fw->inherits( "QAbstractSpinBox" )
				|| fw->inherits( "QComboBox" ) );
			if ( !textInput ) {
				if ( e->type() == QEvent::KeyPress )
					ogl->frameSelected();
				e->accept();
				return true;
			}
		}
		// Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y step through the SELECTION history first
		// (Blender: selections are undoable) when the pointer is over the viewport.
		// If there is no selection change to revert, fall through to the global
		// model undo/redo so mesh edits still undo normally.
		if ( ogl && graphicsView && isActiveWindow()
			&& ( ke->modifiers() & Qt::ControlModifier ) && !( ke->modifiers() & Qt::AltModifier )
			&& ( ke->key() == Qt::Key_Z || ke->key() == Qt::Key_Y )
			&& graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) ) ) {
			bool redo = ( ke->key() == Qt::Key_Y ) || ( ke->modifiers() & Qt::ShiftModifier );
			bool avail = redo ? ogl->hasSelectionRedo() : ogl->hasSelectionUndo();
			if ( avail ) {
				if ( e->type() == QEvent::KeyPress )
					( redo ? ogl->selectionRedo() : ogl->selectionUndo() );
				e->accept();
				return true;
			}
			// nothing selection-wise to undo: let the global undo/redo run
		}
		// G / R / S start a modal transform when the pointer is over the viewport,
		// so an object/node picked in the block list (which keeps key focus) can be
		// moved/rotated/scaled. Only starts when no gesture is running - during a
		// gesture the viewport keeps in-gesture mode switching.
		if ( pointerOverViewport && !ogl->freeCamera && !ogl->riggingWeightPaintModeActive()
			&& !ogl->vertexPaintModeActive()
			&& e->type() == QEvent::KeyPress && !keyFocusIsTextInput ) {
			int mode = 0;
			if ( vpKeys.matches( "viewport.transform.move", ke->key(), ke->modifiers() ) )
				mode = 1;
			else if ( vpKeys.matches( "viewport.transform.rotate", ke->key(), ke->modifiers() ) )
				mode = 2;
			else if ( vpKeys.matches( "viewport.transform.scale", ke->key(), ke->modifiers() ) )
				mode = 3;
			if ( mode && ogl->startModalTransform( mode ) ) {
				e->accept();
				return true;
			}
		}
		// A / Alt+A = select all / deselect all when the pointer is over the
		// viewport, routed by pointer position (like G/R/S) rather than keyboard
		// focus — focus-follows-mouse alone is unreliable when focus is taken
		// while the pointer is already inside the viewport, so A "sometimes"
		// missed. This makes it work every time the cursor is over the viewport.
		if ( pointerOverViewport && !ogl->freeCamera && e->type() == QEvent::KeyPress
			&& !ke->isAutoRepeat() && !keyFocusIsTextInput && nif ) {
			const bool selAll = vpKeys.matches( "viewport.select.all", ke->key(), ke->modifiers() );
			const bool selNone = !selAll && vpKeys.matches( "viewport.select.none", ke->key(), ke->modifiers() );
			if ( selAll || selNone ) {
				ogl->selectAll( selNone ? 2 : 0 );
				e->accept();
				return true;
			}
		}
		break;
	}
	case QEvent::MouseButtonPress: {
		// Right-clicking an entry in the Recent Files / Recent Archives menus
		// (or the toolbar Open flyout) offers opening in a new window instead
		// of replacing this window's document. Scoped to exactly these menus.
		if ( o == ui->mRecentFiles || o == ui->mRecentArchives
			|| o == mRecentArchiveFiles || o == mOpenFlyout ) {
			auto me = static_cast<QMouseEvent *>( e );
			if ( me->button() != Qt::RightButton )
				break;
			QMenu * menu = static_cast<QMenu *>( o );
			QAction * act = menu->actionAt( me->position().toPoint() );
			int kind = -1;	// 0 = NIF file, 1 = archive, 2 = file inside archive
			for ( int i = 0; i < NumRecentFiles && kind < 0; i++ ) {
				if ( act == recentFileActs[i] )
					kind = 0;
				else if ( act == recentArchiveActs[i] )
					kind = 1;
				else if ( act == recentArchiveFileActs[i] )
					kind = 2;
			}
			const QString path = act ? act->data().toString() : QString();
			if ( kind >= 0 && !path.isEmpty() ) {
				const QPoint globalPos = me->globalPosition().toPoint();
				// pop up OVER the still-open menu chain (Qt stacks popups the
				// same way submenus do). Only an actual choice closes the
				// chain — dismissing drops back into the open menu.
				QMenu ctx( this );
				QAction * inNew = ctx.addAction( tr( "Open in New Window" ) );
				if ( ctx.exec( globalPos ) == inNew ) {
					for ( QWidget * w : QApplication::topLevelWidgets() )
						if ( qobject_cast<QMenu *>( w ) && w->isVisible() )
							w->close();
					if ( kind == 0 ) {
						NifSkope::createWindow( path );
					} else if ( kind == 1 ) {
						NifSkope * w = NifSkope::createWindow();
						w->openArchive( path );
					} else {
						openArchiveFileString( currentArchive, path, true );
					}
				}
			}
			// swallow every right-click in these menus (some styles would
			// otherwise trigger the entry, i.e. replace the document)
			e->accept();
			return true;
		}
		break;
	}

	case QEvent::MouseButtonRelease:
		// Global mouse release
		if ( o->isWindowType() ) {
			//qDebug() << "Mouse Release";

			// Back/Forward button support for cycling through indices
			auto mouseEvent = static_cast<QMouseEvent *>(e);
			if ( mouseEvent ) {
				if ( mouseEvent->button() == Qt::ForwardButton ) {
					mouseEvent->accept();
					indexStack->redo();
				}

				if ( mouseEvent->button() == Qt::BackButton ) {
					mouseEvent->accept();
					indexStack->undo();
				}
			}
		}
		break;

	case QEvent::ContextMenu:
		if ( o == ogl ) {
			ogl->contextMenuEvent( static_cast< QContextMenuEvent * >(e) );
			return true;
		}
		break;
	case QEvent::DragEnter:
		if ( o == ogl ) {
			ogl->dragEnterEvent( static_cast< QDragEnterEvent * >(e) );
			return true;
		}
		break;
	case QEvent::DragLeave:
		if ( o == ogl ) {
			ogl->dragLeaveEvent( static_cast< QDragLeaveEvent * >(e) );
			return true;
		}
		break;
	case QEvent::DragMove:
		if ( o == ogl ) {
			ogl->dragMoveEvent( static_cast< QDragMoveEvent * >(e) );
			return true;
		}
		break;
	case QEvent::Drop:
		if ( o == ogl ) {
			ogl->dropEvent( static_cast< QDropEvent * >(e) );
			return true;
		}
		break;

	default:
		break;
	}

	return QMainWindow::eventFilter( o, e );
}


/*
* Slots
* **********************
*/


void NifSkope::transferNormalsFromSelection( int target, const QVector<int> & sources )
{
	if ( !nif || target < 0 || sources.isEmpty() )
		return;

	// Mapping and mix, and nothing else: the selection already said which meshes
	QDialog dlg( this );
	dlg.setWindowTitle( tr( "Transfer Normals" ) );
	QGridLayout * lay = new QGridLayout( &dlg );
	QLabel * what = new QLabel( tr( "From %n selected mesh(es) onto '%1'", "", sources.size() )
		.arg( nif->get<QString>( nif->getBlockIndex( target ), "Name" ) ), &dlg );
	what->setWordWrap( true );
	lay->addWidget( what, 0, 0, 1, 2 );

	QComboBox * mapBox = new QComboBox( &dlg );
	for ( int m = 0; m < NormalTransfer::MappingCount; m++ )
		mapBox->addItem( NormalTransfer::mappingName( m ) );
	mapBox->setCurrentIndex( NormalTransfer::NearestFaceInterpolated );
	// index for index cannot mean anything across a combination of meshes
	if ( sources.size() > 1 )
		qobject_cast<QStandardItemModel *>( mapBox->model() )
			->item( NormalTransfer::Topology )->setEnabled( false );

	QDoubleSpinBox * mixBox = new QDoubleSpinBox( &dlg );
	mixBox->setRange( 0.0, 1.0 );
	mixBox->setSingleStep( 0.05 );
	mixBox->setValue( 1.0 );

	lay->addWidget( new QLabel( tr( "Mapping" ), &dlg ), 1, 0 );
	lay->addWidget( mapBox, 1, 1 );
	lay->addWidget( new QLabel( tr( "Mix Factor" ), &dlg ), 2, 0 );
	lay->addWidget( mixBox, 2, 1 );
	QPushButton * ok = new QPushButton( tr( "Transfer" ), &dlg );
	QPushButton * cancel = new QPushButton( tr( "Cancel" ), &dlg );
	connect( ok, &QPushButton::clicked, &dlg, &QDialog::accept );
	connect( cancel, &QPushButton::clicked, &dlg, &QDialog::reject );
	lay->addWidget( cancel, 3, 0 );
	lay->addWidget( ok, 3, 1 );
	if ( dlg.exec() != QDialog::Accepted )
		return;

	const int mapping = mapBox->currentIndex();
	const float mix = float( mixBox->value() );

	QVector<NormalTransfer::Mesh> parts;
	for ( int b : sources ) {
		NormalTransfer::Mesh m = NormalTransfer::read( nif, b );
		if ( m.valid() )
			parts.append( m );
	}
	const NormalTransfer::Mesh tgt = NormalTransfer::read( nif, target );
	if ( parts.isEmpty() || !tgt.valid() ) {
		QMessageBox::warning( this, tr( "Transfer Normals" ),
			tr( "Nothing to read: the selected meshes carry no normals." ) );
		return;
	}
	const NormalTransfer::Mesh src = NormalTransfer::combine( parts );
	if ( mapping == NormalTransfer::Topology && src.pos.size() != tgt.pos.size() ) {
		QMessageBox::warning( this, tr( "Transfer Normals" ),
			tr( "Topology mapping needs the same vertex count on both sides: "
				"the source has %1, the target %2." ).arg( src.pos.size() ).arg( tgt.pos.size() ) );
		return;
	}

	QApplication::setOverrideCursor( Qt::WaitCursor );
	const QVector<Vector3> result = NormalTransfer::map( src, tgt, mapping, mix );
	const int written = NormalTransfer::apply( nif, target, result );
	QApplication::restoreOverrideCursor();

	// how far they actually turned, because "N written" is equally true of a
	// transfer that changed nothing
	double worst = 0.0, total = 0.0;
	for ( int v = 0; v < result.size() && v < tgt.nrm.size(); v++ ) {
		Vector3 a = tgt.nrm.at( v ), b = result.at( v );
		if ( a.squaredLength() < 1.0e-12f || b.squaredLength() < 1.0e-12f )
			continue;
		a.normalize();
		b.normalize();
		const double ang = std::acos( std::clamp( double( Vector3::dotproduct( a, b ) ), -1.0, 1.0 ) )
			* 180.0 / M_PI;
		worst = std::max( worst, ang );
		total += ang;
	}
	QMessageBox::information( this, tr( "Transfer Normals" ),
		tr( "%1 of %2 normal(s) written using %3.\n\nTurned by %4° on average, %5° at most." )
			.arg( written ).arg( tgt.pos.size() ).arg( NormalTransfer::mappingName( mapping ) )
			.arg( result.isEmpty() ? 0.0 : total / result.size(), 0, 'f', 2 )
			.arg( worst, 0, 'f', 2 ) );
}

/*! Everything in the Block List's menu that is not a spell.
 *
 *  Split out of contextMenu so it can be built without a right-click: the verb
 *  row and the trailing group are the parts a reader cannot check by eye, and
 *  contextMenu ends in exec(), which blocks. See WW_MENUTREE_TEST.
 *
 *  \param book a SpellBook already built against  idx
 */
/*! Select & View — the six entries that work on the scene, not on the file.
 *
 *  Built here rather than as spells because five of the six are private members
 *  of GLView, and glview.h grants friendship to NifSkope only. A spell can reach
 *  the GLView pointer but not isolateSelected, restoreAllVisibility or
 *  joinSelectedObjects.
 *
 *  Two entries had to be written from scratch: Select Branch and Select Same
 *  Type do not exist anywhere in the tree. They are the reason the rest of this
 *  submenu is worth having — three entries in this menu consume a
 *  multi-selection and, until now, nothing in it could build one.
 */
void NifSkope::buildBlockListSelectAndView( SpellBook & contextBook, const QModelIndex & idx )
{
	if ( !nif || !ogl || ogl->editMode )
		return;
	const int bn = nif->getBlockNumber( idx );
	if ( bn < 0 )
		return;

	auto selectInList = [this]( const QList<int> & blocks ) {
		if ( !list->selectionModel() )
			return;
		list->selectionModel()->clearSelection();
		for ( int b : blocks ) {
			QModelIndex src = nif->getBlockIndex( b );
			if ( !src.isValid() )
				continue;
			QModelIndex vis = proxy ? proxy->mapFrom( src, QModelIndex() ) : src;
			if ( vis.isValid() )
				list->selectionModel()->select( vis,
					QItemSelectionModel::Select | QItemSelectionModel::Rows );
		}
		// wireBlockListSelection turns the list selection into ogl->objSelection,
		// so everything below sees it without a second code path
	};

	QMenu * view = new QMenu( tr( "Select && View" ), &contextBook );

	QAction * aBranch = view->addAction( tr( "Select Branch" ) );
	aBranch->setToolTip( tr( "Select this block and everything under it" ) );
	connect( aBranch, &QAction::triggered, this, [this, bn, selectInList]() {
		QList<int> branch;
		QList<int> queue{ bn };
		// breadth-first with a seen set: a NIF is a DAG, not a tree — a shared
		// texture set under two shapes would otherwise be walked twice, and a
		// cycle would not terminate at all
		while ( !queue.isEmpty() ) {
			const int b = queue.takeFirst();
			if ( branch.contains( b ) || !nif->isValidBlockNumber( b ) )
				continue;
			branch << b;
			for ( int c : nif->getChildLinks( b ) )
				queue << c;
		}
		selectInList( branch );
	} );

	QAction * aSame = view->addAction( tr( "Select Same Type" ) );
	const QString typeName = nif->itemName( nif->getBlockIndex( bn ) );
	aSame->setToolTip( tr( "Select every %1 in the file" ).arg( typeName ) );
	connect( aSame, &QAction::triggered, this, [this, typeName, selectInList]() {
		// exact type, not blockInherits: "same type" answering with every
		// NiAVObject when you clicked an NiNode is not what the label says
		QList<int> same;
		for ( int b = 0; b < nif->getBlockCount(); b++ )
			if ( nif->itemName( nif->getBlockIndex( b ) ) == typeName )
				same << b;
		selectInList( same );
	} );

	view->addSeparator();

	QAction * aFrame = view->addAction( tr( "Frame in Viewport\tNum ." ) );
	connect( aFrame, &QAction::triggered, this, [this]() { ogl->frameSelected(); } );

	/* Hide walks up from the CURRENT block to the nearest NiAVObject and hides
	 * that one node — it does not read objSelection, whatever its name suggests.
	 * So the label says "This", rather than promising the menu will act on a
	 * multi-selection it will ignore.
	 */
	QAction * aHide = view->addAction( tr( "Hide This\tH" ) );
	connect( aHide, &QAction::triggered, this, [this]() { ogl->hideSelected(); } );

	QAction * aIsolate = view->addAction( tr( "Isolate Selected\t/" ) );
	aIsolate->setToolTip( tr( "Hide everything that is not selected" ) );
	connect( aIsolate, &QAction::triggered, this, [this]() { ogl->isolateSelected(); } );

	// restoreAllVisibility, NOT unhideAll: the latter clears hiddenNodes only and
	// leaves a solo'd node and any hidden triangles in place, so "Restore All
	// Hidden" would not restore all hidden things
	QAction * aRestore = view->addAction( tr( "Restore All Hidden\tAlt+H" ) );
	connect( aRestore, &QAction::triggered, this, [this]() { ogl->restoreAllVisibility(); } );

	/* Join, with the viewport's own enable condition mirrored rather than
	 * guessed: joinSelectedObjects returns silently unless there is an active
	 * object, two or more selected, and the active one is a BSTriShape.
	 */
	const bool canJoin = ogl->objSelection.size() >= 2 && ogl->objActive >= 0
		&& nif->blockInherits( nif->getBlockIndex( ogl->objActive ), "BSTriShape" );
	view->addSeparator();
	QAction * aJoin = view->addAction( tr( "Join Selected Shapes\tCtrl+J" ) );
	aJoin->setEnabled( canJoin );
	aJoin->setToolTip( canJoin
		? tr( "Merge the selected shapes into the active one" )
		: tr( "Select two or more shapes, with a BSTriShape active" ) );
	connect( aJoin, &QAction::triggered, this, [this]() { ogl->joinSelectedObjects(); } );

	// after Transform, which is where the audit puts it and where the other
	// scene-facing group already sits
	QAction * anchor = nullptr;
	const QList<QAction *> tops = contextBook.actions();
	for ( int i = 0; i < tops.size(); i++ )
		if ( tops.at( i )->menu() && tops.at( i )->menu()->title() == Spell::tr( "Transform" ) ) {
			anchor = ( i + 1 < tops.size() ) ? tops.at( i + 1 ) : nullptr;
			break;
		}
	if ( anchor )
		contextBook.insertMenu( anchor, view );
	else
		contextBook.addMenu( view );
}

void NifSkope::buildBlockListMenuExtras( SpellBook & contextBook, const QModelIndex & idx )
{
	if ( !nif )
		return;
	buildBlockListSelectAndView( contextBook, idx );

	/* Open in <Manager> — ONE row, titled from the clicked block's type.
	 *
	 * Navigation between the Block List and the manager docks was one-directional:
	 * both the Collision and Material managers offer "select this in the Block
	 * List", and nothing went the other way. Three fixed rows would have been the
	 * obvious shape and the wrong one — two of them would always be inapplicable,
	 * and the menu is already too long.
	 *
	 * show() BEFORE select(): the collision dock's currentNifIndexChanged handler
	 * returns early while the dock is hidden, so selecting first leaves it open on
	 * the wrong row.
	 */
	if ( const int bn = nif->getBlockNumber( idx ); bn >= 0 ) {
		struct DockFor { const char * type; const char * objectName; const char * title; };
		static const DockFor dockFor[] = {
			{ "bhkSerializable",           "CollisionManagerDock", QT_TR_NOOP( "Collision Manager" ) },
			{ "bhkNiCollisionObject",      "CollisionManagerDock", QT_TR_NOOP( "Collision Manager" ) },
			{ "BSShaderProperty",          "MatTexManagerDock",    QT_TR_NOOP( "Material Manager" ) },
			{ "BSShaderTextureSet",        "MatTexManagerDock",    QT_TR_NOOP( "Material Manager" ) },
			{ "NiSkinInstance",            "RiggingManagerDock",   QT_TR_NOOP( "Rigging Manager" ) },
			{ "BSSkin::Instance",          "RiggingManagerDock",   QT_TR_NOOP( "Rigging Manager" ) },
			{ "NiControllerManager",       "TimelineDock",         QT_TR_NOOP( "Animation" ) },
			{ "NiControllerSequence",      "TimelineDock",         QT_TR_NOOP( "Animation" ) },
		};
		const QModelIndex block = nif->getBlockIndex( bn );
		for ( const DockFor & d : dockFor ) {
			if ( !nif->blockInherits( block, d.type ) )
				continue;
			const QString objectName = QLatin1String( d.objectName );
			if ( !findChild<QDockWidget *>( objectName ) )
				break;			// dock not built in this session
			QAction * aOpen = contextBook.addAction(
				tr( "Open in %1" ).arg( tr( d.title ) ) );
			aOpen->setToolTip( tr( "Show this block in the dock that edits it" ) );
			connect( aOpen, &QAction::triggered, this,
				[this, objectName, pidx = QPersistentModelIndex( idx )]() {
					QDockWidget * dock = findChild<QDockWidget *>( objectName );
					if ( !dock )
						return;
					dock->show();
					dock->raise();
					select( QModelIndex( pidx ) );
				} );
			break;				// one row, and the first match is the most specific
		}
	}

	// the separator goes in afterwards, once it is known that something
	// followed it — all three of these actions are conditional, and on a
	// non-block row with no diff reference and an empty field clipboard none
	// of them appear, leaving a rule at the bottom of the menu separating
	// nothing from nothing
	const int wasCount = contextBook.actions().size();
	const int bn = nif->getBlockNumber( idx );

	if ( bn >= 0 ) {
		// Pin, against the same store the ★ button above the list writes.
		// The pin lived entirely on that toolbar: the only menu entry for it
		// was on the OTHER widget, the Block Details tree, which pins fields.
		const bool pinned = blockListPins.contains( bn );
		QAction * aPin = contextBook.addAction( pinned
			? tr( "Unpin This Block" ) : tr( "Pin This Block" ) );
		aPin->setToolTip( tr( "Pinned blocks are listed under the ★ button above the "
			"block list, whatever the filter is showing" ) );
		connect( aPin, &QAction::triggered, this, [this, bn]() {
			if ( blockListPins.contains( bn ) )
				blockListPins.remove( bn );
			else
				blockListPins.insert( bn );
			updateBlockListNavigation( currentNifIndex() );
		} );
	}
	if ( bn >= 0 && bn != nif->diffRefBlock ) {
		QAction * aRef = contextBook.addAction( tr( "Set as Diff Reference" ) );
		aRef->setToolTip( tr( "Highlight how any block you select differs from this one" ) );
		connect( aRef, &QAction::triggered, this,
			[this, pidx = QPersistentModelIndex( idx )]() { setDiffReference( pidx ); } );
	}
	if ( nif->diffRefBlock >= 0 ) {
		QAction * aClr = contextBook.addAction(
			tr( "Clear Diff Reference (%1)" ).arg( nif->diffRefBlock ) );
		connect( aClr, &QAction::triggered, this, [this]() { clearDiffReference(); } );
	}
	// Offered until now only when you right-clicked a DIFFERING FIELD, and not
	// from the list where you choose which block to compare in the first place
	if ( nif->diffRefBlock >= 0 && !nif->diffRefValues.isEmpty() ) {
		QAction * aTakeAll = contextBook.addAction(
			tr( "Take All Reference Values (%1)" ).arg( nif->diffRefValues.size() ) );
		aTakeAll->setToolTip( tr( "Copy every differing value from the reference block onto this one" ) );
		connect( aTakeAll, &QAction::triggered, this, [this]() { wwTakeAllReferenceValues(); } );
	}
	if ( wwFieldClipboardValid() ) {
		// paste onto the block-list multi-selection; fall back to the
		// clicked block when nothing (else) is selected
		QList<qint32> blocks;
		if ( list->selectionModel() ) {
			for ( const QModelIndex & pidx : list->selectionModel()->selectedIndexes() ) {
				if ( pidx.column() != 0 )
					continue;
				QModelIndex src = ( pidx.model() == proxy ) ? proxy->mapTo( pidx ) : pidx;
				int b = nif->getBlockNumber( src );
				if ( b >= 0 && !blocks.contains( b ) )
					blocks.append( b );
			}
		}
		if ( blocks.isEmpty() && bn >= 0 )
			blocks.append( bn );
		if ( !blocks.isEmpty() ) {
			QAction * aPaste = contextBook.addAction(
				tr( "Paste %1 to %2 Block(s)" ).arg( wwFieldClipboardLabel() ).arg( blocks.size() ) );
			connect( aPaste, &QAction::triggered, this,
				[this, blocks]() { wwPasteFieldToBlocks( blocks ); } );
		}
	}
	const QList<QAction *> acts = contextBook.actions();
	if ( acts.size() > wasCount && wasCount > 0 )
		contextBook.insertSeparator( acts.at( wasCount ) );

	/* THE VERB ROW: the five things done most often, flat and first.
	 *
	 * These are the author's own frequency signal — they are where the
	 * hotkeys are — and every one of them was two levels deep, or in Delete's
	 * case not on this widget at all. Copy/Paste/Duplicate Branch are HOISTED
	 * rather than duplicated: the same QAction is moved out of the Block
	 * submenu, so it keeps its spell, its shortcut and its enabled state, and
	 * there is still exactly one of each in the menu.
	 *
	 * Built last, and inserted at the front, so the trailing separator above
	 * still lands on the index it was measured against.
	 */
	if ( bn >= 0 ) {
		QList<QAction *> verbs;
		for ( const char * spellName : { "Copy Branch", "Paste Branch", "Duplicate Branch" } ) {
			QAction * a = contextBook.actionFor( QLatin1String( spellName ) );
			if ( !a || !a->isVisible() )
				continue;		// inapplicable here; leave it where it is
			// take it out of whichever submenu currently holds it
			for ( QAction * top : contextBook.actions() )
				if ( top->menu() && top->menu()->actions().contains( a ) )
					top->menu()->removeAction( a );
			verbs << a;
		}

		QList<qint32> selBlocks;
		if ( list->selectionModel() ) {
			for ( const QModelIndex & sidx : list->selectionModel()->selectedIndexes() ) {
				if ( sidx.column() != 0 )
					continue;
				QModelIndex src = ( sidx.model() == proxy ) ? proxy->mapTo( sidx ) : sidx;
				const int b = nif->getBlockNumber( src );
				if ( b >= 0 && !selBlocks.contains( b ) )
					selBlocks.append( b );
			}
		}
		// a right-click outside the selection targets what was clicked
		if ( !selBlocks.contains( bn ) )
			selBlocks = QList<qint32>{ bn };

		/* Deleting blocks was reachable only from the viewport's Object ▸
		 * Delete or the X key — so the one widget that can build a
		 * multi-selection of blocks had no way to delete it.
		 * deleteBlocksWithConfirm closes the branch, prunes dangling parents,
		 * confirms once and pushes ONE undo step, which is what makes it
		 * worth routing here rather than casting Remove Branch per row.
		 */
		QAction * aDel = new QAction(
			tr( "Delete %n Block(s)", "", int( selBlocks.size() ) ), &contextBook );
		aDel->setToolTip( tr( "Delete the selected blocks and everything under them, "
			"in one undoable step" ) );
		const QVector<int> victims( selBlocks.cbegin(), selBlocks.cend() );
		connect( aDel, &QAction::triggered, this, [this, victims]() {
			if ( ogl )
				ogl->deleteBlocksWithConfirm( victims );
		} );
		verbs << aDel;

		// the F2 / double-click path, which had no menu entry at all — the
		// only discoverable route was a modal spell that does something
		// subtly different
		QAction * aRename = new QAction( tr( "Rename…\tF2" ), &contextBook );
		aRename->setToolTip( tr( "Rename this block, propagating the new name where it is referenced" ) );
		connect( aRename, &QAction::triggered, this,
			[this, pidx = QPersistentModelIndex( idx )]() {
				renameBlockListIndex( QModelIndex( pidx ), true );
			} );
		verbs << aRename;

		QAction * before = contextBook.actions().value( 0 );
		for ( QAction * v : verbs ) {
			if ( before )
				contextBook.insertAction( before, v );
			else
				contextBook.addAction( v );
		}
		if ( before )
			contextBook.insertSeparator( before );
	}
}

void NifSkope::contextMenu( const QPoint & pos )
{
	QModelIndex idx;
	QPoint p = pos;

	if ( sender() == tree ) {
		idx = tree->indexAt( pos );
		p = tree->mapToGlobal( pos );
	} else if ( sender() == list ) {
		idx = list->indexAt( pos );
		p = list->mapToGlobal( pos );
	} else if ( sender() == header ) {
		idx = header->indexAt( pos );
		p = header->mapToGlobal( pos );
	} else {
		return;
	}

	while ( idx.model() && idx.model()->inherits( "NifProxyModel" ) ) {
		idx = qobject_cast<const NifProxyModel *>(idx.model())->mapTo( idx );
	}

	SpellBook contextBook( nif, idx, this, SLOT( select( const QModelIndex & ) ) );

	if ( sender() == list && !ogl->editMode ) {
		// The Block List selection is synchronised to objSelection by
		// wireBlockListSelection(), so these actions behave exactly like their
		// viewport counterparts (including multi-select and active-parent rules).
		int clickedObject = nif->getBlockNumber( idx );
		while ( clickedObject >= 0
			&& !nif->blockInherits( nif->getBlockIndex( clickedObject ), "NiAVObject" ) )
			clickedObject = nif->getParent( clickedObject );
		if ( ogl->objSelection.isEmpty() && clickedObject >= 0 )
			ogl->syncObjectSelection( clickedObject );

		bool hasSceneSelection = !ogl->objSelection.isEmpty();
		QMenu * hierarchy = new QMenu( tr( "Hierarchy" ), &contextBook );
		QAction * setParent = hierarchy->addAction( tr( "Set Parent…\tCtrl+P" ),
			[this]() { ogl->showParentMenu(); } );
		setParent->setEnabled( hasSceneSelection );
		setParent->setToolTip( tr( "Parent the selected scene blocks to an NiNode" ) );
		QAction * clearParent = hierarchy->addAction( tr( "Clear Parent…\tAlt+P" ),
			[this]() { ogl->showClearParentMenu(); } );
		clearParent->setEnabled( hasSceneSelection );
		clearParent->setToolTip( tr( "Remove the selected scene blocks from their NiNode parents" ) );

		// Nest the Hierarchy submenu directly beneath the Block category rather
		// than pinning it to the top of the menu.
		QAction * blockPage = nullptr;
		for ( QAction * a : contextBook.actions() ) {
			if ( a->menu() && a->menu()->title() == tr( "Block" ) ) { blockPage = a; break; }
		}
		const auto topActs = contextBook.actions();
		QAction * anchor = nullptr;	// insert the Hierarchy menu *before* this action
		if ( blockPage ) {
			int bi = topActs.indexOf( blockPage );
			anchor = ( bi + 1 < topActs.size() ) ? topActs.at( bi + 1 ) : nullptr;
		} else {
			anchor = topActs.isEmpty() ? nullptr : topActs.first();
		}
		if ( anchor )
			contextBook.insertMenu( anchor, hierarchy );
		else
			contextBook.addMenu( hierarchy );

		/* Transfer Normals, off the Block List selection.
		 *
		 * The spell of that name asks which mesh to take from, because a spell is
		 * handed one block and nothing else. Here the selection already says it:
		 * the SECONDARIES are the source and the PRIMARY — the current, clicked
		 * row — is what gets written, which is the direction bungo asked for and
		 * the one the highlight already communicates.
		 *
		 * Several sources are combined rather than applied in turn: every mapping
		 * asks which source vertex or face is nearest, and over a set of meshes
		 * that is the union of them. Five armour pieces then behave as the one
		 * surface they visually are.
		 */
		QVector<int> selMeshes;
		int primaryMesh = -1;
		if ( list->selectionModel() ) {
			auto isGeom = [this]( int b ) {
				QModelIndex iB = nif->getBlockIndex( b );
				return nif->blockInherits( iB, "BSTriShape" )
					|| nif->isNiBlock( iB, { "NiTriShape", "BSLODTriShape" } );
			};
			const int clicked = nif->getBlockNumber( idx );
			if ( clicked >= 0 && isGeom( clicked ) )
				primaryMesh = clicked;
			for ( const QModelIndex & pidx : list->selectionModel()->selectedIndexes() ) {
				QModelIndex bi = pidx;
				while ( bi.model() && bi.model()->inherits( "NifProxyModel" ) )
					bi = qobject_cast<const NifProxyModel *>( bi.model() )->mapTo( bi );
				const int b = nif->getBlockNumber( bi );
				if ( b >= 0 && b != primaryMesh && isGeom( b ) && !selMeshes.contains( b ) )
					selMeshes.append( b );
			}
		}
		if ( primaryMesh >= 0 && !selMeshes.isEmpty() ) {
			contextBook.addSeparator();
			QAction * aTN = contextBook.addAction(
				tr( "Transfer Normals from %n Selected…", "", selMeshes.size() ) );
			aTN->setToolTip( tr( "Take normals from the other selected meshes and write them "
				"onto this one" ) );
			const int target = primaryMesh;
			connect( aTN, &QAction::triggered, this, [this, target, selMeshes]() {
				transferNormalsFromSelection( target, selMeshes );
			} );
		}
	}

	// WW: field clipboard + diff-vs-reference actions
	if ( sender() == tree && nif && idx.isValid() && idx.model() == nif ) {
		const NifItem * item = nif->getItem( idx );
		const bool leafValue = item && item->valueType() != NifValue::tNone
			&& nif->getBlockNumber( idx ) >= 0;
		const bool differing = nif->diffRefBlock >= 0 && item && nif->diffItems.contains( item );
		// pin/unpin: any field under a block, not just leaves — starring a
		// compound (a whole Bounding Sphere, say) is a legitimate thing to want
		const bool pinnable = item && !wwFieldPath( idx ).isEmpty();
		if ( leafValue || differing || pinnable )
			contextBook.addSeparator();
		if ( pinnable ) {
			const bool pinned = wwIsFieldPinned( idx );
			QAction * aPin = contextBook.addAction( pinned
				? tr( "Unpin Field" ) : tr( "Pin Field" ) );
			aPin->setToolTip( tr( "Pinned fields are starred on every block of this type, "
				"and the ★ button above the field list filters down to just them." ) );
			connect( aPin, &QAction::triggered, this,
				[this, pidx = QPersistentModelIndex( idx )]() { wwTogglePinField( pidx ); } );
		}
		if ( leafValue ) {
			QAction * aCopy = contextBook.addAction( tr( "Copy Field Value" ) );
			connect( aCopy, &QAction::triggered, this,
				[this, pidx = QPersistentModelIndex( idx )]() { wwCopyFieldValue( pidx ); } );
			if ( wwFieldClipboardValid() ) {
				QAction * aPasteRow = contextBook.addAction(
					tr( "Paste Field Value (%1)" ).arg( wwFieldClipboardLabel() ) );
				connect( aPasteRow, &QAction::triggered, this,
					[this, pidx = QPersistentModelIndex( idx )]() { wwPasteFieldToRow( pidx ); } );
			}
		}
		if ( differing ) {
			if ( nif->diffRefValues.contains( item ) ) {
				QAction * aTake = contextBook.addAction( tr( "Take Reference Value" ) );
				connect( aTake, &QAction::triggered, this,
					[this, pidx = QPersistentModelIndex( idx )]() { wwTakeReferenceValue( pidx ); } );
				QAction * aCopyRef = contextBook.addAction( tr( "Copy Reference Value" ) );
				connect( aCopyRef, &QAction::triggered, this,
					[this, pidx = QPersistentModelIndex( idx )]() { wwCopyReferenceValue( pidx ); } );
			}
			if ( !nif->diffRefValues.isEmpty() ) {
				QAction * aTakeAll = contextBook.addAction(
					tr( "Take All Reference Values (%1)" ).arg( nif->diffRefValues.size() ) );
				connect( aTakeAll, &QAction::triggered, this,
					[this]() { wwTakeAllReferenceValues(); } );
			}
		}
	}
	if ( sender() == list )
		buildBlockListMenuExtras( contextBook, idx );

	/* Search… — first row, and the palette opens AFTER exec returns.
	 *
	 * Not from inside the menu: contextBook is a stack local, and a palette
	 * opened from a menu handler would outlive the exec() that owns it. So the
	 * row does nothing but be chosen, exec returns it, and the palette runs down
	 * here with the book still alive.
	 */
	QAction * searchRow = nullptr;
	if ( sender() == list ) {
		searchRow = new QAction( tr( "Search…\tCtrl+Shift+P" ), &contextBook );
		searchRow->setToolTip( tr( "Find any spell or action by name, including the ones "
			"that do not apply here" ) );
		QAction * first = contextBook.actions().value( 0 );
		if ( first ) {
			contextBook.insertAction( first, searchRow );
			contextBook.insertSeparator( first );
		} else {
			contextBook.addAction( searchRow );
		}
	}

	// four of the actions built above carry a tooltip, and QMenu suppresses them
	// unless asked; they have been dead text since they were written
	contextBook.setToolTipsVisible( true );

	if ( !idx.isValid() || nif->flags( idx ) & (Qt::ItemIsEnabled | Qt::ItemIsSelectable) ) {
		QAction * chosen = contextBook.exec( p );
		if ( chosen && chosen == searchRow ) {
			const int bn = nif->getBlockNumber( idx );
			const QString what = bn >= 0
				? QStringLiteral( "[%1] %2" ).arg( bn ).arg( nif->itemName( nif->getBlockIndex( bn ) ) )
				: QString();
			if ( QAction * run = wwSpellPalette( this, contextBook, what ) )
				run->trigger();
		}
	}
}

void NifSkope::overrideViewFont()
{
	QSettings settings;
	QVariant var = settings.value( "UI/View Font" );

	if ( var.canConvert<QFont>() ) {
		setViewFont( var.value<QFont>() );
	}
}


/*
* Automatic Slots
* **********************
*/


void NifSkope::on_aCloseArchives_triggered()
{
	Game::GameManager::close_resources( true );
}

void NifSkope::on_aUpdateView_triggered()
{
	ogl->flush();
	ogl->updateShaders();
	emit ogl->getScene()->sceneUpdated();
	ogl->updateScene();
}

void NifSkope::on_aLoadXML_triggered()
{
	NifModel::loadXML();
	KfmModel::loadXML();
}

void NifSkope::on_aReload_triggered()
{
	if ( NifModel::loadXML() ) {
		reload();
	}
}

void NifSkope::on_aArchiveExtractor_triggered()
{
	if ( !nif )
		return;
	std::set< std::string_view >	filePaths;
	nif->listResourceFiles( filePaths );
	FileBrowserWidget	fileBrowser( 640, 600, "Browse Resources", filePaths, std::string_view(),
										&( nif->getGameResources() ), true );
	(void) fileBrowser.exec();
}

void NifSkope::on_aSelectFont_triggered()
{
	bool ok;
	QFont fnt = QFontDialog::getFont( &ok, list->font(), this );

	if ( !ok )
		return;

	setViewFont( fnt );
	QSettings settings;
	settings.setValue( "UI/View Font", fnt );
}

void NifSkope::on_aWindow_triggered()
{
	createWindow();
}

void NifSkope::on_aShredder_triggered()
{
	TestShredder::create();
}

void NifSkope::on_aHeader_triggered()
{
	if ( tree )
		tree->clearRootIndex();

	select( nif->getHeaderIndex() );
}


void NifSkope::on_tRender_actionTriggered( QAction * action )
{
	Q_UNUSED( action );
}

void NifSkope::on_aViewTop_triggered( bool checked )
{
	if ( checked ) {
		ogl->setOrientation( GLView::ViewTop );
	}
}

void NifSkope::on_aViewFront_triggered( bool checked )
{
	if ( checked ) {
		ogl->setOrientation( GLView::ViewFront );
	}
}

void NifSkope::on_aViewLeft_triggered( bool checked )
{
	if ( checked ) {
		ogl->setOrientation( GLView::ViewLeft );
	}
}

void NifSkope::on_aViewCenter_triggered()
{
	ogl->center();
}

void NifSkope::on_aViewFlip_triggered( bool checked )
{
	Q_UNUSED( checked );
	ogl->flipOrientation();
}

void NifSkope::on_aViewPerspective_toggled( bool checked )
{
	ogl->setProjection( checked );
}

void NifSkope::on_aViewWalk_triggered( bool checked )
{
	if ( checked ) {
		ogl->setOrientation( GLView::ViewWalk );
	}
}


void NifSkope::on_aViewUserSave_triggered( bool checked )
{
	Q_UNUSED( checked );
	ogl->saveUserView();
	ui->aViewUser->setChecked( true );
}


void NifSkope::on_aViewUser_toggled( bool checked )
{
	if ( checked ) {
		ogl->setOrientation( GLView::ViewUser, false );
		ogl->loadUserView();
	}
}

void NifSkope::on_aSettings_triggered()
{
	options->show();
	options->raise();
	options->activateWindow();
}

void NifSkope::on_mTheme_triggered( QAction * action )
{
	auto newTheme = WindowTheme( action->data().toInt() );

	setTheme( newTheme );
}

