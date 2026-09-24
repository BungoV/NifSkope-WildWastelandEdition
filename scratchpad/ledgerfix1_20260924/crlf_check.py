"""For every tracked modified file: LF-only at HEAD must stay CR-free; a mixed file's CR delta must equal
(CRLF lines added - CRLF lines removed) in the byte diff. Prints only failures, then a verdict line."""
import subprocess
R = "E:/Projects/NifskopeWildWastelandEdition"
def git(*a):
    return subprocess.run(["git", "-C", R] + list(a), capture_output=True).stdout
mods = [l[3:] for l in git("status", "--porcelain", "-uno").decode("utf-8", "replace").splitlines() if l[:2].strip() in ("M", "RM", "R")]
mods = [m.split(" -> ")[-1] for m in mods]
bad, mixed = [], 0
for p in mods:
    head = git("show", "HEAD:" + p)
    if not head:  # renamed: find source
        continue
    cur = open(R + "/" + p, "rb").read()
    hc, cc = head.count(b"\r"), cur.count(b"\r")
    if hc == 0:
        if cc:
            bad.append((p, "LF-only at HEAD, now CR", cc))
        continue
    mixed += 1
    d = git("diff", "-U0", "--no-color", "--", p)
    add = sum(1 for l in d.split(b"\n") if l.startswith(b"+") and not l.startswith(b"+++") and l.endswith(b"\r"))
    rem = sum(1 for l in d.split(b"\n") if l.startswith(b"-") and not l.startswith(b"---") and l.endswith(b"\r"))
    if cc - hc != add - rem:
        bad.append((p, "mixed", hc, cc, add, rem))
print("tracked modified", len(mods), "mixed", mixed, "failures", len(bad))
for b in bad:
    print("  ", b)
