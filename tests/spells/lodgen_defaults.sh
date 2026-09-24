#!/bin/bash
#
# THE DEFAULTS GATE (lane DEFAULTS1, 2026-09-12).
#
# bungo ruled four defaults on 2026-09-12 and this spell is the only thing that
# can say they are actually IN the code rather than in a report:
#
#   (a) object identity OFF      -- `--identity` is the opt-in
#   (b) terrain identity OFF     -- `--terrain-identity` is the opt-in
#   (c) the land look of panel (c) of LAND1's `a_land_guide_*.png`:
#       hex 256, warp amplitude 341, mip bias -0.22, guide flatwarp strength 1
#   (d) `--road-ground-paint` 0  -- the verge planes beside the kerb are not road
#
# and one behaviour change that rides with them (brief item 2): the
# `.bto.manifest.txt` SIDECAR, the texture arrays and the impostor cards are no
# longer gated on the identity flag, so the far rings keep working with identity
# off, and so do the native `.lodo` / `.lodi` / `.lodt` / `.lodl` files.
#
# RUNG-FREE SINCE 2026-09-16 (lane GENSMALL1, director ROW B).
#
# Every phase used to compare this exe with `release/NifSkope.before_defaults1.exe`.
# That binary is no longer on disk and DEFAULTS1's change was never committed, so
# it cannot be rebuilt from git either: the spell exited 2 before running a
# single check, which is a gate that cannot go green OR red. It now proves the
# same sentence -- "the defaults ARE the switches" -- on the exe under test
# alone, by spelling the switches instead of keeping an old binary around.
#
# THE THREE SHAPES EVERY PHASE IS BUILT FROM, so a reader can check the logic
# rather than the wording:
#
#   SAME      the DEFAULT bake and a bake with the ruled values SPELLED OUT are
#             byte-identical. Only `.lodb` is excused, and only because the
#             ledger hashes the SWITCH LIST by design, so spelled and defaulted
#             can never agree there. (The `.manifest.txt` sidecar used to be
#             excused as well, because the rung did not write one; both sides
#             are the same exe now, so it is held to the byte like everything
#             else. The gate got TIGHTER losing the rung, not looser.)
#   DIFFERENT a bake with the OLD values spelled out is a DIFFERENT bake, and
#             the differing files are named -- a `.BTR` for the terrain switch
#             and a `.BTO` for the object one, or the phase has not shown that
#             the switches reach anything.
#   REFUTER   one deliberately WRONG explicit switch must break the SAME
#             comparison that the right ones pass. A green that cannot go red
#             is not a measurement.
#
#   (a) DEFAULTS ARE THE SWITCHES. The DEFAULT bake equals a bake with
#       `--no-identity --no-terrain-identity --road-ground-paint 0` and the four
#       land switches of panel (c), file for file, `.lodb` aside. REFUTER: the
#       same bake with ONE switch deliberately wrong (`--land-hex 0` in place of
#       256) must NOT match.
#   (b) THE OLD VALUES ARE A DIFFERENT BAKE. The old switches spelled out
#       (`--identity --terrain-identity --road-ground-paint 1 --land-hex 0
#       --land-warp 0 --land-mip-bias 0 --land-guide off`) differ from the
#       default, and both a `.BTR` and a `.BTO` are among the files that moved.
#       That is the way back, and it is named file by file rather than claimed.
#   (c) THE LEGACY FILES ARE VANILLA'S LAYOUT. Every `.BTR` of the default bake
#       carries vanilla's land descriptor 52776558133763 and every `.BTO`
#       carries the plain object descriptor 474989027590661 (no colour, no
#       UV 2), at dim 4, 8, 16 and 32 over Sanctuary. REFUTER: the same read on
#       THIS exe asked for `--identity --terrain-identity` shows the wide
#       descriptors, so the check is capable of failing.
#   (d) THE FAR RINGS ARE NOT EMPTY. The same chunk baked with `--arrays` and
#       `--impostors`: identity OFF (the default) places the same number of
#       cards, array layers and manifest rows as identity ON spelled out.
#       FLOOR: the counts must be non-zero, or an empty bake would pass.
#   (e) THE NATIVE DATA DOES NOT THIN OUT. `.lodo` / `.lodi` / `.lodt` /
#       `.lodl` are byte-identical between the default (identity OFF) and
#       `--identity --terrain-identity` spelled out, with every OTHER switch
#       equal, and `--native-verify` is clean on the pair. REFUTER:
#       `--native-no-ladder` writes DIFFERENT native files, so the file-for-file
#       comparator above is capable of reporting a difference.
#
# TWO CHECKS WERE DROPPED WITH THE RUNG, BY NAME, because nothing on this exe
# can stand in for them -- both were statements about the OLD BINARY'S
# behaviour, not about this one's:
#
#   * "(a) refuter: the rung with --no-identity wrote NO manifest" -- the
#     sidecar used to be gated on the identity flag and is not any more. Only
#     the old binary can demonstrate the gating it no longer has.
#   * "(e) refuter: on the OLD code the flag DID thin the native data" -- same
#     shape. The thinning is gone; only the code that had it can show it.
#
# Each is replaced one-for-one by a refuter that measures THIS exe (the wrong
# land switch in (a), `--native-no-ladder` in (e)), so the count stays at 28
# and no check is a silent casualty.
#
# USAGE
#   bash tests/spells/lodgen_defaults.sh
#   PHASES=ab bash tests/spells/lodgen_defaults.sh     # only the byte phases
#   KEEP=1 bash tests/spells/lodgen_defaults.sh        # reuse existing bakes
#
# It is slow: (c) at dim 32 and (d) at dim 16 are large chunks. Nothing here
# touches an installed game folder; every output lands under
# `scratchpad/defaults1_20260912/gate/`.

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
# no RUNG: see the header. Every comparison is this exe against itself with
# the switches spelled out.
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
CARDS="${CARDS:-$ROOT/scratchpad/showcase1_20260912/cards}"
PHASES="${PHASES:-abcdef}"
DIG="$ROOT/tests/spells/lodgen_tree_digest.py"
W="${WORKDIR:-$ROOT/scratchpad/defaults1_20260912/gate}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"

# the four land switches of panel (c), and the four that undo them
LAND_NEW="--land-hex 256 --land-warp 341 --land-mip-bias -0.22 --land-guide flatwarp:1.0"
LAND_OLD="--land-hex 0 --land-warp 0 --land-mip-bias 0 --land-guide off"

checks=0
fails=0
ok()  { checks=$(( checks + 1 )); echo "  ok   $1"; }
bad() { checks=$(( checks + 1 )); fails=$(( fails + 1 )); echo "  FAIL $1"; }
say() { echo "       $1"; }

[ -x "$NS" ]   || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ]  || { echo "no ESM at $ESM"; exit 2; }
[ -d "$DATA" ] || { echo "no unpacked Data at $DATA"; exit 2; }

# The game, and the one-instance rule. -no-gui lodgen opens no window, but a
# bake while Fallout 4 is up is a lane's mistake either way (CONSTITUTION 6).
if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
	echo "FAIL: Fallout 4 is running"
	exit 2
fi

echo "== lodgen_defaults: the exe under test =="
ls -l --time-style=+%Y-%m-%d_%H:%M:%S "$NS" | sed 's/^/       /'

[ "${KEEP:-0}" = "1" ] || rm -rf "$W"
mkdir -p "$W"
WA="$(cygpath -m "$W" 2>/dev/null || echo "$W")"

# bake <exe> <name> <dim> <x0 y0 x1 y1> -- <extra switches...>
bake() {
	local exe="$1" name="$2" dim="$3" x0="$4" y0="$5" x1="$6" y1="$7"; shift 7
	[ "$1" = "--" ] && shift
	mkdir -p "$W/$name" "$W/$name/tex"
	local t0 t1
	t0="$(date +%s)"
	"$exe" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region "$x0" "$y0" "$x1" "$y1" --dim "$dim" \
		--data-root "$DATA" --out-dir "$WA/$name" --tex-dir "$WA/$name/tex" \
		"$@" > "$W/$name.log" 2>&1
	local rc=$?
	t1="$(date +%s)"
	say "$name: rc=$rc, $(( t1 - t0 )) s, $(find "$W/$name" -type f | wc -l) files"
	return $rc
}

# descOf <exe> <file> -- the Vertex Desc of the first block that has one
descOf() {
	local b v
	for b in 0 1 2 3 4 5 6 7 8 9 10; do
		v="$("$1" -no-gui get "$2" -b "$b" -f "Vertex Desc" 2>/dev/null | tr -dc 0-9)"
		[ -n "$v" ] && { echo "$v"; return 0; }
	done
	echo ""
}

# The .lodb ledger (lane INCR1) records WHICH SWITCHES a bake ran with and the
# sha1 of every file it wrote, so it can never be byte-equal between a default
# run and a run with the same thing spelled out -- the switch hash is the point
# of it. What CAN be asked of it is the only thing that matters here: that the
# two bakes wrote the same files with the same hashes, manifest aside.
lodbcmp() {
	"$PY" - "$1" "$2" "$ROOT/tests/spells" <<'PYEOF'
import sys
# THE ONE READER (lane BAKEREC1, 2026-09-17). This used to carry its own copy
# of the v1 binary container's parser; the record is version 2 plain text now
# and tests/spells/lodb_read.py is the only thing that parses it.
sys.path.insert(0, sys.argv[3])
import lodb_read
def out(path):
    j = lodb_read.read(path)
    return j, lodb_read.outputs(j)
ja, fa = out(sys.argv[1])
jb, fb = out(sys.argv[2])
ka = set(n for n in fa if 'manifest' not in n)
kb = set(n for n in fb if 'manifest' not in n)
moved = sorted(n for n in ka & kb if fa[n] != fb[n])
print('       ledger: %d files each side, %d listed only on one side, %d with a different hash'
      % (len(ka), len(ka ^ kb), len(moved)))
for n in sorted(ka ^ kb):
    print('         only on one side: %s' % n)
for n in moved:
    print('         hash moved: %s' % n)
inputs_a = [c.get('inputs') for c in ja.get('chunks', [])]
inputs_b = [c.get('inputs') for c in jb.get('chunks', [])]
print('       ledger: input hashes equal: %s; switch hashes equal: %s (they must NOT be)'
      % (inputs_a == inputs_b, ja.get('switches') == jb.get('switches')))
manifest_only_new = sorted(n for n in set(fb) - set(fa) if 'manifest' in n)
if manifest_only_new:
    print('       ledger: the sidecar is listed as an output of the new bake: %s'
          % ', '.join(manifest_only_new))
bad = len(ka ^ kb) + len(moved) + (0 if inputs_a == inputs_b else 1)
sys.exit(1 if bad else 0)
PYEOF
}

# compare two trees; $3 = a grep pattern of paths that are ALLOWED to differ
treecmp() {
	"$PY" "$DIG" "$1" "$2" > "$W/cmp.txt" 2>&1
	local rc=$?
	sed 's/^/       /' "$W/cmp.txt"
	if [ "$rc" -eq 0 ]; then
		return 0
	fi
	if [ -n "${3:-}" ]; then
		local bad_lines
		bad_lines="$(grep -E "^  (differs|only in)" "$W/cmp.txt" | grep -vE "$3" | wc -l)"
		if [ "$bad_lines" -eq 0 ]; then
			say "the only differences are the excused ones ($3)"
			return 0
		fi
		say "$bad_lines differing files are NOT excused"
	fi
	return 1
}

# ---- (a) the defaults ARE the switches --------------------------------------
if [ "${PHASES#*a}" != "$PHASES" ]; then
	echo "== (a) the DEFAULT bake == the same bake with the switches SPELLED OUT =="
	bake "$NS" a_new 4 -20 24 -20 24 -- --road-detail 1
	bake "$NS" a_spelled 4 -20 24 -20 24 -- --road-detail 1 \
		--no-identity --no-terrain-identity --road-ground-paint 0 $LAND_NEW
	# only the ledger is excused, and only because it hashes the switch LIST
	if treecmp "$W/a_spelled" "$W/a_new" "[.]lodb"; then
		ok "(a) default bake == the switches spelled out, only the switch-list ledger excused"
	else
		bad "(a) default bake differs from the same switches spelled out"
	fi
	if lodbcmp "$W/a_spelled/Commonwealth.lodb" "$W/a_new/Commonwealth.lodb"; then
		ok "(a) the ledger agrees: same files, same hashes, only the switch hash and the sidecar move"
	else
		bad "(a) the ledger says a file other than the sidecar moved"
	fi
	# the excused file must EXIST on the new side -- item 2's whole point
	if ls "$W"/a_new/*.manifest.txt >/dev/null 2>&1; then
		m="$(ls "$W"/a_new/*.manifest.txt | head -1)"
		rows="$(grep -c "^[0-9]" "$m")"
		say "the new default bake's sidecar: $(basename "$m"), $rows object rows"
		[ "$rows" -ge 50 ] && ok "(a) the manifest sidecar is written with identity OFF (floor 50 rows)" \
			|| bad "(a) the sidecar has only $rows rows, floor is 50"
	else
		bad "(a) no .manifest.txt beside the default bake -- item 2 did not land"
	fi
	# THE REFUTER, replacing the one the rung used to carry: the SAME
	# comparison with ONE switch deliberately wrong. bungo ruled hex 256; this
	# bake spells 0 and everything else right, and (a) must go red on it. If it
	# does not, (a) is not reading the land switches at all and its green above
	# means nothing.
	bake "$NS" a_wrong 4 -20 24 -20 24 -- --road-detail 1 \
		--no-identity --no-terrain-identity --road-ground-paint 0 \
		--land-hex 0 --land-warp 341 --land-mip-bias -0.22 --land-guide flatwarp:1.0
	if treecmp "$W/a_wrong" "$W/a_new" "[.]lodb" >/dev/null 2>&1; then
		bad "(a) refuter broken: --land-hex 0 bakes the same bytes as the ruled 256"
	else
		ok "(a) refuter: ONE wrong explicit switch (--land-hex 0) breaks the same comparison"
	fi

	# the road-bearing chunk, where --road-ground-paint actually paints
	echo "== (a2) a road-bearing region, where the verge default is visible =="
	bake "$NS" a2_new 4 -20 20 -17 23 -- --road-detail 1 --cover --roads
	bake "$NS" a2_spelled 4 -20 20 -17 23 -- --road-detail 1 --cover --roads \
		--no-identity --no-terrain-identity --road-ground-paint 0 $LAND_NEW
	if treecmp "$W/a2_spelled" "$W/a2_new" "[.]lodb"; then
		ok "(a2) roads: default bake == the switches spelled out"
	else
		bad "(a2) roads: default bake differs from the switches spelled out"
	fi
	if lodbcmp "$W/a2_spelled/Commonwealth.lodb" "$W/a2_new/Commonwealth.lodb"; then
		ok "(a2) roads: the ledger agrees, file for file and hash for hash"
	else
		bad "(a2) roads: the ledger says a file moved"
	fi
	# and the refuter for the verge default (d): road-ground-paint 1 is a
	# DIFFERENT bake on THIS exe, which is what makes the 0 above a choice
	bake "$NS" a2_old 4 -20 20 -17 23 -- --road-detail 1 --cover --roads \
		--no-identity --no-terrain-identity --road-ground-paint 1 $LAND_NEW
	if treecmp "$W/a2_old" "$W/a2_new" "[.]lodb" >/dev/null 2>&1; then
		bad "(a2) refuter broken: road-ground-paint 1 and 0 give the same bytes"
	else
		ok "(a2) refuter: --road-ground-paint 1 bakes DIFFERENT bytes from the ruled 0"
	fi
fi

# ---- (b) the way back exists -------------------------------------------------
if [ "${PHASES#*b}" != "$PHASES" ]; then
	echo "== (b) the OLD values spelled out are a DIFFERENT bake, file by file =="
	# The rung-free half of the old (b). It used to say "the way back equals the
	# rung's default bake", which needed the old binary. What it can say on this
	# exe alone is the half that matters to a user: asking for the old values
	# CHANGES the output, and it changes the files the switches are supposed to
	# reach. A switch that is accepted and changes nothing is the failure this
	# phase exists to catch.
	bake "$NS" b_old 4 -20 24 -20 24 -- --road-detail 1 \
		--identity --terrain-identity --road-ground-paint 1 $LAND_OLD
	# phase (b) bakes its own default side, so PHASES=b alone still works
	[ -d "$W/b_def" ] || bake "$NS" b_def 4 -20 24 -20 24 -- --road-detail 1
	if treecmp "$W/b_old" "$W/b_def" "[.]lodb" >/dev/null 2>&1; then
		bad "(b) the OLD switches bake the SAME bytes as the default -- they reach nothing"
	else
		grep -E "^  (differs|only in)" "$W/cmp.txt" | sed 's/^/       /'
		ok "(b) the OLD values spelled out are a DIFFERENT bake from the default"
	fi
	# and WHICH files: terrain identity reaches the .BTR, object identity the
	# .BTO. Both must be among the movers or only one half of the ruling is live.
	moved_btr="$(grep -cE "^  differs.*[.]BTR" "$W/cmp.txt")"
	moved_bto="$(grep -cE "^  differs.*[.]BTO" "$W/cmp.txt")"
	say "files that moved: $moved_btr .BTR, $moved_bto .BTO"
	if [ "$moved_btr" -ge 1 ] && [ "$moved_bto" -ge 1 ]; then
		ok "(b) both a .BTR and a .BTO moved, so both identity switches reach their file"
	else
		bad "(b) $moved_btr .BTR and $moved_bto .BTO moved; each floor is 1"
	fi
fi

# ---- (c) the legacy files carry vanilla's layout -----------------------------
if [ "${PHASES#*c}" != "$PHASES" ]; then
	echo "== (c) .BTR and .BTO descriptors at dim 4, 8, 16, 32 =="
	LAND_VAN=52776558133763
	OBJ_PLAIN=474989027590661
	for dim in ${DIMS:-4 8 16 32}; do
		bake "$NS" "c_new_$dim" "$dim" -20 24 -20 24 -- --road-detail 1 --no-ao
		btr="$(find "$W/c_new_$dim" -name "*.BTR" | head -1)"
		bto="$(find "$W/c_new_$dim" -name "*.BTO" | head -1)"
		if [ -z "$btr" ]; then
			bad "(c) dim $dim: no .BTR came out at all"
			continue
		fi
		dr="$(descOf "$NS" "$btr")"
		say "dim $dim: BTR desc $dr (vanilla $LAND_VAN)"
		[ "$dr" = "$LAND_VAN" ] && ok "(c) dim $dim .BTR is vanilla's land descriptor" \
			|| bad "(c) dim $dim .BTR descriptor is $dr, not vanilla's $LAND_VAN"
		if [ -z "$bto" ]; then
			# A LEVEL WITH NO OBJECT CHUNK IS NOT A REGRESSION BY ITSELF. This
			# one-cell region has no object that survives to the dim-16 level,
			# so no .BTO is written -- but that claim has to be PROVED against
			# the rung, or a chunk we stopped writing would pass silently.
			bake "$NS" "c_wide_$dim" "$dim" -20 24 -20 24 -- --road-detail 1 --no-ao \
				--identity --terrain-identity
			rb="$(find "$W/c_wide_$dim" -name "*.BTO" | head -1)"
			say "dim $dim: no .BTO by default; with identity spelled ON it wrote ${rb:-none}"
			[ -z "$rb" ] \
				&& ok "(c) dim $dim writes no .BTO with identity either way (nothing was lost)" \
				|| bad "(c) dim $dim: identity ON wrote a .BTO here and the default did not"
			continue
		fi
		do_="$(descOf "$NS" "$bto")"
		say "dim $dim: BTO desc $do_ (plain $OBJ_PLAIN)"
		[ "$do_" = "$OBJ_PLAIN" ] && ok "(c) dim $dim .BTO has no colour and no UV 2" \
			|| bad "(c) dim $dim .BTO descriptor is $do_, not the plain $OBJ_PLAIN"
	done
	# REFUTER, rung-free: the SAME exe asked for identity must show the WIDE
	# descriptors on the same chunk. The reader is `descOf`, unchanged, so a
	# reader that always answered "vanilla" would go red here.
	bake "$NS" c_wide_4 4 -20 24 -20 24 -- --road-detail 1 --no-ao \
		--identity --terrain-identity
	rbtr="$(find "$W/c_wide_4" -name "*.BTR" | head -1)"
	rbto="$(find "$W/c_wide_4" -name "*.BTO" | head -1)"
	rdr="$(descOf "$NS" "$rbtr")"
	rdo="$(descOf "$NS" "$rbto")"
	say "refuter, identity spelled ON at dim 4: BTR desc $rdr | BTO desc $rdo"
	[ -n "$rdr" ] && [ "$rdr" != "$LAND_VAN" ] \
		&& ok "(c) refuter: --terrain-identity gives the WIDE land descriptor, so the read can differ" \
		|| bad "(c) refuter broken: --terrain-identity still reads as vanilla's $LAND_VAN"
	[ -n "$rdo" ] && [ "$rdo" != "$OBJ_PLAIN" ] \
		&& ok "(c) refuter: --identity gives the wide object descriptor (colour + UV 2)" \
		|| bad "(c) refuter broken: --identity still reads as the plain $OBJ_PLAIN"
fi

# ---- (d) the far rings are not empty -----------------------------------------
if [ "${PHASES#*d}" != "$PHASES" ]; then
	echo "== (d) arrays and impostor cards with identity OFF =="
	DIMD="${DIMD:-16}"
	if [ ! -d "$CARDS" ]; then
		echo "  SKIP: no impostor card directory at $CARDS"
	else
		CA="$(cygpath -m "$CARDS" 2>/dev/null || echo "$CARDS")"
		bake "$NS" d_new "$DIMD" -20 24 -20 24 -- --road-detail 1 --no-ao \
			--arrays --impostors "$CA" --slot-fallback
		bake "$NS" d_on "$DIMD" -20 24 -20 24 -- --road-detail 1 --no-ao \
			--arrays --impostors "$CA" --slot-fallback --identity --terrain-identity
		mn="$(find "$W/d_new" -name "*.manifest.txt" | head -1)"
		mr="$(find "$W/d_on"  -name "*.manifest.txt" | head -1)"
		if [ -z "$mn" ]; then
			bad "(d) the default bake wrote NO manifest with identity off"
		elif [ -z "$mr" ]; then
			bad "(d) identity spelled ON wrote no manifest -- the comparison has no other side"
		else
			for tag in rows C I M A; do
				case "$tag" in
					rows) pn="$(grep -c "^[0-9]" "$mn")"; pr="$(grep -c "^[0-9]" "$mr")" ;;
					*)    pn="$(grep -c "^$tag " "$mn")"; pr="$(grep -c "^$tag " "$mr")" ;;
				esac
				say "$tag lines: default (identity off) $pn | identity spelled on $pr"
				[ "$pn" -gt 0 ] || { bad "(d) no $tag lines with identity off (floor 1)"; continue; }
				[ "$pn" = "$pr" ] && ok "(d) $tag line count is unchanged with identity off ($pn)" \
					|| bad "(d) $tag lines: $pn with identity off vs $pr with it on"
			done
			an="$(find "$W/d_new/tex" -name "*LodgenArrays*" | wc -l)"
			ar="$(find "$W/d_on/tex"  -name "*LodgenArrays*" | wc -l)"
			say "texture-array files: identity off $an | identity on $ar"
			[ "$an" -gt 0 ] && [ "$an" = "$ar" ] \
				&& ok "(d) the texture arrays are still written with identity off ($an files)" \
				|| bad "(d) texture arrays: $an with identity off vs $ar with it on"
		fi
	fi
fi

# ---- (e) the native data does not thin out -----------------------------------
if [ "${PHASES#*e}" != "$PHASES" ]; then
	echo "== (e) the native .lodo / .lodi / .lodt / .lodl with identity OFF =="
	# Every OTHER switch is spelled equal on both sides, so the ONLY difference
	# between these two runs is the identity flag: OFF by default, ON when it is
	# spelled. The native files must not move either way -- that is item 2's
	# whole claim, and it needs no second binary to say it.
	# `--lodl` and `--vt` are NOT asked for here. Both run their own whole-world
	# pass and return before the chunk pass, so a run carrying them writes the
	# land file and NO .lodo / .lodi at all -- the first version of this phase
	# compared one `Commonwealth.lodl` and called it a green.
	for who in e_new e_on; do
		[ "$who" = e_on ] && IDSW="--identity --terrain-identity" || IDSW=""
		mkdir -p "$W/$who" "$W/$who/tex" "$W/$who/nat"
		# shellcheck disable=SC2086
		"$NS" -no-gui lodgen "$ESM" --worldspace 3C \
			--terrain-region -20 24 -19 25 --dim 4 --data-root "$DATA" \
			--out-dir "$WA/$who" --tex-dir "$WA/$who/tex" \
			--native "$WA/$who/nat" \
			--cover --road-detail 1 --road-ground-paint 0 $LAND_NEW $IDSW \
			> "$W/$who.log" 2>&1
		say "$who: rc=$?, $(find "$W/$who/nat" -type f 2>/dev/null | wc -l) native files"
	done
	nsame=0; ndiff=0; nmiss=0
	for rel in $(cd "$W/e_new" && find nat -type f 2>/dev/null | sed 's|\\|/|g' | sort); do
		A="$W/e_on/$rel"; B="$W/e_new/$rel"
		if [ ! -f "$A" ]; then say "only in the identity-off tree: $rel"; nmiss=$(( nmiss + 1 ))
		elif cmp -s "$A" "$B"; then nsame=$(( nsame + 1 ))
		else say "DIFFERS: $rel ($(stat -c%s "$A") vs $(stat -c%s "$B") bytes)"; ndiff=$(( ndiff + 1 ))
		fi
	done
	say "native files: $nsame identical, $ndiff differ, $nmiss only on the identity-off side"
	[ "$nsame" -gt 0 ] || bad "(e) no native files were produced at all (floor 1)"
	[ "$nsame" -gt 0 ] && { [ "$ndiff" -eq 0 ] && [ "$nmiss" -eq 0 ] \
		&& ok "(e) the native data is byte-identical with identity off ($nsame files)" \
		|| bad "(e) $ndiff native files moved and $nmiss appeared when identity went off"; }

	LODO="$(find "$W/e_new/nat" -name "*.lodo" | head -1)"   # find: either layout
	LODI="$(find "$W/e_new/nat" -name "*.lodi" | head -1)"
	if [ -n "$LODO" ] && [ -n "$LODI" ]; then
		if "$NS" -no-gui lodgen "$ESM" --worldspace 3C --native-verify \
			"$(cygpath -m "$LODO")" "$(cygpath -m "$LODI")" > "$W/e_verify.txt" 2>&1; then
			ok "(e) --native-verify is clean on the identity-off pair"
		else
			bad "(e) --native-verify failed on the identity-off pair"
			tail -8 "$W/e_verify.txt" | sed 's/^/       /'
		fi
	else
		bad "(e) no .lodo / .lodi pair to verify"
	fi

	# THE REFUTER, replacing the one the rung used to carry. The old one said
	# "the OLD binary's --no-identity thinned the native data"; that is a fact
	# about a binary this tree cannot build any more. What has to be shown here
	# instead is that the file-for-file comparator above CAN report a
	# difference: a switch that really does reach the native files
	# (--native-ladder, the simplified levels; the default since 2026-09-17 is one
	# authored level a mesh) must
	# make it say so. Without this, "0 differ" could mean "it never compared".
	mkdir -p "$W/e_ref" "$W/e_ref/tex" "$W/e_ref/nat"
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region -20 24 -19 25 --dim 4 --data-root "$DATA" \
		--out-dir "$WA/e_ref" --tex-dir "$WA/e_ref/tex" \
		--native "$WA/e_ref/nat" \
		--cover --road-detail 1 --road-ground-paint 0 $LAND_NEW \
		--native-ladder > "$W/e_ref.log" 2>&1
	rdiff=0
	for rel in $(cd "$W/e_new" && find nat -type f 2>/dev/null | sed 's|\\|/|g' | sort); do
		cmp -s "$W/e_ref/$rel" "$W/e_new/$rel" || rdiff=$(( rdiff + 1 ))
	done
	say "--native-ladder: $rdiff of the native files differ from the default"
	[ "$rdiff" -gt 0 ] \
		&& ok "(e) refuter: a switch that reaches the native files makes the comparator say so ($rdiff files)" \
		|| bad "(e) refuter broken: --native-ladder wrote byte-identical native files"
fi

# ---- (f) a REFUSED switch value says what actually stands --------------------
if [ "${PHASES#*f}" != "$PHASES" ]; then
	echo "== (f) a --land-guide value that is not one of the six =="
	# src/nifcli.cpp:7610 prints "... is not one of off|drag|aspect|aspecthex|
	# slopewarp|flatwarp; off stands" and then calls NOTHING, so what stands is
	# the ruled DEFAULT (flatwarp:1.0), not off. The bake is right and must not
	# move -- changing it would be a default change -- so what is gated is that
	# the sentence names the value that is really in force.
	bake "$NS" f_bad 4 -20 24 -20 24 -- --road-detail 1 --land-guide flatwrap
	[ -d "$W/f_def" ] || bake "$NS" f_def 4 -20 24 -20 24 -- --road-detail 1
	bake "$NS" f_off 4 -20 24 -20 24 -- --road-detail 1 --land-guide off
	# the floor: --land-guide off is a DIFFERENT bake, or nothing below means
	# anything
	if treecmp "$W/f_off" "$W/f_def" "[.]lodb" >/dev/null 2>&1; then
		bad "(f) --land-guide off bakes the same bytes as the default -- the floor is gone"
	else
		ok "(f) --land-guide off is a different bake from the default (the floor)"
	fi
	WARN="$(grep -a -m1 "is not one of" "$W/f_bad.log")"
	say "the warning: ${WARN:-<none>}"
	if [ -z "$WARN" ]; then
		bad "(f) a bad --land-guide value printed no warning at all"
	elif treecmp "$W/f_bad" "$W/f_def" "[.]lodb" >/dev/null 2>&1; then
		ok "(f) the bad value left the DEFAULT bake standing, byte for byte"
		case "$WARN" in
		*"off stands"*)
			bad "(f) the warning says OFF stands, and the bake is the DEFAULT one, not the off one" ;;
		*)
			ok "(f) the warning does not claim an outcome the bake did not have" ;;
		esac
	else
		# it changed the bake: then it must be the one the message names
		if treecmp "$W/f_bad" "$W/f_off" "[.]lodb" >/dev/null 2>&1; then
			ok "(f) the bad value really did fall back to off, as the warning says"
		else
			bad "(f) the bad value baked neither the default nor the off bake"
		fi
	fi
fi

echo "$checks checks, $fails failures"
[ "$fails" -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ "$fails" -eq 0 ]
