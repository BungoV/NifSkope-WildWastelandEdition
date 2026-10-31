"""Lane CLOUDLIGHT1: the top light's column is wired as gated (tests/spells/cloudlight_check.py judges cvTopOn 2).
    python -I tests/cloudlight/wiring_check.py <cloudvol.cpp> <cloud_march_step.glsl> <tests/spells/cloudvol_check.py>
W1 cs() reads WW_CLOUDVOL_TOPCOLUMN through V::defaultOnSwitch (the DEFAULT, live); W2 marchPass sets cvTopOn 2 with it,
1 for the taps, 0 for the red notop; W3 the red toptaps and the echoes; W4 the step's column branch (cvTopOn > 1.5, no
cvDens read in it) beside the taps; W5 the checkers draw the default (cloudvol_check march_uni cvTopOn 2, toptaps 1).
Red: the base's files (git show 563dcf97:...): FAIL."""
import re, sys

src = open(sys.argv[1], 'rb').read().decode('latin-1')
step = open(sys.argv[2], 'rb').read().decode('latin-1')
chk = open(sys.argv[3], 'rb').read().decode('latin-1')
ok = True


def line(tag, good, msg):
    global ok
    ok &= bool(good)
    print('%s %s  %s' % (tag, 'PASS' if good else 'FAIL', msg))


line('W1', re.search(r'const QByteArray (\w+) = wwEnvStr\( "WW_CLOUDVOL_TOPCOLUMN" \)\.trimmed\(\)\.toLower\(\)\.toLatin1\(\);'
                     r'\s*s\.topColumn = V::defaultOnSwitch\( \1\.constData\(\) \) \? 1 : 0;', src), 'cs() reads the switch')
line('W2', re.search(r'p->uni1f\( "cvTopOn", red\( "notop" \) \? 0\.0f : d\.topColumn \? 2\.0f : 1\.0f \);', src),
     'cvTopOn 0 / 2 / 1')
line('W3', re.search(r'd\.topColumn = s\.topColumn != 0 && !red\( "toptaps" \);', src) and ',RED=toptaps' in src and
     ',topcolumn=on' in src, 'red toptaps + echoes')
m = re.search(r'if \( cvTopOn > 1\.5 \) \{(.*?)\} else \{', step, re.S)
line('W4', m and 'cvDens' not in m.group(1) and 'tauB = dens * kCvSigma' in m.group(1), 'the column branch reads nothing')
line('W5', "1.0 if red == 'toptaps' else 2.0" in chk, 'the checkers draw cvTopOn 2 (toptaps 1)')
print('CLOUDLIGHT-WIRING %s' % ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
