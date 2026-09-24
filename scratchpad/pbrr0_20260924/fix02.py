import sys
p = r"E:\Projects\NifskopeWildWastelandEdition\tests\spells\pbr_shade_ab.py"
b = open(p, "rb").read()
assert b.count(b"\r") == 0
def rep(old, new):
    global b
    assert b.count(old) == 1, old[:60]
    b = b.replace(old, new)
rep(b"""        if f:
            cases.append((f[0], f[1], len(f) > 2 and f[2] == "MISSING"))
""", b"""        if f:
            d = dict(w[1:].split("=", 1) if "=" in w else (w[1:], "") for w in f[2:] if w.startswith("@"))
            cases.append((f[0], f[1], "MISSING" in f[2:], d))
""")
rep(b"""zero_fail = census_fail = 0
for name, gate, missing in cases:""", b"""zero_fail = census_fail = 0
empties, aim_shader, aim_census = [], set(), set()
for name, gate, missing, dirs in cases:
    empty_ok = dirs.get("empty")""")
rep(b"""            verdict = st["px"] <= bar_px and st["max"] <= bar_max
            if content < 0.002:
                verdict, why = False, " EMPTY FRAME (content %.4f < 0.002 floor)" % content
""", b"""            verdict = st["px"] <= bar_px and st["max"] <= bar_max
            content_new = float((np.abs(nw - nw[0, 0]).max(axis=2) > 0).mean())
            if empty_ok and content < 0.002 and content_new < 0.002:
                # measured empty in this viewer: byte-identical says nothing about pixels
                why = " EMPTY BY THE VIEWER (%s; guards no particle pixel)" % empty_ok
                if verdict:
                    empties.append(name)
            elif empty_ok:
                verdict, why = False, " marked @empty=%s but DREW (content old %.4f new %.4f): drop the marker" % (
                    empty_ok, content, content_new)
            elif content < 0.002:
                verdict, why = False, " EMPTY FRAME (content %.4f < 0.002 floor)" % content
            else:
                aim_shader.add(name)
""")
rep(b"""              content, progs_s, why, "PASS" if verdict else "FAIL"))""",
    b"""              content, progs_s, why, ("PASS-EMPTY" if name in empties else "PASS") if verdict else "FAIL"))""")
rep(b"""        ka, kb, kn = census_key(P("old_a." + suf)), census_key(P("old_b." + suf)), census_key(P("new." + suf))
        if ka is None or kn is None:""", b"""        ka, kb, kn = census_key(P("old_a." + suf)), census_key(P("old_b." + suf)), census_key(P("new." + suf))
        if tag == "prog" and ka and any("fo4_default.prog" in r for r in ka):
            aim_census.add(name)
        if tag == "prog" and ka is None and kb is None and kn is None:
            # nothing went through setupProgram in any run (particles draw via useProgram)
            notes.append("prog=none(all runs)")
        elif ka is None or kn is None:""")
rep(b"""            for r in body:
                m = re.search(r"kind=(\S+)", r)""", b"""            badp = [r for r in body if "kind=particles" in r
                    and 'prog="particles.prog"' not in r and 'prog="(not drawn' not in r]
            if badp:
                notes.append("pbrm=PARTICLES-NOT-particles.prog(%d)" % len(badp)); ok = False
            drawn = sum(1 for r in body if "kind=particles" in r and 'prog="particles.prog"' in r)
            undrawn = sum(1 for r in body if "kind=particles" in r and 'prog="(not drawn' in r)
            if drawn or undrawn:
                notes.append("particles=%d drawn/%d not" % (drawn, undrawn))
            for r in body:
                m = re.search(r"kind=(\S+)", r)""")
rep(b"""    say("pbr_shade_ab SUMMARY %d cases, %d failures -> %s" % (n, fails, "PASS" if fails == 0 else "FAIL"))""",
    b"""    say("pbr_shade_ab SUMMARY %d cases, %d failures, %d empty by the viewer (%s) -> %s"
        % (n, fails, len(empties), ",".join(empties) or "none", "PASS" if fails == 0 else "FAIL"))""")
rep(b"""    aim_fail = zero_fail if a.red == "shader" else census_fail
    other = census_fail if a.red == "shader" else 0
    bites = n > 0 and aim_fail == n and other == 0
    say("pbr_shade_ab RED CONTROL %s: %d of %d cases FAILED the aimed gate, %d failed the other -> %s"
        % (a.red, aim_fail, n, other, "BITES (control PASS)" if bites else "DOES NOT BITE (control FAIL)"))""",
    b"""    # aimed set: shader -> every case with pixels (not @empty); census -> every
    # case whose OLD program census names fo4_default.prog (the sabotaged .prog)
    aim = aim_shader if a.red == "shader" else aim_census
    hit = sorted(x for x in aim if x in (zero_failed if a.red == "shader" else census_failed))
    other = sorted(census_failed) if a.red == "shader" else []
    bites = len(aim) > 0 and len(hit) == len(aim) and not other
    say("pbr_shade_ab RED CONTROL %s: %d of %d aimed cases FAILED the aimed gate (%s), %d failed the other%s -> %s"
        % (a.red, len(hit), len(aim), ",".join(sorted(aim)), len(other), (" (" + ",".join(other) + ")") if other else "",
           "BITES (control PASS)" if bites else "DOES NOT BITE (control FAIL)"))""")
open(p, "wb").write(b)
print("ok")
