# IMPOSTORTEAR1 job 1 -- the way back for the cut rule: ImpostorDrawOptions::cutOnMean
# (default false = the new rule) and WW_IMPOSTOR_CUT=mean, read by the draw itself so
# every path that draws a card honours it; the preview harness logs which rule drew.
import sys

R = 'E:/Projects/NifskopeWildWastelandEdition/src/'


def once(s, a, n, f):
    c = s.count(a)
    if c != 1:
        sys.exit('REFUSED: %s anchor %r matches %d times' % (f, a[:70], c))
    return s.replace(a, n)


def edit(f, pairs):
    p = R + f
    b = open(p, 'rb').read()
    cr0 = b.count(b'\r')
    s = b.decode('utf-8')
    for a, n in pairs:
        s = once(s, a, n, f)
    out = s.encode('utf-8')
    assert out.count(b'\r') == cr0, f + ' CR moved'
    open(p, 'wb').write(out)
    print('patched', f, 'CR', cr0)


edit('gl/impostordraw.h', [('''	bool heightBlend = true;
''', '''	bool heightBlend = true;
	//! THE CUT'S COVERAGE (lane IMPOSTORTEAR1, 2026-09-23). false = the cut
	//! reads the coverage of the STRONGEST contributing frame (the largest
	//! blend weight), which stops the tear bungo called "like somebody ripped
	//! out a piece of paper"; true = the way back, the 3-frame weighted mean the
	//! cut read before. `WW_IMPOSTOR_CUT=mean` forces it for every draw.
	bool cutOnMean = false;
''')])

edit('gl/impostordraw.cpp', [('''	prog->uni1b( "useHeightBlend", opt.heightBlend );
''', '''	prog->uni1b( "useHeightBlend", opt.heightBlend );
	/* The cut's coverage (lane IMPOSTORTEAR1): the strongest frame's, or with
	 * `cutOnMean` / WW_IMPOSTOR_CUT=mean the old 3-frame mean. Read here and not
	 * by the caller so every path that draws a card has the way back. */
	static const bool envCutMean = qEnvironmentVariable( "WW_IMPOSTOR_CUT" ).compare(
			QStringLiteral( "mean" ), Qt::CaseInsensitive ) == 0;
	prog->uni1b( "cutOnMean", opt.cutOnMean || envCutMean );
''')])

edit('impostorpreviewtest.cpp', [('''	s.opt.debugChannel = qEnvironmentVariableIntValue( "WW_IMPOSTOR_CHANNEL" );
''', '''	s.opt.debugChannel = qEnvironmentVariableIntValue( "WW_IMPOSTOR_CHANNEL" );
	s.opt.cutOnMean    = qEnvironmentVariable( "WW_IMPOSTOR_CUT" ).compare(
			QStringLiteral( "mean" ), Qt::CaseInsensitive ) == 0;
	s.log << ( s.opt.cutOnMean
			? QStringLiteral( "cut rule: the 3-frame MEAN coverage (WW_IMPOSTOR_CUT=mean, the way back)" )
			: QStringLiteral( "cut rule: the STRONGEST frame's coverage (IMPOSTORTEAR1)" ) ) << "\\n";
''')])
