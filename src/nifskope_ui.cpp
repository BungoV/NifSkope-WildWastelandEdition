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
#include <QDebug>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontDialog>
#include <QFrame>
#include <QGridLayout>
#include <QInputDialog>
#include <QGroupBox>
#include <QLabel>
#include <QHeaderView>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QProgressBar>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSettings>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QWidgetAction>

#include <QProcess>
#include <QStyleFactory>
#include <QRegularExpression>

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

NifSkope * NifSkope::createWindow( const QString & fname )
{
	NifSkope * skope = new NifSkope;
	skope->setAttribute( Qt::WA_DeleteOnClose );
	skope->loadTheme();
	skope->show();
	skope->raise();
	skope->restoreUi();

	if ( !fname.isEmpty() ) {
		skope->loadFile( fname );
	}

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

	// Animation timeline
	dTimeline = new QDockWidget( tr( "Timeline" ), this );
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
	aAutoKey->setToolTip( tr( "After a gizmo transform (G/R/S in the viewport), key it on the timeline's transform lane at the playhead" ) );
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

	// Blender-style redo panel: tweak the parameters of the last transform
	{
		QFrame * rp = new QFrame( graphicsView );
		rp->setObjectName( QStringLiteral( "GizmoRedoPanel" ) );
		rp->setFrameShape( QFrame::StyledPanel );
		rp->setAutoFillBackground( true );
		rp->hide();

		QGridLayout * rpl = new QGridLayout( rp );
		rpl->setContentsMargins( 8, 6, 8, 6 );
		rpl->setHorizontalSpacing( 6 );

		QLabel * rpTitle = new QLabel( rp );
		QFont tf = rpTitle->font();
		tf.setBold( true );
		rpTitle->setFont( tf );
		QToolButton * rpClose = new QToolButton( rp );
		rpClose->setText( QStringLiteral( "✕" ) );
		rpClose->setAutoRaise( true );
		rpl->addWidget( rpTitle, 0, 0, 1, 5 );
		rpl->addWidget( rpClose, 0, 5 );
		connect( rpClose, &QToolButton::clicked, rp, &QWidget::hide );

		QLabel * rpLbl0 = new QLabel( rp ), * rpLbl1 = new QLabel( rp ), * rpLbl2 = new QLabel( rp );
		QDoubleSpinBox * rpVal0 = new QDoubleSpinBox( rp ), * rpVal1 = new QDoubleSpinBox( rp ), * rpVal2 = new QDoubleSpinBox( rp );
		QLabel * rpLbls[3] = { rpLbl0, rpLbl1, rpLbl2 };
		QDoubleSpinBox * rpVals[3] = { rpVal0, rpVal1, rpVal2 };
		for ( int i = 0; i < 3; i++ ) {
			rpVals[i]->setRange( -1.0e6, 1.0e6 );
			rpVals[i]->setDecimals( 4 );
			rpVals[i]->setKeyboardTracking( false );
			rpl->addWidget( rpLbls[i], 1, i * 2 );
			rpl->addWidget( rpVals[i], 1, i * 2 + 1 );
		}

		auto applyEdit = [this, rp, rpVal0, rpVal1, rpVal2]() {
			if ( !rp->isVisible() )
				return;
			if ( !ogl->gizmoReapply( Vector3( (float)rpVal0->value(), (float)rpVal1->value(), (float)rpVal2->value() ) ) )
				rp->hide();
		};
		for ( auto sb : { rpVal0, rpVal1, rpVal2 } )
			connect( sb, qOverload<double>( &QDoubleSpinBox::valueChanged ), applyEdit );

		connect( ogl, &GLView::transformGesture,
			[this, rp, rpTitle, rpLbl0, rpLbl1, rpLbl2, rpVal0, rpVal1, rpVal2]( int mode, int axis, const Vector3 & p ) {
			static const char * axisNames[4] = { "View", "X", "Y", "Z" };
			static const char * orientNames[4] = { "Global", "Local", "Parent", "View" };
			QLabel * lbls[3] = { rpLbl0, rpLbl1, rpLbl2 };
			QDoubleSpinBox * vals[3] = { rpVal0, rpVal1, rpVal2 };

			for ( auto sb : vals )
				sb->blockSignals( true );

			bool three = ( mode == 1 );
			if ( mode == 1 ) {
				rpTitle->setText( tr( "Move  (%1%2)" ).arg( QLatin1String( orientNames[ogl->gizmoOrient] ),
					axis > 0 ? QStringLiteral( " " ) + QLatin1String( axisNames[axis] ) : QString() ) );
				const char * comps[3] = { "X", "Y", "Z" };
				for ( int i = 0; i < 3; i++ ) {
					lbls[i]->setText( QLatin1String( comps[i] ) );
					vals[i]->setValue( p[i] );
				}
			} else if ( mode == 2 ) {
				rpTitle->setText( tr( "Rotate around %1 (%2)" ).arg( QLatin1String( axisNames[axis] ),
					QLatin1String( orientNames[ogl->gizmoOrient] ) ) );
				lbls[0]->setText( tr( "Angle°" ) );
				vals[0]->setValue( p[0] );
			} else {
				rpTitle->setText( tr( "Scale (uniform)" ) );
				lbls[0]->setText( tr( "Factor" ) );
				vals[0]->setValue( p[0] );
			}

			for ( int i = 1; i < 3; i++ ) {
				lbls[i]->setVisible( three );
				vals[i]->setVisible( three );
			}
			for ( auto sb : vals )
				sb->blockSignals( false );

			rp->adjustSize();
			rp->move( 10, graphicsView->height() - rp->height() - 10 );
			rp->show();
			rp->raise();
		} );

		connect( this, &NifSkope::completeLoading, rp, &QWidget::hide );
	}

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

	// Snapping (Blender-style snap target for Ctrl-dragging)
	QMenu * mSnapTgt = new QMenu( tr( "Snap Target (Ctrl)" ), this );
	QActionGroup * grpSnapTgt = new QActionGroup( this );
	const char * snapNames[4] = {
		QT_TR_NOOP( "Grid Step" ), QT_TR_NOOP( "Vertex" ), QT_TR_NOOP( "Edge" ), QT_TR_NOOP( "Face" )
	};
	const char * snapIcons[4] = { "snap_increment", "vert", "edge", "face" };
	for ( int i = 0; i < 4; i++ ) {
		QAction * a = mSnapTgt->addAction( tlMakeIcon( QLatin1String( snapIcons[i] ), icoColHdr ), tr( snapNames[i] ) );
		a->setCheckable( true );
		a->setChecked( i == 0 );
		grpSnapTgt->addAction( a );
		connect( a, &QAction::triggered, [this, i]() { ogl->snapTargetMode = i; } );
	}
	mSnapTgt->addSeparator();
	QAction * aAlignRot = mSnapTgt->addAction( tr( "Align Rotation to Target" ) );
	aAlignRot->setCheckable( true );
	aAlignRot->setToolTip( tr( "When face snapping, orient the node's +Z to the surface normal" ) );
	connect( aAlignRot, &QAction::toggled, [this]( bool on ) { ogl->snapAlignRot = on; } );
	QAction * aRotSnap = mSnapTgt->addAction( tr( "Rotation Snap Angle..." ) );
	connect( aRotSnap, &QAction::triggered, [this]() {
		bool ok = false;
		double v = QInputDialog::getDouble( this, tr( "Rotation snap" ),
			tr( "Snap increment for Ctrl-rotating (degrees):" ), GLView::gizmoRotSnapDeg, 0.1, 180.0, 1, &ok );
		if ( ok )
			GLView::gizmoRotSnapDeg = (float)v;
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
		bool ok = false;
		double a = QInputDialog::getDouble( this, tr( "Select Linked by Angle" ),
			tr( "Grow across faces meeting within this angle (degrees):" ), 30.0, 0.0, 180.0, 1, &ok );
		if ( ok )
			ogl->selectLinked( true, (float)a );
	} );
	// (3D cursor menu is on the toolbar Cursor button)

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
	ui->menuShow->addAction(dTimeline->toggleViewAction());

	// ---- main toolbar overhaul: smaller icons, merged dropdowns, transform header ----

	// smaller icons everywhere (~75%)
	for ( QToolBar * tb : { ui->tFile, ui->tRender, ui->tAnim, ui->tView, ui->tLOD } ) {
		QSize is = tb->iconSize();
		tb->setIconSize( QSize( is.width() * 3 / 4, is.height() * 3 / 4 ) );
	}

	// open/save already live in the File menu; drop them from the toolbar
	ui->tFile->removeAction( ui->aOpenMenu );
	ui->tFile->removeAction( ui->aSaveMenu );

	// Object / Edit mode selector replaces the two select-mode buttons
	{
		QComboBox * cbMode = new QComboBox( this );
		cbMode->addItems( { tr( "Object Mode" ), tr( "Edit Mode" ) } );
		// Blender-style mode icons: cube for object mode, vertices for edit mode
		cbMode->setItemIcon( 0, tlMakeIcon( QStringLiteral( "orient_local" ), QColor( 228, 228, 232 ) ) );
		cbMode->setItemIcon( 1, tlMakeIcon( QStringLiteral( "vert" ), QColor( 228, 228, 232 ) ) );
		cbMode->setToolTip( tr( "Object / Edit mode (Tab). Edit mode enables vertex/edge/face editing on the selected mesh." ) );
		connect( cbMode, qOverload<int>( &QComboBox::activated ), [this]( int i ) {
			ogl->setEditMode( i == 1 );
		} );
		connect( ogl, &GLView::editModeChanged, [cbMode]( bool editing ) {
			QSignalBlocker sb( cbMode );
			cbMode->setCurrentIndex( editing ? 1 : 0 );
		} );
		ui->tRender->insertWidget( ui->aSelectObject, cbMode );
		ui->tRender->removeAction( ui->aSelectObject );
		ui->tRender->removeAction( ui->aSelectVertex );
	}

	// view directions collapse into one dropdown button
	{
		QToolButton * btn = new QToolButton( this );
		btn->setPopupMode( QToolButton::InstantPopup );
		btn->setToolTip( tr( "Viewpoint" ) );
		btn->setIcon( ui->aViewPerspective->icon() );
		QMenu * m = new QMenu( btn );
		const QList<QAction *> vs = { ui->aViewTop, ui->aViewFront, ui->aViewLeft, ui->aViewFlip,
			ui->aViewPerspective, ui->aViewWalk, ui->aViewUser, ui->aViewUserSave };
		for ( QAction * a : vs )
			m->addAction( a );
		btn->setMenu( m );
		ui->tRender->insertWidget( ui->aViewTop, btn );
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

		btn->setMenu( m );
		ui->tRender->insertWidget( ui->aShowCollision, btn );
		for ( QAction * a : ds )
			ui->tRender->removeAction( a );
		ui->tRender->removeAction( aSolo );
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
		connect( btnMagnet, &QToolButton::toggled, [this]( bool on ) { ogl->snapDefaultOn = on; } );
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
				} );
				arl->addWidget( b );
			}
			pl->addWidget( arow );

			QCheckBox * cbAlign = new QCheckBox( tr( "Align Rotation to Target" ), panel );
			cbAlign->setChecked( aAlignRot->isChecked() );
			connect( cbAlign, &QCheckBox::toggled, aAlignRot, &QAction::setChecked );
			connect( aAlignRot, &QAction::toggled, cbAlign, &QCheckBox::setChecked );
			pl->addWidget( cbAlign );

			addHeader( tr( "Rotation Increment" ) );
			QWidget * rrow = new QWidget( panel );
			QHBoxLayout * rrl = new QHBoxLayout( rrow );
			rrl->setContentsMargins( 0, 0, 0, 0 );
			rrl->setSpacing( 2 );
			QDoubleSpinBox * spRot = new QDoubleSpinBox( rrow );
			spRot->setRange( 0.1, 180.0 );
			spRot->setDecimals( 1 );
			spRot->setSuffix( QStringLiteral( "°" ) );
			spRot->setValue( GLView::gizmoRotSnapDeg );
			connect( spRot, qOverload<double>( &QDoubleSpinBox::valueChanged ),
				[]( double v ) { GLView::gizmoRotSnapDeg = (float)v; } );
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
					if ( !ogl->editMode )
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
		}

		aGizmoHandles->setIconText( tr( "Gizmo" ) );
		ui->tRender->addAction( aGizmoHandles );

		// wireframe overlay toggle - on by default in object mode (shows the
		// selected mesh wireframe), auto-off when entering edit mode
		QToolButton * btnWire = new QToolButton( this );
		btnWire->setText( tr( "Wire" ) );
		btnWire->setCheckable( true );
		btnWire->setChecked( false );
		btnWire->setAutoRaise( true );
		btnWire->setToolTip( tr( "Wireframe overlay on the active/edit mesh. Off by default (selected objects show an outline instead); toggle on when you want the full wireframe." ) );
		ogl->wireframeOverlay = false;
		connect( btnWire, &QToolButton::toggled, [this]( bool on ) {
			ogl->wireframeOverlay = on;
			ogl->update();
		} );
		connect( ogl, &GLView::editModeChanged, [btnWire]( bool editing ) {
			// off on entering edit mode (only the selection shows)
			if ( editing && btnWire->isChecked() )
				btnWire->setChecked( false );
		} );
		ui->tRender->addWidget( btnWire );

		// Blender-style origin dots + parent relationship lines toggle
		QToolButton * btnOrig = new QToolButton( this );
		btnOrig->setText( tr( "Origins" ) );
		btnOrig->setCheckable( true );
		btnOrig->setChecked( true );
		btnOrig->setAutoRaise( true );
		btnOrig->setToolTip( tr( "Show origin points of selected nodes and dashed lines to their parents" ) );
		connect( btnOrig, &QToolButton::toggled, [this]( bool on ) {
			ogl->showOrigins = on;
			ogl->update();
		} );
		ui->tRender->addWidget( btnOrig );

		QToolButton * btnCursor = new QToolButton( this );
		btnCursor->setPopupMode( QToolButton::InstantPopup );
		btnCursor->setText( tr( "Cursor" ) );
		btnCursor->setToolTip( tr( "3D cursor and element utilities" ) );
		btnCursor->setMenu( mCursor );
		ui->tRender->addWidget( btnCursor );
	}

	// Material / Texture Manager panel (starts floating; toggled from Panels)
	extern QDockWidget * tlCreateMatTexManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl );
	QDockWidget * dMatMgr = tlCreateMatTexManagerDock( nif, this, ogl );
	dMatMgr->toggleViewAction()->setText( tr( "Material / Texture Manager" ) );

	// dock toggles collapse into one dropdown on the View toolbar
	{
		QToolButton * btn = new QToolButton( this );
		btn->setPopupMode( QToolButton::InstantPopup );
		btn->setText( tr( "Panels" ) );
		btn->setToolTip( tr( "Show/hide panels" ) );
		QMenu * m = new QMenu( btn );
		for ( QDockWidget * dw : { dList, dTree, dHeader, dBrowser, dInsp, dKfm, dRefr, dTimeline, dMatMgr } ) {
			m->addAction( dw->toggleViewAction() );
			ui->tView->removeAction( dw->toggleViewAction() );
		}
		btn->setMenu( m );
		ui->tView->addWidget( btn );
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


	// BSA Recent Archives
	auto tRecentArchives = new QToolButton( this );
	tRecentArchives->setText( "Recent Archives" );
	setFlyout( tRecentArchives, ui->mRecentArchives );

	// BSA Recent Files
	auto tRecentArchiveFiles = new QToolButton( this );
	tRecentArchiveFiles->setText( "Recent Files" );
	setFlyout( tRecentArchiveFiles, mRecentArchiveFiles );

	ui->bsaTitleBar->layout()->addWidget( tRecentArchives );
	ui->bsaTitleBar->layout()->addWidget( tRecentArchiveFiles );


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
	lightingWidget->setActions( {ui->aLighting, ui->aTextures, ui->aVertexColors,
								ui->aSpecular, ui->aCubeMapping, ui->aGlow,
								ui->aVisNormals, ui->aSilhouette} );
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

	if ( !saveConfirm() )
		return;

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

		ogl->setOrientation( ogl->cfg.startupDirection );

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

	// Center the model on load
	ogl->center();

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
	settings.setValue( "GLView/Perspective", ui->aViewPerspective->isChecked() );
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

	auto isPersp = settings.value( "GLView/Perspective", true ).toBool();
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


bool NifSkope::eventFilter( QObject * o, QEvent * e )
{
	// TODO: This doesn't seem to be doing anything extra
	//if ( e->type() == QEvent::Polish ) {
	//	QTimer::singleShot( 0, this, SLOT( overrideViewFont() ) );
	//}

	switch ( e->type() ) {
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
		auto startModal = [this]( int m ) {
			if ( ogl->editMode && !ogl->pickedElems.isEmpty() && ogl->gizmoBeginElement( m ) )
				return;
			ogl->gizmoBegin( m );
		};

		QMenu menu( this );
		menu.addAction( tr( "Place Gizmo (3D Cursor) Here" ), [this, clickPos]() {
			ogl->placeCursor( QPointF( clickPos ) );
		} );
		menu.addSeparator();
		menu.addAction( tr( "Move\tG" ), [startModal]() { startModal( 1 ); } );
		menu.addAction( tr( "Rotate\tR" ), [startModal]() { startModal( 2 ); } );
		menu.addAction( tr( "Scale\tS" ), [startModal]() { startModal( 3 ); } );
		if ( ogl->editMode ) {
			menu.addSeparator();
			QAction * aDel = menu.addAction( tr( "Delete Selected Elements\tX" ), [this]() {
				ogl->deletePickedElements();
			} );
			aDel->setEnabled( !ogl->pickedElems.isEmpty() );
			QAction * aLinked = menu.addAction( tr( "Select Linked\tCtrl+L" ), [this]() {
				ogl->selectLinked( false );
			} );
			aLinked->setEnabled( !ogl->pickedElems.isEmpty() );
		}
		if ( idx.isValid() ) {
			// flatten the spell categories into the bottom of this menu
			menu.addSeparator();
			const auto spellActs = contextBook.actions();
			for ( QAction * a : spellActs )
				menu.addAction( a );
		}
		menu.exec( p );
		return;
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
