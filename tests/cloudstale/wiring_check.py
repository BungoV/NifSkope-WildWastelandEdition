"""Lane CLOUD6g: Move Clamp / Change Clamp are wired as gated (tests/spells/cloudstale_check.py judges the shader + the
uniforms the dump says cloudvol.cpp sets; this checks that cloudvol.cpp really sets them that way).
    python -I tests/cloudstale/wiring_check.py <cloudvol.cpp> <cloud_resolve.frag> <cloudband_dump.exe>   exit 0 = all hold
W1 cs() reads WW_CLOUDVOL_MOVECLAMP and WW_CLOUDVOL_CHANGECLAMP through V::defaultOnSwitch (the DEFAULT, live: cs()
   re-reads on any WW_CLOUDVOL* set); W2 resolvePass sets cvMoveClamp on every draw and cvAccReset = V::kChangeReset with
   the accum twins; W3 the reds staleread / looseclamp turn them off and the echoes name them; W4 the shader declares
   both uniforms and both programs (Off + accum) clamp their reprojected pixels; W5 the dump's rule for the switch values
   (unset / empty / 1 / on / yes -> on; 0 / off / false / no -> off), the env handed by this script.
Red: the base's cloudvol.cpp + cloud_resolve.frag (git show 2f4d613d:...): W1-W4 FAIL."""
import os, re, subprocess, sys

src = open(sys.argv[1], 'rb').read().decode('latin-1')
frag = open(sys.argv[2], 'rb').read().decode('latin-1')
dump = sys.argv[3] if len(sys.argv) > 3 else ''
ok = True


def line(tag, good, msg):
    global ok
    ok &= bool(good)
    print('%s %s  %s' % (tag, 'PASS' if good else 'FAIL', msg))


for name, member in (('WW_CLOUDVOL_MOVECLAMP', 'moveClamp'), ('WW_CLOUDVOL_CHANGECLAMP', 'changeClamp')):
    m = re.search(r'const QByteArray (\w+) = wwEnvStr\( "%s" \)\.trimmed\(\)\.toLower\(\)\.toLatin1\(\);\s*'
                  r's\.%s = V::defaultOnSwitch\( \1\.constData\(\) \) \? 1 : 0;' % (name, member), src)
    line('W1', m, '%s -> s.%s through defaultOnSwitch' % (name, member))
line('W2', re.search(r'p->uni1f\( "cvMoveClamp", d\.moveClamp \? 1\.0f : 0\.0f \);', src), 'cvMoveClamp set every draw')
line('W2', re.search(r'p->uni1f\( "cvAccReset", d\.changeClamp \? V::kChangeReset : 0\.0f \);', src) and
     re.search(r'p->uni1f\( "cvAccResetSd", V::kChangeResetSd \);', src),
     'cvAccReset = kChangeReset (+ cvAccResetSd) with the accum twins')
line('W3', re.search(r'd\.moveClamp = s\.moveClamp != 0 && !red\( "staleread" \);', src) and
     re.search(r'd\.changeClamp = s\.changeClamp != 0 && !red\( "looseclamp" \);', src) and
     ',RED=staleread' in src and ',RED=looseclamp' in src and ',moveclamp=box' in src, 'reds + echoes')
line('W4', 'uniform float cvMoveClamp;' in frag and 'uniform float cvAccReset;' in frag and 'uniform float cvAccResetSd;' in frag and
     frag.count('cvMoveBox( q, uv, pv,') == 2, 'shader uniforms, both programs clamp the reprojected pixels')
if dump:
    for v, want in (('', 1), ('1', 1), ('on', 1), ('yes', 1), (' On ', 1), ('0', 0), ('off', 0), ('false', 0), ('no', 0),
                    ('OFF', 0)):
        env = dict(os.environ, PATH='C:/msys64/ucrt64/bin;' + os.environ['PATH'], WW_CLOUDVOL_MOVECLAMP=v,
                   WW_CLOUDVOL_CHANGECLAMP=v)
        out = subprocess.run([dump], env=env, capture_output=True, text=True).stdout
        gl = [l.split() for l in out.split('\n') if l.startswith('cloud6g ')]
        got = (int(gl[0][2]), int(gl[0][4])) if gl else (-1, -1)
        line('W5', got == (want, want), 'value %r -> moveclamp %d changeclamp %d (want %d)' % (v, got[0], got[1], want))
print('CLOUDSTALE-WIRING %s' % ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
