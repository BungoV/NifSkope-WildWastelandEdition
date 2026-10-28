#include "taainputs.h"
#include "wwswitch.h"

#include "gl/cellgrass.h"
#include "gl/cellunits.h"	// lane WATERFIX7: the passes' named units
#include "gl/celluniform.h"	// lane CELLUNI5: the tone map's known state
#include "gl/gametaa.h"
#include "gl/glparticles.h"
#include "gl/glproperty.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/renderer.h"
#include "model/nifmodel.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

// lane TAAINPUTS1: velocity + reactive/composition mask for a temporal resolve (taainputs.h)

namespace {

constexpr int kUnitA = WwPassUnit::TaaInA, kUnitB = WwPassUnit::TaaInB;	// above the TAA's 8-11

struct Dmat
{
	double m[4][4];	// [row][col]
	static Dmat identity()
	{
		Dmat d;
		for ( int r = 0; r < 4; r++ )
			for ( int c = 0; c < 4; c++ )
				d.m[r][c] = r == c ? 1.0 : 0.0;
		return d;
	}
	Dmat operator*( const Dmat & b ) const
	{
		Dmat o;
		for ( int r = 0; r < 4; r++ )
			for ( int c = 0; c < 4; c++ ) {
				double s = 0.0;
				for ( int k = 0; k < 4; k++ )
					s += m[r][k] * b.m[k][c];
				o.m[r][c] = s;
			}
		return o;
	}
};

Dmat fromColumnMajor( const float * f )
{
	Dmat d;
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 4; c++ )
			d.m[r][c] = double( f[c * 4 + r] );
	return d;
}

Dmat fromTransform( const Transform & t )
{
	Dmat d = Dmat::identity();
	for ( int r = 0; r < 3; r++ ) {
		for ( int c = 0; c < 3; c++ )
			d.m[r][c] = double( t.rotation( r, c ) ) * double( t.scale );
		d.m[r][3] = double( t.translation[r] );
	}
	return d;
}

bool invert( const Dmat & a, Dmat & out )
{
	double t[4][8];
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 8; c++ )
			t[r][c] = c < 4 ? a.m[r][c] : ( c - 4 == r ? 1.0 : 0.0 );
	for ( int c = 0; c < 4; c++ ) {
		int p = c;
		for ( int r = c + 1; r < 4; r++ )
			if ( std::fabs( t[r][c] ) > std::fabs( t[p][c] ) )
				p = r;
		if ( std::fabs( t[p][c] ) < 1e-300 )
			return false;
		if ( p != c )
			for ( int k = 0; k < 8; k++ )
				std::swap( t[p][k], t[c][k] );
		const double inv = 1.0 / t[c][c];
		for ( int k = 0; k < 8; k++ )
			t[c][k] *= inv;
		for ( int r = 0; r < 4; r++ ) {
			if ( r == c || t[r][c] == 0.0 )
				continue;
			const double f = t[r][c];
			for ( int k = 0; k < 8; k++ )
				t[r][k] -= f * t[c][k];
		}
	}
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 4; c++ )
			out.m[r][c] = t[r][c + 4];
	return true;
}

Matrix4 toMatrix4( const Dmat & d )
{
	float f[16];
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 4; c++ )
			f[c * 4 + r] = float( d.m[r][c] );
	return Matrix4( f );
}

bool sameTransform( const Transform & a, const Transform & b )
{
	return a.rotation == b.rotation && a.translation == b.translation && a.scale == b.scale;
}

// one shape's last two steps: [cur] recorded at the last pass, [prev] the step before it
struct ShapeRec
{
	quint64 step = 0;
	bool prevOk = false;
	Transform curWorld, prevWorld;
	Matrix4 curClip, prevClip;	// unjittered projection x view transform
	Vector3 curSamp[4], prevSamp[4];	// a skinned shape's sample vertices: did the skin move?
	std::vector<Vector3> curSkin, prevSkin;	// full CPU skin, kept only while it moves
};

struct InGpu
{
	GLuint fbo = 0, camFbo = 0;
	GLuint vel = 0, mask = 0, depth = 0;
	int w = 0, h = 0;
	GLenum dsFormat = 0;
	GLbitfield depthBits = 0;
	bool ok = false;	// this frame's textures were drawn
	GLuint q[4] = { 0, 0, 0, 0 };	// two begin/end timestamp pairs
	bool qPending[2] = { false, false };
	int qNext = 0;
};

struct InState
{
	bool init = false;
	quint64 liveGen = 0;
	int pin = -1;	// -1 unset, 0 never, 1 always
	int view = 0;	// 0 off, 1 velocity, 2 mask, 3 both
	float viewScale = 8.0f;
	bool redCamOnly = false, redNoReactive = false;
	bool redStaleStep = false;	// lane TAA4: the step key without the TAA frame (the stale-camera bug)
	QString dumpDir;
	int dumpFrame = 8;

	int requested = 0;	// passes left on the last request
	int passes = 0;

	// the step bookkeeping (a repaint of the same camera and time repeats the step)
	bool haveKey = false;
	float keyP[16] = {};
	Transform keyView;
	float keyTime = 0.0f;
	int keyFrame = -1;	// lane TAA4: the TAA frame index of the last step (-1 none)
	quint64 step = 0;
	bool haveVP = false;
	Dmat curVP = Dmat::identity(), prevVP = Dmat::identity();
	QHash<const Shape *, ShapeRec> shapes;

	QHash<Scene *, QVector<Node *>> blended;
	double gpuMs = -1.0, cpuMs = 0.0;
	QString last = QStringLiteral( "off" ), said;
};

InState & st()
{
	static InState s;
	// a live set of any WW_TAAINPUTS* re-reads them all (defaults first)
	if ( !s.init || wwSwitchMovedFor( s.liveGen, { "WW_TAAINPUTS" } ) ) {
		s.init = true;
		s.liveGen = wwSwitchGen();
		const QString pin = wwEnvStr( "WW_TAAINPUTS" ).trimmed();
		s.pin = pin.isEmpty() ? -1 : ( pin == QLatin1StringView( "0" ) || pin.compare( QLatin1StringView( "off" ), Qt::CaseInsensitive ) == 0 ) ? 0 : 1;
		const QString v = wwEnvStr( "WW_TAAINPUTS_VIEW" ).trimmed().toLower();
		s.view = v == QLatin1StringView( "velocity" ) || v == QLatin1StringView( "vel" ) ? 1 : v == QLatin1StringView( "mask" ) ? 2
			: v == QLatin1StringView( "both" ) ? 3 : 0;
		bool ok = false;
		const float sc = wwEnvStr( "WW_TAAINPUTS_VIEW_SCALE" ).toFloat( &ok );
		s.viewScale = ok && sc > 0.0f ? sc : 8.0f;
		const QString red = wwEnvStr( "WW_TAAINPUTS_RED" );
		s.redCamOnly = red.contains( QLatin1StringView( "camonly" ) );
		s.redNoReactive = red.contains( QLatin1StringView( "noreactive" ) );
		s.redStaleStep = red.contains( QLatin1StringView( "stalestep" ) );
		s.dumpDir = wwEnvStr( "WW_TAAINPUTS_DUMP" ).trimmed();
		const int df = wwEnvStr( "WW_TAAINPUTS_DUMP_FRAME" ).toInt( &ok );
		s.dumpFrame = ok ? df : 8;
		s.passes = 0;	// a live set starts the dump's count again (one dump per row)
	}
	return s;
}

QHash<Renderer *, InGpu> & gpus()
{
	static QHash<Renderer *, InGpu> g;
	return g;
}

void note( InState & s, const QString & line, const QString & key )
{
	s.last = line;
	if ( s.said != key ) {	// changes only, not every frame
		s.said = key;
		qInfo().noquote() << "taa inputs:" << line;
	}
}

GLuint makeTex( Renderer * r, GLenum internal, GLenum format, GLenum type, int w, int h )
{
	auto fn = r->fn;
	GLuint t = 0;
	fn->glGenTextures( 1, &t );
	fn->glBindTexture( GL_TEXTURE_2D, t );
	fn->glTexImage2D( GL_TEXTURE_2D, 0, GLint( internal ), w, h, 0, format, type, nullptr );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
	if ( format == GL_DEPTH_COMPONENT || format == GL_DEPTH_STENCIL )
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	return t;
}

void freeTex( Renderer * r, InGpu & g )
{
	for ( GLuint * t : { &g.vel, &g.mask, &g.depth } ) {
		if ( *t )
			r->fn->glDeleteTextures( 1, t );
		*t = 0;
	}
}

std::vector<float> readFloat( Renderer * r, GLuint fbo, GLenum attachment, GLenum format, int ch, int w, int h )
{
	auto fn = r->fn;
	std::vector<float> v( size_t( w ) * size_t( h ) * size_t( ch ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, fbo );
	fn->glReadBuffer( attachment );
	fn->glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	fn->glReadPixels( 0, 0, w, h, format, GL_FLOAT, v.data() );
	return v;
}

// float32: int32 w h channels, then rows bottom-up (GL order) -- WW_TAA_DUMP's layout
bool dumpFloat( const QString & path, int w, int h, int ch, const std::vector<float> & v )
{
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly ) )
		return false;
	const std::int32_t hdr[3] = { w, h, ch };
	f.write( reinterpret_cast<const char *>( hdr ), sizeof( hdr ) );
	f.write( reinterpret_cast<const char *>( v.data() ), qint64( v.size() * sizeof( float ) ) );
	return true;
}

// a skinned shape's sample vertices (first, quarter, half, last)
void skinSamples( const Shape * sh, Vector3 out[4] )
{
	const int n = int( sh->verts.size() );
	const int at[4] = { 0, n / 4, n / 2, n - 1 };
	for ( int i = 0; i < 4; i++ )
		out[i] = sh->skinVertex( at[i], sh->verts.at( at[i] ) );
}

// the base map's alpha (alpha-tested object vectors, blended mask): false when the shape has none to give
bool bindBase( NifSkopeOpenGLContext::Program * prog, const Shape * sh )
{
	BSShaderLightingProperty * bsp = sh->wwShaderLighting();
	if ( !bsp || sh->coords.isEmpty() || sh->coords.constFirst().size() != sh->verts.size() )
		return false;
	int texunit = 0;
	if ( !prog->uniSampler( bsp, "BaseMap", 0, texunit, QString(), bsp->clampMode, QString() ) )
		return false;
	prog->uni2f( "uvScale", bsp->uvScale.x, bsp->uvScale.y );
	prog->uni2f( "uvOffset", bsp->uvOffset.x, bsp->uvOffset.y );
	return true;
}

}	// namespace

void wwTaaInputsRequest()
{
	st().requested = 2;
}

void wwTaaInputsNoteBlended( Scene * scene, const QVector<Node *> & drawn )
{
	InState & s = st();
	const bool want = s.pin == 1 || ( s.pin != 0 && ( s.requested > 0 || s.view || !s.dumpDir.isEmpty() ) );
	if ( !want || s.redNoReactive || !scene || scene->selecting )
		return;
	s.blended[scene] = drawn;
}

unsigned wwTaaInputsVelocityTex( Scene * scene, int & w, int & h )
{
	if ( !scene || !scene->renderer )
		return 0;
	auto it = gpus().find( scene->renderer );
	if ( it == gpus().end() || !it->ok )
		return 0;
	w = it->w;
	h = it->h;
	return it->vel;
}

unsigned wwTaaInputsMaskTex( Scene * scene, int & w, int & h )
{
	if ( !scene || !scene->renderer )
		return 0;
	auto it = gpus().find( scene->renderer );
	if ( it == gpus().end() || !it->ok )
		return 0;
	w = it->w;
	h = it->h;
	return it->mask;
}

QString wwTaaInputsEcho()
{
	return st().last;
}

void wwTaaInputsPass( Scene * scene )
{
	InState & s = st();
	if ( !scene || !scene->renderer || scene->selecting )
		return;
	Renderer * r = scene->renderer;
	{
		auto it = gpus().find( r );
		if ( it != gpus().end() )
			it->ok = false;	// the textures count only for the frame that drew them
	}
	const bool want = s.pin == 1 || ( s.pin != 0 && ( s.requested > 0 || s.view || !s.dumpDir.isEmpty() ) );
	if ( s.requested > 0 )
		s.requested--;
	const QVector<Node *> blended = s.blended.take( scene );
	if ( !want ) {
		if ( s.last != QLatin1StringView( "off" ) )
			note( s, QStringLiteral( "off (nobody asked)" ), QStringLiteral( "off" ) );
		s.last = QStringLiteral( "off" );
		s.haveKey = false;
		if ( !s.redStaleStep )
			s.haveVP = false;	// lane TAA4: the next pass starts from its own camera, not the one it last saw
		s.shapes.clear();
		return;
	}
	QElapsedTimer cpu;
	cpu.start();
	auto fn = r->fn;
	while ( fn->glGetError() != GL_NO_ERROR ) {
	}

	/* lane CELLUNI5: run from the linear resolve (inside cellhdr's End), the tone map's known state; after the tone map
	 * (glview's post) the slot is empty: asked, as before */
	const WwCellUni::KnownGl * kn = WwCellUni::known( fn );
	GLint prevDraw = 0, prevRead = 0, vp[4] = { 0, 0, 1, 1 };
	if ( kn ) {
		prevDraw = kn->draw;
		prevRead = kn->read;
		std::memcpy( vp, kn->vp, sizeof vp );
	} else {
		fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
		fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
		fn->glGetIntegerv( GL_VIEWPORT, vp );
	}
	const int W = vp[2], H = vp[3];
	if ( W < 2 || H < 2 )
		return;

	// the frame's depth format (the copy goes by a blit: the same format both sides)
	GLint depthBits = 0, stencilBits = 0, compType = 0;
	const GLenum att = prevDraw ? GL_DEPTH_ATTACHMENT : GL_DEPTH;
	fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, att, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depthBits );
	fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, prevDraw ? GL_STENCIL_ATTACHMENT : GL_STENCIL,
		GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencilBits );
	fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, att, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &compType );
	GLenum dsFormat = 0, dsFmt = 0, dsType = 0;
	GLbitfield blitBits = GL_DEPTH_BUFFER_BIT;
	if ( stencilBits == 8 && depthBits == 24 && compType != GL_FLOAT ) {
		dsFormat = GL_DEPTH24_STENCIL8; dsFmt = GL_DEPTH_STENCIL; dsType = GL_UNSIGNED_INT_24_8;
		blitBits |= GL_STENCIL_BUFFER_BIT;
	} else if ( stencilBits == 8 && depthBits == 32 && compType == GL_FLOAT ) {
		dsFormat = GL_DEPTH32F_STENCIL8; dsFmt = GL_DEPTH_STENCIL; dsType = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
		blitBits |= GL_STENCIL_BUFFER_BIT;
	} else if ( stencilBits == 0 && depthBits == 24 ) {
		dsFormat = GL_DEPTH_COMPONENT24; dsFmt = GL_DEPTH_COMPONENT; dsType = GL_UNSIGNED_INT;
	} else if ( stencilBits == 0 && depthBits == 32 && compType == GL_FLOAT ) {
		dsFormat = GL_DEPTH_COMPONENT32F; dsFmt = GL_DEPTH_COMPONENT; dsType = GL_FLOAT;
	}
	if ( !dsFormat ) {
		const QString why = QStringLiteral( "refused (the frame's depth is %1 bits, %2 stencil)" ).arg( depthBits ).arg( stencilBits );
		note( s, why, why );
		return;
	}

	// the camera: the projection as uploaded (jittered when the TAA jitters) and without the jitter
	const Matrix4 Pjit( &( r->globalUniforms->projectionMatrix[0][0] ) );
	Matrix4 Pun = Pjit;
	float jx = 0.0f, jy = 0.0f;
	if ( !( wwGameTaaUnjittered( Pun ) && wwGameTaaJitter( jx, jy ) ) ) {
		Pun = Pjit;
		jx = jy = 0.0f;
	}
	const Dmat curVP = fromColumnMajor( Pun.data() ) * fromTransform( scene->view );

	// the step: the same camera and time again is a repaint of the same step (its "previous" stays).
	// lane TAA4: and the same TAA frame. The camera path holds the time, so on a still path every frame was a
	// "repaint" and prevVP stayed a stale camera: Ultra's vectors read hundreds of px. The step now follows the TAA
	// history: next frame = a step, same frame = a repaint, any other frame = a cut (no previous).
	int taaFrame = -1;
	const bool haveFrame = !s.redStaleStep && wwGameTaaFrame( taaFrame );
	const bool sameCam = s.haveKey && std::memcmp( s.keyP, Pun.data(), sizeof( s.keyP ) ) == 0
		&& sameTransform( s.keyView, scene->view ) && s.keyTime == scene->time;
	const bool repeat = sameCam && ( !haveFrame || taaFrame == s.keyFrame );
	const bool cut = haveFrame && s.haveKey && !repeat && s.keyFrame >= 0 && taaFrame != s.keyFrame + 1;
	std::memcpy( s.keyP, Pun.data(), sizeof( s.keyP ) );
	s.keyView = scene->view;
	s.keyTime = scene->time;
	s.keyFrame = haveFrame ? taaFrame : -1;
	const bool hadKey = s.haveKey;
	s.haveKey = true;
	if ( !repeat ) {
		s.step += cut ? 2 : 1;	// a cut: no shape was seen on the step before
		s.prevVP = s.haveVP && ( s.redStaleStep || ( hadKey && !cut ) ) ? s.curVP : curVP;
	}
	const bool camOk = hadKey && s.haveVP;
	s.curVP = curVP;
	s.haveVP = true;

	// the shapes that move on their own this step
	struct Mover
	{
		const Shape * sh;
		const ShapeRec * rec;
		int mode;	// 0 rigid (world moved), 1 skinned, 2 grass wind
	};
	std::vector<Mover> movers;
	int nSkinned = 0, nGrass = 0, nRigid = 0, nShapes = 0;
	float wind[4] = { 0, 0, 0, 0 }, wind2[4] = { 0, 0, 0, 0 };
	const bool haveWind = wwCellGrassWind( wind, wind2 ) && wind2[1] > 0.0f;
	const bool cpuSkin = scene->hasOption( Scene::DoSkinning );
	if ( !s.redCamOnly ) {
		for ( Node * node : scene->nodes.list() ) {
			const Shape * sh = dynamic_cast<const Shape *>( node );
			if ( !sh || !sh->isVisible() || sh->verts.isEmpty() || sh->triangles.isEmpty() )
				continue;
			nShapes++;
			ShapeRec & rec = s.shapes[sh];
			const bool seenLast = rec.step != 0 && rec.step + 1 == s.step;	// step 0 = a fresh record
			if ( !repeat ) {
				rec.prevOk = seenLast;
				if ( seenLast ) {
					rec.prevWorld = rec.curWorld;
					rec.prevClip = rec.curClip;
					for ( int i = 0; i < 4; i++ )
						rec.prevSamp[i] = rec.curSamp[i];
					std::swap( rec.prevSkin, rec.curSkin );
				} else {
					rec.prevSkin.clear();
				}
				rec.curSkin.clear();
			} else if ( rec.step != s.step ) {
				rec.prevOk = false;	// new on a repaint
			}
			rec.step = s.step;
			rec.curWorld = sh->worldTrans();
			rec.curClip = Pun * sh->viewTrans().toMatrix4();
			const bool skinned = cpuSkin && sh->wwGpuSkinned();
			if ( skinned )
				skinSamples( sh, rec.curSamp );
			if ( !rec.prevOk )
				continue;
			bool skinMoved = false;
			if ( skinned ) {
				for ( int i = 0; i < 4 && !skinMoved; i++ )
					skinMoved = !( rec.curSamp[i] == rec.prevSamp[i] );
			}
			const bool worldMoved = !sameTransform( rec.curWorld, rec.prevWorld );
			if ( skinned && ( skinMoved || worldMoved ) ) {
				// the full skin while it moves; the step it starts, its previous positions are this step's
				if ( rec.curSkin.size() != size_t( sh->verts.size() ) ) {
					rec.curSkin.resize( size_t( sh->verts.size() ) );
					for ( qsizetype v = 0; v < sh->verts.size(); v++ )
						rec.curSkin[size_t( v )] = sh->skinVertex( int( v ), sh->verts.at( v ) );
				}
				movers.push_back( { sh, &rec, 1 } );
				nSkinned++;
			} else if ( haveWind && wwCellGrassIsShape( scene->nifModel, sh->id() ) ) {
				movers.push_back( { sh, &rec, 2 } );
				nGrass++;
			} else if ( worldMoved ) {
				movers.push_back( { sh, &rec, 0 } );
				nRigid++;
			}
		}
		// forget shapes gone for a while (a closed cell, a streamed-out block)
		if ( ( s.passes & 63 ) == 0 ) {
			for ( auto it = s.shapes.begin(); it != s.shapes.end(); ) {
				if ( it->step + 2 < s.step )
					it = s.shapes.erase( it );
				else
					++it;
			}
		}
	}

	InGpu & g = gpus()[r];
	if ( !g.fbo || g.w != W || g.h != H || g.dsFormat != dsFormat ) {
		freeTex( r, g );
		if ( !g.fbo ) {
			fn->glGenFramebuffers( 1, &g.fbo );
			fn->glGenFramebuffers( 1, &g.camFbo );
			fn->glGenQueries( 4, g.q );
		}
		g.vel = makeTex( r, GL_RG16F, GL_RG, GL_FLOAT, W, H );
		g.mask = makeTex( r, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, W, H );
		g.depth = makeTex( r, dsFormat, dsFmt, dsType, W, H );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.vel, 0 );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g.mask, 0 );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, dsFmt == GL_DEPTH_STENCIL ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT,
			GL_TEXTURE_2D, g.depth, 0 );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.camFbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.vel, 0 );
		g.w = W;
		g.h = H;
		g.dsFormat = dsFormat;
		g.depthBits = blitBits;
	}

	// last frame's GPU time, when the pair has landed (never a stall). Timestamp pairs: elapsed queries cannot nest.
	for ( int i = 0; i < 2; i++ ) {
		if ( !g.qPending[i] )
			continue;
		GLint avail = 0;
		fn->glGetQueryObjectiv( g.q[i * 2 + 1], GL_QUERY_RESULT_AVAILABLE, &avail );
		if ( avail ) {
			GLuint64 t0 = 0, t1 = 0;
			fn->glGetQueryObjectui64v( g.q[i * 2], GL_QUERY_RESULT, &t0 );
			fn->glGetQueryObjectui64v( g.q[i * 2 + 1], GL_QUERY_RESULT, &t1 );
			s.gpuMs = t1 > t0 ? double( t1 - t0 ) * 1e-6 : 0.0;
			g.qPending[i] = false;
		}
	}
	const int qi = g.qNext;
	const bool timed = !g.qPending[qi];
	if ( timed ) {
		fn->glQueryCounter( g.q[qi * 2], GL_TIMESTAMP );
		g.qNext ^= 1;
	}

	// state kept for the restore
	GLint prevActive = 0, blendEq = 0, bSrc = 0, bDst = 0, bSrcA = 0, bDstA = 0, depthFunc = 0;
	if ( kn )
		prevActive = kn->active;
	else
		fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glGetIntegerv( GL_BLEND_EQUATION_RGB, &blendEq );
	fn->glGetIntegerv( GL_BLEND_SRC_RGB, &bSrc );
	fn->glGetIntegerv( GL_BLEND_DST_RGB, &bDst );
	fn->glGetIntegerv( GL_BLEND_SRC_ALPHA, &bSrcA );
	fn->glGetIntegerv( GL_BLEND_DST_ALPHA, &bDstA );
	fn->glGetIntegerv( GL_DEPTH_FUNC, &depthFunc );
	GLboolean colorMask[4], depthMask = GL_TRUE;
	if ( kn ) {
		std::memcpy( colorMask, kn->colorMask, sizeof colorMask );
		depthMask = kn->depthMask;
	} else {
		fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
		fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	}
	const bool wasDepth = kn ? kn->depth : fn->glIsEnabled( GL_DEPTH_TEST ), wasBlend = kn ? kn->blend : fn->glIsEnabled( GL_BLEND );
	const bool wasCull = kn ? kn->cull : fn->glIsEnabled( GL_CULL_FACE ), wasStencil = kn ? kn->stencil : fn->glIsEnabled( GL_STENCIL_TEST );
	const bool wasScissor = kn ? kn->scissor : fn->glIsEnabled( GL_SCISSOR_TEST ), wasOffset = fn->glIsEnabled( GL_POLYGON_OFFSET_FILL );
	GLfloat offFactor = 0.0f, offUnits = 0.0f;
	fn->glGetFloatv( GL_POLYGON_OFFSET_FACTOR, &offFactor );
	fn->glGetFloatv( GL_POLYGON_OFFSET_UNITS, &offUnits );

	// 1. the frame's depth, cleared targets
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.fbo );
	fn->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, g.depthBits, GL_NEAREST );
	const GLenum blitErr = fn->glGetError();
	{
		const GLenum two[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		fn->glDrawBuffers( 2, two );
		fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		const GLfloat zero[4] = { 0, 0, 0, 0 };
		fn->glClearBufferfv( GL_COLOR, 0, zero );
		fn->glClearBufferfv( GL_COLOR, 1, zero );
	}
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDepthMask( GL_FALSE );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_STENCIL_TEST );

	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * quadAttrs = quad;
	bool ok = blitErr == GL_NO_ERROR;

	// 2. the camera's vectors everywhere (game_taa_mv: the same math as the game TAA's own)
	if ( ok ) {
		Dmat invCur, reproj = Dmat::identity();
		if ( camOk && invert( curVP, invCur ) )
			reproj = s.prevVP * invCur;
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.camFbo );
		const GLenum one = GL_COLOR_ATTACHMENT0;
		fn->glDrawBuffers( 1, &one );
		auto prog = r->useProgram( "game_taa_mv.prog" );
		if ( !prog ) {
			ok = false;
		} else {
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitA ) );
			fn->glBindTexture( GL_TEXTURE_2D, g.depth );
			prog->uni1i( "depthTex", kUnitA );
			prog->uni4m( "reproj", toMatrix4( reproj ) );
			prog->uni2f( "jitterNdc", jx, jy );
			prog->uni2f( "invSize", 1.0f / float( W ), 1.0f / float( H ) );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &quadAttrs, idx );
			r->stopProgram();
			fn->glBindTexture( GL_TEXTURE_2D, 0 );
		}
	}

	// 3. the things that moved on their own, depth-tested against the frame (a hair nearer: the same surface passes)
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
	fn->glEnable( GL_DEPTH_TEST );
	fn->glDepthFunc( GL_LEQUAL );
	fn->glEnable( GL_POLYGON_OFFSET_FILL );
	fn->glPolygonOffset( -1.0f, -4.0f );
	int drawnObj = 0;
	if ( ok && !movers.empty() ) {
		const GLenum two[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		fn->glDrawBuffers( 2, two );
		fn->glColorMaski( 1, GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE );	// the own-motion flag only
		auto prog = r->useProgram( "taa_objvel.prog" );
		if ( prog ) {
			std::vector<Vector3> prevPos;
			for ( const Mover & m : movers ) {
				const Shape * sh = m.sh;
				const Matrix4 mv = sh->viewTrans().toMatrix4();
				prog->uni4m( "clipJit", Pjit * mv );
				prog->uni4m( "clipCur", m.rec->curClip );
				prog->uni4m( "clipPrev", m.rec->prevClip );
				prog->uni4m( "modelViewMatrix", mv );
				prog->uni1i( "velMode", m.mode );
				const bool at = sh->wwAlphaTested() && bindBase( prog, sh );
				prog->uni1i( "alphaTest", at ? 1 : 0 );
				if ( at ) {
					const BSLightingShaderProperty * lsp = sh->wwLightingShader();
					prog->uni1f( "alpha", lsp ? lsp->alpha : 1.0f );
					prog->uni1f( "alphaThreshold", sh->wwAlphaThreshold() );
				}
				if ( m.mode == 2 )
					wwCellGrassUniforms( scene, sh->id() );	// the fade and the wind's previous + current phase
				else
					prog->uni3f( "wwGrassFade", 0.0f, 0.0f, 0.0f );
				if ( m.mode == 1 ) {
					const std::vector<Vector3> & cur = m.rec->curSkin;
					const std::vector<Vector3> & prv = m.rec->prevSkin.size() == cur.size() ? m.rec->prevSkin : cur;
					const float * attrv[9] = { &( cur.front()[0] ), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
						at ? &( sh->coords.constFirst().constFirst()[0] ) : nullptr, &( prv.front()[0] ) };
					const std::uint64_t mask = 0x300000003ULL | ( at ? 0x20000000ULL : 0ULL );
					r->drawShape( (unsigned int) sh->verts.size(), mask, (unsigned int) ( sh->triangles.size() * 3 ),
						GL_TRIANGLES, GL_UNSIGNED_SHORT, attrv, sh->triangles.constData() );
				} else {
					if ( !sh->bindShape() )
						continue;
					fn->glDrawElements( GL_TRIANGLES, GLsizei( sh->triangles.size() * 3 ), GL_UNSIGNED_SHORT, nullptr );
				}
				drawnObj++;
			}
			r->stopProgram();
		} else {
			ok = false;
		}
		fn->glColorMaski( 1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	}

	// 4. the see-through layers into the mask (R reactive, G composition), by MAX
	int nBlend = 0, nPart = 0;
	if ( ok && !s.redNoReactive && !blended.isEmpty() ) {
		const GLenum second[2] = { GL_NONE, GL_COLOR_ATTACHMENT1 };
		fn->glDrawBuffers( 2, second );
		fn->glColorMaski( 1, GL_TRUE, GL_TRUE, GL_FALSE, GL_FALSE );
		fn->glEnable( GL_BLEND );
		fn->glBlendEquation( GL_MAX );
		fn->glBlendFunc( GL_ONE, GL_ONE );
		auto prog = r->useProgram( "taa_reactive.prog" );
		if ( prog ) {
			const Matrix4 view = scene->view.toMatrix4();
			// the camera's forward in the world (the eye's -Z): the sprites face it, as particles.geom turns them
			const Vector3 fwd( -scene->view.rotation( 2, 0 ), -scene->view.rotation( 2, 1 ), -scene->view.rotation( 2, 2 ) );
			QVector<Vector3> tris;
			QVector<Vector2> uvs;
			QVector<Color4> cols;
			std::vector<std::uint32_t> pidx;
			for ( Node * node : blended ) {
				if ( Particles * p = dynamic_cast<Particles *>( node ) ) {
					if ( !p->bakeSprites( fwd, tris, uvs, cols ) || tris.isEmpty() )
						continue;
					pidx.resize( size_t( tris.size() ) );
					for ( size_t i = 0; i < pidx.size(); i++ )
						pidx[i] = std::uint32_t( i );
					prog->uni4m( "clipJit", Pjit * view );
					prog->uni1i( "useTex", 0 );
					prog->uni1i( "hasColor", 1 );
					prog->uni1f( "matAlpha", 1.0f );
					prog->uni2f( "outRG", 1.0f, 0.0f );
					const float * attrv[2] = { &( tris.constFirst()[0] ), &( cols.constFirst()[0] ) };
					r->drawShape( (unsigned int) tris.size(), 0x43ULL, (unsigned int) pidx.size(), GL_TRIANGLES, GL_UNSIGNED_INT,
						attrv, pidx.data() );
					nPart++;
					continue;
				}
				const Shape * sh = dynamic_cast<const Shape *>( node );
				if ( !sh || sh->verts.isEmpty() || sh->triangles.isEmpty() || sh->wwDecalDrawsFirst() )
					continue;
				prog->uni4m( "clipJit", Pjit * sh->viewTrans().toMatrix4() );
				const bool water = sh->wwWaterDraws();
				prog->uni1i( "hasColor", !water && sh->colors.size() >= sh->verts.size() ? 1 : 0 );
				if ( water ) {
					prog->uni1i( "useTex", 0 );
					prog->uni1f( "matAlpha", 1.0f );
					prog->uni2f( "outRG", 0.0f, 1.0f );
				} else {
					const BSLightingShaderProperty * lsp = sh->wwLightingShader();
					const auto * esp = dynamic_cast<const BSEffectShaderProperty *>( sh->wwShaderLighting() );
					prog->uni1i( "useTex", bindBase( prog, sh ) ? 1 : 0 );
					prog->uni1f( "matAlpha", lsp ? lsp->alpha : esp ? esp->getAlpha() : 1.0f );
					prog->uni2f( "outRG", 1.0f, lsp ? 1.0f : 0.0f );	// lit see-through (glass): composition too
				}
				if ( !sh->bindShape() )
					continue;
				fn->glDrawElements( GL_TRIANGLES, GLsizei( sh->triangles.size() * 3 ), GL_UNSIGNED_SHORT, nullptr );
				nBlend++;
			}
			r->stopProgram();
		} else {
			ok = false;
		}
		fn->glColorMaski( 1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	}

	if ( timed ) {
		fn->glQueryCounter( g.q[qi * 2 + 1], GL_TIMESTAMP );
		g.qPending[qi] = true;
	}

	// the dump: the two textures of pass WW_TAAINPUTS_DUMP_FRAME
	s.passes++;
	if ( ok && !s.dumpDir.isEmpty() && s.passes == s.dumpFrame ) {
		QDir().mkpath( s.dumpDir );
		const QString d = s.dumpDir + QLatin1Char( '/' );
		dumpFloat( d + "vel.bin", W, H, 2, readFloat( r, g.fbo, GL_COLOR_ATTACHMENT0, GL_RG, 2, W, H ) );
		dumpFloat( d + "mask.bin", W, H, 4, readFloat( r, g.fbo, GL_COLOR_ATTACHMENT1, GL_RGBA, 4, W, H ) );
		QFile f( d + "inputs.txt" );
		if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream t( &f );
			t << "pass " << s.passes << "\nstep " << s.step << "\nrepeat " << ( repeat ? 1 : 0 ) << "\nW " << W << "\nH " << H
			  << "\njitter " << jx << ' ' << jy << "\nshapes " << nShapes << "\nmovers " << movers.size() << "\nskinned " << nSkinned
			  << "\ngrass " << nGrass << "\nrigid " << nRigid << "\nblended " << nBlend << "\nparticles " << nPart
			  << "\nred " << ( s.redCamOnly ? "camonly" : s.redNoReactive ? "noreactive" : "none" ) << "\ntaaframe " << taaFrame
			  << "\ncut " << ( cut ? 1 : 0 ) << "\nstalestep " << ( s.redStaleStep ? 1 : 0 ) << '\n';
		}
	}

	for ( int u : { kUnitA, kUnitB } ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
	}
	fn->glActiveTexture( GLenum( prevActive ) );
	if ( wasStencil ) fn->glEnable( GL_STENCIL_TEST ); else fn->glDisable( GL_STENCIL_TEST );
	if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST ); else fn->glDisable( GL_DEPTH_TEST );
	if ( wasBlend ) fn->glEnable( GL_BLEND ); else fn->glDisable( GL_BLEND );
	if ( wasCull ) fn->glEnable( GL_CULL_FACE ); else fn->glDisable( GL_CULL_FACE );
	if ( wasScissor ) fn->glEnable( GL_SCISSOR_TEST );
	if ( wasOffset ) fn->glEnable( GL_POLYGON_OFFSET_FILL ); else fn->glDisable( GL_POLYGON_OFFSET_FILL );
	fn->glPolygonOffset( offFactor, offUnits );
	fn->glBlendEquation( GLenum( blendEq ) );
	fn->glBlendFuncSeparate( GLenum( bSrc ), GLenum( bDst ), GLenum( bSrcA ), GLenum( bDstA ) );
	fn->glDepthFunc( GLenum( depthFunc ) );
	fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
	fn->glDepthMask( depthMask );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	if ( prevDraw ) {
		const GLenum one = GL_COLOR_ATTACHMENT0;
		fn->glDrawBuffers( 1, &one );
	}
	g.ok = ok;
	s.cpuMs = double( cpu.nsecsElapsed() ) * 1e-6;
	if ( !ok ) {
		const QString why = blitErr != GL_NO_ERROR ? QStringLiteral( "refused (the depth blit failed, GL error %1)" ).arg( blitErr )
			: QStringLiteral( "refused (a taa inputs program is missing)" );
		note( s, why, why );
		return;
	}
	const QString counts = QStringLiteral( "%1 movers (%2 skinned, %3 grass, %4 rigid) of %5 shapes; %6 blended, %7 particle systems%8" )
		.arg( movers.size() ).arg( nSkinned ).arg( nGrass ).arg( nRigid ).arg( nShapes ).arg( nBlend ).arg( nPart )
		.arg( ( s.redCamOnly ? QStringLiteral( " RED camonly" ) : s.redNoReactive ? QStringLiteral( " RED noreactive" ) : QString() )
			+ ( s.redStaleStep ? QStringLiteral( " RED stalestep" ) : QString() ) );
	note( s, QStringLiteral( "%1x%2 %3; gpu %4 ms cpu %5 ms" ).arg( W ).arg( H ).arg( counts )
		.arg( s.gpuMs < 0.0 ? QStringLiteral( "?" ) : QString::number( s.gpuMs, 'f', 3 ) ).arg( s.cpuMs, 0, 'f', 3 ),
		QStringLiteral( "on %1x%2 %3" ).arg( W ).arg( H ).arg( counts ) );
	Q_UNUSED( drawnObj );
}

void wwTaaInputsView( Scene * scene )
{
	InState & s = st();
	if ( !s.view || !scene || !scene->renderer || scene->selecting )
		return;
	Renderer * r = scene->renderer;
	auto it = gpus().find( r );
	if ( it == gpus().end() || !it->ok )
		return;
	auto fn = r->fn;
	GLint vp[4] = { 0, 0, 1, 1 }, prevActive = 0;
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	if ( vp[2] != it->w || vp[3] != it->h )
		return;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasBlend = fn->glIsEnabled( GL_BLEND );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_BLEND );
	auto prog = r->useProgram( "taa_inputs_view.prog" );
	if ( prog ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitA ) );
		fn->glBindTexture( GL_TEXTURE_2D, it->vel );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitB ) );
		fn->glBindTexture( GL_TEXTURE_2D, it->mask );
		prog->uni1i( "velTex", kUnitA );
		prog->uni1i( "maskTex", kUnitB );
		prog->uni1i( "viewMode", s.view );
		prog->uni2f( "size", float( it->w ), float( it->h ) );
		prog->uni1f( "viewScale", s.viewScale );
		static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
		static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
		const float * attrs = quad;
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
		r->stopProgram();
		for ( int u : { kUnitA, kUnitB } ) {
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
			fn->glBindTexture( GL_TEXTURE_2D, 0 );
		}
	}
	fn->glActiveTexture( GLenum( prevActive ) );
	if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST );
	if ( wasBlend ) fn->glEnable( GL_BLEND );
}
