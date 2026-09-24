/* THE ANIMATION WORKSPACE -- lane HKXEDIT2, 2026-09-10. Replaces the
   Animation Manager dock (bungo, verbatim: "Animation manager was one of the
   first features for nifskope, and it's pretty old and outdated btw";
   "Just make hkx fully editable in our nifskope"). Blender's timeline + dope
   sheet are the reference; divergences are listed in the lane report.

   ONE LIST at the side: the NIF's NiControllerSequence blocks and the loaded
   .hkx clips, distinct glyphs, one selection -- selecting drives the scene
   (GLView::setSceneSequence for a sequence, WwHkxAnimHub::activate for a
   clip, which MEASURES whether it bound). Under the list, the settings bands
   (nifskope-ww-panel-style): the old manager's per-sequence controls as rows
   (Cycle Type, Frequency, Start Time, Stop Time -- written to the NIF through
   ChangeValueCommand on the NIF's own undo stack), and the edit rows (reduce
   tolerance, annotation name from the vocabulary or free text, trim range,
   retime rate, root-motion track, float-track value). The DOPE SHEET fills
   the rest: the ruler at the clip's own rate, the marker row, one row per
   tracked bone under the NIF's hierarchy (collapsible), unbound tracks under
   a folded group, float tracks, the playhead. The transport is one row
   (to start, previous key, play back, play, stop, next key, to end, loop,
   speed, rate, frame field, readout). The summary-or-refusal line and the
   action bar are pinned under everything.

   EDITING is the document's (src/hkxclipedit.h): the workspace owns one
   HkxClipDocument per loaded clip, wraps every operation in ONE undo command
   holding the document before and after (a snapshot -- exact, which is what
   gate (h) needs), and after every change hands the clip back to the
   playback (HkxPlayback::replaceClip), so the viewport shows the edit at the
   current frame. Pose a bone with the viewport's gizmo: while the "Pose"
   row is on, the selected bone's node is HELD out of the clip's pose so the
   gizmo's result is what you see; Insert key reads the bone's local TRS off
   the NIF block, keys it at the playhead, and puts the block back.

   THE .hkx DOCUMENT (lane HKXEDIT1's HkxModel, the Blocks tab) and this
   workspace edit the same clip: with WW_ANIMWS_HKXMODEL (the hook-up
   defines it) a Blocks-tab edit re-decodes the clip into the workspace, and
   Save from the workspace reloads the Blocks tab from the saved bytes.
   Undo/Redo across the window is one QUndoGroup (wwAnimUndoGroup()): the
   NIF's stack, the .hkx model's, and this workspace's; the active stack
   follows keyboard focus. */

#ifndef ANIMWORKSPACE_H
#define ANIMWORKSPACE_H

#include "animdopesheet.h"
#include "hkxclipedit.h"

#include <QHash>
#include <QPersistentModelIndex>
#include <QWidget>

#include <functional>

class GLView;
class NifModel;
class HkxPlayback;
class QAction;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidgetItem;
class QListWidget;
class QMenu;
class QMenuBar;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QSplitter;
class QTimer;
class QToolButton;
class QUndoGroup;
class QUndoStack;
class QWidget;
#ifdef WW_ANIMWS_HKXMODEL
class HkxModel;
#endif

//! The one undo group of the window (the hook-up adds the NIF's and the .hkx model's stacks).
QUndoGroup * wwAnimUndoGroup();

class AnimWorkspace final : public QWidget
{
	Q_OBJECT

public:
	explicit AnimWorkspace( QWidget * parent = nullptr );
	~AnimWorkspace();

	void setNif( NifModel * nif );
	void setGLView( GLView * view );
	//! Mirror the render toolbar's loop / switch-animation actions.
	void addAnimActions( QAction * loop, QAction * sw );
#ifdef WW_ANIMWS_HKXMODEL
	//! The Blocks-tab model of an .hkx document (lane HKXEDIT1).
	void setHkxModel( HkxModel * model );
#endif
	/*! Bind Edit > Undo / Redo to the group: the workspace's stack is active
	 *  while keyboard focus is inside this widget, `nifStack` (or `hkxStack`
	 *  when the .hkx model owns the block views) otherwise. One line in the
	 *  hook-up. */
	void installUndoGroup( QUndoStack * nifStack, QUndoStack * hkxStack );
	QUndoStack * undoStack() const { return undo; }

	QSize sizeHint() const override;

	// ---- readers for the harness (object names cover the rest)
	AnimDopeSheet * dopeSheet() const { return sheet; }
	QString selectedEntry() const { return curEntry; }
	bool selectedIsClip() const { return curIsClip; }
	const HkxClipDocument * document() const;
	int currentFrame() const;
	//! Bone rows (bound), unbound rows, float rows on the sheet.
	int boneRowCount() const;
	int unboundRowCount() const;
	int floatRowCount() const;
	int selectedTrack() const;
	int selectedNodeBlock() const;
	//! The summary line's text and whether it is a refusal.
	QString noteText() const;
	/*! What the summary line's TOOLTIP carries -- the full sentence the label
	 *  folds (lane UI6, 2026-09-11; bungo: "look at all this text clutter").
	 *  Nothing is lost, only folded, and the gate reads both. */
	QString noteDetail() const;
	bool noteIsRefusal() const { return noteRefusal; }
	//! The annotation vocabulary offered (res/hkx_annotation_vocabulary.txt + the clip's own).
	QStringList vocabulary() const;

signals:
	void indexSelected( const QModelIndex & );
	void timeChanged( float );
	void sequenceActivated( const QString & );
	//! dir: 1 forward, -1 reverse, 0 stop (the application owns the clock)
	void playPauseRequested( int dir );
	void isolateBlock( int blockNumber );

public slots:
	void refresh();
	void refreshLater();
	//! Scene time from GLView
	void setTime( float t, float mn, float mx );
	void setPlayingState( bool playing, bool reverse );
	//! An index selected elsewhere: select its bone row (viewport -> sheet).
	void setCurrentIndex( const QModelIndex & );
	//! A sequence change made elsewhere (the render toolbar).
	void setSequenceByName( const QString & name );
	//! GLView::transformCommitted with auto-key on: key the node at the playhead.
	void keyNodeTransform( int nodeBlock );

	// ---- the actions (buttons call these; so does the harness)
	void insertKeyAtPlayhead();
	/* THE CLICKED FRAME (bungo's ruling 7, 2026-09-12: "why can't I right
	   click and insert an annotation anywhere?"). Every right-click entry is
	   built from the frame under the cursor; the playhead versions above are
	   these with currentFrame() passed in. */
	void insertKeyAtFrame( int frame );
	void addAnnotationAtFrame( int frame, const QString & name );
	void deleteSelected();
	void reduceKeys();
	void addAnnotationAtPlayhead();
	void renameSelectedAnnotation();
	void deleteSelectedAnnotation();
	//! Cut the clip down to the play range now (what the Trim button was).
	void trimToRange();
	/* THE PLAY RANGE (bungo's ruling 9, 2026-09-12). The Start and End boxes in
	   the transport row and the two grips on the sheet's ruler are the same two
	   numbers; either commits one undoable edit on the document, and a save
	   writes the range alone. These replace the old Trim from / Trim to rows. */
	void setRangeFromBoxes();
	void retimeClip();
	void bakeRootMotion();
	void unbakeRootMotion();
	void removeSelectedTrack();
	/* REMOVE TRANSFORM AXES (bungo's ruling 3, 2026-09-12): hold the ticked
	   directions of one track still at their frame-0 value. The dialog is the
	   hand's way in; applyAxisStrip is the one the menu, the dialog and the
	   harness all end at, so none of them can drift. */
	void removeTransformAxesDialog();
	void applyAxisStrip( int track, const HkxAxisMask & mask );
	void renameSelectedTrack();
	void addFloatTrack();
	void setFloatKeyAtPlayhead();
	void saveClip();
	void saveClipAs();
	//! Apply an edited document to an entry (the undo commands call this).
	void applyDocument( const QString & entry, const HkxClipDocument & d, const QString & what );
	void hkxChooseFile();

	/* THE ANIMATIONS LIST (bungo's ruling 5, 2026-09-12), verbatim: "For these
	   animations, I should be able to use a shortcut to delete, copy, paste,
	   etc. them, same goes with reordering with a drag and drop, same goes with
	   right clicking and selecting each such option."

	   Each of these is reached three ways -- a shortcut on the list, an entry
	   in its right-click menu, and (for the order) a drag with a drop line --
	   and all three call the SAME slot, which calls the hub, so they cannot
	   drift apart. They act on every selected row; rows that are the NIF's own
	   sequences refuse with one sentence, because those live in NIF blocks and
	   are edited in the Blocks tab. */
	void listDelete();
	void listCopy();
	void listCut();
	void listPaste();
	void listDuplicate();
	void listRename();
	void listSelectAll();
	void listMoveUp();
	void listMoveDown();
	/*! Open or close the right-side panel (bungo's ruling 6a).
	 *  PUBLIC so a harness can force the state it measures instead of
	 *  inheriting whatever QSettings remembered from the last session. */
	void setSidePanelOpen( bool on );

private slots:
	void listRowChosen();
	void sheetRowSelected( int row );
	void sheetFrameScrubbed( int frame );
	void sheetKeysDragged( const QVector<HkxKeyRef> & keys, int deltaFrames, bool copy );
	void sheetMarkerDragged( const AnimWsMarkerRef & marker, int frame );
	void sheetRangeDragged( int first, int last );
	void sheetMarkerActivated( const AnimWsMarkerRef & marker );
	void sheetContextMenu( int row, int frame, const AnimWsMarkerRef & marker, const QPoint & globalPos );
	//! The sheet's inline editor named a new annotation / renamed one.
	void sheetMarkerNameEntered( int frame, const QString & name );
	void sheetMarkerRenamed( const AnimWsMarkerRef & marker, const QString & name );
	//! M / Ctrl+M on the sheet: open the inline editor.
	void sheetMarkerAddRequested( int frame );
	void sheetMarkerRenameRequested( const AnimWsMarkerRef & marker );
	void sequenceFieldEdited();
	//! The Track section's Bone field: renames the selected track (one undo step).
	void trackNameEdited();
	void poseToggled( bool on );
	void clipsChanged();
	void updateReadout();
	void refreshSummary();
	void rebuildUnloadMenu();
	void hkxModelChanged();
	// ---- the Animations list (ruling 5)
	void listContextMenu( const QPoint & pos );
	void listItemChanged( QListWidgetItem * item );
	void listItemDoubleClicked( QListWidgetItem * item );
	//! After a drop: push what the eye sees into the hub's order.
	void commitListOrder();

protected:
	void showEvent( QShowEvent * event ) override;
	//! The list's viewport, for the drop that ends a drag-and-drop reorder.
	bool eventFilter( QObject * watched, QEvent * event ) override;

private:
	void buildUi();
	QWidget * buildTransport();
	QWidget * buildSettings();
	//! Every former button, as a QAction under the same object name (ruling 6).
	void buildActions();
	//! The dock's header: the menu bar that holds them, and the panel toggle.
	QWidget * buildMenuBar();
	//! Show the one section the current selection is about, and no others.
	void updateSections();
	void rebuildList();
	void rebuildRows();
	//! Tell the scene the clip plays only its play range (Scene::timeMin/timeMax
	//! answer from animTags, so the scrub bar and the wrap follow with no new
	//! transport code) -- bungo's ruling 9.
	void pushRangeToScene();
	void buildNifRows( const QModelIndex & iSeq );
	void selectEntry( const QString & name, bool isClip, bool drive );
	HkxClipDocument * docFor( const QString & entry, bool create );
	HkxPlayback * playback() const;
	//! Run one edit as one undo command: `op` mutates the copy; refused = nothing pushed.
	bool edit( const QString & what, const std::function<HkxEditResult( HkxClipDocument & )> & op );
	void say( const QString & text, bool refusal, const QString & detail = QString() );
	int frameOf( float t ) const;
	float timeOf( int frame ) const;
	float sheetFps() const;
	//! The bone's local TRS as the NIF block holds it (what the gizmo wrote).
	bool readBlockTransform( int nodeBlock, HkxTransform & out ) const;
	void writeBlockTransform( int nodeBlock, const HkxTransform & xf );
	void holdSelectedBone( bool on );
	void loadVocabulary();
	void pushSequenceField( const QModelIndex & iSeq, const char * field, const QVariant & v );
	QStringList trackNamesOf( const QString & entry ) const;
	//! Move every selected loaded clip one row up (-1) or down (+1).
	void listMoveBy( int delta );
	//! The selected loaded clips, in the order the list shows them.
	QStringList selectedClipNames() const;
	//! Put the cursor on a row by name (and drive the scene with it).
	void selectListRow( const QString & name );

	/* WHAT A COPY KEEPS: the clip and the names its tracks carry, not the
	   binding. A paste works the binding out again against the NIF that is
	   open now, which is what makes a clip copied before a NIF was opened
	   still useful after. */
	struct ClipCopy
	{
		QString name, path, skeletonSource;
		HkxAnimClip clip;
		QStringList trackBone;
		bool additive = false;
	};
	QVector<ClipCopy> listClipboard;
	QListWidgetItem * renameItem = nullptr;   //!< the row being renamed in place
	QString renameFrom;                        //!< its name before the edit

	NifModel * nif = nullptr;
	GLView * glView = nullptr;
#ifdef WW_ANIMWS_HKXMODEL
	HkxModel * hkxModel = nullptr;
	QString hkxModelEntry;   //!< the list entry that is the .hkx document's clip ("" none)
#endif
	QUndoStack * undo = nullptr;
	QUndoStack * nifUndo = nullptr;
	QUndoStack * hkxUndo = nullptr;

	QHash<QString, HkxClipDocument *> docs;   //!< owned; one per loaded clip, made on first selection
	QString curEntry;
	bool curIsClip = false;
	QPersistentModelIndex curSeq;      //!< the selected NIF sequence block
	QVector<QPersistentModelIndex> sequences;
	QStringList clipEntries;
	QStringList vocab;
	bool syncing = false;
	bool noteRefusal = false;
	float curTime = 0.0f, sceneMin = 0.0f, sceneMax = 0.0f;
	int heldBlock = -1;
	HkxTransform heldBind;             //!< the block's TRS before the hold, put back after the key
	bool refreshPending = false;
	QTimer * refreshTimer = nullptr;

	// widgets
	QSplitter * split = nullptr;
	QListWidget * list = nullptr;
	QToolButton * btnLoadAnim = nullptr;
	QToolButton * btnUnloadAnim = nullptr;
	QMenu * unloadMenu = nullptr;
	QToolButton * btnRootMotion = nullptr;
	QScrollArea * settingsArea = nullptr;
	QWidget * seqSection = nullptr;
	QComboBox * cycleBox = nullptr;
	QDoubleSpinBox * freqBox = nullptr;
	QDoubleSpinBox * startBox = nullptr;
	QDoubleSpinBox * stopBox = nullptr;
	QWidget * editSection = nullptr;
	/* ---- ruling 6: one section per KIND of selection, inside editSection. */
	QWidget * clipSection = nullptr;
	QWidget * keySection = nullptr;
	QWidget * annotSection = nullptr;
	QWidget * trackSection = nullptr;
	QWidget * floatSection = nullptr;
	QLineEdit * trackNameBox = nullptr;
	QDoubleSpinBox * tolTransBox = nullptr;
	QDoubleSpinBox * tolDegBox = nullptr;
	QComboBox * annotBox = nullptr;
	QSpinBox * rangeStartBox = nullptr;   //!< the play range, in the transport row
	QSpinBox * rangeEndBox = nullptr;
	QDoubleSpinBox * retimeBox = nullptr;
	QComboBox * rootTrackBox = nullptr;
	QDoubleSpinBox * floatValueBox = nullptr;
	/* ruling 6: the two gizmo switches are transport-row toggles now, where
	   Blender keeps its auto-key record button. Same object names. */
	QToolButton * poseCheck = nullptr;
	QToolButton * autoKeyCheck = nullptr;
	AnimDopeSheet * sheet = nullptr;
	// transport
	QToolButton * btnToStart = nullptr;
	QToolButton * btnPrevKey = nullptr;
	QToolButton * btnPlayBack = nullptr;
	QToolButton * btnPlay = nullptr;
	QToolButton * btnStop = nullptr;
	QToolButton * btnNextKey = nullptr;
	QToolButton * btnToEnd = nullptr;
	QToolButton * btnLoop = nullptr;
	QDoubleSpinBox * speedBox = nullptr;
	QLabel * rateLabel = nullptr;
	QSpinBox * frameBox = nullptr;
	QLabel * readout = nullptr;
	QAction * loopAction = nullptr;
	QAction * switchAction = nullptr;
	// header (ruling 6) and the right-side panel (ruling 6a)
	QMenuBar * headerMenu = nullptr;
	QToolButton * btnSidePanel = nullptr;
	QWidget * sidePanel = nullptr;
	int sidePanelWidth = 260;          //!< remembered across sessions
	// pinned
	QLabel * note = nullptr;
	/* ---- RULING 6, verbatim: "The bottom button row goes away; every action
	   stays reachable by menu, context menu and shortcut." These are the same
	   fifteen actions the buttons were, under the SAME object names, so every
	   gate and every context menu that asked for one by name still finds it. */
	QAction * actInsertKey = nullptr;
	QAction * actDeleteKeys = nullptr;
	QAction * actReduce = nullptr;
	QAction * actAddAnnot = nullptr;
	QAction * actRenameAnnot = nullptr;
	QAction * actDeleteAnnot = nullptr;
	QAction * actTrim = nullptr;
	QAction * actRetime = nullptr;
	QAction * actBake = nullptr;
	QAction * actUnbake = nullptr;
	QAction * actRemoveTrack = nullptr;
	QAction * actRenameTrack = nullptr;
	QAction * actRemoveAxes = nullptr;
	QAction * actAddFloat = nullptr;
	QAction * actSetFloat = nullptr;
	QAction * actSave = nullptr;
	QAction * actSaveAs = nullptr;
	QAction * actFrameAll = nullptr;
	QAction * actSidePanel = nullptr;
};

/*! WW_ANIMWS_TEST: lane HKXEDIT2's gates, run inside the real application
 *  (src/animworkspacetest.cpp). Does nothing unless WW_ANIMWS_TEST is set. */
void wwAnimWorkspaceHarness( class NifSkope * skope );

#endif // ANIMWORKSPACE_H
