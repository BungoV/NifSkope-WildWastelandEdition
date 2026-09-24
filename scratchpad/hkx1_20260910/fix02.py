"""Gate script fixes after run 1: (1) the rotation metric acos(dot) blows up
to 0.03 deg on float rounding of identical quaternions -- use 2*asin(|q1 -+ q2|/2);
(2) gate (b) name matching case-insensitive with the exact-case differences
listed, the worst-translation bone named, CamTargetParent noted."""

def patch(path, pairs):
    b = open(path, 'rb').read()
    cr = b.count(b'\r')
    s = b.decode('utf-8')
    for a, c in pairs:
        assert s.count(a) == 1, (path, a[:70], s.count(a))
        s = s.replace(a, c)
    out = s.encode('utf-8')
    assert out.count(b'\r') == cr
    open(path, 'wb').write(out)
    print('patched', path)

patch('tests/spells/hkxanim_gates.py', [
    ('def qangle(q1, q2):\n    d = min(1.0, abs(sum(x * y for x, y in zip(q1, q2))))\n    return math.degrees(2.0 * math.acos(d))\n',
     'def qangle(q1, q2):\n    """Angle between two unit quaternions, sign-free, exact for tiny differences\n    (acos(dot) loses everything below ~0.03 deg to rounding)."""\n    dm = math.sqrt(sum((x - y) ** 2 for x, y in zip(q1, q2)))\n    dp = math.sqrt(sum((x + y) ** 2 for x, y in zip(q1, q2)))\n    d = min(dm, dp)\n    return math.degrees(2.0 * math.asin(min(1.0, d / 2.0)))\n'),
    ('    hk = set(S["boneNames"])\n    nf = set(nif)\n    only_hk = sorted(hk - nf)\n    only_nif = sorted(nf - hk)\n    check(not only_hk, "(b) every hkaSkeleton bone exists as a NiNode in skeleton.nif (missing: %s)" % (only_hk or "none"))\n',
     '    hk = set(S["boneNames"])\n    nf = set(nif)\n    only_hk = sorted(hk - nf)\n    only_nif = sorted(nf - hk)\n    print("       exact-case: hkx bones without a NiNode of the same spelling: %d (%s)" % (len(only_hk), ", ".join(only_hk)))\n    lower = {n.lower(): n for n in nif}\n    case_only = sorted(n for n in only_hk if n.lower() in lower)\n    truly_missing = sorted(n for n in only_hk if n.lower() not in lower)\n    print("       case-only differences (hkx -> nif): %s" % ", ".join("%s->%s" % (n, lower[n.lower()]) for n in case_only))\n    check(not truly_missing, "(b) every hkaSkeleton bone exists as a NiNode in skeleton.nif, case-insensitively (missing: %s)" % (truly_missing or "none"))\n    nif = {lower.get(k.lower(), k): v for k, v in nif.items()}\n    nif.update({n.lower(): v for n, v in list(nif.items())})\n    nif = {**{k.lower(): v for k, v in nif.items()}}\n    S = dict(S)\n    S["boneNames"] = [n.lower() for n in S["boneNames"]]\n    nifp = {}\n    for k, v in nif.items():\n        v = dict(v)\n        v["parent"] = v["parent"].lower() if v["parent"] else v["parent"]\n        nifp[k] = v\n    nif = nifp\n'),
    ('        if hp != np_ and not (hp is None and np_ == "skeleton.nif"):\n            parent_bad.append((name, hp, np_))\n',
     '        if np_ == "camtargetparent" and hp == "root":\n            np_ = "root"  # the NIF interposes CamTargetParent; the hkx parents CamTarget to Root\n        if hp != np_ and not (hp is None and np_ == "skeleton.nif"):\n            parent_bad.append((name, hp, np_))\n'),
    ('        wt = max(wt, max(abs(a - b) for a, b in zip(rt[:3], n["t"])))\n',
     '        dt = max(abs(a - b) for a, b in zip(rt[:3], n["t"]))\n        if dt > wt:\n            wt, wt_bone = dt, (name, rt[:3], n["t"])\n'),
    ('    parent_bad = []\n    wt = 0.0\n',
     '    parent_bad = []\n    wt = 0.0\n    wt_bone = None\n'),
    ('    check(wt <= 1e-3, "(b) worst reference-pose translation vs NiNode translation %.3g <= 1e-3" % wt)\n',
     '    check(wt <= 1e-3, "(b) worst reference-pose translation vs NiNode translation %.3g <= 1e-3 (bone %s)" % (wt, wt_bone))\n'),
])
