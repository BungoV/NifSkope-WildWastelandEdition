// MODULES1 near: the tome gate's culling-off depth probe, moved from glview.cpp (GLView::paintGL calls it).
// Draws the scene once into its own depth FBO with the cull off and hands the depth to wwCellUmbraDepthWrite.
#include "features/near/cellcull.h"
#include "gl/glscene.h"
#include "gl/renderer.h"

#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>

#include <algorithm>
#include <memory>
#include <vector>

//! the depth probe for the tome's visibility gate (WW_CELL_UMBRA_DUMP=<dir> with WW_CELL_UMBRA_DEPTH=1).
void wwCellUmbraDepthPass( Scene * scene )
{
	if ( !scene || !scene->renderer || !wwCellUmbraDepthWanted( scene ) )
		return;
	auto fn = scene->renderer->fn;
	GLint prevFbo = 0, vp[4] = { 0, 0, 0, 0 };
	glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevFbo );
	glGetIntegerv( GL_VIEWPORT, vp );
	const int w = std::max( 1, int( vp[2] ) ), h = std::max( 1, int( vp[3] ) );
	static std::unique_ptr<QOpenGLFramebufferObject> fbo;
	if ( !fbo || fbo->width() != w || fbo->height() != h ) {
		QOpenGLFramebufferObjectFormat fmt;
		fmt.setAttachment( QOpenGLFramebufferObject::Attachment::CombinedDepthStencil );
		fbo = std::make_unique<QOpenGLFramebufferObject>( w, h, fmt );
	}
	if ( !fbo->isValid() )
		return;
	GLfloat prevClear[4];
	glGetFloatv( GL_COLOR_CLEAR_VALUE, prevClear );
	fbo->bind();
	glViewport( 0, 0, w, h );
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	glDepthMask( GL_TRUE );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
	wwCellUmbraProbing( true );
	scene->draw();
	wwCellUmbraProbing( false );
	glDepthMask( GL_TRUE );
	std::vector<float> depth( size_t( w ) * size_t( h ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, fbo->handle() );
	glPixelStorei( GL_PACK_ALIGNMENT, 4 );
	glReadPixels( 0, 0, w, h, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data() );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, GLuint( prevFbo ) );
	glViewport( vp[0], vp[1], vp[2], vp[3] );
	glClearColor( prevClear[0], prevClear[1], prevClear[2], prevClear[3] );
	wwCellUmbraDepthWrite( scene, depth.data(), w, h );
}
