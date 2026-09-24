p = 'tests/spells/lodgen_terrain_vt.sh'
s = open(p, 'r', encoding='utf-8', newline='').read()


def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (a[:70], s.count(a))
    s = s.replace(a, b)


rep("""# Data\\Terrain\\, indexed by a terrainVT .lodm, FOUR sheets a tile -- colour,
# model-space normal, data (AO, wetness, shore, cover) and HEIGHT (R16, the
# shadow heightmap's own encoding, on the same grid with the same border, so a
# consumer that wants nested grids has the geometry side too).""",
"""# Data\\Terrain\\, indexed by a terrainVT .lodm, carrying the OBJECT TEXTURE
# FAMILY since container version 2 (bungo, 2026-09-11 09:5x): colour,
# model-space normal, MASK (rmaos -- R roughness, G metallic, B sky AO, A ground
# cover), HEIGHT (R16, the shadow heightmap's own encoding, on the same grid
# with the same border) and, only when a layer supplies one, EMISSIVE.
# Version 1's role-3 `data` sheet -- AO, wetness, shore, cover -- is RETIRED:
# shore proximity is a runtime subtraction from the .lodl water planes and
# wetness is a close-up effect. A v1 file is refused, not converted.""")

rep("""    ('version',      lambda x: w32(x, 4, 2)),""",
    """    # version 2 is what this build writes, so the mutation is to ONE -- the
    # retired four-sheet layout -- which must be refused by NAME rather than as
    # "bad version", and to a future 3 which must not be guessed at
    ('versionOneRetired', lambda x: w32(x, 4, 1)),
    ('versionFuture', lambda x: w32(x, 4, 3)),""")

rep("""    ('sheetFormat',  lambda x: w16(x, 0xA0, 999)),
    ('sheetRole',    lambda x: x.__setitem__(0xA4, 0)),""",
    """    ('sheetFormat',  lambda x: w16(x, 0xA0, 999)),
    ('sheetRole',    lambda x: x.__setitem__(0xA4, 0)),
    # --- version 2's own roles and stamps, one mutation each ---
    # the mask sheet (index 2) put back to the retired role 3
    ('roleDataRetired', lambda x: x.__setitem__(0xA0 + 2 * 8 + 4, 3)),
    # the mask renamed emissive, so no mask sheet exists at all
    ('maskSheetMissing', lambda x: x.__setitem__(0xA0 + 2 * 8 + 4, 6)),
    # a SECOND cover carrier: the colour sheet given a differing cover format
    ('twoCoverCarriers', lambda x: w16(x, 0xA0 + 2, 77)),
    # the cover alpha claimed by a sheet that may not carry it (the msn)
    ('coverCarrierOnMsn', lambda x: w16(x, 0xA0 + 1 * 8 + 2, 77)),
    # a role outside the version 2 set
    ('roleOutOfRange', lambda x: x.__setitem__(0xA0 + 2 * 8 + 4, 9)),
    # the reserved tail moved from 0xC0 to 0xD0 in v2; the two words that were
    # reserved in v1 are sheet descriptors now and must be zero past sheetCount
    ('sheetPastCount', lambda x: x.__setitem__(0xA0 + 5 * 8 + 4, 5)),""")

rep("""	if "$NS" -no-gui lodgen --lodt-check "$m" > "$W/m.txt" 2>&1; then
		say "$name: ACCEPTED (it should have been refused)"
	else
		msg="$(grep -a '^lodt refused' "$W/m.txt" | head -1)"
		say "$name: ${msg:-refused with no message}"
		refused=$((refused + 1))
	fi
done
say "refusals: $refused of $total"
[ "$refused" = "$total" ] && ok "V2 every mutated container is refused" \\
	|| bad "V2 every mutated container is refused ($refused of $total)\"""",
"""	if "$NS" -no-gui lodgen --lodt-check "$m" > "$W/m.txt" 2>&1; then
		say "$name: ACCEPTED (it should have been refused)"
	else
		msg="$(grep -a '^lodt refused' "$W/m.txt" | head -1)"
		say "$name: ${msg:-refused with no message}"
		refused=$((refused + 1))
		# REFUSED BY NAME, not merely refused: a validator that answers
		# "invalid" to everything passes a count and cannot be mutation-tested.
		# Only the version 2 rows are matched by text here; the older rows kept
		# their count-only bar so this lane moves no number it did not mean to.
		case "$name" in
			versionOneRetired) echo "$msg" | grep -qa 'version 1 container' || named=$((named + 1)) ;;
			roleDataRetired)   echo "$msg" | grep -qa 'role 3' || named=$((named + 1)) ;;
			maskSheetMissing)  echo "$msg" | grep -qa 'mask' || named=$((named + 1)) ;;
			twoCoverCarriers)  echo "$msg" | grep -qa 'both declare' || named=$((named + 1)) ;;
			coverCarrierOnMsn) echo "$msg" | grep -qa 'ground-cover alpha' || named=$((named + 1)) ;;
		esac
	fi
done
say "refusals: $refused of $total"
[ "$refused" = "$total" ] && ok "V2 every mutated container is refused" \\
	|| bad "V2 every mutated container is refused ($refused of $total)"
[ "$named" = "0" ] && ok "V2b the five version-2 mutations are refused BY NAME" \\
	|| bad "V2b $named of the five version-2 mutations refused without naming their rule\"""")

rep("""refused=0
total=0
for m in "$W/mut"/*.lodt; do""",
    """refused=0
total=0
named=0
for m in "$W/mut"/*.lodt; do""")

open(p, 'w', encoding='utf-8', newline='').write(s)
b = open(p, 'rb').read()
print('CR', b.count(b'\r'), 'LF', b.count(b'\n'))
