"""Count which cubemap vanilla BGSM files name (any length-prefixed string containing 'cubemaps')."""
import os, re, collections
ROOT = r"E:/Tools/Fallout 4/DataUnpacked/Data/materials"
pat = re.compile(rb"[\x20-\x7e]*[Cc]ube[Mm]aps[\\/][\x20-\x7e]+?\.dds", re.I)
c = collections.Counter(); files = 0; with_env = 0
for dp, dn, fn in os.walk(ROOT):
    for f in fn:
        if not f.lower().endswith(".bgsm"): continue
        files += 1
        b = open(os.path.join(dp, f), "rb").read()
        m = pat.search(b)
        if m:
            with_env += 1
            s = m.group(0).decode("latin1").lower().replace("/", "\\")
            s = s[s.find("shared\\") if "shared\\" in s else 0:]
            c[s] += 1
print(f"BGSM files={files} naming a cubemap={with_env}")
print("TOP:", c.most_common(6))
