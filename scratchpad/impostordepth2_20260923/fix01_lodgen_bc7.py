# fix01 -- lane IMPOSTORDEPTH2: the card bake writes `_n` as BC7 (per-card set and card array).
import sys
p = 'E:/Projects/NifskopeWildWastelandEdition/src/lodgen.cpp'
b = open(p, 'rb').read()
s = b.decode('utf-8')
cr0 = s.count('\r')

def rep(old, new, count=1):
    global s
    n = s.count(old)
    assert n == count, (n, old[:120])
    s = s.replace(old, new)

# 0. include
rep('#include "lodgenparallel.h"\n', '#include "lodgenparallel.h"\n#include "lodgenbc7.h"\n')

# 1. forward declaration
rep('''	const std::vector<quint32> & bgra, bool bc3 = false, int maxMips = 0,
	bool bc1Alpha = false, quint32 stamp0 = 0, quint32 stamp1 = 0,
	bool mipsToOne = false );
''', '''	const std::vector<quint32> & bgra, bool bc3 = false, int maxMips = 0,
	bool bc1Alpha = false, quint32 stamp0 = 0, quint32 stamp1 = 0,
	bool mipsToOne = false, bool bc7 = false );
''')

# 2. definition signature + mip alpha + header + block encode
rep('''bool lodgenWriteDds( const QString & path, int w, int h,
	const std::vector<quint32> & bgra, bool bc3, int maxMips, bool bc1Alpha,
	quint32 stamp0, quint32 stamp1, bool mipsToOne )
{''', '''bool lodgenWriteDds( const QString & path, int w, int h,
	const std::vector<quint32> & bgra, bool bc3, int maxMips, bool bc1Alpha,
	quint32 stamp0, quint32 stamp1, bool mipsToOne, bool bc7 )
{
	/* bc7 (lane IMPOSTORDEPTH2, 2026-09-23): the same mip chain with alpha
	 * kept, every block BC7 (src/lodgenbc7.h, weighted for a card `_n`: see
	 * kCardNormalBc7Weights), behind a DX10 header with DXGI 98
	 * (BC7_UNORM) and an array size of 1. Off, every byte is what it was. */
	if ( bc7 )
		bc3 = true;''')

rep('''	const quint32 blockBytes = bc3 ? 16 : 8;
	quint32 hdr[32] = { 0 };
	hdr[0] = 0x20534444;            // 'DDS '
	hdr[1] = 124;
	hdr[2] = 0x000A1007;            // caps|height|width|linearsize|pf|mipcount
	hdr[3] = quint32( h );
	hdr[4] = quint32( w );
	hdr[5] = quint32( ( ( w + 3 ) / 4 ) * ( ( h + 3 ) / 4 ) * blockBytes );
	hdr[7] = quint32( mips.size() );
	hdr[19] = 32;
	hdr[20] = 0x4;                  // fourCC
	hdr[21] = bc3 ? 0x35545844U : 0x31545844U;   // 'DXT5' / 'DXT1'
''', '''	const quint32 blockBytes = bc3 ? 16 : 8;
	quint32 hdr[32] = { 0 };
	hdr[0] = 0x20534444;            // 'DDS '
	hdr[1] = 124;
	hdr[2] = 0x000A1007;            // caps|height|width|linearsize|pf|mipcount
	hdr[3] = quint32( h );
	hdr[4] = quint32( w );
	hdr[5] = quint32( ( ( w + 3 ) / 4 ) * ( ( h + 3 ) / 4 ) * blockBytes );
	hdr[7] = quint32( mips.size() );
	hdr[19] = 32;
	hdr[20] = 0x4;                  // fourCC
	hdr[21] = bc7 ? 0x30315844U : bc3 ? 0x35545844U : 0x31545844U;   // 'DX10' / 'DXT5' / 'DXT1'
''')

rep('''	hdr[8] = stamp0;
	hdr[9] = stamp1;
	f.write( reinterpret_cast<const char *>( hdr ), 128 );
	for ( size_t mi = 0; mi < mips.size(); mi++ ) {''', '''	hdr[8] = stamp0;
	hdr[9] = stamp1;
	f.write( reinterpret_cast<const char *>( hdr ), 128 );
	if ( bc7 ) {
		// DDS_HEADER_DXT10: BC7_UNORM, 2D, no flags, one layer
		const quint32 dx10[5] = { 98U, 3U, 0U, 1U, 0U };
		f.write( reinterpret_cast<const char *>( dx10 ), 20 );
	}
	for ( size_t mi = 0; mi < mips.size(); mi++ ) {''')

rep('''			for ( int bx = 0; bx < bw; bx++ ) {
				quint8 * out = block.data() + ( size_t( by ) * bw + bx ) * blockBytes;
				if ( bc3 ) {''', '''			for ( int bx = 0; bx < bw; bx++ ) {
				quint8 * out = block.data() + ( size_t( by ) * bw + bx ) * blockBytes;
				if ( bc7 ) {
					lodgenEncodeBC7Block( mip.data(), mw, mh, bx, by, out );
					continue;
				}
				if ( bc3 ) {''')

# 3. the BC7 block helper, beside lodgenEncodeBC1Block (end of its namespace)
rep('''	out[4] = quint8( bits ); out[5] = quint8( bits >> 8 );
	out[6] = quint8( bits >> 16 ); out[7] = quint8( bits >> 24 );
}

} // namespace

bool lodgenWriteDds( const QString & path, int w, int h,''', '''	out[4] = quint8( bits ); out[5] = quint8( bits >> 8 );
	out[6] = quint8( bits >> 16 ); out[7] = quint8( bits >> 24 );
}

/*! The per-channel error weights (R G B A) the card `_n` sheet is encoded
 *  under: the HEIGHT is B, and the depth search reads it, so it counts 32
 *  times a normal channel. Measured on the n8_2k fixture (1,037,765 covered
 *  texels) against the bake's own PNG: height error mean 0.61 levels, p95 2,
 *  all 59 heights surviving, normals R/G mean 3.6; under DXT5 the same sheet
 *  read 2.81 / 8 / 27 of 59 and R/G 11.3. Weight 64 gave 0.49 but lost a
 *  height; weight 1 gave 1.37 (lane IMPOSTORDEPTH2, 2026-09-23). */
static const int kCardNormalBc7Weights[4] = { 1, 1, 32, 1 };

//! One BC7 block of a 0xAARRGGBB image, edge texels clamped as BC1/BC3 do.
static void lodgenEncodeBC7Block( const quint32 * px, int w, int h, int bx, int by, quint8 * out )
{
	uint8_t rgba[16][4];
	for ( int i = 0; i < 16; i++ ) {
		const int sx = qMin( bx * 4 + ( i & 3 ), w - 1 );
		const int sy = qMin( by * 4 + ( i >> 2 ), h - 1 );
		const quint32 p = px[size_t( sy ) * w + sx];
		rgba[i][0] = uint8_t( p >> 16 );
		rgba[i][1] = uint8_t( p >> 8 );
		rgba[i][2] = uint8_t( p );
		rgba[i][3] = uint8_t( p >> 24 );
	}
	LodgenBc7::encodeBlock( rgba, kCardNormalBc7Weights, out );
}

} // namespace

bool lodgenWriteDds( const QString & path, int w, int h,''')

# 4. array layer encoder
rep('''static int lodgenEncodeArrayLayer( const std::vector<quint32> & bgra, int w, int h, bool bc3,
	std::vector<quint8> & out, int maxMips = 0 )
{''', '''static int lodgenEncodeArrayLayer( const std::vector<quint32> & bgra, int w, int h, bool bc3,
	std::vector<quint8> & out, int maxMips = 0, bool bc7 = false )
{
	if ( bc7 )
		bc3 = true;   // alpha rides the mip chain; blocks are 16 bytes''')
rep('''				quint8 * o = out.data() + at + ( size_t( by ) * bw + bx ) * blockBytes;
				if ( bc3 ) {''', '''				quint8 * o = out.data() + at + ( size_t( by ) * bw + bx ) * blockBytes;
				if ( bc7 ) {
					lodgenEncodeBC7Block( mip.data(), mw, mh, bx, by, o );
					continue;
				}
				if ( bc3 ) {''')

# 5. array writer
rep('''bool lodgenWriteDdsArray( const QString & path, int w, int h,
	const std::vector<std::vector<quint32>> & layers, bool bc3, int maxMips = 0 )
{''', '''bool lodgenWriteDdsArray( const QString & path, int w, int h,
	const std::vector<std::vector<quint32>> & layers, bool bc3, int maxMips = 0, bool bc7 = false )
{
	if ( bc7 )
		bc3 = true;''')
rep('''		mips = lodgenEncodeArrayLayer( layer, w, h, bc3, data, maxMips );''',
    '''		mips = lodgenEncodeArrayLayer( layer, w, h, bc3, data, maxMips, bc7 );''')
rep('''	const quint32 dx10[5] = { bc3 ? 77U : 71U, 3U, 0U, quint32( layers.size() ), 0U };''',
    '''	const quint32 dx10[5] = { bc7 ? 98U : bc3 ? 77U : 71U, 3U, 0U, quint32( layers.size() ), 0U };''')

# 6. per-card set: `_n` is BC7, the mask stays BC3
rep('''				const struct { QString suffix; const QImage * img; } sheets[2] = {
					{ QStringLiteral( "_n.DDS" ), &nrmA }, { maskSfx, &rmA } };
				for ( const auto & s : sheets ) {
					const QString path = base + s.suffix;
					if ( !QFile::exists( path ) )
						ok = lodgenWriteDds( path, aw, ah, pixels( *s.img ), true, auxMips ) && ok;
				}''', '''				/* `_n` is BC7 (bungo's ruling, lane IMPOSTORDEPTH2 2026-09-23):
				 * its B is the card's HEIGHT, which DXT5 carried in the 5:6:5
				 * colour block and returned 2.81 levels off on average. The
				 * mask sheet keeps BC3: nothing searches it. */
				const struct { QString suffix; const QImage * img; bool bc7; } sheets[2] = {
					{ QStringLiteral( "_n.DDS" ), &nrmA, true }, { maskSfx, &rmA, false } };
				for ( const auto & s : sheets ) {
					const QString path = base + s.suffix;
					if ( !QFile::exists( path ) )
						ok = lodgenWriteDds( path, aw, ah, pixels( *s.img ), true, auxMips,
								false, 0, 0, false, s.bc7 ) && ok;
				}''')

# 7. card arrays: `_n` BC7, built from the same PNGs (invariant 2)
rep('''		const struct { QString suffix; const std::vector<std::vector<quint32>> * px; bool bc3; bool aux; } sheets[4] = {
			{ colorSfx, &g.color, true, false }, { QStringLiteral( "_n.DDS" ), &g.n, true, true },
			{ maskSfx, &g.mask, true, true },
			// the emissive is BC1: three channels and no alpha to carry
			{ emSfx, &g.emis, false, true } };''', '''		// `_n` is BC7, as the per-card set is (lane IMPOSTORDEPTH2)
		const struct { QString suffix; const std::vector<std::vector<quint32>> * px; bool bc3; bool aux; bool bc7; } sheets[4] = {
			{ colorSfx, &g.color, true, false, false }, { QStringLiteral( "_n.DDS" ), &g.n, true, true, true },
			{ maskSfx, &g.mask, true, true, false },
			// the emissive is BC1: three channels and no alpha to carry
			{ emSfx, &g.emis, false, true, false } };''')
rep('''			if ( !lodgenWriteDdsArray( fileBase + s.suffix, sw, sh, *s.px, s.bc3, sm ) )''',
    '''			if ( !lodgenWriteDdsArray( fileBase + s.suffix, sw, sh, *s.px, s.bc3, sm, s.bc7 ) )''')

# 8. the stale comment
rep(''' *  BC re-encode puts a block grid straight back. There is no BC7 encoder in
 *  this tree, so BC7 is not an option here; what uncompressed costs is in the
 *  lane report, for bungo to rule on.''', ''' *  BC re-encode puts a block grid straight back. (A BC7 encoder exists since
 *  2026-09-23, src/lodgenbc7.h, for the card `_n` sheet; BC7 is still a 4x4
 *  block format and is not used here -- whether its grid shows on this sheet
 *  is unmeasured.) What uncompressed costs is in the lane report, for bungo
 *  to rule on.''')

assert s.count('\r') == cr0, 'CR count moved'
open(p, 'wb').write(s.encode('utf-8'))
print('fix01 applied; CR', cr0)
