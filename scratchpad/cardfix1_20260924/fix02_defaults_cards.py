# CARDFIX1: lodgen_defaults.sh leg (d) read "no C lines with identity off" in every WORKTREE, on every exe.
# Cause: the default CARDS dir is repo-relative, and git carries only the 24 .txt sidecars of that card set
# (the PNGs are untracked, main tree only). lodgen --impostors then finds no card image and places no C line.
# Measured 2026-09-24: rung exe, CARDS = the main tree's dir -> d_new manifest C = 7210 (worktree dir -> 0).
# Fix: the leg refuses BY NAME when the card dir holds no image, instead of reporting a generator defect.
ROOT = 'E:/Projects/NifskopeWWE-cardfix1/'


def patch(path, edits):
    b = open(ROOT + path, 'rb').read()
    cr = b.count(b'\r')
    s = b.decode('utf-8')
    for old, new in edits:
        c = s.count(old)
        assert c == 1, (path, old[:70], c)
        s = s.replace(old, new)
    out = s.encode('utf-8')
    assert out.count(b'\r') == cr, path
    open(ROOT + path, 'wb').write(out)
    print('patched', path)


OLD = ('''	if [ ! -d "$CARDS" ]; then
		echo "  SKIP: no impostor card directory at $CARDS"
	else
''')
NEW = ('''	nimg="$(find "$CARDS" -maxdepth 1 \\( -iname "*.png" -o -iname "*.dds" \\) 2>/dev/null | wc -l)"
	if [ ! -d "$CARDS" ]; then
		echo "  SKIP: no impostor card directory at $CARDS"
	elif [ "$nimg" -eq 0 ]; then
		# git carries only the .txt sidecars of this card set, so a fresh WORKTREE has the
		# directory and no image: the bake then places no card and every C count reads 0,
		# which looks like a generator defect and is not (CARDFIX1, 2026-09-24). Point
		# CARDS= at a directory holding the card images (the main tree's copy).
		bad "(d) the card directory holds no card image ($CARDS): set CARDS= to a baked card set"
	else
''')
patch('tests/spells/lodgen_defaults.sh', [(OLD, NEW)])
