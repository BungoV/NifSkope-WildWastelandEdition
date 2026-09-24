"""After git add: staged numstat summary; flags any tracked file whose staged diff deletes > 50% of its HEAD lines
(a whole-file rewrite), any staged file over 5 MB, and any game-data path. One verdict block."""
import subprocess, re
R = "E:/Projects/NifskopeWildWastelandEdition"
def git(*a):
    return subprocess.run(["git", "-C", R] + list(a), capture_output=True).stdout.decode("utf-8", "replace")
ns = [l.split("\t") for l in git("diff", "--cached", "--numstat").splitlines() if l]
files, add, rem, flags = len(ns), 0, 0, []
for a, d, p in ns:
    if a == "-":
        flags.append(("binary", p)); continue
    add += int(a); rem += int(d)
    if int(d) > 20:
        head = git("show", "HEAD:" + p)
        n = head.count("\n")
        if n and int(d) > 0.5 * n:
            flags.append(("rewrite", p, d, n))
sizes = git("diff", "--cached", "--name-only", "-z").split("\0")
import os
for p in sizes:
    if p and os.path.exists(R + "/" + p):
        if os.path.getsize(R + "/" + p) > 5 * 2**20:
            flags.append(("over5MB", p))
        if re.search(r"(?i)\.(esm|esp|esl|ba2|bsa)$", p):
            flags.append(("gamedata", p))
print("staged files", files, "+%d -%d" % (add, rem), "flags", len(flags))
for f in flags[:20]:
    print("  ", f)
