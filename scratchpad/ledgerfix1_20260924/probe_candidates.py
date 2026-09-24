import subprocess, re
R = "E:/Projects/NifskopeWildWastelandEdition/"
FH = "C:/Users/bungo/.claude/file-history/843169d8-671d-4705-ba26-ce53a8012252/5fcfe493b278bdd9@v76"
def head(p):
    return subprocess.run(["git", "-C", R, "show", "720762a:" + p], capture_output=True).stdout
for name, cand in (("HANDOFF.md", FH), ("WW_CHANGES.md", R + "scratchpad/build8_20260910/WW_CHANGES.md.bak")):
    c = open(cand, "rb").read()
    h = head(name)
    # how much of the 09-09 tail is a suffix of candidate (compare LF-normalised)
    cn, hn = c.replace(b"\r\n", b"\n"), h.replace(b"\r\n", b"\n")
    tail_ok = cn.endswith(hn[-20000:])
    # longest common suffix
    n = 0
    while n < min(len(cn), len(hn)) and cn[-1 - n] == hn[-1 - n]:
        n += 1
    dates = sorted(set(re.findall(rb"2026-09-\d\d", c)))[-3:]
    print(name, "cand", len(c), "CR", c.count(b"\r"), "LF", c.count(b"\n"), "| head0909", len(h), "CR", h.count(b"\r"),
          "| tail20k_suffix", tail_ok, "common_suffix_LFnorm", n, "| last_dates", [d.decode() for d in dates],
          "| ends_nl", c.endswith(b"\n"), "last40", c[-40:])
