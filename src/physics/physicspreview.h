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
		Drag,       //!< spring grab, let go where it lies
		Throw,      //!< spring grab, let go carrying the hand's velocity
		Shoot,      //!< impulse along the view ray at the point it hits
		Pin,        //!< nail a body in place, click again to free it
		Blast,      //!< radial impulse centred where the ray hits
		Wind        //!< steady force along the view direction while held
	};
	void setTool( Tool t );
	Tool tool() const { return m_tool; }
	//! Human name, for the toolbar and the status line.
	static QString toolName( Tool t );

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

	void setSelfCollision( bool on ) { m_sim.selfCollision = on; }
	bool selfCollision() const { return m_sim.selfCollision; }
	void setAngularLimits( bool on ) { m_sim.angularLimits = on; }
	bool angularLimits() const { return m_sim.angularLimits; }

	//! Live solver health, for the overlay. Cheap: the step already computed it.
	SimStats stats() const { return m_sim.stats(); }
	//! Bodies whose joints are outside their limits right now, for highlighting.
	QSet<int> bodiesViolatingLimits() const;

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
	Tool m_tool = Tool::Drag;
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
};

#endif // PHYSICSPREVIEW_H
