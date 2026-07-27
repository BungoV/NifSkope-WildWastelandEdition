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

#include "glview.h"
#include "nifmerge.h"
#include "message.h"
#include "nifsnapshot.h"
#include "shortcutregistry.h"
#include "spellbook.h"
#include "wwskin.h"
#include "skeletontools.h"

#include <QProcessEnvironment>
#include <QScopeGuard>
#include "version.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/renderer.h"
#include "model/kfmmodel.h"
#include "model/nifmodel.h"
#include "model/nifproxymodel.h"
#include "ui/widgets/filebrowser.h"
#include "ui/widgets/fileselect.h"
#include "ui/widgets/floatslider.h"
#include "ui/widgets/floatedit.h"
#include "ui/widgets/lightingwidget.h"
#include "ui/widgets/nifview.h"
#include "ui/widgets/refrbrowser.h"
#include "ui/widgets/inspect.h"
#include "ui/widgets/timeline.h"
#include "ui/widgets/xmlcheck.h"
#include "ui/about_dialog.h"
#include "ui/settingsdialog.h"
#include "ui/settingspane.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QByteArray>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QMimeData>
#include <QDebug>
#include <QElapsedTimer>
#include <QDialog>
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
#include <QSlider>
#include <QTabWidget>
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

//! Blender-style number field: click-drag left/right scrubs the value, a plain
//! click (no drag) enters edit mode, and the ‹ › arrows shown on hover step it.
//! No Q_OBJECT (it reuses QDoubleSpinBox::valueChanged) so it stays inline.
class DragSpinBox final : public QDoubleSpinBox
{
public:
	explicit DragSpinBox( QWidget * parent = nullptr ) : QDoubleSpinBox( parent )
	{
		setButtonSymbols( QAbstractSpinBox::NoButtons );	// we draw ‹ › ourselves
		// Blender number field: dark rounded well, value roughly centered
		setAlignment( Qt::AlignCenter );
		setStyleSheet( QStringLiteral(
			"QDoubleSpinBox { background: %1; border: none; border-radius: 3px; color: %2; }"
			"QLineEdit { background: transparent; border: none; color: %2;"
			" selection-background-color: %3; selection-color: #ffffff; }" )
			.arg( wwSkinColor( "bgInput" ), wwSkinColor( "text" ), wwSkinColor( "bgBtnDown" ) ) );
		if ( QLineEdit * le = lineEdit() ) {
			// the spin box's internal line edit gets the mouse events, so drive
			// the drag from an event filter on it (a subclass override never fires)
			le->installEventFilter( this );
			le->setMouseTracking( true );
			le->setCursor( Qt::SizeHorCursor );
		}
	}
protected:
	static constexpr int arrowW = 16;
	void resizeEvent( QResizeEvent * e ) override
	{
		QDoubleSpinBox::resizeEvent( e );
		if ( QLineEdit * le = lineEdit() )	// inset so the ‹ › arrows have room
			le->setGeometry( arrowW, 0, std::max( width() - 2 * arrowW, 0 ), height() );
	}
	void enterEvent( QEnterEvent * e ) override
	{
		QDoubleSpinBox::enterEvent( e );
		m_hover = true;		// hover reveals the ‹ › arrows (Blender)
		update();
	}
	void leaveEvent( QEvent * e ) override
	{
		QDoubleSpinBox::leaveEvent( e );
		m_hover = false;
		update();
	}
	void mousePressEvent( QMouseEvent * e ) override
	{
		// clicks in the side margins (outside the inset line edit) step the value
		if ( e->button() == Qt::LeftButton ) {
			int x = int( e->position().x() );
			if ( x < arrowW ) { stepBy( -1 ); Q_EMIT editingFinished(); return; }
			if ( x > width() - arrowW ) { stepBy( 1 ); Q_EMIT editingFinished(); return; }
		}
		QDoubleSpinBox::mousePressEvent( e );
	}
	bool eventFilter( QObject * o, QEvent * ev ) override
	{
		QLineEdit * le = lineEdit();
		if ( o == le ) {
			if ( ev->type() == QEvent::MouseButtonPress ) {
				auto me = static_cast<QMouseEvent *>( ev );
				if ( me->button() == Qt::LeftButton ) {
					m_dragging = true;
					m_moved = false;
					m_pressX = int( me->globalPosition().x() );
					m_startVal = value();
					return true;	// swallow: don't let the line edit start selecting
				}
			} else if ( ev->type() == QEvent::MouseMove && m_dragging ) {
				auto me = static_cast<QMouseEvent *>( ev );
				int dx = int( me->globalPosition().x() ) - m_pressX;
				if ( !m_moved && std::abs( dx ) > 2 ) {
					m_moved = true;
					update();	// light up the field while scrubbing (Blender)
				}
				if ( m_moved ) {
					double scale = ( me->modifiers() & Qt::ShiftModifier ) ? 0.01 : 0.1;
					setValue( m_startVal + double( dx ) * singleStep() * scale );
				}
				return true;
			} else if ( ev->type() == QEvent::MouseButtonRelease && m_dragging ) {
				m_dragging = false;
				const bool finishedScrub = m_moved;
				if ( !m_moved ) {		// a plain click: edit the value
					le->selectAll();
					le->setFocus();
				}
				m_moved = false;
				update();
				if ( finishedScrub )
					Q_EMIT editingFinished();
				return true;
			}
		}
		return QDoubleSpinBox::eventFilter( o, ev );
	}
	void paintEvent( QPaintEvent * ev ) override
	{
		QDoubleSpinBox::paintEvent( ev );
		QPainter p( this );
		p.setRenderHint( QPainter::Antialiasing );
		if ( m_dragging && m_moved ) {
			// scrub highlight: brighten the well while the value is being dragged
			p.setPen( Qt::NoPen );
			p.setBrush( QColor( 255, 255, 255, 45 ) );
			p.drawRoundedRect( rect().adjusted( 0, 0, -1, -1 ), 3.0, 3.0 );
		}
		// ‹ › step arrows only appear under the pointer, and not while typing
		if ( m_hover && !( lineEdit() && lineEdit()->hasFocus() ) ) {
			p.setPen( QColor( 230, 230, 230 ) );
			p.drawText( QRect( 0, 0, arrowW, height() ), Qt::AlignCenter, QStringLiteral( "‹" ) );
			p.drawText( QRect( width() - arrowW, 0, arrowW, height() ), Qt::AlignCenter, QStringLiteral( "›" ) );
		}
	}
private:
	bool m_dragging = false, m_moved = false, m_hover = false;
	int m_pressX = 0;
	double m_startVal = 0.0;
};

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
static QString wwBoxedButtonQss( const QString & padding )
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
		skope->show();
		skope->raise();
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
					// deterministic transfer options; suppress the generic
					// not-undoable confirmation (a CheckableMessageBox the
					// driver does not know how to answer)
					QSettings cfg;
					cfg.setValue( "Settings/Suppress Undoable Confirmation", true );
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
					// fallback must slot each root into the nearest NiNode. Suppress
					// the generic not-undoable confirm the SpellBook would pop. ---
					QSettings cfg;
					cfg.setValue( "Settings/Suppress Undoable Confirmation", true );
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

					QSettings cfg;
					cfg.setValue( "Settings/Suppress Undoable Confirmation", true );
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
				auto segCoverage = [nif]( int block, int & sumPrim, bool & contiguous, int & numSeg ) {
					QModelIndex iB = nif->getBlockIndex( block );
					numSeg = nif->get<int>( iB, "Num Segments" );
					QModelIndex iSeg = nif->getIndex( iB, "Segment" );
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
				auto zeroWeights = [nif]( int block ) {
					int zero = 0;
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
					int sb = -1, sbVerts = -1;
					if ( qEnvironmentVariableIsSet( "WW_SEP_BLOCK" ) ) {
						int req = QString::fromLocal8Bit( qgetenv( "WW_SEP_BLOCK" ) ).toInt();
						if ( req >= 0 && req < nif->getBlockCount()
							&& nif->blockInherits( nif->getBlockIndex( req ), "BSSubIndexTriShape" ) )
							sb = req;
						else
							log << "WW_SEP_BLOCK " << req << " is not a BSSubIndexTriShape; using largest\n";
					}
					if ( sb < 0 )
						for ( int b = 0; b < nif->getBlockCount(); b++ )
							if ( nif->blockInherits( nif->getBlockIndex( b ), "BSSubIndexTriShape" ) ) {
								int nv = nif->get<int>( nif->getBlockIndex( b ), "Num Vertices" );
								if ( nv > sbVerts ) { sbVerts = nv; sb = b; }
							}
					if ( sb < 0 ) { log << "no BSSubIndexTriShape\n"; break; }
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

					// build the scene, enter edit mode, select the first half of faces
					skope->ogl->grabFramebuffer();
					QSet<int> objSel; objSel.insert( sb );
					skope->ogl->setObjectSelection( objSel, sb );
					skope->ogl->setEditMode( true );
					log << "editMode=" << skope->ogl->editModeActive() << "\n";
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

					int nw = -1;
					for ( int b = origBlocks; b < nif->getBlockCount(); b++ )
						if ( nif->blockInherits( nif->getBlockIndex( b ), "BSSubIndexTriShape" ) ) { nw = b; break; }
					if ( nw < 0 ) { log << "no separated shape produced\n"; break; }

					const int srcTris = nif->get<int>( nif->getBlockIndex( sb ), "Num Triangles" );
					const int newTris = nif->get<int>( nif->getBlockIndex( nw ), "Num Triangles" );
					const int srcVerts = nif->get<int>( nif->getBlockIndex( sb ), "Num Vertices" );
					const int newVerts = nif->get<int>( nif->getBlockIndex( nw ), "Num Vertices" );
					const QPair<int, int> srcSkinAfter = skinData( sb );
					const QPair<int, int> newSkin = skinData( nw );
					log << "source now: tris " << srcTris << " verts " << srcVerts
						<< " skin(inst " << srcSkinAfter.first << ", data " << srcSkinAfter.second << ")\n";
					log << "new block " << nw << ": tris " << newTris << " verts " << newVerts
						<< " skin(inst " << newSkin.first << ", data " << newSkin.second << ")\n";

					int srcSum, newSum, srcSeg, newSeg; bool srcCont, newCont;
					segCoverage( sb, srcSum, srcCont, srcSeg );
					segCoverage( nw, newSum, newCont, newSeg );
					log << "source segments Num=" << srcSeg << " sumPrim=" << srcSum
						<< " (expect " << srcTris << ") contiguous=" << srcCont << "\n";
					log << "new segments Num=" << newSeg << " sumPrim=" << newSum
						<< " (expect " << newTris << ") contiguous=" << newCont << "\n";

					const int srcZero = zeroWeights( sb ), newZero = zeroWeights( nw );
					log << "zero-weight verts src=" << srcZero << " new=" << newZero << " (expect 0/0)\n";

					bool srcGeom, srcNoOrphan, srcColor, newGeom, newNoOrphan, newColor;
					int srcVC = 0, newVC = 0;
					validateShape( sb, srcGeom, srcNoOrphan, srcColor, srcVC );
					validateShape( nw, newGeom, newNoOrphan, newColor, newVC );
					log << "source: verts " << srcVC << " (orig " << origVerts << ") geomOk " << srcGeom
						<< " noOrphan " << srcNoOrphan << " colorOk " << srcColor << "\n";
					log << "new:    verts " << newVC << " geomOk " << newGeom
						<< " noOrphan " << newNoOrphan << " colorOk " << newColor
						<< " (colors " << ( srcHasColor ? "present" : "n/a" ) << ")\n";

					const bool skinDistinct = newSkin.first >= 0 && newSkin.first != srcSkinAfter.first
						&& newSkin.second >= 0 && newSkin.second != srcSkinAfter.second
						&& srcSkinAfter.first == srcSkinBefore.first;
					const bool trisConserved = ( srcTris + newTris == origTris );
					const bool segOk = srcSum == srcTris && newSum == newTris && srcCont && newCont
						&& srcSeg > 0 && newSeg > 0;
					const bool weightsOk = srcZero == 0 && newZero == 0;
					const bool geomOk = srcGeom && newGeom && srcNoOrphan && newNoOrphan;
					const bool colorOk = srcColor && newColor;
					const bool trimmed = srcVC < origVerts && newVC < origVerts;	// orphans dropped
					const bool pass = skinDistinct && trisConserved && segOk && weightsOk
						&& geomOk && colorOk && trimmed;
					log << "skinDistinct=" << skinDistinct << " trisConserved=" << trisConserved
						<< " segOk=" << segOk << " weightsOk=" << weightsOk << " geomOk=" << geomOk
						<< " colorOk=" << colorOk << " trimmed=" << trimmed << "\n";
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
							&& uSum == origTris && uCont && uBlocks == origBlocks
							&& uGeom && uColor && uZero == 0;
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

	// TODO: Back/Forward button in Block List
	//idxForwardAction = indexStack->createRedoAction( this );
	//idxBackAction = indexStack->createUndoAction( this );

	ui->tFile->addAction( undoAction );
	ui->tFile->addAction( redoAction );

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
	addDockWidget( Qt::BottomDockWidgetArea, dTimeline );

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

	connect( timeline, &TimelineWidget::playPauseRequested, [this]() {
		// No aAnimate gate: it made the dock's play button do nothing, silently,
		// whenever View > Animations was off. aAnimPlay now enables it itself.
		ui->aAnimPlay->trigger();
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
		QDoubleSpinBox * rpVal0 = new DragSpinBox( rpBody ), * rpVal1 = new DragSpinBox( rpBody ), * rpVal2 = new DragSpinBox( rpBody );
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
		QDoubleSpinBox * opVal = new DragSpinBox( opBody );
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
	// F0.a): a typed parameter list — floats/ints as DragSpinBoxes, bools as
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
			QDoubleSpinBox * sp = new DragSpinBox( xpBody );
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
	const QColor icoColHdr( 228, 228, 232 );
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

	ui->menuShow->addAction(dList->toggleViewAction());
	ui->menuShow->addAction(dTree->toggleViewAction());
	ui->menuShow->addAction(dHeader->toggleViewAction());
	ui->menuShow->addAction(dBrowser->toggleViewAction());
	ui->menuShow->addAction(dInsp->toggleViewAction());
	ui->menuShow->addAction(dKfm->toggleViewAction());
	ui->menuShow->addAction(dRefr->toggleViewAction());
	// Manager docks are selected from the Workspaces button, not the generic
	// panel list.

	// ---- main toolbar overhaul: smaller icons, merged dropdowns, transform header ----

	// smaller icons everywhere (~75%); toolbars are fixed in place — the
	// dotted drag grips read as sliders and wasted row width, so group
	// boundaries are drawn by the thin 2px separator line instead
	for ( QToolBar * tb : { ui->tFile, ui->tRender, ui->tMode, ui->tView, ui->tLOD } ) {
		QSize is = tb->iconSize();
		tb->setIconSize( QSize( is.width() * 3 / 4, is.height() * 3 / 4 ) );
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
		const QColor iconColor( 228, 228, 232 );
		// All viewport-mode glyphs are greyscale tlMakeIcon designs so the whole
		// selector reads as one consistent family (object cube, edit triangle,
		// vertex-paint dots, weight-paint brush, segment split).
		const QIcon objectIcon = tlMakeIcon( QStringLiteral( "mode_object" ), iconColor );
		const QIcon editIcon = tlMakeIcon( QStringLiteral( "mode_edit" ), iconColor );
		const QIcon vertexPaintIcon = tlMakeIcon( QStringLiteral( "mode_vertexpaint" ), iconColor );
		const QIcon weightPaintIcon = tlMakeIcon( QStringLiteral( "mode_weightpaint" ), iconColor );
		const QIcon segmentPaintIcon = tlMakeIcon( QStringLiteral( "mode_segment" ), iconColor );
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
		QAction * poseMode = modeMenu->addAction( objectIcon, tr( "Pose Mode" ) );
		QAction * vertexPaintMode = modeMenu->addAction( vertexPaintIcon, tr( "Vertex Paint" ) );
		QAction * weightPaintMode = modeMenu->addAction( weightPaintIcon, tr( "Weight Paint" ) );
		QAction * segmentPaintMode = modeMenu->addAction( segmentPaintIcon, tr( "Segment Paint" ) );
		objectMode->setObjectName( QStringLiteral( "ViewportObjectModeAction" ) );
		editMode->setObjectName( QStringLiteral( "ViewportEditModeAction" ) );
		poseMode->setObjectName( QStringLiteral( "ViewportPoseModeAction" ) );
		vertexPaintMode->setObjectName( QStringLiteral( "ViewportVertexPaintAction" ) );
		weightPaintMode->setObjectName( QStringLiteral( "ViewportWeightPaintAction" ) );
		segmentPaintMode->setObjectName( QStringLiteral( "ViewportSegmentPaintAction" ) );
		poseMode->setToolTip( tr( "Pose the skeleton: bones are drawn and clickable; click one then G/R/S to pose it." ) );
		vertexPaintMode->setToolTip( tr( "Paint per-vertex RGB colour or alpha on the active mesh." ) );
		segmentPaintMode->setToolTip( tr( "Paint binary face membership for FO4 segments and subsegments." ) );
		for ( QAction * action : { objectMode, editMode, poseMode, vertexPaintMode, weightPaintMode, segmentPaintMode } ) {
			action->setCheckable( true );
			modeGroup->addAction( action );
		}
		modeButton->setMenu( modeMenu );

		// constant width across every mode label, so the toolbar row doesn't
		// shift when the mode changes
		{
			const QFontMetrics fm( modeButton->font() );
			int wMax = 0;
			for ( const QString & s : { tr( "Object Mode" ), tr( "Edit Mode" ), tr( "Pose Mode" ),
					tr( "Vertex Paint" ), tr( "Weight Paint" ), tr( "Segment Paint" ) } )
				wMax = std::max( wMax, fm.horizontalAdvance( s ) );
			// text + icon + paddings/border + menu indicator
			modeButton->setFixedWidth( wMax + modeButton->iconSize().width() + 46 );
		}

		auto syncModeButton = [this, modeButton, objectMode, editMode, poseMode, vertexPaintMode,
			weightPaintMode, segmentPaintMode, objectIcon, editIcon, vertexPaintIcon, weightPaintIcon, segmentPaintIcon]() {
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
			bool anyOther = paintingWeights || paintingVertices || paintingSegments || editing || posing;
			objectMode->setChecked( !anyOther );
			editMode->setChecked( !paintingWeights && !paintingVertices && !paintingSegments && !posing && editing );
			poseMode->setChecked( posing );
			vertexPaintMode->setChecked( paintingVertices );
			weightPaintMode->setChecked( paintingWeights );
			segmentPaintMode->setChecked( paintingSegments );
			modeButton->setText( paintingWeights ? QObject::tr( "Weight Paint" )
				: paintingVertices ? QObject::tr( "Vertex Paint" )
				: paintingSegments ? QObject::tr( "Segment Paint" )
				: posing ? QObject::tr( "Pose Mode" )
				: ( editing ? QObject::tr( "Edit Mode" ) : QObject::tr( "Object Mode" ) ) );
			modeButton->setIcon( paintingWeights ? weightPaintIcon
				: paintingVertices ? vertexPaintIcon : paintingSegments ? segmentPaintIcon
				: ( editing ? editIcon : objectIcon ) );
		};
		connect( objectMode, &QAction::triggered, this, [this]() {
			ogl->setRiggingWeightPaintMode( false );
			ogl->setVertexPaintMode( false );
			ogl->setSegmentPaintMode( false );
			ogl->setPoseMode( false );
			ogl->setEditMode( false );
		} );
		connect( editMode, &QAction::triggered, this, [this]() {
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
		ui->tRender->insertWidget( ui->aSelectObject, modeButton );

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
		modeBar->addSeparator();

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
		makeMenuButton( tr( "Object" ), &GLView::populateObjectMenu, &aObject );
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
		visibilityButton->setIcon( ui->aShowHidden->icon() );
		visibilityButton->setToolTip( tr( "Isolate, hide, or restore viewport geometry" ) );
		QMenu * visibilityMenu = new QMenu( visibilityButton );
		QAction * isolateSelected = visibilityMenu->addAction( tr( "Isolate Selected Objects" ) );
		QAction * isolatePrimary = visibilityMenu->addAction( tr( "Isolate Primary Object" ) );
		QAction * hideSecondary = visibilityMenu->addAction( tr( "Hide Secondary Selected Objects" ) );
		visibilityMenu->addSeparator();
		QAction * restoreAll = visibilityMenu->addAction( ui->aShowHidden->icon(), tr( "Restore All Hidden" ) );
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
		ui->tRender->insertWidget( ui->aViewTop, visibilityButton );
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
		btn->setPopupMode( QToolButton::InstantPopup );
		btn->setToolTip( tr( "Display options" ) );
		btn->setIcon( ui->aShowNodes->icon() );
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
		aShowCursor->setIcon( tlMakeIcon( QStringLiteral( "cursor3d" ), icoColHdr ) );
		mCursor->removeAction( aShowCursor );
		m->addAction( aShowCursor );
		aGizmoHandles->setIcon( tlMakeIcon( QStringLiteral( "gizmo" ), icoColHdr ) );
		m->addAction( aGizmoHandles );
		QAction * aOrigins = new QAction( tlMakeIcon( QStringLiteral( "origins" ), icoColHdr ), tr( "Show Origins" ), this );
		aOrigins->setCheckable( true );
		aOrigins->setChecked( true );
		aOrigins->setToolTip( tr( "Show origin points of selected nodes and dashed lines to their parents" ) );
		connect( aOrigins, &QAction::toggled, [this]( bool on ) {
			ogl->showOrigins = on;
			ogl->update();
		} );
		m->addAction( aOrigins );

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

		btn->setMenu( m );
		ui->tRender->insertWidget( ui->aShowCollision, btn );
		for ( QAction * a : ds )
			ui->tRender->removeAction( a );
		ui->tRender->removeAction( aSolo );
	}

	// Camera focus commands share one compact dropdown. It sits between the
	// display menu and transform controls: Center Viewpoint resets the orbit
	// pivot, while Frame Selected also fits the active selection in the view.
	{
		// Removing the legacy viewpoint actions can leave their old separator at
		// the end of the toolbar. Do not let it split Display from this related
		// focus control; the transform-group separator is added after this button.
		while ( !ui->tRender->actions().isEmpty()
			&& ui->tRender->actions().last()->isSeparator() )
			ui->tRender->removeAction( ui->tRender->actions().last() );
		ui->aViewCenter->setText( tr( "Center Viewpoint" ) );
		ui->aViewCenter->setIcon( tlMakeIcon( QStringLiteral( "view_center" ), QColor( 228, 228, 232 ) ) );
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
		ui->tRender->addWidget( focusButton );
	}

	// Blender-style transform header on the freed toolbar space
	{
		ui->tRender->addSeparator();

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
		btnMagnet->setIcon( tlMakeIcon( QStringLiteral( "magnet" ), QColor( 228, 228, 232 ) ) );
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
				QDoubleSpinBox * spGrid = new DragSpinBox( grow );
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
			QDoubleSpinBox * spRot = new DragSpinBox( rrow );
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
		ui->tRender->addSeparator();

		// Blender-style vertex / edge / face select buttons (edit mode)
		{
			QColor icoCol( 228, 228, 232 );
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
			bDeformedCage->setIcon( tlMakeIcon( QStringLiteral( "mode_deform" ), QColor( 228, 228, 232 ) ) );
			bDeformedCage->setText( tr( "Deformed" ) );
			bDeformedCage->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
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
			bWeightBrush->setIcon( tlMakeIcon( QStringLiteral( "mode_weightpaint" ), QColor( 228, 228, 232 ) ) );
			bWeightBrush->setText( tr( "Brush" ) );
			bWeightBrush->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
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

		ui->tRender->addSeparator();

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
		auto greyscaleIcon = []( const QIcon & icon ) -> QIcon {
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
		};
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
	const QList<QDockWidget *> workspaceManagers = {
		dTimeline, dMatMgr, dCollisionMgr, dRiggingMgr, dVertexPaintMgr, dUVMgr, dPoseMgr, dSkeletonMgr
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

	/* Animation transport: play/pause, sequence, loop, more, and a mini timeline.
	 *
	 * Sits at the head of the View toolbar so it is reachable without opening the
	 * Timeline dock or switching to the Animation workspace - the common case is
	 * "play this and look at it", which should not cost a layout change.
	 *
	 * It drives the EXISTING actions (aAnimPlay / aAnimLoop / aAnimSwitch) and
	 * Scene::animGroups rather than keeping its own state, so this, the Timeline
	 * dock's transport and Space in the viewport can never disagree.
	 *
	 * Always present, disabled when the file has no sequences. A control that
	 * vanishes between files is worse than one that is visibly unavailable: you
	 * cannot learn where it lives, and its absence is indistinguishable from not
	 * having found it yet.
	 */
	{
		const QColor icoCol( wwSkinColor( "text" ) );
		const QString boxQss = wwBoxedButtonQss( QStringLiteral( "3px 6px" ) )
			+ QStringLiteral( "QToolButton:disabled { color:%1; border-color:%2; }" )
				.arg( wwSkinColor( "textMuted" ), wwSkinColor( "borderDim" ) );

		auto * animPlayBtn = new QToolButton( this );
		animPlayBtn->setDefaultAction( ui->aAnimPlay );
		animPlayBtn->setToolButtonStyle( Qt::ToolButtonIconOnly );
		animPlayBtn->setAutoRaise( false );
		animPlayBtn->setStyleSheet( boxQss );
		// the glyph follows the action's checked state: showing "pause" while
		// paused would say what the button IS rather than what it DOES
		const QColor icoColOff( wwSkinColor( "textMuted" ) );
		auto syncPlayIcon = [this, animPlayBtn, icoCol, icoColOff]() {
			const bool playing = ui->aAnimPlay->isChecked();
			ui->aAnimPlay->setIcon( tlMakeIcon(
				playing ? QStringLiteral( "pause" ) : QStringLiteral( "play" ),
				ui->aAnimPlay->isEnabled() ? icoCol : icoColOff ) );
			animPlayBtn->setToolTip( playing ? tr( "Pause animation" ) : tr( "Play animation" ) );
		};
		syncPlayIcon();
		connect( ui->aAnimPlay, &QAction::toggled, this, syncPlayIcon );

		auto * animSeqBtn = new QToolButton( this );
		animSeqBtn->setPopupMode( QToolButton::InstantPopup );
		animSeqBtn->setIcon( tlMakeIcon( QStringLiteral( "sequence" ), icoCol ) );
		animSeqBtn->setToolButtonStyle( Qt::ToolButtonIconOnly );
		animSeqBtn->setAutoRaise( false );
		animSeqBtn->setStyleSheet( boxQss );
		animSeqBtn->setToolTip( tr( "Animation sequence to play" ) );
		auto * animSeqMenu = new QMenu( animSeqBtn );
		animSeqBtn->setMenu( animSeqMenu );

		auto * animLoopBtn = new QToolButton( this );
		animLoopBtn->setDefaultAction( ui->aAnimLoop );
		ui->aAnimLoop->setIcon( tlMakeIcon( QStringLiteral( "loop" ), icoCol ) );
		animLoopBtn->setToolButtonStyle( Qt::ToolButtonIconOnly );
		animLoopBtn->setAutoRaise( false );
		animLoopBtn->setStyleSheet( boxQss );

		auto * animMoreBtn = new QToolButton( this );
		animMoreBtn->setPopupMode( QToolButton::InstantPopup );
		animMoreBtn->setIcon( tlMakeIcon( QStringLiteral( "settings" ), icoCol ) );
		animMoreBtn->setToolButtonStyle( Qt::ToolButtonIconOnly );
		animMoreBtn->setAutoRaise( false );
		animMoreBtn->setStyleSheet( boxQss );
		animMoreBtn->setToolTip( tr( "More animation settings" ) );
		auto * animMoreMenu = new QMenu( animMoreBtn );
		animMoreBtn->setMenu( animMoreMenu );

		// Reverse is a sign on the playback rate, so the two share one exclusive
		// speed group and cannot contradict each other.
		animMoreMenu->addSection( tr( "Playback" ) );
		QAction * aReverse = animMoreMenu->addAction( tr( "Play Reversed" ) );
		aReverse->setCheckable( true );
		animMoreMenu->addAction( ui->aAnimSwitch );		// cycle through sequences
		animMoreMenu->addSection( tr( "Speed" ) );
		auto * speedGroup = new QActionGroup( animMoreMenu );
		speedGroup->setExclusive( true );
		const float speeds[] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f };
		for ( float sp : speeds ) {
			QAction * a = animMoreMenu->addAction( tr( "%1x" ).arg( sp ) );
			a->setCheckable( true );
			a->setChecked( sp == 1.0f );
			a->setData( sp );
			speedGroup->addAction( a );
		}
		auto applySpeed = [this, speedGroup, aReverse]() {
			float sp = 1.0f;
			if ( QAction * a = speedGroup->checkedAction() )
				sp = a->data().toFloat();
			ogl->setAnimSpeed( aReverse->isChecked() ? -sp : sp );
		};
		connect( speedGroup, &QActionGroup::triggered, this, [applySpeed]( QAction * ) { applySpeed(); } );
		connect( aReverse, &QAction::toggled, this, [applySpeed]( bool ) { applySpeed(); } );
		animMoreMenu->addSeparator();
		animMoreMenu->addAction( dTimeline->toggleViewAction() );

		// Mini timeline. Integer slider over a float range, so it carries its own
		// resolution rather than borrowing the sequence's.
		const int animTicks = 1000;
		auto * animScrub = new QSlider( Qt::Horizontal, this );
		animScrub->setRange( 0, animTicks );
		animScrub->setFixedWidth( 64 );
		animScrub->setToolTip( tr( "Scrub the current sequence" ) );
		animScrub->setFocusPolicy( Qt::NoFocus );

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

		// Sequence list and the enabled/disabled state both follow the file.
		auto refreshAnimBar = [this, animSeqMenu, animSeqBtn, animMoreBtn, animScrub,
							   icoCol, icoColOff, syncPlayIcon]() {
			Scene * sc = ogl->getScene();
			const QStringList groups = sc ? sc->animGroups : QStringList();
			/* Animatable is a TIME RANGE, not a sequence list.
			 *
			 * Plenty of files animate through standalone controllers with no named
			 * NiControllerSequence at all - every NiPSys effect in Meshes/Effects is
			 * like this, and they are exactly what someone opens to watch something
			 * move. Gating on animGroups greyed the transport out on all of them.
			 * The sequence BUTTON still needs a non-empty list, since with nothing
			 * to choose from it would open an empty menu.
			 */
			const bool have = sc && sc->timeMax() > sc->timeMin();
			const bool haveGroups = !groups.isEmpty();

			animSeqMenu->clear();
			for ( const QString & g : groups ) {
				QAction * a = animSeqMenu->addAction( g.isEmpty() ? tr( "(unnamed)" ) : g );
				a->setCheckable( true );
				a->setChecked( g == sc->animGroup );
				connect( a, &QAction::triggered, this, [this, g]() { ogl->setSceneSequence( g ); } );
			}
			QString label = sc ? sc->animGroup : QString();
			if ( !haveGroups )
				label = tr( "No named sequence" );
			else if ( label.isEmpty() )
				label = tr( "(unnamed)" );
			animSeqBtn->setToolTip( haveGroups ? tr( "Sequence: %1" ).arg( label )
											   : tr( "Animation sequence to play" ) );

			// The two action-backed buttons take their enabled state from the
			// ACTION, not the button, so disabling the widget alone left them live.
			ui->aAnimPlay->setEnabled( have );
			ui->aAnimLoop->setEnabled( have );
			animSeqBtn->setEnabled( haveGroups );
			animMoreBtn->setEnabled( have );
			animScrub->setEnabled( have );

			// regenerate the glyphs in the state's colour, then the play/pause one
			// through its own helper so the play-vs-pause choice stays in one place
			animSeqBtn->setIcon( tlMakeIcon( QStringLiteral( "sequence" ),
											 haveGroups ? icoCol : icoColOff ) );
			ui->aAnimLoop->setIcon( tlMakeIcon( QStringLiteral( "loop" ),
												have ? icoCol : icoColOff ) );
			animMoreBtn->setIcon( tlMakeIcon( QStringLiteral( "settings" ),
											  have ? icoCol : icoColOff ) );
			syncPlayIcon();
		};
		connect( ogl, &GLView::sequencesUpdated, this, refreshAnimBar );
		connect( this, &NifSkope::completeLoading, this,
			[refreshAnimBar]( bool, QString & ) { refreshAnimBar(); } );
		refreshAnimBar();

		ui->tView->addWidget( animPlayBtn );
		ui->tView->addWidget( animSeqBtn );
		ui->tView->addWidget( animLoopBtn );
		ui->tView->addWidget( animMoreBtn );
		ui->tView->addWidget( animScrub );
	}

	// Regular dock toggles collapse into one dropdown on the View toolbar.
	// Manager docks live in the adjacent Workspaces menu instead.
	{
		// thin line where this toolbar's drag grip used to sit
		ui->tView->addSeparator();
		QToolButton * btn = new QToolButton( this );
		btn->setPopupMode( QToolButton::InstantPopup );
		btn->setText( tr( "Panels" ) );
		btn->setIcon( tlMakeIcon( QStringLiteral( "panel" ), QColor( 228, 228, 232 ) ) );
		btn->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
		btn->setAutoRaise( false );
		btn->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "3px 6px" ) ) );
		btn->setToolTip( tr( "Show/hide panels" ) );
		QMenu * m = new QMenu( btn );
		for ( QDockWidget * dw : { dList, dTree, dHeader, dBrowser, dInsp, dKfm, dRefr } ) {
			m->addAction( dw->toggleViewAction() );
			ui->tView->removeAction( dw->toggleViewAction() );
		}
		btn->setMenu( m );
		ui->tView->addWidget( btn );

		QToolButton * workspaces = new QToolButton( this );
		workspaces->setPopupMode( QToolButton::InstantPopup );
		workspaces->setText( tr( "Workspaces" ) );
		workspaces->setIcon( tlMakeIcon( QStringLiteral( "workspace" ), QColor( 228, 228, 232 ) ) );
		workspaces->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
		workspaces->setAutoRaise( false );
		workspaces->setStyleSheet( btn->styleSheet() );
		workspaces->setToolTip( tr( "Switch the active task workspace" ) );
		QMenu * workspaceMenu = new QMenu( workspaces );
		QActionGroup * workspaceGroup = new QActionGroup( workspaceMenu );
		workspaceGroup->setExclusive( true );
		const QStringList workspaceNames = {
			tr( "Default" ), tr( "Animation" ), tr( "Materials" ), tr( "Collision" ),
			tr( "Rigging" ), tr( "Vertex Paint" ), tr( "UV Editing" ), tr( "Pose" ),
				tr( "Skeleton" )
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
			dTimeline, dMatMgr, dCollisionMgr, dRiggingMgr, dVertexPaintMgr, dUVMgr, dPoseMgr, dSkeletonMgr
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
		ui->tView->addWidget( workspaces );

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

	// Populate Toolbars menu with all enabled toolbars
	for ( QObject * o : children() ) {
		QToolBar * tb = qobject_cast<QToolBar *>(o);
		if ( tb && tb->objectName() != "tFile" ) {
			// Do not add tFile to the list
			ui->mToolbars->addAction( tb->toggleViewAction() );
		}
	}

	// Insert SpellBook class before Help
	ui->menubar->insertMenu( ui->menubar->actions().at( 3 ), book.get() );

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

	// Lighting Menu
	auto mLight = lightingWidget();

	// Append Menu to tRender actions
	for ( auto child : ui->tRender->findChildren<QToolButton *>() ) {

		if ( child->defaultAction() == ui->aLightMenu ) {
			setFlyout( child, mLight );
		} else {
			child->setObjectName( "btnRender" );
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

	connect ( ogl->scene, &Scene::disableSave, [this]() {
		ui->aSave->setDisabled(true);
		ui->aSaveAs->setDisabled(true);
		ui->aReload->setDisabled(true);
	} );

	// LOD Toolbar
	QToolBar * tLOD = ui->tLOD;

	//QSettings settings;
	//int lodLevel = settings.value( "GLView/LOD Level", 0 ).toInt();
	//settings.setValue( "GLView/LOD Level", lodLevel );

	QSlider * lodSlider = new QSlider( Qt::Horizontal );
	lodSlider->setFocusPolicy( Qt::StrongFocus );
	lodSlider->setTickPosition( QSlider::TicksBelow );
	lodSlider->setTickInterval( 1 );
	lodSlider->setSingleStep( 1 );
	lodSlider->setMinimum( 0 );
	lodSlider->setMaximum( 3 );
	lodSlider->setValue(0);

	tLOD->addWidget( lodSlider );
	tLOD->setEnabled( false );
	tLOD->setVisible( false );

	connect( lodSlider, &QSlider::valueChanged, ogl->getScene(), &Scene::updateLodLevel );
	connect( lodSlider, &QSlider::valueChanged, ogl, &GLView::update_GL );
	connect( nif, &NifModel::lodSliderChanged, [tLOD]( bool enabled ) { tLOD->setEnabled( enabled ); tLOD->setVisible( enabled ); } );
}

void NifSkope::initConnections()
{
	connect( nif, &NifModel::beginUpdateHeader, this, &NifSkope::enableUi );

	connect( this, &NifSkope::beginLoading, this, &NifSkope::onLoadBegin );
	connect( this, &NifSkope::beginSave, this, &NifSkope::onSaveBegin );

	connect( this, &NifSkope::completeLoading, this, &NifSkope::onLoadComplete );
	connect( this, &NifSkope::completeSave, this, &NifSkope::onSaveComplete );
}


QMenu * NifSkope::lightingWidget()
{
	QMenu * mLight = new QMenu( this );
	mLight->setObjectName( "mLight" );


	auto lightingWidget = new LightingWidget( ogl, mLight );
	lightingWidget->setActions( {ui->aLighting} );
	auto aLightingWidget = new QWidgetAction( mLight );
	aLightingWidget->setDefaultWidget( lightingWidget );

	mLight->addAction( aLightingWidget );

	connect( ui->aSaveLighting, &QAction::triggered, lightingWidget, &LightingWidget::saveSettings );

	return mLight;
}


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
	QSettings settings;
	restoreGeometry( settings.value( "Window Geometry"_uip ).toByteArray() );
	if ( isMaximized() )
		QApplication::processEvents();
	restoreState( settings.value( "Window State"_uip ).toByteArray(), 0x073 );

	aSanitize->setChecked( settings.value( "File/Auto Sanitize", true ).toBool() );

	if ( settings.value( "List Mode"_uip, "hierarchy" ).toString() == "list" )
		aList->setChecked( true );
	else
		aHierarchy->setChecked( true );

	setListMode();

	aCondition->setChecked( settings.value( "Show Non-applicable Rows"_uip, false ).toBool() );

	list->header()->restoreState( settings.value( "List Header"_uip ).toByteArray() );
	tree->header()->restoreState( settings.value( "Tree Header"_uip ).toByteArray() );
	header->header()->restoreState( settings.value( "Header Header"_uip ).toByteArray() );
	kfmtree->header()->restoreState( settings.value( "Kfmtree Header"_uip ).toByteArray() );

	// header restoreState from a pre-Reference-column layout leaves the new
	// section visible; it must start hidden everywhere (diff state shows it)
	tree->setColumnHidden( NifModel::WwRefCol, true );
	header->setColumnHidden( NifModel::WwRefCol, true );
	kfmtree->setColumnHidden( NifModel::WwRefCol, true );

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

	toolbarSize = ToolbarSize( settings.value( "Settings/Theme/Large Icons", ToolbarLarge ).toBool() );

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
	QSize size = {16, 16};
	if ( toolbarSize == ToolbarLarge )
		size = {36, 36};

	for ( QObject * o : children() ) {
		auto tb = qobject_cast<QToolBar *>(o);
		if ( tb )
			tb->setIconSize(size);
	}
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
		if ( pointerOverViewport && !keyFocusIsTextInput
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
		if ( pointerOverViewport
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
	if ( sender() == list && nif ) {
		contextBook.addSeparator();
		const int bn = nif->getBlockNumber( idx );
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
	}

	if ( !idx.isValid() || nif->flags( idx ) & (Qt::ItemIsEnabled | Qt::ItemIsSelectable) )
		contextBook.exec( p );
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
