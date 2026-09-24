p = 'src/nativeemit.cpp'
s = open(p, encoding='utf-8', newline='').read()


def rep(o, n, c=1):
    global s
    assert s.count(o) == c, (s.count(o), o[:90])
    s = s.replace(o, n)


# 1. cut the struct out of the middle of the function, leaving the narrative comment
old = """	/* ---- v7: THE GROUPING. bungo 2026-09-18: "The houses should be one object
	 * each though, for identity".
	 *
	 * THIS RULE IS A PROPOSAL. Every knob it turns is in one struct, right
	 * here, so that ruling on it is a matter of changing four numbers rather
	 * than reading the loop. `identity` is NOT touched: it stays the stock
	 * bake's unique per-placement index, and the group is a SECOND word beside
	 * it (docs s4.9). */
	struct GroupKnobs
	{"""
head = """	/* ---- v7: THE GROUPING. bungo 2026-09-18: "The houses should be one object
	 * each though, for identity".
	 *
	 * THIS RULE IS A PROPOSAL. Every knob it turns is in `GroupKnobs` above
	 * this file's one emit function, so that ruling on it is a matter of
	 * changing four numbers rather than reading the loop. `identity` is NOT
	 * touched: it stays the stock bake's unique per-placement index, and the
	 * group is a SECOND word beside it (docs s4.9). */
	const GroupKnobs KNOB;
"""
i = s.index(old)
j = s.index("\t};\n\tconst GroupKnobs KNOB;\n", i)
body = s[i + len(old) - len("\tstruct GroupKnobs\n\t{"):j + len("\t};\n")]
assert body.startswith("\tstruct GroupKnobs") and body.endswith("\t};\n"), body[:40]
s = s[:i] + head + s[j + len("\t};\n\tconst GroupKnobs KNOB;\n"):]

# 2. paste it at file scope, one tab shallower, right before the emit function
body = '\n'.join(l[1:] if l.startswith('\t') else l for l in body.split('\n'))
rep("""bool lodgenNativeWrite( QString * report, QString * error )""",
    """/*! v7 grouping: THE KNOBS, in one place, because the rule is a PROPOSAL and
 *  ruling on it must be a matter of changing four numbers. Read by the
 *  placement loop (which placements are eligible) and by the grouping block
 *  (how they merge). */
""" + body + """
bool lodgenNativeWrite( QString * report, QString * error )""")

open(p, 'w', encoding='utf-8', newline='').write(s)
print('knobs hoisted to file scope')
