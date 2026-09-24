#!/bin/bash
# AUDIT1 step 2, the two rows the first run could not produce:
#
#  (1) THE NULL INCREMENTAL, spelled correctly. bake_all.sh ran
#      `... --native <dir> --incremental` with the flag LAST and no directory,
#      which parses to an empty string and full-bakes (the product half of that
#      is a confirmed bug in section 4 and section 6). Here it is spelled
#      `--incremental <the finished bake>` over a COPY of this lane's own
#      sanctuary_fo4cs tree, and what is measured is byte identity, the census
#      words, and the two wall clocks.
#
#  (2) THE STOCK TARGET AGAINST A RUNG. The stock bake is a hard byte gate, so
#      the three stock trees of section 2 are re-baked with the rung exe
#      (release/NifSkope.before_perf1.exe, the build before the audited one)
#      and compared file by file. The `.lodb` bake record is a record of the
#      RUN -- its own time, its own switch paths, its own census -- so it is
#      compared by its content lines instead, exactly as lodgen_roads.sh does.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
NS="$ROOT/release/NifSkope.exe"
RUNG="$ROOT/release/NifSkope.before_perf1.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
B="$ROOT/scratchpad/audit1_20260916/bake"
W() { cygpath -m "$1" 2>/dev/null || echo "$1"; }

echo "=== (1) the null incremental, spelled with its directory  $(date '+%F %T')"
rm -rf "$B/sanctuary_incr2" "$B/sanctuary_prev"
cp -r "$B/sanctuary_fo4cs" "$B/sanctuary_prev"
mkdir -p "$B/sanctuary_incr2/tex"
cp -r "$B/sanctuary_prev/." "$B/sanctuary_incr2/"
WI="$(W "$B/sanctuary_incr2")"
WP="$(W "$B/sanctuary_prev")"
t0=$(date +%s)
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -9 35 --dim 4 \
	--data-root "$DATA" --out-dir "$WI" --tex-dir "$WI/tex" --native "$WI" \
	--incremental "$WI" > "$B/sanctuary_incr2.log" 2>&1
rc=$?
t1=$(date +%s)
echo "(1) rc=$rc  $(( t1 - t0 )) s"
echo "--- the words the run used about the library and the cache:"
grep -aE "^(native-library-build|native cache|bake census|stage times|incremental)" "$B/sanctuary_incr2.log" | sed 's/^/    /'
echo "--- byte identity against the bake it replayed (the .lodb record excluded, compared below):"
( cd "$B/sanctuary_prev" && find . -type f | sort ) > "$B/prev_files.txt"
( cd "$B/sanctuary_incr2" && find . -type f | sort ) > "$B/incr2_files.txt"
diff "$B/prev_files.txt" "$B/incr2_files.txt" > /dev/null && echo "    the same file LIST on both sides" \
	|| { echo "    the file lists DIFFER:"; diff "$B/prev_files.txt" "$B/incr2_files.txt" | head -10 | sed 's/^/      /'; }
same=0; diff=0
while read -r f; do
	case "$f" in *.lodb) continue ;; esac
	if cmp -s "$B/sanctuary_prev/$f" "$B/sanctuary_incr2/$f"; then same=$(( same + 1 ));
	else diff=$(( diff + 1 )); echo "    DIFFERS: $f"; fi
done < "$B/prev_files.txt"
echo "    $same identical, $diff differ (the .lodb record aside)"
echo "--- and the record itself, by its content lines:"
for r in "$B/sanctuary_prev" "$B/sanctuary_incr2"; do
	find "$r" -name '*.lodb' | while read -r p; do
		grep -aE "^(chunk|out)$(printf '\t')" "$p" | sort | md5sum | sed "s|^|    $(basename "$(dirname "$p")")/$(basename "$p") content-lines md5 |"
	done
done

echo
echo "=== (2) the stock target against the rung exe  $(date '+%F %T')"
ls -l --time-style=+%Y-%m-%d_%H:%M:%S "$RUNG" | sed 's/^/    /'
for r in "sanctuary:-20 24 -9 35" "coast:4 -28 15 -17" "urban:0 -12 11 -1"; do
	name="${r%%:*}"; reg="${r#*:}"
	out="$B/${name}_stock_rung"
	rm -rf "$out"; mkdir -p "$out/tex"
	WO="$(W "$out")"
	t0=$(date +%s)
	# shellcheck disable=SC2086
	"$RUNG" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $reg --dim 4 \
		--data-root "$DATA" --out-dir "$WO" --tex-dir "$WO/tex" \
		> "$B/${name}_stock_rung.log" 2>&1
	rc=$?
	t1=$(date +%s)
	echo "  ${name}_stock_rung rc=$rc  $(( t1 - t0 )) s  $(find "$out" -type f | wc -l) files"
	s=0; d=0; o=0
	( cd "$out" && find . -type f | sort ) > "$B/${name}_rung_files.txt"
	( cd "$B/${name}_stock" && find . -type f | sort ) > "$B/${name}_new_files.txt"
	diff "$B/${name}_rung_files.txt" "$B/${name}_new_files.txt" > /dev/null \
		&& echo "    the same file LIST on both sides" \
		|| { echo "    the file lists DIFFER:"; diff "$B/${name}_rung_files.txt" "$B/${name}_new_files.txt" | sed 's/^/      /'; o=1; }
	while read -r f; do
		case "$f" in *.lodb) continue ;; esac
		[ -f "$B/${name}_stock/$f" ] || continue
		if cmp -s "$out/$f" "$B/${name}_stock/$f"; then s=$(( s + 1 ));
		else d=$(( d + 1 )); echo "    DIFFERS: $f"; fi
	done < "$B/${name}_rung_files.txt"
	echo "    $name: $s identical, $d differ, list-mismatch=$o (the .lodb record aside)"
done
echo "REST-COMPLETE $(date '+%F %T')"
