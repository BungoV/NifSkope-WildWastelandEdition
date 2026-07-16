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
#include "ui_nifskope.h"

#include "glview.h"
#include "message.h"
#include "nifsnapshot.h"
#include "spellbook.h"
#include "version.h"
#include "gl/glscene.h"
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

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QCursor>
#include <QDebug>
#include <QElapsedTimer>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFontDialog>
#include <QFrame>
#include <QGridLayout>
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
#include <QPushButton>
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
			"QDoubleSpinBox { background: #545454; border: none; border-radius: 3px; color: #e6e6e6; }"
			"QLineEdit { background: transparent; border: none; color: #e6e6e6;"
			" selection-background-color: #4772b3; selection-color: #ffffff; }" ) );
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


QColor NifSkope::defaultsDark[6] = {
	QColor( 60, 60, 60 ),    /// nstheme::Base
	QColor( 50, 50, 50 ),    /// nstheme::BaseAlt
	Qt::white,               /// nstheme::Text
	QColor( 204, 204, 204 ), /// nstheme::Highlight
	Qt::black,               /// nstheme::HighlightText
	QColor( 255, 66, 58 )    /// nstheme::BrightText
};

QColor NifSkope::defaultsLight[6] = {
	QColor( 245, 245, 245 ), /// nstheme::Base
	QColor( 255, 255, 255 ), /// nstheme::BaseAlt
	Qt::black,               /// nstheme::Text
	QColor( 42, 130, 218 ),  /// nstheme::Highlight
	Qt::white,               /// nstheme::HighlightText
	Qt::red                  /// nstheme::BrightText
};



//! @file nifskope_ui.cpp UI logic for %NifSkope's main window.

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
					skope->select( iP );
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
							log << " [" << nif->itemName( ch ) << ( predHidden ? "|PRED!" : "" ) << "]";
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

	if ( !fname.isEmpty() ) {
		skope->loadFile( fname );
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
	for ( auto i : ui->tAnim->actions() )
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

	connect( graphicsView, &QWidget::customContextMenuRequested, this, &NifSkope::contextMenu );

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
		if ( ui->aAnimate->isChecked() )
			ui->aAnimPlay->trigger();
	} );

	// Solo / preview-only rendering of the selected node
	QAction * aSolo = new QAction( tr( "Solo Selected" ), this );
	aSolo->setCheckable( true );
	aSolo->setShortcut( QKeySequence( Qt::ALT | Qt::Key_Q ) );
	aSolo->setToolTip( tr( "Render only the selected node's subtree, hiding all other geometry (Alt+Q)" ) );
	connect( aSolo, &QAction::toggled, ogl, &GLView::setSoloMode );
	ui->tRender->addAction( aSolo );
	ui->mRender->addAction( aSolo );

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
		" background: #2f2f2f; border: 1px solid #202020; }"
		"QCheckBox { color: #cccccc; background: transparent; }"
		"QLabel { color: #cccccc; background: transparent; }"
		"QToolButton { color: #cccccc; background: transparent; border: none; }"
		"QToolButton:hover { color: #ffffff; }"
		"QPushButton { background: #545454; color: #e6e6e6; border: none; border-radius: 3px; padding: 3px 14px; }"
		"QPushButton:hover { background: #656565; }"
		"QPushButton:pressed { background: #4772b3; }"
		"QComboBox { background: #282828; color: #e6e6e6; border: none; border-radius: 3px; padding: 2px 6px; }" );

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

	// per-object edit-mode selections are remembered only until a new file loads
	connect( this, &NifSkope::completeLoading, [this]() { ogl->savedElemSelections.clear(); } );

	// A selection restored while the file was still parsing bails out of the
	// row-hiding pass (model state != Default) and a later select() of the
	// same block skips setRootIndex — re-apply once the load has completed so
	// version-mismatched rows (Skyrim fields on FO4 NIFs) never stay stranded.
	connect( this, &NifSkope::completeLoading, [this]() {
		if ( tree )
			tree->refreshRowHiding();
		if ( header )
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
	ui->mRender->addAction( aGizmoHandles );

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
	// cursor / element utilities live in the Render menu
	ui->mRender->addMenu( mCursor );

	connect( ogl, &GLView::transformCommitted, timeline, &TimelineWidget::keyNodeTransform );

	// Space in the viewport toggles animation playback
	QAction * aPlaySpace = new QAction( this );
	aPlaySpace->setShortcut( QKeySequence( Qt::Key_Space ) );
	aPlaySpace->setShortcutContext( Qt::WidgetWithChildrenShortcut );
	graphicsView->addAction( aPlaySpace );
	connect( aPlaySpace, &QAction::triggered, [this]() {
		if ( ui->aAnimate->isChecked() )
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

	// smaller icons everywhere (~75%)
	for ( QToolBar * tb : { ui->tFile, ui->tRender, ui->tAnim, ui->tView, ui->tLOD } ) {
		QSize is = tb->iconSize();
		tb->setIconSize( QSize( is.width() * 3 / 4, is.height() * 3 / 4 ) );
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
		modeButton->setMinimumWidth( 118 );
		modeButton->setAutoRaise( false );
		modeButton->setStyleSheet( QStringLiteral(
			"QToolButton { padding: 4px 10px; border: 1px solid #555; border-radius: 4px;"
			" background: #383838; color: #ddd; font-weight: 600; }"
			"QToolButton:hover { background: #4a4a4a; border-color: #777; color: white; }"
			"QToolButton:pressed, QToolButton::menu-button:pressed { background: #2b2b2b; }"
			"QToolButton::menu-indicator { subcontrol-position: right center; subcontrol-origin: padding; }" ) );
		modeButton->setToolTip( tr( "Viewport interaction mode. Tab toggles Object Mode and the last non-object mode." ) );

		QMenu * modeMenu = new QMenu( modeButton );
		QActionGroup * modeGroup = new QActionGroup( modeMenu );
		modeGroup->setExclusive( true );
		QAction * objectMode = modeMenu->addAction( objectIcon, tr( "Object Mode" ) );
		QAction * editMode = modeMenu->addAction( editIcon, tr( "Edit Mode" ) );
		QAction * vertexPaintMode = modeMenu->addAction( vertexPaintIcon, tr( "Vertex Paint" ) );
		QAction * weightPaintMode = modeMenu->addAction( weightPaintIcon, tr( "Weight Paint" ) );
		QAction * segmentPaintMode = modeMenu->addAction( segmentPaintIcon, tr( "Segment Paint" ) );
		objectMode->setObjectName( QStringLiteral( "ViewportObjectModeAction" ) );
		editMode->setObjectName( QStringLiteral( "ViewportEditModeAction" ) );
		vertexPaintMode->setObjectName( QStringLiteral( "ViewportVertexPaintAction" ) );
		weightPaintMode->setObjectName( QStringLiteral( "ViewportWeightPaintAction" ) );
		segmentPaintMode->setObjectName( QStringLiteral( "ViewportSegmentPaintAction" ) );
		vertexPaintMode->setToolTip( tr( "Paint per-vertex RGB colour or alpha on the active mesh." ) );
		segmentPaintMode->setToolTip( tr( "Paint binary face membership for FO4 segments and subsegments." ) );
		for ( QAction * action : { objectMode, editMode, vertexPaintMode, weightPaintMode, segmentPaintMode } ) {
			action->setCheckable( true );
			modeGroup->addAction( action );
		}
		modeButton->setMenu( modeMenu );

		auto syncModeButton = [this, modeButton, objectMode, editMode, vertexPaintMode,
			weightPaintMode, segmentPaintMode, objectIcon, editIcon, vertexPaintIcon, weightPaintIcon, segmentPaintIcon]() {
			bool paintingWeights = ogl->riggingWeightPaintModeActive();
			bool paintingVertices = ogl->vertexPaintModeActive();
			bool paintingSegments = ogl->segmentPaintModeActive();
			bool editing = ogl->editModeActive();
			QSignalBlocker objectBlocker( objectMode );
			QSignalBlocker editBlocker( editMode );
			QSignalBlocker vertexBlocker( vertexPaintMode );
			QSignalBlocker weightBlocker( weightPaintMode );
			QSignalBlocker segmentBlocker( segmentPaintMode );
			objectMode->setChecked( !paintingWeights && !paintingVertices && !paintingSegments && !editing );
			editMode->setChecked( !paintingWeights && !paintingVertices && !paintingSegments && editing );
			vertexPaintMode->setChecked( paintingVertices );
			weightPaintMode->setChecked( paintingWeights );
			segmentPaintMode->setChecked( paintingSegments );
			modeButton->setText( paintingWeights ? QObject::tr( "Weight Paint" )
				: paintingVertices ? QObject::tr( "Vertex Paint" )
				: paintingSegments ? QObject::tr( "Segment Paint" )
				: ( editing ? QObject::tr( "Edit Mode" ) : QObject::tr( "Object Mode" ) ) );
			modeButton->setIcon( paintingWeights ? weightPaintIcon
				: paintingVertices ? vertexPaintIcon : paintingSegments ? segmentPaintIcon
				: ( editing ? editIcon : objectIcon ) );
		};
		connect( objectMode, &QAction::triggered, this, [this]() {
			ogl->setRiggingWeightPaintMode( false );
			ogl->setVertexPaintMode( false );
			ogl->setSegmentPaintMode( false );
			ogl->setEditMode( false );
		} );
		connect( editMode, &QAction::triggered, this, [this]() {
			ogl->setRiggingWeightPaintMode( false );
			ogl->setVertexPaintMode( false );
			ogl->setSegmentPaintMode( false );
			ogl->setEditMode( true );
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
		// GLView always starts in object mode; subsequent changes arrive through
		// the mode signals (including Tab, Esc, RMB, and manager-button changes).
		syncModeButton();
		ui->tRender->insertWidget( ui->aSelectObject, modeButton );
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
		QAction * specGlossWorkflow = shadeMenu->addAction( ui->aSpecular->icon(), tr( "Specular / Gloss" ) );
		specGlossWorkflow->setCheckable( true );
		specGlossWorkflow->setChecked( true );
		specGlossWorkflow->setToolTip( tr( "Use the currently supported specular/gloss material workflow" ) );
		workflowGrp->addAction( specGlossWorkflow );
		QAction * pbrWorkflow = shadeMenu->addAction( tr( "PBR: Roughness / Metallic (Planned)" ) );
		pbrWorkflow->setCheckable( true );
		pbrWorkflow->setEnabled( false );
		pbrWorkflow->setToolTip( tr( "Reserved for the future PBR viewport renderer" ) );
		workflowGrp->addAction( pbrWorkflow );

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
			"QToolButton:hover { background: #555555; }"
			"QToolButton:checked { background: #4772b3; color: #ff9d00; }"
			"QToolButton:disabled { color: #777777; }" );
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
				groupLabel->setStyleSheet( QStringLiteral( "color: #a8a8a8; padding: 3px 2px 1px 2px;" ) );
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
			"QToolBar::separator { background: #7a7a7a; width: 2px; height: 2px; margin: 4px 6px; }" ) );
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

	// These docks occupy one manager/workspace slot. Keep the policy on
	// the docks themselves so the planned Blender-style workspace selector can
	// later activate a role instead of knowing about each concrete manager.
	const QList<QDockWidget *> workspaceManagers = {
		dTimeline, dMatMgr, dCollisionMgr, dRiggingMgr, dVertexPaintMgr, dUVMgr
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

	// Regular dock toggles collapse into one dropdown on the View toolbar.
	// Manager docks live in the adjacent Workspaces menu instead.
	{
		QToolButton * btn = new QToolButton( this );
		btn->setPopupMode( QToolButton::InstantPopup );
		btn->setText( tr( "Panels" ) );
		btn->setIcon( tlMakeIcon( QStringLiteral( "panel" ), QColor( 228, 228, 232 ) ) );
		btn->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
		btn->setMinimumWidth( 96 );
		btn->setAutoRaise( false );
		btn->setStyleSheet( QStringLiteral(
			"QToolButton { padding: 4px 10px; border: 1px solid #555; border-radius: 4px;"
			" background: #383838; color: #ddd; font-weight: 600; }"
			"QToolButton:hover { background: #4a4a4a; border-color: #777; color: white; }"
			"QToolButton:pressed, QToolButton::menu-button:pressed { background: #2b2b2b; }"
			"QToolButton::menu-indicator { subcontrol-position: right center; subcontrol-origin: padding; }" ) );
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
		workspaces->setMinimumWidth( 118 );
		workspaces->setAutoRaise( false );
		workspaces->setStyleSheet( btn->styleSheet() );
		workspaces->setToolTip( tr( "Switch the active task workspace" ) );
		QMenu * workspaceMenu = new QMenu( workspaces );
		QActionGroup * workspaceGroup = new QActionGroup( workspaceMenu );
		workspaceGroup->setExclusive( true );
		const QStringList workspaceNames = {
			tr( "Default" ), tr( "Animation" ), tr( "Materials" ), tr( "Collision" ),
			tr( "Rigging" ), tr( "Vertex Paint" ), tr( "UV Editing" )
		};
		QList<QAction *> workspaceActions;
		for ( const QString & name : workspaceNames ) {
			QAction * action = workspaceMenu->addAction( name );
			action->setCheckable( true );
			workspaceGroup->addAction( action );
			workspaceActions.append( action );
		}
		workspaceMenu->addSeparator();
		struct PlannedWorkspace {
			QString name;
			QString description;
		};
		const QList<PlannedWorkspace> plannedWorkspaces = {
			{ tr( "Skeleton Manager (Planned)" ),
				tr( "Future skeleton hierarchy, rest-pose, bone transform, and validation workspace" ) },
			{ tr( "Pose Manager (Planned)" ),
				tr( "Future character posing, prop staging, reusable pose, and load-screen composition workspace" ) }
		};
		for ( const PlannedWorkspace & planned : plannedWorkspaces ) {
			QAction * action = workspaceMenu->addAction( planned.name );
			action->setEnabled( false );
			action->setToolTip( planned.description );
		}
		const QList<QDockWidget *> managers = {
			dTimeline, dMatMgr, dCollisionMgr, dRiggingMgr, dVertexPaintMgr, dUVMgr
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
		if ( weightPaintMode )
			connect( weightPaintMode, &QAction::triggered, this,
				[this, activateWorkspace, dRiggingMgr, weightPaintMode, objectMode, editMode]() {
				activateWorkspace( 4 );
				QTreeWidget * bones = dRiggingMgr->findChild<QTreeWidget *>( QStringLiteral( "RiggingBoneTree" ) );
				if ( bones && !bones->currentItem() && bones->topLevelItemCount() > 0 )
					bones->setCurrentItem( bones->topLevelItem( 0 ) );
				QPushButton * paint = dRiggingMgr->findChild<QPushButton *>( QStringLiteral( "RiggingWeightPaintButton" ) );
				if ( paint && paint->isEnabled() && !paint->isChecked() )
					paint->click();
				if ( !ogl->riggingWeightPaintModeActive() ) {
					weightPaintMode->setChecked( false );
					if ( ogl->editModeActive() && editMode ) editMode->setChecked( true );
					else if ( objectMode ) objectMode->setChecked( true );
				}
				} );
		if ( vertexPaintMode )
			connect( vertexPaintMode, &QAction::triggered, this,
				[this, activateWorkspace, dVertexPaintMgr, vertexPaintMode, objectMode, editMode]() {
				activateWorkspace( 5 );
				QPushButton * paint = dVertexPaintMgr->findChild<QPushButton *>( QStringLiteral( "VertexPaintButton" ) );
				if ( paint && paint->isEnabled() && !paint->isChecked() )
					paint->click();
				if ( !ogl->vertexPaintModeActive() ) {
					vertexPaintMode->setChecked( false );
					if ( ogl->editModeActive() && editMode ) editMode->setChecked( true );
					else if ( objectMode ) objectMode->setChecked( true );
				}
				} );
		if ( segmentPaintMode )
			connect( segmentPaintMode, &QAction::triggered, this,
				[this, activateWorkspace, dRiggingMgr, segmentPaintMode, objectMode, editMode]() {
				activateWorkspace( 4 );
				QPushButton * paint = dRiggingMgr->findChild<QPushButton *>( QStringLiteral( "RiggingSegmentPaintButton" ) );
				if ( paint && paint->isEnabled() && !paint->isChecked() ) paint->click();
				if ( !ogl->segmentPaintModeActive() ) {
					segmentPaintMode->setChecked( false );
					if ( ogl->editModeActive() && editMode ) editMode->setChecked( true );
					else if ( objectMode ) objectMode->setChecked( true );
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


	// Animate
	connect( ui->aAnimate, &QAction::toggled, ui->tAnim, &QToolBar::setVisible );
	connect( ui->tAnim, &QToolBar::visibilityChanged, ui->aAnimate, &QAction::setChecked );

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
	connect( ui->aAnimPlay, &QAction::triggered, ogl, &GLView::updateAnimationState );
	connect( ui->aAnimLoop, &QAction::toggled, ogl, &GLView::updateAnimationState );
	connect( ui->aAnimSwitch, &QAction::toggled, ogl, &GLView::updateAnimationState );

	// Animation timeline slider
	auto animSlider = new FloatSlider( Qt::Horizontal, true, true );
	auto animSliderEdit = new FloatEdit( ui->tAnim );

	animSlider->addEditor( animSliderEdit );
	animSlider->setParent( ui->tAnim );
	animSlider->setMinimumWidth( 200 );
	animSlider->setMaximumWidth( 500 );
	animSlider->setSizePolicy( QSizePolicy::Minimum, QSizePolicy::MinimumExpanding );

	connect( ogl, &GLView::sceneTimeChanged, animSlider, &FloatSlider::set );
	connect( ogl, &GLView::sceneTimeChanged, animSliderEdit, &FloatEdit::set );
	connect( animSlider, &FloatSlider::valueChanged, ogl, &GLView::setSceneTime );
	connect( animSlider, &FloatSlider::valueChanged, animSliderEdit, &FloatEdit::setValue );
	connect( animSliderEdit, static_cast<void (FloatEdit::*)(float)>(&FloatEdit::sigEdited), ogl, &GLView::setSceneTime );
	connect( animSliderEdit, static_cast<void (FloatEdit::*)(float)>(&FloatEdit::sigEdited), animSlider, &FloatSlider::setValue );

	// Animations
	animGroups = new QComboBox( ui->tAnim );
	animGroups->setMinimumWidth( 60 );
	animGroups->setSizeAdjustPolicy( QComboBox::AdjustToContents );
	animGroups->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Minimum );
	connect( animGroups, &QComboBox::textActivated, ogl, &GLView::setSceneSequence );

	ui->tAnim->addWidget( animSlider );
	animGroupsAction = ui->tAnim->addWidget( animGroups );

	connect( ogl, &GLView::sequencesDisabled, ui->tAnim, &QToolBar::hide );
	connect( ogl, &GLView::sequenceStopped, ui->aAnimPlay, &QAction::toggle );
	connect( ogl, &GLView::sequenceChanged, [this]( const QString & seqname ) {
		animGroups->setCurrentIndex( ogl->getScene()->animGroups.indexOf( seqname ) );
	} );
	connect( ogl, &GLView::sequencesUpdated, [this]() {
		ui->tAnim->show();

		animGroups->clear();
		animGroups->addItems( ogl->getScene()->animGroups );
		animGroups->setCurrentIndex( ogl->getScene()->animGroups.indexOf( ogl->getScene()->animGroup ) );

		if ( animGroups->count() == 0 ) {
			animGroupsAction->setVisible( false );
			ui->aAnimSwitch->setVisible( false );
		} else {
			ui->aAnimSwitch->setVisible( animGroups->count() != 1 );
			animGroupsAction->setVisible( true );
			animGroups->adjustSize();
		}
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
	ui->tAnim->setEnabled( false );

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
	ui->tAnim->setEnabled( true );
	animGroups->clear();


	ui->tRender->setEnabled( true );

	// We only need to enable the UI once, disconnect
	disconnect( nif, &NifModel::beginUpdateHeader, this, &NifSkope::enableUi );
}

void NifSkope::saveUi() const
{
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
	QSize size = {18, 18};
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
		// keep the floating redo panels glued to the viewport corner
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
		const bool pointerOverViewport = ogl && graphicsView && isActiveWindow()
			&& graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) );
		QWidget * keyFocus = QApplication::focusWidget();
		const bool keyFocusIsTextInput = keyFocus && ( keyFocus->inherits( "QLineEdit" )
			|| keyFocus->inherits( "QTextEdit" ) || keyFocus->inherits( "QPlainTextEdit" )
			|| keyFocus->inherits( "QAbstractSpinBox" ) || keyFocus->inherits( "QComboBox" ) );
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
			&& ke->key() == Qt::Key_Tab && ke->modifiers() == Qt::NoModifier ) {
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
			&& !keyFocusIsTextInput && ke->key() == Qt::Key_X
			&& ke->modifiers() == Qt::ControlModifier ) {
			if ( e->type() == QEvent::KeyPress && !ke->isAutoRepeat() )
				ogl->fillRiggingWeightSelection();
			e->accept();
			return true;
		}
		if ( pointerOverViewport && ogl->segmentPaintModeActive()
			&& !keyFocusIsTextInput && ke->key() == Qt::Key_X
			&& ke->modifiers() == Qt::ControlModifier ) {
			if ( e->type() == QEvent::KeyPress && !ke->isAutoRepeat() )
				ogl->fillSegmentPaintSelection();
			e->accept();
			return true;
		}
		// Alt+H (unhide, Blender) would otherwise be eaten by the Help menu
		// mnemonic before the viewport ever sees the key
		if ( ogl && graphicsView && ke->key() == Qt::Key_H
			&& ( ke->modifiers() & Qt::AltModifier )
			&& !( ke->modifiers() & ( Qt::ControlModifier | Qt::ShiftModifier ) )
			&& isActiveWindow()
			&& graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) ) ) {
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
		if ( ogl && graphicsView && !ogl->editMode && ke->key() == Qt::Key_P
			&& isActiveWindow()
			&& ( graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) )
				|| ( list && list->rect().contains( list->mapFromGlobal( QCursor::pos() ) ) ) ) ) {
			Qt::KeyboardModifiers mods = ke->modifiers();
			bool setParent = ( mods & Qt::ControlModifier ) && !( mods & ( Qt::AltModifier | Qt::ShiftModifier ) );
			bool clearParent = ( mods & Qt::AltModifier ) && !( mods & ( Qt::ControlModifier | Qt::ShiftModifier ) );
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
		if ( ogl && graphicsView && ogl->editMode && !ogl->riggingWeightPaintModeActive() && !ogl->vertexPaintModeActive()
			&& ke->key() == Qt::Key_P
			&& !( ke->modifiers() & ( Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier ) )
			&& isActiveWindow()
			&& graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) ) ) {
			if ( e->type() == QEvent::KeyPress )
				ogl->showSeparateMenu();
			e->accept();
			return true;
		}
		// M opens the edit-mode Merge menu (Blender) with the pointer over the view
		if ( ogl && graphicsView && ogl->editMode && !ogl->riggingWeightPaintModeActive() && !ogl->vertexPaintModeActive()
			&& ke->key() == Qt::Key_M
			&& !( ke->modifiers() & ( Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier ) )
			&& isActiveWindow()
			&& graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) ) ) {
			if ( e->type() == QEvent::KeyPress )
				ogl->showMergeMenu();
			e->accept();
			return true;
		}
		// E extrudes the selection (Blender) with the pointer over the view;
		// in free-camera / walk mode E stays camera-up
		if ( ogl && graphicsView && ogl->editMode && !ogl->freeCamera
			&& !ogl->riggingWeightPaintModeActive() && !ogl->vertexPaintModeActive()
			&& ke->key() == Qt::Key_E
			&& !( ke->modifiers() & ( Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier ) )
			&& isActiveWindow()
			&& graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) ) ) {
			if ( e->type() == QEvent::KeyPress && !ke->isAutoRepeat() )
				ogl->extrudeRegion();
			e->accept();
			return true;
		}
		// F fills a hole / bridges two rims (Blender) in edit mode with the
		// pointer over the view; F stays Front View everywhere else
		if ( ogl && graphicsView && ogl->editMode && !ogl->freeCamera
			&& !ogl->riggingWeightPaintModeActive() && !ogl->vertexPaintModeActive()
			&& ke->key() == Qt::Key_F
			&& !( ke->modifiers() & ( Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier ) )
			&& isActiveWindow()
			&& graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) ) ) {
			if ( e->type() == QEvent::KeyPress && !ke->isAutoRepeat() )
				ogl->smartConnect();
			e->accept();
			return true;
		}
		// the Blender modeling operators (edit mode, pointer over the view):
		// I inset, Ctrl+R loop cut, Shift+V edge slide, Ctrl+X dissolve
		if ( ogl && graphicsView && ogl->editMode && !ogl->freeCamera
			&& !ogl->riggingWeightPaintModeActive() && !ogl->vertexPaintModeActive()
			&& !ogl->segmentPaintModeActive()
			&& isActiveWindow()
			&& graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) ) ) {
			const auto mods = ke->modifiers() & ( Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier );
			int op = 0;
			if ( ke->key() == Qt::Key_I && mods == Qt::KeyboardModifiers() )
				op = 1;
			else if ( ke->key() == Qt::Key_R && mods == Qt::ControlModifier )
				op = 2;
			else if ( ke->key() == Qt::Key_V && mods == Qt::ShiftModifier )
				op = 3;
			else if ( ke->key() == Qt::Key_X && mods == Qt::ControlModifier )
				op = 4;
			if ( op ) {
				if ( e->type() == QEvent::KeyPress && !ke->isAutoRepeat() ) {
					if ( op == 1 )
						ogl->insetRegion();
					else if ( op == 2 )
						ogl->loopCut();
					else if ( op == 3 )
						ogl->edgeSlide();
					else
						ogl->dissolveVerts();
				}
				e->accept();
				return true;
			}
		}
		// Shift+A adds a primitive (object mode, pointer over the view)
		if ( ogl && graphicsView && !ogl->editMode && !ogl->freeCamera
			&& ke->key() == Qt::Key_A
			&& ( ke->modifiers() & Qt::ShiftModifier )
			&& !( ke->modifiers() & ( Qt::ControlModifier | Qt::AltModifier ) )
			&& isActiveWindow()
			&& graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) ) ) {
			if ( e->type() == QEvent::KeyPress && !ke->isAutoRepeat() )
				ogl->showAddPrimitiveMenu();
			e->accept();
			return true;
		}
		if ( ogl && graphicsView
			&& ke->key() == Qt::Key_F && ( ke->modifiers() & Qt::ShiftModifier )
			&& !( ke->modifiers() & ( Qt::ControlModifier | Qt::AltModifier ) )
			&& isActiveWindow()
			&& graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) ) ) {
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
		// B arms Blender box select (edit or object mode) with the pointer over
		// the viewport, whatever widget has key focus
		if ( ogl && graphicsView && ke->key() == Qt::Key_B
			&& !( ke->modifiers() & ( Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier ) )
			&& isActiveWindow()
			&& graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) ) ) {
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
		// Ctrl+I inverts the selection (edit: elements, object: objects)
		if ( ogl && graphicsView && ke->key() == Qt::Key_I
			&& ( ke->modifiers() & Qt::ControlModifier )
			&& !( ke->modifiers() & ( Qt::AltModifier | Qt::ShiftModifier ) )
			&& isActiveWindow()
			&& graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) ) ) {
			if ( e->type() == QEvent::KeyPress )
				ogl->invertSelection();
			e->accept();
			return true;
		}
		// C arms the circle-select brush (edit or object mode) with the pointer
		// over the viewport, whatever widget has key focus
		if ( ogl && graphicsView && ke->key() == Qt::Key_C
			&& !( ke->modifiers() & ( Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier ) )
			&& isActiveWindow()
			&& graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) ) ) {
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
		// Ctrl+= / Ctrl+- grow / shrink the edit-mode selection (Select More/Less)
		if ( ogl && graphicsView && ogl->editMode
			&& ( ke->key() == Qt::Key_Plus || ke->key() == Qt::Key_Equal || ke->key() == Qt::Key_Minus )
			&& ( ke->modifiers() & Qt::ControlModifier )
			&& !( ke->modifiers() & Qt::AltModifier )
			&& isActiveWindow()
			&& graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) ) ) {
			if ( e->type() == QEvent::KeyPress )
				ogl->selectMoreLess( ke->key() != Qt::Key_Minus );
			e->accept();
			return true;
		}
		// Numpad-. (or plain .) frames the selection with the pointer over the view
		if ( ogl && graphicsView && ke->key() == Qt::Key_Period
			&& !( ke->modifiers() & ( Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier ) )
			&& isActiveWindow()
			&& graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) ) ) {
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
		if ( ogl && graphicsView && !ogl->freeCamera && !ogl->riggingWeightPaintModeActive() && !ogl->vertexPaintModeActive()
			&& e->type() == QEvent::KeyPress
			&& ( ke->key() == Qt::Key_G || ke->key() == Qt::Key_R || ke->key() == Qt::Key_S )
			&& !( ke->modifiers() & ( Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier ) )
			&& isActiveWindow()
			&& graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) ) ) {
			QWidget * fw = QApplication::focusWidget();
			bool textInput = fw && ( fw->inherits( "QLineEdit" ) || fw->inherits( "QTextEdit" )
				|| fw->inherits( "QPlainTextEdit" ) || fw->inherits( "QAbstractSpinBox" )
				|| fw->inherits( "QComboBox" ) );
			if ( !textInput ) {
				int mode = ( ke->key() == Qt::Key_G ) ? 1 : ( ke->key() == Qt::Key_R ? 2 : 3 );
				if ( ogl->startModalTransform( mode ) ) {
					e->accept();
					return true;
				}
			}
		}
		// A / Alt+A = select all / deselect all when the pointer is over the
		// viewport, routed by pointer position (like G/R/S) rather than keyboard
		// focus — focus-follows-mouse alone is unreliable when focus is taken
		// while the pointer is already inside the viewport, so A "sometimes"
		// missed. This makes it work every time the cursor is over the viewport.
		if ( ogl && graphicsView && !ogl->freeCamera && e->type() == QEvent::KeyPress
			&& ke->key() == Qt::Key_A && !ke->isAutoRepeat()
			&& !( ke->modifiers() & ( Qt::ControlModifier | Qt::ShiftModifier ) )
			&& isActiveWindow()
			&& graphicsView->rect().contains( graphicsView->mapFromGlobal( QCursor::pos() ) ) ) {
			QWidget * fw = QApplication::focusWidget();
			bool textInput = fw && ( fw->inherits( "QLineEdit" ) || fw->inherits( "QTextEdit" )
				|| fw->inherits( "QPlainTextEdit" ) || fw->inherits( "QAbstractSpinBox" )
				|| fw->inherits( "QComboBox" ) );
			if ( !textInput && nif ) {
				ogl->selectAll( ( ke->modifiers() & Qt::AltModifier ) ? 2 : 0 );
				e->accept();
				return true;
			}
		}
		break;
	}
	case QEvent::MouseButtonPress:
		// Global mouse press
		if ( o->isWindowType() ) {
			//qDebug() << "Mouse Press";
		}
		break;

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
	} else if ( sender() == graphicsView ) {
		idx = ogl->indexAt( pos, ogl->contextMenuShiftModifier );
		p = graphicsView->mapToGlobal( pos );
	} else {
		return;
	}

	while ( idx.model() && idx.model()->inherits( "NifProxyModel" ) ) {
		idx = qobject_cast<const NifProxyModel *>(idx.model())->mapTo( idx );
	}

	SpellBook contextBook( nif, idx, this, SLOT( select( const QModelIndex & ) ) );

	if ( sender() == graphicsView ) {
		// viewport right-click: Blender-style menu with the transform actions
		// on top and the block spells as a submenu
		QPoint clickPos = pos;
		const bool weightPaint = ogl->riggingWeightPaintModeActive();
		const bool vertexPaint = ogl->vertexPaintModeActive();
		const bool segmentPaint = ogl->segmentPaintModeActive();
		const bool anyPaint = weightPaint || vertexPaint || segmentPaint;
		auto startModal = [this]( int m ) {
			if ( ogl->editMode && !ogl->pickedElems.isEmpty() && ogl->gizmoBeginElement( m ) )
				return;
			ogl->gizmoBegin( m );
		};

		QMenu menu( this );
		menu.addAction( tr( "Place Gizmo (3D Cursor) Here" ), [this, clickPos]() {
			ogl->placeCursor( QPointF( clickPos ) );
		} );
		if ( !anyPaint ) {
			menu.addSeparator();
			menu.addAction( tr( "Move\tG" ), [startModal]() { startModal( 1 ); } );
			menu.addAction( tr( "Rotate\tR" ), [startModal]() { startModal( 2 ); } );
			menu.addAction( tr( "Scale\tS" ), [startModal]() { startModal( 3 ); } );
		}
		// grow across faces within the sharpness angle; the redo panel that pops
		// up afterwards lets you readjust the angle live
		auto linkedByAngle = [this]() {
			ogl->selectLinked( true, ( ogl->lastOpKind == 2 ) ? ogl->lastOpParam : 30.0f );
		};
		if ( ogl->editMode ) {
			bool hasSel = !ogl->pickedElems.isEmpty();
			menu.addSeparator();
			menu.addAction( tr( "Select All\tA" ), [this]() { ogl->selectAll( 1 ); } );
			menu.addAction( tr( "Deselect All" ), [this]() { ogl->selectAll( 2 ); } );
			menu.addAction( tr( "Invert Selection\tCtrl+I" ), [this]() { ogl->invertSelection(); } );
			menu.addAction( tr( "Box Select\tB" ), [this]() { ogl->beginBoxSelect(); } );
			menu.addAction( tr( "Circle Select\tC" ), [this]() { ogl->beginCircleSelect(); } );
			menu.addAction( tr( "Select More\tCtrl+=" ), [this]() { ogl->selectMoreLess( true ); } )->setEnabled( hasSel );
			menu.addAction( tr( "Select Less\tCtrl+-" ), [this]() { ogl->selectMoreLess( false ); } )->setEnabled( hasSel );
			menu.addAction( tr( "Select Linked\tCtrl+L" ), [this]() { ogl->selectLinked( false ); } )->setEnabled( hasSel );
			menu.addAction( tr( "Select Linked by Angle…\tCtrl+Alt+Shift+F" ), linkedByAngle )->setEnabled( hasSel );
			if ( weightPaint || segmentPaint ) {
				menu.addSeparator();
				menu.addAction( weightPaint ? tr( "Fill Selected Weight\tCtrl+X" )
					: tr( "Apply Segment Brush to Selection\tCtrl+X" ), [this, weightPaint]() {
					if ( weightPaint ) ogl->fillRiggingWeightSelection();
					else ogl->fillSegmentPaintSelection();
				} )->setEnabled( hasSel );
				menu.addSeparator();
				menu.addAction( tr( "Hide Selection\tH" ), [this]() { ogl->hideSelectedElements(); } )->setEnabled( hasSel );
				menu.addAction( tr( "Unhide All\tAlt+H" ), [this]() { ogl->unhideAllElements(); } );
			} else {
				menu.addSeparator();
				menu.addAction( tr( "Extrude Region…\tE" ), [this]() { ogl->extrudeRegion(); } )->setEnabled( hasSel );
			menu.addAction( tr( "Fill / Bridge…\tF" ), [this]() { ogl->smartConnect(); } )->setEnabled( hasSel );
			menu.addAction( tr( "Inset Faces…\tI" ), [this]() { ogl->insetRegion(); } )->setEnabled( hasSel );
			menu.addAction( tr( "Loop Cut…\tCtrl+R" ), [this]() { ogl->loopCut(); } );
			menu.addAction( tr( "Subdivide" ), [this]() { ogl->subdivideSelection(); } )->setEnabled( hasSel );
			menu.addAction( tr( "Edge Slide…\tShift+V" ), [this]() { ogl->edgeSlide(); } )->setEnabled( hasSel );
			menu.addAction( tr( "Smooth Vertices…" ), [this]() { ogl->smoothVertices(); } )->setEnabled( hasSel );
			menu.addAction( tr( "Dissolve Vertices\tCtrl+X" ), [this]() { ogl->dissolveVerts(); } )->setEnabled( hasSel );
			menu.addSeparator();
			menu.addAction( tr( "Flip Normals" ), [this]() { ogl->flipSelectedFaces(); } )->setEnabled( hasSel );
			menu.addAction( tr( "Recalculate Normals" ), [this]() { ogl->recalcSelectedNormals(); } )->setEnabled( hasSel );
			menu.addAction( tr( "Symmetrize…" ), [this]() { ogl->symmetrizeShape(); } );
			menu.addSeparator();
			menu.addAction( tr( "Duplicate\tShift+D" ), [this]() { ogl->duplicateElements(); } )->setEnabled( hasSel );
			QMenu * meshTools = nullptr;
			for ( QAction * action : contextBook.actions() ) {
				if ( action->menu() && action->menu()->title() == QLatin1String( "Mesh" ) ) {
					meshTools = action->menu();
					break;
				}
			}
			if ( !meshTools ) {
				meshTools = new QMenu( tr( "Mesh" ), &contextBook );
				contextBook.addMenu( meshTools );
			}
			if ( !meshTools->actions().isEmpty() )
				meshTools->addSeparator();
			QAction * floatingDecal = meshTools->addAction( tr( "Create Floating Decal…" ), [this]() { ogl->createFloatingDecal(); } );
			floatingDecal->setEnabled( hasSel );
			floatingDecal->setToolTip( tr( "Copy selected faces to a separate shape and offset them along their normals" ) );
			meshTools->setEnabled( true );
			menu.addAction( tr( "Merge…\tM" ), [this]() { ogl->showMergeMenu(); } )->setEnabled( hasSel );
			menu.addAction( tr( "Separate…\tP" ), [this]() { ogl->showSeparateMenu(); } )->setEnabled( hasSel );
			menu.addSeparator();
			menu.addAction( tr( "Snap…\tShift+S" ), [this]() { ogl->showSnapMenu(); } );
			menu.addAction( tr( "Set Origin…\tShift+Ctrl+Alt+C" ), [this]() { ogl->showSetOriginMenu(); } );
			QMenu * mCur = menu.addMenu( tr( "3D Cursor" ) );
			mCur->addAction( tr( "Snap Cursor to Picked" ), [this]() {
				if ( !ogl->pickedElems.isEmpty() ) {
					ogl->cursorPos = ogl->pickedMedian();
					ogl->update();
				}
			} )->setEnabled( hasSel );
			mCur->addAction( tr( "Snap Cursor to World Origin" ), [this]() {
				ogl->cursorPos = Vector3();
				ogl->update();
			} );
			mCur->addAction( tr( "Move Picked Vertices to Cursor" ), [this]() { ogl->movePickedVertsToCursor(); } )->setEnabled( hasSel );
			menu.addSeparator();
			menu.addAction( tr( "Hide Selection\tH" ), [this]() { ogl->hideSelectedElements(); } )->setEnabled( hasSel );
			menu.addAction( tr( "Unhide All\tAlt+H" ), [this]() { ogl->unhideAllElements(); } );
			menu.addSeparator();
			menu.addAction( tr( "Delete…\tX" ), [this]() { ogl->showDeleteMenu(); } )->setEnabled( hasSel );
			}
		} else {
			// object mode: the transforms above plus the object-level operations
			bool hasSel = !ogl->objSelection.isEmpty();
			menu.addSeparator();
			menu.addAction( tr( "Select All\tA" ), [this]() { ogl->selectAll( 1 ); } );
			menu.addAction( tr( "Deselect All" ), [this]() { ogl->selectAll( 2 ); } );
			menu.addAction( tr( "Invert Selection\tCtrl+I" ), [this]() { ogl->invertSelection(); } );
			menu.addAction( tr( "Box Select\tB" ), [this]() { ogl->beginBoxSelect(); } );
			menu.addAction( tr( "Circle Select\tC" ), [this]() { ogl->beginCircleSelect(); } );
			menu.addSeparator();
			menu.addAction( tr( "Add Primitive…\tShift+A" ), [this]() { ogl->showAddPrimitiveMenu(); } );
			menu.addAction( tr( "Duplicate\tShift+D" ), [this]() { ogl->duplicateSelection(); } )->setEnabled( hasSel );
			menu.addAction( tr( "Join\tCtrl+J" ), [this]() { ogl->joinSelectedObjects(); } )->setEnabled( ogl->objSelection.size() >= 2 );
			QMenu * parentMenu = menu.addMenu( tr( "Parent" ) );
			parentMenu->addAction( tr( "Set Parent...\tCtrl+P" ), [this]() { ogl->showParentMenu(); } )->setEnabled( hasSel );
			parentMenu->addAction( tr( "Clear Parent...\tAlt+P" ), [this]() { ogl->showClearParentMenu(); } )->setEnabled( hasSel );
			menu.addSeparator();
			menu.addAction( tr( "Snap…\tShift+S" ), [this]() { ogl->showSnapMenu(); } );
			menu.addAction( tr( "Set Origin…\tShift+Ctrl+Alt+C" ), [this]() { ogl->showSetOriginMenu(); } );
			QMenu * mCur = menu.addMenu( tr( "3D Cursor" ) );
			mCur->addAction( tr( "Snap Cursor to World Origin" ), [this]() {
				ogl->cursorPos = Vector3();
				ogl->update();
			} );
			mCur->addAction( tr( "Snap Node to Cursor" ), [this]() { ogl->snapNodeToCursor(); } )->setEnabled( hasSel );
			menu.addSeparator();
			menu.addAction( tr( "Hide\tH" ), [this]() { ogl->hideSelected(); } )->setEnabled( hasSel );
			menu.addAction( tr( "Unhide All\tAlt+H" ), [this]() { ogl->unhideAll(); } );
		}
		if ( !weightPaint && ( idx.isValid() || ogl->editMode ) ) {
			// Append the regular spell category submenus (Mesh, Havok, etc.).
			menu.addSeparator();
			const auto spellActs = contextBook.actions();
			for ( QAction * a : spellActs )
				menu.addAction( a );
		}
		menu.exec( p );
		return;
	}

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
