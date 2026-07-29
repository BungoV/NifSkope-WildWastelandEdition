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

#include "freezeanim.h"
#include "glview.h"
#include "loadingscreen.h"
#include "message.h"
#include "nifmerge.h"
#include "nifsnapshot.h"
#include "data/niftypes.h"
#include "spellbook.h"
#include "spells/animationsetup.h"
#include "spells/blocks.h"	// setBlockListSelection for Copy Branch multi-select
#include "version.h"
#include "wwskin.h"
#include "gl/glscene.h"
#include "model/kfmmodel.h"
#include "model/nifmodel.h"
#include "model/nifproxymodel.h"
#include "model/undocommands.h"
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
#include <QFileDialog>
#include <QButtonGroup>
#include <QByteArray>
#include <QCloseEvent>
#include <QDebug>
#include <QDesktopServices>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QSortFilterProxyModel>
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
#include <QMouseEvent>
#include <QPainter>
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
#include <QScrollBar>
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

/*! Loaded NIFs row: the name, plus two display toggles at the right edge.
 *
 * Solid and ghost are drawn as two small buttons rather than one cycling
 * control, because the question "is this one solid or see-through" should be
 * answerable at a glance down a list of six limbs, and a tri-state checkbox
 * makes you click to find out. Clicking the lit one turns the document off, so
 * two buttons still cover all three states.
 */
class LoadedNifsDelegate final : public QStyledItemDelegate
{
public:
	explicit LoadedNifsDelegate( QObject * parent = nullptr ) : QStyledItemDelegate( parent ) {}

	//! 0 = hidden, 1 = solid, 2 = ghost; -1 when the row is not a document.
	std::function<int( const QModelIndex & )> displayMode;
	//! Called with the mode the user clicked; the same mode again means "off".
	std::function<void( const QModelIndex &, int )> setDisplayMode;

	static constexpr int GlyphW = 20;

	//! Right-edge slots: [0] solid, [1] ghost.
	static QRect glyphRect( const QRect & row, int slot )
	{
		const int size = std::min( GlyphW, row.height() - 2 );
		const int top = row.top() + ( row.height() - size ) / 2;
		return QRect( row.right() - ( 2 - slot ) * ( GlyphW + 2 ) - 2, top, size, size );
	}

	void paint( QPainter * painter, const QStyleOptionViewItem & option,
		const QModelIndex & index ) const override
	{
		QStyleOptionViewItem opt( option );
		initStyleOption( &opt, index );
		const int mode = displayMode ? displayMode( index ) : -1;
		if ( mode >= 0 )		// keep the text clear of the buttons
			opt.rect.setRight( opt.rect.right() - 2 * ( GlyphW + 2 ) - 4 );
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
		if ( mode < 0 )
			return;

		// filled disc = solid, half disc = see-through; lit when that mode is on
		painter->save();
		painter->setRenderHint( QPainter::Antialiasing, true );
		for ( int slot = 0; slot < 2; slot++ ) {
			const QRect r = glyphRect( option.rect, slot );
			const bool on = ( mode == slot + 1 );
			const QColor ink = on ? QColor( 0xFF, 0x9D, 0x00 ) : QColor( 0x77, 0x7c, 0x84 );
			painter->setPen( QPen( ink, 1 ) );
			painter->setBrush( Qt::NoBrush );
			painter->drawEllipse( r.adjusted( 3, 3, -3, -3 ) );
			QRectF fill = QRectF( r.adjusted( 3, 3, -3, -3 ) );
			painter->setPen( Qt::NoPen );
			painter->setBrush( ink );
			if ( slot == 0 )
				painter->drawEllipse( fill );
			else
				painter->drawChord( fill, 90 * 16, 180 * 16 );	// left half
		}
		painter->restore();
	}

	bool editorEvent( QEvent * event, QAbstractItemModel *, const QStyleOptionViewItem & option,
		const QModelIndex & index ) override
	{
		if ( event->type() != QEvent::MouseButtonRelease || !setDisplayMode
			|| !displayMode || displayMode( index ) < 0 )
			return false;
		auto * me = static_cast<QMouseEvent *>( event );
		if ( me->button() != Qt::LeftButton )
			return false;
		for ( int slot = 0; slot < 2; slot++ ) {
			if ( !glyphRect( option.rect, slot ).contains( me->pos() ) )
				continue;
			// clicking the mode already on turns the document off
			const int want = ( displayMode( index ) == slot + 1 ) ? 0 : slot + 1;
			setDisplayMode( index, want );
			return true;
		}
		return false;
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
		/* A background document used to be read-only by assumption — no editing
		 * UI, therefore never dirty. The workspace breaks that: posing, syncing
		 * and freezing edit every loaded document, and NifModel::undoStack was
		 * created in exactly one place, NifSkope's constructor, so these had no
		 * undo at all. One each, owned here since this is not a QObject.
		 */
		nif->undoStack = new QUndoStack;
	}

	~BackgroundNifDocument()
	{
		sessionBackgroundDocuments.removeAll( this );
		delete nif->undoStack;
		delete nif;
	}

	//! Remember the bytes as loaded, so there is always a way back that does not
	//! depend on the source still being reachable, or on every edit having had a
	//! command behind it.
	void captureLoadedState()
	{
		pristine.clear();
		QBuffer buf( &pristine );
		if ( buf.open( QIODevice::WriteOnly ) )
			nif->save( buf );
	}

	/*! Put the document back exactly as it was loaded.
	 *
	 * Re-parsing the pristine bytes rather than unwinding the undo stack, because
	 * the two are not equivalent: an edit made without a command has no undo, and
	 * re-entering a value by hand does not restore the file — the header string
	 * table only grows, so a name set and set back leaves an orphan behind and
	 * the bytes differ (measured: 765798 against 765774). Re-parsing cannot have
	 * that problem.
	 */
	bool revertToLoaded()
	{
		if ( pristine.isEmpty() )
			return false;
		QBuffer buf( &pristine );
		if ( !buf.open( QIODevice::ReadOnly ) )
			return false;
		const bool ok = nif->load( buf );
		nif->undoStack->clear();
		return ok;
	}

	//! true when the model no longer matches the bytes it was loaded from
	bool isModified() const
	{
		if ( pristine.isEmpty() )
			return false;
		QByteArray now;
		QBuffer buf( &now );
		if ( !buf.open( QIODevice::WriteOnly ) || !nif->save( buf ) )
			return false;
		return now != pristine;
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
	//! The file exactly as parsed, for revertToLoaded().
	QByteArray pristine;
	//! Display path: an absolute loose-file path or a "[Game]/meshes/..." label.
	QString currentFile;
	int configuredResourceGame = -1;
	QString configuredResourcePath;
	//! The visible window whose workspace this document belongs to.
	NifSkope * workspaceRoot = nullptr;
	bool sessionPreviewVisible = false;
	bool sessionPreviewUnloaded = false;
	//! Draw this one translucent — visible for placing things against, without
	//! competing with whatever is being worked on.
	bool sessionPreviewGhost = false;
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
	blockListRelations->setText( tr( "0/0" ) );
	blockListRelations->setToolTip( tr( "Links: outgoing links / blocks that reference the selection" ) );
	blockListRelations->setPopupMode( QToolButton::InstantPopup );
	blockListRelations->setMenu( new QMenu( blockListRelations ) );
	blockNavigationLayout->addWidget( blockListRelations );
	ui->verticalLayout_2->removeWidget( ui->listButtonFrame );
	blockNavigationLayout->addWidget( ui->listButtonFrame );
	ui->verticalLayout_2->insertWidget( 0, blockNavigation );
	// The Block List dock must fold down to a sliver when the user wants the
	// viewport: none of the header rows may impose a minimum width. The search
	// field compresses first (it is the stretch item); past its minimum the
	// rows simply clip from the right.
	blockListSearch->setMinimumWidth( 48 );
	blockNavigation->setMinimumWidth( 1 );

	/* One dropdown in the search row, not eight chips in a row of their own.
	 *
	 * The chips were already mutually exclusive, which is exactly what a menu
	 * expresses, and the row they occupied was the third strip of chrome above the
	 * list - search, chips, breadcrumb - before a single block was visible. The
	 * button wears the active category's icon, so the current filter stays
	 * readable at a glance without spending a row on it.
	 */
	struct BlockFilterDef { int id; QString name; QString icon; };
	const QList<BlockFilterDef> blockFilterDefs = {
		{ 0, tr( "All blocks" ), QString() },
		{ 1, tr( "Geometry" ), QStringLiteral( ":/btn/blockGeometry" ) },
		// icon supplied below by wwNodeCategoryIcon(), so the chip and the tree
		// row show the same mark for this category
		{ 2, tr( "Scene nodes" ), QString() },
		{ 3, tr( "Skinning and bones" ), QStringLiteral( ":/btn/skinned" ) },
		{ 4, tr( "Materials, shaders, and textures" ), QStringLiteral( ":/btn/blockMaterial" ) },
		{ 5, tr( "Collision" ), QStringLiteral( ":/btn/showCollision" ) },
		{ 6, tr( "Animation and controllers" ), QStringLiteral( ":/btn/blockAnimation" ) },
		{ 7, tr( "Extra data" ), QStringLiteral( ":/btn/blockExtraData" ) }
	};
	blockListFilterButton = new QToolButton( blockNavigation );
	blockListFilterButton->setPopupMode( QToolButton::InstantPopup );
	blockListFilterButton->setAutoRaise( true );
	blockListFilterMenu = new QMenu( blockListFilterButton );
	blockListFilterButton->setMenu( blockListFilterMenu );
	QActionGroup * blockFilterActions = new QActionGroup( blockListFilterMenu );
	blockFilterActions->setExclusive( true );
	for ( const BlockFilterDef & def : blockFilterDefs ) {
		QAction * a = blockListFilterMenu->addAction( def.name );
		a->setCheckable( true );
		a->setChecked( def.id == 0 );
		a->setData( def.id );
		if ( def.id == 2 ) a->setIcon( wwNodeCategoryIcon() );
		else if ( !def.icon.isEmpty() ) a->setIcon( QIcon( def.icon ) );
		blockFilterActions->addAction( a );
	}
	connect( blockFilterActions, &QActionGroup::triggered, this,
		[this]( QAction * a ) { setBlockListQuickFilter( a->data().toInt() ); } );
	blockNavigationLayout->addWidget( blockListFilterButton );
	setBlockListQuickFilter( 0 );
	// the labels can be arbitrarily long — allow them to clip so the dock folds
	blockListBreadcrumb = new QLabel( ui->dockWidgetContents_4 );
	blockListBreadcrumb->setTextInteractionFlags( Qt::TextSelectableByMouse );
	blockListBreadcrumb->setStyleSheet( QStringLiteral( "color: %1; padding: 1px 2px;" ).arg( wwSkinColor( "textMuted" ) ) );
	blockListBreadcrumb->setToolTip( tr( "Scene-parent path for the selected block" ) );
	blockListBreadcrumb->setMinimumWidth( 1 );
	ui->verticalLayout_2->insertWidget( 2, blockListBreadcrumb );
	blockListFooter = new QLabel( ui->dockWidgetContents_4 );
	blockListFooter->setStyleSheet( QStringLiteral( "color: %1; padding: 2px;" ).arg( wwSkinColor( "textMuted" ) ) );
	blockListFooter->setMinimumWidth( 1 );
	ui->verticalLayout_2->addWidget( blockListFooter );
	connect( blockListSearch, &QLineEdit::textChanged, this, [this]() { applyBlockListFilter(); } );
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
	// One deferred filter run per event-loop turn, no matter how many model
	// signals arrive — a block insert/remove burst used to run the whole
	// filter walk once per signal, immediately.
	auto blockFilterPending = std::make_shared<bool>( false );
	auto scheduleBlockFilter = [this, blockFilterPending]() {
		if ( *blockFilterPending )
			return;
		*blockFilterPending = true;
		QTimer::singleShot( 0, this, [this, blockFilterPending]() {
			*blockFilterPending = false;
			applyBlockListFilter();
			updateBlockListNavigation( currentNifIndex() );
		} );
	};
	connect( proxy, &QAbstractItemModel::modelReset, this, scheduleBlockFilter );
	connect( proxy, &QAbstractItemModel::layoutChanged, this, scheduleBlockFilter );
	connect( proxy, &QAbstractItemModel::rowsInserted, this,
		[scheduleBlockFilter]( const QModelIndex &, int, int ) { scheduleBlockFilter(); } );
	connect( proxy, &QAbstractItemModel::rowsRemoved, this,
		[scheduleBlockFilter]( const QModelIndex &, int, int ) { scheduleBlockFilter(); } );
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
	// the Reference column only appears while a diff reference is set
	tree->setColumnHidden( NifModel::WwRefCol, true );
	// Summary is a Block LIST column; Block Details shows fields, not blocks
	tree->setColumnHidden( NifModel::WwSummaryCol, true );
	/* Type is off by default, and toggled from the header's context menu.
	 *
	 * It is a permanent ~90px of width showing things like Ref<NiTimeController>,
	 * which matters when you are authoring structure and not when you are editing
	 * values - the common case by a wide margin. Off by default gives Value the
	 * room; the header right-click is where Qt users already look for column
	 * visibility, so it costs no chrome to offer it.
	 */
	{
		QSettings cfg;
		tree->setColumnHidden( NifModel::TypeCol,
			!cfg.value( QStringLiteral( "BlockDetails/Show Type Column" ), false ).toBool() );
		tree->header()->setContextMenuPolicy( Qt::CustomContextMenu );
		connect( tree->header(), &QWidget::customContextMenuRequested, this,
			[this]( const QPoint & pos ) {
				QMenu m( this );
				QAction * a = m.addAction( tr( "Type Column" ) );
				a->setCheckable( true );
				a->setChecked( !tree->isColumnHidden( NifModel::TypeCol ) );
				if ( m.exec( tree->header()->mapToGlobal( pos ) ) != a )
					return;
				const bool show = !a->isChecked();
				tree->setColumnHidden( NifModel::TypeCol, !show );
				QSettings().setValue( QStringLiteral( "BlockDetails/Show Type Column" ), show );
			} );
	}
	blockDetailsSearch = new QLineEdit( ui->dockWidgetContents_2 );
	blockDetailsSearch->setClearButtonEnabled( true );
	blockDetailsSearch->setPlaceholderText( tr( "Filter fields by name or value..." ) );
	blockDetailsSearch->setToolTip( tr( "Parents remain visible when a nested field matches. Ctrl+Shift+F focuses this field." ) );
	// filter row: search field + the pinned-only toggle, flat and auto-raise
	// like the Block List header buttons
	{
		auto * filterRow = new QWidget( ui->dockWidgetContents_2 );
		auto * fl = new QHBoxLayout( filterRow );
		fl->setContentsMargins( 0, 0, 0, 0 );
		fl->setSpacing( 2 );
		fl->addWidget( blockDetailsSearch, 1 );

		wwLoadPinnedFields();
		blockDetailsPinFilter = new QToolButton( filterRow );
		blockDetailsPinFilter->setText( QString::fromUtf8( "\xe2\x98\x85" ) );	// ★
		blockDetailsPinFilter->setCheckable( true );
		blockDetailsPinFilter->setAutoRaise( true );
		blockDetailsPinFilter->setToolTip(
			tr( "Show only pinned fields.\nRight-click any field to pin it; pins are remembered per block type." ) );
		fl->addWidget( blockDetailsPinFilter, 0 );
		ui->verticalLayout->insertWidget( 0, filterRow );
		connect( blockDetailsPinFilter, &QToolButton::toggled, this, [this]( bool on ) {
			wwPinnedOnly = on;
			applyBlockDetailsFilter();
		} );
	}
	connect( blockDetailsSearch, &QLineEdit::textChanged, this, [this]() { applyBlockDetailsFilter(); } );
	// diff-vs-reference banner: one flat grey line above the filter, hidden
	// until a reference block is set (Block List right-click)
	wwDiffBanner = new QWidget( ui->dockWidgetContents_2 );
	{
		auto * bannerLayout = new QHBoxLayout( wwDiffBanner );
		bannerLayout->setContentsMargins( 4, 2, 2, 2 );
		bannerLayout->setSpacing( 4 );
		wwDiffLabel = new QLabel( wwDiffBanner );
		wwDiffLabel->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "textMuted" ) ) );
		bannerLayout->addWidget( wwDiffLabel, 1 );
		auto * clearDiffBtn = new QToolButton( wwDiffBanner );
		clearDiffBtn->setText( QStringLiteral( "✕" ) );
		clearDiffBtn->setAutoRaise( true );
		clearDiffBtn->setToolTip( tr( "Stop diffing against the reference block" ) );
		connect( clearDiffBtn, &QToolButton::clicked, this, [this]() { clearDiffReference(); } );
		bannerLayout->addWidget( clearDiffBtn );
	}
	ui->verticalLayout->insertWidget( 0, wwDiffBanner );
	wwDiffBanner->hide();
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
	header->setColumnHidden( NifModel::WwRefCol, true );
	header->setColumnHidden( NifModel::WwSummaryCol, true );

	// KFM
	kfmtree = ui->kfmtree;
	kfmtree->setModel( kfm );
	kfmtree->setItemDelegate( kfm->createDelegate( this ) );
	kfmtree->installEventFilter( this );
	kfmtree->setColumnHidden( NifModel::WwRefCol, true );
	kfmtree->setColumnHidden( NifModel::WwSummaryCol, true );

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
	connect( refreshBrowser, &QPushButton::clicked, this, [this]() {
		// Refresh means "re-read the disk" — drop the cached signatures so the
		// populate below cannot take the unchanged-resources fast path.
		nifBrowserIndexSignature.clear();
		nifBrowserTreeSignature.clear();
		populateConfiguredNifBrowser();
	} );
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
	auto * loadedDelegate = new LoadedNifsDelegate( loadedNifsView );
	// The two right-edge buttons read and write the same workspace flags the row
	// menu does, so the menu and the buttons can never disagree.
	loadedDelegate->displayMode = [this]( const QModelIndex & idx ) {
		if ( NifSkope * doc = documentFromBrowserIndex( idx ) ) {
			if ( doc == this )
				return -1;			// the primary is always drawn; nothing to toggle
			if ( !doc->sessionPreviewVisible || doc->sessionPreviewUnloaded )
				return 0;
			return doc->sessionPreviewGhost ? 2 : 1;
		}
		if ( BackgroundNifDocument * bg = backgroundDocumentFromBrowserIndex( idx ) ) {
			if ( !bg->selectedInWorkspace() )
				return 0;
			return bg->sessionPreviewGhost ? 2 : 1;
		}
		return -1;
	};
	loadedDelegate->setDisplayMode = [this]( const QModelIndex & idx, int mode ) {
		if ( NifSkope * doc = documentFromBrowserIndex( idx ); doc && doc != this ) {
			doc->sessionPreviewVisible = ( mode != 0 );
			doc->sessionPreviewUnloaded = false;
			doc->sessionPreviewGhost = ( mode == 2 );
		} else if ( BackgroundNifDocument * bg = backgroundDocumentFromBrowserIndex( idx ) ) {
			bg->sessionPreviewVisible = ( mode != 0 );
			bg->sessionPreviewUnloaded = false;
			bg->sessionPreviewGhost = ( mode == 2 );
		} else {
			return;
		}
		refreshAllDocumentSessions();
	};
	loadedNifsView->setItemDelegate( loadedDelegate );
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
			// Two or more rows selected is a different question from "what do I
			// want to do with this document" — it is "combine these", so it gets
			// its own menu rather than a merge item buried in the per-row one.
			if ( loadedNifsView->selectionModel()->selectedRows().size() > 1 ) {
				mergeLoadedDocumentsMenu( loadedNifsView->viewport()->mapToGlobal( pos ) );
				return;
			}
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

	// diff-vs-reference upkeep: edits re-derive the differing rows once per
	// burst; block insert/remove revalidates the reference; a fresh load
	// drops it entirely
	connect( nif, &NifModel::dataChanged, this, [this]() { queueDiffRecompute(); } );
	connect( nif, &NifModel::rowsInserted, this, [this]() { queueDiffRecompute(); } );
	connect( nif, &NifModel::rowsRemoved, this, [this]() { queueDiffRecompute(); } );
	connect( this, &NifSkope::beginLoading, this, [this]() { clearDiffReference(); } );

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
	QAction * ghost = menu.addAction( tr( "Show Semi-Transparent" ) );
	ghost->setCheckable( true );
	ghost->setChecked( document->sessionPreviewGhost );
	ghost->setEnabled( document != this );
	QAction * isolate = menu.addAction( tr( "Isolate This Secondary with Primary" ) );
	isolate->setEnabled( document != this );
	QAction * showAll = menu.addAction( tr( "Show All Secondary Documents" ) );
	QAction * hideAll = menu.addAction( tr( "Hide All Secondary Documents" ) );
	menu.addSeparator();
	QAction * freeze = menu.addAction( tr( "Freeze Animation…" ) );
	freeze->setEnabled( document->nif && !AnimSetup::sequenceNames( document->nif ).isEmpty() );
	freeze->setToolTip( tr( "Bake one instant of a sequence into the fields it drives" ) );
	menu.setToolTipsVisible( true );
	menu.addSeparator();
	QAction * unload = menu.addAction( tr( "Remove from Loaded NIFs" ) );
	// The primary's automatic row cannot be removed from its own workspace.
	unload->setEnabled( document != this );
	QAction * close = menu.addAction( tr( "Close Document" ) );
	QAction * chosen = menu.exec( globalPos );
	if ( chosen == makePrimary ) activateDocumentTab( index );
	else if ( chosen == freeze ) {
		if ( freezeDocumentDialog( document->nif, QFileInfo( document->currentFile ).fileName() ) )
			refreshAllDocumentSessions();
	}
	else if ( chosen == ghost ) {
		document->sessionPreviewGhost = ghost->isChecked();
		refreshAllDocumentSessions();
	} else if ( chosen == visible ) {
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

/*! Merge the documents selected in Loaded NIFs into one new file.
 *
 * The FIRST selected row is the target and every other is spliced into a copy
 * of it — so on a rig the skeleton goes first and dictates position for
 * everything: a bone that exists in both is the skeleton's, and the armour
 * piece's flat copy of it de-duplicates away. Effects land on the node their
 * AttachT names.
 *
 * Nothing loaded is touched. The merge runs on a fresh model built from the
 * target's bytes, so a merge that turns out wrong costs a file on disk and not
 * the documents in the workspace.
 */
void NifSkope::mergeLoadedDocumentsMenu( const QPoint & globalPos )
{
	// resolve the selection, in the order the list shows it
	QList<QPair<QString, NifModel *>> picked;
	const QModelIndexList rows = loadedNifsView->selectionModel()->selectedRows();
	for ( const QModelIndex & row : rows ) {
		if ( NifSkope * doc = documentFromBrowserIndex( row ) )
			picked.append( { QFileInfo( doc->currentFile ).fileName(), doc->nif } );
		else if ( BackgroundNifDocument * bg = backgroundDocumentFromBrowserIndex( row ) )
			picked.append( { bg->displayName(), bg->nif } );
	}
	if ( picked.size() < 2 )
		return;

	QMenu menu( this );
	menu.addSection( tr( "%1 documents selected" ).arg( picked.size() ) );
	QAction * doMerge = menu.addAction( tr( "Merge into a new NIF…" ) );
	doMerge->setToolTip( tr( "%1 is the target; the rest are spliced into a copy of it" )
		.arg( picked.first().first ) );
	// The last step of the loading-screen pipeline: merge, then bake the result
	// flat. Offered here because it is the same selection and the same target.
	QAction * doScreen = menu.addAction( tr( "Merge and Convert to Loading Screen…" ) );
	doScreen->setToolTip( tr( "Merge, then evaluate the skins away and drop the skeleton, "
		"as the vanilla LoadScreenArt files are built" ) );
	menu.setToolTipsVisible( true );
	QAction * chosen = menu.exec( globalPos );
	if ( chosen != doMerge && chosen != doScreen )
		return;
	const bool toLoadingScreen = ( chosen == doScreen );

	QString out = QFileDialog::getSaveFileName( this,
		toLoadingScreen ? tr( "Save loading screen NIF" ) : tr( "Save merged NIF" ),
		QFileInfo( picked.first().second->getFileInfo() ).absolutePath(),
		tr( "NIF files (*.nif)" ) );
	if ( out.isEmpty() )
		return;
	if ( !out.endsWith( QStringLiteral( ".nif" ), Qt::CaseInsensitive ) )
		out += QStringLiteral( ".nif" );

	// a copy of the target, through its own serializer, so the live document is
	// never the thing being edited
	auto snapshot = []( const NifModel * src, QByteArray & bytes ) {
		QBuffer buf( &bytes );
		return buf.open( QIODevice::WriteOnly ) && src->save( buf );
	};
	QByteArray targetBytes;
	NifModel merged;
	merged.setMessageMode( BaseModel::MSG_TEST );
	{
		QBuffer buf( &targetBytes );
		if ( !snapshot( picked.first().second, targetBytes ) || !buf.open( QIODevice::ReadOnly )
			|| !merged.load( buf ) ) {
			QMessageBox::warning( this, tr( "Merge" ),
				tr( "Could not copy %1 to merge into." ).arg( picked.first().first ) );
			return;
		}
	}

	QStringList report, dupes;
	int shapes = 0;
	for ( int i = 1; i < picked.size(); i++ ) {
		QByteArray bytes;
		if ( !snapshot( picked.at( i ).second, bytes ) ) {
			QMessageBox::warning( this, tr( "Merge" ),
				tr( "Could not read %1." ).arg( picked.at( i ).first ) );
			return;
		}
		NifMergeResult r;
		if ( !nifMergeData( &merged, bytes, picked.at( i ).first, true, r ) ) {
			QMessageBox::warning( this, tr( "Merge" ), r.error );
			return;
		}
		shapes += r.shapesAdded;
		dupes << r.duplicateNames;
		QString where;
		if ( !r.namedAttachments.isEmpty() )
			where = tr( ", branches attached by name to %1" )
				.arg( r.namedAttachments.join( QStringLiteral( ", " ) ) );
		else if ( !r.attachedTo.isEmpty() )
			where = tr( ", attached to %1" ).arg( r.attachedTo );
		else if ( r.isEffect )
			where = tr( ", attached to the ROOT — its AttachT names no node" );
		report << tr( "%1: %2 shape(s), %3 bone(s) shared%4" )
			.arg( picked.at( i ).first ).arg( r.shapesAdded ).arg( r.nodesReused ).arg( where );
	}

	LoadingScreen::Result screen;
	if ( toLoadingScreen ) {
		QString error;
		screen = LoadingScreen::convert( &merged, true, false, &error );
		if ( !screen.ok ) {
			QMessageBox::warning( this, tr( "Loading Screen" ), error );
			return;
		}
	}

	if ( !merged.saveToFile( out ) ) {
		QMessageBox::warning( this, tr( "Merge" ), tr( "Could not write %1." ).arg( out ) );
		return;
	}
	dupes.removeDuplicates();
	QString text = tr( "Merged %1 file(s) into %2 — %3 shape(s) added.\n\n%4" )
		.arg( picked.size() - 1 ).arg( QFileInfo( out ).fileName() ).arg( shapes )
		.arg( report.join( QLatin1Char( '\n' ) ) );
	if ( toLoadingScreen ) {
		text += tr( "\n\nBaked to loading-screen art: %1 skinned shape(s) evaluated, "
			"%2 rigid shape(s) folded, %3 node(s) and %4 block(s) removed." )
			.arg( screen.shapesBaked ).arg( screen.shapesFolded )
			.arg( screen.nodesRemoved ).arg( screen.blocksRemoved );
		if ( !screen.notes.isEmpty() )
			text += tr( "\n\n• %1" ).arg( screen.notes.join( QStringLiteral( "\n• " ) ) );
	}
	if ( !dupes.isEmpty() )
		text += tr( "\n\nWARNING: %1 bone name(s) now appear on more than one node "
			"(%2). Posing will address only one of each." )
			.arg( dupes.size() ).arg( dupes.mid( 0, 8 ).join( QStringLiteral( ", " ) ) );
	QMessageBox::information( this, tr( "Merge" ), text );
}

bool NifSkope::freezeDocumentDialog( NifModel * nif, const QString & displayName )
{
	if ( !nif )
		return false;

	const QStringList sequences = AnimSetup::sequenceNames( nif );
	if ( sequences.isEmpty() ) {
		QMessageBox::information( this, tr( "Freeze Animation" ),
			tr( "%1 has no animation sequences to freeze." ).arg( displayName ) );
		return false;
	}

	QDialog dlg( this );
	dlg.setWindowTitle( tr( "Freeze Animation — %1" ).arg( displayName ) );
	auto * form = new QVBoxLayout( &dlg );

	form->addWidget( new QLabel( tr( "Write what this sequence evaluates to at one instant into the "
		"fields it drives, then remove the controllers." ), &dlg ) );

	auto * seqRow = new QHBoxLayout;
	seqRow->addWidget( new QLabel( tr( "Sequence" ), &dlg ) );
	auto * seqBox = new QComboBox( &dlg );
	seqBox->addItems( sequences );
	seqRow->addWidget( seqBox, 1 );
	form->addLayout( seqRow );

	auto * timeRow = new QHBoxLayout;
	timeRow->addWidget( new QLabel( tr( "Time" ), &dlg ) );
	auto * timeBox = new QDoubleSpinBox( &dlg );
	timeBox->setDecimals( 3 );
	timeBox->setSingleStep( 0.05 );
	timeBox->setSuffix( tr( " s" ) );
	timeRow->addWidget( timeBox, 1 );
	auto * rangeLabel = new QLabel( &dlg );
	timeRow->addWidget( rangeLabel );
	form->addLayout( timeRow );

	auto * strip = new QCheckBox( tr( "Remove the controller graph" ), &dlg );
	strip->setChecked( true );
	strip->setToolTip( tr( "Unchecked bakes the values but leaves the file animating, so a time "
		"can be tried before committing to it" ) );
	form->addWidget( strip );

	// The time box follows the chosen sequence's own range, and starts wherever
	// the timeline is parked — the moment the user just scrubbed to.
	auto syncRange = [&]() {
		float a = 0, b = 0;
		FreezeAnim::sequenceRange( nif, seqBox->currentText(), &a, &b );
		if ( !( b > a ) )
			b = a;
		timeBox->setRange( a, b );
		rangeLabel->setText( tr( "of %1 – %2 s" ).arg( a, 0, 'f', 3 ).arg( b, 0, 'f', 3 ) );
		const float now = ogl ? ogl->sceneTime() : a;
		timeBox->setValue( ( now >= a && now <= b ) ? now : a );
	};
	syncRange();
	connect( seqBox, &QComboBox::currentTextChanged, &dlg, [&]( const QString & ) { syncRange(); } );

	auto * buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg );
	buttons->button( QDialogButtonBox::Ok )->setText( tr( "Freeze" ) );
	connect( buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
	connect( buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );
	form->addWidget( buttons );

	if ( dlg.exec() != QDialog::Accepted )
		return false;

	QString error;
	const FreezeAnim::Result r = FreezeAnim::freeze( nif, seqBox->currentText(),
		float( timeBox->value() ), strip->isChecked(), &error );
	if ( !r.ok ) {
		QMessageBox::warning( this, tr( "Freeze Animation" ), error );
		return false;
	}

	QString text = tr( "Froze '%1' at %2 s — %3 value(s) written, %4 left alone, %5 block(s) removed." )
		.arg( seqBox->currentText() ).arg( timeBox->value(), 0, 'f', 3 )
		.arg( r.baked ).arg( r.skipped ).arg( r.blocksRemoved );
	// Everything not baked is spelled out. A freeze that quietly did two thirds
	// of the job would read as a complete one.
	if ( !r.unhandled.isEmpty() )
		text += tr( "\n\nLeft alone:\n• %1" ).arg( r.unhandled.join( QStringLiteral( "\n• " ) ) );
	if ( !r.notes.isEmpty() )
		text += tr( "\n\nNotes:\n• %1" ).arg( r.notes.join( QStringLiteral( "\n• " ) ) );
	QMessageBox::information( this, tr( "Freeze Animation" ), text );
	return true;
}

void NifSkope::showBackgroundDocumentMenu( BackgroundNifDocument * document, const QPoint & globalPos )
{
	if ( !document ) return;
	QMenu menu( this );
	QAction * makePrimary = menu.addAction( tr( "Make Primary / Edit" ) );
	QAction * visible = menu.addAction( tr( "Selected / Visible in Workspace" ) );
	visible->setCheckable( true );
	visible->setChecked( document->selectedInWorkspace() );
	QAction * ghost = menu.addAction( tr( "Show Semi-Transparent" ) );
	ghost->setCheckable( true );
	ghost->setChecked( document->sessionPreviewGhost );
	QAction * isolate = menu.addAction( tr( "Isolate This Secondary with Primary" ) );
	QAction * showAll = menu.addAction( tr( "Show All Secondary Documents" ) );
	QAction * hideAll = menu.addAction( tr( "Hide All Secondary Documents" ) );
	menu.addSeparator();
	// Per file, per limb: this is what makes "freeze each part at its own moment,
	// then merge" a thing you can actually do.
	QAction * freeze = menu.addAction( tr( "Freeze Animation…" ) );
	freeze->setEnabled( !AnimSetup::sequenceNames( document->nif ).isEmpty() );
	freeze->setToolTip( tr( "Bake one instant of a sequence into the fields it drives" ) );
	menu.addSeparator();
	// The workspace edits loaded documents in place — posing, syncing, freezing —
	// so there has to be a way back that is always available and does not depend
	// on every edit having had an undo command behind it.
	const bool modified = document->isModified();
	QAction * revert = menu.addAction( modified
		? tr( "Revert to Loaded State (modified)" ) : tr( "Revert to Loaded State" ) );
	revert->setEnabled( modified );
	revert->setToolTip( tr( "Re-parse the bytes this document was loaded from, discarding every change" ) );
	menu.setToolTipsVisible( true );
	menu.addSeparator();
	// A data-only document exists solely as a workspace member, so removing it
	// from the Loaded NIFs list and closing it are the same operation.
	QAction * unload = menu.addAction( tr( "Remove from Loaded NIFs" ) );
	QAction * close = menu.addAction( tr( "Close Document" ) );
	QAction * chosen = menu.exec( globalPos );
	if ( chosen == revert ) {
		if ( document->revertToLoaded() )
			refreshAllDocumentSessions();
		else
			QMessageBox::warning( this, tr( "Revert" ),
				tr( "Could not re-parse %1's loaded state." ).arg( document->displayName() ) );
	}
	else if ( chosen == freeze ) {
		if ( freezeDocumentDialog( document->nif, document->displayName() ) )
			refreshAllDocumentSessions();
	}
	else if ( chosen == ghost ) {
		document->sessionPreviewGhost = ghost->isChecked();
		refreshAllDocumentSessions();
	}
	else if ( chosen == makePrimary ) promoteBackgroundDocument( document );
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
	/* Two routes, because the two buttons mean different things. A SOLID document
	 * is rendered for real — its own Scene, materials and textures. A GHOSTED one
	 * is a flat translucent soup, which is what "show me roughly where this sits"
	 * wants and which avoids threading a global alpha through every shader.
	 */
	QVector<Vector3> solid, ghost;
	QVector<NifModel *> sceneModels;
	auto addDocument = [&]( NifModel * nif, bool isGhost ) {
		if ( isGhost )
			ghost += sessionDocumentTriangleSoup( nif );
		else
			sceneModels.append( nif );
	};

	for ( NifSkope * document : std::as_const( sessionDocumentWindows ) ) {
		if ( !document || document == this || !document->sessionCollectionMember
			|| !document->sessionPreviewVisible
			|| document->sessionPreviewUnloaded || document->currentFile.isEmpty() )
			continue;
		addDocument( document->nif, document->sessionPreviewGhost );
	}
	for ( BackgroundNifDocument * document : std::as_const( sessionBackgroundDocuments ) ) {
		if ( !document || !document->selectedInWorkspace() || document->currentFile.isEmpty() )
			continue;
		addDocument( document->nif, document->sessionPreviewGhost );
	}
	ogl->setWorkspaceRenderModels( sceneModels );
	if ( solid.isEmpty() && ghost.isEmpty() ) ogl->clearSessionDocumentPreview();
	else ogl->setSessionDocumentPreview( solid, ghost );
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

void NifSkope::setBlockListQuickFilter( int id )
{
	blockListQuickFilter = id;
	// a type filter means "show me those blocks ONLY". Hierarchy mode can't do
	// that (a tree row can never show without its ancestors), so it temporarily
	// switches the list to the flat list view; All restores the hierarchy if the
	// filter was what left it.
	if ( id != 0 ) {
		if ( list && list->model() == proxy ) {
			blockListFilterRestoreHierarchy = true;
			if ( aList )
				aList->setChecked( true );
			setListMode();
		}
	} else if ( blockListFilterRestoreHierarchy ) {
		blockListFilterRestoreHierarchy = false;
		if ( aHierarchy )
			aHierarchy->setChecked( true );
		setListMode();
	}
	applyBlockListFilter();

	// the button carries which category is active, now that there is no chip row
	if ( !blockListFilterButton || !blockListFilterMenu )
		return;
	for ( QAction * a : blockListFilterMenu->actions() ) {
		if ( a->data().toInt() != id )
			continue;
		a->setChecked( true );
		const bool isAll = a->icon().isNull();
		blockListFilterButton->setIcon( a->icon() );
		blockListFilterButton->setText( isAll ? tr( "All" ) : QString() );
		blockListFilterButton->setToolButtonStyle(
			isAll ? Qt::ToolButtonTextOnly : Qt::ToolButtonIconOnly );
		blockListFilterButton->setToolTip( tr( "Filter: %1" ).arg( a->text() ) );
	}
}

void NifSkope::applyBlockListFilter()
{
	if ( !list || !proxy || !nif || !blockListSearch ) return;
	const QStringList terms = blockListSearch->text().simplified().split(
		QLatin1Char( ' ' ), Qt::SkipEmptyParts );
	// No filter and none to clear: skip. Without this every block-list rows
	// signal walked all blocks and built a formatted searchable string per
	// block (getBlockNumber + itemName + Name lookups + .arg) just to decide
	// "keep everything".
	const bool filterActive = !terms.isEmpty() || blockListQuickFilter != 0;
	if ( !filterActive && !blockListFilterWasActive )
		return;
	blockListFilterWasActive = filterActive;
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
	// The pinned-only toggle is a filter in its own right, so it has to defeat
	// the empty-terms fast path and keep the walk alive while it is on.
	const bool pinnedOn = wwPinnedOnly;
	if ( terms.isEmpty() && !pinnedOn && !blockDetailsFilterWasActive )
		return;
	blockDetailsFilterWasActive = !terms.isEmpty() || pinnedOn;

	if ( terms.isEmpty() && !pinnedOn ) {
		tree->setDetailsFilter( false, {} );
		tree->refreshRowHiding();	// back to pure condition/version hiding
		return;
	}

	// Collect the rows to KEEP: matches (against name, value, AND type),
	// their ancestors (the path to a match stays visible), and matches'
	// whole subtrees (expanding a matching compound must show its members).
	// The keep set is handed to the view and enforced inside isRowHidden(),
	// because the view re-derives row visibility on every relayout — a
	// filter applied as one-shot setRowHidden calls was clobbered by the
	// next derivation pass, which made searching look broken.
	QSet<const void *> keep;
	std::function<void( const NifItem * )> keepSubtree =
		[&]( const NifItem * it ) {
		if ( !it || keep.contains( it ) )
			return;
		keep.insert( it );
		for ( const NifItem * c : it->children() )
			keepSubtree( c );
	};

	// Pinned-only: the keep set is the pinned rows, their subtrees (a pinned
	// compound must be expandable) and their ancestors (the path to a pinned
	// row has to stay visible, same rule the text filter uses). With no text
	// typed this IS the filter; with text, the search narrows within it.
	if ( pinnedOn ) {
		wwUpdatePinnedItems();
		const QModelIndex root = tree->rootIndex();
		for ( const void * p : nif->pinnedItems ) {
			const NifItem * item = static_cast<const NifItem *>( p );
			keepSubtree( item );
			for ( const NifItem * a = item->parent(); a; a = a->parent() ) {
				if ( a == nif->getItem( root ) )
					break;
				keep.insert( a );
			}
		}
		if ( terms.isEmpty() ) {
			tree->setDetailsFilter( true, std::move( keep ) );
			tree->refreshRowHiding();
			return;
		}
	}
	const QSet<const void *> pinnedKeep = pinnedOn ? keep : QSet<const void *>();
	if ( pinnedOn )
		keep.clear();
	std::function<bool( const QModelIndex & )> walk =
		[&]( const QModelIndex & parent ) -> bool {
		bool branchMatches = false;
		for ( int row = 0; row < nif->rowCount( parent ); row++ ) {
			QModelIndex nameIndex = nif->index( row, NifModel::NameCol, parent );
			const NifItem * item = nif->getItem( nameIndex );
			if ( !item )
				continue;
			QString searchable = nameIndex.data( Qt::DisplayRole ).toString();
			QModelIndex valueIndex = nif->index( row, NifModel::ValueCol, parent );
			if ( valueIndex.isValid() )
				searchable += QLatin1Char( ' ' ) + valueIndex.data( Qt::DisplayRole ).toString();
			QModelIndex typeIndex = nif->index( row, NifModel::TypeCol, parent );
			if ( typeIndex.isValid() )
				searchable += QLatin1Char( ' ' ) + typeIndex.data( Qt::DisplayRole ).toString();
			bool rowMatches = true;
			for ( const QString & term : terms ) {
				if ( !searchable.contains( term, Qt::CaseInsensitive ) ) {
					rowMatches = false;
					break;
				}
			}
			if ( rowMatches ) {
				// a match keeps its whole subtree — and skips the per-member
				// stringify walk (a matched Vertex Data no longer costs a
				// 38k-element text build)
				keepSubtree( item );
				branchMatches = true;
				continue;
			}
			if ( walk( nameIndex ) ) {
				keep.insert( item );
				tree->expand( nameIndex );
				branchMatches = true;
			}
		}
		return branchMatches;
	};
	walk( tree->rootIndex() );
	if ( pinnedOn )
		keep.intersect( pinnedKeep );
	tree->setDetailsFilter( true, std::move( keep ) );
	tree->refreshRowHiding();
}

// ---- Block Details sticky view state (WW) ---------------------------------
// Expansion + scroll are remembered per block TYPE, so hopping between five
// BSTriShapes keeps the same rows open instead of resetting every click.

namespace
{
const QChar wwPathSep( '\x1f' );

QString wwDetailsRowKey( const NifModel * nif, const QModelIndex & idx )
{
	// row identity: field name normally, row number inside arrays (array
	// element names are positional pseudonyms)
	QModelIndex parent = idx.parent();
	if ( parent.isValid() && nif->isArray( parent ) )
		return QString::number( idx.row() );
	return nif->itemName( idx );
}
}

// ---- pinned fields (WW) ---------------------------------------------------
// Pinning is remembered per block TYPE as a field PATH, not as an item
// pointer or a row number: the whole point is that starring Glossiness on one
// BSLightingShaderProperty stars it on every one you click afterwards. The
// path convention is shared with the sticky-expansion state above.

QString NifSkope::wwFieldPath( const QModelIndex & index ) const
{
	if ( !nif || !index.isValid() || index.model() != nif )
		return QString();
	const QModelIndex block = nif->getBlockIndex( index );
	if ( !block.isValid() )
		return QString();

	QStringList segs;
	QModelIndex idx = index.sibling( index.row(), 0 );
	while ( idx.isValid() && idx != block ) {
		segs.prepend( wwDetailsRowKey( nif, idx ) );
		idx = idx.parent();
	}
	// index was not under a block (header/footer/the block row itself)
	if ( !idx.isValid() || segs.isEmpty() )
		return QString();
	return segs.join( wwPathSep );
}

QModelIndex NifSkope::wwResolveFieldPath( const QModelIndex & root, const QString & path ) const
{
	if ( !nif || !root.isValid() || path.isEmpty() )
		return QModelIndex();

	QModelIndex idx = root;
	const QStringList segs = path.split( wwPathSep );
	for ( const QString & seg : segs ) {
		QModelIndex next;
		bool numeric = false;
		const int row = seg.toInt( &numeric );
		if ( numeric && nif->isArray( idx ) ) {
			next = nif->index( row, 0, idx );
		} else {
			next = nif->getIndex( idx, seg );
			if ( next.isValid() )
				next = next.sibling( next.row(), 0 );
		}
		idx = next;
		if ( !idx.isValid() )
			return QModelIndex();
	}
	return idx;
}

bool NifSkope::wwIsFieldPinned( const QModelIndex & index ) const
{
	if ( !nif )
		return false;
	const QModelIndex block = nif->getBlockIndex( index );
	const QString path = wwFieldPath( index );
	if ( !block.isValid() || path.isEmpty() )
		return false;
	return wwPinnedFields.value( nif->itemName( block ) ).contains( path );
}

void NifSkope::wwTogglePinField( const QModelIndex & index )
{
	if ( !nif )
		return;
	const QModelIndex block = nif->getBlockIndex( index );
	const QString path = wwFieldPath( index );
	if ( !block.isValid() || path.isEmpty() )
		return;

	const QString type = nif->itemName( block );
	QSet<QString> & set = wwPinnedFields[type];
	if ( set.contains( path ) )
		set.remove( path );
	else
		set.insert( path );
	if ( set.isEmpty() )
		wwPinnedFields.remove( type );

	wwSavePinnedFields();
	wwUpdatePinnedItems();
	// the star lives in the Name column's DisplayRole
	if ( tree )
		tree->viewport()->update();
	// a pin/unpin while the pinned-only filter is up changes what's visible
	if ( wwPinnedOnly )
		applyBlockDetailsFilter();
}

void NifSkope::wwUpdatePinnedItems()
{
	if ( !nif )
		return;
	nif->pinnedItems.clear();
	if ( !tree || tree->model() != nif )
		return;
	const QModelIndex root = tree->rootIndex();
	if ( !root.isValid() || !nif->isNiBlock( root ) )
		return;

	const auto it = wwPinnedFields.constFind( nif->itemName( root ) );
	if ( it == wwPinnedFields.constEnd() )
		return;
	for ( const QString & path : *it ) {
		QModelIndex idx = wwResolveFieldPath( root, path );
		if ( idx.isValid() ) {
			if ( const NifItem * item = nif->getItem( idx ) )
				nif->pinnedItems.insert( item );
		}
	}
}

void NifSkope::wwLoadPinnedFields()
{
	wwPinnedFields.clear();
	QSettings settings;
	settings.beginGroup( QStringLiteral( "BlockDetails/PinnedFields" ) );
	for ( const QString & type : settings.childKeys() ) {
		const QStringList paths = settings.value( type ).toStringList();
		if ( !paths.isEmpty() )
			wwPinnedFields.insert( type, QSet<QString>( paths.constBegin(), paths.constEnd() ) );
	}
	settings.endGroup();
}

void NifSkope::wwSavePinnedFields() const
{
	QSettings settings;
	settings.beginGroup( QStringLiteral( "BlockDetails/PinnedFields" ) );
	settings.remove( QString() );	// drop types that no longer have pins
	for ( auto it = wwPinnedFields.constBegin(); it != wwPinnedFields.constEnd(); ++it ) {
		QStringList paths( it.value().constBegin(), it.value().constEnd() );
		paths.sort();
		settings.setValue( it.key(), paths );
	}
	settings.endGroup();
}

void NifSkope::wwCaptureDetailsState()
{
	if ( !tree || !nif || tree->model() != nif )
		return;
	QModelIndex root = tree->rootIndex();
	if ( !root.isValid() || !nif->isNiBlock( root ) )
		return;

	WwDetailsState state;
	std::function<void( const QModelIndex &, const QString & )> walk =
		[&]( const QModelIndex & parent, const QString & prefix ) {
		const int cnt = nif->rowCount( parent );
		// don't walk giant arrays (their elements aren't worth remembering,
		// and a 38k-row walk per block switch is exactly the kind of cost
		// this dock just got rid of)
		if ( cnt > 2000 )
			return;
		for ( int r = 0; r < cnt; r++ ) {
			QModelIndex c = nif->index( r, 0, parent );
			if ( !c.isValid() || !tree->isExpanded( c ) )
				continue;
			const QString key = prefix.isEmpty()
				? wwDetailsRowKey( nif, c )
				: prefix + wwPathSep + wwDetailsRowKey( nif, c );
			state.expanded << key;
			walk( c, key );
		}
	};
	walk( root, QString() );
	state.scroll = tree->verticalScrollBar() ? tree->verticalScrollBar()->value() : 0;
	wwDetailsState.insert( nif->itemName( root ), state );
}

bool NifSkope::wwHasDetailsState( const QModelIndex & root ) const
{
	return root.isValid() && nif && wwDetailsState.contains( nif->itemName( root ) );
}

void NifSkope::wwRestoreDetailsState( const QModelIndex & root )
{
	if ( !tree || !nif || !root.isValid() )
		return;
	const auto it = wwDetailsState.constFind( nif->itemName( root ) );
	if ( it == wwDetailsState.constEnd() )
		return;

	for ( const QString & path : it->expanded ) {
		QModelIndex idx = root;
		const QStringList segs = path.split( wwPathSep );
		for ( const QString & seg : segs ) {
			QModelIndex next;
			bool numeric = false;
			const int row = seg.toInt( &numeric );
			if ( numeric && nif->isArray( idx ) ) {
				next = nif->index( row, 0, idx );
			} else {
				next = nif->getIndex( idx, seg );
				if ( next.isValid() )
					next = next.sibling( next.row(), 0 );
			}
			idx = next;
			if ( !idx.isValid() )
				break;
		}
		if ( idx.isValid() && idx != root )
			tree->expand( idx );
	}

	// scroll after the expansions have relaid the view
	const int scroll = it->scroll;
	QTimer::singleShot( 0, tree, [this, scroll]() {
		if ( tree && tree->verticalScrollBar() )
			tree->verticalScrollBar()->setValue( scroll );
	} );
}

// ---- field-value clipboard (WW) -------------------------------------------
// Copy any leaf field once, then paste it onto every selected block that has
// the same field (resolved by name path), as a single undo step. Shared
// across windows like the row clipboard.

namespace
{
struct WwFieldClipboard {
	QString fieldName;
	QString displayValue;
	QVector<QPair<QString, int>> path;	// (name, row) segments from block root
	NifValue value;
	bool valid = false;
};
WwFieldClipboard wwFieldClip;
}

void NifSkope::wwCopyFieldValue( const QModelIndex & index )
{
	if ( !nif )
		return;
	const NifItem * item = nif->getItem( index );
	if ( !item || item->valueType() == NifValue::tNone )
		return;

	QVector<QPair<QString, int>> path;
	const NifItem * walk = item;
	while ( walk && !nif->isTopItem( walk ) ) {
		path.prepend( { walk->name(), walk->row() } );
		walk = walk->parent();
	}
	// only block fields multi-paste (header rows have no counterparts)
	if ( !walk || nif->getBlockNumber( walk ) < 0 || path.isEmpty() )
		return;

	wwFieldClip.fieldName = item->name();
	wwFieldClip.displayValue = item->getValueAsString();
	if ( wwFieldClip.displayValue.size() > 24 )
		wwFieldClip.displayValue = wwFieldClip.displayValue.left( 21 ) + QLatin1String( "..." );
	wwFieldClip.path = path;
	wwFieldClip.value = item->value();
	wwFieldClip.valid = true;
}

bool NifSkope::wwFieldClipboardValid() const
{
	return wwFieldClip.valid;
}

QString NifSkope::wwFieldClipboardLabel() const
{
	if ( !wwFieldClip.valid )
		return QString();
	if ( wwFieldClip.displayValue.isEmpty() )
		return QStringLiteral( "\"%1\"" ).arg( wwFieldClip.fieldName );
	return QStringLiteral( "\"%1\" = %2" ).arg( wwFieldClip.fieldName, wwFieldClip.displayValue );
}

void NifSkope::wwPasteFieldToBlocks( const QList<qint32> & blocks )
{
	if ( !nif || !wwFieldClip.valid || blocks.isEmpty() )
		return;

	ChangeValueCommand::createTransaction();
	int applied = 0;
	for ( qint32 b : blocks ) {
		const NifItem * item = nif->getBlockItem( b );
		for ( const auto & seg : std::as_const( wwFieldClip.path ) ) {
			if ( !item )
				break;
			if ( item->isArray() )
				item = item->child( seg.second );
			else
				item = nif->getItem( item, seg.first, false );
		}
		if ( !item || item->valueType() != wwFieldClip.value.type() )
			continue;
		QModelIndex vi = nif->itemToIndex( item, NifModel::ValueCol );
		if ( !vi.isValid() )
			continue;
		// old value is captured before the push; push() itself applies the new
		nif->undoStack->push( new ChangeValueCommand(
			vi, item->value(), wwFieldClip.value, item->name(), nif ) );
		applied++;
	}
	statusBar()->showMessage(
		tr( "Pasted %1 onto %2 of %3 selected blocks" )
			.arg( wwFieldClipboardLabel() ).arg( applied ).arg( blocks.size() ), 4000 );
}

// ---- diff-vs-reference (WW) -----------------------------------------------
// Pin one block as the reference; whatever Block Details then shows gets its
// differing rows accented in the standard orange. The differing-item sets
// live in NifModel (served per-role); the reference values for "Take" live
// here. Recomputed once per block switch / edit burst, never per paint.

void NifSkope::setDiffReference( const QModelIndex & blockIndex )
{
	if ( !nif )
		return;
	QModelIndex block = blockIndex;
	if ( block.isValid() && block.model() == proxy )
		block = proxy->mapTo( block );
	const int bn = nif->getBlockNumber( block );
	if ( bn < 0 )
		return;
	wwDiffRefIndex = QPersistentModelIndex( nif->getBlockIndex( bn ) );
	nif->diffRefBlock = bn;
	updateDiffHighlight();
}

void NifSkope::clearDiffReference()
{
	wwDiffRefIndex = QPersistentModelIndex();
	if ( nif ) {
		nif->diffRefBlock = -1;
		nif->diffItems.clear();
		nif->diffRefText.clear();
		nif->diffRefValues.clear();
	}
	if ( wwDiffBanner )
		wwDiffBanner->hide();
	if ( tree ) {
		tree->setColumnHidden( NifModel::WwRefCol, true );
		tree->viewport()->update();
	}
	if ( list )
		list->viewport()->update();
}

void NifSkope::queueDiffRecompute()
{
	if ( wwDiffRecomputeQueued || !nif || nif->diffRefBlock < 0 )
		return;
	wwDiffRecomputeQueued = true;
	QTimer::singleShot( 0, this, [this]() {
		wwDiffRecomputeQueued = false;
		updateDiffHighlight();
	} );
}

void NifSkope::updateDiffHighlight()
{
	if ( !nif )
		return;
	if ( !wwDiffRefIndex.isValid() ) {
		// reference block got deleted (or the file reloaded)
		if ( nif->diffRefBlock >= 0 )
			clearDiffReference();
		return;
	}
	if ( nif->getState() != BaseModel::Default ) {
		queueDiffRecompute();
		return;
	}

	nif->diffItems.clear();
	nif->diffRefText.clear();
	nif->diffRefValues.clear();
	nif->diffRefBlock = nif->getBlockNumber( QModelIndex( wwDiffRefIndex ) );

	const QModelIndex rootIdx = ( tree && tree->model() == nif ) ? tree->rootIndex() : QModelIndex();
	const NifItem * cur = nif->getItem( rootIdx );
	const NifItem * ref = nif->getItem( QModelIndex( wwDiffRefIndex ) );
	int diffLeaves = 0;

	if ( cur && ref && cur != ref && rootIdx.isValid() && nif->isNiBlock( rootIdx ) ) {
		auto valueDiffers = []( const NifItem * a, const NifItem * b ) {
			if ( a->valueType() != b->valueType() )
				return true;
			if ( a->valueType() == NifValue::tNone )
				return false;
			return a->getValueAsVariant() != b->getValueAsVariant();
		};
		std::function<bool( const NifItem *, const NifItem * )> walk =
			[&]( const NifItem * c, const NifItem * r ) -> bool {
			const int ccnt = c->childCount();
			const int rcnt = r->childCount();
			if ( !ccnt && !rcnt ) {
				if ( !valueDiffers( c, r ) )
					return false;
				diffLeaves++;
				// bound the stored reference data on pathological diffs
				if ( nif->diffRefValues.size() < 4000 ) {
					QString refText = r->getValueAsString();
					if ( refText.size() > 100 )
						refText = refText.left( 97 ) + QLatin1String( "..." );
					nif->diffRefText.insert( c, refText );
					nif->diffRefValues.insert( c, r->value() );
				}
				return true;
			}
			bool differs = false;
			if ( c->isArray() && ccnt != rcnt ) {
				// different lengths: flag the array row, skip element compare
				differs = true;
			} else if ( ccnt > 500 ) {
				// big arrays are compared by length only (perf guardrail);
				// element-level diff of vertex data is the table view's job
			} else {
				for ( int i = 0; i < ccnt; i++ ) {
					const NifItem * cc = c->child( i );
					if ( !cc )
						continue;
					const NifItem * rc = ( i < rcnt ) ? r->child( i ) : nullptr;
					if ( rc && !c->isArray() && rc->name() != cc->name() )
						rc = nullptr;
					if ( !rc && !c->isArray() ) {
						for ( int j = 0; j < rcnt; j++ ) {
							const NifItem * cand = r->child( j );
							if ( cand && cand->name() == cc->name() ) {
								rc = cand;
								break;
							}
						}
					}
					if ( !rc ) {
						// no counterpart on the reference side
						nif->diffItems.insert( cc );
						differs = true;
						continue;
					}
					if ( walk( cc, rc ) ) {
						nif->diffItems.insert( cc );
						differs = true;
					}
				}
			}
			return differs;
		};
		walk( cur, ref );
	}

	// banner: one plain grey line above the filter field
	if ( wwDiffBanner && wwDiffLabel ) {
		const QModelIndex refIdx( wwDiffRefIndex );
		QString text = tr( "Diff vs: %1 %2" )
			.arg( nif->diffRefBlock ).arg( nif->itemName( refIdx ) );
		if ( cur && ref && cur == ref )
			text += tr( " — showing the reference block" );
		else if ( cur && ref && cur->name() != ref->name() )
			text += tr( " — different types, matching fields only" );
		else if ( cur && ref )
			text += tr( " — %1 row(s) differ" ).arg( diffLeaves );
		wwDiffLabel->setText( text );
		wwDiffBanner->show();
	}

	// the Reference column rides the diff state: shown right after Value,
	// hidden again when the reference is cleared
	if ( tree && tree->isColumnHidden( NifModel::WwRefCol ) ) {
		tree->setColumnHidden( NifModel::WwRefCol, false );
		QHeaderView * hh = tree->header();
		const int wantVisual = hh->visualIndex( NifModel::ValueCol ) + 1;
		if ( hh->visualIndex( NifModel::WwRefCol ) != wantVisual )
			hh->moveSection( hh->visualIndex( NifModel::WwRefCol ), wantVisual );
		if ( hh->sectionSize( NifModel::WwRefCol ) < 60 )
			hh->resizeSection( NifModel::WwRefCol, 160 );
	}

	if ( tree )
		tree->viewport()->update();
	if ( list )
		list->viewport()->update();
}

void NifSkope::wwTakeReferenceValue( const QModelIndex & index )
{
	if ( !nif )
		return;
	const NifItem * item = nif->getItem( index );
	const auto it = nif->diffRefValues.constFind( item );
	if ( !item || it == nif->diffRefValues.constEnd() )
		return;
	QModelIndex vi = nif->itemToIndex( item, NifModel::ValueCol );
	if ( !vi.isValid() )
		return;
	ChangeValueCommand::createTransaction();
	nif->undoStack->push( new ChangeValueCommand( vi, item->value(), it.value(), item->name(), nif ) );
}

void NifSkope::wwTakeAllReferenceValues()
{
	if ( !nif || nif->diffRefValues.isEmpty() )
		return;
	ChangeValueCommand::createTransaction();
	// iterate over a copy: the pushes fire dataChanged, which queues (deferred)
	// diff recomputes that will rebuild the model's reference sets afterwards
	const auto refValues = nif->diffRefValues;
	for ( auto it = refValues.constBegin(); it != refValues.constEnd(); ++it ) {
		const NifItem * item = static_cast<const NifItem *>( it.key() );
		QModelIndex vi = nif->itemToIndex( item, NifModel::ValueCol );
		if ( !vi.isValid() )
			continue;
		nif->undoStack->push( new ChangeValueCommand( vi, item->value(), it.value(), item->name(), nif ) );
	}
}

void NifSkope::wwPasteFieldToRow( const QModelIndex & index )
{
	if ( !nif || !wwFieldClip.valid )
		return;
	const NifItem * item = nif->getItem( index );
	if ( !item || item->valueType() != wwFieldClip.value.type() )
		return;
	QModelIndex vi = nif->itemToIndex( item, NifModel::ValueCol );
	if ( !vi.isValid() )
		return;
	ChangeValueCommand::createTransaction();
	nif->undoStack->push( new ChangeValueCommand( vi, item->value(), wwFieldClip.value, item->name(), nif ) );
}

void NifSkope::wwCopyReferenceValue( const QModelIndex & index )
{
	// same clipboard as Copy Field Value, but carrying the REFERENCE block's
	// value for this row — so it can be pasted onto this block, other rows,
	// or the block-list multi-selection
	if ( !nif )
		return;
	const NifItem * item = nif->getItem( index );
	const auto it = nif->diffRefValues.constFind( item );
	if ( !item || it == nif->diffRefValues.constEnd() )
		return;

	QVector<QPair<QString, int>> path;
	const NifItem * walk = item;
	while ( walk && !nif->isTopItem( walk ) ) {
		path.prepend( { walk->name(), walk->row() } );
		walk = walk->parent();
	}
	if ( !walk || nif->getBlockNumber( walk ) < 0 || path.isEmpty() )
		return;

	wwFieldClip.fieldName = item->name();
	wwFieldClip.displayValue = it.value().toString();
	if ( wwFieldClip.displayValue.size() > 24 )
		wwFieldClip.displayValue = wwFieldClip.displayValue.left( 21 ) + QLatin1String( "..." );
	wwFieldClip.path = path;
	wwFieldClip.value = it.value();
	wwFieldClip.valid = true;
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
	setBlockListQuickFilter( 0 );
	if ( blockListFilterRestoreHierarchy ) {
		blockListFilterRestoreHierarchy = false;
		if ( aHierarchy )
			aHierarchy->setChecked( true );
		setListMode();
	}
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
		// compact "out/in" counts only — the word "Links" cost ~35px of the
		// dock's minimum width (the tooltip carries the meaning)
		blockListRelations->setText( tr( "%1/%2" ).arg( outgoing.size() ).arg( incoming.size() ) );
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

			const bool rootChanged = ( tree->rootIndex() != root );
			// sticky per-type view state: remember the block we're leaving,
			// and prefer the remembered layout over the type auto-expand
			const bool hasSticky = rootChanged && wwHasDetailsState( root );
			if ( rootChanged ) {
				wwCaptureDetailsState();
				tree->setRootIndex( root );
			} else
				tree->refreshRowHiding();	// same block: recover a stranded hiding pass

			if ( hasSticky )
				tree->doAutoExpanding = false;
			tree->setCurrentIndex( idx.sibling( idx.row(), 0 ) );
			if ( hasSticky ) {
				tree->doAutoExpanding = true;
				wwRestoreDetailsState( root );
			}

			// diff-vs-reference follows whatever block is shown
			if ( rootChanged && nif->diffRefBlock >= 0 )
				updateDiffHighlight();

			// pinned stars are per block type, so they must be re-resolved
			// against this block's items on every switch
			if ( rootChanged ) {
				wwUpdatePinnedItems();
				if ( wwPinnedOnly )
					applyBlockDetailsFilter();
			}

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
			list->setColumnHidden( NifModel::WwRefCol, true );
			list->setColumnHidden( NifModel::WwSummaryCol, false );
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
			// proxy model has three columns (see columnCount in nifproxymodel.h)
			list->setColumnHidden( 0, false );
			list->setColumnHidden( 1, false );
			list->setColumnHidden( 2, false );
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

	// Indexing every configured archive and rebuilding the tree is by far the
	// most expensive completeLoading reaction. While the browser dock is hidden
	// (closed or a background tab) defer the whole rebuild; the dock's
	// visibilityChanged hook replays it when it next shows.
	if ( dBrowser && !dBrowser->isVisible() ) {
		nifBrowserPopulatePending = true;
		return;
	}
	populateConfiguredNifBrowserNow();
}

void NifSkope::populateConfiguredNifBrowserNow()
{
	if ( !bsaModel || !bsaProxyModel || !bsaView || !nif ) return;
	nifBrowserPopulatePending = false;

	configuredNifBrowserPopulated = true;
	const Game::GameMode game = Game::GameManager::get_game( nif );
	const bool includeArchives = !nifBrowserArchivesToggle || nifBrowserArchivesToggle->isChecked();
	const bool includeLoose = !nifBrowserLooseToggle || nifBrowserLooseToggle->isChecked();

	// Re-scanning the game's archives and rebuilding the item tree only depends
	// on (game, configured resource paths) and the two source toggles. Loading
	// a nif changes neither, so cache both stages behind signatures: an
	// unchanged tree only refreshes the Loaded NIFs group (the one part that
	// does change per load), and an unchanged index skips the archive re-scan
	// even when the tree must re-filter (a toggle flip). The explicit
	// open-archive browser mode sets currentArchivePath and must never be
	// treated as the configured index, and Refresh clears the signatures to
	// force a true re-scan.
	const QStringList resourceFolders = Game::GameManager::folders( game );
	const QString indexSignature = QString::number( int( game ) ) + QLatin1Char( '\n' )
		+ resourceFolders.join( QLatin1Char( '\n' ) );
	const QString treeSignature = indexSignature + QLatin1Char( '\n' )
		+ QLatin1Char( includeArchives ? '1' : '0' ) + QLatin1Char( includeLoose ? '1' : '0' );
	const bool configuredIndexLive = currentArchive && currentArchivePath.isEmpty();
	if ( configuredIndexLive && treeSignature == nifBrowserTreeSignature
		&& bsaView->model() == bsaProxyModel && bsaProxyModel->sourceModel() == bsaModel ) {
		rebuildLoadedNifsBrowserGroup();
		return;
	}

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
	// Reuse the previously built index when the (game, paths) signature matches
	// — a source-toggle flip re-filters the tree but must not re-read the disk.
	if ( !( configuredIndexLive && indexSignature == nifBrowserIndexSignature ) ) {
		if ( currentArchive ) delete currentArchive;
		currentArchive = new BA2File();
		currentArchivePath.clear();
		currentArchiveNames.clear();
		nifBrowserSkippedResources = 0;
		for ( const QString & resourcePath : resourceFolders ) {
			if ( resourcePath.isEmpty() ) continue;
			// Resource lists commonly contain dedicated texture and material folders.
			// They cannot contain meshes and BA2File reports them as invalid archive
			// roots, so do not feed them to the NIF-only browser indexer.
			const QFileInfo resourceInfo( resourcePath );
			const QString leafName = resourceInfo.fileName();
			if ( resourceInfo.isDir()
				&& ( leafName.compare( QStringLiteral( "textures" ), Qt::CaseInsensitive ) == 0
					|| leafName.compare( QStringLiteral( "materials" ), Qt::CaseInsensitive ) == 0 ) ) {
				++nifBrowserSkippedResources;
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
				++nifBrowserSkippedResources;
			}
		}
		nifBrowserIndexSignature = indexSignature;
	}
	if ( nifBrowserSkippedResources > 0 ) {
		available->setToolTip( available->toolTip() + tr( "\n%1 non-mesh or unreadable resource path(s) skipped." )
			.arg( nifBrowserSkippedResources ) );
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

	nifBrowserTreeSignature = treeSignature;
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

/*! Add a loose NIF to Loaded NIFs by path, without going through the browser
 *  tree. The browser route needs a QModelIndex, which a script or a harness has
 *  no way to produce; this is the same work with the source named directly.
 */
bool NifSkope::addWorkspaceDocumentFromFile( const QString & path )
{
	auto * document = new BackgroundNifDocument;
	document->workspaceRoot = this;
	if ( !document->nif->loadFromFile( path ) ) {
		delete document;
		return false;
	}
	document->currentFile = path;
	document->sessionPreviewVisible = true;
	document->captureLoadedState();
	sessionBackgroundDocuments.append( document );
	refreshAllDocumentSessions();
	return true;
}

/*! How a workspace document draws: 0 hidden, 1 solid, 2 semi-transparent. The
 *  same state the row's two buttons and its menu drive, addressed by position so
 *  it can be scripted.
 */
bool NifSkope::setWorkspaceDisplayMode( int backgroundIndex, int mode )
{
	if ( backgroundIndex < 0 || backgroundIndex >= sessionBackgroundDocuments.size() )
		return false;
	BackgroundNifDocument * document = sessionBackgroundDocuments.at( backgroundIndex );
	document->sessionPreviewVisible = ( mode != 0 );
	document->sessionPreviewUnloaded = false;
	document->sessionPreviewGhost = ( mode == 2 );
	refreshAllDocumentSessions();
	return true;
}

//! Count of data-only workspace members, for scripting and tests.
int NifSkope::workspaceDocumentCount() const
{
	return int( sessionBackgroundDocuments.size() );
}

bool NifSkope::grabLoadedNifsView( const QString & path ) const
{
	return loadedNifsView && loadedNifsView->grab().save( path );
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

bool NifSkope::pickNifFromBrowser( QWidget * parent, QByteArray & bytesOut, QString & labelOut )
{
	if ( !bsaModel || !bsaProxyModel )
		return false;

	// Build the archive/loose tree now, even if the browser dock is hidden.
	populateConfiguredNifBrowserNow();
	if ( bsaModel->rowCount() == 0 ) {
		QMessageBox::information( parent, tr( "Load skeleton from archive" ),
			tr( "No game archives or loose files are available.\n\n"
			    "Configure a game's data folders in Settings → Resources, then its "
			    "meshes will be listed here." ) );
		return false;
	}

	QDialog dlg( parent );
	dlg.setWindowTitle( tr( "Load skeleton from archive" ) );
	dlg.resize( 560, 640 );
	auto * lay = new QVBoxLayout( &dlg );

	auto * filterEdit = new QLineEdit( &dlg );
	filterEdit->setPlaceholderText( tr( "Filter by name…" ) );
	filterEdit->setClearButtonEnabled( true );
	lay->addWidget( filterEdit );

	// A private filter proxy chained on the shared browser model, so filtering
	// here never disturbs the NIF Browser dock's own view of the same model.
	auto * filterProxy = new QSortFilterProxyModel( &dlg );
	filterProxy->setSourceModel( bsaProxyModel );
	filterProxy->setRecursiveFilteringEnabled( true );
	filterProxy->setFilterCaseSensitivity( Qt::CaseInsensitive );
	filterProxy->setFilterKeyColumn( 0 );

	auto * view = new QTreeView( &dlg );
	view->setModel( filterProxy );
	view->setSelectionMode( QAbstractItemView::SingleSelection );
	view->setSelectionBehavior( QAbstractItemView::SelectRows );
	view->setUniformRowHeights( true );
	if ( filterProxy->columnCount() > 2 )
		view->setColumnHidden( 2, true );   // keep name + path, hide size
	lay->addWidget( view, 1 );

	connect( filterEdit, &QLineEdit::textChanged, filterProxy,
		[filterProxy]( const QString & t ) { filterProxy->setFilterFixedString( t ); } );

	auto * buttons = new QDialogButtonBox( QDialogButtonBox::Open | QDialogButtonBox::Cancel, &dlg );
	if ( auto * ok = buttons->button( QDialogButtonBox::Open ) )
		ok->setText( tr( "Load skeleton" ) );
	lay->addWidget( buttons );
	connect( buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
	connect( buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );
	connect( view, &QTreeView::doubleClicked, &dlg, &QDialog::accept );

	if ( dlg.exec() != QDialog::Accepted )
		return false;

	QModelIndex sel = view->currentIndex();
	if ( !sel.isValid() )
		return false;
	sel = filterProxy->mapToSource( sel );     // back to the browser model
	const QModelIndex nameIndex = sel.sibling( sel.row(), 0 );
	const QString path = sel.sibling( sel.row(), 1 ).data( Qt::EditRole ).toString();
	if ( path.isEmpty() )                       // a folder / archive row, not a file
		return false;
	const int source = nameIndex.data( NifBrowserSourceRole ).toInt();
	const int game = nameIndex.data( NifBrowserGameRole ).toInt();

	if ( source == NifBrowserConfiguredResource ) {
		QString displayPath;
		if ( !extractConfiguredNifBytes( game, path, bytesOut, displayPath ) )
			return false;
		labelOut = QFileInfo( displayPath.isEmpty() ? path : displayPath ).fileName();
	} else if ( source == NifBrowserLooseFile ) {
		QFile f( path );
		if ( !f.open( QIODevice::ReadOnly ) )
			return false;
		bytesOut = f.readAll();
		labelOut = QFileInfo( path ).fileName();
	} else {
		return false;                           // e.g. a Loaded-NIFs row
	}
	return true;
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
	document->captureLoadedState();
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

	/* A load is a new scene, so drop whatever mode the last file was being worked
	 * in and reframe the camera.
	 *
	 * Staying in Edit or Pose or Physics Sim across a load leaves the mode pointed
	 * at blocks that no longer exist, and keeping the old camera frames the new
	 * file from wherever the last one happened to be looked at -- which on a
	 * differently sized model is often nowhere near it.
	 */
	if ( ogl ) {
		ogl->setPhysicsSimMode( false );
		ogl->setRiggingWeightPaintMode( false );
		ogl->setVertexPaintMode( false );
		ogl->setSegmentPaintMode( false );
		ogl->setPoseMode( false );
		ogl->setEditMode( false );
	}

	bool loaded = nif->loadFromFile( fname );
	perfMark( "loadFromFile (views detached)" );

	emit completeLoading( loaded, fname );
	perfMark( "completeLoading consumers" );
	// reframe on the new contents, after the scene has been rebuilt
	if ( loaded && ogl )
		ogl->setOrientation( ogl->viewState(), true );

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
