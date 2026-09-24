#!/usr/bin/env python
"""ROW B: make tests/spells/lodgen_defaults.sh rung-free.

Applied from a FILE, not a heredoc: the replacements carry backslashes and
MSYS2/Git-Bash collapses them inside a quoted heredoc.

Every replacement must match EXACTLY ONCE or the script refuses and writes
nothing.
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_defaults.sh'

R = []


def sub(old, new):
    R.append((old, new))


# ---------------------------------------------------------------- the header
sub(b'''# Five phases, each a number with the floor or the refuter it is judged against.
# Every one of them compares the NEW exe with the RUNG
# (`release/NifSkope.before_defaults1.exe`), because a default is only a default
# if the same bytes come out of the old exe when you spell the switch.
#
#   (a) DEFAULTS ARE THE SWITCHES. The new exe's DEFAULT bake equals the rung's
#       bake with `--no-identity --no-terrain-identity --road-ground-paint 0`
#       and the four land switches of panel (c), file for file. The one
#       ALLOWED difference is the `.manifest.txt` sidecar, which item 2
#       deliberately writes with identity off and the rung did not: the gate
#       names every differing file and fails on any that is not a manifest.
#   (b) THE WAY BACK EXISTS. The new exe with the old switches spelled out
#       (`--identity --terrain-identity --road-ground-paint 1 --land-hex 0
#       --land-warp 0 --land-mip-bias 0 --land-guide off`) equals the rung's
#       DEFAULT bake, file for file, with nothing excused.
#   (c) THE LEGACY FILES ARE VANILLA'S LAYOUT. Every `.BTR` of the default bake
#       carries vanilla's land descriptor 52776558133763 and every `.BTO`
#       carries the plain object descriptor 474989027590661 (no colour, no
#       UV 2), at dim 4, 8, 16 and 32 over Sanctuary. REFUTER: the same read on
#       the RUNG's default bake shows the wide identity descriptors, so the
#       check is capable of failing.
#   (d) THE FAR RINGS ARE NOT EMPTY. The same chunk baked with `--arrays` and
#       `--impostors`: the new exe with identity OFF places the same number of
#       cards, array layers and manifest rows as the rung did with identity ON.
#       FLOOR: the counts must be non-zero, or an empty bake would pass.
#   (e) THE NATIVE DATA DOES NOT THIN OUT. `.lodo` / `.lodi` / `.lodt` /
#       `.lodl` are byte-identical between the rung (identity ON, its default)
#       and the new exe (identity OFF, its default) with every OTHER switch
#       spelled equal, and `--native-verify` is clean on the new pair.
#       REFUTER: the rung asked for `--no-identity` writes DIFFERENT native
#       files -- that is the thinning item 2 removed.
''', b'''# RUNG-FREE SINCE 2026-09-16 (lane GENSMALL1, director ROW B).
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
''')

# ------------------------------------------------------------ the rung itself
sub(b'RUNG="${RUNG:-$ROOT/release/NifSkope.before_defaults1.exe}"\n',
    b'# no RUNG: see the header. Every comparison is this exe against itself with\n'
    b'# the switches spelled out.\n')

sub(b'[ -x "$RUNG" ] || { echo "no rung at $RUNG"; exit 2; }\n', b'')

sub(b'echo "== lodgen_defaults: the two exes =="\n'
    b'ls -l --time-style=+%Y-%m-%d_%H:%M:%S "$NS" "$RUNG" | sed \'s/^/       /\'\n',
    b'echo "== lodgen_defaults: the exe under test =="\n'
    b'ls -l --time-style=+%Y-%m-%d_%H:%M:%S "$NS" | sed \'s/^/       /\'\n')

# ----------------------------------------------------------------- phase (a)
sub(b'''	echo "== (a) the new exe's DEFAULT bake == the rung's bake with the switches spelled =="
	bake "$NS" a_new 4 -20 24 -20 24 -- --road-detail 1
	bake "$RUNG" a_rung 4 -20 24 -20 24 -- --road-detail 1 \\
		--no-identity --no-terrain-identity --road-ground-paint 0 $LAND_NEW
	if treecmp "$W/a_rung" "$W/a_new" "manifest|[.]lodb"; then
		ok "(a) default bake == rung with the switches spelled, sidecar and ledger excused"
	else
		bad "(a) default bake differs from the rung with the switches spelled"
	fi
	if lodbcmp "$W/a_rung/Commonwealth.lodb" "$W/a_new/Commonwealth.lodb"; then
''', b'''	echo "== (a) the DEFAULT bake == the same bake with the switches SPELLED OUT =="
	bake "$NS" a_new 4 -20 24 -20 24 -- --road-detail 1
	bake "$NS" a_spelled 4 -20 24 -20 24 -- --road-detail 1 \\
		--no-identity --no-terrain-identity --road-ground-paint 0 $LAND_NEW
	# only the ledger is excused, and only because it hashes the switch LIST
	if treecmp "$W/a_spelled" "$W/a_new" "[.]lodb"; then
		ok "(a) default bake == the switches spelled out, only the switch-list ledger excused"
	else
		bad "(a) default bake differs from the same switches spelled out"
	fi
	if lodbcmp "$W/a_spelled/Commonwealth.lodb" "$W/a_new/Commonwealth.lodb"; then
''')

sub(b'''	if ls "$W"/a_rung/*.manifest.txt >/dev/null 2>&1; then
		bad "(a) refuter broken: the RUNG wrote a manifest with --no-identity"
	else
		ok "(a) refuter: the rung with --no-identity wrote NO manifest (that is what changed)"
	fi
''', b'''	# THE REFUTER, replacing the one the rung used to carry: the SAME
	# comparison with ONE switch deliberately wrong. bungo ruled hex 256; this
	# bake spells 0 and everything else right, and (a) must go red on it. If it
	# does not, (a) is not reading the land switches at all and its green above
	# means nothing.
	bake "$NS" a_wrong 4 -20 24 -20 24 -- --road-detail 1 \\
		--no-identity --no-terrain-identity --road-ground-paint 0 \\
		--land-hex 0 --land-warp 341 --land-mip-bias -0.22 --land-guide flatwarp:1.0
	if treecmp "$W/a_wrong" "$W/a_new" "[.]lodb" >/dev/null 2>&1; then
		bad "(a) refuter broken: --land-hex 0 bakes the same bytes as the ruled 256"
	else
		ok "(a) refuter: ONE wrong explicit switch (--land-hex 0) breaks the same comparison"
	fi
''')

# ---------------------------------------------------------------- phase (a2)
sub(b'''	bake "$NS" a2_new 4 -20 20 -17 23 -- --road-detail 1 --cover --roads
	bake "$RUNG" a2_rung 4 -20 20 -17 23 -- --road-detail 1 --cover --roads \\
		--no-identity --no-terrain-identity --road-ground-paint 0 $LAND_NEW
	if treecmp "$W/a2_rung" "$W/a2_new" "manifest|[.]lodb"; then
		ok "(a2) roads: default bake == rung with the switches spelled"
	else
		bad "(a2) roads: default bake differs from the rung with the switches spelled"
	fi
	if lodbcmp "$W/a2_rung/Commonwealth.lodb" "$W/a2_new/Commonwealth.lodb"; then
		ok "(a2) roads: the ledger agrees, file for file and hash for hash"
	else
		bad "(a2) roads: the ledger says a file other than the sidecar moved"
	fi
	# and the refuter for (d): the OLD verge default is a DIFFERENT bake
	bake "$RUNG" a2_old 4 -20 20 -17 23 -- --road-detail 1 --cover --roads \\
		--no-identity --no-terrain-identity $LAND_NEW
	if treecmp "$W/a2_old" "$W/a2_new" "manifest|[.]lodb" >/dev/null 2>&1; then
		bad "(a2) refuter broken: road-ground-paint 1 and 0 give the same bytes"
	else
		ok "(a2) refuter: the rung at road-ground-paint 1 bakes DIFFERENT bytes"
	fi
''', b'''	bake "$NS" a2_new 4 -20 20 -17 23 -- --road-detail 1 --cover --roads
	bake "$NS" a2_spelled 4 -20 20 -17 23 -- --road-detail 1 --cover --roads \\
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
	bake "$NS" a2_old 4 -20 20 -17 23 -- --road-detail 1 --cover --roads \\
		--no-identity --no-terrain-identity --road-ground-paint 1 $LAND_NEW
	if treecmp "$W/a2_old" "$W/a2_new" "[.]lodb" >/dev/null 2>&1; then
		bad "(a2) refuter broken: road-ground-paint 1 and 0 give the same bytes"
	else
		ok "(a2) refuter: --road-ground-paint 1 bakes DIFFERENT bytes from the ruled 0"
	fi
''')

# ----------------------------------------------------------------- phase (b)
sub(b'''	echo "== (b) the new exe with the OLD switches spelled == the rung's DEFAULT bake =="
	bake "$NS" b_new 4 -20 24 -20 24 -- --road-detail 1 \\
		--identity --terrain-identity --road-ground-paint 1 $LAND_OLD
	bake "$RUNG" b_rung 4 -20 24 -20 24 -- --road-detail 1
	# Only the .lodb ledger is excused, and only because it hashes the SWITCH
	# LIST: spelled-out and defaulted can never agree there. Everything the
	# game actually loads -- .BTR, .BTO, the sidecar, the three sheets -- is
	# held to the byte.
	if treecmp "$W/b_rung" "$W/b_new" "[.]lodb"; then
		ok "(b) the way back is byte-exact, only the switch-list ledger excused"
	else
		bad "(b) the way back does not reproduce the rung's default bake"
	fi
	if lodbcmp "$W/b_rung/Commonwealth.lodb" "$W/b_new/Commonwealth.lodb"; then
		ok "(b) the ledger agrees: same files, same hashes, on the way back"
	else
		bad "(b) the ledger says the way back wrote a different file"
	fi
''', b'''	echo "== (b) the OLD values spelled out are a DIFFERENT bake, file by file =="
	# The rung-free half of the old (b). It used to say "the way back equals the
	# rung's default bake", which needed the old binary. What it can say on this
	# exe alone is the half that matters to a user: asking for the old values
	# CHANGES the output, and it changes the files the switches are supposed to
	# reach. A switch that is accepted and changes nothing is the failure this
	# phase exists to catch.
	bake "$NS" b_old 4 -20 24 -20 24 -- --road-detail 1 \\
		--identity --terrain-identity --road-ground-paint 1 $LAND_OLD
	if treecmp "$W/b_old" "$W/a_new" "[.]lodb" >/dev/null 2>&1; then
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
''')

# ----------------------------------------------------------------- phase (c)
sub(b'''			bake "$RUNG" "c_rung_$dim" "$dim" -20 24 -20 24 -- --road-detail 1 --no-ao
			rb="$(find "$W/c_rung_$dim" -name "*.BTO" | head -1)"
			say "dim $dim: no .BTO from the new exe; the rung wrote ${rb:-none}"
			[ -z "$rb" ] \\
				&& ok "(c) dim $dim writes no .BTO on EITHER exe (nothing was lost)" \\
				|| bad "(c) dim $dim: the rung wrote a .BTO here and the new exe did not"
''', b'''			bake "$NS" "c_wide_$dim" "$dim" -20 24 -20 24 -- --road-detail 1 --no-ao \\
				--identity --terrain-identity
			rb="$(find "$W/c_wide_$dim" -name "*.BTO" | head -1)"
			say "dim $dim: no .BTO by default; with identity spelled ON it wrote ${rb:-none}"
			[ -z "$rb" ] \\
				&& ok "(c) dim $dim writes no .BTO with identity either way (nothing was lost)" \\
				|| bad "(c) dim $dim: identity ON wrote a .BTO here and the default did not"
''')

sub(b'''	# REFUTER: the rung's default bake, same chunk, must show the WIDE ones
	bake "$RUNG" c_rung_4 4 -20 24 -20 24 -- --road-detail 1 --no-ao
	rbtr="$(find "$W/c_rung_4" -name "*.BTR" | head -1)"
	rbto="$(find "$W/c_rung_4" -name "*.BTO" | head -1)"
	rdr="$(descOf "$NS" "$rbtr")"
	rdo="$(descOf "$NS" "$rbto")"
	say "refuter, rung default dim 4: BTR desc $rdr | BTO desc $rdo"
	[ -n "$rdr" ] && [ "$rdr" != "$LAND_VAN" ] \\
		&& ok "(c) refuter: the rung's .BTR carries the WIDE terrain-identity descriptor" \\
		|| bad "(c) refuter broken: the rung's .BTR already read as vanilla's"
	[ -n "$rdo" ] && [ "$rdo" != "$OBJ_PLAIN" ] \\
		&& ok "(c) refuter: the rung's .BTO carries the identity descriptor (colour + UV 2)" \\
		|| bad "(c) refuter broken: the rung's .BTO already read as plain"
''', b'''	# REFUTER, rung-free: the SAME exe asked for identity must show the WIDE
	# descriptors on the same chunk. The reader is `descOf`, unchanged, so a
	# reader that always answered "vanilla" would go red here.
	bake "$NS" c_wide_4 4 -20 24 -20 24 -- --road-detail 1 --no-ao \\
		--identity --terrain-identity
	rbtr="$(find "$W/c_wide_4" -name "*.BTR" | head -1)"
	rbto="$(find "$W/c_wide_4" -name "*.BTO" | head -1)"
	rdr="$(descOf "$NS" "$rbtr")"
	rdo="$(descOf "$NS" "$rbto")"
	say "refuter, identity spelled ON at dim 4: BTR desc $rdr | BTO desc $rdo"
	[ -n "$rdr" ] && [ "$rdr" != "$LAND_VAN" ] \\
		&& ok "(c) refuter: --terrain-identity gives the WIDE land descriptor, so the read can differ" \\
		|| bad "(c) refuter broken: --terrain-identity still reads as vanilla's $LAND_VAN"
	[ -n "$rdo" ] && [ "$rdo" != "$OBJ_PLAIN" ] \\
		&& ok "(c) refuter: --identity gives the wide object descriptor (colour + UV 2)" \\
		|| bad "(c) refuter broken: --identity still reads as the plain $OBJ_PLAIN"
''')

# ----------------------------------------------------------------- phase (d)
sub(b'''		bake "$RUNG" d_rung "$DIMD" -20 24 -20 24 -- --road-detail 1 --no-ao \\
			--arrays --impostors "$CA" --slot-fallback
		mn="$(find "$W/d_new"  -name "*.manifest.txt" | head -1)"
		mr="$(find "$W/d_rung" -name "*.manifest.txt" | head -1)"
		if [ -z "$mn" ]; then
			bad "(d) the new exe wrote NO manifest with identity off"
		elif [ -z "$mr" ]; then
			bad "(d) the rung wrote no manifest -- the comparison has no other side"
''', b'''		bake "$NS" d_on "$DIMD" -20 24 -20 24 -- --road-detail 1 --no-ao \\
			--arrays --impostors "$CA" --slot-fallback --identity --terrain-identity
		mn="$(find "$W/d_new" -name "*.manifest.txt" | head -1)"
		mr="$(find "$W/d_on"  -name "*.manifest.txt" | head -1)"
		if [ -z "$mn" ]; then
			bad "(d) the default bake wrote NO manifest with identity off"
		elif [ -z "$mr" ]; then
			bad "(d) identity spelled ON wrote no manifest -- the comparison has no other side"
''')

sub(b'''				say "$tag lines: new (identity off) $pn | rung (identity on) $pr"
''', b'''				say "$tag lines: default (identity off) $pn | identity spelled on $pr"
''')

sub(b'''			an="$(find "$W/d_new/tex"  -name "*LodgenArrays*" | wc -l)"
			ar="$(find "$W/d_rung/tex" -name "*LodgenArrays*" | wc -l)"
			say "texture-array files: new $an | rung $ar"
''', b'''			an="$(find "$W/d_new/tex" -name "*LodgenArrays*" | wc -l)"
			ar="$(find "$W/d_on/tex"  -name "*LodgenArrays*" | wc -l)"
			say "texture-array files: identity off $an | identity on $ar"
''')

# ----------------------------------------------------------------- phase (e)
sub(b'''	# Every OTHER switch is spelled equal on both sides, so the ONLY difference
	# between these two runs is the identity flag: ON by default on the rung,
	# OFF by default on the new exe. The native files must not move.
''', b'''	# Every OTHER switch is spelled equal on both sides, so the ONLY difference
	# between these two runs is the identity flag: OFF by default, ON when it is
	# spelled. The native files must not move either way -- that is item 2's
	# whole claim, and it needs no second binary to say it.
''')

sub(b'''	for who in e_new e_rung; do
		[ "$who" = e_new ] && E="$NS" || E="$RUNG"
		mkdir -p "$W/$who" "$W/$who/tex" "$W/$who/nat"
		"$E" -no-gui lodgen "$ESM" --worldspace 3C \\
			--terrain-region -20 24 -19 25 --dim 4 --data-root "$DATA" \\
			--out-dir "$WA/$who" --tex-dir "$WA/$who/tex" \\
			--native "$WA/$who/nat" \\
			--cover --road-detail 1 --road-ground-paint 0 $LAND_NEW \\
			> "$W/$who.log" 2>&1
		say "$who: rc=$?, $(find "$W/$who/nat" -type f 2>/dev/null | wc -l) native files"
	done
''', b'''	for who in e_new e_on; do
		[ "$who" = e_on ] && IDSW="--identity --terrain-identity" || IDSW=""
		mkdir -p "$W/$who" "$W/$who/tex" "$W/$who/nat"
		# shellcheck disable=SC2086
		"$NS" -no-gui lodgen "$ESM" --worldspace 3C \\
			--terrain-region -20 24 -19 25 --dim 4 --data-root "$DATA" \\
			--out-dir "$WA/$who" --tex-dir "$WA/$who/tex" \\
			--native "$WA/$who/nat" \\
			--cover --road-detail 1 --road-ground-paint 0 $LAND_NEW $IDSW \\
			> "$W/$who.log" 2>&1
		say "$who: rc=$?, $(find "$W/$who/nat" -type f 2>/dev/null | wc -l) native files"
	done
''')

sub(b'''		A="$W/e_rung/$rel"; B="$W/e_new/$rel"
''', b'''		A="$W/e_on/$rel"; B="$W/e_new/$rel"
''')

sub(b'''		if [ ! -f "$A" ]; then say "only in the new tree: $rel"; nmiss=$(( nmiss + 1 ))
''', b'''		if [ ! -f "$A" ]; then say "only in the identity-off tree: $rel"; nmiss=$(( nmiss + 1 ))
''')

sub(b'''	say "native files: $nsame identical, $ndiff differ, $nmiss only on the new side"
''', b'''	say "native files: $nsame identical, $ndiff differ, $nmiss only on the identity-off side"
''')

sub(b'''	# REFUTER: the rung ASKED for identity off wrote thinner native files
	mkdir -p "$W/e_ref" "$W/e_ref/tex" "$W/e_ref/nat"
	"$RUNG" -no-gui lodgen "$ESM" --worldspace 3C \\
		--terrain-region -20 24 -19 25 --dim 4 --data-root "$DATA" \\
		--out-dir "$WA/e_ref" --tex-dir "$WA/e_ref/tex" \\
		--native "$WA/e_ref/nat" \\
		--cover --road-detail 1 --road-ground-paint 0 $LAND_NEW \\
		--no-identity --no-terrain-identity > "$W/e_ref.log" 2>&1
	rdiff=0
	for rel in $(cd "$W/e_new" && find nat -type f 2>/dev/null | sed 's|\\\\|/|g' | sort); do
		cmp -s "$W/e_ref/$rel" "$W/e_new/$rel" || rdiff=$(( rdiff + 1 ))
	done
	say "the rung asked for --no-identity: $rdiff of the native files differ from the new default"
	[ "$rdiff" -gt 0 ] \\
		&& ok "(e) refuter: on the OLD code the flag DID thin the native data ($rdiff files)" \\
		|| bad "(e) refuter broken: the old code's --no-identity native files already matched"
''', b'''	# THE REFUTER, replacing the one the rung used to carry. The old one said
	# "the OLD binary's --no-identity thinned the native data"; that is a fact
	# about a binary this tree cannot build any more. What has to be shown here
	# instead is that the file-for-file comparator above CAN report a
	# difference: a switch that really does reach the native files
	# (--native-no-ladder, which writes one level and no occluder boxes) must
	# make it say so. Without this, "0 differ" could mean "it never compared".
	mkdir -p "$W/e_ref" "$W/e_ref/tex" "$W/e_ref/nat"
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C \\
		--terrain-region -20 24 -19 25 --dim 4 --data-root "$DATA" \\
		--out-dir "$WA/e_ref" --tex-dir "$WA/e_ref/tex" \\
		--native "$WA/e_ref/nat" \\
		--cover --road-detail 1 --road-ground-paint 0 $LAND_NEW \\
		--native-no-ladder > "$W/e_ref.log" 2>&1
	rdiff=0
	for rel in $(cd "$W/e_new" && find nat -type f 2>/dev/null | sed 's|\\\\|/|g' | sort); do
		cmp -s "$W/e_ref/$rel" "$W/e_new/$rel" || rdiff=$(( rdiff + 1 ))
	done
	say "--native-no-ladder: $rdiff of the native files differ from the default"
	[ "$rdiff" -gt 0 ] \\
		&& ok "(e) refuter: a switch that reaches the native files makes the comparator say so ($rdiff files)" \\
		|| bad "(e) refuter broken: --native-no-ladder wrote byte-identical native files"
''')

d = open(P, 'rb').read()
before = len(d)
for old, new in R:
    n = d.count(old)
    if n != 1:
        sys.exit('anchor appears %d times (need 1): %r' % (n, old[:70]))
    d = d.replace(old, new)
open(P, 'wb').write(d)
orig = open(P, 'rb').read()
print('lodgen_defaults.sh  %d -> %d bytes  CR %d  LF %d  (%d replacements)'
      % (before, len(orig), orig.count(b'\r'), orig.count(b'\n'), len(R)))
