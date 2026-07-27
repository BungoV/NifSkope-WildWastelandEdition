#include "ragdollsim.h"

#include <algorithm>
#include <cmath>

/* XPBD rigid bodies. The formulation is Muller et al., "Detailed Rigid Body
 * Simulation with Extended Position Based Dynamics": predict poses, solve
 * constraints as POSITION corrections, then read velocities back out of the pose
 * change. Stiffness costs nothing in stability, which is why ragdoll joints stay
 * put here where an impulse solver would need careful tuning.
 *
 * niftypes has no quaternion product, so the few operations needed live here
 * rather than growing the shared header for one caller.
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

inline Quat qConj( const Quat & q ) { return Quat( q[0], -q[1], -q[2], -q[3] ); }

//! rotate v by q
inline Vector3 qRot( const Quat & q, const Vector3 & v )
{
	const Vector3 u( q[1], q[2], q[3] );
	const float s = q[0];
	const Vector3 uv = Vector3::crossproduct( u, v );
	return v + ( uv * s + Vector3::crossproduct( u, uv ) ) * 2.0f;
}

inline Quat qNorm( const Quat & q )
{
	float n = std::sqrt( q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3] );
	if ( !( n > 1.0e-20f ) )
		return Quat( 1, 0, 0, 0 );
	return Quat( q[0] / n, q[1] / n, q[2] / n, q[3] / n );
}

//! q advanced by angular velocity w over h: q += 0.5 * (0,w) * q * h
inline Quat qIntegrate( const Quat & q, const Vector3 & w, float h )
{
	const Quat dq = qMul( Quat( 0.0f, w[0], w[1], w[2] ), q );
	return qNorm( Quat( q[0] + 0.5f * h * dq[0], q[1] + 0.5f * h * dq[1],
		q[2] + 0.5f * h * dq[2], q[3] + 0.5f * h * dq[3] ) );
}

//! basis rows (as decoded) -> quaternion. Shepperd's method: pick the largest
//! diagonal term so the divisor never approaches zero.
Quat qFromRows( const Vector3 & r0, const Vector3 & r1, const Vector3 & r2 )
{
	const float m00 = r0[0], m01 = r0[1], m02 = r0[2];
	const float m10 = r1[0], m11 = r1[1], m12 = r1[2];
	const float m20 = r2[0], m21 = r2[1], m22 = r2[2];
	const float tr = m00 + m11 + m22;
	if ( tr > 0.0f ) {
		float s = std::sqrt( tr + 1.0f ) * 2.0f;
		return qNorm( Quat( 0.25f * s, ( m21 - m12 ) / s, ( m02 - m20 ) / s, ( m10 - m01 ) / s ) );
	}
	if ( m00 > m11 && m00 > m22 ) {
		float s = std::sqrt( 1.0f + m00 - m11 - m22 ) * 2.0f;
		return qNorm( Quat( ( m21 - m12 ) / s, 0.25f * s, ( m01 + m10 ) / s, ( m02 + m20 ) / s ) );
	}
	if ( m11 > m22 ) {
		float s = std::sqrt( 1.0f + m11 - m00 - m22 ) * 2.0f;
		return qNorm( Quat( ( m02 - m20 ) / s, ( m01 + m10 ) / s, 0.25f * s, ( m12 + m21 ) / s ) );
	}
	float s = std::sqrt( 1.0f + m22 - m00 - m11 ) * 2.0f;
	return qNorm( Quat( ( m10 - m01 ) / s, ( m02 + m20 ) / s, ( m12 + m21 ) / s, 0.25f * s ) );
}

//! world-space inverse inertia applied to v, for a diagonal body-space tensor
inline Vector3 applyInvInertia( const SimBody & b, const Vector3 & v )
{
	const Vector3 local = qRot( qConj( b.q ), v );
	const Vector3 scaled( local[0] * b.invInertia[0], local[1] * b.invInertia[1],
		local[2] * b.invInertia[2] );
	return qRot( b.q, scaled );
}

//! generalised inverse mass for a correction along n applied at offset r
inline float genInvMass( const SimBody & b, const Vector3 & r, const Vector3 & n )
{
	if ( b.invMass <= 0.0f && b.invInertia.length() <= 0.0f )
		return 0.0f;
	const Vector3 rn = Vector3::crossproduct( r, n );
	return b.invMass + Vector3::dotproduct( rn, applyInvInertia( b, rn ) );
}

//! XPBD positional correction between two bodies at world offsets rA / rB
void applyPositional( SimBody & A, SimBody & B, const Vector3 & rA, const Vector3 & rB,
	const Vector3 & corr )
{
	const float c = corr.length();
	if ( !( c > 1.0e-9f ) )
		return;
	const Vector3 n = corr / c;
	const float wA = A.pinned ? 0.0f : genInvMass( A, rA, n );
	const float wB = B.pinned ? 0.0f : genInvMass( B, rB, n );
	const float wSum = wA + wB;
	if ( !( wSum > 1.0e-12f ) )
		return;

	const Vector3 p = n * ( c / wSum );
	if ( !A.pinned ) {
		A.x += p * A.invMass;
		const Vector3 t = applyInvInertia( A, Vector3::crossproduct( rA, p ) );
		A.q = qIntegrate( A.q, t, 1.0f );
	}
	if ( !B.pinned ) {
		B.x -= p * B.invMass;
		const Vector3 t = applyInvInertia( B, Vector3::crossproduct( rB, p ) );
		B.q = qIntegrate( B.q, -t, 1.0f );
	}
}

//! XPBD angular correction: corr is an axis scaled by the angle to remove
void applyAngular( SimBody & A, SimBody & B, const Vector3 & corr )
{
	const float theta = corr.length();
	if ( !( theta > 1.0e-9f ) )
		return;
	const Vector3 n = corr / theta;
	const float wA = A.pinned ? 0.0f : Vector3::dotproduct( n, applyInvInertia( A, n ) );
	const float wB = B.pinned ? 0.0f : Vector3::dotproduct( n, applyInvInertia( B, n ) );
	const float wSum = wA + wB;
	if ( !( wSum > 1.0e-12f ) )
		return;

	const Vector3 p = n * ( theta / wSum );
	if ( !A.pinned )
		A.q = qIntegrate( A.q, applyInvInertia( A, p ), 1.0f );
	if ( !B.pinned )
		B.q = qIntegrate( B.q, -applyInvInertia( B, p ), 1.0f );
}

/*! Constrain the angle of n1 about n to [lo, hi], correcting toward the nearer
 *  bound. This is Muller's limitAngle; the branchy bit is unwrapping the signed
 *  angle so a limit that straddles +-pi still behaves.
 */
bool limitAngle( SimBody & A, SimBody & B, const Vector3 & n, const Vector3 & n1,
	const Vector3 & n2, float lo, float hi )
{
	const float PI_F = 3.14159265358979323846f;
	float phi = std::asin( std::clamp(
		Vector3::dotproduct( Vector3::crossproduct( n1, n2 ), n ), -1.0f, 1.0f ) );
	if ( Vector3::dotproduct( n1, n2 ) < 0.0f )
		phi = PI_F - phi;
	if ( phi > PI_F )
		phi -= 2.0f * PI_F;
	if ( phi < -PI_F )
		phi += 2.0f * PI_F;

	if ( phi >= lo && phi <= hi )
		return false;

	phi = std::clamp( phi, lo, hi );
	// rotate n1 onto the bound, then the residual cross product is the correction
	const float s = std::sin( 0.5f * phi ), c = std::cos( 0.5f * phi );
	const Quat rot( c, n[0] * s, n[1] * s, n[2] * s );
	const Vector3 n1r = qRot( rot, n1 );
	applyAngular( A, B, Vector3::crossproduct( n1r, n2 ) );
	return true;
}

} // namespace


bool RagdollSim::build( const HknpSystem & sys, QString * error )
{
	m_bodies.clear();
	m_joints.clear();

	if ( sys.shapes.isEmpty() ) {
		if ( error )
			*error = QStringLiteral( "The collision decoded no shapes to simulate." );
		return false;
	}

	// one sim body per decoded shape, indexed by decoded body id
	int maxBody = -1;
	for ( const HknpShape & s : sys.shapes )
		maxBody = std::max( maxBody, s.bodyId );
	if ( maxBody < 0 ) {
		if ( error )
			*error = QStringLiteral( "The collision has no body attribution to simulate." );
		return false;
	}
	m_bodies.resize( maxBody + 1 );

	/* Place the bodies at the REST POSE, not at their shape centroids.
	 *
	 * A shape's capA/capB are in its own body space, so treating them as world
	 * positions piles every body near the origin and leaves the joints violated
	 * before the first step -- which is exactly what the first version did, and
	 * the solver duly exploded. hkaSkeleton's referencePose is the rest pose, and
	 * bone index equals body index (proven in WW_CHANGES 07-28b), so accumulate
	 * it down the hierarchy. Bones are stored parents-first, so a single forward
	 * pass suffices.
	 */
	QVector<Vector3> restPos( maxBody + 1 );
	QVector<Quat> restRot( maxBody + 1, Quat( 1, 0, 0, 0 ) );
	for ( int i = 0; i < sys.bones.size() && i <= maxBody; i++ ) {
		const HknpBone & bone = sys.bones.at( i );
		if ( bone.parent >= 0 && bone.parent < i ) {
			restRot[i] = qMul( restRot.at( bone.parent ), bone.rotation );
			restPos[i] = restPos.at( bone.parent )
				+ qRot( restRot.at( bone.parent ), bone.translation );
		} else {
			restRot[i] = bone.rotation;
			restPos[i] = bone.translation;
		}
	}

	for ( const HknpShape & s : sys.shapes ) {
		if ( s.bodyId < 0 || s.bodyId > maxBody )
			continue;
		SimBody & b = m_bodies[s.bodyId];
		b.bodyId = s.bodyId;
		b.primType = s.primType;
		b.capA = s.capA;
		b.capB = s.capB;
		b.radius = s.primRadius;
		// body origin is the BONE origin, so the joint pivots -- which are given
		// in bone space -- can be used as they stand
		b.x = restPos.value( s.bodyId );
		b.q = restRot.value( s.bodyId );

		const HknpBodyPhys phys = sys.bodyPhys.value( s.bodyId );
		const float mass = phys.mass > 0.0f ? phys.mass : sys.mass;
		b.invMass = ( mass > 1.0e-6f ) ? 1.0f / mass : 0.0f;
		/* dyn_inertia +0x20 holds INVERSE inertia, not inertia.
		 *
		 * +0x04 alongside it is plainly inverse mass (0.2, 0.05, 1.0 -> 5, 20 and
		 * 1 kg), and the same reading makes the tensor physically sensible: the
		 * brahmin pelvis at 5 kg with 4.16 there gives I = 0.24 and a radius near
		 * 0.22 m. Taken as inertia it would imply a 0.9 m radius pelvis. So use it
		 * as it stands -- reciprocating it here inverts it twice, which is what
		 * made the first run explode.
		 */
		for ( int k = 0; k < 3; k++ )
			b.invInertia[k] = std::max( 0.0f, phys.inertia[k] );
		// a body with no mass would be immovable; treat that as static
		if ( b.invMass <= 0.0f )
			b.pinned = true;
	}

	for ( const HknpConstraint & jc : sys.constraints ) {
		if ( !jc.hasFrames || jc.childBody < 0 || jc.parentBody < 0 )
			continue;
		if ( jc.childBody > maxBody || jc.parentBody > maxBody )
			continue;
		SimJoint j;
		j.a = jc.childBody;
		j.b = jc.parentBody;
		// already in each body's own space, and our body origin IS the bone
		// origin, so these are used as they stand
		j.pivotA = jc.pivotA;
		j.pivotB = jc.pivotB;
		j.frameA = qFromRows( jc.rotA[0], jc.rotA[1], jc.rotA[2] );
		j.frameB = qFromRows( jc.rotB[0], jc.rotB[1], jc.rotB[2] );
		j.twist = jc.twist;
		j.cone = jc.cone;
		j.plane = jc.plane;
		j.hinge = jc.hinge;
		m_joints.append( j );
	}

	return true;
}

void RagdollSim::setPinned( int body, bool pinned )
{
	if ( body >= 0 && body < m_bodies.size() )
		m_bodies[body].pinned = pinned;
}

void RagdollSim::setPosition( int body, const Vector3 & pos )
{
	if ( body >= 0 && body < m_bodies.size() ) {
		m_bodies[body].x = pos;
		m_bodies[body].v = Vector3();
	}
}

void RagdollSim::solveJoints( float h )
{
	Q_UNUSED( h )
	for ( const SimJoint & j : std::as_const( m_joints ) ) {
		SimBody & A = m_bodies[j.a];
		SimBody & B = m_bodies[j.b];

		// --- ball socket: the two attachment points must coincide -------------
		const Vector3 rA = qRot( A.q, j.pivotA );
		const Vector3 rB = qRot( B.q, j.pivotB );
		applyPositional( A, B, rA, rB, ( B.x + rB ) - ( A.x + rA ) );

		if ( !angularLimits )
			continue;

		// --- angular limits ---------------------------------------------------
		// joint frames in world; column 0 is the twist axis, 1 and 2 the swing
		// axes, matching how the hkTransform rows were decoded
		const Quat fa = qMul( A.q, j.frameA );
		const Quat fb = qMul( B.q, j.frameB );
		const Vector3 a0 = qRot( fa, Vector3( 1, 0, 0 ) );
		const Vector3 a1 = qRot( fa, Vector3( 0, 1, 0 ) );
		const Vector3 b0 = qRot( fb, Vector3( 1, 0, 0 ) );
		const Vector3 b1 = qRot( fb, Vector3( 0, 1, 0 ) );
		const Vector3 b2 = qRot( fb, Vector3( 0, 0, 1 ) );

		/* Swing (cone) limit: how far the child's twist axis may lean off the
		 * parent's. The rotation axis is the one perpendicular to both, i.e.
		 * a0 x b0, whose length is already sin(angle). Havok writes -100 as
		 * "no lower bound" on a cone, so only max carries information.
		 */
		if ( j.cone.present ) {
			Vector3 sw = Vector3::crossproduct( a0, b0 );
			if ( sw.length() > 1.0e-6f ) {
				sw.normalize();
				limitAngle( A, B, sw, a0, b0, -j.cone.max, j.cone.max );
			}
		}
		// plane: the perpendicular swing, measured about the parent's third axis
		if ( j.plane.present )
			limitAngle( A, B, b2, a0, b0, j.plane.min, j.plane.max );

		/* Twist about the shared axis. The swing axes MUST be projected
		 * perpendicular to it first: measuring the angle between un-projected
		 * axes mixes swing into the twist reading and the two limits then fight
		 * each other every substep.
		 */
		if ( j.twist.present ) {
			Vector3 n = a0 + b0;
			if ( n.length() > 1.0e-6f ) {
				n.normalize();
				Vector3 n1 = a1 - n * Vector3::dotproduct( n, a1 );
				Vector3 n2 = b1 - n * Vector3::dotproduct( n, b1 );
				if ( n1.length() > 1.0e-6f && n2.length() > 1.0e-6f ) {
					n1.normalize();
					n2.normalize();
					limitAngle( A, B, n, n1, n2, j.twist.min, j.twist.max );
				}
			}
		}
		// a limited hinge: hold the two axes aligned, then limit the swing about them
		if ( j.hinge.present ) {
			applyAngular( A, B, Vector3::crossproduct( a0, b0 ) );
			limitAngle( A, B, b0, a1, b1, j.hinge.min, j.hinge.max );
		}
	}
}

void RagdollSim::step( float dt, int substeps )
{
	if ( m_bodies.isEmpty() || substeps < 1 )
		return;
	const float h = dt / float( substeps );
	const float damp = std::clamp( 1.0f - damping * h, 0.0f, 1.0f );

	for ( int s = 0; s < substeps; s++ ) {
		for ( SimBody & b : m_bodies ) {
			b.xPrev = b.x;
			b.qPrev = b.q;
			if ( b.pinned )
				continue;
			b.v += gravity * h;
			b.v *= damp;
			b.w *= damp;
			b.x += b.v * h;
			b.q = qIntegrate( b.q, b.w, h );
		}

		solveJoints( h );

		for ( SimBody & b : m_bodies ) {
			if ( b.pinned ) {
				b.v = Vector3();
				b.w = Vector3();
				continue;
			}
			b.v = ( b.x - b.xPrev ) / h;
			// angular velocity from the pose change: 2 * (dq.xyz) / h
			const Quat dq = qMul( b.q, qConj( b.qPrev ) );
			Vector3 om( dq[1], dq[2], dq[3] );
			om = om * ( 2.0f / h );
			b.w = ( dq[0] >= 0.0f ) ? om : -om;
		}
	}
}

SimStats RagdollSim::stats() const
{
	SimStats st;
	for ( const SimBody & b : m_bodies ) {
		if ( b.pinned || b.invMass <= 0.0f )
			continue;
		const float m = 1.0f / b.invMass;
		const float sp = b.v.length();
		st.energy += 0.5f * m * sp * sp;
		st.maxSpeed = std::max( st.maxSpeed, sp );
		if ( !std::isfinite( b.x[0] ) || !std::isfinite( b.x[1] ) || !std::isfinite( b.x[2] )
			|| !std::isfinite( sp ) )
			st.diverged = true;
	}
	for ( const SimJoint & j : m_joints ) {
		const SimBody & A = m_bodies.at( j.a );
		const SimBody & B = m_bodies.at( j.b );
		const Vector3 pa = A.x + qRot( A.q, j.pivotA );
		const Vector3 pb = B.x + qRot( B.q, j.pivotB );
		st.maxJointError = std::max( st.maxJointError, ( pa - pb ).length() );
	}
	if ( !std::isfinite( st.maxJointError ) || st.maxSpeed > 1.0e4f )
		st.diverged = true;
	return st;
}
