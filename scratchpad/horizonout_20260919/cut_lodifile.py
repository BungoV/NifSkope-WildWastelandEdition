#!/usr/bin/env python3
"""HORIZONOUT: the .lodi writer stops emitting the v8 per-vertex horizon stream;
the READER stays tolerant of a v8 file met in the wild (header words parsed and
validated, region named in the note line, payload skipped by length, never a
crash). v9 becomes "v7 layout + the scrappable bit".
"""
import sys

CHECK = "--check" in sys.argv
ROOT = r"E:/Projects/NifskopeWildWastelandEdition/"


class F:
    def __init__(self, rel):
        self.path = ROOT + rel
        with open(self.path, "rb") as f:
            raw = f.read()
        if raw.count(b"\r"):
            sys.exit("REFUSED: %s carries CR bytes" % rel)
        self.d = raw.decode("utf-8")
        self.orig = self.d
        self.n = 0

    def sub(self, old, new, what):
        c = self.d.count(old)
        if c != 1:
            sys.exit("REFUSED [%s]: anchor appears %d times, want 1: %r" % (what, c, old[:90]))
        self.d = self.d.replace(old, new)
        self.n += 1

    def cut(self, start, end, what, repl=""):
        a = self.d.find(start)
        if a < 0 or self.d.find(start, a + 1) >= 0:
            sys.exit("REFUSED [%s]: start anchor not unique: %r" % (what, start[:90]))
        b = self.d.find(end, a)
        if b < 0:
            sys.exit("REFUSED [%s]: end anchor not found after start: %r" % (what, end[:90]))
        b += len(end)
        nl = self.d.find("\n", b)
        b = len(self.d) if nl < 0 else nl + 1
        self.d = self.d[:a] + repl + self.d[b:]
        self.n += 1

    def save(self):
        print("%-22s %d edits, %d -> %d chars, %d -> %d lines"
              % (self.path.rsplit("/", 1)[-1], self.n, len(self.orig), len(self.d),
                 self.orig.count("\n"), self.d.count("\n")))
        if CHECK:
            return
        with open(self.path, "wb") as f:
            f.write(self.d.encode("utf-8"))


# ===========================================================================
# src/lodifile.h
# ===========================================================================
h = F("src/lodifile.h")

h.sub("""constexpr quint32 LODI_VERSION_HORIZON = 8;""",
      """constexpr quint32 LODI_VERSION_HORIZON = 8;
/*! **VERSION 8 IS RETIRED AND NO WRITER IN THIS TREE PRODUCES IT** (lane
 *  HORIZONOUT, 2026-09-19, from bungo's "horizon goes bye bye now, we're back
 *  to identity"). The measured reason is in the version table of
 *  docs/LODGEN_NATIVE_LODO_LODI.md s3.7: baked per-vertex object horizons
 *  disagreed with a ray-cast sun on 50-58 per cent of object pixels at a low
 *  sun, where the identity far shadow map simulated at 64 units disagreed on
 *  about 9 per cent.
 *
 *  THE READER STAYS TOLERANT, deliberately: a v8 file baked by
 *  `release/NifSkope.before_horizonout.exe` or by any earlier build is still
 *  opened, its five header words are still parsed and still validated, and the
 *  stream's region is still named in the note line -- its PAYLOAD is skipped by
 *  its own length rather than copied into the table, because nothing in this
 *  tree consumes it any more. A file met in the wild must never crash a reader
 *  for carrying a retired stream. The constants below stay for the same
 *  reason: they are what the bytes of such a file MEAN. */""",
      "v8 retired note")

h.sub("""/*! v9, THE WORKSHOP-SCRAPPABLE BIT (lane HORIZON3, 2026-09-19).""",
      """/*! v9, THE WORKSHOP-SCRAPPABLE BIT (lane HORIZON3, 2026-09-19; the layout
 *  settled by lane HORIZONOUT the same day).
 *
 *  **VERSION 9 IS THE v7 LAYOUT PLUS BIT 6 OF THE INSTANCE FLAGS. IT CARRIES NO
 *  HORIZON STREAM.** A default bake writes version 7; a bake with
 *  `--scrappable` writes version 9. Version 8 sits between the two numbers and
 *  outside the line of descent -- it is the retired baked-horizon stream above,
 *  and 9 is NOT a superset of it.
 *
 *  WHY THE NUMBER MOVES AT ALL, when the layout does not. The version word is
 *  the only field that tells a reader WHICH INSTANCE FLAG BITS MAY APPEAR.
 *  Below v9, bit 6 is reserved-zero and a reader is entitled to refuse it --
 *  which is exactly what this one does. Writing the bit into a file stamped
 *  version 7 would be a file that its own contract says cannot exist, so the
 *  number has to move even though not one byte of layout does.
 *
 *  WHY NOT REUSE 7. A v7 reader built before today -- ours on an older exe, and
 *  any consumer -- reads `LODI_INST_FLAGS_KNOWN` as 0x3F. A v7 file with bit 6
 *  set would be refused by it, or worse, silently accepted by a looser one. A
 *  new number is the only honest way to say "there is one more bit here".""",
      "v9 doc head")

h.sub(""" *  The rule is `EsmScrapIndex` in `src/esmdata.h` and the count it produces on
 *  the measured urban region is 14 of 33,123. `--horizon-scrappable 0` (the
 *  default) writes no bit, keeps the version at 8, and the file is byte for
 *  byte the v8 file. */""",
      """ *  The rule is `EsmScrapIndex` in `src/esmdata.h` and the count it produces on
 *  the measured urban region is 14 of 33,123. Leaving `--scrappable` off (the
 *  default) writes no bit, keeps the version at 7, and the file is byte for
 *  byte the v7 file. */""",
      "v9 doc tail")

h.sub(""" *  IT COSTS NOTHING. It is bit 6 of the instance flags word the 24-byte record
 *  has carried since v1, so a v9 file is a v8 file with one more bit
 *  meaningful; the version word is the only thing that moved. The version has
 *  to move all the same: below v9 that bit is reserved-zero and a reader is
 *  entitled to refuse it, which is exactly what this one does.
 *
""",
      """ *  IT COSTS NOTHING. It is bit 6 of the instance flags word the 24-byte record
 *  has carried since v1, so a v9 file is a v7 file with one more bit
 *  meaningful.
 *
""",
      "v9 doc middle")

h.sub("""/*! THE THREE KNOBS, AND THIS IS THE ONE PLACE THEY LIVE (contract s4.11).
 *  `--horizon-azimuths` / `--horizon-reach` move them; the softening width is
 *  the CONSUMER'S and lives in `LODI_HORIZON_SOFT_DEG` beside them so a reader
 *  of this header needs no second file.""",
      """/*! WHAT A VERSION-8 FILE'S BYTES MEAN (contract s4.11). No writer in this tree
 *  produces such a file any more (see LODI_VERSION_HORIZON above) and there is
 *  no longer a switch that moves these; they stay because a reader that meets
 *  a v8 file in the wild needs them, and needs no second file to have them.""",
      "horizon knobs doc")

# the writer's own switch and its three knobs on LodiSrcSet
h.cut("""\t/*! v8. FALSE is the module's off value (`--lodi-v7`): no horizon stream, no""",
      """\tfloat horizonReach = LODI_HORIZON_REACH;""", "LodiSrcSet horizon knobs")

h.cut("""\tstd::vector<quint32> vertexHorizonFirst;        //!< v8, instanceCount + 1 entries into `vertexHorizon`""",
      """\tstd::vector<quint8> vertexHorizon;              //!< v8, `horizonAzimuths` bytes a library vertex an instance""",
      "LodiTable horizon vectors",
      """\t/* v8's per-vertex horizon stream is NOT read into this table (lane
\t * HORIZONOUT, 2026-09-19). A v8 file still opens and its stream is still
\t * bounds-checked, but the payload is skipped by its length: nothing in this
\t * tree consumes it, and a vector nobody reads is a vector that goes stale. */
""")

h.cut("""\t/* v8, the horizon. The brief's census words, all WRITTEN and MOVING: the""",
      """\tfloat horizonReach = 0.0f;          //!< the march reach in WORLD units as written to 0x12C""",
      "LodiStats horizon words")

# LodiSrcInstance::vertexHorizon (the writer's input)
h.cut("""\t/*! v8: the horizon, `LodiSrcSet::horizonAzimuths` bytes per vertex of the""",
      """\tstd::vector<quint8> vertexHorizon;""", "LodiSrcInstance::vertexHorizon")

h.save()

# ===========================================================================
# src/lodifile.cpp
# ===========================================================================
c = F("src/lodifile.cpp")

# --- writer ---------------------------------------------------------------
c.cut("""\t/* v8 (b): THE VERTEX-HORIZON STREAM, s4.11 -- s4.10's layout with A bytes a""",
      """\t\t\tcursor += quint32( r.vertexHorizon.size() );
\t\t}
\t}
""", "writer: vhor stream build")

c.cut("""\t/* v8: the horizon is its own switch on top. `--lodi-v7` -- `set.horizon`""",
      """\t\th.horizonReach = set.horizonReach;
\t}
""", "writer: v8 version stamp")

c.sub("""\t// v8: the horizon stream after every one of them, so nothing at all moves
\tif ( !vhor.empty() )
\t\th.offVertexHorizon = payload( vhor.data(), quint64( vhor.size() ) );
""", "", "writer: vhor payload")

c.sub("""\t/* v8: the horizon stream joins after the sky stream. Absent, zero bytes fold
\t * in and a v3..v7 file's CRC does not move. */
\th.indexCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( vhor.data() ), qsizetype( vhor.size() ), h.indexCrc32 );
""", "", "writer: vhor crc fold")

c.sub("""\tif ( set.horizon ) {
\t\tputLE<quint64>( file, H_OFF_VHOR, h.offVertexHorizon );
\t\tputLE<quint32>( file, H_VHORBYTES, h.vertexHorizonBytes );
\t\tputLE<quint16>( file, H_HORAZ, h.horizonAzimuths );
\t\tputLE<quint16>( file, H_HORSTEPS, h.horizonSteps );
\t\tputLE<quint32>( file, H_HORREACH, f32Bits( h.horizonReach ) );
\t}
""", "", "writer: v8 header words")

c.cut("""\t\tstats->vertexHorizonBytes = h.vertexHorizonBytes;""",
      """\t\t\t: 0.0f;""", "writer: horizon stats")

# --- the v9 guard ---------------------------------------------------------
c.sub("""\t * THE GUARD, and it is why this is not two lines. Version 9 IMPLIES version
\t * 8's 512-byte header. A bake run with `--lodi-v7` has a 256-byte header,
\t * so writing 9 on it would claim a layout the file does not have. On such a
\t * file the bit is DROPPED rather than written into a version that would be
\t * a lie -- and dropped visibly, because `scrappableWritten` comes back 0
\t * and the caller's census says 0 where the rule said otherwise. */""",
      """\t * THE GUARD, and it is why this is not two lines. Version 9 IMPLIES version
\t * 7's 512-byte header block (lane HORIZONOUT, 2026-09-19: 9 is the v7 layout
\t * plus this bit, and NOT a superset of the retired version 8). A bake run
\t * with `--lodi-v6` has a 256-byte header, so writing 9 on it would claim a
\t * layout the file does not have. On such a file the bit is DROPPED rather
\t * than written into a version that would be a lie -- and dropped visibly,
\t * because `scrappableWritten` comes back 0 and the caller's census says 0
\t * where the rule said otherwise. */""",
      "v9 guard comment")

c.sub("""\t\tif ( h.version >= LODI_VERSION_HORIZON ) {""",
      """\t\tif ( h.version >= LODI_VERSION_GROUP_SKY ) {""",
      "v9 guard test")

# --- reader ---------------------------------------------------------------
c.sub("""\tconst bool v8 = ( h.version == LODI_VERSION_HORIZON || h.version == LODI_VERSION_SCRAPPABLE );
\tconst bool v7 = ( h.version == LODI_VERSION_GROUP_SKY ) || v8;""",
      """\t/* v8 is the RETIRED baked-horizon version and is the ONLY version that
\t * carries the stream (lane HORIZONOUT, 2026-09-19). v9 is the v7 layout plus
\t * the scrappable flag bit and carries no stream, so it is v7-shaped here. */
\tconst bool v8 = ( h.version == LODI_VERSION_HORIZON );
\tconst bool v7 = ( h.version == LODI_VERSION_GROUP_SKY ) || v8
\t\t|| ( h.version == LODI_VERSION_SCRAPPABLE );""",
      "reader: v8/v7 predicate")

c.sub("""\t\t\tif ( v8 ) {
\t\t\t\t/* v8: the horizon stream and the three knobs the cast used. The
\t\t\t\t * stream is what version 8 IS -- a bake without it is written at
\t\t\t\t * version 7, which is what makes `--lodi-v7` byte-identical. */""",
      """\t\t\tif ( v8 ) {
\t\t\t\t/* v8, RETIRED: the horizon stream and the three knobs the cast
\t\t\t\t * used. Nothing in this tree writes this any more, and nothing
\t\t\t\t * reads the PAYLOAD; the words are still parsed and still checked
\t\t\t\t * so that a v8 file met in the wild is either opened honestly or
\t\t\t\t * refused by name, never mis-read. */""",
      "reader: v8 header comment")

c.sub("""\t\t\t\treturn refuse( QString( "version %1 carrying version-8 header words (horizon stream at 0x11C = %2, "
\t\t\t\t\t"%3 bytes, %4 azimuths at 0x128). Version 7 reserves 0x11C..0x1FF and writes zeros there" )""",
      """\t\t\t\treturn refuse( QString( "version %1 carrying version-8 header words (horizon stream at 0x11C = %2, "
\t\t\t\t\t"%3 bytes, %4 azimuths at 0x128). Versions 7 and 9 reserve 0x11C..0x1FF and write zeros there" )""",
      "reader: v7-carrying-v8-words message")

# the payload parse: offsets validated locally, payload skipped by length
c.sub("""\tif ( iVhor >= 0 ) {
\t\t/* v8: s4.11, which is s4.10's layout with `horizonAzimuths` bytes a
\t\t * vertex. The three offset rules are asked in the same words, and then
\t\t * the population cross-check is asked with the stride in it. */
\t\tconst size_t nOff = size_t( h.instanceCount ) + 1;
\t\tT.vertexHorizonFirst.resize( nOff );
\t\tconst unsigned char * hb = p + h.offVertexHorizon;
\t\tfor ( size_t i = 0; i < nOff; i++ ) {
\t\t\tT.vertexHorizonFirst[i] = getLE<quint32>( hb + 4 * i );
\t\t\tif ( i && T.vertexHorizonFirst[i] < T.vertexHorizonFirst[i - 1] )
\t\t\t\treturn refuse( QString( "vertex-horizon offset %1 (%2) is below offset %3 (%4)" )
\t\t\t\t\t.arg( i ).arg( T.vertexHorizonFirst[i] ).arg( i - 1 ).arg( T.vertexHorizonFirst[i - 1] ) );
\t\t}
\t\tconst quint64 dataBytes = quint64( h.vertexHorizonBytes ) - 4ull * nOff;
\t\tif ( T.vertexHorizonFirst[0] != 0 || quint64( T.vertexHorizonFirst[nOff - 1] ) != dataBytes )
\t\t\treturn refuse( QString( "vertex-horizon offsets run %1..%2 but the stream carries %3 horizon bytes "
\t\t\t\t"after its %4 offsets" ).arg( T.vertexHorizonFirst[0] ).arg( T.vertexHorizonFirst[nOff - 1] )
\t\t\t\t.arg( dataBytes ).arg( nOff ) );
\t\tT.vertexHorizon.resize( size_t( dataBytes ) );
\t\tif ( dataBytes ) std::memcpy( T.vertexHorizon.data(), hb + 4 * nOff, size_t( dataBytes ) );
\t\t/* One vertex population, A bytes each. A slice that is not exactly A
\t\t * times the AO slice means one of the two casts ran on a different
\t\t * mesh, and a consumer indexing vertex v at v * A would read another
\t\t * vertex's bins without ever knowing. */
\t\tif ( iVao >= 0 )
\t\t\tfor ( size_t i = 0; i < size_t( h.instanceCount ); i++ ) {
\t\t\t\tconst quint32 hn = T.vertexHorizonFirst[i + 1] - T.vertexHorizonFirst[i];
\t\t\t\tconst quint32 an = T.vertexAoFirst[i + 1] - T.vertexAoFirst[i];
\t\t\t\tif ( hn && hn != an * quint32( h.horizonAzimuths ) )
\t\t\t\t\treturn refuse( QString( "instance %1 has %2 horizon bytes but %3 AO bytes x %4 azimuths = %5; "
\t\t\t\t\t\t"the two streams are one vertex population" ).arg( i ).arg( hn ).arg( an )
\t\t\t\t\t\t.arg( h.horizonAzimuths ).arg( an * quint32( h.horizonAzimuths ) ) );
\t\t\t}
\t}
""",
      """\tif ( iVhor >= 0 ) {
\t\t/* v8's RETIRED per-vertex horizon stream, SKIPPED BY LENGTH (lane
\t\t * HORIZONOUT, 2026-09-19). The payload is never copied -- nothing in
\t\t * this tree consumes it -- but its offset table is still walked, because
\t\t * skipping a region is only honest if the region is the size it claims
\t\t * to be. The offsets live on the stack and die here; `LodiTable` carries
\t\t * no horizon vector at all.
\t\t *
\t\t * The stream's extent is ALSO still in `tabs`, so the payload sweep and
\t\t * `indexCrc32` still cover it and the note line still names it. A v8 file
\t\t * therefore opens, reports honestly what it holds, and is refused BY NAME
\t\t * if the stream is malformed -- it is never quietly mis-read. */
\t\tconst size_t nOff = size_t( h.instanceCount ) + 1;
\t\tconst unsigned char * hb = p + h.offVertexHorizon;
\t\tstd::vector<quint32> hfirst( nOff );
\t\tfor ( size_t i = 0; i < nOff; i++ ) {
\t\t\thfirst[i] = getLE<quint32>( hb + 4 * i );
\t\t\tif ( i && hfirst[i] < hfirst[i - 1] )
\t\t\t\treturn refuse( QString( "vertex-horizon offset %1 (%2) is below offset %3 (%4)" )
\t\t\t\t\t.arg( i ).arg( hfirst[i] ).arg( i - 1 ).arg( hfirst[i - 1] ) );
\t\t}
\t\tconst quint64 dataBytes = quint64( h.vertexHorizonBytes ) - 4ull * nOff;
\t\tif ( hfirst[0] != 0 || quint64( hfirst[nOff - 1] ) != dataBytes )
\t\t\treturn refuse( QString( "vertex-horizon offsets run %1..%2 but the stream carries %3 horizon bytes "
\t\t\t\t"after its %4 offsets" ).arg( hfirst[0] ).arg( hfirst[nOff - 1] )
\t\t\t\t.arg( dataBytes ).arg( nOff ) );
\t\t/* One vertex population, A bytes each. A slice that is not exactly A
\t\t * times the AO slice means one of the two casts ran on a different
\t\t * mesh, and a consumer indexing vertex v at v * A would read another
\t\t * vertex's bins without ever knowing. */
\t\tif ( iVao >= 0 )
\t\t\tfor ( size_t i = 0; i < size_t( h.instanceCount ); i++ ) {
\t\t\t\tconst quint32 hn = hfirst[i + 1] - hfirst[i];
\t\t\t\tconst quint32 an = T.vertexAoFirst[i + 1] - T.vertexAoFirst[i];
\t\t\t\tif ( hn && hn != an * quint32( h.horizonAzimuths ) )
\t\t\t\t\treturn refuse( QString( "instance %1 has %2 horizon bytes but %3 AO bytes x %4 azimuths = %5; "
\t\t\t\t\t\t"the two streams are one vertex population" ).arg( i ).arg( hn ).arg( an )
\t\t\t\t\t\t.arg( h.horizonAzimuths ).arg( an * quint32( h.horizonAzimuths ) ) );
\t\t\t}
\t}
""",
      "reader: skip the payload by length")

# the dump's read-back of the payload
c.cut("""\t\t/* v8: the horizon stream, READ BACK from the bytes in the file and not""",
      """\t\t\t\t<< QString( "horizonMaxByte %1" ).arg( nb ? eMax : 0 );""",
      "dump: horizon read-back",
      """\t\t/* v8's retired horizon stream is not read into the table any more (lane
\t\t * HORIZONOUT), so there is nothing here to read back. The five header
\t\t * words above still print, which is what tells a person a v8 file in
\t\t * front of them carries one. */
""")

c.save()
print("OK" if not CHECK else "--check: nothing written")
