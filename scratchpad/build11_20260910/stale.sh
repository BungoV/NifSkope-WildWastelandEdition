#!/bin/bash
# Lane BUILD11: the two staleness sweeps nifskope-ww-build-verify demands, as one script.
#   (1) THE FLAG SWEEP -- a changed DEFINES line makes an object stale and no mtime says so
#       (MISTAKES.md 2026-09-10, lane BUILD9). Every TU that reads one of the three WW
#       macros, plus every TU that includes a header which reads one.
#   (2) THE HEADER SWEEP -- an object older than a header it includes is a stale build,
#       whatever make says.
# Prints; deletes only with --delete.
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
OBJ=GeneratedFiles/.obj
DEL=0; [ "$1" = "--delete" ] && DEL=1

echo "=== (1) FLAG SWEEP: WW_HKXANIM_UI / WW_HKXCLIP_CANON / WW_ANIMWS_HKXMODEL ==="
READERS=$(grep -rl "WW_HKXANIM_UI\|WW_HKXCLIP_CANON\|WW_ANIMWS_HKXMODEL" src/ 2>/dev/null)
echo "sources that read a macro:"; echo "$READERS" | sed 's/^/    /'
TUS=""
for f in $READERS; do
  case "$f" in
    *.cpp) TUS="$TUS $f" ;;
    *.h)   TUS="$TUS $(grep -rl "#include \"$(basename $f)\"" src/ | grep '\.cpp$')" ;;
  esac
done
for f in $(echo $TUS | tr ' ' '\n' | sort -u); do
  o="$OBJ/$(basename $f .cpp).o"
  if [ -f "$o" ]; then
    echo "    FLAG-STALE $o  ($f)"
    [ $DEL -eq 1 ] && rm -f "$o" && echo "        deleted"
  else
    echo "    (no object) $o"
  fi
done

echo "=== (2) HEADER SWEEP over every changed/new header ==="
HDRS=$(git status --porcelain -- src | awk '{print $NF}' | grep '\.h$')
for h in $HDRS; do
  base=$(basename "$h")
  n=0
  for f in $(grep -rl "#include \"$base\"" src/ 2>/dev/null | grep '\.cpp$'); do
    o="$OBJ/$(basename $f .cpp).o"
    [ -f "$o" ] || continue
    if [ "$o" -nt "$h" ]; then :; else
      echo "    STALE $o  (includes $h)"
      n=$((n+1))
      [ $DEL -eq 1 ] && rm -f "$o" && echo "        deleted"
    fi
  done
  [ $n -eq 0 ] && echo "    ok    $h  (no stale object)"
done
echo "SWEEP-DONE delete=$DEL"
