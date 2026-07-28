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
	// clamp the frame: a stall (a dialog, a reload) would otherwise hand the
	// solver a half-second step and fire the ragdoll across the scene
	m_sim.step( std::min( dt, 1.0f / 30.0f ) );
}

void PhysicsPreview::reset()
{
	if ( !m_active )
		return;
	const bool wasPaused = m_paused;
	m_sim.clearDrag();
	QString err;
	m_sim.build( m_system, &err );
	// build() resets the whole solver, so the floor has to be put back
	m_sim.ground = true;
	m_sim.groundZ = m_sim.lowestPoint() - 0.05f;
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

void PhysicsPreview::release()
{
	m_sim.clearDrag();
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
