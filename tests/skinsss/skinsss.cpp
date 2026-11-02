// lane SKINSSS1: src/gl/wwskinsss.h (the twin of res/shaders/cell_skinsss.frag) against an independent reading of
// FO4CS's separable subsurface scattering (ExtractCS, BlurXCS / BlurYCS, the host 5-Gaussian kernel), the design gates
// (the terminator softens, red bleeds furthest, non-skin and depth edges keep their light), and pins of the shader and
// C++ text. usage: skinsss <tree root>; rc 0 = all pass. Built with -DWWSKINSSS_RED it must fail (tests/skinsss/skinsss.sh).
#include "gl/wwskinsss.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace WwSkinSss;

static int fails = 0;
static void check( bool ok, const char * what, double v = 0.0 )
{
	std::printf( "%s %s %.6g\n", ok ? "ok  " : "FAIL", what, v );
	if ( !ok )
		fails++;
}

static std::string readAll( const std::string & p )
{
	std::ifstream f( p, std::ios::binary );
	std::stringstream s;
	s << f.rdbuf();
	return s.str();
}

static size_t countOf( const std::string & s, const std::string & k )
{
	size_t n = 0;
	for ( size_t at = s.find( k ); at != std::string::npos; at = s.find( k, at + k.size() ) )
		n++;
	return n;
}

// ---- the independent reading, in double, written from the FO4CS sources (notes/skinsss1/tmp/fo4cs)
struct D4
{
	double r = 0, g = 0, b = 0, a = 0;
};

// SubsurfaceScattering.cpp SubsurfaceScatteringProfileWeight, Skin profile; range 2 (kSubsurfaceScatteringProfileRange)
static void indWeight( double nr, double out[3] )
{
	const double fall[3] = { 1.0, 0.37, 0.30 };
	const double w[5] = { 0.100, 0.118, 0.113, 0.358, 0.078 }, v[5] = { 0.0484, 0.187, 0.567, 1.99, 7.41 };
	const double r = std::min( std::max( nr, 0.0 ), 1.0 ) * 2.0;
	for ( int c = 0; c < 3; c++ ) {
		const double rr = r / ( 0.001 + fall[c] );
		double s = 0.0;
		for ( int g = 0; g < 5; g++ )
			s += w[g] * std::exp( -rr * rr / ( 2.0 * v[g] ) ) / ( 2.0 * 3.14159265358979 * v[g] );
		out[c] = s;
	}
}

struct Img
{
	int w = 0, h = 0;
	std::vector<D4> px;
	D4 & at( int x, int y ) { return px[size_t( y * w + x )]; }
};

// ExtractCS: w = material weight x saturate( amount ); rgb = Saturate( light, saturation ); skin profile 0.65 / 1.08
static Img indExtract( const Img & lit, const std::vector<int> & skin )
{
	Img o = lit;
	for ( size_t i = 0; i < lit.px.size(); i++ ) {
		const D4 c = lit.px[i];
		const double wt = skin[i] ? 0.65 : 0.0;
		if ( wt <= 0.0 ) {
			o.px[i] = D4();
			continue;
		}
		const double l = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
		o.px[i] = { std::max( l + ( c.r - l ) * 1.08, 0.0 ), std::max( l + ( c.g - l ) * 1.08, 0.0 ), std::max( l + ( c.b - l ) * 1.08, 0.0 ), wt };
	}
	return o;
}

// BlurXCS / BlurYCS: taps -r..r clamped to the image, weight = kernel[|t|] x depthWeight x sample.a x (same class)
static Img indBlur( const Img & in, const std::vector<double> & depth, const std::vector<int> & skin, int radius, bool alongX )
{
	Img o = in;
	for ( int y = 0; y < in.h; y++ )
		for ( int x = 0; x < in.w; x++ ) {
			const D4 c = in.px[size_t( y * in.w + x )];
			if ( c.a <= 0.0 ) {
				o.at( x, y ) = D4();
				continue;
			}
			const double dc = depth[size_t( y * in.w + x )];
			double sum[3] = { 0, 0, 0 }, ws[3] = { 0, 0, 0 };
			for ( int t = -radius; t <= radius; t++ ) {
				const int sx = alongX ? std::min( std::max( x + t, 0 ), in.w - 1 ) : x;
				const int sy = alongX ? y : std::min( std::max( y + t, 0 ), in.h - 1 );
				const size_t j = size_t( sy * in.w + sx );
				const D4 s = in.px[j];
				double k[3];
				indWeight( double( std::abs( t ) ) / double( radius ), k );
				const double dw = std::min( std::max( 1.0 - std::fabs( depth[j] - dc ) / 0.003 * 1.0, 0.0 ), 1.0 );
				const double cls = skin[j] == skin[size_t( y * in.w + x )] ? 1.0 : 0.0;
				const double sv[3] = { s.r, s.g, s.b };
				for ( int ch = 0; ch < 3; ch++ ) {
					const double wt = k[ch] * dw * s.a * cls;
					sum[ch] += sv[ch] * wt;
					ws[ch] += wt;
				}
			}
			const double cv[3] = { c.r, c.g, c.b };
			double r[3];
			for ( int ch = 0; ch < 3; ch++ )
				r[ch] = ws[ch] > 1e-5 ? sum[ch] / ws[ch] : cv[ch];
			o.at( x, y ) = { r[0], r[1], r[2], c.a };
		}
	return o;
}

// the composite on a forward frame (RESULTS.md ruling): lit + ( scattered - lit ) x w, w = the receiver's coverage
static Img indComposite( const Img & lit, const Img & s )
{
	Img o = lit;
	for ( size_t i = 0; i < lit.px.size(); i++ ) {
		const D4 l = lit.px[i], c = s.px[i];
		if ( c.a <= 0.0 )
			continue;
		o.px[i] = { l.r + ( c.r - l.r ) * c.a, l.g + ( c.g - l.g ) * c.a, l.b + ( c.b - l.b ) * c.a, l.a };
	}
	return o;
}

// ---- the twin over the same image (the GL pass's order: extract, blur x, blur y, composite)
static Img twinRun( const Img & lit, const std::vector<double> & depthD, const std::vector<int> & skin )
{
	const int W = lit.w, H = lit.h, R = kernelRadius();
	V3 k[kMaxTaps];
	kernel( k, R );
	std::vector<Px> e( static_cast<size_t>( W * H ) ), bx( e.size() ), by( e.size() );
	std::vector<float> depth( depthD.begin(), depthD.end() );
	for ( size_t i = 0; i < e.size(); i++ )
		e[i] = extract( { float( lit.px[i].r ), float( lit.px[i].g ), float( lit.px[i].b ) }, float( skin[i] ) );
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ )
			bx[size_t( y * W + x )] = blur( &e[size_t( y * W )], &depth[size_t( y * W )], W, x, k, R );
	std::vector<Px> col( static_cast<size_t>( H ) );
	std::vector<float> dcol( static_cast<size_t>( H ) );
	for ( int x = 0; x < W; x++ ) {
		for ( int y = 0; y < H; y++ ) {
			col[size_t( y )] = bx[size_t( y * W + x )];
			dcol[size_t( y )] = depth[size_t( y * W + x )];
		}
		for ( int y = 0; y < H; y++ )
			by[size_t( y * W + x )] = blur( col.data(), dcol.data(), H, y, k, R );
	}
	Img o = lit;
	for ( size_t i = 0; i < e.size(); i++ ) {
		if ( by[i].a <= 0.0f )
			continue;	// the shader discards: the frame keeps the sample
		const V3 c = composite( { float( lit.px[i].r ), float( lit.px[i].g ), float( lit.px[i].b ) }, by[i] );
		o.px[i] = { c.r, c.g, c.b, lit.px[i].a };
	}
	return o;
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const std::string root = argc > 1 ? argv[1] : ".";

	// 1. the kernel: radius ceil( 3 x 2 ) = 6 at full resolution, and the 5-Gaussian table per |offset|
	const int R = kernelRadius();
	check( R == 6, "kernel radius (FO4CS blurRadius 3 half-res work pixels at full resolution)", R );
	{
		V3 k[kMaxTaps];
		kernel( k, R );
		double worst = 0.0;
		for ( int i = 0; i <= R; i++ ) {
			double d[3];
			indWeight( double( i ) / double( R ), d );
			const double t[3] = { k[i].r, k[i].g, k[i].b };
			for ( int c = 0; c < 3; c++ )
				worst = std::max( worst, std::fabs( t[c] - d[c] ) / std::max( d[c], 1e-12 ) );
		}
		check( worst < 1e-5, "kernel = FO4CS's 5-Gaussian (worst relative error)", worst );
		bool zero = true;
		for ( int i = R + 1; i < kMaxTaps; i++ )
			zero = zero && k[i].r == 0.0f && k[i].g == 0.0f && k[i].b == 0.0f;
		check( zero, "kernel zero past the radius" );
		check( k[R].r / k[0].r > 3.0 * ( k[R].b / k[0].b ), "red bleeds furthest (edge/centre ratio red > 3 x blue)",
			double( k[R].r / k[0].r ) / double( k[R].b / k[0].b ) );
	}

	// 2. a scene: a skin face (left half, near) with a hard shadow terminator, a non-skin wall (right), and a depth step
	// inside the skin (a chin over a neck) 0.01 apart in window depth
	const int W = 40, H = 24;
	Img lit;
	lit.w = W;
	lit.h = H;
	lit.px.resize( size_t( W * H ) );
	std::vector<double> depth( lit.px.size() );
	std::vector<int> skin( lit.px.size() );
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ ) {
			const size_t i = size_t( y * W + x );
			const bool s = x < 24;
			skin[i] = s ? 1 : 0;
			const bool litSide = x < 12;
			lit.px[i] = s ? ( litSide ? D4{ 0.80, 0.55, 0.45, 1.0 } : D4{ 0.08, 0.05, 0.04, 1.0 } ) : D4{ 0.30, 0.30, 0.32, 1.0 };
			depth[i] = s ? ( y < 12 ? 0.9500 : 0.9600 ) : 0.9700 + 0.0001 * x;
		}
	const Img ind = indComposite( lit, indBlur( indBlur( indExtract( lit, skin ), depth, skin, R, true ), depth, skin, R, false ) );
	const Img twn = twinRun( lit, depth, skin );
	{
		double worst = 0.0;
		for ( size_t i = 0; i < lit.px.size(); i++ )
			worst = std::max( { worst, std::fabs( ind.px[i].r - twn.px[i].r ), std::fabs( ind.px[i].g - twn.px[i].g ),
				std::fabs( ind.px[i].b - twn.px[i].b ) } );
		check( worst < 1e-5, "twin = independent FO4CS reading over the scene (worst abs error)", worst );
	}
	{
		bool same = true;
		for ( size_t i = 0; i < lit.px.size(); i++ )
			if ( !skin[i] )
				same = same && twn.px[i].r == lit.px[i].r && twn.px[i].g == lit.px[i].g && twn.px[i].b == lit.px[i].b;
		check( same, "non-skin pixels keep the frame exactly" );
	}
	{
		// the first shadowed skin pixel takes light across the terminator: red most (the warm bleed)
		const size_t i = size_t( 5 * W + 12 );
		const double dr = twn.px[i].r - lit.px[i].r, db = twn.px[i].b - lit.px[i].b;
		check( dr > 0.05, "the terminator softens: shadowed skin next to the light gains red", dr );
		check( dr > 2.0 * db, "the bleed is warm (red gain > 2 x blue gain)", db > 0 ? dr / db : 99.0 );
		const size_t j = size_t( 5 * W + 11 );
		check( lit.px[j].r - twn.px[j].r > 0.05, "the lit side next to the terminator loses light", lit.px[j].r - twn.px[j].r );
	}
	{
		// the depth step (0.01 > the 0.003 threshold): a pixel just above it takes nothing from below; compare against
		// the same scene with the lower half's light changed (an edge leak would move it)
		Img lit2 = lit;
		for ( int y = 12; y < H; y++ )
			for ( int x = 0; x < 24; x++ )
				lit2.at( x, y ) = D4{ 0.02, 0.9, 0.02, 1.0 };
		const Img t2 = twinRun( lit2, depth, skin );
		double moved = 0.0;
		for ( int x = 0; x < 24; x++ ) {
			const size_t i = size_t( 11 * W + x );
			moved = std::max( { moved, std::fabs( t2.px[i].r - twn.px[i].r ), std::fabs( t2.px[i].g - twn.px[i].g ) } );
		}
		check( moved == 0.0, "no light crosses a depth step above the threshold", moved );
	}
	{
		// a flat skin field: the blur is the identity, the composite is lit + ( saturate( lit ) - lit ) x 0.65
		Img f;
		f.w = 16;
		f.h = 16;
		f.px.assign( 256, D4{ 0.6, 0.4, 0.3, 1.0 } );
		const std::vector<double> d( 256, 0.5 );
		const std::vector<int> sk( 256, 1 );
		const Img t = twinRun( f, d, sk );
		const double l = 0.2126 * 0.6 + 0.7152 * 0.4 + 0.0722 * 0.3, want = 0.6 + ( l + ( 0.6 - l ) * 1.08 - 0.6 ) * 0.65;
		check( std::fabs( t.px[8 * 16 + 8].r - want ) < 1e-5, "flat field: the saturation x amount only", t.px[8 * 16 + 8].r - want );
	}

	// 3. pins: the shader and C++ text carry the twin's law and the wiring
	{
		const std::string fs = readAll( root + "/res/shaders/cell_skinsss.frag" );
		const std::string pg = readAll( root + "/res/shaders/cell_skinsss.prog" );
		const std::string fd = readAll( root + "/res/shaders/fo4_default.frag" );
		const std::string rd = readAll( root + "/src/gl/renderer.cpp" );
		const std::string cl = readAll( root + "/src/gl/celllights.cpp" );
		const std::string cs = readAll( root + "/src/gl/cellskinsss.cpp" );
		const std::string gc = readAll( root + "/src/gl/glcontext.cpp" );
		const std::string gv = readAll( root + "/src/glview.cpp" );
		const std::string lk = readAll( root + "/src/gl/celllooks.cpp" );
		const std::string tw = readAll( root + "/src/gl/wwskinsss.h" );
		check( pg.find( "cell_ao.vert cell_skinsss.frag" ) != std::string::npos, "pin: program" );
		check( fs.find( "float l = dot( c, vec3( 0.2126, 0.7152, 0.0722 ) );" ) != std::string::npos, "pin: shader luma" );
		check( fs.find( "return clamp( 1.0 - abs( ds - dc ) / sssParams.z * max( sssParams.w, 0.0 ), 0.0, 1.0 );" ) != std::string::npos,
			"pin: shader depth weight" );
		check( fs.find( "vec3 k = sssKernel[abs( t )] * ( sssDepthWeight( dc, texelFetch( depthTex, q, 0 ).r ) * s.a );" ) != std::string::npos,
			"pin: shader tap weight" );
		check( fs.find( "ws.r > 1e-5 ? sum.r / ws.r : c.r" ) != std::string::npos, "pin: shader centre fallback" );
		check( fs.find( "fragColor = vec4( sssRed ? lit.rgb : lit.rgb + ( o - lit.rgb ) * clamp( c.a, 0.0, 1.0 ), lit.a );" ) != std::string::npos,
			"pin: shader composite" );
		check( tw.find( "gw[5] = { 0.100f, 0.118f, 0.113f, 0.358f, 0.078f };" ) != std::string::npos
			&& tw.find( "gv[5] = { 0.0484f, 0.187f, 0.567f, 1.99f, 7.41f };" ) != std::string::npos
			&& tw.find( "falloff[3] = { 1.0f, 0.37f, 0.30f };" ) != std::string::npos, "pin: twin 5-Gaussian constants" );
		check( countOf( fd, "cellEmitOut = vec4( wwSkinOn ? 1.0 : 0.0 );" ) == 2, "pin: both probe-20 sites write the tag",
			double( countOf( fd, "cellEmitOut = vec4( wwSkinOn ? 1.0 : 0.0 );" ) ) );
		// lane SKINSSS3: the bindless twins write the tag too; red WW_CELL_SKINSSS_RED=nobl keeps them out
		check( fd.find( "#if defined( WW_CELL_SKINSSS ) && defined( WW_CELLLIGHTS ) && !( defined( WW_BINDLESS ) && defined( WW_SKINSSS_RED_NOBL ) )" )
				!= std::string::npos, "pin: the tag in the cell programs and their bindless twins (red nobl)" );
		check( gc.find( "if ( wwEnv( \"WW_CELL_SKINSSS_RED\" ).trimmed() == \"nobl\" )" ) != std::string::npos
			&& gc.find( "\"#define WW_SKINSSS_RED_NOBL 1\\n\"" ) > gc.find( "\"#define WW_CELL_SKINSSS 1\\n\"" ), "pin: loader red nobl inside the row's define" );
		{
			// wwSkinOn is set while the draw packet records, so two draws that differ in it have different packets, and the
			// multi-draw join (sameButHandles) never puts a skin and a non-skin draw under one uniform value
			const std::string dp = readAll( root + "/src/gl/celldrawpacket.cpp" );
			const std::string bs = readAll( root + "/src/gl/bsshape.cpp" );
			const size_t pb = rd.find( "WwDp::begin( *dpSlot, prog );" ), pe = rd.find( "WwDp::end( true, texunit );" );
			const size_t ps = rd.find( "prog->uni1b( \"wwSkinOn\"," );
			check( pb != std::string::npos && pe != std::string::npos && ps != std::string::npos && pb < ps && ps < pe,
				"pin: wwSkinOn is set inside the packet's recording" );
			check( dp.find( "return std::equal( a.data.begin() + from, a.data.end(), b.data.begin() + from );" ) != std::string::npos
				&& dp.find( "if ( x.kind != y.kind || x.a != y.a || x.b != y.b || x.off != y.off )" ) != std::string::npos,
				"pin: the join compares every recorded value but the handles" );
			check( bs.find( "!WwDp::sameButHandles( *lp, *fp )" ) != std::string::npos, "pin: the multi-draw join asks sameButHandles" );
		}
		check( rd.find( "( bgsm && ( bgsm->bSkinTint || bgsm->bFacegen ) )" ) != std::string::npos
			&& rd.find( "lsp->isST( ShaderFlags::ST_SkinTint ) || lsp->isST( ShaderFlags::ST_Facegen )" ) != std::string::npos, "pin: skin gate" );
		check( cl.find( "GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g.skinTex, 0" ) != std::string::npos
			&& cl.find( "r->fn->glColorMaski( 1, sk, sk, sk, sk );" ) != std::string::npos, "pin: opaque pass attachment 1 + mask" );
		check( gc.find( "\"#define WW_CELL_SKINSSS 1\\n\"" ) != std::string::npos, "pin: loader define" );
		check( gv.find( "wwCellSkinSssApply( scene );" ) != std::string::npos
			&& gv.find( "wwCellSkinSssApply( scene );" ) < gv.find( "wwCellPostApplyLinear( scene );" ), "pin: before the linear post chain" );
		check( cs.find( "wwEnv( \"WW_CELL_SKINSSS_RED\" ).trimmed() == \"1\"" ) != std::string::npos, "pin: live red" );
		check( lk.find( "{ \"Skin subsurface\", \"WW/CellSkinSubsurface\" }," ) != std::string::npos, "pin: row ships off" );
		// lane HAIRDLC04: cell facegen shapes name no BGSM and the cell writer forces Shader Type 0, so the source type
		// (4 face, 5 skin, 6 hair) is carried to the property as extra data and read by the skin and hair gates
		const std::string lg = readAll( root + "/src/lodgen.cpp" );
		const std::string cv = readAll( root + "/src/cellview.cpp" );
		const std::string gp = readAll( root + "/src/gl/glproperty.cpp" );
		check( lg.find( "s.srcShaderType = src.get<quint32>( iShader, \"Shader Type\" );" ) != std::string::npos
			&& lg.find( "n.srcShaderType = s.srcShaderType;" ) != std::string::npos, "pin: loader keeps the source Shader Type" );
		// SKINSSS2 (EYECORNEA-CELL) added the eye (16) to the kept types; this pin was stale at 510b5d92 (fixed in SKINSSS3)
		check( cv.find( "( ( s.srcShaderType >= 4u && s.srcShaderType <= 6u ) || s.srcShaderType == 16u ) ? s.srcShaderType : 0u" ) != std::string::npos
			&& cv.find( "QStringLiteral( \"|T%1\" ).arg( srcSt )" ) != std::string::npos, "pin: face / skin / hair keep their own bucket" );
		check( cv.find( "nif->set<QString>( iSt, \"Name\", QStringLiteral( \"WWSrcShaderType\" ) );" ) != std::string::npos
			&& cv.find( "wwEnv( \"WW_CELL_SRCST_RED\" ).trimmed() != \"1\"" ) != std::string::npos, "pin: cell writer tags the shader, red" );
		check( gp.find( "== QLatin1String( \"WWSrcShaderType\" )" ) != std::string::npos, "pin: property reads the tag" );
		check( rd.find( "|| lsp->wwSrcShaderType == int( ShaderFlags::ST_HairTint );" ) != std::string::npos, "pin: hair gate takes facegen hair" );
		check( rd.find( "|| lsp->wwSrcShaderType == int( ShaderFlags::ST_SkinTint )" ) != std::string::npos
			&& rd.find( "|| lsp->wwSrcShaderType == int( ShaderFlags::ST_Facegen ) );" ) != std::string::npos, "pin: skin gate takes facegen face + skin" );
	}

	std::printf( "%s %d failed\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
