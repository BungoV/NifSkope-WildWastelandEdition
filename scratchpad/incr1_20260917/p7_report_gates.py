#!/usr/bin/env python
# INCR1 step 7 -- append section 11 (the before/after gate table) to the lane
# report.  The report is pure LF; this writes pure LF and re-measures.
#
#   python p7_report_gates.py <report> <gates_after_dir>
#
# The AFTER numbers are READ OUT OF THE LOGS, never typed from memory: the
# summary file gives rc and seconds, each log gives its own check/failure
# counts in its own house format.  The BEFORE column is quoted from
# ARCHLOCK1's recorded table and is labelled as quoted, not re-measured.
import io
import os
import re
import sys

LF = chr(10)
TAB = chr(9)


def read(p):
    with io.open(p, "rb") as f:
        return f.read().decode("utf-8", "replace")


def counts(text):
    """Return (checks, failures) the way the harnesses themselves count."""
    m = re.search(r"(\d+) checks?, (\d+) failures?", text)
    if m:
        return int(m.group(1)), int(m.group(2))
    ok = len(re.findall(r"^\s*ok\s", text, re.M))
    bad = len(re.findall(r"^\s*FAIL\s", text, re.M))
    return ok + bad, bad


def main():
    rep, gdir = sys.argv[1], sys.argv[2]
    summary = read(os.path.join(gdir, "summary.txt"))
    rows = []
    for line in summary.split(LF):
        parts = line.split()
        if len(parts) == 3 and parts[1].startswith("rc="):
            name, rc, secs = parts[0], parts[1][3:], parts[2].rstrip("s")
            log = os.path.join(gdir, name + ".txt")
            c, f = counts(read(log)) if os.path.exists(log) else (0, 0)
            rows.append((name, c, f, secs, rc))
    if not rows:
        sys.stderr.write("no gate rows in summary.txt" + LF)
        return 2
    for name, c, f, secs, rc in rows:
        sys.stdout.write("%s checks=%d failures=%d %ss rc=%s%s"
                         % (name, c, f, secs, rc, LF))
    if "--emit" not in sys.argv:
        return 0

    body = read(sys.argv[sys.argv.index("--emit") + 1])
    old = read(rep)
    assert old.count(chr(13)) == 0, "report already has CR"
    assert body.count(chr(13)) == 0, "body has CR"
    assert old.count("## 11.") == 0, "section 11 already present"
    with io.open(rep, "wb") as f:
        f.write((old.rstrip(LF) + LF + LF + body.rstrip(LF) + LF).encode("utf-8"))
    n = read(rep)
    sys.stdout.write("report %d bytes, %d LF, %d CR%s"
                     % (len(n.encode("utf-8")), n.count(LF), n.count(chr(13)), LF))
    return 0


if __name__ == "__main__":
    sys.exit(main())
