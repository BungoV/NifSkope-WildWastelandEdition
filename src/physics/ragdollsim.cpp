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

/*! World-space inverse inertia applied to v, for a diagonal body-space tensor.
 *
 * Carries the mass-splitting scale, so every use of it -- both computing a
 * correction's size and applying it -- is scaled consistently. Scaling only one
 * of the two would break the constraint solve rather than soften it.
 */
inline Vector3 applyInvInertia( const SimBody & b, const Vector3 & v )
{
	const Vector3 local = qRot( qConj( b.q ), v );
	const float s = b.solverScale;
	const Vector3 scaled( local[0] * b.invInertia[0] * s, local[1] * b.invInertia[1] * s,
		local[2] * b.invInertia[2] * s );
	return qRot( b.q, scaled );
}

/*! Cap a correction rotation at the angle where the maths stops being valid.
 *
 * The XPBD rotational update is the LINEARISED quaternion step q += 0.5*w*q, good
 * only while w is small; feeding it a couple of radians does not rotate by a
 * couple of radians, it produces something the renormalisation then has to
 * rescue. Usually that never arises, but the eyebot's antennae carry an inverse
 * inertia of 4417 about their own axis against 3.67 across it -- a ratio of 1200
 * -- so any correction with a component along the thin axis is amplified into
 * exactly that regime, and the ragdoll leaves at 116 m/s.
 *
 * Capping costs nothing real: the sweeps that follow finish whatever this one
 * left, so a capped correction converges in a few more iterations instead of
 * diverging in one. 0.2 rad is about 11 degrees.
 */
inline Vector3 capRotation( const Vector3 & dtheta )
{
	const float MAX = 0.2f;
	const float m = dtheta.length();
	return ( m > MAX ) ? dtheta * ( MAX / m ) : dtheta;
}

//! generalised inverse mass for a correction along n applied at offset r
inline float genInvMass( const SimBody & b, const Vector3 & r, const Vector3 & n )
{
	if ( b.invMass <= 0.0f && b.invInertia.length() <= 0.0f )
		return 0.0f;
	const Vector3 rn = Vector3::crossproduct( r, n );
	return b.invMass * b.solverScale + Vector3::dotproduct( rn, applyInvInertia( b, rn ) );
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
		A.x += p * ( A.invMass * A.solverScale );
		const Vector3 t = applyInvInertia( A, Vector3::crossproduct( rA, p ) );
		A.q = qIntegrate( A.q, capRotation( t ), 1.0f );
	}
	if ( !B.pinned ) {
		B.x -= p * ( B.invMass * B.solverScale );
		const Vector3 t = applyInvInertia( B, Vector3::crossproduct( rB, p ) );
		B.q = qIntegrate( B.q, capRotation( -t ), 1.0f );
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
		A.q = qIntegrate( A.q, capRotation( applyInvInertia( A, p ) ), 1.0f );
	if ( !B.pinned )
		B.q = qIntegrate( B.q, capRotation( -applyInvInertia( B, p ) ), 1.0f );
}

/*! The two joint frames in world. Column 0 is the twist axis and 1 and 2 the
 *  swing axes, matching how the hkTransform rows were decoded.
 *
 * Shared by the solver and the limit report on purpose: a report that measured
 * the angles its own way could disagree with the solver and send us hunting the
 * wrong thing.
 */
struct JointAxes { Vector3 a0, a1, b0, b1, b2; };

JointAxes jointAxes( const SimBody & A, const SimBody & B, const SimJoint & j )
{
	const Quat fa = qMul( A.q, j.frameA );
	const Quat fb = qMul( B.q, j.frameB );
	JointAxes ax;
	ax.a0 = qRot( fa, Vector3( 1, 0, 0 ) );
	ax.a1 = qRot( fa, Vector3( 0, 1, 0 ) );
	ax.b0 = qRot( fb, Vector3( 1, 0, 0 ) );
	ax.b1 = qRot( fb, Vector3( 0, 1, 0 ) );
	ax.b2 = qRot( fb, Vector3( 0, 0, 1 ) );
	return ax;
}

//! Signed angle from n1 to n2 measured about n, unwrapped to [-pi, pi].
float signedAngle( const Vector3 & n, const Vector3 & n1, const Vector3 & n2 )
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
	return phi;
}

/*! Constrain the angle of n1 about n to [lo, hi], correcting toward the nearer
 *  bound. This is Muller's limitAngle; the fiddly part is the unwrapping above,
 *  so that a limit straddling +-pi still behaves.
 *
 * Returns how far out of range the angle WAS, in radians, so a caller can tell a
 * joint that is merely resting against its limit from one that is being forced
 * through it. Those are not the same thing at all: a ragdoll in motion has limits
 * firing constantly, and treating that as distress spent eight passes on every
 * one of them and unsettled five animals that had been fine.
 */
float limitAngle( SimBody & A, SimBody & B, const Vector3 & n, const Vector3 & n1,
	const Vector3 & n2, float lo, float hi )
{
	const float phi0 = signedAngle( n, n1, n2 );
	if ( phi0 >= lo && phi0 <= hi )
		return 0.0f;

	float phi = std::clamp( phi0, lo, hi );
	const float excess = std::fabs( phi0 - phi );
	// rotate n1 onto the bound, then the residual cross product is the correction
	const float s = std::sin( 0.5f * phi ), c = std::cos( 0.5f * phi );
	const Quat rot( c, n[0] * s, n[1] * s, n[2] * s );
	const Vector3 n1r = qRot( rot, n1 );
	applyAngular( A, B, Vector3::crossproduct( n1r, n2 ) );
	return excess;
}

/*! Closest points between two segments, Ericson's routine.
 *
 * Every shape a ragdoll uses is a capsule or a sphere, and both are "all points
 * within r of a segment" -- a sphere just has a zero-length one. So the whole
 * narrow phase is this one function plus a radius comparison: exact, no GJK, no
 * iteration, no tolerance to tune.
 */
void closestPtSegSeg( const Vector3 & p1, const Vector3 & q1,
	const Vector3 & p2, const Vector3 & q2, Vector3 & c1, Vector3 & c2 )
{
	const Vector3 d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
	const float a = Vector3::dotproduct( d1, d1 );
	const float e = Vector3::dotproduct( d2, d2 );
	const float f = Vector3::dotproduct( d2, r );
	const float EPS = 1.0e-12f;
	float s = 0.0f, t = 0.0f;

	if ( a <= EPS && e <= EPS ) {           // both degenerate: two spheres
		c1 = p1;
		c2 = p2;
		return;
	}
	if ( a <= EPS ) {                        // first degenerate
		t = std::clamp( f / e, 0.0f, 1.0f );
	} else {
		const float c = Vector3::dotproduct( d1, r );
		if ( e <= EPS ) {                    // second degenerate
			s = std::clamp( -c / a, 0.0f, 1.0f );
		} else {
			const float b = Vector3::dotproduct( d1, d2 );
			const float denom = a * e - b * b;
			// parallel segments leave s free; anchor it at zero
			s = ( denom > EPS ) ? std::clamp( ( b * f - c * e ) / denom, 0.0f, 1.0f ) : 0.0f;
			t = ( b * s + f ) / e;
			// t outside the segment means the closest point is an end cap, so
			// pin t and re-solve s against it
			if ( t < 0.0f ) {
				t = 0.0f;
				s = std::clamp( -c / a, 0.0f, 1.0f );
			} else if ( t > 1.0f ) {
				t = 1.0f;
				s = std::clamp( ( b - c ) / a, 0.0f, 1.0f );
			}
		}
	}
	c1 = p1 + d1 * s;
	c2 = p2 + d2 * t;
}

//! A body's shape in world. capA/capB are in bone space and x is the centre of
//! mass, so the shape has to be rebased through com on the way out.
inline void worldSegment( const SimBody & b, Vector3 & p, Vector3 & q )
{
	p = b.x + qRot( b.q, b.capA - b.com );
	q = ( b.primType == 2 ) ? ( b.x + qRot( b.q, b.capB - b.com ) ) : p;
}

//! Radius of the world-space sphere that bounds the whole shape, for broad phase
inline float boundRadius( const SimBody & b )
{
	float r = 0.0f;
	for ( const SimBody::SimPoint & sp : b.points )
		r = std::max( r, ( sp.p - b.com ).length() + sp.r );
	return r;
}

//! Does this body have anything to collide with at all?
inline bool hasGeometry( const SimBody & b )
{
	return !b.points.isEmpty();
}

//! A shape point in world, remembering that x is the centre of mass
inline Vector3 worldPoint( const SimBody & b, const SimBody::SimPoint & sp )
{
	return b.x + qRot( b.q, sp.p - b.com );
}

/*! Can these two bodies use the exact segment test?
 *
 * Only when each is a single capsule or sphere. A compound is a point set with
 * no faces, and pretending otherwise would let a limb slip between a box's
 * vertices while reporting no contact.
 */
inline bool exactPair( const SimBody & a, const SimBody & b )
{
	return a.shapeCount == 1 && b.shapeCount == 1 && a.primType != 0 && b.primType != 0;
}

/*! Five degrees: the line between a joint resting on its limit and one being
 *  driven through it. Only the latter earns extra solver passes.
 *
 * Measured across the corpus with verified builds: 5 degrees settles 36 of 37
 * ragdolls with the eyebot the only failure at 17.2 m/s; 10 settles 35, improving
 * the eyebot to 10.0 but pushing the mirelurk hunter just over the line; 15
 * settles 35 with a corpus worst of 41.3. Too tight and healthy joints resting on
 * their limits are treated as distressed; too loose and the genuinely stuck ones
 * get no help.
 *
 * An earlier sweep of these same numbers was measured against stale builds and
 * reported 34.6 / 20.1 / 46.3, which is why the build is now verified before each
 * measurement rather than piped to /dev/null.
 */
constexpr float FORCED_RAD = 0.0873f;
inline bool FORCED( float excessRadians ) { return excessRadians > FORCED_RAD; }

} // namespace


bool RagdollSim::build( const HknpSystem & sys, QString * error )
{
	m_bodies.clear();
	m_joints.clear();
	m_rebasedJoints = 0;

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

	/* Seat EVERY body at its rest pose, not only the ones that carry a shape.
	 *
	 * A body with no shape used to keep x = (0,0,0) and pinned = false, so any
	 * joint touching it was violated by the full distance to the origin, for ever
	 * -- the solver corrected it every substep and pumped the energy straight into
	 * the other end. It also has no geometry to simulate, so pin it.
	 */
	for ( int i = 0; i <= maxBody; i++ ) {
		m_bodies[i].bodyId = i;
		m_bodies[i].x = restPos.value( i );
		m_bodies[i].q = restRot.value( i );
		m_bodies[i].pinned = true;      // released below if a shape arrives
	}

	for ( const HknpShape & s : sys.shapes ) {
		if ( s.bodyId < 0 || s.bodyId > maxBody )
			continue;
		SimBody & b = m_bodies[s.bodyId];
		b.pinned = false;
		b.bodyId = s.bodyId;
		b.shapeCount++;
		b.primType = s.primType;
		b.capA = s.capA;
		b.capB = s.capB;
		b.radius = s.primRadius;
		// accumulate, never overwrite: several shapes can share one body
		if ( s.primType == 2 ) {
			b.points.append( { s.capA, s.primRadius } );
			b.points.append( { s.capB, s.primRadius } );
		} else if ( s.primType == 1 ) {
			b.points.append( { s.capA, s.primRadius } );
		} else {
			for ( const Vector3 & v : s.verts )
				b.points.append( { v, 0.0f } );
		}

		const HknpBodyPhys phys = sys.bodyPhys.value( s.bodyId );
		/* Sit the body on its CENTRE OF MASS, not on the bone origin.
		 *
		 * The inertia tensor is expressed about the centre of mass, and a limb
		 * bone's origin is its joint -- a good 0.2 m away. Treating the two as one
		 * point applies the tensor about the wrong axis: by the parallel axis
		 * theorem the true inertia about the bone origin is larger by m*d^2, which
		 * on the brahmin thigh is a factor of about 27. The solver then
		 * over-rotates every correction, the far end of the bone whips, and the
		 * next substep has a bigger violation to fix than the one it just fixed.
		 * That is a MODELLING error, not a discretisation one, which is precisely
		 * why more substeps made the blow-up worse instead of better.
		 *
		 * The file has no centre-of-mass field to read -- cinfo +0x30 turns out to
		 * be the body position, equal to the bone origin on all 39 brahmin bodies
		 * -- so take the shape's own centroid. The decoded tensor corroborates it:
		 * body 8's I_xx gives a capsule radius of 0.083 m and its I_yy a length of
		 * 0.59 m, which is the 0.428 m bone plus a radius at each end. Read as a
		 * bone-origin tensor the same numbers would imply a 0.30 m capsule on a
		 * 0.428 m bone, i.e. one that fails to reach its own child joint.
		 */
		/* Capsules give the centroid exactly; a convex hull has to be averaged
		 * over its vertices. Turrets, Liberty Prime and every prop are polytopes
		 * rather than capsules, so without this branch they all kept a centre of
		 * mass of (0,0,0) -- the bone origin -- and inherited the entire
		 * parallel-axis error this fix exists to remove.
		 */
		Vector3 sum;
		for ( const SimBody::SimPoint & sp : std::as_const( b.points ) )
			sum += sp.p;
		if ( !b.points.isEmpty() )
			b.com = sum / float( b.points.size() );
		b.restOrigin = restPos.value( s.bodyId );
		b.restOrient = restRot.value( s.bodyId );
		b.cinfoPos = phys.position;
		b.cinfoRot = phys.orientation;
		b.x = restPos.value( s.bodyId ) + qRot( restRot.value( s.bodyId ), b.com );
		b.q = restRot.value( s.bodyId );

		const float mass = phys.mass > 0.0f ? phys.mass : sys.mass;
		b.invMass = ( mass > 1.0e-6f ) ? 1.0f / mass : 0.0f;
		b.linDamping = std::max( 0.0f, phys.linDamping );
		b.angDamping = std::max( 0.0f, phys.angDamping );
		b.layer = phys.layer;
		b.filterGroup = phys.filterGroup;
		b.filterFlags = phys.filterFlags;
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
			b.invInertia[k] = std::max( 0.0f, phys.invInertia[k] );
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
		// the decoded pivots are in bone space, but a body now sits on its centre
		// of mass, so rebase them onto the same origin the solver rotates about
		j.pivotA = jc.pivotA - m_bodies.at( jc.childBody ).com;
		j.pivotB = jc.pivotB - m_bodies.at( jc.parentBody ).com;
		/* The three decoded hkVector4 are hkRotation's COLUMNS, not its rows.
		 *
		 * qFromRows reads them as rows, so it yields the transpose; for a rotation
		 * that is the inverse, and the conjugate undoes it. Getting this backwards
		 * is not subtle in its effects but is invisible in the numbers: it left 22
		 * of the brahmin's 38 joints violating their own limits in the rest pose,
		 * with cone angles up to 169 degrees, and the solver then fought that
		 * every substep.
		 */
		j.frameA = qConj( qFromRows( jc.rotA[0], jc.rotA[1], jc.rotA[2] ) );
		j.frameB = qConj( qFromRows( jc.rotB[0], jc.rotB[1], jc.rotB[2] ) );
		j.pivotBRaw = j.pivotB;
		j.kind = jc.kind;
		j.twist = jc.twist;
		j.cone = jc.cone;
		j.plane = jc.plane;
		j.hinge = jc.hinge;
		/* A range stored the wrong way round is not satisfiable by anything, so
		 * the solver would correct it on every substep for ever. Two vanilla
		 * hinges are authored that way. std::clamp with lo above hi is undefined
		 * behaviour besides, so this is not merely tidiness.
		 */
		for ( HknpAngLimit * l : { &j.twist, &j.cone, &j.plane, &j.hinge } )
			if ( l->present && l->min > l->max )
				std::swap( l->min, l->max );
		m_joints.append( j );
	}

	/* Some constraints ship with their parent-side transform never filled in.
	 *
	 * Not a decode fault -- the raw bytes really do hold an identity rotation and
	 * a zero pivot. The turret's first two joints are like that, and taken at face
	 * value they assert that a child body's origin coincides with its parent's,
	 * which in the rest pose is 0.67 m from true. The solver cannot satisfy that
	 * and spends every substep trying, which is where those models' energy came
	 * from. Havok fills these in at setup time from the bodies' current transforms
	 * (setInBodySpace), so deriving the pivot from the rest pose is what the
	 * engine would have done rather than a fudge.
	 *
	 * Only pivots visibly wrong are touched, and the count is reported, so this
	 * cannot quietly paper over a real decode error later. The FRAMES are left
	 * alone: they drive the angular limits, and inventing them would change
	 * authored limit behaviour in a way nothing here can check.
	 */
	for ( SimJoint & j : m_joints ) {
		const SimBody & A = m_bodies.at( j.a );
		const SimBody & B = m_bodies.at( j.b );
		const Vector3 worldA = A.x + qRot( A.q, j.pivotA );
		if ( ( worldA - ( B.x + qRot( B.q, j.pivotB ) ) ).length() <= 0.001f )
			continue;
		j.pivotB = qRot( qConj( B.q ), worldA - B.x );
		j.rebased = true;
		m_rebasedJoints++;
	}

	rescaleForJointCount();
	buildCollisionFilter();
	return true;
}

void RagdollSim::buildCollisionFilter()
{
	m_noCollide.clear();
	m_restOverlaps = 0;
	const int n = m_bodies.size();
	// the stride the stored keys are encoded with, frozen here: spawning a prop
	// grows m_bodies, and re-deriving it from the new size would reinterpret them
	m_filterStride = n;
	m_propStart = n;
	auto key = []( int a, int b, int n ) { return std::min( a, b ) * n + std::max( a, b ); };

	// jointed bodies always overlap where they meet, by construction
	for ( const SimJoint & j : std::as_const( m_joints ) )
		if ( j.a >= 0 && j.b >= 0 )
			m_noCollide.insert( key( j.a, j.b, n ) );

	for ( int i = 0; i < n; i++ ) {
		for ( int k = i + 1; k < n; k++ ) {
			if ( m_noCollide.contains( key( i, k, n ) ) )
				continue;
			const SimBody & A = m_bodies.at( i );
			const SimBody & B = m_bodies.at( k );

			/* Havok's own answer first: bit 0x40 of the filter means "do not
			 * collide inside my group", which is exactly how a ragdoll stops its
			 * own limbs fighting. Where the file says so, believe it.
			 */
			if ( A.filterGroup && A.filterGroup == B.filterGroup
				&& ( ( A.filterFlags | B.filterFlags ) & 0x40u ) ) {
				m_noCollide.insert( key( i, k, n ) );
				continue;
			}

			/* Otherwise fall back on the pose. The authored rest pose has limbs
			 * genuinely intersecting -- a thigh inside a pelvis -- and a solver
			 * asked to separate those would tear the ragdoll apart on the first
			 * frame. Anything already overlapping at rest was never meant to
			 * collide.
			 */
			if ( !exactPair( A, B ) )
				continue;
			Vector3 p1, q1, p2, q2, c1, c2;
			worldSegment( A, p1, q1 );
			worldSegment( B, p2, q2 );
			closestPtSegSeg( p1, q1, p2, q2, c1, c2 );
			if ( ( c1 - c2 ).length() < A.radius + B.radius ) {
				m_noCollide.insert( key( i, k, n ) );
				m_restOverlaps++;
			}
		}
	}
}

void RagdollSim::collectPairs()
{
	m_pairs.clear();
	const int n = m_bodies.size();
	auto key = []( int a, int b, int n ) { return std::min( a, b ) * n + std::max( a, b ); };

	if ( ground ) {
		for ( int i = 0; i < n; i++ )
			if ( !m_bodies.at( i ).pinned && hasGeometry( m_bodies.at( i ) ) )
				m_pairs.append( { i, -1 } );
	}
	if ( !selfCollision )
		return;

	for ( int i = 0; i < n; i++ ) {
		const SimBody & A = m_bodies.at( i );
		for ( int k = i + 1; k < n; k++ ) {
			const SimBody & B = m_bodies.at( k );
			// body-body is exact only for single capsules and spheres; a compound
			// is a point set with no faces, so it is left to the ground plane
			if ( !exactPair( A, B ) || ( A.pinned && B.pinned ) )
				continue;
			/* The body in hand can be excused from the rig, so a limb that is
			 * already inside the torso can be pulled back out of it. A prop is
			 * never excused: it is the thing being thrown AT the rig.
			 */
			if ( dragNoCollide && m_dragBody >= 0 && ( i == m_dragBody || k == m_dragBody )
				&& !A.prop && !B.prop )
				continue;
			/* The stored keys use the stride the filter was BUILT with, and a prop
			 * has no entry in it at all -- looking one up would be reading a key
			 * that was never written, which can land on another pair's.
			 */
			if ( i < m_propStart && k < m_propStart
				&& m_noCollide.contains( key( i, k, m_filterStride ) ) )
				continue;
			// cheap bounding-sphere reject before the exact test
			const float reach = boundRadius( A ) + boundRadius( B );
			if ( ( A.x - B.x ).squaredLength() > reach * reach )
				continue;
			m_pairs.append( { i, k } );
		}
	}
}

void RagdollSim::solveContacts( float h, bool record )
{
	Q_UNUSED( h )
	static SimBody staticGround;    // pinned, never written; stands in for the plane
	staticGround.pinned = true;
	if ( record )
		m_contacts.clear();

	for ( const SimPair & pr : std::as_const( m_pairs ) ) {
		SimBody & A = m_bodies[pr.a];
		Vector3 p1, q1;
		worldSegment( A, p1, q1 );

		Vector3 normal, point;
		float depth = 0.0f;

		if ( pr.b < 0 ) {
			/* Every point below the plane is a contact, not just the deepest one:
			 * correcting one would let a box teeter on a corner and a capsule
			 * pivot about whichever end happened to be lower. Against a plane this
			 * is exact -- no support mapping or clipping needed.
			 *
			 * They must be SHARED, though, exactly as several joints on one body
			 * are. A box landing flat puts eight vertices through the floor, and
			 * eight full corrections lift it eight times as far as one; the sentry
			 * left the ground at 24 m/s that way. Counting first and splitting by
			 * that count is the same remedy as solverScale, applied to contacts.
			 */
			normal = Vector3( 0, 0, 1 );
			int touching = 0;
			for ( const SimBody::SimPoint & sp : A.points )
				if ( groundZ + sp.r - worldPoint( A, sp )[2] > 0.0f )
					touching++;
			if ( !touching )
				continue;
			const float share = 1.0f / float( touching );
			float deepest = 0.0f;
			Vector3 deepestPoint;
			for ( const SimBody::SimPoint & sp : A.points ) {
				const Vector3 c = worldPoint( A, sp );
				const float d = groundZ + sp.r - c[2];
				if ( d <= 0.0f )
					continue;
				point = Vector3( c[0], c[1], groundZ );
				// the deepest point is the one that struck; the others are along for
				// the ride, and a bounce comes off one place rather than off eight
				if ( d > deepest ) {
					deepest = d;
					deepestPoint = point;
				}
				staticGround.x = point;
				applyPositional( A, staticGround, point - A.x, Vector3(),
					normal * ( d * share ) );

				/* Friction against the FLOOR.
				 *
				 * This branch used to `continue` straight past the Coulomb correction
				 * below, so body-on-body contacts had friction and the ground had
				 * none: a ragdoll landed and then slid across the floor for ever.
				 * Shared by the same count as the normal correction, for the same
				 * reason -- eight vertices on the floor must not brake eight times.
				 */
				if ( groundFriction > 0.0f ) {
					Vector3 rel = A.x - A.xPrev;
					rel = rel - normal * Vector3::dotproduct( rel, normal );
					const float slide = rel.length();
					if ( slide > 1.0e-9f ) {
						const float take = std::min( slide, groundFriction * d * share );
						applyPositional( A, staticGround, point - A.x, Vector3(),
							rel * ( -take / slide ) );
					}
				}
			}
			if ( record && deepest > 0.0f )
				m_contacts.append( { pr.a, -1, deepestPoint - A.x, Vector3(), Vector3( 0, 0, 1 ) } );
			continue;
		}

		SimBody & B = m_bodies[pr.b];
		Vector3 p2, q2, c1, c2;
		worldSegment( B, p2, q2 );
		closestPtSegSeg( p1, q1, p2, q2, c1, c2 );

		Vector3 sep = c1 - c2;
		const float dist = sep.length();
		depth = A.radius + B.radius - dist;
		if ( depth <= 0.0f )
			continue;
		// exactly coincident axes give no normal; push along Z rather than NaN
		normal = ( dist > 1.0e-6f ) ? ( sep / dist ) : Vector3( 0, 0, 1 );
		point = ( c1 + c2 ) * 0.5f;
		applyPositional( A, B, point - A.x, point - B.x, normal * depth );
		if ( record )
			m_contacts.append( { pr.a, pr.b, point - A.x, point - B.x, normal } );

		/* Coulomb friction, as a tangential position correction bounded by the
		 * normal one. Without it a ragdoll on the ground slides for ever, which
		 * looks broken even though it is perfectly stable.
		 */
		if ( friction > 0.0f ) {
			const Vector3 rA = point - A.x, rB = point - B.x;
			Vector3 rel = ( A.x - A.xPrev ) - ( B.x - B.xPrev );
			rel = rel - normal * Vector3::dotproduct( rel, normal );
			const float slide = rel.length();
			if ( slide > 1.0e-9f ) {
				const float take = std::min( slide, friction * depth );
				applyPositional( A, B, rA, rB, rel * ( -take / slide ) );
			}
		}
	}
}

void RagdollSim::rescaleForJointCount()
{
	// total simulated mass, for the drag limit. Pinned and static bodies have no
	// inverse mass and contribute nothing that has to be hauled about.
	m_totalMass = 0.0f;
	for ( const SimBody & b : m_bodies )
		if ( b.invMass > 0.0f )
			m_totalMass += 1.0f / b.invMass;
	if ( !( m_totalMass > 0.0f ) )
		m_totalMass = 1.0f;
	/* Scale by JOINT count, and resist the temptation to refine it.
	 *
	 * Weighting each joint by how many corrections it actually applies -- a hinge
	 * aligns the axles and then bounds the swing, so it is worth three, not one --
	 * sounds strictly better and measures worse: 30 of 37 ragdolls settling
	 * against 32, with the workshop turret going from 60 m/s to 149. Softening
	 * that hard leaves each sweep barely correcting, and the residual becomes
	 * velocity. Raising the global sweep count instead is no better; 16 sweeps
	 * fixed the turret and took the sentry from 8.7 m/s to 75. Both were tried on
	 * the full corpus and both were rejected by it.
	 */
	for ( SimBody & b : m_bodies )
		b.solverScale = 0.0f;       // reused as the count, normalised below
	for ( const SimJoint & j : std::as_const( m_joints ) ) {
		if ( j.a >= 0 && j.a < m_bodies.size() )
			m_bodies[j.a].solverScale += 1.0f;
		if ( j.b >= 0 && j.b < m_bodies.size() )
			m_bodies[j.b].solverScale += 1.0f;
	}
	for ( SimBody & b : m_bodies )
		b.solverScale = ( b.solverScale > 1.0f ) ? 1.0f / b.solverScale : 1.0f;
}

bool RagdollSim::buildTestCase( const QString & name )
{
	m_bodies.clear();
	m_joints.clear();

	// every case hangs off a pinned anchor at the origin
	auto addBody = [this]( const Vector3 & pos, float invInertia, const Quat & rot ) -> int {
		SimBody b;
		b.x = pos;
		b.q = rot;
		b.invMass = 1.0f;       // 1 kg throughout, so energy is easy to read
		b.linDamping = 0.0f;    // the rigs test conservation; damping would mask it
		b.angDamping = 0.0f;
		b.invInertia = Vector3( invInertia, invInertia, invInertia );
		b.bodyId = m_bodies.size();
		m_bodies.append( b );
		return m_bodies.size() - 1;
	};
	auto addJoint = [this]( int child, int parent, const Vector3 & pa, const Vector3 & pb ) {
		SimJoint j;
		j.a = child;
		j.b = parent;
		j.pivotA = pa;
		j.pivotB = pb;
		j.frameA = Quat( 1, 0, 0, 0 );
		j.frameB = Quat( 1, 0, 0, 0 );
		m_joints.append( j );
	};

	SimBody anchor;
	anchor.pinned = true;
	anchor.bodyId = 0;
	m_bodies.append( anchor );

	/* A chain of n links, each half a metre long, hanging off the anchor. Every
	 * body's centre of mass sits at its own middle, which is how a real ragdoll
	 * body is built once the parallel axis correction is in.
	 */
	auto chain = [&]( int n, float ii ) {
		for ( int k = 0; k < n; k++ ) {
			const int me = addBody( Vector3( 0, 0, -0.25f - 0.5f * k ), ii, Quat( 1, 0, 0, 0 ) );
			addJoint( me, me - 1, Vector3( 0, 0, 0.25f ),
				( k == 0 ) ? Vector3( 0, 0, 0 ) : Vector3( 0, 0, -0.25f ) );
		}

	};

	/* Start every rig spinning rigidly about the anchor.
	 *
	 * The obvious initial condition -- shove one body sideways and leave the rest
	 * at rest -- is not a state the rig can actually be in: a body held at a pivot
	 * with no angular velocity has no linear velocity either. The solver dutifully
	 * projects the impossible part away in the first substep, and the energy that
	 * costs is a fixed amount no matter how fine the substeps are. That looked
	 * exactly like a substep-independent leak in the solver and was nothing of the
	 * kind. A rigid rotation about the anchor satisfies every ball socket at the
	 * velocity level, so energy really is conserved from the first step.
	 */
	auto done = [this]() {
		const Vector3 omega( 0.0f, 1.5f, 0.0f );
		for ( SimBody & b : m_bodies ) {
			if ( b.pinned )
				continue;
			b.w = omega;
			b.v = Vector3::crossproduct( omega, b.x );
		}
		rescaleForJointCount();
		return true;
	};

	if ( name == QLatin1String( "pendulum" ) )   { chain( 1, 1.0f );   return done(); }
	if ( name == QLatin1String( "heavy" ) )      { chain( 1, 60.0f );  return done(); }
	if ( name == QLatin1String( "chain3" ) )     { chain( 3, 1.0f );   return done(); }
	//! a brahmin spine is this deep, and its bodies really do carry these numbers
	if ( name == QLatin1String( "chain8" ) )     { chain( 8, 1.0f );   return done(); }
	if ( name == QLatin1String( "chain8h" ) )    { chain( 8, 300.0f ); return done(); }

	if ( name == QLatin1String( "fork" ) || name == QLatin1String( "forkh" ) ) {
		// one parent, four children -- the brahmin's Spine has exactly that many
		const float ii = ( name == QLatin1String( "forkh" ) ) ? 300.0f : 1.0f;
		addBody( Vector3( 0, 0, -0.25f ), ii, Quat( 1, 0, 0, 0 ) );
		addJoint( 1, 0, Vector3( 0, 0, 0.25f ), Vector3( 0, 0, 0 ) );
		for ( int k = 0; k < 4; k++ ) {
			const float off = -0.45f + 0.3f * k;
			const int me = addBody( Vector3( off, 0, -0.75f ), ii, Quat( 1, 0, 0, 0 ) );
			addJoint( me, 1, Vector3( 0, 0, 0.25f ), Vector3( off, 0, -0.25f ) );

		}
		return done();
	}

	/* A single box dropped flat onto the plane. Contacts are dissipative, so
	 * energy is the wrong test here; what must hold is that it comes to REST
	 * without sinking and without being launched. Eight vertices land at once,
	 * which is exactly the case that lifted the sentry off the ground at 24 m/s
	 * when each contact applied a full correction instead of a share.
	 */
	if ( name == QLatin1String( "box" ) ) {
		m_bodies.clear();
		SimBody b;
		b.x = Vector3( 0, 0, 1.0f );
		b.q = Quat( 1, 0, 0, 0 );
		b.invMass = 1.0f;
		b.linDamping = 0.0f;
		b.angDamping = 0.0f;
		b.invInertia = Vector3( 6, 6, 6 );     // unit cube, 1 kg
		b.bodyId = 0;
		b.shapeCount = 1;
		b.primType = 0;
		for ( int i = 0; i < 8; i++ )
			b.points.append( { Vector3( ( i & 1 ) ? 0.5f : -0.5f, ( i & 2 ) ? 0.5f : -0.5f,
				( i & 4 ) ? 0.5f : -0.5f ), 0.0f } );
		m_bodies.append( b );
		ground = true;
		groundZ = 0.0f;
		rescaleForJointCount();
		return true;
	}

	if ( name == QLatin1String( "spun" ) ) {
		/* Same pendulum, but the bob rests rotated 90 degrees about Y, which maps
		 * local (x,y,z) to (z,y,-x). The pivot is written in the bob's own space
		 * and has to come back to (0,0,0.5) in world, so locally it is
		 * (-0.5,0,0). If the frame maths is wrong this starts violated and never
		 * recovers.
		 */
		const float s = std::sqrt( 0.5f );
		addBody( Vector3( 0, 0, -0.5f ), 1.0f, Quat( s, 0, s, 0 ) );
		addJoint( 1, 0, Vector3( -0.5f, 0, 0 ), Vector3( 0, 0, 0 ) );

		return done();
	}

	return false;
}

float RagdollSim::totalEnergy() const
{
	float e = 0.0f;
	for ( const SimBody & b : m_bodies ) {
		if ( b.pinned || b.invMass <= 0.0f )
			continue;
		const float m = 1.0f / b.invMass;
		e += 0.5f * m * Vector3::dotproduct( b.v, b.v );
		// rotational term, with the tensor back in body space
		const Vector3 wl = qRot( qConj( b.q ), b.w );
		for ( int k = 0; k < 3; k++ )
			if ( b.invInertia[k] > 1.0e-9f )
				e += 0.5f * ( wl[k] * wl[k] ) / b.invInertia[k];
		// potential, measured against gravity
		e -= m * Vector3::dotproduct( gravity, b.x );
	}
	return e;
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

void RagdollSim::setDrag( int body, const Vector3 & localPoint, const Vector3 & target,
	float firmness )
{
	if ( body < 0 || body >= m_bodies.size() ) {
		clearDrag();
		return;
	}
	m_dragBody = body;
	m_dragLocal = localPoint;
	m_dragTarget = target;
	m_dragFirmness = std::clamp( firmness, 1.0e-3f, 1.0f );
	m_dragLambda = 0.0f;
}

void RagdollSim::moveDrag( const Vector3 & target )
{
	m_dragTarget = target;
}

void RagdollSim::setDragOrientation( const Quat & q )
{
	m_dragRotate = true;
	m_dragQ = qNorm( q );
	m_dragQLambda = 0.0f;
}

void RagdollSim::clearDragOrientation()
{
	m_dragRotate = false;
	m_dragQLambda = 0.0f;
}

void RagdollSim::unpinAll()
{
	for ( SimBody & b : m_bodies )
		b.pinned = false;
}

int RagdollSim::addProp( const Vector3 & pos, const Vector3 & vel, float radius, float mass )
{
	if ( !( radius > 0.0f ) || !( mass > 0.0f ) )
		return -1;
	SimBody b;
	b.prop = true;
	b.x = pos;
	b.v = vel;
	b.q = Quat( 1, 0, 0, 0 );
	b.primType = 1;             // a sphere: capA == capB, so the segment test degenerates
	b.shapeCount = 1;           // ...and exactPair accepts it against the rig's capsules
	b.radius = radius;
	b.capA = b.capB = Vector3();
	b.com = Vector3();
	b.restOrigin = pos;
	b.cinfoPos = pos;
	b.points.append( { Vector3(), radius } );
	b.invMass = 1.0f / mass;
	/* Solid sphere: I = 2/5 m r^2 about every axis, so the inverse is isotropic.
	 * Written out rather than left at zero -- a body with no inverse inertia cannot
	 * be spun by an off-centre hit, and a ball that slides without rolling is the
	 * most obviously wrong thing a physics preview can show.
	 */
	const float inv = 1.0f / ( 0.4f * mass * radius * radius );
	b.invInertia = Vector3( inv, inv, inv );
	b.linDamping = 0.05f;
	b.angDamping = 0.05f;
	b.solverScale = 1.0f;       // no joints, so nothing to share a correction with
	b.layer = 1;
	m_bodies.append( b );
	// the hand's strength is measured against the total mass, so a scene with a
	// boulder in it must not leave the drag calibrated for the rig alone
	m_totalMass += mass;
	return int( m_bodies.size() ) - 1;
}

void RagdollSim::clearProps()
{
	if ( m_propStart <= 0 || m_bodies.size() <= m_propStart )
		return;
	// a drag or a pin holding a prop has to go with it, or it would refer to a
	// body index that no longer exists
	if ( m_dragBody >= m_propStart )
		clearDrag();
	for ( int i = m_propStart; i < m_bodies.size(); i++ )
		if ( m_bodies.at( i ).invMass > 0.0f )
			m_totalMass -= 1.0f / m_bodies.at( i ).invMass;
	m_bodies.resize( m_propStart );
	if ( !( m_totalMass > 0.0f ) )
		m_totalMass = 1.0f;
	m_pairs.clear();
}

void RagdollSim::applyImpulse( int body, const Vector3 & localPoint, const Vector3 & impulse )
{
	if ( body < 0 || body >= m_bodies.size() )
		return;
	SimBody & b = m_bodies[body];
	if ( b.pinned || b.invMass <= 0.0f )
		return;
	b.v += impulse * b.invMass;
	const Vector3 r = qRot( b.q, localPoint );
	b.w += applyInvInertia( b, Vector3::crossproduct( r, impulse ) );
}

void RagdollSim::blast( const Vector3 & centre, float radius, float strength )
{
	if ( !( radius > 0.0f ) )
		return;
	for ( int i = 0; i < m_bodies.size(); i++ ) {
		SimBody & b = m_bodies[i];
		if ( b.pinned || b.invMass <= 0.0f )
			continue;
		Vector3 d = b.x - centre;
		const float dist = d.length();
		if ( dist >= radius )
			continue;
		// a body exactly at the centre has no direction to go; push it up rather
		// than dividing by zero or leaving the one body nearest the blast untouched
		const Vector3 dir = ( dist > 1.0e-6f ) ? d / dist : Vector3( 0.0f, 0.0f, 1.0f );
		b.v += dir * ( strength * ( 1.0f - dist / radius ) * b.invMass );
	}
}

void RagdollSim::setVelocity( int body, const Vector3 & v )
{
	if ( body >= 0 && body < m_bodies.size() )
		m_bodies[body].v = v;
}

void RagdollSim::freeze()
{
	for ( SimBody & b : m_bodies ) {
		b.v = Vector3();
		b.w = Vector3();
	}
}

void RagdollSim::clearDrag()
{
	m_dragBody = -1;
	m_dragLambda = 0.0f;
	// the orientation half goes with it: left set, it would hold the NEXT body
	// grabbed at the angle the last one was released at
	clearDragOrientation();
}

Vector3 RagdollSim::toWorld( int body, const Vector3 & localPoint ) const
{
	if ( body < 0 || body >= m_bodies.size() )
		return localPoint;
	const SimBody & b = m_bodies.at( body );
	return b.x + qRot( b.q, localPoint );
}

/*! Nearest body along a ray, against the solver's own sphere set.
 *
 * The shape points are in BONE space and x is the centre of mass, so a point
 * sits at x + q*(p - com) -- the same conversion drawing a capsule needs, and
 * getting it wrong offsets every pick by the com shift, which on a limb is most
 * of its length.
 */
Vector3 RagdollSim::toLocal( int body, const Vector3 & worldPoint ) const
{
	if ( body < 0 || body >= m_bodies.size() )
		return worldPoint;
	const SimBody & b = m_bodies.at( body );
	return qRot( qConj( b.q ), worldPoint - b.x );
}

SimPick RagdollSim::pick( const Vector3 & rayOrigin, const Vector3 & dir ) const
{
	SimPick best;
	const float dl = dir.length();
	if ( !( dl > 1.0e-12f ) )
		return best;
	const Vector3 d = dir / dl;

	for ( int i = 0; i < m_bodies.size(); i++ ) {
		const SimBody & b = m_bodies.at( i );
		for ( const SimBody::SimPoint & sp : b.points ) {
			const Vector3 c = b.x + qRot( b.q, sp.p - b.com );
			/* A polytope contributes its vertices at radius ZERO, which nothing can
			 * ever hit. Give every point a floor so a hull is grabbable at all --
			 * generous on purpose, since a pick wants to be forgiving and a contact
			 * does not.
			 */
			const float r = std::max( sp.r, 0.02f );
			const Vector3 m = c - rayOrigin;
			const float tc = Vector3::dotproduct( m, d );
			const float d2 = Vector3::dotproduct( m, m ) - tc * tc;
			if ( d2 > r * r )
				continue;
			const float thc = std::sqrt( std::max( 0.0f, r * r - d2 ) );
			// the near root, or the far one when the ray starts inside
			float t = tc - thc;
			if ( t < 0.0f )
				t = tc + thc;
			if ( t < 0.0f )
				continue;
			if ( best.body >= 0 && t >= best.distance )
				continue;
			best.body = i;
			best.distance = t;
			best.worldPoint = rayOrigin + d * t;
			best.localPoint = qRot( qConj( b.q ), best.worldPoint - b.x );
		}
	}
	return best;
}

float RagdollSim::dragError() const
{
	if ( m_dragBody < 0 || m_dragBody >= m_bodies.size() )
		return 0.0f;
	const SimBody & b = m_bodies.at( m_dragBody );
	return ( b.x + qRot( b.q, m_dragLocal ) - m_dragTarget ).length();
}

/*! Pull the grabbed point toward the target, XPBD style.
 *
 * The same shape as a ball socket with one side immovable, and it goes through
 * applyPositional so the drag cannot inject energy the joint solve would not.
 * The compliance term is what makes it a spring rather than a pin: alpha scales
 * as 1/h^2, so the correction a substep applies is bounded by the stiffness
 * rather than by the size of the gap, and yanking the mouse across the screen
 * pulls the limb along instead of teleporting the whole rig.
 *
 * A pinned body is left alone -- the caller pinned it on purpose, and a drag
 * that quietly overrode that would be worse than one that does nothing.
 */
void RagdollSim::solveDrag( float h )
{
	if ( m_dragBody < 0 || m_dragBody >= m_bodies.size() || !( h > 0.0f ) )
		return;
	SimBody & b = m_bodies[m_dragBody];
	if ( b.pinned )
		return;

	const Vector3 r = qRot( b.q, m_dragLocal );
	const Vector3 diff = m_dragTarget - ( b.x + r );
	const float c = diff.length();
	if ( !( c > 1.0e-9f ) )
		return;
	const Vector3 n = diff / c;

	const float w = genInvMass( b, r, n );
	if ( !( w > 1.0e-12f ) )
		return;
	/* alpha scaled BY the body's own inverse mass, so firmness means the same
	 * thing on every bone. A fixed compliance divided by h^2 instead makes the
	 * grab's strength depend on the body's mass and on the substep count, which
	 * is how the first version came to lag two thirds of the pull.
	 */
	const float alpha = w * ( 1.0f - m_dragFirmness ) / m_dragFirmness;
	float dLambda = ( c - alpha * m_dragLambda ) / ( w + alpha );

	/* Bounded by what a hand could actually pull with.
	 *
	 * The correction this applies is a position impulse: displacement is
	 * dLambda * invMass, so dLambda = F * h^2 for a force F. Capping the
	 * ACCUMULATED lambda therefore caps the force, and once the chain is taut the
	 * hand pulls at its limit and the rig follows as a whole instead of a joint
	 * opening up. See RagdollSim::dragStrength.
	 */
	if ( dragStrength > 0.0f && b.invMass > 0.0f ) {
		/* Measured against the WHOLE rig's mass, not the held body's.
		 *
		 * Against its own weight, dragging the root barely moved: the root is the
		 * hub, so pulling it pulls all 39 bodies, and 25x the weight of one of them
		 * is nowhere near enough to shift the lot. A limb, whose neighbours mostly
		 * hang off it anyway, felt fine -- which is why this looked right until the
		 * COM was tried.
		 *
		 * It stays scale-free either way, and the taut behaviour is unaffected:
		 * what stops a joint tearing is that the force is FINITE and the joints
		 * solve after it, not the particular number.
		 */
		const float maxLambda = dragStrength * m_totalMass * 9.81f * h * h;
		if ( m_dragLambda + dLambda > maxLambda )
			dLambda = std::max( 0.0f, maxLambda - m_dragLambda );
	}
	m_dragLambda += dLambda;

	/* Applied directly rather than through applyPositional, which is the shared
	 * helper the joints use. That helper performs its OWN 1/w solve, so handing it
	 * a dLambda that already carries the 1/(w + alpha) factor divides by the
	 * inverse mass twice -- a 3x overshoot on a body of a few kg, which pushed the
	 * brahmin to 185 m/s. The soft first default hid it by making dLambda tiny.
	 *
	 * What follows is exactly applyPositional's single-body branch with the
	 * impulse supplied rather than recomputed.
	 */
	const Vector3 p = n * dLambda;
	b.x += p * ( b.invMass * b.solverScale );
	b.q = qIntegrate( b.q, capRotation( applyInvInertia( b, Vector3::crossproduct( r, p ) ) ), 1.0f );
}

/*! Turn the held body toward the orientation the hand is asking for.
 *
 * The angular twin of solveDrag, and deliberately built the same way: a
 * compliant XPBD correction scaled by the body's own inverse inertia, so
 * "firmness" means the same thing on a jaw and on a Liberty Prime torso. One
 * side is the immovable hand, so there is no second body to share with.
 *
 * The correction is the shortest arc from the current pose to the target,
 * which is what the sign flip is for: a quaternion and its negation are the
 * same orientation, and taking the difference without choosing between them
 * makes the hand rotate a bone the long way round about half the time.
 */
void RagdollSim::solveDragOrientation( float h )
{
	if ( !m_dragRotate || m_dragBody < 0 || m_dragBody >= m_bodies.size() || !( h > 0.0f ) )
		return;
	SimBody & b = m_bodies[m_dragBody];
	if ( b.pinned )
		return;

	Quat dq = qMul( m_dragQ, qConj( b.q ) );
	if ( dq[0] < 0.0f )
		dq = Quat( -dq[0], -dq[1], -dq[2], -dq[3] );
	const Vector3 axis( dq[1], dq[2], dq[3] );
	const float s = axis.length();
	if ( !( s > 1.0e-9f ) )
		return;
	const float theta = 2.0f * std::atan2( s, dq[0] );
	const Vector3 n = axis / s;

	const float w = Vector3::dotproduct( n, applyInvInertia( b, n ) );
	if ( !( w > 1.0e-12f ) )
		return;
	const float alpha = w * ( 1.0f - m_dragFirmness ) / m_dragFirmness;
	const float dLambda = ( theta - alpha * m_dragQLambda ) / ( w + alpha );
	m_dragQLambda += dLambda;
	// capRotation bounds the step to where the linearised quaternion update is
	// still valid; the sweeps that follow finish whatever this one left
	b.q = qIntegrate( b.q, capRotation( applyInvInertia( b, n * dLambda ) ), 1.0f );
}

/*! Put the bounce back, as a velocity pass after the position solve.
 *
 * XPBD resolves a contact by moving the body out of the surface, which removes
 * the approach velocity as a side effect -- every landing is perfectly dead.
 * Restitution therefore cannot be read off the post-solve state: by then the
 * body already IS at rest. It uses the speed recorded at the top of the substep
 * instead, which is the speed it genuinely arrived with.
 *
 * Only contacts that were closing get a bounce. A body resting on the floor is
 * touching it on every substep for ever, and reflecting that would feed it
 * energy from nothing and walk it off the ground.
 */
void RagdollSim::applyRestitution()
{
	if ( !( restitution > 0.0f ) )
		return;

	auto bounce = []( SimBody & B, const Vector3 & r, const Vector3 & n, float want ) {
		const float have = Vector3::dotproduct( B.v + Vector3::crossproduct( B.w, r ), n );
		if ( have >= want )
			return;
		const float w = genInvMass( B, r, n );
		if ( !( w > 1.0e-12f ) )
			return;
		const Vector3 p = n * ( ( want - have ) / w );
		B.v += p * ( B.invMass * B.solverScale );
		B.w += applyInvInertia( B, Vector3::crossproduct( r, p ) );
	};

	for ( const SimContact & c : std::as_const( m_contacts ) ) {
		if ( c.a < 0 || c.a >= m_bodies.size() )
			continue;
		SimBody & A = m_bodies[c.a];

		if ( c.b < 0 ) {
			if ( A.pinned || A.invMass <= 0.0f )
				continue;
			const float vnPre = Vector3::dotproduct(
				A.vPre + Vector3::crossproduct( A.wPre, c.rA ), c.n );
			// only what was closing: a body resting on the floor is in contact on
			// every substep for ever, and reflecting that would feed it energy from
			// nothing and walk it off the ground
			if ( vnPre >= 0.0f )
				continue;
			bounce( A, c.rA, c.n, -restitution * vnPre );
			continue;
		}

		if ( c.b >= m_bodies.size() )
			continue;
		SimBody & B = m_bodies[c.b];
		const float vnPre = Vector3::dotproduct(
			( A.vPre + Vector3::crossproduct( A.wPre, c.rA ) )
			- ( B.vPre + Vector3::crossproduct( B.wPre, c.rB ) ), c.n );
		if ( vnPre >= 0.0f )
			continue;
		const float want = -restitution * vnPre;
		if ( !A.pinned && A.invMass > 0.0f )
			bounce( A, c.rA, c.n, want );
		if ( !B.pinned && B.invMass > 0.0f )
			bounce( B, c.rB, -c.n, want );
	}
}

/*! Solve one joint once. Returns true if it was still meaningfully violated.
 *
 * The caller uses that to spend extra sweeps only where they are needed. A joint
 * doing its job reports false on the first pass and costs exactly what it did
 * before, which is the point: every previous attempt at helping the stiff joints
 * -- a higher global sweep count, a heavier scaling rule -- paid for it by
 * disturbing the thirty-odd ragdolls that were already correct.
 */
bool RagdollSim::solveOneJoint( const SimJoint & j )
{
	{
		SimBody & A = m_bodies[j.a];
		SimBody & B = m_bodies[j.b];
		bool busy = false;

		// --- ball socket: the two attachment points must coincide -------------
		const Vector3 rA = qRot( A.q, j.pivotA );
		const Vector3 rB = qRot( B.q, j.pivotB );
		const Vector3 sep = ( B.x + rB ) - ( A.x + rA );
		// a millimetre of separation, and five degrees on the angular tests below.
		// Tight tolerances here do not mean precision, they mean treating healthy
		// joints as sick.
		if ( sep.length() > 0.001f )
			busy = true;
		applyPositional( A, B, rA, rB, sep );

		if ( !angularLimits )
			return busy;

		// --- angular limits ---------------------------------------------------
		const JointAxes ax = jointAxes( A, B, j );
		const Vector3 a0 = ax.a0, a1 = ax.a1, b0 = ax.b0, b1 = ax.b1, b2 = ax.b2;

		/* Swing (cone) limit: how far the child's twist axis may lean off the
		 * parent's. The rotation axis is the one perpendicular to both, i.e.
		 * a0 x b0, whose length is already sin(angle). Havok writes -100 as
		 * "no lower bound" on a cone, so only max carries information.
		 */
		if ( j.cone.present && useCone ) {
			Vector3 sw = Vector3::crossproduct( a0, b0 );
			if ( sw.length() > 1.0e-6f ) {
				sw.normalize();
				busy |= FORCED( limitAngle( A, B, sw, a0, b0, -j.cone.max, j.cone.max ) );
			}
		}
		/* Plane limit: how far the child's twist axis may leave the parent's
		 * plane, whose normal is the parent's plane axis.
		 *
		 * The old reading -- the angle from a0 to b0 measured about b2 -- was not a
		 * well defined signed angle at all, since neither a0 nor b0 is
		 * perpendicular to b2. It mixed the cone angle into the plane reading and
		 * the two limits then spent every substep undoing each other: on its own
		 * this limit took the brahmin to 4.7 million units of energy. Constraining
		 * the angle between a0 and the normal instead is the same quantity the cone
		 * limit already handles correctly, only measured against a different axis,
		 * so it reuses that machinery.
		 */
		if ( j.plane.present && usePlane ) {
			Vector3 pax = Vector3::crossproduct( a0, b1 );
			if ( pax.length() > 1.0e-6f ) {
				pax.normalize();
				// HALF_PI comes from niftypes.h
				busy |= FORCED( limitAngle( A, B, pax, a0, b1,
					float( HALF_PI ) - j.plane.max, float( HALF_PI ) - j.plane.min ) );
			}
		}
		Q_UNUSED( b2 )

		/* Twist about the shared axis. The swing axes MUST be projected
		 * perpendicular to it first: measuring the angle between un-projected
		 * axes mixes swing into the twist reading and the two limits then fight
		 * each other every substep.
		 */
		if ( j.twist.present && useTwist ) {
			/* The twist axis is the bisector of the two frames' own axes, which
			 * degenerates as they approach opposite. Bail out well before that:
			 * a bisector recovered from two nearly cancelling vectors is mostly
			 * rounding error, and twisting about a random axis is a good way to
			 * launch a limb. 0.1 is a swing of about 168 degrees, far outside any
			 * cone a ragdoll actually authorises.
			 */
			Vector3 n = a0 + b0;
			if ( n.length() > 0.1f ) {
				n.normalize();
				Vector3 n1 = a1 - n * Vector3::dotproduct( n, a1 );
				Vector3 n2 = b1 - n * Vector3::dotproduct( n, b1 );
				if ( n1.length() > 1.0e-6f && n2.length() > 1.0e-6f ) {
					n1.normalize();
					n2.normalize();
					busy |= FORCED( limitAngle( A, B, n, n1, n2, j.twist.min, j.twist.max ) );
				}
			}
		}
		/* A limited hinge: hold the two axles aligned, then limit the swing about
		 * them.
		 *
		 * a1 must be projected perpendicular to the axle before the angle is
		 * measured. It is only perpendicular to a0, and a0 equals b0 solely once
		 * the alignment above has converged -- which it has not, since these axes
		 * were sampled before it ran. Measuring against an unprojected a1
		 * therefore mixes the misalignment into the swing reading, and the two
		 * corrections spend every sweep undoing each other. That is what the
		 * eyebot's seven antenna hinges were doing: 5,681 units of energy from the
		 * hinges alone against 0.9 from the plane limits. b1 needs no projection,
		 * being perpendicular to b0 by construction.
		 */
		if ( j.hinge.present && useHinge ) {
			const Vector3 align = Vector3::crossproduct( a0, b0 );
			if ( align.length() > FORCED_RAD )
				busy = true;
			applyAngular( A, B, align );
			// 0.1, not 1e-6: a nearly cancelling projection is rounding error, and
			// the same trap as the twist bisector
			Vector3 n1 = a1 - b0 * Vector3::dotproduct( b0, a1 );
			if ( n1.length() > 0.1f ) {
				n1.normalize();
				busy |= FORCED( limitAngle( A, B, b0, n1, b1, j.hinge.min, j.hinge.max ) );
			}
		}
		return busy;
	}
}

void RagdollSim::solveJoints( float h, bool reverse )
{
	Q_UNUSED( h )
	/* One pass over every joint, then extra passes only over the joints that are
	 * still fighting. A near-welded hinge -- the workshop turret's pelvis is
	 * limited to one degree -- needs several passes to settle where a shoulder
	 * needs one, and spending them globally is what broke the sentry.
	 *
	 * Sweeping FORWARD ONLY, though. Alternating direction each iteration is
	 * textbook symmetric Gauss-Seidel and propagates a correction both up and down
	 * a chain instead of one joint per pass, which ought to help anything held at
	 * an extremity -- and it measures worse: 32 of 37 ragdolls settling against 36,
	 * with Liberty Prime going from 1.5 m/s to 20.9. It was also not needed. The
	 * 0.41 m separations that prompted it came from a test that pinned the root AND
	 * dragged a forearm past the arm's reach, which no solver can satisfy.
	 */
	Q_UNUSED( reverse )
	const int n = m_joints.size();
	for ( int idx = 0; idx < n; idx++ ) {
		const SimJoint & j = m_joints.at( idx );
		for ( int k = 0; k < std::max( 1, stiffIterations ); k++ )
			if ( !solveOneJoint( j ) )
				break;
	}
}

void RagdollSim::step( float dt, int substeps )
{
	if ( m_bodies.isEmpty() || substeps < 1 )
		return;
	const float h = dt / float( substeps );

	// broad phase once per step, not per substep: bodies move a fraction of a
	// millimetre in a substep, so the candidate set cannot meaningfully change
	collectPairs();

	for ( int s = 0; s < substeps; s++ ) {
		for ( SimBody & b : m_bodies ) {
			b.xPrev = b.x;
			b.qPrev = b.q;
			// before gravity, and before the solve removes it: this is the speed
			// the body genuinely arrives with, which is what a bounce reflects
			b.vPre = b.v;
			b.wPre = b.w;
			if ( b.pinned )
				continue;
			b.v += gravity * h;
			if ( wind.length() > 0.0f )
				b.v += wind * ( b.invMass * h );
			// each body's own authored damping, plus whatever the caller added
			b.v *= std::clamp( 1.0f - ( b.linDamping + damping ) * h, 0.0f, 1.0f );
			b.w *= std::clamp( 1.0f - ( b.angDamping + damping ) * h, 0.0f, 1.0f );
			b.x += b.v * h;
			b.q = qIntegrate( b.q, b.w, h );
		}

		m_dragLambda = 0.0f;
		m_dragQLambda = 0.0f;
		/* A live drag gets extra joint sweeps.
		 *
		 * Letting the hand pull hard enough to shift a whole rig -- which it must,
		 * or the root cannot be dragged at all -- widened the worst joint
		 * separation from 4 mm to 19 mm. The answer is not to weaken the hand again
		 * but to let the joints keep up with it, since what tears a chain is the
		 * solver running out of sweeps, not the force itself. Only while something
		 * is actually held, so an idle rig costs nothing.
		 */
		const int sweeps = std::max( 1, iterations ) * ( m_dragBody >= 0 ? 3 : 1 );
		/* The drag is solved BEFORE the joints, so the joints get the last word.
		 *
		 * With it last, a hand hauling a limb away from a pinned root yanked the
		 * body clear and the joints only got to answer on the next iteration --
		 * hauling a brahmin left 1.9 m of ball-socket separation, which is the rig
		 * coming apart rather than a chain going taut. A joint is a point
		 * constraint; nothing the user does should be able to out-pull it.
		 */
		for ( int it = 0; it < sweeps; it++ ) {
			solveDrag( h );
			solveDragOrientation( h );
			solveJoints( h, ( it & 1 ) != 0 );
			/* The FIRST sweep keeps its contacts, not the last.
			 *
			 * Each sweep removes some of the overlap, so by the last one there is
			 * usually none left to see -- which is the same reason the velocity pass
			 * cannot re-derive them itself. The first sweep sees the impact as it
			 * arrived, which is the geometry a bounce should come off.
			 */
			solveContacts( h, restitution > 0.0f && it == 0 );
		}

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

		// after the velocities are back, never before: see applyRestitution
		applyRestitution();
	}
}

QVector<SimLimitCheck> RagdollSim::checkLimits() const
{
	QVector<SimLimitCheck> out;
	out.reserve( m_joints.size() );
	for ( int i = 0; i < m_joints.size(); i++ ) {
		const SimJoint & j = m_joints.at( i );
		const SimBody & A = m_bodies.at( j.a );
		const SimBody & B = m_bodies.at( j.b );
		const JointAxes ax = jointAxes( A, B, j );

		SimLimitCheck c;
		c.joint = i;
		c.child = j.a;
		c.parent = j.b;
		/* A tenth of a degree of slack, so a twist-locked joint -- bounds of
		 * exactly [0, 0], which several tail links carry -- is not reported as
		 * broken every time the measured angle lands on 1e-6 instead of 0. The
		 * solver itself uses no such slack; this is only about what is worth
		 * showing someone.
		 */
		const float tol = 0.1f / 57.2957795f;

		if ( j.cone.present ) {
			Vector3 sw = Vector3::crossproduct( ax.a0, ax.b0 );
			if ( sw.length() > 1.0e-6f ) {
				sw.normalize();
				c.cone = signedAngle( sw, ax.a0, ax.b0 );
			}
			c.coneBad = ( c.cone < -j.cone.max - tol || c.cone > j.cone.max + tol );
		}
		if ( j.plane.present ) {
			// reported as the angle out of the plane, so it reads the same way
			// round as the decoded bounds rather than as its complement
			Vector3 pax = Vector3::crossproduct( ax.a0, ax.b1 );
			if ( pax.length() > 1.0e-6f ) {
				pax.normalize();
				c.plane = float( HALF_PI ) - signedAngle( pax, ax.a0, ax.b1 );
			}
			c.planeBad = ( c.plane < j.plane.min - tol || c.plane > j.plane.max + tol );
		}
		if ( j.twist.present ) {
			Vector3 n = ax.a0 + ax.b0;
			if ( n.length() > 1.0e-6f ) {
				n.normalize();
				Vector3 n1 = ax.a1 - n * Vector3::dotproduct( n, ax.a1 );
				Vector3 n2 = ax.b1 - n * Vector3::dotproduct( n, ax.b1 );
				if ( n1.length() > 1.0e-6f && n2.length() > 1.0e-6f ) {
					n1.normalize();
					n2.normalize();
					c.twist = signedAngle( n, n1, n2 );
				}
			}
			c.twistBad = ( c.twist < j.twist.min - tol || c.twist > j.twist.max + tol );
		}
		if ( j.hinge.present ) {
			// same projection the solver uses, or the report would disagree with it
			Vector3 n1 = ax.a1 - ax.b0 * Vector3::dotproduct( ax.b0, ax.a1 );
			if ( n1.length() > 1.0e-6f ) {
				n1.normalize();
				c.hinge = signedAngle( ax.b0, n1, ax.b1 );
			}
			c.hingeBad = ( c.hinge < j.hinge.min - tol || c.hinge > j.hinge.max + tol );
		}
		out.append( c );
	}
	return out;
}

QVector<SimPoseCheck> RagdollSim::checkPoseFromJoints() const
{
	QVector<SimPoseCheck> out( m_bodies.size() );
	for ( int i = 0; i < m_bodies.size(); i++ )
		out[i].body = i;

	QVector<Vector3> pos( m_bodies.size() );
	QVector<Quat> rot( m_bodies.size(), Quat( 1, 0, 0, 0 ) );

	// a root is a body that is nobody's child; seed it from the reference pose so
	// the comparison measures the SHAPE of the reconstruction, not where it sits
	QVector<bool> isChild( m_bodies.size(), false );
	for ( const SimJoint & j : m_joints )
		if ( j.a >= 0 && j.a < isChild.size() )
			isChild[j.a] = true;
	for ( int i = 0; i < m_bodies.size(); i++ ) {
		if ( isChild.at( i ) )
			continue;
		pos[i] = m_bodies.at( i ).x;
		rot[i] = m_bodies.at( i ).q;
		out[i].placed = true;
	}

	/* Place children until nothing more can be placed, propagating POSITION only.
	 *
	 * A ball socket pins where the child sits, not how it is turned -- that is
	 * three free rotational degrees of freedom, which is the entire point of the
	 * joint. An earlier version of this also derived the child's orientation by
	 * assuming the two joint frames coincide, i.e. that every joint rests at its
	 * own zero. Nothing requires that, and it duly reported the deathclaw's pose
	 * as 0.74 m and 50 degrees out when the deathclaw simulates perfectly. So take
	 * the orientations from the reference pose -- which cinfo independently
	 * corroborates to 0.1 degrees -- and let the pivots alone say where bodies go.
	 *
	 * Iterating rather than recursing keeps it safe against a binding order that
	 * lists a child before its parent, and against a cycle.
	 */
	for ( int i = 0; i < m_bodies.size(); i++ )
		rot[i] = m_bodies.at( i ).q;
	for ( bool progress = true; progress; ) {
		progress = false;
		for ( const SimJoint & j : m_joints ) {
			if ( j.a < 0 || j.b < 0 || out.at( j.a ).placed || !out.at( j.b ).placed )
				continue;
			pos[j.a] = pos.at( j.b ) + qRot( rot.at( j.b ), j.pivotBRaw )
				- qRot( rot.at( j.a ), j.pivotA );
			out[j.a].placed = true;
			progress = true;
		}
	}

	for ( int i = 0; i < m_bodies.size(); i++ ) {
		if ( !out.at( i ).placed )
			continue;
		const SimBody & b = m_bodies.at( i );
		out[i].posDiff = ( pos.at( i ) - b.x ).length();
		const Quat & p = rot.at( i );
		const float dot = p[0] * b.q[0] + p[1] * b.q[1] + p[2] * b.q[2] + p[3] * b.q[3];
		out[i].rotDiffDeg = 2.0f * std::acos( std::clamp( std::fabs( dot ), 0.0f, 1.0f ) )
			* 57.2957795f;
	}
	return out;
}

float RagdollSim::lowestPoint() const
{
	float lowest = 0.0f;
	bool any = false;
	for ( const SimBody & b : m_bodies ) {
		if ( !hasGeometry( b ) )
			continue;
		float z = b.x[2];
		for ( const SimBody::SimPoint & sp : b.points )
			z = std::min( z, worldPoint( b, sp )[2] - sp.r );
		lowest = any ? std::min( lowest, z ) : z;
		any = true;
	}
	return lowest;
}

int RagdollSim::looseBodies() const
{
	QVector<bool> jointed( m_bodies.size(), false );
	for ( const SimJoint & j : m_joints ) {
		if ( j.a >= 0 && j.a < jointed.size() ) jointed[j.a] = true;
		if ( j.b >= 0 && j.b < jointed.size() ) jointed[j.b] = true;
	}
	int n = 0;
	for ( int i = 0; i < m_bodies.size(); i++ )
		// a prop is jointless by definition, and counting it here would report a
		// clean ragdoll as a kit the moment a ball was thrown at it
		if ( !jointed.at( i ) && !m_bodies.at( i ).pinned && !m_bodies.at( i ).prop )
			n++;
	return n;
}

void RagdollSim::pinLooseBodies()
{
	QVector<bool> jointed( m_bodies.size(), false );
	for ( const SimJoint & j : m_joints ) {
		if ( j.a >= 0 && j.a < jointed.size() ) jointed[j.a] = true;
		if ( j.b >= 0 && j.b < jointed.size() ) jointed[j.b] = true;
	}
	for ( int i = 0; i < m_bodies.size(); i++ )
		// props excepted, for the same reason: this is for quieting a kit file's
		// unattached parts, and nailing down the ball you just threw is not that
		if ( !jointed.at( i ) && !m_bodies.at( i ).prop )
			m_bodies[i].pinned = true;
}

SimStats RagdollSim::stats() const
{
	SimStats st;
	for ( int i = 0; i < m_bodies.size(); i++ ) {
		const SimBody & b = m_bodies.at( i );
		if ( b.pinned || b.invMass <= 0.0f )
			continue;
		const float m = 1.0f / b.invMass;
		const float sp = b.v.length();
		st.energy += 0.5f * m * sp * sp;
		if ( sp > st.maxSpeed || st.worstBody < 0 ) {
			st.maxSpeed = sp;
			st.worstBody = i;
		}
		if ( !std::isfinite( b.x[0] ) || !std::isfinite( b.x[1] ) || !std::isfinite( b.x[2] )
			|| !std::isfinite( sp ) )
			st.diverged = true;
	}
	for ( int i = 0; i < m_joints.size(); i++ ) {
		const SimJoint & j = m_joints.at( i );
		const SimBody & A = m_bodies.at( j.a );
		const SimBody & B = m_bodies.at( j.b );
		const Vector3 pa = A.x + qRot( A.q, j.pivotA );
		const Vector3 pb = B.x + qRot( B.q, j.pivotB );
		const float e = ( pa - pb ).length();
		if ( e > st.maxJointError || st.worstJoint < 0 ) {
			st.maxJointError = e;
			st.worstJoint = i;
		}
	}
	/* Penetration is measured over the same pairs the solver was given, so the
	 * report cannot flatter the solver by checking a different set. Recomputed
	 * from the live poses rather than from whatever the last substep left behind.
	 */
	for ( const SimPair & pr : m_pairs ) {
		const SimBody & A = m_bodies.at( pr.a );
		Vector3 p1, q1;
		worldSegment( A, p1, q1 );
		float depth = 0.0f;
		if ( pr.b < 0 ) {
			for ( const SimBody::SimPoint & sp : A.points )
				depth = std::max( depth, groundZ + sp.r - worldPoint( A, sp )[2] );
		} else {
			const SimBody & B = m_bodies.at( pr.b );
			Vector3 p2, q2, c1, c2;
			worldSegment( B, p2, q2 );
			closestPtSegSeg( p1, q1, p2, q2, c1, c2 );
			depth = A.radius + B.radius - ( c1 - c2 ).length();
		}
		if ( depth > 0.0f ) {
			st.contacts++;
			st.maxPenetration = std::max( st.maxPenetration, depth );
		}
	}

	if ( !std::isfinite( st.maxJointError ) || st.maxSpeed > 1.0e4f )
		st.diverged = true;
	return st;
}
