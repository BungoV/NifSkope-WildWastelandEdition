#ifndef RAGDOLLSIM_H
#define RAGDOLLSIM_H

#include "data/niftypes.h"
#include "gl/hknpdecode.h"

#include <QSet>
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

	/*! 1 / (number of joints on this body) -- Mueller's mass splitting.
	 *
	 * A body carrying several joints is solved once per joint per sweep, and with
	 * a real body's inverse inertia each of those corrections rotates it enough to
	 * disturb its OTHER joints by more than the error just removed. Five joints on
	 * one pelvis put the round-trip gain above one and the ragdoll flies apart.
	 * Dividing the body's response by its joint count makes the corrections sum to
	 * what a single joint would have applied, which pulls the gain back under one.
	 * Kept separate from invMass/invInertia so the energy report still uses the
	 * true values.
	 */
	float solverScale = 1.0f;

	//! shape, copied from the decode so the solver needs no back-reference.
	//! capA/capB stay in BONE space; x is the centre of mass, so drawing them
	//! means x + q*(capA - com).
	int primType = 0;           //!< 1 sphere, 2 capsule (HknpShape::primType)
	Vector3 capA, capB;
	float radius = 0.0f;
	int shapeCount = 0;         //!< a body may carry several shapes

	/*! Every shape this body carries, reduced to spheres in bone space.
	 *
	 * A body is NOT one shape. Liberty Prime's torso is a capsule plus two
	 * polytopes and the workshop turret's body 1 is four polytopes; keeping only
	 * the last one, as the first version did, lost most of the machine's geometry
	 * and left it falling through the floor at terminal velocity while being
	 * reported as an unstable solver.
	 *
	 * A capsule contributes its two end points at its radius, a sphere its centre,
	 * a polytope its vertices at radius zero. That is exact against a plane and
	 * for capsule/sphere pairs, and approximate for a polytope's faces -- a
	 * vertex set has no faces between the vertices. Good enough for ground
	 * contact, which is what a preview needs; noted rather than hidden.
	 */
	struct SimPoint { Vector3 p; float r = 0.0f; };
	QVector<SimPoint> points;
	Vector3 com;                //!< centre of mass in bone space, for that conversion
	Vector3 restOrigin;         //!< the bone origin in world, before that shift
	//! what cinfo itself says about this body, for checking against the skeleton
	Vector3 cinfoPos;
	Quat cinfoRot = Quat( 1, 0, 0, 0 );

	int bodyId = -1;            //!< the decoded body id, for mapping back to nodes

	//! Havok's collision filter, carried so self-collision honours what the file
	//! actually authorises rather than a guess
	quint32 layer = 1;
	quint32 filterGroup = 0;
	quint8 filterFlags = 0;

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
	//! Havok class name, kept so a misbehaving joint can be attributed to a
	//! constraint TYPE rather than just to an index
	QString kind;
	//! true if pivotB had to be derived from the rest pose, see build()
	bool rebased = false;
	//! pivotB exactly as the file stored it, kept because the pose reconstruction
	//! has to use the file's own numbers -- feeding it a rebased pivot would make
	//! it agree with the rest pose by construction and prove nothing
	Vector3 pivotBRaw;

	HknpAngLimit twist, cone, plane, hinge;
};

/*! One overlapping pair, found by the broad phase and re-measured each substep.
 *
 * Only the pairing is cached. The geometry is recomputed from the live poses
 * every substep, because a contact point frozen at the start of a step is wrong
 * by the end of it and pushes bodies in a direction they have already left.
 */
struct SimPair
{
	int a = -1, b = -1;         //!< b < 0 means the ground plane
};

//! Aggregate health of a run, so stability is measured rather than eyeballed.
struct SimStats
{
	float maxPenetration = 0.0f;    //!< deepest overlap left unresolved, world units
	int contacts = 0;               //!< how many pairs were actually touching
	float energy = 0.0f;        //!< kinetic, for spotting blow-up
	float maxJointError = 0.0f; //!< worst ball-socket separation, world units
	float maxSpeed = 0.0f;
	int steps = 0;
	bool diverged = false;      //!< a NaN or a runaway appeared
	//! who is misbehaving -- a blow-up always starts somewhere specific, and
	//! knowing where turns "the ragdoll exploded" into one joint to look at
	int worstBody = -1;
	int worstJoint = -1;
};

/*! One joint's angular limits measured against the pose it is actually in.
 *
 * The rest pose is a ragdoll's neutral stance, so every limit should be
 * comfortably satisfied there. A limit reported violated at rest means the
 * decoded bounds and the measured angle are not in the same convention -- and
 * the solver will then apply a fixed correction on every substep for ever, which
 * reads out as velocity proportional to 1/h and blows the ragdoll up.
 */
struct SimLimitCheck
{
	int joint = -1;
	int child = -1, parent = -1;
	//! measured angle, radians; NaN where the constraint carries no such limit
	float twist = 0.0f, cone = 0.0f, plane = 0.0f, hinge = 0.0f;
	bool twistBad = false, coneBad = false, planeBad = false, hingeBad = false;
	bool any() const { return twistBad || coneBad || planeBad || hingeBad; }
};

/*! One body's pose as the CONSTRAINTS describe it, against the pose it is in.
 *
 * Walking the joint tree from its root, each constraint fully determines where
 * its child must sit relative to its parent: the two joint frames have to
 * coincide. So the constraint data alone reconstructs a complete pose, and
 * comparing that with hkaSkeleton's reference pose answers a question no amount
 * of solver work can -- whether a ragdoll was authored against a different bind
 * pose from the one its skeleton stores.
 */
struct SimPoseCheck
{
	int body = -1;
	bool placed = false;        //!< false if no constraint chain reaches it
	float posDiff = 0.0f;       //!< metres from the reference pose
	float rotDiffDeg = 0.0f;
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

	/*! Replace the contents with a synthetic rig of known masses.
	 *
	 * A 38-joint ragdoll is the wrong place to debug a solver. Each of these
	 * exercises the same code path with one property of the real thing changed,
	 * and with damping off the total energy of every one of them is a conserved
	 * quantity -- so drift is the solver's own error, with no decode involved.
	 *
	 *   pendulum  anchor + one bob. The baseline.
	 *   chain3    a three-deep chain: does depth alone destabilise it?
	 *   fork      two children sharing one parent, which is what a ragdoll
	 *             pelvis looks like and what a single Gauss-Seidel pass might
	 *             over-correct.
	 *   heavy     the pendulum with the inverse inertia a real body carries
	 *             (51-66, against the baseline's 1).
	 *   spun      the pendulum with a non-identity rest orientation, so the
	 *             pivot has to be rotated into world -- the one piece of frame
	 *             maths the baseline never touches.
	 *
	 * Returns false for an unknown name.
	 */
	bool buildTestCase( const QString & name );
	//! The baseline case, kept as its own name because it is the reference.
	void buildTestPendulum() { buildTestCase( QStringLiteral( "pendulum" ) ); }

	//! Total energy (kinetic + potential). Constant for the pendulum above.
	float totalEnergy() const;

	SimStats stats() const;
	//! Measure every joint's limits in the current pose, using the solver's own
	//! axis construction so the two cannot disagree.
	QVector<SimLimitCheck> checkLimits() const;
	//! Rebuild every body's pose from the constraint frames alone and report how
	//! far that lands from the reference pose. See SimPoseCheck.
	QVector<SimPoseCheck> checkPoseFromJoints() const;
	/*! World Z of the lowest point of any body's actual geometry.
	 *
	 * Callers placing a ground plane need the real extent, not the centre of
	 * mass: a polytope has no radius to subtract, so estimating from x - radius
	 * buried every machine in the floor and the solver blew them straight back
	 * out. Returns 0 if nothing has geometry.
	 */
	float lowestPoint() const;

	const QVector<SimBody> & bodies() const { return m_bodies; }
	const QVector<SimJoint> & joints() const { return m_joints; }
	QVector<SimBody> & bodies() { return m_bodies; }

	//! Metres per second squared, negative Z (Havok space). Settable for tests.
	Vector3 gravity = Vector3( 0.0f, 0.0f, -9.81f );

	//! Ground plane at this height, off by default so a pinned ragdoll can be
	//! tested on its joints alone.
	bool ground = false;
	float groundZ = 0.0f;
	/*! Collide the ragdoll's own bodies against each other.
	 *
	 * Two exclusions make this usable. Bodies joined by a constraint always
	 * overlap at the joint and must never be tested. And any pair already
	 * overlapping in the REST pose is excluded permanently: the authored pose has
	 * a thigh inside a pelvis in places, and a solver told to separate those would
	 * tear the ragdoll apart on frame one. Havok expresses the same intent through
	 * filter groups, which are honoured first where the file sets them.
	 */
	bool selfCollision = true;
	//! Coulomb friction at contacts. Zero makes a ragdoll slide for ever, which
	//! reads as broken even though it is stable.
	float friction = 0.5f;
	//! Velocity damping per second, keeps a settling ragdoll from ringing.
	float damping = 0.02f;
	/*! Gauss-Seidel sweeps per substep.
	 *
	 * XPBD's usual advice is one sweep and many substeps, which holds while the
	 * bodies are weakly coupled. A ragdoll limb is not: its inverse inertia runs
	 * into the hundreds, so a correction at one end of a bone swings the other end
	 * by about half as much again, and every joint on a shared body fights the
	 * rest. Mass splitting keeps that under one but leaves each single sweep well
	 * short of resolved, so the sweeps have to be paid for directly.
	 *
	 * Four is measured, not guessed: on the synthetic rigs it takes the shared
	 * parent case from +1142% energy to -1.6% and the deep heavy chain from
	 * +20632% to -0.1%, both then converging as substeps rise. Sixteen buys
	 * nothing over four.
	 */
	int iterations = 4;
	//! Solve the angular limits as well as the ball sockets. Off isolates the
	//! two halves when a run misbehaves -- a pure ball-socket tree is a solved
	//! problem, so if that alone is unstable the fault is not in the limits.
	bool angularLimits = true;
	//! ...and these isolate the four kinds from each other, which is the only
	//! practical way to tell which one a misbehaving joint is fighting.
	bool useTwist = true, useCone = true, usePlane = true, useHinge = true;

	//! Pairs currently in the broad phase, for reporting.
	const QVector<SimPair> & pairs() const { return m_pairs; }
	//! Pairs permanently excluded because they already overlap at rest.
	int restOverlaps() const { return m_restOverlaps; }
	//! Joints whose parent-side pivot the file left unset, see build().
	int rebasedJoints() const { return m_rebasedJoints; }

private:
	void solveJoints( float h );
	//! fill in every body's solverScale from how many joints touch it
	void rescaleForJointCount();
	//! decide once which body pairs may ever collide
	void buildCollisionFilter();
	//! refresh the broad phase; called once per step, not per substep
	void collectPairs();
	void solveContacts( float h );

	QVector<SimBody> m_bodies;
	QVector<SimJoint> m_joints;
	QVector<SimPair> m_pairs;
	//! key = a * m_bodies.size() + b, a < b; membership means "never collide"
	QSet<int> m_noCollide;
	int m_restOverlaps = 0;
	int m_rebasedJoints = 0;
};

#endif // RAGDOLLSIM_H
