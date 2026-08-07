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
	//! Selected workspace members as model/display-path pairs. Unlike
	//! selectedWorkspaceDocuments() this includes data-only background documents,
	//! which own a NifModel but no window; tools that only read donor geometry
	//! must use this list.
	static QList<QPair<NifModel *, QString>> selectedWorkspaceModels();
	static NifSkope * documentForModel( const NifModel * model );

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
	void saveAsDlg();

	void archiveDlg();
	void archiveFolderDlg();

	void load();
	void save();

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
	void initToolBars();
	void initMenu();
	void initConnections();
	void initDocumentSession();
	void rebuildDocumentTabs();
	void rebuildLoadedNifsBrowserGroup();
	void wireLoadedNifsSelection();
	void addNifBrowserIndexToLoaded( const QModelIndex & index );
	void queueNifBrowserIndexToLoaded( const QModelIndex & index );
	void processNextNifBrowserLoad();
	void addNifBrowserSelectionToLoaded();
	void activateDocumentTab( int index );
	void showDocumentTabMenu( const QPoint & pos );
	void showDocumentMenu( NifSkope * document, const QPoint & globalPos );
	NifSkope * documentFromBrowserIndex( const QModelIndex & index ) const;
	BackgroundNifDocument * backgroundDocumentFromBrowserIndex( const QModelIndex & index ) const;
	void showBackgroundDocumentMenu( BackgroundNifDocument * document, const QPoint & globalPos );
public:
	//! Add a loose NIF to Loaded NIFs by path (the browser route needs a
	//! QModelIndex, which a script has no way to produce).
	bool addWorkspaceDocumentFromFile( const QString & path );
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

	//! Mark a workspace document as the skeleton the rest snap to; -1 unmarks.
	bool setWorkspaceSkeletonDocument( int backgroundIndex );
	//! Block count of a workspace document, so a merge into it can be measured.
	int workspaceBlockCount( int backgroundIndex ) const;
	//! The model behind a workspace document, so a test can pose it.
	NifModel * workspaceDocumentModel( int backgroundIndex ) const;
	//! Splice workspace documents into another one in place (the "Merge Into" menu
	//! item, addressed by position so it can be scripted).
	bool mergeWorkspaceDocumentsInto( int targetIndex, const QList<int> & donorIndices );
	//! Render the Loaded NIFs list offscreen to a PNG — the row buttons can only
	//! be checked by looking at them.
	bool grabLoadedNifsView( const QString & path ) const;
private:
	//! Loaded NIFs, 2+ rows selected: offer to merge them into a new file. The
	//! FIRST row is the target, so the skeleton goes first and dictates position
	//! for everything spliced in after it. Nothing loaded is modified.
	//! Multi-row menu: bulk display settings for the selection, then the merges.
	void showSelectionMenu( const QModelIndex & clicked, const QPoint & globalPos );
	//! Merge menu. `clicked` names the target the others are spliced into.
	void mergeLoadedDocumentsMenu( const QPoint & globalPos,
	                               const QModelIndex & clicked = QModelIndex() );
	void mergeIntoLoadedDocument( const QList<QPair<QString, class NifModel *>> & picked );
	//! Ask which sequence and which instant, then bake it. Per loaded document, so
	//! each limb can be frozen at its own moment before anything is merged.
	//! Returns true when the model was changed.
	bool freezeDocumentDialog( NifModel * nif, const QString & displayName );
	//! Attach a real window to a data-only background document: create it hidden,
	//! reload the NIF from its source, then run the normal primary switch.
	void promoteBackgroundDocument( BackgroundNifDocument * document );
	void removeBackgroundDocument( BackgroundNifDocument * document );
	//! Locate a configured-resource NIF in the primary's combined archive and
	//! return its raw bytes plus the "[Game]/path" display path.
	bool extractConfiguredNifBytes( int gameID, const QString & path,
		QByteArray & bytes, QString & displayPath ) const;
	void refreshSessionPreview();
	static void refreshAllDocumentSessions();
	QString documentDisplayName() const;

	void loadFile( const QString & );
	void saveFile( const QString & );
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
	QMimeData * blockListDragMimeData( const QList<qint32> & blocks ) const;
	//! DragEnter/DragMove/DragLeave/Drop on the block list's viewport.
	bool blockListDragEvent( QEvent * event );
	//! The block number under a viewport point, through whichever model is
	//! current, or -1.
	qint32 blockListBlockAt( const QPoint & viewportPos ) const;
	/*! Where a drop at this point would land.
	 *
	 *  On a row's middle it re-parents onto that block (`position` -1); near a
	 *  row's top or bottom edge it inserts into that row's PARENT at that
	 *  position, which is how a block is dragged up or down among its siblings.
	 *  `lineY` comes back >= 0 for the second case so the view can draw it.
	 */
	qint32 blockListDropSpot( const QPoint & viewportPos, int * position, int * lineY = nullptr,
		int * lineFrom = nullptr ) const;
	//! The block numbers carried by a block-list drag, or empty.
	QList<qint32> blockListDragPayload( const QMimeData * mime ) const;
	//! Highlight (or clear, with -1) the row a drop would land on.
	void setBlockListDropTarget( qint32 block );
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
	//! "Pinned only" toggle beside the Block Details filter.
	QToolButton * blockDetailsPinFilter = nullptr;
	//! A Block Details filter is (or just was) active; lets the empty-filter
	//! case skip the full-block walk that cost ~0.5 s per click on big shapes
	bool blockDetailsFilterWasActive = false;

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
	QPushButton * nifBrowserArchivesToggle = nullptr;
	QPushButton * nifBrowserLooseToggle = nullptr;
	//! Separate lower pane containing the live multi-NIF session.
	QTreeView * loadedNifsView = nullptr;

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

	QDockWidget * dList;
	QDockWidget * dTree;
	QDockWidget * dHeader;
	QDockWidget * dKfm;
	QDockWidget * dRefr;
	QDockWidget * dInsp;
	QDockWidget * dBrowser;
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
