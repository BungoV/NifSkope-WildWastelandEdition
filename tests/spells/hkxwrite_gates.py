#!/usr/bin/env python3
"""Lane HKX5's gate suite: the glTF importer (src/gltfimport.cpp) and the .hkx
writer (src/hkxwrite.cpp).  Every gate was pre-registered in the lane brief.

  (a) ROUND TRIP 1  a shipped spline clip -> decode -> write interleaved ->
                    decode again; every bone at every frame within 1e-4 units
                    and 0.01 degrees.  Run on both write routes.
  (b) ROUND TRIP 2  clip -> glTF (lane HKX4's exporter) -> import -> write ->
                    decode, same bars, restricted to the bones the exporter
                    could carry.
  (c) A hand-written 3-bone glTF with hand-computed numbers imports exactly
      (LINEAR + STEP + CUBICSPLINE in one file).
  (d) Corruptions of the glTF and of the written .hkx, each refused BY NAME,
      plus the floor that shows the comparator can go red.
  (e) The written .hkx re-read by HKXPACK: the interleaved class, the expected
      transforms count and the 48-byte stride.
  (f) The Mixamo fixture (empty binding, 60 fps) through round trip 1.

Run from the repo root:  python tests/spells/hkxwrite_gates.py
Needs release/hkxwrite_dump.exe (scratchpad/hkx5_20260910/build_dump.sh) and,
for (b) and (e), release/gltfexport_dump.exe and HKXPACK.
"""
import os, subprocess, sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
LANE = os.path.join(REPO, 'scratchpad', 'hkx5_20260910')
OUT = os.path.join(LANE, 'out')
EXE = os.path.join(REPO, 'release', 'hkxwrite_dump.exe')
EXPORT = os.path.join(REPO, 'release', 'gltfexport_dump.exe')
CLIPS = os.path.join(REPO, 'scratchpad', 'hkx1_20260910', 'clips')
JAR = r"E:/Tools/Fallout 4/HKXPACK/hkxpack-cli.jar"
SKELNIF = r"E:/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/character/CharacterAssets/skeleton.nif"

sys.path.insert(0, LANE)
import interleaved_decode as dec            # noqa: E402
import tsvcmp                                # noqa: E402

TOL_T, TOL_DEG = 1e-4, 0.01
results = []


def run(args, **kw):
    p = subprocess.run(args, capture_output=True, text=True, cwd=REPO, **kw)
    return p.returncode, (p.stdout or '') + (p.stderr or '')


def note(name, ok, detail=""):
    results.append((name, ok, detail))
    print("  %-52s %s %s" % (name, "PASS" if ok else "FAIL", detail))


def gate_a():
    print("\n== gate (a) ROUND TRIP 1: shipped clip -> interleaved -> decode ==")
    fixtures = [("jog", "jog.hkx"), ("turn", "turn.hkx"), ("twoblock", "twoblock.hkx"),
                ("q48", "q48.hkx"), ("tpose_idle", "tpose_idle.hkx")]
    for route in ("b", "a"):
        label = "direct" if route == "b" else "HKXPACK"
        for nm, fn in fixtures:
            src = os.path.join(CLIPS, fn)
            if not os.path.exists(src):
                note("(a) %s %s" % (nm, label), False, "fixture missing")
                continue
            hkx = os.path.join(OUT, "a_%s_%s.hkx" % (nm, route))
            stsv = os.path.join(OUT, "a_%s_src.tsv" % nm)
            rc, out = run([EXE, 'write', src, hkx, '--route', route, '--tsv', stsv])
            if rc != 0:
                note("(a) %s %s" % (nm, label), False, out.strip().splitlines()[-1][:100])
                continue
            dtsv = os.path.join(OUT, "a_%s_%s.tsv" % (nm, route))
            try:
                d = dec.decode(hkx, quiet=True)
                dec.write_tsv(d, dtsv)
            except dec.Refusal as e:
                note("(a) %s %s" % (nm, label), False, "decode refused: %s" % e)
                continue
            ok, m = tsvcmp.compare(stsv, dtsv, TOL_T, TOL_DEG, "    (a) %s via %s" % (nm, label))
            note("(a) %s via %s" % (nm, label), ok,
                 "%d rows, dT %.2e, %.2e deg" % (m.get('rows', 0), m.get('maxT', 9), m.get('maxDeg', 9)) if m else "")


def gate_b():
    print("\n== gate (b) ROUND TRIP 2: clip -> glTF -> import -> write -> decode ==")
    if not os.path.exists(EXPORT):
        note("(b) round trip 2", False, "release/gltfexport_dump.exe is not built (lane HKX4)")
        return
    gltf = os.path.join(OUT, "rt2_jog.gltf")
    rc, out = run([EXPORT, '--skeleton', SKELNIF, '--clip', os.path.join(CLIPS, 'jog.hkx'),
                   '--bones', os.path.join(CLIPS, 'skeleton.hkx'), '--name', 'jog', '--out', gltf])
    if rc != 0:
        note("(b) export", False, out.strip().splitlines()[-1][:120])
        return
    hkx = os.path.join(OUT, "rt2_jog.hkx")
    rc, out = run([EXE, 'import', gltf, hkx, '--bones', os.path.join(OUT, 'skeleton_bones.txt'),
                   '--no-static-tracks', '--fps', '30'])
    if rc != 0:
        note("(b) import", False, out.strip().splitlines()[-1][:120])
        return
    tsv = os.path.join(OUT, "rt2_jog.tsv")
    d = dec.decode(hkx, quiet=True)
    dec.write_tsv(d, tsv)
    ok, m = tsvcmp.compare(os.path.join(OUT, "a_jog_src.tsv"), tsv, TOL_T, TOL_DEG,
                           "    (b) jog", bykey='bone', only_common=True, ignore_root=True)
    note("(b) round trip 2 (jog, 78 bones the exporter carries)", ok,
         "%d rows, dT %.2e, %.2e deg" % (m.get('rows', 0), m.get('maxT', 9), m.get('maxDeg', 9)) if m else "")


def gate_c():
    print("\n== gate (c) hand-written 3-bone glTF, known numbers ==")
    rc, out = run([sys.executable, os.path.join(LANE, 'make_3bone.py'), 'write', os.path.join(OUT, '3bone.gltf')])
    hkx = os.path.join(OUT, '3bone.hkx')
    rc, out = run([EXE, 'import', os.path.join(OUT, '3bone.gltf'), hkx, '--fps', '30'])
    if rc != 0:
        note("(c) 3-bone import", False, out.strip().splitlines()[-1][:120])
        return
    d = dec.decode(hkx, quiet=True)
    dec.write_tsv(d, os.path.join(OUT, '3bone.tsv'))
    rc, out = run([sys.executable, os.path.join(LANE, 'make_3bone.py'), 'check', os.path.join(OUT, '3bone.tsv')])
    print("   " + out.strip().replace("\n", "\n   "))
    note("(c) 3-bone glTF imports to the hand-computed values", rc == 0)


def gate_d():
    print("\n== gate (d) corruptions refused by name, and the floor ==")
    rc, out = run([sys.executable, os.path.join(LANE, 'mutate.py'), os.path.join(OUT, '3bone.gltf'),
                   os.path.join(OUT, '3bone.hkx'), os.path.join(OUT, '3bone.tsv')])
    last = [l for l in out.splitlines() if l.startswith('gate (d):')]
    print("   " + out.strip().replace("\n", "\n   "))
    note("(d) corruptions + floor", rc == 0, last[0] if last else "")


def gate_e():
    print("\n== gate (e) HKXPACK re-reads our own direct-route file ==")
    hkx = os.path.join(OUT, "a_jog_b.hkx")
    xml = os.path.join(OUT, "e_jog_reread.xml")
    if not os.path.exists(JAR):
        note("(e) HKXPACK re-read", False, "HKXPACK is not at %s" % JAR)
        return
    rc, out = run(['java', '-jar', JAR, 'unpack', hkx, '-o', xml])
    if rc != 0 or not os.path.exists(xml):
        note("(e) HKXPACK re-read", False, out.strip()[:150])
        return
    text = open(xml, encoding='latin-1').read()
    d = dec.decode(hkx, quiet=True)
    want = d['tracks'] * d['frames']
    checks = [
        ('class="hkaInterleavedUncompressedAnimation"' in text, "the interleaved class"),
        ('signature="0xa5eff3f2"' in text, "signature 0xa5eff3f2"),
        ('name="transforms" numelements="%d"' % want in text, "transforms numelements=%d" % want),
        ('name="numberOfTransformTracks">%d' % d['tracks'] in text, "numberOfTransformTracks=%d" % d['tracks']),
        ('name="transformTrackToBoneIndices" numelements="%d"' % d['tracks'] in text, "binding maps %d" % d['tracks']),
    ]
    # the 48-byte stride, read from the file itself
    stride_ok = False
    import struct
    b = open(hkx, 'rb').read()
    pf = dec.read_packfile(b)
    for off, cls, _ in pf['objs']:
        if cls == 'hkaInterleavedUncompressedAnimation':
            p, n = dec.arr(pf, off + 0x38, 48, "transforms")
            nxt = min([o for o, _, _ in pf['objs'] if o > off] + [pf['dt'][1]])
            stride_ok = (n == want) and (p + 48 * n <= nxt or p + 48 * n <= pf['dt'][1])
    checks.append((stride_ok, "48-byte hkQsTransform stride fits the payload"))
    for ok, what in checks:
        note("(e) %s" % what, ok)


def gate_f():
    print("\n== gate (f) the Mixamo fixture through round trip 1 ==")
    src = os.path.join(REPO, 'fixtures', 'Running_To_Slide_And_Back_To_Running.hkx')
    if not os.path.exists(src):
        note("(f) Mixamo fixture", False, "fixture missing")
        return
    hkx = os.path.join(OUT, "f_mixamo.hkx")
    stsv = os.path.join(OUT, "f_mixamo_src.tsv")
    rc, out = run([EXE, 'write', src, hkx, '--tsv', stsv])
    if rc != 0:
        note("(f) Mixamo write", False, out.strip().splitlines()[-1][:140])
        return
    d = dec.decode(hkx, quiet=True)
    dtsv = os.path.join(OUT, "f_mixamo.tsv")
    dec.write_tsv(d, dtsv)
    ok, m = tsvcmp.compare(stsv, dtsv, TOL_T, TOL_DEG, "    (f) Mixamo")
    note("(f) Mixamo round trip 1", ok,
         "%d rows, dT %.2e, %.2e deg" % (m.get('rows', 0), m.get('maxT', 9), m.get('maxDeg', 9)) if m else "")
    fps = 1.0 / d['frameDuration']
    note("(f) 60 fps preserved (round trip 1 never resamples)", abs(fps - 60.0) < 0.01, "%.4f fps" % fps)
    note("(f) the empty binding was written out as an explicit identity map",
         (not d['identityBinding']) and d['bones'] == list(range(d['tracks'])),
         "binding maps %d tracks 0..%d" % (d['tracks'], d['tracks'] - 1))
    rm = d['rootmotion']
    mx = max(max(abs(c) for c in s) for s in rm['samples']) if rm else -1
    note("(f) root motion carried and still zero (lane FIXTURE's finding)", rm is not None and mx == 0.0,
         "%d samples, max |component| %g" % (len(rm['samples']), mx) if rm else "absent")


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    if not os.path.exists(EXE):
        print("release/hkxwrite_dump.exe is not built; run scratchpad/hkx5_20260910/build_dump.sh")
        sys.exit(9)
    gate_a()
    gate_b()
    gate_c()
    gate_d()
    gate_e()
    gate_f()
    npass = sum(1 for _, ok, _ in results if ok)
    print("\n==== lane HKX5 gates: %d/%d ====" % (npass, len(results)))
    for name, ok, detail in results:
        if not ok:
            print("  FAILED: %s %s" % (name, detail))
    sys.exit(0 if npass == len(results) else 1)
