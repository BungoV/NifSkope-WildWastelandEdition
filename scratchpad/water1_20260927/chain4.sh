#!/bin/bash
# WATER1 continuation 4: river pass, pin pass, then resume12 all. A pass that stops with an empty log
# (Avast sandbox) is retried after a 10 min wait (night rule), at most 5 tries; skips pictures on disk.
ME=/e/Projects/NifskopeWWE-water1/scratchpad/water1_20260927; cd $ME || exit 1
pass() { # <group> <portbase>
  for t in 1 2 3 4 5; do
    PORTBASE=$(( $2 + t*20 )) bash $ME/render_all.sh $1 > $ME/${1}_pass4_$t.out 2>&1
    cat $ME/${1}_pass4_$t.out
    grep -q "RENDER PASS DONE" $ME/${1}_pass4_$t.out && return 0
    echo "$(date +%H:%M) $1 try $t stopped; waiting 600 s"; sleep 600
  done; return 1
}
pass river 46200 || { echo "CHAIN STOPPED river"; exit 2; }
pass pin 46400 || { echo "CHAIN STOPPED pin"; exit 2; }
PORTBASE=46600 bash $ME/resume12.sh all
echo "CHAIN4 END"
