# Re-measure the two measured blocks at the foot of CHANGED_FILES.txt -- the
# NOT TOUCHED list and the LANE ARTEFACTS list -- from disk, after the DONE2
# round added files to both.  Everything above the "NOT TOUCHED" line is left
# exactly as it is.
import io, os, sys, time

ROOT = "E:/Projects/NifskopeWWE_ui"
P = os.path.join(ROOT, "scratchpad/uinotes2_20260912/CHANGED_FILES.txt")

NOT_TOUCHED = [
    "src/nifskope_ui.cpp",
    "res/style.qss",
    "src/nifskope.cpp",
    "src/lodgen.h",
    "src/nifcli.cpp",
    "src/lodgen.cpp",
    "src/lodgenmanager.cpp",
]

TAIL_NOTE = """
    src/nifskope.cpp is the mixed-CRLF file the brief names and its CR count above
    is the proof this lane never opened it. src/nifskope_ui.cpp is LF-only and was
    last written at 02:08, before this lane began. src/lodgen.h and src/nifcli.cpp
    carry lane ROADS4's 06:07 edits; they were never opened for writing here and
    are listed only because my build compiled them (report, section 2b).

LANE ARTEFACTS (new, all under scratchpad/, none of it product code), re-measured
after the DONE2 round; the patch scripts fix7..fix10, measure_lit_core.py, the
build_main3/4 and animws_done2* logs and the re-taken pictures are this round's:
"""

with io.open(P, "rb") as f:
    raw = f.read()
if raw.count(b"\r"):
    sys.exit("REFUSED: CHANGED_FILES.txt carries CR bytes")
s = raw.decode("utf-8")
anchor = "NOT TOUCHED, checked rather than assumed:\n"
if s.count(anchor) != 1:
    sys.exit("REFUSED: NOT TOUCHED anchor found %d times" % s.count(anchor))
head = s.split(anchor)[0] + anchor

lines = []
for rel in NOT_TOUCHED:
    p = os.path.join(ROOT, rel)
    d = io.open(p, "rb").read()
    mt = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(os.path.getmtime(p)))
    lines.append("    %-24s %9d B  CR %6d  LF %7d  mtime %s"
                 % (rel, len(d), d.count(b"\r"), d.count(b"\n"), mt))
body = "\n".join(lines) + "\n" + TAIL_NOTE

art = []
base = os.path.join(ROOT, "scratchpad")
for dirpath, dirnames, filenames in os.walk(base):
    dirnames.sort()
    for fn in sorted(filenames):
        full = os.path.join(dirpath, fn)
        rel = os.path.relpath(full, ROOT).replace("\\", "/")
        if not (rel.startswith("scratchpad/uinotes2_20260912/") or rel == "scratchpad/lane_uinotes2_report.md"):
            continue
        art.append("A   %-66s %9d" % (rel, os.path.getsize(full)))
art.sort(key=lambda l: l[4:])

out = (head + body + "\n".join(art) + "\n").encode("utf-8")
assert b"\r" not in out
with io.open(P, "wb") as f:
    f.write(out)
print("CHANGED_FILES.txt %d B  LF %d  (%d artefacts)" % (len(out), out.count(b"\n"), len(art)))
