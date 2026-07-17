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

#include "glscene.h"

#include "gl/renderer.h"
#include "gl/gltex.h"
#include "gl/glcontroller.h"
#include "gl/controllers.h"
#include "gl/glmesh.h"
#include "gl/bsshape.h"
#include "gl/BSMesh.h"
#include "gl/glparticles.h"
#include "gl/gltex.h"
#include "model/nifmodel.h"
#include "glview.h"
#include "nifskope.h"

#include <QAction>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QCoreApplication>
#include <QFile>
#include <QSettings>
#include <QTextStream>


//! \file glscene.cpp %Scene management

Scene::Scene( TexCache * texcache, QObject * parent ) :
	QObject( parent )
{
	currentBlock = currentIndex = QModelIndex();
	selecting = 0;
	animate = true;
	defaultSkeletonRoot = -1;

	time = 0.0;
	sceneBoundsValid = timeBoundsValid = false;

	textures = texcache;

	options = ( DoLighting | DoTexturing | DoMultisampling | DoBlending | DoVertexColors | DoSpecular | DoGlow
				| DoCubeMapping | DoDiffuse | DoNormalMap | DoVertexAlpha | DoParallax | DoMaterialTint
				| DoDetailTextures | DoGloss );

	lodLevel = Level0;

	visMode = VisNone;

	selMode = SelObject;

	// Startup Defaults

	QSettings settings;
	settings.beginGroup( "Settings/Render/General/Startup Defaults" );

	if ( settings.value( "Show Axes", true ).toBool() )
		options |= ShowAxes;
	if ( settings.value( "Show Grid", true ).toBool() )
		options |= ShowGrid;
	if ( settings.value( "Show Collision" ).toBool() )
		options |= ShowCollision;
	if ( settings.value( "Show Constraints" ).toBool() )
		options |= ShowConstraints;
	if ( settings.value( "Show Markers" ).toBool() )
		options |= ShowMarkers;
	if ( settings.value( "Show Nodes" ).toBool() )
		options |= ShowNodes;
	if ( settings.value( "Show Hidden" ).toBool() )
		options |= ShowHidden;
	if ( settings.value( "Do Skinning", true ).toBool() )
		options |= DoSkinning;
	if ( settings.value( "Do Error Color", true ).toBool() )
		options |= DoErrorColor;

	settings.endGroup();

	currentGLColor = FloatVector4( 0.0f );
	currentGLLineWidth = 1.0f;
	currentGLPointSize = 1.0f;
	currentModelViewMatrix = modelViewMatrixStack;

	updateSettings( settings );
}

Scene::~Scene()
{
	if ( renderer )
		delete renderer;
}

void Scene::setOpenGLContext( QOpenGLContext * context )
{
	if ( renderer || !context )
		return;
	renderer = new Renderer( context );
}

void Scene::updateShaders()
{
	renderer->updateShaders();
}

void Scene::updateSettings( QSettings & settings )
{
	settings.beginGroup( "Settings/Render/Colors/" );

	// Blender-matched grid brightness. The grid draws with FRAMEBUFFER_SRGB
	// off, so the old default (99,99,99 @ 0.8) displayed much dimmer than
	// Blender's; values saved with that old default migrate to the new one.
	QColor gridQC = settings.value( "Grid Color", QColor( 150, 150, 150, 235 ) ).value<QColor>();
	if ( gridQC == QColor( 99, 99, 99, 204 ) )
		gridQC = QColor( 150, 150, 150, 235 );
	gridColor = FloatVector4( Color4( gridQC ) );
	highlightColor = FloatVector4( Color4( settings.value( "Highlight", QColor( 255, 255, 0 ) ).value<QColor>() ) );
	wireframeColor = FloatVector4( Color4( settings.value( "Wireframe", QColor( 0, 255, 0 ) ).value<QColor>() ) );

	settings.endGroup();

	int	tmp = settings.value( "Settings/Render/General/Default Skeleton Root", -1 ).toInt();
	defaultSkeletonRoot = std::int16_t( std::clamp< int >( tmp, -1, 32767 ) );

	tmp = settings.value( "Settings/Render/General/Bound Sphere Quality", 4 ).toInt();
	boundSphereQuality = (unsigned char) std::clamp< int >( tmp, 2, 42 );
}

void Scene::clear( [[maybe_unused]] bool flushTextures )
{
	nodes.clear();
	properties.clear();
	roots.clear();
	shapes.clear();

	animGroups.clear();
	animTags.clear();

	//if ( flushTextures )
	textures->flush();
	if ( renderer )
		renderer->flushCache();

	sceneBoundsValid = timeBoundsValid = false;

	nifModel = nullptr;
}

void Scene::update( const NifModel * nif, const QModelIndex & index )
{
	if ( !nif )
		return;

	nifModel = nif;

	if ( index.isValid() ) {
		QModelIndex block = nif->getBlockIndex( index );
		if ( !block.isValid() )
			return;

		for ( Property * prop : properties )
			prop->update( nif, block );

		for ( Node * node : nodes.list() )
			node->update( nif, block );
	} else {
		properties.validate();
		nodes.validate();

		for ( Property * p : properties )
			p->update( nif, p->index() );

		for ( Node * n : nodes.list() )
			n->update( nif, n->index() );

		roots.clear();
		for ( const auto link : nif->getRootLinks() ) {
			QModelIndex iBlock = nif->getBlockIndex( link );
			if ( iBlock.isValid() ) {
				Node * node = getNode( nif, iBlock );
				if ( node ) {
					node->makeParent( 0 );
					roots.add( node );
				}
			}
		}
	}

	timeBoundsValid = false;
}

void Scene::updateSceneOptions( bool checked )
{
	Q_UNUSED( checked );

	QAction * action = qobject_cast<QAction *>(sender());
	if ( action ) {
		options ^= SceneOptions( action->data().toInt() );
		emit sceneUpdated();
	}
}

void Scene::updateSceneOptionsGroup( QAction * action )
{
	if ( !action )
		return;

	options ^= SceneOptions( action->data().toInt() );
	emit sceneUpdated();
}

void Scene::updateSelectMode( QAction * action )
{
	if ( !action )
		return;

	selMode = SelMode( action->data().toInt() );
	emit sceneUpdated();
}

void Scene::updateLodLevel( int level )
{
	if ( Game::GameManager::get_game( nifModel ) != Game::STARFIELD )
		level = std::min( level, 2 );
	lodLevel = LodLevel( level );

	for ( Shape * s : shapes )
		s->updateLodLevel();
}

void Scene::make( NifModel * nif, bool flushTextures )
{
	clear( flushTextures );

	if ( !nif )
		return;

	update( nif, QModelIndex() );

	if ( !animGroups.contains( animGroup ) ) {
		if ( animGroups.isEmpty() )
			animGroup = QString();
		else
			animGroup = animGroups.first();
	}

	setSequence( animGroup );
}

Node * Scene::getNode( const NifModel * nif, const QModelIndex & iNode )
{
	if ( !nif || !iNode.isValid() )
		return nullptr;

	Node * node = nodes.get( iNode );

	if ( node )
		return node;

	auto nodeName = nif->itemName(iNode);
	if ( nif->blockInherits( iNode, "NiNode" ) ) {
		if ( nodeName == "NiLODNode" )
			node = new LODNode( this, iNode );
		else if ( nodeName == "NiBillboardNode" )
			node = new BillboardNode( this, iNode );
		else
			node = new Node( this, iNode );
	} else if ( nodeName == "NiTriShape" || nodeName == "NiTriStrips" || nif->blockInherits( iNode, "NiTriBasedGeom" ) ) {
		node = new Mesh( this, iNode );
		shapes += static_cast<Shape *>(node);
	} else if ( nif->checkVersion( 0x14050000, 0 ) && nodeName == "NiMesh" ) {
		node = new Mesh( this, iNode );
	}
	//else if ( nif->blockInherits( iNode, "AParticleNode" ) || nif->blockInherits( iNode, "AParticleSystem" ) )
	else if ( nif->blockInherits( iNode, "NiParticles" ) ) {
		// ... where did AParticleSystem go?
		node = new Particles( this, iNode );
	} else if ( nif->blockInherits( iNode, "BSTriShape" ) ) {
		node = new BSShape( this, iNode );
		shapes += static_cast<Shape *>(node);
	} else if ( nif->blockInherits(iNode, "BSGeometry") ) {
		node = new BSMesh(this, iNode);
		shapes += static_cast<Shape*>(node);
	} else if ( nif->blockInherits( iNode, "NiAVObject" ) ) {
		if ( nodeName == "BSTreeNode" )
			node = new Node( this, iNode );
	}

	if ( node ) {
		nodes.add( node );
		node->update( nif, iNode );
	}

	return node;
}

Property * Scene::getProperty( const NifModel * nif, const QModelIndex & iProperty )
{
	Property * prop = properties.get( iProperty );
	if ( prop )
		return prop;

	prop = Property::create( this, nif, iProperty );
	if ( prop )
		properties.add( prop );
	return prop;
}

Property * Scene::getProperty( const NifModel * nif, const QModelIndex & iParentBlock, const QString & itemName, const QString & mustInherit )
{
	QModelIndex iPropertyBlock = nif->getBlockIndex( nif->getLink(iParentBlock, itemName) );
	if ( iPropertyBlock.isValid() && nif->blockInherits(iPropertyBlock, mustInherit) )
		return getProperty( nif, iPropertyBlock );
	return nullptr;
}

void Scene::setSequence( const QString & seqname )
{
	animGroup = seqname;

	for ( Node * node : nodes.list() ) {
		node->setSequence( seqname );
	}
	for ( Property * prop : properties ) {
		prop->setSequence( seqname );
	}

	timeBoundsValid = false;
}

void Scene::transform( const Transform & trans, float time )
{
	view = trans;
	this->time = time;

	transformCache.clear();

	for ( Property * prop : properties ) {
		prop->transform();
	}
	for ( Node * node : roots.list() ) {
		node->transform();
	}
	for ( Node * node : roots.list() ) {
		node->transformShapes();
	}

	sceneBoundsValid = false;

	// TODO: purge unused textures
}

void Scene::draw()
{
	QSettings collisionSettings;
	const bool collisionOnly = collisionSettings.value( "CollisionManager/CollisionOnly", false ).toBool()
		&& hasOption( ShowCollision );
	if ( collisionOnly )
		drawGrid();
	else
		drawShapes();

	glDisable( GL_CULL_FACE );
	glDisable( GL_STENCIL_TEST );
	glDisable( GL_FRAMEBUFFER_SRGB );

	if ( hasOption(ShowNodes) )
		drawNodes();
	if ( hasOption(ShowCollision) )
		drawHavok();
	if ( hasOption(ShowMarkers) )
		drawFurn();

	drawSelection();
}

bool Scene::grabRefractionSource()
{
	if ( !renderer )
		return false;
	auto fn = renderer->fn;

	GLint vp[4];
	glGetIntegerv( GL_VIEWPORT, vp );
	int w = vp[2], h = vp[3];
	if ( w < 1 || h < 1 )
		return false;

	GLint prevRead = 0, prevDraw = 0, prevActive = 0, prevTex = 0;
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );

	if ( !refractionTexId || refractionTexW != w || refractionTexH != h ) {
		if ( !refractionTexId )
			fn->glGenTextures( 1, &refractionTexId );
		fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &prevTex );
		fn->glBindTexture( GL_TEXTURE_2D, refractionTexId );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glBindTexture( GL_TEXTURE_2D, GLuint( prevTex ) );
		if ( !refractionFbo )
			fn->glGenFramebuffers( 1, &refractionFbo );
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, refractionFbo );
		fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, refractionTexId, 0 );
		refractionTexW = w;
		refractionTexH = h;
	} else {
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, refractionFbo );
	}

	// resolve-blit works from both multisampled and plain framebuffers
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBlitFramebuffer( vp[0], vp[1], vp[0] + w, vp[1] + h, 0, 0, w, h,
	                       GL_COLOR_BUFFER_BIT, GL_NEAREST );

	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glActiveTexture( GLenum( prevActive ) );
	return true;
}

void Scene::drawShapes()
{
	pendingBolts.clear();

	if ( hasOption(DoBlending) ) {
		NodeList secondPass;

		for ( Node * node : roots.list() ) {
			node->drawShapes( &secondPass );
		}

		drawGrid();

		if ( secondPass.list().count() > 0 )
			drawSelection(); // for transparency pass

		secondPass.alphaSort();

		// particle systems are additive VFX: draw them after the other
		// transparent shapes, so standard alpha blending cannot darken
		// rectangular patches over the already-drawn sprites
		QVector<Node *> deferredParticles;
		for ( Node * node : secondPass.list() ) {
			if ( dynamic_cast<Particles *>( node ) )
				deferredParticles.append( node );
			else
				node->drawShapes();
		}
		for ( Node * node : deferredParticles )
			node->drawShapes();
	} else {
		for ( Node * node : roots.list() ) {
			node->drawShapes();
		}

		drawGrid();
	}

	// additive procedural lightning goes last, over the transparent geometry
	for ( ProcLightningController * pl : pendingBolts )
		pl->drawPreview();
	pendingBolts.clear();
}

void Scene::drawGrid()
{
	// Draw the grid
	NifSkope *	w;
	// TEMP DIAGNOSTIC (WW_PERF_TEST): why does the grid skip drawing?
	if ( qEnvironmentVariableIsSet( "WW_PERF_TEST" ) ) {
		QFile f( QCoreApplication::applicationDirPath() + "/ww_perf_test.log" );
		if ( f.open( QIODevice::Append | QIODevice::Text ) )
			QTextStream( &f ) << "    [drawGrid: opts=0x" << QString::number( options, 16 )
				<< " showGrid=" << hasOption( ShowGrid )
				<< " model=" << ( nifModel != nullptr )
				<< " win=" << ( nifModel && dynamic_cast< NifSkope * >( nifModel->getWindow() ) != nullptr )
				<< "]\n";
	}
	if ( !hasOption(ShowGrid) || !nifModel || ( w = dynamic_cast< NifSkope * >( nifModel->getWindow() ) ) == nullptr )
		return;
	GLView *	v = w->getGLView();
	if ( !v )
		return;

	glDisable( GL_FRAMEBUFFER_SRGB );
	glEnable( GL_DEPTH_TEST );
	glDepthMask( GL_TRUE );
	glDepthFunc( GL_LESS );

	FloatVector4	c0 = gridColor;
	FloatVector4	cX( 0.85f, 0.16f, 0.16f, 1.0f );
	FloatVector4	cY( 0.20f, 0.65f, 0.24f, 1.0f );
	FloatVector4	cZ( 0.20f, 0.38f, 0.85f, 1.0f );

	// Blender-style orthographic grid: align it to the current screen plane,
	// adapt spacing to zoom, anchor subdivisions to world zero, and place it
	// behind the scene so geometry cleanly occludes it instead of z-fighting.
	if ( !v->isPerspectiveProjection() ) {
		const GLView::ViewState axisView = v->axisAlignedViewState();
		if ( axisView == GLView::ViewDefault )
			return; // Blender hides the planar grid in arbitrary User Orthographic views.
		Transform gridTrans = view;
		gridTrans.rotation = Matrix();
		const QSize viewport = v->getSizeInPixels();
		const float halfH = std::max( v->orthographicHalfHeight(), v->scale() );
		const float halfW = halfH * float( std::max( viewport.width(), 1 ) )
			/ float( std::max( viewport.height(), 1 ) );
		const float worldPerPixel = ( halfH * 2.0f ) / float( std::max( viewport.height(), 1 ) );
		BoundSphere viewBounds = view * bounds();
		const float clipRadius = std::max( viewBounds.radius, 1024.0f * v->scale() );
		gridTrans.translation[2] = -( std::fabs( viewBounds.center[2] ) + clipRadius * 1.45f );

		FloatVector4 axisHorizontal = c0;
		FloatVector4 axisVertical = c0;
		switch ( axisView ) {
		case GLView::ViewTop:
		case GLView::ViewBottom:
			axisHorizontal = cX;
			axisVertical = cY;
			break;
		case GLView::ViewFront:
		case GLView::ViewBack:
			axisHorizontal = cX;
			axisVertical = cZ;
			break;
		case GLView::ViewLeft:
		case GLView::ViewRight:
			axisHorizontal = cY;
			axisVertical = cZ;
			break;
		default:
			break;
		}
		axisHorizontal = ( ( axisHorizontal + c0 ) * 0.5f ).blendValues( c0, 0x08 );
		axisVertical = ( ( axisVertical + c0 ) * 0.5f ).blendValues( c0, 0x08 );
		loadModelViewMatrix( gridTrans );

		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

		// Blender-style decade crossfade: draw three power-of-ten grid levels
		// whose brightness fades with the sub-decade zoom position. Lines shared
		// between levels (every 10th, every 100th) are reinforced by alpha-over
		// blending, giving the fine/minor/major hierarchy with no popping as the
		// view crosses a decade boundary.
		const float anchorPx = 42.0f;	// screen spacing of the finest level at full fade-in
		const float logPos = std::log10( std::max( worldPerPixel * anchorPx, 1.0e-9f ) );
		const float baseExp = std::floor( logPos );
		float frac = logPos - baseExp;
		frac = frac * frac * ( 3.0f - 2.0f * frac );		// smoothstep easing
		const float fineStep = std::pow( 10.0f, baseExp );
		const float levelStep[3]  = { fineStep, fineStep * 10.0f, fineStep * 100.0f };
		const float levelAlpha[3] = { 1.0f - frac, 1.0f, frac };
		const float halfExtent = std::max( halfW, halfH ) * 1.15f;
		for ( int lv = 0; lv < 3; lv++ ) {
			if ( levelAlpha[lv] <= 0.02f )
				continue;
			const float st = levelStep[lv];
			const int nHalf = std::max( 1, int( std::ceil( halfExtent / st ) ) );
			const float cx = std::round( -view.translation[0] / st ) * st;
			const float cy = std::round( -view.translation[1] / st ) * st;
			FloatVector4 lc = c0;
			lc[3] = c0[3] * levelAlpha[lv];
			glEnable( GL_BLEND );
			drawGrid( float( nHalf ) * st, nHalf * 2, 1, lc, lc, lc,
				Vector3( cx, cy, 0.0f ), 0.0f );
		}

		// Axis lines through the world origin, opaque, over the faded grid.
		glEnable( GL_BLEND );
		setGLLineWidth( GLView::Settings::lineWidthGrid );
		{
			const float vcx = -view.translation[0];
			const float vcy = -view.translation[1];
			FloatVector4 * acol = nullptr;
			Vector3 * ap = allocateVertexAttr( 4, &acol );
			if ( ap ) {
				ap[0] = Vector3( vcx - halfExtent, 0.0f, 0.0f );
				ap[1] = Vector3( vcx + halfExtent, 0.0f, 0.0f );
				ap[2] = Vector3( 0.0f, vcy - halfExtent, 0.0f );
				ap[3] = Vector3( 0.0f, vcy + halfExtent, 0.0f );
				acol[0] = acol[1] = axisHorizontal;
				acol[2] = acol[3] = axisVertical;
				drawLines( ap, 4, acol );
			}
		}
		glDisable( GL_BLEND );
		return;
	}

	FloatVector4 c1 = cX;
	FloatVector4 c2 = cY;

	// Keep the grid "grounded" regardless of Up Axis
	Transform gridTrans = view;
	if ( v->cfg.upAxis != GLView::ZAxis ) {
		static const float	axisRotation[27] = {
			1.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f, 1.0f,		// Z
			0.0f, 1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f,		// Y
			0.0f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f		// X
		};
		const float *	ap = axisRotation;
		if ( v->cfg.upAxis == GLView::YAxis ) {
			ap = ap + 9;
			c2 = c1;
			c1.shuffleValues( 0xC6 );	// 2, 1, 0, 3
		} else if ( v->cfg.upAxis == GLView::XAxis ) {
			ap = ap + 18;
			c1 = c2;
			c2.shuffleValues( 0xD8 );	// 0, 2, 1, 3
		}
		gridTrans.rotation = gridTrans.rotation * Matrix( ap );
	}
	c1 = ( ( c1 + c0 ) * 0.5f ).blendValues( c0, 0x08 );
	c2 = ( ( c2 + c0 ) * 0.5f ).blendValues( c0, 0x08 );

	loadModelViewMatrix( gridTrans );

	// TODO: Configurable grid in Settings
	// 1024 game units, major lines every 128, minor lines every 64
	drawGrid( ( nifModel->getBSVersion() >= 170 ? 16.0f : 1024.0f ), 16, 2, c0, c1, c2 );
}

void Scene::drawNodes()
{
	for ( Node * node : roots.list() ) {
		node->draw();
	}
}

void Scene::drawHavok()
{
	for ( Node * node : roots.list() ) {
		node->drawHavok();
	}
}

void Scene::drawFurn()
{
	for ( Node * node : roots.list() ) {
		node->drawFurn();
	}
}

void Scene::drawSelection() const
{
	if ( selecting )
		return; // do not render the selection when selecting

	if ( editMode )
		return; // GLView draws its own vertex/edge/face selection overlay

	if ( objSelActive )
		return; // object-mode selection shows a GLView outline instead

	for ( Node * node : nodes.list() ) {
		node->drawSelection();
	}
}

BoundSphere Scene::bounds() const
{
	if ( !sceneBoundsValid ) {
		bndSphere = BoundSphere();
		for ( Node * node : nodes.list() ) {
			if ( node->isVisible() )
				bndSphere |= node->bounds();
		}
		sceneBoundsValid = true;
	}

	return bndSphere;
}

void Scene::updateTimeBounds() const
{
	if ( !nodes.list().isEmpty() ) {
		tMin = +1000000000; tMax = -1000000000;
		for ( Node * node : nodes.list() ) {
			node->timeBounds( tMin, tMax );
		}
		for ( Property * prop : properties ) {
			prop->timeBounds( tMin, tMax );
		}
	} else {
		tMin = tMax = 0;
	}

	timeBoundsValid = true;
}

float Scene::timeMin() const
{
	if ( animTags.contains( animGroup ) ) {
		if ( animTags[ animGroup ].contains( "start" ) )
			return animTags[ animGroup ][ "start" ];
	}

	if ( !timeBoundsValid )
		updateTimeBounds();

	return ( tMin > tMax ? 0 : tMin );
}

float Scene::timeMax() const
{
	if ( animTags.contains( animGroup ) ) {
		if ( animTags[ animGroup ].contains( "end" ) )
			return animTags[ animGroup ][ "end" ];
	}

	if ( !timeBoundsValid )
		updateTimeBounds();

	return ( tMin > tMax ? 0 : tMax );
}

QString Scene::textStats()
{
	for ( Node * node : nodes.list() ) {
		if ( node->index() == currentBlock ) {
			return node->textStats();
		}
	}
	return QString();
}


Scene::TransformCache::TransformCache()
	: hashTable( nullptr ), hashMask( 0 ), numTransforms( 0 )
{
	rehashCache();
}

Scene::TransformCache::~TransformCache()
{
	delete[] hashTable;
}

const Transform * Scene::TransformCache::find( std::uint64_t key ) const
{
	std::uint64_t	h = 0xFFFFFFFFU;
	hashFunctionUInt64( h, key );
	const std::pair< Transform *, std::uint64_t > *	p = hashTable;
	std::uint32_t	m = hashMask;
	for ( size_t i = size_t( h & m ); p[i].first; i = ( i + 1 ) & m ) {
		if ( p[i].second == key ) [[likely]]
			return p[i].first;
	}
	return nullptr;
}

bool Scene::TransformCache::find( Transform *& valuePtr, std::uint64_t key )
{
	std::uint64_t	h = 0xFFFFFFFFU;
	hashFunctionUInt64( h, key );
	std::pair< Transform *, std::uint64_t > *	p = hashTable;
	std::uint32_t	m = hashMask;
	size_t	i = size_t( h & m );
	for ( ; p[i].first; i = ( i + 1 ) & m ) {
		if ( p[i].second == key ) [[likely]] {
			valuePtr = p[i].first;
			return true;
		}
	}

	Transform *	t = transformBuf.allocateObject< Transform >();
	p[i].first = t;
	p[i].second = key;
	valuePtr = t;
	numTransforms++;
	if ( ( size_t( numTransforms ) * 3 ) >= ( size_t( m ) * 2 ) ) [[unlikely]]
		rehashCache();

	return false;
}

void Scene::TransformCache::rehashCache()
{
	size_t	m = hashMask;
	while ( ( size_t( numTransforms ) * 3 ) >= ( m * 2 ) )
		m = ( m << 1 ) | 0x3F;
	size_t	n = m + 1;
	std::pair< Transform *, std::uint64_t > *	tmp = new std::pair< Transform *, std::uint64_t >[n]();
	if ( numTransforms > 0 ) {
		for ( size_t i = 0; i <= hashMask; i++ ) {
			if ( !hashTable[i].first )
				continue;
			std::uint64_t	h = 0xFFFFFFFFU;
			hashFunctionUInt64( h, hashTable[i].second );
			size_t	j = size_t( h & m );
			while ( tmp[j].first )
				j = ( j + 1 ) & m;
			tmp[j] = hashTable[i];
		}
	}
	delete[] hashTable;
	hashTable = tmp;
	hashMask = std::uint32_t( m );
}

void Scene::TransformCache::clear()
{
	delete[] hashTable;
	hashTable = nullptr;
	hashMask = 0;
	numTransforms = 0;
	transformBuf.clear();
	rehashCache();
}
