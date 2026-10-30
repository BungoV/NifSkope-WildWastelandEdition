"""lane CLOUD6f: the lightning's wiring in the sources, and its switches in the stage table (live = stage 0).
    python -I tests/cloudlightning/wiring_check.py <tree root> [<rev>]   (with <rev>: read the files at that commit: the red)
Checks: cloudvol.cpp reads WW_CLOUD_LIGHTNING + its red and sets the flash uniforms in BOTH composites (the backdrop and
the pass over the scene), on every draw; cloud_composite.frag has the flash term behind cvLtOn; esmweather.cpp counts the
thunder SNAMs (type 3) and reads the fade / colour bytes; lookdevstage.cpp has wwLookdevLightning on stormShare;
src/wwswitchstages.inc lists WW_CLOUD_LIGHTNING and WW_CLOUD_LIGHTNING_RED at stage 0."""
import re, subprocess, sys

root = sys.argv[1]
rev = sys.argv[2] if len(sys.argv) > 2 else ''


def text(path):
    if rev:
        r = subprocess.run(['git', '-C', root, 'show', '%s:%s' % (rev, path)], capture_output=True)
        return r.stdout.decode('latin-1') if r.returncode == 0 else ''
    try:
        return open(root + '/' + path, 'rb').read().decode('latin-1')
    except OSError:
        return ''


cv = text('src/gl/cloudvol.cpp')
fr = text('res/shaders/cloud_composite.frag')
ew = text('src/esmweather.cpp')
ld = text('src/gl/lookdevstage.cpp')
st = text('src/wwswitchstages.inc')
checks = [
    ('cloudvol reads WW_CLOUD_LIGHTNING', 'wwEnvStr( "WW_CLOUD_LIGHTNING" )' in cv),
    ('cloudvol reads WW_CLOUD_LIGHTNING_RED', 'wwEnvStr( "WW_CLOUD_LIGHTNING_RED" )' in cv),
    ('cvLtOn set on every draw', 'p->uni1i( "cvLtOn", lt.on ? 1 : 0 );' in cv),
    ('both composites set the flash', len(re.findall(r'\blightningUniforms\( r, p,', cv)) >= 2),
    ('the backdrop binds the history depth', 'g.hist[cur].dep' in cv),
    ('composite frag: the flash behind cvLtOn', 'if ( cvLtOn != 0 )' in fr and 'cvLtField' in fr),
    ('esmweather counts thunder SNAMs', 'out.thunderSounds++' in ew and 'f == "SNAM"' in ew),
    ('esmweather reads the colour bytes', 'out.lightningRgb[c] = quint8( f.data()[12 + c] );' in ew),
    ('lookdevstage wwLookdevLightning', 'bool wwLookdevLightning(' in ld and 'stormShare(' in ld),
]
for name in ('WW_CLOUD_LIGHTNING', 'WW_CLOUD_LIGHTNING_RED'):
    m = re.search(r'\{ "%s", (\d+),' % name, st)
    checks.append(('%s stage 0' % name, bool(m) and m.group(1) == '0'))
bad = [n for n, ok in checks if not ok]
for n, ok in checks:
    print('%-44s %s' % (n, 'ok' if ok else 'MISSING'))
print('CLOUDLIGHTNING-WIRING %s%s' % ('PASS' if not bad else 'FAIL', (' (rev %s)' % rev) if rev else ''))
sys.exit(1 if bad else 0)
