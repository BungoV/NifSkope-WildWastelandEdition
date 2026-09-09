#!/usr/bin/env python3
import sys
PATH = "E:/Projects/Claude/.claude/skills/nifskope-ww-render-shot/SKILL.md"
OLD = b"WW_LOD_CHANNEL for the generated vertex channels), off every screen, one instance at a time --"
NEW = (b"WW_LOD_CHANNEL for the generated vertex channels), one instance at a time and never a desktop"
       b" capture (the off-screen placement is written but INERT as built -- read the first section) --")
with open(PATH, "rb") as fh:
    b = fh.read()
cr0 = b.count(b"\r")
if b.count(OLD) != 1:
    print("ABORT: anchor count %d" % b.count(OLD)); sys.exit(2)
b = b.replace(OLD, NEW)
if b.count(b"\r") != cr0:
    print("ABORT: CR moved"); sys.exit(2)
with open(PATH, "wb") as fh:
    fh.write(b)
print("OK description amended, %d bytes" % len(b))
