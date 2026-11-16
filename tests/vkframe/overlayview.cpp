// Lane VKFLIGHT4 item 1: the view's QPainter overlay without GL (vkoverlay.inc + overlay_blend_cs.hlsl).
// One paint routine (text, a translucent box, antialiased lines, an ellipse: what the F3 line, the pass timers, the labels
// and the gizmo draw) runs twice over the same backdrop B (RGBA8, GL's rows, the bottom first):
//   G: GL's QPainter on a QOpenGLPaintDevice bound to an FBO holding B (the route the app had), read with glReadPixels;
//   Q: the same calls into a QImage (ARGB32 premultiplied, the app's new target), then WwVkPresent::overlayBlendTest
//      (overlaySet's scan + ovStage + ovRecord on the present device: the ring's own code) blends Q over B -> V.
//   C: a CPU reference of V: premultiplied over (ONE, ONE_MINUS_SRC_ALPHA), Q's row y on B's row h - 1 - y.
// Bars, written before the first run:
//   V vs C: every pixel within 1 level per channel, ink pixels compared > 0, pixels without ink EXACT (B kept).
//   V vs G: the box of pixels that differ from B (placement and the flip) within 2 px on every edge, the IoU of the two
//           changed sets >= 0.80; the mean and max level difference over their union are printed, not barred (QPainter's
//           raster and GL engines may rasterise text differently: reported, never widened).
// Reds (each must FAIL V vs C): red 1 no flip, red 2 R and B swapped, red 4 straight alpha, dropped (no overlay bits).
// Arms: DPR 1 (320 x 200) and DPR 2 (640 x 400); each prints both devices' logical DPI and DPR.
// Last line: `OVERLAYVIEW GATE OK` or `OVERLAYVIEW GATE FAILED`.
#include <windows.h>

#include "features/vkframe/vkpresent.h"

#include <QColor>
#include <QCoreApplication>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLPaintDevice>
#include <QPainter>
#include <QPainterPath>
#include <QSurfaceFormat>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
int fails = 0;

void check( bool ok, const char * name, const std::string & said )
{
	std::printf( "OVERLAYVIEW %s %s%s%s\n", name, ok ? "OK" : "FAIL", said.empty() ? "" : ": ", said.c_str() );
	if ( !ok )
		fails++;
}

void * glProc( const char * name )
{
	QOpenGLContext * c = QOpenGLContext::currentContext();
	return c ? reinterpret_cast<void *>( c->getProcAddress( name ) ) : nullptr;
}

GLuint gFbo = 0;

class FboDevice : public QOpenGLPaintDevice
{
public:
	explicit FboDevice( const QSize & s ) : QOpenGLPaintDevice( s ) {}
	void ensureActiveTarget() override
	{
		QOpenGLContext::currentContext()->functions()->glBindFramebuffer( GL_FRAMEBUFFER, gFbo );
	}
};

// the overlay: logical coordinates (the device's DPR scales them), what glview.cpp's QPainter block draws
void paintOverlay( QPainter & p, const QSize & logical )
{
	p.setRenderHint( QPainter::Antialiasing, true );
	p.setRenderHint( QPainter::TextAntialiasing, true );
	p.fillRect( QRect( 8, 6, 190, 34 ), QColor( 0, 0, 0, 140 ) );  // the F3 panel's backing
	QFont f( QStringLiteral( "Arial" ) );
	f.setPixelSize( 13 );
	p.setFont( f );
	p.setPen( QColor( 255, 220, 40 ) );
	p.drawText( QPoint( 12, 20 ), QStringLiteral( "F3 61.2 fps  gl 4.56 + vk 1.20 ms" ) );
	p.setPen( QColor( 120, 255, 160, 200 ) );
	p.drawText( QPoint( 12, 35 ), QStringLiteral( "opaque 2.10  sorted 0.31  tone 0.22" ) );
	// the gizmo: three antialiased axes and a ring
	const QPointF c( logical.width() - 50.0, logical.height() - 50.0 );
	p.setPen( QPen( QColor( 230, 40, 40 ), 2.0 ) );
	p.drawLine( c, c + QPointF( 34, 6 ) );
	p.setPen( QPen( QColor( 40, 210, 60 ), 2.0 ) );
	p.drawLine( c, c + QPointF( -4, -36 ) );
	p.setPen( QPen( QColor( 50, 90, 240 ), 2.0 ) );
	p.drawLine( c, c + QPointF( -26, 22 ) );
	p.setPen( QPen( QColor( 255, 255, 255, 110 ), 1.5 ) );
	p.drawEllipse( c, 30.0, 30.0 );
	// a label with a translucent blue box (straight alpha would differ here)
	p.fillRect( QRectF( 40.5, logical.height() - 40.0, 90.0, 18.0 ), QColor( 30, 60, 255, 96 ) );
	p.setPen( QColor( 255, 255, 255 ) );
	p.drawText( QPointF( 44.0, logical.height() - 27.0 ), QStringLiteral( "Label 07" ) );
}

std::vector<std::uint8_t> backdrop( int w, int h )
{
	std::vector<std::uint8_t> b( size_t( w ) * size_t( h ) * 4 );
	for ( int y = 0; y < h; y++ )
		for ( int x = 0; x < w; x++ ) {
			std::uint8_t * d = &b[( size_t( y ) * size_t( w ) + size_t( x ) ) * 4];
			d[0] = std::uint8_t( 40 + ( x * 160 ) / std::max( w - 1, 1 ) );
			d[1] = std::uint8_t( 30 + ( y * 180 ) / std::max( h - 1, 1 ) );
			d[2] = std::uint8_t( ( ( x / 16 + y / 16 ) & 1 ) ? 170 : 90 );
			d[3] = 255;
		}
	return b;
}

// CPU reference: Q (top row first) over B (bottom row first), 8-bit unorm, round to nearest
std::vector<std::uint8_t> reference( const std::vector<std::uint8_t> & b, const QImage & q, int & ink )
{
	const int w = q.width(), h = q.height();
	std::vector<std::uint8_t> out = b;
	ink = 0;
	for ( int y = 0; y < h; y++ ) {
		const std::uint32_t * row = reinterpret_cast<const std::uint32_t *>( q.constScanLine( y ) );
		for ( int x = 0; x < w; x++ ) {
			const std::uint32_t p = row[x];
			if ( !p )
				continue;
			ink++;
			const float s[4] = { float( ( p >> 16 ) & 255u ) / 255.0f, float( ( p >> 8 ) & 255u ) / 255.0f, float( p & 255u ) / 255.0f,
				float( p >> 24 ) / 255.0f };
			std::uint8_t * d = &out[( size_t( h - 1 - y ) * size_t( w ) + size_t( x ) ) * 4];
			for ( int c = 0; c < 4; c++ ) {
				const float o = s[c] + float( d[c] ) / 255.0f * ( 1.0f - s[3] );
				d[c] = std::uint8_t( std::lround( std::min( std::max( o, 0.0f ), 1.0f ) * 255.0f ) );
			}
		}
	}
	return out;
}

struct Cmp
{
	int compared = 0, over1 = 0, inkFree = 0, maxd = 0;
};

Cmp compare( const std::vector<std::uint8_t> & v, const std::vector<std::uint8_t> & c, const std::vector<std::uint8_t> & b )
{
	Cmp r;
	for ( size_t i = 0; i < v.size(); i += 4 ) {
		int d = 0;
		bool same = true;
		for ( int k = 0; k < 4; k++ ) {
			d = std::max( d, std::abs( int( v[i + k] ) - int( c[i + k] ) ) );
			same = same && c[i + k] == b[i + k];
		}
		r.maxd = std::max( r.maxd, d );
		if ( same ) {
			if ( d != 0 )
				r.inkFree++;  // no ink here: V must keep B exactly
		} else {
			r.compared++;
		}
		if ( d > 1 )
			r.over1++;
	}
	return r;
}

struct Box
{
	int x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1, n = 0;
};

Box changed( const std::vector<std::uint8_t> & a, const std::vector<std::uint8_t> & b, int w, std::vector<char> & mask )
{
	Box r;
	mask.assign( a.size() / 4, 0 );
	for ( size_t i = 0; i < a.size(); i += 4 )
		if ( std::memcmp( &a[i], &b[i], 3 ) != 0 ) {
			const int px = int( i / 4 );
			const int x = px % w, y = px / w;
			mask[size_t( px )] = 1;
			r.n++;
			r.x0 = std::min( r.x0, x );
			r.x1 = std::max( r.x1, x );
			r.y0 = std::min( r.y0, y );
			r.y1 = std::max( r.y1, y );
		}
	return r;
}

void arm( QOpenGLExtraFunctions * f, int w, int h, qreal dpr )
{
	char tag[64];
	std::snprintf( tag, sizeof( tag ), "dpr%g", double( dpr ) );
	const std::string t = tag;
	const QSize size( w, h );
	const QSize logical( int( w / dpr ), int( h / dpr ) );
	const std::vector<std::uint8_t> b = backdrop( w, h );

	// G: GL's QPainter over B in an FBO
	GLuint tex = 0;
	f->glGenTextures( 1, &tex );
	f->glBindTexture( GL_TEXTURE_2D, tex );
	f->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, b.data() );
	f->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	f->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	f->glGenFramebuffers( 1, &gFbo );
	f->glBindFramebuffer( GL_FRAMEBUFFER, gFbo );
	f->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0 );
	/* the view's D24S8 (run 1 had none: GL's QPainter strokes the ellipse through the stencil, and without one it filled
	 * the ring's whole hull: gizmo zone gl 3865 px vs vk 800, every gl-only pixel 3..90 levels off B) */
	GLuint ds = 0;
	f->glGenRenderbuffers( 1, &ds );
	f->glBindRenderbuffer( GL_RENDERBUFFER, ds );
	f->glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h );
	f->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, ds );
	f->glClear( GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	check( f->glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE, ( t + "-gl-fbo" ).c_str(), "" );
	std::vector<std::uint8_t> g( b.size() );
	{
		FboDevice dev( size );
		dev.setDevicePixelRatio( dpr );
		QPainter p( &dev );
		paintOverlay( p, logical );
		p.end();
		std::printf( "OVERLAYVIEW %s gl device: logical dpi %d x %d, dpr %g, %d x %d px\n", t.c_str(), dev.logicalDpiX(),
			dev.logicalDpiY(), double( dev.devicePixelRatio() ), w, h );
	}
	f->glBindFramebuffer( GL_FRAMEBUFFER, gFbo );
	f->glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	f->glReadPixels( 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, g.data() );
	f->glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	f->glDeleteFramebuffers( 1, &gFbo );
	f->glDeleteRenderbuffers( 1, &ds );
	f->glDeleteTextures( 1, &tex );

	// Q: the same calls into the app's image
	QImage q( size, QImage::Format_ARGB32_Premultiplied );
	q.fill( Qt::transparent );
	q.setDevicePixelRatio( dpr );
	{
		QPainter p( &q );
		paintOverlay( p, logical );
		p.end();
	}
	std::printf( "OVERLAYVIEW %s image: logical dpi %d x %d, dpr %g\n", t.c_str(), q.logicalDpiX(), q.logicalDpiY(),
		double( q.devicePixelRatio() ) );
	int ink = 0;
	const std::vector<std::uint8_t> c = reference( b, q, ink );
	const std::uint32_t * bits = reinterpret_cast<const std::uint32_t *>( q.constBits() );
	const int stride = int( q.bytesPerLine() / 4 );

	// green: V vs C
	std::string why;
	std::vector<std::uint8_t> v;
	const bool ran = WwVkPresent::overlayBlendTest( b, w, h, bits, stride, 0, v, why );
	check( ran, ( t + "-blend-ran" ).c_str(), why );
	if ( !ran )
		return;
	const Cmp m = compare( v, c, b );
	char said[256];
	std::snprintf( said, sizeof( said ), "ink %d px, compared %d px, over 1 level %d, max %d, ink-free changed %d (bars: compared > 0, "
		"over 1 = 0, ink-free = 0)", ink, m.compared, m.over1, m.maxd, m.inkFree );
	check( ink > 0 && m.compared > 0 && m.over1 == 0 && m.inkFree == 0, ( t + "-vulkan-vs-cpu" ).c_str(), said );

	// V vs G: placement, the flip and the coverage against GL's own QPainter
	std::vector<char> mv, mg;
	const Box bv = changed( v, b, w, mv ), bg = changed( g, b, w, mg );
	int inter = 0, uni = 0;
	long long sum = 0;
	int maxd = 0;
	for ( size_t i = 0; i < mv.size(); i++ ) {
		if ( mv[i] && mg[i] )
			inter++;
		if ( mv[i] || mg[i] ) {
			uni++;
			for ( int k = 0; k < 3; k++ ) {
				const int d = std::abs( int( v[i * 4 + k] ) - int( g[i * 4 + k] ) );
				sum += d;
				maxd = std::max( maxd, d );
			}
		}
	}
	const double iou = uni ? double( inter ) / double( uni ) : 0.0;
	const int edge = std::max( std::max( std::abs( bv.x0 - bg.x0 ), std::abs( bv.x1 - bg.x1 ) ),
		std::max( std::abs( bv.y0 - bg.y0 ), std::abs( bv.y1 - bg.y1 ) ) );
	std::snprintf( said, sizeof( said ), "changed vk %d px box %d,%d-%d,%d / gl %d px box %d,%d-%d,%d; edge %d (bar 2), IoU %.4f "
		"(bar 0.80); over the union %d px: mean %.3f, max %d levels (reported)", bv.n, bv.x0, bv.y0, bv.x1, bv.y1, bg.n, bg.x0,
		bg.y0, bg.x1, bg.y1, edge, iou, uni, uni ? double( sum ) / double( uni * 3 ) : 0.0, maxd );
	check( bv.n > 0 && bg.n > 0 && edge <= 2 && iou >= 0.80, ( t + "-vulkan-vs-gl-qpainter" ).c_str(), said );
	{	// where the two engines differ (reported, no bar): image rows top first; the F3 panel, the label, the gizmo
		const int lw = logical.width(), lh = logical.height();
		const struct
		{
			const char * name;
			int x0, y0, x1, y1;
		} zone[] = { { "f3-panel", 0, 0, 210, 45 }, { "label", 30, lh - 50, 140, lh - 15 }, { "gizmo", lw - 100, lh - 100, lw, lh } };
		for ( const auto & z : zone ) {
			int nv = 0, ng = 0, both = 0, glOnlyFaint = 0, glOnlyMax = 0;
			for ( int y = int( z.y0 * dpr ); y < int( z.y1 * dpr ) && y < h; y++ )
				for ( int x = int( z.x0 * dpr ); x < int( z.x1 * dpr ) && x < w; x++ ) {
					const size_t i = size_t( h - 1 - y ) * size_t( w ) + size_t( x );
					nv += mv[i];
					ng += mg[i];
					both += mv[i] && mg[i];
					if ( mg[i] && !mv[i] ) {
						int d = 0;
						for ( int k = 0; k < 3; k++ )
							d = std::max( d, std::abs( int( g[i * 4 + k] ) - int( b[i * 4 + k] ) ) );
						glOnlyMax = std::max( glOnlyMax, d );
						glOnlyFaint += d <= 2;
					}
				}
			std::printf( "OVERLAYVIEW %s zone %s: changed vk %d, gl %d, both %d; gl only: %d within 2 levels of B, max %d levels\n",
				t.c_str(), z.name, nv, ng, both, glOnlyFaint, glOnlyMax );
		}
	}

	// reds: each must FAIL the V vs C bar
	const struct
	{
		const char * name;
		std::uint32_t red;
		bool drop;
	} reds[] = { { "red-no-flip", 1u, false }, { "red-rb-swapped", 2u, false }, { "red-straight-alpha", 4u, false },
		{ "red-dropped", 0u, true } };
	for ( const auto & r : reds ) {
		std::vector<std::uint8_t> rv;
		std::string rw;
		const bool rr = WwVkPresent::overlayBlendTest( b, w, h, r.drop ? nullptr : bits, stride, r.red, rv, rw );
		const Cmp rm = rr ? compare( rv, c, b ) : Cmp {};
		const bool passes = rr && rm.compared > 0 && rm.over1 == 0 && rm.inkFree == 0;
		std::snprintf( said, sizeof( said ), "ran %d, over 1 level %d, max %d, ink-free changed %d: %s", rr ? 1 : 0, rm.over1, rm.maxd,
			rm.inkFree, passes ? "PASSES the bar (blind)" : "fails the bar" );
		check( rr && !passes, ( t + "-" + r.name ).c_str(), said );
	}
	const WwVkPresent::OverlayStat st = WwVkPresent::takeOverlayStat();
	std::printf( "OVERLAYVIEW %s stat: drawn %d, empty %d, lost %d, refused %d, box %dx%d, scan %.3f ms\n", t.c_str(), st.drawn,
		st.empty, st.lost, st.refused, st.boxW, st.boxH, st.scanMs );
}
}  // namespace

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	// the pipeline cache path cannot be made with this name: nothing is written there
	QCoreApplication::setOrganizationName( QStringLiteral( "ww|overlayview" ) );
	QCoreApplication::setApplicationName( QStringLiteral( "ww|overlayview" ) );
	QGuiApplication app( argc, argv );
	QSurfaceFormat fmt;
	fmt.setVersion( 4, 5 );
	fmt.setProfile( QSurfaceFormat::CompatibilityProfile );
	QOffscreenSurface surf;
	surf.setFormat( fmt );
	surf.create();
	QOpenGLContext ctx;
	ctx.setFormat( fmt );
	if ( !ctx.create() || !ctx.makeCurrent( &surf ) ) {
		std::printf( "OVERLAYVIEW GATE FAILED (no GL 4.5 context)\n" );
		return 2;
	}
	QOpenGLExtraFunctions * f = ctx.extraFunctions();
	std::printf( "GL on %s\n", reinterpret_cast<const char *>( f->glGetString( GL_RENDERER ) ) );
	std::string why;
	WwVkPresent::Device p;
	check( WwVkPresent::shareDevice( glProc, p, why ), "present-device-up", why );
	check( WwVkPresent::overlayReady( why ), "overlay-pipeline", why );
	arm( f, 320, 200, 1.0 );
	arm( f, 640, 400, 2.0 );
	std::printf( "OVERLAYVIEW %d checks failed\n", fails );
	std::printf( fails == 0 ? "OVERLAYVIEW GATE OK\n" : "OVERLAYVIEW GATE FAILED\n" );
	return fails == 0 ? 0 : 1;
}
