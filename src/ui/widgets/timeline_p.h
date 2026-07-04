/***** BEGIN LICENSE BLOCK *****

BSD License - see timeline.h

***** END LICENCE BLOCK *****/

#ifndef TIMELINE_P_H
#define TIMELINE_P_H

#include "gl/glcontroller.h"
#include "data/niftypes.h"

#include <QColor>
#include <QPointF>
#include <QPolygonF>
#include <QString>

//! @file timeline_p.h Shared internals of the timeline implementation files

class QWidget;

// Layout constants
constexpr int TL_RULER_H   = 20;
constexpr int TL_SUMMARY_H = 16;
constexpr int TL_LANE_H    = 18;
constexpr int TL_KEY_R     = 4;
constexpr int TL_INSP_W    = 210;

// Component colors (X/R, Y/G, Z/B, W/A)
inline const QColor & tlCompColor( int comp )
{
	static const QColor colors[4] = {
		QColor( 0xe0, 0x55, 0x55 ), QColor( 0x55, 0xc0, 0x60 ),
		QColor( 0x55, 0x88, 0xe0 ), QColor( 0xe0, 0xa0, 0x30 )
	};
	return colors[comp & 3];
}

QString tlKeyTypeName( int t );
float tlNiceStep( float raw );

inline QPolygonF tlDiamond( const QPointF & c, float r )
{
	QPolygonF p;
	p << QPointF( c.x(), c.y() - r ) << QPointF( c.x() + r, c.y() )
	  << QPointF( c.x(), c.y() + r ) << QPointF( c.x() - r, c.y() );
	return p;
}

double QInputDialog_getDouble_compat( QWidget * parent, const QString & title, const QString & label,
                                      double value, double min, double max, int decimals, bool * ok );

// Explicit specializations defined in gl/glcontroller.cpp
template <> bool Controller::interpolate( float & value, const QModelIndex & array, float time, int & last );
template <> bool Controller::interpolate( Vector3 & value, const QModelIndex & array, float time, int & last );
template <> bool Controller::interpolate( Color3 & value, const QModelIndex & array, float time, int & last );
template <> bool Controller::interpolate( Color4 & value, const QModelIndex & array, float time, int & last );
template <> bool Controller::interpolate( bool & value, const QModelIndex & array, float time, int & last );

#endif
