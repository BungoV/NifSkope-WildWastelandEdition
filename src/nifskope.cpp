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
#include "data/niftypes.h"
#include "spellbook.h"
#include "spells/blocks.h"	// setBlockListSelection for Copy Branch multi-select
#include "version.h"
#include "gl/glscene.h"
#include "model/kfmmodel.h"
#include "model/nifmodel.h"
#include "model/nifproxymodel.h"
#include "ui/widgets/fileselect.h"
#include "ui/widgets/nifview.h"
#include "ui/widgets/refrbrowser.h"
#include "ui/widgets/inspect.h"
#include "ui/widgets/timeline.h"
#include "ui/about_dialog.h"
#include "ui/settingsdialog.h"
#include "qt5compat.hpp"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBuffer>
#include <QButtonGroup>
#include <QByteArray>
#include <QCloseEvent>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QSettings>
#include <QSignalBlocker>
#include <QTimer>
#include <QTranslator>
#include <QUrl>
#include <QVBoxLayout>
#include <QCryptographicHash>

#include <QListView>
#include <QLineEdit>
#include <QShortcut>
#include <QTreeView>
#include <QStandardItemModel>
#include <QStyle>
#include <QStyleFactory>
#include <QSplitter>
#include <QTabBar>
#include <QToolBar>
#include <QToolButton>

#include <functional>
#include <algorithm>
#include <utility>
#include <vector>

#include "ba2file.hpp"
#include "bsamodel.h"

#ifdef WIN32
#  define WINDOWS_LEAN_AND_MEAN
#  include "windows.h"
#endif


//! @file nifskope.cpp The main file for %NifSkope

SettingsDialog * NifSkope::options;

const QList<QPair<QString, QString>> NifSkope::filetypes = {
	// NIF types
	{ "NIF", "nif" }, { "Bethesda Terrain", "btr" }, { "Bethesda Terrain Object", "bto" },
	// KF types
	{ "Keyframe", "kf" }, { "Keyframe Animation", "kfa" }, { "Keyframe Motion", "kfm" },
	// Miscellaneous NIF types
	{ "NIFCache", "nifcache" }, { "TEXCache", "texcache" }, { "PCPatch", "pcpatch" }, { "JMI", "jmi" },
	{ "Divinity 2 Character Template", "cat" }
};

namespace
{

QList<NifSkope *> sessionDocumentWindows;
QList<BackgroundNifDocument *> sessionBackgroundDocuments;

constexpr int NifBrowserSourceRole = Qt::UserRole + 37;
constexpr int NifBrowserLooseFile = 1;
constexpr int NifBrowserDocumentRole = Qt::UserRole + 38;
constexpr int NifBrowserBackgroundDocumentRole = Qt::UserRole + 39;
constexpr int NifBrowserConfiguredResource = 2;
constexpr int NifBrowserGameRole = Qt::UserRole + 40;
constexpr int NifBrowserFolderPathRole = Qt::UserRole + 41;

class LoadedNifsTreeView final : public QTreeView
{
public:
	explicit LoadedNifsTreeView( QWidget * parent = nullptr ) : QTreeView( parent ) {}
	QTreeView * sourceView = nullptr;
	std::function<void()> addBrowserSelection;

protected:
	void dragEnterEvent( QDragEnterEvent * event ) override
	{
		if ( event->source() == sourceView ) event->acceptProposedAction();
		else QTreeView::dragEnterEvent( event );
	}

	void dragMoveEvent( QDragMoveEvent * event ) override
	{
		if ( event->source() == sourceView ) event->acceptProposedAction();
		else QTreeView::dragMoveEvent( event );
	}

	void dropEvent( QDropEvent * event ) override
	{
		if ( event->source() == sourceView && addBrowserSelection ) {
			addBrowserSelection();
			event->acceptProposedAction();
			return;
		}
		QTreeView::dropEvent( event );
	}
};

class LoadedNifsDelegate final : public QStyledItemDelegate
{
public:
	explicit LoadedNifsDelegate( QObject * parent = nullptr ) : QStyledItemDelegate( parent ) {}

	void paint( QPainter * painter, const QStyleOptionViewItem & option,
		const QModelIndex & index ) const override
	{
		QStyleOptionViewItem opt( option );
		initStyleOption( &opt, index );
		const QVariant background = index.data( Qt::BackgroundRole );
		const QVariant foreground = index.data( Qt::ForegroundRole );
		if ( background.canConvert<QColor>() ) {
			opt.backgroundBrush = background.value<QColor>();
			// The Block List lets its explicit primary/secondary role colours win
			// over the platform selection brush; mirror that behavior exactly.
			opt.state &= ~QStyle::State_Selected;
		}
		if ( foreground.canConvert<QColor>() ) {
			const QColor color = foreground.value<QColor>();
			opt.palette.setColor( QPalette::Text, color );
			opt.palette.setColor( QPalette::HighlightedText, color );
		}
		const QWidget * widget = option.widget;
		QStyle * style = widget ? widget->style() : QApplication::style();
		style->drawControl( QStyle::CE_ItemViewItem, &opt, painter, widget );
	}
};

static int sessionSceneParent( const NifModel * nif, int block )
{
	int found = -1;
	for ( int parent = 0; parent < nif->getBlockCount(); parent++ ) {
		QModelIndex iParent = nif->getBlockIndex( parent );
		if ( !nif->blockInherits( iParent, "NiNode" )
			|| !nif->getLinkArray( iParent, "Children" ).contains( block ) )
			continue;
		if ( found >= 0 )
			return -2;
		found = parent;
	}
	return found;
}

static bool sessionAbsoluteWorld( const NifModel * nif, int block, Transform & world )
{
	QVector<int> chain;
	QSet<int> seen;
	for ( int current = block; nif->isValidBlockNumber( current ) && !seen.contains( current ); ) {
		seen << current;
		if ( !nif->blockInherits( nif->getBlockIndex( current ), "NiAVObject" ) )
			return false;
		chain << current;
		int parent = sessionSceneParent( nif, current );
		if ( parent == -2 )
			return false;
		current = parent;
	}
	if ( chain.isEmpty() )
		return false;
	world = Transform();
	for ( int i = chain.size() - 1; i >= 0; i-- )
		world = world * Transform( nif, nif->getBlockIndex( chain.at( i ) ) );
	return true;
}

//! Flatten FO4 BSTriShape geometry from a secondary document into world-space
//! triangles. This is preview-only: materials, picking and NIF ownership remain
//! with the document that owns the model.
static QVector<Vector3> sessionDocumentTriangleSoup( const NifModel * nif )
{
	QVector<Vector3> soup;
	if ( !nif )
		return soup;
	for ( int block = 0; block < nif->getBlockCount(); block++ ) {
		QModelIndex shape = nif->getBlockIndex( block );
		if ( !nif->blockInherits( shape, "BSTriShape" ) )
			continue;
		QModelIndex vertices = nif->getIndex( shape, "Vertex Data" );
		QModelIndex triangles = nif->getIndex( shape, "Triangles" );
		if ( !vertices.isValid() || !triangles.isValid() )
			continue;
		Transform world;
		if ( !sessionAbsoluteWorld( nif, block, world ) )
			world = Transform( nif, shape );
		QVector<Vector3> points;
		points.reserve( nif->rowCount( vertices ) );
		for ( int vertex = 0; vertex < nif->rowCount( vertices ); vertex++ )
			points << ( world * nif->get<Vector3>( nif->getIndex( nif->getIndex( vertices, vertex ), "Vertex" ) ) );
		for ( int triangle = 0; triangle < nif->rowCount( triangles ); triangle++ ) {
			Triangle t = nif->get<Triangle>( nif->getIndex( triangles, triangle ) );
			if ( t.v1() >= points.size() || t.v2() >= points.size() || t.v3() >= points.size() )
				continue;
			soup << points.at( t.v1() ) << points.at( t.v2() ) << points.at( t.v3() );
		}
	}
	return soup;
}

//! A one-cell editor used by the Block List.  Unlike the general value
//! delegate, it lets us validate unique node names and update animation
//! references as one undoable operation before the edit is accepted.
class BlockListRenameEdit final : public QLineEdit
{
public:
	explicit BlockListRenameEdit( QWidget * parent ) : QLineEdit( parent ) {}

	std::function<bool( const QString & )> commitRename;

	void cancel()
	{
		if ( finished ) return;
		finished = true;
		hide();
		deleteLater();
	}

protected:
	void keyPressEvent( QKeyEvent * event ) override
	{
		if ( event->key() == Qt::Key_Escape ) {
			cancel();
			event->accept();
			return;
		}
		if ( event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter ) {
			acceptRename();
			event->accept();
			return;
		}
		QLineEdit::keyPressEvent( event );
	}

	void focusOutEvent( QFocusEvent * event ) override
	{
		QLineEdit::focusOutEvent( event );
		if ( !finished && !committing ) acceptRename();
	}

private:
	void acceptRename()
	{
		if ( finished || committing ) return;
		committing = true;
		const bool accepted = !commitRename || commitRename( text() );
		committing = false;
		if ( accepted ) {
			finished = true;
			hide();
			deleteLater();
		} else {
			QTimer::singleShot( 0, this, [this]() {
				if ( !finished ) {
					show();
					setFocus( Qt::OtherFocusReason );
					selectAll();
				}
			} );
		}
	}

	bool finished = false;
	bool committing = false;
};

static int propagateSceneObjectName( NifModel * nif, int nodeNumber,
	const QString & oldName, const QString & newName )
{
	int fixes = 0;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex block = nif->getBlockIndex( b );
		if ( nif->blockInherits( block, "NiDefaultAVObjectPalette" ) ) {
			QModelIndex objects = nif->getIndex( block, "Objs" );
			for ( int row = 0; row < nif->rowCount( objects ); row++ ) {
				QModelIndex object = nif->index( row, 0, objects );
				bool matches = nif->getLink( object, "AV Object" ) == nodeNumber;
				if ( !matches && !oldName.isEmpty() )
					matches = nif->resolveString( object, "Name" ) == oldName;
				if ( matches ) {
					nif->assignString( object, "Name", newName );
					fixes++;
				}
			}
		} else if ( nif->blockInherits( block, "NiControllerSequence" ) ) {
			QModelIndex controlled = nif->getIndex( block, "Controlled Blocks" );
			for ( int row = 0; row < nif->rowCount( controlled ); row++ ) {
				QModelIndex entry = nif->index( row, 0, controlled );
				if ( !oldName.isEmpty() && nif->resolveString( entry, "Node Name" ) == oldName ) {
					nif->assignString( entry, "Node Name", newName );
					fixes++;
				}
			}
		}
	}
	return fixes;
}

static bool renameSceneObjectInline( NifModel * nif, const QModelIndex & block,
	const QString & requestedName, QString * error, int * updatedReferences )
{
	if ( error ) error->clear();
	if ( updatedReferences ) *updatedReferences = 0;
	if ( !nif || !block.isValid() || !nif->blockInherits( block, "NiAVObject" ) ) {
		if ( error ) *error = QObject::tr( "This block has no unique scene-object Name to rename." );
		return false;
	}

	const QString newName = requestedName.trimmed();
	const QString oldName = nif->resolveString( block, "Name" );
	if ( newName == oldName ) return true;
	if ( newName.isEmpty() ) {
		if ( error ) *error = QObject::tr( "A scene-object name cannot be empty." );
		return false;
	}

	const int nodeNumber = nif->getBlockNumber( block );
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		if ( b == nodeNumber ) continue;
		QModelIndex other = nif->getBlockIndex( b );
		if ( nif->blockInherits( other, "NiAVObject" )
			&& nif->resolveString( other, "Name" ).compare( newName, Qt::CaseInsensitive ) == 0 ) {
			if ( error ) *error = QObject::tr(
				"Another scene object already uses the name '%1'. Choose a unique name." ).arg( newName );
			return false;
		}
	}

	int fixes = 0;
	const bool saved = nifSnapshotOp( nif, QObject::tr( "Rename node to %1" ).arg( newName ), [&]() {
		nif->assignString( block, "Name", newName );
		fixes = propagateSceneObjectName( nif, nodeNumber, oldName, newName );
	} );
	if ( !saved ) {
		if ( error ) *error = QObject::tr( "The rename could not be added to the undo history." );
		return false;
	}
	if ( updatedReferences ) *updatedReferences = fixes;
	return true;
}

} // namespace

/*! A Loaded NIFs workspace member without any window, viewport, or views.
 *
 * Explicitly enrolled donors used to be full hidden NifSkope windows; each one
 * constructed a complete main-window UI and a GL viewport it never showed.
 * This data-only layer keeps just the parsed model plus enough source identity
 * to reload it into a real window if the user promotes it to primary. Because
 * a background document has no editing UI, its model can never become dirty,
 * so promotion may safely re-parse from the original source.
 */
class BackgroundNifDocument
{
public:
	BackgroundNifDocument()
	{
		nif = new NifModel;
		// Batch background parses must never raise modal message boxes.
		nif->setMessageMode( BaseModel::MSG_TEST );
	}

	~BackgroundNifDocument()
	{
		sessionBackgroundDocuments.removeAll( this );
		delete nif;
	}

	QString displayName() const
	{
		QString name = QFileInfo( currentFile ).fileName();
		return name.isEmpty() ? QObject::tr( "Untitled" ) : name;
	}

	bool selectedInWorkspace() const
	{
		return sessionPreviewVisible && !sessionPreviewUnloaded;
	}

	NifModel * nif = nullptr;
	//! Display path: an absolute loose-file path or a "[Game]/meshes/..." label.
	QString currentFile;
	int configuredResourceGame = -1;
	QString configuredResourcePath;
	//! The visible window whose workspace this document belongs to.
	NifSkope * workspaceRoot = nullptr;
	bool sessionPreviewVisible = false;
	bool sessionPreviewUnloaded = false;
};

static const QHash<QString, QString> migrateTo1_2 = {
		{ "Export Settings/export_culling", "Export Settings/Export Culling" },
		{ "auto sanitize", "File/Auto Sanitize" },
		{ "last load", "File/Last Load" }, { "last save", "File/Last Save" },
		{ "fsengine/archives", "FSEngine/Archives" },
		{ "enable animations", "GLView/Enable Animations" }, { "LOD/LOD Level", "GLView/LOD Level" },
		{ "loop animation", "GLView/Loop Animation" }, { "perspective", "GLView/Perspective" },
		{ "play animation", "GLView/Play Animation" }, { "switch animation", "GLView/Switch Animation" },
		{ "view action", "GLView/View Action" },
		{ "JPEG/Quality", "JPEG/Quality" },
		{ "Render Settings/Anti Aliasing", "Render Settings/Anti Aliasing" }, { "Render Settings/Background", "Render Settings/Background" },
		{ "Render Settings/Cull Expression", "Render Settings/Cull Expression" }, { "Render Settings/Cull Nodes By Name", "Render Settings/Cull Nodes By Name" },
		{ "Render Settings/Cull Non Textured", "Render Settings/Cull Non Textured" }, { "Render Settings/Draw Axes", "Render Settings/Draw Axes" },
		{ "Render Settings/Draw Collision Geometry", "Render Settings/Draw Collision Geometry" }, { "Render Settings/Draw Constraints", "Render Settings/Draw Constraints" },
		{ "Render Settings/Draw Furniture Markers", "Render Settings/Draw Furniture Markers" }, { "Render Settings/Draw Nodes", "Render Settings/Draw Nodes" },
		{ "Render Settings/Enable Shaders", "Render Settings/Enable Shaders" }, { "Render Settings/Foreground", "Render Settings/Foreground" },
		{ "Render Settings/Highlight", "Render Settings/Highlight" },
		{ "Render Settings/Show Hidden Objects", "Render Settings/Show Hidden Objects" }, { "Render Settings/Show Stats", "Render Settings/Show Stats" },
		{ "Render Settings/Texture Alternatives", "Render Settings/Texture Alternatives" }, { "Render Settings/Texture Folders", "Render Settings/Texture Folders" },
		{ "Render Settings/Texturing", "Render Settings/Texturing" }, { "Render Settings/Up Axis", "Render Settings/Up Axis" },
		{ "Settings/Language", "Settings/Language" }, { "Settings/Startup Version", "Settings/Startup Version" },
		{ "hide condition zero", "UI/Hide Mismatched Rows" }, { "realtime condition updating", "UI/Realtime Condition Updating" },
		{ "XML Checker/Directory", "XML Checker/Directory" }, { "XML Checker/Recursive", "XML Checker/Recursive" },
		{ "XML Checker/Threads", "XML Checker/Threads" }, { "XML Checker/check kf", "XML Checker/Check KF" },
		{ "XML Checker/check kfm", "XML Checker/Check KFM" }, { "XML Checker/check nif", "XML Checker/Check NIF" },
		{ "XML Checker/report errors only", "XML Checker/Report Errors Only" },
		{ "import-export/3ds/File Name", "Import-Export/3DS/File Name" }, { "import-export/obj/File Name", "Import-Export/OBJ/File Name" },
		{ "spells/Block/Remove By Id/match expression", "Spells/Block/Remove By Id/Match Expression" },
		{ "last texture path", "Spells/Texture/Choose/Last Texture Path" },
		{ "spells/Texture/Export Template/Antialias", "Spells/Texture/Export Template/Antialias" },
		{ "spells/Texture/Export Template/File Name", "Spells/Texture/Export Template/File Name" },
		{ "spells/Texture/Export Template/Image Size", "Spells/Texture/Export Template/Image Size" },
		{ "spells/Texture/Export Template/Wire Color", "Spells/Texture/Export Template/Wire Color" },
		{ "spells/Texture/Export Template/Wrap Mode", "Spells/Texture/Export Template/Wrap Mode" },
		{ "version", "Version" }
};

static const QHash<QString, QString> migrateTo2_0 = {
		{ "Export Settings/Export Culling", "Export Settings/Export Culling" },
		{ "File/Recent File List", "File/Recent File List" },
		{ "File/Auto Sanitize", "File/Auto Sanitize" },
		{ "File/Last Load", "File/Last Load" }, { "File/Last Save", "File/Last Save" },
		{ "FSEngine/Archives", "FSEngine/Archives" },
		{ "Render Settings/Anti Aliasing", "Render Settings/Anti Aliasing" },
		{ "Render Settings/Texturing", "Render Settings/Texturing" },
		{ "Render Settings/Enable Shaders", "Render Settings/Enable Shaders" },
		{ "Render Settings/Background", "Render Settings/Background" },
		{ "Render Settings/Foreground", "Render Settings/Foreground" },
		{ "Render Settings/Highlight", "Render Settings/Highlight" },
		{ "Render Settings/Texture Alternatives", "Render Settings/Texture Alternatives" },
		{ "Render Settings/Texture Folders", "Render Settings/Texture Folders" },
		{ "Render Settings/Up Axis", "Render Settings/Up Axis" },
		{ "Settings/Language", "Settings/Language" }, { "Settings/Startup Version", "Settings/Startup Version" },
};

QStringList NifSkope::fileExtensions()
{
	QStringList fileExts;
	for ( int i = 0; i < filetypes.size(); i++ ) {
		fileExts << filetypes.at( i ).second;
	}

	return fileExts;
}

QString NifSkope::fileFilter( const QString & ext )
{
	QString filter;

	for ( int i = 0; i < filetypes.size(); i++ ) {
		auto& ft = filetypes.at(i);
		if ( ft.second == ext )
			filter = QString( "%1 (*.%2)" ).arg( ft.first ).arg( ft.second );
	}

	return filter;
}

QString NifSkope::fileFilters( bool allFiles )
{
	QStringList filters;

	if ( allFiles ) {
		filters << QString( "All Files (*.%1)" ).arg( fileExtensions().join( " *." ) );
	}

	for ( int i = 0; i < filetypes.size(); i++ ) {
		filters << QString( "%1 (*.%2)" ).arg( filetypes.at( i ).first ).arg( filetypes.at( i ).second );
	}

	return filters.join( ";;" );
}


/*
 * main GUI window
 */

NifSkope::NifSkope( bool background )
	: QMainWindow(), ui( new Ui::MainWindow )
{
	backgroundWorkspaceDocument = background;
	// Init UI
	ui->setupUi( this );

	for ( const auto & s : QStyleFactory::keys() ) {
		ui->mTheme->addAction( s )->setCheckable( true );
	}

	if ( !backgroundWorkspaceDocument ) {
		qApp->installEventFilter( this );
		applicationEventFilterInstalled = true;
	}

	// Init Dialogs

	if ( !options )
		options = new SettingsDialog;

	// Migrate settings from older versions of NifSkope
	migrateSettings();

	// Update Settings struct from registry
	updateSettings();

	// Create models
	/* ********************** */

	nif = new NifModel( this );
	proxy = new NifProxyModel( this );
	proxy->setModel( nif );

	nifEmpty = new NifModel( this );
	proxyEmpty = new NifProxyModel( this );

	nif->setMessageMode( BaseModel::MSG_USER );

	// Setup QUndoStack
	nif->undoStack = new QUndoStack( this );

	indexStack = new QUndoStack( this );

	// Setup Window Modified on data change
	connect( nif, &NifModel::dataChanged, [this]( const QModelIndex &, const QModelIndex & ) {
		// Only if UI is enabled (prevents asterisk from flashing during save/load)
		const bool becameDirty = !isWindowModified() && !windowTitle().isEmpty() && isEnabled();
		if ( !windowTitle().isEmpty() && isEnabled() )
			setWindowModified( true );
		// Editing the primary cannot change its secondary preview geometry. Only
		// refresh tab labels here; rebuilding every open mesh per vertex edit
		// would make transforms and paint strokes unnecessarily expensive.
		if ( becameDirty )
			for ( NifSkope * document : std::as_const( sessionDocumentWindows ) )
				if ( document && ( !document->backgroundWorkspaceDocument || document->isVisible() ) )
					document->rebuildDocumentTabs();
	} );

	kfm = new KfmModel( this );
	kfmEmpty = new KfmModel( this );

	book = SpellBookPtr( new SpellBook( nif, QModelIndex(), this, SLOT( select( const QModelIndex & ) ) ) );

	// Setup Views
	/* ********************** */

	// Block List
	list = ui->list;
	list->setModel( proxy );
	list->setSortingEnabled( false );
	list->setItemDelegate( nif->createDelegate( this, book ) );
	list->installEventFilter( this );
	list->header()->resizeSection( NifModel::NameCol, 250 );

	// Block-list multi-selection (Blender-style): Shift/Ctrl-click several blocks.
	// Row colours come from NifModel BackgroundRole (active #FFA040 / secondary
	// #FF602A), which the delegate prefers over the selection highlight.
	list->setSelectionMode( QAbstractItemView::ExtendedSelection );
	list->setSelectionBehavior( QAbstractItemView::SelectRows );
	wireBlockListSelection();

	// Compact recursive filter for both hierarchy and flat Block List modes.
	// Parents stay visible when any descendant matches; search is deliberately
	// view-only and never changes the NIF or object selection.
	QWidget * blockNavigation = new QWidget( ui->dockWidgetContents_4 );
	QHBoxLayout * blockNavigationLayout = new QHBoxLayout( blockNavigation );
	blockNavigationLayout->setContentsMargins( 0, 0, 0, 0 );
	blockNavigationLayout->setSpacing( 2 );
	// Themed grey chevrons instead of Qt's black standard arrow icons, which
	// clashed with the rest of the toolbar.
	const QColor navIconColor( 210, 210, 214 );
	blockListBack = new QToolButton( blockNavigation );
	blockListBack->setAutoRaise( true );
	blockListBack->setIcon( tlMakeIcon( QStringLiteral( "chevron_left" ), navIconColor ) );
	blockListBack->setToolTip( tr( "Previous selected block" ) );
	blockListForward = new QToolButton( blockNavigation );
	blockListForward->setAutoRaise( true );
	blockListForward->setIcon( tlMakeIcon( QStringLiteral( "chevron_right" ), navIconColor ) );
	blockListForward->setToolTip( tr( "Next selected block" ) );
	blockNavigationLayout->addWidget( blockListBack );
	blockNavigationLayout->addWidget( blockListForward );
	blockListSearch = new QLineEdit( blockNavigation );
	blockListSearch->setObjectName( QStringLiteral( "BlockListSearch" ) );
	blockListSearch->setClearButtonEnabled( true );
	blockListSearch->setPlaceholderText( tr( "Search blocks by type, name, value, or #..." ) );
	blockListSearch->setToolTip( tr( "Space-separated terms must all match. Ctrl+F focuses this field; Esc clears it." ) );
	blockNavigationLayout->addWidget( blockListSearch, 1 );
	QToolButton * goToBlockButton = new QToolButton( blockNavigation );
	goToBlockButton->setAutoRaise( true );
	goToBlockButton->setText( QStringLiteral( "#" ) );
	goToBlockButton->setToolTip( tr( "Go to block (Ctrl+G)" ) );
	blockNavigationLayout->addWidget( goToBlockButton );
	blockListPin = new QToolButton( blockNavigation );
	blockListPin->setAutoRaise( true );
	blockListPin->setCheckable( true );
	blockListPin->setText( QString::fromUtf8( "\xE2\x98\x85" ) );
	blockListPin->setToolTip( tr( "Pin this block; use the arrow to revisit pinned blocks" ) );
	blockListPin->setPopupMode( QToolButton::MenuButtonPopup );
	blockListPin->setMenu( new QMenu( blockListPin ) );
	blockNavigationLayout->addWidget( blockListPin );
	blockListRelations = new QToolButton( blockNavigation );
	blockListRelations->setAutoRaise( true );
	blockListRelations->setText( tr( "Links" ) );
	blockListRelations->setToolTip( tr( "Outgoing links and blocks that reference the selection" ) );
	blockListRelations->setPopupMode( QToolButton::InstantPopup );
	blockListRelations->setMenu( new QMenu( blockListRelations ) );
	blockNavigationLayout->addWidget( blockListRelations );
	ui->verticalLayout_2->removeWidget( ui->listButtonFrame );
	blockNavigationLayout->addWidget( ui->listButtonFrame );
	ui->verticalLayout_2->insertWidget( 0, blockNavigation );

	QWidget * blockFilters = new QWidget( ui->dockWidgetContents_4 );
	QHBoxLayout * blockFilterLayout = new QHBoxLayout( blockFilters );
	blockFilterLayout->setContentsMargins( 0, 0, 0, 0 );
	blockFilterLayout->setSpacing( 1 );
	blockListFilterGroup = new QButtonGroup( blockFilters );
	blockListFilterGroup->setExclusive( true );
	struct BlockFilterDef { int id; QString name; QString icon; };
	const QList<BlockFilterDef> blockFilterDefs = {
		{ 0, tr( "All blocks" ), QString() },
		{ 1, tr( "Geometry" ), QStringLiteral( ":/btn/blockGeometry" ) },
		{ 2, tr( "Scene nodes" ), QStringLiteral( ":/btn/blockNode" ) },
		{ 3, tr( "Skinning and bones" ), QStringLiteral( ":/btn/skinned" ) },
		{ 4, tr( "Materials, shaders, and textures" ), QStringLiteral( ":/btn/blockMaterial" ) },
		{ 5, tr( "Collision" ), QStringLiteral( ":/btn/showCollision" ) },
		{ 6, tr( "Animation and controllers" ), QStringLiteral( ":/btn/blockAnimation" ) },
		{ 7, tr( "Extra data" ), QStringLiteral( ":/btn/blockExtraData" ) }
	};
	for ( const BlockFilterDef & def : blockFilterDefs ) {
		QToolButton * button = new QToolButton( blockFilters );
		button->setAutoRaise( true );
		button->setCheckable( true );
		button->setToolTip( def.name );
		if ( def.icon.isEmpty() ) button->setText( tr( "All" ) );
		else button->setIcon( QIcon( def.icon ) );
		button->setChecked( def.id == 0 );
		blockListFilterGroup->addButton( button, def.id );
		blockFilterLayout->addWidget( button );
	}
	blockFilterLayout->addStretch( 1 );
	ui->verticalLayout_2->insertWidget( 1, blockFilters );
	blockListBreadcrumb = new QLabel( ui->dockWidgetContents_4 );
	blockListBreadcrumb->setTextInteractionFlags( Qt::TextSelectableByMouse );
	blockListBreadcrumb->setStyleSheet( QStringLiteral( "color: #a8a8a8; padding: 1px 2px;" ) );
	blockListBreadcrumb->setToolTip( tr( "Scene-parent path for the selected block" ) );
	ui->verticalLayout_2->insertWidget( 2, blockListBreadcrumb );
	blockListFooter = new QLabel( ui->dockWidgetContents_4 );
	blockListFooter->setStyleSheet( QStringLiteral( "color: #a8a8a8; padding: 2px;" ) );
	ui->verticalLayout_2->addWidget( blockListFooter );
	connect( blockListSearch, &QLineEdit::textChanged, this, [this]() { applyBlockListFilter(); } );
	connect( blockListFilterGroup, &QButtonGroup::idClicked, this, [this]( int id ) {
		blockListQuickFilter = id;
		applyBlockListFilter();
	} );
	connect( blockListBack, &QToolButton::clicked, this, [this]() { navigateBlockListHistory( -1 ); } );
	connect( blockListForward, &QToolButton::clicked, this, [this]() { navigateBlockListHistory( 1 ); } );
	connect( goToBlockButton, &QToolButton::clicked, this, &NifSkope::goToBlock );
	connect( blockListPin, &QToolButton::clicked, this, [this]( bool checked ) {
		int block = nif ? nif->getBlockNumber( currentNifIndex() ) : -1;
		if ( block < 0 ) return;
		if ( checked ) blockListPins.insert( block );
		else blockListPins.remove( block );
		updateBlockListNavigation( currentNifIndex() );
	} );
	auto scheduleBlockFilter = [this]() {
		QTimer::singleShot( 0, this, [this]() {
			applyBlockListFilter();
			updateBlockListNavigation( currentNifIndex() );
		} );
	};
	connect( proxy, &QAbstractItemModel::modelReset, this, scheduleBlockFilter );
	connect( proxy, &QAbstractItemModel::layoutChanged, this, scheduleBlockFilter );
	connect( proxy, &QAbstractItemModel::rowsInserted, this,
		[this]( const QModelIndex &, int, int ) { applyBlockListFilter(); } );
	connect( proxy, &QAbstractItemModel::rowsRemoved, this,
		[this]( const QModelIndex &, int, int ) { applyBlockListFilter(); } );
	connect( proxy, &QAbstractItemModel::dataChanged, this,
		[this]( const QModelIndex &, const QModelIndex &, const QList<int> & ) {
			if ( blockListSearch && !blockListSearch->text().isEmpty() ) applyBlockListFilter();
		} );
	auto * findBlocks = new QShortcut( QKeySequence::Find, ui->ListDock );
	findBlocks->setContext( Qt::WidgetWithChildrenShortcut );
	connect( findBlocks, &QShortcut::activated, blockListSearch, [this]() {
		blockListSearch->setFocus( Qt::ShortcutFocusReason );
		blockListSearch->selectAll();
	} );
	auto * clearBlockSearch = new QShortcut( QKeySequence( Qt::Key_Escape ), blockListSearch );
	clearBlockSearch->setContext( Qt::WidgetShortcut );
	connect( clearBlockSearch, &QShortcut::activated, blockListSearch, &QLineEdit::clear );
	auto * goToBlockShortcut = new QShortcut( QKeySequence( QStringLiteral( "Ctrl+G" ) ), ui->ListDock );
	goToBlockShortcut->setContext( Qt::WidgetWithChildrenShortcut );
	connect( goToBlockShortcut, &QShortcut::activated, this, &NifSkope::goToBlock );
	auto * renameBlock = new QShortcut( QKeySequence( Qt::Key_F2 ), list );
	renameBlock->setContext( Qt::WidgetWithChildrenShortcut );
	connect( renameBlock, &QShortcut::activated, this,
		[this]() { renameBlockListIndex( list->currentIndex(), true ); } );
	connect( list, &NifTreeView::doubleClicked, this,
		[this]( const QModelIndex & index ) { renameBlockListIndex( index, false ); } );

	// Block Details
	tree = ui->tree;
	tree->setModel( nif );
	tree->setSortingEnabled( false );
	tree->setItemDelegate( nif->createDelegate( this, book ) );
	tree->installEventFilter( this );
	tree->header()->moveSection( 1, 2 );
	tree->header()->resizeSection( NifModel::NameCol, 135 );
	tree->header()->resizeSection( NifModel::ValueCol, 250 );
	blockDetailsSearch = new QLineEdit( ui->dockWidgetContents_2 );
	blockDetailsSearch->setClearButtonEnabled( true );
	blockDetailsSearch->setPlaceholderText( tr( "Filter fields by name or value..." ) );
	blockDetailsSearch->setToolTip( tr( "Parents remain visible when a nested field matches. Ctrl+Shift+F focuses this field." ) );
	ui->verticalLayout->insertWidget( 0, blockDetailsSearch );
	connect( blockDetailsSearch, &QLineEdit::textChanged, this, [this]() { applyBlockDetailsFilter(); } );
	auto * findBlockFields = new QShortcut( QKeySequence( QStringLiteral( "Ctrl+Shift+F" ) ), ui->TreeDock );
	findBlockFields->setContext( Qt::WidgetWithChildrenShortcut );
	connect( findBlockFields, &QShortcut::activated, blockDetailsSearch, [this]() {
		blockDetailsSearch->setFocus( Qt::ShortcutFocusReason );
		blockDetailsSearch->selectAll();
	} );
	// Allow multi-row paste
	//	Note: this has some side effects such as vertex selection
	//	in viewport being wrong if you attempt to select many rows.
	tree->setSelectionMode( QAbstractItemView::ExtendedSelection );
	tree->doAutoExpanding = true;

	// Header Details
	header = ui->header;
	header->setModel( nif );
	header->setItemDelegate( nif->createDelegate( this, book ) );
	header->installEventFilter( this );
	header->header()->moveSection( 1, 2 );
	header->header()->resizeSection( NifModel::NameCol, 135 );
	header->header()->resizeSection( NifModel::ValueCol, 250 );

	// KFM
	kfmtree = ui->kfmtree;
	kfmtree->setModel( kfm );
	kfmtree->setItemDelegate( kfm->createDelegate( this ) );
	kfmtree->installEventFilter( this );

	// Help Browser
	refrbrwsr = ui->refrBrowser;
	refrbrwsr->setNifModel( nif );

	// NIF Browser
	bsaView = ui->bsaView;
	connect( bsaView, &QTreeView::doubleClicked, this,
		[this]( const QModelIndex & index ) { openArchiveFile( index ); } );
	bsaView->setSelectionMode( QAbstractItemView::ExtendedSelection );
	bsaView->setSelectionBehavior( QAbstractItemView::SelectRows );
	bsaView->setDragEnabled( true );
	bsaView->setDragDropMode( QAbstractItemView::DragOnly );
	bsaView->setDefaultDropAction( Qt::CopyAction );
	bsaView->setContextMenuPolicy( Qt::CustomContextMenu );
	ui->bsaFilter->setPlaceholderText( tr( "Search available and loaded NIFs..." ) );
	auto * loadBrowserSelection = new QPushButton( tr( "Load Selected" ), ui->frame );
	loadBrowserSelection->setToolTip( tr( "Load every selected NIF as a document" ) );
	ui->horizontalLayout_2->addWidget( loadBrowserSelection );
	connect( loadBrowserSelection, &QPushButton::clicked,
		this, &NifSkope::openNifBrowserSelection );
	auto * refreshBrowser = new QPushButton( tr( "Refresh" ), ui->bsaTitleBar );
	refreshBrowser->setToolTip( tr( "Reload available NIFs from the resource paths configured in Settings" ) );
	nifBrowserArchivesToggle = new QPushButton( tr( "Load Archives" ), ui->bsaTitleBar );
	nifBrowserArchivesToggle->setCheckable( true );
	nifBrowserArchivesToggle->setChecked( true );
	nifBrowserArchivesToggle->setToolTip( tr( "Show NIFs stored in configured BA2/BSA archives" ) );
	nifBrowserLooseToggle = new QPushButton( tr( "Load Loose NIFs" ), ui->bsaTitleBar );
	nifBrowserLooseToggle->setCheckable( true );
	nifBrowserLooseToggle->setChecked( true );
	nifBrowserLooseToggle->setToolTip( tr( "Show loose NIF files from configured mesh folders" ) );
	ui->bsaTitleBar->layout()->addWidget( nifBrowserArchivesToggle );
	ui->bsaTitleBar->layout()->addWidget( nifBrowserLooseToggle );
	ui->bsaTitleBar->layout()->addWidget( refreshBrowser );
	connect( refreshBrowser, &QPushButton::clicked,
		this, &NifSkope::populateConfiguredNifBrowser );
	connect( nifBrowserArchivesToggle, &QPushButton::toggled,
		this, &NifSkope::populateConfiguredNifBrowser );
	connect( nifBrowserLooseToggle, &QPushButton::toggled,
		this, &NifSkope::populateConfiguredNifBrowser );

	bsaModel = new BSAModel( this );
	bsaProxyModel = new BSAProxyModel( this );
	loadedNifsModel = new QStandardItemModel( this );
	loadedNifsModel->setHorizontalHeaderLabels( { tr( "Loaded NIFs" ) } );
	auto * loadedWorkspaceView = new LoadedNifsTreeView( ui->dockWidgetContents_7 );
	loadedNifsView = loadedWorkspaceView;
	loadedNifsView->setObjectName( QStringLiteral( "LoadedNifsView" ) );
	loadedNifsView->setModel( loadedNifsModel );
	loadedNifsView->setItemDelegate( new LoadedNifsDelegate( loadedNifsView ) );
	loadedNifsView->setRootIsDecorated( false );
	loadedNifsView->setAlternatingRowColors( false );
	loadedNifsView->setSelectionMode( QAbstractItemView::ExtendedSelection );
	loadedNifsView->setSelectionBehavior( QAbstractItemView::SelectRows );
	loadedNifsView->setAcceptDrops( true );
	loadedNifsView->setDragDropMode( QAbstractItemView::DropOnly );
	loadedNifsView->setDropIndicatorShown( true );
	loadedNifsView->setVerticalScrollBarPolicy( Qt::ScrollBarAsNeeded );
	loadedNifsView->setHorizontalScrollBarPolicy( Qt::ScrollBarAsNeeded );
	loadedNifsView->setContextMenuPolicy( Qt::CustomContextMenu );
	loadedNifsView->setMinimumHeight( 82 );
	loadedNifsView->header()->setStretchLastSection( true );
	loadedWorkspaceView->sourceView = bsaView;
	loadedWorkspaceView->addBrowserSelection = [this]() { addNifBrowserSelectionToLoaded(); };
	wireLoadedNifsSelection();

	// Available resources and loaded documents are related but distinct. Keep
	// them in resizable upper/lower panes instead of mixing both into one tree.
	ui->verticalLayout_5->removeWidget( bsaView );
	auto * browserSplitter = new QSplitter( Qt::Vertical, ui->dockWidgetContents_7 );
	browserSplitter->setObjectName( QStringLiteral( "NifBrowserSplitter" ) );
	browserSplitter->setChildrenCollapsible( false );
	browserSplitter->addWidget( bsaView );
	browserSplitter->addWidget( loadedNifsView );
	browserSplitter->setStretchFactor( 0, 5 );
	browserSplitter->setStretchFactor( 1, 1 );
	browserSplitter->setSizes( { 420, 120 } );
	ui->verticalLayout_5->addWidget( browserSplitter );
	bsaModel->init();
	bsaProxyModel->setSourceModel( bsaModel );
	bsaView->setModel( bsaProxyModel );
	bsaView->setSortingEnabled( true );
	bsaView->hideColumn( 1 );
	bsaView->setColumnWidth( 0, 300 );
	bsaView->setColumnWidth( 2, 70 );
	bsaProxyModel->sort( 0, Qt::AscendingOrder );
	ui->bsaFilter->setEnabled( true );
	ui->bsaFilenameOnly->setEnabled( true );
	auto * browserFilterTimer = new QTimer( this );
	browserFilterTimer->setSingleShot( true );
	connect( ui->bsaFilter, &QLineEdit::textChanged,
		[browserFilterTimer]() { browserFilterTimer->start( 300 ); } );
	connect( browserFilterTimer, &QTimer::timeout, this, [this]() {
		const QString text = ui->bsaFilter->text();
		bsaProxyModel->setFilterRegularExpression(
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
			QRegularExpression( QRegularExpression::wildcardToRegularExpression( text ).chopped( 2 ).mid( 2 ),
				QRegularExpression::CaseInsensitiveOption )
#else
			QRegularExpression::fromWildcard(
				text, Qt::CaseInsensitive, QRegularExpression::UnanchoredWildcardConversion )
#endif
		);
		bsaView->expandAll();
		if ( text.isEmpty() ) {
			bsaView->collapseAll();
			bsaProxyModel->resetFilter();
			rebuildLoadedNifsBrowserGroup();
		}
	} );
	connect( ui->bsaFilenameOnly, &QCheckBox::toggled,
		bsaProxyModel, &BSAProxyModel::setFilterByNameOnly );
	connect( bsaView, &QTreeView::customContextMenuRequested, this,
		[this]( const QPoint & pos ) {
			const QModelIndex index = bsaView->indexAt( pos );
			if ( NifSkope * document = documentFromBrowserIndex( index ) ) {
				showDocumentMenu( document, bsaView->viewport()->mapToGlobal( pos ) );
				return;
			}
			const QString path = index.sibling( index.row(), 1 ).data( Qt::EditRole ).toString();
			if ( path.isEmpty() ) return;
			// Right-clicking inside a multi-selection acts on the whole selection,
			// like the Load Selected button; right-clicking outside it acts on the
			// row under the cursor only.
			QModelIndexList selectedRows;
			if ( bsaView->selectionModel() )
				selectedRows = bsaView->selectionModel()->selectedRows( 0 );
			const bool useSelection = selectedRows.size() > 1
				&& selectedRows.contains( index.sibling( index.row(), 0 ) );
			QMenu menu( this );
			QAction * open = menu.addAction( tr( "Open NIF" ) );
			QAction * openNew = menu.addAction( tr( "Open NIF in New Window" ) );
			QAction * add = menu.addAction( useSelection
				? tr( "Add %1 Selected to Loaded NIFs" ).arg( selectedRows.size() )
				: tr( "Add to Loaded NIFs" ) );
			QAction * chosen = menu.exec( bsaView->viewport()->mapToGlobal( pos ) );
			if ( chosen == open ) openArchiveFile( index );
			else if ( chosen == openNew ) openArchiveFile( index, true );
			else if ( chosen == add ) {
				if ( useSelection ) addNifBrowserSelectionToLoaded();
				else queueNifBrowserIndexToLoaded( index );
			}
		} );
	connect( loadedNifsView, &QTreeView::doubleClicked, this,
		[this]( const QModelIndex & index ) {
			if ( NifSkope * document = documentFromBrowserIndex( index ) )
				activateDocumentTab( documentTabWindows.indexOf( document ) );
			else if ( BackgroundNifDocument * background = backgroundDocumentFromBrowserIndex( index ) )
				promoteBackgroundDocument( background );
		} );
	connect( loadedNifsView, &QTreeView::customContextMenuRequested, this,
		[this]( const QPoint & pos ) {
			const QModelIndex index = loadedNifsView->indexAt( pos );
			if ( NifSkope * document = documentFromBrowserIndex( index ) )
				showDocumentMenu( document, loadedNifsView->viewport()->mapToGlobal( pos ) );
			else if ( BackgroundNifDocument * background = backgroundDocumentFromBrowserIndex( index ) )
				showBackgroundDocumentMenu( background, loadedNifsView->viewport()->mapToGlobal( pos ) );
		} );

	// Empty Model for swapping out before model fill
	emptyModel = new QStandardItemModel( this );

	// Connect models with views
	/* ********************** */

	connect( list, &NifTreeView::sigCurrentIndexChanged, this, &NifSkope::select );
	connect( this, &NifSkope::currentNifIndexChanged, this,
		[this]( const QModelIndex & index ) {
			updateBlockListNavigation( index );
			applyBlockDetailsFilter();
		} );
	connect( this, &NifSkope::beginLoading, this, [this]() {
		blockListHistory.clear();
		blockListPins.clear();
		blockListHistoryPosition = -1;
		navigatingBlockListHistory = false;
		if ( blockListBack ) blockListBack->setEnabled( false );
		if ( blockListForward ) blockListForward->setEnabled( false );
		if ( blockListBreadcrumb ) blockListBreadcrumb->setText( tr( "Loading..." ) );
		if ( blockListFooter ) blockListFooter->clear();
	} );
	connect( list, &NifTreeView::customContextMenuRequested, this, &NifSkope::contextMenu );
	connect( tree, &NifTreeView::sigCurrentIndexChanged, this, &NifSkope::select );
	connect( tree, &NifTreeView::customContextMenuRequested, this, &NifSkope::contextMenu );
	connect( tree, &NifTreeView::sigCurrentIndexChanged, refrbrwsr, &ReferenceBrowser::browse );
	// Block Details: double-clicking a link field's NAME column jumps to the
	// linked block (the Value column keeps its inline editor for retargeting)
	connect( tree, &NifTreeView::doubleClicked, this, [this]( const QModelIndex & idx ) {
		if ( !nif || !idx.isValid() || idx.model() != nif || idx.column() != 0 )
			return;
		if ( !nif->isLink( idx ) )
			return;
		const int link = nif->getLink( idx );
		if ( nif->isValidBlockNumber( link ) )
			select( nif->getBlockIndex( link ) );
	} );
	connect( header, &NifTreeView::customContextMenuRequested, this, &NifSkope::contextMenu );
	connect( kfmtree, &NifTreeView::customContextMenuRequested, this, &NifSkope::contextMenu );

	// Create GLView
	/* ********************** */

	ogl = new GLView( nullptr );
	ogl->setObjectName( "OGL1" );
	ogl->setNif( nif );

	// Create InspectView
	/* ********************** */

	inspect = new InspectView;
	inspect->setNifModel( nif );
	inspect->setScene( ogl->getScene() );

	// Create Progress Bar
	/* ********************** */
	progress = new QProgressBar( ui->statusbar );
	progress->setMaximumSize( 200, 18 );
	progress->setVisible( false );

	// Process progress events
	connect( nif, &NifModel::sigProgress, [this]( int c, int m ) {
		progress->setRange( 0, m );
		progress->setValue( c );
		qApp->processEvents();
	} );

	/*
	 * UI Init
	 * **********************
	 */

	// Init Scene and View
	graphicsView = ogl->createWindowContainer( this );

	// Set central widget and viewport
	setCentralWidget( graphicsView );

	setContextMenuPolicy( Qt::NoContextMenu );

	// Set Actions
	initActions();

	// Dock Widgets
	initDockWidgets();

	// Toolbars
	initToolBars();

	// Menus
	initMenu();

	// Connections (that are required to load after all other inits)
	initConnections();

	connect( options, &SettingsDialog::saveSettings, this, &NifSkope::updateSettings );
	// Rebindable QAction shortcuts: apply stored overrides now that every
	// action exists, and re-apply after each settings save (the Shortcuts
	// pane writes before this connection runs, so the values are current)
	applyShortcutOverrides();
	connect( options, &SettingsDialog::saveSettings, this, &NifSkope::applyShortcutOverrides );
	if ( !backgroundWorkspaceDocument ) {
		connect( options, &SettingsDialog::saveSettings, this,
			[this]() { QTimer::singleShot( 0, this, &NifSkope::populateConfiguredNifBrowser ); } );
		connect( options, &SettingsDialog::localeChanged, this, &NifSkope::sltLocaleChanged );
		connect( this, &NifSkope::completeLoading, this,
			[this]( bool success, const QString & ) {
				if ( success ) QTimer::singleShot( 0, this, &NifSkope::populateConfiguredNifBrowser );
			} );
		connect( qApp, &QApplication::lastWindowClosed, this, &NifSkope::exitRequested );
	}

	sessionDocumentWindows.append( this );
	initDocumentSession();
	connect( nif->undoStack, &QUndoStack::cleanChanged, this,
		[]( bool ) { NifSkope::refreshAllDocumentSessions(); } );
	refreshAllDocumentSessions();
}

void NifSkope::exitRequested()
{
	if ( applicationEventFilterInstalled ) {
		qApp->removeEventFilter( this );
		applicationEventFilterInstalled = false;
	}
	// Must disconnect from this signal as it's set once for each widget for some reason
	disconnect( qApp, &QApplication::lastWindowClosed, this, &NifSkope::exitRequested );

	if ( options ) {
		delete options;
		options = nullptr;
	}
}

NifSkope::~NifSkope()
{
	// work around crash that would occur if the UV editor is still open and it is the last window
	disconnect( qApp, &QApplication::lastWindowClosed, this, &NifSkope::exitRequested );
	if ( applicationEventFilterInstalled ) {
		qApp->removeEventFilter( this );
		applicationEventFilterInstalled = false;
	}

	sessionDocumentWindows.removeAll( this );
	refreshAllDocumentSessions();
	delete ui;
	if ( currentArchive )
		delete currentArchive;
}

QList<NifSkope *> NifSkope::openDocuments()
{
	QList<NifSkope *> documents;
	for ( NifSkope * document : std::as_const( sessionDocumentWindows ) )
		if ( document ) documents << document;
	return documents;
}

QList<NifSkope *> NifSkope::selectedWorkspaceDocuments()
{
	QList<NifSkope *> documents;
	for ( NifSkope * document : std::as_const( sessionDocumentWindows ) )
		if ( document && document->sessionCollectionMember
			&& document->sessionPreviewVisible && !document->sessionPreviewUnloaded )
			documents << document;
	return documents;
}

QList<QPair<NifModel *, QString>> NifSkope::selectedWorkspaceModels()
{
	QList<QPair<NifModel *, QString>> models;
	for ( NifSkope * document : NifSkope::selectedWorkspaceDocuments() )
		models << qMakePair( document->nif, document->currentFile );
	for ( BackgroundNifDocument * document : std::as_const( sessionBackgroundDocuments ) )
		if ( document && document->selectedInWorkspace() )
			models << qMakePair( document->nif, document->currentFile );
	return models;
}

NifSkope * NifSkope::documentForModel( const NifModel * model )
{
	for ( NifSkope * document : std::as_const( sessionDocumentWindows ) )
		if ( document && document->nif == model )
			return document;
	return nullptr;
}

QString NifSkope::documentDisplayName() const
{
	QString name = QFileInfo( currentFile ).fileName();
	if ( name.isEmpty() )
		name = tr( "Untitled" );
	if ( isWindowModified() || ( nif && nif->undoStack && !nif->undoStack->isClean() ) )
		name += QStringLiteral( " *" );
	return name;
}

void NifSkope::initDocumentSession()
{
	// Keep a non-visual tab model for the existing document switching logic;
	// the actual session UI is an expandable Loaded NIFs category in the NIF
	// Browser tree.
	documentTabs = new QTabBar( this );
	documentTabs->setObjectName( QStringLiteral( "OpenNifDocumentTabs" ) );
	documentTabs->hide();
	connect( documentTabs, &QTabBar::currentChanged, this, &NifSkope::activateDocumentTab );
	connect( documentTabs, &QTabBar::tabCloseRequested, this, [this]( int index ) {
		if ( index < 0 || index >= documentTabWindows.size() ) return;
		NifSkope * document = documentTabWindows.at( index );
		if ( !document ) return;
		if ( document != this && ( document->isWindowModified()
			|| !document->nif->undoStack->isClean() ) ) {
			activateDocumentTab( index );
			QTimer::singleShot( 0, document, &QWidget::close );
		} else document->close();
	} );
}

void NifSkope::rebuildDocumentTabs()
{
	if ( !documentTabs )
		return;
	QSignalBlocker blocker( documentTabs );
	while ( documentTabs->count() > 0 )
		documentTabs->removeTab( documentTabs->count() - 1 );
	documentTabWindows.clear();
	int current = -1;
	for ( NifSkope * document : std::as_const( sessionDocumentWindows ) ) {
		if ( !document ) continue;
		int tab = documentTabs->addTab( document == this
			? tr( "Primary: %1" ).arg( document->documentDisplayName() )
			: document->documentDisplayName() );
		documentTabWindows << document;
		if ( document == this ) current = tab;
		if ( document != this ) {
			documentTabs->setTabIcon( tab, style()->standardIcon(
				document->sessionPreviewVisible && !document->sessionPreviewUnloaded
					? QStyle::SP_DialogApplyButton : QStyle::SP_DialogCancelButton ) );
			documentTabs->setTabToolTip( tab, document->sessionPreviewVisible
				&& !document->sessionPreviewUnloaded
				? tr( "Secondary document visible in the combined viewport" )
				: tr( "Secondary document excluded from the combined viewport" ) );
		} else {
			documentTabs->setTabToolTip( tab, tr( "Primary editable document" ) );
		}
	}
	if ( current >= 0 ) documentTabs->setCurrentIndex( current );
	documentTabs->hide();
	rebuildLoadedNifsBrowserGroup();
}

void NifSkope::rebuildLoadedNifsBrowserGroup()
{
	if ( !loadedNifsModel ) return;
	syncingLoadedNifsSelection = true;
	loadedNifsModel->removeRows( 0, loadedNifsModel->rowCount() );
	for ( NifSkope * document : std::as_const( sessionDocumentWindows ) ) {
		if ( !document ) continue;
		const bool primary = document == this;
		// The primary document always appears in its own Loaded NIFs list, even
		// when it was never explicitly enrolled, so the marked-primary row and
		// the viewport always agree about what is being edited.
		if ( !primary && !document->sessionCollectionMember ) continue;
		const quintptr pointer = reinterpret_cast<quintptr>( document );
		const bool visible = document->sessionPreviewVisible && !document->sessionPreviewUnloaded;
		auto * name = new QStandardItem( document->documentDisplayName() );
		name->setEditable( false );
		name->setData( qulonglong( pointer ), NifBrowserDocumentRole );
		if ( primary ) {
			name->setIcon( style()->standardIcon( QStyle::SP_ArrowRight ) );
			name->setBackground( QColor::fromRgb( 74, 122, 176 ) );
			name->setForeground( QColor::fromRgb( 255, 157, 0 ) );
		} else if ( visible ) {
			name->setBackground( QColor::fromRgb( 43, 66, 95 ) );
			name->setForeground( QColor::fromRgb( 255, 114, 0 ) );
		}
		name->setToolTip( primary ? tr( "Primary editable document" )
			: ( visible
				? tr( "Selected secondary document; visible and available to workspace tools" )
				: tr( "Loaded but unselected; not shown or used by workspace tools" ) ) );
		loadedNifsModel->appendRow( name );
		if ( loadedNifsView && visible && !primary ) {
			const QModelIndex row = loadedNifsModel->index( loadedNifsModel->rowCount() - 1, 0 );
			loadedNifsView->selectionModel()->select( row,
				QItemSelectionModel::Select | QItemSelectionModel::Rows );
		}
	}
	// Data-only background documents share the secondary palette; they can never
	// be the primary row because promotion always goes through a real window.
	for ( BackgroundNifDocument * document : std::as_const( sessionBackgroundDocuments ) ) {
		if ( !document ) continue;
		const bool visible = document->selectedInWorkspace();
		auto * name = new QStandardItem( document->displayName() );
		name->setEditable( false );
		name->setData( qulonglong( reinterpret_cast<quintptr>( document ) ),
			NifBrowserBackgroundDocumentRole );
		if ( visible ) {
			name->setBackground( QColor::fromRgb( 43, 66, 95 ) );
			name->setForeground( QColor::fromRgb( 255, 114, 0 ) );
		}
		name->setToolTip( visible
			? tr( "Selected secondary document; visible and available to workspace tools" )
			: tr( "Loaded but unselected; not shown or used by workspace tools" ) );
		loadedNifsModel->appendRow( name );
		if ( loadedNifsView && visible ) {
			const QModelIndex row = loadedNifsModel->index( loadedNifsModel->rowCount() - 1, 0 );
			loadedNifsView->selectionModel()->select( row,
				QItemSelectionModel::Select | QItemSelectionModel::Rows );
		}
	}
	if ( loadedNifsView ) {
		loadedNifsView->header()->setStretchLastSection( true );
		loadedNifsView->viewport()->update();
	}
	syncingLoadedNifsSelection = false;
}

void NifSkope::wireLoadedNifsSelection()
{
	if ( !loadedNifsView || !loadedNifsView->selectionModel() ) return;
	connect( loadedNifsView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
		[this]( const QItemSelection &, const QItemSelection & ) {
			if ( syncingLoadedNifsSelection ) return;
			QSet<NifSkope *> selected;
			QSet<BackgroundNifDocument *> selectedBackground;
			for ( const QModelIndex & row : loadedNifsView->selectionModel()->selectedRows( 0 ) ) {
				if ( NifSkope * document = documentFromBrowserIndex( row ) ) selected << document;
				if ( BackgroundNifDocument * document = backgroundDocumentFromBrowserIndex( row ) )
					selectedBackground << document;
			}
			for ( NifSkope * document : std::as_const( sessionDocumentWindows ) ) {
				if ( !document || !document->sessionCollectionMember ) continue;
				document->sessionPreviewVisible = ( document == this || selected.contains( document ) );
				document->sessionPreviewUnloaded = false;
			}
			for ( BackgroundNifDocument * document : std::as_const( sessionBackgroundDocuments ) ) {
				if ( !document ) continue;
				document->sessionPreviewVisible = selectedBackground.contains( document );
				document->sessionPreviewUnloaded = false;
			}
			refreshAllDocumentSessions();
		} );
}

void NifSkope::activateDocumentTab( int index )
{
	if ( index < 0 || index >= documentTabWindows.size() )
		return;
	NifSkope * document = documentTabWindows.at( index );
	if ( !document || document == this )
		return;
	if ( document->sessionCollectionMember ) {
		document->sessionPreviewVisible = true;
		document->sessionPreviewUnloaded = false;
	}
	// The NIF Browser is a session-level workflow even though each document
	// retains its own window/model. Lazily mirror the selected archive/game
	// folder into a document the first time it becomes primary.
	if ( !currentArchivePath.isEmpty() && document->currentArchivePath.isEmpty() )
		document->openArchive( currentArchivePath );
	document->setGeometry( geometry() );
	if ( applicationEventFilterInstalled ) {
		qApp->removeEventFilter( this );
		applicationEventFilterInstalled = false;
	}
	if ( !document->applicationEventFilterInstalled ) {
		qApp->installEventFilter( document );
		document->applicationEventFilterInstalled = true;
	}
	if ( isMaximized() ) document->showMaximized();
	else document->showNormal();
	document->raise();
	document->activateWindow();
	hide();
	if ( !document->configuredNifBrowserPopulated )
		QTimer::singleShot( 0, document, &NifSkope::populateConfiguredNifBrowser );
	document->refreshSessionPreview();
	refreshAllDocumentSessions();
}

void NifSkope::showDocumentTabMenu( const QPoint & pos )
{
	if ( !documentTabs ) return;
	int index = documentTabs->tabAt( pos );
	if ( index < 0 || index >= documentTabWindows.size() ) return;
	NifSkope * document = documentTabWindows.at( index );
	if ( !document ) return;
	showDocumentMenu( document, documentTabs->mapToGlobal( pos ) );
}

NifSkope * NifSkope::documentFromBrowserIndex( const QModelIndex & index ) const
{
	if ( !index.isValid() ) return nullptr;
	QModelIndex name = index.sibling( index.row(), 0 );
	const quintptr pointer = quintptr( name.data( NifBrowserDocumentRole ).toULongLong() );
	if ( !pointer ) return nullptr;
	NifSkope * document = reinterpret_cast<NifSkope *>( pointer );
	return sessionDocumentWindows.contains( document ) ? document : nullptr;
}

BackgroundNifDocument * NifSkope::backgroundDocumentFromBrowserIndex( const QModelIndex & index ) const
{
	if ( !index.isValid() ) return nullptr;
	QModelIndex name = index.sibling( index.row(), 0 );
	const quintptr pointer = quintptr( name.data( NifBrowserBackgroundDocumentRole ).toULongLong() );
	if ( !pointer ) return nullptr;
	BackgroundNifDocument * document = reinterpret_cast<BackgroundNifDocument *>( pointer );
	return sessionBackgroundDocuments.contains( document ) ? document : nullptr;
}

void NifSkope::showDocumentMenu( NifSkope * document, const QPoint & globalPos )
{
	// The primary shows in its own list even without being an enrolled member;
	// its menu still offers the whole-workspace and close actions.
	if ( !document || ( document != this && !document->sessionCollectionMember ) ) return;
	const int index = documentTabWindows.indexOf( document );
	if ( index < 0 ) return;
	QMenu menu( this );
	QAction * makePrimary = menu.addAction( tr( "Make Primary / Edit" ) );
	makePrimary->setEnabled( document != this );
	QAction * visible = menu.addAction( tr( "Selected / Visible in Workspace" ) );
	visible->setCheckable( true );
	visible->setChecked( document->sessionPreviewVisible && !document->sessionPreviewUnloaded );
	visible->setEnabled( document != this );
	QAction * isolate = menu.addAction( tr( "Isolate This Secondary with Primary" ) );
	isolate->setEnabled( document != this );
	QAction * showAll = menu.addAction( tr( "Show All Secondary Documents" ) );
	QAction * hideAll = menu.addAction( tr( "Hide All Secondary Documents" ) );
	menu.addSeparator();
	QAction * unload = menu.addAction( tr( "Remove from Loaded NIFs" ) );
	// The primary's automatic row cannot be removed from its own workspace.
	unload->setEnabled( document != this );
	QAction * close = menu.addAction( tr( "Close Document" ) );
	QAction * chosen = menu.exec( globalPos );
	if ( chosen == makePrimary ) activateDocumentTab( index );
	else if ( chosen == visible ) {
		document->sessionPreviewVisible = visible->isChecked();
		document->sessionPreviewUnloaded = false;
		refreshAllDocumentSessions();
	} else if ( chosen == isolate ) {
		for ( NifSkope * other : std::as_const( sessionDocumentWindows ) )
			if ( other && other != this && other->sessionCollectionMember ) {
				other->sessionPreviewVisible = ( other == document );
				if ( other == document ) other->sessionPreviewUnloaded = false;
			}
		for ( BackgroundNifDocument * other : std::as_const( sessionBackgroundDocuments ) )
			if ( other ) other->sessionPreviewVisible = false;
		refreshAllDocumentSessions();
	} else if ( chosen == showAll || chosen == hideAll ) {
		for ( NifSkope * other : std::as_const( sessionDocumentWindows ) )
			if ( other && other != this && other->sessionCollectionMember ) {
				other->sessionPreviewVisible = ( chosen == showAll );
				if ( chosen == showAll ) other->sessionPreviewUnloaded = false;
			}
		for ( BackgroundNifDocument * other : std::as_const( sessionBackgroundDocuments ) )
			if ( other ) {
				other->sessionPreviewVisible = ( chosen == showAll );
				if ( chosen == showAll ) other->sessionPreviewUnloaded = false;
			}
		refreshAllDocumentSessions();
	} else if ( chosen == unload ) {
		document->sessionCollectionMember = false;
		document->sessionPreviewUnloaded = true;
		document->sessionPreviewVisible = false;
		refreshAllDocumentSessions();
	} else if ( chosen == close ) {
		if ( document != this && ( document->isWindowModified()
			|| !document->nif->undoStack->isClean() ) ) {
			activateDocumentTab( index );
			QTimer::singleShot( 0, document, &QWidget::close );
		} else document->close();
	}
}

void NifSkope::showBackgroundDocumentMenu( BackgroundNifDocument * document, const QPoint & globalPos )
{
	if ( !document ) return;
	QMenu menu( this );
	QAction * makePrimary = menu.addAction( tr( "Make Primary / Edit" ) );
	QAction * visible = menu.addAction( tr( "Selected / Visible in Workspace" ) );
	visible->setCheckable( true );
	visible->setChecked( document->selectedInWorkspace() );
	QAction * isolate = menu.addAction( tr( "Isolate This Secondary with Primary" ) );
	QAction * showAll = menu.addAction( tr( "Show All Secondary Documents" ) );
	QAction * hideAll = menu.addAction( tr( "Hide All Secondary Documents" ) );
	menu.addSeparator();
	// A data-only document exists solely as a workspace member, so removing it
	// from the Loaded NIFs list and closing it are the same operation.
	QAction * unload = menu.addAction( tr( "Remove from Loaded NIFs" ) );
	QAction * close = menu.addAction( tr( "Close Document" ) );
	QAction * chosen = menu.exec( globalPos );
	if ( chosen == makePrimary ) promoteBackgroundDocument( document );
	else if ( chosen == visible ) {
		document->sessionPreviewVisible = visible->isChecked();
		document->sessionPreviewUnloaded = false;
		refreshAllDocumentSessions();
	} else if ( chosen == isolate ) {
		for ( NifSkope * other : std::as_const( sessionDocumentWindows ) )
			if ( other && other != this && other->sessionCollectionMember )
				other->sessionPreviewVisible = false;
		for ( BackgroundNifDocument * other : std::as_const( sessionBackgroundDocuments ) )
			if ( other ) {
				other->sessionPreviewVisible = ( other == document );
				if ( other == document ) other->sessionPreviewUnloaded = false;
			}
		refreshAllDocumentSessions();
	} else if ( chosen == showAll || chosen == hideAll ) {
		for ( NifSkope * other : std::as_const( sessionDocumentWindows ) )
			if ( other && other != this && other->sessionCollectionMember ) {
				other->sessionPreviewVisible = ( chosen == showAll );
				if ( chosen == showAll ) other->sessionPreviewUnloaded = false;
			}
		for ( BackgroundNifDocument * other : std::as_const( sessionBackgroundDocuments ) )
			if ( other ) {
				other->sessionPreviewVisible = ( chosen == showAll );
				if ( chosen == showAll ) other->sessionPreviewUnloaded = false;
			}
		refreshAllDocumentSessions();
	} else if ( chosen == unload || chosen == close ) {
		removeBackgroundDocument( document );
	}
}

void NifSkope::promoteBackgroundDocument( BackgroundNifDocument * document )
{
	if ( !document ) return;
	// The window starts as a hidden background window so it cannot flash before
	// its model is ready; activateDocumentTab() performs the visible switch.
	NifSkope * window = NifSkope::createWindow( QString(), true );
	window->sessionCollectionMember = true;
	window->sessionPreviewVisible = false;
	window->sessionPreviewUnloaded = false;
	bool loaded = false;
	QString sourceLabel = document->displayName();
	if ( document->configuredResourceGame >= 0 && !document->configuredResourcePath.isEmpty() ) {
		loaded = loadConfiguredNifIntoDocument( window,
			document->configuredResourceGame, document->configuredResourcePath );
	} else {
		QString fname = document->currentFile;
		emit window->beginLoading();
		loaded = window->nif->loadFromFile( fname );
		if ( loaded ) {
			window->configuredResourceGame = -1;
			window->configuredResourcePath.clear();
			window->setCurrentFile( fname );
		}
		emit window->completeLoading( loaded, fname );
	}
	if ( !loaded ) {
		window->sessionCollectionMember = false;
		window->close();
		statusBar()->showMessage(
			tr( "Could not reload %1 for editing." ).arg( sourceLabel ), 5000 );
		refreshAllDocumentSessions();
		return;
	}
	delete document;
	// The new window replaces the data-only entry; rebuild this window's tab
	// bookkeeping so the promoted document can be activated by index.
	refreshAllDocumentSessions();
	const int index = documentTabWindows.indexOf( window );
	if ( index >= 0 )
		activateDocumentTab( index );
}

void NifSkope::removeBackgroundDocument( BackgroundNifDocument * document )
{
	if ( !document ) return;
	// The destructor detaches the document from the session list.
	delete document;
	refreshAllDocumentSessions();
}

void NifSkope::refreshSessionPreview()
{
	if ( !ogl ) return;
	QVector<Vector3> soup;
	for ( NifSkope * document : std::as_const( sessionDocumentWindows ) ) {
		if ( !document || document == this || !document->sessionCollectionMember
			|| !document->sessionPreviewVisible
			|| document->sessionPreviewUnloaded || document->currentFile.isEmpty() )
			continue;
		soup += sessionDocumentTriangleSoup( document->nif );
	}
	for ( BackgroundNifDocument * document : std::as_const( sessionBackgroundDocuments ) ) {
		if ( !document || !document->selectedInWorkspace() || document->currentFile.isEmpty() )
			continue;
		soup += sessionDocumentTriangleSoup( document->nif );
	}
	if ( soup.isEmpty() ) ogl->clearSessionDocumentPreview();
	else ogl->setSessionDocumentPreview( soup );
}

void NifSkope::refreshAllDocumentSessions()
{
	for ( NifSkope * document : std::as_const( sessionDocumentWindows ) )
		if ( document && ( !document->backgroundWorkspaceDocument || document->isVisible() ) )
			document->rebuildDocumentTabs();
	for ( NifSkope * document : std::as_const( sessionDocumentWindows ) )
		if ( document && document->isVisible() ) document->refreshSessionPreview();
}

void NifSkope::wireBlockListSelection()
{
	// Block-list multi-selection (Blender-style): Shift/Ctrl-click several blocks.
	// Row colours come from NifModel BackgroundRole (active #FFA040 / secondary
	// #FF602A). This must be re-run after every list->setModel() because
	// QAbstractItemView creates a fresh QItemSelectionModel on setModel(),
	// which silently drops this connection.
	connect( list->selectionModel(), &QItemSelectionModel::selectionChanged, this,
		[this]( const QItemSelection &, const QItemSelection & ) {
		// Publish the raw multi-selection (block numbers) so the Copy Branch
		// spell can union every selected block's branch. Done before the
		// object-sync guards below so it stays current even while selection
		// is driven programmatically.
		if ( nif ) {
			QList<qint32> selBlocks;
			for ( const QModelIndex & pidx : list->selectionModel()->selectedIndexes() ) {
				if ( pidx.column() != 0 )
					continue;
				QModelIndex src = ( pidx.model() == proxy ) ? proxy->mapTo( pidx ) : pidx;
				int b = nif->getBlockNumber( src );
				if ( b >= 0 && !selBlocks.contains( b ) )
					selBlocks.append( b );
			}
			setBlockListSelection( selBlocks );
		}
		// 'selecting' is true while NifSkope::select() programmatically drives
		// the list (e.g. after a viewport click); without this guard the
		// resulting single-row ClearAndSelect echoed back into the object
		// selection and collapsed viewport multi-select.
		if ( !nif || !ogl || ogl->editMode || syncingObjToList || selecting )
			return;
		auto toAV = [this]( QModelIndex idx ) {
			if ( idx.model() == proxy )
				idx = proxy->mapTo( idx );
			int b = nif->getBlockNumber( idx );
			while ( b >= 0 && !nif->blockInherits( nif->getBlockIndex( b ), "NiAVObject" ) )
				b = nif->getParent( b );
			return b;
		};
		QSet<int> sel;
		// use selectedIndexes() (column 0) so it works regardless of the view's
		// selection behaviour; selectedRows() can come back empty
		for ( const QModelIndex & pidx : list->selectionModel()->selectedIndexes() ) {
			if ( pidx.column() != 0 )
				continue;
			int b = toAV( pidx );
			if ( b >= 0 )
				sel.insert( b );
		}
		if ( sel.isEmpty() )
			return;	// don't wipe a selection that came from the viewport
		// the list already shows exactly what the user clicked; tell the mirror
		// not to re-drive/re-scroll the list off this change
		updatingObjFromList = true;
		ogl->setObjectSelection( sel, toAV( list->selectionModel()->currentIndex() ) );
		updatingObjFromList = false;
	} );
}

void NifSkope::applyBlockListFilter()
{
	if ( !list || !proxy || !nif || !blockListSearch ) return;
	const QStringList terms = blockListSearch->text().simplified().split(
		QLatin1Char( ' ' ), Qt::SkipEmptyParts );
	auto directMatch = [this, &terms]( const QModelIndex & viewIndex ) {
		QModelIndex source = ( viewIndex.model() == proxy ? proxy->mapTo( viewIndex ) : viewIndex );
		int block = nif->getBlockNumber( source );
		if ( block < 0 || !blockMatchesQuickFilter( block ) ) return false;
		QModelIndex blockIndex = nif->getBlockIndex( block );
		QString searchable = QStringLiteral( "%1 #%1 %2 %3" ).arg( block )
			.arg( nif->itemName( blockIndex ), nif->get<QString>( blockIndex, "Name" ) );
		if ( viewIndex.isValid() ) {
			searchable += QLatin1Char( ' ' ) + viewIndex.data( Qt::DisplayRole ).toString();
			if ( viewIndex.sibling( viewIndex.row(), 1 ).isValid() )
				searchable += QLatin1Char( ' ' ) + viewIndex.sibling( viewIndex.row(), 1 ).data( Qt::DisplayRole ).toString();
		}
		for ( const QString & term : terms )
			if ( !searchable.contains( term, Qt::CaseInsensitive ) ) return false;
		return true;
	};

	if ( list->model() == nif ) {
		for ( int row = 0; row < nif->rowCount(); row++ ) {
			QModelIndex index = nif->index( row, 0 );
			int block = nif->getBlockNumber( index );
			const bool isBlock = block >= 0;
			const bool keep = !isBlock || directMatch( index );
			list->setRowHidden( row, QModelIndex(), !keep );
		}
		return;
	}
	if ( list->model() != proxy ) return;
	auto filterBranch = [&]( auto && self, const QModelIndex & parent ) -> bool {
		bool branchMatches = false;
		for ( int row = 0; row < proxy->rowCount( parent ); row++ ) {
			QModelIndex index = proxy->index( row, 0, parent );
			const bool childMatches = self( self, index );
			const bool rowMatches = directMatch( index );
			const bool keep = rowMatches || childMatches;
			list->setRowHidden( row, parent, !keep );
			if ( !terms.isEmpty() && childMatches ) list->expand( index );
			branchMatches = branchMatches || rowMatches || childMatches;
		}
		return branchMatches;
	};
	filterBranch( filterBranch, QModelIndex() );
}

void NifSkope::applyBlockDetailsFilter()
{
	if ( !tree || !nif || !blockDetailsSearch || tree->model() != nif ) return;
	const QStringList terms = blockDetailsSearch->text().simplified().split(
		QLatin1Char( ' ' ), Qt::SkipEmptyParts );
	// No filter and none to clear: skip the walk. It recurses the whole
	// current block and STRINGIFIES every value column - on a 38k-vertex
	// shape that was ~0.5 s per selection change (and it runs twice per
	// viewport click via the list-mirror echo), which made click-selecting
	// high-poly shapes take seconds.
	if ( terms.isEmpty() && !blockDetailsFilterWasActive )
		return;
	blockDetailsFilterWasActive = !terms.isEmpty();
	auto filterBranch = [&]( auto && self, const QModelIndex & parent ) -> bool {
		bool branchMatches = false;
		for ( int row = 0; row < nif->rowCount( parent ); row++ ) {
			QModelIndex nameIndex = nif->index( row, NifModel::NameCol, parent );
			const bool childMatches = self( self, nameIndex );
			QString searchable = nameIndex.data( Qt::DisplayRole ).toString();
			QModelIndex valueIndex = nif->index( row, NifModel::ValueCol, parent );
			if ( valueIndex.isValid() ) searchable += QLatin1Char( ' ' ) + valueIndex.data( Qt::DisplayRole ).toString();
			bool rowMatches = true;
			for ( const QString & term : terms )
				if ( !searchable.contains( term, Qt::CaseInsensitive ) ) {
					rowMatches = false;
					break;
				}
			const bool keep = terms.isEmpty() || rowMatches || childMatches;
			tree->setRowHidden( row, parent, !keep );
			if ( !terms.isEmpty() && childMatches ) tree->expand( nameIndex );
			branchMatches = branchMatches || rowMatches || childMatches;
		}
		return branchMatches;
	};
	filterBranch( filterBranch, tree->rootIndex() );
}

bool NifSkope::blockMatchesQuickFilter( int block ) const
{
	if ( !nif || block < 0 || blockListQuickFilter == 0 ) return true;
	QModelIndex index = nif->getBlockIndex( block );
	if ( !index.isValid() ) return false;
	const QString type = nif->itemName( index );
	switch ( blockListQuickFilter ) {
	case 1:
		return nif->blockInherits( index, "NiGeometry" )
			|| type.contains( QStringLiteral( "TriShape" ), Qt::CaseInsensitive )
			|| type.contains( QStringLiteral( "Mesh" ), Qt::CaseInsensitive );
	case 2:
		return nif->blockInherits( index, "NiNode" );
	case 3:
		return type.contains( QStringLiteral( "Skin" ), Qt::CaseInsensitive )
			|| type.contains( QStringLiteral( "Bone" ), Qt::CaseInsensitive );
	case 4:
		return nif->blockInherits( index, "NiProperty" )
			|| type.contains( QStringLiteral( "Shader" ), Qt::CaseInsensitive )
			|| type.contains( QStringLiteral( "Material" ), Qt::CaseInsensitive )
			|| type.contains( QStringLiteral( "Texture" ), Qt::CaseInsensitive )
			|| type.contains( QStringLiteral( "Image" ), Qt::CaseInsensitive );
	case 5:
		return type.startsWith( QStringLiteral( "bhk" ) )
			|| type.contains( QStringLiteral( "Collision" ), Qt::CaseInsensitive );
	case 6:
		return nif->blockInherits( index, "NiTimeController" )
			|| type.contains( QStringLiteral( "Controller" ), Qt::CaseInsensitive )
			|| type.contains( QStringLiteral( "Interpolator" ), Qt::CaseInsensitive )
			|| type.contains( QStringLiteral( "Sequence" ), Qt::CaseInsensitive )
			|| type.contains( QStringLiteral( "Keyframe" ), Qt::CaseInsensitive );
	case 7:
		return nif->blockInherits( index, "NiExtraData" )
			|| type.contains( QStringLiteral( "ExtraData" ), Qt::CaseInsensitive );
	default:
		return true;
	}
}

void NifSkope::navigateBlockListHistory( int delta )
{
	if ( !nif || blockListHistory.isEmpty() ) return;
	int next = blockListHistoryPosition + delta;
	while ( next >= 0 && next < blockListHistory.size() ) {
		int block = blockListHistory.at( next );
		if ( nif->getBlockIndex( block ).isValid() ) {
			blockListHistoryPosition = next;
			navigatingBlockListHistory = true;
			select( nif->getBlockIndex( block ) );
			return;
		}
		next += delta;
	}
}

void NifSkope::goToBlock()
{
	if ( !nif || nif->getBlockCount() <= 0 ) return;
	bool accepted = false;
	QString query = QInputDialog::getText( this, tr( "Go to Block" ),
		tr( "Block number, type, or name:" ), QLineEdit::Normal, QString(), &accepted ).trimmed();
	if ( !accepted || query.isEmpty() ) return;
	QString numberText = query;
	if ( numberText.startsWith( QLatin1Char( '#' ) ) ) numberText.remove( 0, 1 );
	bool numberOK = false;
	int block = numberText.toInt( &numberOK );
	if ( !( numberOK && block >= 0 && block < nif->getBlockCount() ) ) {
		block = -1;
		for ( int candidate = 0; candidate < nif->getBlockCount(); candidate++ ) {
			QModelIndex index = nif->getBlockIndex( candidate );
			QString haystack = nif->itemName( index ) + QLatin1Char( ' ' ) + nif->get<QString>( index, "Name" );
			if ( haystack.contains( query, Qt::CaseInsensitive ) ) {
				block = candidate;
				break;
			}
		}
	}
	if ( block < 0 ) {
		ui->statusbar->showMessage( tr( "No block matches \"%1\"." ).arg( query ), 3500 );
		return;
	}
	blockListSearch->clear();
	blockListQuickFilter = 0;
	if ( blockListFilterGroup && blockListFilterGroup->button( 0 ) )
		blockListFilterGroup->button( 0 )->setChecked( true );
	applyBlockListFilter();
	select( nif->getBlockIndex( block ) );
}

void NifSkope::updateBlockListNavigation( const QModelIndex & selection )
{
	if ( !nif ) return;
	QModelIndex index = selection.isValid() ? selection : currentNifIndex();
	int block = nif->getBlockNumber( index );
	if ( block >= 0 ) {
		if ( navigatingBlockListHistory ) {
			navigatingBlockListHistory = false;
		} else if ( blockListHistoryPosition < 0 || blockListHistory.value( blockListHistoryPosition, -1 ) != block ) {
			while ( blockListHistory.size() > blockListHistoryPosition + 1 ) blockListHistory.removeLast();
			blockListHistory.append( block );
			if ( blockListHistory.size() > 64 ) blockListHistory.removeFirst();
			blockListHistoryPosition = blockListHistory.size() - 1;
		}
	}
	if ( blockListBack ) blockListBack->setEnabled( blockListHistoryPosition > 0 );
	if ( blockListForward ) blockListForward->setEnabled(
		blockListHistoryPosition >= 0 && blockListHistoryPosition + 1 < blockListHistory.size() );

	auto blockLabel = [this]( int b ) {
		QModelIndex i = nif->getBlockIndex( b );
		QString name = nif->get<QString>( i, "Name" );
		return name.isEmpty() ? tr( "#%1 %2" ).arg( b ).arg( nif->itemName( i ) )
			: tr( "#%1 %2" ).arg( b ).arg( name );
	};
	if ( blockListBreadcrumb ) {
		QStringList path;
		QSet<int> visited;
		int parent = block;
		while ( parent >= 0 && !visited.contains( parent ) ) {
			visited.insert( parent );
			path.prepend( blockLabel( parent ) );
			parent = nif->getParent( parent );
		}
		blockListBreadcrumb->setText( path.isEmpty() ? tr( "No block selected" ) : path.join( QStringLiteral( "  >  " ) ) );
		blockListBreadcrumb->setToolTip( blockListBreadcrumb->text() );
	}
	if ( blockListPin ) {
		QSignalBlocker blocker( blockListPin );
		blockListPin->setEnabled( block >= 0 );
		blockListPin->setChecked( blockListPins.contains( block ) );
		QMenu * menu = blockListPin->menu();
		menu->clear();
		QList<int> pins = blockListPins.values();
		std::sort( pins.begin(), pins.end() );
		if ( pins.isEmpty() ) menu->addAction( tr( "No pinned blocks" ) )->setEnabled( false );
		for ( int pinned : pins ) {
			if ( !nif->getBlockIndex( pinned ).isValid() ) continue;
			QAction * action = menu->addAction( blockLabel( pinned ) );
			connect( action, &QAction::triggered, this, [this, pinned]() { select( nif->getBlockIndex( pinned ) ); } );
		}
	}
	if ( blockListRelations ) {
		QMenu * menu = blockListRelations->menu();
		menu->clear();
		QList<int> outgoing = ( block >= 0 ? nif->getChildLinks( block ) : QList<int>() );
		QList<int> incoming = ( block >= 0 ? nif->getParentLinks( block ) : QList<int>() );
		std::sort( outgoing.begin(), outgoing.end() );
		std::sort( incoming.begin(), incoming.end() );
		outgoing.erase( std::unique( outgoing.begin(), outgoing.end() ), outgoing.end() );
		incoming.erase( std::unique( incoming.begin(), incoming.end() ), incoming.end() );
		blockListRelations->setEnabled( block >= 0 && !( outgoing.isEmpty() && incoming.isEmpty() ) );
		blockListRelations->setText( tr( "Links %1/%2" ).arg( outgoing.size() ).arg( incoming.size() ) );
		auto addLinks = [this, menu, &blockLabel]( const QString & title, const QList<int> & links ) {
			menu->addSection( title );
			if ( links.isEmpty() ) menu->addAction( tr( "None" ) )->setEnabled( false );
			for ( int linked : links ) {
				if ( !nif->getBlockIndex( linked ).isValid() ) continue;
				QAction * action = menu->addAction( blockLabel( linked ) );
				connect( action, &QAction::triggered, this, [this, linked]() { select( nif->getBlockIndex( linked ) ); } );
			}
		};
		addLinks( tr( "Links to" ), outgoing );
		addLinks( tr( "Referenced by" ), incoming );
	}
	if ( blockListFooter ) {
		qint64 vertices = 0;
		qint64 triangles = 0;
		int shapes = 0;
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex shape = nif->getBlockIndex( b );
			if ( !( nif->blockInherits( shape, "NiGeometry" )
				|| nif->itemName( shape ).contains( QStringLiteral( "TriShape" ), Qt::CaseInsensitive ) ) ) continue;
			shapes++;
			QModelIndex counts = shape;
			if ( !nif->getIndex( counts, "Num Vertices" ).isValid() ) {
				int data = nif->getLink( shape, "Data" );
				if ( data >= 0 ) counts = nif->getBlockIndex( data );
			}
			if ( nif->getIndex( counts, "Num Vertices" ).isValid() ) vertices += nif->get<int>( counts, "Num Vertices" );
			if ( nif->getIndex( counts, "Num Triangles" ).isValid() ) triangles += nif->get<int>( counts, "Num Triangles" );
		}
		blockListFooter->setText( tr( "%1 blocks  ·  %2 shapes  ·  %3 verts  ·  %4 tris" )
			.arg( nif->getBlockCount() ).arg( shapes ).arg( vertices ).arg( triangles ) );
		blockListFooter->setToolTip( tr( "NIF %1 · Bethesda version %2" ).arg( nif->getVersion() ).arg( nif->getBSVersion() ) );
	}
}

void NifSkope::renameBlockListIndex( const QModelIndex & index, bool notifyIfUnavailable )
{
	if ( !nif || !proxy || !index.isValid() || index.model() != proxy ) return;
	// Mouse activation is intentionally limited to the visible object-name
	// column. F2 passes notifyIfUnavailable=true and remains row-oriented.
	if ( !notifyIfUnavailable && index.column() != 1 ) return;
	QModelIndex block = nif->getBlockIndex( proxy->mapTo( index.sibling( index.row(), 0 ) ) );
	const bool renameable = block.isValid() && nif->blockInherits( block, "NiAVObject" )
		&& nif->getIndex( block, "Name" ).isValid();
	if ( !renameable ) {
		if ( notifyIfUnavailable && ui && ui->statusbar )
			ui->statusbar->showMessage( tr( "This block has no unique scene-object Name to rename." ), 3000 );
		return;
	}

	// Blender-style in-place editing: turn the displayed Value cell into a
	// line edit instead of opening a modal input dialog.
	if ( auto * previous = dynamic_cast<BlockListRenameEdit *>(
		list->viewport()->findChild<QLineEdit *>( QStringLiteral( "BlockListRenameEdit" ) ) ) )
		previous->cancel();

	const QModelIndex valueIndex = index.sibling( index.row(), 1 );
	list->scrollTo( valueIndex );
	QRect cell = list->visualRect( valueIndex );
	if ( !cell.isValid() || cell.isEmpty() ) return;

	const int blockNumber = nif->getBlockNumber( block );
	auto * editor = new BlockListRenameEdit( list->viewport() );
	editor->setObjectName( QStringLiteral( "BlockListRenameEdit" ) );
	editor->setGeometry( cell );
	editor->setText( nif->resolveString( block, "Name" ) );
	editor->setToolTip( tr( "Enter accepts; Esc cancels." ) );
	editor->commitRename = [this, blockNumber]( const QString & name ) {
		QModelIndex currentBlock = nif ? nif->getBlockIndex( blockNumber ) : QModelIndex();
		QString error;
		int updatedReferences = 0;
		if ( !renameSceneObjectInline( nif, currentBlock, name, &error, &updatedReferences ) ) {
			QMessageBox::warning( this, tr( "Rename" ), error );
			return false;
		}
		if ( updatedReferences > 0 && ui && ui->statusbar )
			ui->statusbar->showMessage(
				tr( "Renamed node and updated %1 palette/sequence reference(s)." )
					.arg( updatedReferences ), 4000 );
		QTimer::singleShot( 0, this, [this]() { applyBlockListFilter(); } );
		return true;
	};
	editor->show();
	editor->setFocus( Qt::MouseFocusReason );
	editor->selectAll();
}

void NifSkope::swapModels()
{
	// Swap out the models with empty versions while loading the file
	// This is so that the views do not update while loading the file
	if ( tree->model() == nif ) {
		list->setModel( proxyEmpty );
		tree->setModel( nifEmpty );
		header->setModel( nifEmpty );
		kfmtree->setModel( kfmEmpty );
	} else {
		list->setModel( proxy );
		tree->setModel( nif );
		header->setModel( nif );
		kfmtree->setModel( kfm );
	}
	// setModel() on the block list created a new selection model; re-wire.
	wireBlockListSelection();
	QTimer::singleShot( 0, this, [this]() { applyBlockListFilter(); } );
}

void NifSkope::updateSettings()
{
	QSettings settings;

	settings.beginGroup( "Settings" );

	cfg.locale = settings.value( "Locale", "en" ).toLocale();
	cfg.suppressSaveConfirm = settings.value( "UI/Suppress Save Confirmation", false ).toBool();

	settings.endGroup();
}

SettingsDialog * NifSkope::getOptions()
{
	return options;
}



void NifSkope::closeEvent( QCloseEvent * e )
{
	if ( closingWorkspaceGroup ) {
		e->accept();
		return;
	}

	if ( !backgroundWorkspaceDocument || isVisible() ) saveUi();
	if ( !saveConfirm() ) {
		e->ignore();
		return;
	}

	// A visible document is the one user-facing window for its loaded-NIF
	// workspace. Closing it closes every invisible model container in the same
	// group instead of promoting one of those containers into another window.
	if ( isVisible() ) {
		NifSkope * group = workspaceRoot ? workspaceRoot : this;
		QList<NifSkope *> members;
		for ( NifSkope * document : std::as_const( sessionDocumentWindows ) ) {
			if ( !document || document == this ) continue;
			NifSkope * documentGroup = document->workspaceRoot
				? document->workspaceRoot : document;
			if ( documentGroup == group ) members << document;
		}
		// Confirm every potentially edited member before closing any of them, so
		// Cancel leaves the complete workspace intact.
		for ( NifSkope * document : std::as_const( members ) ) {
			if ( !document->saveConfirm() ) {
				e->ignore();
				return;
			}
		}
		for ( NifSkope * document : std::as_const( members ) )
			document->closingWorkspaceGroup = true;
		for ( NifSkope * document : std::as_const( members ) )
			document->close();
		// Data-only members have no window to close and can never hold unsaved
		// edits; delete the ones owned by this workspace outright. Iterate a
		// copy because each destructor detaches itself from the session list.
		const QList<BackgroundNifDocument *> backgroundMembers = sessionBackgroundDocuments;
		for ( BackgroundNifDocument * document : backgroundMembers )
			if ( document && ( !document->workspaceRoot || document->workspaceRoot == group ) )
				delete document;
	}
	e->accept();
}


void NifSkope::castSpell( const QString & id, const QModelIndex & index )
{
	if ( book ) {
		SpellPtr spell = SpellBook::lookup( id );
		if ( spell )
			book->cast( nif, index, spell );
	}
}

void NifSkope::select( const QModelIndex & index )
{
	if ( selecting )
		return;

	QModelIndex idx = index;

	if ( idx.model() == proxy )
		idx = proxy->mapTo( index );

	if ( idx.isValid() && idx.model() != nif )
		return;

	QModelIndex prevIdx = currentIdx;
	currentIdx = idx;

	// TEMP DIAGNOSTIC (WW_PERF_TEST): stage timing for the slow click-select
	QElapsedTimer perfT;
	const bool perfOn = qEnvironmentVariableIsSet( "WW_PERF_TEST" );
	if ( perfOn )
		perfT.start();
	auto perfMark = [&perfT, perfOn]( const char * what ) {
		if ( !perfOn )
			return;
		QFile f( QApplication::applicationDirPath() + "/ww_perf_test.log" );
		if ( f.open( QIODevice::Append | QIODevice::Text ) )
			QTextStream( &f ) << "    [select/" << what << ": " << perfT.restart() << " ms]\n";
	};

	// The persistent menubar SpellBook must follow the same normalized NIF
	// index as the views. Context menus create a fresh SpellBook at the clicked
	// index, but without this update the top Spells menu remains at its initial
	// invalid index and hides selection-specific pages such as Rigging.
	if ( book )
		book->sltIndex( currentIdx );
	perfMark( "book sltIndex" );

	selecting = true;

	// Push to index stack only if there is a sender
	//	Must also come AFTER selecting=true
	//	Both of these things prevent infinite recursion
	if ( sender() && !currentIdx.parent().isValid() ) {
		// Skips index selection in Block Details
		// NOTE: QUndoStack::push() calls the redo() command which calls NifSkope::select()
		//	therefore infinite recursion is possible.
		indexStack->push( new SelectIndexCommand( this, currentIdx, prevIdx ) );
	}

	// TEST: Cast sender to GLView
	//auto s = qobject_cast<GLView *>(sender());
	//if ( s )
	//	qDebug() << sender()->objectName();

	if ( sender() != ogl ) {
		ogl->setCurrentIndex( idx );
	}
	perfMark( "ogl setCurrentIndex" );

	if ( timeline && sender() != timeline )
		timeline->setCurrentIndex( idx );
	perfMark( "timeline setCurrentIndex" );

	// selecting a block from the tree/list/timeline updates the object-mode
	// selection (outline + block-list highlight); viewport clicks handle this
	// themselves. This is the reliable single-block colouring path (the block
	// list's own handler additionally captures multi-selection). Wrapped in
	// updatingObjFromList so the mirror doesn't re-scroll/jump the list.
	if ( ogl && sender() != ogl && idx.isValid() ) {
		int av = nif->getBlockNumber( idx );
		while ( av >= 0 && !nif->blockInherits( nif->getBlockIndex( av ), "NiAVObject" ) )
			av = nif->getParent( av );
		updatingObjFromList = true;
		ogl->syncObjectSelection( av );
		updatingObjFromList = false;
	}
	perfMark( "syncObjectSelection" );

	// Selecting a key on the timeline unfolds it in Block Details
	if ( sender() == timeline && idx.isValid() && idx.parent().isValid() ) {
		QModelIndex p = idx.parent();
		while ( p.isValid() ) {
			tree->expand( p.sibling( p.row(), 0 ) );
			p = p.parent();
		}
		// Unroll the key itself, and only that one: fold its sibling keys
		QModelIndex keyRow = idx.sibling( idx.row(), 0 );
		QModelIndex iKeys = idx.parent();
		if ( nif->rowCount( keyRow ) > 0 ) {
			for ( int r = 0; r < nif->rowCount( iKeys ); r++ ) {
				QModelIndex sib = nif->getIndex( iKeys, r );
				if ( sib.isValid() && sib.row() != keyRow.row() )
					tree->collapse( sib.sibling( sib.row(), 0 ) );
			}
			tree->expand( keyRow );
		}
		tree->scrollTo( idx );
	}

	// Sequences and object palettes open with their main array unfolded
	if ( idx.isValid() && !idx.parent().isValid() ) {
		const char * arrayName = nullptr;
		if ( nif->blockInherits( idx, "NiControllerSequence" ) )
			arrayName = "Controlled Blocks";
		else if ( nif->blockInherits( idx, "NiDefaultAVObjectPalette" ) )
			arrayName = "Objs";

		if ( arrayName ) {
			QModelIndex iArr = nif->getIndex( idx, arrayName );
			if ( iArr.isValid() ) {
				tree->expand( idx.sibling( idx.row(), 0 ) );
				tree->expand( iArr.sibling( iArr.row(), 0 ) );
				for ( int r = 0; r < nif->rowCount( iArr ); r++ )
					tree->expand( nif->getIndex( iArr, r ) );
			}
		}
	}

	if ( sender() == ogl ) {
		if ( dList->isVisible() )
			dList->raise();
	}

	// Switch to Block Details tab if not selecting inside Header tab
	if ( sender() != header ) {
		if ( dTree->isVisible() )
			dTree->raise();
	}

	if ( sender() != list ) {
		if ( list->model() == proxy ) {
			QModelIndex idxProxy = proxy->mapFrom( nif->getBlockIndex( idx ), list->currentIndex() );

			// Fix for NiDefaultAVObjectPalette (et al.) bug
			//	mapFrom() stops at the first result for the given block number,
			//	thus when clicking in the viewport, the actual NiTriShape is not selected
			//	but the reference to it in NiDefaultAVObjectPalette or other non-NiAVObjects.

			// The true parent of the NIF block
			QModelIndex blockParent = nif->index( nif->getParent( idx ) + 1, 0 );
			QModelIndex blockParentProxy = proxy->mapFrom( blockParent, list->currentIndex() );
			QString blockParentString = blockParentProxy.data( Qt::DisplayRole ).toString();

			// The parent string for the proxy result (possibly incorrect)
			QString proxyIdxParentString = idxProxy.parent().data( Qt::DisplayRole ).toString();

			// Determine if proxy result is incorrect
			if ( proxyIdxParentString != blockParentString ) {
				// Find ALL QModelIndex which match the display string
				for ( const QModelIndex & i : list->model()->match( list->model()->index( 0, 0 ), Qt::DisplayRole, idxProxy.data( Qt::DisplayRole ),
					100, Qt::MatchRecursive ) )
				{
					// Skip if child of NiDefaultAVObjectPalette, et al.
					if ( i.parent().data( Qt::DisplayRole ).toString() != blockParentString )
						continue;

					list->setCurrentIndex( i );
				}
			} else {
				// Proxy parent is already an ancestor of NiAVObject
				list->setCurrentIndex( idxProxy );
			}

		} else if ( list->model() == nif ) {
			list->setCurrentIndex( nif->getTopIndex( idx ) );
		}
	}
	perfMark( "list mapping/setCurrentIndex" );

	if ( sender() != tree ) {
		if ( dList->isVisible() ) {
			QModelIndex root = nif->getTopIndex( idx );

			if ( tree->rootIndex() != root )
				tree->setRootIndex( root );
			else
				tree->refreshRowHiding();	// same block: recover a stranded hiding pass

			tree->setCurrentIndex( idx.sibling( idx.row(), 0 ) );

			// Expand BSShaderTextureSet by default
			//if ( root.child( 1, 0 ).data().toString() == "Textures" )
			//	tree->expandAll();

		} else {
			if ( tree->rootIndex() != QModelIndex() )
				tree->setRootIndex( QModelIndex() );

			tree->setCurrentIndex( idx.sibling( idx.row(), 0 ) );
		}
	}
	perfMark( "tree root/current" );

	selecting = false;
	emit currentNifIndexChanged( currentIdx );
	perfMark( "emit currentNifIndexChanged" );
}

void NifSkope::setListMode()
{
	QModelIndex idx = list->currentIndex();
	QAction * a = gListMode->checkedAction();

	if ( !a || a == aList ) {
		if ( list->model() != nif ) {
			// switch to list view
			QHeaderView * head = list->header();
			int s0 = head->sectionSize( head->logicalIndex( 0 ) );
			int s1 = head->sectionSize( head->logicalIndex( 1 ) );
			list->setModel( nif );
			list->setItemsExpandable( false );
			list->setRootIsDecorated( false );
			list->setCurrentIndex( proxy->mapTo( idx ) );
			list->setColumnHidden( NifModel::NameCol, false );
			list->setColumnHidden( NifModel::TypeCol, true );
			list->setColumnHidden( NifModel::ValueCol, false );
			list->setColumnHidden( NifModel::ArgCol, true );
			list->setColumnHidden( NifModel::Arr1Col, true );
			list->setColumnHidden( NifModel::Arr2Col, true );
			list->setColumnHidden( NifModel::CondCol, true );
			list->setColumnHidden( NifModel::Ver1Col, true );
			list->setColumnHidden( NifModel::Ver2Col, true );
			list->setColumnHidden( NifModel::VerCondCol, true );
			head->resizeSection( 0, s0 );
			head->resizeSection( 1, s1 );
		}
	} else {
		if ( list->model() != proxy ) {
			// switch to hierarchy view
			QHeaderView * head = list->header();
			int s0 = head->sectionSize( head->logicalIndex( 0 ) );
			int s1 = head->sectionSize( head->logicalIndex( 1 ) );
			list->setModel( proxy );
			list->setItemsExpandable( true );
			list->setRootIsDecorated( true );
			QModelIndex pidx = proxy->mapFrom( idx, QModelIndex() );
			list->setCurrentIndex( pidx );
			// proxy model has only two columns (see columnCount in nifproxymodel.h)
			list->setColumnHidden( 0, false );
			list->setColumnHidden( 1, false );
			head->resizeSection( 0, s0 );
			head->resizeSection( 1, s1 );
		}
	}
	wireBlockListSelection();
	applyBlockListFilter();
}

// 'Recent Files' Helpers

QString strippedName( const QString & fullFileName )
{
	return QFileInfo( fullFileName ).fileName();
}

int updateRecentActions( QAction * acts[], const QStringList & files )
{
	int numRecentFiles = std::min< qsizetype >( files.size(), NifSkope::NumRecentFiles );

	for ( int i = 0; i < NifSkope::NumRecentFiles; ++i ) {
		QString fileName;
		QString text;
		if ( i >= numRecentFiles ) {
			if ( i > 0 ) {
				acts[i]->setVisible( false );
				continue;
			}
			text = "<None>";
		} else {
			fileName = files[i];
			text = QString( "&%1 %2" ).arg( i + 1 ).arg( strippedName( files[i] ) );
		}
		acts[i]->setText( text );
		acts[i]->setData( fileName );
		acts[i]->setStatusTip( fileName.isEmpty() ? fileName
			: fileName + QObject::tr( "  (right-click: Open in New Window)" ) );
		acts[i]->setVisible( true );
	}
	acts[0]->setEnabled( numRecentFiles > 0 );

	return numRecentFiles;
}

void updateRecentFiles( QStringList & files, const QString & file )
{
	files.removeAll( file );
	files.prepend( file );
	while ( files.size() > NifSkope::NumRecentFiles )
		files.removeLast();
}
// End Helpers


void NifSkope::updateRecentFileActions()
{
	QSettings settings;
	QStringList files = settings.value( "File/Recent File List" ).toStringList();

	::updateRecentActions( recentFileActs, files );
}

void NifSkope::updateAllRecentFileActions()
{
	for ( QWidget * widget : QApplication::topLevelWidgets() ) {
		NifSkope * win = qobject_cast<NifSkope *>(widget);
		if ( win ) {
			win->updateRecentFileActions();
			win->updateRecentArchiveActions();
			win->updateRecentArchiveFileActions();
		}
	}
}

QString NifSkope::getCurrentFile() const
{
	return currentFile;
}

void NifSkope::setCurrentFile( const QString & filename )
{
	currentFile = QDir::fromNativeSeparators( filename );
	if ( QFileInfo( currentFile ).isAbsolute() ) {
		configuredResourceGame = -1;
		configuredResourcePath.clear();
	}

	nif->refreshFileInfo( currentFile );

	setWindowFilePath( currentFile );

	// Avoid adding files opened from BSAs to Recent Files
	QFileInfo file( currentFile );
	if ( !file.exists() && !file.isAbsolute() ) {
		setCurrentArchiveFile( filename );
		return;
	}

	QSettings settings;
	QStringList files = settings.value( "File/Recent File List" ).toStringList();
	::updateRecentFiles( files, currentFile );

	settings.setValue( "File/Recent File List", files );

	updateAllRecentFileActions();
	refreshAllDocumentSessions();
}

void NifSkope::setCurrentArchiveFile( const QString & filepath )
{
	QString bsa = filepath.split( "/" ).first();
	if ( !bsa.endsWith( ".bsa", Qt::CaseInsensitive ) && !bsa.endsWith( ".ba2", Qt::CaseInsensitive ) )
		return;

	// Strip BSA name from beginning of path
	QString path = filepath;
	path.replace( bsa + "/", "" );

	if ( !currentArchiveNames.empty() )
		bsa = currentArchiveNames.back();

	QSettings settings;
	QHash<QString, QVariant> hash = settings.value( "File/Recent Archive Files" ).toHash();

	// Retrieve and update existing Recent Files for BSA
	QStringList filepaths = hash.value( bsa ).toStringList();
	::updateRecentFiles( filepaths, path );

	// Replace BSA's Recent Files
	hash[bsa] = filepaths;

	settings.setValue( "File/Recent Archive Files", hash );

	updateAllRecentFileActions();
}

void NifSkope::clearCurrentFile()
{
	QSettings settings;
	QStringList files = settings.value( "File/Recent File List" ).toStringList();
	files.removeAll( currentFile );
	settings.setValue( "File/Recent File List", files );

	updateAllRecentFileActions();
}

void NifSkope::setCurrentArchive( bool isArchiveFolder )
{
	QString	archiveName( currentArchivePath );
	qsizetype	n = -1;
	if ( isArchiveFolder && archiveName.length() > 5 ) {
		if ( archiveName.endsWith( "/Data", Qt::CaseInsensitive ) || archiveName.endsWith( "\\Data", Qt::CaseInsensitive ) )
			n = -6;
	}
	archiveName = archiveName.mid( archiveName.lastIndexOf( QChar('/'), n ) + 1 );
	if ( isArchiveFolder )
		archiveName += "/*.bsa,*.ba2";
	currentArchiveNames += archiveName;

	{
		QSettings settings;
		QStringList files = settings.value( "File/Recent Archive List" ).toStringList();
		::updateRecentFiles( files, currentArchivePath );

		settings.setValue( "File/Recent Archive List", files );
	}
	updateAllRecentFileActions();
}

void NifSkope::clearCurrentArchive()
{
	if ( !currentArchive )
		return;

	QSettings settings;
	QStringList files = settings.value( "File/Recent Archive List" ).toStringList();

	files.removeAll( currentArchivePath );
	settings.setValue( "File/Recent Archive List", files );

	updateAllRecentFileActions();

	currentArchivePath.clear();
	currentArchiveNames.clear();
	delete currentArchive;
	currentArchive = nullptr;
}

void NifSkope::updateRecentArchiveActions()
{
	QSettings settings;
	QStringList files = settings.value( "File/Recent Archive List" ).toStringList();

	::updateRecentActions( recentArchiveActs, files );
}

void NifSkope::updateRecentArchiveFileActions()
{
	QSettings settings;
	QHash<QString, QVariant> hash = settings.value( "File/Recent Archive Files" ).toHash();

	QStringList files;
	if ( currentArchive && !currentArchiveNames.empty() )
		files = hash.value( currentArchiveNames.back() ).toStringList();

	::updateRecentActions( recentArchiveFileActs, files );
}

QModelIndex NifSkope::currentNifIndex() const
{
	QModelIndex index;
	if ( dList->isVisible() ) {
		if ( list->model() == proxy ) {
			index = proxy->mapTo(list->currentIndex());
		} else if ( list->model() == nif ) {
			index = list->currentIndex();
		}
	} else if ( dTree->isVisible() ) {
		if ( tree->model() == proxy ) {
			index = proxy->mapTo(tree->currentIndex());
		} else if ( tree->model() == nif ) {
			index = tree->currentIndex();
		}
	}
	return index;
}

QByteArray fileChecksum( const QString &fileName, QCryptographicHash::Algorithm hashAlgorithm )
{
	QFile f( fileName );
	if ( f.open( QFile::ReadOnly ) ) {
		QCryptographicHash hash( hashAlgorithm );
		if ( hash.addData( &f ) ) {
			return hash.result();
		}
	}
	return QByteArray();
}

void NifSkope::checkFile( QFileInfo fInfo, QByteArray hash )
{
	QString fname = fInfo.fileName();
	QString fpath = fInfo.filePath();
	QDir::temp().mkdir( "NifSkope" );
	QString tmpDir = QDir::tempPath() + "/NifSkope";
	QDir tmp( tmpDir );
	QString tmpFile = tmpDir + "/" + fInfo.fileName();

	emit beginSave();
	bool saved = nif->saveToFile( tmpFile );
	if ( saved ) {
		auto filehash2 = fileChecksum( tmpFile, QCryptographicHash::Md5 );

		if ( hash == filehash2 ) {
			tmp.remove( fname );
		} else {
			QString err = "An MD5 hash comparison indicates this file will not be 100% identical upon saving. This could indicate underlying issues with the data in this file.";
			Message::warning( this, err, fpath );
#ifdef QT_NO_DEBUG
			tmp.remove( fname );
#endif
		}
	}
	emit completeSave( saved, fpath );
}

static bool archiveFilterFunction( [[maybe_unused]] void * p, const std::string_view & s )
{
	return ( s.ends_with( ".nif" ) || s.ends_with( ".bto" ) || s.ends_with( ".btr" ) );
}

void NifSkope::populateConfiguredNifBrowser()
{
	if ( !bsaModel || !bsaProxyModel || !bsaView || !nif ) return;
	configuredNifBrowserPopulated = true;
	const Game::GameMode game = Game::GameManager::get_game( nif );
	const bool includeArchives = !nifBrowserArchivesToggle || nifBrowserArchivesToggle->isChecked();
	const bool includeLoose = !nifBrowserLooseToggle || nifBrowserLooseToggle->isChecked();

	// Replace only available-source rows. Loaded NIFs are rebuilt below from
	// the live session and therefore remain independent of resource refreshes.

	// Preserve which Available-NIFs folders the user had expanded: the rebuild
	// below replaces every item, which would otherwise snap the tree shut to the
	// root on each refresh / nif load. Folder items are tagged with their
	// accumulated path (NifBrowserFolderPathRole); record the open ones now and
	// re-expand the matching paths once the new tree is in place.
	QSet<QString> expandedFolderPaths;
	if ( bsaView->model() == bsaProxyModel ) {
		std::function<void( QStandardItem * )> collectExpanded = [&]( QStandardItem * item ) {
			for ( int r = 0; r < item->rowCount(); ++r ) {
				QStandardItem * child = item->child( r, 0 );
				if ( !child )
					continue;
				const QVariant folderPath = child->data( NifBrowserFolderPathRole );
				if ( folderPath.isValid() ) {
					const QModelIndex proxyIdx = bsaProxyModel->mapFromSource( child->index() );
					if ( proxyIdx.isValid() && bsaView->isExpanded( proxyIdx ) )
						expandedFolderPaths.insert( folderPath.toString() );
				}
				if ( child->hasChildren() )
					collectExpanded( child );
			}
		};
		collectExpanded( bsaModel->invisibleRootItem() );
	}

	if ( bsaModel->rowCount() > 0 )
		bsaModel->removeRows( 0, bsaModel->rowCount() );
	if ( bsaModel->columnCount() < 3 ) bsaModel->init();

	auto * available = new QStandardItem( tr( "Available NIFs" ) );
	available->setToolTip( tr( "Merged archive and loose files from the configured %1 resource paths" )
		.arg( Game::StringForMode( game ) ) );
	QHash<QString, QStandardItem *> folders;
	folders.insert( QString(), available );

	// The normal renderer resource cache deliberately excludes NIFs for modern
	// Bethesda games. Build a dedicated mesh-only virtual filesystem from the
	// exact same configured paths so archives and loose mesh folders both work.
	if ( currentArchive ) delete currentArchive;
	currentArchive = new BA2File();
	currentArchivePath.clear();
	currentArchiveNames.clear();
	int skippedResourceCount = 0;
	for ( const QString & resourcePath : Game::GameManager::folders( game ) ) {
		if ( resourcePath.isEmpty() ) continue;
		// Resource lists commonly contain dedicated texture and material folders.
		// They cannot contain meshes and BA2File reports them as invalid archive
		// roots, so do not feed them to the NIF-only browser indexer.
		const QFileInfo resourceInfo( resourcePath );
		const QString leafName = resourceInfo.fileName();
		if ( resourceInfo.isDir()
			&& ( leafName.compare( QStringLiteral( "textures" ), Qt::CaseInsensitive ) == 0
				|| leafName.compare( QStringLiteral( "materials" ), Qt::CaseInsensitive ) == 0 ) ) {
			++skippedResourceCount;
			continue;
		}
		try {
#ifdef Q_OS_WIN32
			currentArchive->loadArchivePath(
				resourcePath.toLocal8Bit().constData(), &archiveFilterFunction );
#else
			currentArchive->loadArchivePath(
				resourcePath.toStdString().c_str(), &archiveFilterFunction );
#endif
		} catch ( const std::exception & ) {
			// A bad or non-mesh configured entry is not an application error. In
			// particular, never use qWarning here: the application-wide Qt message
			// handler presents every warning as a modal dialog.
			++skippedResourceCount;
		}
	}
	if ( skippedResourceCount > 0 ) {
		available->setToolTip( available->toolTip() + tr( "\n%1 non-mesh or unreadable resource path(s) skipped." )
			.arg( skippedResourceCount ) );
	}
	std::vector<std::string_view> resourceFiles;
	currentArchive->getFileList( resourceFiles, false, &archiveFilterFunction );
	int fileCount = 0;
	for ( const std::string_view & filePathView : resourceFiles ) {
		const BA2File::FileInfo * fileInfo = currentArchive->findFile( filePathView );
		const bool isLooseFile = fileInfo && fileInfo->archiveType < 0;
		if ( ( isLooseFile && !includeLoose ) || ( !isLooseFile && !includeArchives ) )
			continue;
		QString fullPath = QString::fromUtf8( filePathView.data(), qsizetype( filePathView.size() ) )
			.replace( '\\', '/' );
		if ( !fullPath.startsWith( QStringLiteral( "meshes/" ), Qt::CaseInsensitive ) )
			continue;
		QString relativePath = fullPath.mid( 7 );
		QString folderPath = relativePath.section( '/', 0, -2 );
		QStandardItem * parent = available;
		QString accumulated;
		for ( const QString & part : folderPath.split( '/', Qt::SkipEmptyParts ) ) {
			if ( !accumulated.isEmpty() ) accumulated += QChar( '/' );
			accumulated += part;
			if ( !folders.contains( accumulated ) ) {
				auto * folder = new QStandardItem( part );
				folder->setData( accumulated, NifBrowserFolderPathRole );
				parent->appendRow( { folder, new QStandardItem(), new QStandardItem() } );
				folders.insert( accumulated, folder );
			}
			parent = folders.value( accumulated );
		}

		auto * name = new QStandardItem( relativePath.section( '/', -1 ) );
		name->setToolTip( isLooseFile ? tr( "Loose NIF" ) : tr( "Archive NIF" ) );
		name->setData( NifBrowserConfiguredResource, NifBrowserSourceRole );
		name->setData( int( game ), NifBrowserGameRole );
		auto * path = new QStandardItem( fullPath );
		path->setData( NifBrowserConfiguredResource, NifBrowserSourceRole );
		path->setData( int( game ), NifBrowserGameRole );
		parent->appendRow( { name, path, new QStandardItem() } );
		++fileCount;
	}

	if ( fileCount == 0 ) {
		auto * empty = new QStandardItem( tr( "No configured NIF resources for %1" )
			.arg( Game::StringForMode( game ) ) );
		empty->setEnabled( false );
		available->appendRow( { empty, new QStandardItem(), new QStandardItem() } );
	}
	bsaModel->insertRow( 0, { available, new QStandardItem(), new QStandardItem() } );
	rebuildLoadedNifsBrowserGroup();

	if ( bsaProxyModel->sourceModel() != bsaModel ) bsaProxyModel->setSourceModel( bsaModel );
	if ( bsaView->model() != bsaProxyModel ) bsaView->setModel( bsaProxyModel );
	bsaView->setSortingEnabled( true );
	bsaView->hideColumn( 1 );
	bsaProxyModel->sort( 0, Qt::AscendingOrder );
	ui->bsaName->setText( tr( "%1 configured resources" ).arg( Game::StringForMode( game ) ) );
	QModelIndex visibleAvailable = bsaProxyModel->mapFromSource( available->index() );
	if ( visibleAvailable.isValid() ) bsaView->expand( visibleAvailable );

	// Re-open the folders that were expanded before the rebuild, so loading a nif
	// (or hitting Refresh) leaves the tree where the user had it.
	for ( auto it = folders.constBegin(); it != folders.constEnd(); ++it ) {
		if ( it.key().isEmpty() || !expandedFolderPaths.contains( it.key() ) )
			continue;
		const QModelIndex proxyIdx = bsaProxyModel->mapFromSource( it.value()->index() );
		if ( proxyIdx.isValid() )
			bsaView->expand( proxyIdx );
	}
}

bool NifSkope::extractConfiguredNifBytes( int gameID, const QString & path,
	QByteArray & bytes, QString & displayPath ) const
{
	if ( path.isEmpty() || gameID < int( Game::OTHER )
		|| gameID >= int( Game::NUM_GAMES ) ) return false;
	const Game::GameMode game = Game::GameMode( gameID );
	displayPath = QStringLiteral( "[%1]/%2" )
		.arg( Game::StringForMode( game ), path );
	if ( !currentArchive ) return false;
	const std::string virtualPath = path.toLower().toStdString();
	const BA2File::FileInfo * file = currentArchive->findFile( virtualPath );
	if ( !file ) {
		if ( ui && ui->statusbar ) ui->statusbar->showMessage(
			tr( "Could not load %1 from configured resources." ).arg( path ), 5000 );
		return false;
	}
	BA2File::UCharArray extracted;
	const unsigned char * dataPtr = nullptr;
	const size_t dataSize = currentArchive->extractFile( dataPtr, extracted, virtualPath );
	bytes = QByteArray( reinterpret_cast<const char *>( dataPtr ), qsizetype( dataSize ) );
	return !bytes.isEmpty();
}

bool NifSkope::loadConfiguredNifIntoDocument( NifSkope * target, int gameID, const QString & path )
{
	if ( !target ) return false;
	QByteArray bytes;
	QString displayPath;
	if ( !extractConfiguredNifBytes( gameID, path, bytes, displayPath ) )
		return false;

	QBuffer buffer( &bytes );
	if ( !buffer.open( QIODevice::ReadOnly ) ) return false;
	emit target->beginLoading();
	const bool loaded = target->nif->load( buffer );
	if ( loaded ) {
		target->configuredResourceGame = gameID;
		target->configuredResourcePath = path;
		target->setCurrentFile( displayPath );
	}
	emit target->completeLoading( loaded, displayPath );
	buffer.close();
	refreshAllDocumentSessions();
	return loaded;
}

bool NifSkope::openConfiguredNif( int game, const QString & path, bool newWindow )
{
	// open in place unless a new window was asked for; unsaved changes in
	// the current document prompt Save/Discard/Cancel first
	NifSkope * target = this;
	if ( newWindow )
		target = NifSkope::createWindow();
	else if ( !saveConfirm() )
		return false;
	if ( !loadConfiguredNifIntoDocument( target, game, path ) && target != this )
		target->close();
	return true;
}

bool NifSkope::loadArchivesFromFolder( QString archive )
{
	if ( !( archive.endsWith( "/Data", Qt::CaseInsensitive ) || archive.endsWith( "\\Data", Qt::CaseInsensitive ) ) ) {
		QString	dataDir( archive + "/Data" );
		if ( QFileInfo( dataDir ).isDir() )
			archive = dataDir;
	}

	QStringList	archiveNames( Game::GameManager::get_archive_list( archive ) );
	if ( archiveNames.isEmpty() ) {
		qCWarning( nsIo ) << "The archive folder could not be opened, or no archives were found.";
		return false;
	}

	for ( qsizetype i = 0; i < archiveNames.size(); i++ ) {
		static const char *	excludeFilters[6] = {
			" - faceanimation", " - sounds", " - terrain", " - textures", " - voices", " - wwisesounds"
		};
		bool	isExcluded = false;
		for ( size_t j = 0; j < 6 && !isExcluded; j++ ) {
			if ( archiveNames[i].contains( excludeFilters[j], Qt::CaseInsensitive ) )
				isExcluded = true;
		}
		if ( isExcluded )
			continue;

		QString	fullPath = archive + QChar('/') + archiveNames[i];
		try {
			size_t	prvCnt = currentArchive->getArchiveFileCnt();
			currentArchive->loadArchivePath( fullPath.toStdString().c_str(), &archiveFilterFunction );
			if ( currentArchive->getArchiveFileCnt() > prvCnt )
				currentArchiveNames += archiveNames[i];
		} catch ( std::exception & ) {
			qCWarning( nsIo ) << QString( "The BSA %1 could not be opened." ).arg( fullPath );
		}
	}
	return true;
}

void NifSkope::openArchive( const QString & archive )
{
	// Clear memory from previously opened archives
	bsaModel->clear();
	bsaProxyModel->invalidate();
	bsaProxyModel->setSourceModel( emptyModel );
	bsaView->setModel( emptyModel );
	bsaView->setSortingEnabled( false );

	if ( currentArchive ) {
		delete currentArchive;
		currentArchive = nullptr;
	}
	currentArchivePath = archive;
	currentArchiveNames.clear();

	currentArchive = new BA2File();
	bool	isArchiveFolder = QFileInfo( archive ).isDir();
	if ( !isArchiveFolder ) {
		// load single archive
		try {
			currentArchive->loadArchivePath( archive.toStdString().c_str(), &archiveFilterFunction );
		} catch ( std::exception & ) {
			qCWarning( nsIo ) << "The BSA could not be opened.";
			clearCurrentArchive();
			return;
		}
	} else {
		// load all mesh archives from a folder
		if ( !loadArchivesFromFolder( archive ) ) {
			// A Data folder containing only loose meshes is still a valid NIF
			// Browser source; appendLooseNifsToBrowser() handles it below.
			qCWarning( nsIo ) << "No mesh archives found; checking loose files.";
		}
	}

	{
		setCurrentArchive( isArchiveFolder );

		// Models
		bsaModel->init();

		// Populate the unified NIF Browser from archives and any loose files in
		// the same Data folder.
		bsaModel->fillModel( currentArchive, "meshes" );
		QString looseRoot = archive;
		if ( isArchiveFolder ) {
			if ( !( looseRoot.endsWith( "/Data", Qt::CaseInsensitive )
				|| looseRoot.endsWith( "\\Data", Qt::CaseInsensitive ) ) ) {
				QString dataFolder = QDir( looseRoot ).filePath( QStringLiteral( "Data" ) );
				if ( QFileInfo( dataFolder ).isDir() ) looseRoot = dataFolder;
			}
		} else {
			looseRoot = QFileInfo( archive ).absolutePath();
		}
		appendLooseNifsToBrowser( looseRoot );

		if ( bsaModel->rowCount() == 0 ) {
			qCWarning( nsIo ) << "No archived or loose NIF meshes were found.";
			clearCurrentArchive();
			return;
		}

		// Set proxy and view only after filling source model
		bsaProxyModel->setSourceModel( bsaModel );
		bsaView->setModel( bsaProxyModel );
		bsaView->setSortingEnabled( true );

		bsaView->hideColumn( 1 );
		bsaView->setColumnWidth( 0, 300 );
		bsaView->setColumnWidth( 2, 50 );
		rebuildLoadedNifsBrowserGroup();

		// Sort proxy after model/view is populated
		bsaProxyModel->sort( 0, Qt::AscendingOrder );
#if 0
		// this is not needed because archives are already filtered on load
		bsaProxyModel->setFiletypes( { ".nif", ".bto", ".btr" } );
#endif
		bsaProxyModel->resetFilter();

		// Set filename label
		ui->bsaName->setText( currentArchiveNames.back() );

		ui->bsaFilter->setEnabled( true );
		ui->bsaFilenameOnly->setEnabled( true );

		// Bring tab to front
		dBrowser->raise();

		// Reapply the existing search to the newly populated source.
		ui->bsaFilter->textChanged( ui->bsaFilter->text() );
	}
}

void NifSkope::appendLooseNifsToBrowser( const QString & dataFolder )
{
	if ( !bsaModel || dataFolder.isEmpty() ) return;
	QDir dataDir( dataFolder );
	QString meshesPath = dataDir.filePath( QStringLiteral( "meshes" ) );
	if ( !QFileInfo( meshesPath ).isDir() ) return;

	auto * looseRoot = new QStandardItem( tr( "Loose Files" ) );
	auto * loosePath = new QStandardItem();
	auto * looseSize = new QStandardItem();
	QHash<QString, QStandardItem *> folders;
	folders.insert( QString(), looseRoot );

	QDirIterator it( meshesPath,
		QStringList{ QStringLiteral( "*.nif" ), QStringLiteral( "*.bto" ), QStringLiteral( "*.btr" ) },
		QDir::Files, QDirIterator::Subdirectories );
	int fileCount = 0;
	while ( it.hasNext() ) {
		QString absolutePath = QDir::fromNativeSeparators( it.next() );
		QString relativePath = dataDir.relativeFilePath( absolutePath ).replace( '\\', '/' );
		QString folderPath = relativePath.section( '/', 0, -2 );
		QStandardItem * parent = looseRoot;
		QString accumulated;
		for ( const QString & part : folderPath.split( '/', Qt::SkipEmptyParts ) ) {
			if ( !accumulated.isEmpty() ) accumulated += QChar( '/' );
			accumulated += part;
			if ( !folders.contains( accumulated ) ) {
				auto * folder = new QStandardItem( part );
				parent->appendRow( { folder, new QStandardItem(), new QStandardItem() } );
				folders.insert( accumulated, folder );
			}
			parent = folders.value( accumulated );
		}

		QFileInfo info( absolutePath );
		auto * fileItem = new QStandardItem( info.fileName() );
		fileItem->setData( NifBrowserLooseFile, NifBrowserSourceRole );
		auto * pathItem = new QStandardItem( absolutePath );
		pathItem->setData( NifBrowserLooseFile, NifBrowserSourceRole );
		const qint64 bytes = info.size();
		auto * sizeItem = new QStandardItem( bytes > 1024
			? QString::number( bytes / 1024 ) + QStringLiteral( "KB" )
			: QString::number( bytes ) + QStringLiteral( "B" ) );
		parent->appendRow( { fileItem, pathItem, sizeItem } );
		++fileCount;
	}

	if ( fileCount > 0 )
		bsaModel->appendRow( { looseRoot, loosePath, looseSize } );
	else {
		delete looseRoot;
		delete loosePath;
		delete looseSize;
	}
}

bool NifSkope::openArchiveFile( const QModelIndex & index, bool newWindow )
{
	if ( NifSkope * document = documentFromBrowserIndex( index ) ) {
		activateDocumentTab( documentTabWindows.indexOf( document ) );
		return true;
	}
	QString filepath = index.sibling( index.row(), 1 ).data( Qt::EditRole ).toString();

	if ( filepath.isEmpty() ) return true;
	QModelIndex nameIndex = index.sibling( index.row(), 0 );
	const int source = nameIndex.data( NifBrowserSourceRole ).toInt();
	if ( source == NifBrowserConfiguredResource )
		return openConfiguredNif( nameIndex.data( NifBrowserGameRole ).toInt(), filepath, newWindow );
	if ( source == NifBrowserLooseFile ) {
		if ( newWindow ) {
			NifSkope::createWindow( filepath );
			return true;
		}
		return openFile( filepath );
	}
	return openArchiveFileString( currentArchive, filepath, newWindow );
}

void NifSkope::openNifBrowserSelection()
{
	if ( !bsaView || !bsaView->selectionModel() ) return;
	const QModelIndexList rows = bsaView->selectionModel()->selectedRows( 0 );
	// like a multi-select Open: the first file replaces the current document
	// (unsaved-changes prompt included; a cancel aborts the batch), every
	// additional file gets its own window
	bool first = true;
	for ( const QModelIndex & row : rows ) {
		if ( documentFromBrowserIndex( row )
			|| row.sibling( row.row(), 1 ).data( Qt::EditRole ).toString().isEmpty() )
			continue;
		if ( !openArchiveFile( row, !first ) )
			return;
		first = false;
	}
}

void NifSkope::addNifBrowserIndexToLoaded( const QModelIndex & index )
{
	if ( !index.isValid() ) return;
	const QModelIndex nameIndex = index.sibling( index.row(), 0 );
	const QString path = index.sibling( index.row(), 1 ).data( Qt::EditRole ).toString();
	if ( path.isEmpty() ) return;
	const int source = nameIndex.data( NifBrowserSourceRole ).toInt();
	const int game = nameIndex.data( NifBrowserGameRole ).toInt();

	NifSkope * target = nullptr;
	for ( NifSkope * document : std::as_const( sessionDocumentWindows ) ) {
		if ( !document ) continue;
		if ( source == NifBrowserConfiguredResource
			&& document->configuredResourceGame == game
			&& document->configuredResourcePath.compare( path, Qt::CaseInsensitive ) == 0 ) {
			target = document;
			break;
		}
		if ( source == NifBrowserLooseFile
			&& QFileInfo( document->currentFile ).absoluteFilePath().compare(
				QFileInfo( path ).absoluteFilePath(), Qt::CaseInsensitive ) == 0 ) {
			target = document;
			break;
		}
	}

	if ( target ) {
		const bool wasMember = target->sessionCollectionMember;
		target->sessionCollectionMember = true;
		target->sessionPreviewUnloaded = false;
		if ( !wasMember ) target->sessionPreviewVisible = ( target == this );
		refreshAllDocumentSessions();
		return;
	}

	// Already enrolled as a data-only document?
	for ( BackgroundNifDocument * document : std::as_const( sessionBackgroundDocuments ) ) {
		if ( !document ) continue;
		const bool matchesConfigured = source == NifBrowserConfiguredResource
			&& document->configuredResourceGame == game
			&& document->configuredResourcePath.compare( path, Qt::CaseInsensitive ) == 0;
		const bool matchesLoose = source == NifBrowserLooseFile
			&& document->configuredResourceGame < 0
			&& QFileInfo( document->currentFile ).absoluteFilePath().compare(
				QFileInfo( path ).absoluteFilePath(), Qt::CaseInsensitive ) == 0;
		if ( matchesConfigured || matchesLoose ) {
			document->sessionPreviewUnloaded = false;
			refreshAllDocumentSessions();
			return;
		}
	}

	// New workspace members are parsed into a data-only background document:
	// just a NifModel plus its source identity, with no hidden window, UI, or
	// GL viewport. A real window is only constructed if the user promotes the
	// document to primary.
	auto * document = new BackgroundNifDocument;
	document->workspaceRoot = workspaceRoot ? workspaceRoot : this;
	bool loaded = false;
	if ( source == NifBrowserConfiguredResource ) {
		QByteArray bytes;
		QString displayPath;
		if ( extractConfiguredNifBytes( game, path, bytes, displayPath ) ) {
			QBuffer buffer( &bytes );
			if ( buffer.open( QIODevice::ReadOnly ) ) {
				loaded = document->nif->load( buffer );
				buffer.close();
			}
		}
		if ( loaded ) {
			document->configuredResourceGame = game;
			document->configuredResourcePath = path;
			document->currentFile = displayPath;
		}
	} else if ( source == NifBrowserLooseFile ) {
		loaded = document->nif->loadFromFile( path );
		if ( loaded )
			document->currentFile = path;
	}
	if ( !loaded ) {
		delete document;
		if ( ui && ui->statusbar ) ui->statusbar->showMessage(
			tr( "Could not load %1 into the Loaded NIFs workspace." ).arg( path ), 5000 );
		return;
	}
	sessionBackgroundDocuments.append( document );
	refreshAllDocumentSessions();
}

void NifSkope::addNifBrowserSelectionToLoaded()
{
	if ( !bsaView || !bsaView->selectionModel() ) return;
	const QModelIndexList rows = bsaView->selectionModel()->selectedRows( 0 );
	for ( const QModelIndex & row : rows )
		queueNifBrowserIndexToLoaded( row );
}

void NifSkope::queueNifBrowserIndexToLoaded( const QModelIndex & index )
{
	if ( !index.isValid() ) return;
	const QPersistentModelIndex persistentIndex( index );
	if ( !pendingWorkspaceLoads.contains( persistentIndex ) )
		pendingWorkspaceLoads.append( persistentIndex );
	if ( processingWorkspaceLoad ) return;
	processingWorkspaceLoad = true;
	QTimer::singleShot( 0, this, &NifSkope::processNextNifBrowserLoad );
}

void NifSkope::processNextNifBrowserLoad()
{
	if ( pendingWorkspaceLoads.isEmpty() ) {
		processingWorkspaceLoad = false;
		statusBar()->showMessage( tr( "Finished loading background NIFs" ), 3000 );
		return;
	}

	const QPersistentModelIndex index = pendingWorkspaceLoads.takeFirst();
	statusBar()->showMessage( tr( "Loading background NIFs... %1 remaining" )
		.arg( pendingWorkspaceLoads.size() + 1 ) );
	if ( index.isValid() ) addNifBrowserIndexToLoaded( index );

	// NifModel and the renderer are UI-thread objects. Yield between documents
	// instead of moving them unsafely to a worker thread, so input and painting
	// can be processed between individual file parses.
	QTimer::singleShot( 0, this, &NifSkope::processNextNifBrowserLoad );
}

bool NifSkope::openArchiveFileString( const BA2File * bsa, const QString & filepath, bool newWindow,
	bool confirmReplace )
{
	if ( !bsa || currentArchiveNames.empty() )
		return true;
	std::string	filePathStr( filepath.toLower().toStdString() );
	auto	fd = bsa->findFile( filePathStr );
	if ( !fd )
		return true;

	// Read data from BSA
	BA2File::UCharArray	data;
	const unsigned char *	dataPtr;
	size_t	dataSize = bsa->extractFile( dataPtr, data, filePathStr );
	QByteArray bytes( reinterpret_cast< const char * >( dataPtr ), qsizetype( dataSize ) );
	QBuffer	buf( &bytes );

	// Format like "BSANAME.BSA/path/to/file.nif"
	QString path( currentArchiveNames[std::min( size_t(fd->archiveFile), size_t(currentArchiveNames.size() - 1) )] );
	path = path + "/" + filepath;

	if ( buf.open( QBuffer::ReadOnly ) ) {
		// open in place unless a new window was asked for; unsaved changes
		// in the current document prompt Save/Discard/Cancel first
		NifSkope * target = this;
		if ( newWindow )
			target = NifSkope::createWindow();
		else if ( confirmReplace && !saveConfirm() )
			return false;
		target->configuredResourceGame = -1;
		target->configuredResourcePath.clear();

		emit target->beginLoading();

		bool loaded = target->nif->load( buf );
		if ( loaded )
			target->setCurrentFile( path );

		emit target->completeLoading( loaded, path );

		//if ( loaded ) {
		//	QCryptographicHash hash( QCryptographicHash::Md5 );
		//	hash.addData( data );
		//	filehash = hash.result();
		//
		//	QFileInfo f( path );
		//
		//	checkFile( f, filehash );
		//}

		buf.close();
		refreshAllDocumentSessions();
	}
	return true;
}


bool NifSkope::openFile( QString & file )
{
	// Open in place: the new file replaces the current document, prompting
	// Save/Discard/Cancel first when there are unsaved changes. Right-click
	// a recent entry (or use the NIF browser context menu) for a new window.
	if ( !saveConfirm() )
		return false;
	loadFile( file );
	return true;
}

void NifSkope::openRecentFile()
{
	QAction * action = qobject_cast<QAction *>(sender());
	if ( action ) {
		QString file = action->data().toString();
		openFile( file );
	}
}

void NifSkope::openRecentArchive()
{
	QAction * action = qobject_cast<QAction *>(sender());
	if ( action )
		openArchive( action->data().toString() );
}

void NifSkope::openRecentArchiveFile()
{
	QAction * action = qobject_cast<QAction *>(sender());
	if ( action )
		openArchiveFileString( currentArchive, action->data().toString() );
}


void NifSkope::openFiles( QStringList & files )
{
	if ( files.isEmpty() )
		return;

	// The first file replaces the current document (with the unsaved-changes
	// prompt); every additional selected file still gets its own window.
	// Cancelling the prompt aborts the whole batch.
	QString first = files.takeFirst();
	if ( !first.isEmpty() && !openFile( first ) )
		return;

	for ( const QString & file : files ) {
		NifSkope::createWindow( file );
	}
}

void NifSkope::saveFile( const QString & filename )
{
	configuredResourceGame = -1;
	configuredResourcePath.clear();
	setCurrentFile( filename );
	save();
}

void NifSkope::loadFile( const QString & filename )
{
	QApplication::setOverrideCursor( Qt::WaitCursor );

	configuredResourceGame = -1;
	configuredResourcePath.clear();
	setCurrentFile( filename );
	QTimer::singleShot( 0, this, SLOT( load() ) );
}

void NifSkope::reload()
{
	QTimer::singleShot( 0, this, SLOT( load() ) );
}

void NifSkope::load()
{
	if ( configuredResourceGame >= 0 && !configuredResourcePath.isEmpty() ) {
		loadConfiguredNifIntoDocument(
			this, configuredResourceGame, configuredResourcePath );
		return;
	}
	{
		QString	fname = currentFile.toLower().replace('\\', '/');
		qsizetype	n1 = fname.indexOf(".ba2/");
		qsizetype	n2 = fname.indexOf(".bsa/");
		if ( n1 == qsizetype(-1) )
			n1 = n2;
		if ( n1 != qsizetype(-1) && currentArchive ) {
			fname.remove( 0, n1 + 5 );
			if ( !fname.isEmpty() ) {
				// Reload of an archive-loaded file: re-extract in place with
				// no unsaved-changes prompt (Reload is an explicit discard,
				// like the loose-file reload below)
				openArchiveFileString( currentArchive, fname, false, false );
				return;
			}
		}
	}

	// TEMP DIAGNOSTIC (WW_PERF_TEST): phase timing of the real load path
	QElapsedTimer perfT;
	const bool perfOn = qEnvironmentVariableIsSet( "WW_PERF_TEST" );
	if ( perfOn )
		perfT.start();
	auto perfMark = [&perfT, perfOn]( const char * what ) {
		if ( !perfOn )
			return;
		QFile f( QApplication::applicationDirPath() + "/ww_perf_test.log" );
		if ( f.open( QIODevice::Append | QIODevice::Text ) )
			QTextStream( &f ) << "[load/" << what << ": " << perfT.restart() << " ms]\n";
	};

	emit beginLoading();
	perfMark( "beginLoading consumers" );

	QFileInfo f( QDir::fromNativeSeparators( currentFile ) );
	f.makeAbsolute();

	QString fname = f.filePath();

	// TODO: This is rather poor in terms of file validation

	if ( f.suffix().compare( "kfm", Qt::CaseInsensitive ) == 0 ) {
		emit completeLoading( kfm->loadFromFile( fname ), fname );

		f.setFile( kfm->getFolder(), kfm->get<QString>( kfm->getKFMroot(), "NIF File Name" ) );

		return;
	}

	bool loaded = nif->loadFromFile( fname );
	perfMark( "loadFromFile (views detached)" );

	emit completeLoading( loaded, fname );
	perfMark( "completeLoading consumers" );

	//if ( loaded ) {
	//	filehash = fileChecksum( fname, QCryptographicHash::Md5 );
	//
	//	checkFile( f, filehash );
	//}
}

void NifSkope::save()
{
	// Assure file path is absolute
	// If not absolute, it is loaded from a BSA
	QFileInfo curFile( currentFile );
	if ( !curFile.isAbsolute() ) {
		saveAsDlg();
		return;
	}

	emit beginSave();

	QString fname = currentFile;

	// TODO: This is rather poor in terms of file validation

	if ( fname.endsWith( ".KFM", Qt::CaseInsensitive ) ) {
		emit completeSave( kfm->saveToFile( fname ), fname );
	} else {
		if ( aSanitize->isChecked() ) {
			QModelIndex idx = SpellBook::sanitize( nif );
			if ( idx.isValid() )
				select( idx );
		}

		emit completeSave( nif->saveToFile( fname ), fname );
	}
}


//! Opens website links using the QAction's tooltip text
void NifSkope::openURL()
{
	// Note: This method may appear unused but this slot is
	//	utilized in the nifskope.ui file.

	if ( !sender() )
		return;

	QAction * aURL = qobject_cast<QAction *>( sender() );
	if ( !aURL )
		return;

	// Sender is an action, grab URL from tooltip
	QUrl URL(aURL->toolTip());
	if ( !URL.isValid() )
		return;

	QDesktopServices::openUrl( URL );
}


/*
 *	SelectIndexCommand
 *		Manages cycling between previously selected indices like a browser Back/Forward button
 */

SelectIndexCommand::SelectIndexCommand( NifSkope * wnd, const QModelIndex & cur, const QModelIndex & prev )
{
	nifskope = wnd;

	curIdx = cur;
	prevIdx = prev;
}

void SelectIndexCommand::redo()
{
	nifskope->select( curIdx );
}

void SelectIndexCommand::undo()
{
	nifskope->select( prevIdx );
}


//! Application-wide debug and warning message handler
void NifSkope::MessageOutput( QtMsgType type, const QMessageLogContext & context, const QString & str )
{
	switch ( type ) {
	case QtDebugMsg:
		fprintf( stderr, "[Debug] %s\n", qPrintable( str ) );
		break;
	case QtWarningMsg:
		fprintf( stderr, "[Warning] %s\n", qPrintable( str ) );
		Message::message( qApp->activeWindow(), str, &context, QMessageBox::Warning );
		break;
	case QtCriticalMsg:
		fprintf( stderr, "[Critical] %s\n", qPrintable( str ) );
		Message::message( qApp->activeWindow(), str, &context, QMessageBox::Critical );
		break;
	case QtFatalMsg:
		fprintf( stderr, "[Fatal] %s\n", qPrintable( str ) );
		break;
	case QtInfoMsg:
		fprintf( stderr, "[Info] %s\n", qPrintable( str ) );
		break;
	}
}


static QTranslator * mTranslator = nullptr;

//! Sets application locale and loads translation files
void NifSkope::SetAppLocale( QLocale curLocale )
{
	QDir directory( QApplication::applicationDirPath() );

	if ( !directory.cd( "lang" ) ) {
#ifdef Q_OS_LINUX
	if ( !directory.cd( "/usr/share/nifskope/lang" ) ) {}
#endif
	}

	QString fileName = directory.filePath( "NifSkope_" ) + curLocale.name();

	if ( !QFile::exists( fileName + ".qm" ) )
		fileName = directory.filePath( "NifSkope_" ) + curLocale.name().section( '_', 0, 0 );

	if ( !QFile::exists( fileName + ".qm" ) ) {
		if ( mTranslator ) {
			qApp->removeTranslator( mTranslator );
			delete mTranslator;
			mTranslator = nullptr;
		}
	} else {
		if ( !mTranslator ) {
			mTranslator = new QTranslator();
			qApp->installTranslator( mTranslator );
		}

		(void) mTranslator->load( fileName );
	}

	QLocale::setDefault( QLocale::C );
}

void NifSkope::sltLocaleChanged()
{
	SetAppLocale( cfg.locale );

	QMessageBox mb( QMessageBox::Information, "NifSkope",
	                tr( "NifSkope must be restarted for this setting to take full effect." ),
	                QMessageBox::Ok | QMessageBox::Default, qApp->activeWindow()
	);
	mb.setIconPixmap( QPixmap( ":/res/nifskope.png" ) );
	mb.exec();

	// TODO: Retranslate dynamically
	//ui->retranslateUi( this );
}


void NifSkope::migrateSettings() const
{
	// Load current NifSkope settings
	QSettings settings;
	// Load pre-1.2 NifSkope settings
	QSettings cfg1_1( "NifTools", "NifSkope" );
	// Load NifSkope 1.2 settings
	QSettings cfg1_2( "NifTools", "NifSkope 1.2" );

	// Current version strings
	QString curVer = NIFSKOPE_VERSION;
	QString curQtVer = QT_VERSION_STR;
	QString curDisplayVer = NifSkopeVersion::rawToDisplay( NIFSKOPE_VERSION, true );

	// New Install, no need to migrate anything
	if ( !settings.value( "Version" ).isValid() && !cfg1_1.value( "version" ).isValid() ) {
		// QSettings constructor creates an empty folder, so clear it.
		cfg1_1.clear();

		// Set version values
		settings.setValue( "Version", curVer );
		settings.setValue( "Qt Version", curQtVer );
		settings.setValue( "Display Version", curDisplayVer );

		return;
	}

	QString prevVer = curVer;
	QString prevQtVer = settings.value( "Qt Version" ).toString();
	QString prevDisplayVer = settings.value( "Display Version" ).toString();

	// Set full granularity for version comparisons
	NifSkopeVersion::setNumParts( 7 );

	// Test migration lambda
	//	Note: Sets value of prevVer
	auto testMigration = [&prevVer]( QSettings & migrateFrom, const char * migrateTo ) {
		if ( migrateFrom.value( "version" ).isValid() && !migrateFrom.value( "migrated" ).isValid() ) {
			prevVer = migrateFrom.value( "version" ).toString();

			NifSkopeVersion tmp( prevVer );
			if ( tmp < migrateTo )
				return true;
		}
		return false;
	};

	// Migrate lambda
	//	Using a QHash of registry keys (stored in version.h), migrates from one version to another.
	auto migrate = []( QSettings & migrateFrom, QSettings & migrateTo, const QHash<QString, QString> & migration ) {
		QHash<QString, QString>::const_iterator i;
		for ( i = migration.begin(); i != migration.end(); ++i ) {
			QVariant val = migrateFrom.value( i.key() );

			if ( val.isValid() ) {
				migrateTo.setValue( i.value(), val );
			}
		}

		migrateFrom.setValue( "migrated", true );
	};

	// NOTE: These set `prevVer` and must come before setting `oldVersion`
	bool migrateFrom1_1 = testMigration( cfg1_1, "1.2.0" );
	bool migrateFrom1_2 = testMigration( cfg1_2, "2.0" );

	if ( !migrateFrom1_1 && !migrateFrom1_2 ) {
		prevVer = settings.value( "Version" ).toString();
	}

	NifSkopeVersion oldVersion( prevVer );
	NifSkopeVersion newVersion( curVer );

	// Check NifSkope Version
	//	Assure full granularity here
	NifSkopeVersion::setNumParts( 7 );
	if ( oldVersion != newVersion ) {

		// Migrate from 1.1.x to 1.2
		if ( migrateFrom1_1 ) {
			qDebug() << "Migrating from 1.1 to 1.2";
			migrate( cfg1_1, cfg1_2, migrateTo1_2 );
		}

		// Migrate from 1.2.x to 2.0
		if ( migrateFrom1_2 ) {
			qDebug() << "Migrating from 1.2 to 2.0";
			migrate( cfg1_2, settings, migrateTo2_0 );
		}

		// Set new Version
		settings.setValue( "Version", curVer );

		if ( prevDisplayVer != curDisplayVer )
			settings.setValue( "Display Version", curDisplayVer );

		// Migrate to new Settings
		if ( oldVersion <= NifSkopeVersion( "2.0.dev1" ) ) {
			qDebug() << "Migrating to new Settings";

			// Remove old keys

			settings.remove( "FSEngine" );
			settings.remove( "Render Settings" );
			settings.remove( "Settings/Language" );
			settings.remove( "Settings/Startup Version" );
		}
	}

#ifdef QT_NO_DEBUG
	// Check Qt Version
	if ( curQtVer != prevQtVer ) {
		// Check all keys and delete all QByteArrays
		// to prevent portability problems between Qt versions
		QStringList keys = settings.allKeys();

		for ( const auto& key : keys ) {
			if ( getQVariantMetaType( settings.value( key ) ) == QMetaType::QByteArray ) {
				qDebug() << "Removing Qt version-specific settings" << key
					<< "while migrating settings from previous version";
				settings.remove( key );
			}
		}

		settings.setValue( "Qt Version", curQtVer );
	}
#endif
}

bool NifSkope::batchProcessFiles(
	const QStringList & fileList, bool (*processFunc)( NifModel *, void * ), void * processFuncData )
{
	qsizetype	n = fileList.size();
	if ( n < 1 || !processFunc )
		return true;

	QDialog	dlg;
	QLabel *	lb = new QLabel( &dlg );
	lb->setText( QString( "Processing file %1..." ).arg( fileList.first() ) );
	QProgressBar *	pb = new QProgressBar( &dlg );
	pb->setMinimum( 0 );
	pb->setMaximum( int( n ) );
	QPushButton *	cb = new QPushButton( "Cancel", &dlg );
	QGridLayout *	grid = new QGridLayout;
	dlg.setLayout( grid );
	grid->addWidget( lb, 0, 0, 1, 3 );
	grid->addWidget( pb, 1, 0, 1, 3 );
	grid->addWidget( cb, 2, 1, 1, 1 );
	QObject::connect( cb, &QPushButton::clicked, &dlg, &QDialog::reject );
	dlg.setModal( true );
	dlg.setResult( QDialog::Accepted );
	dlg.show();

	NifModel *	tmpNif = nullptr;
	bool	noErrors = true;
	for ( qsizetype i = 0; i < n; i++ ) {
		const QString &	filePath = fileList[i];
		lb->setText( QString( "Processing file %1..." ).arg( filePath ) );
		try {
			QCoreApplication::processEvents();
			if ( dlg.result() == QDialog::Rejected )
				return false;

			QString	fileName( QDir::fromNativeSeparators( filePath ) );
			tmpNif = new NifModel();
			tmpNif->setBatchProcessingMode( true );
			{
				QFile	f( fileName );
				if ( !f.open( QIODevice::ReadOnly ) )
					throw NifSkopeError( "error opening file" );
				std::string	tmp( fileName.toStdString() );
				tmpNif->load( f, tmp.c_str() );
			}
			if ( !tmpNif->getMessages().isEmpty() )
				throw NifSkopeError( "error parsing NIF data" );

			QCoreApplication::processEvents();
			if ( dlg.result() == QDialog::Rejected )
				return false;

			bool	saveFlag = processFunc( tmpNif, processFuncData );
			tmpNif->setBatchProcessingMode( false );

			QCoreApplication::processEvents();
			if ( dlg.result() == QDialog::Rejected )
				return false;

			if ( saveFlag ) {
				QFile	f( fileName );
				if ( !f.open( QIODevice::WriteOnly ) )
					throw NifSkopeError( "error opening file" );
				tmpNif->save( f );
			}

			delete tmpNif;
			tmpNif = nullptr;
		} catch ( std::exception & e ) {
			if ( tmpNif ) {
				delete tmpNif;
				tmpNif = nullptr;
			}
			if ( QMessageBox::critical( this, "NifSkope error",
										QString( "Error processing '%1': %2. Continue?" ).arg( filePath ).arg( e.what() ),
										QMessageBox::Yes | QMessageBox::No ) != QMessageBox::Yes ) {
				return false;
			}
			noErrors = false;
		}
		pb->setValue( int( i + 1 ) );
	}

	return noErrors;
}
