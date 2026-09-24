# fix02 -- lane IMPOSTORDEPTH2: the drawer's crisp..smooth slider, the coverage filter always on.
R = 'E:/Projects/NifskopeWildWastelandEdition/'

def patch(rel, pairs):
    p = R + rel
    s = open(p, 'rb').read().decode('utf-8')
    cr0 = s.count('\r')
    for old, new in pairs:
        n = s.count(old)
        assert n == 1, (rel, n, old[:100])
        s = s.replace(old, new)
    assert s.count('\r') == cr0, (rel, 'CR moved')
    open(p, 'wb').write(s.encode('utf-8'))
    print('patched', rel)

# ---------------------------------------------------------------- header
patch('src/gl/impostordraw.h', [
('''	//! THE DEPTH SEARCH (lane IMPOSTORDEPTH1, 2026-09-23), the shader's
	//! `depthSearchSteps`. 0 = the one-step parallax, exactly the picture
	//! before the lane. N > 0 = each frame marches the view ray through the
	//! card's depth in N steps and refines once onto the first surface its
	//! height channel puts there, so the frames agree where a thin trunk is.
	//! Costs up to 2N + 1 sheet reads per frame. Needs `heightBlend`.
	//! `WW_IMPOSTOR_SEARCH=N` forces it for every draw. NOT a default: the
	//! shipped DXT5 `_n` sheets hold the height too coarsely for it to help.
	int depthSearchSteps = 0;
	//! SNAP (IMPOSTORDEPTH1): the nearest frame alone at full weight, with
	//! the height parallax (and the search, if on) still applied to it --
	//! unlike `heightBlend = false`, which draws that frame flat on the card
	//! plane. `WW_IMPOSTOR_SNAP=1` forces it for every draw.
	bool snap = false;
	//! COVERAGE DECODED, THEN FILTERED (IMPOSTORDEPTH1), the shader's
	//! `coverageDecodedFilter`: the four texels are decoded before the bilinear
	//! mix instead of after it, so an edge texel is not pulled a quarter texel
	//! inward by the encoding's jump at `cardCovBase`. Off = the picture before
	//! the lane. `WW_IMPOSTOR_COVFILTER=1` forces it for every draw.
	bool coverageDecodedFilter = false;
''', '''	/*! THE SLIDER, crisp (0) to smooth (1) -- docs/FO4CS_IMPROVED_LOD_PLAN.md's
	 *  slider contract, bungo's rulings of 2026-09-23 (lane IMPOSTORDEPTH2):
	 *    * 0, the CRISP end and THE DEFAULT, is SNAP: the nearest baked frame
	 *      alone at full weight, no blend, placed at its own depth by the
	 *      height parallax (`kCrispSearchSteps` of depth search). It jumps
	 *      when the nearest frame changes; that is ruled acceptable.
	 *    * 1, the SMOOTH end, is the three-frame blend under the STIPPLED cut
	 *      (`cutRule` 0) with a 16-step depth search (`kSmoothSearchSteps`).
	 *    * between, the three frames with their weights SHARPENED, w^(1/s)
	 *      renormalised, under the same stipple and search. The middle is
	 *      wired, not measured.
	 *  `WW_IMPOSTOR_SLIDER=<0..1>` forces it for every draw. There is no menu
	 *  row for it yet: no impostor option has one (env switches are the
	 *  viewer's convention for the card drawer). */
	float slider = 0.0f;
	//! THE DEPTH SEARCH (lane IMPOSTORDEPTH1), the shader's
	//! `depthSearchSteps`: each frame marches the view ray through the card's
	//! depth in N steps and refines once onto the first surface its height
	//! channel puts there, so the frames agree where a thin trunk is. Costs up
	//! to 2N + 1 sheet reads per frame. Needs `heightBlend`. -1 (the default)
	//! = the SLIDER's own number (see `slider`); N >= 0 forces N.
	//! `WW_IMPOSTOR_SEARCH=N` forces N for every draw -- a HARNESS override,
	//! not a user setting.
	int depthSearchSteps = -1;
	//! SNAP (IMPOSTORDEPTH1): forces the slider's crisp end whatever `slider`
	//! says -- the nearest frame alone at full weight, keeping the height
	//! parallax and the search -- unlike `heightBlend = false`, which draws
	//! that frame flat on the card plane. `WW_IMPOSTOR_SNAP=1` forces it for
	//! every draw.
	bool snap = false;
	/* The coverage is ALWAYS decoded per texel, then filtered (IMPOSTORDEPTH1's
	 * fix, made the only path by bungo's ruling, lane IMPOSTORDEPTH2): there is
	 * no field and no switch; `WW_IMPOSTOR_COVFILTER` is retired. */
''' ),
('''bool drawCard( Scene * scene, const ImpostorCardSet & set,
				const Vector3 & worldOffset, const Options & opt, QString * why = nullptr );
''', '''bool drawCard( Scene * scene, const ImpostorCardSet & set,
				const Vector3 & worldOffset, const Options & opt, QString * why = nullptr );

//! The slider's two ends, in depth-search steps (lane IMPOSTORDEPTH2).
constexpr int kCrispSearchSteps  = 0;
constexpr int kSmoothSearchSteps = 16;

/*! What `drawCard` will actually do with `opt` once the environment's
 *  overrides are applied: ONE function, so the harness's log line and the
 *  draw cannot disagree. */
struct Resolved
{
	float slider = 0.0f;      //!< 0 crisp .. 1 smooth, after WW_IMPOSTOR_SLIDER
	bool  snap = true;        //!< the crisp end (or WW_IMPOSTOR_SNAP / Options::snap)
	int   frameCount = 1;     //!< 1 snap or flat, 3 blended
	int   searchSteps = 0;    //!< after WW_IMPOSTOR_SEARCH / Options::depthSearchSteps
	float sharpen = 1.0f;     //!< the weights' exponent, 1/slider between the ends
	bool  sliderForced = false, snapForced = false, searchForced = false;
};
Resolved resolve( const Options & opt );
''' ),
])

# ---------------------------------------------------------------- drawer
patch('src/gl/impostordraw.cpp', [
('''	/* SNAP (lane IMPOSTORDEPTH1): the nearest frame alone like the no-blend
	 * picture below, but it keeps the height parallax and the depth search.
	 * Read here, not by the caller, like WW_IMPOSTOR_CUT, so every path that
	 * draws a card has it. */
	static const bool envSnap = qEnvironmentVariableIntValue( "WW_IMPOSTOR_SNAP" ) != 0;
	const bool snap = opt.snap || envSnap;
	const int frameCount = ( opt.heightBlend && !snap ) ? 3 : 1;
	if ( frameCount == 1 ) {''', '''	/* THE SLIDER (lane IMPOSTORDEPTH2): crisp end = SNAP, the nearest frame
	 * alone like the no-blend picture below but keeping the height parallax;
	 * smooth end = three frames, stipple, search 16; between = three frames
	 * with the weights sharpened. `resolve` reads the environment's overrides,
	 * here and not by the caller, like WW_IMPOSTOR_CUT, so every path that
	 * draws a card has them. */
	const Resolved rs = resolve( opt );
	const int frameCount = rs.frameCount;
	if ( frameCount == 3 && rs.sharpen != 1.0f ) {
		float sum = 0.0f;
		for ( int k = 0; k < 3; k++ ) {
			w[k] = std::pow( qMax( 0.0f, w[k] ), rs.sharpen );
			sum += w[k];
		}
		if ( sum > 0.0f )
			for ( int k = 0; k < 3; k++ )
				w[k] /= sum;
	}
	if ( frameCount == 1 ) {'''),
('''	/* The depth search (lane IMPOSTORDEPTH1): 0 = the one-step parallax, the
	 * picture before the lane. WW_IMPOSTOR_SEARCH=N forces N steps for every
	 * draw; clamped to 64 so a typo cannot hang the GPU. */
	static const int envSearch = qEnvironmentVariableIsSet( "WW_IMPOSTOR_SEARCH" )
			? qBound( 0, qEnvironmentVariableIntValue( "WW_IMPOSTOR_SEARCH" ), 64 ) : -1;
	prog->uni1i( "depthSearchSteps", envSearch >= 0 ? envSearch
			: qBound( 0, opt.depthSearchSteps, 64 ) );
	/* Coverage decoded per texel before the bilinear mix (IMPOSTORDEPTH1). */
	static const bool envCovFilter = qEnvironmentVariableIntValue( "WW_IMPOSTOR_COVFILTER" ) != 0;
	prog->uni1b( "coverageDecodedFilter", opt.coverageDecodedFilter || envCovFilter );
''', '''	/* The depth search (lane IMPOSTORDEPTH1): the slider's number unless
	 * Options or WW_IMPOSTOR_SEARCH=N force one (clamped to 64 so a typo cannot
	 * hang the GPU). The coverage is always decoded per texel, then filtered:
	 * that is the shader's only path now (lane IMPOSTORDEPTH2). */
	prog->uni1i( "depthSearchSteps", rs.searchSteps );
'''),
('''bool ImpostorDraw::drawCard( Scene * scene, const ImpostorCardSet & set,''', '''ImpostorDraw::Resolved ImpostorDraw::resolve( const Options & opt )
{
	static const float envSlider = qEnvironmentVariableIsSet( "WW_IMPOSTOR_SLIDER" )
			? qBound( 0.0f, float( qEnvironmentVariable( "WW_IMPOSTOR_SLIDER" ).toDouble() ), 1.0f ) : -1.0f;
	static const bool envSnap = qEnvironmentVariableIntValue( "WW_IMPOSTOR_SNAP" ) != 0;
	static const int envSearch = qEnvironmentVariableIsSet( "WW_IMPOSTOR_SEARCH" )
			? qBound( 0, qEnvironmentVariableIntValue( "WW_IMPOSTOR_SEARCH" ), 64 ) : -1;
	Resolved r;
	r.sliderForced = envSlider >= 0.0f;
	r.slider = r.sliderForced ? envSlider : qBound( 0.0f, opt.slider, 1.0f );
	r.snapForced = envSnap;
	r.snap = opt.snap || envSnap || r.slider <= 0.0f;
	r.frameCount = ( opt.heightBlend && !r.snap ) ? 3 : 1;
	r.searchForced = envSearch >= 0;
	r.searchSteps = r.searchForced ? envSearch
			: opt.depthSearchSteps >= 0 ? qBound( 0, opt.depthSearchSteps, 64 )
			: r.snap ? kCrispSearchSteps : kSmoothSearchSteps;
	r.sharpen = ( r.snap || r.slider >= 1.0f ) ? 1.0f : 1.0f / r.slider;
	return r;
}

bool ImpostorDraw::drawCard( Scene * scene, const ImpostorCardSet & set,'''),
])

# ---------------------------------------------------------------- harness
patch('src/impostorpreviewtest.cpp', [
('''	s.opt.snap = qEnvironmentVariableIntValue( "WW_IMPOSTOR_SNAP" ) != 0;
	s.opt.depthSearchSteps = qBound( 0, qEnvironmentVariableIntValue( "WW_IMPOSTOR_SEARCH" ), 64 );
	s.log << ( !s.opt.heightBlend
			? QStringLiteral( "frames: ONE, flat on the card plane (WW_IMPOSTOR_BLEND=0)" )
			: s.opt.snap
			? QStringLiteral( "frames: ONE, the nearest, with height parallax (WW_IMPOSTOR_SNAP=1)" )
			: QStringLiteral( "frames: THREE, height-blended" ) );
	s.log << QStringLiteral( "depth search: %1" ).arg( s.opt.depthSearchSteps > 0
			? QStringLiteral( "%1 steps + 1 refinement (WW_IMPOSTOR_SEARCH)" ).arg( s.opt.depthSearchSteps )
			: QStringLiteral( "off -- the one-step parallax" ) );
	s.opt.coverageDecodedFilter = qEnvironmentVariableIntValue( "WW_IMPOSTOR_COVFILTER" ) != 0;
	s.log << ( s.opt.coverageDecodedFilter
			? QStringLiteral( "coverage filter: DECODED per texel, then bilinear (WW_IMPOSTOR_COVFILTER=1)" )
			: QStringLiteral( "coverage filter: hardware bilinear over the encoded alpha" ) );
''', '''	// Lane IMPOSTORDEPTH2: the slider (crisp end = snap, the default; smooth
	// end = stipple + search 16). The drawer resolves the environment's
	// overrides itself; the log prints ITS answer, not a second copy.
	const ImpostorDraw::Resolved rs = ImpostorDraw::resolve( s.opt );
	s.log << QStringLiteral( "slider: %1 -- %2%3" ).arg( double( rs.slider ), 0, 'f', 2 )
			.arg( rs.slider <= 0.0f ? QStringLiteral( "the CRISP end, snap" )
				: rs.slider >= 1.0f ? QStringLiteral( "the SMOOTH end, stipple + search" )
				: QStringLiteral( "between the ends, weights sharpened to the power %1" ).arg( double( rs.sharpen ), 0, 'f', 3 ) )
			.arg( rs.sliderForced ? QStringLiteral( " (WW_IMPOSTOR_SLIDER)" ) : QStringLiteral( " (the default)" ) );
	s.log << ( !s.opt.heightBlend
			? QStringLiteral( "frames: ONE, flat on the card plane (WW_IMPOSTOR_BLEND=0)" )
			: rs.snap
			? QStringLiteral( "frames: ONE, the nearest, with height parallax (%1)" ).arg( rs.snapForced
				? QStringLiteral( "WW_IMPOSTOR_SNAP=1" ) : QStringLiteral( "the slider's crisp end" ) )
			: QStringLiteral( "frames: THREE, height-blended" ) );
	s.log << QStringLiteral( "depth search: %1" ).arg( rs.searchSteps > 0
			? QStringLiteral( "%1 steps + 1 refinement (%2)" ).arg( rs.searchSteps )
				.arg( rs.searchForced ? QStringLiteral( "WW_IMPOSTOR_SEARCH" ) : QStringLiteral( "the slider's" ) )
			: QStringLiteral( "off -- the one-step parallax (%1)" )
				.arg( rs.searchForced ? QStringLiteral( "WW_IMPOSTOR_SEARCH" ) : QStringLiteral( "the slider's" ) ) );
	s.log << QStringLiteral( "coverage filter: DECODED per texel, then bilinear -- always (WW_IMPOSTOR_COVFILTER retired)" );
'''),
])

# ---------------------------------------------------------------- shader
patch('res/shaders/impostor_oct.frag', [
('''/* COVERAGE, DECODED THEN FILTERED (lane IMPOSTORDEPTH1, 2026-09-23).
 * `coverageDecodedFilter` false -- GL's default, what every caller that does
 * not set it gets -- is the hardware's bilinear over the ENCODED alpha, then
 * `coverageOf`: the picture before the lane. True reads the four texels with
 * texelFetch, decodes EACH, and filters the fractions.
''', '''/* COVERAGE, DECODED THEN FILTERED (lane IMPOSTORDEPTH1, 2026-09-23; the ONLY
 * path since lane IMPOSTORDEPTH2 on bungo's ruling -- the `coverageDecodedFilter`
 * uniform and its WW_IMPOSTOR_COVFILTER switch are gone). The four texels are
 * read with texelFetch, EACH is decoded, and the fractions are filtered. The
 * old path was the hardware's bilinear over the ENCODED alpha, then
 * `coverageOf`.
'''),
('''uniform bool  coverageDecodedFilter;

float covAt( vec2 uv )
{
	if ( !coverageDecodedFilter )
		return coverageOf( textureLod( ColourSheet, uv, 0.0 ).a );
	ivec2 sz''', '''float covAt( vec2 uv )
{
	ivec2 sz'''),
])
