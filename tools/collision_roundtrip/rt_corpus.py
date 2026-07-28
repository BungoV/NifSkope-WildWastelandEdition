"""Run the encoder round-trip across the WHOLE mesh tree, not just actor skeletons.

Every number claimed so far -- 819 capsules, 68 polytopes, 30 spheres -- comes from
39 actor skeletons. TO_BE_IMPLEMENTED says in as many words that architecture,
SetDressing, SCOL and Landscape are unmeasured and are where compressed meshes,
hulls and compounds actually live. So this tests the claims against the data they
were NOT derived from, which is the only way they can fail informatively.

Deterministic stride so the sample is reproducible and proportional per category.
"""
import os
import re
import subprocess
import sys
from collections import Counter, defaultdict

EXE = os.environ.get( "NIFSKOPE_EXE",
	os.path.join( os.path.dirname( os.path.dirname( os.path.dirname(
		os.path.abspath( __file__ ) ) ) ), "release", "NifSkope.exe" ) )
ROOT = os.environ.get( "FO4_MESHES", os.path.join(
	"E:" + os.sep, "Tools", "Fallout 4", "DataUnpacked", "Data", "meshes" ) )
WANT = int( sys.argv[1] ) if len( sys.argv ) > 1 else 700

PAT = {
    "polytopes": re.compile(r"polytopes\s+(\d+)\s+byte-exact (\d+)"),
    "spheres": re.compile(r"spheres\s+(\d+)\s+byte-exact (\d+)"),
    "massprops": re.compile(r"massprops\s+(\d+)\s+byte-exact (\d+)"),
    "compounds": re.compile(r"compounds\s+(\d+)\s+byte-exact (\d+)"),
}
CAPS = re.compile(r"capsules\s+(\d+)")
CAPSOK = re.compile(r"structure byte-exact\s+(\d+)")
INERT = re.compile(r"\(\+(\d+) differing")
VERR = re.compile(r"worst vertex error\s+([0-9.e+-]+)")


def main():
    files = []
    for dirpath, _, names in os.walk(ROOT):
        for n in names:
            if n.lower().endswith(".nif"):
                files.append(os.path.join(dirpath, n))
    files.sort()
    stride = max(1, len(files) // WANT)
    sample = files[::stride][:WANT]

    seen = Counter()
    exact = Counter()
    inert = 0
    worstVert = 0.0
    bad = []
    cat = Counter()
    catHit = Counter()
    timeouts = 0

    for i, f in enumerate(sample):
        try:
            p = subprocess.run([EXE, "-no-gui", "collision", f, "--roundtrip"],
                               capture_output=True, text=True, timeout=120)
        except subprocess.TimeoutExpired:
            timeouts += 1
            continue
        out = p.stdout
        c = f[len(ROOT) + 1:].split(os.sep)[0]
        cat[c] += 1

        hit = False
        for key, rx in PAT.items():
            m = rx.search(out)
            if m:
                seen[key] += int(m.group(1))
                exact[key] += int(m.group(2))
                hit = True
                # an all-zero packed vector keeps whatever exponent Havok landed on,
                # which is inert and unrecoverable -- not a failure
                shortfall = int(m.group(1)) - int(m.group(2))
                if key == "massprops":
                    mi = INERT.search(out)
                    shortfall -= int(mi.group(1)) if mi else 0
                if shortfall > 0:
                    bad.append((key, f, m.group(0)))
        mi = INERT.search(out)
        if mi:
            inert += int(mi.group(1))
        mc, mo = CAPS.search(out), CAPSOK.search(out)
        if mc and int(mc.group(1)) and mo:
            seen["capsules"] += int(mc.group(1))
            exact["capsules"] += int(mo.group(1))
            hit = True
            if int(mo.group(1)) < int(mc.group(1)):
                bad.append(("capsules", f, mo.group(0)))
        mv = VERR.search(out)
        if mv:
            worstVert = max(worstVert, float(mv.group(1)))
        if hit:
            catHit[c] += 1

        if (i + 1) % 100 == 0:
            print("  ... %d/%d" % (i + 1, len(sample)), flush=True)

    print("\nsampled %d of %d nif files (stride %d), %d timeouts"
          % (len(sample), len(files), stride, timeouts))
    print("files carrying an encodable shape: %d\n" % sum(catHit.values()))
    print("%-12s %8s %10s %s" % ("type", "seen", "byte-exact", ""))
    for k in ("compounds", "polytopes", "spheres", "massprops", "capsules"):
        if not seen[k]:
            continue
        note = "  (+%d inert exponent)" % inert if k == "massprops" and inert else ""
        note += "  [structure only]" if k == "capsules" else ""
        print("%-12s %8d %10d%s" % (k, seen[k], exact[k], note))
    print("\nworst capsule vertex error: %.3g m" % worstVert)

    print("\ncategories sampled: %s" % dict(cat.most_common(8)))
    print("categories WITH collision: %s" % dict(catHit.most_common(8)))

    if bad:
        print("\n%d file/type combinations NOT byte-exact:" % len(bad))
        for key, f, txt in bad[:20]:
            print("   %-10s %-70s %s" % (key, f[len(ROOT) + 1:][:70], txt))
    else:
        print("\nno mismatches")


if __name__ == "__main__":
    main()
