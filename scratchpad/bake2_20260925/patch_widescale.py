"""BAKE2 patch: .lodi v10, the wide-scale instance bit (director's ruling 2026-09-25, option (a)).
Every anchor must match exactly once; LF-only files stay LF-only."""
import sys
R = 'E:/Projects/NifskopeWWE-bake2/'
edits = {}

def ed(path, old, new):
    edits.setdefault(path, []).append((old, new))

# ---------------------------------------------------------------- lodifile.h
ed('src/lodifile.h', '''constexpr quint32 LODI_VERSION_SCRAPPABLE = 9;
''', '''constexpr quint32 LODI_VERSION_SCRAPPABLE = 9;
/*! VERSION 10 IS THE v9 LAYOUT PLUS BIT 7 OF THE INSTANCE FLAGS, `SCALE_WIDE`
 *  (lane BAKE2, 2026-09-25; the director's ruling (a) the same day).
 *
 *  THE CEILING IT LIFTS. The instance record stores its scale as u16 / 8192, so
 *  nothing above 65535/8192 = 7.99988 fits, and the writer refused the whole
 *  file on such a ref. The engine and the CK allow a reference scale up to 10.0,
 *  and Nuka-World places four LOD-carrying cliffs above the old line
 *  (0604D45A 9.97, 0604D45D 8.33, 0604DDA1 9.23, 0604DDB9 8.33): its .lodi could
 *  not be written at all.
 *
 *  THE RULE. Bit 7 set: scale = 8 + v / 8192, range 8 .. 15.99988. Bit 7 clear:
 *  scale = v / 8192, exactly as before. The step is 1/8192 on both sides, so no
 *  instance anywhere reads coarser than it did, and 16 is 60 percent headroom
 *  over the engine's 10. A scale above 15.99988 is still REFUSED, not clamped.
 *
 *  WHAT DOES NOT MOVE. A placement at or below 7.99988 is written with bit 7
 *  clear and the very same u16, and the version rises to 10 ONLY when some
 *  instance carries the bit. A file whose scales all fit is therefore byte for
 *  byte the file this writer wrote before (v7, or v9 with `--scrappable`); the
 *  installed Commonwealth .lodi stays a v7 file every reader keeps accepting.
 *
 *  Like v9, the number moves because the version word is the only thing that
 *  tells a reader which flag bits may appear: below v10, bit 7 is reserved zero.
 *  10 implies v7's 512-byte header block, so a `--lodi-v6` bake that meets a
 *  wide scale is refused (no pre-v7 version can say it). The FO4CS reader owes
 *  the same decode. */
constexpr quint32 LODI_VERSION_WIDE_SCALE = 10;
''')
ed('src/lodifile.h', '''constexpr float LODI_SCALE_MAX = 65535.0f / 8192.0f;  //!< 7.99988, the refusal line
''', '''constexpr float LODI_SCALE_MAX = 65535.0f / 8192.0f;  //!< 7.99988, the narrow range's top (v10: bit 7 above it)
constexpr float LODI_SCALE_WIDE_BASE = 8.0f;          //!< v10: bit 7 set adds this to v / 8192
constexpr float LODI_SCALE_MAX_WIDE = LODI_SCALE_WIDE_BASE + 65535.0f / 8192.0f;  //!< 15.99988, the refusal line
''')
ed('src/lodifile.h', '''	LODI_INST_SCRAPPABLE = 64
};
constexpr quint16 LODI_INST_FLAGS_KNOWN = 0x7F;
''', '''	LODI_INST_SCRAPPABLE = 64,
	//! v10: scale = 8 + v / 8192 (LODI_VERSION_WIDE_SCALE); set by the writer, never by a caller
	LODI_INST_SCALE_WIDE = 128
};
constexpr quint16 LODI_INST_FLAGS_KNOWN = 0xFF;

/*! v10, the one encoder and the one decoder of the instance scale. At or below
 *  LODI_SCALE_MAX the word is `lround( s x 8192 )` clamped to u16 -- the exact
 *  arithmetic of every version before 10 -- and the bit is clear. */
inline bool lodiScaleIsWide( float s )
{
	return s > LODI_SCALE_MAX;
}
inline quint16 lodiScaleWord( float s )
{
	const float b = lodiScaleIsWide( s ) ? s - LODI_SCALE_WIDE_BASE : s;
	const long v = std::lround( b * LODI_SCALE_DIVISOR );
	return quint16( v < 0 ? 0 : ( v > 65535 ? 65535 : v ) );
}
inline float lodiScaleValue( quint16 word, quint16 flags )
{
	const float s = float( word ) / LODI_SCALE_DIVISOR;
	return ( flags & LODI_INST_SCALE_WIDE ) ? s + LODI_SCALE_WIDE_BASE : s;
}
//! the quantised scale a consumer reads back, for the writer's own bounds
inline float lodiScaleQuantised( float s )
{
	return lodiScaleValue( lodiScaleWord( s ), lodiScaleIsWide( s ) ? quint16( LODI_INST_SCALE_WIDE ) : quint16( 0 ) );
}
''')
ed('src/lodifile.h', '''	quint16 scale;      //!< scale = v / 8192
''', '''	quint16 scale;      //!< scale = v / 8192; v10 with flags bit 7: 8 + v / 8192 (lodiScaleValue)
''')
ed('src/lodifile.h', '''	quint32 version = LODI_VERSION;     //!< the version word actually written
};
''', '''	quint32 version = LODI_VERSION;     //!< the version word actually written
	quint32 wideScaleInstances = 0;     //!< v10: instances written with bit 7 (scale above 7.99988)
};
''')
ed('src/lodifile.h', '''#include <vector>
''', '''#include <cmath>
#include <vector>
''')

# ---------------------------------------------------------------- lodifile.cpp
ed('src/lodifile.cpp', '''		if ( r.scale > LODI_SCALE_MAX || r.scale < 0.0f )
			return fail( QString( "ref 0x%1 part %2 (base %3): scale %4 is outside 0 .. %5 (the u16/8192 ceiling); refused, not clamped" )
				.arg( r.refFormId, 8, 16, QChar( '0' ) ).arg( r.scolPart ).arg( r.baseName ).arg( double( r.scale ) ).arg( double( LODI_SCALE_MAX ), 0, 'f', 5 ) );
''', '''		if ( !( r.scale <= LODI_SCALE_MAX_WIDE ) || r.scale < 0.0f )
			return fail( QString( "ref 0x%1 part %2 (base %3): scale %4 is outside 0 .. %5 (8 + the u16/8192 ceiling, v10); refused, not clamped" )
				.arg( r.refFormId, 8, 16, QChar( '0' ) ).arg( r.scolPart ).arg( r.baseName ).arg( double( r.scale ) ).arg( double( LODI_SCALE_MAX_WIDE ), 0, 'f', 5 ) );
		if ( r.flags & LODI_INST_SCALE_WIDE )
			return fail( QString( "ref 0x%1: the caller set instance flag bit 7 (SCALE_WIDE); the writer alone decides it from the scale" )
				.arg( r.refFormId, 8, 16, QChar( '0' ) ) );
''')
ed('src/lodifile.cpp', '''			// the quantised scale, because that is the one the consumer reads
			const float qs = float( std::clamp( int( std::lround( r.scale * LODI_SCALE_DIVISOR ) ), 0, 65535 ) ) / LODI_SCALE_DIVISOR;
''', '''			// the quantised scale, because that is the one the consumer reads
			const float qs = lodiScaleQuantised( r.scale );
''')
ed('src/lodifile.cpp', '''			q.scale = quint16( std::clamp( int( std::lround( r.scale * LODI_SCALE_DIVISOR ) ), 0, 65535 ) );
			q.baseId = quint16( r.baseId );
			q.ao = r.ao; q.sky = r.sky; q.ground = r.ground; q.seed = r.seed;
			q.flags = r.flags;
''', '''			q.scale = lodiScaleWord( r.scale );
			q.baseId = quint16( r.baseId );
			q.ao = r.ao; q.sky = r.sky; q.ground = r.ground; q.seed = r.seed;
			q.flags = quint16( r.flags | ( lodiScaleIsWide( r.scale ) ? LODI_INST_SCALE_WIDE : 0 ) );
''')
ed('src/lodifile.cpp', '''			const float qs = float( std::clamp( int( std::lround( r.scale * LODI_SCALE_DIVISOR ) ), 0, 65535 ) ) / LODI_SCALE_DIVISOR;
			const double vol''', '''			const float qs = lodiScaleQuantised( r.scale );
			const double vol''')
ed('src/lodifile.cpp', '''				const float qs = float( std::clamp( int( std::lround( r.scale * LODI_SCALE_DIVISOR ) ), 0, 65535 ) ) / LODI_SCALE_DIVISOR;
				LodiOccluder b;''', '''				const float qs = lodiScaleQuantised( r.scale );
				LodiOccluder b;''')
ed('src/lodifile.cpp', '''	const quint32 headerBytes = lodiHeaderBytes( h.version );
	QByteArray file;
''', '''	/* v10, THE WIDE-SCALE BIT (lane BAKE2, 2026-09-25): like v9, no table and no
	 * header word, only bit 7 of the instance flags and the version word. It
	 * rises ONLY when an instance carries the bit, so a file whose scales all
	 * fit stays the v7/v9 file it was, byte for byte. v10 is the v9 layout (bit
	 * 6 keeps its meaning), so it needs v7's header block: a pre-v7 bake with a
	 * wide scale is REFUSED -- dropping the bit would misplace the object by 8x
	 * its size, and no pre-v7 version can say it. */
	quint32 wideScaleWritten = 0;
	for ( const LodiInstance & r : inst )
		if ( r.flags & LODI_INST_SCALE_WIDE )
			wideScaleWritten++;
	if ( wideScaleWritten ) {
		if ( h.version < LODI_VERSION_GROUP_SKY )
			return fail( QString( "%1 instance(s) carry a scale above %2, which needs version %3 (the v7 header "
				"block); this bake asked for a version-%4 file (--lodi-v6?). Refused, not clamped" )
				.arg( wideScaleWritten ).arg( double( LODI_SCALE_MAX ), 0, 'f', 5 )
				.arg( LODI_VERSION_WIDE_SCALE ).arg( h.version ) );
		h.version = LODI_VERSION_WIDE_SCALE;
	}
	const quint32 headerBytes = lodiHeaderBytes( h.version );
	QByteArray file;
''')
ed('src/lodifile.cpp', '''		stats->maxScale = maxScale;
''', '''		stats->maxScale = maxScale;
		stats->wideScaleInstances = wideScaleWritten;
''')
ed('src/lodifile.cpp', '''		&& h.version != LODI_VERSION_SCRAPPABLE )
		return refuse( QString( "version %1; this reader knows %2, %3, %4, %5, %6, %7 and %8" )
			.arg( h.version ).arg( LODI_VERSION ).arg( LODI_VERSION_AGGREGATE )
			.arg( LODI_VERSION_PLACEMENT_AO ).arg( LODI_VERSION_VERTEX_AO )
			.arg( LODI_VERSION_GROUP_SKY ).arg( LODI_VERSION_HORIZON )
			.arg( LODI_VERSION_SCRAPPABLE ) );
''', '''		&& h.version != LODI_VERSION_SCRAPPABLE && h.version != LODI_VERSION_WIDE_SCALE )
		return refuse( QString( "version %1; this reader knows %2, %3, %4, %5, %6, %7, %8 and %9" )
			.arg( h.version ).arg( LODI_VERSION ).arg( LODI_VERSION_AGGREGATE )
			.arg( LODI_VERSION_PLACEMENT_AO ).arg( LODI_VERSION_VERTEX_AO )
			.arg( LODI_VERSION_GROUP_SKY ).arg( LODI_VERSION_HORIZON )
			.arg( LODI_VERSION_SCRAPPABLE ).arg( LODI_VERSION_WIDE_SCALE ) );
''')
ed('src/lodifile.cpp', '''		|| ( h.version == LODI_VERSION_SCRAPPABLE );
''', '''		|| ( h.version == LODI_VERSION_SCRAPPABLE ) || ( h.version == LODI_VERSION_WIDE_SCALE );
''')
ed('src/lodifile.cpp', '''						.arg( LODI_VERSION_SCRAPPABLE ) );
				/* bungo 2026-09-11 08:0x item 3''', '''						.arg( LODI_VERSION_SCRAPPABLE ) );
				// v10: bit 7 is reserved zero below version 10, for the same reason
				if ( ( r.flags & LODI_INST_SCALE_WIDE ) && h.version < LODI_VERSION_WIDE_SCALE )
					return refuse( QString( "instance %1 carries the wide-scale bit (0x%2) in a version-%3 file, "
						"where that bit is reserved zero; version %4 is the one that means it" )
						.arg( i ).arg( int( LODI_INST_SCALE_WIDE ), 0, 16 ).arg( h.version )
						.arg( LODI_VERSION_WIDE_SCALE ) );
				/* bungo 2026-09-11 08:0x item 3''')
ed('src/lodifile.cpp', '''				if ( r.scale == 0 )
					return refuse(''', '''				if ( r.scale == 0 && !( r.flags & LODI_INST_SCALE_WIDE ) )
					return refuse(''')

# ---------------------------------------------------------------- lodinative.cpp
ed('src/lodinative.cpp', '''			const float scale = float( inst.scale ) / LODI_SCALE_DIVISOR;
''', '''			const float scale = lodiScaleValue( inst.scale, inst.flags );
''')

# ---------------------------------------------------------------- nativeemit.cpp (the census line)
ed('src/nativeemit.cpp', '''		/* v9 (lane HORIZON3, 2026-09-19), on its own prefix. THE GATE NUMBER IS
''', '''		/* v10 (lane BAKE2, 2026-09-25): placements above the old 7.99988 line,
		 * written with instance flag bit 7. 0 = the file is the v7/v9 file it
		 * always was; any other number = version 10. */
		ladderLine += QString( "\\n  native-wide-scale: %1 of %2 placements above %3 (flag bit 7); max scale %4; "
			".lodi version %5" )
			.arg( stats.wideScaleInstances ).arg( stats.instances ).arg( double( LODI_SCALE_MAX ), 0, 'f', 5 )
			.arg( double( stats.maxScale ), 0, 'f', 4 ).arg( stats.version );
		/* v9 (lane HORIZON3, 2026-09-19), on its own prefix. THE GATE NUMBER IS
''')

# ---------------------------------------------------------------- the independent decoder
D = 'tests/spells/lodgen_native_decode.py'
ed(D, '''    if h['version'] not in (3, 4, 5, 6, 7, 8, 9):
        raise Refusal('version %d; this reader knows 3, 4, 5, 6, 7, 8 and 9' % h['version'])
''', '''    if h['version'] not in (3, 4, 5, 6, 7, 8, 9, 10):
        raise Refusal('version %d; this reader knows 3, 4, 5, 6, 7, 8, 9 and 10' % h['version'])
''')
ed(D, '''    v9 = h['version'] == 9
''', '''    # v10 = v9 + instance flag bit 7, SCALE_WIDE: scale = 8 + u16 / 8192 (lane BAKE2, 2026-09-25).
    v10 = h['version'] == 10
    v9 = h['version'] == 9 or v10
''')
ed(D, '''            if r['flags'] & ~(0x7F if v9 else 0x3F):
                raise Refusal('instance %d reserved flags 0x%04x (version %d knows 0x%02x)'
                              % (i, r['flags'], h['version'], 0x7F if v9 else 0x3F))
            if r['scale'] == 0:
''', '''            known = 0xFF if v10 else (0x7F if v9 else 0x3F)
            if r['flags'] & ~known:
                raise Refusal('instance %d reserved flags 0x%04x (version %d knows 0x%02x)'
                              % (i, r['flags'], h['version'], known))
            if r['scale'] == 0 and not r['flags'] & 0x80:
''')
ed(D, '''            r['scaleF'] = r['scale'] / 8192.0
''', '''            r['scaleF'] = r['scale'] / 8192.0 + (8.0 if r['flags'] & 0x80 else 0.0)
''')

# ---------------------------------------------------------------- the fields spell
F = 'tests/spells/lodgen_native_fields.py'
ed(F, '''    ck.check('j0 the .lodo is at version 5 (SEAM1 W4) and the .lodi at 3, 4, 5, 6, 7 or 8 (%d / %d)'
             % (h['version'], ih['version']),
             h['version'] == 5 and ih['version'] in (3, 4, 5, 6, 7, 8))
''', '''    ck.check('j0 the .lodo is at version 5 (SEAM1 W4) and the .lodi at 3, 4, 5, 6, 7, 8, 9 or 10 (%d / %d)'
             % (h['version'], ih['version']),
             h['version'] == 5 and ih['version'] in (3, 4, 5, 6, 7, 8, 9, 10))
    # v10 (lane BAKE2): bit 7 appears in a v10 file and only there, and a v10 file carries at least one
    wide = sum(1 for r in T['instances'] if r['flags'] & 0x80)
    ck.check('j0b the wide-scale bit (0x80) is set on %d instance(s): > 0 exactly when the .lodi is version 10 (%d)'
             % (wide, ih['version']), (wide > 0) == (ih['version'] == 10))
''')

# ---------------------------------------------------------------- the other Python consumers of the scale word
ed('tests/spells/lodgen_census_check.py', """    d['lodi.maxScale'] = round(max(r['scale'] for r in T['instances']) / 8192.0, 4)
""", """    d['lodi.maxScale'] = round(max(r['scaleF'] for r in T['instances']), 4)   # v10: 8 + u16/8192 under bit 7
""")
ed(F, """    ck.check('c1 no record has scale 0', all(r['scale'] != 0 for r in T['instances']),
             sum(1 for r in T['instances'] if r['scale'] == 0))
""", """    ck.check('c1 no record has scale 0', all(r['scaleF'] != 0 for r in T['instances']),
             sum(1 for r in T['instances'] if r['scaleF'] == 0))
""")
ed('tests/spells/lodgen_native_cut.py', """            r['scaleF'] = r['scale'] / 8192.0
""", """            r['scaleF'] = r['scale'] / 8192.0 + (8.0 if r['flags'] & 0x80 else 0.0)   # v10 SCALE_WIDE
""")

fail = 0
for path, lst in edits.items():
    b = open(R + path, 'rb').read()
    cr = b.count(b'\r')
    s = b.decode('utf-8')
    for old, new in lst:
        c = s.count(old)
        if c != 1:
            print('ANCHOR x%d in %s: %r' % (c, path, old[:70])); fail += 1; continue
        s = s.replace(old, new)
    if fail:
        continue
    nb = s.encode('utf-8')
    assert nb.count(b'\r') == cr, path
    edits[path] = nb
if fail:
    sys.exit('refused: %d anchor(s)' % fail)
import os
if os.environ.get('DRY'):
    sys.exit('DRY: all anchors ok')
for path, nb in edits.items():
    open(R + path, 'wb').write(nb)
    print('patched', path)
