#include "physics/physicspreview.h"

#include "model/nifmodel.h"

#include <algorithm>

bool PhysicsPreview::start( const NifModel * nif, QString * error )
{
	stop();
	if ( !nif ) {
		if ( error )
			*error = QStringLiteral( "No file loaded." );
		return false;
	}

	for ( qint32 b = 0; b < nif->getBlockCount(); b++ ) {
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
		m_jointCount = int( sys.constraints.size() );

		/* Drawable geometry per body, taken from the DECODED shapes and shifted
		 * into the frame the solver poses: the solver's x is a centre of mass and
		 * the shape data is in bone space, so subtracting com here means posing is
		 * just x + q*v and the shift cannot be forgotten at draw time.
		 */
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
	m_system = HknpSystem();
	m_active = false;
	m_paused = false;
	m_jointCount = 0;
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
	m_sim.step( h );
}

void PhysicsPreview::reset()
{
	if ( !m_active )
		return;
	const bool wasPaused = m_paused;
	const bool ground = m_sim.ground, selfColl = m_sim.selfCollision, angLimits = m_sim.angularLimits;
	const float groundZ = m_sim.groundZ;
	const Vector3 w = m_sim.wind;
	m_sim.clearDrag();
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
	setGravityEnabled( m_gravityOn );
	m_paused = wasPaused;
}

bool PhysicsPreview::grab( const Vector3 & rayOrigin, const Vector3 & rayDir )
{
	if ( !m_active )
		return false;
	const SimPick p = m_sim.pick( rayOrigin / SCALE, rayDir );
	if ( !p.hit() )
		return false;
	m_grabDepth = p.distance;
	m_sim.setDrag( p.body, p.localPoint, p.worldPoint );
	return true;
}

void PhysicsPreview::dragTo( const Vector3 & rayOrigin, const Vector3 & rayDir )
{
	if ( !m_active || !grabbing() )
		return;
	const float len = rayDir.length();
	if ( !( len > 1.0e-12f ) )
		return;
	m_sim.moveDrag( rayOrigin / SCALE + ( rayDir / len ) * m_grabDepth );
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

QString PhysicsPreview::toolName( Tool t )
{
	switch ( t ) {
	case Tool::Drag:  return QStringLiteral( "Drag" );
	case Tool::Throw: return QStringLiteral( "Throw" );
	case Tool::Shoot: return QStringLiteral( "Shoot" );
	case Tool::Pin:   return QStringLiteral( "Pin" );
	case Tool::Blast: return QStringLiteral( "Blast" );
	case Tool::Wind:  return QStringLiteral( "Wind" );
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
	m_sim.gravity = Vector3( 0.0f, 0.0f, on ? -m_gravityG : 0.0f );
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
	case Tool::Drag:
	case Tool::Throw:
		if ( !grab( rayOrigin, rayDir ) )
			return false;
		m_lastGrabTarget = m_sim.toWorld( m_sim.draggedBody(), Vector3() );
		m_grabVelocity = Vector3();
		m_sinceGrabMove = 0.0f;
		return true;

	case Tool::Shoot: {
		const SimPick p = m_sim.pick( rayOrigin / SCALE, dir );
		if ( !p.hit() )
			return false;
		/* 12 kg m/s along the ray: about what a heavy pistol round carries, and
		 * enough to spin a limb without launching a brahmin across the scene.
		 * Off-centre by construction -- the impulse lands where the ray hit, not
		 * at the centre of mass, which is what makes a shot to a leg twist it.
		 */
		m_sim.applyImpulse( p.body, p.localPoint, dir * 12.0f );
		return true;
	}

	case Tool::Pin: {
		const SimPick p = m_sim.pick( rayOrigin / SCALE, dir );
		if ( !p.hit() )
			return false;
		const bool wasPinned = m_sim.bodies().at( p.body ).pinned;
		m_sim.setPinned( p.body, !wasPinned );
		return true;
	}

	case Tool::Blast: {
		/* Centred where the ray hits something, or on the rig if it hits nothing
		 * -- a blast that did nothing because the aim was slightly off would just
		 * look broken. 2 m radius, 30 kg m/s at the centre.
		 */
		const SimPick p = m_sim.pick( rayOrigin / SCALE, dir );
		if ( !p.hit() )
			return false;
		m_sim.blast( p.worldPoint, 2.0f, 30.0f );
		return true;
	}

	case Tool::Wind:
		m_holding = true;
		m_holdDir = dir;
		m_sim.wind = dir * 40.0f;
		return true;
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
			m_sim.wind = m_holdDir * 40.0f;
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
	/* Throw hands the body the hand's own velocity. The solver derives velocity
	 * from the position change each substep, so a dragged body is ALREADY moving
	 * at roughly the hand's speed -- but letting go simply stops driving it, and
	 * the constraint solve bleeds that off within a frame or two. Setting it
	 * explicitly is what makes a throw carry.
	 */
	if ( m_tool == Tool::Throw && m_sim.draggedBody() >= 0 )
		m_sim.setVelocity( m_sim.draggedBody(), m_grabVelocity );
	m_sim.clearDrag();
	return true;
}
