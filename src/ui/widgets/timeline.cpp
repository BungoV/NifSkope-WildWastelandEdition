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
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
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

//! Crisp Blender-style toolbar icons; unicode glyphs render badly in the Windows UI font
QIcon tlMakeIcon( const QString & id, const QColor & col )
{
	const int S = 64;
	QPixmap pm( S, S );
	pm.fill( Qt::transparent );
	QPainter p( &pm );
	p.setRenderHint( QPainter::Antialiasing );
	QPen pen( col, 5.0 );
	pen.setCapStyle( Qt::RoundCap );
	pen.setJoinStyle( Qt::RoundJoin );
	p.setPen( pen );
	p.setBrush( col );

	auto tri = [&p]( bool right, float xFlat, float xTip, float yMid = 32, float h = 19 ) {
		QPolygonF t;
		t << QPointF( xFlat, yMid - h ) << QPointF( xTip, yMid ) << QPointF( xFlat, yMid + h );
		Q_UNUSED( right );
		p.drawPolygon( t );
	};
	auto diamond = [&p]( QPointF c, float r ) {
		QPolygonF d;
		d << c + QPointF( 0, -r ) << c + QPointF( r, 0 ) << c + QPointF( 0, r ) << c + QPointF( -r, 0 );
		p.drawPolygon( d );
	};

	if ( id == QLatin1String( "play" ) ) {
		tri( true, 20, 50 );
	} else if ( id == QLatin1String( "playback" ) ) {
		tri( false, 44, 14 );
	} else if ( id == QLatin1String( "loop" ) ) {
		// arrow-headed ring: a circle with a wedge at the top-right
		QPen ring( col, 6.0 );
		ring.setCapStyle( Qt::FlatCap );
		p.setPen( ring );
		p.setBrush( Qt::NoBrush );
		p.drawArc( QRectF( 15, 15, 34, 34 ), 60 * 16, 300 * 16 );
		p.setPen( Qt::NoPen );
		p.setBrush( col );
		QPolygonF head;
		head << QPointF( 40, 8 ) << QPointF( 52, 18 ) << QPointF( 38, 24 );
		p.drawPolygon( head );
	} else if ( id == QLatin1String( "sequence" ) ) {
		// three stacked bars: a list of takes
		p.setPen( Qt::NoPen );
		p.drawRoundedRect( QRectF( 14, 17, 36, 7 ), 3, 3 );
		p.drawRoundedRect( QRectF( 14, 29, 36, 7 ), 3, 3 );
		p.drawRoundedRect( QRectF( 14, 41, 24, 7 ), 3, 3 );
	} else if ( id == QLatin1String( "settings" ) ) {
		// horizontal ellipsis - "more", deliberately not a gear, which reads as
		// application preferences rather than as options for this control
		p.setPen( Qt::NoPen );
		for ( int i = 0; i < 3; i++ )
			p.drawEllipse( QPointF( 16.0 + i * 16.0, 32.0 ), 5.0, 5.0 );
	} else if ( id == QLatin1String( "pause" ) ) {
		p.drawRoundedRect( QRectF( 18, 14, 9, 36 ), 2, 2 );
		p.drawRoundedRect( QRectF( 37, 14, 9, 36 ), 2, 2 );
	} else if ( id == QLatin1String( "tostart" ) ) {
		p.drawRoundedRect( QRectF( 14, 13, 7, 38 ), 2, 2 );
		tri( false, 52, 26 );
	} else if ( id == QLatin1String( "toend" ) ) {
		tri( true, 12, 38 );
		p.drawRoundedRect( QRectF( 43, 13, 7, 38 ), 2, 2 );
	} else if ( id == QLatin1String( "prevkey" ) ) {
		diamond( QPointF( 18, 32 ), 10 );
		tri( false, 52, 34, 32, 14 );
	} else if ( id == QLatin1String( "nextkey" ) ) {
		tri( true, 12, 30, 32, 14 );
		diamond( QPointF( 46, 32 ), 10 );
	} else if ( id == QLatin1String( "magnet" ) ) {
		// Blender-style horseshoe magnet: tilted, darker body, bright pole tips
		p.save();
		p.translate( 32, 34 );
		p.rotate( -40.0 );
		p.setBrush( Qt::NoBrush );
		QColor body = col.darker( 145 );
		QPen mp( body, 11 );
		mp.setCapStyle( Qt::FlatCap );
		p.setPen( mp );
		p.drawArc( QRectF( -16, -24, 32, 32 ), 0, 180 * 16 );
		p.drawLine( QPointF( -16, -8 ), QPointF( -16, 10 ) );
		p.drawLine( QPointF( 16, -8 ), QPointF( 16, 10 ) );
		p.setPen( Qt::NoPen );
		p.setBrush( col );
		p.drawRect( QRectF( -21.5, 10, 11, 9 ) );
		p.drawRect( QRectF( 10.5, 10, 11, 9 ) );
		p.restore();
	} else if ( id == QLatin1String( "target" ) ) {
		p.setBrush( Qt::NoBrush );
		p.drawEllipse( QPointF( 32, 32 ), 16, 16 );
		p.setBrush( col );
		p.drawEllipse( QPointF( 32, 32 ), 6, 6 );
		p.drawLine( QPointF( 32, 8 ), QPointF( 32, 16 ) );
		p.drawLine( QPointF( 32, 48 ), QPointF( 32, 56 ) );
		p.drawLine( QPointF( 8, 32 ), QPointF( 16, 32 ) );
		p.drawLine( QPointF( 48, 32 ), QPointF( 56, 32 ) );
	} else if ( id == QLatin1String( "brush" ) || id == QLatin1String( "mode_weightpaint" ) ) {
		// paintbrush: diagonal wooden handle, metal ferrule, tapered bristles
		p.save();
		p.translate( 33, 31 );
		p.rotate( 45.0 );
		p.setPen( Qt::NoPen );
		p.setBrush( col );
		p.drawRoundedRect( QRectF( -3.5, -28, 7, 20 ), 3, 3 );  // slim handle
		p.setBrush( col.darker( 150 ) );
		p.drawRoundedRect( QRectF( -6, -9, 12, 7 ), 1.5, 1.5 ); // ferrule band
		p.setBrush( col );
		QPainterPath bristles;                                  // bristle tuft
		bristles.moveTo( -6, -2 );
		bristles.lineTo( 6, -2 );
		bristles.lineTo( 3.2, 20 );
		bristles.quadTo( 0, 24, -3.2, 20 );
		bristles.closeSubpath();
		p.drawPath( bristles );
		p.restore();
	} else if ( id == QLatin1String( "chevron_left" ) || id == QLatin1String( "chevron_right" ) ) {
		// thin nav chevron (‹ ›) in the theme colour, not Qt's black arrow icon
		const bool left = ( id == QLatin1String( "chevron_left" ) );
		QPen cp( col, 6.0 );
		cp.setCapStyle( Qt::RoundCap );
		cp.setJoinStyle( Qt::RoundJoin );
		p.setPen( cp );
		p.setBrush( Qt::NoBrush );
		const float xTip = left ? 25.0f : 39.0f;
		const float xBack = left ? 39.0f : 25.0f;
		QPolygonF chev;
		chev << QPointF( xBack, 18 ) << QPointF( xTip, 32 ) << QPointF( xBack, 46 );
		p.drawPolyline( chev );
	} else if ( id == QLatin1String( "mode_object" ) ) {
		// solid-ish isometric cube (Object Mode)
		p.setBrush( Qt::NoBrush );
		QPen op( col, 4.0 );
		op.setJoinStyle( Qt::RoundJoin );
		p.setPen( op );
		QPolygonF top;
		top << QPointF( 32, 12 ) << QPointF( 52, 23 ) << QPointF( 32, 34 ) << QPointF( 12, 23 );
		p.drawPolygon( top );
		p.drawLine( QPointF( 12, 23 ), QPointF( 12, 43 ) );
		p.drawLine( QPointF( 52, 23 ), QPointF( 52, 43 ) );
		p.drawLine( QPointF( 32, 34 ), QPointF( 32, 54 ) );
		p.drawLine( QPointF( 12, 43 ), QPointF( 32, 54 ) );
		p.drawLine( QPointF( 52, 43 ), QPointF( 32, 54 ) );
	} else if ( id == QLatin1String( "mode_edit" ) ) {
		// wireframe triangle with vertex handles (Edit Mode)
		p.setBrush( Qt::NoBrush );
		QPen ep( col, 3.5 );
		ep.setJoinStyle( Qt::RoundJoin );
		p.setPen( ep );
		QPolygonF tri;
		tri << QPointF( 32, 14 ) << QPointF( 52, 50 ) << QPointF( 12, 50 );
		p.drawPolygon( tri );
		p.setPen( Qt::NoPen );
		p.setBrush( col );
		for ( const QPointF & v : { QPointF( 32, 14 ), QPointF( 52, 50 ), QPointF( 12, 50 ) } )
			p.drawRect( QRectF( v.x() - 4.5, v.y() - 4.5, 9, 9 ) );
	} else if ( id == QLatin1String( "mode_vertexpaint" ) ) {
		// triangle with per-vertex greyscale dots (Vertex Paint)
		p.setBrush( Qt::NoBrush );
		QPen vp( col.darker( 160 ), 3.0 );
		vp.setJoinStyle( Qt::RoundJoin );
		p.setPen( vp );
		QPolygonF tri;
		tri << QPointF( 32, 15 ) << QPointF( 51, 49 ) << QPointF( 13, 49 );
		p.drawPolygon( tri );
		p.setPen( Qt::NoPen );
		p.setBrush( col );
		p.drawEllipse( QPointF( 32, 15 ), 6.5, 6.5 );
		p.setBrush( col.darker( 175 ) );
		p.drawEllipse( QPointF( 51, 49 ), 6.5, 6.5 );
		p.setBrush( col.darker( 260 ) );
		p.drawEllipse( QPointF( 13, 49 ), 6.5, 6.5 );
	} else if ( id == QLatin1String( "mode_segment" ) ) {
		// a shape split into greyscale segments (Segment Paint)
		p.setPen( Qt::NoPen );
		p.setBrush( col.darker( 240 ) );
		p.drawRect( QRectF( 15, 18, 11.3, 28 ) );
		p.setBrush( col.darker( 160 ) );
		p.drawRect( QRectF( 26.3, 18, 11.3, 28 ) );
		p.setBrush( col );
		p.drawRect( QRectF( 37.6, 18, 11.3, 28 ) );
		p.setBrush( Qt::NoBrush );
		QPen sp( col, 3.0 );
		sp.setJoinStyle( Qt::RoundJoin );
		p.setPen( sp );
		p.drawRoundedRect( QRectF( 15, 18, 34, 28 ), 3, 3 );
	} else if ( id == QLatin1String( "mode_deform" ) ) {
		// deformation lattice/cage around a blob (Deformed Cage toggle)
		p.setPen( Qt::NoPen );
		p.setBrush( col.darker( 210 ) );
		p.drawEllipse( QPointF( 32, 34 ), 9, 9 );
		p.setBrush( Qt::NoBrush );
		QPen dp( col, 2.4 );
		dp.setStyle( Qt::DashLine );
		p.setPen( dp );
		p.drawRect( QRectF( 13, 15, 38, 38 ) );
		p.setPen( Qt::NoPen );
		p.setBrush( col );
		for ( const QPointF & v : { QPointF( 13, 15 ), QPointF( 51, 15 ), QPointF( 13, 53 ),
			QPointF( 51, 53 ), QPointF( 32, 15 ), QPointF( 13, 34 ), QPointF( 51, 34 ), QPointF( 32, 53 ) } )
			p.drawRect( QRectF( v.x() - 3.2, v.y() - 3.2, 6.4, 6.4 ) );
	} else if ( id == QLatin1String( "collision" ) ) {
		// wireframe cube: collision preview
		p.setBrush( Qt::NoBrush );
		QPen cp( col, 4.0 );
		cp.setJoinStyle( Qt::RoundJoin );
		p.setPen( cp );
		p.drawRect( QRectF( 10, 22, 32, 32 ) );
		p.drawRect( QRectF( 22, 10, 32, 32 ) );
		p.drawLine( QPointF( 10, 22 ), QPointF( 22, 10 ) );
		p.drawLine( QPointF( 42, 22 ), QPointF( 54, 10 ) );
		p.drawLine( QPointF( 10, 54 ), QPointF( 22, 42 ) );
		p.drawLine( QPointF( 42, 54 ), QPointF( 54, 42 ) );
	} else if ( id == QLatin1String( "view_center" ) ) {
		// re-center the camera: four corner brackets framing a center dot
		p.setBrush( Qt::NoBrush );
		QPen bp( col, 5.0 );
		bp.setCapStyle( Qt::RoundCap );
		p.setPen( bp );
		auto bracket = [&p]( QPointF c, float sx, float sy ) {
			p.drawLine( c, c + QPointF( 14 * sx, 0 ) );
			p.drawLine( c, c + QPointF( 0, 14 * sy ) );
		};
		bracket( QPointF( 10, 10 ), 1, 1 );
		bracket( QPointF( 54, 10 ), -1, 1 );
		bracket( QPointF( 10, 54 ), 1, -1 );
		bracket( QPointF( 54, 54 ), -1, -1 );
		p.setBrush( col );
		p.setPen( Qt::NoPen );
		p.drawEllipse( QPointF( 32, 32 ), 6, 6 );
	} else if ( id == QLatin1String( "cursor3d" ) ) {
		// Blender 3D cursor: red/white dashed circle with crosshair ticks
		p.setBrush( Qt::NoBrush );
		const QColor red( 214, 66, 66 );
		const float r = 15.0f;
		for ( int i = 0; i < 8; i++ ) {
			QPen dp( ( i & 1 ) ? col : red, 6.0 );
			dp.setCapStyle( Qt::FlatCap );
			p.setPen( dp );
			p.drawArc( QRectF( 32 - r, 32 - r, r * 2.0f, r * 2.0f ), ( i * 45 + 22 ) * 16, 45 * 16 );
		}
		QPen chp( col, 4.0 );
		chp.setCapStyle( Qt::RoundCap );
		p.setPen( chp );
		p.drawLine( QPointF( 32, 5 ), QPointF( 32, 19 ) );
		p.drawLine( QPointF( 32, 45 ), QPointF( 32, 59 ) );
		p.drawLine( QPointF( 5, 32 ), QPointF( 19, 32 ) );
		p.drawLine( QPointF( 45, 32 ), QPointF( 59, 32 ) );
	} else if ( id == QLatin1String( "gizmo" ) ) {
		// move-gizmo cross: four arrows from the center
		p.setBrush( col );
		QPen gp( col, 5 );
		gp.setCapStyle( Qt::RoundCap );
		p.setPen( gp );
		p.drawLine( QPointF( 32, 14 ), QPointF( 32, 50 ) );
		p.drawLine( QPointF( 14, 32 ), QPointF( 50, 32 ) );
		auto arrow = [&p]( QPointF tip, QPointF l, QPointF r ) {
			QPolygonF a;
			a << tip << l << r;
			p.drawPolygon( a );
		};
		arrow( QPointF( 32, 6 ),  QPointF( 25, 16 ), QPointF( 39, 16 ) );
		arrow( QPointF( 32, 58 ), QPointF( 25, 48 ), QPointF( 39, 48 ) );
		arrow( QPointF( 6, 32 ),  QPointF( 16, 25 ), QPointF( 16, 39 ) );
		arrow( QPointF( 58, 32 ), QPointF( 48, 25 ), QPointF( 48, 39 ) );
	} else if ( id == QLatin1String( "origins" ) ) {
		// origin dot with a dashed relationship line to the parent
		p.setBrush( col );
		p.setPen( Qt::NoPen );
		p.drawEllipse( QPointF( 20, 46 ), 8, 8 );
		p.drawEllipse( QPointF( 48, 16 ), 5, 5 );
		QPen dp( col, 4 );
		dp.setStyle( Qt::DotLine );
		dp.setCapStyle( Qt::RoundCap );
		p.setPen( dp );
		p.setBrush( Qt::NoBrush );
		p.drawLine( QPointF( 25, 40 ), QPointF( 44, 21 ) );
	} else if ( id == QLatin1String( "shade_wire" ) ) {
		// Blender wireframe shading: sphere of wires
		p.setBrush( Qt::NoBrush );
		QPen wp( col, 4 );
		p.setPen( wp );
		p.drawEllipse( QPointF( 32, 32 ), 22, 22 );
		p.drawEllipse( QRectF( 22, 10, 20, 44 ) );
		p.drawEllipse( QRectF( 10, 22, 44, 20 ) );
	} else if ( id == QLatin1String( "shade_solid" ) ) {
		// Blender solid shading: plain sphere
		p.setPen( QPen( col, 3 ) );
		p.setBrush( Qt::NoBrush );
		p.drawEllipse( QPointF( 32, 32 ), 22, 22 );
		p.setPen( Qt::NoPen );
		p.setBrush( col );
		p.drawEllipse( QPointF( 32, 32 ), 14, 14 );
	} else if ( id == QLatin1String( "shade_flat" ) ) {
		// Blender flat/wireframe-mode faces: a plain filled sphere, no highlight
		p.setPen( Qt::NoPen );
		p.setBrush( col.darker( 130 ) );
		p.drawEllipse( QPointF( 32, 32 ), 22, 22 );
	} else if ( id == QLatin1String( "shade_material" ) ) {
		// Blender material/rendered shading: sphere with a highlight
		p.setPen( Qt::NoPen );
		p.setBrush( col );
		p.drawEllipse( QPointF( 32, 32 ), 22, 22 );
		p.setBrush( QColor( 40, 40, 46 ) );
		p.drawEllipse( QPointF( 38, 38 ), 16, 16 );
		p.setBrush( col );
		p.drawEllipse( QPointF( 24, 24 ), 7, 7 );
	} else if ( id == QLatin1String( "shade_normalspec" ) ) {
		// Surface diagnostics: neutral sphere, normal direction and gloss glint.
		p.setPen( Qt::NoPen );
		p.setBrush( col.darker( 145 ) );
		p.drawEllipse( QPointF( 32, 32 ), 22, 22 );
		QPen np( col, 5.0 );
		np.setCapStyle( Qt::RoundCap );
		p.setPen( np );
		p.drawLine( QPointF( 21, 43 ), QPointF( 42, 22 ) );
		QPolygonF arrow;
		arrow << QPointF( 42, 22 ) << QPointF( 32, 25 ) << QPointF( 39, 32 );
		p.setBrush( col );
		p.drawPolygon( arrow );
		p.setPen( Qt::NoPen );
		p.setBrush( QColor( 255, 255, 255, 230 ) );
		p.drawEllipse( QPointF( 23, 21 ), 5, 5 );
	} else if ( id == QLatin1String( "shade_vertexcolor" ) ) {
		// Vertex colours: a sphere painted with red/green/blue/yellow patches
		p.setPen( Qt::NoPen );
		p.setClipRegion( QRegion( QRect( 10, 10, 44, 44 ), QRegion::Ellipse ) );
		p.fillRect( QRectF( 10, 10, 22, 22 ), QColor( 210, 80, 80 ) );
		p.fillRect( QRectF( 32, 10, 22, 22 ), QColor( 95, 185, 95 ) );
		p.fillRect( QRectF( 10, 32, 22, 22 ), QColor( 85, 125, 220 ) );
		p.fillRect( QRectF( 32, 32, 22, 22 ), QColor( 225, 205, 85 ) );
		p.setClipping( false );
	} else if ( id == QLatin1String( "xray" ) ) {
		// Blender toggle x-ray: two overlapping squares, the back one showing
		// through the front
		p.setBrush( Qt::NoBrush );
		QPen xp( col, 4 );
		xp.setJoinStyle( Qt::RoundJoin );
		p.setPen( xp );
		p.drawRect( QRectF( 22, 10, 32, 32 ) );
		p.setBrush( col );
		p.drawRect( QRectF( 10, 22, 32, 32 ) );
		p.setBrush( QColor( 40, 40, 46 ) );
		p.setPen( Qt::NoPen );
		p.drawRect( QRectF( 24, 24, 16, 16 ) );
	} else if ( id == QLatin1String( "normalize" ) ) {
		p.drawLine( QPointF( 32, 18 ), QPointF( 32, 46 ) );
		QPolygonF up;
		up << QPointF( 24, 20 ) << QPointF( 32, 8 ) << QPointF( 40, 20 );
		p.drawPolygon( up );
		QPolygonF dn;
		dn << QPointF( 24, 44 ) << QPointF( 32, 56 ) << QPointF( 40, 44 );
		p.drawPolygon( dn );
	} else if ( id == QLatin1String( "follow" ) ) {
		// playhead marker with a trailing arrow
		QPolygonF ph;
		ph << QPointF( 10, 10 ) << QPointF( 26, 10 ) << QPointF( 18, 24 );
		p.drawPolygon( ph );
		p.drawLine( QPointF( 18, 24 ), QPointF( 18, 54 ) );
		p.drawLine( QPointF( 28, 40 ), QPointF( 50, 40 ) );
		QPolygonF ar;
		ar << QPointF( 44, 32 ) << QPointF( 56, 40 ) << QPointF( 44, 48 );
		p.drawPolygon( ar );
	} else if ( id == QLatin1String( "check" ) ) {
		p.setBrush( Qt::NoBrush );
		QPen cp( col, 9 );
		cp.setCapStyle( Qt::RoundCap );
		cp.setJoinStyle( Qt::RoundJoin );
		p.setPen( cp );
		QPolygonF ck;
		ck << QPointF( 13, 34 ) << QPointF( 26, 48 ) << QPointF( 51, 16 );
		p.drawPolyline( ck );
	} else if ( id == QLatin1String( "panel" ) ) {
		p.setBrush( Qt::NoBrush );
		p.drawRoundedRect( QRectF( 10, 15, 44, 34 ), 4, 4 );
		p.setBrush( col );
		p.drawRect( QRectF( 39, 15, 15, 34 ) );
	} else if ( id == QLatin1String( "workspace" ) ) {
		// Four clean layout cells, matching the monochrome NifSkope toolbar set.
		p.setBrush( Qt::NoBrush );
		QPen wp( col, 4.0 );
		wp.setJoinStyle( Qt::RoundJoin );
		p.setPen( wp );
		p.drawRoundedRect( QRectF( 9, 11, 46, 42 ), 4, 4 );
		p.drawLine( QPointF( 32, 12 ), QPointF( 32, 52 ) );
		p.drawLine( QPointF( 10, 32 ), QPointF( 54, 32 ) );
		p.setPen( Qt::NoPen );
		p.setBrush( col );
		p.drawRoundedRect( QRectF( 35, 35, 16, 14 ), 2, 2 );
	} else if ( id == QLatin1String( "vert" ) ) {
		// Blender vertex-select: square of dots, one corner highlighted
		p.setBrush( Qt::NoBrush );
		QPen tp( col, 3 );
		p.setPen( tp );
		p.drawRect( QRectF( 18, 18, 28, 28 ) );
		p.setBrush( col );
		p.setPen( Qt::NoPen );
		for ( QPointF c : { QPointF( 18, 18 ), QPointF( 46, 18 ), QPointF( 18, 46 ), QPointF( 46, 46 ) } )
			p.drawEllipse( c, 5, 5 );
	} else if ( id == QLatin1String( "edge" ) ) {
		// Blender edge-select: square with the top edge highlighted
		p.setBrush( Qt::NoBrush );
		QPen tp( col, 3 );
		p.setPen( tp );
		p.drawRect( QRectF( 18, 18, 28, 28 ) );
		QPen ep( col, 7 );
		ep.setCapStyle( Qt::RoundCap );
		p.setPen( ep );
		p.drawLine( QPointF( 18, 18 ), QPointF( 46, 18 ) );
		p.setBrush( col );
		p.setPen( Qt::NoPen );
		p.drawEllipse( QPointF( 18, 18 ), 5, 5 );
		p.drawEllipse( QPointF( 46, 18 ), 5, 5 );
	} else if ( id == QLatin1String( "face" ) ) {
		// Blender face-select: filled square with a center dot
		p.setBrush( Qt::NoBrush );
		QPen tp( col, 3 );
		p.setPen( tp );
		p.drawRect( QRectF( 18, 18, 28, 28 ) );
		QColor fill = col;
		fill.setAlpha( 110 );
		p.fillRect( QRectF( 20, 20, 24, 24 ), fill );
		p.setBrush( col );
		p.setPen( Qt::NoPen );
		p.drawEllipse( QPointF( 32, 32 ), 5, 5 );
	} else if ( id == QLatin1String( "orient_global" ) ) {
		// Blender globe: circle + equator + meridian
		p.setBrush( Qt::NoBrush );
		QPen gp( col, 4 );
		p.setPen( gp );
		p.drawEllipse( QPointF( 32, 32 ), 21, 21 );
		p.drawEllipse( QPointF( 32, 32 ), 9, 21 );
		p.drawLine( QPointF( 11, 32 ), QPointF( 53, 32 ) );
	} else if ( id == QLatin1String( "orient_local" ) ) {
		// small cube (local space)
		p.setBrush( Qt::NoBrush );
		QPen gp( col, 4 );
		gp.setJoinStyle( Qt::RoundJoin );
		p.setPen( gp );
		p.drawRect( QRectF( 14, 22, 26, 26 ) );
		p.drawLine( QPointF( 14, 22 ), QPointF( 26, 12 ) );
		p.drawLine( QPointF( 40, 22 ), QPointF( 52, 12 ) );
		p.drawLine( QPointF( 26, 12 ), QPointF( 52, 12 ) );
		p.drawLine( QPointF( 52, 12 ), QPointF( 52, 38 ) );
		p.drawLine( QPointF( 40, 48 ), QPointF( 52, 38 ) );
	} else if ( id == QLatin1String( "orient_parent" ) ) {
		// hierarchy: parent box connected to a child box
		p.setBrush( Qt::NoBrush );
		QPen gp( col, 4 );
		p.setPen( gp );
		p.drawRect( QRectF( 10, 8, 20, 18 ) );
		p.drawRect( QRectF( 34, 38, 20, 18 ) );
		p.drawLine( QPointF( 20, 26 ), QPointF( 20, 47 ) );
		p.drawLine( QPointF( 20, 47 ), QPointF( 34, 47 ) );
	} else if ( id == QLatin1String( "orient_view" ) ) {
		// monitor (view space)
		p.setBrush( Qt::NoBrush );
		QPen gp( col, 4 );
		p.setPen( gp );
		p.drawRoundedRect( QRectF( 10, 12, 44, 30 ), 4, 4 );
		p.drawLine( QPointF( 32, 42 ), QPointF( 32, 52 ) );
		p.drawLine( QPointF( 22, 52 ), QPointF( 42, 52 ) );
	} else if ( id == QLatin1String( "pivot_origin" ) ) {
		// circle with center dot (median/origin point)
		p.setBrush( Qt::NoBrush );
		QPen gp( col, 4 );
		p.setPen( gp );
		p.drawEllipse( QPointF( 32, 32 ), 18, 18 );
		p.setBrush( col );
		p.setPen( Qt::NoPen );
		p.drawEllipse( QPointF( 32, 32 ), 6, 6 );
	} else if ( id == QLatin1String( "pivot_bounds" ) ) {
		// bounding box with center dot
		p.setBrush( Qt::NoBrush );
		QPen gp( col, 4 );
		p.setPen( gp );
		p.drawRect( QRectF( 14, 14, 36, 36 ) );
		p.setBrush( col );
		p.setPen( Qt::NoPen );
		p.drawEllipse( QPointF( 32, 32 ), 6, 6 );
	} else if ( id == QLatin1String( "pivot_median" ) ) {
		// three points with their median marked
		p.setBrush( Qt::NoBrush );
		QPen gp( col, 4 );
		p.setPen( gp );
		p.drawEllipse( QPointF( 18, 44 ), 8, 8 );
		p.drawEllipse( QPointF( 46, 44 ), 8, 8 );
		p.drawEllipse( QPointF( 32, 16 ), 8, 8 );
		p.setBrush( col );
		p.setPen( Qt::NoPen );
		p.drawEllipse( QPointF( 32, 36 ), 6, 6 );
	} else if ( id == QLatin1String( "snap_increment" ) ) {
		// grid of dots (Blender increment/grid snap)
		p.setPen( Qt::NoPen );
		p.setBrush( col );
		for ( int y = 0; y < 3; y++ ) {
			for ( int x = 0; x < 3; x++ )
				p.drawEllipse( QPointF( 16 + x * 16, 16 + y * 16 ), 4.5, 4.5 );
		}
	} else if ( id == QLatin1String( "chevron_right" ) ) {
		p.setBrush( Qt::NoBrush );
		QPen cp( col, 7 );
		cp.setCapStyle( Qt::RoundCap );
		cp.setJoinStyle( Qt::RoundJoin );
		p.setPen( cp );
		QPolygonF ck;
		ck << QPointF( 26, 18 ) << QPointF( 40, 32 ) << QPointF( 26, 46 );
		p.drawPolyline( ck );
	} else if ( id == QLatin1String( "chevron_down" ) ) {
		p.setBrush( Qt::NoBrush );
		QPen cp( col, 7 );
		cp.setCapStyle( Qt::RoundCap );
		cp.setJoinStyle( Qt::RoundJoin );
		p.setPen( cp );
		QPolygonF ck;
		ck << QPointF( 18, 26 ) << QPointF( 32, 40 ) << QPointF( 46, 26 );
		p.drawPolyline( ck );
	} else if ( id == QLatin1String( "pivot_cursor" ) ) {
		// mini 3D cursor: circle + crosshair ticks
		p.setBrush( Qt::NoBrush );
		QPen gp( col, 4 );
		p.setPen( gp );
		p.drawEllipse( QPointF( 32, 32 ), 13, 13 );
		p.drawLine( QPointF( 32, 8 ), QPointF( 32, 16 ) );
		p.drawLine( QPointF( 32, 48 ), QPointF( 32, 56 ) );
		p.drawLine( QPointF( 8, 32 ), QPointF( 16, 32 ) );
		p.drawLine( QPointF( 48, 32 ), QPointF( 56, 32 ) );
	}

	return QIcon( pm );
}

TimelineWidget::TimelineWidget( QWidget * parent ) : QWidget( parent )
{
	setFocusPolicy( Qt::StrongFocus );

	// fixed light color: the dark UI style leaves QPalette::ButtonText nearly
	// black, which made the drawn icons invisible on the dark toolbar
	const QColor icoCol( 228, 228, 232 );

	auto mkBtn = [this]( const QString & text, const QString & tip, bool checkable ) {
		auto b = new QToolButton( this );
		b->setText( text );
		b->setToolTip( tip );
		b->setCheckable( checkable );
		b->setAutoRaise( true );
		return b;
	};

	auto mkIconBtn = [this, &icoCol]( const QString & icon, const QString & tip, bool checkable ) {
		auto b = new QToolButton( this );
		b->setIcon( tlMakeIcon( icon, icoCol ) );
		b->setToolTip( tip );
		b->setCheckable( checkable );
		b->setAutoRaise( true );
		return b;
	};

	seqBox = new QComboBox( this );
	seqBox->setSizeAdjustPolicy( QComboBox::AdjustToContents );
	seqBox->setMinimumWidth( 100 );
	seqBox->setToolTip( tr( "Animation sequence shown in the Animation Manager (synced with the Animation toolbar)" ) );
	connect( seqBox, qOverload<int>( &QComboBox::activated ), this, &TimelineWidget::sequenceChosen );

	filterBox = new QLineEdit( this );
	filterBox->setPlaceholderText( tr( "Filter lanes" ) );
	filterBox->setClearButtonEnabled( true );
	filterBox->setMaximumWidth( 130 );
	connect( filterBox, &QLineEdit::textChanged, this, &TimelineWidget::filterEdited );

	btnToStart = mkIconBtn( QStringLiteral( "tostart" ), tr( "Jump to start" ), false );
	connect( btnToStart, &QToolButton::clicked, [this]() {
		transportStop();
		curTime = prevRangeOn ? prevStart : tMin;
		emit timeChanged( curTime );
		ensurePlayheadVisible();
		updateViews();
	} );

	btnPrevKey = mkIconBtn( QStringLiteral( "prevkey" ), tr( "Jump to previous key of the selected lane (,)" ), false );
	connect( btnPrevKey, &QToolButton::clicked, [this]() { stepToKey( -1 ); } );

	btnPlayBack = mkIconBtn( QStringLiteral( "playback" ), tr( "Play backward (Shift+Space)" ), true );
	connect( btnPlayBack, &QToolButton::clicked, [this]() { transportToggle( -1 ); } );

	btnPlay = mkIconBtn( QStringLiteral( "play" ), tr( "Play/pause (Space)" ), true );
	connect( btnPlay, &QToolButton::clicked, [this]() { transportToggle( 1 ); } );

	btnNextKey = mkIconBtn( QStringLiteral( "nextkey" ), tr( "Jump to next key of the selected lane (.)" ), false );
	connect( btnNextKey, &QToolButton::clicked, [this]() { stepToKey( 1 ); } );

	btnToEnd = mkIconBtn( QStringLiteral( "toend" ), tr( "Jump to end" ), false );
	connect( btnToEnd, &QToolButton::clicked, [this]() {
		transportStop();
		curTime = prevRangeOn ? prevEnd : tMax;
		emit timeChanged( curTime );
		ensurePlayheadVisible();
		updateViews();
	} );

	playTimer = new QTimer( this );
	playTimer->setInterval( 16 );
	connect( playTimer, &QTimer::timeout, [this]() {
		float lo = prevRangeOn ? prevStart : tMin;
		float hi = prevRangeOn ? prevEnd : tMax;
		if ( hi <= lo ) {
			transportStop();
			return;
		}
		curTime += playDir * 0.016f;
		if ( curTime > hi )
			curTime = lo + ( curTime - hi );
		else if ( curTime < lo )
			curTime = hi - ( lo - curTime );
		emit timeChanged( curTime );
		if ( followPlayhead )
			ensurePlayheadVisible();
		updateViews();
	} );

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

	btnSnap = mkIconBtn( QStringLiteral( "magnet" ), tr( "Snap dragging to steps" ), true );
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

	btnNormalize = mkIconBtn( QStringLiteral( "normalize" ), tr( "Normalize curves (each scaled to its own range)" ), true );
	connect( btnNormalize, &QToolButton::toggled, [this]( bool on ) {
		normalized = on;
		graphView->invalidateCurves();
	} );

	btnFollow = mkIconBtn( QStringLiteral( "follow" ), tr( "Follow playhead during playback" ), true );
	btnFollow->setChecked( followPlayhead );
	connect( btnFollow, &QToolButton::toggled, [this]( bool on ) { followPlayhead = on; } );

	btnIsolate = mkIconBtn( QStringLiteral( "target" ), tr( "Show only lanes of the selected node" ), true );
	connect( btnIsolate, &QToolButton::toggled, [this]( bool on ) {
		autoIsolate = on;
		if ( !on )
			filterTargetBlock = -1;
		applyFilter();
		updateViews();
	} );

	btnLint = mkIconBtn( QStringLiteral( "check" ), tr( "Check animation for common problems" ), false );
	connect( btnLint, &QToolButton::clicked, this, &TimelineWidget::runLint );

	btnInspector = mkIconBtn( QStringLiteral( "panel" ), tr( "Show/hide the inspector panel" ), true );
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
	topLay = topLayout;
	topLayout->setContentsMargins( 4, 2, 4, 2 );
	topLayout->setSpacing( 4 );
	topLayout->addWidget( seqBox );
	topLayout->addWidget( filterBox );
	topLayout->addWidget( btnIsolate );
	topLayout->addSpacing( 6 );
	topLayout->addWidget( btnToStart );
	topLayout->addWidget( btnPrevKey );
	topLayout->addWidget( btnPlayBack );
	topLayout->addWidget( btnPlay );
	topLayout->addWidget( btnNextKey );
	topLayout->addWidget( btnToEnd );
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

	auto hSplit = new QSplitter( Qt::Horizontal, this );
	hSplit->addWidget( leftWidget );
	hSplit->addWidget( inspector );
	hSplit->setStretchFactor( 0, 1 );
	hSplit->setStretchFactor( 1, 0 );
	hSplit->setCollapsible( 0, false );
	hSplit->setSizes( { 900, TL_INSP_W } );

	auto mainLayout = new QHBoxLayout( this );
	mainLayout->setContentsMargins( 0, 0, 0, 0 );
	mainLayout->setSpacing( 0 );
	mainLayout->addWidget( hSplit );

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

void TimelineWidget::showEvent( QShowEvent * event )
{
	QWidget::showEvent( event );
	// Repay a refresh that was skipped while the dock was hidden.
	if ( refreshPending )
		refreshTimer->start();
}

void TimelineWidget::transportToggle( int dir )
{
	if ( playDir == dir ) {
		transportStop();
		return;
	}

	playDir = dir;
	btnPlay->setChecked( dir == 1 );
	btnPlayBack->setChecked( dir == -1 );
	const QColor c( 228, 228, 232 );
	btnPlay->setIcon( tlMakeIcon( dir == 1 ? QStringLiteral( "pause" ) : QStringLiteral( "play" ), c ) );
	btnPlayBack->setIcon( tlMakeIcon( dir == -1 ? QStringLiteral( "pause" ) : QStringLiteral( "playback" ), c ) );
	playTimer->start();
}

void TimelineWidget::transportStop()
{
	playDir = 0;
	playTimer->stop();
	btnPlay->setChecked( false );
	btnPlayBack->setChecked( false );
	const QColor c( 228, 228, 232 );
	btnPlay->setIcon( tlMakeIcon( QStringLiteral( "play" ), c ) );
	btnPlayBack->setIcon( tlMakeIcon( QStringLiteral( "playback" ), c ) );
}

void TimelineWidget::addAnimActions( QAction * loop, QAction * sw )
{
	if ( !topLay )
		return;
	int at = topLay->indexOf( timeField );
	for ( QAction * a : { sw, loop } ) {
		if ( !a )
			continue;
		auto b = new QToolButton( this );
		b->setDefaultAction( a );
		b->setAutoRaise( true );
		topLay->insertWidget( at, b );
	}
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
	// Never scan while the model is loading/saving/processing: the refresh
	// timer can fire from processEvents() inside NifModel::load() and would
	// read a half-built model. Re-arm and try again once the model settles.
	if ( nif && nif->getState() != BaseModel::Default ) {
		refreshTimer->start();
		return;
	}

	// scanModel() walks every block of the file. While the Animation Manager
	// dock is hidden (closed or a background tab) that work is invisible —
	// remember that a refresh is owed and run it when the dock next shows.
	if ( !isVisible() ) {
		refreshPending = true;
		return;
	}
	refreshPending = false;

	scanning = true;

	qint32 prevSeqBlock = -1;
	int prevSeq = seqBox->currentIndex();
	bool prevLoose = ( prevSeq == 1 );
	if ( prevSeq >= 2 && prevSeq - 2 < sequences.count() && nif )
		prevSeqBlock = nif->getBlockNumber( QModelIndex( sequences[prevSeq - 2] ) );

	int prevLaneBlock = ( currentLane >= 0 && currentLane < lanes.count() && nif )
	                    ? nif->getBlockNumber( QModelIndex( lanes[currentLane].iSelect ) ) : -1;

	scanModel();

	int comboRow = prevLoose ? 1 : 0;
	if ( prevSeqBlock >= 0 ) {
		for ( int i = 0; i < sequences.count(); i++ ) {
			if ( nif->getBlockNumber( QModelIndex( sequences[i] ) ) == prevSeqBlock ) {
				comboRow = i + 2;
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
	seqBox->addItem( tr( "All animations" ) );
	seqBox->addItem( tr( "Loose animations" ) );

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
	if ( comboRow >= 2 && comboRow - 2 < sequences.count() && sequences[comboRow - 2].isValid() ) {
		// switching the displayed animation must NOT change the block-list /
		// viewport selection (only switch which sequence drives the timeline)
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
	for ( int i = 2; i < seqBox->count(); i++ ) {
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
	// Keep the key selection alive across rebuilds triggered by value edits
	// (e.g. dragging a key): the persistent indexes stay valid, only the lane
	// number has to be re-resolved after the lanes are rebuilt.
	QVector<QPersistentModelIndex> keepSel = selKeys;
	QPersistentModelIndex keepPrimary = primaryKey;
	QPersistentModelIndex keepLaneSel;
	if ( currentLane >= 0 && currentLane < lanes.size() )
		keepLaneSel = lanes[currentLane].iSelect;

	lanes.clear();
	currentLane = -1;
	selKeys.clear();
	primaryKey = QPersistentModelIndex();

	if ( !nif ) {
		infoLabel->clear();
		return;
	}

	markers.clear();
	markerChannel = TimelineChannel();

	// Combo rows: 0 = all controllers, 1 = loose interpolators, 2+ = sequences
	int view = seqBox->currentIndex();

	// interpolators referenced by sequences / attached to controllers
	QSet<int> seqInterps;
	for ( const auto & s : sequences ) {
		QModelIndex iCtrl = nif->getIndex( QModelIndex( s ), "Controlled Blocks" );
		for ( int r = 0; r < nif->rowCount( iCtrl ); r++ ) {
			qint32 l = nif->getLink( nif->getIndex( iCtrl, r ), "Interpolator" );
			if ( l >= 0 )
				seqInterps.insert( l );
		}
	}

	QSet<int> attachedInterps;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex iBlock = nif->getBlockIndex( b );
		if ( !nif->blockInherits( iBlock, "NiTimeController" ) )
			continue;
		for ( const char * ln : { "Interpolator", "Visibility Interpolator" } ) {
			qint32 l = nif->getLink( iBlock, ln );
			if ( l >= 0 ) {
				attachedInterps.insert( l );
				qint32 d = nif->getLink( nif->getBlockIndex( l ), "Data" );
				if ( d >= 0 )
					attachedInterps.insert( d );
			}
		}
		QModelIndex iW = nif->getIndex( iBlock, "Interpolator Weights" );
		for ( int r = 0; r < nif->rowCount( iW ); r++ ) {
			qint32 l = nif->getLink( nif->getIndex( iW, r ), "Interpolator" );
			if ( l >= 0 )
				attachedInterps.insert( l );
		}
	}

	if ( view >= 2 && view - 2 < sequences.count() ) {
		QModelIndex iSeq( sequences[view - 2] );
		addSequenceLanes( iSeq, false );
		collectMarkers( nif->getBlockIndex( nif->getLink( iSeq, "Text Keys" ), "NiTextKeyExtraData" ) );
	} else {
		if ( view == 0 ) {
			// standalone controllers first (manager/multi-target are wiring, not lanes)
			for ( int b = 0; b < nif->getBlockCount(); b++ ) {
				QModelIndex iBlock = nif->getBlockIndex( b );
				if ( nif->blockInherits( iBlock, "NiTimeController" )
				     && !nif->blockInherits( iBlock, "NiControllerManager" )
				     && !nif->blockInherits( iBlock, "NiMultiTargetTransformController" ) )
					addControllerLanes( iBlock );
			}

			// then each sequence as a collapsible group
			for ( const auto & s : sequences )
				addSequenceLanes( QModelIndex( s ), true );
		}

		// loose interpolators: reachable neither from a controller nor a sequence
		QSet<int> known = seqInterps;
		known.unite( attachedInterps );
		for ( const auto & lane : lanes )
			known.unite( lane.blockNums );

		int looseHeader = -1;
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			if ( known.contains( b ) )
				continue;
			QModelIndex iBlock = nif->getBlockIndex( b );
			if ( !nif->blockInherits( iBlock, "NiInterpolator" ) )
				continue;

			if ( view == 0 && looseHeader < 0 ) {
				TimelineLane header;
				header.isHeader = true;
				header.groupSeq = -2;
				header.label = tr( "Loose animations" );
				lanes.append( header );
				looseHeader = lanes.count() - 1;
			}

			addInterpolatorLane( iBlock, tr( "(loose)" ) );
			lanes.last().groupSeq = ( view == 0 ) ? -2 : -1;
		}

		// global text key markers
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex iBlock = nif->getBlockIndex( b );
			if ( nif->blockInherits( iBlock, "NiTextKeyExtraData" ) )
				collectMarkers( iBlock );
		}
	}

	int numKeys = 0;
	for ( const auto & lane : lanes )
		numKeys += lane.keys.count();

	infoLabel->setText( tr( "%1 lanes, %2 keys" ).arg( lanes.count() ).arg( numKeys ) );

	// Restore the selection that survived the rebuild
	for ( const auto & p : keepSel ) {
		if ( p.isValid() )
			selKeys.append( p );
	}
	if ( keepPrimary.isValid() )
		primaryKey = keepPrimary;
	else if ( !selKeys.isEmpty() )
		primaryKey = selKeys.first();
	if ( keepLaneSel.isValid() ) {
		for ( int i = 0; i < lanes.size(); i++ ) {
			if ( lanes[i].iSelect == keepLaneSel ) {
				currentLane = i;
				break;
			}
		}
	}
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

void TimelineWidget::addSequenceLanes( const QModelIndex & iSeq, bool withHeader, QSet<int> * seen )
{
	int seqBlock = nif->getBlockNumber( iSeq );

	if ( withHeader ) {
		TimelineLane header;
		header.isHeader = true;
		header.groupSeq = seqBlock;
		header.iSelect = iSeq;
		QModelIndex iCtrl0 = nif->getIndex( iSeq, "Controlled Blocks" );
		header.label = QString( "%1  (%2)" ).arg( nif->resolveString( iSeq, "Name" ) ).arg( nif->rowCount( iCtrl0 ) );
		header.start = nif->get<float>( iSeq, "Start Time" );
		header.stop = nif->get<float>( iSeq, "Stop Time" );
		header.hasCtrlRange = tlSaneTime( header.start ) && tlSaneTime( header.stop ) && header.stop >= header.start;
		lanes.append( header );
	}

	QModelIndex iCtrl = nif->getIndex( iSeq, "Controlled Blocks" );
	for ( int r = 0; r < nif->rowCount( iCtrl ); r++ ) {
		QModelIndex iRow = nif->getIndex( iCtrl, r );

		QString nodeName = nif->resolveString( iRow, "Node Name" );
		QModelIndex iCtlr = nif->getBlockIndex( nif->getLink( iRow, "Controller" ) );

		// Prefer the real controller block for the label so shader
		// controllers show their controlled variable
		QString ctype;
		if ( iCtlr.isValid() && !nif->blockInherits( iCtlr, "NiMultiTargetTransformController" ) )
			ctype = controllerLabel( iCtlr );
		else if ( iCtlr.isValid() )
			ctype = tr( "Transform" );
		else
			ctype = shortTypeName( nif->resolveString( iRow, "Controller Type" ) );

		QString label = nodeName;
		if ( !ctype.isEmpty() )
			label = ( label.isEmpty() ? QString() : label + QStringLiteral( " · " ) ) + ctype;
		if ( label.isEmpty() )
			label = tr( "Controlled Block %1" ).arg( r );

		QModelIndex iInterp = nif->getBlockIndex( nif->getLink( iRow, "Interpolator" ), "NiInterpolator" );
		if ( iInterp.isValid() ) {
			addInterpolatorLane( iInterp, label, iCtlr );
			TimelineLane & lane = lanes.last();
			lane.groupSeq = withHeader ? seqBlock : -1;
			lane.targetBlock = findAVObjectByName( nodeName );
			lane.searchText += QLatin1Char( ' ' ) + nodeName.toLower();
			if ( seen )
				seen->unite( lane.blockNums );
		}
	}
}

void TimelineWidget::collectMarkers( const QModelIndex & iTextKeys )
{
	if ( !iTextKeys.isValid() )
		return;

	QModelIndex iKeys = nif->getIndex( iTextKeys, "Text Keys" );
	if ( !iKeys.isValid() )
		return;

	bool first = !markerChannel.iKeysArray.isValid();
	if ( first ) {
		markerChannel.name = tr( "Text" );
		markerChannel.iKeysArray = iKeys;
		markerChannel.type = TimelineChannel::TextVal;
		markerChannel.numComponents = 0;
		markerChannel.interpolation = 5;
	}

	for ( int k = 0; k < nif->rowCount( iKeys ); k++ ) {
		QModelIndex iKey = nif->getIndex( iKeys, k );
		TimelineKey key;
		key.time = nif->get<float>( iKey, "Time" );
		key.idx = iKey;
		key.text = nif->resolveString( iKey, "Value" );
		markers.append( key );
		if ( first )
			markerChannel.keys.append( key );
	}
}

void TimelineWidget::toggleSeqCollapse( int seqBlock )
{
	if ( collapsedSeqs.contains( seqBlock ) )
		collapsedSeqs.remove( seqBlock );
	else
		collapsedSeqs.insert( seqBlock );

	applyFilter();
	lanesView->update();
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
	if ( seq >= 2 && seq - 2 < sequences.count() && nif ) {
		expand( nif->get<float>( QModelIndex( sequences[seq - 2] ), "Start Time" ) );
		expand( nif->get<float>( QModelIndex( sequences[seq - 2] ), "Stop Time" ) );
	}
	for ( const auto & m : markers )
		expand( m.time );

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
		if ( lanes[i].isHeader ) {
			if ( needle.isEmpty() && !( autoIsolate && filterTargetBlock >= 0 ) )
				visibleLanes.append( i );
			continue;
		}

		if ( lanes[i].groupSeq != -1 && collapsedSeqs.contains( lanes[i].groupSeq ) )
			continue;

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

	for ( const auto & k : markers ) {
		if ( dir > 0 && k.time > curTime + 1.0e-4f && ( !found || k.time < best ) ) {
			best = k.time;
			found = true;
		} else if ( dir < 0 && k.time < curTime - 1.0e-4f && ( !found || k.time > best ) ) {
			best = k.time;
			found = true;
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
	if ( nif->getState() != BaseModel::Default )
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
		transportToggle( shift ? -1 : 1 );
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
		QAction * mute = menu.addAction( l.muted ? tr( "Enable (set controller active)" ) : tr( "Disable (set controller inactive)" ),
			[this, lane]() { toggleLaneMute( lane ); } );
		Q_UNUSED( mute );
	}

	if ( l.targetBlock >= 0 ) {
		menu.addAction( tr( "Isolate target mesh in viewport" ), [this, lane]() {
			emit isolateBlock( lanes[lane].targetBlock );
		} );
		menu.addAction( tr( "Clear viewport isolation" ), [this]() { emit isolateBlock( -1 ); } );
	}

	QAction * lock = menu.addAction( l.locked ? tr( "Unlock lane" ) : tr( "Lock lane (block edits)" ), [this, lane]() {
		lanes[lane].locked = !lanes[lane].locked;
		lanesView->update();
	} );
	Q_UNUSED( lock );

	menu.exec( globalPos );
}
