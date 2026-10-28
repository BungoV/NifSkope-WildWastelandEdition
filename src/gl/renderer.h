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

#ifndef GLSHADER_H
#define GLSHADER_H

#include <data/niftypes.h>

#include <QCoreApplication>
#include <QMap>
#include <QVector>
#include <QString>

#include <array>
#include <string>

#include "glcontext.hpp"
#include "material.hpp"

//! @file renderer.h Renderer, Renderer::ConditionSingle, Renderer::ConditionGroup, Renderer::Shader, Renderer::Program

class NifModel;
class Scene;
class Shape;

typedef unsigned int GLenum;
typedef unsigned int GLuint;

//! lane PERF1: the scene-global setters run once per program inside an armed pass (Scene::drawShapes)
//! lane FRAMESPEED2 (D5): passOwned = a uniform the pass's draw hook writes after every setup (the check ignores it)
void wwSetupMemoArm( bool on, const char * passOwned = nullptr );
/* lane PERF2FIX1: the obscurance/reflection opaque passes skip a sorted shape whose last draw (last frame) wrote
 * nothing there: blended or not cell-lit. NewFrame once per obscurance pass; Arm around the sorted draws */
void wwOpaqueCutNewFrame();
void wwOpaqueCutArm( bool on, const char * pass );
bool wwOpaqueCutChecking();	// WW_CELL_OPAQUE_CUT_CHECK=1 (gates): draw all, count the skips that would have written
bool wwOpaqueCutArmed();
bool wwOpaqueCutSkips( const Shape * s );
void wwOpaqueCutSaw( bool opaque );	// the pass's own verdict (wwCellAoDraw / wwCellSsrDraw), for the check
void wwOpaqueCutCheckNote( bool predictedSkip, const QString & name );

//! Manages rendering and shaders
class Renderer : public QObject, public NifSkopeOpenGLContext
{
	/* The PBRM program, once any shape has been routed to it. A shape caches
	 * the program it last drew with and hands it back as the hint; if that
	 * cached program is this one while the current verdict is "not PBRM", the
	 * hint must be refused or the shape stays pinned to a program that has no
	 * lodChannelView uniform. See setupProgram. */
	NifSkopeOpenGLContext::Program * pbrmProgramSeen = nullptr;
	//! pbr_route.prog once the PBR route view has drawn a shape (lane PBRR1):
	//! any hint equal to it is stale once the view is off.
	NifSkopeOpenGLContext::Program * routeProgramSeen = nullptr;

	Q_OBJECT

public:
	Renderer( QOpenGLContext * c );
	~Renderer();

	//! Set up shader program
	Program * setupProgram( Shape *, Program * hint = nullptr );

public slots:
	void updateSettings();

protected:
	// Starfield
	bool setupProgramCE2( const NifModel *, Program *, Shape * );
	// Skyrim, Fallout 4, Fallout 76
	bool setupProgramCE1( const NifModel *, Program *, Shape * );
	// Oblivion, Fallout 3/New Vegas
	bool setupProgramFO3( const NifModel *, Program *, Shape * );
	// PBRM (PBR Material Editor) metallic/roughness, selected by name rather
	// than by .prog conditions — see setupProgram()
	bool setupProgramPBRM( const NifModel *, Program *, Shape * );
	// the PBR route debug view (pbr_route.prog), selected by name
	bool setupProgramRoute( const NifModel *, Program *, Shape * );
	// other games
	void setupFixedFunction( Shape * );

	struct Settings
	{
		std::uint8_t	meshCacheSize = 16;		// in units of 8 MiB
		QString	cubeMapPathFO76;
		QString	cubeMapPathSTF;
	} cfg;

public:
	bool drawSkyBox( Scene * scene );
};

#endif
