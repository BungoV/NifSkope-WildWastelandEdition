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

#ifndef GLSHAPE_H
#define GLSHAPE_H

#include "gl/glnode.h" // Inherited
#include "gl/gltools.h"
#include "gl/glcontext.hpp"

#include <QPersistentModelIndex>
#include <QSharedPointer>
#include <QVector>
#include <QString>

//! @file glshape.h Shape

class NifModel;

class Shape : public Node
{
	friend class MorphController;
	friend class UVController;
	friend class Renderer;
	friend class GltfStore;

public:
	Shape( Scene * s, const QModelIndex & b );

	// IControllable

	void clear() override;
	void transform() override;

	// end IControllable

	void updateBoneTransforms();
	void convertTriangleStrip( const void * indicesData, size_t numIndices );
	void removeInvalidIndices();
	void drawVerts( float pointSize, int vertexSelected ) const;
	// btnMask & 1 = draw bitangents, btnMask & 2 = draw tangents, btnMask & 4 = draw normals
	void drawNormals( int btnMask = 4, int vertexSelected = -1, float lineLength = 0.25f ) const;
	void drawWireframe( FloatVector4 color, float lineWidth = -1.0f ) const;
	// i = first triangle, n = number of triangles to draw
	void drawTriangles( qsizetype i, qsizetype n, FloatVector4 color ) const;
	void drawWeights( int vertexSelected ) const;
	void drawBoundingSphere( const BoundSphere & sph, FloatVector4 color ) const;
	void drawBoundingBox( const Vector3 & boundsCenter, const Vector3 & boundsDims, FloatVector4 color ) const;
	void setUniforms( NifSkopeOpenGLContext::Program * prog ) const;
	bool bindShape() const;
	// lane GPUDRIVEN1: the content hash bindShape keys the geometry cache with, kept until the shape changes; no
	// upload, nullptr without geometry. vertexAttrs (16 entries) receives the attribute table when given.
	const NifSkopeOpenGLContext::ShapeDataHash * wwDataHash( const float ** vertexAttrs = nullptr ) const;
	std::uint64_t wwVertexAttrs( const float ** vertexAttrs ) const;
	//! Evaluate/invert the same blended vertex skin transform used by the GPU.
	//! False from unskinVertex means the weighted matrix was singular.
	Vector3 skinVertex( int vertexIndex, const Vector3 & local ) const;
	bool unskinVertex( int vertexIndex, const Vector3 & skinnedLocal, Vector3 & local ) const;

	virtual QModelIndex vertexAt( int ) const { return QModelIndex(); }
	virtual QModelIndex triangleAt( int ) const { return QModelIndex(); }
	virtual void updateLodLevel() { lodTriangleCount = triangles.size(); }

protected:
	int shapeNumber;

	void setController( const NifModel * nif, const QModelIndex & controller ) override;
	void updateImpl( const NifModel * nif, const QModelIndex & index ) override;
	virtual void updateData( const NifModel* nif ) = 0;

	void boneSphere( const NifModel * nif, const QModelIndex & index ) const;

	//! Shape data
	QPersistentModelIndex iData;
	//! Tangent data
	QPersistentModelIndex iTangentData;
	//! Does the data need updating?
	bool needUpdateData = false;

	void resetSkinning();

public:
	//! Skin instance
	QPersistentModelIndex iSkin;
	//! Skin data
	QPersistentModelIndex iSkinData;
	//! Skin partition
	QPersistentModelIndex iSkinPart;

	//! Vertices
	QVector<Vector3> verts;
	//! Normals
	QVector<Vector3> norms;
	//! Vertex colors
	QVector<Color4> colors;
	//! Tangents
	QVector<Vector3> tangents;
	//! Bitangents
	QVector<Vector3> bitangents;
	//! UV coordinate sets
	QVector<TexCoords> coords;
	//! Triangles
	QVector<Triangle> triangles;

	//! How many bones drive this shape; 0 means it is not skinned.
	/*! Declared inside this EXISTING public run on purpose — a fresh public: in
	 *  this header flips the access of everything below it. */
	int boneCount() const { return int( boneData.size() ); }
	//! The name of bone `i` as its own file records it, or empty.
	QString boneNameAt( int i ) const;
	//! lane CSM1: an opaque, depth-writing, non-effect shape goes into the sun's shadow map
	bool wwCastsSunShadow() const { return !drawInSecondPass && !translucent && depthWrite && !bsesp; }
	bool wwDoubleSided() const { return isDoubleSided; }
	//! lane SHADOWSHIMMER3 (FARSWAYLEGACY): the tree flag every pass sways by (TREECROWN1's FarLOD Tree_Anim rule included)
	bool wwTreeAnim() const { return isVertexAlphaAnimation; }
	//! lane FXD1: with Cell lights on, a second-pass decal of the lighting shader draws before the blended shapes
	bool wwDecalDrawsFirst() const;
	//! lane EFFECTPARITY1: an alpha-tested, unblended material clears the NIF's blend bit (game rule)
	bool wwMaterialClearsBlend() const;
	//! lane WATER1: a cell water surface; with Cell lights on it draws after the decals, before the blended shapes
	bool wwWaterDraws() const;
	//! lane FXRES1: a Soft effect, blended in the sorted pass, writing no depth, not a decal, not lit by its spot:
	//! the low-resolution layer may take it (src/gl/cellfxres.h)
	bool wwFxResSoft() const;
	Scene * wwFxResScene() const { return scene; }
	//! lane PERF2FIX1: did this shape's last draw write in an opaque-only pass (cell program, no blend), and when
	bool wwCutWrote = true;
	unsigned wwCutStamp = 0;
	//! lane SHADOW1: the shape alpha-tests (its depth-only caster would cast a solid card)
	bool wwAlphaTested() const;
	//! lane VOLAIR1: what the alpha-tested sun caster reads (the base map's property, the material alpha, the threshold)
	BSShaderLightingProperty * wwShaderLighting() const { return bssp; }
	const BSLightingShaderProperty * wwLightingShader() const { return bslsp; }
	float wwAlphaThreshold() const;
	/*! lane ALPHABGSM1: the alpha test the colour draw runs with (Renderer::setupProgramCE1): a material file's test
	 *  and threshold whenever the shader property has one, the NiAlphaProperty's only without. The side passes read
	 *  this: `pass` is "sun", "cube", "refl" or "taa", for the red WW_ALPHABGSM_RED (nif = every pass, or a comma
	 *  list of passes: they read the NIF's test again). */
	struct WwDrawAlpha
	{
		bool test = false;
		float threshold = 0.0f;
		bool fromMaterial = false;	// the material file decided (it may say "no test" where the NIF tests)
	};
	WwDrawAlpha wwDrawAlpha( const char * pass ) const;
	//! lane ALPHABGSM1: moves when WW_ALPHABGSM_RED does, so the passes that keep their maps draw them again
	static int wwDrawAlphaGen();
	/*! lane TREELEAF2: the ref the colour pass binds this shape's base map at (the TexCache "<file>|wwcov<ref>" copy,
	 *  src/gl/wwleafcov.h colourRef; 0 = the plain file). `pass` is "colour", "sun", "cube", "taa" or "refl", for the red
	 *  WW_TREELEAF_RED (1: every pass, side or a comma list: those side passes bind the plain file again). */
	int wwLeafCovRef( const char * pass ) const;
	//! lane TREELEAF2: arms the scene's TexCache with wwLeafCovRef( pass ) for the next base-map bind; disarm after it
	void wwLeafCovArm( const char * pass ) const;
	void wwLeafCovDisarm() const;
	/*! lane TREELEAF2: the threshold a side pass uploads for its `discard a < t` test (da = wwDrawAlpha of that pass), so
	 *  it keeps the colour pass's texels: the next float above ref / 255 where the colour test is a strict `>` */
	float wwLeafSideThreshold( const WwDrawAlpha & da, const char * pass ) const;
	/*! lane TREELEAF3: what a side pass's `discard a < t` test reads (src/gl/wwfxcov.h Side): fxMode, the alpha factor
	 *  and the threshold. A lighting shape: today's (mode 0, its material alpha, wwLeafSideThreshold); an alpha-tested
	 *  effect shape (fo4_effectshader): the colour pass's composed alpha (baseColor.a^2, the vertex alpha, the palette's
	 *  alpha) and its strict `>`. Red WW_TREELEAF_RED=fx (or 1 / side): the raw base alpha, `>=`. */
	struct WwFxAlpha
	{
		int mode = 0;	// 0 today's test; 4 + 1 the vertex alpha + 2 the palette's alpha
		float mult = 1.0f;
		float threshold = 0.0f;
		/* lane TREELEAF4 (FXCOVRESID1): mode + 8 the falloff (falloffParams: start / stop angle, start / stop opacity),
		 * + 16 the cell's soft fade (softDepth, game units), as fo4_effectshader.frag composes them. Red
		 * WW_FXCOVRESID_RED=1: TREELEAF3's rule (neither). volPart: a volumetric lit card (cell_volpart.glsl), whose
		 * colour alpha is a puff the side pass does not reproduce (neither factor, no coverage rule). */
		float falloff[4] = { 1.0f, 0.0f, 1.0f, 1.0f };
		float softDepth = 1.0f;
		bool volPart = false;
	};
	WwFxAlpha wwFxSideAlpha( const WwDrawAlpha & da, const char * pass ) const;
	/*! lane TREELEAF4 (TAABLENDMOVER1): the colour draw blends this shape (Renderer::setupProgramCE1's GL_BLEND: a
	 *  readable material's alpha blend, else a translucent shape, else the NiAlphaProperty's blend; never with the
	 *  Blending option off). The TAA object vectors draw such a mover by its coverage (taainputs.cpp). */
	bool wwBlendedAsDrawn() const;
	//! the GPU applies the bone transforms to this shape (the caster pass skins it on the CPU)
	bool wwGpuSkinned() const { return isSkinned && !transformRigid && !boneTransforms.empty(); }
	//! lane FARDRAW1: the instanced far ring this shape draws, or null
	const struct CellMeshInst * wwFarInstData() const { return wwFarInst.data(); }
protected:
	//! Number of triangles to render at the current level of detail
	qsizetype lodTriangleCount = 0;
	//! Offsets and lengths of converted triangle strips in triangles
	QVector< std::pair<qsizetype, qsizetype> > tristripOffsets;

	bool isLOD = false;

	void resetVertexData();

	//! Toggle for skinning
	bool isSkinned = false;
	//! Is the transform rigid or weighted?
	bool transformRigid = true;

	//! Bone transforms as 4x3 matrices in row-major order
	std::vector<FloatVector4> boneTransforms;
	//! Bone weights 0 to 3 (integer part = bone index, fractional part = weight * 65535.0 / 65536.0), terminated by 0.0
	std::vector<FloatVector4> boneWeights0;
	//! Bone weights 4 to 7 (may be empty if the maximum number of weights per vertex is 4 or less)
	std::vector<FloatVector4> boneWeights1;

	int skeletonRoot = 0;
	QVector<int> bones;
	QVector<BoneData> boneData;
	QVector<SkinPartition> partitions;

	void resetSkeletonData();

	//! Holds the shader program used by this shape
	NifSkopeOpenGLContext::Program * shader = nullptr;
	//! lane FARDRAW1: set = an instanced far ring shape (cellmesh.h CellMeshInst): no vertex arrays, drawn by gl/cellfarinst
	QSharedPointer<struct CellMeshInst> wwFarInst;

	//! Shader property
	BSShaderLightingProperty * bssp = nullptr;
	//! Skyrim shader property
	BSLightingShaderProperty * bslsp = nullptr;
	//! Skyrim effect shader property
	BSEffectShaderProperty * bsesp = nullptr;

	AlphaProperty * alphaProperty = nullptr;

	//! Is shader set to double sided?
	bool isDoubleSided = false;
	//! Is shader set to animate using vertex alphas?
	bool isVertexAlphaAnimation = false;
	//! Is "Has Vertex Colors" set to Yes
	bool hasVertexColors = false;

	bool depthTest = true;
	bool depthWrite = true;
	bool drawInSecondPass = false;
	bool translucent = false;
	bool surfaceDecal = false;	// lane FXD1
	bool effectDecal = false;	// lane DECALLIT1: an effect shader with the Decal flag (the game draws it into the surface)

	void updateShader();

	mutable bool needUpdateBounds = false;
	mutable BoundSphere boundSphere;

	mutable NifSkopeOpenGLContext::ShapeDataHash	dataHash;

public:
	inline void clearHash()
	{
		dataHash.attrMask = 0;
	}
	//! lane FARHOLE2: the far mask changed (cellfarhide.h); the next transform takes the arrays again
	inline void wwRefreshData()
	{
		needUpdateData = true;
	}
};

class Material;
/*! lane TREELEAF2 (TREELEAF5's dial): WW_LODL_TREE_ALPHA (1..255) when the material's first texture lives under a trees/
 *  folder, else 0. The colour pass tests such a material at the dial (renderer.cpp), so its coverage copy and the side
 *  passes' threshold take the dial too. */
int wwLodlTreeDial( const Material * mat );

#endif
