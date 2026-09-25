# CARDFIX1 step 4 (R5, ruled 2026-09-24 21:1x: "crisp cards -- N8, crisp cut, slider crisp end").
# N8 (bake driver OCT, panel cardFrames) and slider 0 (flat snap) were already the defaults (DEFAULTS2 /
# IMPOSTORDEPTH2). The CUT at the crisp end was the stipple rule applied to one frame at weight 1, which
# equals the strongest-frame cut only by arithmetic. This names it: resolve() returns the cut rule, and
# the crisp end draws under rule 2 (the strongest frame alone) by name, so a later edit to the stipple can
# never reach the default picture. Pixels unchanged at slider 0 (gate: byte-identical renders).
ROOT = 'E:/Projects/NifskopeWWE-cardfix1/'


def patch(path, edits):
    b = open(ROOT + path, 'rb').read()
    cr = b.count(b'\r')
    s = b.decode('utf-8')
    for old, new in edits:
        c = s.count(old)
        assert c == 1, (path, old[:70], c)
        s = s.replace(old, new)
    out = s.encode('utf-8')
    assert out.count(b'\r') == cr, path
    open(ROOT + path, 'wb').write(out)
    print('patched', path)


patch('src/gl/impostordraw.h', [
    ('''	//! as impostor_draw.sh row 18's red control. `WW_IMPOSTOR_CUT=mean|strong`
	//! forces 1 or 2 for every draw.
	int cutRule = 0;
''',
     '''	//! as impostor_draw.sh row 18's red control. `WW_IMPOSTOR_CUT=mean|strong`
	//! forces 1 or 2 for every draw.
	//! THIS IS THE BLENDED SLIDER'S CUT. At the crisp end (the default, one
	//! frame) the cut is rule 2, the strongest frame, BY NAME -- bungo's R5
	//! (2026-09-24 21:1x: "crisp cards -- N8, crisp cut, slider crisp end");
	//! see Resolved::cutRule.
	int cutRule = 0;
'''),
    ('''	float sharpen = 1.0f;     //!< the weights' exponent, 1/slider between the ends
	bool  sliderForced = false, snapForced = false, searchForced = false;
''',
     '''	float sharpen = 1.0f;     //!< the weights' exponent, 1/slider between the ends
	int   cutRule = 2;        //!< the cut drawn: 2 (strongest frame) at the crisp end,
	                          //!< Options::cutRule once blended; WW_IMPOSTOR_CUT still wins
	bool  sliderForced = false, snapForced = false, searchForced = false;
'''),
])

patch('src/gl/impostordraw.cpp', [
    ('''	r.sharpen = ( r.snap || r.slider >= 1.0f ) ? 1.0f : 1.0f / r.slider;
	return r;
''',
     '''	r.sharpen = ( r.snap || r.slider >= 1.0f ) ? 1.0f : 1.0f / r.slider;
	/* THE CRISP CUT (R5, bungo 2026-09-24 21:1x). One frame at weight 1 under
	 * the stipple rule already cut exactly where that frame's coverage does;
	 * naming rule 2 here makes it so by construction, not by arithmetic. */
	r.cutRule = ( r.frameCount == 1 ) ? 2 : opt.cutRule;
	return r;
'''),
    ('''	prog->uni1i( "cutRule", envCutRule >= 0 ? envCutRule : opt.cutRule );
''',
     '''	prog->uni1i( "cutRule", envCutRule >= 0 ? envCutRule : rs.cutRule );
'''),
])
