p = 'tests/spells/lodi_v7.sh'
s = open(p, encoding='utf-8', newline='').read()


def rep(o, n, c=1):
    global s
    assert s.count(o) == c, (s.count(o), o[:90])
    s = s.replace(o, n)


# the scene the render opens, beside the other fixtures
rep("""V7I="$V7DIR/Commonwealth.lodi"; V7O="$V7DIR/Commonwealth.lodo\"""",
    """V7I="$V7DIR/Commonwealth.lodi"; V7O="$V7DIR/Commonwealth.lodo"
# WW_RENDER_SHOT only arms when a FILE is on the command line -- the hook hangs off
# `completeLoading`, so an exe launched with no scene renders nothing, quits never
# and wedges the one-instance rule. Root MISTAKES 2026-09-18 10:39.
V="$ROOT/scratchpad/viewfix_20260917"
LODL="${LODL:-$V/chunkB/lodl/FO4CSLOD/Commonwealth/Commonwealth.lodl}"
SHEETS="${SHEETS:-$ROOT/scratchpad/slab1_20260918/after/vt/FO4CSLOD/Commonwealth}"
REGION="4,-12,7,-9,0\"""")

rep("""for f in "$V7I" "$V7O" "$REF" "$DEC"; do""",
    """for f in "$V7I" "$V7O" "$REF" "$DEC" "$LODL"; do""")

rep("""shot () {  # shot <tag> <channel> <lodi>
	WW_LODL_OBJECTS="$(winpath "$3")" \\
	WW_LODL_CHANNEL="$2" \\
	WW_RENDER_SHOT="$(winpath "$OUT/$1.png")" \\
	WW_RENDER_SIZE="$SIZE" WW_RENDER_CENTER="$CX,$CY,$CZ" \\
	WW_RENDER_ORTHO="$ORTHO" WW_RENDER_VIEW="$VIEW" WW_RENDER_FLAT=1 WW_RENDER_CLEAN=1 \\
	WW_WINDOW_AT=1960,40 \\
		"$EXE" --port "$PORT" > "$OUT/$1.log" 2>&1
}""",
    """shot () {  # shot <tag> <channel> <lodi>
	# the .lodl is the SCENE and it is not optional: no file, no render, no exit.
	# `timeout` is not belt-and-braces either -- it is the only thing that keeps a
	# stuck window from wedging every later gate in the tree.
	WW_LODL_OBJECTS="$(winpath "$3")" \\
	WW_LODL_SHEETS="$(winpath "$SHEETS")" \\
	WW_LODL_REGION="$REGION" WW_LODI_LEVEL=0 WW_LODI_SLOT=0 \\
	WW_LODL_CHANNEL="$2" \\
	WW_RENDER_SHOT="$(winpath "$OUT/$1.png")" \\
	WW_RENDER_SIZE="$SIZE" WW_RENDER_CENTER="$CX,$CY,$CZ" \\
	WW_RENDER_ORTHO="$ORTHO" WW_RENDER_VIEW="$VIEW" WW_RENDER_FLAT=1 WW_RENDER_CLEAN=1 \\
	WW_WINDOW_AT=1960,40 \\
		timeout 600 "$EXE" --port "$PORT" "$(winpath "$LODL")" > "$OUT/$1.log" 2>&1
	[ -s "$OUT/$1.png" ] || echo "  (no picture for $1, exit $?)"
}""")

open(p, 'w', encoding='utf-8', newline='').write(s)
print('gate shot() fixed')
