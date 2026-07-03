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

#include "timeline.h"

#include "gl/glcontroller.h"
#include "model/nifmodel.h"
#include "data/niftypes.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cmath>

//! @file timeline.cpp TimelineWidget, TimelineLanesView, TimelineGraphView

// Specializations defined in glcontroller.cpp
template <> bool Controller::interpolate( float & value, const QModelIndex & array, float time, int & last );
template <> bool Controller::interpolate( Vector3 & value, const QModelIndex & array, float time, int & last );
template <> bool Controller::interpolate( Color3 & value, const QModelIndex & array, float time, int & last );
template <> bool Controller::interpolate( Color4 & value, const QModelIndex & array, float time, int & last );
template <> bool Controller::interpolate( bool & value, const QModelIndex & array, float time, int & last );

// Layout constants shared by both panes
static constexpr int LABEL_W = 240;
static constexpr int RULER_H = 20;
static constexpr int LANE_H  = 18;
static constexpr int KEY_R   = 4;

// Component colors (X/R, Y/G, Z/B, W/A)
static const QColor COMP_COLORS[4] = {
	QColor( 0xe0, 0x55, 0x55 ), QColor( 0x55, 0xc0, 0x60 ),
	QColor( 0x55, 0x88, 0xe0 ), QColor( 0xe0, 0xa0, 0x30 )
};

static QString keyTypeName( int t )
{
	switch ( t ) {
	case 1: return QStringLiteral( "Linear" );
	case 2: return QStringLiteral( "Quadratic" );
	case 3: return QStringLiteral( "TBC" );
	case 4: return QStringLiteral( "XYZ" );
	case 5: return QStringLiteral( "Const" );
	}
	return QStringLiteral( "?" );
}

static QPolygonF diamond( const QPointF & c, float r )
{
	QPolygonF p;
	p << QPointF( c.x(), c.y() - r ) << QPointF( c.x() + r, c.y() )
	  << QPointF( c.x(), c.y() + r ) << QPointF( c.x() - r, c.y() );
	return p;
}

// A "nice" tick step >= raw (1/2/5 * 10^n)
static float niceStep( float raw )
{
	if ( raw <= 0.0f )
		return 1.0f;

	float m = std::pow( 10.0f, std::floor( std::log10( raw ) ) );
	for ( float f : { 1.0f, 2.0f, 5.0f, 10.0f } ) {
		if ( f * m >= raw )
			return f * m;
	}
	return 10.0f * m;
}

static QString timeLabel( float t, float step )
{
	int decimals = 0;
	if ( step < 1.0f )
		decimals = std::min( 4, (int)std::ceil( -std::log10( step ) ) );

	return QString::number( t, 'f', decimals );
}


/*
 *  TimelineWidget
 */

TimelineWidget::TimelineWidget( QWidget * parent ) : QWidget( parent )
{
	seqBox = new QComboBox( this );
	seqBox->setSizeAdjustPolicy( QComboBox::AdjustToContents );
	seqBox->setMinimumWidth( 120 );
	connect( seqBox, qOverload<int>( &QComboBox::activated ), this, &TimelineWidget::sequenceChosen );

	infoLabel = new QLabel( this );

	lanesView = new TimelineLanesView( this );
	graphView = new TimelineGraphView( this );

	laneScroll = new QScrollBar( Qt::Vertical, this );
	laneScroll->setRange( 0, 0 );
	connect( laneScroll, &QScrollBar::valueChanged, lanesView, qOverload<>( &QWidget::update ) );

	auto lanesArea = new QWidget( this );
	auto lanesLayout = new QHBoxLayout( lanesArea );
	lanesLayout->setContentsMargins( 0, 0, 0, 0 );
	lanesLayout->setSpacing( 0 );
	lanesLayout->addWidget( lanesView );
	lanesLayout->addWidget( laneScroll );

	split = new QSplitter( Qt::Vertical, this );
	split->addWidget( lanesArea );
	split->addWidget( graphView );
	split->setStretchFactor( 0, 2 );
	split->setStretchFactor( 1, 1 );
	split->setCollapsible( 0, false );

	auto topLayout = new QHBoxLayout;
	topLayout->setContentsMargins( 4, 2, 4, 2 );
	topLayout->addWidget( new QLabel( tr( "Sequence:" ), this ) );
	topLayout->addWidget( seqBox );
	topLayout->addStretch();
	topLayout->addWidget( infoLabel );

	auto mainLayout = new QVBoxLayout( this );
	mainLayout->setContentsMargins( 0, 0, 0, 0 );
	mainLayout->setSpacing( 0 );
	mainLayout->addLayout( topLayout );
	mainLayout->addWidget( split );

	refreshTimer = new QTimer( this );
	refreshTimer->setSingleShot( true );
	refreshTimer->setInterval( 250 );
	connect( refreshTimer, &QTimer::timeout, this, &TimelineWidget::refresh );
}

QSize TimelineWidget::sizeHint() const
{
	return { 800, 240 };
}

void TimelineWidget::setNif( NifModel * n )
{
	if ( nif ) {
		disconnect( nif, nullptr, this, nullptr );
	}

	nif = n;

	if ( nif ) {
		connect( nif, &NifModel::dataChanged, this, &TimelineWidget::refreshLater );
		connect( nif, &NifModel::rowsInserted, this, &TimelineWidget::refreshLater );
		connect( nif, &NifModel::rowsRemoved, this, &TimelineWidget::refreshLater );
		connect( nif, &NifModel::modelReset, this, &TimelineWidget::refreshLater );
	}

	refresh();
}

void TimelineWidget::refreshLater()
{
	refreshTimer->start();
}

void TimelineWidget::refresh()
{
	scanning = true;

	// Try to keep the selected sequence across refreshes
	qint32 prevSeqBlock = -1;
	int prevSeq = seqBox->currentIndex();
	if ( prevSeq > 0 && prevSeq - 1 < sequences.count() && nif )
		prevSeqBlock = nif->getBlockNumber( QModelIndex( sequences[prevSeq - 1] ) );

	scanModel();

	int comboRow = 0;
	if ( prevSeqBlock >= 0 ) {
		for ( int i = 0; i < sequences.count(); i++ ) {
			if ( nif->getBlockNumber( QModelIndex( sequences[i] ) ) == prevSeqBlock ) {
				comboRow = i + 1;
				break;
			}
		}
	}
	seqBox->setCurrentIndex( comboRow );

	// Keep the current zoom if the data range did not change
	float oldMin = tMin, oldMax = tMax;
	float oldV0 = viewT0, oldV1 = viewT1;

	buildLanes();
	computeRange();

	if ( oldMin == tMin && oldMax == tMax ) {
		viewT0 = oldV0;
		viewT1 = oldV1;
	}

	scanning = false;

	lanesView->update();
	graphView->invalidateCurves();
}

void TimelineWidget::scanModel()
{
	sequences.clear();
	seqBox->clear();
	seqBox->addItem( tr( "All controllers" ) );

	if ( !nif )
		return;

	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex iBlock = nif->getBlockIndex( b );
		if ( nif->blockInherits( iBlock, "NiControllerSequence" ) ) {
			sequences.append( iBlock );
			QString name = nif->resolveString( iBlock, "Name" );
			if ( name.isEmpty() )
				name = QString( "%1 [%2]" ).arg( nif->itemName( iBlock ) ).arg( b );
			seqBox->addItem( name );
		}
	}

	seqBox->setEnabled( seqBox->count() > 1 );
}

void TimelineWidget::sequenceChosen( int comboRow )
{
	if ( comboRow > 0 && comboRow - 1 < sequences.count() && sequences[comboRow - 1].isValid() )
		emit indexSelected( QModelIndex( sequences[comboRow - 1] ) );

	buildLanes();
	computeRange();
	currentLane = lanes.isEmpty() ? -1 : std::min( currentLane, (int)lanes.count() - 1 );
	lanesView->update();
	graphView->invalidateCurves();
}

void TimelineWidget::buildLanes()
{
	lanes.clear();
	currentLane = -1;
	selKeyIdx = QPersistentModelIndex();

	if ( !nif ) {
		infoLabel->clear();
		return;
	}

	int seq = seqBox->currentIndex();

	if ( seq > 0 && seq - 1 < sequences.count() ) {
		// Lanes from the chosen NiControllerSequence
		QModelIndex iSeq( sequences[seq - 1] );

		QModelIndex iCtrl = nif->getIndex( iSeq, "Controlled Blocks" );
		for ( int r = 0; r < nif->rowCount( iCtrl ); r++ ) {
			QModelIndex iRow = nif->getIndex( iCtrl, r );

			QString label = nif->resolveString( iRow, "Node Name" );
			QString ctype = nif->resolveString( iRow, "Controller Type" );
			if ( !ctype.isEmpty() )
				label = label.isEmpty() ? ctype : label + QStringLiteral( " — " ) + ctype;
			if ( label.isEmpty() )
				label = tr( "Controlled Block %1" ).arg( r );

			QModelIndex iInterp = nif->getBlockIndex( nif->getLink( iRow, "Interpolator" ), "NiInterpolator" );
			if ( iInterp.isValid() )
				addInterpolatorLane( iInterp, label );
		}

		QModelIndex iText = nif->getBlockIndex( nif->getLink( iSeq, "Text Keys" ), "NiTextKeyExtraData" );
		if ( iText.isValid() )
			addTextKeyLane( iText, tr( "Text Keys" ) );
	} else {
		// All controllers in the file, then interpolators only reachable through sequences
		QSet<int> seen;

		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex iBlock = nif->getBlockIndex( b );

			if ( nif->blockInherits( iBlock, "NiTimeController" ) ) {
				int before = lanes.count();
				addControllerLanes( iBlock );
				for ( int i = before; i < lanes.count(); i++ )
					seen.unite( lanes[i].blockNums );
			} else if ( nif->blockInherits( iBlock, "NiTextKeyExtraData" ) ) {
				addTextKeyLane( iBlock, nif->itemName( iBlock ) );
				seen.insert( b );
			}
		}

		// Label interpolators driven by controller sequences
		QMap<int, QString> seqInterps;
		for ( const auto & s : sequences ) {
			QModelIndex iSeq( s );
			QString seqName = nif->resolveString( iSeq, "Name" );
			QModelIndex iCtrl = nif->getIndex( iSeq, "Controlled Blocks" );
			for ( int r = 0; r < nif->rowCount( iCtrl ); r++ ) {
				QModelIndex iRow = nif->getIndex( iCtrl, r );
				qint32 l = nif->getLink( iRow, "Interpolator" );
				if ( l >= 0 && !seqInterps.contains( l ) )
					seqInterps[l] = seqName + QStringLiteral( " / " ) + nif->resolveString( iRow, "Node Name" );
			}
		}

		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			if ( seen.contains( b ) )
				continue;

			QModelIndex iBlock = nif->getBlockIndex( b );
			if ( nif->blockInherits( iBlock, "NiInterpolator" ) ) {
				bool referenced = false;
				for ( const auto & lane : lanes ) {
					if ( lane.blockNums.contains( b ) ) {
						referenced = true;
						break;
					}
				}
				if ( !referenced )
					addInterpolatorLane( iBlock, seqInterps.value( b, tr( "(unreferenced)" ) ) );
			}
		}
	}

	int numKeys = 0;
	for ( const auto & lane : lanes )
		numKeys += lane.keys.count();

	infoLabel->setText( tr( "%1 lanes, %2 keys" ).arg( lanes.count() ).arg( numKeys ) );
}

void TimelineWidget::addControllerLanes( const QModelIndex & iController )
{
	QString ctype = nif->itemName( iController );

	QModelIndex iTarget = nif->getBlockIndex( nif->getLink( iController, "Target" ) );
	QString tname = nif->resolveString( iTarget, "Name" );
	if ( tname.isEmpty() && iTarget.isValid() )
		tname = nif->itemName( iTarget );

	QString label = tname.isEmpty() ? ctype : tname + QStringLiteral( " — " ) + ctype;

	int before = lanes.count();

	QModelIndex iInterp = nif->getBlockIndex( nif->getLink( iController, "Interpolator" ), "NiInterpolator" );
	if ( iInterp.isValid() )
		addInterpolatorLane( iInterp, label );

	// NiGeomMorpherController and its ilk
	QModelIndex iWeights = nif->getIndex( iController, "Interpolator Weights" );
	for ( int r = 0; r < nif->rowCount( iWeights ); r++ ) {
		QModelIndex iw = nif->getBlockIndex( nif->getLink( nif->getIndex( iWeights, r ), "Interpolator" ), "NiInterpolator" );
		if ( iw.isValid() )
			addInterpolatorLane( iw, label + tr( " [morph %1]" ).arg( r ) );
	}

	// Old style controllers link their data directly
	QModelIndex iData = nif->getBlockIndex( nif->getLink( iController, "Data" ) );
	if ( iData.isValid() && lanes.count() == before ) {
		TimelineLane lane;
		lane.label = label;
		lane.iSelect = iController;
		lane.blockNums.insert( nif->getBlockNumber( iController ) );
		lane.blockNums.insert( nif->getBlockNumber( iData ) );
		collectChannels( iData, lane );
		finalizeLane( lane );
		lanes.append( lane );
	}

	// A lane for the controller itself if nothing else was found,
	// so every controller stays visible and clickable
	if ( lanes.count() == before ) {
		TimelineLane lane;
		lane.label = label;
		lane.iSelect = iController;
		lane.blockNums.insert( nif->getBlockNumber( iController ) );
		lane.rangeOnly = true;
		lane.start = nif->get<float>( iController, "Start Time" );
		lane.stop = nif->get<float>( iController, "Stop Time" );
		lanes.append( lane );
	} else {
		for ( int i = before; i < lanes.count(); i++ )
			lanes[i].blockNums.insert( nif->getBlockNumber( iController ) );
	}
}

void TimelineWidget::addInterpolatorLane( const QModelIndex & iInterp, const QString & label )
{
	TimelineLane lane;
	lane.label = label + QStringLiteral( "  · " ) + nif->itemName( iInterp );
	lane.iSelect = iInterp;
	lane.blockNums.insert( nif->getBlockNumber( iInterp ) );

	if ( nif->blockInherits( iInterp, "NiBSplineInterpolator" ) ) {
		// Compact spline control points, no discrete keys
		lane.rangeOnly = true;
		lane.start = nif->get<float>( iInterp, "Start Time" );
		lane.stop = nif->get<float>( iInterp, "Stop Time" );
	} else {
		for ( const char * linkName : { "Data", "Path Data", "Percent Data" } ) {
			QModelIndex iData = nif->getBlockIndex( nif->getLink( iInterp, linkName ) );
			if ( iData.isValid() ) {
				lane.blockNums.insert( nif->getBlockNumber( iData ) );
				collectChannels( iData, lane );
			}
		}
	}

	finalizeLane( lane );
	lanes.append( lane );
}

void TimelineWidget::addTextKeyLane( const QModelIndex & iTextKeys, const QString & label )
{
	TimelineLane lane;
	lane.label = label + QStringLiteral( "  · " ) + nif->itemName( iTextKeys );
	lane.iSelect = iTextKeys;
	lane.blockNums.insert( nif->getBlockNumber( iTextKeys ) );
	collectChannels( iTextKeys, lane );
	finalizeLane( lane );
	lanes.append( lane );
}

void TimelineWidget::addKeyGroupChannel( const QModelIndex & iKeyGroup, TimelineLane & lane, const QString & name )
{
	QModelIndex iKeys = nif->getIndex( iKeyGroup, "Keys" );
	int n = nif->rowCount( iKeys );
	if ( n <= 0 )
		return;

	TimelineChannel ch;
	ch.name = name;
	ch.iKeyGroup = iKeyGroup;
	ch.iKeysArray = iKeys;
	ch.interpolation = nif->get<int>( iKeyGroup, "Interpolation" );

	for ( int k = 0; k < n; k++ ) {
		QModelIndex iKey = nif->getIndex( iKeys, k );
		TimelineKey key;
		key.time = nif->get<float>( iKey, "Time" );
		key.idx = iKey;
		ch.keys.append( key );
	}

	QModelIndex iVal = nif->getIndex( nif->getIndex( iKeys, 0 ), "Value" );
	QString vt = nif->itemStrType( iVal );

	if ( vt == QLatin1String( "float" ) ) {
		ch.type = TimelineChannel::Float;
		ch.numComponents = 1;
	} else if ( vt == QLatin1String( "Vector3" ) ) {
		ch.type = TimelineChannel::Vector3Val;
		ch.numComponents = 3;
	} else if ( vt == QLatin1String( "Color3" ) ) {
		ch.type = TimelineChannel::Color3Val;
		ch.numComponents = 3;
	} else if ( vt == QLatin1String( "Color4" ) ) {
		ch.type = TimelineChannel::Color4Val;
		ch.numComponents = 4;
	} else if ( vt == QLatin1String( "byte" ) || vt == QLatin1String( "bool" ) ) {
		ch.type = TimelineChannel::BoolVal;
		ch.numComponents = 1;
	} else {
		ch.type = TimelineChannel::Unknown;
		ch.numComponents = 0;
	}

	lane.channels.append( ch );
}

void TimelineWidget::collectChannels( const QModelIndex & iParent, TimelineLane & lane, int depth )
{
	if ( depth > 3 || !iParent.isValid() )
		return;

	for ( int r = 0; r < nif->rowCount( iParent ); r++ ) {
		QModelIndex iChild = nif->getIndex( iParent, r );
		if ( !iChild.isValid() )
			continue;

		QString name = nif->itemName( iChild );

		if ( nif->getIndex( iChild, "Keys" ).isValid() ) {
			addKeyGroupChannel( iChild, lane, name );
		} else if ( name == QLatin1String( "Quaternion Keys" ) && nif->rowCount( iChild ) > 0 ) {
			TimelineChannel ch;
			ch.name = tr( "Rotations" );
			ch.iKeysArray = iChild;
			ch.type = TimelineChannel::QuatVal;
			ch.numComponents = 4;
			ch.interpolation = nif->get<int>( iParent, "Rotation Type" );

			for ( int k = 0; k < nif->rowCount( iChild ); k++ ) {
				QModelIndex iKey = nif->getIndex( iChild, k );
				TimelineKey key;
				key.time = nif->get<float>( iKey, "Time" );
				key.idx = iKey;
				ch.keys.append( key );
			}

			lane.channels.append( ch );
		} else if ( name == QLatin1String( "Text Keys" ) && nif->rowCount( iChild ) > 0 ) {
			TimelineChannel ch;
			ch.name = tr( "Text" );
			ch.iKeysArray = iChild;
			ch.type = TimelineChannel::TextVal;
			ch.numComponents = 0;
			ch.interpolation = 5;

			for ( int k = 0; k < nif->rowCount( iChild ); k++ ) {
				QModelIndex iKey = nif->getIndex( iChild, k );
				TimelineKey key;
				key.time = nif->get<float>( iKey, "Time" );
				key.idx = iKey;
				key.text = nif->resolveString( iKey, "Value" );
				ch.keys.append( key );
			}

			lane.channels.append( ch );
		} else if ( name == QLatin1String( "XYZ Rotations" ) ) {
			// Three float KeyGroups, one per axis
			static const char * axes[3] = { "Rot X", "Rot Y", "Rot Z" };
			for ( int a = 0; a < nif->rowCount( iChild ) && a < 3; a++ )
				addKeyGroupChannel( nif->getIndex( iChild, a ), lane, axes[a] );
		} else if ( nif->rowCount( iChild ) > 0 && !nif->isArray( iChild ) ) {
			collectChannels( iChild, lane, depth + 1 );
		}
	}
}

void TimelineWidget::finalizeLane( TimelineLane & lane )
{
	lane.keys.clear();

	for ( const auto & ch : lane.channels )
		lane.keys.append( ch.keys );

	std::sort( lane.keys.begin(), lane.keys.end(),
		[]( const TimelineKey & a, const TimelineKey & b ) { return a.time < b.time; } );
}

void TimelineWidget::computeRange()
{
	bool any = false;
	float mn = 0, mx = 1;

	auto expand = [&]( float t ) {
		if ( !any ) {
			mn = mx = t;
			any = true;
		} else {
			mn = std::min( mn, t );
			mx = std::max( mx, t );
		}
	};

	for ( const auto & lane : lanes ) {
		for ( const auto & key : lane.keys )
			expand( key.time );

		if ( lane.rangeOnly ) {
			expand( lane.start );
			expand( lane.stop );
		}
	}

	if ( !any ) {
		mn = 0;
		mx = 1;
	}

	if ( mx - mn < 1.0e-4f )
		mx = mn + 1.0f;

	tMin = mn;
	tMax = mx;
	viewT0 = mn;
	viewT1 = mx;
}

float TimelineWidget::timeToX( float t, int width ) const
{
	float span = std::max( viewT1 - viewT0, 1.0e-6f );
	return LABEL_W + ( t - viewT0 ) / span * std::max( width - LABEL_W - 8, 1 );
}

float TimelineWidget::xToTime( int x, int width ) const
{
	float span = std::max( viewT1 - viewT0, 1.0e-6f );
	return viewT0 + float( x - LABEL_W ) / std::max( width - LABEL_W - 8, 1 ) * span;
}

void TimelineWidget::zoomAt( float t, float factor )
{
	float span = ( viewT1 - viewT0 ) * factor;
	float fullSpan = tMax - tMin;
	span = std::max( span, 1.0e-3f );
	span = std::min( span, fullSpan * 4.0f + 1.0f );

	float frac = ( viewT1 - viewT0 ) > 0 ? ( t - viewT0 ) / ( viewT1 - viewT0 ) : 0.5f;
	viewT0 = t - frac * span;
	viewT1 = viewT0 + span;

	lanesView->update();
	graphView->invalidateCurves();
}

void TimelineWidget::selectLane( int lane, bool emitSelect )
{
	if ( lane < 0 || lane >= lanes.count() )
		return;

	currentLane = lane;

	if ( emitSelect && lanes[lane].iSelect.isValid() )
		emit indexSelected( QModelIndex( lanes[lane].iSelect ) );

	lanesView->update();
	graphView->invalidateCurves();
}

void TimelineWidget::selectKey( const TimelineKey & key )
{
	selKeyIdx = key.idx;

	if ( key.idx.isValid() )
		emit indexSelected( QModelIndex( key.idx ) );

	lanesView->update();
	graphView->update();
}

void TimelineWidget::setTime( float t, float mn, float mx )
{
	Q_UNUSED( mn );
	Q_UNUSED( mx );

	curTime = t;
	lanesView->update();
	graphView->update();
}

void TimelineWidget::setCurrentIndex( const QModelIndex & index )
{
	if ( scanning || !nif || !index.isValid() || index.model() != nif )
		return;

	int blockNum = nif->getBlockNumber( index );
	if ( blockNum < 0 )
		return;

	int lane = -1;
	for ( int i = 0; i < lanes.count(); i++ ) {
		if ( lanes[i].blockNums.contains( blockNum ) ) {
			lane = i;
			break;
		}
	}

	if ( lane < 0 )
		return;

	if ( lane != currentLane ) {
		currentLane = lane;
		graphView->invalidateCurves();
	}

	// If the index sits inside a Keys array, highlight that key
	selKeyIdx = QPersistentModelIndex();
	QModelIndex walk = index;
	while ( walk.isValid() ) {
		QModelIndex parent = walk.parent();
		for ( const auto & ch : lanes[lane].channels ) {
			if ( parent == ch.iKeysArray ) {
				selKeyIdx = nif->getIndex( parent, walk.row() );
				break;
			}
		}
		if ( selKeyIdx.isValid() )
			break;
		walk = parent;
	}

	lanesView->update();
	graphView->update();
}


/*
 *  TimelineLanesView
 */

TimelineLanesView::TimelineLanesView( TimelineWidget * parent ) : QWidget( parent ), tl( parent )
{
	setMouseTracking( false );
	setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
}

void TimelineLanesView::paintEvent( QPaintEvent * )
{
	QPainter p( this );
	p.setRenderHint( QPainter::Antialiasing );

	const QPalette & pal = palette();
	int w = width();
	int h = height();

	p.fillRect( rect(), pal.base() );

	// Scrollbar range
	int contentH = tl->lanes.count() * LANE_H;
	int viewH = h - RULER_H;
	tl->laneScroll->setRange( 0, std::max( 0, contentH - viewH ) );
	tl->laneScroll->setPageStep( std::max( viewH, LANE_H ) );
	int scroll = tl->laneScroll->value();

	QColor gridCol = pal.color( QPalette::Text );
	gridCol.setAlpha( 40 );

	// Lanes
	QFont f = p.font();
	f.setPointSizeF( std::max( 7.0, f.pointSizeF() - 1.0 ) );
	p.setFont( f );
	QFontMetrics fm( f );

	for ( int i = 0; i < tl->lanes.count(); i++ ) {
		int y = RULER_H + i * LANE_H - scroll;
		if ( y + LANE_H < RULER_H || y > h )
			continue;

		const TimelineLane & lane = tl->lanes[i];
		QRect laneRect( 0, y, w, LANE_H );

		if ( i == tl->currentLane ) {
			QColor hl = pal.color( QPalette::Highlight );
			hl.setAlpha( 70 );
			p.fillRect( laneRect, hl );
		} else if ( i & 1 ) {
			p.fillRect( laneRect, pal.alternateBase() );
		}

		// Label
		p.setPen( pal.color( QPalette::Text ) );
		p.drawText( QRect( 4, y, LABEL_W - 8, LANE_H ), Qt::AlignVCenter | Qt::AlignLeft,
			fm.elidedText( lane.label, Qt::ElideMiddle, LABEL_W - 8 ) );

		// Range bar for key-less lanes (B-splines etc.)
		if ( lane.rangeOnly ) {
			float x0 = tl->timeToX( lane.start, w );
			float x1 = tl->timeToX( lane.stop, w );
			QColor barCol = pal.color( QPalette::Highlight );
			barCol.setAlpha( 120 );
			QRectF bar( std::min( x0, x1 ), y + 5, std::max( std::abs( x1 - x0 ), 2.0f ), LANE_H - 10 );
			p.setPen( Qt::NoPen );
			p.setBrush( barCol );
			p.drawRoundedRect( bar, 3, 3 );
		}

		// Keys
		float cy = y + LANE_H * 0.5f;
		for ( const auto & key : lane.keys ) {
			float x = tl->timeToX( key.time, w );
			if ( x < LABEL_W - KEY_R || x > w + KEY_R )
				continue;

			bool sel = tl->selKeyIdx.isValid() && key.idx == tl->selKeyIdx;

			p.setPen( Qt::NoPen );
			p.setBrush( sel ? pal.color( QPalette::BrightText ) : pal.color( QPalette::Highlight ) );
			p.drawPolygon( diamond( QPointF( x, cy ), sel ? KEY_R + 1.5f : KEY_R ) );

			if ( !key.text.isEmpty() ) {
				p.setPen( pal.color( QPalette::Text ) );
				p.drawText( QPointF( x + KEY_R + 2, cy + fm.ascent() * 0.5f - 1 ), key.text );
			}
		}
	}

	// Label / time area separator
	p.setPen( gridCol );
	p.drawLine( LABEL_W, 0, LABEL_W, h );

	// Ruler
	p.fillRect( QRect( 0, 0, w, RULER_H ), pal.window() );
	p.setPen( gridCol );
	p.drawLine( 0, RULER_H, w, RULER_H );

	float step = niceStep( ( tl->viewT1 - tl->viewT0 ) / std::max( ( w - LABEL_W ) / 70, 2 ) );
	float t = std::floor( tl->viewT0 / step ) * step;
	p.setPen( pal.color( QPalette::Text ) );

	for ( ; t <= tl->viewT1 + step; t += step ) {
		float x = tl->timeToX( t, w );
		if ( x < LABEL_W )
			continue;

		p.setPen( gridCol );
		p.drawLine( QPointF( x, RULER_H ), QPointF( x, h ) );
		p.setPen( pal.color( QPalette::Text ) );
		p.drawLine( QPointF( x, RULER_H - 5 ), QPointF( x, RULER_H ) );
		p.drawText( QPointF( x + 2, RULER_H - 6 ), timeLabel( t, step ) );
	}

	// Playhead
	float px = tl->timeToX( tl->curTime, w );
	if ( px >= LABEL_W ) {
		p.setPen( QPen( QColor( 0xe0, 0x40, 0x40 ), 1.5 ) );
		p.drawLine( QPointF( px, 0 ), QPointF( px, h ) );
	}

	if ( tl->lanes.isEmpty() ) {
		p.setPen( pal.color( QPalette::Text ) );
		p.drawText( rect().adjusted( 0, RULER_H, 0, 0 ), Qt::AlignCenter, tr( "No animation controllers in this file" ) );
	}
}

int TimelineLanesView::laneAtY( int y ) const
{
	if ( y < RULER_H )
		return -1;

	int lane = ( y - RULER_H + tl->laneScroll->value() ) / LANE_H;
	return ( lane >= 0 && lane < tl->lanes.count() ) ? lane : -1;
}

void TimelineLanesView::scrub( int x )
{
	float t = tl->xToTime( std::max( x, LABEL_W ), width() );
	tl->curTime = t;
	emit tl->timeChanged( t );
	update();
	tl->graphView->update();
}

void TimelineLanesView::mousePressEvent( QMouseEvent * ev )
{
	if ( ev->button() != Qt::LeftButton )
		return;

	QPoint pos = ev->pos();

	if ( pos.y() < RULER_H && pos.x() >= LABEL_W ) {
		scrubbing = true;
		scrub( pos.x() );
		return;
	}

	int lane = laneAtY( pos.y() );
	if ( lane < 0 )
		return;

	// A key within reach?
	const TimelineLane & l = tl->lanes[lane];
	int best = -1;
	float bestDist = KEY_R + 3.0f;

	for ( int k = 0; k < l.keys.count(); k++ ) {
		float d = std::abs( tl->timeToX( l.keys[k].time, width() ) - pos.x() );
		if ( d < bestDist ) {
			bestDist = d;
			best = k;
		}
	}

	tl->selectLane( lane, best < 0 );

	if ( best >= 0 )
		tl->selectKey( l.keys[best] );
}

void TimelineLanesView::mouseMoveEvent( QMouseEvent * ev )
{
	if ( scrubbing && ( ev->buttons() & Qt::LeftButton ) )
		scrub( ev->pos().x() );
	else if ( !( ev->buttons() & Qt::LeftButton ) )
		scrubbing = false;
}

void TimelineLanesView::wheelEvent( QWheelEvent * ev )
{
	int delta = ev->angleDelta().y();

	if ( ev->modifiers() & Qt::ControlModifier ) {
		float t = tl->xToTime( ev->position().x(), width() );
		tl->zoomAt( t, delta > 0 ? 0.8f : 1.25f );
	} else {
		tl->laneScroll->setValue( tl->laneScroll->value() - delta / 120 * LANE_H * 3 );
	}

	ev->accept();
}


/*
 *  TimelineGraphView
 */

TimelineGraphView::TimelineGraphView( TimelineWidget * parent ) : QWidget( parent ), tl( parent )
{
	setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
}

void TimelineGraphView::sampleCurves()
{
	curves.clear();
	curveSrc.clear();
	vMin = 0;
	vMax = 1;
	curvesDirty = false;

	if ( !tl->nif || tl->currentLane < 0 || tl->currentLane >= tl->lanes.count() )
		return;

	const TimelineLane & lane = tl->lanes[tl->currentLane];

	bool any = false;
	auto expand = [&]( float v ) {
		if ( !any ) {
			vMin = vMax = v;
			any = true;
		} else {
			vMin = std::min( vMin, v );
			vMax = std::max( vMax, v );
		}
	};

	int numSamples = std::max( ( width() - LABEL_W ) / 2, 16 );
	float t0 = tl->viewT0;
	float dt = ( tl->viewT1 - tl->viewT0 ) / numSamples;

	for ( int c = 0; c < lane.channels.count(); c++ ) {
		const TimelineChannel & ch = lane.channels[c];

		if ( ch.type == TimelineChannel::TextVal || ch.type == TimelineChannel::Unknown )
			continue;

		if ( ch.type == TimelineChannel::QuatVal ) {
			// Polylines through the raw key values (rendering uses slerp; this
			// is close enough for a value display)
			for ( int comp = 0; comp < 4; comp++ ) {
				QVector<QPointF> curve;
				for ( const auto & key : ch.keys ) {
					Quat q = tl->nif->get<Quat>( QModelIndex( key.idx ), "Value" );
					expand( q[comp] );
					curve.append( QPointF( key.time, q[comp] ) );
				}
				curves.append( curve );
				curveSrc.append( { c, comp } );
			}
			continue;
		}

		QModelIndex iKeyGroup( ch.iKeyGroup );
		if ( !iKeyGroup.isValid() )
			continue;

		for ( int comp = 0; comp < ch.numComponents; comp++ ) {
			QVector<QPointF> curve;
			curve.reserve( numSamples + 1 );
			int last = 0;

			for ( int s = 0; s <= numSamples; s++ ) {
				float t = t0 + s * dt;
				float v = 0;
				bool ok = false;

				switch ( ch.type ) {
				case TimelineChannel::Float:
					{
						float fv;
						ok = Controller::interpolate( fv, iKeyGroup, t, last );
						v = fv;
					}
					break;
				case TimelineChannel::Vector3Val:
					{
						Vector3 vec;
						ok = Controller::interpolate( vec, iKeyGroup, t, last );
						v = vec[comp];
					}
					break;
				case TimelineChannel::Color3Val:
					{
						Color3 col;
						ok = Controller::interpolate( col, iKeyGroup, t, last );
						v = ( comp == 0 ) ? col.red() : ( comp == 1 ) ? col.green() : col.blue();
					}
					break;
				case TimelineChannel::Color4Val:
					{
						Color4 col;
						ok = Controller::interpolate( col, iKeyGroup, t, last );
						v = ( comp == 0 ) ? col.red() : ( comp == 1 ) ? col.green()
							: ( comp == 2 ) ? col.blue() : col.alpha();
					}
					break;
				case TimelineChannel::BoolVal:
					{
						bool bv;
						ok = Controller::interpolate( bv, iKeyGroup, t, last );
						v = bv ? 1.0f : 0.0f;
					}
					break;
				default:
					break;
				}

				if ( ok ) {
					expand( v );
					curve.append( QPointF( t, v ) );
				}
			}

			curves.append( curve );
			curveSrc.append( { c, comp } );
		}
	}

	if ( !any ) {
		vMin = 0;
		vMax = 1;
	}

	if ( vMax - vMin < 1.0e-6f ) {
		vMin -= 1.0f;
		vMax += 1.0f;
	} else {
		float pad = ( vMax - vMin ) * 0.08f;
		vMin -= pad;
		vMax += pad;
	}
}

float TimelineGraphView::valueToY( float v ) const
{
	float span = std::max( vMax - vMin, 1.0e-6f );
	return height() - 6 - ( v - vMin ) / span * std::max( height() - 12, 1 );
}

void TimelineGraphView::paintEvent( QPaintEvent * )
{
	if ( curvesDirty )
		sampleCurves();

	QPainter p( this );
	p.setRenderHint( QPainter::Antialiasing );

	const QPalette & pal = palette();
	int w = width();
	int h = height();

	p.fillRect( rect(), pal.base() );

	QFont f = p.font();
	f.setPointSizeF( std::max( 7.0, f.pointSizeF() - 1.0 ) );
	p.setFont( f );
	QFontMetrics fm( f );

	QColor gridCol = pal.color( QPalette::Text );
	gridCol.setAlpha( 40 );

	if ( !tl->nif || tl->currentLane < 0 || tl->currentLane >= tl->lanes.count() ) {
		p.setPen( pal.color( QPalette::Text ) );
		p.drawText( rect(), Qt::AlignCenter, tr( "Click a lane above to see its value curves" ) );
		return;
	}

	const TimelineLane & lane = tl->lanes[tl->currentLane];

	// Horizontal value grid
	float vstep = niceStep( ( vMax - vMin ) / std::max( h / 40, 2 ) );
	float v = std::floor( vMin / vstep ) * vstep;
	for ( ; v <= vMax; v += vstep ) {
		float y = valueToY( v );
		p.setPen( gridCol );
		p.drawLine( QPointF( LABEL_W, y ), QPointF( w, y ) );
		p.setPen( pal.color( QPalette::Text ) );
		p.drawText( QRectF( 4, y - fm.height() * 0.5, LABEL_W - 10, fm.height() ),
			Qt::AlignRight | Qt::AlignVCenter, timeLabel( v, vstep ) );
	}

	// Vertical time grid, aligned with the lanes pane
	float tstep = niceStep( ( tl->viewT1 - tl->viewT0 ) / std::max( ( w - LABEL_W ) / 70, 2 ) );
	float t = std::floor( tl->viewT0 / tstep ) * tstep;
	p.setPen( gridCol );
	for ( ; t <= tl->viewT1 + tstep; t += tstep ) {
		float x = tl->timeToX( t, w );
		if ( x >= LABEL_W )
			p.drawLine( QPointF( x, 0 ), QPointF( x, h ) );
	}

	p.setPen( gridCol );
	p.drawLine( LABEL_W, 0, LABEL_W, h );

	// Curves
	p.setClipRect( QRect( LABEL_W, 0, w - LABEL_W, h ) );

	for ( int i = 0; i < curves.count(); i++ ) {
		const auto & curve = curves[i];
		if ( curve.count() < 2 )
			continue;

		int comp = curveSrc[i].second;
		const TimelineChannel & ch = lane.channels[curveSrc[i].first];

		QColor col = ( ch.numComponents > 1 ) ? COMP_COLORS[comp & 3] : pal.color( QPalette::Highlight );

		QPainterPath path;
		path.moveTo( tl->timeToX( curve[0].x(), w ), valueToY( curve[0].y() ) );

		bool stepped = ( ch.type == TimelineChannel::BoolVal );
		for ( int s = 1; s < curve.count(); s++ ) {
			float x = tl->timeToX( curve[s].x(), w );
			float y = valueToY( curve[s].y() );
			if ( stepped )
				path.lineTo( x, valueToY( curve[s - 1].y() ) );
			path.lineTo( x, y );
		}

		p.setPen( QPen( col, 1.4 ) );
		p.setBrush( Qt::NoBrush );
		p.drawPath( path );
	}

	// Key diamonds on the curves
	for ( int c = 0; c < lane.channels.count(); c++ ) {
		const TimelineChannel & ch = lane.channels[c];
		if ( ch.type == TimelineChannel::TextVal || ch.type == TimelineChannel::Unknown )
			continue;

		for ( const auto & key : ch.keys ) {
			QModelIndex iKey( key.idx );
			float x = tl->timeToX( key.time, w );
			if ( x < LABEL_W - KEY_R || x > w + KEY_R )
				continue;

			bool sel = tl->selKeyIdx.isValid() && key.idx == tl->selKeyIdx;

			for ( int comp = 0; comp < std::max( ch.numComponents, 1 ); comp++ ) {
				float kv = 0;
				switch ( ch.type ) {
				case TimelineChannel::Float:
					kv = tl->nif->get<float>( iKey, "Value" );
					break;
				case TimelineChannel::Vector3Val:
					kv = tl->nif->get<Vector3>( iKey, "Value" )[comp];
					break;
				case TimelineChannel::Color3Val:
					{
						Color3 col = tl->nif->get<Color3>( iKey, "Value" );
						kv = ( comp == 0 ) ? col.red() : ( comp == 1 ) ? col.green() : col.blue();
					}
					break;
				case TimelineChannel::Color4Val:
					{
						Color4 col = tl->nif->get<Color4>( iKey, "Value" );
						kv = ( comp == 0 ) ? col.red() : ( comp == 1 ) ? col.green()
							: ( comp == 2 ) ? col.blue() : col.alpha();
					}
					break;
				case TimelineChannel::BoolVal:
					kv = tl->nif->get<int>( iKey, "Value" ) ? 1.0f : 0.0f;
					break;
				case TimelineChannel::QuatVal:
					kv = tl->nif->get<Quat>( iKey, "Value" )[comp];
					break;
				default:
					break;
				}

				QColor col = ( ch.numComponents > 1 ) ? COMP_COLORS[comp & 3] : pal.color( QPalette::Highlight );
				p.setPen( Qt::NoPen );
				p.setBrush( sel ? pal.color( QPalette::BrightText ) : col );
				p.drawPolygon( diamond( QPointF( x, valueToY( kv ) ), sel ? KEY_R + 1.0f : KEY_R - 1.0f ) );
			}
		}
	}

	// Playhead
	float px = tl->timeToX( tl->curTime, w );
	if ( px >= LABEL_W ) {
		p.setPen( QPen( QColor( 0xe0, 0x40, 0x40 ), 1.5 ) );
		p.drawLine( QPointF( px, 0 ), QPointF( px, h ) );
	}

	p.setClipping( false );

	// Legend: channel names with interpolation type
	int ly = 4;
	for ( int c = 0; c < lane.channels.count(); c++ ) {
		const TimelineChannel & ch = lane.channels[c];
		if ( ch.type == TimelineChannel::TextVal || ch.type == TimelineChannel::Unknown )
			continue;

		QString text = QString( "%1  [%2]" ).arg( ch.name, keyTypeName( ch.interpolation ) );

		int lx = 8;
		for ( int comp = 0; comp < ch.numComponents; comp++ ) {
			QColor col = ( ch.numComponents > 1 ) ? COMP_COLORS[comp & 3] : pal.color( QPalette::Highlight );
			p.setPen( Qt::NoPen );
			p.setBrush( col );
			p.drawRect( QRectF( lx, ly + 3, 8, 8 ) );
			lx += 11;
		}

		p.setPen( pal.color( QPalette::Text ) );
		p.drawText( QPointF( lx + 3, ly + fm.ascent() ), text );
		ly += fm.height() + 2;
	}
}

void TimelineGraphView::mousePressEvent( QMouseEvent * ev )
{
	if ( ev->button() != Qt::LeftButton )
		return;

	if ( !tl->nif || tl->currentLane < 0 || tl->currentLane >= tl->lanes.count() )
		return;

	QPoint pos = ev->pos();
	const TimelineLane & lane = tl->lanes[tl->currentLane];

	// Nearest key diamond in screen space (using time distance only, since
	// one key spans several component diamonds)
	int bestCh = -1, bestKey = -1;
	float bestDist = KEY_R + 4.0f;

	for ( int c = 0; c < lane.channels.count(); c++ ) {
		const TimelineChannel & ch = lane.channels[c];
		for ( int k = 0; k < ch.keys.count(); k++ ) {
			float d = std::abs( tl->timeToX( ch.keys[k].time, width() ) - pos.x() );
			if ( d < bestDist ) {
				bestDist = d;
				bestCh = c;
				bestKey = k;
			}
		}
	}

	if ( bestCh >= 0 ) {
		tl->selectKey( lane.channels[bestCh].keys[bestKey] );
	} else if ( pos.x() >= LABEL_W ) {
		scrubbing = true;
		float t = tl->xToTime( pos.x(), width() );
		tl->curTime = t;
		emit tl->timeChanged( t );
		update();
		tl->lanesView->update();
	}
}

void TimelineGraphView::mouseMoveEvent( QMouseEvent * ev )
{
	if ( scrubbing && ( ev->buttons() & Qt::LeftButton ) ) {
		float t = tl->xToTime( std::max( (int)ev->pos().x(), LABEL_W ), width() );
		tl->curTime = t;
		emit tl->timeChanged( t );
		update();
		tl->lanesView->update();
	} else if ( !( ev->buttons() & Qt::LeftButton ) ) {
		scrubbing = false;
	}
}

void TimelineGraphView::wheelEvent( QWheelEvent * ev )
{
	float t = tl->xToTime( ev->position().x(), width() );
	tl->zoomAt( t, ev->angleDelta().y() > 0 ? 0.8f : 1.25f );
	ev->accept();
}
