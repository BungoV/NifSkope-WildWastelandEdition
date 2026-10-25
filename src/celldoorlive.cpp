#include "celldoorlive.h"
#include "wwswitch.h"	// lane LIVESET1: the request follows a live set

#include "gl/celllights.h"
#include "gl/celllooks.h"	// lane GPURELIGHT1: Probe sharing
#include "gl/glcontroller.h"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "model/nifmodel.h"
#include "proberelight.h"

#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>
#include <QWindow>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

namespace {

enum Mode { ModeOff = 0, ModeOpen, ModeClose, ModeCycle, ModeList };
enum Red { RedNone = 0, RedStatic, RedSnap };

struct Req
{
	quint32 ref = 0;
	int mode = ModeOff;
	int red = RedNone;
};

const Req & req()
{
	WW_SWITCH_STATIC( Req, r, [] {	// lane LIVESET1: re-read on a live set (a new door ref still needs the reopen)
		Req q;
		const QString v = QString::fromLocal8Bit( qgetenv( "WW_CELL_DOOR_ANIM" ) ).trimmed().toLower();
		if ( v == QLatin1String( "list" ) ) {
			q.mode = ModeList;
		} else if ( !v.isEmpty() ) {
			bool ok = false;
			const quint32 ref = v.section( QLatin1Char( ':' ), 0, 0 ).toUInt( &ok, 16 );
			const QString m = v.section( QLatin1Char( ':' ), 1, 1 );
			if ( ok && ref ) {
				q.ref = ref;
				q.mode = m == QLatin1String( "open" ) ? ModeOpen : m == QLatin1String( "close" ) ? ModeClose : ModeCycle;
			}
		}
		const QByteArray red = qgetenv( "WW_CELL_DOORLIVE_RED" ).trimmed();
		q.red = red == "static" ? RedStatic : red == "snap" ? RedSnap : RedNone;
		return q;
	}() );
	return r;
}

struct NodePose
{
	int id = -1;
	Vector3 t;
	Quat q;
};

struct Kept
{
	// the branch
	QPointer<NifModel> nif;
	quint32 ref = 0;
	QString model, note;
	qint32 wrapper = -1, root = -1, count = 0;
	Transform place;	// the wrapper's placement (the hinge fallback turns it)
	// the GI
	bool giKept = false;
	QString giWhy;
	std::unique_ptr<ProbeRelightOps> ops;
	std::unique_ptr<ProbeRelightGpu> gpu;
	bool gpuTried = false, gpuOk = false;
	std::unique_ptr<WwCellGi> base;
	ProbeRelightOut open;	// the all-open run (the bake's state)
	bool haveOpen = false;
	bool share = false;	// lane GPURELIGHT1: Probe sharing as the base was relit (the runs match the base)
	int slot = -1;
	float floorP[3] = { 0, 0, 0 };
	bool haveFloor = false;
	// the binding
	const Scene * boundScene = nullptr;
	int boundCount = -1;
	QPointer<Node> rootNode;
	Controller * mttc = nullptr;
	QString seqOpen, seqClose;
	float openStart = 0, openStop = 0, closeStart = 0, closeStop = 0;
	bool hinge = false;
	int moverId = -1;
	bool moverTurns = false;
	NodePose closedPose, openPose;
	// the clock
	int mode = ModeOff;
	QElapsedTimer clock;
	bool driven = false;
	double drivenT = 0;
	// the state
	QString state = QStringLiteral( "closed" );
	float frac = 0.0f;		// the open fraction now
	float relitShut = 0.0f;	// the shut fraction the GI shows (the bake: open)
	bool relitAny = false;
	size_t airMoved = 0;	// SKY3: air voxels the last relight moved
	bool pending = false;
	double relightMs = 0;
	int relights = 0;
	float floorE = -1.0f;
	QString lastEcho;
	QPointer<QWindow> view;
};

Kept & kept()
{
	static Kept k;
	return k;
}

double holdS()
{
	WW_SWITCH_STATIC( double, h, qEnvironmentVariableIsSet( "WW_CELL_DOORLIVE_HOLD" )
		? std::max( 0.0, qEnvironmentVariable( "WW_CELL_DOORLIVE_HOLD" ).toDouble() ) : 1.0 );
	return h;
}

// the hinge fallback's swing (no Open or Close sequence in the model)
constexpr double kHingeS = 1.5;
constexpr float kHingeDeg = 90.0f;

double openLen( const Kept & k )
{
	if ( k.hinge )
		return kHingeS;
	if ( !k.seqOpen.isEmpty() )
		return std::max( 0.0f, k.openStop - k.openStart );
	return std::max( 0.0f, k.closeStop - k.closeStart );
}

double closeLen( const Kept & k )
{
	if ( k.hinge )
		return kHingeS;
	if ( !k.seqClose.isEmpty() )
		return std::max( 0.0f, k.closeStop - k.closeStart );
	return std::max( 0.0f, k.openStop - k.openStart );
}

struct Pose
{
	QString seq;	// empty: the hinge
	float t = 0;	// the sequence's time, or the hinge's open fraction
	QString state;
};

//! phase 0 closed, 1 opening (x seconds in), 2 open, 3 closing (x seconds in)
Pose poseOf( const Kept & k, int phase, double x )
{
	Pose p;
	static const char * names[4] = { "closed", "opening", "open", "closing" };
	p.state = QLatin1String( names[phase] );
	if ( k.hinge ) {
		p.t = phase == 0 ? 0.0f : phase == 2 ? 1.0f : phase == 1 ? float( x / kHingeS ) : float( 1.0 - x / kHingeS );
		p.t = std::clamp( p.t, 0.0f, 1.0f );
		return p;
	}
	const bool o = !k.seqOpen.isEmpty(), c = !k.seqClose.isEmpty();
	switch ( phase ) {
	case 0:
		p.seq = o ? k.seqOpen : k.seqClose;
		p.t = o ? k.openStart : k.closeStop;
		break;
	case 1:
		p.seq = o ? k.seqOpen : k.seqClose;	// no Open: the Close played backwards
		p.t = o ? float( k.openStart + x ) : float( k.closeStop - x );
		break;
	case 2:
		p.seq = o ? k.seqOpen : k.seqClose;
		p.t = o ? k.openStop : k.closeStart;
		break;
	default:
		p.seq = c ? k.seqClose : k.seqOpen;	// no Close: the Open played backwards
		p.t = c ? float( k.closeStart + x ) : float( k.openStop - x );
		break;
	}
	return p;
}

//! the door's pose at `s` seconds of its clock under the mode
Pose poseAtClock( const Kept & k, double s )
{
	const double H = holdS(), Do = openLen( k ), Dc = closeLen( k );
	s = std::max( 0.0, s );
	if ( k.mode == ModeOpen ) {
		if ( s < H )
			return poseOf( k, 0, 0 );
		return s < H + Do ? poseOf( k, 1, s - H ) : poseOf( k, 2, 0 );
	}
	if ( k.mode == ModeClose ) {
		if ( s < H )
			return poseOf( k, 2, 0 );
		return s < H + Dc ? poseOf( k, 3, s - H ) : poseOf( k, 0, 0 );
	}
	const double P = 2 * H + Do + Dc;
	const double u = P > 0 ? std::fmod( s, P ) : 0.0;
	if ( u < H )
		return poseOf( k, 0, 0 );
	if ( u < H + Do )
		return poseOf( k, 1, u - H );
	if ( u < 2 * H + Do )
		return poseOf( k, 2, 0 );
	return poseOf( k, 3, u - 2 * H - Do );
}

bool inBranch( const Kept & k, int id )
{
	return id >= k.root && id < k.root + k.count;
}

Node * nodeOf( Scene * scene, int id )
{
	for ( Node * n : scene->nodes.list() )
		if ( n && n->id() == id )
			return n;
	return nullptr;
}

//! poses the door (the sequence's interpolators, or the hinge's turn of the wrapper)
void applyPose( Scene * scene, Kept & k, const Pose & p )
{
	if ( k.hinge ) {
		if ( !k.nif )
			return;
		Transform t = k.place;
		t.rotation = k.place.rotation * Matrix::euler( 0.0f, 0.0f, float( double( p.t ) * double( kHingeDeg ) * 3.14159265358979 / 180.0 ) );
		t.writeBack( k.nif, k.nif->getBlockIndex( k.wrapper ) );
		scene->transformDirty = true;
		return;
	}
	if ( !k.rootNode || !k.mttc )
		return;
	// a scene update may have bound another sequence: bind ours every time (a few controlled blocks)
	k.rootNode->setSequence( p.seq );
	// frequency 0, phase t: the controller's own time, whatever the scene's clock (it may animate), lands on t
	k.mttc->frequency = 0.0f;
	k.mttc->phase = p.t;
	k.mttc->extrapolation = Controller::Constant;
	k.mttc->active = true;
	k.mttc->updateTime( 0.0f );
	scene->transformDirty = true;
}

NodePose snap( Scene * scene, int id )
{
	NodePose np;
	np.id = id;
	if ( Node * n = nodeOf( scene, id ) ) {
		const Transform & t = n->localTrans();
		np.t = t.translation;
		np.q = t.rotation.toQuat();
	}
	return np;
}

float quatAngle( const Quat & a, const Quat & b )
{
	const float d = std::min( 1.0f, std::fabs( Quat::dotproduct( a, b ) ) );
	return 2.0f * std::acos( d );
}

//! the open fraction from the mover's pose (its slide, or its turn), between the closed and open poses
float openFraction( Scene * scene, const Kept & k, const Pose & p )
{
	if ( k.hinge )
		return p.t;
	if ( k.moverId < 0 )
		return p.state == QLatin1String( "open" ) ? 1.0f : 0.0f;
	const NodePose now = snap( scene, k.moverId );
	float f;
	if ( k.moverTurns ) {
		const float full = quatAngle( k.closedPose.q, k.openPose.q );
		f = full > 1e-6f ? quatAngle( k.closedPose.q, now.q ) / full : 0.0f;
	} else {
		const Vector3 d = k.openPose.t - k.closedPose.t;
		const float dd = Vector3::dotproduct( d, d );
		f = dd > 1e-12f ? Vector3::dotproduct( now.t - k.closedPose.t, d ) / dd : 0.0f;
	}
	return std::clamp( f, 0.0f, 1.0f );
}

//! finds the door's nodes and controller in this scene, its sequences, and the mover
bool bind( Scene * scene, Kept & k )
{
	if ( !scene || !k.nif || scene->nifModel != k.nif.data() || k.root < 0 )
		return false;
	if ( k.boundScene == scene && k.boundCount == scene->nodes.list().size() && ( k.hinge || ( k.rootNode && k.mttc ) ) )
		return true;
	k.boundScene = scene;
	k.boundCount = int( scene->nodes.list().size() );
	k.rootNode = nodeOf( scene, k.root );
	k.mttc = nullptr;
	k.seqOpen.clear();
	k.seqClose.clear();
	const NifModel * nif = k.nif.data();
	qint32 mttcBlock = -1;
	for ( qint32 b = k.root; b < k.root + k.count; b++ ) {
		const QModelIndex ib = nif->getBlockIndex( b );
		if ( !ib.isValid() )
			continue;
		if ( mttcBlock < 0 && nif->blockInherits( ib, "NiMultiTargetTransformController" ) )
			mttcBlock = b;
		if ( nif->blockInherits( ib, "NiControllerSequence" ) ) {
			const QString n = nif->get<QString>( ib, "Name" );
			if ( n.compare( QLatin1String( "Open" ), Qt::CaseInsensitive ) == 0 && k.seqOpen.isEmpty() ) {
				k.seqOpen = n;
				k.openStart = nif->get<float>( ib, "Start Time" );
				k.openStop = nif->get<float>( ib, "Stop Time" );
			} else if ( n.compare( QLatin1String( "Close" ), Qt::CaseInsensitive ) == 0 && k.seqClose.isEmpty() ) {
				k.seqClose = n;
				k.closeStart = nif->get<float>( ib, "Start Time" );
				k.closeStop = nif->get<float>( ib, "Stop Time" );
			}
		}
	}
	if ( mttcBlock >= 0 ) {
		const QModelIndex im = nif->getBlockIndex( mttcBlock );
		for ( Node * n : scene->nodes.list() )
			if ( n && inBranch( k, n->id() ) )
				if ( Controller * c = static_cast<IControllable *>( n )->findController( im ) ) {
					k.mttc = c;
					break;
				}
	}
	k.hinge = ( k.seqOpen.isEmpty() && k.seqClose.isEmpty() ) || !k.mttc || !k.rootNode;
	if ( k.hinge ) {
		k.moverId = -1;
		return true;
	}
	// the reference poses, and the node that moves the most between them
	std::vector<NodePose> c0, c1;
	applyPose( scene, k, poseOf( k, 0, 0 ) );
	for ( Node * n : scene->nodes.list() )
		if ( n && inBranch( k, n->id() ) )
			c0.push_back( snap( scene, n->id() ) );
	applyPose( scene, k, poseOf( k, 2, 0 ) );
	for ( Node * n : scene->nodes.list() )
		if ( n && inBranch( k, n->id() ) )
			c1.push_back( snap( scene, n->id() ) );
	float bestT = 0.0f, bestA = 0.0f;
	int idT = -1, idA = -1;
	for ( size_t i = 0; i < c0.size() && i < c1.size(); i++ ) {
		const float dt = ( c1[i].t - c0[i].t ).length(), da = quatAngle( c0[i].q, c1[i].q );
		if ( dt > bestT ) {
			bestT = dt;
			idT = int( i );
		}
		if ( da > bestA ) {
			bestA = da;
			idA = int( i );
		}
	}
	// a slide of a unit or more reads as the slide; else the turn (a swing door's hinge node)
	const int pick = bestT >= 1.0f ? idT : idA;
	k.moverTurns = bestT < 1.0f;
	k.moverId = pick >= 0 ? c0[size_t( pick )].id : -1;
	if ( pick >= 0 ) {
		k.closedPose = c0[size_t( pick )];
		k.openPose = c1[size_t( pick )];
	}
	return true;
}

int voxelOf( const WwCellGi & g, const float p[3] )
{
	int ix[3];
	for ( int a = 0; a < 3; a++ ) {
		ix[a] = int( std::floor( ( p[a] - g.origin[a] ) / g.voxel ) );
		if ( ix[a] < 0 || ix[a] >= g.dims[a] )
			return -1;
	}
	return ix[0] + g.dims[0] * ( ix[1] + g.dims[1] * ix[2] );
}

//! the +Z slab's luminance at a voxel of a grid in probegi's layout (slot 0)
float upE( const std::vector<float> & grid, size_t nVox, int v )
{
	if ( v < 0 || grid.size() < nVox * 24 )
		return -1.0f;
	const size_t o = ( size_t( 4 ) * nVox + size_t( v ) ) * 4;
	return 0.2126f * grid[o] + 0.7152f * grid[o + 1] + 0.0722f * grid[o + 2];
}

bool gpuReady( Kept & k, QString * why )
{
	if ( !k.gpuTried ) {
		k.gpuTried = true;
		k.gpu = std::make_unique<ProbeRelightGpu>();
		k.gpuOk = k.gpu->init( why ) && k.gpu->upload( *k.ops, why );
		if ( !k.gpuOk )
			k.gpu.reset();
	}
	return k.gpuOk;
}

bool runState( Kept & k, const ProbeRelightState & st, ProbeRelightOut * out, QString * why )
{
	if ( gpuReady( k, why ) )
		return k.gpu->run( st, out, why );
	return probeRelightCpu( *k.ops, st, out, why );	// no GL 4.3: the CPU reference (slow; the census says so)
}

//! relights for the shut fraction and publishes bake + (run - open run)
void relight( Kept & k, float shut )
{
	if ( !k.giKept )
		return;
	if ( req().red == RedStatic ) {	// the bake stays published; the floor point reads it
		if ( k.haveFloor && k.base && k.ops )
			k.floorE = upE( k.base->rgba, size_t( k.ops->nVox ), voxelOf( *k.base, k.floorP ) );
		return;
	}
	if ( req().red == RedSnap )
		shut = shut > 0.5f ? 1.0f : 0.0f;
	if ( k.relitAny && shut == k.relitShut )
		return;
	QElapsedTimer t;
	t.start();
	QString why;
	if ( !k.haveOpen ) {
		ProbeRelightState st;
		st.share = k.share;
		k.haveOpen = runState( k, st, &k.open, &why );
		if ( !k.haveOpen ) {
			k.giKept = false;
			k.giWhy = QStringLiteral( "the open run failed: %1" ).arg( why );
			return;
		}
	}
	WwCellGi gi = *k.base;
	const size_t nVox = size_t( k.ops->nVox );
	if ( shut > 0.0f ) {
		ProbeRelightState st;
		st.share = k.share;
		st.doorShut.assign( k.ops->doorRefs.size(), 0.0f );
		st.doorShut[size_t( k.slot )] = shut;
		ProbeRelightOut o;
		if ( !runState( k, st, &o, &why ) ) {
			k.giKept = false;
			k.giWhy = QStringLiteral( "the relight failed: %1" ).arg( why );
			return;
		}
		auto add = [&]( size_t at, const std::vector<float> & S, const std::vector<float> & O ) {
			for ( size_t i = 0; i + 3 < S.size() && i + 3 < O.size() && at + i + 3 < gi.rgba.size(); i += 4 )
				for ( int c = 0; c < 3; c++ )
					gi.rgba[at + i + size_t( c )] = std::max( 0.0f, gi.rgba[at + i + size_t( c )] + S[i + size_t( c )] - O[i + size_t( c )] );
		};
		add( 0, o.grid, k.open.grid );
		if ( gi.rgba.size() >= nVox * 48 )
			add( nVox * 24, o.grid2, k.open.grid2 );
		/* SKY3: the air volume follows the door as the grid does: each voxel adds its recorded probes' change (shut run
		 * minus open run). It kept the bake's air before. Red WW_CELL_DOORLIVE_AIR_RED=1: the bake's air, as before. */
		k.airMoved = 0;
		const size_t kT = size_t( ProbeRelightOps::kAirTaps );
		const size_t na = size_t( gi.airDims[0] ) * size_t( gi.airDims[1] ) * size_t( gi.airDims[2] );
		const size_t np = size_t( k.ops->probes );
		if ( wwEnv( "WW_CELL_DOORLIVE_AIR_RED" ).trimmed() != "1" && na > 0 && gi.air.size() == na * 24
			&& k.ops->airProbe.size() == na * kT && o.E.size() == np * 18 && k.open.E.size() == np * 18 ) {
			std::vector<quint8> moved( np, 0 );
			for ( size_t p = 0; p < np; p++ )
				for ( int e = 0; e < 18 && !moved[p]; e++ )
					moved[p] = o.E[p * 18 + size_t( e )] != k.open.E[p * 18 + size_t( e )] ? 1 : 0;
			size_t airMoved = 0;
			for ( size_t v = 0; v < na; v++ ) {
				double d[6][3] = {};
				bool any = false;
				for ( size_t t = 0; t < kT; t++ ) {
					const int p = k.ops->airProbe[v * kT + t];
					if ( p < 0 || size_t( p ) >= np || !moved[size_t( p )] )
						continue;
					any = true;
					const double w = k.ops->airW[v * kT + t];
					for ( int a = 0; a < 6; a++ )
						for ( int c = 0; c < 3; c++ )
							d[a][c] += w * ( double( o.E[size_t( p ) * 18 + size_t( a * 3 + c )] ) - k.open.E[size_t( p ) * 18 + size_t( a * 3 + c )] );
				}
				if ( !any )
					continue;
				airMoved++;
				for ( int a = 0; a < 6; a++ )
					for ( int c = 0; c < 3; c++ ) {
						float & A = gi.air[( size_t( a ) * na + v ) * 4 + size_t( c )];
						A = std::max( 0.0f, float( double( A ) + d[a][c] ) );
					}
			}
			k.airMoved = airMoved;
		}
		k.floorE = k.haveFloor ? upE( gi.rgba, nVox, voxelOf( gi, k.floorP ) ) : -1.0f;
	} else {
		k.airMoved = 0;
		k.floorE = k.haveFloor ? upE( gi.rgba, nVox, voxelOf( gi, k.floorP ) ) : -1.0f;	// fully open: the bake exactly
	}
	wwCellGiPublish( k.nif.data(), gi );
	k.relitShut = shut;
	k.relitAny = true;
	k.relights++;
	k.relightMs = double( t.nsecsElapsed() ) / 1.0e6;
}

QString echoOf( const Kept & k )
{
	const char * red = req().red == RedStatic ? "static" : req().red == RedSnap ? "snap" : "none";
	return QStringLiteral( "door live: ref %1 state %2 open %3 relight %4 ms (%5, relights %6) gi %7 shut %8 floor E %9 "
		"| drive %10 seq %11 red %12 | air moved %13" )
		.arg( k.ref, 8, 16, QLatin1Char( '0' ) ).arg( k.state ).arg( double( k.frac ), 0, 'f', 3 )
		.arg( k.relightMs, 0, 'f', 2 ).arg( k.gpuOk ? QStringLiteral( "gpu" ) : k.gpuTried ? QStringLiteral( "cpu" ) : QStringLiteral( "-" ) )
		.arg( k.relights )
		.arg( k.giKept ? QStringLiteral( "follows" ) : QStringLiteral( "none (%1)" ).arg( k.giWhy ) )
		.arg( double( k.relitShut ), 0, 'f', 3 ).arg( double( k.floorE ), 0, 'g', 5 )
		.arg( k.hinge ? QStringLiteral( "hinge fallback (no Open/Close sequence)" ) : QStringLiteral( "sequence" ) )
		.arg( k.hinge ? QStringLiteral( "-" ) : ( k.seqOpen.isEmpty() ? QStringLiteral( "Close reversed" ) : k.seqOpen )
			+ QLatin1Char( '/' ) + ( k.seqClose.isEmpty() ? QStringLiteral( "Open reversed" ) : k.seqClose ) )
		.arg( QLatin1String( red ) ).arg( qint64( k.airMoved ) );
}

//! poses the door at clock s; returns true when the pose (open fraction) changed
bool poseNow( Scene * scene, Kept & k, double s )
{
	if ( !bind( scene, k ) )
		return false;
	const Pose p = poseAtClock( k, s );
	applyPose( scene, k, p );
	const float f = openFraction( scene, k, p );
	const bool moved = f != k.frac || p.state != k.state;
	if ( p.state != k.state )
		fprintf( stderr, "door live: ref %08x %s\n", k.ref, qPrintable( p.state ) );
	k.state = p.state;
	k.frac = f;
	if ( moved && k.nif )
		wwCellShadowKick( k.nif.data() );	// the cell lights' cubes see the door where it is now
	return moved;
}

}	// namespace

quint32 wwDoorLiveRef()
{
	return req().mode != ModeList ? req().ref : 0;
}

bool wwDoorLiveWantsGi()
{
	return req().mode != ModeOff;
}

void wwDoorLiveSetBranch( NifModel * nif, quint32 ref, const QString & model, qint32 wrapper, qint32 root, qint32 count,
	const QString & note )
{
	Kept & k = kept();
	k.nif = nif;
	k.ref = ref;
	k.model = model;
	k.wrapper = wrapper;
	k.root = root;
	k.count = count;
	k.note = note;
	k.place = nif && wrapper >= 0 ? Transform( nif, nif->getBlockIndex( wrapper ) ) : Transform();
	k.boundScene = nullptr;
	k.giKept = false;
	k.giWhy = QStringLiteral( "no GI kept (the GI row off, or no bake)" );
	k.mode = req().mode;
	k.clock.start();
	k.driven = false;
	k.relitAny = false;
	k.relights = 0;
	k.state = QStringLiteral( "closed" );
	k.frac = 0.0f;
}

void wwDoorLiveRemap( const void * nif, const WwBlockMap & m )
{
	Kept & k = kept();
	if ( !nif || k.nif.data() != nif )
		return;
	k.wrapper = k.wrapper >= 0 ? m( k.wrapper ) : -1;
	k.root = k.root >= 0 ? m( k.root ) : -1;
	k.boundScene = nullptr;	// the scene is made again from the renumbered document: bind afresh
}

QString wwDoorLiveKeepGi( const void * nif, const ProbeRelightOps & ops, const WwCellGi & gi,
	const std::vector<WwDoorLiveBox> & boxes )
{
	Kept & k = kept();
	QString notes;
	if ( req().mode == ModeOff )
		return notes;
	auto boxOf = [&]( quint32 ref ) -> const WwDoorLiveBox * {
		for ( const WwDoorLiveBox & b : boxes )
			if ( b.ref == ref )
				return &b;
		return nullptr;
	};
	const size_t nVox = size_t( ops.nVox );
	// the dark side of a door: its two floor points a little past the box on the thin axis
	auto sides = [&]( const WwDoorLiveBox & b, float pts[2][3], int * axis ) {
		const float ex = b.hi[0] - b.lo[0], ey = b.hi[1] - b.lo[1];
		const int a = ex < ey ? 0 : 1;
		*axis = a;
		for ( int s = 0; s < 2; s++ ) {
			for ( int c = 0; c < 3; c++ )
				pts[s][c] = 0.5f * ( b.lo[c] + b.hi[c] );
			pts[s][a] += ( s ? 1.0f : -1.0f ) * ( 0.5f * ( b.hi[a] - b.lo[a] ) + 96.0f );
			pts[s][2] = b.lo[2] + 16.0f;
		}
	};
	if ( req().mode == ModeList ) {
		Kept tmp;
		tmp.ops = std::make_unique<ProbeRelightOps>( ops );
		QString why;
		ProbeRelightOut O;
		if ( !runState( tmp, ProbeRelightState(), &O, &why ) )
			return QStringLiteral( "  door list FAILED: %1\n" ).arg( why );
		double sumO = 0;
		for ( float v : O.B )
			sumO += v;
		struct Row { double drop; QString line; };
		std::vector<Row> rows;
		const int cap = qEnvironmentVariableIsSet( "WW_CELL_DOORLIVE_LIST_MAX" ) ? qEnvironmentVariableIntValue( "WW_CELL_DOORLIVE_LIST_MAX" ) : 32;
		for ( size_t d = 0; d < ops.doorRefs.size() && int( d ) < cap; d++ ) {
			ProbeRelightState st;
			st.doorShut.assign( ops.doorRefs.size(), 0.0f );
			st.doorShut[d] = 1.0f;
			ProbeRelightOut S;
			if ( !runState( tmp, st, &S, &why ) )
				continue;
			double sumS = 0;
			for ( float v : S.B )
				sumS += v;
			const double drop = sumO > 0 ? 1.0 - sumS / sumO : 0.0;
			QString line = QStringLiteral( "door list: ref %1 drop %2%" ).arg( ops.doorRefs[d], 8, 16, QLatin1Char( '0' ) )
				.arg( 100.0 * drop, 0, 'f', 2 );
			if ( const WwDoorLiveBox * b = boxOf( ops.doorRefs[d] ) ) {
				float pts[2][3];
				int a = 0;
				sides( *b, pts, &a );
				float ratio[2];
				for ( int s = 0; s < 2; s++ ) {
					const int v = voxelOf( gi, pts[s] );
					const float eo = upE( O.grid, nVox, v ), es = upE( S.grid, nVox, v );
					ratio[s] = eo > 0 ? es / eo : 1.0f;
				}
				const int dark = ratio[0] <= ratio[1] ? 0 : 1;
				const float sg = dark ? 1.0f : -1.0f;
				float eye[3], at[3];
				for ( int c = 0; c < 3; c++ )
					eye[c] = at[c] = 0.5f * ( b->lo[c] + b->hi[c] );
				eye[a] += sg * ( 0.5f * ( b->hi[a] - b->lo[a] ) + 320.0f );
				eye[2] = b->lo[2] + 120.0f;
				at[a] = pts[dark][a];
				at[2] = b->lo[2];
				line += QStringLiteral( " box %1 %2 %3 .. %4 %5 %6 | dark side %7%8 (floor E closed/open %9 vs %10) | cam %11 %12 %13 %14 %15 %16 70 | floor %17 %18 %19" )
					.arg( double( b->lo[0] ), 0, 'f', 0 ).arg( double( b->lo[1] ), 0, 'f', 0 ).arg( double( b->lo[2] ), 0, 'f', 0 )
					.arg( double( b->hi[0] ), 0, 'f', 0 ).arg( double( b->hi[1] ), 0, 'f', 0 ).arg( double( b->hi[2] ), 0, 'f', 0 )
					.arg( dark ? QLatin1Char( '+' ) : QLatin1Char( '-' ) ).arg( a ? QLatin1Char( 'y' ) : QLatin1Char( 'x' ) )
					.arg( double( ratio[dark] ), 0, 'f', 3 ).arg( double( ratio[1 - dark] ), 0, 'f', 3 )
					.arg( double( eye[0] ), 0, 'f', 1 ).arg( double( eye[1] ), 0, 'f', 1 ).arg( double( eye[2] ), 0, 'f', 1 )
					.arg( double( at[0] ), 0, 'f', 1 ).arg( double( at[1] ), 0, 'f', 1 ).arg( double( at[2] ), 0, 'f', 1 )
					.arg( double( pts[dark][0] ), 0, 'f', 1 ).arg( double( pts[dark][1] ), 0, 'f', 1 ).arg( double( pts[dark][2] ), 0, 'f', 1 );
			}
			rows.push_back( { drop, line } );
		}
		std::sort( rows.begin(), rows.end(), []( const Row & a, const Row & b ) { return a.drop > b.drop; } );
		for ( const Row & r : rows ) {
			notes += QStringLiteral( "  %1\n" ).arg( r.line );
			fprintf( stderr, "%s\n", qPrintable( r.line ) );
		}
		notes += QStringLiteral( "  door list: %1 door slots, %2 ranked (%3)\n" ).arg( ops.doorRefs.size() ).arg( rows.size() )
			.arg( tmp.gpuOk ? QStringLiteral( "gpu" ) : QStringLiteral( "cpu" ) );
		return notes;
	}
	if ( !k.nif || k.nif.data() != nif || k.ref != req().ref ) {
		notes += QStringLiteral( "  door live: ref %1 not copied in (%2); the GI does not follow\n" )
			.arg( req().ref, 8, 16, QLatin1Char( '0' ) ).arg( k.note.isEmpty() ? QStringLiteral( "no placement with that ref" ) : k.note );
		return notes;
	}
	k.slot = -1;
	for ( size_t d = 0; d < ops.doorRefs.size(); d++ )
		if ( ops.doorRefs[d] == k.ref )
			k.slot = int( d );
	if ( k.slot < 0 ) {
		k.giWhy = QStringLiteral( "the ref is no door slot of the relight (not a door base, or no opening tagged)" );
		notes += QStringLiteral( "  door live: %1\n" ).arg( k.giWhy );
		return notes;
	}
	const size_t g24 = nVox * 24;
	if ( gi.rgba.size() != g24 && gi.rgba.size() != 2 * g24 ) {
		k.giWhy = QStringLiteral( "the published grid is not the relight's layout (%1 floats, %2 voxels)" ).arg( gi.rgba.size() ).arg( nVox );
		notes += QStringLiteral( "  door live: %1\n" ).arg( k.giWhy );
		return notes;
	}
	k.ops = std::make_unique<ProbeRelightOps>( ops );
	k.base = std::make_unique<WwCellGi>( gi );
	k.share = wwLookOn( WwLook::ProbeShare );
	k.gpu.reset();
	k.gpuTried = k.gpuOk = false;
	k.haveOpen = false;
	k.giKept = true;
	k.giWhy.clear();
	k.haveFloor = false;
	if ( const WwDoorLiveBox * b = boxOf( k.ref ) ) {
		// the floor patch: on the side darker in the bake (WW_CELL_DOORLIVE_FLOOR=x y z names one instead)
		float pts[2][3];
		int a = 0;
		sides( *b, pts, &a );
		const float e0 = upE( gi.rgba, nVox, voxelOf( gi, pts[0] ) ), e1 = upE( gi.rgba, nVox, voxelOf( gi, pts[1] ) );
		const int dark = e0 <= e1 ? 0 : 1;
		if ( qEnvironmentVariableIsSet( "WW_CELL_DOORLIVE_FLOOR" ) ) {
			const QStringList w = qEnvironmentVariable( "WW_CELL_DOORLIVE_FLOOR" ).split( QLatin1Char( ' ' ), Qt::SkipEmptyParts );
			if ( w.size() == 3 )
				for ( int c = 0; c < 3; c++ )
					k.floorP[c] = w[c].toFloat();
		} else {
			for ( int c = 0; c < 3; c++ )
				k.floorP[c] = pts[dark][c];
		}
		k.haveFloor = true;
	}
	notes += QStringLiteral( "  door live: ref %1 model %2 slot %3 of %4, %5; floor point %6 %7 %8; mode %9\n" )
		.arg( k.ref, 8, 16, QLatin1Char( '0' ) ).arg( k.model ).arg( k.slot ).arg( ops.doorRefs.size() ).arg( k.note )
		.arg( double( k.floorP[0] ), 0, 'f', 1 ).arg( double( k.floorP[1] ), 0, 'f', 1 ).arg( double( k.floorP[2] ), 0, 'f', 1 )
		.arg( k.mode == ModeOpen ? QStringLiteral( "open" ) : k.mode == ModeClose ? QStringLiteral( "close" ) : QStringLiteral( "cycle" ) );
	return notes;
}

void wwDoorLiveTick( Scene * scene, QWindow * view )
{
	Kept & k = kept();
	if ( k.root < 0 || !k.nif || !scene || scene->nifModel != k.nif.data() )
		return;
	k.view = view;
	const double s = k.driven ? k.drivenT : double( k.clock.elapsed() ) / 1000.0;
	poseNow( scene, k, s );
	if ( !k.driven && k.state != QLatin1String( "closed" ) && k.state != QLatin1String( "open" ) && view )
		QTimer::singleShot( 16, view, [view]() { view->requestUpdate(); } );	// the swing plays on its own
	if ( !k.giKept || req().red == RedStatic || k.pending )
		return;
	const float shut = 1.0f - k.frac;
	if ( k.relitAny && shut == k.relitShut )
		return;
	// the relight runs out of the paint (its own GL context), then asks for a frame
	k.pending = true;
	QTimer::singleShot( 0, view, [view]() {
		Kept & kk = kept();
		kk.pending = false;
		relight( kk, 1.0f - kk.frac );
		kk.lastEcho = echoOf( kk );
		if ( view )
			view->requestUpdate();
	} );
}

QString wwDoorLiveStep( Scene * scene, double seconds )
{
	Kept & k = kept();
	if ( k.root < 0 || !k.nif || !scene || scene->nifModel != k.nif.data() )
		return QString();
	k.driven = true;
	k.drivenT = seconds;
	poseNow( scene, k, seconds );
	relight( k, 1.0f - k.frac );
	k.lastEcho = echoOf( k ) + QStringLiteral( " | t %1" ).arg( seconds, 0, 'f', 4 );
	return k.lastEcho;
}

QString wwDoorLiveCommand( Scene * scene, const QString & arg )
{
	Kept & k = kept();
	if ( k.root < 0 || !k.nif )
		return QStringLiteral( "door live: off (WW_CELL_DOOR_ANIM=<ref>:<mode> before the cell opens)" );
	const QString a = arg.trimmed().toLower();
	if ( a == QLatin1String( "open" ) || a == QLatin1String( "close" ) || a == QLatin1String( "cycle" ) ) {
		k.mode = a == QLatin1String( "open" ) ? ModeOpen : a == QLatin1String( "close" ) ? ModeClose : ModeCycle;
		k.driven = false;
		k.clock.restart();
	} else if ( a.startsWith( QLatin1String( "t=" ) ) ) {
		return wwDoorLiveStep( scene, a.mid( 2 ).toDouble() );
	} else if ( a != QLatin1String( "state" ) ) {
		return QStringLiteral( "door live: unknown command %1 (open|close|cycle|t=<s>|state)" ).arg( arg );
	}
	if ( k.view )
		k.view->requestUpdate();
	k.lastEcho = echoOf( k );
	return k.lastEcho;
}

QString wwDoorLiveEcho()
{
	Kept & k = kept();
	if ( k.root < 0 )
		return QString();
	return k.lastEcho.isEmpty() ? echoOf( k ) : k.lastEcho;
}
