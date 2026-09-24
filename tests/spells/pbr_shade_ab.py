"""pbr_shade_ab.py -- judge one pbr_shade_ab.sh run (lane PBRR0,
docs/NIFSKOPE_PBR_RENDERER.md s9).

Per case, from <out>/<case>_old_{a,b,c}.png and <case>_new.png:
  noise    OLD a|b and OLD a|c -> the bar = (max differing px, max |d|) of the two
  zero     NEW vs OLD a: size equal, px <= bar px, max <= bar max, and the frame
           is not empty (floor: >= 0.2% of pixels differ from the corner colour)
  census   WW_PROGRAM_CENSUS and WW_CAMERA_CENSUS identical OLD a vs NEW (and
           OLD a vs OLD b, the census's own noise); WW_PBRM_CENSUS identical when
           both arms write one, otherwise NEW-only with its floors: a header, >= 1
           row, and at stage=R0 every row route=legacy naming pbrmFeatureEnabled.
           Particle rows must name prog="particles.prog" or say "(not drawn: ...)".
  @empty   a case marked @empty=<reason> (measured: draws nothing visible here)
           passes zero only as PASS-EMPTY -- both arms empty and identical -- and
           FAILS if it ever draws, so the marker cannot outlive its reason.
  --red    shader: every case with pixels must FAIL zero, no census may fail;
           census: every case whose OLD program census names fo4_default.prog
           must FAIL census.
Writes <case>_diff.png (OLD | NEW | |d| x16) and one verdict line per gate to
stdout and <out>/verdicts.txt. Exit 0 = every gate PASS (or, with --red, every
case FAILED the gate the sabotage aims at while the other stayed PASS).
"""
import argparse, hashlib, os, re, sys
import numpy as np
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument("--out", required=True)
ap.add_argument("--red", default="none")
a = ap.parse_args()
OUT = a.out
lines, fails = [], 0


def say(s):
    print(s)
    lines.append(s)


def sha8(p):
    return hashlib.sha1(open(p, "rb").read()).hexdigest()[:8]


def img(p):
    return np.asarray(Image.open(p).convert("RGB"), dtype=np.int16)


def dstat(x, y):
    if x.shape != y.shape:
        return None
    d = np.abs(x - y)
    m = d.max(axis=2)
    px = int((m > 0).sum())
    return dict(px=px, frac=px / float(m.size), max=int(d.max()), mean=float(d.mean()), d=d)


def rows(p):
    if not os.path.exists(p):
        return None
    return [l.rstrip("\n") for l in open(p, encoding="utf-8", errors="replace") if l.strip()]


def census_key(p):
    r = rows(p)
    return None if r is None else sorted(set(r))


cases = []
if os.path.exists(os.path.join(OUT, "cases.run")):
    for l in open(os.path.join(OUT, "cases.run")):
        f = l.split()
        if f:
            d = dict(w[1:].split("=", 1) if "=" in w else (w[1:], "") for w in f[2:] if w.startswith("@"))
            cases.append((f[0], f[1], "MISSING" in f[2:], d))

kinds, refusals = set(), set()
zero_fail = census_fail = 0
empties, aim_shader, aim_census, zero_failed, census_failed = [], set(), set(), set(), set()
for name, gate, missing, dirs in cases:
    empty_ok = dirs.get("empty")
    P = lambda s: os.path.join(OUT, "%s_%s" % (name, s))
    if missing:
        say("pbr_shade_ab %s gate=%s fixture missing -> FAIL" % (name, gate))
        fails += 1; zero_fail += 1; zero_failed.add(name)
        continue
    need = [P("old_a.png"), P("old_b.png"), P("new.png")]
    absent = [os.path.basename(x) for x in need if not os.path.exists(x)]
    if absent:
        say("pbr_shade_ab %s gate=%s no picture: %s -> FAIL" % (name, gate, ",".join(absent)))
        fails += 1; zero_fail += 1; zero_failed.add(name)
        continue
    oa, ob, nw = img(P("old_a.png")), img(P("old_b.png")), img(P("new.png"))
    noise = [dstat(oa, ob)]
    if os.path.exists(P("old_c.png")):
        noise.append(dstat(oa, img(P("old_c.png"))))
    if any(n is None for n in noise):
        say("pbr_shade_ab %s gate=%s OLD runs differ in SIZE (the machine, not the code) -> FAIL" % (name, gate))
        fails += 1; zero_fail += 1; zero_failed.add(name)
        continue
    bar_px = max(n["px"] for n in noise)
    bar_max = max(n["max"] for n in noise)
    st = dstat(oa, nw)
    h, w = oa.shape[:2]
    corner = oa[0, 0]
    content = float((np.abs(oa - corner).max(axis=2) > 0).mean())
    prog_new = rows(P("new.prog.txt")) or []
    progs = sorted({m.group(1) for r in prog_new for m in [re.search(r"prog=(\S+)", r)] if m})
    progs_s = ",".join(progs) if progs else "none"

    # diff picture: OLD | NEW | |d| x16
    if st is not None:
        dd = np.clip(st["d"] * 16, 0, 255).astype(np.uint8)
        Image.fromarray(np.concatenate([oa.astype(np.uint8), nw.astype(np.uint8), dd], axis=1)).save(P("diff.png"))

    if gate == "zero":
        if st is None:
            verdict, why = False, " size mismatch %s vs %s" % (oa.shape[:2], nw.shape[:2])
            st = dict(px=-1, frac=-1, max=-1, mean=-1)
        else:
            why = ""
            verdict = st["px"] <= bar_px and st["max"] <= bar_max
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
        say("pbr_shade_ab %s gate=zero old=%s new=%s size=%dx%d px=%d frac=%.6f max=%d mean=%.5f "
            "bar=%d/%d noise=%s content=%.3f progs=%s ->%s %s"
            % (name, sha8(P("old_a.png")), sha8(P("new.png")), w, h, st["px"], st["frac"], st["max"],
               st["mean"], bar_px, bar_max, "|".join("%d/%d" % (n["px"], n["max"]) for n in noise),
               content, progs_s, why, ("PASS-EMPTY" if name in empties else "PASS") if verdict else "FAIL"))
        if not verdict:
            fails += 1; zero_fail += 1; zero_failed.add(name)
    else:
        say("pbr_shade_ab %s gate=%s not built in R0 -> FAIL" % (name, gate))
        fails += 1; zero_fail += 1; zero_failed.add(name)

    # ---- census
    notes, ok = [], True
    for tag, suf in (("prog", "prog.txt"), ("cam", "cam.txt")):
        ka, kb, kn = census_key(P("old_a." + suf)), census_key(P("old_b." + suf)), census_key(P("new." + suf))
        if tag == "prog" and ka and any("fo4_default.prog" in r for r in ka):
            aim_census.add(name)
        if tag == "prog" and ka is None and kb is None and kn is None:
            # nothing went through setupProgram in any run (particles draw via useProgram)
            notes.append("prog=none(all runs)")
        elif ka is None or kn is None:
            notes.append("%s=ABSENT" % tag); ok = False
        elif ka != kb:
            notes.append("%s=OLD-UNSTABLE" % tag); ok = False
        elif ka != kn:
            notes.append("%s=DIFF(%d vs %d rows)" % (tag, len(ka), len(kn))); ok = False
        else:
            notes.append("%s=same(%d)" % (tag, len(kn)))
    po, pn = rows(P("old_a.pbrm.txt")), rows(P("new.pbrm.txt"))
    if pn is None:
        notes.append("pbrm=ABSENT"); ok = False
    else:
        hdr = [r for r in pn if r.startswith("# WW_PBRM_CENSUS")]
        body = [r for r in pn if not r.startswith("#")]
        if len(hdr) != 1 or not body:
            notes.append("pbrm=NO-HEADER-OR-ROWS"); ok = False
        else:
            if "stage=R0" in hdr[0]:
                bad = [r for r in body if "route=legacy" not in r or "pbrmFeatureEnabled=false" not in r]
                if bad:
                    notes.append("pbrm=R0-NOT-LEGACY(%d)" % len(bad)); ok = False
            elif "mode=legacy" in hdr[0]:
                # R1+: the display default is Legacy (ruling Q9), so every row a
                # Legacy run writes is served by the legacy route.
                bad = [r for r in body if "route=legacy" not in r]
                if bad:
                    notes.append("pbrm=LEGACY-MODE-SERVED-PBR(%d)" % len(bad)); ok = False
            badp = [r for r in body if "kind=particles" in r
                    and 'prog="particles.prog"' not in r and 'prog="(not drawn' not in r]
            if badp:
                notes.append("pbrm=PARTICLES-NOT-particles.prog(%d)" % len(badp)); ok = False
            drawn = sum(1 for r in body if "kind=particles" in r and 'prog="particles.prog"' in r)
            undrawn = sum(1 for r in body if "kind=particles" in r and 'prog="(not drawn' in r)
            if drawn or undrawn:
                notes.append("particles=%d drawn/%d not" % (drawn, undrawn))
            for r in body:
                m = re.search(r"kind=(\S+)", r)
                kinds.add(m.group(1) if m else "?")
                m = re.search(r'refusal="([^"]*)"', r)
                refusals.add(m.group(1) if m else "?")
            ho = [r for r in (po or []) if r.startswith("# WW_PBRM_CENSUS")]
            so = re.search(r"stage=(\S+)", ho[0]).group(1) if ho else None
            sn = re.search(r"stage=(\S+)", hdr[0]).group(1)
            if po is None:
                notes.append("pbrm=new-only(%d rows; old arm predates it)" % len(body))
            elif so != sn:
                # a new stage writes new fields and refusal wording by design; what
                # must not move across stages is WHAT was drawn and by WHICH route
                proj = lambda rs: sorted({" ".join(m.group(0) for m in re.finditer(
                    r'(?:shape|kind|prog|route)=("[^"]*"|\S+)', r)) for r in rs if not r.startswith("#")})
                if proj(po) != proj(pn):
                    notes.append("pbrm=DIFF(%s->%s shape/kind/prog/route)" % (so, sn)); ok = False
                else:
                    notes.append("pbrm=same-projection(%d; %s->%s shape/kind/prog/route)" % (len(body), so, sn))
            elif sorted(set(po)) != sorted(set(pn)):
                notes.append("pbrm=DIFF"); ok = False
            else:
                notes.append("pbrm=same(%d)" % len(body))
    say("pbr_shade_ab %s census %s -> %s" % (name, " ".join(notes), "PASS" if ok else "FAIL"))
    if not ok:
        fails += 1; census_fail += 1; census_failed.add(name)

# ---- the census must MOVE across the fixtures (rule 2026-09-04 21:33 #1)
if len(cases) >= 3:
    moves = len(kinds) >= 2 and len(refusals) >= 2
    say("pbr_shade_ab census-moves kinds=%s distinct_refusals=%d -> %s"
        % (",".join(sorted(kinds)) or "none", len(refusals), "PASS" if moves else "FAIL"))
    if not moves:
        fails += 1

n = len(cases)
if a.red == "none":
    say("pbr_shade_ab SUMMARY %d cases, %d failures, %d empty by the viewer (%s) -> %s"
        % (n, fails, len(empties), ",".join(empties) or "none", "PASS" if fails == 0 else "FAIL"))
    rc = 0 if fails == 0 else 1
else:
    # aimed set: shader -> every case with pixels (not @empty); census -> every
    # case whose OLD program census names fo4_default.prog (the sabotaged .prog)
    aim = aim_shader if a.red == "shader" else aim_census
    hit = sorted(x for x in aim if x in (zero_failed if a.red == "shader" else census_failed))
    other = sorted(census_failed) if a.red == "shader" else []
    bites = len(aim) > 0 and len(hit) == len(aim) and not other
    say("pbr_shade_ab RED CONTROL %s: %d of %d aimed cases FAILED the aimed gate (%s), %d failed the other%s -> %s"
        % (a.red, len(hit), len(aim), ",".join(sorted(aim)), len(other), (" (" + ",".join(other) + ")") if other else "",
           "BITES (control PASS)" if bites else "DOES NOT BITE (control FAIL)"))
    rc = 0 if bites else 1
open(os.path.join(OUT, "verdicts.txt"), "w").write("\n".join(lines) + "\n")
sys.exit(rc)
