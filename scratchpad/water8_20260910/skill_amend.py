#!/usr/bin/env python3
"""Lane WATER8-GATE: amend nifskope-ww-resume-pending section 5 with the
one-copy rule the double gate-chain launch of 2026-09-11 paid for.

Refusing script (ww-anchored-hookup): exact-once anchor carrying the file's own
line ending, CR byte assert, --check writes nothing.
"""
import sys, pathlib

P = pathlib.Path(".claude/skills/nifskope-ww-resume-pending/SKILL.md")
ANCHOR = b"## 6. When a gate fails, measure the cause and STOP"
MARKER = b"### Exactly one copy of the chain, proved by the process list"

NEW = b"""### Exactly one copy of the chain, proved by the process list

Two copies of the same chain script ran at once on 2026-09-11 (lane
WATER8-GATE) and every harness after the first collided on its FIXED port:
`animws.sh` "the harness wrote no log (did the app exit before it ran, or is
port 42317 bound?)", `water_mark.sh` "no dock log". The summary file interleaved
two runs, which is the tell -- `### water_ui start 05:23:53` printed next to
`### files_tab rc=1 05:23:55`.

* **An empty log is not "did not start".** A chain that has been alive for two
  seconds has written nothing yet. The only honest answer is the process list,
  by command line, the same filter `nifskope-ww-build-verify` uses for NifSkope:

```bash
powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \\"Name='bash.exe'\\" | Where-Object { \\$_.CommandLine -like '*gates.sh*' } | Select-Object ProcessId, CommandLine | Format-List"
```

* **Never redirect a launcher into a directory the launched script creates.**
  `bash chain.sh > $OUT/logs/stdout.log` fails before the script runs, because
  the shell opens the redirect first -- and it looks exactly like a script that
  refused to start. `mkdir -p` the log directory in the launching shell.
* **Give the chain script its own lock**, three lines, so a second copy is
  impossible rather than merely unlikely:

```bash
mkdir "$OUT/.lock" 2>/dev/null || { echo "REFUSED: a chain is already running ($OUT/.lock)"; exit 8; }
trap 'rmdir "$OUT/.lock" 2>/dev/null' EXIT
```

* When it happens anyway, KEEP the ruined run beside the good one
  (`logs_void_<reason>/`) instead of deleting it: the interleaved timestamps are
  the evidence that the numbers were rubbish, and a reader of the report will
  want them.

"""

check = "--check" in sys.argv
b = P.read_bytes()
cr0, n0 = b.count(b"\r"), len(b)
if b.count(MARKER) == 1:
    print("REFUSED: already applied (marker present x1)")
    sys.exit(0)
n = b.count(ANCHOR)
print("anchor x%d | CR %d | %d bytes" % (n, cr0, n0))
if n != 1:
    print("REFUSED: the anchor does not match exactly once")
    sys.exit(1)
out = b.replace(ANCHOR, NEW + ANCHOR)
assert out.count(b"\r") == cr0, "CR count moved"
assert len(out) > n0
if check:
    print("check only, wrote nothing; would grow %d -> %d bytes" % (n0, len(out)))
    sys.exit(0)
P.write_bytes(out)
b2 = P.read_bytes()
print("written: CR %d, %d bytes, marker x%d" % (b2.count(b"\r"), len(b2), b2.count(MARKER)))
