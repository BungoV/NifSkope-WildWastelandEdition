"""Director: apply PLAN-FO4CS's amendment (new section 2a) to ww-census-contract in BOTH skill trees."""
import re
A = "E:/Projects/NifskopeWildWastelandEdition/scratchpad/plan_fo4cs_20260911/SKILL_AMENDMENT_ww_census_contract.md"
TREES = ["E:/Projects/NifskopeWildWastelandEdition/.claude/skills/ww-census-contract/SKILL.md",
         "E:/Projects/Claude/.claude/skills/ww-census-contract/SKILL.md"]
am = open(A, "rb").read()
assert b"\r" not in am
k = am.find(b"\n## 2a")
assert k >= 0, "no '## 2a' section in the amendment file"
body = am[k + 1:].rstrip(b"\n") + b"\n\n"
print("amendment body:", len(body), "bytes, first line:", body.split(b"\n", 1)[0])
for p in TREES:
    b = open(p, "rb").read()
    assert b"\r" not in b
    if b.count(b"\n## 2a") > 0:
        print(p, "already amended"); continue
    anchor = b"\n## 3. "
    assert b.count(anchor) == 1, (p, b.count(anchor))
    i = b.find(anchor)
    new = b[:i].rstrip(b"\n") + b"\n\n" + body + b[i + 1:]
    assert new.count(b"\r") == 0
    open(p, "wb").write(new)
    print(p, "amended, bytes +", len(new) - len(b))
a, c = [open(p, "rb").read() for p in TREES]
print("trees identical:", a == c)
