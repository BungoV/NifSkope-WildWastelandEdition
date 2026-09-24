#!/usr/bin/env python3
"""Lane BUILD10 -- the water window's self-test held a DELETED document.

`WaterWindow::openFile` does `delete doc; doc = new WaterMarkDoc();`
(`src/waterwindow.cpp` 1610), and `runWindowSelfTest` captured
`WaterMarkDoc * doc = win->document();` ONCE at the top (line 2232) and then
reopened the file at W3 (line 2382). Every use of that local pointer after the
reopen -- W4's `hashWords( *doc, ... )` first -- was a use-after-free.

It did not crash before lane WATER6 because `WaterMarkDoc::sweep`'s first act
was to read a bool and a pointer out of the freed block, which happened to
survive; WATER6 made `sweep` begin by comparing two QVector members
(`syncRasters`), and the harness died with a segmentation fault (exit 139)
straight after the override check, taking gates W4, W5, W6 and the summary
count with it.

This repairs the INSTRUMENT -- one line, no check, no assertion and no widget
touched -- so the gates can run at all. The defect is lane WATER5's and is
written up in `MISTAKES.md` as such.
"""
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
P = os.path.join(ROOT, "src", "waterwindow.cpp")

OLD = ("\t\tcheck( QStringLiteral( \"the land file reopens\" ), win->openFile( file ) );\n"
       "\t\tWaterCurveDoc fromStore = win->model();\n")
NEW = ("\t\tcheck( QStringLiteral( \"the land file reopens\" ), win->openFile( file ) );\n"
       "\t\t/* openFile DELETES the old document and makes a new one, so the local\n"
       "\t\t * `doc` captured at the top of this function is dangling from here on.\n"
       "\t\t * Everything below used it -- W4 first (lane BUILD10, 2026-09-10). */\n"
       "\t\tdoc = win->document();\n"
       "\t\tWaterCurveDoc fromStore = win->model();\n")

b = open(P, "rb").read()
cr = b.count(b"\r")
n = b.count(OLD.encode("utf-8"))
print("anchor count=%d CR=%d bytes=%d" % (n, cr, len(b)))
assert n == 1
out = b.replace(OLD.encode("utf-8"), NEW.encode("utf-8"))
assert out.count(b"\r") == cr == 0
open(P, "wb").write(out)
print("wrote src/waterwindow.cpp %d -> %d bytes, CR 0" % (len(b), len(out)))
