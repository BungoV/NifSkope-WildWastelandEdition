#!/usr/bin/env python3
"""HORIZONOUT: the viewer loses `horizon`, `horizonbin` and `WW_SUN`.

`identity`, `placement`, `sky` and `scrappable` stay. `scrappable` keeps its
magenta-yes / grey-no drawing and its note line; its note line stops naming a
switch that no longer exists.
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
        print("%-16s %2d edits, %d -> %d chars, %d -> %d lines (CR %d -> %d)"
              % (name, self.n, len(self.orig), len(self.d),
                 self.orig.count("\n"), self.d.count("\n"),
                 self.cr, self.d.count("\r")))
        if self.d.count("\r") != self.cr:
            sys.exit("REFUSED: %s CR count moved" % name)
        if not CHECK:
            with open(self.path, "wb") as f:
                f.write(self.d.encode("utf-8"))


# ===========================================================================
h = F("src/lodinative.h")

h.cut("""\t/*! v8: SHADOW. Objects and terrain flat, lit where `WW_SUN`'s elevation""",
      """\tHorizonBin,""", "LodlChannel::Horizon + HorizonBin")

h.cut("""/* ---- the sun, and the one softening knob (lane HORIZON1, 2026-09-18) -------""",
      """inline float lodlHorizonGrey( float lit ) { return LODL_HORIZON_DARK + ( 1.0f - LODL_HORIZON_DARK ) * lit; }""",
      "the sun + the horizon helpers",
      """/* THE SUN (`WW_SUN`) AND THE HORIZON CHANNELS WERE REMOVED 2026-09-19 by lane
 * HORIZONOUT. They existed to draw the baked far shadow the `.lodi` v8 stream
 * carried, and that route was dropped on bungo's word the same day: the far
 * shadow map keys on the `.lodi` GROUP id now, which is what the `identity`
 * channel draws. `release/NifSkope.before_horizonout.exe` still has both
 * channels and still bakes the stream they read. The two numbers that ended
 * the route are in docs/LODGEN_NATIVE_LODO_LODI.md's history paragraph. */
""")
h.save()

# ===========================================================================
c = F("src/lodinative.cpp")

c.sub('#include "lodghorizon.h"\n', "", "include lodghorizon.h")

c.sub("""\t{ "horizon", LodlChannel::Horizon },
\t{ "horizonbin", LodlChannel::HorizonBin },
""", "", "channel table rows")

c.sub("""\t/* ONE name may carry an argument: `horizonbin=<n>`. The split happens here
\t * and nowhere else, so both halves of the viewer get the same bin from the
\t * same string, and a bin that will not parse refuses the WHOLE name rather
\t * than quietly drawing bin 0. */
\tconst int eq = env.indexOf( QLatin1Char( '=' ) );
\tconst QString head = ( eq >= 0 ) ? env.left( eq ) : env;
\tfor ( const ChannelName & c : CHANNEL_NAMES ) {
\t\tif ( head != QLatin1String( c.name ) )
\t\t\tcontinue;
\t\tif ( c.channel == LodlChannel::HorizonBin ) {
\t\t\tbool ok = false;
\t\t\tconst int n = env.mid( eq + 1 ).toInt( &ok );
\t\t\tif ( eq < 0 || !ok || n < 0 || n > 63 )
\t\t\t\treturn LodlChannel::None;
\t\t\tif ( bin )
\t\t\t\t*bin = n;
\t\t} else if ( eq >= 0 ) {
\t\t\treturn LodlChannel::None;       // an argument on a name that takes none
\t\t}
\t\treturn c.channel;
\t}""",
    """\t/* NO CHANNEL TAKES AN ARGUMENT any more -- `horizonbin=<n>` was the only
\t * one and it went with the baked-horizon route (lane HORIZONOUT). The split
\t * stays because the REFUSAL is the point: a name with an argument on it is
\t * refused whole, so a stale `WW_LODL_CHANNEL=horizonbin=3` in a script draws
\t * nothing and is named in the note line, rather than quietly drawing the
\t * default picture. */
\tconst int eq = env.indexOf( QLatin1Char( '=' ) );
\tconst QString head = ( eq >= 0 ) ? env.left( eq ) : env;
\tfor ( const ChannelName & c : CHANNEL_NAMES ) {
\t\tif ( head != QLatin1String( c.name ) )
\t\t\tcontinue;
\t\tif ( eq >= 0 )
\t\t\treturn LodlChannel::None;       // an argument on a name that takes none
\t\treturn c.channel;
\t}""",
    "lodlChannelFromEnv argument split")

c.cut("""bool lodlSunFromEnv( float * azimuthDeg, float * elevationDeg, QString * given )""",
      """\treturn c * c * ( 3.0f - 2.0f * c );     // smoothstep, the same curve both halves use
}
""", "lodlSunFromEnv + lodlHorizon* helpers")

c.sub("""\t\tall << ( n.channel == LodlChannel::HorizonBin
\t\t\t? QString( QLatin1String( n.name ) ) + QStringLiteral( "=<n>" )
\t\t\t: QString( QLatin1String( n.name ) ) );""",
      "\t\tall << QString( QLatin1String( n.name ) );",
      "lodlChannelNames")

# --- the draw ------------------------------------------------------------
c.cut("""\t/* v8 (lane HORIZON1): the per-vertex HORIZON stream, gated on what was READ""",
      """\t\t&& !table.vertexHorizonFirst.empty() && !table.vertexHorizon.empty();""",
      "horizon locals")

c.sub("""\t\t\t\t\tnb.withColour = wantAo || objectChannel || skyPerVertex || horizonPerVertex;""",
      "\t\t\t\t\tnb.withColour = wantAo || objectChannel || skyPerVertex;",
      "bucket withColour")

c.cut("""\t/* v8 read-back: the DEGREES that went into the compare, never the byte the""",
      """\t\thzVerts++;
\t};
""", "hz accumulators")

c.cut("""\t\t\t/* v8: the per-vertex HORIZON slice. The same shape as the AO and sky""",
      """\t\t\t\t\t\thzMismatch++;
\t\t\t\t\t}
\t\t\t\t}
\t\t\t}
""", "per-vertex horizon slice")

c.cut("""\t\t\t\t\t} else if ( horizonPerVertex ) {""",
      """\t\t\t\t\t\t\t\thzLitVerts++;
\t\t\t\t\t\t}
""", "the horizon draw arm")

c.cut("""\t/* v8, and every number in it READ BACK from what went into the buffer: the""",
      """\t\t\t.arg( double( 360.0f / float( horizonAz > 0 ? horizonAz : 1 ) ), 0, 'f', 1 );""",
      "horizon note lines")

c.sub("""\t\t\t\t"not because nothing is scrappable; re-bake with --horizon-scrappable" )""",
      """\t\t\t\t"not because nothing is scrappable; re-bake with --scrappable" )""",
      "scrappable note line switch name")

c.save()
print("--check: nothing written" if CHECK else "OK")
