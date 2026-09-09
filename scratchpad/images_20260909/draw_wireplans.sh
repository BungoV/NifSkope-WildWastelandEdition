#!/bin/bash
# The triangulation halves, drawn offline from each chunk's own bytes.
# (IMAGES3 ran wireplan.py by hand; IMAGES4 keeps it as a script so every
# picture regenerates from a command, per the brief's gate.)
set -u
B=E:/Projects/NifskopeWildWastelandEdition/scratchpad/images_20260909
for spec in "4 Commonwealth.4.28.24" "8 Commonwealth.8.24.24" \
            "16 Commonwealth.16.16.16" "32 Commonwealth.32.0.0"; do
  read -r d t <<< "$spec"
  python "$B/wireplan.py" "$B/gen/van$d/$t.BTR"  "$B/img/wire_van$d.png"  "$d" "vanilla  $t"
  python "$B/wireplan.py" "$B/gen/ours$d/$t.BTR" "$B/img/wire_ours$d.png" "$d" "ours  $t"
done
echo WIREPLAN-DONE
