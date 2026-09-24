def edit(p, pairs):
    s = open(p, encoding='utf-8', newline='').read()
    for o, n in pairs:
        assert s.count(o) == 1, (p, s.count(o), o[:80])
        s = s.replace(o, n)
    open(p, 'w', encoding='utf-8', newline='').write(s)
    print('spliced', p)


edit('tests/spells/lodl_channels.sh', [
    ("""for c in identity identityraw sky ground seed sway selfao ao \\
	mask-r mask-g mask-b mask-a; do""",
     """for c in identity placement identityraw sky ground seed sway selfao ao \\
	mask-r mask-g mask-b mask-a; do"""),
])

edit('tests/spells/lodl_channels_check.py', [
    ("""PLACEMENT = {'identity': 'identity', 'identityraw': 'identitylow', 'sky': 'sky',
             'ground': 'ground', 'seed': 'seed'}""",
     """# `placement` (v7) reads the same per-placement identity `identity` read before
# v7 existed, so it is graded against the same column of the reader's table.
PLACEMENT = {'identity': 'identity', 'placement': 'identity',
             'identityraw': 'identitylow', 'sky': 'sky',
             'ground': 'ground', 'seed': 'seed'}"""),
    ("""ORDER = ['identity', 'identityraw', 'sky', 'ground', 'seed', 'sway', 'selfao',""",
     """ORDER = ['identity', 'placement', 'identityraw', 'sky', 'ground', 'seed', 'sway', 'selfao',"""),
    ("""    # (d) the way back: WW_LODL_AO=1 with no channel is byte-for-byte `ao`""",
     """    # (f) THE VERSION-6 FALLBACK (v7). This fixture is a version-6 pair: it has no
    # group table and no per-vertex sky stream. Both new channels must therefore
    # fall back, and -- the part that matters -- must SAY SO. A viewer that drew
    # the fallback silently would be indistinguishable from one that drew the
    # feature, which is the whole defect class root MISTAKES 05:1x records.
    idn = refusal(os.path.join(d, 'identity.log')) or ''
    if not idn:
        idn = ' '.join(notes(os.path.join(d, 'identity.log'), 'identity') or [])
    check('no group table' in idn,
          '(f) on a version-6 file `identity` NAMES its fallback to the placement '
          'identity -- %s' % (idn[:140] or 'NO NOTE LINE'))
    skn = ' '.join(notes(os.path.join(d, 'sky.log'), 'sky') or [])
    check('PLACEMENT BYTE' in skn or 'no per-vertex stream' in skn,
          '(f) on a version-6 file `sky` NAMES the placement byte as what served it '
          '-- %s' % (skn[:140] or 'NO NOTE LINE'))
    # and with nothing to fall back FROM, the two must be the same picture
    i = open(os.path.join(d, 'identity.png'), 'rb').read()
    pl = open(os.path.join(d, 'placement.png'), 'rb').read()
    check(i == pl, '(f) with no group table, `identity` and `placement` are the SAME '
                   'picture (%d B vs %d B) -- on a version-7 file they must differ, '
                   'which tests/spells/lodi_v7.sh G4 asserts' % (len(i), len(pl)))

    # (d) the way back: WW_LODL_AO=1 with no channel is byte-for-byte `ao`"""),
])
