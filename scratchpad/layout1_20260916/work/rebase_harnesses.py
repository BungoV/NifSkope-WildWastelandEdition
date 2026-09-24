"""Lane LAYOUT1 (2026-09-16): re-base the harnesses that spell a path.

Written to disk rather than typed into a heredoc: a heredoc halves backslashes
(the trap the nifskope-ww-lodgen skill records) and several of these strings
are full of them.
"""
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'

HELPER = (
    '# THE PAIR MOVED (lane LAYOUT1, 2026-09-16, bungo 19:3x): a FO4CS bake\n'
    "# writes its .lodo/.lodi under <mod folder>/FO4CSLOD/<ws>/, and `--native`\n"
    '# names that mod folder. A RUNG exe from before the move writes them at the\n'
    '# --native directory itself, so this asks the tree where the pair is rather\n'
    '# than spelling a layout that depends on which exe baked it.\n'
    'pairdir () {\n'
    '\tif [ -d "$1/FO4CSLOD/Commonwealth" ]; then echo "$1/FO4CSLOD/Commonwealth"\n'
    '\telse echo "$1"; fi\n'
    '}\n'
)

EDITS = {
    # file: (anchor to insert the helper after, [(old, new), ...])
    'tests/spells/lodgen_ladder.sh': [],
    'tests/spells/lodgen_native.sh': [],
    'tests/spells/lodgen_btofree.sh': [],
    'tests/spells/lodgen_defaults.sh': [],
    'tests/spells/lodgen_stage_times.sh': [],
}


def patch(rel, subs, helper_after=None, need_helper=True):
    p = ROOT + rel
    s = open(p, encoding='utf-8', newline='').read()
    for a, b in subs:
        n = s.count(a)
        if n < 1:
            print('MISS in %s: %r' % (rel, a[:70]))
            sys.exit(1)
        s = s.replace(a, b)
    if need_helper and 'pairdir ()' not in s:
        if helper_after is None or helper_after not in s:
            print('no helper anchor in %s' % rel)
            sys.exit(1)
        s = s.replace(helper_after, helper_after + '\n' + HELPER, 1)
    open(p, 'w', encoding='utf-8', newline='').write(s)
    d = open(p, 'rb').read()
    print('ok %-38s CR %d LF %d' % (rel, d.count(b'\r'), d.count(b'\n')))


# ---- lodgen_ladder.sh: three trees, one of them baked by the rung exe --------
patch('tests/spells/lodgen_ladder.sh',
      [('"$WA/near/Native/Commonwealth.lod', '"$(pairdir "$WA/near/Native")/Commonwealth.lod'),
       ('"$WA/mnam/Native/Commonwealth.lod', '"$(pairdir "$WA/mnam/Native")/Commonwealth.lod'),
       ('"$WA/rung/Native/Commonwealth.lod', '"$(pairdir "$WA/rung/Native")/Commonwealth.lod')],
      helper_after='echo "== 0. the exe and the rung"')

# ---- lodgen_native.sh -------------------------------------------------------
patch('tests/spells/lodgen_native.sh',
      [('"$W/native/Native/Commonwealth.lod', '"$(pairdir "$W/native/Native")/Commonwealth.lod'),
       ('"$WA/native/Native/Commonwealth.lod', '"$(pairdir "$WA/native/Native")/Commonwealth.lod'),
       ('"$W/noladder/Native/Commonwealth.lod', '"$(pairdir "$W/noladder/Native")/Commonwealth.lod'),
       ('"$WA/occ/Native/Commonwealth.lod', '"$(pairdir "$WA/occ/Native")/Commonwealth.lod')],
      helper_after='checks=0')

# ---- lodgen_btofree.sh ------------------------------------------------------
patch('tests/spells/lodgen_btofree.sh',
      [('	for f in nat/Commonwealth.lodo nat/Commonwealth.lodi; do\n'
        '		if [ -f "$W/rung_native/$f" ] && cmp -s "$W/rung_native/$f" "$W/drop/$f"; then\n'
        '			note "(a) $(basename "$f") is byte-identical to the rung\'s ($(stat -c%s "$W/drop/$f") bytes)"\n'
        '		else bad "(a) $(basename "$f") is byte-identical to the rung\'s"; fi\n'
        '	done\n',
        '	# the pair moved under FO4CSLOD/<ws>/ (lane LAYOUT1, 2026-09-16) and the\n'
        '	# rung exe predates the move, so each side is asked where its own is.\n'
        '	for f in Commonwealth.lodo Commonwealth.lodi; do\n'
        '		a="$(pairdir "$W/rung_native/nat")/$f"; b="$(pairdir "$W/drop/nat")/$f"\n'
        '		if [ -f "$a" ] && cmp -s "$a" "$b"; then\n'
        '			note "(a) $f is byte-identical to the rung\'s ($(stat -c%s "$b") bytes)"\n'
        '		else bad "(a) $f is byte-identical to the rung\'s"; fi\n'
        '	done\n')],
      helper_after='skip () { echo "  SKIP $1"; }')

# ---- lodgen_defaults.sh -----------------------------------------------------
patch('tests/spells/lodgen_defaults.sh',
      [('LODO="$(find "$W/e_new/nat" -name "*.lodo" | head -1)"',
        'LODO="$(find "$W/e_new/nat" -name "*.lodo" | head -1)"   # find: either layout'),
       ],
      need_helper=False)

print('done')
