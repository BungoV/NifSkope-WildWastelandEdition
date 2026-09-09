#!/usr/bin/env python
"""TERRAINFIX step 3: the worldspace's default water reaches the .lodt.

The per-cell record already carries the RESOLVED water height and a water type
of 0xFFFF meaning "the worldspace default" -- and the default's WATR form was
in no section of the file, so 0xFFFF was unresolvable. Header version 2 appends
the two worldspace-level water fields AFTER the ten section offsets, so every
offset a v1 reader uses keeps its place."""

H = 'src/lodtfile.h'
C = 'src/lodtfile.cpp'


def patch(path, pairs):
    b = open(path, 'rb').read().decode('utf-8')
    cr0 = b.count('\r')
    for old, new in pairs:
        c = b.count(old)
        assert c == 1, 'anchor count %d for %r in %s' % (c, old[:60], path)
        b = b.replace(old, new)
    assert b.count('\r') == cr0, 'line endings moved in ' + path
    open(path, 'wb').write(b.encode('utf-8'))
    print('%s patched, CR %d (unchanged)' % (path, cr0))


patch(H, [
    ('''\t//! Baked AO, samples per cell edge. 0 = none.
\tint aoSamples = 8;''',
     '''\t//! Baked AO, samples per cell edge. 0 = none.
\tint aoSamples = 8;
\t/*! Header version to WRITE. 2 adds the worldspace default water height and
\t *  WATR form after the section offsets, which is what makes a cell's
\t *  "water type 0xFFFF = the worldspace default" resolvable at all; 1 is the
\t *  exact bytes this writer produced before that, for a consumer that has
\t *  not learned version 2 yet. Both are accepted by the reader. The
\t *  environment variable WW_LODT_VERSION overrides it, so the fallback is
\t *  reachable without a rebuild. */
\tint headerVersion = 2;'''),
    ('''\tint watrCount() const { return int( watr.size() ); }''',
     '''\tint watrCount() const { return int( watr.size() ); }
\tint headerVersion() const { return ver; }
\t/*! The worldspace's own water, from WRLD DNAM/NAM2 -- what a cell whose
\t *  water type reads 0xFFFF is inheriting. Version 1 files carry neither,
\t *  and report 0 and "no default water": the fields did not exist, so a
\t *  reader must not treat the zero as a form id. */
\tbool hasDefaultWater() const { return ver >= 2 && defWaterType != 0; }
\tfloat defaultWaterHeight() const { return defWaterH; }
\tquint32 defaultWaterType() const { return defWaterType; }'''),
    ('''\tint spc = 32, blkEdge = 32, levels = 4, aoS = 0, ovS = 0;''',
     '''\tint spc = 32, blkEdge = 32, levels = 4, aoS = 0, ovS = 0;
\tint ver = 1;
\tfloat defWaterH = 0.0f;
\tquint32 defWaterType = 0;'''),
])

patch(C, [
    # --- the constant becomes a range ---------------------------------
    ('''constexpr quint32 LODT_VERSION = 1;''',
     '''/* Version 2 appended the worldspace default water fields AFTER the ten
 * section offsets, so 0x00..0x97 is unchanged and every offset a version 1
 * reader uses still sits where it did; only the first section moved, and it is
 * addressed by an offset in the header. The writer emits 2; both are read. */
constexpr quint32 LODT_VERSION = 2;
constexpr quint32 LODT_VERSION_MIN = 1;
constexpr qsizetype LODT_HEADER_V1 = 0x98;
constexpr qsizetype LODT_HEADER_V2 = 0xA0;'''),
    # --- the source carries them --------------------------------------
    ('''\tquint32 defaultWaterType = 0;''',
     '''\tquint32 defaultWaterType = 0;
\tfloat defaultWaterHeight = 0.0f;'''),
    # --- the writer -----------------------------------------------------
    ('''\tBuf h;
\th.u32( LODT_MAGIC );
\th.u32( LODT_VERSION );''',
     '''\t/* The written version. Two paths reach it: the caller's option, and
\t * WW_LODT_VERSION for a consumer that must be handed version 1 bytes
\t * without a rebuild. Anything else is a refusal rather than a silent
\t * downgrade -- a wrong version number is exactly the field that makes a
\t * reader misparse instead of refuse. */
\tquint32 version = quint32( opts.headerVersion );
\tif ( qEnvironmentVariableIsSet( "WW_LODT_VERSION" ) )
\t\tversion = quint32( qEnvironmentVariableIntValue( "WW_LODT_VERSION" ) );
\tif ( version < LODT_VERSION_MIN || version > LODT_VERSION )
\t\treturn fail( QStringLiteral( "header version %1 is not one this writer knows (%2..%3)" )
\t\t\t.arg( version ).arg( LODT_VERSION_MIN ).arg( LODT_VERSION ) );

\tBuf h;
\th.u32( LODT_MAGIC );
\th.u32( version );'''),
    ('''\tconst qsizetype offSizeAt = h.size();     h.u64( 0 );
''',
     '''\tconst qsizetype offSizeAt = h.size();     h.u64( 0 );
\tif ( version >= 2 ) {
\t\t/* The worldspace's own water. Without these two fields the per-cell
\t\t * "water type = 0xFFFF, meaning the worldspace default" is a promise
\t\t * the file cannot keep: the default's WATR form appears in no table,
\t\t * because the writer deliberately does not intern it (that is what
\t\t * makes 0xFFFF distinguishable from "explicitly this type"). The
\t\t * height is here too, so a consumer can tell an inheriting cell's
\t\t * plane from the file alone -- the per-cell water height is already
\t\t * resolved, so the two must agree and now visibly do. */
\t\th.f32( src.defaultWaterHeight );
\t\th.u32( src.defaultWaterType );
\t}
\t/* The header size is what every section offset is measured against, so it
\t * is checked here rather than asserted in a debug build nobody runs. */
\tif ( h.size() != ( version >= 2 ? LODT_HEADER_V2 : LODT_HEADER_V1 ) )
\t\treturn fail( QStringLiteral( "header assembled to %1 bytes, not the %2 version %3 declares" )
\t\t\t.arg( h.size() ).arg( version >= 2 ? LODT_HEADER_V2 : LODT_HEADER_V1 ).arg( version ) );
'''),
    # --- the ESM source fills them --------------------------------------
    ('''\tsrc.defaultWaterType = world.defaultWaterType();''',
     '''\tsrc.defaultWaterType = world.defaultWaterType();
\tsrc.defaultWaterHeight = world.defaultWaterHeight();'''),
    ('''\t\tsrc.defaultWaterType = waterFrom->defaultWaterType();''',
     '''\t\tsrc.defaultWaterType = waterFrom->defaultWaterType();
\t\tsrc.defaultWaterHeight = waterFrom->defaultWaterHeight();'''),
    # --- the census line names them -------------------------------------
    # %19..%21, because %1..%18 are taken and Qt reads two digits
    ('''\t\t*error = QString( "cells %1x%2, land %3, water %4, WATR types %5, "''',
     '''\t\t*error = QString( "v%19 cells %1x%2, land %3, water %4 "
\t\t\t"(worldspace default height %20 type %21), WATR types %5, "'''),
    ('''\t\t\t.arg( nullPlanes );''',
     '''\t\t\t.arg( nullPlanes )
\t\t\t.arg( version ).arg( double( src.defaultWaterHeight ) )
\t\t\t.arg( src.defaultWaterType, 8, 16, QChar( '0' ) );'''),
    # --- the reader accepts both ----------------------------------------
    ('''\tbuf = file.read( 0x98 );
\tif ( buf.size() < 0x98 )
\t\treturn fail( QStringLiteral( "too short to be a .lodt" ) );
\tif ( rd<quint32>( buf, 0 ) != LODT_MAGIC )
\t\treturn fail( QStringLiteral( "not a .lodt (bad magic)" ) );
\t{
\t\tconst quint64 dataAt = rd<quint64>( buf, 0x88 );
\t\tconst quint64 total = rd<quint64>( buf, 0x90 );
\t\tif ( dataAt < 0x98 || dataAt > total || total != quint64( file.size() ) )''',
     '''\tbuf = file.read( LODT_HEADER_V1 );
\tif ( buf.size() < LODT_HEADER_V1 )
\t\treturn fail( QStringLiteral( "too short to be a .lodt" ) );
\tif ( rd<quint32>( buf, 0 ) != LODT_MAGIC )
\t\treturn fail( QStringLiteral( "not a .lodt (bad magic)" ) );
\tver = int( rd<quint32>( buf, 4 ) );
\t{
\t\tconst qsizetype hdrBytes = ver >= 2 ? LODT_HEADER_V2 : LODT_HEADER_V1;
\t\tconst quint64 dataAt = rd<quint64>( buf, 0x88 );
\t\tconst quint64 total = rd<quint64>( buf, 0x90 );
\t\tif ( dataAt < quint64( hdrBytes ) || dataAt > total || total != quint64( file.size() ) )'''),
    ('''\tif ( rd<quint32>( buf, 4 ) != LODT_VERSION )
\t\treturn fail( QStringLiteral( "unsupported version %1" ).arg( rd<quint32>( buf, 4 ) ) );''',
     '''\tif ( quint32( ver ) < LODT_VERSION_MIN || quint32( ver ) > LODT_VERSION )
\t\treturn fail( QStringLiteral( "unsupported version %1 (this reader knows %2..%3)" )
\t\t\t.arg( ver ).arg( LODT_VERSION_MIN ).arg( LODT_VERSION ) );
\tif ( ver >= 2 ) {
\t\tdefWaterH = rd<float>( buf, 0x98 );
\t\tdefWaterType = rd<quint32>( buf, 0x9C );
\t}'''),
])
