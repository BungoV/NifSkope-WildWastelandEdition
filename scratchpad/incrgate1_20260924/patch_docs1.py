import os
D = os.path.dirname(os.path.abspath(__file__))
R = r'E:\Projects\NifskopeWWE-incrgate1'


def rep(s, a, b):
    assert s.count(a) == 1, (a[:60], s.count(a))
    return s.replace(a, b)


def patch(rel, pairs):
    p = os.path.join(R, rel)
    src = open(p, 'rb').read()
    cr0 = src.count(b'\r')
    for a, b in pairs:
        src = rep(src, a, b)
    assert src.count(b'\r') == cr0
    open(p + '.tmp', 'wb').write(src)
    os.replace(p + '.tmp', p)
    print('patched', rel)


snip = open(os.path.join(D, 'snip_doc_ledger.txt'), 'rb').read()
patch('docs/LODGEN_LEDGER_FORMAT.md', [
    (b'edits it in place and keeps the fast path.\n\n---\n\n## 4. The refusals',
     b'edits it in place and keeps the fast path.\n\n' + snip + b'---\n\n## 4. The refusals'),
])
patch('docs/LODGEN_BAKE_RECORD.md', [
    (b'skip lists (`gLgSwitchSkip`, `gLgSwitchSkipValue`) described in\n`docs/LODGEN_LEDGER_FORMAT.md` \xc2\xa73.\n',
     b'skip lists (`gLgSwitchSkip`, `gLgSwitchSkipValue`) described in\n`docs/LODGEN_LEDGER_FORMAT.md` \xc2\xa73.\n'
     b'**Since lane INCRGATE1 (2026-09-24)** the `switches` value folds in the\n'
     b'**identity word** too, a hash of every effective setting, so a default that\n'
     b'moves inside the exe moves `switches` (ledger \xc2\xa73, "The identity word"). The\n'
     b'panel row "Rebake only what changed" writes `switch --panel` and the same\n'
     b'word.\n'),
])
