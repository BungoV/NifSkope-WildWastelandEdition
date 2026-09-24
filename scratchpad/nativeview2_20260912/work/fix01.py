#!/usr/bin/env python
"""Lane NATIVEVIEW2's three edits, applied as exact byte splices (never a
heredoc: this tree is LF-only and a heredoc arrives CRLF).

  A  res/shaders/fo4_default.frag  -- the model-space normal branch
  B  src/gl/renderer.cpp           -- feed it `hasModelSpaceNormals`
  C  src/gl/renderer.cpp           -- WW_PROGRAM_CENSUS, the written counter

Run with --check to see the anchors resolve without writing.
"""
import sys

ROOT = "E:/Projects/NifskopeWildWastelandEdition"
FRAG = ROOT + "/res/shaders/fo4_default.frag"
REND = ROOT + "/src/gl/renderer.cpp"


def rd(p):
    b = open(p, "rb").read()
    assert b.count(b"\r\n") == 0, (p, "CRLF already present")
    return b.decode("utf-8")


def wr(p, s):
    open(p, "wb").write(s.encode("utf-8"))


def once(s, old, name):
    n = s.count(old)
    assert n == 1, "%s: anchor found %d times" % (name, n)
    return n


# ------------------------------------------------------------------ A: frag
FRAG_UNI_OLD = "uniform bool greyscaleColor;\n"
FRAG_UNI_NEW = """uniform bool greyscaleColor;

// Shader Flags 1 bit 12 (SLSF1_Model_Space_Normals): the bound normal map is an
// `_msn`, a MODEL-space map, not a tangent-space one.
uniform bool hasModelSpaceNormals;
// model -> view rotation, the same row-major uniform the vertex stage uses for
// the normal, tangent and bitangent.  Declared here because the model-space
// path has no tangent frame to ride on.
uniform mat3 normalMatrix;
"""

FRAG_N_OLD = """	vec3 normal = normalMap.rgb * 2.0 - 1.0;
	// Calculate missing blue channel
	normal.b = sqrt(max(1.0 - dot(normal.rg, normal.rg), 0.0));
	normal = normalize( btnMatrix_norm * normal );
	if ( !gl_FrontFacing )
"""

FRAG_N_NEW = """	vec3 normal;
	if ( hasModelSpaceNormals ) {
		/* A model-space map carries all three components already, in the
		 * model's own axes, so it needs model -> view and NOTHING else.  The
		 * tangent-space path below would instead read the map's two stored
		 * channels as offsets along the mesh's tangent and bitangent -- and a
		 * terrain LOD tile's tangent frame is arbitrary (btdterrain.cpp builds
		 * T = n x worldUp, B = n x T), so the sheet's "up" lands sideways and
		 * the surface shades in blotches.
		 *
		 * Channel order MEASURED on Bethesda's own shipped sheets
		 * (Data/Textures/Terrain/Commonwealth/Commonwealth.16.*_msn.DDS, six
		 * 512x512 tiles, correlated against the heights of the same cells):
		 *   R = EAST  (+x)   corr 0.364 with -dh/dx, 0.001 with -dh/dy
		 *   B = NORTH (+y)   corr 0.422 with -dh/dy, 0.002 with -dh/dx
		 *   G = UP    (+z)   mean 238.6 of 255
		 * Alpha is a constant 255 on every tile and carries nothing.
		 * sk_msn.frag's `.rbg` swizzle is this same order.
		 *
		 * The blue channel is NOT recomputed here: it is real data. */
		vec3 msn = normalMap.rgb * 2.0 - 1.0;
		normal = normalize( vec3( msn.r, msn.b, msn.g ) * normalMatrix );
	} else {
		normal = normalMap.rgb * 2.0 - 1.0;
		// Calculate missing blue channel
		normal.b = sqrt(max(1.0 - dot(normal.rg, normal.rg), 0.0));
		normal = normalize( btnMatrix_norm * normal );
	}
	if ( !gl_FrontFacing )
"""

# -------------------------------------------------------------- B: renderer
REND_B_OLD = """		prog->uniSampler( lsp, "NormalMap", 1, texunit, emptyString, clamp, *forced );

		prog->uniSampler( lsp, "GlowMap", 2, texunit, black, clamp );
"""
REND_B_NEW = """		prog->uniSampler( lsp, "NormalMap", 1, texunit, emptyString, clamp, *forced );

		/* Shader Flags 1 bit 12 says the map just bound is a MODEL-space
		 * normal map (an `_msn`).  fo4_default.frag then transforms it by the
		 * model matrix alone instead of the tangent frame.
		 *
		 * Gated on `forced == &emptyString`, the SAME test that decided whether
		 * a real normal map was bound just above: with lighting or normal maps
		 * switched off the substitute is default_n, a flat TANGENT-space
		 * texel, and reading that as model-space would read it as "north" and
		 * tilt the whole surface over. */
		prog->uni1i( "hasModelSpaceNormals",
			int( forced == &emptyString
				&& lsp->hasSF1( ShaderFlags::SLSF1_Model_Space_Normals ) ) );

		prog->uniSampler( lsp, "GlowMap", 2, texunit, black, clamp );
"""

# -------------------------------------------------------------- C: census
REND_C_OLD = ("NifSkopeOpenGLContext::Program * Renderer::setupProgram"
              "( Shape * mesh, Program * hint )\n{\n")

_Q = '\\"'
REND_C_NEW = '''/* WW_PROGRAM_CENSUS -- which program actually lights which shape.
 *
 * `fo4_default.prog` excludes Shader Type 18 and `sk_msn.prog`'s conditions
 * look like they match an FO4 .BTR, so which of the two lights the legacy
 * terrain cannot be settled by reading the condition files: it is decided at
 * runtime by the scan order in setupProgram below.  This writes the answer
 * down instead of inferring it -- one line per first-sighted (shape, program)
 * pair, plus the view-space light direction the lighting arithmetic uses.
 *
 * Armed only by an ABSOLUTE path in WW_PROGRAM_CENSUS; it renders nothing and
 * touches no uniform, so a frame is byte-identical whether it is armed or not.
 */
static NifSkopeOpenGLContext::Program * wwProgramCensus( const NifModel * nif, Shape * mesh,
	NifSkopeOpenGLContext::Program * program, const FloatVector4 & lightViewDir )
{
	static int	armed = -1;
	static QString	censusPath;
	if ( armed < 0 ) {
		QString	p = QString::fromLocal8Bit( qgetenv( "WW_PROGRAM_CENSUS" ) ).trimmed();
		armed = ( !p.isEmpty() && QDir::isAbsolutePath( p ) ) ? 1 : 0;
		censusPath = p;
	}
	if ( !armed )
		return program;

	QString	progName( "(none)" );
	if ( program )
		progName = QString::fromLatin1( program->name.data(), qsizetype( program->name.length() ) );

	int	msn = 0;
	int	lodLand = 0;
	if ( mesh && mesh->bslsp ) {
		msn = int( mesh->bslsp->hasSF1( ShaderFlags::SLSF1_Model_Space_Normals ) );
		lodLand = int( mesh->bslsp->isST( ShaderFlags::ST_WorldMap4 ) );
	}
	QString	row = QString( "shape=@Q@%1@Q@ bsver=%2 msn=%3 lodland=%4 prog=%5" )
		.arg( mesh ? mesh->getName() : QString( "(null)" ) )
		.arg( nif ? int( nif->getBSVersion() ) : -1 )
		.arg( msn ).arg( lodLand ).arg( progName );

	static QSet<QString>	seen;
	if ( seen.contains( row ) )
		return program;
	bool	first = seen.isEmpty();
	seen.insert( row );

	QFile	f( censusPath );
	if ( f.open( QIODevice::Append | QIODevice::Text ) ) {
		QTextStream	s( &f );
		if ( first ) {
			s << "# WW_PROGRAM_CENSUS  light(view) = "
			  << lightViewDir[0] << " " << lightViewDir[1] << " " << lightViewDir[2] << "@N@";
		}
		s << row << "@N@";
	}
	return program;
}

NifSkopeOpenGLContext::Program * Renderer::setupProgram( Shape * mesh, Program * hint )
{
'''.replace("@Q@", _Q).replace("@N@", "\\n")

REND_INC_OLD = "#include <QSettings>\n"
REND_INC_NEW = "#include <QSet>\n#include <QSettings>\n"


def main():
    check = "--check" in sys.argv

    frag = rd(FRAG)
    once(frag, FRAG_UNI_OLD, "frag uniforms")
    once(frag, FRAG_N_OLD, "frag normal block")
    frag2 = frag.replace(FRAG_UNI_OLD, FRAG_UNI_NEW).replace(FRAG_N_OLD, FRAG_N_NEW)

    rend = rd(REND)
    once(rend, REND_INC_OLD, "renderer include")
    once(rend, REND_B_OLD, "renderer NormalMap anchor")
    once(rend, REND_C_OLD, "setupProgram header")
    rend2 = (rend.replace(REND_INC_OLD, REND_INC_NEW)
                 .replace(REND_B_OLD, REND_B_NEW)
                 .replace(REND_C_OLD, REND_C_NEW))

    # the six returns of setupProgram, and only those
    head = REND_C_OLD
    tail = ('\tuseProgram( "default.prog" );\n\tsetupFixedFunction( mesh );\n'
            '\treturn currentProgram;\n}\n')
    i = rend2.index(head)
    j = rend2.index(tail, i) + len(tail)
    body = rend2[i:j]
    cens = "wwProgramCensus( nif, mesh, %s, globalUniforms->lightSourcePosition[0] )"
    n1 = body.count("return currentProgram;")
    n2 = body.count("return program;")
    assert (n1, n2) == (2, 4), ("setupProgram returns: %d currentProgram, %d program"
                                % (n1, n2))
    body = body.replace("return currentProgram;", "return " + cens % "currentProgram" + ";")
    body = body.replace("return program;", "return " + cens % "program" + ";")
    rend2 = rend2[:i] + body + rend2[j:]

    print("anchors OK: frag 2, renderer 3, setupProgram returns %d + %d" % (n1, n2))
    if check:
        return 0
    wr(FRAG, frag2)
    wr(REND, rend2)
    for p in (FRAG, REND):
        b = open(p, "rb").read()
        print("%-40s %8d B  CRLF %d  LF %d" % (p, len(b), b.count(b"\r\n"),
                                               b.count(b"\n") - b.count(b"\r\n")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
