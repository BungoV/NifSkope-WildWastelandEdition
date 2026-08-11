/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENCE BLOCK *****/

#ifndef GLVIEW_H_INCLUDED
#define GLVIEW_H_INCLUDED

#include "gl/glscene.h"
#include "physics/physicspreview.h"
#include "model/nifmodel.h"

#include <QOpenGLWindow> // Inherited
#include <QByteArray>
#include <QHash>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QSet>
#include <chrono>
#include <functional>


//! @file glview.h GLView

class NifSkope;
class QMenu;
class QTimer;


//! The main [Viewport](@ref viewport_details) class
class GLView final : public QOpenGLWindow
{
	Q_OBJECT

	friend class NifSkope;

public:
	GLView( QWindow * parent );
	~GLView();
	QWidget * createWindowContainer( QWidget * parent );

	//! One typed parameter of the generalized operator redo panel (Redo Panel
	//! v2, MODELING_TOOLS_PLAN.md F0.a): floats/ints render as DragSpinBoxes,
	//! bools as checkboxes, enums as dropdowns.
	struct TlOpParam
	{
		enum Type { Float, Int, Bool, Enum };
		QString label;
		int type = Float;
		double value = 0.0;                 //!< Bool: 0/1; Enum: index
		double mn = -1.0e6, mx = 1.0e6, step = 0.01;
		int decimals = 4;
		QStringList enumNames;
	};
	//! Temporary collision-authoring overlay. The triangle soup is in world
	//! coordinates and never touches the NIF until the operator is applied.
	void setCollisionPreview( const QVector<Vector3> & triangleSoup );
	void clearCollisionPreview();
	//! Read-only Rigging donor overlay. Geometry is copied into target model
	//! space and is never inserted into the active NIF or selection buffer.
	void setRiggingDonorPreview( const QVector<Vector3> & triangleSoup );
	void setRiggingDonorPreviewStyle( bool filled, bool wireframe, float opacity );
	void clearRiggingDonorPreview();
	int riggingDonorPreviewTriangleCount() const { return riggingDonorPreviewSoup.size() / 3; }
	//! Read-only geometry from other open NIF documents. It is deliberately a
	//! separate neutral overlay so Rigging's selected cyan donor remains clear.
	//! Workspace preview soups: \a triangleSoup opaque, \a ghostSoup translucent
	//! in a blended pass afterwards. Two soups rather than mixed alpha in one,
	//! because a translucent triangle must be drawn after what it shows through.
	void setSessionDocumentPreview( const QVector<Vector3> & triangleSoup,
		const QVector<Vector3> & ghostSoup = QVector<Vector3>() );
	/*! Workspace SOLIDS: documents to render properly, with their own materials,
	 *  textures and shaders. One Scene each, built on demand and kept until the
	 *  model leaves the list; they share this view's TexCache, so a texture used
	 *  by two documents is loaded once.
	 */
	//! \a models are drawn; \a keepScene are still loaded and keep their Scene
	//! without being drawn, because other documents pose against it.
	void setWorkspaceRenderModels( const QVector<NifModel *> & models,
		const QVector<NifModel *> & keepScene = QVector<NifModel *>() );
	void clearSessionDocumentPreview();
	//! Per-corner colour overlay used by the Rigging selected-bone heatmap.
	void setRiggingWeightPreview( const QVector<Vector3> & triangleSoup,
		const QVector<FloatVector4> & colors );
	//! Replace only heatmap colours during a live paint stroke; geometry is static.
	void setRiggingWeightPreviewColors( const QVector<FloatVector4> & colors );
	void clearRiggingWeightPreview();
	int riggingWeightPreviewTriangleCount() const { return riggingWeightPreviewSoup.size() / 3; }
	//! Modal screen-space brush used by the Rigging Manager. The viewport only
	//! identifies target vertices and falloff; the manager owns model writes.
	void setRiggingWeightPaintMode( bool enabled, int targetBlock = -1, int brushMode = 0,
		float radius = 32.0f, float paintWeight = 1.0f, float strength = 0.25f );
	bool riggingWeightPaintModeActive() const { return riggingWeightPaintMode; }
	void setRiggingWeightPaintBrushEnabled( bool enabled );
	bool riggingWeightPaintBrushActive() const { return riggingWeightPaintMode && riggingWeightPaintBrushEnabled; }
	//! Replace the active bone's weight on every selected vertex (Ctrl+X).
	//! Runs deferred and re-entrancy-guarded: a Ctrl+X fill triggers a full
	//! model snapshot, so it must never run nested inside the key event or
	//! stack up under keyboard autorepeat (that was a hard freeze).
	void fillRiggingWeightSelection();
	//! Assign/remove all selected faces for the active binary segment (Ctrl+X).
	void fillSegmentPaintSelection();
	//! Guard so a deferred Ctrl+X fill cannot re-enter or queue duplicates.
	bool paintFillPending = false;
	float riggingWeightPaintBrushRadius() const { return riggingWeightPaintRadius; }
	//! Vertex Paint shares the same evaluated edit-cage selection and navigation
	//! model as Weight Paint. The manager owns RGBA writes and Undo snapshots.
	void setVertexPaintMode( bool enabled, int targetBlock = -1, float radius = 32.0f );
	bool vertexPaintModeActive() const { return vertexPaintMode; }

	// Pose Mode: a viewport mode (peer of Object/Edit/paint) for posing the
	// skeleton. While active the bones are drawn connected by parenting and
	// clicks pick bones instead of meshes; a click selects the bone node so the
	// existing G/R/S transform poses it. No mesh topology is touched.
	void setPoseMode( bool enabled );
	bool poseModeActive() const { return poseMode; }
	//! Skeleton Manager armature display: octahedral bones, names and parent
	//! relationship lines, without entering Pose Mode.
	void setSkeletonView( bool on );
	bool skeletonViewActive() const { return skeletonView; }
	//! Bones currently drawn, for the dock's selection sync.
	QVector<int> skeletonDrawnBones() const { return poseBones; }
	//! Restrict the drawn armature to these bones (Blender's Isolate). Empty
	//! means draw everything.
	void setSkeletonIsolated( const QSet<int> & bones );
	//! Show bone names beside the bones (toggle).
	void setPoseShowBoneNames( bool on ) { poseShowBoneNames = on; update(); }
	//! Show dashed parent-relationship lines (toggle).
	void setPoseShowRelations( bool on ) { poseShowRelations = on; update(); }
	//! Which bones are drawn/pickable: 0 all, 1 deforming only, 2 face sculpt (skin_bone_C_*).
	void setPoseBoneFilter( int filter ) { poseBoneFilterMode = filter; refreshPoseBones(); update(); }
	int poseBoneFilter() const { return poseBoneFilterMode; }
	//! Restore bone(s) to the rest captured on entering pose mode. channels
	//! bitmask: 1 rotation, 2 location, 4 scale (7 = all). block < 0 = all bones.
	void poseResetBone( int block, int channels );
	//! Mirror a bone's pose onto its L/R counterpart across the X axis (Blender
	//! X-Axis Mirror). Assumes a symmetric rig. Returns the counterpart block,
	//! or -1 if none was found by name.
	int poseMirrorBone( int block );
	//! Import / export an Outfit Studio pose XML, using the rest captured on
	//! entering pose mode as the delta base. Import returns bones applied (0 =
	//! failure, message in *error); export returns success.
	int poseImportOutfitStudio( const QString & path, float blend, QString * error );
	bool poseExportOutfitStudio( const QString & path, const QString & name, QString * error );
	//! Import a Screen Archer Menu pose (.json). SAM transforms are ABSOLUTE, so
	//! no rest base is used and blend interpolates from the bone's current
	//! transform. Returns bones applied (0 = failure, message in *error); on
	//! success *error may still carry a non-fatal note. Import only — no export.
	int poseImportSam( const QString & path, float blend, QString * error, int * missing = nullptr );
	//! Test hooks (WW_POSEDRAW_TEST): bone count, and does poseBoneAt resolve a
	//! bone at its own drawn screen position.
	int poseBoneCountForTest() const { return poseBones.size(); }
	int poseBoneProbeForTest() const;
	bool worldToScreenForTest( const Vector3 & w, QPointF & out ) const { return worldToScreen( w, out ); }
	int objectSelectionCountForTest() const { return objSelection.size(); }
	int poseWeightPointCountForTest() const { int n = 0; for ( int i = 0; i < PoseWeightBuckets; i++ ) n += poseWeightPts[i].size(); return n; }
	//! Set up a central-vertex pick + proportional move on the biggest shape;
	//! reports neighbours gathered and whether nearer neighbours got more weight.
	int proportionalSelfTestForTest( bool & falloffMonotonic );
	void setVertexPaintBrushEnabled( bool enabled );
	bool vertexPaintBrushActive() const { return vertexPaintMode && vertexPaintBrushEnabled; }
	float vertexPaintBrushRadius() const { return vertexPaintRadius; }
	//! Lightweight live preview; does not touch the NIF model.
	void setVertexPaintPreviewColors( int targetBlock, const QVector<Color4> & colors );
	//! Binary face-membership painting for FO4 BSSubIndexTriShape segments.
	void setSegmentPaintMode( bool enabled, int targetBlock = -1, float radius = 32.0f );
	bool segmentPaintModeActive() const { return segmentPaintMode; }
	void setSegmentPaintBrushEnabled( bool enabled );
	bool segmentPaintBrushActive() const { return segmentPaintMode && segmentPaintBrushEnabled; }
	float segmentPaintBrushRadius() const { return segmentPaintRadius; }
	bool editModeActive() const { return editMode; }

	/*! Physics Sim: a viewport mode alongside Object and Edit.
	 *
	 * The ragdoll runs live, a drag grabs a bone with a spring, Space pauses and R
	 * resets. It is a PREVIEW -- nothing it does is written back to the file, so
	 * leaving the mode puts everything where it was.
	 */
	//! `systemBlock` picks WHICH collision system to simulate; -1 takes the first
	//! jointed one, which is right for a creature file and arbitrary for a
	//! skeleton file that carries several.
	bool setPhysicsSimMode( bool on, int systemBlock = -1 );
	bool physicsSimActive() const { return physicsPreview.active(); }
	PhysicsPreview & physicsSim() { return physicsPreview; }
	//! advance the preview AND refresh what is drawn; dt comes from advanceGears.
	//! Public so a harness can drive the real per-frame path rather than stepping
	//! the solver behind the viewport's back and never noticing the drawn
	//! geometry had stopped following it.
	void physicsTick( float dt );
	//! Write the simulated pose back to the bound nodes; returns how many moved.
	//! The only thing in this mode that touches the file.
	int physicsCapturePose();
	//! The rate a physics recording is captured and scrubbed at.
	static constexpr float PHYSICS_RECORD_FPS = 60.0f;
	//! Seconds of physics recording available, 0 when there is none. While this
	//! is non-zero the timeline dock scrubs the recording instead of the scene.
	float physicsRecordingRange() const;
	//! Push the recording's extent to the timeline, as frames accumulate.
	void refreshPhysicsTimeline();
	//! what the viewport is currently drawing as a collision preview
	const QVector<Vector3> & collisionPreview() const { return collisionPreviewSoup; }
	bool physicsStatsShown() const { return physicsShowStats; }
	void setPhysicsStatsShown( bool on ) { physicsShowStats = on; update(); }
	//! Blender-style mouse mapping: false = select with LMB and place the 3D
	//! cursor/gizmo with RMB (default); true = swapped (2.7x right-click select).
	//! Only the click roles swap; drags (orbit / zoom), the select gadgets and
	//! the gizmo handles keep their buttons.
	bool selectWithRightMouse = false;
	Qt::MouseButton selectMouseButton() const { return selectWithRightMouse ? Qt::RightButton : Qt::LeftButton; }
	Qt::MouseButton cursorPlaceButton() const { return selectWithRightMouse ? Qt::LeftButton : Qt::RightButton; }
	void setSelectWithRightMouse( bool on ) { selectWithRightMouse = on; }
	//! Edit the evaluated, skinned cage (default) or raw authored bind vertices.
	void setEditDeformedCage( bool enabled );
	bool editDeformedCageEnabled() const { return editDeformedCage; }
	bool editDeformedCageActive() const { return editMode && ( riggingWeightPaintMode || vertexPaintMode || segmentPaintMode || editDeformedCage ); }
	//! Evaluate a shape vertex with its current skin and rendered node transform.
	bool evaluatedVertexWorld( int shapeBlock, int vertexIndex, Vector3 & world ) const;
	//! Object-mode selection exposed read-only for contextual workspace tools.
	//! The active object is the primary selection; all others are secondary.
	const QSet<int> & selectedObjectBlocks() const { return objSelection; }
	int activeObjectBlock() const { return objActive; }

	NifSkopeOpenGLContext * glContext = nullptr;

	float	toneMapping = 0.23641851f;	// 0.05 to 1.0
	float	brightnessScale = 1.0f;		// overall brightness
	float	glowScale = 1.0f;
	float	ambient = 1.0f;				// environment map / ambient light level
	float	brightnessL = 1.0f;			// directional light intensity,
	float	lightColor = 0.0f;			// and color temperature (-1.0 to 1.0)
	float	declination = 0.0f;
	float	planarAngle = 0.0f;
	float	envMapRotation = 0.0f;
	bool	frontalLight = true;

	enum AnimationStates
	{
		AnimDisabled = 0x0,
		AnimEnabled = 0x1,
		AnimPlay = 0x2,
		AnimLoop = 0x4,
		AnimSwitch = 0x8
	};
	Q_DECLARE_FLAGS( AnimationState, AnimationStates );

	enum ViewState : unsigned char
	{
		ViewDefault,
		ViewTop,
		ViewBottom,
		ViewLeft,
		ViewRight,
		ViewFront,
		ViewBack,
		ViewWalk,
		ViewUser
	};

	enum DebugMode : unsigned char
	{
		DbgNone = 0,
		DbgColorPicker = 1,
		DbgBounds = 2
	};

	enum UpAxis : unsigned char
	{
		XAxis = 0,
		YAxis = 1,
		ZAxis = 2
	};

	void setNif( NifModel * );

	Scene * getScene();
	void updateShaders();
	void updateViewpoint();

	void flush();

	void center();
	//! Blender Numpad-.: frame the current selection (falls back to center())
	void frameSelected();
	//! Blender Home: frame the complete visible scene, ignoring the active block.
	void frameAll();
	//! Handle viewport-scoped Blender numpad navigation. When trigger is false,
	//! only reports whether the key is owned so ShortcutOverride can reserve it.
	bool handleBlenderNumpad( int key, Qt::KeyboardModifiers modifiers, bool trigger );
	/*! Whether a viewport mode has already claimed \a key for itself.
	 *
	 *  Space and Q carry the Search menu and Quick Favourites, and a QAction
	 *  shortcut is matched before the key ever reaches this widget — so a mode
	 *  that uses either letter loses it silently. ShortcutOverride reserves the
	 *  key back for the mode that owns it, which is what makes both bindings
	 *  contextual the way Blender's per-editor keymaps are.
	 */
	bool viewportClaimsKey( int key ) const;
	void move( float, float, float );
	void rotate( float, float, float );
	void rotateLight( float, float );

	void setCenter();
	void setDistance( float );
	void setPosition( float, float, float );
	void setPosition( const Vector3 & );
	void setProjection( bool );
	void setRotation( float, float, float );
	void setZoom( float );

	void setOrientation( GLView::ViewState, bool recenter = true );
	void flipOrientation();
	inline ViewState viewState() const { return view; }
	//! Radius of the origin axes marker; glProjection() extends the clip
	//! range by an origin sphere of this radius when Show Axes is on, and
	//! the ortho grid must mirror that to stay inside the far plane
	float axisMarkerRadius() const { return float( axis ); }
	//! Return the exact axis view represented by the current rotation, or
	//! ViewDefault when the camera is at an arbitrary user angle.
	ViewState axisAlignedViewState() const;
	inline bool isPerspectiveProjection() const { return perspectiveMode || view == ViewWalk; }
	inline float orthographicHalfHeight() const { return float( Dist / Zoom ); }

	void setDebugMode( DebugMode );
	static bool selectPBRCubeMapForGame( quint32 bsVersion );

	// Starfield: 1 unit = 1 meter
	// older games: 64 units = 1 yard = 0.9144 m
	float scale() { return (scene->nifModel && scene->nifModel->getBSVersion() >= 170) ? float(1.0 / 64.0) : 1.0f; };

	Color4 clearColor() const;


	QModelIndex indexAt( const QPointF & p, bool shiftModifier = false );

	//! Where the timeline is parked right now.
	/*! The freeze dialog seeds itself from this, so "scrub until it looks right,
	 *  then freeze this file there" needs no second guess at the number. */
	float sceneTime() const { return time; }

	//! One effect — arc or sprite cloud — snapshotted as static world geometry.
	struct BakedEffect
	{
		QVector<Vector3> tris;   //!< world space, 3 per triangle
		QVector<Vector2> uvs;    //!< one per vertex of `tris`
		QVector<Color4> cols;    //!< one per vertex; carries the fade
		Color4 tint;
		QModelIndex shaderProperty;   //!< the effect's BSEffectShaderProperty
		QModelIndex alphaProperty;    //!< its NiAlphaProperty, if it has one
		QString name;                 //!< the node the geometry came from
		bool fromParticles = false;   //!< sprite cloud rather than lightning
	};

	//! Snapshot every procedural-lightning arc in the scene as it looks NOW.
	/*! `viewAxis` pins the billboard — static geometry cannot turn to face the
	 *  camera, so the caller chooses which way the ribbon presents its width.
	 *  The arcs are generated deterministically from the scene time, so the same
	 *  instant always yields the same geometry. */
	QVector<BakedEffect> bakeLightningArcs( const Vector3 & viewAxis );

	//! Snapshot every particle system in the scene as it looks NOW.
	/*! Unlike the arcs this is NOT reproducible from the time alone: sprite
	 *  positions integrate frame to frame, so what comes out depends on how the
	 *  scene was walked to here. Step, never jump. */
	QVector<BakedEffect> bakeParticles( const Vector3 & viewAxis );

	//! Both of the above, for one loaded document rather than for the primary.
	/*! A background document that is visible in the workspace has its own live
	 *  Scene, stepped with the same time as the primary (see paintGL) — that is
	 *  what lets "freeze each limb at its own instant" bake effects too. Returns
	 *  empty when `model` has no live scene, so the caller can say so instead of
	 *  silently producing nothing. */
	QVector<BakedEffect> bakeEffects( NifModel * model, const Vector3 & viewAxis );

	//! Whether `model` currently has a live scene a bake could read.
	bool hasLiveScene( NifModel * model ) const;

	//! The direction the camera is looking, in world space.
	/*! What a bake needs to pin a billboard so the snapshot faces the viewer it was
	 *  taken from — the same axis drawPreview passes when the effect is live. */
	Vector3 viewForwardAxis() const;

	//! Mark one loaded document as THE skeleton the rest snap to; null unmarks.
	/*! Every other document then evaluates its bones against this one BY NAME,
	 *  which is how a skinned armour piece stops sitting at bind pose without being
	 *  merged into anything. Strictly opt-in: until something is marked, every
	 *  document is transformed exactly as it was before this existed — a file that
	 *  merely happens to contain a skeleton changes nothing. */
	void setWorkspaceSkeleton( NifModel * model );
	NifModel * workspaceSkeleton() const { return workspaceSkeletonModel; }

	//! Mark one loaded document as FOLLOWING the skeleton, per document.
	/*! The skull says which file is the skeleton; this says who moves with it. A
	 *  marked document re-anchors its skinned geometry to that skeleton's bones by
	 *  name, live, every time the pose changes and without a merge — nothing is
	 *  written into the follower, and unmarking puts it back on its own flat bone
	 *  copies immediately.
	 *
	 *  The skeleton it follows is the skull-marked document if there is one, and
	 *  otherwise the PRIMARY when the primary has a real bone hierarchy. A follower
	 *  with neither available renders exactly as it did before it was marked.
	 *
	 *  While no document is marked, not one transform is computed differently from
	 *  before this existed: the skull's own all-documents behaviour is left alone
	 *  and only takes over when the follower set is empty. */
	void setWorkspaceFollower( NifModel * model, bool follow );
	bool isWorkspaceFollower( const NifModel * model ) const;
	//! Live follower marks, pruning any whose document has gone.
	int workspaceFollowerCount();
	/*! The live Scene a loaded document draws through, or null.
	 *
	 *  getScene() answers this for the primary; this answers it for a workspace
	 *  row. Anything that has to measure what a document actually RENDERS — a
	 *  follower's evaluated skin, rather than the file it was loaded from — needs
	 *  the Scene, because the file is exactly what does not change. */
	Scene * workspaceSceneOf( NifModel * model );

	//! Turn animation on or off programmatically.
	/*! updateAnimationState is a QAction slot: it reads sender()->data() and does
	 *  nothing at all when called directly. Anything that needs the controllers
	 *  running without a menu click — a bake, a harness — needs this instead. */
	void setAnimationEnabled( bool on );

public slots:
	void update();
	void setCurrentIndex( const QModelIndex & );
	void setSceneTime( float );
	/*! Playback rate, and the only way to play backwards.
	 *
	 * Negative runs the sequence in reverse; advanceGears() mirrors the timeMax
	 * wrap at timeMin so loop and cycle behave the same in both directions. 1.0
	 * is real time.
	 */
	void setAnimSpeed( float speed );
	float animationSpeed() const { return animSpeed; }
	//! +1 forwards, -1 after a CYCLE_REVERSE turn; so a test can see the ping-pong
	float animationDirection() const { return animDir; }
	void setSceneSequence( const QString & );
	/*! Play no sequence at all — only what the file animates on its own.
	 *
	 *  Not the same as picking an empty name. Selecting a sequence BINDS its
	 *  interpolators onto the controllers it names, and that binding outlives the
	 *  selection: asking for a sequence that does not exist leaves the last one's
	 *  bindings in place. So this rebuilds the controllers from the model, which
	 *  puts each one back on the interpolator its own block points at — the file
	 *  as authored, which for every NiPSys effect in Meshes/Effects is the ONLY
	 *  animation there is.
	 */
	void clearSceneSequence();
	/*! Re-publish the selected sequence's Cycle Type and face playback forwards.
	 *
	 *  Call after anything that changes WHICH sequence is selected — including
	 *  Scene::make, which picks one itself without going through
	 *  setSceneSequence.
	 */
	void announceSequenceCycle();
	//! Render only the currently selected node's subtree (follows the selection)
	void setSoloMode( bool );
	//! Render only the given block's subtree (-1 clears; independent of solo mode)
	void setSoloBlock( int blockNumber );
	void saveUserView();
	void loadUserView();
	void setBrightness( int );
	void setLightLevel( int );
	void setLightColor( int );
	void setToneMapping( int );
	void setAmbient( int );
	void setEnvMapRotation( int );
	void setGlowScale( int );
	void setFrontalLight( bool );
	void updateScene();
	void updateAnimationState( bool checked );
	void setVisMode( Scene::VisMode, bool checked = true );
	void updateSettings();
	void update3D();
	void selectPBRCubeMap();
	void update_GL( [[maybe_unused]] int tmp ) { update(); }

signals:
	void clicked( const QModelIndex & );
	void paintUpdate();
	void sceneTimeChanged( float t, float mn, float mx );
	//! Modal transform gizmo status line (empty = clear)
	void gizmoStatus( const QString & );
	//! A transform gesture was committed: mode (1 move / 2 rotate / 3 scale), axis,
	//! parameter (move: basis-space offsets; rotate: angle in [0]; scale: factor in [0])
	void transformGesture( int mode, int axis, const Vector3 & param );
	//! Show the operator redo panel for Merge (1), Select-Linked (2), or Floating Decal (3)
	void operatorPanel( int kind, float param );
	//! Show the generalized operator redo panel (typed parameter list)
	void operatorPanelEx( const QString & title, const QVector<GLView::TlOpParam> & params );
	//! F9: move the visible adjust panel next to the mouse cursor
	void redoPanelToCursor();
	//! A box select was applied (shows the gesture redo panel)
	void boxSelectApplied();
	//! A circle-select paint stroke finished (shows the gesture redo panel)
	void circleSelectApplied();
	//! A gizmo transform was committed on this block (for auto-keying)
	void transformCommitted( int blockNumber );
	//! Object/Edit mode changed (for the toolbar mode selector)
	void editModeChanged( bool editing );
	/*! Physics Sim entered or left.
	 *
	 * Every other viewport mode announces itself and the mode button listens.
	 * This one was added without a signal, so leaving it for Object Mode changed
	 * nothing the button was watching -- it kept saying "Physics Sim" and the mode
	 * looked stuck.
	 */
	void physicsSimModeChanged( bool simulating );
	void editDeformedCageChanged( bool enabled );
	//! Element pick mode changed (0 none, 1 vertex, 2 edge, 3 face)
	void pickModeChanged( int mode );
	//! Edit-mode element selection changed (emitted once per event-loop turn,
	//! after the mutation completed; consumed by the UV editor for mirroring)
	void elementSelectionChanged();
	//! Object-mode multi-selection changed (for the block-list highlight)
	void objectSelectionChanged();
	//! Viewport-hidden nodes changed (for greying block-list rows)
	void hiddenNodesChanged();
	//! Rigging paint samples are deliberately model-free. One begin/end pair is
	//! one prospective Undo command; commit=false discards the collected stroke.
	void riggingWeightStrokeBegan();
	void riggingWeightBrushSample( int targetBlock, const QVector<int> & vertices,
		const QVector<float> & falloff, int brushMode, float paintWeight, float strength );
	//! The X-mirrored half of a brush sample (only while riggingPaintMirrorX):
	//! the rigging panel accumulates these separately and commits them onto
	//! the L/R counterpart bone
	void riggingWeightBrushSampleMirrored( int targetBlock, const QVector<int> & vertices,
		const QVector<float> & falloff, int brushMode, float paintWeight, float strength );
	void riggingWeightStrokeEnded( bool commit );
	void riggingWeightPaintModeChanged( bool enabled );
	void riggingWeightPaintBrushChanged( bool enabled );
	void vertexPaintStrokeBegan();
	void vertexPaintBrushSample( int targetBlock, const QVector<int> & vertices,
		const QVector<float> & falloff );
	void vertexPaintStrokeEnded( bool commit );
	void vertexPaintModeChanged( bool enabled );
	void vertexPaintBrushChanged( bool enabled );
	void poseModeChanged( bool enabled );
	//! A bone was picked in the viewport (block number), so the dock can sync.
	void poseBonePicked( int blockNumber );
	void segmentPaintStrokeBegan();
	void segmentPaintBrushSample( int targetBlock, const QVector<int> & triangles );
	void segmentPaintStrokeEnded( bool commit );
	void segmentPaintModeChanged( bool enabled );
	void segmentPaintBrushChanged( bool enabled );
	void viewpointChanged();
	void projectionChanged( bool perspective );
	void frontalLightChanged( bool isFrontal );

	void sequenceStopped();
	void sequenceChanged( const QString & );
	void sequencesUpdated();
	void sequencesDisabled( bool );

	/*! The selected sequence's authored Cycle Type (Scene::CycleType), and
	 *  whether that means "keep playing at the end".
	 *
	 *  Emitted whenever the sequence changes, so the Loop toggle can start from
	 *  what the FILE says rather than from whatever the last sequence left it on.
	 */
	void sequenceCycleChanged( int cycleType, bool repeats );

protected:
	//! Sets up the OpenGL rendering context, defines display lists, etc.
	void initializeGL() override final;
	//! Sets up the OpenGL viewport, projection, etc.
	void resizeGL( int width, int height ) override final;
	void resizeEvent( QResizeEvent * event ) override final;
	//! Renders the OpenGL scene.
	void paintGL() override final;
	void glProjection( int x = -1, int y = -1 );

	// QWidget Event Handlers

	//! No viewport context menu: a plain right-click drops the gizmo/3D cursor
	void contextMenuEvent( QContextMenuEvent * );
	void dragEnterEvent( QDragEnterEvent * );
	void dragLeaveEvent( QDragLeaveEvent * );
	void dragMoveEvent( QDragMoveEvent * );
	void dropEvent( QDropEvent * );
	void focusOutEvent( QFocusEvent * ) override final;
	void keyPressEvent( QKeyEvent * ) override final;
	void keyReleaseEvent( QKeyEvent * ) override final;
	void mouseDoubleClickEvent( QMouseEvent * ) override final;
	void mouseMoveEvent( QMouseEvent * ) override final;
	void mousePressEvent( QMouseEvent * ) override final;
	void mouseReleaseEvent( QMouseEvent * ) override final;
	void wheelEvent( QWheelEvent * ) override final;

protected slots:
	void saveImage();

private:
	static const Vector3 viewRotations[6];

	NifModel * model;
	Scene * scene = nullptr;
	QVector<Vector3> collisionPreviewSoup;
	PhysicsPreview physicsPreview;
	bool physicsShowStats = true;
	//! forward a viewport event to the physics mode; true if it consumed it
	bool physicsMousePress( QMouseEvent * event );
	bool physicsMouseMove( QMouseEvent * event );
	bool physicsMouseRelease( QMouseEvent * event );
	bool physicsKeyPress( QKeyEvent * event );
	//! wheel: reel the held body in or out, or roll it with Ctrl. False when
	//! nothing is held, so the wheel keeps zooming the camera the rest of the time.
	bool physicsWheel( QWheelEvent * event );
	//! where the cursor was on the last move, so a Ctrl-drag has a delta to
	//! measure -- the rotate gesture is relative and the events are absolute
	QPointF physicsRotateLast;
	QVector<Vector3> sessionDocumentPreviewSoup;
	//! Per-vertex flat-shaded colors for the opaque session preview; computed
	//! once per soup rebuild from face normals against a fixed light direction.
	QVector<FloatVector4> sessionDocumentPreviewColors;
	//! Documents flagged translucent, drawn in a second blended pass.
	QVector<Vector3> sessionDocumentGhostSoup;
	QVector<FloatVector4> sessionDocumentGhostColors;
	//! Fully rendered workspace documents, one Scene per model. Keyed by model
	//! so a rebuild is only paid when a document joins, not every frame.
	QVector<NifModel *> workspaceRenderOrder;
	QHash<NifModel *, class Scene *> workspaceScenes;
	class Scene * workspaceSceneFor( NifModel * model );
	//! Secondary models edited since their Scene last read them.
	QSet<NifModel *> workspaceScenesStale;
	//! Block count when each secondary Scene was made; a change means structural.
	QHash<NifModel *, int> workspaceSceneBlockCount;
	//! How many times a Scene has been BUILT for a model. A rebuild is what a
	//! destroyed Scene looks like from outside, and "does it have one" cannot see
	//! that, because asking builds one.
	QHash<NifModel *, int> workspaceSceneBuilds;
public:
	int workspaceSceneBuildCount( NifModel * model ) const
	{
		return workspaceSceneBuilds.value( model );
	}
private:
	void flushStaleWorkspaceScenes();
	bool workspaceStaleFlushQueued = false;
	//! Pose the marked skeleton and give its pose to every scene that snaps to it.
	void applyWorkspaceSkeleton( const Transform & viewTrans );
	NifModel * workspaceSkeletonModel = nullptr;
	//! NaN forces the next push; see applyWorkspaceSkeleton.
	double workspaceSkeletonFingerprint = 0.0;
	//! Give the pose to the documents that asked for it. True when it took charge.
	bool applyPoseFollowers( const Transform & viewTrans );
	/*! Documents marked as pose followers. Weak, exactly like the face-donor mark:
	 *  a closed document must take its mark with it, and this is compared against
	 *  live models on the render path. */
	QList<QPointer<NifModel>> workspaceFollowerModels;
	//! NaN forces the next push; covers the pose AND which documents follow it.
	double workspaceFollowFingerprint = 0.0;
	//! Drop every workspace Scene (on close, or when the list empties).
	void clearWorkspaceScenes();
	QVector<Vector3> riggingDonorPreviewSoup;
	bool riggingDonorPreviewFilled = true;
	bool riggingDonorPreviewWireframe = true;
	float riggingDonorPreviewOpacity = 0.24f;
	QVector<Vector3> riggingWeightPreviewSoup;
	QVector<FloatVector4> riggingWeightPreviewColors;

	ViewState view;
	DebugMode debugMode;
	bool perspectiveMode;
	bool soloMode = false;

	void updateSoloNode();

	// Blender style modal transform gizmo (G/R/S, X/Y/Z constraint, LMB commit, Esc cancel)
	int gizmoMode = 0;                  // 0 none, 1 move, 2 rotate, 3 scale
	int gizmoAxis = 0;                  // 0 view plane / all, 1 X, 2 Y, 3 Z
	int gizmoPlane = 0;                 // move: 0 none, 1..3 = axis excluded (Shift+X/Y/Z)
	bool gizmoAxisLocal = false;        // second tap of the axis key switched to local
	bool gizmoTrackball = false;        // R,R: trackball rotation
	Matrix gizmoBasisOrig;              // basis at gesture start (restore after X,X local)
	QStringList gizmoNum;               // typed numeric entry (Blender style); all parts empty = mouse drive
	int gizmoNumCur = 0;                // part being typed (Tab cycles on unconstrained move)
	QPoint gizmoStartPos;
	//! Accumulated cursor-wrap offset: the modal drag is unbounded (Blender),
	//! so when the cursor wraps around a screen edge the jump is carried here
	//! and added to the reported position to keep the motion continuous
	QPoint gizmoWrapOffset;
	Vector3 gizmoOrigTrans;
	Matrix gizmoOrigRot;
	float gizmoOrigScale = 1;
	QPersistentModelIndex gizmoBlock;

	//! Per-node state for transforming a multi-selection together (Blender)
	struct GizmoNodeState
	{
		QPersistentModelIndex iBlock;
		Vector3 origTrans;
		Matrix origRot;
		float origScale = 1.0f;
		Matrix parentRot;
		Vector3 parentPos;
		float parentScale = 1.0f;
		Vector3 origWorldPos;
	};
	QVector<GizmoNodeState> gizmoNodes; // every transformed node; first = primary

	// captured at gizmoBegin so the frame of reference stays fixed during the drag
	Matrix gizmoBasisM;                 // orientation basis (world <- constraint space)
	Vector3 gizmoPivotWorld;            // pivot point in world space
	Vector3 gizmoOrigWorldPos;          // node origin in world space at begin
	Matrix gizmoParentRot;              // parent world rotation
	Vector3 gizmoParentPos;             // parent world translation
	float gizmoParentScale = 1;

	//! Orientation basis for the current gizmoOrient setting (world <- basis)
	Matrix gizmoBasis( const QModelIndex & iBlock ) const;
	//! Orientation basis for an explicit orientation (0 Global, 1 Local, 2 Parent, 3 View)
	Matrix gizmoBasisFor( const QModelIndex & iBlock, int orient ) const;
	//! World-space pivot point for the current gizmoPivot setting
	Vector3 gizmoPivotPoint( const QModelIndex & iBlock ) const;

	// draggable handles (arrows / rings / boxes drawn at the pivot)
	bool gizmoHandleDrag = false;       // LMB held on a handle; release commits
	int gizmoHover = 0;                 // handle under the mouse (highlight)

	// last committed gesture, frozen for the redo panel
	Vector3 gizmoLastParam;             // updated live during the gesture
	Vector3 gizmoLastRotAxis;           // world axis of the last rotation
	int lastGizmoMode = 0;
	int lastGizmoAxis = 0;
	int lastGizmoOrient = 0;
	QPersistentModelIndex lastGizmoBlock;
	Matrix lastBasis, lastParentRot, lastOrigRot;
	Vector3 lastPivot, lastParentPos, lastOrigWorldPos, lastOrigTrans;
	float lastParentScale = 1.0f;
	float lastOrigScale = 1.0f;
	int lastUndoIndex = -1;
	// last committed EDIT-MODE (element) gesture, frozen for the redo panel
	// (lastElemVerts is declared after the ElemVert struct, below)
	bool lastGestureElement = false;    //!< the last gesture edited verts, not a node
	Vector3 lastElemPivot;
	int lastElemMode = 0;               //!< 1 move, 2 rotate, 3 scale
	int lastElemAxis = 0;
	//! Re-apply the last edit-mode transform with new parameters (redo panel)
	bool gizmoReapplyElement( const Vector3 & param, int axisOverride );
public:
	//! Camera transform identical to the one paintGL uses (used by the UV
	//! editor's Project From View)
	Transform viewTransform() const;
	//! Project a world point to logical widget coordinates
	bool worldToScreen( const Vector3 & w, QPointF & out ) const;
private:
	//! Which handle is under this position: 0 none, 1-3 move XYZ, 4 view-plane move,
	//! 5-7 rotate rings, 8-10 scale boxes
	int gizmoHandleHitTest( const QPointF & pos ) const;
	//! Gizmo size in world units for a constant on-screen size (like the cursor)
	float gizmoScale( const Vector3 & pivot ) const;
	//! Show origin points + parent links of selected nodes (Blender)
	bool showOrigins = true;
	//! Blender-style silhouette outlines around the object-mode selection
	//! (active #FF9D00, secondary #FF7200, white while transforming)
	void drawObjectOutlines();

	// --- Blender-style navigation gizmo (top-right axis-ball widget) ---
	bool navGizmoDrag = false;          //!< dragging the gizmo to orbit
	int  navGizmoHover = -1;            //!< axis ball under the mouse (0..5), 6 = ring, -1 none
	//! Centre / radius / ball radius of the gizmo in logical (device-independent) px.
	void navGizmoLayout( QPointF & center, float & radius, float & ballRadius ) const;
	//! Screen position (logical px) and view-space depth of the 6 axis balls
	//! (0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z).
	void navGizmoBalls( QPointF pos[6], float depth[6] ) const;
	//! Axis ball (0..5) under pos, 6 for the orbit ring, or -1.
	int  navGizmoHitTest( const QPointF & pos ) const;
	//! Snap the view so the given axis (0..5) points at the camera.
	void snapToAxis( int axis );
	//! Draw the navigation gizmo overlay with QPainter.
	void drawNavGizmo( class QPainter & painter );
	//! Draw the Blender-style 3D cursor overlay (constant screen size).
	void drawCursorOverlay( class QPainter & painter );

public:
	static float gizmoSnapStep;
	static float gizmoSizeMul;          // user scale for gizmo handles + 3D cursor
	static float wireWidthMul;          // wireframe thickness multiplier (overlays + edit mode)
	static float vertexPointSize;       // edit-mode vertex dot size in logical px
	static float selLineWidth;          // selected edge/element line width in logical px
	bool gizmoAutoKey = false;
	bool gizmoHandlesOn = true;         // draw + pick the draggable handles
	int gizmoOrient = 0;                // 0 Global, 1 Local, 2 Parent, 3 View
	int gizmoPivot = 0;                 // 0 node origin, 1 bounding center

	//! Re-apply the last committed gesture with new parameters (redo panel).
	//! axisOverride (0 View, 1-3 XYZ) and orientOverride (0-3) re-express the
	//! gesture on another axis / in another orientation when >= 0.
	//! Returns false if the gesture went stale (undo stack moved on).
	bool gizmoReapply( const Vector3 & param, int axisOverride = -1, int orientOverride = -1 );

	// ---- element reference picking (vertex / edge / face) ----

	int pickMode = 0;                   // 0 off, 1 vertex, 2 edge, 3 face

	struct PickedElement
	{
		int shapeBlock = -1;            // block number of the shape
		int type = 0;                   // 1 vertex, 2 edge, 3 face
		int e0 = -1, e1 = -1;           // vertex indices (vertex: e0; edge: e0-e1; face: triangle index in e0)
		Vector3 worldPos;               // reference point (vertex / edge midpoint / face center)
		Vector3 worldNormal;            // face normal for face picks
		Vector3 wA, wB, wC;             // world corners for drawing
		bool operator==( const PickedElement & o ) const
		{
			return shapeBlock == o.shapeBlock && type == o.type && e0 == o.e0 && e1 == o.e1;
		}
	};
	QVector<PickedElement> pickedElems;
	//! Per-shape edit-mode selections remembered for the session (restored on
	//! re-entering edit mode on the same mesh; cleared when a new file loads)
	QHash<int, QVector<PickedElement>> savedElemSelections;

	Vector3 pickedMedian() const;
	//! Pick the element under pos into the selection; additive toggles membership
	bool pickElementAt( const QPointF & pos, bool additive );
	//! The element under pos, without changing the selection
	bool pickElementUnder( const QPointF & pos, PickedElement & pe ) const;
	//! Blender path select: select the shortest path (BFS over the edge graph)
	//! from the active element to the element under pos, in-between geometry included
	bool pickPathSelect( const QPointF & pos );
	//! Mouse ray in world space
	void mouseRayWorld( const QPointF & pos, Vector3 & origin, Vector3 & dir ) const;

	struct SceneRayHit
	{
		class Shape * shape = nullptr;
		int tri = -1;
		float dist = 1.0e30f;           // world-space distance along the ray
		Vector3 hitLocal;
	};
	//! Closest triangle hit under pos; excludeBlock (and its subtree) is skipped.
	//! If onlyShapes is non-null, only shapes whose block is in it are considered.
	SceneRayHit raycastScene( const QPointF & pos, int excludeBlock = -1, const QSet<int> * onlyShapes = nullptr ) const;
	//! Render-accurate local->world transform of a node (honours billboard facing,
	//! unlike worldTrans()), so picking and highlights line up with what is drawn
	Transform shapeRenderTrans( class Node * n ) const;
	//! Vertex position in the coordinate space currently exposed by Edit Mode.
	Vector3 editVertexLocal( class Shape * shape, int vertexIndex ) const;
	//! Convert an Edit Mode cage position back to the authored vertex position.
	bool editVertexRawLocal( class Shape * shape, int vertexIndex,
		const Vector3 & cageLocal, Vector3 & rawLocal ) const;
	void refreshPickedElementPositions();

	// ---- 3D cursor ----
	Vector3 cursorPos;                  // world space
	bool showCursor = true;

	// ---- Normals menu (Alt+N) ----
	//! Copy Vector / Paste Vector hold one normal between invocations, as
	//! Blender's do. Shape-local, and deliberately not persisted: a normal from
	//! a file you have since closed is not a thing anyone means to paste.
	Vector3 normalClipboard;
	bool normalClipboardValid = false;
	//! The selection, triangle list and positions every Normals operator reads
	//! before it starts. False when there is nothing usable to work on, with the
	//! reason already reported.
	bool normalsOpContext( int & sb, QSet<int> & sv, QSet<int> & faces,
		QVector<Triangle> & tris, QVector<Vector3> & pos, const char * opName );
	//! Place the cursor on the surface under pos (or on the view plane at Dist)
	void placeCursor( const QPointF & pos );
	//! Move the selected node so its origin lands on the cursor (undoable)
	void snapNodeToCursor();
	//! Set a block's Translation so its world origin lands on worldPos
	//! (pushes onto the current ChangeValueCommand transaction)
	bool snapBlockWorldPos( int blockNum, const Vector3 & worldPos );
	//! Move all picked vertices to the cursor (edits mesh data, undoable snapshot)
	void movePickedVertsToCursor();

	// edit-mode hide (Blender H / Alt+H): per shape block, hidden triangle indices
	QHash<int, QSet<int>> editHiddenTris;
	void hideSelectedElements();
	void unhideAllElements();

	// ---- quad layer (Blender-style quads over the triangle-only NIF) ----
	//! A quad is a pair of adjacent triangles whose shared edge is "marked" as
	//! a diagonal: the wireframe hides it, face picking selects both halves,
	//! and Loop Cut prefers it over the geometric guess. The NIF data itself
	//! stays triangles at all times, so saving needs no triangulation step.
	//! Marks are validated against the live topology on use; a changed vertex
	//! count invalidates a shape's marks wholesale (guarded by quadMarkVerts).
	QHash<int, QSet<quint64>> quadDiagonals;
	QHash<int, int> quadMarkVerts;      //!< vertex count the marks were made for
	static quint64 quadEdgeKey( int a, int b )
	{
		if ( a > b ) { int t = a; a = b; b = t; }
		return ( quint64( quint32( a ) ) << 32 ) | quint32( b );
	}
	//! The validated diagonal set for a shape (empty when stale)
	const QSet<quint64> quadMarksFor( int shapeBlock ) const;
	bool isQuadDiagonal( int shapeBlock, int a, int b ) const;
	//! The marked partner triangle of tri (same shape), or -1
	int quadPartnerTri( int shapeBlock, int tri ) const;
	//! Cached tri -> partner map for a shape's marked quads (O(1) lookups for
	//! box/circle select and Subdivide; invalidated on mark changes and by
	//! the vertex-count guard)
	const QHash<int, int> & quadPartnerMap( int shapeBlock ) const;
	mutable QHash<int, QPair<int, QHash<int, int>>> quadPartnerCache;

	// ---- overlay soup caches (perf) ----
	//! Camera-independent edit-overlay structures cached across repaints.
	//! Rebuilding the unique-edge / quad-adjacency / filled-tris sets is an
	//! O(T) hash pass with fresh allocations that used to run every frame,
	//! camera orbits included. Positions stay per-frame (they depend on the
	//! eye); topology survives gestures (a move changes no indices) and
	//! growth (extrude) is caught by the size fingerprints. filledTris also
	//! depends on the selection — fingerprinted per frame via selHash, so
	//! selection mutations need no hooks.
	struct EditOverlaySets {
		QVector<QPair<int, int>> edges;
		QVector<int> visVerts;
		QHash<quint64, int> markAdj;
		QSet<int> filledTris;
		int nTris = -1, nVerts = -1, nHidden = -1, nMarks = -1;
		quint64 selHash = 1;
		int pickModeUsed = -1;
	};
	QHash<int, EditOverlaySets> editOverlaySets;
	bool editOverlaySetsValid = false;
	//! Wireframe-overlay unique edge lists (same idea; positions per frame)
	QHash<int, QVector<QPair<int, int>>> wireEdgeCache;
	QHash<int, quint64> wireEdgeCacheKey;
	bool wireEdgeCacheValid = false;
	//! Drop both overlay caches — call after anything that changes topology,
	//! hidden triangles, or quad marks outside the size fingerprints' reach
	void invalidateOverlayCaches() { editOverlaySetsValid = false; wireEdgeCacheValid = false; }

	// ---- X-mirror editing (Blender's Mirror X) ----
	//! Modal transforms also move the unselected mirror partner of each
	//! vertex, X-negated in local space; partners are paired by position
	//! (1e-3 tolerance) at first use and cached per topology
	bool mirrorEditing = false;
	int mirrorAxis = 0;                  //!< 0 X, 1 Y, 2 Z (Blender mirror axis)
	mutable QHash<int, QPair<int, QHash<int, int>>> mirrorPairCache;
	int mirrorPartnerOf( int shapeBlock, int vi ) const;
public:
	void setMirrorEditing( bool on ) { mirrorEditing = on; }
	bool mirrorEditingActive() const { return mirrorEditing; }
	void setMirrorAxis( int a ) { a = qBound( 0, a, 2 ); if ( a != mirrorAxis ) { mirrorAxis = a; mirrorPairCache.clear(); } }
	int mirrorAxisValue() const { return mirrorAxis; }
private:
	//! Fill a face PickedElement for a triangle (world corners, center, normal)
	bool buildFacePick( int shapeBlock, int tri, PickedElement & pe ) const;
	//! Add the missing quad partners of any face picks in elems (a marked
	//! quad always selects / deselects as one face)
	void expandQuadPartners( QVector<PickedElement> & elems ) const;
	//! Replace a shape's marks through the undo stack. vertCountOverride
	//! records the marks against a vertex count other than the shape's
	//! current one (for ops whose scene state is not yet rebuilt).
	void setQuadMarks( int shapeBlock, const QSet<quint64> & marks, const QString & opName,
		int vertCountOverride = -1 );
	//! F: form a quad from 2 adjacent face-picked tris / 4 verts, else fill/bridge
	void makeFace();
	//! Alt+J: greedily pair the face-selected triangles into quads.
	//! armPanel arms the Blender-style adjust-last-operation panel (off when
	//! called from the panel's own rerun).
	void trisToQuads( float maxFaceAngleDeg = 40.0f, float maxShapeAngleDeg = 40.0f,
		bool armPanel = true );
	//! Ctrl+T: dissolve quads in the selection back to visible triangles.
	//! diagonalMode: 0 keep current, 1 beauty (max-min-angle), 2 shortest
	//! diagonal, 3 longest diagonal — flips rewrite the two triangles.
	void triangulateSelection( int diagonalMode = 0, bool armPanel = true );
	/*! Ctrl+N / Shift+Ctrl+N: make the selected faces' winding CONSISTENT and
	 *  face them out (or in), as Blender's Recalculate Outside/Inside does.
	 *
	 *  Distinct from recalcSelectedNormals(), which only re-derives vertex
	 *  normals from the winding already there — the thing Blender calls Reset
	 *  Vectors. This one decides the winding itself.
	 */
	void recalcNormalsSelection( bool inside = false, bool armPanel = true );
	//! Alt+N: the Normals menu, at the cursor.
	void showNormalsMenu();
	//! Set each selected face's corner normals to that face's own normal —
	//! Blender's Normals ▸ Set from Faces.
	void normalsSetFromFaces();
	//! Point the selected vertices' normals at (or away from) the 3D cursor —
	//! Blender's Normals ▸ Point to Target.
	void normalsPointToTarget( bool invert = false );
	//! Average the normals of selected vertices that share a position, so a
	//! seam shades as one surface — Blender's Normals ▸ Merge.
	void normalsMergeCoincident();
	//! Recompute selected normals weighted by face area or by corner angle —
	//! Blender's Normals ▸ Average ▸ Face Area / Corner Angle.
	void normalsAverage( int mode );
	//! Copy the active vertex's normal / paste it onto the selection —
	//! Blender's Normals ▸ Copy Vector / Paste Vector.
	void normalsCopyVector();
	void normalsPasteVector();
	//! Blur each selected normal towards its neighbours' average.
	void normalsSmoothVectors( float factor = 0.5f, bool armPanel = true );
	//! Universal toolbar visibility commands. In Edit/Weight Paint, isolate the
	//! selected geometry; in Object Mode, isolate all selected objects.
	void isolateSelected();
	//! Isolate only the active/primary object, regardless of viewport mode.
	void isolatePrimary();
	//! Hide every selected object except the active/primary object.
	void hideSecondarySelection();
	//! Clear object isolation, hidden objects, and hidden edit geometry.
	void restoreAllVisibility();
	//! Blender-style delete menu (X / Delete) in edit mode
	void showDeleteMenu();
	//! Delete geometry per Blender mode: 0 Vertices, 1 Edges, 2 Faces, 3 Only Faces
	void deleteGeometry( int mode );
	//! Blender-style Merge menu (M) in edit mode
	void showMergeMenu();
	//! Merge selected vertices. mode: 0 At Center, 1 At Cursor, 2 By Distance
	void mergeVertices( int mode, float threshold = 0.001f );

	// ---- Separate (P) / Join (Ctrl+J) / Duplicate (Shift+D), Blender-style ----
	//! Edit mode: P menu (Selection / By Loose Parts / By Segment)
	void showSeparateMenu();
	//! Move the selected geometry into a new BSTriShape sibling, preserving
	//! the vertex data (incl. normals) verbatim so a later Join is seamless
	void separateSelection();
	//! Split every edited mesh into its connected pieces (loose = true) or into
	//! one shape per FO4 / SSE sub-index segment (loose = false). Both ignore the
	//! element selection and act on the whole mesh, as Blender's variants do.
	void separateByGroups( bool loose );
	//! How many NEW shapes separateByGroups( loose ) would produce right now — the
	//! P menu shows it in the item text, so the count is visible before the click
	//! rather than after the undo.
	int separateGroupsPreview( bool loose );
	//! Outcome of a Separate: triangles moved out of their source shape, shapes
	//! created, and the last of them (selected afterwards).
	struct SeparateResult { int moved = 0; int newObjects = 0; int lastNew = -1; };
	//! Shared core for every Separate variant. grouper( shapeBlock, group ) fills a
	//! per-triangle group id and returns the group count; group 0 stays in the
	//! source shape and each other group becomes a new sibling. One undo macro.
	SeparateResult separateShapes( const QVector<int> & shapes,
		const std::function<int( int, QVector<int> & )> & grouper, const QString & opName );
	//! Meshes the whole-mesh Separate variants act on (everything in edit mode).
	QVector<int> separateTargetShapes() const;
	//! Leave edit mode and select the newly made shape — shared post-amble.
	void finishSeparate( const SeparateResult & r );
	//! Object mode: merge the selected compatible BSTriShapes into the active
	//! node (verts transformed into its space, triangles reindexed)
	void joinSelectedObjects();
	//! Object mode: decimate the selected BSTriShapes to a ratio of their
	//! triangles, arming the redo panel so the ratio can be scrubbed afterwards.
	void decimateSelectedObjects( float ratio = 0.5f );
	//! The same join, told what to join rather than reading the viewport's own
	//! selection — so the Block List can run the identical gesture.
	void joinObjects( int active, QSet<int> selection );
	//! Object-mode X / Delete (Blender): delete the current object selection,
	//! whole branch each, as one undo step. X asks first, Delete does not.
	void deleteSelectedObjects( bool confirm = true );
	//! Delete the given blocks and their child branches as one undo step.
	//! `confirm` puts up the Blender-style "Delete selected objects?" popup:
	//! the X key and the menus ask, the Delete key does not. Shared by the
	//! object-mode viewport and the Block List. Returns blocks removed.
	int deleteBlocks( const QVector<int> & blocks, bool confirm = true );
	//! Shift+D: duplicate the selection and start a move gesture
	void duplicateSelection();
	//! Edit-mode Shift+D: duplicate the picked verts/faces within the mesh
	void duplicateElements();
	//! Y: detach the face selection in place — boundary verts are duplicated
	//! and the selected faces re-pointed onto the copies (no new block)
	void splitSelection();
	//! V: rip along the selected interior edge path; the side under the
	//! cursor takes the duplicated verts and a chained move starts.
	//! v1: interior path verts only (>= 2 edges), one mesh per rip.
	void ripSelection();
	//! Ctrl+B: bevel the selected edge path — rip + offset both rows into
	//! their side's surface plane + bridge with a marked-quad strip that
	//! tapers closed into the welded endpoints. Width scrubbable (panel);
	//! width < 0 = derive from the average path edge length.
	void bevelSelection( float width = -1.0f, bool armPanel = true );
	//! Blender-style Ctrl+P menu. The active NiNode is the parent; any selected
	//! NiAVObject-compatible blocks are children.
	void showParentMenu();
	//! Set the selection's parent. mode: 0 replace parents + keep world,
	//! 1 replace parents + keep local, 2 add another parent + keep local.
	void parentSelection( int mode );
	//! Blender-style Alt+P menu.
	void showClearParentMenu();
	//! Remove all NiNode parents from selected objects, optionally preserving world transforms.
	void clearParentSelection( bool keepWorld );
	//! Copy the selected faces to a separate BSTriShape and lift them along their
	//! vertex normals, ready for an independently assigned decal material.
	void createFloatingDecal( float offset = 0.1f );
	//! Shift+Ctrl+Alt+C: Blender-style Set Origin menu (object mode)
	void showSetOriginMenu();
	//! Set the node origin / move geometry. mode: 0 geometry-to-origin,
	//! 1 origin-to-geometry, 2 origin-to-cursor, 3 CoM surface, 4 CoM volume
	void setOrigin( int mode );

	// ---- element modal transform (G/R/S on picked verts/edges/faces) ----
	bool elemTransform = false;
	struct ElemVert { int shape; int idx; Vector3 origLocal; Vector3 origWorld; Vector3 currentLocal;
		int mirrorOf = -1;	//!< >= 0: follower of elemVerts[mirrorOf], X-negated (X-mirror)
		float falloff = 1.0f; };	//!< < 1: proportional-editing neighbour, transform scaled
	QVector<ElemVert> elemVerts;
	QVector<ElemVert> lastElemVerts;    //!< last committed edit gesture, for the redo panel

	// Proportional editing (Blender O): a transform spreads to unselected
	// vertices within a radius, scaled by a falloff curve.
	bool proportionalEdit = false;
	int  proportionalFalloff = 0;       //!< 0 Smooth 1 Sphere 2 Root 3 InvSquare 4 Sharp 5 Linear 6 Constant 7 Random
	float proportionalRadius = 0.0f;    //!< world units; 0 = auto from model size
	//! Falloff weight for a neighbour at world distance d within the radius.
	float proportionalWeight( float d, float radius, int vseed ) const;
	//! Effective radius (auto-sized from the edited shapes' bounds if 0).
	float proportionalEffectiveRadius() const;
public:
	void setProportionalEdit( bool on ) { proportionalEdit = on; emit gizmoStatus( on ? tr( "Proportional editing on" ) : tr( "Proportional editing off" ) ); }
	bool proportionalEditActive() const { return proportionalEdit; }
	void setProportionalFalloff( int f ) { proportionalFalloff = qBound( 0, f, 7 ); }
	int  proportionalFalloffType() const { return proportionalFalloff; }
	void setProportionalRadius( float r ) { proportionalRadius = qMax( 0.0f, r ); }
	float proportionalRadiusValue() const { return proportionalRadius; }
	Vector3 elemPivot;                  // world-space pivot (median or 3D cursor)
	//! Vertices affected by the current picked-element selection, grouped per shape
	QHash<int, QSet<int>> pickedVertexRefs() const;
	//! Nearest vertex to a screen position across the editable shapes (within
	//! radius px); returns its distance and fills out. Floating verts (extruded
	//! spurs) sit in front of other surfaces and never raycast, so vertex
	//! picking must consider this in addition to the surface hit.
	float nearestScreenVertex( const QPointF & pos, float radius,
		const QSet<int> * only, PickedElement & out ) const;
	bool gizmoBeginElement( int mode );
	void gizmoUpdateElement( const QPoint & pos, Qt::KeyboardModifiers mods );
	void gizmoEndElement( bool commit );

	// Blender-like free camera: keyboard movement only while enabled (Shift+F)
	bool freeCamera = false;
	//! Enter/exit the free camera: cursor, keyboard grab and status message
	void setFreeCamera( bool on );
	//! Fly-mode movement speed multiplier, adjusted with the scroll wheel
	float freeCamSpeed = 1.0f;
	//! First-person look that pivots at the camera eye (free camera mouse-look)
	void freeCameraLook( float dPitch, float dYaw );

	// Blender-like edit mode: vertex/edge/face editing on the selected mesh
	bool editMode = false;
	bool editSkinningWasEnabled = false; //!< render option restored when leaving Edit/Weight Paint
	bool editDeformedCage = true;        //!< evaluated skinned cage; false exposes raw bind vertices
	int editShapeBlock = -1;            //!< primary mesh (pivot/gizmo fallback)
	QSet<int> editShapeBlocks;          //!< all meshes being edited (multi-mesh)
	bool wireframeOverlay = false;      //!< Blender wireframe overlay (black wires over the shaded render)

	// Blender-style object-mode multi-selection (viewport + block list)
	QSet<int> objSelection;             //!< selected NiAVObject block numbers
	int objActive = -1;                 //!< active (last-selected) block
	//! Apply a viewport click to the object selection (Shift = extend/toggle)
	void objectSelectClick( int avBlock, bool shift );
	//! Sync the object selection from a single tree/list selection
	void syncObjectSelection( int avBlock );
	//! Set the whole object selection at once (from block-list multi-selection)
	void setObjectSelection( const QSet<int> & sel, int active );

	// ---- Blender box select (B) + invert (Ctrl+I) ----
	bool boxSelecting = false;          //!< armed by B until a drag completes / cancels
	bool boxSelectDrag = false;         //!< actively rubber-banding a rectangle
	QPoint boxSelectStart;              //!< drag anchor (widget coords)
	QPoint boxSelectCur;                //!< current drag corner (widget coords)
	int boxSelectPrevActive = -1;       //!< object-mode primary before the box select
	//! B: arm box select in edit or object mode
	void beginBoxSelect();
	//! Apply the finished rectangle. Additive: plain adds, Shift/Ctrl deselects
	void applyBoxSelect( const QRect & rect, Qt::KeyboardModifiers mods );
	QRect lastBoxRect;                  //!< last applied rectangle (for the redo panel)
	bool boxReapplying = false;         //!< suppress re-arming the panel during Deselect
	int lastGestureKind = 0;            //!< 1 box, 2 circle stroke (for the redo panel)
	QVector<QPointF> lastCircleStroke;  //!< brush positions of the last select stroke
	float lastCircleStrokeRad = 26.0f;  //!< brush radius of that stroke
	//! The redo panel's Deselect button: re-apply the last box / circle stroke
	//! subtractively
	void deselectLastGesture();
	//! Ctrl+I: invert the selection (object mode: objects; edit mode: elements)
	void invertSelection();
	//! A key / context menu: 0 = Blender A toggle, 1 = select all, 2 = deselect all
	void selectAll( int action = 0 );
	//! Ctrl+= / Ctrl+-: grow / shrink the edit-mode selection one adjacency ring
	void selectMoreLess( bool more );
	//! Blender Checker Deselect: drop every Nth selected element (redo panel)
	void checkerDeselect( int nth = 2, int offset = 0, bool armPanel = true );
	//! Shift+R: re-run the last adjust-panel operator with its current
	//! parameter values on the current selection (Blender Repeat Last)
	void repeatLastOperator();
	//! Alt+click: select the edge loop through the edge under pos (extend keeps
	//! the current selection). Returns false if no edge was hit.
	bool selectEdgeLoop( const QPointF & pos, bool extend );

	// ---- Blender circle select (C) ----
	bool circleSelecting = false;       //!< armed by C until RMB / Esc exits
	bool circlePainting = false;        //!< LMB held: painting select
	bool circleErasing = false;         //!< MMB held: painting deselect
	QPointF circleSelectPos;            //!< brush position (widget coords)
	static float circleSelectRadius;    //!< brush radius in px (persists for the session)
	//! C: arm the circle-select brush (LMB paints, MMB erases, wheel resizes,
	//! RMB / Esc exits)
	void beginCircleSelect();
	//! Select (erase=false) / deselect (erase=true) everything under the brush
	void applyCircleSelect( const QPointF & pos, bool erase );

	// ---- knife (K): Blender-style cut tool ----
	//! One placed (or hovered) cut point: snapped to a vertex, onto an edge
	//! at edgeT, or a free point on a face (waypoint only in v1)
	struct KnifePoint
	{
		int shapeBlock = -1;
		int snapVert = -1;
		int edgeA = -1, edgeB = -1;
		float edgeT = 0.0f;
		int faceTri = -1;               //!< free point: the hit triangle …
		Vector3 bary;                   //!< … and its barycentric coordinates
		Vector3 world;
		QPointF screen;
		bool valid() const { return shapeBlock >= 0; }
	};
	bool knifeActive = false;           //!< armed by K until Enter applies / Esc cancels
	bool knifeCutThrough = true;        //!< Z while armed: also cut occluded (but front-facing) edges
	QVector<KnifePoint> knifePoints;    //!< committed cut points, in click order
	KnifePoint knifeHoverPt;            //!< live point under the cursor
	bool knifeHoverValid = false;
	//! undo-stack activity while the knife is armed invalidates the cut
	//! points' vertex indices, so it cancels the knife (armed in beginKnife)
	QMetaObject::Connection knifeUndoConn;
	//! K: arm the knife (LMB places cut points, MMB orbits, Enter cuts, Esc cancels)
	void beginKnife();
	void cancelKnife();
	//! place a cut point at the cursor
	void knifeAddPoint( const QPointF & pos );
	//! Z while armed: toggle cutting occluded front-facing edges
	void knifeToggleCutThrough();
	//! Enter: v2 — interior points become poked vertices (one per triangle),
	//! every crossed edge splits at the crossing, multi-mesh polylines apply
	//! per shape in one undo macro
	void knifeApply();
	//! raycast + Blender-style snapping (vertex 11px, edge 8px, else face)
	bool knifeProbe( const QPointF & pos, KnifePoint & kp ) const;

	// ---- Loop Cut modal (Ctrl+R): Blender behavior ----
	// The ring under the cursor previews in yellow; the ring walk crosses
	// MARKED quads only (plain triangles stop the loop, like Blender).
	// Wheel / typed digits / +/- set the cut count. LMB or Enter applies the
	// cut CENTERED, marks the new cells as quads, selects the new loop as
	// edges (orange), and arms the adjust panel (Number of Cuts + Factor).
	// RMB / Esc cancels. One undo step for the whole gesture.
	bool loopCutActive = false;
	int loopCutCuts = 1;
	int loopCutTyped = -1;              //!< digit accumulator; -1 = none typed
	//! ring resolved from the hovered edge, in index space (world preview
	//! positions derive per paint so orbiting keeps the loop glued)
	int loopCutShape = -1;
	QVector<QPair<int, int>> loopCutRingEdges;
	QVector<QPair<int, int>> loopCutQuadTris;
	bool loopCutClosed = false;
	quint64 loopCutSeedEdge = 0;        //!< hover cache: same edge = no recompute
	//! per-shape adjacency cached while the modal is up — a probe runs per
	//! mouse move and must not rebuild an O(T) hash on a big mesh each time
	int loopCutAdjShape = -1;
	QVector<Triangle> loopCutTriCache;
	QHash<quint64, QVector<int>> loopCutAdjCache;
	QVector<PickedElement> loopCutSeedSel;   //!< selection snapshot for the redo panel
	void beginLoopCut();
	void cancelLoopCut();
	//! resolve the ring under the cursor
	void loopCutProbe( const QPointF & pos );
	//! LMB/Enter: apply the centered cut, mark quads, select the loop, panel
	void loopCutConfirmRing();
	//! digits / +/- / Backspace while the modal is up; true = key consumed
	bool loopCutModalKey( int key );

	// ---- Rigging weight-paint brush ----
	bool riggingWeightPaintMode = false;
	bool riggingWeightPaintBrushEnabled = true;
	//! Mirror weight-paint strokes across local X (position-paired partners;
	//! the rigging panel commits the mirrored half onto the L/R counterpart
	//! bone when one exists)
	bool riggingPaintMirrorX = false;
	bool riggingWeightPaintStroke = false;
	int riggingWeightPaintTarget = -1;
	int riggingWeightPaintBrushMode = 0;
	float riggingWeightPaintRadius = 32.0f;
	float riggingWeightPaintWeight = 1.0f;
	float riggingWeightPaintStrength = 0.25f;
	QPointF riggingWeightPaintPos;
	QPointF riggingWeightPaintLastSample;
	//! Static projection/front-face cache for one LMB stroke. Camera, mesh and
	//! selection cannot change during that stroke, so recomputing them for every
	//! high-frequency mouse event only adds latency.
	bool riggingWeightPaintProjectionValid = false;
	QVector<QPointF> riggingWeightPaintScreen;
	QVector<int> riggingWeightPaintCandidates;
	std::chrono::steady_clock::time_point riggingWeightPaintSampleTime;
	//! Emit vertices touched by a swept screen-space brush segment.
	void applyRiggingWeightPaintBrush( const QPointF & from, const QPointF & to );

	// ---- RGBA vertex-paint brush ----
	bool vertexPaintMode = false;
	bool vertexPaintBrushEnabled = true;
	bool vertexPaintStroke = false;
	int vertexPaintTarget = -1;
	float vertexPaintRadius = 32.0f;
	QPointF vertexPaintPos;

	// ---- Pose Mode ----
	bool poseMode = false;
	//! Skeleton Manager armature display. Draws the same bones as Pose Mode but
	//! octahedral (direction is readable) and without making them drag targets.
	bool skeletonView = false;
	QSet<int> skeletonIsolated;            //!< non-empty = draw only these
	bool poseShowBoneNames = false;
	bool poseShowRelations = true;        //!< draw parent-relationship lines
	bool poseShowWeights = false;         //!< highlight the hovered/selected bone's verts
	bool poseNonDestructive = true;       //!< restore real bone nodes on exit
	bool poseBaked = false;               //!< this session's pose was committed
	int  poseBoneFilterMode = 0;          //!< 0 all, 1 deforming, 2 face sculpt
	QSet<int> posePinned;                 //!< bones locked from transforms
public:
	void setPoseShowWeights( bool on ) { poseShowWeights = on; update(); }
	void setPoseNonDestructive( bool on ) { poseNonDestructive = on; }
	bool poseNonDestructiveActive() const { return poseNonDestructive; }
	//! Commit the current pose so leaving pose mode won't restore the bones.
	void poseBakeToBones() { poseBaked = true; capturePoseRest(); emit gizmoStatus( tr( "Pose baked into the bones" ) ); }
	//! Pin / unpin a bone (locked from transforms, mirror, proportional spread).
	void poseTogglePin( int block ) { if ( posePinned.contains( block ) ) posePinned.remove( block ); else posePinned.insert( block ); update(); }
	bool poseIsPinned( int block ) const { return posePinned.contains( block ); }
	void poseClearPins() { posePinned.clear(); update(); }
	QVector<int> poseBones;               //!< bone block numbers currently drawn/pickable
	int  poseHoverBone = -1;              //!< bone under the cursor (hover highlight)
	float poseBoneSize = 4.0f;            //!< characteristic bone length (capped stub size)
	// weight-influence overlay: verts the hovered/active bone drives, bucketed
	// by weight so a dense mesh draws in a handful of point batches (rebuilt
	// only when the target bone changes, keyed by poseWeightSig)
	static constexpr int PoseWeightBuckets = 5;
	QVector<Vector3> poseWeightPts[PoseWeightBuckets];
	quint64 poseWeightSig = ~0ull;
	void rebuildPoseWeights();
	void drawPoseWeights();
	//! Rebuild poseBones from the file's skinned shapes under the active filter.
	void refreshPoseBones();
	//! Derive poseBoneSize from the drawn bones' spacing.
	void refreshPoseBoneSize();
	//! Draw the skeleton (bones + parenting) — called from paintGL in pose mode.
	void drawPoseSkeleton();
	//! Blender's octahedral bone as a 12-segment wireframe, head to tail. The
	//! taper is what makes the bone's direction visible.
	void drawOctahedralBone( const Vector3 & head, const Vector3 & tail );
	//! Screen-space nearest bone to a viewport point; -1 if none within range.
	int poseBoneAt( const QPointF & pos ) const;
	//! A bone's tail in world space: its sole child's origin, the mean of several
	//! children, or a short stub down the local axis for a leaf.
	Vector3 poseBoneTail( int boneBlock ) const;
	//! Local transforms captured on entering pose mode = the "rest" a reset
	//! restores to. Keyed by block number.
	QHash<int, Transform> poseRestPose;
	void capturePoseRest();
	//! Rest transforms keyed by bone NAME (the key OS pose XML uses).
	QHash<QString, Transform> poseRestByName() const;
	QPointF vertexPaintLastSample;
	bool vertexPaintProjectionValid = false;
	QVector<QPointF> vertexPaintScreen;
	QVector<int> vertexPaintCandidates;
	std::chrono::steady_clock::time_point vertexPaintSampleTime;
	void applyVertexPaintBrush( const QPointF & from, const QPointF & to );

	// ---- Binary segment/subsegment face-paint brush ----
	bool segmentPaintMode = false;
	bool segmentPaintBrushEnabled = true;
	bool segmentPaintStroke = false;
	int segmentPaintTarget = -1;
	float segmentPaintRadius = 32.0f;
	QPointF segmentPaintPos;
	QPointF segmentPaintLastSample;
	bool segmentPaintProjectionValid = false;
	QVector<QPointF> segmentPaintScreen;
	QVector<int> segmentPaintCandidates;
	std::chrono::steady_clock::time_point segmentPaintSampleTime;
	void applySegmentPaintBrush( const QPointF & from, const QPointF & to );
	//! Start a modal G/R/S transform from a shortcut (edit elements if picked,
	//! else the active object/node). Returns false if a gesture is already active
	//! so the viewport can handle in-gesture mode switching. Lets the block-list
	//! selection be transformed even when the list, not the view, has focus.
	bool startModalTransform( int mode );

	// ---- operator redo panel (Merge distance / Select-Linked angle / decal offset) ----
	int lastOpKind = 0;                 //!< 0 none, 3 floating decal (1/2 migrated to Redo Panel v2)
	float lastOpParam = 0.0f;
	float lastMergeDistance = 0.0001f;  //!< Remove Doubles' default (last used By Distance value)
	QVector<PickedElement> lastOpSeed;  //!< selection to restore before a re-run
	int lastOpUndoIndex = -1;
	bool opReapplying = false;          //!< suppress re-arming the panel while re-running
	struct DecalPreviewVert { int shape = -1; int vertex = -1; Vector3 base; Vector3 normal; };
	QVector<DecalPreviewVert> lastDecalVerts; //!< cached generated verts for fast offset preview
	//! Re-run the last Merge / Select-Linked / Floating-Decal operation with a new
	//! value. Returns false if the gesture went stale (something else touched
	//! the undo stack) so the panel can freeze its inputs.
	bool reapplyOperator( float param );
	//! Store the final in-place decal preview in the structural command's redo snapshot.
	void commitOperatorPreview();

	// ---- generalized operator redo panel (Redo Panel v2, typed params) ----
	QVector<TlOpParam> lastOpExParams;
	int lastOpExUndoSteps = 0;          //!< undo-stack entries the whole gesture spans
	int lastOpExUndoIndex = -1;         //!< stale guard (undo index right after the op)
	QVector<PickedElement> lastOpExSeed; //!< selection to restore before a re-run
	//! Re-executes the armed operator with new params. Undo handling and seed
	//! restore are reapplyOperatorEx's job; this only (re)runs the op.
	std::function<void( const QVector<TlOpParam> & )> lastOpExRerun;
	bool opExReapplying = false;
	//! Arm the generalized panel: the op sets lastOpExRerun first, then calls
	//! this with the current param values, the seed (pre-op selection) and how
	//! many undo entries the interactive gesture pushed.
	void armOperatorPanelEx( const QString & title, const QVector<TlOpParam> & params,
		int undoSteps, const QVector<PickedElement> & seed );
	//! Undo the armed gesture, restore its seed selection and re-run with the
	//! new parameters. Returns false when stale (panel freezes its inputs).
	bool reapplyOperatorEx( const QVector<TlOpParam> & params );

	// ---- Extrude (E), MODELING_TOOLS_PLAN Phase 1 ----
	//! Blender E: extrude the picked region / edge run of one BSTriShape,
	//! then chain a modal move on the new cap
	void extrudeRegion();
	bool extrudeChainArmed = false;     //!< the running element move belongs to an extrude
	QVector<PickedElement> extrudeSeed; //!< pre-extrude selection (redo panel seed)
	int extrudeUndoIndexBase = -1;      //!< undo index before the extrude snapshot
	//! Verts whose normals refresh when the chained move commits (cap + ring)
	QSet<int> extrudeTouchedVerts;
	int extrudeTouchedShape = -1;
	//! Arm (and immediately consolidate) the extrude redo panel after the
	//! chained move commits or cancels; worldDelta = the move's world offset
	void armExtrudeRedoPanel( const Vector3 & worldDelta );

	// ---- Fill (F) / Bridge Edge Loops, MODELING_TOOLS_PLAN Phase 2 ----
	//! Blender-style smart connect: one closed rim loop selected = Fill (cap
	//! the hole), two rim loops = Bridge Edge Loops (band between them)
	void smartConnect();

	// ---- Phase 3/4 modeling operators ----
	//! Shared validation for the single-shape vertex operators
	bool vertexOpTarget( int & sb, QSet<int> & sv, const char * opName );
	//! Shift+V: slide the selection along its unselected neighbor edges
	//! (the redo panel's Factor is the modal)
	void edgeSlide();
	//! Laplacian relax of the selected verts (panel: Factor / Iterations)
	void smoothVertices();
	//! Reverse the winding of the selected faces (+ normal refresh)
	void flipSelectedFaces();
	//! Area-weighted normal recompute over the selected verts
	void recalcSelectedNormals();
	//! Subdivide the selected edges/faces at their midpoints (tri-native)
	void subdivideSelection();
	//! I: inset the selected face region (panel: Thickness / Depth)
	void insetRegion();
	//! Ctrl+R: insert cut rings across the edge ring under the cursor
	//! (panel: Number of Cuts)
	void loopCut();
	//! Ctrl+X: dissolve the selected verts, re-capping each one's 1-ring
	void dissolveVerts();
	//! Mirror the mesh across a local axis, welding the seam
	//! (panel: Axis / Merge Distance)
	void symmetrizeShape();
	//! Shift+A (object mode): add a primitive BSTriShape cloned from the
	//! active shape's layout + material. kind: 0 plane, 1 cube, 2 cylinder,
	//! 3 UV sphere
	void addPrimitive( int kind );
	void showAddPrimitiveMenu();
	//! W: Blender 2.7x-style Specials quick menu (edit and object mode)
	void showSpecialsMenu();

	// ---- Blender-style viewport header menus ----
	// Populate functions shared by the toolbar menu buttons and the W quick
	// menu so the two entry points can never drift apart. Enabled states are
	// evaluated at populate time; callers rebuild the menu on aboutToShow.
	//! Move / Rotate / Scale (G/R/S) via the modal transform
	void populateTransformMenu( QMenu * m );
	//! Select menu: All/None/Invert/Box/Circle (+ More/Less/Linked in edit mode)
	void populateSelectMenu( QMenu * m );
	//! Add menu (object mode): the primitive shapes
	void populateAddMenu( QMenu * m );
	//! Object menu (object mode): transform/snap/origin/duplicate/join/parent/show-hide
	void populateObjectMenu( QMenu * m );
	//! Mesh menu (edit mode): transform/snap/duplicate/extrude/separate/normals/delete
	void populateMeshMenu( QMenu * m );
	//! Vertex menu (edit mode): merge/doubles/smooth/dissolve
	void populateVertexMenu( QMenu * m );
	//! Edge menu (edit mode): loop cut/subdivide/edge slide
	void populateEdgeMenu( QMenu * m );
	//! Face menu (edit mode): extrude/inset/fill-bridge
	void populateFaceMenu( QMenu * m );
	//! Paint menu (weight/segment/vertex paint): fill selection + show/hide
	void populatePaintMenu( QMenu * m );

	// ---- selection undo (Blender: selections are undoable, Ctrl+Z) ----
	struct SelState { QVector<PickedElement> picked; QSet<int> objSel; int objActive = -1; };
	QVector<SelState> selUndo, selRedo;
	int selUndoModelIndex = -2;         //!< model undo index the sel stacks are synced to
	//! Coalesces elementSelectionChanged into one deferred emission per turn
	bool elemSelNotifyPending = false;
	void scheduleElementSelectionNotify();
	//! Snapshot the current selection before a change (call at the top of any
	//! selection mutator so Ctrl+Z can step back through selections)
	void recordSelection();
	bool selectionUndo();               //!< restore the previous selection; false if none
	bool selectionRedo();
	bool hasSelectionUndo();
	bool hasSelectionRedo();

	//! Replace one shape's edit-mode element selection from an external editor
	//! (the UV workspace); other shapes' picks are kept. World positions are
	//! re-derived internally, so callers only fill block/type/indices.
	void setElementSelectionExternal( int shapeBlock, const QVector<PickedElement> & elems, int mode );
	//! Set the element pick mode (1 vertex / 2 edge / 3 face) with notification.
	void setElementPickMode( int mode );

	//! Enter/leave edit mode (only enters on an editable mesh, not particles)
	void setEditMode( bool on );
	//! Is this block a mesh whose vertices we can edit (excludes particle systems)?
	bool isEditableMesh( const QModelIndex & iBlock ) const;
	//! Set the element pick mode (1 vertex, 2 edge, 3 face) and notify the toolbar
	void setPickMode( int m );
	//! Select all geometry connected to the current selection (Ctrl+L).
	//! flatOnly limits growth to faces within maxAngleDeg of each other
	//! (Shift+Ctrl+Alt+F uses ~1 deg; "by angle" prompts for a larger value).
	void selectLinked( bool flatOnly, float maxAngleDeg = 1.5f );
	/*! Hide the selected objects in the viewport (H); Alt+H via unhideAll.
	 *
	 *  THE one hide entry point. The viewport's H, the Block List's H, both
	 *  context menus and the Block List's eye glyph all end up in here or in
	 *  setBlockHidden, so there is a single hidden set (Scene::hiddenNodes) and
	 *  no surface can drift from another. Reads objSelection — which the Block
	 *  List publishes on every selection change — and falls back to the current
	 *  block. */
	void hideSelected();
	//! Reveal everything hidden with H
	void unhideAll();
	//! The nearest NiAVObject at or above a block, or -1: the promotion rule
	//! every hide/see-through path shares
	int hideTargetBlock( int b ) const;
	//! Hide or reveal one block's subtree (the Block List's eye)
	void setBlockHidden( int block, bool hidden );
	//! Draw one block's subtree see-through, or stop (the Block List's disc)
	void setBlockGhosted( int block, bool ghost );
	//! Is this block's own node in the hidden set (not merely inheriting it)?
	bool isBlockHidden( int block ) const;
	//! Is this block's own node in the see-through set?
	bool isBlockGhosted( int block ) const;
	//! Recompute model->dimmedBlocks (hidden nodes + descendants) + repaint list
	void updateDimmedBlocks();
	//! The rendered Shape for a block number, or nullptr
	class Shape * shapeForBlock( int b ) const;
	//! Blender Shift+S snap pie: cursor/selection snapping
	void showSnapMenu();
	//! Snap picked vertices to the grid (Selection to Grid)
	void snapSelectionToGrid();

	// ---- snapping ----
	int snapTargetMode = 0;             // 0 grid step, 1 vertex, 2 edge, 3 face
	int snapBase = 0;                   // Blender Snap Base: 0 closest, 1 center, 2 median, 3 active
	//! Blender-style snap marker: shown at the snapped element while a
	//! transform gesture is element-snapping
	bool snapIndicator = false;
	Vector3 snapIndicatorPos;
	bool snapAlignRot = false;          // orient +Z to the target face normal
	bool snapDefaultOn = false;         // magnet: snap without holding Ctrl (Ctrl inverts)
	int snapAffect = 7;                 // bitmask: 1 move, 2 rotate, 4 scale (Blender "Affect")
	bool orbitSelection = false;        // orbit the view around the selection center
	static float gizmoRotSnapDeg;       // rotation snap increment

private:
	bool gizmoBegin( int mode );
	void gizmoUpdate( const QPoint & pos, Qt::KeyboardModifiers mods );
	void gizmoEnd( bool commit );
	bool gizmoNumActive() const;
	bool gizmoSwallowClick = false;

	AnimationState animState;

	class TexCache * textures;

	QTimer * timer;
	std::chrono::steady_clock::time_point lastTime;
	float time;
	float animSpeed = 1.0f;

	/*! Which way playback is currently running, as a multiplier on animSpeed.
	 *
	 *  Only CYCLE_REVERSE ever flips it: that cycle type ping-pongs, so the
	 *  direction is playback state, not a setting. Kept apart from animSpeed
	 *  because animSpeed belongs to the Speed and Reverse controls, and the next
	 *  time either of those is touched it is rewritten wholesale — a sign flipped
	 *  into it here would be silently undone, and worse, would leave the Reverse
	 *  checkbox disagreeing with the direction on screen.
	 */
	float animDir = 1.0f;

	float Dist;
public:
	//! camera distance, so a test can show a gesture did not move the camera
	float cameraDistance() const { return Dist; }
private:
	Vector3 Pos;
	Vector3 Rot;
	// The explicit Save/Load View commands are session-only. Keeping the
	// snapshot on GLView preserves it while opening another NIF in this window
	// without leaking camera transforms into the next application session.
	bool userViewSaved = false;
	float userViewDist = 128.0f;
	Vector3 userViewPos;
	Vector3 userViewRot;
	GLdouble Zoom;
	GLdouble axis;

	GLdouble aspect;

	std::uint64_t kbdState = 0;
	QPointF lastPos;
	QPointF pressPos;
	Vector3 mouseMov;
	Vector3 mouseRot;
	std::uint32_t mouseButtonState = 0;

	QPersistentModelIndex iDragTarget;
	QString fnDragTex, fnDragTexOrg;

	bool isDisabled = false;
	unsigned char doCompile = 0;
	bool doCenter = false;
	unsigned char updatePending = 0;
	//! Extra repaints scheduled after a scene compile: the first frame after
	//! a compile renders the streaming line geometry (grid / origin axes)
	//! invisibly and the failing draw self-heals, so without these the user
	//! sits on a gridless stale frame until the next input-driven repaint
	unsigned char postCompileRepaints = 0;

	QTimer * lightVisTimer;
	int lightVisTimeout;

	int pixelWidth = 640;
	int pixelHeight = 480;

	QWidget * graphicsView = nullptr;

	enum Key : unsigned char
	{
		Key_CenterView = 1,
		Key_FrontView = 2,
		Key_LeftView = 3,
		Key_MoveBack = 4,
		Key_MoveCam = 5,
		Key_MoveDown = 6,
		Key_MoveForward = 7,
		Key_MoveLeft = 8,
		Key_MoveRight = 9,
		Key_MoveUp = 10,
		Key_Perspective = 11,
		Key_RotateDown = 12,
		Key_RotateLeft = 13,
		Key_RotateRight = 14,
		Key_RotateUp = 15,
		Key_Shift = 16,
		Key_ToggleGrid = 17,
		Key_TopView = 18,
		Key_Update = 19,
		Key_ZoomIn = 20,
		Key_ZoomOut = 21,
		Key_RotateXY = 22,
		Key_RotateZ = 23,
		Key_Scale = 24,
		Key_TranslateXY = 25
	};

	int convertKeyCode( int n, Qt::KeyboardModifiers mods = Qt::NoModifier, bool anyModifiers = false ) const;
	inline bool kbd( int n ) const;
	void transformItem( float dx, float dy );

public:
	struct Settings
	{
		Color4 background;
		float fov = 60.0f;
		float moveSpd = 350.0f;
		float rotSpd = 45.0f;

		UpAxis upAxis = ZAxis;
		ViewState startupDirection = ViewFront;

		static float	vertexPointSize;
		static float	tbnPointSize;
		static float	vertexSelectPointSize;
		static float	vertexPointSizeSelected;
		static float	lineWidthAxes;
		static float	lineWidthWireframe;
		static float	lineWidthHighlight;
		static float	lineWidthGrid;
		static float	lineWidthSelect;
		static float	zoomInScale;
		static float	zoomOutScale;
	} cfg;

	//! Returns the actual dimensions in pixels
	QSize getSizeInPixels() const
	{
		return QSize( pixelWidth, pixelHeight );
	}

	inline void setDisabled( bool n )
	{
		isDisabled = n;
	}

	inline QOpenGLContext * pushGLContext();
	inline void popGLContext( QOpenGLContext * prvContext );
	static const char * getGLErrorString( int err );
	inline TexCache * getTexCache()
	{
		return textures;
	}

private slots:
	void advanceGears();

	void dataChanged( const QModelIndex &, const QModelIndex & );
	void modelChanged();
	void modelLinked();
	void modelDestroyed();

private:
	QStringList draggedNifs;
};

Q_DECLARE_OPERATORS_FOR_FLAGS( GLView::AnimationState )

inline QOpenGLContext * GLView::pushGLContext()
{
	QOpenGLContext *	prvContext = QOpenGLContext::currentContext();
	if ( context() != prvContext )
		makeCurrent();
	return prvContext;
}

inline void GLView::popGLContext( QOpenGLContext * prvContext )
{
	if ( !prvContext )
		doneCurrent();
	else if ( prvContext != context() )
		prvContext->makeCurrent( prvContext->surface() );
}

#endif
