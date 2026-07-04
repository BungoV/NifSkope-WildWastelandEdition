/***** BEGIN LICENSE BLOCK *****

BSD License - see timeline.h

***** END LICENCE BLOCK *****/

#include "timeline.h"
#include "timeline_p.h"

#include "model/nifmodel.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cmath>

//! @file timelineviews.cpp TimelineLanesView, TimelineGraphView, TimelineInspector

double QInputDialog_getDouble_compat( QWidget * parent, const QString & title, const QString & label,
                                      double value, double min, double max, int decimals, bool * ok )
{
	return QInputDialog::getDouble( parent, title, label, value, min, max, decimals, ok );
}

// Draw a key marker whose shape encodes the interpolation type
static void tlDrawKeyMarker( QPainter & p, const QPointF & c, float r, int interpolation )
{
	switch ( interpolation ) {
	case 2:  // quadratic: circle
		p.drawEllipse( c, r * 0.9, r * 0.9 );
		break;
	case 5:  // const: square
		p.drawRect( QRectF( c.x() - r * 0.75, c.y() - r * 0.75, r * 1.5, r * 1.5 ) );
		break;
	default: // linear / TBC / unknown: diamond
		p.drawPolygon( tlDiamond( c, r ) );
		break;
	}
}


/*
 *  TimelineLanesView
 */

TimelineLanesView::TimelineLanesView( TimelineWidget * parent ) : QWidget( parent ), tl( parent )
{
	setMouseTracking( true );
	setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

	marqueeTimer = new QTimer( this );
	marqueeTimer->setInterval( 120 );
	connect( marqueeTimer, &QTimer::timeout, [this]() {
		marqueeOffset += 6;
		update();
	} );
}

int TimelineLanesView::rowAtY( int y ) const
{
	if ( y < TL_RULER_H )
		return -1;
	if ( y < TL_RULER_H + TL_SUMMARY_H )
		return -2;

	int row = ( y - TL_RULER_H - TL_SUMMARY_H + tl->laneScroll->value() ) / TL_LANE_H;
	return ( row >= 0 && row < tl->visibleLanes.count() ) ? row : -3;
}

QModelIndex TimelineLanesView::keyAt( int lane, int x ) const
{
	if ( lane < 0 || lane >= tl->lanes.count() )
		return QModelIndex();

	const TimelineLane & l = tl->lanes[lane];
	float bestDist = TL_KEY_R + 3.0f;
	QModelIndex best;

	for ( const auto & key : l.keys ) {
		float d = std::abs( tl->timeToX( key.time, width() ) - x );
		if ( d < bestDist ) {
			bestDist = d;
			best = QModelIndex( key.idx );
		}
	}

	return best;
}

void TimelineLanesView::scrub( int x )
{
	float t = tl->xToTime( std::max( x, tl->labelW ), width() );
	t = tl->snapTime( t );
	tl->curTime = t;
	emit tl->timeChanged( t );
	tl->updateViews();
}

QImage TimelineLanesView::laneStrip( const TimelineLane & lane, int laneIdx, int w, int h )
{
	quint64 cacheKey = (quint64)laneIdx << 32 | (quint64)( w & 0xffff ) << 8 | (quint64)( h & 0xff );
	auto it = stripCache.constFind( cacheKey );
	if ( it != stripCache.constEnd() )
		return *it;

	QImage img( std::max( w, 1 ), std::max( h, 1 ), QImage::Format_ARGB32_Premultiplied );
	img.fill( Qt::transparent );

	// Pick the channel to visualize: first color channel, else first float channel
	const TimelineChannel * colorCh = nullptr;
	const TimelineChannel * floatCh = nullptr;
	for ( const auto & ch : lane.channels ) {
		if ( ch.isColor() && !colorCh )
			colorCh = &ch;
		else if ( ch.type == TimelineChannel::Float && !floatCh )
			floatCh = &ch;
	}

	const TimelineChannel * ch = colorCh ? colorCh : floatCh;
	if ( !ch || !ch->iKeyGroup.isValid() ) {
		stripCache.insert( cacheKey, img );
		return img;
	}

	QPainter sp( &img );

	// Checkerboard for transparency
	bool wantsChecker = ( colorCh && colorCh->type == TimelineChannel::Color4Val ) || ( !colorCh && floatCh );
	if ( wantsChecker ) {
		const int cs = 4;
		for ( int y = 0; y < h; y += cs ) {
			for ( int x = 0; x < w; x += cs ) {
				bool dark = ( ( x / cs ) + ( y / cs ) ) & 1;
				sp.fillRect( x, y, cs, cs, dark ? QColor( 90, 90, 90 ) : QColor( 160, 160, 160 ) );
			}
		}
	}

	QModelIndex iKeyGroup( ch->iKeyGroup );
	int last = 0;
	float vmin = 0, vmax = 1;

	if ( floatCh == ch ) {
		// establish range for intensity mapping
		bool first = true;
		int l2 = 0;
		for ( int x = 0; x < w; x += 4 ) {
			float t = tl->viewT0 + ( tl->viewT1 - tl->viewT0 ) * x / std::max( w - 1, 1 );
			float v;
			if ( Controller::interpolate( v, iKeyGroup, t, l2 ) ) {
				if ( first ) {
					vmin = vmax = v;
					first = false;
				} else {
					vmin = std::min( vmin, v );
					vmax = std::max( vmax, v );
				}
			}
		}
		if ( vmax - vmin < 1.0e-6f ) {
			vmin = std::min( vmin, 0.0f );
			vmax = std::max( vmax, 1.0f );
		}
	}

	for ( int x = 0; x < w; x++ ) {
		float t = tl->viewT0 + ( tl->viewT1 - tl->viewT0 ) * x / std::max( w - 1, 1 );
		QColor col;
		bool ok = false;

		if ( ch->type == TimelineChannel::Color3Val ) {
			Color3 c;
			ok = Controller::interpolate( c, iKeyGroup, t, last );
			if ( ok )
				col = QColor::fromRgbF( std::clamp( c.red(), 0.0f, 1.0f ), std::clamp( c.green(), 0.0f, 1.0f ), std::clamp( c.blue(), 0.0f, 1.0f ) );
		} else if ( ch->type == TimelineChannel::Color4Val ) {
			Color4 c;
			ok = Controller::interpolate( c, iKeyGroup, t, last );
			if ( ok )
				col = QColor::fromRgbF( std::clamp( c.red(), 0.0f, 1.0f ), std::clamp( c.green(), 0.0f, 1.0f ),
				                        std::clamp( c.blue(), 0.0f, 1.0f ), std::clamp( c.alpha(), 0.0f, 1.0f ) );
		} else {
			float v;
			ok = Controller::interpolate( v, iKeyGroup, t, last );
			if ( ok ) {
				float a = std::clamp( ( v - vmin ) / std::max( vmax - vmin, 1.0e-6f ), 0.0f, 1.0f );
				col = QColor::fromRgbF( 1.0f, 1.0f, 1.0f, a );
			}
		}

		if ( ok ) {
			sp.setPen( Qt::NoPen );
			sp.setBrush( col );
			sp.drawRect( x, 0, 1, h );
		}
	}

	sp.end();
	stripCache.insert( cacheKey, img );
	return img;
}

void TimelineLanesView::paintLaneContent( QPainter & p, const TimelineLane & lane, int laneIdx, const QRect & r )
{
	const QPalette & pal = palette();
	int w = width();
	float cy = r.center().y() + 0.5f;

	// Controller range shading + draggable edges
	if ( lane.hasCtrlRange || lane.rangeOnly ) {
		float x0 = tl->timeToX( lane.start, w );
		float x1 = tl->timeToX( lane.stop, w );
		QColor barCol = pal.color( QPalette::Highlight );
		barCol.setAlpha( lane.rangeOnly ? 110 : 36 );
		QRectF bar( std::min( x0, x1 ), r.top() + 2, std::max( std::abs( x1 - x0 ), 2.0f ), r.height() - 4 );
		p.setPen( Qt::NoPen );
		p.setBrush( barCol );
		p.drawRoundedRect( bar, 3, 3 );

		if ( lane.hasCtrlRange ) {
			QColor edge = pal.color( QPalette::Highlight );
			edge.setAlpha( 130 );
			p.setPen( QPen( edge, 2 ) );
			p.drawLine( QPointF( x0, r.top() + 1 ), QPointF( x0, r.bottom() - 1 ) );
			p.drawLine( QPointF( x1, r.top() + 1 ), QPointF( x1, r.bottom() - 1 ) );
		}
	}

	if ( lane.rangeOnly )
		return;

	// Determine effective viz mode
	TimelineLane::Viz viz = lane.viz;
	bool hasColor = false, hasBoolOnly = !lane.channels.isEmpty(), hasFloat = false;
	for ( const auto & ch : lane.channels ) {
		if ( ch.isColor() )
			hasColor = true;
		if ( ch.type != TimelineChannel::BoolVal && ch.type != TimelineChannel::TextVal )
			hasBoolOnly = false;
		if ( ch.type == TimelineChannel::Float )
			hasFloat = true;
	}

	if ( viz == TimelineLane::VizAuto )
		viz = hasColor ? TimelineLane::VizStrip
		    : hasBoolOnly ? TimelineLane::VizDiamonds  // bool bars drawn below regardless
		    : hasFloat ? TimelineLane::VizSparkline : TimelineLane::VizDiamonds;

	// Color / intensity strip
	if ( viz == TimelineLane::VizStrip ) {
		int stripW = w - tl->labelW - 8;
		if ( stripW > 4 ) {
			QImage strip = laneStrip( lane, laneIdx, stripW, r.height() - 4 );
			p.drawImage( QPoint( tl->labelW, r.top() + 2 ), strip );
		}
	}

	// Bool on/off bars
	for ( const auto & ch : lane.channels ) {
		if ( ch.type != TimelineChannel::BoolVal || ch.keys.isEmpty() )
			continue;

		QColor onCol = pal.color( QPalette::Highlight );
		onCol.setAlpha( 150 );
		p.setPen( Qt::NoPen );
		p.setBrush( onCol );

		for ( int k = 0; k < ch.keys.count(); k++ ) {
			int v = tl->nif ? tl->nif->get<int>( QModelIndex( ch.keys[k].idx ), "Value" ) : 0;
			if ( !v )
				continue;
			float x0 = tl->timeToX( ch.keys[k].time, w );
			float x1 = ( k + 1 < ch.keys.count() ) ? tl->timeToX( ch.keys[k + 1].time, w )
			                                       : tl->timeToX( std::max( lane.stop, ch.keys[k].time ), w );
			if ( x1 < x0 + 2 )
				x1 = x0 + 2;
			p.drawRect( QRectF( x0, cy - 3, x1 - x0, 6 ) );
		}
	}

	// Sparklines
	if ( viz == TimelineLane::VizSparkline ) {
		for ( const auto & ch : lane.channels ) {
			if ( ch.type != TimelineChannel::Float || !ch.iKeyGroup.isValid() || ch.keys.isEmpty() )
				continue;

			QModelIndex iKeyGroup( ch.iKeyGroup );
			int last = 0;
			float vmin = 0, vmax = 0;
			bool first = true;
			QVector<QPointF> pts;
			int x0 = tl->labelW, x1 = w - 8;

			for ( int x = x0; x <= x1; x += 2 ) {
				float t = tl->xToTime( x, w );
				float v;
				if ( Controller::interpolate( v, iKeyGroup, t, last ) ) {
					pts.append( QPointF( x, v ) );
					if ( first ) {
						vmin = vmax = v;
						first = false;
					} else {
						vmin = std::min( vmin, v );
						vmax = std::max( vmax, v );
					}
				}
			}

			if ( pts.count() > 1 ) {
				float span = std::max( vmax - vmin, 1.0e-6f );
				QPainterPath path;
				for ( int i = 0; i < pts.count(); i++ ) {
					float y = r.bottom() - 2 - ( pts[i].y() - vmin ) / span * ( r.height() - 5 );
					if ( i == 0 )
						path.moveTo( pts[i].x(), y );
					else
						path.lineTo( pts[i].x(), y );
				}
				QColor lc = pal.color( QPalette::Text );
				lc.setAlpha( 110 );
				p.setPen( QPen( lc, 1 ) );
				p.setBrush( Qt::NoBrush );
				p.drawPath( path );
			}
		}
	}

	// Key markers per channel (shape encodes interpolation)
	QFontMetrics fm( p.font() );
	for ( const auto & ch : lane.channels ) {
		for ( const auto & key : ch.keys ) {
			float x = tl->timeToX( key.time, w );
			if ( x < tl->labelW - TL_KEY_R || x > w + TL_KEY_R )
				continue;

			bool sel = tl->selKeys.contains( key.idx );
			bool outOfRange = lane.hasCtrlRange && ( key.time < lane.start - 1.0e-4f || key.time > lane.stop + 1.0e-4f );

			p.setPen( outOfRange ? QPen( QColor( 0xe2, 0x4b, 0x4a ), 1 ) : QPen( Qt::NoPen ) );
			p.setBrush( sel ? palette().color( QPalette::BrightText )
			                : outOfRange ? QColor( 0xe2, 0x4b, 0x4a )
			                : palette().color( QPalette::Highlight ) );
			tlDrawKeyMarker( p, QPointF( x, cy ), sel ? TL_KEY_R + 1.5f : TL_KEY_R, ch.interpolation );

			if ( !key.text.isEmpty() ) {
				p.setPen( palette().color( QPalette::Text ) );
				p.drawText( QPointF( x + TL_KEY_R + 2, cy + fm.ascent() * 0.5f - 1 ), key.text );
			}
		}
	}
}

void TimelineLanesView::paintEvent( QPaintEvent * )
{
	QPainter p( this );
	p.setRenderHint( QPainter::Antialiasing );

	const QPalette & pal = palette();
	int w = width();
	int h = height();

	p.fillRect( rect(), pal.base() );

	int headerH = TL_RULER_H + TL_SUMMARY_H;
	int contentH = tl->visibleLanes.count() * TL_LANE_H;
	int viewH = h - headerH;
	tl->laneScroll->setRange( 0, std::max( 0, contentH - viewH ) );
	tl->laneScroll->setPageStep( std::max( viewH, TL_LANE_H ) );
	int scroll = tl->laneScroll->value();

	QColor gridCol = pal.color( QPalette::Text );
	gridCol.setAlpha( 40 );

	QFont f = p.font();
	f.setPointSizeF( std::max( 7.0, f.pointSizeF() - 1.0 ) );
	p.setFont( f );
	QFontMetrics fm( f );

	// Lanes
	for ( int row = 0; row < tl->visibleLanes.count(); row++ ) {
		int laneIdx = tl->visibleLanes[row];
		int y = headerH + row * TL_LANE_H - scroll;
		if ( y + TL_LANE_H < headerH || y > h )
			continue;

		const TimelineLane & lane = tl->lanes[laneIdx];
		QRect laneRect( 0, y, w, TL_LANE_H );

		if ( laneIdx == tl->currentLane ) {
			QColor hl = pal.color( QPalette::Highlight );
			hl.setAlpha( 70 );
			p.fillRect( laneRect, hl );
		} else if ( row & 1 ) {
			p.fillRect( laneRect, pal.alternateBase() );
		}

		// Gutter icons: mute + lock
		int textX = 4;
		if ( lane.iController.isValid() ) {
			p.setPen( lane.muted ? QColor( 0xe2, 0x4b, 0x4a ) : gridCol );
			p.drawText( QRect( 2, y, 14, TL_LANE_H ), Qt::AlignCenter, lane.muted ? QStringLiteral( "M" ) : QStringLiteral( "♪" ) );
			textX = 16;
		}
		if ( lane.locked ) {
			p.setPen( pal.color( QPalette::Text ) );
			p.drawText( QRect( textX, y, 12, TL_LANE_H ), Qt::AlignCenter, QStringLiteral( "🔒" ) );
			textX += 13;
		}

		// Label with marquee scroll for long names
		p.setPen( lane.muted ? gridCol : pal.color( QPalette::Text ) );
		int availW = tl->labelW - textX - 6;
		int textW = fm.horizontalAdvance( lane.label );

		p.save();
		p.setClipRect( QRect( textX, y, availW, TL_LANE_H ) );
		if ( textW > availW && row == hoverRow ) {
			int off = marqueeOffset % ( textW + 40 );
			p.drawText( QRect( textX - off, y, textW + 10, TL_LANE_H ), Qt::AlignVCenter | Qt::AlignLeft, lane.label );
			p.drawText( QRect( textX - off + textW + 40, y, textW + 10, TL_LANE_H ), Qt::AlignVCenter | Qt::AlignLeft, lane.label );
		} else {
			p.drawText( QRect( textX, y, availW, TL_LANE_H ), Qt::AlignVCenter | Qt::AlignLeft,
				fm.elidedText( lane.label, Qt::ElideMiddle, availW ) );
		}
		p.restore();

		p.save();
		p.setClipRect( QRect( tl->labelW, headerH, w - tl->labelW, viewH ) );
		paintLaneContent( p, lane, laneIdx, laneRect );
		p.restore();
	}

	// Label / time separator (draggable)
	p.setPen( QPen( gridCol, 2 ) );
	p.drawLine( tl->labelW, 0, tl->labelW, h );

	// Summary row
	QRect sumRect( 0, TL_RULER_H, w, TL_SUMMARY_H );
	p.fillRect( sumRect, pal.window() );
	p.setPen( pal.color( QPalette::Text ) );
	p.drawText( QRect( 4, TL_RULER_H, tl->labelW - 8, TL_SUMMARY_H ), Qt::AlignVCenter | Qt::AlignLeft, tr( "Summary" ) );

	p.save();
	p.setClipRect( QRect( tl->labelW, TL_RULER_H, w - tl->labelW, TL_SUMMARY_H ) );
	p.setPen( Qt::NoPen );
	QColor sumCol = pal.color( QPalette::Highlight );
	sumCol.setAlpha( 170 );
	p.setBrush( sumCol );
	for ( int laneIdx : tl->visibleLanes ) {
		for ( const auto & key : tl->lanes[laneIdx].keys ) {
			float x = tl->timeToX( key.time, w );
			if ( x >= tl->labelW - 2 && x <= w + 2 )
				p.drawPolygon( tlDiamond( QPointF( x, TL_RULER_H + TL_SUMMARY_H * 0.5f ), 3.0f ) );
		}
	}
	p.restore();

	p.setPen( gridCol );
	p.drawLine( 0, TL_RULER_H + TL_SUMMARY_H, w, TL_RULER_H + TL_SUMMARY_H );

	// Ruler
	p.fillRect( QRect( 0, 0, w, TL_RULER_H ), pal.window() );
	p.setPen( gridCol );
	p.drawLine( 0, TL_RULER_H, w, TL_RULER_H );

	// Preview range band
	if ( tl->prevRangeOn ) {
		float px0 = tl->timeToX( tl->prevStart, w );
		float px1 = tl->timeToX( tl->prevEnd, w );
		QColor prCol = pal.color( QPalette::Highlight );
		prCol.setAlpha( 90 );
		p.fillRect( QRectF( px0, 0, px1 - px0, TL_RULER_H ), prCol );
	}

	float step = tlNiceStep( ( tl->viewT1 - tl->viewT0 ) / std::max( ( w - tl->labelW ) / 70, 2 ) );
	if ( tl->framesMode ) {
		float frameStep = tlNiceStep( ( tl->viewT1 - tl->viewT0 ) * tl->fps / std::max( ( w - tl->labelW ) / 70, 2 ) );
		step = frameStep / std::max( tl->fps, 1 );
	}
	float t = std::floor( tl->viewT0 / step ) * step;

	for ( ; t <= tl->viewT1 + step; t += step ) {
		float x = tl->timeToX( t, w );
		if ( x < tl->labelW )
			continue;

		p.setPen( gridCol );
		p.drawLine( QPointF( x, TL_RULER_H ), QPointF( x, h ) );
		p.setPen( pal.color( QPalette::Text ) );
		p.drawLine( QPointF( x, TL_RULER_H - 5 ), QPointF( x, TL_RULER_H ) );
		p.drawText( QPointF( x + 2, TL_RULER_H - 6 ), tl->formatTime( t, step ) );
	}

	// Text key markers across the ruler
	p.save();
	p.setClipRect( QRect( tl->labelW, 0, w - tl->labelW, h ) );
	for ( const auto & lane : tl->lanes ) {
		for ( const auto & ch : lane.channels ) {
			if ( ch.type != TimelineChannel::TextVal )
				continue;
			for ( const auto & key : ch.keys ) {
				float x = tl->timeToX( key.time, w );
				QColor mc( 0xd8, 0xa0, 0x30 );
				p.setPen( QPen( mc, 1, Qt::DashLine ) );
				p.drawLine( QPointF( x, TL_RULER_H ), QPointF( x, h ) );
				p.setPen( mc );
				p.setBrush( mc );
				QPolygonF marker;
				marker << QPointF( x - 4, 1 ) << QPointF( x + 4, 1 ) << QPointF( x, 8 );
				p.drawPolygon( marker );
			}
		}
	}
	p.restore();

	// Playhead
	float px = tl->timeToX( tl->curTime, w );
	if ( px >= tl->labelW ) {
		p.setPen( QPen( QColor( 0xe0, 0x40, 0x40 ), 1.5 ) );
		p.drawLine( QPointF( px, 0 ), QPointF( px, h ) );
	}

	// Rubber band
	if ( dragMode == DragRubber && dragMoved ) {
		QColor rb = pal.color( QPalette::Highlight );
		p.setPen( QPen( rb, 1, Qt::DashLine ) );
		rb.setAlpha( 40 );
		p.setBrush( rb );
		p.drawRect( rubberRect );
	}

	if ( tl->lanes.isEmpty() ) {
		p.setPen( pal.color( QPalette::Text ) );
		p.drawText( rect().adjusted( 0, headerH, 0, 0 ), Qt::AlignCenter, tr( "No animation controllers in this file" ) );
	} else if ( tl->visibleLanes.isEmpty() ) {
		p.setPen( pal.color( QPalette::Text ) );
		p.drawText( rect().adjusted( 0, headerH, 0, 0 ), Qt::AlignCenter, tr( "All lanes filtered out" ) );
	}
}

void TimelineLanesView::mousePressEvent( QMouseEvent * ev )
{
	setFocus( Qt::MouseFocusReason );

	if ( ev->button() != Qt::LeftButton )
		return;

	QPoint pos = ev->pos();
	dragStart = pos;
	dragMoved = false;
	dragOrigTimes.clear();

	// Gutter resize
	if ( std::abs( pos.x() - tl->labelW ) <= 4 ) {
		dragMode = DragGutter;
		return;
	}

	// Ruler
	if ( pos.y() < TL_RULER_H ) {
		if ( pos.x() < tl->labelW )
			return;
		if ( ev->modifiers() & Qt::ShiftModifier ) {
			dragMode = DragPrevRange;
			tl->prevRangeOn = true;
			tl->prevStart = tl->prevEnd = tl->snapTime( tl->xToTime( pos.x(), width() ) );
			update();
		} else {
			dragMode = DragScrub;
			scrub( pos.x() );
		}
		return;
	}

	// Summary row: select all keys near this time across visible lanes
	if ( pos.y() < TL_RULER_H + TL_SUMMARY_H ) {
		if ( pos.x() < tl->labelW )
			return;

		bool additive = ev->modifiers() & Qt::ShiftModifier;
		if ( !additive )
			tl->selKeys.clear();

		for ( int laneIdx : tl->visibleLanes ) {
			for ( const auto & key : tl->lanes[laneIdx].keys ) {
				if ( std::abs( tl->timeToX( key.time, width() ) - pos.x() ) <= TL_KEY_R ) {
					if ( !tl->selKeys.contains( key.idx ) )
						tl->selKeys.append( key.idx );
				}
			}
		}

		if ( !tl->selKeys.isEmpty() ) {
			tl->primaryKey = tl->selKeys.first();
			dragMode = DragKeys;
			dragStartTime = tl->xToTime( pos.x(), width() );
			for ( const auto & k : tl->selKeys ) {
				int lane, ch, key;
				if ( tl->findKeyRef( QModelIndex( k ), lane, ch, key ) )
					dragOrigTimes.append( { k, tl->lanes[lane].channels[ch].keys[key].time } );
			}
		}

		tl->inspector->rebuild();
		update();
		tl->graphView->update();
		return;
	}

	int row = rowAtY( pos.y() );
	int laneIdx = tl->laneRow( row );
	if ( laneIdx < 0 )
		return;

	TimelineLane & lane = tl->lanes[laneIdx];

	// Gutter icon clicks
	if ( pos.x() < tl->labelW ) {
		if ( lane.iController.isValid() && pos.x() < 16 ) {
			tl->toggleLaneMute( laneIdx );
			return;
		}
		tl->selectLane( laneIdx, true );
		return;
	}

	// Controller range edge drag
	if ( lane.hasCtrlRange && !lane.locked ) {
		float x0 = tl->timeToX( lane.start, width() );
		float x1 = tl->timeToX( lane.stop, width() );
		if ( std::abs( pos.x() - x0 ) <= 3 || std::abs( pos.x() - x1 ) <= 3 ) {
			dragMode = DragCtrlRange;
			dragLane = laneIdx;
			dragRangeEdge = ( std::abs( pos.x() - x1 ) < std::abs( pos.x() - x0 ) ) ? 1 : 0;
			return;
		}
	}

	// Key hit?
	QModelIndex keyIdx = keyAt( laneIdx, pos.x() );
	if ( keyIdx.isValid() ) {
		bool additive = ev->modifiers() & Qt::ShiftModifier;
		if ( !additive && !tl->selKeys.contains( QPersistentModelIndex( keyIdx ) ) )
			tl->selectKey( keyIdx, false );
		else if ( additive )
			tl->selectKey( keyIdx, true );
		else
			tl->primaryKey = QPersistentModelIndex( keyIdx );

		if ( !lane.locked ) {
			dragMode = DragKeys;
			dragLane = laneIdx;
			dragStartTime = tl->xToTime( pos.x(), width() );
			for ( const auto & k : tl->selKeys ) {
				int l, c, ki;
				if ( tl->findKeyRef( QModelIndex( k ), l, c, ki ) && !tl->lanes[l].locked )
					dragOrigTimes.append( { k, tl->lanes[l].channels[c].keys[ki].time } );
			}
		}
		return;
	}

	// Empty lane area: select lane, start rubber band
	tl->selectLane( laneIdx, true );
	dragMode = DragRubber;
	rubberRect = QRect( pos, pos );
}

void TimelineLanesView::mouseMoveEvent( QMouseEvent * ev )
{
	QPoint pos = ev->pos();

	// Hover tracking for marquee
	if ( dragMode == DragNone ) {
		int row = rowAtY( pos.y() );
		if ( row != hoverRow ) {
			hoverRow = row;
			marqueeOffset = 0;
			int laneIdx = tl->laneRow( row );
			if ( laneIdx >= 0 ) {
				QFontMetrics fm( font() );
				if ( fm.horizontalAdvance( tl->lanes[laneIdx].label ) > tl->labelW - 24 )
					marqueeTimer->start();
				else
					marqueeTimer->stop();
				setToolTip( tl->lanes[laneIdx].tooltip );
			} else {
				marqueeTimer->stop();
				setToolTip( QString() );
			}
			update();
		}

		if ( std::abs( pos.x() - tl->labelW ) <= 4 || dragMode == DragGutter )
			setCursor( Qt::SplitHCursor );
		else
			unsetCursor();
	}

	if ( !( ev->buttons() & Qt::LeftButton ) )
		return;

	if ( ( pos - dragStart ).manhattanLength() > 2 )
		dragMoved = true;

	switch ( dragMode ) {
	case DragScrub:
		scrub( pos.x() );
		break;

	case DragGutter:
		tl->labelW = std::clamp( pos.x(), 100, width() - 100 );
		invalidateStrips();
		tl->graphView->update();
		break;

	case DragPrevRange:
		tl->prevEnd = tl->snapTime( tl->xToTime( pos.x(), width() ) );
		if ( tl->prevEnd < tl->prevStart )
			std::swap( tl->prevStart, tl->prevEnd );
		update();
		break;

	case DragCtrlRange:
		if ( dragLane >= 0 && dragLane < tl->lanes.count() && tl->nif ) {
			float t = tl->snapTime( tl->xToTime( pos.x(), width() ) );
			const TimelineLane & lane = tl->lanes[dragLane];
			QModelIndex iField = tl->nif->getIndex( QModelIndex( lane.iController ), dragRangeEdge ? "Stop Time" : "Start Time" );
			if ( iField.isValid() )
				tl->pushFieldChange( iField, t, !dragMoved );
		}
		break;

	case DragKeys:
		if ( !dragOrigTimes.isEmpty() ) {
			float dt = tl->xToTime( pos.x(), width() ) - dragStartTime;
			bool first = true;
			for ( const auto & pair : dragOrigTimes ) {
				int l, c, k;
				if ( tl->findKeyRef( QModelIndex( pair.first ), l, c, k ) ) {
					float nt = tl->snapTime( pair.second + dt );
					nt = tl->clampKeyTime( tl->lanes[l].channels[c], k, nt );
					QModelIndex iTime = tl->nif->getIndex( QModelIndex( pair.first ), "Time" );
					if ( iTime.isValid() ) {
						tl->pushFieldChange( iTime, nt, first && !dragMoved );
						first = false;
					}
				}
			}
			invalidateStrips();
			tl->graphView->invalidateCurves();
		}
		break;

	case DragRubber:
		rubberRect = QRect( dragStart, pos ).normalized();
		update();
		break;

	default:
		break;
	}
}

void TimelineLanesView::mouseReleaseEvent( QMouseEvent * ev )
{
	Q_UNUSED( ev );

	if ( dragMode == DragRubber && dragMoved ) {
		// Select all keys inside the rubber rect
		bool additive = QApplication::keyboardModifiers() & Qt::ShiftModifier;
		if ( !additive )
			tl->selKeys.clear();

		int headerH = TL_RULER_H + TL_SUMMARY_H;
		int scroll = tl->laneScroll->value();

		for ( int row = 0; row < tl->visibleLanes.count(); row++ ) {
			int laneIdx = tl->visibleLanes[row];
			int y = headerH + row * TL_LANE_H - scroll + TL_LANE_H / 2;
			if ( y < rubberRect.top() || y > rubberRect.bottom() )
				continue;

			for ( const auto & key : tl->lanes[laneIdx].keys ) {
				float x = tl->timeToX( key.time, width() );
				if ( x >= rubberRect.left() && x <= rubberRect.right() ) {
					if ( !tl->selKeys.contains( key.idx ) )
						tl->selKeys.append( key.idx );
				}
			}
		}

		if ( !tl->selKeys.isEmpty() )
			tl->primaryKey = tl->selKeys.first();

		tl->inspector->rebuild();
		tl->graphView->update();
	}

	dragMode = DragNone;
	dragLane = -1;
	update();
}

void TimelineLanesView::mouseDoubleClickEvent( QMouseEvent * ev )
{
	if ( ev->button() != Qt::LeftButton )
		return;

	int row = rowAtY( ev->pos().y() );
	int laneIdx = tl->laneRow( row );
	if ( laneIdx >= 0 && ev->pos().x() >= tl->labelW ) {
		float t = tl->snapTime( tl->xToTime( ev->pos().x(), width() ) );
		tl->insertKeyAtTime( laneIdx, t );
	}
}

void TimelineLanesView::wheelEvent( QWheelEvent * ev )
{
	int delta = ev->angleDelta().y();

	if ( ev->modifiers() & Qt::ControlModifier ) {
		float t = tl->xToTime( ev->position().x(), width() );
		tl->zoomAt( t, delta > 0 ? 0.8f : 1.25f );
	} else {
		tl->laneScroll->setValue( tl->laneScroll->value() - delta / 120 * TL_LANE_H * 3 );
	}

	ev->accept();
}

void TimelineLanesView::contextMenuEvent( QContextMenuEvent * ev )
{
	// Ruler: marker + preview range management
	if ( ev->pos().y() < TL_RULER_H && ev->pos().x() >= tl->labelW ) {
		float t = tl->snapTime( tl->xToTime( ev->pos().x(), width() ) );
		QMenu menu;
		menu.addAction( tr( "Add text key here..." ), [this, t]() { tl->addTextKeyMarker( t ); } );
		if ( tl->prevRangeOn )
			menu.addAction( tr( "Clear preview range" ), [this]() {
				tl->prevRangeOn = false;
				update();
			} );
		menu.addAction( tr( "Frame all (Home)" ), [this]() { tl->frameAll(); } );
		menu.exec( ev->globalPos() );
		return;
	}

	int laneIdx = tl->laneRow( rowAtY( ev->pos().y() ) );
	if ( laneIdx >= 0 )
		tl->showLaneContextMenu( laneIdx, ev->globalPos() );
}

void TimelineLanesView::leaveEvent( QEvent * )
{
	hoverRow = -1;
	marqueeTimer->stop();
	marqueeOffset = 0;
	update();
}


/*
 *  TimelineGraphView
 */

TimelineGraphView::TimelineGraphView( TimelineWidget * parent ) : QWidget( parent ), tl( parent )
{
	setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
	setFocusPolicy( Qt::ClickFocus );
}

void TimelineGraphView::sampleCurves()
{
	curves.clear();
	curveSrc.clear();
	curveRange.clear();
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

	int numSamples = std::max( ( width() - tl->labelW ) / 2, 16 );
	float t0 = tl->viewT0;
	float dt = ( tl->viewT1 - tl->viewT0 ) / numSamples;

	for ( int c = 0; c < lane.channels.count(); c++ ) {
		const TimelineChannel & ch = lane.channels[c];

		if ( !ch.plottable() )
			continue;

		if ( ch.type == TimelineChannel::QuatVal ) {
			for ( int comp = 0; comp < 4; comp++ ) {
				QVector<QPointF> curve;
				float cmin = 0, cmax = 0;
				bool cfirst = true;
				for ( const auto & key : ch.keys ) {
					Quat q = tl->nif->get<Quat>( QModelIndex( key.idx ), "Value" );
					expand( q[comp] );
					if ( cfirst ) {
						cmin = cmax = q[comp];
						cfirst = false;
					} else {
						cmin = std::min( cmin, q[comp] );
						cmax = std::max( cmax, q[comp] );
					}
					curve.append( QPointF( key.time, q[comp] ) );
				}
				curves.append( curve );
				curveSrc.append( { c, comp } );
				curveRange.append( { cmin, cmax } );
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
			float cmin = 0, cmax = 0;
			bool cfirst = true;

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
					if ( cfirst ) {
						cmin = cmax = v;
						cfirst = false;
					} else {
						cmin = std::min( cmin, v );
						cmax = std::max( cmax, v );
					}
					curve.append( QPointF( t, v ) );
				}
			}

			curves.append( curve );
			curveSrc.append( { c, comp } );
			curveRange.append( { cmin, cmax } );
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

float TimelineGraphView::valueToY( float v, int curve ) const
{
	float lo = vMin, hi = vMax;

	if ( tl->normalized && curve >= 0 && curve < curveRange.count() ) {
		lo = curveRange[curve].first;
		hi = curveRange[curve].second;
		if ( hi - lo < 1.0e-6f ) {
			lo -= 1.0f;
			hi += 1.0f;
		} else {
			float pad = ( hi - lo ) * 0.08f;
			lo -= pad;
			hi += pad;
		}
	}

	float span = std::max( hi - lo, 1.0e-6f );
	return height() - 6 - ( v - lo ) / span * std::max( height() - 12, 1 );
}

float TimelineGraphView::yToValue( float y ) const
{
	float span = std::max( vMax - vMin, 1.0e-6f );
	return vMin + ( height() - 6 - y ) / std::max( height() - 12, 1 ) * span;
}

bool TimelineGraphView::hitTestKey( const QPoint & pos, QModelIndex & keyIdx, int & channel, int & comp ) const
{
	if ( tl->currentLane < 0 || tl->currentLane >= tl->lanes.count() )
		return false;

	const TimelineLane & lane = tl->lanes[tl->currentLane];
	float bestDist = TL_KEY_R + 5.0f;
	bool found = false;

	for ( int i = 0; i < curves.count(); i++ ) {
		int c = curveSrc[i].first;
		int cp = curveSrc[i].second;
		const TimelineChannel & ch = lane.channels[c];

		for ( const auto & key : ch.keys ) {
			float x = tl->timeToX( key.time, width() );
			float kv = tl->keyComponentValue( ch, QModelIndex( key.idx ), cp );
			float y = valueToY( kv, i );
			float d = std::hypot( x - pos.x(), y - pos.y() );
			if ( d < bestDist ) {
				bestDist = d;
				keyIdx = QModelIndex( key.idx );
				channel = c;
				comp = cp;
				found = true;
			}
		}
	}

	return found;
}

bool TimelineGraphView::hitTestTangent( const QPoint & pos, int & channel, int & keyIndex, int & comp, bool & backward ) const
{
	if ( !tl->primaryKey.isValid() || tl->currentLane < 0 )
		return false;

	int lane, c, k;
	if ( !tl->findKeyRef( QModelIndex( tl->primaryKey ), lane, c, k ) || lane != tl->currentLane )
		return false;

	const TimelineChannel & ch = tl->lanes[lane].channels[c];
	if ( ch.interpolation != 2 || !ch.iKeyGroup.isValid() )
		return false;

	float handleDt = ( tl->viewT1 - tl->viewT0 ) * 0.045f;
	float t = ch.keys[k].time;

	for ( int i = 0; i < curves.count(); i++ ) {
		if ( curveSrc[i].first != c )
			continue;
		int cp = curveSrc[i].second;

		float kv = tl->keyComponentValue( ch, QModelIndex( tl->primaryKey ), cp );

		for ( int b = 0; b < 2; b++ ) {
			bool isBwd = ( b == 1 );
			// Backward tangent = incoming (points left), Forward = outgoing (points right)
			float tangent = tl->keyTangentValue( ch, QModelIndex( tl->primaryKey ), cp, isBwd );
			float segDur = 1.0f;
			if ( isBwd && k + 1 < ch.keys.count() )
				segDur = std::max( ch.keys[k + 1].time - t, 1.0e-4f );
			else if ( !isBwd && k > 0 )
				segDur = std::max( t - ch.keys[k - 1].time, 1.0e-4f );

			float slope = tangent / segDur;
			float hx = isBwd ? t + handleDt : t - handleDt;
			float hv = isBwd ? kv + slope * handleDt : kv - slope * handleDt;

			float x = tl->timeToX( hx, width() );
			float y = valueToY( hv, i );
			if ( std::hypot( x - pos.x(), y - pos.y() ) < 6.0f ) {
				channel = c;
				keyIndex = k;
				comp = cp;
				backward = isBwd;
				return true;
			}
		}
	}

	return false;
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

	// Value grid (hidden in normalized mode, where each curve has its own scale)
	if ( !tl->normalized ) {
		float vstep = tlNiceStep( ( vMax - vMin ) / std::max( h / 40, 2 ) );
		float v = std::floor( vMin / vstep ) * vstep;
		for ( ; v <= vMax; v += vstep ) {
			float y = valueToY( v );
			p.setPen( gridCol );
			p.drawLine( QPointF( tl->labelW, y ), QPointF( w, y ) );
			p.setPen( pal.color( QPalette::Text ) );
			QString label = QString::number( v, 'g', 4 );
			p.drawText( QRectF( 4, y - fm.height() * 0.5, tl->labelW - 10, fm.height() ),
				Qt::AlignRight | Qt::AlignVCenter, label );
		}
	}

	float tstep = tlNiceStep( ( tl->viewT1 - tl->viewT0 ) / std::max( ( w - tl->labelW ) / 70, 2 ) );
	float t = std::floor( tl->viewT0 / tstep ) * tstep;
	p.setPen( gridCol );
	for ( ; t <= tl->viewT1 + tstep; t += tstep ) {
		float x = tl->timeToX( t, w );
		if ( x >= tl->labelW )
			p.drawLine( QPointF( x, 0 ), QPointF( x, h ) );
	}

	p.setPen( gridCol );
	p.drawLine( tl->labelW, 0, tl->labelW, h );

	p.setClipRect( QRect( tl->labelW, 0, w - tl->labelW, h ) );

	// Curves
	for ( int i = 0; i < curves.count(); i++ ) {
		const auto & curve = curves[i];
		if ( curve.count() < 2 )
			continue;

		int comp = curveSrc[i].second;
		const TimelineChannel & ch = lane.channels[curveSrc[i].first];

		QColor col = ( ch.numComponents > 1 ) ? tlCompColor( comp ) : pal.color( QPalette::Highlight );

		QPainterPath path;
		path.moveTo( tl->timeToX( curve[0].x(), w ), valueToY( curve[0].y(), i ) );

		bool stepped = ( ch.type == TimelineChannel::BoolVal );
		for ( int s = 1; s < curve.count(); s++ ) {
			float x = tl->timeToX( curve[s].x(), w );
			float y = valueToY( curve[s].y(), i );
			if ( stepped )
				path.lineTo( x, valueToY( curve[s - 1].y(), i ) );
			path.lineTo( x, y );
		}

		p.setPen( QPen( col, 1.4 ) );
		p.setBrush( Qt::NoBrush );
		p.drawPath( path );
	}

	// Key markers
	for ( int i = 0; i < curves.count(); i++ ) {
		int c = curveSrc[i].first;
		int comp = curveSrc[i].second;
		const TimelineChannel & ch = lane.channels[c];

		QColor col = ( ch.numComponents > 1 ) ? tlCompColor( comp ) : pal.color( QPalette::Highlight );

		for ( const auto & key : ch.keys ) {
			float x = tl->timeToX( key.time, w );
			if ( x < tl->labelW - TL_KEY_R || x > w + TL_KEY_R )
				continue;

			float kv = tl->keyComponentValue( ch, QModelIndex( key.idx ), comp );
			bool sel = tl->selKeys.contains( key.idx );

			p.setPen( Qt::NoPen );
			p.setBrush( sel ? pal.color( QPalette::BrightText ) : col );
			tlDrawKeyMarker( p, QPointF( x, valueToY( kv, i ) ), sel ? TL_KEY_R + 1.0f : TL_KEY_R - 1.0f, ch.interpolation );
		}
	}

	// Tangent handles for the primary selected key (quadratic channels)
	{
		int lIdx, c, k;
		if ( tl->primaryKey.isValid() && tl->findKeyRef( QModelIndex( tl->primaryKey ), lIdx, c, k )
		     && lIdx == tl->currentLane && lane.channels[c].interpolation == 2 ) {
			const TimelineChannel & ch = lane.channels[c];
			float handleDt = ( tl->viewT1 - tl->viewT0 ) * 0.045f;
			float kt = ch.keys[k].time;

			for ( int i = 0; i < curves.count(); i++ ) {
				if ( curveSrc[i].first != c )
					continue;
				int comp = curveSrc[i].second;
				float kv = tl->keyComponentValue( ch, QModelIndex( tl->primaryKey ), comp );
				float kx = tl->timeToX( kt, w );
				float ky = valueToY( kv, i );

				for ( int b = 0; b < 2; b++ ) {
					bool isBwd = ( b == 1 );
					float tangent = tl->keyTangentValue( ch, QModelIndex( tl->primaryKey ), comp, isBwd );
					float segDur = 1.0f;
					if ( isBwd && k + 1 < ch.keys.count() )
						segDur = std::max( ch.keys[k + 1].time - kt, 1.0e-4f );
					else if ( !isBwd && k > 0 )
						segDur = std::max( kt - ch.keys[k - 1].time, 1.0e-4f );

					float slope = tangent / segDur;
					float hx = isBwd ? kt + handleDt : kt - handleDt;
					float hv = isBwd ? kv + slope * handleDt : kv - slope * handleDt;

					float x = tl->timeToX( hx, w );
					float y = valueToY( hv, i );

					QColor hc = pal.color( QPalette::Text );
					hc.setAlpha( 160 );
					p.setPen( QPen( hc, 1 ) );
					p.drawLine( QPointF( kx, ky ), QPointF( x, y ) );
					p.setBrush( pal.color( QPalette::Base ) );
					p.setPen( QPen( pal.color( QPalette::Text ), 1 ) );
					p.drawEllipse( QPointF( x, y ), 3.5, 3.5 );
				}
			}
		}
	}

	// Playhead
	float px = tl->timeToX( tl->curTime, w );
	if ( px >= tl->labelW ) {
		p.setPen( QPen( QColor( 0xe0, 0x40, 0x40 ), 1.5 ) );
		p.drawLine( QPointF( px, 0 ), QPointF( px, h ) );
	}

	// Rubber band
	if ( dragMode == DragRubber && dragMoved ) {
		QColor rb = pal.color( QPalette::Highlight );
		p.setPen( QPen( rb, 1, Qt::DashLine ) );
		rb.setAlpha( 40 );
		p.setBrush( rb );
		p.drawRect( rubberRect );
	}

	p.setClipping( false );

	// Legend
	int ly = 4;
	for ( int c = 0; c < lane.channels.count(); c++ ) {
		const TimelineChannel & ch = lane.channels[c];
		if ( !ch.plottable() )
			continue;

		QString text = QString( "%1  [%2]" ).arg( ch.name, tlKeyTypeName( ch.interpolation ) );

		int lx = 8;
		for ( int comp = 0; comp < ch.numComponents; comp++ ) {
			QColor col = ( ch.numComponents > 1 ) ? tlCompColor( comp ) : pal.color( QPalette::Highlight );
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
	setFocus( Qt::MouseFocusReason );

	if ( ev->button() != Qt::LeftButton )
		return;

	QPoint pos = ev->pos();
	dragStart = pos;
	dragMoved = false;
	dragOrigTimes.clear();
	dragOrigVals.clear();

	if ( !tl->nif || tl->currentLane < 0 || tl->currentLane >= tl->lanes.count() )
		return;

	const TimelineLane & lane = tl->lanes[tl->currentLane];

	// Tangent handle?
	int c, k, comp;
	bool backward;
	if ( hitTestTangent( pos, c, k, comp, backward ) && !lane.locked ) {
		dragMode = DragTangent;
		dragChannel = c;
		dragKeyIndex = k;
		dragComp = comp;
		dragTangentBackward = backward;
		return;
	}

	// Key?
	QModelIndex keyIdx;
	if ( hitTestKey( pos, keyIdx, c, comp ) ) {
		bool additive = ev->modifiers() & Qt::ShiftModifier;
		if ( !additive && !tl->selKeys.contains( QPersistentModelIndex( keyIdx ) ) )
			tl->selectKey( keyIdx, false );
		else if ( additive )
			tl->selectKey( keyIdx, true );
		else
			tl->primaryKey = QPersistentModelIndex( keyIdx );

		if ( !lane.locked ) {
			dragMode = DragKeys;
			dragChannel = c;
			dragComp = comp;
			dragStartTime = tl->xToTime( pos.x(), width() );
			dragStartValue = yToValue( pos.y() );

			for ( const auto & kp : tl->selKeys ) {
				int l2, c2, k2;
				if ( tl->findKeyRef( QModelIndex( kp ), l2, c2, k2 ) && !tl->lanes[l2].locked ) {
					dragOrigTimes.append( { kp, tl->lanes[l2].channels[c2].keys[k2].time } );
					dragOrigVals.append( tl->keyComponentValue( tl->lanes[l2].channels[c2], QModelIndex( kp ), comp ) );
				}
			}
		}
		return;
	}

	// Empty: rubber band select (shift) or scrub
	if ( ev->modifiers() & Qt::ShiftModifier ) {
		dragMode = DragRubber;
		rubberRect = QRect( pos, pos );
	} else if ( pos.x() >= tl->labelW ) {
		dragMode = DragScrub;
		float t = tl->snapTime( tl->xToTime( pos.x(), width() ) );
		tl->curTime = t;
		emit tl->timeChanged( t );
		tl->updateViews();
	}
}

void TimelineGraphView::mouseMoveEvent( QMouseEvent * ev )
{
	QPoint pos = ev->pos();

	if ( !( ev->buttons() & Qt::LeftButton ) )
		return;

	if ( ( pos - dragStart ).manhattanLength() > 2 )
		dragMoved = true;

	switch ( dragMode ) {
	case DragScrub:
		{
			float t = tl->snapTime( tl->xToTime( std::max( pos.x(), tl->labelW ), width() ) );
			tl->curTime = t;
			emit tl->timeChanged( t );
			tl->updateViews();
		}
		break;

	case DragKeys:
		if ( !dragOrigTimes.isEmpty() && tl->nif ) {
			float dt = tl->xToTime( pos.x(), width() ) - dragStartTime;
			float dv = yToValue( pos.y() ) - dragStartValue;
			bool first = true;

			for ( int i = 0; i < dragOrigTimes.count(); i++ ) {
				int l, c, k;
				if ( !tl->findKeyRef( QModelIndex( dragOrigTimes[i].first ), l, c, k ) )
					continue;

				const TimelineChannel & ch = tl->lanes[l].channels[c];

				float nt = tl->snapTime( dragOrigTimes[i].second + dt );
				nt = tl->clampKeyTime( ch, k, nt );
				QModelIndex iTime = tl->nif->getIndex( QModelIndex( dragOrigTimes[i].first ), "Time" );
				if ( iTime.isValid() ) {
					tl->pushFieldChange( iTime, nt, first && !dragMoved );
					first = false;
				}

				if ( dragComp < ch.numComponents && ch.type != TimelineChannel::BoolVal ) {
					float nv = tl->snapValue( dragOrigVals.value( i ) + dv );
					tl->setKeyComponentValue( ch, QModelIndex( dragOrigTimes[i].first ), dragComp, nv );
				}
			}

			invalidateCurves();
			tl->lanesView->invalidateStrips();
		}
		break;

	case DragTangent:
		if ( tl->nif && tl->currentLane >= 0 && dragChannel >= 0 ) {
			const TimelineChannel & ch = tl->lanes[tl->currentLane].channels[dragChannel];
			if ( dragKeyIndex < ch.keys.count() ) {
				float kt = ch.keys[dragKeyIndex].time;
				float kv = tl->keyComponentValue( ch, QModelIndex( ch.keys[dragKeyIndex].idx ), dragComp );

				float mt = tl->xToTime( pos.x(), width() );
				float mv = yToValue( pos.y() );

				float dtm = dragTangentBackward ? ( mt - kt ) : ( kt - mt );
				float dvm = dragTangentBackward ? ( mv - kv ) : ( kv - mv );
				if ( std::abs( dtm ) < 1.0e-4f )
					dtm = ( dtm < 0 ) ? -1.0e-4f : 1.0e-4f;

				float slope = dvm / dtm;

				float segDur = 1.0f;
				if ( dragTangentBackward && dragKeyIndex + 1 < ch.keys.count() )
					segDur = std::max( ch.keys[dragKeyIndex + 1].time - kt, 1.0e-4f );
				else if ( !dragTangentBackward && dragKeyIndex > 0 )
					segDur = std::max( kt - ch.keys[dragKeyIndex - 1].time, 1.0e-4f );

				tl->setKeyTangent( ch, QModelIndex( ch.keys[dragKeyIndex].idx ), dragComp, dragTangentBackward, slope * segDur );
				invalidateCurves();
			}
		}
		break;

	case DragRubber:
		rubberRect = QRect( dragStart, pos ).normalized();
		update();
		break;

	default:
		break;
	}
}

void TimelineGraphView::mouseReleaseEvent( QMouseEvent * ev )
{
	Q_UNUSED( ev );

	if ( dragMode == DragRubber && dragMoved && tl->currentLane >= 0 ) {
		bool additive = QApplication::keyboardModifiers() & Qt::ShiftModifier;
		if ( !additive )
			tl->selKeys.clear();

		const TimelineLane & lane = tl->lanes[tl->currentLane];

		for ( int i = 0; i < curves.count(); i++ ) {
			int c = curveSrc[i].first;
			int comp = curveSrc[i].second;
			const TimelineChannel & ch = lane.channels[c];

			for ( const auto & key : ch.keys ) {
				float x = tl->timeToX( key.time, width() );
				float y = valueToY( tl->keyComponentValue( ch, QModelIndex( key.idx ), comp ), i );
				if ( rubberRect.contains( QPoint( (int)x, (int)y ) ) ) {
					if ( !tl->selKeys.contains( key.idx ) )
						tl->selKeys.append( key.idx );
				}
			}
		}

		if ( !tl->selKeys.isEmpty() )
			tl->primaryKey = tl->selKeys.first();

		tl->inspector->rebuild();
		tl->lanesView->update();
	}

	dragMode = DragNone;
	update();
}

void TimelineGraphView::mouseDoubleClickEvent( QMouseEvent * ev )
{
	if ( ev->button() != Qt::LeftButton || tl->currentLane < 0 )
		return;

	if ( ev->pos().x() >= tl->labelW ) {
		float t = tl->snapTime( tl->xToTime( ev->pos().x(), width() ) );
		tl->insertKeyAtTime( tl->currentLane, t );
	}
}

void TimelineGraphView::wheelEvent( QWheelEvent * ev )
{
	float t = tl->xToTime( ev->position().x(), width() );
	tl->zoomAt( t, ev->angleDelta().y() > 0 ? 0.8f : 1.25f );
	ev->accept();
}


/*
 *  TimelineInspector
 */

TimelineInspector::TimelineInspector( TimelineWidget * parent ) : QWidget( parent ), tl( parent )
{
	setFixedWidth( TL_INSP_W );

	auto outer = new QVBoxLayout( this );
	outer->setContentsMargins( 0, 0, 0, 0 );

	auto scroll = new QScrollArea( this );
	scroll->setWidgetResizable( true );
	scroll->setFrameShape( QFrame::NoFrame );
	scroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	outer->addWidget( scroll );

	content = new QWidget;
	mainLay = new QVBoxLayout( content );
	mainLay->setContentsMargins( 6, 4, 6, 4 );
	mainLay->setSpacing( 4 );
	scroll->setWidget( content );
}

void TimelineInspector::addSection( QVBoxLayout * lay, const QString & title )
{
	auto label = new QLabel( title, content );
	QFont f = label->font();
	f.setBold( true );
	label->setFont( f );
	label->setContentsMargins( 0, 6, 0, 0 );
	lay->addWidget( label );
}

QLineEdit * TimelineInspector::addField( QFormLayout * form, const QString & label, const QString & value,
                                         const QModelIndex & fieldIdx, int comp )
{
	auto edit = new QLineEdit( value, content );

	if ( fieldIdx.isValid() ) {
		QPersistentModelIndex pIdx( fieldIdx );
		connect( edit, &QLineEdit::editingFinished, [this, edit, pIdx, comp]() {
			if ( rebuilding || !tl->nif || !pIdx.isValid() )
				return;
			bool ok = false;
			float v = edit->text().toFloat( &ok );
			if ( !ok )
				return;

			if ( comp < 0 ) {
				tl->pushFieldChange( QModelIndex( pIdx ), v, true );
			} else {
				// component of a compound Value: resolve channel through the key row
				int lane, ch, key;
				QModelIndex keyRow = QModelIndex( pIdx ).parent();
				if ( tl->findKeyRef( QModelIndex( pIdx ), lane, ch, key ) )
					keyRow = QModelIndex( pIdx );
				if ( tl->findKeyRef( keyRow, lane, ch, key ) )
					tl->setKeyComponentValue( tl->lanes[lane].channels[ch], keyRow, comp, v );
			}
			tl->lanesView->invalidateStrips();
			tl->graphView->invalidateCurves();
		} );
	} else {
		edit->setReadOnly( true );
	}

	form->addRow( label, edit );
	return edit;
}

void TimelineInspector::rebuild()
{
	if ( !isVisible() && !tl )
		return;

	rebuilding = true;

	// wipe
	QLayoutItem * item;
	while ( ( item = mainLay->takeAt( 0 ) ) ) {
		if ( item->widget() )
			item->widget()->deleteLater();
		delete item;
	}

	NifModel * nif = tl->nif;

	if ( !nif || tl->currentLane < 0 || tl->currentLane >= tl->lanes.count() ) {
		auto hint = new QLabel( tr( "Select a lane or key" ), content );
		hint->setWordWrap( true );
		mainLay->addWidget( hint );
		mainLay->addStretch();
		rebuilding = false;
		return;
	}

	const TimelineLane & lane = tl->lanes[tl->currentLane];

	// ---- Key section
	int lIdx, cIdx, kIdx;
	if ( tl->primaryKey.isValid() && tl->findKeyRef( QModelIndex( tl->primaryKey ), lIdx, cIdx, kIdx ) ) {
		const TimelineChannel & ch = tl->lanes[lIdx].channels[cIdx];
		QModelIndex keyRow( tl->primaryKey );

		addSection( mainLay, tr( "Key  (%1/%2)" ).arg( kIdx + 1 ).arg( ch.keys.count() ) );
		auto form = new QFormLayout;
		form->setContentsMargins( 0, 0, 0, 0 );
		form->setSpacing( 2 );
		mainLay->addLayout( form );

		QModelIndex iTime = nif->getIndex( keyRow, "Time" );
		addField( form, tr( "Time" ), QString::number( nif->get<float>( iTime ), 'f', 4 ), iTime );

		if ( ch.type == TimelineChannel::TextVal ) {
			auto textEdit = new QLineEdit( nif->resolveString( keyRow, "Value" ), content );
			QPersistentModelIndex pKey( keyRow );
			connect( textEdit, &QLineEdit::editingFinished, [this, textEdit, pKey, nif]() {
				if ( !rebuilding && pKey.isValid() )
					nif->assignString( QModelIndex( pKey ), QStringLiteral( "Value" ), textEdit->text(), false );
			} );
			form->addRow( tr( "Text" ), textEdit );
		} else {
			static const char * compNames4[4] = { "X", "Y", "Z", "W" };
			static const char * colNames4[4] = { "R", "G", "B", "A" };
			int nc = std::max( ch.numComponents, 1 );
			for ( int comp = 0; comp < nc; comp++ ) {
				QString name = ( nc == 1 ) ? tr( "Value" )
					: ch.isColor() ? QString::fromLatin1( colNames4[comp] )
					: ( ch.type == TimelineChannel::QuatVal ) ? QString::fromLatin1( ( comp == 0 ) ? "W" : compNames4[comp - 1] )
					: QString::fromLatin1( compNames4[comp] );
				float v = tl->keyComponentValue( ch, keyRow, comp );
				// pass the key row itself; comp >= 0 routes through setKeyComponentValue
				addField( form, name, QString::number( v, 'f', 4 ), keyRow, comp );
			}

			if ( ch.interpolation == 2 ) {
				for ( int comp = 0; comp < nc; comp++ ) {
					float fv = tl->keyTangentValue( ch, keyRow, comp, false );
					float bv = tl->keyTangentValue( ch, keyRow, comp, true );
					QString suffix = nc > 1 ? QString( " %1" ).arg( compNames4[std::min( comp, 3 )] ) : QString();

					auto fwdEdit = new QLineEdit( QString::number( fv, 'f', 4 ), content );
					auto bwdEdit = new QLineEdit( QString::number( bv, 'f', 4 ), content );
					QPersistentModelIndex pKey( keyRow );
					int c2 = cIdx, l2 = lIdx, compCopy = comp;

					connect( fwdEdit, &QLineEdit::editingFinished, [this, fwdEdit, pKey, l2, c2, compCopy]() {
						bool ok = false;
						float v = fwdEdit->text().toFloat( &ok );
						if ( ok && !rebuilding && pKey.isValid() && l2 < tl->lanes.count() )
							tl->setKeyTangent( tl->lanes[l2].channels[c2], QModelIndex( pKey ), compCopy, false, v );
					} );
					connect( bwdEdit, &QLineEdit::editingFinished, [this, bwdEdit, pKey, l2, c2, compCopy]() {
						bool ok = false;
						float v = bwdEdit->text().toFloat( &ok );
						if ( ok && !rebuilding && pKey.isValid() && l2 < tl->lanes.count() )
							tl->setKeyTangent( tl->lanes[l2].channels[c2], QModelIndex( pKey ), compCopy, true, v );
					} );

					form->addRow( tr( "Fwd" ) + suffix, fwdEdit );
					form->addRow( tr( "Bwd" ) + suffix, bwdEdit );
				}
			}

			if ( ch.interpolation == 3 ) {
				QModelIndex iTBC = nif->getIndex( keyRow, "TBC" );
				if ( iTBC.isValid() ) {
					addField( form, tr( "Tension" ), QString::number( nif->get<float>( iTBC, "t" ), 'f', 3 ), nif->getIndex( iTBC, "t" ) );
					addField( form, tr( "Bias" ), QString::number( nif->get<float>( iTBC, "b" ), 'f', 3 ), nif->getIndex( iTBC, "b" ) );
					addField( form, tr( "Continuity" ), QString::number( nif->get<float>( iTBC, "c" ), 'f', 3 ), nif->getIndex( iTBC, "c" ) );
				}
			}
		}

		if ( tl->selKeys.count() > 1 ) {
			auto multi = new QLabel( tr( "%1 keys selected" ).arg( tl->selKeys.count() ), content );
			mainLay->addWidget( multi );
		}
	}

	// ---- Channel section
	if ( !lane.channels.isEmpty() ) {
		addSection( mainLay, tr( "Channel" ) );

		for ( int c = 0; c < lane.channels.count(); c++ ) {
			const TimelineChannel & ch = lane.channels[c];
			if ( !ch.plottable() || !ch.iKeyGroup.isValid() )
				continue;

			auto row = new QHBoxLayout;
			auto nameLabel = new QLabel( ch.name, content );
			auto combo = new QComboBox( content );
			const int types[4] = { 1, 2, 3, 5 };
			for ( int t : types )
				combo->addItem( tlKeyTypeName( t ), t );
			combo->setCurrentIndex( ch.interpolation == 2 ? 1 : ch.interpolation == 3 ? 2 : ch.interpolation == 5 ? 3 : 0 );

			int laneCopy = tl->currentLane, cCopy = c;
			connect( combo, qOverload<int>( &QComboBox::activated ), [this, combo, laneCopy, cCopy]( int ) {
				if ( !rebuilding )
					tl->setChannelInterpolation( laneCopy, cCopy, combo->currentData().toInt(), false );
			} );

			row->addWidget( nameLabel );
			row->addWidget( combo, 1 );
			mainLay->addLayout( row );
		}

		auto csvRow = new QHBoxLayout;
		auto btnExp = new QPushButton( tr( "CSV out" ), content );
		auto btnImp = new QPushButton( tr( "CSV in" ), content );
		int laneCopy = tl->currentLane;
		connect( btnExp, &QPushButton::clicked, [this, laneCopy]() { tl->csvExport( laneCopy ); } );
		connect( btnImp, &QPushButton::clicked, [this, laneCopy]() { tl->csvImport( laneCopy ); } );
		csvRow->addWidget( btnExp );
		csvRow->addWidget( btnImp );
		mainLay->addLayout( csvRow );
	}

	// ---- Controller section
	if ( lane.iController.isValid() ) {
		QModelIndex iCtlr( lane.iController );
		addSection( mainLay, tr( "Controller" ) );

		auto form = new QFormLayout;
		form->setContentsMargins( 0, 0, 0, 0 );
		form->setSpacing( 2 );
		mainLay->addLayout( form );

		for ( const char * fieldName : { "Start Time", "Stop Time", "Frequency", "Phase" } ) {
			QModelIndex iField = nif->getIndex( iCtlr, fieldName );
			if ( iField.isValid() )
				addField( form, tr( fieldName ), QString::number( nif->get<float>( iField ), 'f', 4 ), iField );
		}

		QModelIndex iFlags = nif->getIndex( iCtlr, "Flags" );
		if ( iFlags.isValid() ) {
			quint16 flags = (quint16)nif->get<int>( iFlags );

			auto cycle = new QComboBox( content );
			cycle->addItem( tr( "Loop" ), 0 );
			cycle->addItem( tr( "Reverse" ), 1 );
			cycle->addItem( tr( "Clamp" ), 2 );
			cycle->setCurrentIndex( ( flags >> 1 ) & 3 );

			auto active = new QCheckBox( tr( "Active" ), content );
			active->setChecked( flags & 0x0008 );

			QPersistentModelIndex pFlags( iFlags );
			auto applyFlags = [this, cycle, active, pFlags]() {
				if ( rebuilding || !pFlags.isValid() || !tl->nif )
					return;
				quint16 f = (quint16)tl->nif->get<int>( QModelIndex( pFlags ) );
				f = ( f & ~0x0006 ) | ( ( cycle->currentData().toInt() & 3 ) << 1 );
				f = active->isChecked() ? ( f | 0x0008 ) : ( f & ~0x0008 );
				tl->pushFieldChange( QModelIndex( pFlags ), (int)f, true );
				tl->refreshLater();
			};
			connect( cycle, qOverload<int>( &QComboBox::activated ), applyFlags );
			connect( active, &QCheckBox::toggled, applyFlags );

			form->addRow( tr( "Cycle" ), cycle );
			form->addRow( QString(), active );
		}
	}

	// ---- Sequences containing this lane's target
	if ( lane.targetBlock >= 0 && !tl->sequences.isEmpty() ) {
		QStringList seqNames;
		QString targetName = nif->resolveString( nif->getBlockIndex( lane.targetBlock ), "Name" );

		for ( const auto & s : tl->sequences ) {
			QModelIndex iSeq( s );
			QModelIndex iCtrl = nif->getIndex( iSeq, "Controlled Blocks" );
			for ( int r = 0; r < nif->rowCount( iCtrl ); r++ ) {
				if ( nif->resolveString( nif->getIndex( iCtrl, r ), "Node Name" ) == targetName ) {
					seqNames << nif->resolveString( iSeq, "Name" );
					break;
				}
			}
		}

		if ( !seqNames.isEmpty() ) {
			addSection( mainLay, tr( "In sequences" ) );
			for ( const QString & sn : seqNames ) {
				auto btn = new QPushButton( sn, content );
				btn->setFlat( true );
				btn->setStyleSheet( QStringLiteral( "text-align:left; text-decoration:underline;" ) );
				connect( btn, &QPushButton::clicked, [this, sn]() { tl->setSequenceByName( sn ); } );
				mainLay->addWidget( btn );
			}
		}
	}

	mainLay->addStretch();
	rebuilding = false;
}
