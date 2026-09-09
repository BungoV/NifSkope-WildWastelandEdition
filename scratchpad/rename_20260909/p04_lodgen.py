# Patch 4 -- src/lodgen.cpp
#   (a) the terrain virtual texture is written as .lodt (was .lodv);
#   (b) the three wrong hex comments beside correct vertex descriptors
#       (WRITER_CHANGES_NEEDED.md item 1). VALUES UNCHANGED -- comments only;
#   (c) `--candidates trees` is fixed in nifcli.cpp, not here.
import os
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")
P = "src/lodgen.cpp"
b = open(P, "rb").read()
assert b.count(b"\r") == 0
s = b.decode("utf-8")
before_values = (s.count("52776558133763ULL"), s.count("474989027590661ULL"),
                 s.count("1037939064898054ULL"))
assert before_values == (1, 1, 1), before_values


def sub(old, new, n=1):
    global s
    c = s.count(old)
    assert c == n, "anchor count %d != %d for %r" % (c, n, old[:70])
    s = s.replace(old, new)


# --- (a) the container's name
sub("/* ============ rung 3b: the terrain virtual texture (.lodv) ============ *",
    "/* ============ rung 3b: the terrain virtual texture (.lodt) ============ *")
sub('\t\t\t"with a terminator, so this worldspace cannot be named in a .lodv" )',
    '\t\t\t"with a terminator, so this worldspace cannot be named in a .lodt" )')
sub('QString( "%1/%2.VT.%3.lodv" ).arg( dir ).arg( ws ).arg( levels[l].dim )',
    'QString( "%1/%2.VT.%3.lodt" ).arg( dir ).arg( ws ).arg( levels[l].dim )')
sub('QString( "Terrain%1%2.VT.%3.lodv" ).arg( QChar( 92 ) ).arg( ws ).arg( levels[l].dim )',
    'QString( "Terrain%1%2.VT.%3.lodt" ).arg( QChar( 92 ) ).arg( ws ).arg( levels[l].dim )')

# --- (b) the three hex comments. Decoded through
# BSVertexDesc::ResetAttributeOffsets( 130 ), src/data/niftypes.h:1934-1979;
# the full table is at the end of docs/LODGEN_VERTEX_PACKING.md.
sub("constexpr std::uint64_t LAND_VERTEX_DESC = 52776558133763ULL;   // 0x300000000303",
    "constexpr std::uint64_t LAND_VERTEX_DESC = 52776558133763ULL;   // 0x0000300000000203")
sub("constexpr std::uint64_t OBJ_VERTEX_DESC = 474989027590661ULL;       // 0x1B00000650405",
    "constexpr std::uint64_t OBJ_VERTEX_DESC = 474989027590661ULL;       // 0x0001B00000430205")
sub("constexpr std::uint64_t OBJ_VERTEX_DESC_COLORS = 1037939064898054ULL; // 0x3B00000650406",
    "constexpr std::uint64_t OBJ_VERTEX_DESC_COLORS = 1037939064898054ULL; // 0x0003B00005430206\n"
    "/* The THIRD object profile -- identity plus the object channels, stride 32 --\n"
    " * is 0x0013F07006543208. It is computed into `objDesc` rather than declared,\n"
    " * so there is nowhere else to read it off. */")

out = s.encode("utf-8")
assert out.count(b"\r") == 0
# the values are the contract; only their comments moved
assert (s.count("52776558133763ULL"), s.count("474989027590661ULL"),
        s.count("1037939064898054ULL")) == before_values
open(P, "wb").write(out)
print("OK", P, len(out), "bytes, CR", out.count(b"\r"), "LF", out.count(b"\n"))
for i, ln in enumerate(s.split("\n"), 1):
    if ".lodv" in ln.lower():
        print("  remaining .lodv @%d: %s" % (i, ln.strip()))
