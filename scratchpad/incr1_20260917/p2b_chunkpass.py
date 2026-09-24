"""INCR1 step 2, patch C -- the chunk pass learns to put a SKIPPED chunk back.

Three options and nothing else: the full queue in its own order, a callback
that replays one chunk's cache, and a sink that is handed a baked chunk's
journal before the pass replays it. The pass walks a cursor over the full queue
so a cached chunk is replayed at EXACTLY the point its own job would have been
retired -- the accumulator is order-sensitive (the ORDERING LEAK comment in
nativeemit.h), so "in order" is the whole correctness argument.

Unset, every one of the three is false/empty and not a single call changes.

Anchors asserted count == 1. Escapes from chr(). LF preserved.
"""
import io
import os
import sys

NL = chr(10)
TAB = chr(9)
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '..', '..'))
CHECK = '--check' in sys.argv[1:]


def load(rel):
    with io.open(os.path.join(ROOT, rel), 'rb') as fh:
        b = fh.read()
    assert b.count(chr(13).encode()) == 0, rel + ' is not LF-only'
    return b.decode('utf-8')


def save(rel, text):
    if CHECK:
        print('  would write %s (%d bytes)' % (rel, len(text.encode('utf-8'))))
        return
    with io.open(os.path.join(ROOT, rel), 'wb') as fh:
        fh.write(text.encode('utf-8'))
    print('  wrote %s (%d bytes)' % (rel, len(text.encode('utf-8'))))


def sub1(text, old, new, what):
    n = text.count(old)
    if n != 1:
        raise SystemExit('ANCHOR %s: found %d times, wanted exactly 1' % (what, n))
    return text.replace(old, new, 1)


def block(lines):
    return NL.join(lines)


# ========================================================= lodgenchunkpass.h
rel = 'src/lodgenchunkpass.h'
t = load(rel)
if 'nativeReplayCached' in t:
    raise SystemExit(rel + ' already carries the hooks')

anchor = (TAB + 'qint64 texBudgetBytes = qint64( 512 ) << 20;   //!< divided among the workers' + NL
          + '};')
new = block([
    TAB + 'qint64 texBudgetBytes = qint64( 512 ) << 20;   //!< divided among the workers',
    '',
    TAB + '/*! THE INCREMENTAL NATIVE CACHE (lane INCR1, 2026-09-17).',
    TAB + ' *',
    TAB + ' *  `--incremental` hands this pass only the DIRTY chunks, and for the',
    TAB + ' *  stock target that is the whole story: a clean chunk\'s `.BTR`/`.BTO`',
    TAB + ' *  are already on disk and nothing has to be said about them. The FO4CS',
    TAB + ' *  target is not like that. `<ws>.lodo` and `<ws>.lodi` are AGGREGATED',
    TAB + ' *  from every chunk\'s arrivals, so a chunk that is skipped silently',
    TAB + ' *  deletes its placements from the pair. That is why `--native` was on',
    TAB + ' *  the whole-region refusal list, and why the ruled pipeline could not',
    TAB + ' *  be rebaked incrementally at all.',
    TAB + ' *',
    TAB + ' *  So a skipped chunk speaks from a cache instead of from a build.',
    TAB + ' *  `nativeAllJobs` is the FULL queue in the order a full bake would have',
    TAB + ' *  run it; `jobs` is the dirty subset of it, in the same order. The pass',
    TAB + ' *  walks a cursor over the full queue and calls `nativeReplayCached` for',
    TAB + ' *  every chunk it passes that is not the next dirty one -- so a cached',
    TAB + ' *  chunk speaks at EXACTLY the point its own job would have been retired.',
    TAB + ' *  That is the whole correctness argument: the accumulator hands out',
    TAB + ' *  arrival indices in arrival order (the ORDERING LEAK comment in',
    TAB + ' *  `nativeemit.h`), so out of order is a different `.lodo`.',
    TAB + ' *',
    TAB + ' *  `nativeJournalSink` is handed a BAKED chunk\'s journal immediately',
    TAB + ' *  before the pass replays it, which is the one moment the chunk\'s own',
    TAB + ' *  calls exist as data; the driver reduces it and writes the `.lodj`.',
    TAB + ' *  Setting it also forces journalling ON at one thread, where the',
    TAB + ' *  emitter would otherwise be spoken to directly and there would be',
    TAB + ' *  nothing to write.',
    TAB + ' *',
    TAB + ' *  All three unset is today\'s behaviour, call for call. */',
    TAB + 'QVector<LodgenChunkJob> nativeAllJobs;',
    TAB + 'std::function<void( const LodgenChunkJob & )> nativeReplayCached;',
    TAB + 'std::function<void( const LodgenChunkOutcome &, LodgenNativeJournal * )> nativeJournalSink;',
    '};',
])
t = sub1(t, anchor, new, 'h options tail')
save(rel, t)

# ======================================================= lodgenchunkpass.cpp
rel = 'src/lodgenchunkpass.cpp'
t = load(rel)
if 'replayCachedBefore' in t:
    raise SystemExit(rel + ' already carries the interleave')

# ---- 1. zero dirty chunks is not "nothing to do" for the native pair --------
t = sub1(t,
         TAB + 'if ( jobs.isEmpty() )' + NL
         + TAB + TAB + 'return true;',
         block([
             TAB + 'if ( jobs.isEmpty() ) {',
             TAB + TAB + '/* NOTHING DIRTY. For the stock target that really is nothing to do.',
             TAB + TAB + ' * For the FO4CS pair it is the opposite: every chunk of the region',
             TAB + TAB + ' * has to speak from its cache, or the rewritten `.lodo`/`.lodi` would',
             TAB + TAB + ' * be EMPTY -- the worst possible shape of this bug, because both',
             TAB + TAB + ' * files would still be written and still be valid. */',
             TAB + TAB + 'if ( opts.nativeReplayCached ) {',
             TAB + TAB + TAB + 'for ( const LodgenChunkJob & cj : opts.nativeAllJobs )',
             TAB + TAB + TAB + TAB + 'opts.nativeReplayCached( cj );',
             TAB + TAB + '}',
             TAB + TAB + 'return true;',
             TAB + '}',
         ]),
         'empty jobs early return')

# ---- 2. the cursor over the full queue --------------------------------------
t = sub1(t,
         TAB + 'bool ok = true;' + NL,
         block([
             TAB + 'bool ok = true;',
             '',
             TAB + '/* THE CURSOR OVER THE FULL QUEUE (lane INCR1). `jobs` is a subsequence',
             TAB + ' * of `opts.nativeAllJobs` in the same order, so one forward cursor puts',
             TAB + ' * every skipped chunk back in its own place; no search, no sorting, and',
             TAB + ' * a queue that is not a subsequence simply drains at the tail rather',
             TAB + ' * than replaying anything twice. */',
             TAB + 'int nativeCursor = 0;',
             TAB + 'const bool haveNativeCache = !opts.nativeAllJobs.isEmpty()',
             TAB + TAB + TAB + TAB + TAB + TAB + '&& bool( opts.nativeReplayCached );',
             TAB + 'auto sameChunk = []( const LodgenChunkJob & a, const LodgenChunkJob & b ) {',
             TAB + TAB + 'return a.dim == b.dim && a.cx == b.cx && a.cy == b.cy;',
             TAB + '};',
             TAB + 'auto replayCachedBefore = [&]( const LodgenChunkJob & job ) {',
             TAB + TAB + 'if ( !haveNativeCache )',
             TAB + TAB + TAB + 'return;',
             TAB + TAB + 'while ( nativeCursor < opts.nativeAllJobs.size()',
             TAB + TAB + TAB + TAB + '&& !sameChunk( opts.nativeAllJobs.at( nativeCursor ), job ) ) {',
             TAB + TAB + TAB + 'opts.nativeReplayCached( opts.nativeAllJobs.at( nativeCursor ) );',
             TAB + TAB + TAB + 'nativeCursor++;',
             TAB + TAB + '}',
             TAB + TAB + 'if ( nativeCursor < opts.nativeAllJobs.size() )',
             TAB + TAB + TAB + 'nativeCursor++;         // this chunk speaks for itself',
             TAB + '};',
             TAB + 'auto replayCachedTail = [&]() {',
             TAB + TAB + 'while ( haveNativeCache && nativeCursor < opts.nativeAllJobs.size() ) {',
             TAB + TAB + TAB + 'opts.nativeReplayCached( opts.nativeAllJobs.at( nativeCursor ) );',
             TAB + TAB + TAB + 'nativeCursor++;',
             TAB + TAB + '}',
             TAB + '};',
             '',
         ]),
         'ok flag')

# ---- 3. the one-thread loop -------------------------------------------------
t = sub1(t,
         TAB + TAB + 'Worker & w = *workers[0];' + NL
         + TAB + TAB + 'for ( int i = 0; i < jobs.size(); i++ ) {' + NL
         + TAB + TAB + TAB + 'if ( cancelled && cancelled() )' + NL
         + TAB + TAB + TAB + TAB + 'break;' + NL
         + TAB + TAB + TAB + 'runJob( jobs.at( i ), i, opts, w, nullptr, false, results[size_t( i )] );' + NL
         + TAB + TAB + TAB + 'retire( results[size_t( i )] );' + NL
         + TAB + TAB + '}',
         block([
             TAB + TAB + 'Worker & w = *workers[0];',
             TAB + TAB + '/* Journalling at ONE thread exists only for the cache: without a sink',
             TAB + TAB + ' * the serial loop speaks to the emitter directly, which is what it has',
             TAB + TAB + ' * always done and the reason `--threads 1` is the way back. With a',
             TAB + TAB + ' * sink the calls have to exist as data for one moment so they can be',
             TAB + TAB + ' * reduced and written, and they are replayed immediately after, on',
             TAB + TAB + ' * this same thread, in this same place. */',
             TAB + TAB + 'const bool journalNative = bool( opts.nativeJournalSink );',
             TAB + TAB + 'for ( int i = 0; i < jobs.size(); i++ ) {',
             TAB + TAB + TAB + 'if ( cancelled && cancelled() )',
             TAB + TAB + TAB + TAB + 'break;',
             TAB + TAB + TAB + 'replayCachedBefore( jobs.at( i ) );',
             TAB + TAB + TAB + 'runJob( jobs.at( i ), i, opts, w, nullptr, journalNative, results[size_t( i )] );',
             TAB + TAB + TAB + 'LodgenChunkOutcome & r1 = results[size_t( i )];',
             TAB + TAB + TAB + 'if ( r1.nativeJournal ) {',
             TAB + TAB + TAB + TAB + 'opts.nativeJournalSink( r1, r1.nativeJournal );',
             TAB + TAB + TAB + TAB + 'lodgenNativeJournalReplay( r1.nativeJournal );',
             TAB + TAB + TAB + TAB + 'lodgenNativeJournalDestroy( r1.nativeJournal );',
             TAB + TAB + TAB + TAB + 'r1.nativeJournal = nullptr;',
             TAB + TAB + TAB + '}',
             TAB + TAB + TAB + 'retire( r1 );',
             TAB + TAB + '}',
             TAB + TAB + 'replayCachedTail();',
         ]),
         'serial loop')

# ---- 4. the fan-out retire loop ---------------------------------------------
t = sub1(t,
         TAB + TAB + 'LodgenChunkOutcome & r = results[size_t( i )];' + NL
         + TAB + TAB + 'if ( r.nativeJournal ) {' + NL
         + TAB + TAB + TAB + 'lodgenNativeJournalReplay( r.nativeJournal );' + NL
         + TAB + TAB + TAB + 'lodgenNativeJournalDestroy( r.nativeJournal );' + NL
         + TAB + TAB + TAB + 'r.nativeJournal = nullptr;' + NL
         + TAB + TAB + '}' + NL
         + TAB + TAB + 'retire( r );' + NL
         + TAB + '}' + NL,
         block([
             TAB + TAB + 'LodgenChunkOutcome & r = results[size_t( i )];',
             TAB + TAB + 'replayCachedBefore( jobs.at( i ) );',
             TAB + TAB + 'if ( r.nativeJournal ) {',
             TAB + TAB + TAB + 'if ( opts.nativeJournalSink )',
             TAB + TAB + TAB + TAB + 'opts.nativeJournalSink( r, r.nativeJournal );',
             TAB + TAB + TAB + 'lodgenNativeJournalReplay( r.nativeJournal );',
             TAB + TAB + TAB + 'lodgenNativeJournalDestroy( r.nativeJournal );',
             TAB + TAB + TAB + 'r.nativeJournal = nullptr;',
             TAB + TAB + '}',
             TAB + TAB + 'retire( r );',
             TAB + '}',
             TAB + 'replayCachedTail();',
             '',
         ]),
         'fan-out retire loop')

save(rel, t)
print(NL + 'done')
