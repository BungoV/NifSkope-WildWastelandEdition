#!/bin/bash
# G5: no write outside mods\FO4CSLOD. The same three listings as the 04:36 before-listings (mods top level,
# the MO2 root top level, the Default profile's FILES with sizes), then diff. Allowed differences: the ./FO4CSLOD
# line in the mods listing (absent before, present after) and the ./mods folder's own mtime in the root listing
# (a new child moves its parent's mtime).
# RED control: the diff must name a line that DID change (FO4CSLOD) -- a diff that is empty everywhere would
# mean the instrument cannot see a new folder.
L=/e/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925
M="/e/Projects/Fallout 4 Mods"
cd $L
(cd "$M/mods" && find . -maxdepth 1 -mindepth 1 -printf "%TY-%Tm-%Td %TH:%TM:%TS %p\n" | sort -k3) > g5_after.txt
(cd "$M" && find . -maxdepth 1 -mindepth 1 -printf "%TY-%Tm-%Td %TH:%TM:%TS %p\n" | sort -k3) > g5_root_after.txt
(cd "$M/profiles/Default" && find . -maxdepth 1 -mindepth 1 -type f -printf "%TY-%Tm-%Td %TH:%TM:%TS %s %p\n" | sort -k4) > g5_profile_after.txt
for p in "" root_ profile_; do
	echo "== g5_${p}before2 vs g5_${p}after ($(wc -l < g5_${p}before2.txt) / $(wc -l < g5_${p}after.txt) entries)"
	diff g5_${p}before2.txt g5_${p}after.txt
done
