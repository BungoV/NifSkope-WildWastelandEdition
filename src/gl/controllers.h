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
	};

	QPointer<Particles> target;
	QVector<Emitter> emitters;
	QVector<SimParticle> parts;
	int maxParticles = 512;
	float lastTime = -1.0e30f;

	// modifiers
	bool hasGravity = false;
	Vector3 gravityDir;
	float gravityStrength = 0;
	float dragPct = 0;
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
	struct Bolt
	{
		QVector<Vector3> pts;
		int parent = -1;                   // -1 = main bolt, else index into bolts[]
		float rootT = 0;                   // start param along the PARENT bolt
		Vector3 dir;                       // direction in the PARENT's frame
		float lenMul = 1;                  // fraction of the parent's length
		float widthMul = 1;
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
	QPointer<Node> startNode;
	QPointer<Node> endNode;
	QPersistentModelIndex iShaderProp;
	ParamCurve cSubdiv, cBranches, cBranchVar, cLength, cLengthVar, cWidth, cArc;
	//! Same seven parameters when the controller holds NiBlendFloatInterpolator
	//! stubs instead: like Generation/Mutation, the real keys then live in the
	//! sequences' Controlled Blocks, keyed by Interpolator ID.
	enum ParamId { PSubdiv, PBranches, PBranchVar, PLength, PLengthVar, PWidth, PArc, PCount };
	QVector<SeqKeys> paramKeys[PCount];
	//! false when there is no _Start/_End pair and the bolt is emitted along the
	//! target's own axis for Length instead of spanning two nodes
	bool spanNodes = false;
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
	float lastMutation = -1.0e30f;
	bool visible = false;
	bool nodesResolved = false;

	//! Deterministic per-mutation RNG. The global random() made every rebuild
	//! unique, so scrubbing the timeline backwards produced a DIFFERENT bolt and
	//! the render-regression harness could not pixel-compare lightning at all.
	//! Seeded from (block number, mutation index) so a given time reproduces.
	quint32 rngSeed = 0;
	quint32 rngState = 1;

	//! Seeded from the QUANTISED TIME, not a mutation counter: a counter makes
	//! the bolt depend on how many times regenerate() happened to run before the
	//! frame was observed, which varies with frame timing and broke
	//! reproducibility. Keyed on time, the same instant always redraws the same
	//! bolt no matter how it was reached.
	void regenerate( float time );

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
	                 QVector<Vector2> & uvs, Color4 & tint )
	{
		QVector<FloatVector4> cols;
		return buildRibbon( viewAxis, tris, cols, uvs, tint );
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
