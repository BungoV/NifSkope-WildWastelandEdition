#include "physics/physicspreview.h"

#include "model/nifmodel.h"

#include <QHash>

#include <algorithm>
#include <cmath>

/* niftypes carries no quaternion product, and ragdollsim.cpp's copy is in its own
 * translation unit's anonymous namespace. Two dozen lines duplicated rather than
 * promoted to a shared header, because the solver's copy is load-bearing and
 * should not acquire callers that constrain how it may change.
 */
namespace {

inline Quat qMul( const Quat & a, const Quat & b )
{
	return Quat(
		a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3],
		a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2],
		a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1],
		a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0] );
}

inline Quat qNorm( const Quat & q )
{
	const float n = std::sqrt( q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3] );
	if ( !( n > 1.0e-20f ) )
		return Quat( 1, 0, 0, 0 );
	return Quat( q[0] / n, q[1] / n, q[2] / n, q[3] / n );
}

//! rotation of `radians` about `axis`, which need not be normalised
Quat qAxisAngle( const Vector3 & axis, float radians )
{
	const float len = axis.length();
	if ( !( len > 1.0e-12f ) || !( std::fabs( radians ) > 1.0e-12f ) )
		return Quat( 1, 0, 0, 0 );
	const Vector3 n = axis / len;
	const float s = std::sin( 0.5f * radians );
	return Quat( std::cos( 0.5f * radians ), n[0] * s, n[1] * s, n[2] * s );
}

inline Quat qConj( const Quat & q ) { return Quat( q[0], -q[1], -q[2], -q[3] ); }

//! rotate v by q
inline Vector3 qRot( const Quat & q, const Vector3 & v )
{
	const Vector3 u( q[1], q[2], q[3] );
	const Vector3 uv = Vector3::crossproduct( u, v );
	return v + ( uv * q[0] + Vector3::crossproduct( u, uv ) ) * 2.0f;
}

//! nearest multiple of `step`, for angle snapping
inline float snapTo( float v, float step )
{
	return ( step > 0.0f ) ? std::round( v / step ) * step : v;
}

} // namespace

bool PhysicsPreview::start( const NifModel * nif, QString * error, int onlyBlock )
{
	stop();
	if ( !nif ) {
		if ( error )
			*error = QStringLiteral( "No file loaded." );
		return false;
	}

	for ( qint32 b = 0; b < nif->getBlockCount(); b++ ) {
		// a named block means the caller has already chosen; the scan is then only
		// a way of reaching it through the same decode and build path
		if ( onlyBlock >= 0 && b != onlyBlock )
			continue;
		const QModelIndex iSys = nif->getBlockIndex( b );
		if ( !nif->blockInherits( iSys, "bhkPhysicsSystem" )
			&& !nif->blockInherits( iSys, "bhkRagdollSystem" ) )
			continue;
		const HknpSystem sys = hknpDecode( nif->get<QByteArray>( iSys, "Binary Data" ) );
		// a system with no joints is furniture: it would just sit there
		if ( !sys.valid || sys.constraints.isEmpty() )
			continue;

		QString err;
		if ( !m_sim.build( sys, &err ) )
			continue;
		m_system = sys;
		m_systemBlock = int( b );
		m_jointCount = int( sys.constraints.size() );

		/* Drawable geometry per body, taken from the DECODED shapes and shifted
		 * into the frame the solver poses: the solver's x is a centre of mass and
		 * the shape data is in bone space, so subtracting com here means posing is
		 * just x + q*v and the shift cannot be forgotten at draw time.
		 */
		/* Body -> node name, so a drag can say WHICH bone is in hand.
		 *
		 * The binding is the collision object's own: it names a body and targets a
		 * node. Matched by the system block it points at, because a skeleton file
		 * holds several systems and the body indices restart in each.
		 */
		m_bodyNames.assign( m_sim.bodies().size(), QString() );
		m_bodyBlocks.assign( m_sim.bodies().size(), -1 );
		for ( qint32 c = 0; c < nif->getBlockCount(); c++ ) {
			const QModelIndex iObj = nif->getBlockIndex( c );
			if ( !nif->blockInherits( iObj, "bhkNPCollisionObject" ) )
				continue;
			if ( nif->getLink( iObj, "Data" ) != b )
				continue;
			const int body = int( nif->get<quint32>( iObj, "Body ID" ) );
			if ( body < 0 || body >= m_bodyNames.size() )
				continue;
			const qint32 target = nif->getLink( iObj, "Target" );
			const QModelIndex iTarget = nif->getBlockIndex( target );
			if ( iTarget.isValid() ) {
				m_bodyNames[body] = nif->get<QString>( iTarget, "Name" );
				m_bodyBlocks[body] = int( target );
			}
		}

		m_meshes.resize( m_sim.bodies().size() );
		for ( const HknpShape & shp : sys.shapes ) {
			if ( shp.bodyId < 0 || shp.bodyId >= m_meshes.size() || shp.tris.isEmpty() )
				continue;
			BodyMesh & bm = m_meshes[shp.bodyId];
			const int base = int( bm.verts.size() );
			const Vector3 com = m_sim.bodies().at( shp.bodyId ).com;
			for ( const Vector3 & v : shp.verts )
				bm.verts.append( shp.transformed( v ) - com );
			for ( const Triangle & t : shp.tris )
				bm.tris.append( Triangle( quint16( base + t[0] ), quint16( base + t[1] ),
					quint16( base + t[2] ) ) );
		}

		/* A floor, just under the rig.
		 *
		 * Without one the ragdoll free-falls and is out of frame in about a second,
		 * which is not a preview of anything. Placed a little BELOW the lowest point
		 * rather than at it, so it starts clear and falls onto the floor instead of
		 * starting half buried and being shoved out -- the same placement the
		 * headless simulate uses, and for the same reason.
		 */
		m_sim.ground = true;
		m_sim.groundZ = m_sim.lowestPoint() - 0.05f;
		m_defaultGroundZ = m_sim.groundZ;
		setGravityEnabled( m_gravityOn );

		// everything past here is a prop, and clearProps() truncates back to it
		m_rigBodies = int( m_sim.bodies().size() );
		m_active = true;
		m_paused = false;
		if ( error )
			error->clear();
		return true;
	}

	if ( error )
		*error = QStringLiteral( "This file has no jointed collision to simulate." );
	return false;
}

void PhysicsPreview::stop()
{
	m_sim.clearDrag();
	m_sim = RagdollSim();
	m_meshes.clear();
	m_bodyNames.clear();
	m_shots.clear();
	m_system = HknpSystem();
	m_active = false;
	m_paused = false;
	m_jointCount = 0;
	m_systemBlock = -1;
	m_rigBodies = 0;
	m_rotating = false;
	m_recording = false;
	m_frameIndex = -1;
	m_frames.clear();
}

void PhysicsPreview::step( float dt )
{
	if ( !m_active || m_paused || !( dt > 0.0f ) )
		return;
	// clamp the frame BEFORE scaling: a stall (a dialog, a reload) would otherwise
	// hand the solver a half-second step and fire the ragdoll across the scene,
	// and clamping after would let slow motion hide the stall instead of damping it
	const float h = std::min( dt, 1.0f / 30.0f ) * m_timeScale;
	if ( grabbing() ) {
		// the hand's velocity, measured rather than assumed, for Throw
		const Vector3 here = m_sim.toWorld( m_sim.draggedBody(), Vector3() );
		m_sinceGrabMove += h;
		if ( m_sinceGrabMove > 1.0f / 60.0f ) {
			m_grabVelocity = ( here - m_lastGrabTarget ) / m_sinceGrabMove;
			m_lastGrabTarget = here;
			m_sinceGrabMove = 0.0f;
		}
	}
	m_sim.step( h, m_substeps );
	stepShots( h );
	if ( m_recording )
		recordFrame();
}

void PhysicsPreview::recordFrame()
{
	Frame f;
	const QVector<SimBody> & bodies = m_sim.bodies();
	f.x.reserve( bodies.size() );
	f.q.reserve( bodies.size() );
	for ( const SimBody & b : bodies ) {
		f.x.append( b.x );
		f.q.append( b.q );
	}
	m_frames.append( f );
	// a ring in effect, but written as a drop-from-the-front so frameIndex stays
	// an index into what is actually held rather than into a rotating buffer
	while ( m_frames.size() > RECORD_MAX )
		m_frames.removeFirst();
}

void PhysicsPreview::setRecording( bool on )
{
	if ( m_recording == on )
		return;
	m_recording = on;
	if ( on ) {
		// start from the pose on screen, so frame 0 is what you were looking at
		// when you pressed record rather than one step after it
		m_frames.clear();
		m_frameIndex = -1;
		recordFrame();
	}
}

void PhysicsPreview::clearRecording()
{
	m_frames.clear();
	m_frameIndex = -1;
}

void PhysicsPreview::seek( int frame )
{
	if ( m_frames.isEmpty() )
		return;
	m_frameIndex = std::clamp( frame, 0, int( m_frames.size() ) - 1 );
	const Frame & f = m_frames.at( m_frameIndex );
	QVector<SimBody> & bodies = m_sim.bodies();
	for ( int i = 0; i < bodies.size() && i < f.x.size(); i++ ) {
		bodies[i].x = f.x.at( i );
		bodies[i].q = f.q.at( i );
		// a scrubbed pose is a still: leaving the velocities would fling the rig
		// the instant it was unpaused, from a frame it had already left
		bodies[i].v = Vector3();
		bodies[i].w = Vector3();
	}
	// running on from here would overwrite the recording from this point, which
	// is not what scrubbing back to look at something means
	m_paused = true;
}

void PhysicsPreview::reset()
{
	if ( !m_active )
		return;
	const bool wasPaused = m_paused;
	const bool ground = m_sim.ground, selfColl = m_sim.selfCollision, angLimits = m_sim.angularLimits;
	const float groundZ = m_sim.groundZ, grip = m_sim.groundFriction;
	const Vector3 w = m_sim.wind;
	const float fric = m_sim.frictionScale, damp = m_sim.damping;
	const float rest = m_sim.restitutionScale;
	const int iters = m_sim.iterations;
	const bool noColl = m_sim.dragNoCollide;
	m_sim.clearDrag();
	m_rotating = false;
	/* Props and the recording both go. A prop is a body index into a solver that
	 * is about to be rebuilt, and a recording is frames of a run that no longer
	 * happened -- keeping either would leave them referring to bodies that have
	 * been replaced underneath them.
	 */
	m_meshes.resize( std::min( int( m_meshes.size() ), m_rigBodies ) );
	m_bodyNames.resize( std::min( int( m_bodyNames.size() ), m_rigBodies ) );
	clearRecording();
	m_shots.clear();
	QString err;
	m_sim.build( m_system, &err );
	/* build() resets the whole solver, so every option has to be put back. This
	 * is the reason they forward to the solver rather than being duplicated here:
	 * the ones stored on this object survive a rebuild and are simply reapplied.
	 */
	m_sim.ground = ground;
	m_sim.groundZ = groundZ;
	m_sim.selfCollision = selfColl;
	m_sim.angularLimits = angLimits;
	m_sim.wind = w;
	m_sim.groundFriction = grip;
	m_sim.frictionScale = fric;
	m_sim.damping = damp;
	m_sim.restitutionScale = rest;
	m_sim.iterations = iters;
	m_sim.dragNoCollide = noColl;
	setGravityEnabled( m_gravityOn );
	m_paused = wasPaused;
}

bool PhysicsPreview::grab( const Vector3 & rayOrigin, const Vector3 & rayDir )
{
	if ( !m_active )
		return false;
	const SimPick p = pick( rayOrigin, rayDir );
	if ( !p.hit() )
		return false;
	m_grabDepth = p.distance;
	m_lastRayOrigin = rayOrigin;
	m_lastRayDir = rayDir;
	m_rotating = false;
	m_rotYaw = m_rotPitch = m_rotRoll = 0.0f;
	m_sim.dragStrength = m_settings.grabStrength;
	m_sim.setDrag( p.body, p.localPoint, p.worldPoint, m_settings.grabFirmness );
	return true;
}

void PhysicsPreview::dragTo( const Vector3 & rayOrigin, const Vector3 & rayDir )
{
	if ( !m_active || !grabbing() )
		return;
	const float len = rayDir.length();
	if ( !( len > 1.0e-12f ) )
		return;
	m_lastRayOrigin = rayOrigin;
	m_lastRayDir = rayDir;
	m_sim.moveDrag( rayOrigin / SCALE + ( rayDir / len ) * m_grabDepth );
}

void PhysicsPreview::adjustGrabDepth( float notches, bool fine )
{
	if ( !m_active || !grabbing() || !( std::fabs( notches ) > 1.0e-6f ) )
		return;
	const float rate = fine ? 1.02f : 1.12f;
	// bounded well inside the float range: an unbounded reel-out would push the
	// target far enough that the drag direction loses all its precision
	m_grabDepth = std::clamp( m_grabDepth * std::pow( rate, notches ), 0.02f, 2000.0f );
	// re-aim along the ray we already have, so the wheel works with the mouse
	// standing still -- otherwise nothing happens until the cursor twitches
	const float len = m_lastRayDir.length();
	if ( len > 1.0e-12f )
		m_sim.moveDrag( m_lastRayOrigin / SCALE + ( m_lastRayDir / len ) * m_grabDepth );
}

void PhysicsPreview::beginGrabRotate( const Vector3 & right, const Vector3 & up, const Vector3 & fwd )
{
	const int b = m_sim.draggedBody();
	if ( !m_active || b < 0 )
		return;
	m_rotating = true;
	m_rotRight = right;
	m_rotUp = up;
	m_rotFwd = fwd;
	m_rotYaw = m_rotPitch = m_rotRoll = 0.0f;
	// measured FROM the pose it is in, so starting a rotate never jumps
	m_rotBase = m_sim.bodies().at( b ).q;
	m_sim.setDragOrientation( m_rotBase );
}

void PhysicsPreview::addGrabRotate( float dYaw, float dPitch, float dRoll, bool snap )
{
	if ( !m_active || !m_rotating || m_sim.draggedBody() < 0 )
		return;
	m_rotYaw += dYaw;
	m_rotPitch += dPitch;
	m_rotRoll += dRoll;

	/* Snapping quantises the ACCUMULATED angle, not each increment.
	 *
	 * Rounding every mouse delta would round a stream of half-degree moves to
	 * zero and the bone would never turn at all; rounding the total means the
	 * hand tracks continuously and the result lands on a multiple of 15.
	 */
	const float yaw = snap ? snapTo( m_rotYaw, ROTATE_SNAP ) : m_rotYaw;
	const float pitch = snap ? snapTo( m_rotPitch, ROTATE_SNAP ) : m_rotPitch;
	const float roll = snap ? snapTo( m_rotRoll, ROTATE_SNAP ) : m_rotRoll;

	Quat q = qMul( qAxisAngle( m_rotRight, pitch ), m_rotBase );
	q = qMul( qAxisAngle( m_rotUp, yaw ), q );
	q = qMul( qAxisAngle( m_rotFwd, roll ), q );
	m_sim.setDragOrientation( qNorm( q ) );
}

void PhysicsPreview::clearGrabRotate()
{
	m_rotating = false;
	m_rotYaw = m_rotPitch = m_rotRoll = 0.0f;
	m_sim.clearDragOrientation();
}

bool PhysicsPreview::grabBeam( Vector3 & gripPoint, Vector3 & handPoint ) const
{
	const int b = m_sim.draggedBody();
	if ( !m_active || b < 0 )
		return false;
	gripPoint = m_sim.toWorld( b, m_sim.dragLocal() ) * SCALE;
	handPoint = m_sim.dragTarget() * SCALE;
	return true;
}

bool PhysicsPreview::bodyDelta( int body, Quat & rotation, Vector3 & translation ) const
{
	if ( !m_active || body < 0 || body >= m_sim.bodies().size() )
		return false;
	const SimBody & b = m_sim.bodies().at( body );
	rotation = qNorm( qMul( b.q, qConj( b.restOrient ) ) );
	/* From BONE ORIGIN to bone origin, not centre of mass to centre of mass.
	 *
	 * x is a centre of mass and restOrigin is a bone origin, so differencing them
	 * directly would fold the com offset into the translation and shift every
	 * captured node by most of a limb's length.
	 */
	const Vector3 origin = b.x - qRot( b.q, b.com );
	translation = ( origin - qRot( rotation, b.restOrigin ) ) * SCALE;
	return true;
}

int PhysicsPreview::pinnedCount() const
{
	int n = 0;
	for ( const SimBody & b : m_sim.bodies() )
		if ( b.pinned )
			n++;
	return n;
}

QVector<Vector3> PhysicsPreview::soup() const
{
	QVector<Vector3> out;
	if ( !m_active )
		return out;
	const QVector<SimBody> & bodies = m_sim.bodies();
	for ( int i = 0; i < m_meshes.size() && i < bodies.size(); i++ ) {
		const BodyMesh & bm = m_meshes.at( i );
		const SimBody & b = bodies.at( i );
		if ( bm.tris.isEmpty() )
			continue;
		QVector<Vector3> posed;
		posed.reserve( bm.verts.size() );
		for ( const Vector3 & v : bm.verts )
			posed.append( m_sim.toWorld( i, v ) * SCALE );
		for ( const Triangle & t : bm.tris ) {
			out.append( posed.at( t[0] ) );
			out.append( posed.at( t[1] ) );
			out.append( posed.at( t[2] ) );
		}
	}
	return out;
}

bool PhysicsPreview::togglePin( const Vector3 & rayOrigin, const Vector3 & rayDir )
{
	if ( !m_active )
		return false;

	/* While something is held, the pin applies to THAT body, wherever the cursor
	 * is pointing.
	 *
	 * Freezing mid-drag is how you say "leave this one here and let me move the
	 * next": the hand is already on the bone you mean, and asking the ray to
	 * agree would freeze whatever happened to be under the cursor -- usually a
	 * neighbour, since a dragged limb rarely stays under the pointer.
	 */
	if ( m_sim.draggedBody() >= 0 ) {
		/* The grab is KEPT, so this is a toggle you can work while holding: freeze
		 * to leave a bone where it is, right-click again and the drag picks it
		 * straight back up. Dropping the grab on freeze meant the second click had
		 * nothing to unfreeze and had to hunt for the bone with the cursor, which
		 * is the aiming problem this exists to avoid.
		 *
		 * A pinned body ignores the drag, so holding one that is frozen simply does
		 * nothing until it is let go again.
		 */
		const int b = m_sim.draggedBody();
		m_sim.setPinned( b, !m_sim.bodies().at( b ).pinned );
		return true;
	}

	const float len = rayDir.length();
	if ( !( len > 1.0e-12f ) )
		return false;
	const SimPick p = pick( rayOrigin, rayDir );
	if ( !p.hit() )
		return false;
	m_sim.setPinned( p.body, !m_sim.bodies().at( p.body ).pinned );
	return true;
}

QVector<Vector3> PhysicsPreview::pinnedSoup() const
{
	QVector<Vector3> out;
	if ( !m_active )
		return out;
	const QVector<SimBody> & bodies = m_sim.bodies();
	for ( int i = 0; i < m_meshes.size() && i < bodies.size(); i++ ) {
		if ( !bodies.at( i ).pinned )
			continue;
		const BodyMesh & bm = m_meshes.at( i );
		QVector<Vector3> posed;
		posed.reserve( bm.verts.size() );
		for ( const Vector3 & v : bm.verts )
			posed.append( m_sim.toWorld( i, v ) * SCALE );
		for ( const Triangle & t : bm.tris ) {
			out.append( posed.at( t[0] ) );
			out.append( posed.at( t[1] ) );
			out.append( posed.at( t[2] ) );
		}
	}
	return out;
}

SimPick PhysicsPreview::pick( const Vector3 & rayOrigin, const Vector3 & rayDir ) const
{
	SimPick best;
	if ( !m_active )
		return best;
	const float dl = rayDir.length();
	if ( !( dl > 1.0e-12f ) )
		return best;
	const Vector3 d = rayDir / dl;
	const Vector3 o = rayOrigin / SCALE;   // the meshes are posed in metres

	// Moller-Trumbore against every body's posed triangles
	const QVector<SimBody> & bodies = m_sim.bodies();
	bool anyGeometry = false;
	for ( int i = 0; i < m_meshes.size() && i < bodies.size(); i++ ) {
		const BodyMesh & bm = m_meshes.at( i );
		if ( bm.tris.isEmpty() )
			continue;
		anyGeometry = true;
		QVector<Vector3> posed;
		posed.reserve( bm.verts.size() );
		for ( const Vector3 & v : bm.verts )
			posed.append( m_sim.toWorld( i, v ) );
		for ( const Triangle & t : bm.tris ) {
			const Vector3 & a = posed.at( t[0] );
			const Vector3 e1 = posed.at( t[1] ) - a, e2 = posed.at( t[2] ) - a;
			const Vector3 pv = Vector3::crossproduct( d, e2 );
			const float det = Vector3::dotproduct( e1, pv );
			// two-sided: collision hulls are drawn from both faces and a click from
			// inside one must still land on it
			if ( std::fabs( det ) < 1.0e-12f )
				continue;
			const float inv = 1.0f / det;
			const Vector3 tv = o - a;
			const float u = Vector3::dotproduct( tv, pv ) * inv;
			if ( u < 0.0f || u > 1.0f )
				continue;
			const Vector3 qv = Vector3::crossproduct( tv, e1 );
			const float v = Vector3::dotproduct( d, qv ) * inv;
			if ( v < 0.0f || u + v > 1.0f )
				continue;
			const float dist = Vector3::dotproduct( e2, qv ) * inv;
			if ( dist < 0.0f || ( best.body >= 0 && dist >= best.distance ) )
				continue;
			best.body = i;
			best.distance = dist;
			best.worldPoint = o + d * dist;
			best.localPoint = m_sim.toLocal( i, best.worldPoint );
		}
	}
	// a rig whose shapes all decoded to nothing drawable still has to be usable
	if ( !best.hit() && !anyGeometry )
		return m_sim.pick( o, d );
	return best;
}

QVector<Vector3> PhysicsPreview::bodySoup( int body ) const
{
	QVector<Vector3> out;
	if ( !m_active || body < 0 || body >= m_meshes.size() )
		return out;
	const BodyMesh & bm = m_meshes.at( body );
	QVector<Vector3> posed;
	posed.reserve( bm.verts.size() );
	for ( const Vector3 & v : bm.verts )
		posed.append( m_sim.toWorld( body, v ) * SCALE );
	for ( const Triangle & t : bm.tris ) {
		out.append( posed.at( t[0] ) );
		out.append( posed.at( t[1] ) );
		out.append( posed.at( t[2] ) );
	}
	return out;
}

QVector<Vector3> PhysicsPreview::grabbedSoup() const
{
	QVector<Vector3> out;
	const int b = m_sim.draggedBody();
	if ( !m_active || b < 0 || b >= m_meshes.size() )
		return out;
	const BodyMesh & bm = m_meshes.at( b );
	QVector<Vector3> posed;
	posed.reserve( bm.verts.size() );
	for ( const Vector3 & v : bm.verts )
		posed.append( m_sim.toWorld( b, v ) * SCALE );
	for ( const Triangle & t : bm.tris ) {
		out.append( posed.at( t[0] ) );
		out.append( posed.at( t[1] ) );
		out.append( posed.at( t[2] ) );
	}
	return out;
}

QVector<Vector3> PhysicsPreview::limitSoup() const
{
	QVector<Vector3> out;
	if ( !m_active || !m_highlightLimits )
		return out;
	const QSet<int> bad = bodiesViolatingLimits();
	if ( bad.isEmpty() )
		return out;
	const QVector<SimBody> & bodies = m_sim.bodies();
	for ( int i = 0; i < m_meshes.size() && i < bodies.size(); i++ ) {
		if ( !bad.contains( i ) )
			continue;
		const BodyMesh & bm = m_meshes.at( i );
		QVector<Vector3> posed;
		posed.reserve( bm.verts.size() );
		for ( const Vector3 & v : bm.verts )
			posed.append( m_sim.toWorld( i, v ) * SCALE );
		for ( const Triangle & t : bm.tris ) {
			out.append( posed.at( t[0] ) );
			out.append( posed.at( t[1] ) );
			out.append( posed.at( t[2] ) );
		}
	}
	return out;
}

QString PhysicsPreview::toolName( Tool t )
{
	switch ( t ) {
	case Tool::Grab:  return QStringLiteral( "Grab" );
	case Tool::Shoot: return QStringLiteral( "Shoot" );
	case Tool::Blast: return QStringLiteral( "Blast" );
	case Tool::Wind:  return QStringLiteral( "Wind" );
	case Tool::Punt:  return QStringLiteral( "Punt" );
	case Tool::Prop:  return QStringLiteral( "Ball" );
	}
	return QString();
}

void PhysicsPreview::setTool( Tool t )
{
	if ( m_tool == t )
		return;
	// let go of whatever the old tool was holding, or a grab made with Drag
	// would still be attached after switching to Shoot
	release();
	m_tool = t;
}

void PhysicsPreview::setGravityEnabled( bool on )
{
	m_gravityOn = on;
	m_sim.gravity = on ? m_gravityDir * m_gravityG : Vector3();
}

void PhysicsPreview::setGravityStrength( float g )
{
	m_gravityG = std::max( 0.0f, g );
	setGravityEnabled( m_gravityOn );
}

void PhysicsPreview::setGroundEnabled( bool on )
{
	m_sim.ground = on;
}

QSet<int> PhysicsPreview::bodiesViolatingLimits() const
{
	QSet<int> out;
	if ( !m_active )
		return out;
	for ( const SimLimitCheck & c : m_sim.checkLimits() ) {
		if ( !c.any() )
			continue;
		// the CHILD is the body that has moved out of range; highlighting the
		// parent too would light up the whole torso from one bad wrist
		if ( c.child >= 0 )
			out.insert( c.child );
	}
	return out;
}

/*! Press with the active tool.
 *
 * Every tool starts from the same ray cast, so a miss behaves identically
 * everywhere: nothing is consumed and the viewport keeps its usual click.
 */
bool PhysicsPreview::press( const Vector3 & rayOrigin, const Vector3 & rayDir )
{
	if ( !m_active )
		return false;
	const float len = rayDir.length();
	if ( !( len > 1.0e-12f ) )
		return false;
	const Vector3 dir = rayDir / len;

	switch ( m_tool ) {
	case Tool::Grab:
		if ( !grab( rayOrigin, rayDir ) )
			return false;
		m_lastGrabTarget = m_sim.toWorld( m_sim.draggedBody(), Vector3() );
		m_grabVelocity = Vector3();
		m_sinceGrabMove = 0.0f;
		return true;

	case Tool::Shoot: {
		const SimPick p = pick( rayOrigin, dir );
		// a projectile can be fired into thin air; a hitscan shot cannot, since
		// there is nothing for it to have hit
		if ( !p.hit() && !m_settings.shootProjectile )
			return false;
		/* 12 kg m/s along the ray: about what a heavy pistol round carries, and
		 * enough to spin a limb without launching a brahmin across the scene.
		 * Off-centre by construction -- the impulse lands where the ray hit, not
		 * at the centre of mass, which is what makes a shot to a leg twist it.
		 */
		if ( m_settings.shootProjectile ) {
			/* A real round: it travels, drops, and can miss. Started slightly along
			 * the ray so it does not begin inside the camera, and aimed at where the
			 * ray hit so a click still goes where it was pointed.
			 */
			Shot shot;
			shot.pos = rayOrigin / SCALE + dir * 0.05f;
			shot.vel = dir * m_settings.projectileSpeed;
			shot.radius = m_settings.projectileRadius;
			shot.mass = m_settings.projectileMass;
			shot.gravity = m_settings.projectileGravity;
			shot.flying = true;
			shot.from = shot.pos * SCALE;
			shot.to = shot.from;
			m_shots.append( shot );
			return true;
		}
		m_sim.applyImpulse( p.body, p.localPoint, dir * m_settings.shootImpulse );
		// a visible trace, because otherwise the only evidence of a shot is the
		// ragdoll twitching -- which cannot tell a miss from a broken tool
		Shot trace;
		trace.from = rayOrigin;
		trace.to = p.worldPoint * SCALE;
		trace.hit = true;
		trace.radius = m_settings.projectileRadius;
		m_shots.append( trace );
		return true;
	}

	case Tool::Blast: {
		/* Centred where the ray hits something, or on the rig if it hits nothing
		 * -- a blast that did nothing because the aim was slightly off would just
		 * look broken. 2 m radius, 30 kg m/s at the centre.
		 */
		const SimPick p = pick( rayOrigin, dir );
		if ( !p.hit() )
			return false;
		m_sim.blast( p.worldPoint, m_settings.blastRadius, m_settings.blastStrength );
		return true;
	}

	case Tool::Wind:
		m_holding = true;
		m_holdDir = dir;
		m_sim.wind = dir * m_settings.windStrength;
		return true;

	case Tool::Punt: {
		/* Along the VIEW, not along the surface normal.
		 *
		 * A punt is aimed: it should send a body where the camera is pointing,
		 * which is the whole difference between this and Shoot. Applied at the hit
		 * point rather than the centre of mass, so punting a foot spins the rig
		 * and punting a chest sends it straight -- and pulling reverses only the
		 * direction, so a yank on a foot spins it the other way.
		 */
		const SimPick p = pick( rayOrigin, dir );
		if ( !p.hit() )
			return false;
		const Vector3 push = dir * ( m_settings.puntPull ? -m_settings.puntStrength
			: m_settings.puntStrength );
		m_sim.shove( p.body, p.localPoint, push );
		Shot trace;
		trace.from = rayOrigin;
		trace.to = p.worldPoint * SCALE;
		trace.hit = true;
		trace.radius = m_settings.projectileRadius;
		m_shots.append( trace );
		return true;
	}

	case Tool::Prop:
		return spawnProp( rayOrigin, dir ) >= 0;
	}
	return false;
}

bool PhysicsPreview::move( const Vector3 & rayOrigin, const Vector3 & rayDir )
{
	if ( !m_active )
		return false;
	if ( m_tool == Tool::Wind && m_holding ) {
		const float len = rayDir.length();
		if ( len > 1.0e-12f ) {
			m_holdDir = rayDir / len;
			m_sim.wind = m_holdDir * m_settings.windStrength;
		}
		return true;
	}
	if ( !grabbing() )
		return false;
	dragTo( rayOrigin, rayDir );
	return true;
}

bool PhysicsPreview::release()
{
	if ( m_holding ) {
		m_holding = false;
		m_sim.wind = Vector3();
		return true;
	}
	if ( !grabbing() )
		return false;
	/* Let go carrying the hand's velocity, which makes a release while moving a
	 * throw and a release while still a drop -- no mode to choose in advance.
	 *
	 * The solver derives velocity from the position change each substep, so a
	 * dragged body is ALREADY moving at roughly the hand's speed; but letting go
	 * simply stops driving it, and the constraint solve bleeds that off within a
	 * frame or two. Setting it explicitly is what makes a throw carry.
	 */
	if ( m_sim.draggedBody() >= 0 )
		m_sim.setVelocity( m_sim.draggedBody(), m_grabVelocity );
	m_sim.clearDrag();
	m_rotating = false;
	return true;
}

/*! Drop a ball into the scene, along the ray and travelling.
 *
 * Placed where the ray HITS if it hits anything, backed off by its own radius so
 * it starts touching rather than embedded -- a prop spawned inside a limb is a
 * penetration the solver has to resolve, and it resolves it by firing the pair
 * apart. On a miss it goes at the depth the last grab used, or a sensible arm's
 * length, so clicking at the sky still produces something you can see.
 */
int PhysicsPreview::spawnProp( const Vector3 & rayOrigin, const Vector3 & rayDir )
{
	if ( !m_active )
		return -1;
	const float len = rayDir.length();
	if ( !( len > 1.0e-12f ) )
		return -1;
	const Vector3 dir = rayDir / len;
	const float r = std::max( 0.005f, m_settings.propRadius );

	const SimPick p = pick( rayOrigin, dir );
	const float reach = p.hit() ? std::max( 0.05f, p.distance - r * 1.5f )
		: ( m_grabDepth > 0.01f ? m_grabDepth : 2.0f );
	const Vector3 pos = rayOrigin / SCALE + dir * reach;

	const int body = m_sim.addProp( pos, dir * std::max( 0.0f, m_settings.propSpeed ),
		r, std::max( 0.001f, m_settings.propMass ) );
	if ( body < 0 )
		return -1;

	/* Give it geometry in the same list every decoded body uses, and drawing,
	 * picking, grabbing and pinning all work on it with no special cases -- which
	 * is what makes a thrown ball something you can then catch.
	 */
	if ( m_meshes.size() < body + 1 )
		m_meshes.resize( body + 1 );
	buildBallMesh( m_meshes[body], r );
	if ( m_bodyNames.size() < body + 1 )
		m_bodyNames.resize( body + 1 );
	m_bodyNames[body] = QStringLiteral( "Ball %1" ).arg( body - m_rigBodies + 1 );
	return body;
}

void PhysicsPreview::clearProps()
{
	if ( !m_active )
		return;
	m_sim.clearProps();
	m_meshes.resize( std::min( int( m_meshes.size() ), m_rigBodies ) );
	m_bodyNames.resize( std::min( int( m_bodyNames.size() ), m_rigBodies ) );
	// frames recorded with props in them describe more bodies than now exist;
	// seek() tolerates that, but the extra columns are meaningless
	clearRecording();
}

int PhysicsPreview::propCount() const
{
	return std::max( 0, int( m_sim.bodies().size() ) - m_rigBodies );
}

/*! A unit icosahedron subdivided twice: 320 triangles, round enough at any size.
 *
 * Generated rather than approximated with a lat/long sphere because a lat/long
 * one bunches its triangles at the poles, and a ball that is visibly faceted at
 * its equator and dense at its top reads as a modelling error rather than as a
 * primitive.
 */
void PhysicsPreview::buildBallMesh( BodyMesh & bm, float radius )
{
	bm.verts.clear();
	bm.tris.clear();

	const float t = 0.5f * ( 1.0f + std::sqrt( 5.0f ) );
	QVector<Vector3> v = {
		{ -1,  t,  0 }, {  1,  t,  0 }, { -1, -t,  0 }, {  1, -t,  0 },
		{  0, -1,  t }, {  0,  1,  t }, {  0, -1, -t }, {  0,  1, -t },
		{  t,  0, -1 }, {  t,  0,  1 }, { -t,  0, -1 }, { -t,  0,  1 } };
	QVector<Triangle> f = {
		{ 0, 11, 5 }, { 0, 5, 1 }, { 0, 1, 7 }, { 0, 7, 10 }, { 0, 10, 11 },
		{ 1, 5, 9 }, { 5, 11, 4 }, { 11, 10, 2 }, { 10, 7, 6 }, { 7, 1, 8 },
		{ 3, 9, 4 }, { 3, 4, 2 }, { 3, 2, 6 }, { 3, 6, 8 }, { 3, 8, 9 },
		{ 4, 9, 5 }, { 2, 4, 11 }, { 6, 2, 10 }, { 8, 6, 7 }, { 9, 8, 1 } };

	for ( int pass = 0; pass < 2; pass++ ) {
		QVector<Triangle> next;
		QHash<quint32, quint16> mid;    // edge key -> the vertex splitting it
		auto midpoint = [&]( quint16 a, quint16 b ) {
			const quint32 key = ( quint32( std::min( a, b ) ) << 16 ) | quint32( std::max( a, b ) );
			const auto it = mid.constFind( key );
			if ( it != mid.constEnd() )
				return it.value();
			const Vector3 m = ( v.at( a ) + v.at( b ) ) * 0.5f;
			v.append( m );
			const quint16 idx = quint16( v.size() - 1 );
			mid.insert( key, idx );
			return idx;
		};
		for ( const Triangle & tr : std::as_const( f ) ) {
			const quint16 a = midpoint( tr[0], tr[1] );
			const quint16 b = midpoint( tr[1], tr[2] );
			const quint16 c = midpoint( tr[2], tr[0] );
			next << Triangle( tr[0], a, c ) << Triangle( tr[1], b, a )
				<< Triangle( tr[2], c, b ) << Triangle( a, b, c );
		}
		f = next;
	}

	bm.verts.reserve( v.size() );
	for ( const Vector3 & p : std::as_const( v ) ) {
		const float l = p.length();
		bm.verts.append( ( l > 1.0e-9f ) ? p * ( radius / l ) : Vector3() );
	}
	bm.tris = f;
}

void PhysicsPreview::setGravityDirection( const Vector3 & d )
{
	const float len = d.length();
	// a zero direction has no meaning; keep the last good one rather than
	// silently turning gravity off through the back door
	if ( len > 1.0e-6f )
		m_gravityDir = d / len;
	setGravityEnabled( m_gravityOn );
}

/*! Advance projectiles and age the traces behind them.
 *
 * Collision is a swept test, not a point test: a 60 m/s round covers 1 m in a
 * 60th of a second, which is most of a limb, so checking only where it ends up
 * would let it pass through a brahmin without touching it. The sweep reuses the
 * picker -- the same code the mouse tools aim with -- so a shot can only hit
 * what a click could have hit.
 */
void PhysicsPreview::stepShots( float h )
{
	for ( int i = m_shots.size() - 1; i >= 0; i-- ) {
		Shot & s = m_shots[i];
		if ( !s.flying ) {
			s.age += h;
			if ( s.age > TRACE_FADE )
				m_shots.remove( i );
			continue;
		}

		if ( s.gravity )
			s.vel += m_sim.gravity * h;
		const Vector3 next = s.pos + s.vel * h;
		const Vector3 delta = next - s.pos;
		const float travelled = delta.length();

		const SimPick p = ( travelled > 1.0e-9f ) ? m_sim.pick( s.pos, delta ) : SimPick();
		if ( p.hit() && p.distance <= travelled ) {
			// momentum, so a heavy slow round and a light fast one differ
			m_sim.applyImpulse( p.body, p.localPoint, ( delta / travelled ) * ( s.mass * s.vel.length() ) );
			s.to = p.worldPoint * SCALE;
			s.pos = p.worldPoint;
			s.flying = false;
			s.hit = true;
			s.age = 0.0f;
			continue;
		}

		s.pos = next;
		s.to = s.pos * SCALE;
		// a round that leaves the neighbourhood is gone: without this they
		// accumulate for as long as the mode is open
		if ( m_sim.ground && s.pos[2] < m_sim.groundZ - 1.0f ) {
			s.flying = false;
			s.age = 0.0f;
		}
	}
}

QVector<Vector3> PhysicsPreview::groundSoup() const
{
	QVector<Vector3> out;
	if ( !m_active || !m_groundVisible || !m_sim.ground )
		return out;

	/* Sized from the rig's own footprint, so the surface reads as a floor under
	 * THIS thing rather than as an arbitrary plane -- a fixed size is a postage
	 * stamp under Liberty Prime and a runway under a cat.
	 */
	Vector3 mn, mx;
	bool first = true;
	for ( const SimBody & b : m_sim.bodies() ) {
		for ( int a = 0; a < 3; a++ ) {
			if ( first ) {
				mn = mx = b.x;
			} else {
				mn[a] = std::min( mn[a], b.x[a] );
				mx[a] = std::max( mx[a], b.x[a] );
			}
		}
		first = false;
	}
	if ( first )
		return out;

	const Vector3 mid = ( mn + mx ) * 0.5f;
	float half = 0.0f;
	for ( int a = 0; a < 2; a++ )
		half = std::max( half, ( mx[a] - mn[a] ) * 0.5f );
	half = std::max( half * 3.0f, 1.0f ) * SCALE;

	const float z = m_sim.groundZ * SCALE;
	const float cx = mid[0] * SCALE, cy = mid[1] * SCALE;
	const Vector3 a( cx - half, cy - half, z ), b( cx + half, cy - half, z );
	const Vector3 c( cx + half, cy + half, z ), d( cx - half, cy + half, z );
	out << a << b << c << a << c << d;
	return out;
}
