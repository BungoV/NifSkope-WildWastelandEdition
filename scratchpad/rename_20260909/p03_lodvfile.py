# Patch 3 -- src/io/lodvfile.{h,cpp} : the terrain TEXTURE sheets become .lodt
# and take a NEW magic, LDTX, so the land format they displaced can be refused
# by name in both directions (bungo 2026-09-09).
import os
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")


def load(p):
    b = open(p, "rb").read()
    assert b.count(b"\r") == 0, p
    return b.decode("utf-8")


class F:
    def __init__(self, p):
        self.p = p
        self.s = load(p)

    def sub(self, old, new, n=1):
        c = self.s.count(old)
        assert c == n, "%s: anchor count %d != %d for %r" % (self.p, c, n, old[:70])
        self.s = self.s.replace(old, new)

    def save(self):
        out = self.s.encode("utf-8")
        assert out.count(b"\r") == 0
        open(self.p, "wb").write(out)
        print("OK", self.p, len(out), "bytes, CR", out.count(b"\r"), "LF", out.count(b"\n"))


# ------------------------------------------------------------------ header
h = F("src/io/lodvfile.h")
h.sub(
    """/*! `.lodv` v1 — one level of a terrain virtual texture.
 *
 *  The format contract is docs/LODGEN_TERRAIN_VT.md; this header is the
 *  layout and nothing else.""",
    """/*! `.lodt` v1 — one level of a terrain virtual texture: the terrain TEXTURE
 *  sheets, per level.
 *
 *  THE NAME MOVED (bungo's ruling, 2026-09-09). This container was written as
 *  `.lodv` until then, and `.lodt` belonged to the whole-worldspace LANDSCAPE
 *  file, which is `.lodl` now. `.lodt` is therefore REPURPOSED, not merely
 *  renamed, and that is why the magic changed with it: LODTEX_MAGIC is `LDTX`
 *  where the old container's was `LODV`, and the landscape file's is still
 *  `LODT`. Each of the three is refused BY NAME by the other reader, so a
 *  yesterday's file opened through today's route says what it actually is
 *  instead of misparsing or saying only "bad magic". The C++ names here --
 *  LodvWriter, lodvValidate, LODV_ROLE_* -- were NOT renamed with the format:
 *  they are internal and appear in no file on disk and in no command.
 *
 *  The format contract is docs/LODGEN_TERRAIN_VT.md; this header is the
 *  layout and nothing else.""",
)
h.sub(
    """ *   * NORTH-UP row order, in the tile table and inside every payload. Two
 *     conventions are live in this codebase (.lodt is row-0-SOUTH) and the""",
    """ *   * NORTH-UP row order, in the tile table and inside every payload. Two
 *     conventions are live in this codebase (.lodl is row-0-SOUTH) and the""",
)
h.sub(
    """ *  The magic is deliberately neither `DDS ` nor `LODT`: a wrong-but-plausible
 *  parse is worse than a refusal. */""",
    """ *  The magic is deliberately neither `DDS ` nor `LODT` (the landscape file's,
 *  which this extension used to name) nor `LODV` (this container's own, before
 *  the 2026-09-09 rename): a wrong-but-plausible parse is worse than a
 *  refusal. */

/*! The terrain texture container's magic, first four bytes, little-endian:
 *  `LDTX`. It is NEW as of 2026-09-09 and it is the whole reason the `.lodt`
 *  extension can be repurposed safely -- src/lodtfile.cpp refuses a file
 *  carrying it by name, and lodvValidate() refuses LODL_MAGIC by name. */
constexpr quint32 LODTEX_MAGIC = 0x5854444CU;   // 'L','D','T','X' little-endian
//! The retired `.lodv` magic, kept ONLY so a stale container is named, not guessed.
constexpr quint32 LODTEX_MAGIC_RETIRED_LODV = 0x56444F4CU;   // 'L','O','D','V'
""",
)
h.save()

# ------------------------------------------------------------------ writer/reader
c = F("src/io/lodvfile.cpp")
c.sub(
    '#include "lodvfile.h"\n\n#include <QFile>\n',
    '#include "lodvfile.h"\n\n'
    '/* Only for LODL_MAGIC: this validator must be able to NAME the LANDSCAPE\n'
    ' * file when it is handed one, because `.lodt` meant that format until\n'
    ' * 2026-09-09 and means this one now. */\n'
    '#include "lodtfile.h"\n\n#include <QFile>\n',
)
c.sub(
    " * by the CLI and by anything that opens a .lodv later, because a rule that",
    " * by the CLI and by anything that opens a .lodt later, because a rule that",
)
c.sub(
    "constexpr quint32 LODV_MAGIC = 0x56444F4CU;      // 'L','O','D','V' little-endian\n",
    "/* LODTEX_MAGIC ('LDTX') is in lodvfile.h, because src/lodtfile.cpp refuses\n"
    " * it by name; LODL_MAGIC ('LODT') is in lodtfile.h for the same reason. */\n",
)
c.sub("\tput32( h + 0x00, LODV_MAGIC );", "\tput32( h + 0x00, LODTEX_MAGIC );")
c.sub(
    '\t\t\t"with a terminator, so this worldspace cannot be named in a .lodv" )',
    '\t\t\t"with a terminator, so this worldspace cannot be named in a .lodt" )',
)
c.sub(
    """	if ( get32( h + 0x00 ) != LODV_MAGIC )                               // rule 2
		return fail( QStringLiteral( "refused: magic is not LODV" ) );""",
    """	if ( get32( h + 0x00 ) != LODTEX_MAGIC ) {                           // rule 2
		/* NAME what the file actually is. `.lodt` named the LANDSCAPE format
		 * until 2026-09-09 and `.lodv` named this one, so both are files a
		 * reader will really be handed, and "bad magic" would tell whoever is
		 * holding one nothing about which mistake they made. */
		const quint32 m = get32( h + 0x00 );
		if ( m == LODL_MAGIC )
			return fail( QStringLiteral( "refused: this is the whole-worldspace LANDSCAPE "
				"file (magic LODT), which is called .lodl since 2026-09-09; .lodt names "
				"the terrain texture sheets now -- open it as .lodl" ) );
		if ( m == LODTEX_MAGIC_RETIRED_LODV )
			return fail( QStringLiteral( "refused: this is a retired .lodv container "
				"(magic LODV); the terrain texture sheets are .lodt with magic LDTX "
				"since 2026-09-09 -- re-bake it" ) );
		return fail( QStringLiteral( "refused: magic is not LDTX" ) );
	}""",
)
c.sub('\tkv( "magic", QStringLiteral( "LODV" ) );',
      '\tkv( "magic", QStringLiteral( "LDTX" ) );')
c.save()

for p in ("src/io/lodvfile.h", "src/io/lodvfile.cpp"):
    for i, ln in enumerate(load(p).split("\n"), 1):
        if ".lodv" in ln.lower():
            print("  remaining .lodv in %s @%d: %s" % (p, i, ln.strip()))
