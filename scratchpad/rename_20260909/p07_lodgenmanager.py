# Patch 7 -- src/lodgenmanager.cpp : the LOD Generation panel's visible strings
# follow the 2026-09-09 rename. objectNames and QSettings keys are NOT touched:
# src/nifskope_ui.cpp's WW_LODGEN_TEST finds the rows by objectName and this
# lane does not own that file, and renaming a settings key would silently
# reset bungo's own ticks.
import os
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")
P = "src/lodgenmanager.cpp"
b = open(P, "rb").read()
assert b.count(b"\r") == 0
s = b.decode("utf-8")


def sub(old, new, n=1):
    global s
    c = s.count(old)
    assert c == n, "anchor count %d != %d for %r" % (c, n, old[:80])
    s = s.replace(old, new)


sub(""" *  Whole-worldspace outputs (the .lodt landscape file, its AO-only refresh,""",
    """ *  Whole-worldspace outputs (the .lodl landscape file, its AO-only refresh,""")
sub("""		lodtCheck = new QCheckBox( tr( "Landscape file (.lodt)" ), page );""",
    """		lodtCheck = new QCheckBox( tr( "Landscape file (.lodl)" ), page );""")
sub("""			"the whole worldspace in one file: Terrain\\\\<worldspace>.lodt." ) );""",
    """			"the whole worldspace in one file: Terrain\\\\<worldspace>.lodl." ) );""")
sub("""		vtCheck = new QCheckBox( tr( "Terrain virtual texture (.lodv)" ), page );""",
    """		vtCheck = new QCheckBox( tr( "Terrain virtual texture (.lodt)" ), page );""")
sub("""with the viewport set to an orthographic top view. The .lodt is not a mesh; it\\n""",
    """with the viewport set to an orthographic top view. The .lodl is not a mesh; it\\n""")
sub("""			// the stock engine has no .lodv reader and no VT sampler""",
    """			// the stock engine has no .lodt texture-pyramid reader and no VT sampler""")
sub("""				parts << tr( "refresh the AO plane of Terrain\\\\%1.lodt" ).arg( ws );""",
    """				parts << tr( "refresh the AO plane of Terrain\\\\%1.lodl" ).arg( ws );""")
sub("""				parts << tr( "Terrain\\\\%1.lodt (about %2 MB)" ).arg( ws )""",
    """				parts << tr( "Terrain\\\\%1.lodl (about %2 MB)" ).arg( ws )""")
sub("""				parts << tr( "Terrain\\\\%1.VT.*.lodv (%2 levels, %3 tiles, about %4 GB)" ).arg( ws )""",
    """				parts << tr( "Terrain\\\\%1.VT.*.lodt (%2 levels, %3 tiles, about %4 GB)" ).arg( ws )""")
sub("""	 *  what was measured (a .lodt is about 1 KB a cell; a native heightmap is""",
    """	 *  what was measured (a .lodl is about 1 KB a cell; a native heightmap is""")
sub("""				const QString path = job.outDir + QStringLiteral( "/Terrain/" ) + w.worldspaceEdid() + QStringLiteral( ".lodt" );""",
    """				const QString path = job.outDir + QStringLiteral( "/Terrain/" ) + w.worldspaceEdid() + QStringLiteral( ".lodl" );""")
sub("""				report += QStringLiteral( ".lodt: " ) + err.section( QChar( '\\n' ), 0, 0 ) + QChar( '\\n' );""",
    """				report += QStringLiteral( ".lodl: " ) + err.section( QChar( '\\n' ), 0, 0 ) + QChar( '\\n' );""")

out = s.encode("utf-8")
assert out.count(b"\r") == 0
# the objectNames and settings keys must NOT have moved
assert s.count('"LodgenLodtCheck"') == 1 and s.count('"LodGeneration/lodt"') == 2, "identity keys moved"
open(P, "wb").write(out)
print("OK", P, len(out), "bytes, CR", out.count(b"\r"), "LF", out.count(b"\n"))
for i, ln in enumerate(s.split("\n"), 1):
    if ".lodt" in ln.lower() or ".lodv" in ln.lower():
        print("  check @%d: %s" % (i, ln.strip()))
