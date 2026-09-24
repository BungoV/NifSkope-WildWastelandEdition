/* The dope sheet of the animation workspace (lane HKXEDIT2, 2026-09-10):
   a frame ruler at the clip's own rate, one row per tracked bone (collapsible
   by the NIF's hierarchy), a marker row for the annotations, float-track rows,
   keys as diamonds, the playhead, box selection, and drag to move or copy.
   Blender's dope sheet is the reference; every divergence is stated in the
   lane report.

   The widget OWNS no data: it paints and edits an HkxClipDocument the
   workspace hands it, and asks the workspace to commit every change (so the
   undo stack is the workspace's, one command per gesture). For a NIF
   sequence it shows the interpolators' key times read-only (editing NIF keys
   stays in the old dock until the follow-up retires it). */

#ifndef ANIMDOPESHEET_H
#define ANIMDOPESHEET_H

#include "hkxclipedit.h"

#include <QColor>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QScrollBar;
class QLineEdit;

//! One row of the sheet.
struct AnimWsRow
{
	enum Kind { Markers, Group, Bone, Unbound, Float, NifTrack };
	Kind kind = Bone;
	QString label;
	QString tooltip;
	int track = -1;        //!< Bone / Unbound: the document track; NifTrack: -1
	int floatTrack = -1;   //!< Float: index into document floatTracks
	int nodeBlock = -1;    //!< Bone / NifTrack: the NiNode block in the NIF (-1 none)
	int depth = 0;         //!< hierarchy indent
	int parentRow = -1;    //!< the bone row this one hangs under (-1 top)
	bool collapsed = false;
	bool hidden = false;   //!< hidden because an ancestor is collapsed
	bool hasChildren = false;
	QVector<int> nifKeyFrames;   //!< NifTrack: key frames at the sheet's rate (read-only)
};

//! The address of an annotation on the marker row.
struct AnimWsMarkerRef
{
	int track = -1;
	int index = -1;
	bool valid() const { return track >= 0 && index >= 0; }
	bool operator==( const AnimWsMarkerRef & o ) const { return track == o.track && index == o.index; }
};

class AnimDopeSheet final : public QWidget
{
	Q_OBJECT

public:
	explicit AnimDopeSheet( QWidget * parent = nullptr );

	//! The document to paint (null = nothing; the rows are cleared).
	void setDocument( const HkxClipDocument * doc );
	const HkxClipDocument * document() const { return doc; }
	//! Rows, built by the workspace (it knows the NIF's hierarchy).
	void setRows( const QVector<AnimWsRow> & rows );
	const QVector<AnimWsRow> & rows() const { return allRows; }
	//! The ruler's rate and length when no document is set (a NIF sequence).
	void setRuler( float fps, int numFrames );
	float rulerFps() const { return fps; }
	int numFrames() const { return frames; }
	/* The clip's PLAYBACK RANGE, in frames, and everything the range does to
	 * the picture. bungo, 2026-09-12, ruling 8: "Areas inside of an animation
	 * should be as they are, outside like in Blender, darkened". Default is the
	 * whole clip (rngEnd < 0), so a clip that was never trimmed has no darkened
	 * zone at all. Ruling 9's grips on the ruler drag these two numbers. */
	int rangeStart() const;
	int rangeEnd() const;
	void setRange( int first, int last );
	/* bungo, 2026-09-12, ruling 08:2x, verbatim: "also, grey out diamond
	 * keyframes out of animations start and end range, to indicate they're not
	 * being taking into consideration anymore". A key outside
	 * rangeStart()..rangeEnd() is painted in THIS dimmed form of whatever
	 * colour it would otherwise have had -- plain, selected or active alike,
	 * so a selected key that has fallen out of the range still reads as
	 * selected while it says "ignored".
	 *
	 * The dim is the darkened band's own arithmetic applied to the INK instead
	 * of the ground: the `animOutOfRange` token composited over the colour at
	 * Blender's alpha (155/255, ANIM_draw_framerange), so a dimmed diamond
	 * looks exactly as though the out-of-range wash had been painted across
	 * it. Public and static because the harness measures the painted pixels
	 * against it rather than re-deriving the constant. */
	static QColor dimOutOfRange( const QColor & c );

	int currentFrame() const { return curFrame; }
	int currentRow() const { return curRow; }
	const QVector<HkxKeyRef> & selectedKeys() const { return selKeys; }
	/* THE ACTIVE MEMBER OF THE SELECTION -- the one last clicked. bungo,
	 * 2026-09-12, rulings 2 and 7b: the active key or annotation is orange
	 * (animKeySel), the others in the same selection are orange-red
	 * (animKeySelOther), an unselected one is plain (animKey). Three states
	 * need two pieces of state, so this is not a bool. Invalid (frame < 0)
	 * means the selection has no active member, which is what a box select
	 * leaves behind until something is clicked. */
	HkxKeyRef activeKey() const { return actKey; }
	AnimWsMarkerRef selectedMarker() const { return selMarker; }
	//! Every selected annotation; selectedMarker() is the active one of these.
	const QVector<AnimWsMarkerRef> & selectedMarkers() const { return selMarkers; }
	//! Visible rows (hidden ones skipped), as indices into rows().
	QVector<int> visibleRows() const;
	//! Keys on one row (document keys for Bone/Unbound/Float rows).
	int keyCountOnRow( int row ) const;
	//! The row of a document track, or -1.
	int rowOfTrack( int track ) const;
	int rowOfNodeBlock( int block ) const;

	QSize minimumSizeHint() const override { return { 240, 96 }; }
	QSize sizeHint() const override { return { 640, 260 }; }

	int labelWidth() const { return labelW; }
	//! x of a frame in widget coordinates, and back
	float frameToX( float frame ) const;
	float xToFrame( int x ) const;
	/* The bands' own geometry, so a gate can sample the picture where the
	   painter drew it instead of guessing: the ruler's height, the marker
	   row's height, and the y of a row's centre line (-1 when the row is
	   hidden, scrolled away or absent). */
	int rulerHeight() const { return rulerH; }
	int markerRowHeight() const { return markerRowH; }
	int rowHeight() const { return rowH; }
	int rowCenterY( int row ) const;
	/* The visible frame range (the ZOOM), so a gate can assert that an edit did
	   not throw it away -- bungo's ruling 7a. */
	float viewFirst() const { return view0; }
	float viewLast() const { return view1; }

	/* THE INLINE NAME EDITOR on the marker row (bungo's ruling 7: right-click
	 * anywhere -> "Add annotation at frame N", "opening an inline name editor
	 * on the new marker"). Nothing is added to the document until the name is
	 * committed, so Escape leaves no empty annotation behind:
	 *   beginNewMarker    -> markerNameEntered( frame, name ) on Return
	 *   beginMarkerRename -> markerRenamed( marker, name ) on Return
	 * The editor is a child QLineEdit named AnimWsMarkerNameEdit, with the
	 * graph's vocabulary as completions. -1 / an invalid ref = not editing. */
	void beginNewMarker( int frame, const QStringList & vocabulary );
	void beginMarkerRename( const AnimWsMarkerRef & marker, const QStringList & vocabulary );
	int pendingMarkerFrame() const { return newMarkerFrame; }
	AnimWsMarkerRef renamingMarker() const { return renameRef; }

public slots:
	void setCurrentFrame( int frame );
	//! Select one row (and emit rowSelected unless `quiet`).
	void selectRow( int row, bool quiet = false );
	//! Replace the key selection; `active` becomes the active key (invalid = the last of `keys`).
	void selectKeys( const QVector<HkxKeyRef> & keys, const HkxKeyRef & active = HkxKeyRef() );
	//! Select one annotation, or add it to the selection and make it active.
	void selectMarker( const AnimWsMarkerRef & m, bool additive = false );
	//! Box-select every key inside a rectangle of widget coordinates.
	void boxSelect( const QRect & r, bool additive );
	void clearSelection();
	void toggleCollapse( int row );
	void frameAll();
	void setView( float firstFrame, float lastFrame );

signals:
	//! The user scrubbed or clicked the ruler: the workspace sets the scene time.
	void frameScrubbed( int frame );
	//! A row was clicked (the workspace selects the bone in the viewport).
	void rowSelected( int row );
	//! Keys were dragged by `deltaFrames`; copy when Shift was held.
	void keysDragged( const QVector<HkxKeyRef> & keys, int deltaFrames, bool copy );
	//! The start or end grip on the ruler was dragged (bungo's ruling 9): the
	//! workspace commits the new play range as one undoable edit.
	void rangeDragged( int first, int last );
	//! A marker was dragged to a new frame.
	void markerDragged( const AnimWsMarkerRef & marker, int frame );
	//! Delete pressed with a selection.
	void deleteRequested();
	//! Double-click on a marker: rename.
	void markerActivated( const AnimWsMarkerRef & marker );
	/* Right-click ANYWHERE on the sheet (bungo's ruling 7). `row` is the row
	   under the cursor or -1 (the ruler and the marker row), `frame` is the
	   frame under the cursor, snapped and clamped to the clip -- every menu
	   entry is built from THAT frame, not from the playhead -- and `marker` is
	   the annotation under the cursor, or invalid. */
	void sheetContextMenu( int row, int frame, const AnimWsMarkerRef & marker, const QPoint & globalPos );
	//! The inline editor committed a name for a NEW annotation at `frame`.
	void markerNameEntered( int frame, const QString & name );
	//! The inline editor committed a new name for an existing annotation.
	void markerRenamed( const AnimWsMarkerRef & marker, const QString & name );
	//! M on the sheet: the workspace opens the inline editor at `frame`.
	void markerAddRequested( int frame );
	//! Ctrl+M on the sheet: the workspace opens the editor on this annotation.
	void markerRenameRequested( const AnimWsMarkerRef & marker );
	void selectionChanged();

protected:
	void paintEvent( QPaintEvent * ) override;
	void mousePressEvent( QMouseEvent * ) override;
	void mouseMoveEvent( QMouseEvent * ) override;
	void mouseReleaseEvent( QMouseEvent * ) override;
	void mouseDoubleClickEvent( QMouseEvent * ) override;
	void wheelEvent( QWheelEvent * ) override;
	void keyPressEvent( QKeyEvent * ) override;
	void resizeEvent( QResizeEvent * ) override;
	void contextMenuEvent( QContextMenuEvent * ) override;
	//! Escape / focus-out on the inline name editor.
	bool eventFilter( QObject * o, QEvent * e ) override;

private:
	void rebuildVisible();
	int rowAtY( int y ) const;          //!< visible row index at y (-1 ruler / none)
	int rowTop( int visibleIndex ) const;
	bool keyAt( int row, int x, HkxKeyRef & out ) const;
	bool markerAt( int x, int y, AnimWsMarkerRef & out ) const;
	//! Which range grip is under (x, y) in the ruler: -1 none, 0 start, 1 end.
	int gripAt( int x, int y ) const;
	void updateScroll();
	void pruneSelection();
	void openNameEdit( const QString & text, const QStringList & vocabulary );
	void positionNameEdit();
	void commitNameEdit();
	void cancelNameEdit();
	QString niceFrameStep( int & step ) const;

	const HkxClipDocument * doc = nullptr;
	QVector<AnimWsRow> allRows;
	QVector<int> visible;
	float fps = 30.0f;
	int frames = 1;
	int curFrame = 0;
	int curRow = -1;
	QVector<HkxKeyRef> selKeys;
	HkxKeyRef actKey;                   //!< the active key (frame < 0 = none)
	AnimWsMarkerRef selMarker;          //!< the ACTIVE annotation of selMarkers
	QVector<AnimWsMarkerRef> selMarkers;
	int rngStart = 0;                   //!< playback range, first frame
	int rngEnd = -1;                    //!< playback range, last frame (-1 = the whole clip)

	// view
	float view0 = 0.0f, view1 = 1.0f;  //!< frames at the left and right edge of the key area
	int labelW = 200;
	int rulerH = 22;
	int rowH = 18;
	int markerRowH = 22;
	int scrollRow = 0;                  //!< first visible row shown
	QScrollBar * vbar = nullptr;

	// the inline name editor on the marker row
	QLineEdit * nameEdit = nullptr;
	int newMarkerFrame = -1;            //!< the ghost marker being named (-1 none)
	AnimWsMarkerRef renameRef;          //!< the annotation being renamed

	// gestures
	enum Drag { DragNone, DragScrub, DragKeys, DragBox, DragMarker, DragPan, DragRangeStart, DragRangeEnd };
	Drag drag = DragNone;
	QPoint dragStart;
	int dragStartFrame = 0;
	int dragLastDelta = 0;
	bool dragCopy = false;
	bool dragMoved = false;
	QRect boxRect;
	AnimWsMarkerRef dragMarker;
	float panView0 = 0.0f;
	int dragRange0 = 0, dragRange1 = 0;   //!< the range as the grip drag began
};

#endif // ANIMDOPESHEET_H
