/*! Live ragdoll preview for the viewport's Physics Sim mode.
 *
 * Owns a RagdollSim, the grab, and the posed geometry the viewport draws. It is
 * deliberately GL-free and Qt-widget-free: everything here can be driven and
 * checked from a headless test, so the only part that genuinely needs a window
 * is the event plumbing in GLView.
 *
 * The viewport works in GAME units and the solver in Havok metres, so this
 * converts at the boundary rather than leaving every caller to remember the
 * factor -- rays come in as game units, geometry goes out as game units, and
 * nothing outside sees a metre.
 */

#ifndef PHYSICSPREVIEW_H
#define PHYSICSPREVIEW_H

#include "gl/hknpdecode.h"
#include "physics/ragdollsim.h"

#include <QSet>
#include <QString>
#include <QVector>

#include <algorithm>

class NifModel;

class PhysicsPreview
{
public:
	//! Havok metres -> game units. The same constant glnode.cpp draws with.
	static constexpr float SCALE = 69.99125f;

	/*! What a click does. One active at a time, like a paint tool.
	 *
	 * Drag and Throw share their press and move handling and differ only on
	 * release, which is why Throw is nearly free: the drag already tracks where
	 * the hand is, so letting go with that velocity is one extra step.
	 */
	enum class Tool
	{
		/*! Grab a bone with a spring, and let go carrying the motion of the hand.
		 *
		 * Drag and Throw used to be separate tools. They were the same gesture with
		 * one difference at the moment of release, which is not a choice worth
		 * making in advance: releasing while still is a drop, and releasing while
		 * moving is a throw. The tool now does whichever the hand was doing.
		 */
		Grab,
		Shoot,      //!< impulse along the view ray at the point it hits
		Blast,      //!< radial impulse centred where the ray hits
		Wind,       //!< steady force along the view direction while held
		/*! Shove a body away along the view ray, or yank it toward the camera.
		 *
		 * The gravity gun's other half, and it differs from Shoot in the one way
		 * that matters: it sets a SPEED rather than delivering an impulse, so what
		 * you point at leaves at the same rate whether it is a jaw or a torso. The
		 * first version described itself as acting "along the view" as though Shoot
		 * did not -- both do -- which made it a heavier Shoot and nothing else.
		 */
		Punt,
		/*! Throw a ball into the scene.
		 *
		 * A tool rather than a menu command because it needs an aim: the ball goes
		 * where the ray points, at the depth of whatever is under the cursor. It
		 * also puts the ball's size, mass and speed in the same parameter row every
		 * other tool uses, instead of inventing a second place for settings.
		 */
		Prop
	};
	void setTool( Tool t );
	Tool tool() const { return m_tool; }
	//! Human name, for the toolbar and the status line.
	static QString toolName( Tool t );

	/*! Everything a tool can be tuned by, in one place.
	 *
	 * These were hardcoded constants inside the tool handlers, which meant
	 * selecting Wind gave you a 40 N push and no way to say otherwise. Grouped
	 * rather than scattered so the panel can show the ones belonging to the
	 * active tool and hide the rest.
	 */
	struct ToolSettings
	{
		//! Drag / Throw: how hard the spring holds. (0, 1]; see setDrag.
		float grabFirmness = 0.9f;
		//! How hard the hand may pull, as a multiple of the held body's weight.
		//! 0 removes the limit, and a drag can then tear the rig open.
		float grabStrength = 25.0f;
		//! Shoot: impulse along the ray, kg m/s. 12 is about a heavy pistol round.
		float shootImpulse = 12.0f;
		//! Shoot: fire a real projectile that travels, rather than hitting instantly.
		bool shootProjectile = false;
		float projectileSpeed = 60.0f;      //!< m/s
		float projectileMass = 0.05f;       //!< kg
		float projectileRadius = 0.03f;     //!< m, for drawing and for the hit test
		bool projectileGravity = true;      //!< let it drop on the way
		//! Blast: radius in metres and the impulse at the centre.
		float blastRadius = 2.0f;
		float blastStrength = 30.0f;
		//! Wind: force in newtons along the view.
		float windStrength = 40.0f;
		//! Punt: the SPEED it leaves at, m/s -- not an impulse. See RagdollSim::shove.
		float puntStrength = 6.0f;
		//! Punt: pull toward the camera instead of pushing away.
		bool puntPull = false;

		/*! The ball a prop spawns as. Radius in metres, mass in kg.
		 *
		 * 0.15 m and 5 kg is a shot put: heavy enough to knock a limb about and
		 * small enough to see the rig behind it.
		 */
		float propRadius = 0.15f;
		float propMass = 5.0f;
		//! Throw speed, m/s. 0 drops it where the ray hits.
		float propSpeed = 8.0f;
	};
	ToolSettings & settings() { return m_settings; }
	const ToolSettings & settings() const { return m_settings; }

	/*! A shot in flight, or the fading trace of one that already landed.
	 *
	 * Both kinds live here so the viewport has one thing to draw. A hitscan shot
	 * has no travel, so it is recorded as a trace that fades -- without it the
	 * only evidence a shot happened was the ragdoll twitching, which is not
	 * enough to tell "I missed" from "the tool is broken".
	 */
	struct Shot
	{
		Vector3 from, to;           //!< the trace, in GAME units
		Vector3 pos, vel;           //!< live projectile, in Havok metres
		float radius = 0.03f;       //!< metres
		float mass = 0.05f;
		bool flying = false;        //!< still travelling
		bool gravity = true;
		float age = 0.0f;           //!< seconds since it landed, for the fade
		bool hit = false;           //!< it connected, rather than sailing past
	};
	//! Shots in flight and traces still fading. Cleared as they expire.
	const QVector<Shot> & shots() const { return m_shots; }
	//! How long a trace stays visible after landing.
	static constexpr float TRACE_FADE = 0.6f;

	/*! Build from the first jointed collision system in the file.
	 *
	 * Returns false and sets `error` when the file has nothing to simulate,
	 * which is the common case and not a fault -- most NIFs are not ragdolls.
	 */
	bool start( const NifModel * nif, QString * error = nullptr, int onlyBlock = -1 );
	void stop();
	bool active() const { return m_active; }
	//! Which block the running system came from, so a picker can say which of a
	//! skeleton file's several is on screen. -1 when nothing is running.
	int systemBlock() const { return m_systemBlock; }

	//! Advance by dt seconds. Does nothing while paused, so the caller can drive
	//! it from a plain frame timer without knowing about pause state.
	void step( float dt );
	//! Back to the pose the file stores, keeping the mode active.
	void reset();

	bool paused() const { return m_paused; }
	void setPaused( bool p ) { m_paused = p; }

	/*! Press, move and release with the active tool. Rays in GAME units.
	 *
	 * Returns true when the tool consumed the event: a miss is not consumed, so
	 * clicking empty space still orbits the camera.
	 */
	bool press( const Vector3 & rayOrigin, const Vector3 & rayDir );
	/*! Right-click: pin or unpin whatever the ray hits, whatever the tool.
	 *
	 * Pinning was its own tool, which meant nailing a bone down cost a trip to the
	 * toolbar and back -- and it is almost always done in the middle of a drag,
	 * to hold what you have. It is the secondary button now, so it composes with
	 * every tool instead of replacing one.
	 */
	bool togglePin( const Vector3 & rayOrigin, const Vector3 & rayDir );
	//! Bodies currently nailed in place, for drawing them apart from the rest.
	QVector<Vector3> pinnedSoup() const;

	/*! The NIF node a body belongs to, or empty.
	 *
	 * Taken from the bhkNPCollisionObject that names the body -- "Body ID" on the
	 * collision object, "Name" on the node it targets. The packfile itself has no
	 * names at all: hkaSkeleton's bone name pointers are null on all 804 corpus
	 * bones, so the NIF is the only place they exist.
	 */
	QString bodyName( int body ) const { return m_bodyNames.value( body ); }
	//! The block number of that node, for writing a captured pose back to it.
	//! -1 when no collision object binds this body to anything.
	int bodyNode( int body ) const { return m_bodyBlocks.value( body, -1 ); }

	/*! How far a body has MOVED from the pose the file stores: a rigid transform,
	 * in GAME units, ready to compose with a node's world transform.
	 *
	 * A difference rather than an absolute pose, deliberately. Whatever constant
	 * offset there is between a body's frame and the node's -- and there is one,
	 * since a body is placed at its centre of mass -- cancels in a difference, so
	 * this is correct without having to first establish what that offset is.
	 */
	bool bodyDelta( int body, Quat & rotation, Vector3 & translation ) const;
	bool move( const Vector3 & rayOrigin, const Vector3 & rayDir );
	bool release();

	/*! Grab whatever the ray hits. Ray in GAME units, as the viewport has it.
	 *
	 * Returns false when the ray misses, which the caller should treat as "no
	 * grab" rather than an error -- clicking empty space is not a mistake.
	 */
	bool grab( const Vector3 & rayOrigin, const Vector3 & rayDir );
	/*! Move the grab. The ray gives a direction, not a point, so the grab keeps
	 * the DEPTH it was made at and slides along the new ray at that distance --
	 * which is what every 3D mouse drag does, and without it a grab would rush
	 * toward the camera the moment the mouse moved.
	 */
	void dragTo( const Vector3 & rayOrigin, const Vector3 & rayDir );
	bool grabbing() const { return m_sim.draggedBody() >= 0; }
	//! The body currently held, or -1.
	int grabbedBody() const { return m_sim.draggedBody(); }

	/*! Reel the held body in or out along the view ray.
	 *
	 * The depth was fixed at whatever the grab was made at, which confines a drag
	 * to a plane parallel to the screen -- so placing a hand in front of a chest
	 * meant orbiting the camera round to a side view first. `notches` is wheel
	 * detents, positive to push away.
	 *
	 * Multiplicative rather than additive: a fixed step of 10 cm is a crawl across
	 * a room and a lurch on a rat, whereas a percentage of the current distance
	 * moves the same fraction of the way there whatever the scale.
	 */
	void adjustGrabDepth( float notches, bool fine = false );
	//! Distance from the eye to the hand, in GAME units, for a readout.
	float grabDepth() const { return m_grabDepth * SCALE; }

	/*! Turn what the hand is holding.
	 *
	 * The rotation frame is captured ONCE, when the gesture starts, and every
	 * later increment is measured in that frame. Taking the camera's live axes
	 * each time instead would make an orbit mid-rotate silently redefine which way
	 * is up, and 15-degree snapping would then quantise against a moving target.
	 *
	 * Angles in radians. Pass the camera's right, up and forward in GAME space.
	 */
	void beginGrabRotate( const Vector3 & right, const Vector3 & up, const Vector3 & fwd );
	void addGrabRotate( float dYaw, float dPitch, float dRoll, bool snap = false );
	//! Give up the orientation hold; the bone is then free to swing again.
	void clearGrabRotate();
	bool grabRotating() const { return m_rotating; }
	//! Snap increment, radians. 15 degrees, as every DCC tool uses.
	static constexpr float ROTATE_SNAP = 0.2617994f;

	/*! The beam: where the hand has hold, and where it is pulling to.
	 *
	 * Both in GAME units. Returns false when nothing is held. Without this the
	 * depth and rotate controls are invisible -- you can feel them working but
	 * cannot see where the hand actually is.
	 */
	bool grabBeam( Vector3 & gripPoint, Vector3 & handPoint ) const;

	//! Let the held bone pass through the rest of the rig, to untangle a limb
	//! that self-collision has trapped inside the torso.
	void setDragNoCollide( bool on ) { m_sim.dragNoCollide = on; }
	bool dragNoCollide() const { return m_sim.dragNoCollide; }

	//! Release every pin. See RagdollSim::unpinAll.
	void unpinAll() { m_sim.unpinAll(); }
	int pinnedCount() const;

	/*! Drop a ball into the scene along the ray, thrown at settings().propSpeed.
	 *
	 * Returns its body index, or -1. The prop joins the rig as an ordinary body,
	 * so it is drawn, picked, grabbed and pinned by exactly the same code -- which
	 * is why a ball can be caught mid-air and thrown again.
	 */
	int spawnProp( const Vector3 & rayOrigin, const Vector3 & rayDir );
	void clearProps();
	int propCount() const;

	/*! Posed collision geometry as a world-space triangle soup, in game units,
	 * ready for GLView::setCollisionPreview.
	 *
	 * Built from the DECODED shapes rather than the solver's sphere set, because
	 * this one is for looking at: a sphere set is what the solver collides, and
	 * drawing it would show a limb as a string of beads.
	 */
	QVector<Vector3> soup() const;
	//! One body's posed triangles, world space, game units. Empty if it has none.
	QVector<Vector3> bodySoup( int body ) const;

	/*! Nearest body along a ray, against the geometry actually DRAWN.
	 *
	 * Not RagdollSim::pick, which tests the solver's sphere set. That set is a
	 * capsule reduced to its two end spheres and a polytope to its bare vertices,
	 * so clicking the middle of a limb was often nearer some neighbour's sphere
	 * than to anything belonging to the limb itself: aiming straight at a shape
	 * returned the WRONG body 107 times in 195 on a brahmin.
	 *
	 * The earlier reasoning for using the sphere set -- that a pick should not
	 * land where the physics has nothing -- does not survive contact with the
	 * problem. A body is rigid, so every point on it is a real place to apply a
	 * force, and a picker that disagrees with what is on screen is simply wrong.
	 *
	 * Ray in GAME units. Falls back to the sphere set for a body that decoded to
	 * no drawable triangles, which would otherwise be unclickable.
	 */
	SimPick pick( const Vector3 & rayOrigin, const Vector3 & rayDir ) const;
	//! Just the bodies breaking a limit, so the viewport can draw them over the
	//! rest in a warning colour. Empty unless highlightLimits() is on.
	QVector<Vector3> limitSoup() const;
	//! Just the body being held, so the viewport can show WHICH bone is grabbed.
	QVector<Vector3> grabbedSoup() const;

	//! How many bodies and joints are being simulated, for a status line.
	int bodyCount() const { return m_sim.bodies().size(); }
	int jointCount() const { return m_jointCount; }

	//! Stop all motion but keep solving, so a settled pose holds. See RagdollSim.
	void freeze() { m_sim.freeze(); }

	/* Options. These forward to the solver rather than shadowing it, so there is
	 * no second copy of the truth to drift out of step. */
	void setGravityEnabled( bool on );
	bool gravityEnabled() const { return m_gravityOn; }
	//! Gravity in m/s^2 along -Z. Sign is handled here; pass a magnitude.
	void setGravityStrength( float g );
	float gravityStrength() const { return m_gravityG; }
	//! Wind force in newtons, world space.
	void setWind( const Vector3 & w ) { m_sim.wind = w; }
	Vector3 wind() const { return m_sim.wind; }

	//! 1.0 is real time. Below that is slow motion; it scales dt, not the substep
	//! count, so the solve stays exactly as accurate as it was.
	void setTimeScale( float s ) { m_timeScale = std::clamp( s, 0.01f, 4.0f ); }
	float timeScale() const { return m_timeScale; }

	void setGroundEnabled( bool on );
	bool groundEnabled() const { return m_sim.ground; }
	//! Ground height in GAME units, as the viewport shows it.
	void setGroundHeight( float z ) { m_sim.groundZ = z / SCALE; }
	float groundHeight() const { return m_sim.groundZ * SCALE; }
	//! Where start() put the floor, so a reset control has something to go back to.
	float defaultGroundHeight() const { return m_defaultGroundZ * SCALE; }

	/*! Floor grip, 0 (ice) to about 2 (rubber). Separate from the ragdoll's own
	 * body-on-body friction, because a floor is a surface with its own character.
	 */
	void setGroundFriction( float f ) { m_sim.groundFriction = std::max( 0.0f, f ); }
	float groundFriction() const { return m_sim.groundFriction; }
	void setSelfCollision( bool on ) { m_sim.selfCollision = on; }
	bool selfCollision() const { return m_sim.selfCollision; }
	void setAngularLimits( bool on ) { m_sim.angularLimits = on; }
	bool angularLimits() const { return m_sim.angularLimits; }

	/*! Multiplier on the friction each body carries, which is not the floor's.
	 *
	 * A scale, because the file authors a coefficient per body and the solver now
	 * honours it; this only adjusts them all together. 1 is the file as authored.
	 */
	void setFriction( float f ) { m_sim.frictionScale = std::max( 0.0f, f ); }
	float friction() const { return m_sim.frictionScale; }
	//! Extra drag per second on top of each body's authored damping. Raise it to
	//! make a rig settle sooner; 0 leaves the file's own values alone.
	void setDamping( float d ) { m_sim.damping = std::max( 0.0f, d ); }
	float damping() const { return m_sim.damping; }
	//! Multiplier on the bounce each body carries. See RagdollSim::restitutionScale.
	void setRestitution( float r ) { m_sim.restitutionScale = std::max( 0.0f, r ); }
	float restitution() const { return m_sim.restitutionScale; }
	//! Gauss-Seidel sweeps per substep, and substeps per frame. Both trade cost
	//! for stability; the stats overlay is what says whether they are needed.
	void setIterations( int n ) { m_sim.iterations = std::clamp( n, 1, 32 ); }
	int iterations() const { return m_sim.iterations; }
	void setSubsteps( int n ) { m_substeps = std::clamp( n, 1, 32 ); }
	int substeps() const { return m_substeps; }

	/*! Gravity direction, as a unit vector in solver space. Default is straight
	 * down. Tilting it is how a slope is tested without authoring one.
	 */
	void setGravityDirection( const Vector3 & d );
	Vector3 gravityDirection() const { return m_gravityDir; }

	//! Live solver health, for the overlay. Cheap: the step already computed it.
	SimStats stats() const { return m_sim.stats(); }
	//! Bodies whose joints are outside their limits right now, for highlighting.
	QSet<int> bodiesViolatingLimits() const;
	//! Draw those bodies in a warning colour. Off by default: on a rig whose
	//! authored pose already breaks a limit it would be lit up from the start.
	void setHighlightLimits( bool on ) { m_highlightLimits = on; }
	bool highlightLimits() const { return m_highlightLimits; }

	//! Put the floor back where start() placed it, under the rig.
	void resetGroundHeight() { m_sim.groundZ = m_defaultGroundZ; }
	//! Draw the ground as a solid surface rather than leaving it invisible.
	void setGroundVisible( bool on ) { m_groundVisible = on; }
	bool groundVisible() const { return m_groundVisible; }
	/*! Two triangles covering the floor under the rig, in GAME units.
	 *
	 * Sized from the rig rather than fixed, so it is neither a postage stamp
	 * under Liberty Prime nor a runway under a cat. Empty when the ground is off
	 * or hidden.
	 */
	QVector<Vector3> groundSoup() const;

	/*! Record every stepped pose so it can be scrubbed back through.
	 *
	 * A ragdoll settles in about two seconds and the one frame worth keeping goes
	 * past in a sixtieth of one. Without a recording the only way back to it is to
	 * reset and try to catch it with the pause key, which is a game of reflexes
	 * rather than a tool. Capped at RECORD_MAX frames, oldest dropped, so it
	 * cannot grow without bound while the mode is left open.
	 */
	void setRecording( bool on );
	bool recording() const { return m_recording; }
	int frameCount() const { return int( m_frames.size() ); }
	//! Which recorded frame is showing, or -1 when the sim is running live.
	int frameIndex() const { return m_frameIndex; }
	//! Show a recorded frame. Pauses, since running on would overwrite it.
	void seek( int frame );
	//! Back to live: drop the scrub and let the sim continue from where it is.
	void resumeLive() { m_frameIndex = -1; }
	void clearRecording();
	//! 20 seconds at 60 Hz.
	static constexpr int RECORD_MAX = 1200;

	RagdollSim & sim() { return m_sim; }
	const RagdollSim & sim() const { return m_sim; }

private:
	//! one body's drawable triangles, in bone space, ready to be posed
	struct BodyMesh
	{
		QVector<Vector3> verts;     //!< already shifted by -com, in Havok metres
		QVector<Triangle> tris;
	};

	//! one recorded pose: every body's place and orientation at one instant
	struct Frame
	{
		QVector<Vector3> x;
		QVector<Quat> q;
	};

	RagdollSim m_sim;
	HknpSystem m_system;
	QVector<BodyMesh> m_meshes;
	QVector<QString> m_bodyNames;
	QVector<int> m_bodyBlocks;
	bool m_active = false;
	bool m_paused = false;
	int m_jointCount = 0;
	int m_systemBlock = -1;
	//! how many bodies and meshes the FILE supplied, so props can be dropped
	//! again without taking any of the rig with them
	int m_rigBodies = 0;
	int m_substeps = 8;
	Vector3 m_gravityDir = Vector3( 0.0f, 0.0f, -1.0f );
	//! distance along the view ray the grab was made at, so dragging keeps depth
	float m_grabDepth = 0.0f;
	//! the last ray the hand was moved along, so changing the DEPTH alone still
	//! moves the body -- the wheel turns with the mouse standing still
	Vector3 m_lastRayOrigin, m_lastRayDir;
	//! the rotate gesture: frame captured at the start, angles accumulated in it
	bool m_rotating = false;
	Vector3 m_rotRight, m_rotUp, m_rotFwd;
	Quat m_rotBase = Quat( 1, 0, 0, 0 );
	float m_rotYaw = 0.0f, m_rotPitch = 0.0f, m_rotRoll = 0.0f;
	bool m_recording = false;
	int m_frameIndex = -1;
	QVector<Frame> m_frames;
	//! capture the current pose into the recording
	void recordFrame();
	//! generate a unit icosphere, so a spawned prop draws and picks like any body
	static void buildBallMesh( BodyMesh & bm, float radius );
	Tool m_tool = Tool::Grab;
	bool m_gravityOn = true;
	float m_gravityG = 9.81f;
	float m_timeScale = 1.0f;
	float m_defaultGroundZ = 0.0f;
	//! where the grab was last frame and how long ago, so Throw can hand the body
	//! the hand's velocity instead of guessing one
	Vector3 m_lastGrabTarget;
	float m_sinceGrabMove = 0.0f;
	Vector3 m_grabVelocity;
	//! Wind and Blast need the ray while the button is held
	bool m_holding = false;
	Vector3 m_holdDir;
	ToolSettings m_settings;
	QVector<Shot> m_shots;
	bool m_highlightLimits = false;
	bool m_groundVisible = false;
	//! advance projectiles and age traces; called from step()
	void stepShots( float h );
};

#endif // PHYSICSPREVIEW_H
