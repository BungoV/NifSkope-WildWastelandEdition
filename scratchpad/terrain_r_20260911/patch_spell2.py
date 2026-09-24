p = 'tests/spells/lodgen_terrain_vt.sh'
s = open(p, 'r', encoding='utf-8', newline='').read()


def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (a[:70], s.count(a))
    s = s.replace(a, b)


# the mask-missing mutation must be refused by the MISSING-MASK rule, not by the
# cover-carrier rule it tripped first; equalise the formats so only one can fire
rep("""    # the mask renamed emissive, so no mask sheet exists at all
    ('maskSheetMissing', lambda x: x.__setitem__(0xA0 + 2 * 8 + 4, 6)),""",
    """    # the mask renamed emissive, so no mask sheet exists at all. Its cover
    # format is equalised in the same mutation, or the cover-carrier rule fires
    # first and the missing-mask rule is never reached -- a mutation testing a
    # rule that is not the one it is named for.
    ('maskSheetMissing', lambda x: (x.__setitem__(0xA0 + 2 * 8 + 4, 6),
                                    w16(x, 0xA0 + 2 * 8 + 2,
                                        int.from_bytes(x[0xA0 + 2 * 8:0xA0 + 2 * 8 + 2],
                                                       'little')))),""")

rep("""			maskSheetMissing)  echo "$msg" | grep -qa 'mask' || named=$((named + 1)) ;;""",
    """			maskSheetMissing)  echo "$msg" | grep -qa 'must carry the colour' || named=$((named + 1)) ;;""")

# V12: the family word is real now, and the index's own new keys are read from
# the JSON rather than from --lodm-check, whose printer emits scalars only
rep("""grep -qa '^lodm family legacy' "$W/lodm.txt" && ok "V12 with family legacy, which is what parses today" \\
	|| bad "V12 with family legacy\"""",
    """# THE FAMILY WORD IS REAL SINCE 2026-09-11 (bungo 09:5x): it said `legacy` and
# the contract called it vestigial; the sheets are the object family's now.
grep -qa '^lodm family pbr' "$W/lodm.txt" && ok "V12 with family pbr, which the rule census below makes auditable" \\
	|| bad "V12 with family pbr"
# The nested keys --lodm-check does not print (its printer emits scalars only),
# read straight out of the envelope's JSON.
"$PY" - "$IDX" > "$W/idx.txt" 2>&1 <<'IDXEOF'
import json, sys
b = open(sys.argv[1], 'rb').read()
d = json.loads(b[12:].decode('utf-8'))
t = d['terrain']
roles = [x['role'] for x in t['sheets']]
print('roles %s' % roles)
print('emissive %s' % t.get('emissive'))
print('dropped %s' % sorted((t.get('dropped') or {}).keys()))
r = t.get('maskRules') or {}
print('rules %s' % json.dumps(r, sort_keys=True))
served = (r.get('pbrm', 0) + r.get('legacyInverted', 0) + r.get('noneDefault', 0))
print('served %d distinct %d' % (served, r.get('distinctLtex', -1)))
IDXEOF
sed 's/^/       /' "$W/idx.txt"
grep -qa "roles \\['color', 'msn', 'mask'" "$W/idx.txt" \\
	&& ok "V12 the sheets are colour, msn and MASK in that order" \\
	|| bad "V12 the sheets are colour, msn and MASK in that order"
grep -qa "'data'" "$W/idx.txt" && bad "V12 the retired data role appears nowhere in the index" \\
	|| ok "V12 the retired data role appears nowhere in the index"
grep -qa "^emissive \\(none\\|present\\)" "$W/idx.txt" \\
	&& ok "V12 the index says in WORDS whether an emissive sheet exists" \\
	|| bad "V12 the index says whether an emissive sheet exists"
grep -qa "dropped \\['shoreProximity', 'wetness'\\]" "$W/idx.txt" \\
	&& ok "V12 the index names what was dropped and where a consumer gets it instead" \\
	|| bad "V12 the index names what was dropped"
# THE CENSUS IS WRITTEN AND IT MOVES (the three rules of 2026-09-04 21:33): the
# rules must account for every distinct landscape texture, and the count must
# not be zero -- an all-zero census would pass a mere presence check.
SERVED="$(grep -oa '^served [0-9]*' "$W/idx.txt" | awk '{print $2}')"
DISTINCT="$(grep -oa 'distinct [0-9-]*' "$W/idx.txt" | awk '{print $2}')"
[ -n "$SERVED" ] && [ "$SERVED" = "$DISTINCT" ] && [ "$SERVED" -gt 0 ] \\
	&& ok "V12 the per-layer rule census accounts for every landscape texture ($SERVED of $DISTINCT)" \\
	|| bad "V12 the per-layer rule census accounts for every landscape texture (served ${SERVED:-?}, distinct ${DISTINCT:-?})\"""")

open(p, 'w', encoding='utf-8', newline='').write(s)
b = open(p, 'rb').read()
print('CR', b.count(b'\r'), 'LF', b.count(b'\n'))
