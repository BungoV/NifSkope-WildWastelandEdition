# CARDFIX1 step 4: the preview harness prints the cut the DRAWER resolves (ImpostorDraw::Resolved::cutRule),
# not the option it was handed -- at the crisp end that is the strongest frame, by name (R5).
# The blended runs (WW_IMPOSTOR_SLIDER=1, impostor_draw row 18, impostor_trunk k_smooth) print exactly
# what they printed before: their resolved rule is still Options::cutRule.
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


OLD = '''	s.log << ( s.opt.cutRule == 1
			? QStringLiteral( "cut rule: mean -- the 3-frame MEAN coverage (WW_IMPOSTOR_CUT=mean, the way back)" )
			: s.opt.cutRule == 2
			? QStringLiteral( "cut rule: strong -- the STRONGEST frame alone (WW_IMPOSTOR_CUT=strong, row 18's red control)" )
'''
NEW = '''	// Lane CARDFIX1 (R5): the rule the DRAWER resolves, so the crisp end's own
	// cut (the strongest frame, by name) is what the log says.
	const int cutDrawn = s.opt.cutRule != 0 ? s.opt.cutRule : ImpostorDraw::resolve( s.opt ).cutRule;
	s.log << ( cutDrawn == 1
			? QStringLiteral( "cut rule: mean -- the 3-frame MEAN coverage (WW_IMPOSTOR_CUT=mean, the way back)" )
			: cutDrawn == 2 && s.opt.cutRule == 0
			? QStringLiteral( "cut rule: strong -- the STRONGEST frame alone: the crisp end's own cut (R5, the default)" )
			: cutDrawn == 2
			? QStringLiteral( "cut rule: strong -- the STRONGEST frame alone (WW_IMPOSTOR_CUT=strong, row 18's red control)" )
'''
patch('src/impostorpreviewtest.cpp', [(OLD, NEW)])
