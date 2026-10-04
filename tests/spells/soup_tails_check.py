#!/usr/bin/env python3
"""Lane LAND5: the probe soup's optional tails after TWO1 are SIZED (u32 magic, u32 count, u32 body bytes, body),
in the fixed order AMK1, EMT1, VNM1, DRG1 (any new tail appended last), and every reader skips a tail it does not
know by its byte count (src/probeplace.h, the AMK1 comment). Synthetic scene from probe_bake.py, no game data.

  soup_tails_check.py <nifskope.exe> <work dir> [--prtp <prtp_reference.exe>] [--red unsized]

Soups: A = the synth scene (ALB1 + TWO1); B = A + VNM1 (the ground's vertex normals tilted); C = A + an unknown
sized tail + VNM1 + another unknown sized tail.
PASS: B's bake differs from A's (the VNM1 tail is read), C's bake is byte-identical to B's (the unknown tails are
skipped by length), both Python readers parse C, and prtp_reference (when given) writes the same table for B and C
and a different one for A.
Red --red unsized: C's first unknown tail is written WITHOUT its byte count (the pre-LAND5 framing), so the reader
cannot find VNM1: the gate must FAIL.
"""
import os
import struct
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import probe_bake as pb   # noqa: E402

VNM_MAGIC = 0x314D4E56
JUNK_MAGIC = 0x315A5A5A   # 'ZZZ1': no reader knows it


def sized(magic, count, body):
    return struct.pack('<III', magic, count, len(body)) + body


def vnm_tail(n):
    vn = np.zeros((n, 9), dtype='<i2')
    t = np.array([0.35, 0.2, 0.9])
    t = t / np.linalg.norm(t)
    q = np.round(t * 32767).astype('<i2')
    for tri in (0, 1):   # scene(): triangles 0 and 1 are the ground
        vn[tri] = np.tile(q, 3)
    return sized(VNM_MAGIC, n, vn.tobytes())


def main():
    a = sys.argv[1:]
    opt = (lambda k: a[a.index(k) + 1] if k in a else '')
    if len(a) < 2:
        print(__doc__)
        return 2
    exe, work = a[0], a[1]
    prtp, red = opt('--prtp'), opt('--red')
    os.makedirs(work, exist_ok=True)
    tris, alb = pb.scene()
    n = len(tris)
    base_path = os.path.join(work, 'tails_A.psp')
    pb.write_soup(base_path, tris, alb)
    base = open(base_path, 'rb').read()
    junk1 = sized(JUNK_MAGIC, 3, b'\x11' * 7)
    junk2 = sized(JUNK_MAGIC, 1, b'\x22' * 12)
    if red == 'unsized':
        junk1 = struct.pack('<II', JUNK_MAGIC, 3) + b'\x11' * 7
    soups = {'A': base, 'B': base + vnm_tail(n), 'C': base + junk1 + vnm_tail(n) + junk2}
    paths = {}
    for k, body in soups.items():
        paths[k] = os.path.join(work, 'tails_%s.psp' % k)
        open(paths[k], 'wb').write(body)
    fails = []
    outs = {}
    for k in 'ABC':
        out = pb.fresh(os.path.join(work, 'tails_%s' % k))
        rc = pb.run_bake(exe, paths[k], out, 256, 0, '')
        if rc.returncode != 0:
            print('soup_tails FAIL: probebake %s rc %d %s' % (k, rc.returncode, rc.stderr.strip()[:300]))
            return 1
        outs[k] = out
        print('bake %s: %d .tbk' % (k, len(pb.files_in(out))))
    if not pb.files_in(outs['A']):
        fails.append('no .tbk written')
    if pb.same_files(outs['A'], outs['B']):
        fails.append('VNM1 tail changed nothing (B == A): the reader did not find it')
    if not pb.same_files(outs['B'], outs['C']):
        fails.append('unknown sized tails changed the bake (C != B): not skipped by length')
    # the Python readers the cell gates use
    # D = C with an AMK1 tail (no maps, every record unmasked) between the first unknown tail and VNM1: the readers
    # must walk past the unknown tail to find it
    amk = sized(0x314B4D41, n, struct.pack('<II', 0, 0) + b''.join(
        struct.pack('<IiiI6f', t, -1, -1, 0, *([0.0] * 6)) for t in range(n)))
    paths['D'] = os.path.join(work, 'tails_D.psp')
    open(paths['D'], 'wb').write(base + junk1 + amk + vnm_tail(n) + junk2)
    try:
        import alphatest_check
        import emissive_cell_check
        for k in 'CD':
            _, t1, am1 = alphatest_check.read_soup(paths[k])
            t2, am2, _ = emissive_cell_check.read_soup(paths[k])
            if len(t1) != n or len(t2) != n:
                fails.append('python reader on %s: triangle count wrong' % k)
            got = [am is not None and len(am['rec']) == n for am in (am1, am2)]
            if got != [k == 'D'] * 2:
                fails.append('python readers on %s: AMK1 found %s, want %s' % (k, got, k == 'D'))
        print('python readers: alphatest_check + emissive_cell_check parse C, and find AMK1 past the unknown tail in D')
    except Exception as e:   # noqa: BLE001
        fails.append('python reader on C: %r' % (e,))
    if prtp:
        probes = os.path.join(work, 'tails_probes.txt')
        x0, y0, z0, x1, y1, z1 = pb.ROOM
        with open(probes, 'w') as f:
            for x in (x0 + 150, (x0 + x1) / 2, x1 - 150):
                f.write('%g %g %g\n' % (x, (y0 + y1) / 2, z0 + 120))
        tab = {}
        for k in 'ABC':
            o = os.path.join(work, 'tails_prtp_%s.tsv' % k)
            rc = subprocess.run([os.path.abspath(prtp), paths[k], probes, '256', '0.3', '0.2', '1', o],
                                capture_output=True, text=True, timeout=600)
            if rc.returncode != 0:
                fails.append('prtp_reference %s rc %d %s' % (k, rc.returncode, rc.stderr.strip()[:200]))
                continue
            tab[k] = open(o, 'rb').read()
        if len(tab) == 3:
            if tab['A'] == tab['B']:
                fails.append('prtp_reference: VNM1 changed nothing (B == A)')
            if tab['B'] != tab['C']:
                fails.append('prtp_reference: unknown sized tails changed the table (C != B)')
            print('prtp_reference: A %s B, B %s C' % ('!=' if tab['A'] != tab['B'] else '==',
                                                     '==' if tab['B'] == tab['C'] else '!='))
    for f in fails:
        print('  fail: ' + f)
    print('soup_tails %s%s' % ('PASS' if not fails else 'FAIL', ' (red %s)' % red if red else ''))
    return 0 if not fails else 1


if __name__ == '__main__':
    sys.exit(main())
