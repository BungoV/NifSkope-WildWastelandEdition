// lane AIRZONE1 (bungo 10-29): the hand-placed volumes in the cell view (src/gl/cellairzone.h)

#include "gl/cellairzone.h"

#include "airzone.h"
#include "airzonemath.h"
#include "cellworldanim.h"	// the world clock (the fog noise's drift)
#include "harnesswindow.h"	// wwHarnessSettingsSuffix: a harness scope starts the row Off
#include "probevolumes.h"	// ProbeVolumes: the volume view's geometry builder draws our shapes
#include "version.h"
#include "wwswitch.h"

#include "gl/cellvolumeview.h"
#include "gl/glscene.h"
#include "gl/renderer.h"

#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

struct Row
{
	bool loaded = false;
	bool on = false;
};

Row & row()
{
	static Row r;
	return r;
}

QString rowApp()
{
	return QStringLiteral( "NifSkope " ) + NifSkopeVersion::rawToMajMin( NIFSKOPE_VERSION ) + wwHarnessSettingsSuffix();
}

// the packing a program reads this frame (one per document, version, eye, clock and red)
struct Frame
{
	const void * nif = nullptr;
	quint64 ver = ~Q_UINT64_C( 0 );
	std::vector<az::Volume> flat;
	double eye[3] = { 0, 0, 0 };
	double seconds = -1.0;
	int red = -1;
	int count = 0;
	int all = 0;
	int dropped = 0;
	std::vector<float> buf;
	std::vector<FloatVector4> v4;
	QString line;
};

Frame & frame()
{
	static Frame f;
	return f;
}

struct View
{
	bool on = false;
	int cell = -1, volume = -1;
	const void * nif = nullptr;
	quint64 ver = ~Q_UINT64_C( 0 );
	int builtCell = -2, builtVolume = -2;
	std::vector<Vector3> lp, sp;	// the volumes' lines (depth-tested), the selected one's (always on top)
	std::vector<FloatVector4> lc, sc;
};

View & view()
{
	static View v;
	return v;
}

int redBits()
{
	return wwAirZoneRed().split( QLatin1Char( ',' ), Qt::SkipEmptyParts ).contains( QStringLiteral( "hardedge" ) ) ? az::kRedHardEdge : 0;
}

void colourOf( const az::Volume & v, float rgb[3] )
{
	const float fog[3] = { 0.45f, 0.70f, 1.00f }, part[3] = { 1.00f, 0.75f, 0.20f }, gi[3] = { 0.35f, 0.90f, 0.45f },
		none[3] = { 0.6f, 0.6f, 0.6f };
	const float * c = v.fog.on ? fog : v.part.on ? part : v.gi.on ? gi : none;
	std::copy( c, c + 3, rgb );
}

// one shape's wireframe through the volume view's own builder (src/gl/cellvolumeview.cpp drawShape), recoloured
void appendShape( const az::Shape & s, const float rgba[4], std::vector<Vector3> & lp, std::vector<FloatVector4> & lc )
{
	ProbeVolumes pv;
	ProbeVolumeRoom r;
	ProbeVolumeShape p;
	p.type = ProbeShapeType( std::clamp( s.type, 0, 4 ) );
	std::copy( s.center, s.center + 3, p.center );
	std::copy( s.size, s.size + 3, p.size );
	std::copy( s.rotation, s.rotation + 3, p.rotation );
	r.shapes.push_back( p );
	pv.rooms.push_back( r );
	const WwCellVolumeGeometry g = wwCellVolumeGeometry( pv );
	for ( size_t i = 0; i + 3 <= g.lines.size(); i += 3 ) {
		lp.push_back( Vector3( g.lines[i], g.lines[i + 1], g.lines[i + 2] ) );
		lc.push_back( FloatVector4( rgba[0], rgba[1], rgba[2], rgba[3] ) );
	}
}

} // namespace

bool wwAirZonesPinned()
{
	return wwEnvIsSet( "WW_CELL_AIRZONE" );
}

bool wwAirZonesOn()
{
	if ( wwAirZonesPinned() ) {
		const QByteArray v = wwEnv( "WW_CELL_AIRZONE" ).trimmed();
		return !v.isEmpty() && v != "0";
	}
	Row & r = row();
	if ( !r.loaded ) {
		r.loaded = true;
		QSettings s( QStringLiteral( "NifTools" ), rowApp() );
		r.on = s.value( QStringLiteral( "WW/CellAirZones" ), false ).toBool();
	}
	return r.on;
}

void wwAirZonesSetOn( bool on )
{
	Row & r = row();
	r.loaded = true;
	r.on = on;
	QSettings s( QStringLiteral( "NifTools" ), rowApp() );
	s.setValue( QStringLiteral( "WW/CellAirZones" ), on );
}

bool wwAirZoneShaderDefine()
{
	return wwAirZonesOn();
}

void wwAirZoneUniforms( Scene * scene )
{
	if ( !scene || !scene->renderer )
		return;
	NifSkopeOpenGLContext::Program * prog = scene->renderer->getCurrentProgram();
	if ( !prog )
		return;
	const int loc = prog->uniLocation( "azZ" );	// only in a program built with WW_CELL_AIRZONE that reads the zones
	if ( loc < 0 )
		return;
	Frame & f = frame();
	quint64 ver = 0;
	const auto vols = wwAirZoneFor( scene->nifModel, &ver );
	bool moved = false;
	if ( f.nif != scene->nifModel || f.ver != ver ) {
		f.nif = scene->nifModel;
		f.ver = ver;
		f.flat.clear();
		if ( vols )
			for ( const AirZoneCellVolumes & cv : *vols )
				f.flat.insert( f.flat.end(), cv.volumes.begin(), cv.volumes.end() );
		moved = true;
	}
	// the eye in the world (the view row's translation column, as celllights.cpp giVolMaskPass's volRow)
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	double eye[3];
	for ( int k = 0; k < 3; k++ ) {
		double w = 0.0;
		for ( int j = 0; j < 3; j++ )
			w -= double( vt.rotation( j, k ) ) * double( vt.translation[j] );
		eye[k] = w / double( sc );
	}
	const double seconds = wwCellWorldSeconds();
	const int red = redBits();
	moved = moved || seconds != f.seconds || red != f.red || eye[0] != f.eye[0] || eye[1] != f.eye[1] || eye[2] != f.eye[2];
	if ( moved ) {
		std::copy( eye, eye + 3, f.eye );
		f.seconds = seconds;
		f.red = red;
		const std::vector<int> picked = az::pick( f.flat, eye, &f.dropped );
		f.buf.assign( size_t( az::kMax * az::kVec4 * 4 ), 0.0f );
		f.count = az::pack( f.flat, picked, seconds, f.buf.data() );
		f.all = 0;
		for ( const az::Volume & v : f.flat )
			f.all += ( v.fog.on || v.part.on ) ? 1 : 0;
		f.v4.resize( size_t( az::kMax * az::kVec4 ) );
		for ( size_t i = 0; i < f.v4.size(); i++ )
			f.v4[i] = FloatVector4( f.buf[i * 4], f.buf[i * 4 + 1], f.buf[i * 4 + 2], f.buf[i * 4 + 3] );
		const QString line = QStringLiteral( "air zones frame: %1 of %2 fog/particle volume(s) read%3%4" ).arg( f.count ).arg( f.all )
			.arg( f.dropped ? QStringLiteral( " (%1 farther dropped: at most %2)" ).arg( f.dropped ).arg( az::kMax ) : QString() )
			.arg( red ? QStringLiteral( ", RED hardedge" ) : QString() );
		if ( line != f.line ) {
			f.line = line;
			std::fprintf( stderr, "%s\n", qPrintable( line ) );
		}
	}
	prog->uni1i( "azCount", f.count );
	prog->uni1i( "azRed", f.red );
	if ( f.count > 0 )
		prog->uni4fv_l( loc, f.v4.data(), size_t( f.count * az::kVec4 ) );
}

void wwAirZoneViewSet( bool on, int cell, int volume )
{
	View & v = view();
	v.on = on;
	v.cell = cell;
	v.volume = volume;
}

QString wwAirZoneFrameLine()
{
	return frame().line;
}

void wwAirZoneViewDraw( Scene * scene, const Transform & viewTrans )
{
	if ( !scene || !scene->nifModel || scene->selecting )
		return;
	View & v = view();
	if ( !v.on && wwEnv( "WW_CELL_AIRZONE_VIEW" ).trimmed() != "1" )
		return;
	quint64 ver = 0;
	const auto vols = wwAirZoneFor( scene->nifModel, &ver );
	if ( !vols )
		return;
	if ( v.nif != scene->nifModel || v.ver != ver || v.builtCell != v.cell || v.builtVolume != v.volume ) {
		v.nif = scene->nifModel;
		v.ver = ver;
		v.builtCell = v.cell;
		v.builtVolume = v.volume;
		v.lp.clear();
		v.lc.clear();
		v.sp.clear();
		v.sc.clear();
		for ( size_t c = 0; c < vols->size(); c++ ) {
			const std::vector<az::Volume> & vv = ( *vols )[c].volumes;
			for ( size_t i = 0; i < vv.size(); i++ ) {
				const bool sel = int( c ) == v.cell && int( i ) == v.volume;
				float rgb[3];
				colourOf( vv[i], rgb );
				const float outer[4] = { sel ? 1.0f : rgb[0], sel ? 1.0f : rgb[1], sel ? 1.0f : rgb[2], 1.0f };
				appendShape( vv[i].shape, outer, sel ? v.sp : v.lp, sel ? v.sc : v.lc );
				if ( vv[i].edge > 0.0f ) {	// where the weight reaches 1: the shape shrunk by the edge, dimmer
					az::Shape in = vv[i].shape;
					for ( int k = 0; k < 3; k++ )
						in.size[k] = std::max( 1.0f, in.size[k] - 2.0f * vv[i].edge );
					const float inner[4] = { outer[0] * 0.5f, outer[1] * 0.5f, outer[2] * 0.5f, 1.0f };
					appendShape( in, inner, sel ? v.sp : v.lp, sel ? v.sc : v.lc );
				}
			}
		}
	}
	if ( v.lp.empty() && v.sp.empty() )
		return;
	scene->loadModelViewMatrix( viewTrans );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glDisable( GL_CULL_FACE );
	constexpr size_t kChunk = 65532;
	for ( size_t i = 0; i < v.lp.size(); i += kChunk )
		scene->drawLines( v.lp.data() + i, std::min( kChunk, v.lp.size() - i ), v.lc.data() + i );
	glDisable( GL_DEPTH_TEST );	// the selected volume shows through the walls while it is placed
	for ( size_t i = 0; i < v.sp.size(); i += kChunk )
		scene->drawLines( v.sp.data() + i, std::min( kChunk, v.sp.size() - i ), v.sc.data() + i );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LESS );
}
