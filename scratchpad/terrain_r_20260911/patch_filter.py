p = 'src/lodgen.cpp'
s = open(p, 'r', encoding='utf-8', newline='').read()


def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (a[:70], s.count(a))
    s = s.replace(a, b)


rep("""void lodgenVtFilterTile( const QMap<int, std::vector<LodgenVtStage>> & childRows,
	int childTilesX, int childTilesY, int content, int border, int stored,
	int tx, int ty, LodgenVtStage & out )
{
	const int mosW = childTilesX * content, mosH = childTilesY * content;
	auto sample = [&]( int u, int v, quint32 * bgra3, quint16 * h16 ) {
		u = qBound( 0, u, mosW - 1 );
		v = qBound( 0, v, mosH - 1 );
		const int ctx = u / content, cty = v / content;
		auto row = childRows.constFind( cty );
		if ( row == childRows.constEnd() || ctx >= int( row->size() ) ) {
			bgra3[0] = 0xFF808080U;
			bgra3[1] = LODGEN_MSN_FLAT;
			bgra3[2] = 0x00FFFFFFU;
			*h16 = 32767;
			return;
		}
		const LodgenVtStage & st = ( *row )[size_t( ctx )];
		const size_t idx = size_t( border + v % content ) * size_t( stored )
			+ size_t( border + u % content );
		bgra3[0] = st.colour[idx];
		bgra3[1] = st.msn[idx];
		bgra3[2] = st.data[idx];
		*h16 = st.height[idx];
	};

	out.colour.assign( size_t( stored ) * stored, 0xFF808080U );
	out.msn.assign( size_t( stored ) * stored, LODGEN_MSN_FLAT );
	out.data.assign( size_t( stored ) * stored, 0x00FFFFFFU );
	out.height.assign( size_t( stored ) * stored, 32767 );
	out.cover = false;

	for ( int j = 0; j < stored; j++ ) {
		const int v0 = 2 * ( ty * content + j - border );
		for ( int i = 0; i < stored; i++ ) {
			const int u0 = 2 * ( tx * content + i - border );
			quint32 acc[3][4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
			quint32 hAcc = 0;
			for ( int dv = 0; dv < 2; dv++ ) {
				for ( int du = 0; du < 2; du++ ) {
					quint32 px[3];
					quint16 hv = 0;
					sample( u0 + du, v0 + dv, px, &hv );
					for ( int s = 0; s < 3; s++ ) {""",
"""void lodgenVtFilterTile( const QMap<int, std::vector<LodgenVtStage>> & childRows,
	int childTilesX, int childTilesY, int content, int border, int stored,
	int tx, int ty, bool wantEmissive, LodgenVtStage & out )
{
	const int mosW = childTilesX * content, mosH = childTilesY * content;
	/* FIVE planes now, not three: colour, msn, the retired-but-still-staged
	 * data plane (the .btr path reads it), the MASK and the EMISSIVE. Adding
	 * them to the same accumulator rather than to a second pass is what keeps
	 * the one rounding law -- (a+b+c+d+2)>>2, round-half-up -- over every
	 * sheet. */
	auto sample = [&]( int u, int v, quint32 * bgra, quint16 * h16 ) {
		u = qBound( 0, u, mosW - 1 );
		v = qBound( 0, v, mosH - 1 );
		const int ctx = u / content, cty = v / content;
		auto row = childRows.constFind( cty );
		if ( row == childRows.constEnd() || ctx >= int( row->size() ) ) {
			bgra[0] = 0xFF808080U;
			bgra[1] = LODGEN_MSN_FLAT;
			bgra[2] = 0x00FFFFFFU;
			bgra[3] = 0x00FF00FFU;
			bgra[4] = 0xFF000000U;
			*h16 = 32767;
			return;
		}
		const LodgenVtStage & st = ( *row )[size_t( ctx )];
		const size_t idx = size_t( border + v % content ) * size_t( stored )
			+ size_t( border + u % content );
		bgra[0] = st.colour[idx];
		bgra[1] = st.msn[idx];
		bgra[2] = st.data[idx];
		bgra[3] = idx < st.mask.size() ? st.mask[idx] : 0x00FF00FFU;
		bgra[4] = idx < st.emissive.size() ? st.emissive[idx] : 0xFF000000U;
		*h16 = st.height[idx];
	};

	out.colour.assign( size_t( stored ) * stored, 0xFF808080U );
	out.msn.assign( size_t( stored ) * stored, LODGEN_MSN_FLAT );
	out.data.assign( size_t( stored ) * stored, 0x00FFFFFFU );
	out.mask.assign( size_t( stored ) * stored, 0x00FF00FFU );
	if ( wantEmissive )
		out.emissive.assign( size_t( stored ) * stored, 0xFF000000U );
	out.height.assign( size_t( stored ) * stored, 32767 );
	out.cover = false;

	for ( int j = 0; j < stored; j++ ) {
		const int v0 = 2 * ( ty * content + j - border );
		for ( int i = 0; i < stored; i++ ) {
			const int u0 = 2 * ( tx * content + i - border );
			quint32 acc[5][4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
				{ 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
			quint32 hAcc = 0;
			for ( int dv = 0; dv < 2; dv++ ) {
				for ( int du = 0; du < 2; du++ ) {
					quint32 px[5];
					quint16 hv = 0;
					sample( u0 + du, v0 + dv, px, &hv );
					for ( int s = 0; s < 5; s++ ) {""")

rep("""			const quint32 cov = ( acc[2][3] + 2 ) >> 2;
			out.data[o] = ( cov << 24 ) | ( ( ( acc[2][0] + 2 ) >> 2 ) << 16 )
				| ( ( ( acc[2][1] + 2 ) >> 2 ) << 8 ) | ( ( acc[2][2] + 2 ) >> 2 );
			if ( cov )
				out.cover = true;
			out.height[o] = quint16( ( hAcc + 2 ) >> 2 );""",
"""			const quint32 cov = ( acc[2][3] + 2 ) >> 2;
			out.data[o] = ( cov << 24 ) | ( ( ( acc[2][0] + 2 ) >> 2 ) << 16 )
				| ( ( ( acc[2][1] + 2 ) >> 2 ) << 8 ) | ( ( acc[2][2] + 2 ) >> 2 );
			/* The mask filters plainly on all four channels. Roughness and
			 * metallic are material constants resampled, so their mean is the
			 * mean material -- unlike AO, which is a fixed-radius horizon march
			 * and therefore scale-dependent, exactly as the contract already
			 * says of the retired data sheet. */
			out.mask[o] = ( ( ( acc[3][3] + 2 ) >> 2 ) << 24 )
				| ( ( ( acc[3][0] + 2 ) >> 2 ) << 16 )
				| ( ( ( acc[3][1] + 2 ) >> 2 ) << 8 ) | ( ( acc[3][2] + 2 ) >> 2 );
			if ( wantEmissive )
				out.emissive[o] = 0xFF000000U | ( ( ( acc[4][0] + 2 ) >> 2 ) << 16 )
					| ( ( ( acc[4][1] + 2 ) >> 2 ) << 8 ) | ( ( acc[4][2] + 2 ) >> 2 );
			if ( cov )
				out.cover = true;
			out.height[o] = quint16( ( hAcc + 2 ) >> 2 );""")

open(p, 'w', encoding='utf-8', newline='').write(s)
b = open(p, 'rb').read()
print('CR', b.count(b'\r'), 'LF', b.count(b'\n'))
