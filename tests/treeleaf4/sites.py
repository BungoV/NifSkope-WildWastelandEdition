# lane TREELEAF4: text twin of the C++ half of FXCOVRESID1 + TAABLENDMOVER1 (the shader half is fxcov_gate.py on the GPU).
#  1. glshape.cpp: wwFxSideAlpha adds the falloff (mode 8, the effect's falloff params) and the cell's soft fade (mode 16,
#     its soft depth) under the red WW_FXCOVRESID_RED, a volumetric card flagged; wwBlendedAsDrawn = setupProgramCE1's blend;
#  2. taainputs.cpp: blended movers (red WW_TAABLENDMOVER_RED) bind their base map, draw covMode 1 then covMode 2 into
#     the mask's R only; the normal map at slot 3, the opaque depth and cellFxFade's inputs; the CPU-skinned frame rides
#     as attributes 2-4; the view rotation as normalMatrix.
#   python -I sites.py <tree root>                the working tree (green: must PASS)
#   python -I sites.py <tree root> --rev <commit>  the files at that commit (red: the lane's base, must FAIL)
import subprocess
import sys

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
    'src/gl/glshape.cpp': [
        'in.effect = bsesp && bssp && bssp->bsVersion >= 130 && bssp->bsVersion <= 139 && ( da.test || wwBlendedAsDrawn() );',
        'wwEnv( "WW_FXCOVRESID_RED" )',
        'if ( ( o.mode & 4 ) && bsesp ) {',
        'o.volPart = scene && wwVolPartOn() && wwCellFxLitFor( scene->nifModel, id() ) != nullptr;',
        'if ( bsesp->useFalloff ) { o.mode |= 8;',
        'o.falloff[0] = bsesp->falloff.startAngle;',
        'o.falloff[3] = bsesp->falloff.stopOpacity;',
        'const bool cellFx = scene && wwCellLightsWanted( scene ) && !( wwCellFxRed() & 1 );',
        'if ( cellFx && bsesp->soft && bsesp->falloff.softDepth > 0.0f && !( wwCellFxRed() & 2 ) ) { o.mode |= 16;',
        'o.softDepth = std::max( bsesp->falloff.softDepth, 1.0f );',
        'bool Shape::wwBlendedAsDrawn() const',
        'if ( const Material * mat = bssp->getMaterial() ) return mat->hasAlphaBlend();',
        'if ( translucent ) return true;',
        'return alphaProperty && alphaProperty->hasAlphaBlend();',
    ],
    'src/gl/glshape.h': ['bool wwBlendedAsDrawn() const;', 'float softDepth = 1.0f;', 'bool volPart = false;'],
    'src/gl/taainputs.cpp': [
        'wwEnv( "WW_TAABLENDMOVER_RED" )',
        'constexpr float kCovThreshold = 0.5f;',
        'const bool fxDepthOk = scene->fxDepthPass == Scene::fxDepthPassNow && scene->fxDepthState > 0 && scene->fxDepthTexId',
        'prog->uni3m( "normalMatrix", sh->viewTrans().rotation );',
        'const bool blended = !blendRed && sh->wwBlendedAsDrawn();',
        'const bool based = ( da.test || blended ) && bindBase( prog, sh );',
        'const bool at = da.test && based;',
        'if ( !fxDepthOk ) fx.mode &= ~16;',
        'const bool cov = blended && based && !fx.volPart;',
        'if ( at || cov ) {',
        '"NormalMap", 3, texunit, QStringLiteral( "#FFFF8080" )',
        'prog->uni4f( "falloffParams", FloatVector4( fx.falloff[0], fx.falloff[1], fx.falloff[2], fx.falloff[3] ) );',
        'prog->uni1b( "fxOrtho", Pg[15] == 1.0f );',
        'fn->glBindTexture( GL_TEXTURE_2D, scene->fxDepthTexId );',
        'prog->uni2f( "fxProj", Pg[10], Pg[14] );',
        'prog->uni1f( "fxDistScale", 1.0f / viewSc );',
        'prog->uni1f( "fxSoftDepth", fx.softDepth );',
        'prog->uni1f( "covThreshold", kCovThreshold );',
        'prog->uni1i( "covMode", cov ? 1 : 0 );',
        'skinN[size_t( v )] = sh->skinVertex( int( v ), sh->norms.at( v ) ) - z;',
        'for ( int pass = 0; pass < ( cov ? 2 : 1 ); pass++ ) {',
        'prog->uni1i( "covMode", 2 );',
        'fn->glColorMaski( 0, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );',
        'fn->glColorMaski( 1, GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE );',
        '( tbnOk ? 0x33300ULL : 0ULL )',
        'fn->glColorMaski( 1, GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE );	// the own-motion flag only, as before',
    ],
    'res/shaders/taa_objvel.vert': ['btnMatrix[2] = normalize( normalVector * normalMatrix );',
                                    'btnMatrix[0] = normalize( bitangentVector * normalMatrix );'],
}

fails = 0
for f, needs in NEED.items():
    t = squash(text(f))
    miss = [n for n in needs if squash(n) not in t]
    fails += bool(miss)
    print('%s %s: %d of %d lines%s' % ('ok  ' if not miss else 'FAIL', f, len(needs) - len(miss), len(needs),
                                       '' if not miss else ' (missing: %s)' % miss[0][:80]))
print('TREELEAF4 SITES %s' % ('PASS' if fails == 0 else 'FAIL (%d files)' % fails))
sys.exit(1 if fails else 0)
