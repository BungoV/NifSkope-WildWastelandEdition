"""Lane CLOUD6h: History Age / Lit Clamp are wired as gated (tests/spells/cloudage_check.py judges the shader + the
uniforms the dump says cloudvol.cpp sets; this checks that cloudvol.cpp really sets them that way).
    python -I tests/cloudage/wiring_check.py <cloudvol.cpp> <cloud_resolve.frag> [<cloudband_dump.exe>]   exit 0 = all hold
W1 cs() reads WW_CLOUDVOL_HISTAGE and WW_CLOUDVOL_LITCLAMP through V::defaultOnSwitch (the DEFAULT, live: cs() re-reads on
   any WW_CLOUDVOL* set); W2 resolvePass sets cvHistAge = V::kHistAgeOld, cvAgeK, cvLitReset = V::kLitReset and cvLitFloor
   with the accum twins on every draw; W3 the reds noage / litblind turn them off and the echoes name them; W4 the depth
   target is RG32F (the age's channel) and the shader declares the four uniforms; W5 the dump's switch rule.
Red: the base's cloudvol.cpp + cloud_resolve.frag (git show 563dcf97:...): W1-W4 FAIL."""
import os, re, subprocess, sys

src = open(sys.argv[1], 'rb').read().decode('latin-1')
frag = open(sys.argv[2], 'rb').read().decode('latin-1')
dump = sys.argv[3] if len(sys.argv) > 3 else ''
ok = True


def line(tag, good, msg):
    global ok
    ok &= bool(good)
    print('%s %s  %s' % (tag, 'PASS' if good else 'FAIL', msg))


for name, member in (('WW_CLOUDVOL_HISTAGE', 'histAge'), ('WW_CLOUDVOL_LITCLAMP', 'litClamp')):
    m = re.search(r'const QByteArray (\w+) = wwEnvStr\( "%s" \)\.trimmed\(\)\.toLower\(\)\.toLatin1\(\);\s*'
                  r's\.%s = V::defaultOnSwitch\( \1\.constData\(\) \) \? 1 : 0;' % (name, member), src)
    line('W1', m, '%s -> s.%s through defaultOnSwitch' % (name, member))
line('W2', re.search(r'p->uni1f\( "cvHistAge", d\.histAge \? V::kHistAgeOld : 0\.0f \);', src) and
     re.search(r'p->uni1f\( "cvAgeK", V::kAgeChangeK \);', src) and
     re.search(r'p->uni1f\( "cvLitReset", d\.litClamp \? V::kLitReset : 0\.0f \);', src) and
     re.search(r'p->uni1f\( "cvLitFloor", V::kLitFloor \);', src), 'the four uniforms set with the accum twins')
line('W3', re.search(r'd\.histAge = s\.histAge != 0 && !red\( "noage" \);', src) and
     re.search(r'd\.litClamp = s\.litClamp != 0 && !red\( "litblind" \);', src) and
     ',RED=noage' in src and ',RED=litblind' in src and ',histage=' in src and ',litclamp=' in src, 'reds + echoes')
line('W4', 'make( t.dep, GL_RG32F, GL_RG );' in src and all('uniform float %s;' % u in frag for u in
     ('cvHistAge', 'cvAgeK', 'cvLitReset', 'cvLitFloor')), 'RG32F depth target, shader uniforms')
if dump:
    for v, want in (('', 1), ('1', 1), ('on', 1), ('yes', 1), (' On ', 1), ('0', 0), ('off', 0), ('false', 0), ('no', 0),
                    ('OFF', 0)):
        env = dict(os.environ, PATH='C:/msys64/ucrt64/bin;' + os.environ['PATH'], WW_CLOUDVOL_HISTAGE=v,
                   WW_CLOUDVOL_LITCLAMP=v)
        out = subprocess.run([dump], env=env, capture_output=True, text=True).stdout
        hl = [l.split() for l in out.split('\n') if l.startswith('cloud6h ')]
        got = (int(hl[0][2]), int(hl[0][8])) if hl else (-1, -1)
        line('W5', got == (want, want), 'value %r -> histage %d litclamp %d (want %d)' % (v, got[0], got[1], want))
print('CLOUDAGE-WIRING %s' % ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
