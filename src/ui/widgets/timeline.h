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

#ifndef TIMELINE_H
#define TIMELINE_H

#include <QWidget> // Inherited
#include <QHash>
#include <QImage>
#include <QPersistentModelIndex>
#include <QSet>
#include <QString>
#include <QVector>

#include <functional>

//! @file timeline.h TimelineWidget, TimelineLanesView, TimelineGraphView, TimelineInspector

class NifModel;

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QMenu;
class QScrollArea;
class QScrollBar;
class QSplitter;
class QTimer;
class QToolButton;
class QVBoxLayout;

class TimelineLanesView;
class TimelineGraphView;
class TimelineInspector;

//! One keyframe marker on the timeline
struct TimelineKey
{
	float time = 0;
	QPersistentModelIndex idx; //!< The key row inside the Keys array
	QString text;              //!< Only set for text keys
};

//! One plottable channel of an interpolator (a KeyGroup, quaternion key array or text key array)
struct TimelineChannel
{
	enum ValueType
	{
		Float, Vector3Val, Color3Val, Color4Val, BoolVal, QuatVal, TextVal, Unknown
	};

	QString name;                      //!< e.g. "Translations", "Rot X"
	QPersistentModelIndex iKeyGroup;   //!< Parent struct passed to Controller::interpolate (invalid for quat/text)
	QPersistentModelIndex iKeysArray;  //!< The Keys array itself
	ValueType type = Unknown;
	int interpolation = 1;             //!< KeyType (1 = linear, 2 = quadratic, 3 = TBC, 4 = XYZ, 5 = const)
	int numComponents = 1;
	QVector<TimelineKey> keys;

	bool isColor() const { return type == Color3Val || type == Color4Val; }
	bool plottable() const { return type != TextVal && type != Unknown; }
};

//! One row of the timeline
struct TimelineLane
{
	enum Viz
	{
		VizAuto = 0, VizDiamonds, VizSparkline, VizStrip
	};

	QString label;
	QString tooltip;
	QPersistentModelIndex iSelect;     //!< Block selected in the tree when the lane is clicked
	QPersistentModelIndex iController; //!< Owning controller block, if known
	QVector<TimelineChannel> channels;
	QVector<TimelineKey> keys;         //!< Merged keys of all channels for the lane row
	bool rangeOnly = false;            //!< B-spline etc. without discrete keys
	bool hasCtrlRange = false;
	float start = 0, stop = 0;         //!< Controller (or B-spline) start/stop
	QSet<int> blockNums;               //!< Block numbers belonging to this lane (for external selection)
	int targetBlock = -1;              //!< Block number of the animated NiAVObject, if known
	QString searchText;                //!< Lowercased haystack for the filter box
	Viz viz = VizAuto;
	bool muted = false;                //!< Controller Active flag off
	bool locked = false;               //!< UI-only edit lock
	bool isHeader = false;             //!< Collapsible sequence group header row
	int groupSeq = -1;                 //!< Block number of the owning sequence (-1 = standalone)
};

//! In-memory copy of one key, used for editing, clipboard and CSV round trips
struct TimelineKeyData
{
	float time = 0;
	QVector<float> comps;
	QVector<float> fwd, bwd;           //!< Quadratic tangents per component
	float tbc[3] = { 0, 0, 0 };
	QString text;
};

//! Whole-channel clipboard entry
struct TimelineClipChannel
{
	TimelineChannel::ValueType type = TimelineChannel::Unknown;
	int interpolation = 1;
	int numComponents = 1;
	QString name;
	QVector<TimelineKeyData> keys;
};

//! A lint finding
struct TimelineLintItem
{
	QString text;
	QPersistentModelIndex idx;
	bool isNameMismatch = false;
	QPersistentModelIndex iNameField;  //!< The string field holding the broken name
};

//! Dockable animation timeline with keyframe lanes, a value graph and an inspector
class TimelineWidget final : public QWidget
{
	Q_OBJECT

	friend class TimelineLanesView;
	friend class TimelineGraphView;
	friend class TimelineInspector;

public:
	explicit TimelineWidget( QWidget * parent = nullptr );
	~TimelineWidget();

	void setNif( NifModel * nif );

	QSize sizeHint() const override;

signals:
	//! Request selection of an index in the main window
	void indexSelected( const QModelIndex & );
	//! Request a scene time change
	void timeChanged( float );
	//! Request the scene sequence to change (name as in Scene::animGroups)
	void sequenceActivated( const QString & );
	//! Request animation play/pause toggle
	void playPauseRequested();
	//! Request viewport isolation of a block (-1 clears it)
	void isolateBlock( int blockNumber );

public slots:
	//! Rescan the model and rebuild all lanes
	void refresh();
	//! Compressed refresh (via single shot timer)
	void refreshLater();
	//! Scene time update from GLView
	void setTime( float t, float mn, float mx );
	//! Highlight the lane / key corresponding to an externally selected index
	void setCurrentIndex( const QModelIndex & );
	//! Follow a sequence change made elsewhere (anim toolbar)
	void setSequenceByName( const QString & name );

protected slots:
	void sequenceChosen( int comboRow );
	void filterEdited( const QString & text );
	void runLint();

protected:
	// ---- scanning (timeline.cpp)
	void scanModel();
	void buildLanes();
	void addControllerLanes( const QModelIndex & iController );
	void addInterpolatorLane( const QModelIndex & iInterp, const QString & label, const QModelIndex & iController = QModelIndex() );
	void addSequenceLanes( const QModelIndex & iSeq, bool withHeader, QSet<int> * seen = nullptr );
	void collectMarkers( const QModelIndex & iTextKeys );
	void toggleSeqCollapse( int seqBlock );
	void collectChannels( const QModelIndex & iParent, TimelineLane & lane, int depth = 0 );
	void addKeyGroupChannel( const QModelIndex & iKeyGroup, TimelineLane & lane, const QString & name );
	void finalizeLane( TimelineLane & lane );
	void computeRange();
	void applyFilter();
	QString controllerLabel( const QModelIndex & iController ) const;
	static QString shortTypeName( QString type );
	int findAVObjectByName( const QString & name ) const;

	// ---- view state helpers (timeline.cpp)
	float timeToX( float t, int width ) const;
	float xToTime( int x, int width ) const;
	float snapTime( float t ) const;
	float snapValue( float v ) const;
	QString formatTime( float t, float step ) const;
	void zoomAt( float t, float factor );
	void frameAll();
	void frameSelected();
	void ensurePlayheadVisible();
	void updateViews();
	int laneRow( int visibleRow ) const;    //!< visible row -> lanes index
	int visibleRowOf( int lane ) const;     //!< lanes index -> visible row (-1 if filtered out)

	// ---- selection (timeline.cpp)
	void selectLane( int lane, bool emitSelect );
	void selectKey( const QModelIndex & keyIdx, bool additive, bool emitSelect = true );
	void clearKeySelection();
	bool findKeyRef( const QModelIndex & keyIdx, int & lane, int & channel, int & key ) const;
	void stepToKey( int dir );
	void jumpToMarker( int dir );

	// ---- editing (timelineedit.cpp)
	QVector<TimelineKeyData> readChannelKeys( const TimelineChannel & ch ) const;
	void writeChannelKeys( const TimelineChannel & ch, const QVector<TimelineKeyData> & keys, int newInterp = -1 );
	bool snapshotOp( const QString & description, const std::function<void()> & op );
	void pushFieldChange( const QModelIndex & fieldIdx, const QVariant & v, bool newTransaction = false );
	float keyComponentValue( const TimelineChannel & ch, const QModelIndex & keyIdx, int comp ) const;
	void setKeyComponentValue( const TimelineChannel & ch, const QModelIndex & keyIdx, int comp, float v );
	float keyTangentValue( const TimelineChannel & ch, const QModelIndex & keyIdx, int comp, bool backward ) const;
	void setKeyTangent( const TimelineChannel & ch, const QModelIndex & keyIdx, int comp, bool backward, float v );
	void setKeyTime( const TimelineChannel & ch, int keyIndex, float t );
	float clampKeyTime( const TimelineChannel & ch, int keyIndex, float t ) const;

	void deleteSelectedKeys();
	void clearChannelKeys( int lane );
	void insertKeyAtTime( int lane, float time );
	void duplicateSelectedKeys();
	void copySelectedKeys();
	void pasteKeysAt( float time );
	void copyChannels( int lane );
	void pasteChannels( int lane );
	void scaleSelectedKeys( float factor );
	void applyEasing( int mode );          //!< 0 flatten, 1 smooth, 2 linearize, 3 ease in, 4 ease out
	void setChannelInterpolation( int lane, int channel, int newType, bool smooth );
	void nudgeSelectedKeys( float dt, int dvSteps );
	void toggleLaneMute( int lane );
	void addTextKeyMarker( float time );

	void csvExport( int lane );
	void csvImport( int lane );

	QVector<TimelineLintItem> lintScan() const;

	// ---- settings (timelineedit.cpp)
	void loadSettings();
	void saveSettings() const;

	void keyPressEvent( QKeyEvent * event ) override;
	void showLaneContextMenu( int lane, const QPoint & globalPos );

	NifModel * nif = nullptr;

	QVector<TimelineLane> lanes;
	QVector<QPersistentModelIndex> sequences;
	QVector<int> visibleLanes;
	QHash<QString, int> avObjectsByName;

	// Text keys shown as global ruler markers (not lanes)
	QVector<TimelineKey> markers;
	TimelineChannel markerChannel;     //!< Writable text channel for adding markers
	QSet<int> collapsedSeqs;

	int currentLane = -1;
	QVector<QPersistentModelIndex> selKeys;   //!< All selected keys, primary first
	QPersistentModelIndex primaryKey;

	float tMin = 0, tMax = 1;
	float viewT0 = 0, viewT1 = 1;
	float curTime = 0;
	float sceneMin = 0, sceneMax = 0;

	// options
	bool snapOn = false;
	float snapTimeStep = 0.05f;
	float snapValueStep = 0.1f;
	bool framesMode = false;
	int fps = 30;
	bool normalized = false;
	bool followPlayhead = true;
	bool autoIsolate = false;              //!< Filter lanes by selected geometry
	bool prevRangeOn = false;
	float prevStart = 0, prevEnd = 1;
	int labelW = 240;
	QString filterText;
	int filterTargetBlock = -1;

	// channel clipboard (whole channels)
	QVector<TimelineClipChannel> channelClipboard;
	// key clipboard (selected keys, per channel)
	QVector<TimelineClipChannel> keyClipboard;
	float keyClipboardRefTime = 0;

	// local transport (drives scene time directly; independent of the anim toolbar)
	void transportToggle( int dir );
	void transportStop();
	QTimer * playTimer = nullptr;
	int playDir = 0;

	// widgets
	QComboBox * seqBox;
	QLineEdit * filterBox;
	QToolButton * btnToStart;
	QToolButton * btnPlayBack;
	QToolButton * btnPlay;
	QToolButton * btnToEnd;
	QLineEdit * timeField;
	QToolButton * btnFrames;
	QToolButton * btnSnap;
	QDoubleSpinBox * snapTimeBox;
	QDoubleSpinBox * snapValueBox;
	QToolButton * btnNormalize;
	QToolButton * btnFollow;
	QToolButton * btnIsolate;
	QToolButton * btnLint;
	QToolButton * btnInspector;
	QLabel * infoLabel;
	QSplitter * split;
	TimelineLanesView * lanesView;
	TimelineGraphView * graphView;
	TimelineInspector * inspector;
	QScrollBar * laneScroll;
	QTimer * refreshTimer;

	bool scanning = false;
	bool syncingSequence = false;
};

//! The upper pane: time ruler, summary row and one row per interpolator
class TimelineLanesView final : public QWidget
{
	Q_OBJECT

public:
	explicit TimelineLanesView( TimelineWidget * parent );

	QSize minimumSizeHint() const override { return { 200, 64 }; }

	void invalidateStrips() { stripCache.clear(); update(); }

protected:
	void paintEvent( QPaintEvent * ) override;
	void mousePressEvent( QMouseEvent * ) override;
	void mouseMoveEvent( QMouseEvent * ) override;
	void mouseReleaseEvent( QMouseEvent * ) override;
	void mouseDoubleClickEvent( QMouseEvent * ) override;
	void wheelEvent( QWheelEvent * ) override;
	void contextMenuEvent( QContextMenuEvent * ) override;
	void leaveEvent( QEvent * ) override;

	void paintLaneContent( QPainter & p, const TimelineLane & lane, int laneIdx, const QRect & r );
	QImage laneStrip( const TimelineLane & lane, int laneIdx, int w, int h );

	int rowAtY( int y ) const;              //!< visible row index at y, -1 ruler, -2 summary
	QModelIndex keyAt( int lane, int x ) const;
	void scrub( int x );

	TimelineWidget * tl;

	enum DragMode
	{
		DragNone, DragScrub, DragKeys, DragRubber, DragGutter, DragPrevRange, DragCtrlRange, DragPan
	};
	DragMode dragMode = DragNone;
	QPoint dragStart;
	float dragStartTime = 0;
	float dragLastTime = 0;
	bool dragMoved = false;
	int dragLane = -1;
	int dragRangeEdge = 0;                  //!< 0 start edge, 1 stop edge
	QRect rubberRect;
	QVector<QPair<QPersistentModelIndex, float>> dragOrigTimes;

	QHash<quint64, QImage> stripCache;
	int hoverRow = -1;
	int marqueeOffset = 0;
	QTimer * marqueeTimer;
};

//! The lower pane: value curves for the selected lane
class TimelineGraphView final : public QWidget
{
	Q_OBJECT

public:
	explicit TimelineGraphView( TimelineWidget * parent );

	QSize minimumSizeHint() const override { return { 200, 48 }; }

	void invalidateCurves() { curvesDirty = true; update(); }

protected:
	void paintEvent( QPaintEvent * ) override;
	void mousePressEvent( QMouseEvent * ) override;
	void mouseMoveEvent( QMouseEvent * ) override;
	void mouseReleaseEvent( QMouseEvent * ) override;
	void mouseDoubleClickEvent( QMouseEvent * ) override;
	void wheelEvent( QWheelEvent * ) override;

	void sampleCurves();
	float valueToY( float v, int curve = -1 ) const;
	float yToValue( float y ) const;
	bool hitTestKey( const QPoint & pos, QModelIndex & keyIdx, int & channel, int & comp ) const;
	bool hitTestTangent( const QPoint & pos, int & channel, int & keyIndex, int & comp, bool & backward ) const;

	TimelineWidget * tl;

	QVector<QVector<QPointF>> curves;
	QVector<QPair<int, int>> curveSrc;      //!< (channel, component) per curve
	QVector<QPair<float, float>> curveRange; //!< per-curve min/max for normalized mode
	float vMin = 0, vMax = 1;
	bool curvesDirty = true;

	enum DragMode
	{
		DragNone, DragScrub, DragKeys, DragRubber, DragTangent, DragPan
	};
	DragMode dragMode = DragNone;
	QPoint dragStart;
	float dragStartTime = 0;
	float dragStartValue = 0;
	bool dragMoved = false;
	int dragChannel = -1, dragKeyIndex = -1, dragComp = 0;
	bool dragTangentBackward = false;
	QRect rubberRect;
	QVector<QPair<QPersistentModelIndex, float>> dragOrigTimes;
	QVector<float> dragOrigVals;
};

//! Right-hand inspector: key fields, channel settings, controller settings, sequence membership
class TimelineInspector final : public QWidget
{
	Q_OBJECT

public:
	explicit TimelineInspector( TimelineWidget * parent );

	//! Rebuild all sections from the current timeline selection
	void rebuild();

protected:
	void addSection( QVBoxLayout * lay, const QString & title );
	QLineEdit * addField( QFormLayout * form, const QString & label, const QString & value,
	                      const QModelIndex & fieldIdx, int comp = -1 );

	TimelineWidget * tl;
	QScrollArea * scrollArea = nullptr;
	QVBoxLayout * mainLay = nullptr;
	QWidget * content = nullptr;
	bool rebuilding = false;
};

#endif
