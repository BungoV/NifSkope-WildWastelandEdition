#ifndef RAGDOLLSIM_H
#define RAGDOLLSIM_H

#include "data/niftypes.h"
#include "gl/hknpdecode.h"

#include <QString>
#include <QVector>

/*! A small rigid-body solver for previewing decoded collision.
 *
 * Built straight from an HknpSystem, so it parses nothing itself: bodies come
 * from the decoded shapes and per-body physics, joints from the decoded ragdoll
 * constraints, and the angular limits are the ones recovered in WW_CHANGES
 * 07-28a. That is the whole point of writing our own rather than taking Bullet --
 * btConeTwistConstraint only approximates Havok's twist+cone, and we know
 * Havok's exact numbers.
 *
 * Method is XPBD (extended position-based dynamics, Muller et al.) with
 * substepping, not sequential impulses. Stiff ragdoll joints are where impulse
 * solvers jitter and blow up; XPBD stays stable at large stiffness because it
 * corrects positions and derives velocities afterwards. Dragging a bone also
 * falls straight out of the formulation -- move a body, let the solver resolve.
 *
 * Everything here works in the ragdoll's own space, which is Havok METRES. Cloth
 * is in game units and is ~70x different; convert at the edges, not in here.
 */

//! One simulated rigid body. Shape is kept for drawing and later collision.
struct SimBody
{
	Vector3 x;                  //!< centre of mass, world space
	Quat q;                     //!< orientation, w first (NifSkope's Quat order)
	Vector3 v, w;               //!< linear and angular velocity

	float invMass = 0.0f;       //!< 0 for a pinned or static body
	Vector3 invInertia;         //!< diagonal of the inverse inertia, body space
	bool pinned = false;        //!< held in place (the root, or a dragged bone)

	//! shape, copied from the decode so the solver needs no back-reference
	int primType = 0;           //!< 1 sphere, 2 capsule (HknpShape::primType)
	Vector3 capA, capB;
	float radius = 0.0f;

	int bodyId = -1;            //!< the decoded body id, for mapping back to nodes

	Vector3 xPrev;              //!< XPBD scratch: pose at the start of a substep
	Quat qPrev;
};

/*! One joint: a ball socket plus whichever angular limits the constraint carried.
 *
 * a is the CHILD and b the PARENT, matching both the binding entry's own order
 * and the measured frame assignment (WW_CHANGES 07-28b) -- getting this backwards
 * silently mirrors every joint.
 */
struct SimJoint
{
	int a = -1, b = -1;
	Vector3 pivotA, pivotB;     //!< joint position in each body's local space
	Quat frameA, frameB;        //!< joint basis in each body's local space

	HknpAngLimit twist, cone, plane, hinge;
};

//! Aggregate health of a run, so stability is measured rather than eyeballed.
struct SimStats
{
	float energy = 0.0f;        //!< kinetic, for spotting blow-up
	float maxJointError = 0.0f; //!< worst ball-socket separation, world units
	float maxSpeed = 0.0f;
	int steps = 0;
	bool diverged = false;      //!< a NaN or a runaway appeared
};

class RagdollSim
{
public:
	/*! Build from a decoded system. Returns false and sets error if the system
	 *  carries no usable bodies. */
	bool build( const HknpSystem & sys, QString * error = nullptr );

	//! Advance by dt, split into substeps (XPBD wants many small ones).
	void step( float dt, int substeps = 8 );

	//! Pin/unpin a body — this is what dragging a bone will use.
	void setPinned( int body, bool pinned );
	//! Move a pinned body directly (the drag handle).
	void setPosition( int body, const Vector3 & pos );

	/*! Replace the contents with a two-body pendulum of known masses.
	 *
	 * A 38-joint ragdoll is the wrong place to debug a solver. This is the
	 * smallest case that exercises the same code path -- one ball socket, one
	 * free body, one pinned anchor -- and with damping off its total energy is a
	 * conserved quantity, so any drift is the solver's own error and nothing else.
	 */
	void buildTestPendulum();

	//! Total energy (kinetic + potential). Constant for the pendulum above.
	float totalEnergy() const;

	SimStats stats() const;

	const QVector<SimBody> & bodies() const { return m_bodies; }
	const QVector<SimJoint> & joints() const { return m_joints; }
	QVector<SimBody> & bodies() { return m_bodies; }

	//! Metres per second squared, negative Z (Havok space). Settable for tests.
	Vector3 gravity = Vector3( 0.0f, 0.0f, -9.81f );
	//! Velocity damping per second, keeps a settling ragdoll from ringing.
	float damping = 0.02f;
	//! Solve the angular limits as well as the ball sockets. Off isolates the
	//! two halves when a run misbehaves -- a pure ball-socket tree is a solved
	//! problem, so if that alone is unstable the fault is not in the limits.
	bool angularLimits = true;

private:
	void solveJoints( float h );
	QVector<SimBody> m_bodies;
	QVector<SimJoint> m_joints;
};

#endif // RAGDOLLSIM_H
