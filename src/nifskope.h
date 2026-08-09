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

#ifndef NIFSKOPE_H
#define NIFSKOPE_H

#include <QMainWindow>     // Inherited
#include <QObject>         // Inherited
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QSet>
#include <QStringList>
#include <QUndoCommand>
#include <QVector>

#include <data/nifvalue.h>

#include <memory>

#if QT_NO_DEBUG
#define NIFSKOPE_IPC_PORT 12583
#else
#define NIFSKOPE_IPC_PORT 12584
#endif

namespace Ui {
	class MainWindow;
}

namespace nstypes
{
	QString operator""_uip( const char * str, size_t sz );
}

class BackgroundNifDocument;
class GLView;
class InspectView;
class KfmModel;
class NifModel;
class NifProxyModel;
class NifTreeView;
class ReferenceBrowser;
class SettingsDialog;
class Spell;
class SpellBook;
class TimelineWidget;
class BA2File;
class BSAModel;
class BSAProxyModel;
class QStandardItemModel;
class QAction;
class QActionGroup;
class QButtonGroup;
class QComboBox;
class QFrame;
class QLineEdit;
class QLabel;
class QMenu;
class QProgressBar;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTimer;
class QToolButton;
class QToolBar;
class QTabBar;
class QTreeView;
class QUdpSocket;

namespace nstheme
{
	enum WindowColor { Base, BaseAlt, Text, Highlight, HighlightText, BrightText };
	enum WindowTheme { ThemeDark, ThemeLight };
}


//! @file nifskope.h NifSkope, IPCsocket

/*! The main application class for NifSkope.
 *
 * This class encapsulates the main NifSkope window. It has members for saving
 * and restoring settings, loading and saving NIF files, loading an XML
 * description, widgets for the various subwindows, menus, and a UDP socket
 * with which NifSkope can communicate with itself.
 */
class NifSkope final : public QMainWindow
{
	Q_OBJECT

public:
	explicit NifSkope( bool background = false );
	~NifSkope();

	Ui::MainWindow * ui;

	//! Save Confirm dialog
	bool saveConfirm();
	//! Save NifSkope application settings.
	void saveUi() const;
	//! Restore NifSkope UI settings.
	void restoreUi();

	//! Returns path of currently open file
	QString getCurrentFile() const;
	//! Currently selected NiBlock index in the list or tree view
	QModelIndex currentNifIndex() const;

	/*! Create and initialize a new NifSkope application window.
	 *
	 * @param fname The name of the file to load in the new NifSkope window.
	 * @return		The newly created NifSkope instance.
	 */
	static NifSkope * createWindow( const QString & fname = QString(), bool background = false );
	//! Open NIF documents in this application session. Each document keeps its
	//! own model, selection, Undo stack and dirty state; the active one is primary.
	static QList<NifSkope *> openDocuments();
	//! Documents explicitly added to the Loaded NIFs workspace and currently selected.
	static QList<NifSkope *> selectedWorkspaceDocuments();
	//! Which window a generated document belongs in: the one that owns the model
	//! it was made from, else the active window, else the first open document.
	//! Null only when no window exists at all.
	static NifSkope * workspaceForNewDocuments( const NifModel * from = nullptr );
	//! Selected workspace members as model/display-path pairs. Unlike
	//! selectedWorkspaceDocuments() this includes data-only background documents,
	//! which own a NifModel but no window; tools that only read donor geometry
	//! must use this list.
	static QList<QPair<NifModel *, QString>> selectedWorkspaceModels();
	static NifSkope * documentForModel( const NifModel * model );

	/*! The loaded NIF marked as the faceBones donor, or null.
	 *
	 *  Marked once from a Loaded NIFs row menu and remembered, the way the
	 *  workspace skeleton is, so the rigging steps stop asking which file the
	 *  sculpt bones come from every time they run. The donor is READ, never
	 *  written to.
	 */
	//! Write a data-only loaded NIF to a file the user picks; false if cancelled
	//! or the write failed. Also what the close prompt calls when you say Save.
	bool saveBackgroundDocumentAs( BackgroundNifDocument * document );
	//! The writing half of Save As, with the path already chosen — the dialog is
	//! the one part a harness cannot drive, and the least interesting part.
	bool saveBackgroundDocumentTo( BackgroundNifDocument * document, const QString & path );
	bool saveWorkspaceDocumentTo( int backgroundIndex, const QString & path );
	//! Build a faceBones NIF from a row using the marked donor; "" on success.
	QString generateFaceBonesFromRow( int backgroundIndex );
	bool markWorkspaceFaceDonorRow( int backgroundIndex );
	bool markWorkspaceDocumentUnsaved( int backgroundIndex );
	//! Load NIFs from anywhere on disk into Loaded NIFs; returns how many.
	int addWorkspaceDocumentsFromDialog();
	//! Show a loaded NIF in THIS window instead of opening another one; the row
	//! it came from goes. False if cancelled at the unsaved prompt.
	bool openBackgroundDocumentHere( BackgroundNifDocument * document );

	/* The Loaded NIFs list addressed by position, for scripts and harnesses —
	 * the panel's documents live in a class with no header, so there is no other
	 * way to ask about a row. Same reason setWorkspaceDisplayMode is here.
	 * (workspaceDocumentCount is declared with the display-mode group below.)
	 */
	QString workspaceDocumentName( int backgroundIndex ) const;
	bool workspaceDocumentModified( int backgroundIndex ) const;
	//! How many times a Scene has been BUILT for this row — a rebuild is what a
	//! destroyed Scene looks like, and "does it have one" cannot see that.
	int workspaceDocumentSceneBuilds( int backgroundIndex );
	bool openWorkspaceDocumentHere( int backgroundIndex );
	//! Build a faceBones NIF from a loaded document using the marked face donor,
	//! and put the result in Loaded NIFs, unsaved. Returns why not, or empty.
	QString generateFaceBonesInto( BackgroundNifDocument * source );

	static NifModel * workspaceFaceDonor();
	static QString workspaceFaceDonorName();
	static void setWorkspaceFaceDonor( NifModel * model, const QString & displayName );

	static SettingsDialog * getOptions();

	//! List of all supported file extensions
	static QStringList fileExtensions();

	//! Return a file filter for a single extension
	static QString fileFilter( const QString & );

	/*! Return a file filter for all supported extensions.
	 *
	 * @param allFiles If true, file filter will be prepended with "All Files (*.nif *.btr ...)"
	 *					so that all supported files will show at once. Used for Open File dialog.
	 */
	static QString fileFilters( bool allFiles = true );

	//! Sets application locale and loads translation files
	static void SetAppLocale( QLocale curLocale );
	//! Application-wide debug and warning message handler
	static void MessageOutput( QtMsgType type, const QMessageLogContext & context, const QString & str );

	//! A map of all the currently support filetypes to their file extensions.
	static const QList<QPair<QString, QString>> filetypes;

	enum { NumRecentFiles = 10 };

	static QColor defaultsDark[6];
	static QColor defaultsLight[6];

	static void reloadTheme();

	inline GLView * getGLView()
	{
		return ogl;
	}
	inline NifModel * getNifModel() const
	{
		return nif;
	}

	/*! For each NIF path on fileList, load the file, run processFunc() on the model, and save the modified file
	 * if processFunc() returned true. The optional processFuncData pointer is passed to the function.
	 * Returns true if processing has been successfully completed.
	 */
	bool batchProcessFiles( const QStringList & fileList,
							bool (*processFunc)( NifModel *, void * ), void * processFuncData = nullptr );

signals:
	void beginLoading();
	void completeLoading( bool, QString & );
	void beginSave();
	void completeSave( bool, QString & );
	//! Emitted after all views and the menubar SpellBook adopt a new NIF index.
	void currentNifIndexChanged( const QModelIndex & );

public slots:
	//! Open in the current window, replacing the current document after an
	//! unsaved-changes prompt; returns false when the user cancels.
	//! (Right-click a recent entry for Open in New Window.)
	bool openFile( QString & );
	void openFiles( QStringList & );

	//! Apply stored "Shortcuts/action.<objectName>" overrides to this
	//! window's QActions (also records each action's factory default the
	//! first time it is seen, so overrides can be removed again)
	void applyShortcutOverrides();

	bool loadArchivesFromFolder( QString );
	void openArchive( const QString & );
	//! These return false only when the user cancels the unsaved-changes
	//! prompt (so batch opens can abort); failures to extract return true.
	//! confirmReplace=false skips the prompt (the Reload path).
	bool openArchiveFile( const QModelIndex & index, bool newWindow = false );
	bool openArchiveFileString( const BA2File *, const QString &, bool newWindow = false,
		bool confirmReplace = true );
	void openNifBrowserSelection();
	//! Modal NIF Browser picker: shows the archive / loose resource tree and, on
	//! accept, fills \a bytesOut with the chosen NIF's data and \a labelOut with a
	//! display name. Returns false if cancelled or nothing suitable was picked.
	//! Reuses the browser's model and the same archive extraction the NIF Browser
	//! dock uses, so no separate archive UI is needed.
	bool pickNifFromBrowser( QWidget * parent, QByteArray & bytesOut, QString & labelOut );

	void enableUi();

	//! Should a window with no file show the starter cube? False for every WW_*
	//! harness run and when the preference is off. Shared by the startup path and
	//! by Reload, which on an untitled document rebuilds the same scene.
	static bool startupCubeWanted();

	void updateSettings();

	//! Select a NIF index
	void select( const QModelIndex & );
	//! Run a registered spell through the persistent SpellBook (menus/workspaces share this path).
	void castSpell( const QString & id, const QModelIndex & index );

	// Automatic slots

	//! Close all resource folders and files.
	void on_aCloseArchives_triggered();

	//! Flush texture cache and update view.
	void on_aUpdateView_triggered();

	//! Reparse the nif.xml and kfm.xml files.
	void on_aLoadXML_triggered();

	//! Reparse the nif.xml and kfm.xml files and reload the current file.
	void on_aReload_triggered();

	//! Open file browser for all resources
	void on_aArchiveExtractor_triggered();

	//! A slot that creates a new NifSkope application window.
	void on_aWindow_triggered();

	//! A slot for starting the XML checker.
	void on_aShredder_triggered();

	//! Reset "block details"
	void on_aHeader_triggered();

	//! Select the font to use
	void on_aSelectFont_triggered();

	void on_tRender_actionTriggered( QAction * );

	void on_aViewTop_triggered( bool );
	void on_aViewFront_triggered( bool );
	void on_aViewLeft_triggered( bool );

	void on_aViewCenter_triggered();
	void on_aViewFlip_triggered( bool );
	void on_aViewPerspective_toggled( bool );
	void on_aViewWalk_triggered( bool );

	void on_aViewUser_toggled( bool );
	void on_aViewUserSave_triggered( bool );

	void on_aSettings_triggered();

	void on_mTheme_triggered( QAction * action );


protected slots:
	void openDlg();
	bool saveAsDlg();

	void archiveDlg();
	void archiveFolderDlg();

	void load();
	bool save();

	void reload();

	void exitRequested();

	void onLoadBegin();
	void onSaveBegin();

	void onLoadComplete( bool, QString & );
	void onSaveComplete( bool, QString & );

	//! Display a context menu at the specified position
	void contextMenu( const QPoint & pos );

	//! The Block List menu's non-spell parts: the trailing group and the verb row.
	//! Separate from contextMenu so a harness can build them without a right-click,
	//! since contextMenu ends in exec() and blocks. See WW_MENUTREE_TEST.
	void buildBlockListMenuExtras( SpellBook & contextBook, const QModelIndex & idx );

	//! The Select & View submenu. Lives here, not in a spell: five of its six
	//! entries are private GLView members and GLView befriends NifSkope only.
	void buildBlockListSelectAndView( SpellBook & contextBook, const QModelIndex & idx );

	//! Block List ▸ Transfer Normals: the other selected meshes are the source,
	//! \a target is written. Mapping and mix are asked for; the source is the
	//! selection, so it is not.
	void transferNormalsFromSelection( int target, const QVector<int> & sources );

	//! Set the list mode
	void setListMode();
	/*! The block list's columns, either side of a model change.
	 *
	 *  Release BEFORE `list->setModel`, apply AFTER. QHeaderView gives a hidden
	 *  section its width back when the model changes without adding it to the
	 *  cached `length`, so hiding it again subtracts it twice; `length` goes
	 *  negative, `visualIndexAt` answers -1 for every position past it, and
	 *  `indexAt` then finds no column and therefore no row anywhere in the view.
	 *  \{ */
	void wwReleaseBlockListColumns();
	void wwApplyBlockListColumns();
	/*! \} */

	//! Override the view font
	void overrideViewFont();

	/*! Sets Import/Export menus
	 *
	 * @see importex/importex.cpp
	 */
	void fillImportExportMenus();
	void updateImportExportMenu(const QMenu* menu);
	//! Perform Import or Export
	void sltImport( QAction* action );
	void sltExport( QAction* action );

	//! Open a URL using the system handler
	void openURL();

	//! Change system locale and notify user that restart may be required
	void sltLocaleChanged();

protected:
	void closeEvent( QCloseEvent * e ) override final;
	//void resizeEvent( QResizeEvent * event ) override final;
	bool eventFilter( QObject * o, QEvent * e ) override final;

private:
	void initActions();
	void initDockWidgets();
	enum LeftColumnMode { LeftBlocks = 0, LeftNifs = 1, LeftHeader = 2 };
	void setLeftColumnMode( LeftColumnMode mode );
	bool leftColumnIs( LeftColumnMode mode ) const { return leftColumnMode == mode; }
	void initToolBars();
	void initMenu();
	void initConnections();
	void initDocumentSession();
	void rebuildDocumentTabs();
	void rebuildLoadedNifsBrowserGroup();
	void applyLoadedNifsFilter();
	//! Refresh Loaded NIFs count, legend and passive empty/filter-empty message.
	void updateLoadedNifsPresentation();
	void wireLoadedNifsSelection();
	void addNifBrowserIndexToLoaded( const QModelIndex & index );
	void queueNifBrowserIndexToLoaded( const QModelIndex & index );
	void processNextNifBrowserLoad();
	void addNifBrowserSelectionToLoaded();
	void addNifBrowserRowsToLoaded( const QList<QPersistentModelIndex> & rows );
	void activateDocumentTab( int index );
	void showDocumentTabMenu( const QPoint & pos );
	void showDocumentMenu( NifSkope * document, const QPoint & globalPos );
	NifSkope * documentFromBrowserIndex( const QModelIndex & index ) const;
	BackgroundNifDocument * backgroundDocumentFromBrowserIndex( const QModelIndex & index ) const;
	void showBackgroundDocumentMenu( BackgroundNifDocument * document, const QPoint & globalPos );
	//! The reverse half of browser -> Loaded NIFs: Save As the dragged loaded row.
	bool saveDraggedLoadedNifAs( const QModelIndex & row,
		const QString & pathForTest = QString() );
	QString nifBrowserFavouriteId( const QModelIndex & index ) const;
	bool isNifBrowserFavourite( const QModelIndex & index ) const;
	void toggleNifBrowserFavourite( const QModelIndex & index );
	void applyNifBrowserFavourites();
	//! Explicitly destructive workspace actions use this before deleting a row.
	bool confirmBackgroundDocumentRemoval( BackgroundNifDocument * document );
	NifSkope * workspaceGroupRoot() const;
	bool sharesWorkspaceGroup( const NifSkope * document ) const;
	QList<BackgroundNifDocument *> workspaceBackgroundDocuments() const;
public:
	//! Add a loose NIF to Loaded NIFs by path (the browser route needs a
	//! QModelIndex, which a script has no way to produce).
	bool addWorkspaceDocumentFromFile( const QString & path );
	//! The same for a NIF that exists only in memory — a generated one. It lands
	//! unsaved, under the name it would take if it were written, and its bytes
	//! are parsed rather than trusted. False when they do not parse.
	bool addWorkspaceDocumentFromMemory( const QByteArray & bytes, const QString & displayPath );
	//! How a workspace document draws: 0 hidden, 1 solid, 2 semi-transparent.
	bool setWorkspaceDisplayMode( int backgroundIndex, int mode );
	//! Read that back: 0 hidden, 1 solid, 2 semi-transparent; -1 for a bad index.
	/*! Visible and see-through are independent flags, so mode 2 means both are on
	 *  and a hidden document keeps whatever see-through setting it had. */
	int workspaceDisplayMode( int backgroundIndex ) const;
	int workspaceDocumentCount() const;
	//! One selected Loaded NIFs row, whichever kind of document it is.
	struct WorkspaceTarget
	{
		bool * visible;
		bool * ghost;
		bool * unloaded;
		class NifModel * nif;
		QString name;
		NifSkope * window;					//!< set for a real document window
		BackgroundNifDocument * background;	//!< set for a data-only document
	};
	QVector<WorkspaceTarget> selectedWorkspaceTargets( const QModelIndex & clicked = QModelIndex() );
	int removeSelectedWorkspaceDocuments( const QModelIndex & clicked = QModelIndex() );

	//! Mark a workspace document as the optional rig-merge skeleton; -1 unmarks.
	bool setWorkspaceSkeletonDocument( int backgroundIndex );
	//! Block count of a workspace document, so a merge into it can be measured.
	int workspaceBlockCount( int backgroundIndex ) const;
	//! The model behind a workspace document, so a test can pose it.
	NifModel * workspaceDocumentModel( int backgroundIndex ) const;
	//! Splice workspace documents together in place. A valid marked skeleton in
	//! this selection becomes the target; otherwise targetIndex remains the target.
	bool mergeWorkspaceDocumentsInto( int targetIndex, const QList<int> & donorIndices );
	//! Render the Loaded NIFs list offscreen to a PNG — the row buttons can only
	//! be checked by looking at them.
	bool grabLoadedNifsView( const QString & path ) const;
private:
	//! Multi-row menu: bulk display settings for the selection, then the merges.
	void showSelectionMenu( const QModelIndex & clicked, const QPoint & globalPos );
	//! Merge menu. A selected skull-marked skeleton wins; otherwise `clicked`
	//! names the target the others are spliced into.
	void mergeLoadedDocumentsMenu( const QPoint & globalPos,
	                               const QModelIndex & clicked = QModelIndex() );
	void mergeIntoLoadedDocument( const QList<QPair<QString, class NifModel *>> & picked );
	//! Ask which sequence and which instant, then bake it. Per loaded document, so
	//! each limb can be frozen at its own moment before anything is merged.
	//! Returns true when the model was changed.
	bool freezeDocumentDialog( NifModel * nif, const QString & displayName );
	//! Make a data-only row primary in this exact window. Only the model/scene is
	//! swapped; the main window and all of its UI remain alive.
	bool promoteBackgroundDocument( BackgroundNifDocument * document );
	bool removeBackgroundDocument( BackgroundNifDocument * document );
	//! Locate a configured-resource NIF in the primary's combined archive and
	//! return its raw bytes plus the "[Game]/path" display path.
	bool extractConfiguredNifBytes( int gameID, const QString & path,
		QByteArray & bytes, QString & displayPath ) const;
	void refreshSessionPreview();
	static void refreshAllDocumentSessions();
	QString documentDisplayName() const;

	void loadFile( const QString & );
	bool saveFile( const QString & );
	void checkFile( QFileInfo fInfo, QByteArray filehash );

	void openRecentFile();
	void setCurrentFile( const QString & );
	void clearCurrentFile();
	void updateRecentFileActions();
	void updateAllRecentFileActions();

	void openRecentArchive();
	void openRecentArchiveFile();
	void setCurrentArchive( bool );
	void setCurrentArchiveFile( const QString & );
	void clearCurrentArchive();
	void appendLooseNifsToBrowser( const QString & );
	void populateConfiguredNifBrowser();
	//! The actual tree rebuild, minus the dock-hidden deferral gate. The picker
	//! (pickNifFromBrowser) calls this directly so it gets a populated tree even
	//! when the browser dock is hidden.
	void populateConfiguredNifBrowserNow();
	bool openConfiguredNif( int game, const QString & path, bool newWindow = false );
	bool loadConfiguredNifIntoDocument( NifSkope * target, int game, const QString & path );
	void updateRecentArchiveActions();
	void updateRecentArchiveFileActions();

	//! Disconnect and reconnect the models to the views
	void swapModels();

	//! (Re)connect the block-list multi-selection handler. Must be re-run
	//! whenever list->setModel() replaces the view's selection model.
	void wireBlockListSelection();
	//! Reapply the recursive Block List text filter.
	//! Apply a Block List category filter by id, and show it on the filter button.
	void setBlockListQuickFilter( int id );
	void applyBlockListFilter();
	//! Reapply the recursive Block Details field-name/value filter.
	void applyBlockDetailsFilter();
	//! Reapply the recursive Header field-name/value/type filter.
	void applyHeaderFilter();
	//! Refresh the standalone Header page's file identity and version summary.
	void updateHeaderPresentation();

	// ---- Block Details sticky view state (WW): expansion + scroll survive
	// switching between blocks of the same type ----
	void wwCaptureDetailsState();
	bool wwHasDetailsState( const QModelIndex & root ) const;
	void wwRestoreDetailsState( const QModelIndex & root );

	// ---- pinned fields (WW): star the handful of fields you actually tune on
	// a block type, then filter down to just those on every block of that type ----
	//! Stable path from the owning block down to a field, '\x1f'-separated
	//! (array elements identify by row, everything else by name).
	QString wwFieldPath( const QModelIndex & index ) const;
	//! Resolve a path produced by wwFieldPath back to an index under a block.
	QModelIndex wwResolveFieldPath( const QModelIndex & root, const QString & path ) const;
	bool wwIsFieldPinned( const QModelIndex & index ) const;
	void wwTogglePinField( const QModelIndex & index );
	//! Re-resolve the shown block's pinned paths into NifModel::pinnedItems.
	void wwUpdatePinnedItems();
	void wwLoadPinnedFields();
	void wwSavePinnedFields() const;

	// ---- field-value clipboard (WW): copy a field once, paste it onto every
	// selected block that has the field, as one undo step ----
	void wwCopyFieldValue( const QModelIndex & index );
	bool wwFieldClipboardValid() const;
	QString wwFieldClipboardLabel() const;
	void wwPasteFieldToBlocks( const QList<qint32> & blocks );
	//! Paste the field clipboard onto one specific row (type-checked).
	void wwPasteFieldToRow( const QModelIndex & index );
	//! Fill the field clipboard with the diff reference's value for this row.
	void wwCopyReferenceValue( const QModelIndex & index );

	// ---- diff-vs-reference (WW): compare the shown block against a pinned
	// reference block; differing rows accent orange ----
	void setDiffReference( const QModelIndex & blockIndex );
	void clearDiffReference();
	//! Recompute the differing-row set for the block currently shown.
	void updateDiffHighlight();
	//! Coalesce recomputes across a dataChanged burst.
	void queueDiffRecompute();
	void wwTakeReferenceValue( const QModelIndex & index );
	void wwTakeAllReferenceValues();
	//! Rebuild breadcrumb, history, pin, relationship and footer state.
	void updateBlockListNavigation( const QModelIndex & index = QModelIndex() );
	//! Rebuild document totals and the filtered/empty result state.
	void updateBlockListFooter();
	//! Move through the recently selected block history.
	void navigateBlockListHistory( int delta );
	//! Ctrl+G block-number/name jump.
	void goToBlock();
	//! Category test used by the compact quick-filter buttons.
	bool blockMatchesQuickFilter( int block ) const;
	//! Begin an inline, synced rename for a uniquely named scene object.
	void renameBlockListIndex( const QModelIndex & index, bool notifyIfUnavailable );

	/*! Block-list drag-and-drop (Blender's Outliner). Plain drop re-parents
	 *  preserving world position, Shift keeps the local transform, Ctrl links.
	 *  \{ */
	//! Start dragging the block-list selection. False leaves the stock drag.
	bool startBlockListDrag();
	//! The drag payload for these blocks. Shared with WW_BLOCKDND_TEST, which
	//! synthesises real drop events rather than a second idea of the format.
	//!  fromParents runs parallel to  blocks: the parent each row was dragged
	//! out of, so a block that sits under several parents moves the INSTANCE that
	//! was picked up and leaves its other placings alone. Empty means "not said".
	QMimeData * blockListDragMimeData( const QList<qint32> & blocks,
		const QList<qint32> & fromParents = QList<qint32>() ) const;
	//! DragEnter/DragMove/DragLeave/Drop on the block list's viewport.
	bool blockListDragEvent( QEvent * event );
	//! The block number under a viewport point, through whichever model is
	//! current, or -1.
	qint32 blockListBlockAt( const QPoint & viewportPos ) const;
	//! A collision shape dropped here, which makes it geometry again.
	bool blockListCollisionDrop( QDropEvent * e, qint32 shape );
	//! Is this window's block list under that GLOBAL point, and what is there?
	//! `block` is -1 for the blank space below the rows.
	bool blockListHoverAt( const QPoint & globalPos, qint32 & block ) const;
	//! The same question asked of every open document, which is what the hover
	//! probe answers. False when no block list is under the point at all.
	static bool blockListHoverResolve( const QPoint & globalPos, qint32 & block );
	//! Register the application-wide hover probe. Stateless, so it belongs to no
	//! window and survives any of them closing.
	static void installBlockListHoverProbe();
	//! WW_BLOCKDND_TEST only: answer the probe from this global point instead of
	//! the real cursor. The one step that harness steps over — the pointer
	//! cannot be moved from inside the process (see block_drag_live.ps1).
	static void wwSetHoverProbePos( const QPoint & globalPos, bool enabled );
	/*! Where a drop at this point would land.
	 *
	 *  On a row's middle it re-parents onto that block (`position` -1); near a
	 *  row's top or bottom edge it inserts into that row's PARENT at that
	 *  position, which is how a block is dragged up or down among its siblings.
	 *  `lineY` comes back >= 0 for the second case so the view can draw it.
	 */
	qint32 blockListDropSpot( const QPoint & viewportPos, int * position, int * lineY = nullptr,
		int * lineFrom = nullptr, bool * unparent = nullptr ) const;
	//! Close branches the drag opened by hovering, so a drag that merely passed
	//! over a node does not leave it unfolded. `except` survives — it is where
	//! the block landed.
	void wwCollapseBlockListBranches( const QSet<qint32> & opened, qint32 except = -1 );
	/*! The block numbers carried by a block-list drag, or empty.
	 *
	 *  PUBLIC because the Collision Manager is a second, legitimate reader: a
	 *  mesh dragged onto that dock makes collision out of it. The FORMAT stays
	 *  private to the block list — this is the only way to read it, so a payload
	 *  dropped somewhere that does not understand it is ignored rather than
	 *  half-understood, which is what the format was made private for.
	 */
public:
	QList<qint32> blockListDragPayload( const QMimeData * mime,
		QList<qint32> * fromParents = nullptr ) const;
private:
	//! Highlight (or clear, with -1) the row a drop would land on.
	void setBlockListDropTarget( qint32 block );
	/*! Select nothing at all, from a click past the last row.
	 *
	 *  Four things have to go, and only the first is the tree's: the Qt
	 *  selection, the current index, the block list published to spells, and the
	 *  OBJECT selection — which is where the row's colour comes from, so leaving
	 *  it behind left the row looking selected while the status bar said nothing
	 *  was.
	 */
	void wwClearBlockListSelection();
	/*! Which branches are open, BY BLOCK NUMBER.
	 *
	 *  A QTreeView keeps expansion against model indices, and the proxy rebuilds
	 *  wholesale whenever links change — so any structural edit closes the tree.
	 *  Block numbers survive that; proxy indices do not.
	 */
	QSet<qint32> wwOpenBlockListBranches() const;
	//! Re-open those branches, plus `alsoOpen` — so a block dropped into a node
	//! is visible where it landed rather than behind a branch that just shut.
	void wwRestoreBlockListBranches( const QSet<qint32> & open, qint32 alsoOpen = -1 );
	/*! Driven by the drag ticker: auto-scroll at the list's edges, and open a
	 *  branch the pointer rests on.
	 *
	 *  On the ticker rather than on DragMove because both are about the pointer
	 *  NOT moving — holding still at the bottom edge has to keep scrolling, and
	 *  hovering a closed node has to be timed. DragMove says nothing while the
	 *  hand is still.
	 */
	void wwBlockListDragTick();
	/*! Write every ON-SCREEN row's GLOBAL rectangle to the drag log, so a real
	 *  mouse can be driven at them from outside the process — a native drag is
	 *  the one path no synthetic event can enter.
	 *
	 *  `expandFirst` is for the live-drag script alone. It must stay false at
	 *  drag time: expanding the tree the moment a drag starts unfolds the user's
	 *  whole file under their hands.
	 */
	void wwLogBlockListRowGeometry( bool expandFirst = false );
	/*! \} */

	QWidget * filePathWidget( QWidget * );

	void setViewFont( const QFont & );

	//! Load the theme
	void loadTheme();
	//! Sync the theme actions in the UI
	void setThemeActions();
	//! Set the toolbar size
	void setToolbarSize();
	//! Set the theme
	void setTheme( nstheme::WindowTheme theme );

	//! Migrate settings from older versions of NifSkope.
	void migrateSettings() const;

	//! All QActions in the UI
	QSet<QAction *> allActions;

	nstheme::WindowTheme theme = nstheme::ThemeDark;

	QString currentFile;
	QString currentArchivePath;
	QStringList currentArchiveNames;
	BA2File * currentArchive = nullptr;

	QByteArray filehash;

	//! Stores the NIF file in memory.
	NifModel * nif;
	//! A hierarchical proxy for the NIF file.
	NifProxyModel * proxy;
	//! Stores the KFM file in memory.
	KfmModel * kfm;

	NifModel * nifEmpty;
	NifProxyModel * proxyEmpty;
	KfmModel * kfmEmpty;

	//! Guard: true while we are pushing the viewport object selection into the
	//! block list, so the list's selectionChanged handler doesn't echo back.
	bool syncingObjToList = false;

	//! Guard: true while the block list is driving the object selection, so the
	//! objectSelectionChanged mirror doesn't re-drive (and re-scroll) the list.
	bool updatingObjFromList = false;

	//! This view shows the block list.
	NifTreeView * list;
	//! Inline Block List search field.
	QLineEdit * blockListSearch = nullptr;
	QLineEdit * blockDetailsSearch = nullptr;
	QLineEdit * headerSearch = nullptr;
	QLabel * headerIdentity = nullptr;
	QLabel * headerMeta = nullptr;
	QToolButton * headerOptions = nullptr;
	QAction * headerCopySourcePath = nullptr;
	QTimer * headerSearchDebounce = nullptr;
	bool headerFilterWasActive = false;
	//! "Pinned only" toggle beside the Block Details filter.
	QToolButton * blockDetailsPinFilter = nullptr;
	//! A Block Details filter is (or just was) active; lets the empty-filter
	//! case skip the full-block walk that cost ~0.5 s per click on big shapes
	bool blockDetailsFilterWasActive = false;
	//! The blank-panel filter is on: an empty keep-set handed to the tree so
	//! that "nothing selected" shows nothing. Tracked separately from the
	//! search/pin filters because selecting a block must lift THIS one and
	//! leave those alone — and because it can outlive the model change that
	//! used to be taken as proof it was gone.
	bool wwBlankDetailsFilter = false;

	//! Pinned fields, per block TYPE -> set of field paths (see wwFieldPath).
	//! Persisted in QSettings under "BlockDetails/PinnedFields".
	QHash<QString, QSet<QString>> wwPinnedFields;
	//! While true the Block Details filter keeps ONLY pinned rows.
	bool wwPinnedOnly = false;
	//! Same idea for the block list: an inactive filter with nothing to clear
	//! must not walk (and build a searchable string for) every block.
	bool blockListFilterWasActive = false;
	QToolButton * blockListBack = nullptr;
	QToolButton * blockListForward = nullptr;
	QToolButton * blockListPin = nullptr;
	QToolButton * blockListRelations = nullptr;
	QToolButton * blockListFilterButton = nullptr;
	QMenu * blockListFilterMenu = nullptr;
	QLabel * blockListBreadcrumb = nullptr;
	QLabel * blockListFooter = nullptr;
	QVector<int> blockListHistory;
	QSet<int> blockListPins;
	int blockListHistoryPosition = -1;
	int blockListQuickFilter = 0;
	enum BlockListSearchField {
		SearchBlockNumber = 0x1,
		SearchBlockType = 0x2,
		SearchBlockName = 0x4,
		SearchDisplayedValues = 0x8
	};
	int blockListSearchFields = SearchBlockNumber | SearchBlockType
		| SearchBlockName | SearchDisplayedValues;
	bool blockListSearchMatchAllTerms = true;
	//! Unique block rows currently visible through search/category filtering; -1 means unfiltered.
	int blockListVisibleBlockCount = -1;
	//! Geometry totals are document state, not selection state; recompute only after model changes.
	bool blockListStatsDirty = true;
	int blockListStatsBlocks = 0;
	int blockListStatsShapes = 0;
	qint64 blockListStatsVertices = 0;
	qint64 blockListStatsTriangles = 0;
	//! a quick-filter chip switched the list to flat mode; All switches back
	bool blockListFilterRestoreHierarchy = false;
	bool navigatingBlockListHistory = false;
	//! This view shows the block details.
	NifTreeView * tree;
	//! Sticky Block Details view state, keyed by block type name.
	struct WwDetailsState {
		QStringList expanded;	//!< paths of expanded rows ("name|row" segments)
		int scroll = 0;
	};
	QHash<QString, WwDetailsState> wwDetailsState;
	//! Diff-vs-reference state (item sets and reference values live in NifModel).
	QPersistentModelIndex wwDiffRefIndex;
	QWidget * wwDiffBanner = nullptr;
	QLabel * wwDiffLabel = nullptr;
	bool wwDiffRecomputeQueued = false;
	//! This view shows the file header.
	NifTreeView * header;
	//! This view shows the archive browser files.
	QTreeView * bsaView;
	QAction * nifBrowserArchivesToggle = nullptr;
	QAction * nifBrowserLooseToggle = nullptr;
	QToolButton * nifBrowserFavouritesOnly = nullptr;
	//! Separate pane containing the live multi-NIF session.
	QTreeView * loadedNifsView = nullptr;
	QLineEdit * loadedNifsFilter = nullptr;

	//! This view shows the KFM file, if any.
	NifTreeView * kfmtree;

	//! Spellbook instance
	std::shared_ptr<SpellBook> book;

	static SettingsDialog * options;

	//! Help browser
	ReferenceBrowser * refrbrwsr;

	//! Transform inspect view
	InspectView * inspect;

	//! Animation timeline
	TimelineWidget * timeline = nullptr;

	//! The main window
	GLView * ogl;
	QWidget * graphicsView;

	/*! The row directly under the 3D view, holding the viewport's own controls.
	 *
	 *  Blender's viewport header, flipped to the bottom: the mode selector, the
	 *  menus that mode governs, the transform widgets, and overlays/shading. The
	 *  application topbar keeps what is not a viewport control - File, View,
	 *  Spells, Options, Help, Workspaces - plus the three popups with no Blender
	 *  counterpart.
	 *
	 *  Populated in restoreUi rather than at construction, because the toolbars
	 *  that move into it must be taken out of the toolbar area AFTER
	 *  QMainWindow::restoreState has replayed the saved layout.
	 */
	QWidget * viewportHeader = nullptr;
	QTabBar * documentTabs = nullptr;
	QList<NifSkope *> documentTabWindows;
	bool sessionPreviewVisible = true;
	bool sessionPreviewUnloaded = false;
	//! Draw this one translucent in the workspace preview — see
	//! BackgroundNifDocument::sessionPreviewGhost.
	bool sessionPreviewGhost = false;
	bool sessionCollectionMember = false;
	bool syncingLoadedNifsSelection = false;
	NifSkope * workspaceRoot = nullptr;
	bool backgroundWorkspaceDocument = false;
	//! restoreUi() has run on this window. A background window skips it, so a
	//! promoted one must never saveUi() its unrestored default layout over the
	//! user's persisted one.
	bool uiRestored = false;
	bool closingWorkspaceGroup = false;
	bool applicationEventFilterInstalled = false;
	bool configuredNifBrowserPopulated = false;
	//! NIF-browser rebuild cache: signatures of the last-built archive index /
	//! tree, so a load with unchanged resources skips the re-scan; a populate
	//! requested while the browser dock is hidden is deferred until it shows.
	QString nifBrowserIndexSignature;
	QString nifBrowserTreeSignature;
	int nifBrowserSkippedResources = 0;
	bool nifBrowserPopulatePending = false;
	QList<QPersistentModelIndex> pendingWorkspaceLoads;
	bool processingWorkspaceLoad = false;
	int configuredResourceGame = -1;
	QString configuredResourcePath;
	bool hasLoadedDocument = false;
	//! Tab toggles Object Mode against the last non-object viewport mode.
	//! Values follow the selector order: 1 Edit, 2 Vertex Paint,
	//! 3 Weight Paint, 4 Segment Paint. Edit is the initial fallback.
	int lastViewportNonObjectMode = 1;

	//! Blender-style operator (redo) panel: a floating tool window, because
	//! the native GL viewport would paint over any child-widget overlay
	QFrame * gizmoRedoPanel = nullptr;
	//! Operator redo panel (Merge by Distance / Select Linked by Angle)
	QFrame * operatorRedoPanel = nullptr;
	//! Box-select redo panel (Deselect the boxed geometry instead)
	QFrame * boxRedoPanel = nullptr;
	//! Generalized operator redo panel (typed parameter list: Extrude, …)
	QFrame * operatorExRedoPanel = nullptr;
	//! Dock the panels at the viewport's bottom-left corner
	void positionRedoPanel();

	bool selecting = false;
	bool initialShowEvent = true;

	QProgressBar * progress = nullptr;

	QDockWidget * dKfm;
	QDockWidget * dRefr;
	QDockWidget * dInsp;
	//! The four legacy core docks are consumed before restoreUi; this one stable
	//! host owns the three editor modes for the lifetime of the window.
	QDockWidget * dLeft = nullptr;
	QTabBar * leftColumnSelector = nullptr;
	QStackedWidget * leftColumnStack = nullptr;
	QSplitter * blockWorkspaceSplitter = nullptr;
	QSplitter * nifWorkspaceSplitter = nullptr;
	QWidget * loadedNifsPane = nullptr;
	LeftColumnMode leftColumnMode = LeftBlocks;
	//! dialog). Not nifSnapshotOp: some spells snapshot themselves, and a run
	//! that changes nothing must not dirty the document. See the definition.

	QDockWidget * dTimeline;

	QToolBar * tool;

	QAction * aSanitize;

	QAction * undoAction;
	QAction * redoAction;

	QActionGroup * selectActions;
	QActionGroup * showActions;
	QActionGroup * shadingActions;

	QActionGroup * gListMode;
	QAction * aList;
	QAction * aHierarchy;
	QAction * aCondition;
	QAction * aRCondition;

	QAction * aSelectFont;

	QMenu * mExport;
	QMenu * mImport;

	/*! Isolate / Hide / Restore, kept so the Object verb menu can borrow them.
	 *
	 *  These were a struck-through eye on the toolbar. Blender files them under
	 *  Object > Show/Hide (H, Alt+H), so the menu owns them now and the glyph is
	 *  gone. The QMenu itself is retained rather than the four QActions because
	 *  its aboutToShow is what retexts them for the current mode and selection
	 *  count - borrowing the actions without firing that would show stale text.
	 */
	QMenu * mViewportVisibility = nullptr;

	QAction * aRecentFilesSeparator;

	QAction * recentFileActs[NumRecentFiles];
	QAction * recentArchiveActs[NumRecentFiles];
	QAction * recentArchiveFileActs[NumRecentFiles];

	struct Settings
	{
		QLocale locale;
		bool suppressSaveConfirm;
	} cfg;

	//! The currently selected index
	QModelIndex currentIdx;

	QUndoStack * indexStack;
	//QAction * idxForwardAction;
	//QAction * idxBackAction;

	BSAModel * bsaModel;
	BSAProxyModel * bsaProxyModel;
	QStandardItemModel * loadedNifsModel = nullptr;
	QStandardItemModel * emptyModel;

	QMenu * mRecentArchiveFiles;
	//! The toolbar Open flyout (holds the recent-file actions too); kept as a
	//! member so the right-click "Open in New Window" filter can scope to it
	QMenu * mOpenFlyout = nullptr;
};


class SelectIndexCommand : public QUndoCommand
{
public:
	SelectIndexCommand( NifSkope *, const QModelIndex &, const QModelIndex & );
	void redo() override;
	void undo() override;
private:
	QModelIndex curIdx, prevIdx;

	NifSkope * nifskope;
};


//! UDP communication between instances
class IPCsocket final : public QObject
{
	Q_OBJECT

public:
	//! Creates a socket
	static IPCsocket * create( int port );

	//! Sends a command
	static void sendCommand( const QString & cmd, int port );

public slots:
	//! Acts on a command
	void execCommand( const QString & cmd );

	void openNif( const QUrl & );

	//! Opens a NIF from a URL
	void openNif( const QString & );

protected slots:
	void processDatagram();

protected:
	IPCsocket( QUdpSocket * );
	~IPCsocket();

	QUdpSocket * socket;
};

#endif
