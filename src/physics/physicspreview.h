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

#include <QString>
#include <QVector>

class NifModel;

class PhysicsPreview
{
public:
	//! Havok metres -> game units. The same constant glnode.cpp draws with.
	static constexpr float SCALE = 69.99125f;

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
	void release();
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
};

#endif // PHYSICSPREVIEW_H
