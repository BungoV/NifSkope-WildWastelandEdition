# Patch 9 -- tests/spells : the three land harnesses and the independent
# authority follow the file to .lodl, and the texture harnesses follow it to
# .lodt. Renames are done with `git mv` so the history shows a rename.
import os
import subprocess
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")

RENAMES = [
    ("tests/spells/lodt_write.sh", "tests/spells/lodl_write.sh"),
    ("tests/spells/lodt_open.sh", "tests/spells/lodl_open.sh"),
    ("tests/spells/lodt_btd.sh", "tests/spells/lodl_btd.sh"),
    ("tests/spells/lodt_open_authority.py", "tests/spells/lodl_open_authority.py"),
]
for a, b in RENAMES:
    if os.path.exists(a):
        r = subprocess.run(["git", "mv", a, b], capture_output=True, text=True)
        if r.returncode != 0:      # untracked file: a plain rename is right
            os.rename(a, b)
        print("renamed", a, "->", b)
    else:
        assert os.path.exists(b), "neither %s nor %s exists" % (a, b)
        print("already renamed:", b)
