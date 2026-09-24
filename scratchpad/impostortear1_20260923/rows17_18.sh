# ---------------------------------------------------------------------------
# 17. THE TEAR ROW (lane IMPOSTORTEAR1, 2026-09-23). bungo, on the cards: "like
#     somebody ripped out a piece of paper".
#
#     The three blended frames do not register, so where ONE frame is solid the
#     3-frame MEAN coverage falls under the cut and a hole opens in the middle
#     of the tree. The number is the TORN SHARE: of the pixels where the mesh is
#     covered AND the nearest frame alone (WW_IMPOSTOR_BLEND=0) is covered, the
#     share the blended card leaves open -- pixels the card itself says are
#     solid, that the object really fills, and that the drawer tore out
#     (impostor_tear_pop.py). 36 azimuths, 10 degrees apart, elevation 20.
#
#     Measured on the lane's own 1-degree sweeps at each subject's torn view
#     (cardRes 512, N=4), the shipped mean cut -> the stippled cut:
#       blast 44.9 % -> 5.9 %   maple 68.3 % -> 19.0 %   rock 27.3 % -> 5.4 %
#     THE BAR is the lane's pre-registered clause, 0.60 x the rung's share on
#     this row's own fixture and views, written down as a number once the rung
#     had been run (the rung and the new drawer's numbers are in the comment
#     beside TEAR_BAR). It fails on the rung by construction of that clause
#     and on nothing else the lane measured.
# ---------------------------------------------------------------------------
tear_views=$( "$PY" -c "print(','.join('%d:20' % a for a in range(0, 360, 10)))" )
pop_az_views=$( "$PY" -c "print(','.join('%d:20' % a for a in range(360)))" )
pop_el_views=$( "$PY" -c "print(','.join('30:%d' % e for e in range(90)))" )
orbit_grab() {   # $1 = out dir, $2 = views, rest = extra env; writes $1/v_*.png and $1.log
	rm -rf "$1"; mkdir -p "$1"; : > "$1.log"
	og_dir="$1"; og_views="$2"; shift 2
	env "$@" \
	WW_IMPOSTOR_PREVIEW=orbit \
	WW_IMPOSTOR_LODM="$IMPOSTOR_LODM" \
	WW_IMPOSTOR_LOG="$( cygpath -m "$og_dir.log" )" \
	WW_IMPOSTOR_SHOT="$( cygpath -m "$og_dir" )/v" \
	WW_IMPOSTOR_ORBIT_VIEWS="$og_views" \
	WW_IMPOSTOR_CHANNEL=2 WW_IMPOSTOR_MESH_CHANNEL=8 WW_RENDER_CLEAN=1 \
	WW_WINDOW_AT=1960,40 \
	WW_RENDER_SIZE=1024x1024 \
	"$exe" --port "$port" ${IMPOSTOR_NIF:+"$IMPOSTOR_NIF"} >> "$log" 2>&1
}
TEAR_BAR="${IMPOSTOR_TEAR_BAR:-0.0000}"
if [ -z "$IMPOSTOR_NIF" ]; then
	bad "17 no mesh: the tear row needs the mesh the card was baked from -- REFUSED without it"
else
	orbit_grab "$tmp/tear/blend" "$tear_views" WW_IMPOSTOR_BLEND=1
	orbit_grab "$tmp/tear/near"  "$tear_views" WW_IMPOSTOR_BLEND=0
	"$PY" "$here/impostor_tear_pop.py" tear "$( cygpath -m "$tmp/tear/blend" )" \
		"$( cygpath -m "$tmp/tear/near" )" "$tear_views" > "$work/ww_impostor_tear.txt" 2>&1
	tee -a "$log" < "$work/ww_impostor_tear.txt"
	tshare=$( sed -n 's/^torn share //p' "$work/ww_impostor_tear.txt" )
	if [ -z "$tshare" ]; then
		bad "17 tear: no torn share measured -- $( tail -1 "$work/ww_impostor_tear.txt" )"
	elif "$PY" -c "import sys; sys.exit(0 if float('$tshare') <= float('$TEAR_BAR') else 1)"; then
		ok "17 tear: torn share $tshare <= bar $TEAR_BAR over 36 azimuths at el 20 ($( sed -n 's/^worst view //p' "$work/ww_impostor_tear.txt" ))"
	else
		bad "17 tear: torn share $tshare > bar $TEAR_BAR over 36 azimuths at el 20 -- the card tears where the nearest frame is solid ($( sed -n 's/^worst view //p' "$work/ww_impostor_tear.txt" ))"
	fi
fi

# ---------------------------------------------------------------------------
# 18. THE POPPING ROW (lane IMPOSTORTEAR1). A tear repair that swaps holes for
#     a silhouette that jumps as the camera turns is not a repair. Per-step
#     change in covered card pixels (XOR) over a 1-degree azimuth ring at el 20
#     (360 views) and a 1-degree elevation sweep at az 30 (0..89), the mesh's
#     own worst step printed beside it.
#
#     THE BAR is the brief's: the default drawer's worst step <= 1.5 x the
#     worst step of the PREVIOUS cut (the 3-frame mean, WW_IMPOSTOR_CUT=mean),
#     both run by THIS exe on THIS bake, so no number is carried between bakes.
#     ITS RED CONTROL is run, not asserted: WW_IMPOSTOR_CUT=strong (the cut on
#     the strongest frame alone, which repairs the tear and pops 1.8 .. 2.6 x on
#     the lane's sweeps) must break the same bar on at least one sweep, or the
#     bar cannot fire and the row fails.
#
#     WHAT FAILS ON THE RUNG, said plainly: the rung's drawer IS the reference
#     (its cut is the mean), so the popping clause itself cannot be red there.
#     The row is red on the rung because the rung does not name a cut rule and
#     cannot run the red control -- an exe that cannot demonstrate its bar can
#     fire does not pass it.
# ---------------------------------------------------------------------------
if [ -z "$IMPOSTOR_NIF" ]; then
	bad "18 no mesh: the popping row -- REFUSED without it"
else
	for sw in az el; do
		eval "vv=\$pop_${sw}_views"
		orbit_grab "$tmp/pop/$sw/default" "$vv"
		orbit_grab "$tmp/pop/$sw/mean"    "$vv" WW_IMPOSTOR_CUT=mean
		orbit_grab "$tmp/pop/$sw/strong"  "$vv" WW_IMPOSTOR_CUT=strong
		"$PY" "$here/impostor_tear_pop.py" pop "$vv" "$( cygpath -m "$tmp/pop/$sw/mean" )" \
			"$( cygpath -m "$tmp/pop/$sw/default" )" "$( cygpath -m "$tmp/pop/$sw/strong" )" \
			> "$work/ww_impostor_pop_$sw.txt" 2>&1
		tee -a "$log" < "$work/ww_impostor_pop_$sw.txt"
	done
	rule=$( sed -n 's/^cut rule: \([a-z]*\).*/\1/p' "$tmp/pop/az/default.log" | tail -1 )
	verdict=$( "$PY" - "$work/ww_impostor_pop_az.txt" "$work/ww_impostor_pop_el.txt" <<'PYEOF'
import re, sys
out, ok, bit = [], True, False
for f in sys.argv[1:]:
    w = dict(re.findall(r'^pop (\w+)\s+card worst (\d+)', open(f).read(), re.M))
    if not all(k in w for k in ('mean', 'default', 'strong')):
        print('MISSING %s' % f); sys.exit(0)
    m, d, s = (int(w[k]) for k in ('mean', 'default', 'strong'))
    ok &= d <= 1.5 * m
    bit |= s > 1.5 * m
    out.append('%s default %d / mean %d = %.2f, strong %.2f' % (f[-6:-4], d, m, d / max(1, m), s / max(1, m)))
print(('POP_OK ' if ok else 'POP_BAD ') + ('RED_BITES ' if bit else 'RED_DEAD ') + '; '.join(out))
PYEOF
)
	case "$verdict" in
	MISSING*)             bad "18 popping: a sweep produced no grabs -- $verdict" ;;
	*) if [ "$rule" != "stipple" ]; then
		bad "18 popping: the exe names no stippled cut rule (read '${rule:-nothing}') -- $verdict"
	   elif [ "${verdict#POP_OK RED_BITES}" != "$verdict" ]; then
		ok "18 popping: worst step <= 1.5 x the mean cut's, and the strongest-frame red control breaks it -- ${verdict#POP_OK RED_BITES }"
	   elif [ "${verdict#POP_OK}" != "$verdict" ]; then
		bad "18 popping: the red control did not bite, the bar cannot fire -- ${verdict#POP_OK RED_DEAD }"
	   else
		bad "18 popping: the default cut pops more than 1.5 x the mean cut -- ${verdict#POP_BAD * }"
	   fi ;;
	esac
fi

