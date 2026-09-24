#!/usr/bin/env python3
"""HORIZONOUT: the two horizon headers leave NifSkope.pro.

Files are REMOVED from the project, so qmake has to run again before make.
"""
p = r"E:/Projects/NifskopeWildWastelandEdition/NifSkope.pro"
b = open(p, "rb").read()
cr = b.count(b"\r")
s = b.decode("utf-8")
old = "\tsrc/lodghorizon.h \\\n\tsrc/lodghorizonrefute.h \\\n"
assert s.count(old) == 1, "anchor count %d" % s.count(old)
s = s.replace(old, "")
assert "lodgsubdiv" not in s
assert "lodghorizon" not in s
out = s.encode("utf-8")
assert out.count(b"\r") == cr, (out.count(b"\r"), cr)
open(p, "wb").write(out)
print("NifSkope.pro: two header rows removed, CR %d unchanged" % cr)
