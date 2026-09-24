"""PBRR0 patch: R0 render pins + WW_PBRM_CENSUS. Refuses unless every anchor is exactly-once.
Run with --check to verify anchors without writing."""
import sys, os
ROOT = r"E:\Projects\NifskopeWildWastelandEdition"
LANE = os.path.join(ROOT, "scratchpad", "pbrr0_20260924")
CHECK = "--check" in sys.argv

def rd(p):
    return open(os.path.join(ROOT, p), "rb").read()

def snip(n):
    return open(os.path.join(LANE, n), "rb").read().replace(b"\r\n", b"\n")

edits = {}

def rep(path, old, new, count=1):
    b = edits.get(path) or rd(path)
    n = b.count(old)
    if n != count:
        sys.exit("REFUSED %s: anchor count %d != %d: %r" % (path, n, count, old[:80]))
    edits[path] = b.replace(old, new)

# --- glproperty.h
H = "src/gl/glproperty.h"
a = b"void setPbrmMode( int mode );\nint pbrmMode();\n"
rep(H, a, a + snip("snip_hdecl.h"))
a = b"\tQHash<int, QString> wwTextureOverride;\n\nprotected:\n\tShaderFlags::SF1 flags1"
rep(H, a, b"\tQHash<int, QString> wwTextureOverride;\n\n"
    b"\t//! The resolved-state fields of one WW_PBRM_CENSUS row (lane PBRR0): the\n"
    b"\t//! route that served the shape, its file, the .pbrm envelope, the refusal.\n"
    b"\tQString wwPbrmCensusFields( const QString & servedProgram ) const;\n\n"
    b"protected:\n\tShaderFlags::SF1 flags1")

# --- glproperty.cpp
C = "src/gl/glproperty.cpp"
a = b'#include "renderer.h"\n'
rep(C, a, a + b"\n#include <QDir>\n#include <QFile>\n#include <QProcessEnvironment>\n#include <QSet>\n#include <QTextStream>\n")
a = b"static bool pbrmAutoReplaceCached = true;\n"
rep(C, a, snip("snip_pins.cpp") + a)
rep(C, b"bool pbrmAutoReplaceEnabled()\n{\n\treturn pbrmFeatureEnabled && pbrmAutoReplaceCached;\n}",
    b"bool pbrmAutoReplaceEnabled()\n{\n"
    b"\t// WW_PBRM_AUTOREPLACE pins the menu value for a harness run (lane PBRR0);\n"
    b"\t// the feature gate still has the last word.\n"
    b"\tconst int pin = wwEnvAutoReplace();\n"
    b"\tconst bool on = ( pin >= 0 ) ? ( pin != 0 ) : pbrmAutoReplaceCached;\n"
    b"\treturn pbrmFeatureEnabled && on;\n}")
rep(C, b"\t// Force Legacy while the feature is gated, whatever is stored.\n"
       b"\treturn pbrmFeatureEnabled ? pbrmModeCached : int( PbrmModeLegacy );",
    b"\t// WW_PBRM_MODE pins the menu value for a harness run (lane PBRR0).\n"
    b"\t// Force Legacy while the feature is gated, whatever is stored or pinned.\n"
    b"\tconst int pin = wwEnvPbrmMode();\n"
    b"\tconst int m = ( pin >= 0 ) ? pin : pbrmModeCached;\n"
    b"\treturn pbrmFeatureEnabled ? m : int( PbrmModeLegacy );")
a = b"void BSShaderLightingProperty::setSFMaterial( const QString & mat_name )\n"
rep(C, a, snip("snip_census.cpp").lstrip(b"\n") + b"\n" + a)

# --- renderer.cpp
R = "src/gl/renderer.cpp"
rep(R, b"static NifSkopeOpenGLContext::Program * wwProgramCensus( const NifModel * nif, Shape * mesh,\n\tint msn,",
       b"static NifSkopeOpenGLContext::Program * wwProgramCensus( const NifModel * nif, Shape * mesh,\n"
       b"\tconst BSShaderLightingProperty * wwSp, const char * wwKind,\n\tint msn,")
rep(R, b"\tstatic int\tarmed = -1;\n\tstatic QString\tcensusPath;\n",
       b"\t/* WW_PBRM_CENSUS (lane PBRR0) rides the same exits: every return of\n"
       b"\t * setupProgram passes through here with the program it actually bound,\n"
       b"\t * so the route it prints is the served one. Pick renders (wwKind null) are skipped. */\n"
       b"\tif ( wwPbrmCensusArmed() && mesh && wwKind ) {\n"
       b"\t\tQString\tserved( \"(none)\" );\n"
       b"\t\tif ( program )\n"
       b"\t\t\tserved = QString::fromLatin1( program->name.data(), qsizetype( program->name.length() ) );\n"
       b"\t\twwPbrmCensus( mesh->getName(), wwKind, wwSp, served );\n"
       b"\t}\n\n"
       b"\tstatic int\tarmed = -1;\n\tstatic QString\tcensusPath;\n")
a = b"\t\t? int( mesh->bslsp->isST( ShaderFlags::ST_WorldMap4 ) ) : 0 );\n"
rep(R, a, a + b"\tconst BSShaderLightingProperty *\twwSp = mesh->bssp;\n"
              b"\t// nullptr during a pick render: the PBRM census skips those.\n"
              b"\tconst char *\twwKind = mesh->scene->selecting ? nullptr\n"
              b"\t\t: ( mesh->bslsp ? \"lit\" : ( mesh->bsesp ? \"effect\" : \"other\" ) );\n")
rep(R, b"wwProgramCensus( nif, mesh, wwMsn, wwLodLand,", b"wwProgramCensus( nif, mesh, wwSp, wwKind, wwMsn, wwLodLand,", count=6)

# --- glparticles.cpp
P = "src/gl/glparticles.cpp"
a = b"\t\tBSShaderLightingProperty * shaderProp = findProperty<BSShaderLightingProperty>();\n"
rep(P, a, a + b"\t\t// WW_PBRM_CENSUS (lane PBRR0): particles are served by particles.prog, legacy.\n"
              b"\t\tif ( wwPbrmCensusArmed() )\n"
              b"\t\t\twwPbrmCensus( getName(), \"particles\", shaderProp, QStringLiteral( \"particles.prog\" ) );\n")

# --- nifskope_ui.cpp (the shot hook only: 7-tab continuation line)
U = "src/nifskope_ui.cpp"
rep(U, b"\t\t\t\t\t\t\t|| qEnvironmentVariableIntValue( \"WW_RENDER_REFRACTION\" ) != 0;\n\t\t\t\t\t\tsc->showParticles = true;\n",
       b"\t\t\t\t\t\t\t|| qEnvironmentVariableIntValue( \"WW_RENDER_REFRACTION\" ) != 0;\n"
       b"\t\t\t\t\t\t// WW_RENDER_PARTICLES=0 turns them off for a paired capture (lane\n"
       b"\t\t\t\t\t\t// PBRR0); unset keeps the old forced-on.\n"
       b"\t\t\t\t\t\tsc->showParticles = wwRenderParticlesPin();\n")

for p, b in edits.items():
    old = rd(p)
    assert old.count(b"\r") == b.count(b"\r"), p + " CR count changed"
    print("%-26s %8d -> %8d bytes  CR %d" % (p, len(old), len(b), b.count(b"\r")))
    if not CHECK:
        open(os.path.join(ROOT, p), "wb").write(b)
print("CHECK ONLY" if CHECK else "WRITTEN")
