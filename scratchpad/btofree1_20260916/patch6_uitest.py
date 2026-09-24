# Lane BTOFREE1, 2026-09-16 -- patch 6: the panel self-test learns the new row.
#
#  * wwNewRows[] gains it, so the round-trip, tooltip, wheel-guard, scroll and
#    "does this row move the bake" passes all reach it without a word from me.
#    No `why` opt-out: ticking it ADDS nine .BTO files to the output tree, and
#    the gate's digest walks that tree file by file, so the honest answer is
#    that the bytes move.
#  * the target-visibility block gains three checks: the row is offered under
#    FO4CS and ships OFF, it is hidden under the stock engine (which never drops
#    a chunk), and the FO4CS summary SAYS the chunks are dropped -- a census
#    field is written and it moves, or it is decoration.
#
# The blocks below carry NO leading indentation; ind() puts it back at the depth
# the file uses. Counting tabs by eye is how the first run of this patch missed
# every anchor.
ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'


def ind(text, n):
    t = '\t' * n
    return '\n'.join((t + ln) if ln.strip() else ln for ln in text.split('\n'))


def patch(path, pairs):
    b = open(ROOT + path, 'rb').read()
    cr0, lf0, n0 = b.count(b'\r'), b.count(b'\n'), len(b)
    s = b.decode('utf-8')
    for old, new in pairs:
        n = s.count(old)
        assert n == 1, 'anchor count %d (want 1) in %s for: %r' % (n, path, old[:90])
        s = s.replace(old, new)
    nb = s.encode('utf-8')
    assert nb.count(b'\r') == cr0, 'CR count moved in %s: %d -> %d' % (path, cr0, nb.count(b'\r'))
    open(ROOT + path, 'wb').write(nb)
    print('%-28s %d -> %d bytes, CR %d -> %d, LF %d -> %d'
          % (path, n0, len(nb), cr0, nb.count(b'\r'), lf0, nb.count(b'\n')))


U = []

# ---- the table row, at the depth wwNewRows[] uses (7 tabs) ------------------
ROW_OLD = '''{ "LodgenMergeCheck", "merge", nullptr,
\t"hidden under the FO4 Community Shaders target this gate arms, and a hidden row reads as its default by design; the stock-engine target is where it reaches the bake", nullptr, nullptr, nullptr, nullptr, "1" },'''
ROW_NEW = '''{ "LodgenKeepBtoCheck", "keepBto", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "0" },
''' + ROW_OLD
U.append((ind(ROW_OLD, 7), ind(ROW_NEW, 7)))

# ---- the target-visibility checks, at 8 tabs --------------------------------
A1 = '''check( "and shows the native .lodo/.lodi row instead", !nativeChk->isHidden() );'''
B1 = A1 + '''
/* THE WAY BACK FROM THE DROP (lane BTOFREE1, 2026-09-16).
 * From today the FO4CS target builds its .BTO chunks in a
 * scratch folder and removes them, so the mod folder gets
 * our five types and nothing else. That is a default, and
 * a default needs a door: the row is offered here, it
 * ships OFF, and under the stock engine -- which never
 * drops a chunk -- it is not offered at all. */
auto * keepBtoChk = findChild<QCheckBox *>( QStringLiteral( "LodgenKeepBtoCheck" ) );
log << "  FO4CS: Keep legacy .BTO chunks hidden: "
\t<< ( !keepBtoChk || keepBtoChk->isHidden() ? "yes" : "no" )
\t<< ", ticked: "
\t<< ( keepBtoChk && keepBtoChk->isChecked() ? "yes" : "no" ) << "\\n";
check( "FO4 Community Shaders offers 'Keep legacy .BTO chunks' and ships it OFF",
\tkeepBtoChk && !keepBtoChk->isHidden() && !keepBtoChk->isChecked() );'''
U.append((ind(A1, 8), ind(B1, 8)))

A2 = '''check( "and never the legacy .btr chunks",
\t!sumCs.contains( QLatin1String( ".btr" ), Qt::CaseInsensitive ) );'''
B2 = A2 + '''
/* The sentence beside Generate has to SAY that a folder
 * will lose files at the end of the run. A surprise is
 * not a census. */
check( "and says the object chunks are built in a scratch folder and dropped",
\tsumCs.contains( QLatin1String( "scratch" ) )
\t&& sumCs.contains( QLatin1String( "dropped" ) ) );'''
U.append((ind(A2, 8), ind(B2, 8)))

A3 = '''check( "and hides the native row, which it cannot read", nativeChk->isHidden() );'''
B3 = A3 + '''
log << "  Stock: Keep legacy .BTO chunks hidden: "
\t<< ( !keepBtoChk || keepBtoChk->isHidden() ? "yes" : "no" ) << "\\n";
check( "the stock engine hides 'Keep legacy .BTO chunks', which it never needs",
\tkeepBtoChk && keepBtoChk->isHidden() );'''
U.append((ind(A3, 8), ind(B3, 8)))

patch('src/nifskope_ui.cpp', U)
print('patch6 ok')
