"""AUDIT1 step 6: the CONFIRMED bugs of section 4, each as the smallest change
that makes the stated failure impossible.  No default moves, no format moves, no
design moves.  Every anchor is validated against the file on disk BEFORE any
file is opened for writing, and every file is written through a temp file and
os.replace.

  F1  src/lodifile.cpp   the payload bounds test wraps          (C1)
  F2  src/lodofile.cpp   the same test, same wrap               (C1)
  F3  src/lodifile.cpp   the v4-only aggregate HEADER rules     (C2)
  F4  src/lodifile.cpp   the v4-only aggregate PAYLOAD rules    (C2)
  F5  src/nifcli.cpp     "off stands" when the default stands   (C4)
  F6  src/nifcli.cpp     a valued switch spelled without a value
"""
import io
import os
import sys
import tempfile

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'
EDITS = []


def edit(path, old, new, note):
    EDITS.append((path, old, new, note))


# --------------------------------------------------------------- F1 + F2 (C1)
edit('src/lodifile.cpp',
     '''		if ( t.off + t.bytes > h.fileBytes )
			return refuse( QString( "%1 runs past the file (%2 + %3 > %4)" ).arg( t.name ).arg( t.off ).arg( t.bytes ).arg( h.fileBytes ) );''',
     '''		/* NOT `t.off + t.bytes > h.fileBytes` (lane AUDIT1, 2026-09-17): that
		 * sum is two quint64 and it WRAPS, so a 4,096-aligned offset near 2^64
		 * passed the only bounds test this reader has, and the pad walk below
		 * then indexed p[i] from prevEnd all the way to it. Asked the other way
		 * round -- the size first, so the subtraction cannot go negative -- the
		 * same question cannot overflow. */
		if ( t.bytes > h.fileBytes || t.off > h.fileBytes - t.bytes )
			return refuse( QString( "%1 runs past the file (%2 + %3 > %4)" ).arg( t.name ).arg( t.off ).arg( t.bytes ).arg( h.fileBytes ) );''',
     'F1 lodi payload bounds cannot wrap')

edit('src/lodofile.cpp',
     '''		if ( t.off + t.bytes > h.fileBytes )
			return refuse( QString( "%1 runs past the file (%2 + %3 > %4)" ).arg( t.name ).arg( t.off ).arg( t.bytes ).arg( h.fileBytes ) );''',
     '''		/* NOT `t.off + t.bytes > h.fileBytes` (lane AUDIT1, 2026-09-17): the
		 * sum of two quint64 WRAPS, and a 4,096-aligned offset near 2^64 then
		 * passes the only bounds test this reader has. The size is tested
		 * first, so the subtraction below cannot go negative. */
		if ( t.bytes > h.fileBytes || t.off > h.fileBytes - t.bytes )
			return refuse( QString( "%1 runs past the file (%2 + %3 > %4)" ).arg( t.name ).arg( t.off ).arg( t.bytes ).arg( h.fileBytes ) );''',
     'F2 lodo payload bounds cannot wrap')

# ------------------------------------------------------------------- F3 (C2)
edit('src/lodifile.cpp',
     '''	if ( h.version == LODI_VERSION_AGGREGATE ) {
		h.offAggregates = getLE<quint64>( p + H_OFF_AGG );''',
     '''	/* v4, AND A v5 FILE THAT CARRIES AGGREGATES (lane AUDIT1, 2026-09-17).
	 * These three rules sat behind the version word alone, so the day the bake
	 * started writing version 5 -- placement AO is on by default -- an
	 * aggregate in a v5 file stopped being asked whether it has two azimuths,
	 * a positive switch distance and a band above 1. The tabs builder below
	 * already asks the question the right way round; this is the same one.
	 * The `aggregateCount == 0` refusal inside stays a v4 rule, and still is
	 * one: a v5 file with no aggregate does not come in here at all. */
	if ( h.version == LODI_VERSION_AGGREGATE || ( v5 && h.aggregateCount ) ) {
		h.offAggregates = getLE<quint64>( p + H_OFF_AGG );''',
     'F3 the aggregate header rules reach v5')

# ------------------------------------------------------------------- F4 (C2)
edit('src/lodifile.cpp',
     '''		if ( h.version == LODI_VERSION_AGGREGATE ) {
			std::vector<quint8> claimed( h.instanceCount, 0 );''',
     '''		/* THE AGGREGATE PAYLOAD IS GATED BY THE AGGREGATE, NOT BY THE VERSION
		 * WORD (lane AUDIT1, 2026-09-17). `h.aggregateCount != 0` is exactly
		 * the question the tabs builder asks as `iAgg >= 0`: v3 never reads the
		 * word (0), v4 refuses a zero count by name, v5 carries the count.
		 * Behind `version == 4` every rule below -- the identity bit, the cell
		 * order, the coveredFirst partition, the index range and the double
		 * cover -- went silent on every default bake the moment placement AO
		 * made the files version 5. */
		if ( h.aggregateCount ) {
			std::vector<quint8> claimed( h.instanceCount, 0 );''',
     'F4 the aggregate payload rules reach v5')

# ------------------------------------------------------------------- F5 (C4)
edit('src/nifcli.cpp',
     '''				fprintf( stderr, "lodgen: --land-guide %s is not one of off|drag|aspect|aspecthex|slopewarp|flatwarp; off stands\\n",''',
     '''				/* "the default stands", not "off stands" (lane AUDIT1,
				 * 2026-09-17): this branch calls NOTHING, so what stands is
				 * whatever the defaults ruling put there -- flatwarp:1.0 since
				 * lane DEFAULTS1 -- and the sheets are flatwarp sheets. The
				 * sibling message above says it this way for the same reason. */
				fprintf( stderr, "lodgen: --land-guide %s is not one of off|drag|aspect|aspecthex|slopewarp|flatwarp; the default stands\\n",''',
     'F5 the land-guide warning names what stands')

# -------------------------------------------------------------------- F6
edit('src/nifcli.cpp',
     '''	lodbClearCensus();
	for ( int i = 0; i < a.size(); i++ ) {
		const QString & t = a.at( i );
		auto next = [&]() -> QString { return ( i + 1 < a.size() ) ? a.at( ++i ) : QString(); };''',
     '''	lodbClearCensus();
	/* A VALUED SWITCH SPELLED WITHOUT ITS VALUE (lane AUDIT1, 2026-09-17).
	 * `next()` used to hand back an empty QString at the end of the vector, and
	 * an empty value cannot be told from a switch that was never given:
	 * `--incremental` last on the line parsed, set an empty directory, skipped
	 * the whole incremental block with every refusal in it, and FULL-baked at
	 * exit 0 while the operator was watching the clock for a cached run. Which
	 * switch it was is remembered here and refused after the loop, so the
	 * refusal comes before any work and names the switch. */
	QString missingValueFor;
	for ( int i = 0; i < a.size(); i++ ) {
		const QString & t = a.at( i );
		auto next = [&]() -> QString {
			if ( i + 1 < a.size() )
				return a.at( ++i );
			missingValueFor = t;
			return QString();
		};''',
     'F6a next() remembers a missing value')

edit('src/nifcli.cpp',
     '''	/* `--roads-legacy` IS THE WAY BACK, and the way back is ROADS1''' + chr(39) + '''s whole''',
     '''	if ( !missingValueFor.isEmpty() ) {
		err() << "error: " << missingValueFor << " needs a value" << Qt::endl;
		err().flush();
		return 2;
	}

	/* `--roads-legacy` IS THE WAY BACK, and the way back is ROADS1''' + chr(39) + '''s whole''',
     'F6b and the run refuses before it starts')


def main():
    src = {}
    for path, old, _new, note in EDITS:
        if path not in src:
            src[path] = io.open(ROOT + path, encoding='utf-8', newline='').read()
        n = src[path].count(old)
        if n != 1:
            print('ABORT: %s anchor found %d times -- %s' % (path, n, note))
            return 1
    out = {}
    for path, old, new, note in EDITS:
        s = out.get(path, src[path])
        assert s.count(old) == 1
        out[path] = s.replace(old, new, 1)
        print('  ok  %-18s %s' % (path.split('/')[-1], note))
    for path, s in out.items():
        before = src[path]
        assert s.count('\r') == before.count('\r') == 0
        d = os.path.dirname(ROOT + path)
        f = tempfile.NamedTemporaryFile('w', encoding='utf-8', newline='', dir=d,
                                        delete=False, suffix='.tmp')
        f.write(s)
        f.close()
        os.replace(f.name, ROOT + path)
        print('%s: %d -> %d bytes, CR %d, LF %d'
              % (path, len(before), len(s), s.count('\r'), s.count('\n')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
