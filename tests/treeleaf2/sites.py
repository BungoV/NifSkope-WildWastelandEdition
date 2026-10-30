# lane TREELEAF2: text twin of the passes that alpha-test a base map. Every BaseMap bind of the colour pass
# (Renderer::setupProgramCE1) and of the side passes (sun cascades per draw + queued, light cubes, TAA inputs, reflection
# probes) must be armed with Shape::wwLeafCovArm( "<pass>" ) on the line before and disarmed on the line after, so the
# TexCache resolves the same "<file>|wwcov<ref>" key (one rule: Shape::wwLeafCovRef, src/gl/wwleafcov.h); every side
# threshold upload must come from Shape::wwLeafSideThreshold.
#   python -I sites.py <tree root>               the working tree (green: must PASS)
#   python -I sites.py <tree root> --rev <commit> the files at that commit (red: d05945bb, today's side passes, must FAIL)
import re, subprocess, sys

root = sys.argv[1]
rev = sys.argv[3] if len(sys.argv) > 3 and sys.argv[2] == '--rev' else None

# file -> (pass, function span start regex or None, expected BaseMap binds, expected threshold sites)
SITES = {
    'src/gl/renderer.cpp': ('colour', r'^bool Renderer::setupProgramCE1\(', 1, 0),
    'src/gl/sunshadow.cpp': ('sun', None, 2, 2),
    'src/gl/celllights.cpp': ('cube', None, 1, 1),
    'src/gl/taainputs.cpp': ('taa', None, 1, 1),
    'src/gl/cellreflprobe.cpp': ('refl', None, 1, 1),
}


def text(f):
    if rev:
        return subprocess.run(['git', '-C', root, 'show', '%s:%s' % (rev, f)], capture_output=True, check=True).stdout.decode('utf-8', 'replace')
    return open(root + '/' + f, 'rb').read().decode('utf-8', 'replace')


fails = 0
for f, (pas, span, wantBinds, wantThr) in SITES.items():
    L = text(f).replace('\r\n', '\n').split('\n')
    lo, hi = 0, len(L)
    if span:
        lo = next(i for i, s in enumerate(L) if re.search(span, s))
        hi = next((i for i in range(lo + 1, len(L)) if re.match(r'^bool Renderer::', L[i])), len(L))
    arm = 'wwLeafCovArm( "%s" )' % pas
    binds = ok = 0
    for i in range(lo, hi):
        s = L[i]
        if 'uniSampler(' in s and '"BaseMap"' in s:
            if pas == 'colour' and 'alt, clamp, *forced' not in s:
                continue
            binds += 1
            good = arm in L[i - 1] and 'wwLeafCovDisarm()' in L[i + 1]
            ok += good
            if not good:
                print('FAIL %s:%d BaseMap bind not armed for "%s": %s' % (f, i + 1, pas, s.strip()[:100]))
    thr = thrOk = 0
    for i in range(lo, hi):
        s = L[i]
        up = re.search(r'uni1f\( "alphaThreshold"', s)
        asg = re.search(r'\.atRef = ', s)
        if pas == 'colour' or not ( up or asg ):	# the colour pass's own test is the reference
            continue
        thr += 1
        good = 'wwLeafSideThreshold( ' in s and ('"%s"' % pas) in s or (up and re.search(r'\b[kc]\.atRef\b', s))
        # lane TREELEAF3: the TAA movers upload Shape::wwFxSideAlpha( da, "taa" ).threshold (wwLeafSideThreshold inside)
        good = good or (up and 'fx.threshold' in s and any(('wwFxSideAlpha( da, "%s" )' % pas) in t for t in L[lo:hi]))
        thrOk += bool(good)
        if not good:
            print('FAIL %s:%d side threshold not the colour test\'s: %s' % (f, i + 1, s.strip()[:100]))
    passed = binds == wantBinds and ok == binds and thrOk == thr and (wantThr == 0 or thr >= wantThr)
    fails += not passed
    print('%s %s: %d of %d BaseMap binds armed "%s" (want %d); %d of %d threshold sites from wwLeafSideThreshold' % (
        'ok  ' if passed else 'FAIL', f, ok, binds, pas, wantBinds, thrOk, thr))
print('TREELEAF2 SITES %s' % ('PASS' if fails == 0 else 'FAIL (%d files)' % fails))
sys.exit(1 if fails else 0)
