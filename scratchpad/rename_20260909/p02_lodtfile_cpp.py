# Patch 2 -- src/lodtfile.cpp : write/read the LAND file as .lodl, refuse a
# terrain TEXTURE file (.lodt, magic LDTX) BY NAME, rename the env fallback.
import os
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")
P = "src/lodtfile.cpp"
b = open(P, "rb").read()
assert b.count(b"\r") == 0
s = b.decode("utf-8")


def sub(old, new, n=1):
    global s
    c = s.count(old)
    assert c == n, "anchor count %d != %d for %r" % (c, n, old[:70])
    s = s.replace(old, new)


# --- the texture reader's magic, for the cross-refusal
sub('#include "lodtfile.h"\n#include "esmdata.h"\n',
    '#include "lodtfile.h"\n#include "esmdata.h"\n\n'
    '/* Only for LODTEX_MAGIC: this reader must be able to NAME a terrain\n'
    ' * TEXTURE file when it is handed one, because `.lodt` meant THIS format\n'
    ' * until 2026-09-09 and means that one now. */\n'
    '#include "io/lodvfile.h"\n')

# --- the constants: the land magic now lives in the header
sub("""constexpr quint32 LODT_MAGIC = 0x54444F4CU;   // 'LODT' little-endian
/* Version 2 appended""",
    """/* LODL_MAGIC is in lodtfile.h -- src/io/lodvfile.cpp refuses it by name.
 * Version 2 appended""")
sub("constexpr quint32 LODT_VERSION = 2;\nconstexpr quint32 LODT_VERSION_MIN = 1;\n"
    "constexpr qsizetype LODT_HEADER_V1 = 0x98;\nconstexpr qsizetype LODT_HEADER_V2 = 0xA0;",
    "constexpr quint32 LODL_VERSION = 2;\nconstexpr quint32 LODL_VERSION_MIN = 1;\n"
    "constexpr qsizetype LODL_HEADER_V1 = 0x98;\nconstexpr qsizetype LODL_HEADER_V2 = 0xA0;")

# every remaining use of the four renamed constants
for a, c in (("LODT_VERSION_MIN", 4), ("LODT_VERSION", 11),
             ("LODT_HEADER_V1", 5), ("LODT_HEADER_V2", 3)):
    got = s.count(a)
    assert got == c, "%s appears %d times, expected %d" % (a, got, c)
s = s.replace("LODT_VERSION_MIN", "LODL_VERSION_MIN")
s = s.replace("LODT_VERSION", "LODL_VERSION")      # after _MIN, so no overlap
s = s.replace("LODT_HEADER_V1", "LODL_HEADER_V1")
s = s.replace("LODT_HEADER_V2", "LODL_HEADER_V2")
# WW_LODL_VERSION is what the two "WW_LODT_VERSION" strings became; that is the
# new environment name and it is deliberate.
assert s.count('"WW_LODL_VERSION"') == 2, s.count('"WW_LODL_VERSION"')

# --- the environment fallback, with the old name refused rather than ignored
sub("""	quint32 version = quint32( opts.headerVersion );
	if ( qEnvironmentVariableIsSet( "WW_LODL_VERSION" ) )
		version = quint32( qEnvironmentVariableIntValue( "WW_LODL_VERSION" ) );
	if ( version < LODL_VERSION_MIN || version > LODL_VERSION )""",
    """	quint32 version = quint32( opts.headerVersion );
	/* The old spelling is REFUSED, not ignored. A run that still says
	 * WW_LODT_VERSION was written for the format this file used to be called,
	 * and silently writing version 2 bytes for it is the failure the rename
	 * was supposed to make impossible. */
	if ( qEnvironmentVariableIsSet( "WW_LODT_VERSION" ) )
		return fail( QStringLiteral( "WW_LODT_VERSION is retired: the landscape file "
			"is .lodl now (.lodt names the terrain texture sheets) -- set "
			"WW_LODL_VERSION instead" ) );
	if ( qEnvironmentVariableIsSet( "WW_LODL_VERSION" ) )
		version = quint32( qEnvironmentVariableIntValue( "WW_LODL_VERSION" ) );
	if ( version < LODL_VERSION_MIN || version > LODL_VERSION )""")

# --- the header write
sub("\th.u32( LODT_MAGIC );", "\th.u32( LODL_MAGIC );")

# --- the written path
sub("""	const QString path = dir + QStringLiteral( "/" ) + edid + QStringLiteral( ".lodt" );""",
    """	const QString path = dir + QStringLiteral( "/" ) + edid + QStringLiteral( ".lodl" );""")

# --- the reader's refusals, both of them by name
sub("""	if ( buf.size() < LODL_HEADER_V1 )
		return fail( QStringLiteral( "too short to be a .lodt" ) );
	if ( rd<quint32>( buf, 0 ) != LODT_MAGIC )
		return fail( QStringLiteral( "not a .lodt (bad magic)" ) );""",
    """	if ( buf.size() < LODL_HEADER_V1 )
		return fail( QStringLiteral( "too short to be a .lodl landscape file" ) );
	{
		/* NAME the other format rather than saying "bad magic". `.lodt` was
		 * this format's own extension until 2026-09-09 and now belongs to the
		 * terrain texture sheets, so being handed one is the expected mistake,
		 * not an exotic one. */
		const quint32 magic = rd<quint32>( buf, 0 );
		if ( magic == LODTEX_MAGIC )
			return fail( QStringLiteral( "refused: %1 is a terrain TEXTURE file "
				"(.lodt, magic LDTX), not a .lodl landscape file" )
				.arg( QFileInfo( path ).fileName() ) );
		if ( magic != LODL_MAGIC )
			return fail( QStringLiteral( "not a .lodl landscape file (bad magic)" ) );
	}""")

out = s.encode("utf-8")
assert out.count(b"\r") == 0
for i, ln in enumerate(s.split("\n"), 1):
    if ".lodt" in ln.lower() and "LODT_VERSION" not in ln:
        print("  remaining .lodt @%d: %s" % (i, ln.strip()))
open(P, "wb").write(out)
print("OK", P, len(out), "bytes, CR", out.count(b"\r"), "LF", out.count(b"\n"))
