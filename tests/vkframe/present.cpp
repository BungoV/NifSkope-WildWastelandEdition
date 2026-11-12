// Lane VKFRAME1 (2026-11-12, PRESENT FIRST): GL's SwapBuffers against src/features/vkframe/vkpresent.cpp (built in
// here with WW_VKPRESENT_NO_QT: the shipped code, not a copy) in one window of its own, behind the others and never
// activated. A 4x MSAA GL back buffer (as the view's), swap interval 0, a light frame (the empty spot's case: the
// present's own cost shows). Usage: present <W> <H> <frames>
// Prints `PRESENT gl ...`, `PRESENT vk ...` (CPU ms of the swap / present call: median, p90; the frame period median),
// `PRESENTMATCH EXACT|FAIL` (the presented swapchain image, read back in the same submit, against glReadPixels of the
// back buffer before the present: every byte), `PRESENTMATCH RED ...` (the same compare without the y flip: must FAIL),
// and `PRESENT GATE OK|FAILED` (vk median under 0.3 ms, EXACT, the red FAILs).
#define WW_VKPRESENT_NO_QT
#include "../../src/features/vkframe/vkpresent.cpp"

#include <GL/gl.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{
HMODULE gOpenGl = nullptr;
void * glProc( const char * name )
{
	void * p = reinterpret_cast<void *>( wglGetProcAddress( name ) );
	const intptr_t i = reinterpret_cast<intptr_t>( p );
	if ( !p || i == 1 || i == 2 || i == 3 || i == -1 )
		p = reinterpret_cast<void *>( GetProcAddress( gOpenGl, name ) );
	return p;
}

LRESULT CALLBACK wndProc( HWND h, UINT m, WPARAM wp, LPARAM lp )
{
	return DefWindowProcA( h, m, wp, lp );
}

void pump()
{
	MSG msg;
	while ( PeekMessageA( &msg, nullptr, 0, 0, PM_REMOVE ) ) {
		TranslateMessage( &msg );
		DispatchMessageA( &msg );
	}
}

// a light frame with edges (the resolve has work) and no vertical symmetry (the red sees a missing flip)
void draw( int W, int H, int f )
{
	glViewport( 0, 0, W, H );
	glDisable( GL_SCISSOR_TEST );
	glClearColor( 0.1f, 0.2f, 0.3f, 1.0f );
	glClear( GL_COLOR_BUFFER_BIT );
	glEnable( GL_SCISSOR_TEST );
	for ( int i = 0; i < 16; i++ ) {
		glScissor( 0, i * H / 32, W / 2, H / 64 + 1 );
		glClearColor( i / 16.0f, 1.0f - i / 16.0f, ( f % 7 ) / 7.0f, 1.0f );
		glClear( GL_COLOR_BUFFER_BIT );
	}
	glDisable( GL_SCISSOR_TEST );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();
	glRotatef( float( f % 360 ), 0, 0, 1 );
	glBegin( GL_TRIANGLES );
	glColor3f( 1, 0, 0 ); glVertex2f( -0.6f, -0.5f );
	glColor3f( 0, 1, 0 ); glVertex2f( 0.7f, -0.3f );
	glColor3f( 0, 0, 1 ); glVertex2f( 0.1f, 0.8f );
	glEnd();
}

double median( std::vector<double> v, double q = 0.5 )
{
	if ( v.empty() )
		return 0.0;
	std::sort( v.begin(), v.end() );
	return v[std::min( v.size() - 1, size_t( q * double( v.size() ) ) )];
}
}  // namespace

int main( int argc, char ** argv )
{
	const int W = argc > 1 ? std::atoi( argv[1] ) : 1280, H = argc > 2 ? std::atoi( argv[2] ) : 720;
	const int N = argc > 3 ? std::atoi( argv[3] ) : 600;
	// the frame's ms (0: as fast as it goes). 2.0 = the 500 fps bar: an unpaced 0.2 ms frame makes the present's GPU
	// work (GL -> Vulkan, the compositor copy) the bound, which no real frame is
	const double paceMs = argc > 4 ? std::atof( argv[4] ) : 0.0;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwpresent";
	RegisterClassA( &wc );
	// a dummy context for wglChoosePixelFormatARB (a window takes one pixel format for life)
	HWND dummy = CreateWindowA( "wwpresent", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, W, H };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwpresent", "vkpresent harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0x2041, 1, 0x2042, 4, 0 };	// window, GL, double, full accel, RGBA8, D24S8, 4 samples
	int fmt = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &fmt, &nf ) || !nf ) {
		std::printf( "PRESENT GATE FAILED (no 4x MSAA pixel format)\n" );
		return 2;
	}
	PIXELFORMATDESCRIPTOR got {};
	DescribePixelFormat( dc, fmt, sizeof( got ), &got );
	SetPixelFormat( dc, fmt, &got );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	wglDeleteContext( drc );
	ReleaseDC( dummy, ddc );
	DestroyWindow( dummy );
	auto swapInterval = reinterpret_cast<BOOL( WINAPI * )( int )>( wglGetProcAddress( "wglSwapIntervalEXT" ) );
	if ( swapInterval )
		swapInterval( 0 );
	ShowWindow( wnd, SW_SHOWNOACTIVATE );
	SetWindowPos( wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	pump();
	GLint samples = 0;
	glGetIntegerv( 0x80A9, &samples );	// GL_SAMPLES
	std::printf( "window %dx%d, back buffer %d samples, swap interval 0, behind the other windows, GL on %s\n", W, H, samples,
		reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );

	// GL's swap
	std::vector<double> swapMs, glFrame;
	double prev = nowMs();
	for ( int f = 0; f < N; f++ ) {
		pump();
		draw( W, H, f );
		const double t0 = nowMs();
		SwapBuffers( dc );
		const double t1 = nowMs();
		if ( f >= 30 ) {
			swapMs.push_back( t1 - t0 );
			glFrame.push_back( t1 - prev );
		}
		prev = t1;
	}
	std::printf( "PRESENT gl SwapBuffers %.3f ms (p90 %.3f), frame %.3f ms\n", median( swapMs ), median( swapMs, 0.9 ), median( glFrame ) );

	// the picture: the back buffer, then the same frame presented and read back in its own submit
	std::string why;
	draw( W, H, 3 );
	glFinish();
	std::vector<std::uint8_t> ref( size_t( W ) * H * 4 ), cap;
	glReadBuffer( GL_BACK );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glReadPixels( 0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, ref.data() );
	WwVkPresent::captureNext( &cap );
	if ( !WwVkPresent::present( wnd, W, H, false, glProc, why ) ) {
		std::printf( "PRESENT vk FAILED: %s\nPRESENT GATE FAILED\n", why.c_str() );
		return 1;
	}
	const WwVkPresent::Stat st0 = WwVkPresent::last();
	size_t diff = 0, diffRed = 0;
	const bool sized = st0.w == W && st0.h == H && cap.size() == ref.size();
	if ( sized )
		for ( int y = 0; y < H; y++ )
			for ( int x = 0; x < W; x++ )
				for ( int c = 0; c < 3; c++ ) {
					const std::uint8_t g = ref[( size_t( H - 1 - y ) * W + x ) * 4 + c];
					const std::uint8_t gRed = ref[( size_t( y ) * W + x ) * 4 + c];
					const std::uint8_t v = cap[( size_t( y ) * W + x ) * 4 + ( 2 - c )];
					diff += g != v;
					diffRed += gRed != v;
				}
	const bool exact = sized && diff == 0, redFails = !sized || diffRed != 0;
	std::printf( "PRESENTMATCH %s (%zu of %zu bytes differ; swapchain %dx%d)\n", exact ? "EXACT" : "FAIL", diff, size_t( W ) * H * 3,
		st0.w, st0.h );
	std::printf( "PRESENTMATCH RED %s (no y flip: %zu bytes differ; must FAIL)\n", redFails ? "FAIL" : "EXACT", diffRed );

	// the Vulkan present
	std::vector<double> vkMs, vkWait, vkFrame;
	prev = nowMs();
	for ( int f = 0; f < N; f++ ) {
		while ( paceMs > 0.0 && nowMs() - prev < paceMs ) {
		}
		pump();
		draw( W, H, f );
		if ( !WwVkPresent::present( wnd, W, H, false, glProc, why ) ) {
			std::printf( "PRESENT vk FAILED at frame %d: %s\nPRESENT GATE FAILED\n", f, why.c_str() );
			return 1;
		}
		const double t1 = nowMs();
		if ( f >= 30 ) {
			vkMs.push_back( WwVkPresent::last().ms );
			vkWait.push_back( WwVkPresent::last().waitMs );
			vkFrame.push_back( t1 - prev );
		}
		prev = t1;
	}
	const WwVkPresent::Stat & st = WwVkPresent::last();
	std::printf( "PRESENT vk present %.3f ms (p90 %.3f, fence wait %.3f), frame %.3f ms, mode %s, %d images, %dx%d, builds %llu\n",
		median( vkMs ), median( vkMs, 0.9 ), median( vkWait ), median( vkFrame ), st.mode, st.images, st.w, st.h,
		static_cast<unsigned long long>( st.builds ) );
	WwVkPresent::shutdown();  // drains the worker
	const WwVkPresent::Split sp = WwVkPresent::takeSplit();
	const double sn = sp.n > 0 ? sp.n : 1;
	std::printf( "PRESENT split: drawing thread GL state %.3f + GL blit/signal %.3f + flush %.3f (%s blit); worker %d frames, "
		"%d failed, %.3f ms (max %.3f) = fence %.3f + acquire %.3f + record %.3f + submit %.3f + present %.3f\n",
		st.stateMs, st.glMs, st.flushMs, st.dsa ? "named" : "bound", sp.n, sp.failed, sp.totalMs / sn, sp.maxMs,
		sp.fenceMs / sn, sp.acquireMs / sn, sp.recordMs / sn, sp.submitMs / sn, sp.presentMs / sn );
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	const bool ok = median( vkMs ) < 0.3 && exact && redFails && sp.n >= N - 1 && sp.failed == 0;
	std::printf( "PRESENT GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}
