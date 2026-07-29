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
		/* Body -> node name, so a drag can say WHICH bone is in hand.
		 *
		 * The binding is the collision object's own: it names a body and targets a
		 * node. Matched by the system block it points at, because a skeleton file
		 * holds several systems and the body indices restart in each.
		 */
		m_bodyNames.assign( m_sim.bodies().size(), QString() );
		for ( qint32 c = 0; c < nif->getBlockCount(); c++ ) {
			const QModelIndex iObj = nif->getBlockIndex( c );
			if ( !nif->blockInherits( iObj, "bhkNPCollisionObject" ) )
				continue;
			if ( nif->getLink( iObj, "Data" ) != b )
				continue;
			const int body = int( nif->get<quint32>( iObj, "Body ID" ) );
			if ( body < 0 || body >= m_bodyNames.size() )
				continue;
			const QModelIndex iTarget = nif->getBlockIndex( nif->getLink( iObj, "Target" ) );
			if ( iTarget.isValid() )
				m_bodyNames[body] = nif->get<QString>( iTarget, "Name" );
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
	stepShots( h );
}

void PhysicsPreview::reset()
{
	if ( !m_active )
		return;
	const bool wasPaused = m_paused;
	const bool ground = m_sim.ground, selfColl = m_sim.selfCollision, angLimits = m_sim.angularLimits;
	const float groundZ = m_sim.groundZ, grip = m_sim.groundFriction;
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
	m_sim.groundFriction = grip;
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
	const SimPick p = m_sim.pick( rayOrigin / SCALE, rayDir / len );
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
	case Tool::Grab:
		if ( !grab( rayOrigin, rayDir ) )
			return false;
		m_lastGrabTarget = m_sim.toWorld( m_sim.draggedBody(), Vector3() );
		m_grabVelocity = Vector3();
		m_sinceGrabMove = 0.0f;
		return true;

	case Tool::Shoot: {
		const SimPick p = m_sim.pick( rayOrigin / SCALE, dir );
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
		const SimPick p = m_sim.pick( rayOrigin / SCALE, dir );
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
	return true;
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
