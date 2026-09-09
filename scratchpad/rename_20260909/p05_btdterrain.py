# Patch 5 -- src/btdterrain.{h,cpp} : the LAND route's extension and its two
# environment overrides follow the file to .lodl. C++ names unchanged (this
# lane owns these two files only for the extension/route rename).
import os
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")


class F:
    def __init__(self, p):
        self.p = p
        b = open(p, "rb").read()
        assert b.count(b"\r") == 0, p
        self.s = b.decode("utf-8")

    def sub(self, old, new, n=1):
        c = self.s.count(old)
        assert c == n, "%s: anchor count %d != %d for %r" % (self.p, c, n, old[:70])
        self.s = self.s.replace(old, new)

    def save(self):
        out = self.s.encode("utf-8")
        assert out.count(b"\r") == 0
        open(self.p, "wb").write(out)
        print("OK", self.p, len(out), "bytes, CR", out.count(b"\r"), "LF", out.count(b"\n"))


h = F("src/btdterrain.h")
h.sub(
    """ * .lodt — our own whole-worldspace landscape file (docs/LODGEN_BTD_FORMAT.md)""",
    """ * .lodl — our own whole-worldspace landscape file (docs/LODGEN_BTD_FORMAT.md)
 *
 * It was written and opened as `.lodt` until 2026-09-09; `.lodt` names the
 * terrain TEXTURE sheets now (src/io/lodvfile.h). The C++ names below still
 * say `lodt` and are internal: read every one of them as "the .lodl file".""")
h.sub(""" * The one thing a .lodt has that a .btd does not is PLANES.""",
      """ * The one thing a .lodl has that a .btd does not is PLANES.""")
h.sub("""//! Height view carries no vertex colours at all and is the same scene a .btd""",
      """//! Height view carries no vertex colours at all and is the same scene a .btd""")
h.sub("""//! Read the .lodt header and tables only. False (with *error set) when the
//! file is not a .lodt or does not describe itself consistently.""",
      """//! Read the .lodl header and tables only. False (with *error set) when the
//! file is not a .lodl or does not describe itself consistently. A terrain
//! TEXTURE file (.lodt) handed to this route is refused BY NAME, not by
//! "bad magic" -- src/lodtfile.cpp, LodtFile::open.""")
h.sub("""//! What a .lodt's header and tables say, read without inflating any block.""",
      """//! What a .lodl's header and tables say, read without inflating any block.""")
h.sub("""/*! One .lodt view: an inclusive cell rectangle, a detail level and a plane.""",
      """/*! One .lodl view: an inclusive cell rectangle, a detail level and a plane.""")
h.sub("""//! WW_LODT_REGION="x0,y0,x1,y1,lod[,plane]" — the no-dialog override, the same
//! shape WW_BTD_REGION has. WW_LODT_PLANE sets the plane on its own.""",
      """//! WW_LODL_REGION="x0,y0,x1,y1,lod[,plane]" — the no-dialog override, the same
//! shape WW_BTD_REGION has. WW_LODL_PLANE sets the plane on its own. The old
//! WW_LODT_* spellings are REFUSED loudly, never silently honoured.""")
h.sub("""//! WW_LODT_REGION is set, which is how harnesses and renders drive this.""",
      """//! WW_LODL_REGION is set, which is how harnesses and renders drive this.""")
h.save()

c = F("src/btdterrain.cpp")
c.sub(""" *  - a `.lodt` is OUR equivalent, written by the LOD generator and specified in""",
      """ *  - a `.lodl` is OUR equivalent, written by the LOD generator and specified in""")
c.sub(""" * The one thing a `.lodt` has that a `.btd` does not is PLANES. Heights are the
 * geometry either way; a `.lodt` view can paint the same surface with any one""",
      """ * The one thing a `.lodl` has that a `.btd` does not is PLANES. Heights are the
 * geometry either way; a `.lodl` view can paint the same surface with any one""")
c.sub(""" * .lodt
 * =========================================================================""",
      """ * .lodl  (written and opened as .lodt until bungo's 2026-09-09 ruling; the
 *         C++ names here are internal and did not move with the extension)
 * =========================================================================""")
c.sub("""	const QByteArray env = qgetenv( "WW_LODT_REGION" );""",
      """	/* The retired spellings are named, not ignored: a harness or a render
	 * still setting WW_LODT_* was written for the format this extension used
	 * to mean, and honouring it silently is exactly what the rename was meant
	 * to stop. Refusing here would leave the caller with no region at all, so
	 * this SAYS SO and then reads nothing from them. */
	for ( const char * old : { "WW_LODT_REGION", "WW_LODT_PLANE" } ) {
		if ( !qgetenv( old ).isEmpty() )
			qCritical().noquote() << QString( "REFUSED: %1 is retired -- the landscape "
				"file is .lodl now (.lodt names the terrain texture sheets). Set %2 "
				"instead; this run is ignoring it." )
				.arg( QLatin1String( old ) )
				.arg( QLatin1String( old ) == QLatin1String( "WW_LODT_REGION" )
					? "WW_LODL_REGION" : "WW_LODL_PLANE" );
	}
	const QByteArray env = qgetenv( "WW_LODL_REGION" );""")
c.sub("""	const QByteArray planeEnv = qgetenv( "WW_LODT_PLANE" );""",
      """	const QByteArray planeEnv = qgetenv( "WW_LODL_PLANE" );""")
c.sub("""	note << QString( "lodt %1: cells [%2,%3]..[%4,%5], %6 samples a cell \"""",
      """	note << QString( "lodl %1: cells [%2,%3]..[%4,%5], %6 samples a cell \"""")
# qCritical() needs QDebug
if '#include <QDebug>' not in c.s:
    c.sub('#include <QCoreApplication>\n', '#include <QCoreApplication>\n#include <QDebug>\n')
c.save()

for p in ("src/btdterrain.h", "src/btdterrain.cpp"):
    b = open(p, "rb").read().decode("utf-8")
    for i, ln in enumerate(b.split("\n"), 1):
        if ".lodt" in ln.lower() or "WW_LODT" in ln:
            print("  remaining in %s @%d: %s" % (p, i, ln.strip()))
