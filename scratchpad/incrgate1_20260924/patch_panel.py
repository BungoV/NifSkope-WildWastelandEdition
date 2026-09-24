"""INCRGATE1 step 4: the panel row "Rebake only what changed" (src/lodgenmanager.cpp).

Every anchor asserted once; CR count unchanged (the file is LF-only).
Snippets with tabs/escapes live in snip_panel_*.txt beside this script."""
W = 'E:/Projects/NifskopeWWE-incrgate1'
L = W + '/scratchpad/incrgate1_20260924'
p = W + '/src/lodgenmanager.cpp'
with open(p, 'rb') as f:
    s = f.read().decode('utf-8')
cr0 = s.count('\r')
assert 'LodgenIncrementalCheck' not in s


def rd(n):
    with open(L + '/' + n, 'rb') as f:
        return f.read().decode('utf-8')


def one(a):
    n = s.count(a)
    assert n == 1, (a[:80], n)
    return s.index(a)


def ins_after(a, text):
    global s
    i = one(a) + len(a)
    s = s[:i] + text + s[i:]


def ins_before(a, text):
    global s
    i = one(a)
    s = s[:i] + text + s[i:]


def rep(a, b):
    global s
    one(a)
    s = s.replace(a, b)


# (A) include
rep('#include "lodgen.h"\n#include "lodgenchunkpass.h"\n',
    '#include "lodbfile.h"\n#include "lodgen.h"\n#include "lodgenchunkpass.h"\n')

# (B) the row, in Run, after Chunk threads
ins_after('\t\t\t\t\t"why one is the default.\\nCommand line: --chunk-threads" ) );\n', rd('snip_panel_row.txt'))

# (C) members
ins_after('\tQString meshDir, texDir, btoScratch, lastReport;\n', rd('snip_panel_members.txt'))

# (D) the pass options become one helper, read by the chunk pass AND the identity word
a0 = '\t\tLodgenChunkPassOptions pass;\n\t\tpass.plugins = pluginString();\n'
a1 = '\t\tpass.object = oopts;\n'
i = one(a0)
j = s.index(a1, i) + len(a1)
assert s.count(a1, i, j) == 1
block = s[i:j]
s = s[:i] + '\t\tLodgenChunkPassOptions pass = chunkPassOptions();\n' + s[j:]
helper = ('\t/*! The chunk pass\'s options, from the rows. One function, because the\n'
          '\t *  identity word of "Rebake only what changed" is read off the same\n'
          '\t *  options the pass bakes with (lane INCRGATE1, 2026-09-24). */\n'
          '\tLodgenChunkPassOptions chunkPassOptions()\n\t{\n' + block + '\t\treturn pass;\n\t}\n\n')
ins_before('\t/*! THE CHUNK QUEUE, over the machine.\n', helper)

# (E) startChunks: the diff, after the folders and before the scratch / the pair
ins_after('\t\tif ( texCheck->isChecked() && btrCheck->isChecked() )\n\t\t\tQDir().mkpath( texDir );\n',
          rd('snip_panel_begin.txt'))

# (F) runChunkQueue: an empty queue still runs when the caches must speak; arm; note
rep('\tvoid runChunkQueue()\n\t{\n\t\tif ( queue.isEmpty() )\n\t\t\treturn;\n',
    '\tvoid runChunkQueue()\n\t{\n'
    '\t\t// an incremental run with nothing dirty still runs: every chunk replays its cache\n'
    '\t\tif ( queue.isEmpty() && !( incOn && incRun.incremental ) )\n\t\t\treturn;\n')
rep('\t\tLodgenChunkPassOptions pass = chunkPassOptions();\n',
    '\t\tLodgenChunkPassOptions pass = chunkPassOptions();\n'
    '\t\tif ( incOn )\n\t\t\tlodgenIncrementalArmCache( incRun, world->worldspaceEdid(), pass );\n')
rep('\t\t\t[this]( const LodgenChunkOutcome & r ) {\n\t\t\t\tprogress->setFormat( tr( "chunk %1 at',
    '\t\t\t[this, &pass]( const LodgenChunkOutcome & r ) {\n'
    '\t\t\t\tif ( incOn )\n\t\t\t\t\tlodgenIncrementalNoteRetired( incRun, pass, r );\n'
    '\t\t\t\tprogress->setFormat( tr( "chunk %1 at')

# (G) step(): the caches must cover the pair before it is written
rep('\t\t\tif ( lodgenNativeActive() ) {\n\t\t\t\tif ( !cancelFlag ) {\n',
    rd('snip_panel_cache.txt') +
    '\t\t\tif ( lodgenNativeActive() ) {\n\t\t\t\tif ( !cancelFlag && !incPairRefused ) {\n')

# (H) the record, and the notes in the result's tooltip
rep('\t\t\tworld.reset();\n\t\t\tlodgenDestroyBakeCaches( bakeCaches );\n\t\t\tbakeCaches = nullptr;\n\t\t\twrittenBto.clear();\n'
    '\t\t\tfinishAll( cancelFlag ? tr( "cancelled after %1 chunk(s)" ).arg( done )\n'
    '\t\t\t\t: tr( "done \\u2014 %1 chunk(s)%2" ).arg( done ).arg( tail ) );\n',
    rd('snip_panel_record.txt') +
    '\t\t\tworld.reset();\n\t\t\tlodgenDestroyBakeCaches( bakeCaches );\n\t\t\tbakeCaches = nullptr;\n\t\t\twrittenBto.clear();\n'
    '\t\t\t/* the notes (refusals, warnings, the record\'s read-back) go under the\n'
    '\t\t\t * first line, so they reach the result\'s tooltip and not the bar */\n'
    '\t\t\tconst QString incTail = incNotes.isEmpty() ? QString()\n'
    '\t\t\t\t: QStringLiteral( "\\n" ) + incNotes.join( QChar( \'\\n\' ) );\n'
    '\t\t\tincNotes.clear();\n'
    '\t\t\tfinishAll( ( cancelFlag ? tr( "cancelled after %1 chunk(s)" ).arg( done )\n'
    '\t\t\t\t: tr( "done \\u2014 %1 chunk(s)%2" ).arg( done ).arg( tail ) ) + incTail );\n')

assert s.count('\r') == cr0
with open(p, 'wb') as f:
    f.write(s.encode('utf-8'))
print('panel patched', s.count('\n'), 'lines')
