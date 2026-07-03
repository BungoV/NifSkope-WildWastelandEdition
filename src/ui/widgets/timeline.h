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
#include <QPersistentModelIndex>
#include <QSet>
#include <QString>
#include <QVector>

//! @file timeline.h TimelineWidget, TimelineLanesView, TimelineGraphView

class NifModel;

class QComboBox;
class QLabel;
class QScrollBar;
class QSplitter;
class QTimer;

class TimelineLanesView;
class TimelineGraphView;

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
};

//! One row of the timeline
struct TimelineLane
{
	QString label;
	QPersistentModelIndex iSelect;     //!< Block selected in the tree when the lane is clicked
	QVector<TimelineChannel> channels;
	QVector<TimelineKey> keys;         //!< Merged keys of all channels for the lane row
	bool rangeOnly = false;            //!< B-spline etc. without discrete keys
	float start = 0, stop = 0;
	QSet<int> blockNums;               //!< Block numbers belonging to this lane (for external selection)
};

//! Dockable animation timeline with keyframe lanes and a value graph
class TimelineWidget final : public QWidget
{
	Q_OBJECT

	friend class TimelineLanesView;
	friend class TimelineGraphView;

public:
	explicit TimelineWidget( QWidget * parent = nullptr );

	void setNif( NifModel * nif );

	QSize sizeHint() const override;

signals:
	//! Request selection of an index in the main window
	void indexSelected( const QModelIndex & );
	//! Request a scene time change
	void timeChanged( float );

public slots:
	//! Rescan the model and rebuild all lanes
	void refresh();
	//! Compressed refresh (via single shot timer)
	void refreshLater();
	//! Scene time update from GLView
	void setTime( float t, float mn, float mx );
	//! Highlight the lane / key corresponding to an externally selected index
	void setCurrentIndex( const QModelIndex & );

protected slots:
	void sequenceChosen( int comboRow );

protected:
	// Scan helpers
	void scanModel();
	void buildLanes();
	void addControllerLanes( const QModelIndex & iController );
	void addInterpolatorLane( const QModelIndex & iInterp, const QString & label );
	void addTextKeyLane( const QModelIndex & iTextKeys, const QString & label );
	void collectChannels( const QModelIndex & iParent, TimelineLane & lane, int depth = 0 );
	void addKeyGroupChannel( const QModelIndex & iKeyGroup, TimelineLane & lane, const QString & name );
	void finalizeLane( TimelineLane & lane );
	void computeRange();

	// View state shared by both canvases
	float timeToX( float t, int width ) const;
	float xToTime( int x, int width ) const;
	void zoomAt( float t, float factor );
	void selectLane( int lane, bool emitSelect );
	void selectKey( const TimelineKey & key );

	NifModel * nif = nullptr;

	QVector<TimelineLane> lanes;
	QVector<QPersistentModelIndex> sequences;

	int currentLane = -1;
	QPersistentModelIndex selKeyIdx;

	float tMin = 0, tMax = 1;      //!< Range of all data
	float viewT0 = 0, viewT1 = 1;  //!< Visible range
	float curTime = 0;

	QComboBox * seqBox;
	QLabel * infoLabel;
	QSplitter * split;
	TimelineLanesView * lanesView;
	TimelineGraphView * graphView;
	QScrollBar * laneScroll;
	QTimer * refreshTimer;

	bool scanning = false;
};

//! The upper pane: time ruler and one row per interpolator
class TimelineLanesView final : public QWidget
{
	Q_OBJECT

public:
	explicit TimelineLanesView( TimelineWidget * parent );

	QSize minimumSizeHint() const override { return { 200, 64 }; }

protected:
	void paintEvent( QPaintEvent * ) override;
	void mousePressEvent( QMouseEvent * ) override;
	void mouseMoveEvent( QMouseEvent * ) override;
	void wheelEvent( QWheelEvent * ) override;

	int laneAtY( int y ) const;
	void scrub( int x );

	TimelineWidget * tl;
	bool scrubbing = false;
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
	void wheelEvent( QWheelEvent * ) override;

	void sampleCurves();
	float valueToY( float v ) const;

	TimelineWidget * tl;

	//! One polyline per channel component, sampled across the visible range
	QVector<QVector<QPointF>> curves;   // (time, value) pairs
	QVector<QPair<int, int>> curveSrc;  // (channel, component) per curve
	float vMin = 0, vMax = 1;
	bool curvesDirty = true;
	bool scrubbing = false;
};

#endif
