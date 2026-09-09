# Patch 8 -- tools/bake_impostor_cards.sh : the CANDIDATES=trees comment said
# what the flag was MEANT to do; the flag did `tree || missing`, a superset of
# the default. nifcli.cpp is fixed; the comment now says what it does.
import os
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")
P = "tools/bake_impostor_cards.sh"
b = open(P, "rb").read()
assert b.count(b"\r") == 0
s = b.decode("utf-8")
old = ("#   CANDIDATES=trees bash tools/bake_impostor_cards.sh ...   "
       "# every tree, for cards from ring 0 (default: missing far slots)")
assert s.count(old) == 1
new = ("#   CANDIDATES=trees bash tools/bake_impostor_cards.sh ...   "
       "# TREES AND ONLY TREES, for cards from ring 0\n"
       "#   (default: `missing` -- bases whose far MNAM slots are empty. Until 2026-09-09\n"
       "#   `trees` meant tree OR missing, so it was a SUPERSET of the default and 14 of a\n"
       "#   33-candidate Sanctuary run were shacks and rock cliffs. It is tree-only now.)")
s = s.replace(old, new)
out = s.encode("utf-8")
assert out.count(b"\r") == 0
open(P, "wb").write(out)
print("OK", P, len(out), "bytes, CR", out.count(b"\r"), "LF", out.count(b"\n"))
