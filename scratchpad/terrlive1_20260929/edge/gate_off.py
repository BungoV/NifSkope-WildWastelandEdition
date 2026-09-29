"""Lane TERRLIVE1 rule paint: the OFF byte gate.

gate_off.py <reference mod dir> <candidate mod dir> [--expect-red]

Every file under the reference must exist under the candidate and be sha1-identical.
The ONLY mask: the flat-objects report's "# Override file:" row, which names the run
folder. New files in the candidate (e.g. a .lodr) are listed, and they make an OFF
gate red (OFF must add nothing). Exit 0 = the verdict matched the expectation.
"""
import hashlib
import os
import sys

ref, cand = sys.argv[1], sys.argv[2]
expect_red = '--expect-red' in sys.argv


def files(root):
    out = {}
    for d, _, fs in os.walk(root):
        for f in fs:
            p = os.path.join(d, f)
            out[os.path.relpath(p, root).replace(os.sep, '/')] = p
    return out


def digest(path):
    b = open(path, 'rb').read()
    if path.endswith('flat_objects_report.txt'):
        b = b'\n'.join(l for l in b.split(b'\n') if not l.startswith(b'# Override file:'))
    return hashlib.sha1(b).hexdigest(), len(b)


R, C = files(ref), files(cand)
findings = []
for rel in sorted(R):
    if rel not in C:
        findings.append('missing in candidate: ' + rel)
        continue
    dr, dc = digest(R[rel]), digest(C[rel])
    print('%-55s %s %12d  %s' % (rel, 'SAME' if dr == dc else 'DIFF', dc[1], dc[0][:12]))
    if dr != dc:
        findings.append('differs: %s (%d vs %d bytes)' % (rel, dr[1], dc[1]))
for rel in sorted(set(C) - set(R)):
    findings.append('new in candidate: %s (%d bytes)' % (rel, os.path.getsize(C[rel])))
print('findings: %d' % len(findings))
for f in findings[:8]:
    print('  ' + f)
red = bool(findings)
print('VERDICT: %s%s' % ('RED' if red else 'GREEN', ' (expected red)' if expect_red else ''))
sys.exit(0 if red == expect_red else 1)
