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

#ifndef CONTROLLERS_H
#define CONTROLLERS_H

#include "gl/glcontroller.h" // Inherited
#include "data/niftypes.h"

#include <QPointer>


//! @file controllers.h Controller subclasses

class Shape;
class Node;

/*! How the last sequence selections resolved their Controlled Blocks.
 *
 *  Binding happens inside `ControllerManager::setSequence` and leaves nothing
 *  behind that names the node it chose, so "did the file's palette actually
 *  decide this" is not a question the model or the scene can answer afterwards.
 *  These counters are the answer, and `differs` is the one that matters: it
 *  counts rows where the palette and a `findChild` name search disagree, which
 *  is zero on a file with unique node names and non-zero on exactly the merged
 *  files that used to mis-bind.
 */
namespace SeqBind
{
	struct Stats
	{
		int rows = 0;        //!< Controlled Blocks seen
		int viaPalette = 0;  //!< resolved through NiDefaultAVObjectPalette
		int differs = 0;     //!< palette and the name search picked different nodes
		int unresolved = 0;  //!< neither route found a node
	};

	//! Counters accumulated since the last reset()
	Stats stats();
	//! Zero the counters
	void reset();
}

//! Controller for `NiControllerManager` blocks
class ControllerManager final : public Controller
{
public:
	ControllerManager( Node * node, const QModelIndex & index );

	void updateTime( float ) override final {}

	bool update( const NifModel * nif, const QModelIndex & index ) override final;

	void setSequence( const QString & seqname ) override final;

protected:
	QPointer<Node> target;
};


//! Controller for `NiKeyframeController` blocks
class KeyframeController final : public Controller
{
public:
	KeyframeController( Node * node, const QModelIndex & index );

	void updateTime( float time ) override final;

	bool update( const NifModel * nif, const QModelIndex & index ) override final;

protected:
	QPointer<Node> target;

	QPersistentModelIndex iTranslations, iRotations, iScales;

	int lTrans, lRotate, lScale;
};


//! Controller for `NiTransformController` blocks
class TransformController final : public Controller
{
public:
	TransformController( Node * node, const QModelIndex & index );

	void updateTime( float time ) override final;

	void setInterpolator( const QModelIndex & idx ) override final;

protected:
	QPointer<Node> target;
	QPointer<TransformInterpolator> interpolator;
};


//! Controller for `NiMultiTargetTransformController` blocks
class MultiTargetTransformController final : public Controller
{
	typedef QPair<QPointer<Node>, QPointer<TransformInterpolator> > TransformTarget;

public:
	MultiTargetTransformController( Node * node, const QModelIndex & index );

	void updateTime( float time ) override final;

	bool update( const NifModel * nif, const QModelIndex & index ) override final;

	bool setInterpolatorNode( Node * node, const QModelIndex & idx );

protected:
	QPointer<Node> target;
	QList<TransformTarget> extraTargets;
};


//! Controller for `NiVisController` blocks
class VisibilityController final : public Controller
{
public:
	VisibilityController( Node * node, const QModelIndex & index );

	void updateTime( float time ) override final;

	bool update( const NifModel * nif, const QModelIndex & index ) override final;

protected:
	QPointer<Node> target;

	int visLast;
};


//! Controller for `NiGeomMorpherController` blocks
class MorphController final : public Controller
{
	//! A representation of Mesh geometry morphs
	struct MorphKey
	{
		QPersistentModelIndex iFrames;
		QVector<Vector3> verts;
		int index;
	};

public:
	MorphController( Shape * mesh, const QModelIndex & index );
	~MorphController();

	void updateTime( float time ) override final;

	bool update( const NifModel * nif, const QModelIndex & index ) override final;

protected:
	QPointer<Shape> target;
	QVector<MorphKey *>  morph;
};


//! Controller for `NiUVController` blocks
class UVController final : public Controller
{
public:
	UVController( Shape * mesh, const QModelIndex & index );

	~UVController();

	void updateTime( float time ) override final;

	bool update( const NifModel * nif, const QModelIndex & index ) override final;

protected:
	QPointer<Shape> target;

	int luv = 0;
};


class Particles;

//! Controller for `NiParticleSystemController` and other blocks
class ParticleController final : public Controller
{
	struct Particle
	{
		Vector3 position;
		Vector3 velocity;
		Vector3 unknown;
		float lifetime = 0;
		float lifespan = 0;
		float lasttime = 0;
		short y = 0;
		short vertex = 0;

		Particle()
		{
		}
	};
	QVector<Particle> list;
	struct Gravity
	{
		float force;
		int type;
		Vector3 position;
		Vector3 direction;
	};
	QVector<Gravity> grav;

	QPointer<Particles> target;

	float emitStart = 0, emitStop = 0, emitRate = 0, emitLast = 0, emitAccu = 0, emitMax = 0;
	QPointer<Node> emitNode;
	Vector3 emitRadius;

	float spd = 0, spdRnd = 0;
	float ttl = 0, ttlRnd = 0;

	float inc = 0, incRnd = 0;
	float dec = 0, decRnd = 0;

	float size = 0;
	float grow = 0;
	float fade = 0;

	float localtime = 0;

	QList<QPersistentModelIndex> iExtras;
	QPersistentModelIndex iColorKeys;

public:
	ParticleController( Particles * particles, const QModelIndex & index );

	bool update( const NifModel * nif, const QModelIndex & index ) override final;

	void updateTime( float time ) override final;

	void startParticle( Particle & p );

	void moveParticle( Particle & p, float deltaTime );

	void sizeParticle( Particle & p, float & size );

	void colorParticle( Particle & p, Color4 & color );
};


//! Preview-grade CPU simulator for modern NiPSys particle systems (FO4 era),
//! attached via NiPSysUpdateCtlr. The files store no particles: everything is
//! generated at runtime from the emitter/modifier stack.
class PSysSimController final : public Controller
{
	struct SimParticle
	{
		Vector3 pos;
		Vector3 vel;
		float age = 0;
		float lifespan = 1;
		float radius = 1;
		Color4 color;
		Vector2 uvOff;                     // flipbook cell (subtexture offset)
		float frame = 0;                   // BSPSysSubTexModifier playhead, in frames
		float frameRate = 0;               // its own rate, fudged at birth
		float frameLoop = 0;               // where its loop restarts, likewise
		float angle = 0;                   // sprite rotation (radians)
		float angVel = 0;                  // rotation speed (radians/sec)
	};

	struct Emitter
	{
		QPersistentModelIndex iBlock;
		QString name;
		int shape = 0;                     // 0 point, 1 box, 2 cylinder, 3 sphere, 4 points (mesh/array)
		float dims[3] = { 0, 0, 0 };
		//! Emitter object / mesh block: the Node is resolved lazily at sim time,
		//! because at update() time the scene graph may not be fully built and
		//! the object's world transform would come back wrong
		QPersistentModelIndex iEmitObj;
		QPointer<Node> emitNode;
		QVector<Vector3> points;           // mesh / BSPositionData spawn points (emitter-local space)
		QVector<Triangle> tris;            // mesh emitter triangles (face/edge emission)
		int emitFrom = 0;                  // NiPSysMeshEmitter emission type (0 verts .. 4 edge surface)
		float speed = 0, speedVar = 0;
		float declination = 0, declinationVar = 0;
		float planar = 0, planarVar = 0;
		float lifeSpan = 1, lifeSpanVar = 0;
		float radius = 1, radiusVar = 0;
		Color4 color;
		float birthRate = 0;               // constant fallback
		QPersistentModelIndex iBirthKeys;  // NiFloatData key group of the BirthRate interpolator
		QPersistentModelIndex iVisKeys;    // NiBoolData key group of the EmitterActive interpolator
		float accum = 0;
		int birthIdx = 0, visIdx = 0;
		int ctlrBlock = -1;                // NiPSysEmitterCtlr block number

		// manager-driven rigs: the emitter controller only holds blend
		// interpolators; the real keys live in the controller sequences
		struct SeqKeys
		{
			QString seq;
			QPersistentModelIndex keys;
			float constVal = 0;
			int idx = 0;
		};
		QVector<SeqKeys> seqBirth;
		QVector<SeqKeys> seqVis;

		/*! The `NiPSysModifierFloatCtlr` family, one slot per emitter field they
		 *  can drive. FO4 ships 51 effect meshes with a Speed controller, 21 with
		 *  Initial Radius, 6 each with Life Span and Declination — small numbers,
		 *  but on an effect that uses one, a static Speed is the difference
		 *  between a jet and a puff.
		 */
		enum EmitCurve
		{
			CSpeed, CSpeedVar, CDeclination, CDeclinationVar,
			CPlanar, CPlanarVar, CLifeSpan, CLifeSpanVar, CRadius, CRadiusVar,
			CCount
		};
		QPersistentModelIndex curveKeys[CCount];
		int curveIdx[CCount] = { 0 };
		//! Block number of the controller driving each slot, for the manager case:
		//! its interpolator is then a NiBlendFloatInterpolator stub and the real
		//! keys live in the sequences, exactly as for BirthRate.
		int curveCtlr[CCount] = { 0 };
		QVector<SeqKeys> seqCurve[CCount];
		//! this frame's values: the authored field, or the curve where there is one
		float live[CCount] = { 0 };
	};

	/*! A modifier's Active flag, static or animated.
	 *
	 *  `NiPSysModifierActiveCtlr` is in 288 of FO4's 692 effect meshes — the most
	 *  common thing in this whole area, and it was ignored, so every modifier was
	 *  permanently on. The static `Active` field was not read either.
	 */
	struct ModActive
	{
		struct SeqKeys
		{
			QString seq;
			QPersistentModelIndex keys;
			int idx = 0;
		};

		QString name;
		bool active = true;                //!< the modifier's own Active field
		QPersistentModelIndex keys;        //!< NiBoolData keys of an Active controller
		int idx = 0;
		int ctlrBlock = -1;                //!< the Active controller's block number
		QVector<SeqKeys> seqKeys;          //!< manager rig: the keys live in the sequences
	};

	QPointer<Particles> target;
	QVector<Emitter> emitters;
	QVector<SimParticle> parts;
	int maxParticles = 512;
	float lastTime = -1.0e30f;

	/*! Every modifier's Active state, by the order they appear in the chain, plus
	 *  the name each of the flat members below came from — so an Active
	 *  controller can switch one off without the flat members having to become a
	 *  per-modifier list.
	 */
	QVector<ModActive> modActive;
	QString colorName, scaleName, rotationName;

	//! Present in the modifier chain at all. The engine does not age a particle
	//! without a NiPSysAgeDeathModifier, nor move one without a
	//! NiPSysPositionModifier; both used to be unconditional here, so a system
	//! that omits either previewed as something the game would not draw. Every
	//! FO4 particle system ships both, so this is about files NifSkope edits.
	bool hasAgeDeath = true, hasPosition = true;

	// modifiers

	/*! One NiPSysGravityModifier.
	 *
	 *  A list, not a flat member, because a particle system may carry several and
	 *  the last one used to overwrite the rest. Each keeps its own name so a
	 *  NiPSysModifierActiveCtlr can switch one of them off without taking the
	 *  others with it.
	 */
	struct GravityMod
	{
		QString name;
		Vector3 axis;			//!< world space, gravity object's rotation applied
		Vector3 origin;			//!< world space; the pull point when spherical
		float strength = 0;
		bool spherical = false;	//!< Force Type FORCE_SPHERICAL
		bool hasOrigin = false;
	};
	QVector<GravityMod> gravities;

	/*! One NiPSysDragModifier.
	 *
	 *  Also a list, and for a stronger reason than gravity: FO4 authors drag as
	 *  THREE of these on one system, named (X-Axis), (Y-Axis) and (Z-Axis) at
	 *  consecutive orders. Measured over 140 stock effect meshes, 944 drag
	 *  modifiers, EVERY ONE of them axis-labelled and in triples. Keeping only
	 *  the last kept the Z one and then applied it to all three axes — which is
	 *  harmless while the three percentages agree and wrong the moment they do
	 *  not: AttachFXMist01.nif asks for 0.07 / 0.07 / 0.02 and got 0.02 on all
	 *  three, BubblesSurface01.nif asks for 0.06 / 0.06 / 0.03 and got 0.03.
	 */
	struct DragMod
	{
		QString name;
		Vector3 axis;			//!< world space, drag object's rotation applied
		float percentage = 0;
	};
	QVector<DragMod> drags;
	bool hasColorMod = false;
	float fadeIn = 0.1f, fadeOut = 0.9f;
	float c1End = 0, c2Start = 0, c2End = 1, c3Start = 1;
	Color4 modColors[3];
	// NiPSysColorModifier: a NiColorData RGBA gradient sampled by particle age
	bool hasColorGradient = false;
	QPersistentModelIndex iColorGradKeys;
	QVector<float> scaleKeys;
	// NiPSysRotationModifier
	bool hasRotation = false;
	float rotSpeed = 0, rotSpeedVar = 0, rotAngle = 0, rotAngleVar = 0;
	bool rotRandomSign = false;
	//! NiPSysData subtexture offsets (flipbook cells): (offU, sizeU, offV, sizeV)
	QVector<Vector4> subtexOffsets;

	/*! BSPSysSubTexModifier — "similar to a Flip Controller, this handles
	 *  particle texture animation on a single texture atlas" (nif.xml).
	 *
	 *  Without it a particle keeps ONE random cell of the atlas for its whole
	 *  life, which is what happened to every one of the 302 stock Fallout 4
	 *  meshes carrying this modifier: a 64-cell explosion sheet is authored to
	 *  PLAY, and it showed a single frozen frame of it.
	 *
	 *  Frame Count is read as frames per SECOND. That is an inference, not a
	 *  documented fact — nif.xml describes none of these fields beyond their
	 *  names. The evidence is that its nif.xml default is 30.0 and the authored
	 *  values cluster on 30, 35, 40, 45, 50, 60, 100, 120: conventional frame
	 *  rates, not counts of anything in the file. Nothing here is tied to the
	 *  atlas size either, because the files are not: End Frame is left at its 63
	 *  default on sheets with 4, 8 and 16 cells, so the frame index has to be
	 *  taken modulo the real cell count rather than trusted.
	 */
	bool hasSubTex = false;
	QString subtexName;
	float stStart = 0, stStartFudge = 0;
	float stEnd = 0, stLoopStart = 0, stLoopFudge = 0;
	float stCount = 0, stCountFudge = 0;

	//! The atlas cell a frame index lands on: (offU, offV) of that Subtexture
	//! Offset, taken modulo the real cell count.
	Vector2 subtexCell( float frame ) const;

	QList<QPersistentModelIndex> iExtras;

public:
	PSysSimController( Particles * particles, const QModelIndex & index );

	bool update( const NifModel * nif, const QModelIndex & index ) override final;

	void updateTime( float time ) override final;

protected:
	void emitParticle( Emitter & e );
	Color4 particleColor( const Emitter & e, float u ) const;
	float particleScale( float u ) const;
};


//! Preview renderer for BSProceduralLightningController: the game generates a
//! jagged bolt between the rig's "*_Start" / "*_End" nodes at runtime (the
//! controlled BSTriShape is an empty stub). Generation / Mutation are gated by
//! bool keys in the controller sequences.
class ProcLightningController final : public Controller
{
	struct SeqKeys
	{
		QString seq;
		QPersistentModelIndex keys;
		int idx = 0;
	};
	//! One bolt polyline in the Start->End frame: pts are (t along axis, v, w)
	/*! One branch of the bolt.
	 *
	 *  Every branch runs along the SAME axis — the target's local +Y. The engine
	 *  has no per-branch direction at all: `Lightning::Process` lays each branch's
	 *  rings out along +Y from its own origin, and the forking look comes entirely
	 *  from the displacement. So `pts` is (axial, v, w) in world units, in the one
	 *  frame the whole bolt shares, with this branch's origin already folded in.
	 */
	struct Bolt
	{
		QVector<Vector3> pts;
		int parent = -1;                   // -1 = the trunk, else index into bolts[]
		int gen = 0;                       // 0 = trunk; drives length, width, arc offset
		float length = 0;                  // this branch's own length, world units
	};

	//! One of Interpolators 3-9 (Subdivision..Arc Offset): a float curve that
	//! animates a generation parameter. Absent (link -1) on most assets.
	struct ParamCurve
	{
		QPersistentModelIndex keys;
		int idx = 0;
		bool valid = false;
	};

	QPointer<Node> target;
	bool ribbonReported = false;    //!< WW_BOLT_DEBUG reports the span drawn, once
	QPersistentModelIndex iShaderProp;
	ParamCurve cSubdiv, cBranches, cBranchVar, cLength, cLengthVar, cWidth, cArc;
	//! Same seven parameters when the controller holds NiBlendFloatInterpolator
	//! stubs instead: like Generation/Mutation, the real keys then live in the
	//! sequences' Controlled Blocks, keyed by Interpolator ID.
	enum ParamId { PSubdiv, PBranches, PBranchVar, PLength, PLengthVar, PWidth, PArc, PCount };
	QVector<SeqKeys> paramKeys[PCount];
	//! false when there is no _Start/_End pair and the bolt is emitted along the
	//! target's own axis for Length instead of spanning two nodes
	int subdivisions = 6;
	int numBranches = 1;
	int numBranchesVar = 0;
	float boltLength = 0.0f, boltLengthVar = 0.0f;
	float width = 16.0f, childWidthMult = 0.75f, arcOffset = 20.0f;
	bool fadeMain = true, fadeChild = true, animateArc = true;
	QVector<SeqKeys> genKeys, mutKeys;
	//! generation parameters after Interpolators 3-9 are applied for this frame
	int effSubdiv = 6, effBranches = 1, effBranchVar = 0;
	float effLength = 0.0f, effLengthVar = 0.0f, effArc = 20.0f, effWidth = 16.0f;
	QVector<Bolt> bolts;
	//! last frame's Mutation value, and the key ordinal it came from
	bool lastMutation = false;
	quint32 lastTick = 0xffffffffu;
	//! last frame's displacement ordinal — see updateTime for why it is separate
	quint32 lastOffsetTick = 0xffffffffu;
	/*! How often the bolt re-offsets when `Animate Arc Offset` is set, in Hz.
	 *
	 *  A PREVIEW CHOICE, not an engine constant: the engine re-runs Process once
	 *  per rendered frame and there is no rate anywhere in it. A frame count is
	 *  not a function of time, so it could not be scrubbed; this quantises it at
	 *  the spacing FO4's own effect curves are authored at.
	 */
	static constexpr float kPreviewMutationHz = 30.0f;
	bool visible = false;

	//! Deterministic per-mutation RNG. The global random() made every rebuild
	//! unique, so scrubbing the timeline backwards produced a DIFFERENT bolt and
	//! the render-regression harness could not pixel-compare lightning at all.
	//! Seeded from (block number, mutation index) so a given time reproduces.
	quint32 rngSeed = 0;
	quint32 rngState = 1;

	//! How many keys of the current sequence's curve are at or before `time`.
	/*! The bolt's seed, and the reason it is an ordinal rather than a running
	 *  count of mutations: a count depends on which frames happened to be
	 *  rendered, so scrubbing backwards would return a different bolt. This is a
	 *  pure function of time.
	 */
	quint32 keyOrdinal( const QVector<SeqKeys> & list, float time ) const;

	//! Rebuild every branch. `tick` seeds the structure, `offsetTick` the
	//! displacement alone; see updateTime for why they are separate.
	void regenerate( quint32 tick, quint32 offsetTick );

	//! Build the bolt ribbon for a given viewer axis. See the definition: only
	//! the width expansion is view-dependent, which is what lets a bake reuse it.
	bool buildRibbon( const Vector3 & viewAxis, QVector<Vector3> & tris,
	                  QVector<FloatVector4> & cols, QVector<Vector2> & uvs, Color4 & tintOut );

public:
	ProcLightningController( Node * node, const QModelIndex & index );

	bool update( const NifModel * nif, const QModelIndex & index ) override final;

	void updateTime( float time ) override final;

	//! Draw the preview bolt; called from Node::drawShapes after the children
	void drawPreview();

	//! The current bolt as static world-space triangles, for baking to a mesh.
	/*! `viewAxis` pins the billboard: a still cannot turn to face the camera, so
	 *  the caller decides which way the arc presents its width. */
	bool bakeRibbon( const Vector3 & viewAxis, QVector<Vector3> & tris,
	                 QVector<Vector2> & uvs, Color4 & tint,
	                 QVector<FloatVector4> * colsOut = nullptr )
	{
		QVector<FloatVector4> cols;
		const bool r = buildRibbon( viewAxis, tris, cols, uvs, tint );
		// The per-vertex colours carry the head/tail fade. Dropping them (as the
		// first bake did) flattens every bolt to a uniform bar.
		if ( colsOut )
			*colsOut = cols;
		return r;
	}

	//! The BSEffectShaderProperty the arc draws with, so a bake can reuse it.
	QModelIndex shaderProperty() const { return QModelIndex( iShaderProp ); }
};


class AlphaProperty;
class MaterialProperty;
class TexturingProperty;
class TextureProperty;
class BSEffectShaderProperty;
class BSLightingShaderProperty;

//! Controller for alpha values in a MaterialProperty
class AlphaController final : public Controller
{
public:
	AlphaController( MaterialProperty * prop, const QModelIndex & index );

	AlphaController( AlphaProperty * prop, const QModelIndex & index );

	void updateTime( float time ) override final;

protected:
	QPointer<MaterialProperty> materialProp;
	QPointer<AlphaProperty> alphaProp;

	int lAlpha = 0;
};


//! Controller for color values in a MaterialProperty
class MaterialColorController final : public Controller
{
public:
	MaterialColorController( MaterialProperty * prop, const QModelIndex & index );

	void updateTime( float time ) override final;

	bool update( const NifModel * nif, const QModelIndex & index ) override final;

protected:
	QPointer<MaterialProperty> target; //!< The MaterialProperty being controlled

	int lColor = 0;                        //!< Last interpolation time
	int tColor = tAmbient;                 //!< The color slot being controlled

	//! Color slots that can be controlled
	enum
	{
		tAmbient = 0,
		tDiffuse = 1,
		tSpecular = 2,
		tSelfIllum = 3
	};
};


//! Controller for source textures in a TexturingProperty
class TexFlipController final : public Controller
{
public:
	TexFlipController( TexturingProperty * prop, const QModelIndex & index );

	TexFlipController( TextureProperty * prop, const QModelIndex & index );

	void updateTime( float time ) override final;

	bool update( const NifModel * nif, const QModelIndex & index ) override final;

protected:
	QPointer<TexturingProperty> target;
	QPointer<TextureProperty> oldTarget;

	float flipDelta = 0;
	int flipSlot = 0;

	int flipLast = 0;

	QPersistentModelIndex iSources;
};


//! Controller for transformations in a TexturingProperty
class TexTransController final : public Controller
{
public:
	TexTransController( TexturingProperty * prop, const QModelIndex & index );

	void updateTime( float time ) override final;

	bool update( const NifModel * nif, const QModelIndex & index ) override final;

protected:
	QPointer<TexturingProperty> target;

	int texSlot = 0;
	int texOP = 0;

	int lX = 0;
};

namespace EffectFloat
{
	enum Variable
	{
		Emissive_Multiple = 0,
		Falloff_Start_Angle = 1,
		Falloff_Stop_Angle = 2,
		Falloff_Start_Opacity = 3,
		Falloff_Stop_Opacity = 4,
		Alpha = 5,
		U_Offset = 6,
		U_Scale = 7,
		V_Offset = 8,
		V_Scale = 9,
		U_Offset_F76 = 11,
		U_Scale_F76 = 12,
		V_Offset_F76 = 13,
		V_Scale_F76 = 14
	};
}


//! Controller for float values in a BSEffectShaderProperty
class EffectFloatController final : public Controller
{
public:
	EffectFloatController( BSEffectShaderProperty * prop, const QModelIndex & index );

	void updateTime( float time ) override final;

	bool update( const NifModel * nif, const QModelIndex & index ) override final;

protected:
	QPointer<BSEffectShaderProperty> target;

	EffectFloat::Variable variable = EffectFloat::Emissive_Multiple;
};


//! Controller for color values in a BSEffectShaderProperty
class EffectColorController final : public Controller
{
public:
	EffectColorController( BSEffectShaderProperty * prop, const QModelIndex & index );

	void updateTime( float time ) override final;

	bool update( const NifModel * nif, const QModelIndex & index ) override final;

protected:
	QPointer<BSEffectShaderProperty> target;

	int variable = 0;
};

namespace LightingFloat
{
	enum Variable
	{
		Refraction_Strength = 0,
		Emissive_Multiple_F76 = 3,
		Reflection_Strength = 8,
		Glossiness = 9,
		Specular_Strength = 10,
		Emissive_Multiple = 11,
		Alpha = 12,
		U_Offset = 20,
		U_Scale = 21,
		V_Offset = 22,
		V_Scale = 23
	};
}


//! Controller for float values in a BSEffectShaderProperty
class LightingFloatController final : public Controller
{
public:
	LightingFloatController( BSLightingShaderProperty * prop, const QModelIndex & index );

	void updateTime( float time ) override final;

	bool update( const NifModel * nif, const QModelIndex & index ) override final;

protected:
	QPointer<BSLightingShaderProperty> target;

	LightingFloat::Variable variable = LightingFloat::Refraction_Strength;
};


//! Controller for color values in a BSEffectShaderProperty
class LightingColorController final : public Controller
{
public:
	LightingColorController( BSLightingShaderProperty * prop, const QModelIndex & index );

	void updateTime( float time ) override final;

	bool update( const NifModel * nif, const QModelIndex & index ) override final;

protected:
	QPointer<BSLightingShaderProperty> target;

	int variable = 0;
};


#endif // CONTROLLERS_H
