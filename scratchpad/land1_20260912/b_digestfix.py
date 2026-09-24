"""Two reds this lane introduced, one root cause: the switch digest ate a
DESTINATION PATH and a flag that cannot make a tracked chunk stale.

  tests/spells/lodgen_roads.sh R1 -- two --no-roads runs are byte-identical.
      The harness gives each run its own directory, so the two commands differ
      only in `--vt <dir>`. `--vt` was taken off the skip list this session
      because it changes the PICTURE; that is right about the flag and wrong
      about its argument, which is a place to put files exactly like --out-dir.

  tests/spells/lodgen_native.sh 5 -- the stock bake is byte-identical with and
      without --native. `--native <dir>` and `--native-mesh-report <file>` were
      also taken off the list, so the ledger in a stock out-dir now records
      whether a run ALSO wrote a .lodo beside it.

So the list splits in two: tokens whose PRESENCE leaves the digest with their
value, and tokens whose VALUE leaves it while the token stays.

And the native pass gets the refusal it always needed. lodgenNativeActive()
collects a NativePlacement PER REF and lighting PER VERTEX inside the chunk
pass (lodgen.cpp:3784, 4069), so an incremental run that skips clean chunks
would write a .lodo/.lodi missing every skipped chunk's placements, silently.
That is the --atlas case word for word, and it was reachable until now.
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/src/nifcli.cpp'

OLD_DOC = """/*! Flags whose VALUE cannot reach an output byte. Three tests, and a flag is
 *  on this list only if it passes all three: it names WHERE files go, or HOW
 *  MANY threads carry them, or WHICH FILE an asset is read from -- and in that
 *  last case only because the ledger digests the asset's BYTES through the
 *  same lodgenReadAsset() the bake uses, so moving a mod folder still dirties
 *  every chunk whose assets changed under it.
 *
 *  --vt, --native and --vanilla-lod-root were on this list for one afternoon
 *  and are NOT any more: --vt makes the chunk sheets come from the pyramid
 *  instead of the stock per-chunk composite, which is a different picture from
 *  the same inputs, and the other two change what a run writes. A flag that
 *  changes the OUTPUT belongs in the digest even though it does not change the
 *  INPUTS, because the ledger's promise is about the files on disk.
 *
 *  --threads is on the list, and that is a claim: BAKEPERF1's pass retires
 *  every job on the calling thread IN JOB ORDER, so the worker count cannot
 *  reach a byte. If that ever stops being true this line is the bug. */
static const char * const gLgSwitchSkip[] = {
	"--out-dir", "--tex-dir", "--data-root", "--incremental",
	"--threads", "--chunk-threads", "--preview-dir",
	"--resource", "--plugins-txt",
	nullptr
};"""

NEW_DOC = """/*! Flags whose TOKEN AND VALUE are both dropped from the digest, because
 *  neither can make a TRACKED CHUNK OUTPUT stale. That is the whole test, and
 *  it is narrower than "cannot reach an output byte": the ledger tracks the
 *  per-chunk .BTO/.BTR/.DDS files it lists and nothing else, so a flag that
 *  writes a SEPARATE product into a SEPARATE directory is not its business.
 *
 *  A flag is here if it names WHERE files go, or HOW MANY threads carry them,
 *  or WHICH FILE an asset is read from -- and in that last case only because
 *  the ledger digests the asset's BYTES through the same lodgenReadAsset() the
 *  bake uses, so moving a mod folder still dirties every chunk whose assets
 *  changed under it.
 *
 *  --native and --native-mesh-report are here, and they were taken OFF for one
 *  afternoon on the reasoning that a flag changing what a run WRITES belongs in
 *  the digest. That reasoning cost tests/spells/lodgen_native.sh check 5 -- the
 *  stock bake is byte-identical with and without --native -- because the only
 *  file that then differed was the ledger recording the flag. The pair goes to
 *  its own --native directory and cannot touch a chunk; the run that WOULD be
 *  wrong (an incremental one) is refused outright a few lines below, which is a
 *  better answer than a switch digest that fires on the honest case too.
 *
 *  --threads is on the list, and that is a claim: BAKEPERF1's pass retires
 *  every job on the calling thread IN JOB ORDER, so the worker count cannot
 *  reach a byte. If that ever stops being true this line is the bug. */
static const char * const gLgSwitchSkip[] = {
	"--out-dir", "--tex-dir", "--data-root", "--incremental",
	"--threads", "--chunk-threads", "--preview-dir",
	"--resource", "--plugins-txt",
	"--native", "--native-mesh-report",
	nullptr
};

/*! Flags whose TOKEN stays in the digest and whose VALUE is dropped. The list
 *  exists because a flag can be both things at once: --vt makes the chunk
 *  sheets come from the virtual-texture pyramid instead of the stock per-chunk
 *  composite -- a different picture from the same inputs, so the flag must be
 *  digested -- while its argument is only a place to put the pyramid, exactly
 *  like --out-dir's.
 *
 *  Digesting that path made two identical commands write two different ledgers
 *  whenever they were pointed at different directories, which is what
 *  tests/spells/lodgen_roads.sh R1 does on purpose: it bakes --no-roads twice
 *  into roadOff/ and roadOff2/ and compares every byte. One of the two kinds of
 *  list would have been enough for either flag; neither was enough for both. */
static const char * const gLgSwitchSkipValue[] = {
	"--vt",
	nullptr
};"""

OLD_FN = """	for ( int i = 0; i < a.size(); i++ ) {
		bool skip = false;
		for ( const char * const * s = gLgSwitchSkip; *s; s++ ) {
			if ( a.at( i ) == QLatin1String( *s ) ) {
				skip = true;
				i++;                    /* and its value */
				break;
			}
		}
		if ( skip )
			continue;
		h.addData( a.at( i ).toUtf8() );"""

NEW_FN = """	for ( int i = 0; i < a.size(); i++ ) {
		bool skip = false;
		for ( const char * const * s = gLgSwitchSkip; *s; s++ ) {
			if ( a.at( i ) == QLatin1String( *s ) ) {
				skip = true;
				i++;                    /* and its value */
				break;
			}
		}
		if ( skip )
			continue;
		for ( const char * const * s = gLgSwitchSkipValue; *s; s++ ) {
			if ( a.at( i ) == QLatin1String( *s ) ) {
				h.addData( a.at( i ).toUtf8() );   /* the flag, never its path */
				h.addData( QByteArray( "\\x1f", 1 ) );
				skip = true;
				i++;
				break;
			}
		}
		if ( skip )
			continue;
		h.addData( a.at( i ).toUtf8() );"""

OLD_REF = """			if ( atlas || arrays || !impostors.isEmpty() ) {
				err() << "refused: --atlas, --arrays and --impostors each build ONE region-wide "
						 "product out of the whole written .BTO list, so a filtered chunk list "
						 "would build them from a FRACTION of the region and not say so."
					  << Qt::endl;
				err() << "  bake without --incremental, or drop those flags from this run and do "
						 "them in a separate full pass over the finished chunks. (The merge and "
						 "the far-ring simplify are NOT on this list: both rewrite one .BTO at a "
						 "time with nothing carried between files.)" << Qt::endl;
				return 1;
			}"""

NEW_REF = """			if ( atlas || arrays || !impostors.isEmpty() ) {
				err() << "refused: --atlas, --arrays and --impostors each build ONE region-wide "
						 "product out of the whole written .BTO list, so a filtered chunk list "
						 "would build them from a FRACTION of the region and not say so."
					  << Qt::endl;
				err() << "  bake without --incremental, or drop those flags from this run and do "
						 "them in a separate full pass over the finished chunks. (The merge and "
						 "the far-ring simplify are NOT on this list: both rewrite one .BTO at a "
						 "time with nothing carried between files.)" << Qt::endl;
				return 1;
			}
			/* --native IS the same case, and it is NOT visible in the output
			 * tree the way an atlas is. lodgenNativeActive() collects one
			 * NativePlacement per drawn reference and one lighting sample per
			 * vertex INSIDE the chunk pass (lodgen.cpp:3784 and :4069), so a
			 * filtered chunk list writes a .lodo/.lodi holding only the chunks
			 * that happened to be dirty -- a pair that loads, verifies its own
			 * hashes, and is missing most of the worldspace. It refuses here
			 * rather than in the switch digest so the message names the reason:
			 * the digest would fire on a stock bake that merely wrote a pair
			 * beside it, which cost lodgen_native.sh check 5. */
			if ( !nativeDir.isEmpty() ) {
				err() << "refused: --native builds ONE .lodo/.lodi pair for the whole region out of "
						 "the placements the chunk pass hands it, so an incremental run would write "
						 "a pair covering only the chunks it rebaked and say nothing about the rest."
					  << Qt::endl;
				err() << "  bake without --incremental, or drop --native from this run and do the "
						 "pair in a separate full pass." << Qt::endl;
				return 1;
			}"""


def main():
	b = open(P, 'rb').read()
	assert b.count(b'\x0d') == 0, 'nifcli.cpp is not LF-only'
	s = b.decode('utf-8')
	for old, new, label in ((OLD_DOC, NEW_DOC, 'skip list'),
							(OLD_FN, NEW_FN, 'digest loop'),
							(OLD_REF, NEW_REF, 'native refusal')):
		n = s.count(old)
		assert n == 1, '%s matched %d times' % (label, n)
		s = s.replace(old, new)
	out = s.encode('utf-8')
	assert out.count(b'\x0d') == 0, 'CR introduced'
	open(P, 'wb').write(out)
	print('nifcli.cpp %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\x0d')))
	return 0


if __name__ == '__main__':
	sys.exit(main())
