#!/usr/bin/env python3
"""HORIZONOUT: `.lodo` version 5 (the subdivided library) never existed.

Unlike `.lodi` v8 -- which `release/NifSkope.before_horizonout.exe` really does
write, and which this tree therefore still READS -- no exe ever wrote a `.lodo`
version 5: lane HORIZON3 wrote the code and never linked it (its own
scratchpad/horizon3_20260919/PENDING.md says so, and the exe on disk at this
lane's launch is HORIZON2's). A reader tolerance for a file that cannot exist
is untested code guarding an impossible case, so version 5 comes out whole:
the pass, the two header words, the conditional version stamp, the reader's
v5 clause and the dump lines. `.lodo` stays at version 4, byte for byte.
"""
import sys

CHECK = "--check" in sys.argv
ROOT = r"E:/Projects/NifskopeWildWastelandEdition/"


class F:
    def __init__(self, rel):
        self.path = ROOT + rel
        with open(self.path, "rb") as f:
            raw = f.read()
        self.cr = raw.count(b"\r")
        self.d = raw.decode("utf-8")
        self.orig = self.d
        self.n = 0

    def sub(self, old, new, what):
        c = self.d.count(old)
        if c != 1:
            sys.exit("REFUSED [%s]: anchor appears %d times, want 1" % (what, c))
        self.d = self.d.replace(old, new)
        self.n += 1

    def cut(self, start, end, what, repl=""):
        a = self.d.find(start)
        if a < 0 or self.d.find(start, a + 1) >= 0:
            sys.exit("REFUSED [%s]: start anchor not unique" % what)
        b = self.d.find(end, a)
        if b < 0:
            sys.exit("REFUSED [%s]: end anchor not found after start" % what)
        b += len(end)
        nl = self.d.find("\n", b)
        b = len(self.d) if nl < 0 else nl + 1
        self.d = self.d[:a] + repl + self.d[b:]
        self.n += 1

    def save(self):
        name = self.path.rsplit("/", 1)[-1]
        print("%-14s %2d edits, %d -> %d chars, %d -> %d lines (CR %d -> %d)"
              % (name, self.n, len(self.orig), len(self.d),
                 self.orig.count("\n"), self.d.count("\n"),
                 self.cr, self.d.count("\r")))
        if self.d.count("\r") != self.cr:
            sys.exit("REFUSED: %s CR count moved" % name)
        if not CHECK:
            with open(self.path, "wb") as f:
                f.write(self.d.encode("utf-8"))


# ===========================================================================
h = F("src/lodofile.h")

h.cut("""/*! v5 (2026-09-19, lane HORIZON3): THE SUBDIVIDED LIBRARY. `--horizon-subdivide""",
      """constexpr quint32 LODO_VERSION_SUBDIVIDED = 5;""",
      "LODO_VERSION_SUBDIVIDED",
      """/* VERSION 5 (lane HORIZON3, 2026-09-19) WAS THE SUBDIVIDED LIBRARY, cut so
 * that the per-vertex horizon stream of `.lodi` v8 had a vertex where a far
 * shadow's edge fell. The baked-horizon route was dropped the same day (lane
 * HORIZONOUT, on bungo's word) and no exe ever wrote a version 5 file, so
 * neither the writer nor the reader carries one: this format's only version
 * is 4. The history paragraph is in docs/LODGEN_NATIVE_LODO_LODI.md. */
""")

h.cut("""\t/*! v5, 0xD4 and 0xD8: what `--horizon-subdivide` ADDED to this library --""",
      """\tquint32 subdivTriangles = 0;""", "LodoHeader subdiv words")

h.cut("""\t/*! v5: what `lodoSubdivideLibrary` did to this library, which is what the""",
      """\tquint32 subdivTriangles = 0;""", "LodoLibrary subdiv words")

h.cut("""/* ---- v5, TIER 2: the subdivision pass (lane HORIZON3, 2026-09-19) ---- */""",
      """\tconst std::vector<float> & meshMaxScale, struct LodgenSubdivStats * stats, QString * error );""",
      "lodoSubdivideLibrary declaration")
h.save()

# ===========================================================================
c = F("src/lodofile.cpp")

c.sub('#include "lodgsubdiv.h"\n', "", "include lodgsubdiv.h")

c.cut("""/*! v5 (lane HORIZON3) takes the next two words of that pad: 0xD4 the library""",
      """constexpr int H_SUBDIV_VERTS = 0xD4, H_SUBDIV_TRIS = 0xD8, H_RESERVED_DC = 0xDC;""",
      "H_SUBDIV_* constants")

c.cut("""//! Renormalise an interpolated direction; a midpoint of two opposed unit""",
      """\tlib.subdivTriangles = st.trianglesAdded();
\treturn finish();
}
""", "lodoSubdivUnit + lodoSubdivideLibrary")

c.sub("""\t/* v5: THE CONDITIONAL VERSION. A library that was never subdivided writes
\t * the same 4 it always wrote, so `--horizon-subdivide 0` is byte-identical
\t * to every bake before this lane (CONSTITUTION 10). */
\tconst bool subdivided = ( lib.subdivVertices != 0 || lib.subdivTriangles != 0 );
\th.version = subdivided ? LODO_VERSION_SUBDIVIDED : LODO_VERSION;
\th.subdivVertices = lib.subdivVertices;
\th.subdivTriangles = lib.subdivTriangles;
\tputLE<quint32>( file, H_VERSION, h.version );""",
      "\tputLE<quint32>( file, H_VERSION, h.version );",
      "writer: conditional version")

c.sub("""\tif ( subdivided ) {
\t\tputLE<quint32>( file, H_SUBDIV_VERTS, h.subdivVertices );
\t\tputLE<quint32>( file, H_SUBDIV_TRIS, h.subdivTriangles );
\t}
""", "", "writer: subdiv header words")

c.sub("""\tif ( h.version != LODO_VERSION && h.version != LODO_VERSION_SUBDIVIDED )
\t\treturn refuse( QString( "version %1; this reader knows %2 and %3" ).arg( h.version )
\t\t\t.arg( LODO_VERSION ).arg( LODO_VERSION_SUBDIVIDED ) );""",
      """\tif ( h.version != LODO_VERSION )
\t\treturn refuse( QString( "version %1; this reader knows %2" ).arg( h.version )
\t\t\t.arg( LODO_VERSION ) );""",
      "reader: version check")

c.cut("""\t/* v5: the two subdivision words at 0xD4/0xD8. They are part of the pad in a""",
      """\t\t\t\t"library and cannot have added every vertex in it" ).arg( h.subdivVertices ).arg( h.vertexCount ) );
\t}
""", "reader: v5 header words")

c.sub("""\t\tif ( h.version >= LODO_VERSION_SUBDIVIDED && i >= H_SUBDIV_VERTS && i < H_RESERVED_DC )
\t\t\tcontinue;
""", "", "reader: pad skip")

c.cut("""\t/* v5: the two subdivision words travel back INTO the library, so a reused""",
      """\tL.subdivTriangles = h.subdivTriangles;""", "reader: words back into the library")

c.sub("""\t\t<< QString( "subdivVertices %1" ).arg( h.subdivVertices )
\t\t<< QString( "subdivTriangles %1" ).arg( h.subdivTriangles )
""", "", "dump lines")

c.save()
print("--check: nothing written" if CHECK else "OK")
