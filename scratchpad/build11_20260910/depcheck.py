#!/usr/bin/env python
"""Lane BUILD11: read qmake's regenerated dependency lists back, per OBJECT, by name.

nifskope-ww-resume-pending section 3: `grep -A3` misses a dependency ten
continuation lines down, so walk each object's whole block.
"""
import re

BS = chr(92)   # a backslash, never through a heredoc (MISTAKES.md)

lines = open("Makefile.Release", encoding="utf-8", errors="replace").read().split("\n")
deps, cur = {}, None
for line in lines:
    m = re.match(r"^(GeneratedFiles/\.obj/[A-Za-z0-9_]+\.o):(.*)$", line)
    if m:
        cur = m.group(1)
        deps[cur] = m.group(2)
    elif cur is not None:
        if line.startswith("\t\t") or line.rstrip().endswith(BS):
            deps[cur] += " " + line
        else:
            cur = None

WANT = [
    ("hkxmodel.o", ["src/hkxmodel.h", "src/hkxfile.h", "src/hkxanim.h"]),
    ("hkxfile.o", ["src/hkxfile.h"]),
    ("hkxmodeltest.o", ["src/hkxmodel.h"]),
    ("animworkspace.o", ["src/animworkspace.h", "src/hkxclipedit.h", "src/animdopesheet.h"]),
    ("animworkspacetest.o", ["src/animworkspace.h", "src/hkxclipedit.h"]),
    ("animdopesheet.o", ["src/hkxclipedit.h", "src/animdopesheet.h"]),
    ("hkxclipedit.o", ["src/hkxclipedit.h"]),
    ("nifskope_ui.o", ["src/animworkspace.h", "src/hkxmodel.h", "src/nifskope.h", "src/glview.h"]),
    ("nifskope.o", ["src/animworkspace.h", "src/hkxmodel.h", "src/nifskope.h"]),
    ("glnode.o", ["src/nifskope.h"]),
    ("skeloverlaytest.o", ["src/glview.h"]),
]

bad = 0
for name, wants in WANT:
    o = "GeneratedFiles/.obj/" + name
    d = deps.get(o)
    if d is None:
        print("MISSING OBJECT BLOCK  " + o)
        bad += 1
        continue
    row = []
    for w in wants:
        ok = w in d
        row.append("%s=%s" % (w.split("/")[-1], "yes" if ok else "NO"))
        if not ok:
            bad += 1
    print("%-26s %s" % (name, "  ".join(row)))
print("DEPCHECK missing=%d" % bad)
