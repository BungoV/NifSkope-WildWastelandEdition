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

#include "glparticles.h"

#include <cmath>

#include <QRegularExpression>

#include "gl/controllers.h"
#include "gl/glproperty.h"
#include "gl/glscene.h"
#include "gl/renderer.h"
#include "glview.h"
#include "model/nifmodel.h"


/*
 *  Particle
 */

void Particles::clear()
{
	Node::clear();

	verts.clear();
	colors.clear();
	sizes.clear();
	uvOffsets.clear();
	uvCell = Vector2( 1.0f, 1.0f );
	angles.clear();
}

void Particles::updateImpl( const NifModel * nif, const QModelIndex & index )
{
	Node::updateImpl( nif, index );

	if ( index == iBlock ) {
		for (const auto link : nif->getChildLinks(id())) {
			QModelIndex iChild = nif->getBlockIndex(link);

			if (!iChild.isValid())
				continue;

			if (nif->blockInherits(iChild, "NiParticlesData")) {
				iData = iChild;
				updateData = true;
			}
		}

		// FO4 particle systems link the shader/alpha directly on the block
		// rather than through the old NiProperty list, so pick them up here
		iShaderProp = nif->getBlockIndex( nif->getLink( iBlock, "Shader Property" ) );
		iAlphaProp = nif->getBlockIndex( nif->getLink( iBlock, "Alpha Property" ) );
	}

	if ( index == iData )
		updateData = true;
}

void Particles::setController( const NifModel * nif, const QModelIndex & index )
{
	auto contrName = nif->itemName(index);
	if ( contrName == "NiParticleSystemController" || contrName == "NiBSPArrayController" ) {
		Controller * ctrl = new ParticleController( this, index );
		registerController(nif, ctrl);
	} else if ( contrName == "NiPSysUpdateCtlr" ) {
		// modern (FO4 era) particle systems store no vertices in the file;
		// the update controller drives a preview-grade CPU simulation instead
		Controller * ctrl = new PSysSimController( this, index );
		registerController(nif, ctrl);
	} else if ( contrName.startsWith( QLatin1String( "NiPSys" ) ) || contrName.startsWith( QLatin1String( "BSPSys" ) ) ) {
		// emitter/modifier controllers are read by the simulator directly
	} else {
		Node::setController( nif, index );
	}
}

void Particles::transform()
{
	auto nif = NifModel::fromValidIndex(iBlock);
	if ( !nif ) {
		clear();
		return;
	}

	if ( updateData ) {
		updateData = false;

		verts  = nif->getArray<Vector3>( nif->getIndex( iData, "Vertices" ) );
		colors = nif->getArray<Color4>( nif->getIndex( iData, "Vertex Colors" ) );
		sizes  = nif->getArray<float>( nif->getIndex( iData, "Sizes" ) );

		active = nif->get<int>( iData, "Num Valid" );
		size = nif->get<float>( iData, "Active Radius" );
	}

	Node::transform();
}

void Particles::transformShapes()
{
	Node::transformShapes();
}

BoundSphere Particles::bounds() const
{
	BoundSphere sphere( verts );
	sphere.radius += size;
	return worldTrans() * sphere | Node::bounds();
}

void Particles::drawShapes( NodeList * secondPass )
{
	if ( isHidden() || scene->selecting > (unsigned char) Scene::SelObject || !scene->renderer || !scene->nifModel
		|| verts.isEmpty() || active < 1 ) {
		return;
	}

	// display option: hide particle systems entirely (still pickable so the
	// block can be selected in the viewport)
	if ( !scene->showParticles && !scene->selecting )
		return;

	const NifModel * nif = scene->nifModel;

	// FO4 particles are transparent VFX (they carry a shader / alpha property
	// linked directly): draw them in the second (alpha-blended) pass
	if ( ( iAlphaProp.isValid() || iShaderProp.isValid() ) && secondPass ) {
		secondPass->add( this );
		return;
	}

	AlphaProperty * aprop = findProperty<AlphaProperty>();

	if ( aprop && aprop->hasAlphaBlend() && secondPass ) {
		secondPass->add( this );
		return;
	}

	auto	prog = scene->renderer->useProgram( !scene->selecting ? "particles.prog" : "selection.prog" );
	if ( !prog )
		return;

	prog->uni4m( "modelViewMatrix", viewTrans().toMatrix4() );

	if ( scene->selecting ) {
		prog->uni1i( "selectionFlags", 0x0001 );
		prog->uni1i( "selectionParam", scene->nifModel->getBlockNumber( iBlock ) );
		glPointSize( GLView::Settings::vertexSelectPointSize );
		glDisable( GL_BLEND );

	} else {
		float	s2 = size * worldTrans().scale;
		prog->uni2f( "particleScale", s2, s2 );

		// the shader property (added to this node by Node::updateImpl for FO4)
		// resolves and binds the source texture through the normal path
		BSShaderLightingProperty * shaderProp = findProperty<BSShaderLightingProperty>();
		QString	srcTex = shaderProp ? shaderProp->fileName( 0 ) : QString();
		if ( srcTex.isEmpty() && iShaderProp.isValid() ) {
			if ( nif->blockInherits( iShaderProp, "BSEffectShaderProperty" ) )
				srcTex = nif->get<QString>( iShaderProp, "Source Texture" );
		}

		// atlas sheets (e.g. T_..._4x4.dds) pack many frames: show a single cell
		// instead of squashing the whole sheet onto every sprite. When the
		// simulator supplies per-particle cell offsets, only the cell size goes
		// through the uniform and the offsets ride a vertex attribute.
		FloatVector4	puv( 0.0f, 0.0f, 1.0f, 1.0f );
		if ( uvOffsets.size() >= verts.size() && !uvOffsets.isEmpty() ) {
			puv = FloatVector4( 0.0f, 0.0f, uvCell[0], uvCell[1] );
		} else {
			int	grid = 0;
			if ( iData.isValid() && scene->nifModel->getIndex( iData, "Num Subtexture Offsets" ).isValid() ) {
				int nSub = scene->nifModel->get<int>( iData, "Num Subtexture Offsets" );
				if ( nSub >= 4 )
					grid = int( std::lround( std::sqrt( double( nSub ) ) ) );
			}
			if ( grid < 2 ) {
				// fall back to an NxN hint in the texture file name (e.g. _4x4)
				QRegularExpressionMatch mm = QRegularExpression( "(\\d+)x(\\d+)" ).match( srcTex );
				if ( mm.hasMatch() )
					grid = mm.captured( 1 ).toInt();
			}
			if ( grid >= 2 ) {
				float sc = 1.0f / float( grid );
				puv = FloatVector4( 0.0f, 0.0f, sc, sc );
			}
		}
		prog->uni4f( "particleUV", puv );

		// setup blending: honour the linked NiAlphaProperty (FO4 electricity is
		// additive: SRC_ALPHA, ONE), else fall back to additive
		AlphaProperty::glProperty( aprop, prog );
		if ( !( aprop && aprop->hasAlphaBlend() ) ) {
			static const GLenum blendMap[16] = {
				GL_ONE, GL_ZERO, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR,
				GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA,
				GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA_SATURATE, GL_ONE, GL_ONE, GL_ONE, GL_ONE, GL_ONE };
			glEnable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );
			int flags = iAlphaProp.isValid() ? nif->get<int>( iAlphaProp, "Flags" ) : 0;
			if ( iAlphaProp.isValid() && ( flags & 1 ) )
				glBlendFunc( blendMap[( flags >> 1 ) & 0xf], blendMap[( flags >> 5 ) & 0xf] );
			else
				glBlendFunc( GL_SRC_ALPHA, GL_ONE );
		}

		// setup vertex colors

		VertexColorProperty::glProperty( nullptr, FloatVector4( colors.size() < verts.size() ? 1.0f : 0.0f ), prog );

		// setup material

		MaterialProperty::glProperty( findProperty<MaterialProperty>(), findProperty<SpecularProperty>(), prog );
		prog->uni4f( "frontMaterialEmission", FloatVector4( 0.0f ) );

		// setup texturing: the sprite texture always goes on texture unit 0
		// (the previous code bound through the shader property WITHOUT
		// activating a unit first, so the texture landed on whatever unit the
		// last drawn shape left active and the sampler read a stale texture)

		for ( int i = 0; i < TexturingProperty::numTextures; i++ ) {
			prog->uni1i_l( prog->uniLocation( "textureUnits[%d]", i ), 0 );
			prog->uni1i_l( prog->uniLocation( "textures[%d].textureUnit", i ), 0 );
		}

		bool	bound = false;

		if ( auto p = findProperty<TexturingProperty>(); p )
			bound = p->bind( 0, 0, prog );	// legacy path sets its own uniforms

		if ( !bound ) {
			scene->textures->activateTextureUnit( 0 );
			if ( shaderProp )
				bound = shaderProp->bind( 0 );
			if ( !bound && !srcTex.isEmpty() )
				bound = scene->textures->bind( srcTex, nif );
			if ( !bound ) {
				static const QString	defaultTexture = "#FFFFFFFF";
				scene->textures->bind( defaultTexture, scene->nifModel );
			}
			prog->uni1i_l( prog->uniLocation( "textureUnits[%d]", 0 ), 0 );
			prog->uni2f_l( prog->uniLocation( "textures[%d].uvCenter", 0 ), 0.5f, 0.5f );
			prog->uni2f_l( prog->uniLocation( "textures[%d].uvScale", 0 ), 1.0f, 1.0f );
			prog->uni2f_l( prog->uniLocation( "textures[%d].uvOffset", 0 ), 0.0f, 0.0f );
			prog->uni1f_l( prog->uniLocation( "textures[%d].uvRotation", 0 ), 0.0f );
			prog->uni1i_l( prog->uniLocation( "textures[%d].coordSet", 0 ), 0 );
			// the fragment shader samples textureUnits[textureUnit - 1]
			prog->uni1i_l( prog->uniLocation( "textures[%d].textureUnit", 0 ), 1 );
		}
	}

	// setup z buffer

	ZBufferProperty::glProperty( findProperty<ZBufferProperty>() );

	// setup stencil

	StencilProperty::glProperty( findProperty<StencilProperty>() );

	// wireframe ?

#if 0
	WireframeProperty::glProperty( findProperty<WireframeProperty>() );
#endif

	// render the particles

	// FO4 effect shaders multiply the sprite by their (BGEM-aware) emissive
	// base colour - without this, blue/tinted electricity renders white
	QVector<Color4>	tinted;
	if ( !scene->selecting && colors.size() >= verts.size() ) {
		if ( auto esp = dynamic_cast<BSEffectShaderProperty *>( findProperty<BSShaderLightingProperty>() ) ) {
			Color4 tint = esp->emissiveColor;
			float m = std::min( std::max( esp->emissiveMult, 0.0f ), 2.0f );
			tint = Color4( std::min( tint[0] * m, 1.0f ), std::min( tint[1] * m, 1.0f ),
			               std::min( tint[2] * m, 1.0f ), tint[3] );
			if ( tint[0] + tint[1] + tint[2] > 0.02f
				&& !( tint[0] == 1.0f && tint[1] == 1.0f && tint[2] == 1.0f && tint[3] == 1.0f ) ) {
				tinted.resize( colors.size() );
				for ( qsizetype i = 0; i < colors.size(); i++ ) {
					const Color4 & c = colors.at( i );
					tinted[i] = Color4( c[0] * tint[0], c[1] * tint[1], c[2] * tint[2], c[3] * tint[3] );
				}
			}
		}
	}

	qsizetype	numVerts = verts.size();
	const float *	attrData[5] = { &( verts.constFirst()[0] ), nullptr, nullptr, nullptr, nullptr };
	unsigned int	attrMask = 0x03;
	if ( colors.size() >= numVerts ) {
		attrData[1] = &( ( tinted.size() >= numVerts ? tinted : colors ).constFirst()[0] );
		attrMask = attrMask | 0x40;
	}
	if ( uvOffsets.size() >= numVerts ) {
		// per-particle flipbook cell offset (vec2 at location 2)
		attrData[2] = &( uvOffsets.constFirst()[0] );
		attrMask = attrMask | 0x200;
	}
	if ( angles.size() >= numVerts ) {
		// per-particle sprite rotation angle (float at location 3)
		attrData[3] = angles.constData();
		attrMask = attrMask | 0x1000;
	}
	if ( sizes.size() >= numVerts ) {
		attrData[4] = sizes.constData();
		attrMask = attrMask | 0x00010000;
	}
	scene->renderer->bindShape( (unsigned int) numVerts, attrMask, 0, attrData, nullptr );

	if ( active < numVerts )
		numVerts = active;

	// transparent sprites must not write depth: they would punch square
	// holes into transparent geometry drawn after them
	if ( !scene->selecting )
		glDepthMask( GL_FALSE );
	scene->renderer->fn->glDrawArrays( GL_POINTS, 0, GLsizei( numVerts ) );
	if ( !scene->selecting )
		glDepthMask( GL_TRUE );
}
