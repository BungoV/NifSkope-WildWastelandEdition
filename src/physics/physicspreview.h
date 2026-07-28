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
		Wind        //!< steady force along the view direction while held
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
	bool start( const NifModel * nif, QString * error = nullptr );
	void stop();
	bool active() const { return m_active; }

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

	/*! Posed collision geometry as a world-space triangle soup, in game units,
	 * ready for GLView::setCollisionPreview.
	 *
	 * Built from the DECODED shapes rather than the solver's sphere set, because
	 * this one is for looking at: a sphere set is what the solver collides, and
	 * drawing it would show a limb as a string of beads.
	 */
	QVector<Vector3> soup() const;
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

	RagdollSim & sim() { return m_sim; }
	const RagdollSim & sim() const { return m_sim; }

private:
	//! one body's drawable triangles, in bone space, ready to be posed
	struct BodyMesh
	{
		QVector<Vector3> verts;     //!< already shifted by -com, in Havok metres
		QVector<Triangle> tris;
	};

	RagdollSim m_sim;
	HknpSystem m_system;
	QVector<BodyMesh> m_meshes;
	bool m_active = false;
	bool m_paused = false;
	int m_jointCount = 0;
	//! distance along the view ray the grab was made at, so dragging keeps depth
	float m_grabDepth = 0.0f;
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
