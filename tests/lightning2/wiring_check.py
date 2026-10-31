"""lane LIGHTNING2: the wiring of the DATA frequency law and of the flash on the rest of the frame, in the sources, and the
switches in the stage table (live = stage 0).
    python -I tests/lightning2/wiring_check.py <tree root> [<rev>]   (with <rev>: read the files at that commit: the red)
Checks: cloudvol.cpp reads WW_LIGHTNING_FREQ / _RED and WW_LIGHTNING_SKY / _RED, builds the schedule from the DATA byte
(freqSchedule, oldSchedule = the red), has one flash function for every reader (lightningAt) and exports wwLightningSky /
wwLightningGround; lookdevstage.cpp hands the frequency byte out of wwLookdevLightning, starts the frame
(wwLightningFrameBegin) and sets ldLtOn on every layer draw; lookdev_clouds.frag adds the flash behind ldLtOn through
lightning_field.glsl; celllights.cpp adds wwLightningGround to the exterior DALC; the four names at stage 0."""
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
cl = text('src/gl/cloudlightning.h')
ld = text('src/gl/lookdevstage.cpp')
ce = text('src/gl/celllights.cpp')
fr = text('res/shaders/lookdev_clouds.frag')
vt = text('res/shaders/lookdev_clouds.vert')
lf = text('res/shaders/lightning_field.glsl')
st = text('src/wwswitchstages.inc')
checks = [
    ('cloudvol reads WW_LIGHTNING_FREQ + red', 'wwEnvStr( "WW_LIGHTNING_FREQ" )' in cv and 'wwEnvStr( "WW_LIGHTNING_FREQ_RED" )' in cv),
    ('cloudvol reads WW_LIGHTNING_SKY + red', 'wwEnvStr( "WW_LIGHTNING_SKY" )' in cv and 'wwEnvStr( "WW_LIGHTNING_SKY_RED" )' in cv),
    ('the schedule from the DATA byte', 'L::freqSchedule( lt.share, freq ) : L::oldSchedule( lt.share )' in cv),
    # FIX1031B review: the DATA law is ours (no game law read): opt-in, CLOUD6f's schedule by default
    ('the DATA law ships Off (opt-in)', 'return v == "1" || v == "on" || v == "true" || v == "yes";' in cv),
    ('cloudlightning.h freqChance = (255 - byte) / 255', 'float( 255u - std::min( byte, 255u ) ) / 255.0f' in cl),
    ('one flash function for every reader', 'CvLightning lightningAt(' in cv and cv.count('lightningAt( ') >= 3),
    ('wwLightningSky + wwLightningGround', 'bool wwLightningSky( Scene * scene' in cv and 'bool wwLightningGround( float add[6][3] )' in cv),
    ('wwLookdevLightning hands the byte out', '*freq = src.thunderFreq;' in ld),
    ('the frame starts in the backdrop', 'wwLightningFrameBegin();' in ld),
    ('ldLtOn on every layer draw', 'prog->uni1i( "ldLtOn", on ? 1 : 0 );' in ld and 'flashUniforms( prog, lsky.on );' in ld
     and 'flashUniforms( prog, false );' in ld),
    ('layers frag: the flash behind ldLtOn', 'if ( ldLtOn != 0 )' in fr and 'ldLtField(' in fr and '#include "lightning_field.glsl"' in fr),
    ('layers vert: the world direction', 'wdir = probe ?' in vt),
    ('lightning_field.glsl', 'float ldLtField( vec4 src, vec4 prm, vec3 p, vec3 d )' in lf),
    ('celllights: the flash on the exterior DALC', 'if ( wwLightningGround( flash ) )' in ce and 'w[a][c] += flash[a][c];' in ce),
]
for name in ('WW_LIGHTNING_FREQ', 'WW_LIGHTNING_FREQ_RED', 'WW_LIGHTNING_SKY', 'WW_LIGHTNING_SKY_RED'):
    m = re.search(r'\{ "%s", (\d+),' % name, st)
    checks.append(('%s stage 0' % name, bool(m) and m.group(1) == '0'))
bad = [n for n, ok in checks if not ok]
for n, ok in checks:
    print('%-48s %s' % (n, 'ok' if ok else 'MISSING'))
print('LIGHTNING2-WIRING %s%s' % ('PASS' if not bad else 'FAIL', (' (rev %s)' % rev) if rev else ''))
sys.exit(1 if bad else 0)
