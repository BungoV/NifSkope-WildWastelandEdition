#!/usr/bin/env python3
"""HORIZONOUT: the .lodt role-7 TERRAIN HORIZON SHEET is no longer baked.

The role itself stays defined in src/io/lodvfile.h and its reader-side
validation stays, so a .lodt baked by an earlier exe still opens. What goes is
the PRODUCER: the cast, the two lattices, the staging planes, the sheet
descriptors, the encoder's horizon pass, the .lodm keys and the census words.
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
        name = self.path.rsplit("/", 1)[-1]
        print("%-16s %2d edits, %d -> %d chars, %d -> %d lines (CR %d -> %d)"
              % (name, self.n, len(self.orig), len(self.d),
                 self.orig.count("\n"), self.d.count("\n"),
                 self.cr, self.d.count("\r")))
        if self.d.count("\r") != self.cr:
            sys.exit("REFUSED: %s CR count moved" % name)
        if CHECK:
            return
        with open(self.path, "wb") as f:
            f.write(self.d.encode("utf-8"))


# ===========================================================================
# src/lodgen.h -- the options
# ===========================================================================
h = F("src/lodgen.h")
h.cut("""\t/*! THE TERRAIN HORIZON SHEET (lane HORIZON1, 2026-09-18), role 7.""",
      """\tint horizonRefute = 0;""",
      "LodgenVtOptions horizon knobs",
      """\t/* THE TERRAIN HORIZON SHEET (lane HORIZON1, 2026-09-18), `.lodt` role 7,
\t * REMOVED 2026-09-19 by lane HORIZONOUT on bungo's "horizon goes bye bye
\t * now, we're back to identity". No bake writes a role-7 sheet any more and
\t * there is no switch that turns one on. The ROLE stays defined in
\t * src/io/lodvfile.h and its reader-side rules stay enforced, so a .lodt
\t * baked by `release/NifSkope.before_horizonout.exe` still opens. The
\t * measured reason is in docs/LODGEN_TERRAIN_VT.md's history paragraph. */
""")
h.save()

# ===========================================================================
# src/lodgen.cpp
# ===========================================================================
c = F("src/lodgen.cpp")

c.sub('#include "lodghorizon.h"\n#include "lodghorizonrefute.h"\n', "", "includes")

# --- the staging planes ---------------------------------------------------
c.cut("""\t/*! THE HORIZON PLANES (role 7), one per four azimuth bins, R,G,B,A.""",
      """\tstd::vector<std::vector<quint32>> horizon;""", "LodgenVtStage::horizon")

# --- the encoder ----------------------------------------------------------
c.sub("""QByteArray lodgenVtEncodeTile( const LodgenVtStage & st, int stored, int mips, bool withHeight,
\tbool withEmissive, bool coverInColor, int horizonSheets )""",
      """QByteArray lodgenVtEncodeTile( const LodgenVtStage & st, int stored, int mips, bool withHeight,
\tbool withEmissive, bool coverInColor )""",
      "lodgenVtEncodeTile signature")

c.cut("""\t/* THE HORIZON SHEETS, last and in bin order, so a consumer finds bin b at""",
      """\t\t\tlodgenVtEncodeRgba8( img, out );
\t\t}
\t}
""", "lodgenVtEncodeTile horizon pass")

# lodgenVtEncodeRgba8 exists only for the horizon sheet
c.cut("""/*! One image as raw R8G8B8A8_UNORM, tightly packed: the HORIZON sheet, and the""",
      """		p[i * 4 + 3] = quint8( v >> 24 );
	}
}
""", "lodgenVtEncodeRgba8")

# --- the level picker and the caster -------------------------------------
c.cut("""/*! The level the horizon sheet (role 7) is authored on: the COARSEST level""",
      """\treturn best;
}
""", "lodgenVtHorizonLevel")

c.cut("""/*! THE TERRAIN HORIZON (lane HORIZON1, 2026-09-18), `.lodt` role 7.""",
      """\t\t\t\trefute.sample( cast, nullptr, &sky, wx, wy, gz, nullptr, rb.data() );
\t\t\t}
\t\t}
\t}
};
""", "LodgenVtHorizon struct")

# --- the estimator --------------------------------------------------------
c.sub("""static qint64 lodgenVtTileBytes( int stored, int mips, bool coverTile, bool withHeight,
\tbool withEmissive = false, bool coverInColor = false, int horizonSheets = 0 )""",
      """static qint64 lodgenVtTileBytes( int stored, int mips, bool coverTile, bool withHeight,
\tbool withEmissive = false, bool coverInColor = false )""",
      "lodgenVtTileBytes signature")

c.sub("\t\tn += qint64( horizonSheets ) * s * s * 4;\n", "", "lodgenVtTileBytes horizon term")

c.cut("""\t/* THE HORIZON LEVEL, in the estimate for the same reason it is in the bake:""",
      """\t\t: 0;
""", "estimate hzSheets/hzLevel/rawHorizon")

c.sub("""\t\tout->pyramidBytes += tiles * ( avgRaw + avgPad ) + tiles * 24 + 4096;
\t\tif ( i == hzLevel )
\t\t\tout->pyramidBytes += tiles * rawHorizon;
""",
      "\t\tout->pyramidBytes += tiles * ( avgRaw + avgPad ) + tiles * 24 + 4096;\n",
      "estimate horizon bytes")

# --- the bake -------------------------------------------------------------
c.cut("""\t/* THE TERRAIN HORIZON (lane HORIZON1, 2026-09-18), role 7, gathered ONCE""",
      """\tconst float hzTexel = ( hzLevel >= 0 )
\t\t? float( levels[hzLevel].dim ) * 4096.0f / float( content ) : 0.0f;
""", "bake: hzSheets/hzLevel/horizon.build")

c.sub("""\t\tconst bool hzHere = ( l == hzLevel ) && horizon.built();
\t\th.sheetCount = quint8( 3 + ( opts.height ? 1 : 0 ) + ( wantEmissive ? 1 : 0 )
\t\t\t+ ( hzHere ? hzSheets : 0 ) );""",
      "\t\th.sheetCount = quint8( 3 + ( opts.height ? 1 : 0 ) + ( wantEmissive ? 1 : 0 ) );",
      "bake: sheetCount")

c.cut("""\t\t/* THE HORIZON SHEETS, last and contiguous, which is the rule the""",
      """\t\t\t\t\tLODV_ROLE_HORIZON, 0 };
""", "bake: sheet descriptors")

c.sub("""\tauto writeTile = [&]( int lv, const LodgenVtStage & st ) -> bool {
\t\tconst int hs = ( lv == hzLevel && horizon.built() ) ? hzSheets : 0;
\t\t/* THE PAYLOAD AND THE HEADER ARE WRITTEN FROM ONE NUMBER. A level whose
\t\t * header declares horizon sheets and whose staging has none would write
\t\t * a tile shorter than the header says it is -- a file every reader sizes
\t\t * wrong and none can diagnose -- so it is refused here, by name. */
\t\tif ( hs && int( st.horizon.size() ) != hs )
\t\t\treturn fail( QString( "level %1 declares %2 horizon sheets and staged %3" )
\t\t\t\t.arg( lv ).arg( hs ).arg( st.horizon.size() ) );
\t\tconst QByteArray raw = lodgenVtEncodeTile( st, stored, mips, opts.height,
\t\t\twantEmissive, opts.coverInColor, hs );""",
      """\tauto writeTile = [&]( int lv, const LodgenVtStage & st ) -> bool {
\t\tconst QByteArray raw = lodgenVtEncodeTile( st, stored, mips, opts.height,
\t\t\twantEmissive, opts.coverInColor );""",
      "bake: writeTile")

c.cut("""\t\t\t/* THE HORIZON IS CAST, NEVER FILTERED. A parent's colour is the mean""",
      """\t\t\t\t\t\tprow[size_t( tx )].horizon );
\t\t\t\t}
""", "bake: parent-level cast")

c.sub("""\t\t\tif ( hzLevel == 0 && horizon.built() )
\t\t\t\thorizon.castTile( cellX0, cellY0, levels[0].dim, content, border,
\t\t\t\t\trow[size_t( tx )].horizon );
""", "", "bake: level-0 cast")

# --- the .lodm ------------------------------------------------------------
c.cut("""\t\t/* THE HORIZON, said in words a consumer can act on without reading this""",
      """\t\t\tt.insert( QStringLiteral( "horizon" ), QStringLiteral( "none" ) );
\t\t}
""", "lodm: horizon object")

c.sub("""\t\t\t// per level, because only ONE level carries the horizon sheets
\t\t\to.insert( QStringLiteral( "horizonSheets" ),
\t\t\t\t( l == hzLevel && horizon.built() ) ? hzSheets : 0 );
""", "", "lodm: per-level horizonSheets")

# --- the census -----------------------------------------------------------
c.cut("""\t\t/* THE HORIZON CENSUS. Every number is read back from the bytes that""",
      """\t\tfor ( const QString & t : horizon.refute.census( QStringLiteral( "horizonRefute" ) ) )
\t\t\tr << t;
""", "census: horizon words")

c.save()
print("OK" if not CHECK else "--check: nothing written")
