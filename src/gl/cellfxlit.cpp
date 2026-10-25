/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellfxlit.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#include "celllights.h"
#include "gl/glscene.h"
#include "gl/renderer.h"
#include "gl/lookdevstage.h"	// lane LIGHTS3: a region's emittance at the clock
#include "gl/celllooks.h"	// lane LIGHTS4: the emittance rows, read at draw time

#include <QFile>
#include <QHash>
#include <QTextStream>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{

struct FxDoc
{
	QVector<WwFxLitModel> models;
	QHash<int, int> shapeModel;     // written shape's block -> index into models
	QHash<int, float> noOffset;     // light index -> base fade / (base fade + the placement's offset)
};

struct FxState
{
	QHash<const void *, FxDoc> docs;
	bool strongest = false;
	int red = 0;        // 1 white, 2 all, 4 nopower, 8 nofade
	int probe = 0;
	QString dump;
};

FxState & st()
{
	static FxState * s = nullptr;
	static quint64 liveGen = 0;
	// lane TESTDRIVER1: a live set of these switches re-reads them (pins and or-ed reds back to their defaults first)
	if ( !s || wwSwitchMovedFor( liveGen, { "WW_CELL_FXLIT", "WW_CELL_LIT_PROBE" } ) ) {
		if ( !s )
			s = new FxState;
		liveGen = wwSwitchGen();
		s->strongest = wwEnv( "WW_CELL_FXLIT_PICK" ) == "strong";
		const QByteArray r = wwEnv( "WW_CELL_FXLIT_RED" );
		s->red = r == "white" ? 1 : r == "all" ? 2 : r == "nopower" ? 4 : r == "nofade" ? 8 : 0;
		s->probe = wwEnvInt( "WW_CELL_LIT_PROBE" );
		s->dump = QString::fromLocal8Bit( wwEnv( "WW_CELL_FXLIT_DUMP" ) );
	}
	return *s;
}

float dist3( const float a[3], const float b[3] )
{
	const float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
	return std::sqrt( x * x + y * y + z * z );
}

// the game's lit-effect falloff at distance d of a light of radius r
float falloff( float d, float r )
{
	const float q = std::clamp( d / std::max( r, 1.0e-3f ), 0.0f, 1.0f );
	return std::pow( 1.0f - q * q, 2.2f );
}

}

void wwCellFxLitPickLights( const QVector<WwCellLight> & lights, const float center[3], float radius,
	bool strongest, int out[4] )
{
	struct Cand
	{
		int index;
		float key;
	};
	std::vector<Cand> cand;
	for ( int i = 0; i < lights.size(); i++ ) {
		const WwCellLight & l = lights[i];
		const float d = dist3( l.pos, center );
		if ( !( d - radius < l.radius ) )
			continue;   // the light does not reach the model's bound
		float key = 0.0f;
		if ( strongest )
			key = std::max( { l.color[0], l.color[1], l.color[2] } ) * falloff( d, l.radius );
		else if ( radius < 150.0f )
			key = ( d - radius ) / l.radius;
		cand.push_back( { i, key } );
	}
	// largest key first; a large model's list (every key 0) keeps the lights' own order
	std::stable_sort( cand.begin(), cand.end(), []( const Cand & a, const Cand & b ) { return a.key > b.key; } );
	for ( int k = 0; k < 4; k++ )
		out[k] = size_t( k ) < cand.size() ? cand[size_t( k )].index : -1;
}

void wwCellFxLitBegin( const void * nif )
{
	st().docs.remove( nif );
}

int wwCellFxLitModel( const void * nif, const WwFxLitModel & m )
{
	FxDoc & d = st().docs[nif];
	d.models.append( m );
	return d.models.size() - 1;
}

void wwCellFxLitShape( const void * nif, int block, int serial )
{
	st().docs[nif].shapeModel.insert( block, serial );
}

void wwCellFxLitForget( const void * nif, int block )
{
	auto d = st().docs.find( nif );
	if ( d != st().docs.end() )
		d->shapeModel.remove( block );
}

void wwCellFxLitNoOffset( const void * nif, int index, float ratio )
{
	if ( st().red & 8 )
		st().docs[nif].noOffset.insert( index, ratio );
}

void wwCellFxLitPick( const void * nif )
{
	FxState & s = st();
	auto it = s.docs.find( nif );
	if ( it == s.docs.end() )
		return;
	const WwCellLighting * L = wwCellLightsFor( nif );
	if ( !L ) {
		s.docs.erase( it );
		return;
	}
	for ( WwFxLitModel & m : it->models ) {
		wwCellFxLitPickLights( L->lights, m.center, m.radius, s.strongest, m.light );
		for ( int k = 0; k < 4; k++ )
			m.scale[k] = it->noOffset.value( m.light[k], 1.0f );
	}
	if ( s.dump.isEmpty() )
		return;
	QFile f( s.dump );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Text ) )
		return;
	QTextStream o( &f );
	o << "fxlit pick=" << ( s.strongest ? "strong" : "game" ) << " red=" << s.red << " models=" << it->models.size()
	  << " shapes=" << it->shapeModel.size() << " lights=" << L->lights.size() << "\n";
	for ( int i = 0; i < it->models.size(); i++ ) {
		const WwFxLitModel & m = it->models[i];
		o << "model " << i << " ref " << QString::number( m.ref, 16 ) << " center " << m.center[0] << " "
		  << m.center[1] << " " << m.center[2] << " radius " << m.radius << " lights";
		for ( int k = 0; k < 4; k++ ) {
			if ( m.light[k] < 0 )
				continue;
			const WwCellLight & l = L->lights[m.light[k]];
			o << " " << m.light[k] << ":" << l.pos[0] << "," << l.pos[1] << "," << l.pos[2];
		}
		o << " model " << m.model << "\n";
	}
}

const WwFxLitModel * wwCellFxLitFor( const void * nif, int block, int * serial )
{
	const FxState & s = st();
	const auto it = s.docs.constFind( nif );
	if ( it == s.docs.constEnd() )
		return nullptr;
	const auto sh = it->shapeModel.constFind( block );
	if ( sh == it->shapeModel.constEnd() || sh.value() < 0 || sh.value() >= it->models.size() )
		return nullptr;
	if ( serial )
		*serial = sh.value();
	return &it->models[sh.value()];
}

void wwCellFxLitUniforms( Scene * scene, int block )
{
	if ( !scene || !scene->renderer )
		return;
	NifSkopeOpenGLContext::Program * prog = scene->renderer->getCurrentProgram();
	if ( !prog || prog->uniLocation( "fxLitMode" ) < 0 )
		return;
	int serial = -1;
	const WwFxLitModel * m = wwCellLightsWanted( scene ) ? wwCellFxLitFor( scene->nifModel, block, &serial ) : nullptr;
	if ( !m ) {
		prog->uni1i( "fxLitMode", 0 );
		return;
	}
	const int red = st().red;
	// 1 lit; 2 every light (red), 4 no 2.2 on the falloff (red), 8 self-lit as before (red)
	prog->uni1i( "fxLitMode", 1 | ( red & 2 ) | ( red & 4 ) | ( ( red & 1 ) ? 8 : 0 ) );
	prog->uni4f( "fxLit", FloatVector4( float( m->light[0] ), float( m->light[1] ), float( m->light[2] ), float( m->light[3] ) ) );
	prog->uni4f( "fxLitScale", FloatVector4( m->scale[0], m->scale[1], m->scale[2], m->scale[3] ) );
	const int id = serial + 1;
	prog->uni3f( "fxLitId", float( id & 255 ) / 255.0f, float( ( id >> 8 ) & 255 ) / 255.0f, float( ( id >> 16 ) & 255 ) / 255.0f );
}

bool wwCellFxLitProbeShape( Scene * scene, int block )
{
	const int p = st().probe;
	return p >= 70 && p <= 75 && scene && wwCellFxLitFor( scene->nifModel, block ) != nullptr;	// lane GIDARK1: + 75
}

bool wwCellFxRoomLit( Scene * scene, int block )
{
	WW_SWITCH_STATIC( bool, red, wwEnv( "WW_CELL_FXROOM_RED" ).trimmed() == "game" );
	if ( red || !scene || !wwCellLightsWanted( scene ) )
		return false;
	const WwCellLighting * L = wwCellLightsFor( scene->nifModel );
	return L && !L->interior && wwCellFxLitFor( scene->nifModel, block ) != nullptr;
}

void wwCellFxLitSphere( const float * xyz, size_t count, float out[4] )
{
	out[0] = out[1] = out[2] = 0.0f;
	out[3] = -1.0f;
	if ( !xyz || !count )
		return;
	float lo[3] = { xyz[0], xyz[1], xyz[2] }, hi[3] = { xyz[0], xyz[1], xyz[2] };
	for ( size_t i = 1; i < count; i++ )
		for ( int k = 0; k < 3; k++ ) {
			lo[k] = std::min( lo[k], xyz[i * 3 + size_t( k )] );
			hi[k] = std::max( hi[k], xyz[i * 3 + size_t( k )] );
		}
	for ( int k = 0; k < 3; k++ )
		out[k] = ( lo[k] + hi[k] ) * 0.5f;
	float r = 0.0f;
	for ( size_t i = 0; i < count; i++ )
		r = std::max( r, dist3( xyz + i * 3, out ) );
	out[3] = r;
}

void wwCellFxLitMerge( float a[4], const float b[4] )
{
	if ( b[3] < 0.0f )
		return;
	if ( a[3] < 0.0f ) {
		for ( int k = 0; k < 4; k++ )
			a[k] = b[k];
		return;
	}
	const float d = dist3( a, b );
	if ( d + b[3] <= a[3] )
		return;     // b is inside a
	if ( d + a[3] <= b[3] ) {
		for ( int k = 0; k < 4; k++ )
			a[k] = b[k];
		return;
	}
	// the smallest sphere holding both
	const float r = ( d + a[3] + b[3] ) * 0.5f;
	const float t = d > 1.0e-6f ? ( r - a[3] ) / d : 0.0f;
	for ( int k = 0; k < 3; k++ )
		a[k] += ( b[k] - a[k] ) * t;
	a[3] = r;
}

// ---- lane LIGHTS2 (XEMI1): the placement emittance colour of each written shape, by document and block
namespace
{
QHash<const void *, QHash<int, quint32>> & xemiDocs()
{
	static QHash<const void *, QHash<int, quint32>> d;
	return d;
}
QVector<quint32> & xemiRegions()
{
	static QVector<quint32> r;
	return r;
}
}

quint32 wwCellXemiRegionCode( quint32 regn )
{
	int slot = xemiRegions().indexOf( regn );
	if ( slot < 0 ) {
		slot = xemiRegions().size();
		xemiRegions().append( regn );
	}
	return 0x02000000U | quint32( slot );
}

void wwCellXemiBegin( const void * nif )
{
	xemiDocs().remove( nif );
}

void wwCellXemiShape( const void * nif, int block, quint32 rgb )
{
	xemiDocs()[nif].insert( block, rgb );
}

void wwCellXemiForget( const void * nif, int block )
{
	auto d = xemiDocs().find( nif );
	if ( d != xemiDocs().end() )
		d->remove( block );
}

bool wwCellXemiCode( quint32 code, float rgb[3] )
{
	// red: magenta on every tinted shape, so the picture shows which shapes the rule reached
	WW_SWITCH_STATIC( QByteArray, red, wwEnv( "WW_CELL_XEMI_RED" ).trimmed() );
	if ( red != "magenta" && ( code & 0x02000000U ) ) {
		const int slot = int( code & 0xFFFFFFU );
		return slot < xemiRegions().size() && wwLookdevRegionEmit( xemiRegions().at( slot ), rgb );
	}
	const quint32 c = red == "magenta" ? 0xFF00FFU : ( code & 0xFFFFFFU );
	rgb[0] = float( ( c >> 16 ) & 0xFF ) / 255.0f;
	rgb[1] = float( ( c >> 8 ) & 0xFF ) / 255.0f;
	rgb[2] = float( c & 0xFF ) / 255.0f;
	return true;
}

bool wwCellXemiFor( const void * nif, int block, float rgb[3] )
{
	const auto d = xemiDocs().constFind( nif );
	if ( d == xemiDocs().constEnd() )
		return false;
	const auto it = d->constFind( block );
	if ( it == d->constEnd() )
		return false;
	// lane LIGHTS4: every shape registers at build; its row is read here, so a toggle needs no reopen
	const quint32 code = it.value();
	if ( !wwLookOn( ( code & 0x02000000U ) ? WwLook::RegionEmittance : WwLook::PlacementEmittance )
		|| ( ( code & 0x04000000U ) && !wwLookOn( WwLook::ParticleEmittance ) ) )
		return false;
	return wwCellXemiCode( code, rgb );
}
