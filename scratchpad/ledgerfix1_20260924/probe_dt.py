import re, os, time
R = "E:/Projects/NifskopeWildWastelandEdition/scratchpad/"
for f in ["pbrr1", "lightangles1", "pbrlodfix1", "pbrr2a", "pbrr2b", "pbrr3", "pbrr4", "pdbscrub1", "pbrwx1"]:
    p = R + f + "_20260924/DELIVERABLE_TEXT.md"
    t = open(p, encoding="utf-8").read().replace("\r\n", "\n")
    heads = re.findall(r"^## [^\n]*", t, re.M)
    done = R + f + "_20260924/DONE.md"
    dm = time.strftime("%H:%M", time.localtime(os.path.getmtime(done))) if os.path.exists(done) else "-"
    first = {}
    for h in heads:
        m = re.search("^" + re.escape(h) + r"\n(.*?)(?=^## |\Z)", t, re.S | re.M)
        body = m.group(1).strip("\n")
        first[h] = (len(body), body.split("\n")[0][:90])
    print(f, "DONE", dm, "| DT", time.strftime("%H:%M", time.localtime(os.path.getmtime(p))))
    for h, (n, l) in first.items():
        print("   ", h[:40], n, "|", l)
