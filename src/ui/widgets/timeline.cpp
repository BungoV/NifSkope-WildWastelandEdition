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
#include "timeline_p.h"

#include "gl/glcontroller.h"
#include "model/nifmodel.h"
#include "data/niftypes.h"

#include <QAction>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

//! @file timeline.cpp TimelineWidget core: scanning, labels, selection, navigation

QString tlKeyTypeName( int t )
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

float tlNiceStep( float raw )
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


/*
 *  TimelineWidget - construction
 */

TimelineWidget::TimelineWidget( QWidget * parent ) : QWidget( parent )
{
	setFocusPolicy( Qt::StrongFocus );

	auto mkBtn = [this]( const QString & text, const QString & tip, bool checkable ) {
		auto b = new QToolButton( this );
		b->setText( text );
		b->setToolTip( tip );
		b->setCheckable( checkable );
		b->setAutoRaise( true );
		return b;
	};

	seqBox = new QComboBox( this );
	seqBox->setSizeAdjustPolicy( QComboBox::AdjustToContents );
	seqBox->setMinimumWidth( 100 );
	seqBox->setToolTip( tr( "Animation sequence shown in the timeline (synced with the Animation toolbar)" ) );
	connect( seqBox, qOverload<int>( &QComboBox::activated ), this, &TimelineWidget::sequenceChosen );

	filterBox = new QLineEdit( this );
	filterBox->setPlaceholderText( tr( "Filter lanes" ) );
	filterBox->setClearButtonEnabled( true );
	filterBox->setMaximumWidth( 130 );
	connect( filterBox, &QLineEdit::textChanged, this, &TimelineWidget::filterEdited );

	btnPlay = mkBtn( QStringLiteral( "▶" ), tr( "Play/pause animation (Space)" ), false );
	connect( btnPlay, &QToolButton::clicked, this, &TimelineWidget::playPauseRequested );

	timeField = new QLineEdit( this );
	timeField->setMaximumWidth( 70 );
	timeField->setToolTip( tr( "Current time" ) );
	connect( timeField, &QLineEdit::editingFinished, [this]() {
		bool ok = false;
		float t = timeField->text().toFloat( &ok );
		if ( ok ) {
			if ( framesMode )
				t /= std::max( fps, 1 );
			curTime = t;
			emit timeChanged( t );
			updateViews();
		}
	} );

	btnFrames = mkBtn( tr( "sec" ), tr( "Toggle seconds / frames display (right-click to set fps)" ), true );
	connect( btnFrames, &QToolButton::toggled, [this]( bool on ) {
		framesMode = on;
		btnFrames->setText( on ? QString( "%1fps" ).arg( fps ) : tr( "sec" ) );
		updateViews();
	} );
	btnFrames->setContextMenuPolicy( Qt::CustomContextMenu );
	connect( btnFrames, &QWidget::customContextMenuRequested, [this]( const QPoint & ) {
		QMenu m;
		for ( int f : { 24, 30, 60 } )
			m.addAction( QString( "%1 fps" ).arg( f ), [this, f]() {
				fps = f;
				if ( framesMode )
					btnFrames->setText( QString( "%1fps" ).arg( fps ) );
				updateViews();
			} );
		m.exec( QCursor::pos() );
	} );

	btnSnap = mkBtn( QStringLiteral( "⦷" ), tr( "Snap dragging to steps" ), true );
	connect( btnSnap, &QToolButton::toggled, [this]( bool on ) { snapOn = on; } );

	snapTimeBox = new QDoubleSpinBox( this );
	snapTimeBox->setRange( 0.001, 10.0 );
	snapTimeBox->setDecimals( 3 );
	snapTimeBox->setSingleStep( 0.01 );
	snapTimeBox->setValue( snapTimeStep );
	snapTimeBox->setToolTip( tr( "Time snap step (seconds)" ) );
	snapTimeBox->setMaximumWidth( 70 );
	connect( snapTimeBox, qOverload<double>( &QDoubleSpinBox::valueChanged ), [this]( double v ) { snapTimeStep = (float)v; } );

	snapValueBox = new QDoubleSpinBox( this );
	snapValueBox->setRange( 0.0001, 1000.0 );
	snapValueBox->setDecimals( 4 );
	snapValueBox->setSingleStep( 0.05 );
	snapValueBox->setValue( snapValueStep );
	snapValueBox->setToolTip( tr( "Value snap step (graph)" ) );
	snapValueBox->setMaximumWidth( 80 );
	connect( snapValueBox, qOverload<double>( &QDoubleSpinBox::valueChanged ), [this]( double v ) { snapValueStep = (float)v; } );

	btnNormalize = mkBtn( QStringLiteral( "↕" ), tr( "Normalize curves (each scaled to its own range)" ), true );
	connect( btnNormalize, &QToolButton::toggled, [this]( bool on ) {
		normalized = on;
		graphView->invalidateCurves();
	} );

	btnFollow = mkBtn( QStringLiteral( "↦" ), tr( "Follow playhead during playback" ), true );
	btnFollow->setChecked( followPlayhead );
	connect( btnFollow, &QToolButton::toggled, [this]( bool on ) { followPlayhead = on; } );

	btnIsolate = mkBtn( QStringLiteral( "◉" ), tr( "Show only lanes of the selected node" ), true );
	connect( btnIsolate, &QToolButton::toggled, [this]( bool on ) {
		autoIsolate = on;
		if ( !on )
			filterTargetBlock = -1;
		applyFilter();
		updateViews();
	} );

	btnLint = mkBtn( QStringLiteral( "✔" ), tr( "Check animation for common problems" ), false );
	connect( btnLint, &QToolButton::clicked, this, &TimelineWidget::runLint );

	btnInspector = mkBtn( QStringLiteral( "◨" ), tr( "Show/hide the inspector panel" ), true );
	btnInspector->setChecked( true );

	infoLabel = new QLabel( this );

	lanesView = new TimelineLanesView( this );
	graphView = new TimelineGraphView( this );
	inspector = new TimelineInspector( this );

	connect( btnInspector, &QToolButton::toggled, [this]( bool on ) { inspector->setVisible( on ); } );

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
	topLayout->setSpacing( 4 );
	topLayout->addWidget( seqBox );
	topLayout->addWidget( filterBox );
	topLayout->addWidget( btnIsolate );
	topLayout->addSpacing( 6 );
	topLayout->addWidget( btnPlay );
	topLayout->addWidget( timeField );
	topLayout->addWidget( btnFrames );
	topLayout->addSpacing( 6 );
	topLayout->addWidget( btnSnap );
	topLayout->addWidget( snapTimeBox );
	topLayout->addWidget( snapValueBox );
	topLayout->addWidget( btnNormalize );
	topLayout->addWidget( btnFollow );
	topLayout->addWidget( btnLint );
	topLayout->addStretch();
	topLayout->addWidget( infoLabel );
	topLayout->addWidget( btnInspector );

	auto leftLayout = new QVBoxLayout;
	leftLayout->setContentsMargins( 0, 0, 0, 0 );
	leftLayout->setSpacing( 0 );
	leftLayout->addLayout( topLayout );
	leftLayout->addWidget( split );

	auto leftWidget = new QWidget( this );
	leftWidget->setLayout( leftLayout );

	auto mainLayout = new QHBoxLayout( this );
	mainLayout->setContentsMargins( 0, 0, 0, 0 );
	mainLayout->setSpacing( 0 );
	mainLayout->addWidget( leftWidget, 1 );
	mainLayout->addWidget( inspector );

	refreshTimer = new QTimer( this );
	refreshTimer->setSingleShot( true );
	refreshTimer->setInterval( 250 );
	connect( refreshTimer, &QTimer::timeout, this, &TimelineWidget::refresh );

	loadSettings();
	btnSnap->setChecked( snapOn );
	btnFrames->setChecked( framesMode );
	btnNormalize->setChecked( normalized );
	snapTimeBox->setValue( snapTimeStep );
	snapValueBox->setValue( snapValueStep );
}

TimelineWidget::~TimelineWidget()
{
	saveSettings();
}

QSize TimelineWidget::sizeHint() const
{
	return { 900, 260 };
}

void TimelineWidget::setNif( NifModel * n )
{
	if ( nif )
		disconnect( nif, nullptr, this, nullptr );

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

void TimelineWidget::updateViews()
{
	if ( framesMode )
		timeField->setText( QString::number( (int)std::lround( curTime * fps ) ) );
	else
		timeField->setText( QString::number( curTime, 'f', 3 ) );

	lanesView->update();
	graphView->update();
}

void TimelineWidget::refresh()
{
	scanning = true;

	qint32 prevSeqBlock = -1;
	int prevSeq = seqBox->currentIndex();
	if ( prevSeq > 0 && prevSeq - 1 < sequences.count() && nif )
		prevSeqBlock = nif->getBlockNumber( QModelIndex( sequences[prevSeq - 1] ) );

	int prevLaneBlock = ( currentLane >= 0 && currentLane < lanes.count() && nif )
	                    ? nif->getBlockNumber( QModelIndex( lanes[currentLane].iSelect ) ) : -1;

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

	float oldMin = tMin, oldMax = tMax;
	float oldV0 = viewT0, oldV1 = viewT1;

	buildLanes();
	computeRange();

	if ( oldMin == tMin && oldMax == tMax ) {
		viewT0 = oldV0;
		viewT1 = oldV1;
	}

	// Restore lane selection if possible
	if ( prevLaneBlock >= 0 ) {
		currentLane = -1;
		for ( int i = 0; i < lanes.count(); i++ ) {
			if ( nif->getBlockNumber( QModelIndex( lanes[i].iSelect ) ) == prevLaneBlock ) {
				currentLane = i;
				break;
			}
		}
	}

	applyFilter();
	scanning = false;

	lanesView->invalidateStrips();
	graphView->invalidateCurves();
	inspector->rebuild();
	updateViews();
}

/*
 *  Scanning
 */

void TimelineWidget::scanModel()
{
	sequences.clear();
	avObjectsByName.clear();
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
		} else if ( nif->blockInherits( iBlock, "NiAVObject" ) ) {
			QString name = nif->resolveString( iBlock, "Name" );
			if ( !name.isEmpty() && !avObjectsByName.contains( name ) )
				avObjectsByName[name] = b;
		}
	}

	seqBox->setEnabled( seqBox->count() > 1 );
}

int TimelineWidget::findAVObjectByName( const QString & name ) const
{
	return avObjectsByName.value( name, -1 );
}

QString TimelineWidget::shortTypeName( QString type )
{
	// NiLightDimmerController -> "Light Dimmer", BSEffectShaderPropertyFloatController -> "Effect Shader Float"
	if ( type.startsWith( QLatin1String( "Ni" ) ) )
		type.remove( 0, 2 );
	else if ( type.startsWith( QLatin1String( "BS" ) ) )
		type.remove( 0, 2 );

	if ( type.endsWith( QLatin1String( "Controller" ) ) )
		type.chop( 10 );
	else if ( type.endsWith( QLatin1String( "Ctlr" ) ) )
		type.chop( 4 );
	else if ( type.endsWith( QLatin1String( "Interpolator" ) ) )
		type.chop( 12 );

	QString out;
	for ( int i = 0; i < type.length(); i++ ) {
		QChar c = type.at( i );
		if ( i > 0 && c.isUpper() && !type.at( i - 1 ).isUpper() )
			out += QLatin1Char( ' ' );
		out += c;
	}

	return out.simplified();
}

QString TimelineWidget::controllerLabel( const QModelIndex & iController ) const
{
	QString label = shortTypeName( nif->itemName( iController ) );

	// Shader controllers: show what variable they drive instead of the generic type
	for ( const char * fieldName : { "Controlled Variable", "Target Variable", "Type of Controlled Variable", "Type of Controlled Color" } ) {
		QModelIndex iVar = nif->getIndex( iController, fieldName );
		if ( iVar.isValid() ) {
			QString varText = iVar.sibling( iVar.row(), NifModel::ValueCol ).data( Qt::DisplayRole ).toString();
			if ( !varText.isEmpty() )
				label = shortTypeName( nif->itemName( iController ) ).section( ' ', 0, 1 ) + QStringLiteral( ": " ) + varText;
			break;
		}
	}

	return label;
}

void TimelineWidget::sequenceChosen( int comboRow )
{
	if ( comboRow > 0 && comboRow - 1 < sequences.count() && sequences[comboRow - 1].isValid() ) {
		emit indexSelected( QModelIndex( sequences[comboRow - 1] ) );
		if ( !syncingSequence )
			emit sequenceActivated( seqBox->itemText( comboRow ) );
	}

	buildLanes();
	computeRange();
	applyFilter();
	currentLane = lanes.isEmpty() ? -1 : std::min( currentLane, (int)lanes.count() - 1 );
	lanesView->invalidateStrips();
	graphView->invalidateCurves();
	inspector->rebuild();
	updateViews();
}

void TimelineWidget::setSequenceByName( const QString & name )
{
	for ( int i = 1; i < seqBox->count(); i++ ) {
		if ( seqBox->itemText( i ) == name ) {
			if ( seqBox->currentIndex() != i ) {
				seqBox->setCurrentIndex( i );
				syncingSequence = true;
				sequenceChosen( i );
				syncingSequence = false;
			}
			return;
		}
	}
}

void TimelineWidget::buildLanes()
{
	lanes.clear();
	currentLane = -1;
	selKeys.clear();
	primaryKey = QPersistentModelIndex();

	if ( !nif ) {
		infoLabel->clear();
		return;
	}

	int seq = seqBox->currentIndex();

	if ( seq > 0 && seq - 1 < sequences.count() ) {
		QModelIndex iSeq( sequences[seq - 1] );

		QModelIndex iCtrl = nif->getIndex( iSeq, "Controlled Blocks" );
		for ( int r = 0; r < nif->rowCount( iCtrl ); r++ ) {
			QModelIndex iRow = nif->getIndex( iCtrl, r );

			QString nodeName = nif->resolveString( iRow, "Node Name" );
			QString ctype = nif->resolveString( iRow, "Controller Type" );
			QString label = nodeName;
			if ( !ctype.isEmpty() )
				label = ( label.isEmpty() ? QString() : label + QStringLiteral( " · " ) ) + shortTypeName( ctype );
			if ( label.isEmpty() )
				label = tr( "Controlled Block %1" ).arg( r );

			QModelIndex iInterp = nif->getBlockIndex( nif->getLink( iRow, "Interpolator" ), "NiInterpolator" );
			QModelIndex iCtlr = nif->getBlockIndex( nif->getLink( iRow, "Controller" ) );
			if ( iInterp.isValid() ) {
				addInterpolatorLane( iInterp, label, iCtlr );
				TimelineLane & lane = lanes.last();
				lane.targetBlock = findAVObjectByName( nodeName );
				lane.searchText += QLatin1Char( ' ' ) + nodeName.toLower();
			}
		}

		QModelIndex iText = nif->getBlockIndex( nif->getLink( iSeq, "Text Keys" ), "NiTextKeyExtraData" );
		if ( iText.isValid() )
			addTextKeyLane( iText, tr( "Text Keys" ) );

		if ( nif->getIndex( iSeq, "Start Time" ).isValid() ) {
			// widen range display to the sequence bounds
		}
	} else {
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

		QMap<int, QString> seqInterps;
		QMap<int, QString> seqNodeNames;
		for ( const auto & s : sequences ) {
			QModelIndex iSeq( s );
			QString seqName = nif->resolveString( iSeq, "Name" );
			QModelIndex iCtrl = nif->getIndex( iSeq, "Controlled Blocks" );
			for ( int r = 0; r < nif->rowCount( iCtrl ); r++ ) {
				QModelIndex iRow = nif->getIndex( iCtrl, r );
				qint32 l = nif->getLink( iRow, "Interpolator" );
				if ( l >= 0 && !seqInterps.contains( l ) ) {
					QString nodeName = nif->resolveString( iRow, "Node Name" );
					QString ctype = nif->resolveString( iRow, "Controller Type" );
					QString label = seqName + QStringLiteral( " / " ) + nodeName;
					if ( !ctype.isEmpty() )
						label += QStringLiteral( " · " ) + shortTypeName( ctype );
					seqInterps[l] = label;
					seqNodeNames[l] = nodeName;
				}
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
				if ( !referenced ) {
					addInterpolatorLane( iBlock, seqInterps.value( b, tr( "(unreferenced)" ) ) );
					if ( seqNodeNames.contains( b ) ) {
						TimelineLane & lane = lanes.last();
						lane.targetBlock = findAVObjectByName( seqNodeNames[b] );
						lane.searchText += QLatin1Char( ' ' ) + seqNodeNames[b].toLower();
					}
				}
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
	QString ctype = controllerLabel( iController );

	QModelIndex iTarget = nif->getBlockIndex( nif->getLink( iController, "Target" ) );
	QString tname = nif->resolveString( iTarget, "Name" );
	if ( tname.isEmpty() && iTarget.isValid() )
		tname = nif->itemName( iTarget );

	QString label = tname.isEmpty() ? ctype : tname + QStringLiteral( " · " ) + ctype;
	int targetBlock = iTarget.isValid() ? nif->getBlockNumber( iTarget ) : -1;

	// If the target block is a property/shader, resolve up to the AV object that owns it
	if ( targetBlock >= 0 && !nif->blockInherits( iTarget, "NiAVObject" ) ) {
		int p = nif->getParent( targetBlock );
		if ( p >= 0 && nif->blockInherits( nif->getBlockIndex( p ), "NiAVObject" ) ) {
			targetBlock = p;
			QString ownerName = nif->resolveString( nif->getBlockIndex( p ), "Name" );
			if ( !ownerName.isEmpty() )
				label = ownerName + QStringLiteral( " · " ) + ctype;
		}
	}

	int before = lanes.count();

	QModelIndex iInterp = nif->getBlockIndex( nif->getLink( iController, "Interpolator" ), "NiInterpolator" );
	if ( iInterp.isValid() )
		addInterpolatorLane( iInterp, label, iController );

	QModelIndex iWeights = nif->getIndex( iController, "Interpolator Weights" );
	for ( int r = 0; r < nif->rowCount( iWeights ); r++ ) {
		QModelIndex iw = nif->getBlockIndex( nif->getLink( nif->getIndex( iWeights, r ), "Interpolator" ), "NiInterpolator" );
		if ( iw.isValid() )
			addInterpolatorLane( iw, label + tr( " [morph %1]" ).arg( r ), iController );
	}

	// PSys controllers keep a separate visibility interpolator
	QModelIndex iVisInterp = nif->getBlockIndex( nif->getLink( iController, "Visibility Interpolator" ), "NiInterpolator" );
	if ( iVisInterp.isValid() )
		addInterpolatorLane( iVisInterp, label + tr( " [visibility]" ), iController );

	QModelIndex iData = nif->getBlockIndex( nif->getLink( iController, "Data" ) );
	if ( iData.isValid() && lanes.count() == before ) {
		TimelineLane lane;
		lane.label = label;
		lane.iSelect = iController;
		lane.iController = iController;
		lane.blockNums.insert( nif->getBlockNumber( iController ) );
		lane.blockNums.insert( nif->getBlockNumber( iData ) );
		collectChannels( iData, lane );
		finalizeLane( lane );
		lanes.append( lane );
	}

	if ( lanes.count() == before ) {
		TimelineLane lane;
		lane.label = label;
		lane.iSelect = iController;
		lane.iController = iController;
		lane.blockNums.insert( nif->getBlockNumber( iController ) );
		lane.rangeOnly = true;
		lane.start = nif->get<float>( iController, "Start Time" );
		lane.stop = nif->get<float>( iController, "Stop Time" );
		finalizeLane( lane );
		lanes.append( lane );
	}

	for ( int i = before; i < lanes.count(); i++ ) {
		lanes[i].blockNums.insert( nif->getBlockNumber( iController ) );
		lanes[i].iController = iController;
		lanes[i].targetBlock = targetBlock;
		if ( !tname.isEmpty() )
			lanes[i].searchText += QLatin1Char( ' ' ) + tname.toLower();
		// controller range + active flag
		lanes[i].start = nif->get<float>( iController, "Start Time" );
		lanes[i].stop = nif->get<float>( iController, "Stop Time" );
		lanes[i].hasCtrlRange = nif->getIndex( iController, "Start Time" ).isValid()
			&& tlSaneTime( lanes[i].start ) && tlSaneTime( lanes[i].stop )
			&& lanes[i].stop >= lanes[i].start;
		quint16 flags = (quint16)nif->get<int>( iController, "Flags" );
		lanes[i].muted = !( flags & 0x0008 );
	}
}

void TimelineWidget::addInterpolatorLane( const QModelIndex & iInterp, const QString & label, const QModelIndex & iController )
{
	TimelineLane lane;
	lane.label = label;
	lane.tooltip = label + QStringLiteral( "  · " ) + nif->itemName( iInterp );
	lane.iSelect = iInterp;
	lane.iController = iController;
	lane.blockNums.insert( nif->getBlockNumber( iInterp ) );

	if ( iController.isValid() ) {
		lane.start = nif->get<float>( iController, "Start Time" );
		lane.stop = nif->get<float>( iController, "Stop Time" );
		lane.hasCtrlRange = nif->getIndex( iController, "Start Time" ).isValid()
			&& tlSaneTime( lane.start ) && tlSaneTime( lane.stop ) && lane.stop >= lane.start;
		quint16 flags = (quint16)nif->get<int>( iController, "Flags" );
		lane.muted = !( flags & 0x0008 );
	}

	if ( nif->blockInherits( iInterp, "NiBSplineInterpolator" ) ) {
		lane.rangeOnly = true;
		lane.start = nif->get<float>( iInterp, "Start Time" );
		lane.stop = nif->get<float>( iInterp, "Stop Time" );
		if ( !tlSaneTime( lane.start ) || !tlSaneTime( lane.stop ) || lane.stop < lane.start ) {
			lane.start = 0;
			lane.stop = 0;
		}
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
	lane.label = label;
	lane.tooltip = label + QStringLiteral( "  · " ) + nif->itemName( iTextKeys );
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

	if ( lane.tooltip.isEmpty() )
		lane.tooltip = lane.label;

	QStringList chNames;
	for ( const auto & ch : lane.channels )
		chNames << QString( "%1 [%2]" ).arg( ch.name, tlKeyTypeName( ch.interpolation ) );
	if ( !chNames.isEmpty() )
		lane.tooltip += QLatin1Char( '\n' ) + chNames.join( QStringLiteral( ", " ) );

	lane.searchText = ( lane.label + QLatin1Char( ' ' ) + lane.tooltip ).toLower();
}

void TimelineWidget::computeRange()
{
	bool any = false;
	float mn = 0, mx = 1;

	auto expand = [&]( float t ) {
		if ( !tlSaneTime( t ) )
			return;
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

		if ( lane.rangeOnly || lane.hasCtrlRange ) {
			expand( lane.start );
			expand( lane.stop );
		}
	}

	int seq = seqBox->currentIndex();
	if ( seq > 0 && seq - 1 < sequences.count() && nif ) {
		expand( nif->get<float>( QModelIndex( sequences[seq - 1] ), "Start Time" ) );
		expand( nif->get<float>( QModelIndex( sequences[seq - 1] ), "Stop Time" ) );
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

void TimelineWidget::applyFilter()
{
	visibleLanes.clear();

	QString needle = filterText.toLower().trimmed();

	for ( int i = 0; i < lanes.count(); i++ ) {
		if ( !needle.isEmpty() && !lanes[i].searchText.contains( needle ) )
			continue;

		if ( autoIsolate && filterTargetBlock >= 0 ) {
			if ( lanes[i].targetBlock != filterTargetBlock
			     && !lanes[i].blockNums.contains( filterTargetBlock ) )
				continue;
		}

		visibleLanes.append( i );
	}
}

void TimelineWidget::filterEdited( const QString & text )
{
	filterText = text;
	applyFilter();
	lanesView->update();
}

int TimelineWidget::laneRow( int visibleRow ) const
{
	return ( visibleRow >= 0 && visibleRow < visibleLanes.count() ) ? visibleLanes[visibleRow] : -1;
}

int TimelineWidget::visibleRowOf( int lane ) const
{
	return visibleLanes.indexOf( lane );
}

/*
 *  View state helpers
 */

float TimelineWidget::timeToX( float t, int width ) const
{
	float span = std::max( viewT1 - viewT0, 1.0e-6f );
	return labelW + ( t - viewT0 ) / span * std::max( width - labelW - 8, 1 );
}

float TimelineWidget::xToTime( int x, int width ) const
{
	float span = std::max( viewT1 - viewT0, 1.0e-6f );
	return viewT0 + float( x - labelW ) / std::max( width - labelW - 8, 1 ) * span;
}

float TimelineWidget::snapTime( float t ) const
{
	float step = framesMode ? 1.0f / std::max( fps, 1 ) : snapTimeStep;
	if ( !snapOn || step <= 0 )
		return t;
	return std::round( t / step ) * step;
}

float TimelineWidget::snapValue( float v ) const
{
	if ( !snapOn || snapValueStep <= 0 )
		return v;
	return std::round( v / snapValueStep ) * snapValueStep;
}

QString TimelineWidget::formatTime( float t, float step ) const
{
	if ( framesMode )
		return QString::number( (int)std::lround( t * fps ) );

	int decimals = 0;
	if ( step < 1.0f )
		decimals = std::min( 4, (int)std::ceil( -std::log10( step ) ) );

	return QString::number( t, 'f', decimals );
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

	lanesView->invalidateStrips();
	graphView->invalidateCurves();
	updateViews();
}

void TimelineWidget::frameAll()
{
	viewT0 = tMin;
	viewT1 = tMax;
	lanesView->invalidateStrips();
	graphView->invalidateCurves();
	updateViews();
}

void TimelineWidget::frameSelected()
{
	bool any = false;
	float mn = 0, mx = 0;

	for ( const auto & k : selKeys ) {
		int lane, ch, key;
		if ( findKeyRef( QModelIndex( k ), lane, ch, key ) ) {
			float t = lanes[lane].channels[ch].keys[key].time;
			if ( !any ) {
				mn = mx = t;
				any = true;
			} else {
				mn = std::min( mn, t );
				mx = std::max( mx, t );
			}
		}
	}

	if ( !any && currentLane >= 0 && currentLane < lanes.count() ) {
		for ( const auto & key : lanes[currentLane].keys ) {
			if ( !any ) {
				mn = mx = key.time;
				any = true;
			} else {
				mn = std::min( mn, key.time );
				mx = std::max( mx, key.time );
			}
		}
	}

	if ( !any )
		return frameAll();

	float pad = std::max( ( mx - mn ) * 0.1f, 0.05f );
	viewT0 = mn - pad;
	viewT1 = mx + pad;
	lanesView->invalidateStrips();
	graphView->invalidateCurves();
	updateViews();
}

void TimelineWidget::ensurePlayheadVisible()
{
	if ( curTime > viewT1 || curTime < viewT0 ) {
		float span = viewT1 - viewT0;
		viewT0 = curTime;
		viewT1 = curTime + span;
		lanesView->invalidateStrips();
		graphView->invalidateCurves();
	}
}

/*
 *  Selection
 */

void TimelineWidget::selectLane( int lane, bool emitSelect )
{
	if ( lane < 0 || lane >= lanes.count() )
		return;

	currentLane = lane;

	if ( emitSelect && lanes[lane].iSelect.isValid() )
		emit indexSelected( QModelIndex( lanes[lane].iSelect ) );

	graphView->invalidateCurves();
	inspector->rebuild();
	lanesView->update();
}

void TimelineWidget::selectKey( const QModelIndex & keyIdx, bool additive, bool emitSelect )
{
	QPersistentModelIndex p( keyIdx );

	if ( additive ) {
		if ( selKeys.contains( p ) )
			selKeys.removeAll( p );
		else
			selKeys.append( p );
	} else {
		selKeys.clear();
		selKeys.append( p );
	}

	primaryKey = p;

	int lane, ch, key;
	if ( findKeyRef( keyIdx, lane, ch, key ) && lane != currentLane )
		selectLane( lane, false );

	if ( emitSelect && keyIdx.isValid() )
		emit indexSelected( keyIdx );

	inspector->rebuild();
	lanesView->update();
	graphView->update();
}

void TimelineWidget::clearKeySelection()
{
	selKeys.clear();
	primaryKey = QPersistentModelIndex();
	inspector->rebuild();
	lanesView->update();
	graphView->update();
}

bool TimelineWidget::findKeyRef( const QModelIndex & keyIdx, int & lane, int & channel, int & key ) const
{
	if ( !keyIdx.isValid() )
		return false;

	QModelIndex parent = keyIdx.parent();

	for ( int l = 0; l < lanes.count(); l++ ) {
		for ( int c = 0; c < lanes[l].channels.count(); c++ ) {
			if ( QModelIndex( lanes[l].channels[c].iKeysArray ) == parent ) {
				lane = l;
				channel = c;
				key = keyIdx.row();
				return key >= 0 && key < lanes[l].channels[c].keys.count();
			}
		}
	}

	return false;
}

void TimelineWidget::stepToKey( int dir )
{
	if ( currentLane < 0 || currentLane >= lanes.count() )
		return;

	const auto & keys = lanes[currentLane].keys;
	if ( keys.isEmpty() )
		return;

	float best = 0;
	bool found = false;

	for ( const auto & k : keys ) {
		if ( dir > 0 && k.time > curTime + 1.0e-4f ) {
			if ( !found || k.time < best ) {
				best = k.time;
				found = true;
			}
		} else if ( dir < 0 && k.time < curTime - 1.0e-4f ) {
			if ( !found || k.time > best ) {
				best = k.time;
				found = true;
			}
		}
	}

	if ( found ) {
		curTime = best;
		emit timeChanged( best );
		ensurePlayheadVisible();
		updateViews();
	}
}

void TimelineWidget::jumpToMarker( int dir )
{
	float best = 0;
	bool found = false;

	for ( const auto & lane : lanes ) {
		for ( const auto & ch : lane.channels ) {
			if ( ch.type != TimelineChannel::TextVal )
				continue;
			for ( const auto & k : ch.keys ) {
				if ( dir > 0 && k.time > curTime + 1.0e-4f && ( !found || k.time < best ) ) {
					best = k.time;
					found = true;
				} else if ( dir < 0 && k.time < curTime - 1.0e-4f && ( !found || k.time > best ) ) {
					best = k.time;
					found = true;
				}
			}
		}
	}

	if ( found ) {
		curTime = best;
		emit timeChanged( best );
		ensurePlayheadVisible();
		updateViews();
	}
}

/*
 *  External sync
 */

void TimelineWidget::setTime( float t, float mn, float mx )
{
	sceneMin = mn;
	sceneMax = mx;

	if ( prevRangeOn && t > prevEnd + 1.0e-4f ) {
		emit timeChanged( prevStart );
		t = prevStart;
	}

	curTime = t;

	if ( followPlayhead )
		ensurePlayheadVisible();

	updateViews();
}

void TimelineWidget::setCurrentIndex( const QModelIndex & index )
{
	if ( scanning || !nif || !index.isValid() || index.model() != nif )
		return;

	int blockNum = nif->getBlockNumber( index );
	if ( blockNum < 0 )
		return;

	// Geometry isolation filter
	if ( autoIsolate ) {
		QModelIndex iBlock = nif->getBlockIndex( blockNum );
		int avBlock = blockNum;
		QModelIndex iAV = iBlock;
		while ( avBlock >= 0 && !nif->blockInherits( iAV, "NiAVObject" ) ) {
			avBlock = nif->getParent( avBlock );
			iAV = nif->getBlockIndex( avBlock );
		}
		if ( avBlock >= 0 && avBlock != filterTargetBlock ) {
			filterTargetBlock = avBlock;
			applyFilter();
		}
	}

	int lane = -1;
	for ( int i = 0; i < lanes.count(); i++ ) {
		if ( lanes[i].blockNums.contains( blockNum ) ) {
			lane = i;
			break;
		}
	}

	if ( lane >= 0 ) {
		if ( lane != currentLane ) {
			currentLane = lane;
			graphView->invalidateCurves();
		}

		selKeys.clear();
		primaryKey = QPersistentModelIndex();
		QModelIndex walk = index;
		while ( walk.isValid() ) {
			QModelIndex parent = walk.parent();
			for ( const auto & ch : lanes[lane].channels ) {
				if ( parent == QModelIndex( ch.iKeysArray ) ) {
					primaryKey = nif->getIndex( parent, walk.row() );
					selKeys.append( primaryKey );
					break;
				}
			}
			if ( primaryKey.isValid() )
				break;
			walk = parent;
		}

		inspector->rebuild();
	}

	lanesView->update();
	graphView->update();
}

/*
 *  Keyboard shortcuts (see TIMELINE_SHORTCUTS.txt)
 */

void TimelineWidget::keyPressEvent( QKeyEvent * event )
{
	const bool ctrl = event->modifiers() & Qt::ControlModifier;
	const bool shift = event->modifiers() & Qt::ShiftModifier;

	switch ( event->key() ) {
	case Qt::Key_Space:
		emit playPauseRequested();
		return;
	case Qt::Key_Home:
		frameAll();
		return;
	case Qt::Key_End:
		curTime = tMax;
		emit timeChanged( curTime );
		updateViews();
		return;
	case Qt::Key_Comma:
		if ( ctrl )
			jumpToMarker( -1 );
		else
			stepToKey( -1 );
		return;
	case Qt::Key_Period:
		if ( ctrl )
			jumpToMarker( 1 );
		else if ( event->modifiers() & Qt::KeypadModifier )
			frameSelected();
		else
			stepToKey( 1 );
		return;
	case Qt::Key_F:
		frameSelected();
		return;
	case Qt::Key_Delete:
	case Qt::Key_Backspace:
		deleteSelectedKeys();
		return;
	case Qt::Key_I:
		if ( currentLane >= 0 )
			insertKeyAtTime( currentLane, snapTime( curTime ) );
		return;
	case Qt::Key_D:
		if ( shift )
			duplicateSelectedKeys();
		return;
	case Qt::Key_C:
		if ( ctrl )
			copySelectedKeys();
		return;
	case Qt::Key_V:
		if ( ctrl )
			pasteKeysAt( snapTime( curTime ) );
		return;
	case Qt::Key_S:
		if ( !ctrl && !selKeys.isEmpty() ) {
			bool ok = false;
			double f = QInputDialog_getDouble_compat( this, tr( "Scale keys" ),
				tr( "Scale selected key times around the playhead by factor:" ), 1.0, 0.01, 100.0, 3, &ok );
			if ( ok )
				scaleSelectedKeys( (float)f );
			return;
		}
		break;
	case Qt::Key_Left:
		nudgeSelectedKeys( -( framesMode ? 1.0f / std::max( fps, 1 ) : snapTimeStep ), 0 );
		return;
	case Qt::Key_Right:
		nudgeSelectedKeys( framesMode ? 1.0f / std::max( fps, 1 ) : snapTimeStep, 0 );
		return;
	case Qt::Key_Up:
		nudgeSelectedKeys( 0, 1 );
		return;
	case Qt::Key_Down:
		nudgeSelectedKeys( 0, -1 );
		return;
	case Qt::Key_Escape:
		clearKeySelection();
		return;
	}

	QWidget::keyPressEvent( event );
}

void TimelineWidget::showLaneContextMenu( int lane, const QPoint & globalPos )
{
	if ( lane < 0 || lane >= lanes.count() )
		return;

	QMenu menu;
	TimelineLane & l = lanes[lane];

	menu.addAction( tr( "Select in tree" ), [this, lane]() { selectLane( lane, true ); } );
	menu.addSeparator();

	menu.addAction( tr( "Insert key at playhead" ), [this, lane]() { insertKeyAtTime( lane, snapTime( curTime ) ); } );

	if ( !selKeys.isEmpty() ) {
		menu.addAction( tr( "Delete selected keys" ), [this]() { deleteSelectedKeys(); } );

		QMenu * ease = menu.addMenu( tr( "Easing (quadratic keys)" ) );
		ease->addAction( tr( "Flatten tangents" ), [this]() { applyEasing( 0 ); } );
		ease->addAction( tr( "Smooth (Catmull-Rom)" ), [this]() { applyEasing( 1 ); } );
		ease->addAction( tr( "Linearize" ), [this]() { applyEasing( 2 ); } );
		ease->addAction( tr( "Ease in" ), [this]() { applyEasing( 3 ); } );
		ease->addAction( tr( "Ease out" ), [this]() { applyEasing( 4 ); } );
	}

	menu.addSeparator();
	menu.addAction( tr( "Copy channel keys" ), [this, lane]() { copyChannels( lane ); } );
	if ( !channelClipboard.isEmpty() )
		menu.addAction( tr( "Paste channel keys onto this interpolator" ), [this, lane]() { pasteChannels( lane ); } );
	menu.addAction( tr( "Clear all keys" ), [this, lane]() { clearChannelKeys( lane ); } );

	QMenu * interp = menu.addMenu( tr( "Set interpolation" ) );
	for ( int c = 0; c < l.channels.count(); c++ ) {
		if ( !l.channels[c].plottable() || !l.channels[c].iKeyGroup.isValid() )
			continue;
		QMenu * chMenu = l.channels.count() > 1 ? interp->addMenu( l.channels[c].name ) : interp;
		const int types[4] = { 1, 2, 3, 5 };
		for ( int t : types ) {
			chMenu->addAction( tlKeyTypeName( t ), [this, lane, c, t]() {
				setChannelInterpolation( lane, c, t, false );
			} );
		}
		if ( l.channels.count() == 1 )
			break;
	}

	menu.addSeparator();
	menu.addAction( tr( "Export channel(s) to CSV..." ), [this, lane]() { csvExport( lane ); } );
	menu.addAction( tr( "Import channel(s) from CSV..." ), [this, lane]() { csvImport( lane ); } );

	menu.addSeparator();
	QMenu * viz = menu.addMenu( tr( "Lane display" ) );
	auto addViz = [&]( const QString & text, TimelineLane::Viz v ) {
		QAction * a = viz->addAction( text, [this, lane, v]() {
			lanes[lane].viz = v;
			lanesView->invalidateStrips();
		} );
		a->setCheckable( true );
		a->setChecked( l.viz == v );
	};
	addViz( tr( "Auto" ), TimelineLane::VizAuto );
	addViz( tr( "Diamonds" ), TimelineLane::VizDiamonds );
	addViz( tr( "Sparkline" ), TimelineLane::VizSparkline );
	addViz( tr( "Strip" ), TimelineLane::VizStrip );

	if ( l.iController.isValid() ) {
		menu.addSeparator();
		QAction * mute = menu.addAction( l.muted ? tr( "Unmute (set controller active)" ) : tr( "Mute (set controller inactive)" ),
			[this, lane]() { toggleLaneMute( lane ); } );
		Q_UNUSED( mute );
	}

	QAction * lock = menu.addAction( l.locked ? tr( "Unlock lane" ) : tr( "Lock lane (block edits)" ), [this, lane]() {
		lanes[lane].locked = !lanes[lane].locked;
		lanesView->update();
	} );
	Q_UNUSED( lock );

	menu.exec( globalPos );
}
