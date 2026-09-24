"""UINOTES2 step 6 -- amend nifskope-ww-build-verify with the half-written exe.

The skill already says make's own exit code gates the chain. It does not say
what a BACKGROUNDED build costs you when you stop watching the exit code and
start watching the file, which is what happened here at 06:55: the timestamp
moved, `make -q` said 0, `cmp` on the stylesheet passed, and two harness runs
were spent on an exe whose first two bytes were 00 00 because the linker had not
finished. Three minutes later it began MZ and the same harness gave 224 / 0.

Refusing script: exact-once anchor, all-or-nothing. This file is LF-only.
"""
import os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, ".claude", "skills", "nifskope-ww-build-verify", "SKILL.md")

with open(SRC, "rb") as f:
    orig = f.read()
cr = orig.count(b"\r")
text = orig.decode("utf-8")

anchor = (
	"* One build at a time (~4 min); the Bash tool's effective timeout is 600 s, so background the chain\n"
	"  and read `tasks/<id>.output`. A failed chain leaves earlier scripts APPLIED: check which steps\n"
	"  wrote before re-running.\n"
)
rep = anchor + (
	"* **A BACKGROUNDED build is finished when its exit code says so, and at no other moment**\n"
	"  (2026-09-12, lane UINOTES2). Once the chain is in the background it is tempting to gate on\n"
	"  what you can see from outside it: the exe's timestamp moving, `make -q` returning 0, `cmp` on\n"
	"  the stylesheet passing. All three are true of a half-written exe. A link in progress had\n"
	"  already stamped `release/NifSkope.exe` with a fresh mtime and 21,850,624 bytes while its first\n"
	"  two bytes were still `00 00`; the harness reported \"wrote no log\" on two ports before the exe\n"
	"  began `MZ` and the same harness gave 224 / 0. Wait for `BUILD-RC=` in the task output. If a\n"
	"  run must start before that, its first two checks are `head -c 2 release/NifSkope.exe` = `MZ`\n"
	"  and `BUILD-RC=0` present in the log.\n"
	"* **Read the build log for what ELSE was rebuilt before crediting a change** (2026-09-12, lane\n"
	"  UINOTES2). A header another lane edited in the same tree pulls its whole dependency fan into\n"
	"  your build: one lane's two-file UI change compiled nine translation units, five of them\n"
	"  lodgen's, so the new exe was not \"the old exe plus my diff\" and a crash that vanished could\n"
	"  not be credited to the diff. `grep -oE \"\-o GeneratedFiles/\.obj/[a-z_0-9]+\.o\" <build log>`\n"
	"  names them in one line.\n"
)

if text.count(anchor) != 1:
	sys.exit("refused: anchor x%d (want 1); nothing written" % text.count(anchor))

text = text.replace(anchor, rep, 1)
out = text.encode("utf-8")
assert out.count(b"\r") == cr, "line endings changed"
with open(SRC, "wb") as f:
	f.write(out)
print("written %s: CR %d LF %d bytes %d (was %d)" % (SRC, out.count(b"\r"), out.count(b"\n"), len(out), len(orig)))
