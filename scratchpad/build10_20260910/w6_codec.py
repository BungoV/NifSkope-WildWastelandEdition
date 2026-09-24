#!/usr/bin/env python3
"""Lane WATER6 -- the flow PNG adopts the DirectX normal-map convention.

R = +X, G = +Y toward the image BOTTOM, both centred on 128 with a scale of
127; B = the speed nibble, A = the confidence nibble.  Proved in numpy before
this ran: all 256 directions round-trip, 0 failures, the same as the old
mapping -- so the convention costs no precision.

Anchors match exactly once; CR asserted unchanged (LF-only file).
"""
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

E = [
    ("src/watercurves.cpp",
     "quint16 WaterCurveDoc::wordFromRgba( quint8 r, quint8 g, quint8 b, quint8 a )\n"
     "{\n"
     "\tif ( !a && !r && !g && !b )\n"
     "\t\treturn 0;\n"
     "\tconst double cx = double( r ) / 255.0 * 2.0 - 1.0;\n"
     "\tconst double cy = double( g ) / 255.0 * 2.0 - 1.0;\n"
     "\tdouble ang = std::atan2( cy, cx );\n",

     "/* THE CONVENTION (lane WATER6).  R and G carry the direction as a DirectX\n"
     " * normal map does: both channels are centred on 128 with a scale of 127,\n"
     " * R is +X (east), and G is +Y TOWARD THE IMAGE BOTTOM -- so a texel whose\n"
     " * water runs north is DARK green (1) and one whose water runs south is\n"
     " * bright (255).  Up to 2026-09-10 this tool wrote +green = north, which is\n"
     " * the OpenGL convention and the opposite of what Substance, Houdini and\n"
     " * every DirectX-era engine exporter write.  B is the speed nibble x 17 and\n"
     " * A the confidence nibble x 17.\n"
     " *\n"
     " * The mapping round-trips every one of the 256 directions exactly (gate\n"
     " * X5b, 65,536 words), and `tests/fixtures/flowmap_directx_4x4.png` is the\n"
     " * same rule written by a script that shares no code with this one (X5c). */\n"
     "quint16 WaterCurveDoc::wordFromRgba( quint8 r, quint8 g, quint8 b, quint8 a )\n"
     "{\n"
     "\tif ( !a && !r && !g && !b )\n"
     "\t\treturn 0;\n"
     "\tconst double cx = ( double( r ) - 128.0 ) / 127.0;\n"
     "\tconst double cy = -( ( double( g ) - 128.0 ) / 127.0 );   // +G is SOUTH\n"
     "\tdouble ang = std::atan2( cy, cx );\n"),

    ("src/watercurves.cpp",
     "\tconst double ang = double( word & 0xFF ) / 256.0 * kTwoPi;\n"
     "\tr = quint8( qBound( 0, int( std::lround( ( std::cos( ang ) + 1.0 ) * 0.5 * 255.0 ) ), 255 ) );\n"
     "\tg = quint8( qBound( 0, int( std::lround( ( std::sin( ang ) + 1.0 ) * 0.5 * 255.0 ) ), 255 ) );\n",

     "\tconst double ang = double( word & 0xFF ) / 256.0 * kTwoPi;\n"
     "\tr = quint8( qBound( 0, int( 128 + std::lround( 127.0 * std::cos( ang ) ) ), 255 ) );\n"
     "\tg = quint8( qBound( 0, int( 128 + std::lround( 127.0 * -std::sin( ang ) ) ), 255 ) );\n"),

    ("src/watercurves.cpp",
     "\t/* THE FLIPPED-GREEN TEST.  A flow map from another tool with +G = south\n"
     "\t * would import as a field mirrored about east-west, and nothing in the\n",

     "\t/* THE FLIPPED-GREEN TEST.  A flow map from another tool with +G = NORTH\n"
     "\t * (the OpenGL convention, and what this tool itself wrote before lane\n"
     "\t * WATER6) would import as a field mirrored about east-west, and nothing\n"
     "\t * in the\n"),

    ("src/watercurves.cpp",
     "\t\treturn fail( QStringLiteral( \"the green channel of %1 looks flipped: over %2 painted texels the \"\n"
     "\t\t\t\"map agrees with this file's flow at %3 as it is and at %4 with green mirrored. This tool \"\n"
     "\t\t\t\"writes +green = north; flip the channel and import again\" )\n",

     "\t\treturn fail( QStringLiteral( \"the green channel of %1 looks flipped: over %2 painted texels the \"\n"
     "\t\t\t\"map agrees with this file's flow at %3 as it is and at %4 with green mirrored. This tool \"\n"
     "\t\t\t\"writes +green = south, the DirectX convention (green grows toward the image bottom); \"\n"
     "\t\t\t\"flip the channel and import again\" )\n"),
]


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "--check"
    state = {}
    ok = True
    for path, old, new in E:
        full = os.path.join(ROOT, path)
        b = state.get(full)
        if b is None:
            b = open(full, "rb").read()
            state[full] = b
        c = b.count(old.encode("utf-8"))
        print("%-24s count=%d CR=%d  %r" % (path, c, b.count(b"\r"), old[:46]))
        if c != 1:
            ok = False
    if not ok:
        print("REFUSED")
        return 1
    if mode != "--apply":
        print("check ok (nothing written)")
        return 0
    for path, old, new in E:
        full = os.path.join(ROOT, path)
        b = state[full]
        assert b.count(old.encode("utf-8")) == 1
        state[full] = b.replace(old.encode("utf-8"), new.encode("utf-8"))
    for full, b in state.items():
        o = open(full, "rb").read()
        assert b.count(b"\r") == o.count(b"\r") == 0
        open(full, "wb").write(b)
        print("wrote %s  %d -> %d bytes, CR 0" % (os.path.relpath(full, ROOT), len(o), len(b)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
