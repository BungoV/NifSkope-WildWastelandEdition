# lane TREELEAF3: text twin of the TAA object-vector pass's effect-alpha rule (src/gl/wwfxcov.h).
#  1. taainputs.cpp: the movers' alpha uniforms come from Shape::wwFxSideAlpha( da, "taa" ) (fxMode, alpha, threshold),
#     the palette is bound when fxMode & 2, and the CPU-skinned draw carries the colours (attribute 1) when fxMode & 1;
#  2. taa_objvel.frag: the tested alpha is wwfxcov.h sideAlpha's expression, line for line; taa_objvel.vert passes the
#     vertex alpha;
#  3. glshape.cpp: wwFxSideAlpha reads the red through the TREELEAF family ("fx").
#   python -I sites.py <tree root>                the working tree (green: must PASS)
#   python -I sites.py <tree root> --rev <commit>  the files at that commit (red: the lane's base, must FAIL)
import subprocess, sys

root = sys.argv[1]
rev = sys.argv[3] if len(sys.argv) > 3 and sys.argv[2] == '--rev' else None


def text(f):
    if rev:
        r = subprocess.run(['git', '-C', root, 'show', '%s:%s' % (rev, f)], capture_output=True)
        return r.stdout.decode('utf-8', 'replace') if r.returncode == 0 else ''
    return open(root + '/' + f, 'rb').read().decode('utf-8', 'replace')


def squash(s):
    return ' '.join(s.replace('\r\n', '\n').split())


NEED = {
    'src/gl/taainputs.cpp': [
        # lane TREELEAF4: a blended mover's coverage reads the same composed alpha (was `fx = at ? ...`)
        'Shape::WwFxAlpha fx = ( at || ( blended && based ) ) ? sh->wwFxSideAlpha( da, "taa" ) : Shape::WwFxAlpha();',
        'prog->uni1i( "fxMode", fx.mode );',
        'prog->uni1f( "alpha", fx.mult );',
        'prog->uni1f( "alphaThreshold", fx.threshold );',
        'if ( fx.mode & 2 ) {',
        '"GreyscaleMap", 1, texunit',
        'const bool vtxA = ( fx.mode & 1 ) != 0;',
        'vtxA ? &( sh->colors.constFirst()[0] ) : nullptr',
        '( vtxA ? 0x40ULL : 0ULL )',
    ],
    'res/shaders/taa_objvel.frag': [
        'uniform int fxMode;',
        'uniform sampler2D GreyscaleMap;',
        'float a = texture( BaseMap, texCoord * uvScale + uvOffset ).a;',
        'float k = ( fxMode & 1 ) != 0 ? vAlpha : 1.0;',
        # lane TREELEAF4: + the falloff f and the soft fade s in fo4_effectshader.frag's order (f = s = 1 is this rule)
        'a = ( fxMode & 2 ) != 0 ? textureLod( GreyscaleMap, vec2( a, ( ( 1.0 * f ) * k ) * alpha * s ), 0.0 ).a '
        ': ( ( a * f ) * k ) * alpha * s;',
        'a *= alpha;',
        'if ( alphaTest != 0 && covMode != 2 && a < alphaThreshold ) discard;',
    ],
    'res/shaders/taa_objvel.vert': ['vAlpha = vertexColor.a;'],
    'src/gl/wwfxcov.h': [
        'return ( o.mode & 2 ) ? pal( base, k * o.mult ) : base * k * o.mult;',
        'o.mult = s.baseColorA * s.baseColorA;',
    ],
    'src/gl/glshape.cpp': ['WwFxCov::side( in, wwLeafPassRed( "fx" ) )'],
}

fails = 0
for f, needs in NEED.items():
    t = squash(text(f))
    miss = [n for n in needs if squash(n) not in t]
    fails += bool(miss)
    print('%s %s: %d of %d lines%s' % ('ok  ' if not miss else 'FAIL', f, len(needs) - len(miss), len(needs),
                                       '' if not miss else ' (missing: %s)' % miss[0][:80]))
print('TREELEAF3 SITES %s' % ('PASS' if fails == 0 else 'FAIL (%d files)' % fails))
sys.exit(1 if fails else 0)
