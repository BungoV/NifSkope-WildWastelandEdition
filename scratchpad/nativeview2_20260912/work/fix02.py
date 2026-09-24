#!/usr/bin/env python
"""Repair for fix01's census: `Shape::bslsp` is PROTECTED, and a file-scope
static function is not a friend of Shape the way Renderer's own members are
(three errors at renderer.cpp:136-138).  The two flags are therefore read in
`Renderer::setupProgram`, which may read them, and passed in.
"""
import sys

REND = "E:/Projects/NifskopeWildWastelandEdition/src/gl/renderer.cpp"

OLD_SIG = """static NifSkopeOpenGLContext::Program * wwProgramCensus( const NifModel * nif, Shape * mesh,
	NifSkopeOpenGLContext::Program * program, const FloatVector4 & lightViewDir )
{"""
NEW_SIG = """static NifSkopeOpenGLContext::Program * wwProgramCensus( const NifModel * nif, Shape * mesh,
	int msn, int lodLand, NifSkopeOpenGLContext::Program * program,
	const FloatVector4 & lightViewDir )
{"""

OLD_FLAGS = """	int	msn = 0;
	int	lodLand = 0;
	if ( mesh && mesh->bslsp ) {
		msn = int( mesh->bslsp->hasSF1( ShaderFlags::SLSF1_Model_Space_Normals ) );
		lodLand = int( mesh->bslsp->isST( ShaderFlags::ST_WorldMap4 ) );
	}
	QString	row"""
NEW_FLAGS = """	QString	row"""

OLD_HEAD = """	const NifModel *	nif = mesh->scene->nifModel;
	if ( nif == nullptr || nif->getBSVersion() == 0 ) {"""
NEW_HEAD = """	const NifModel *	nif = mesh->scene->nifModel;

	/* Read here, not inside wwProgramCensus: `Shape::bslsp` is protected and
	 * only a Shape's friends -- Renderer's own members -- may read it. */
	const int	wwMsn = ( mesh->bslsp
		? int( mesh->bslsp->hasSF1( ShaderFlags::SLSF1_Model_Space_Normals ) ) : 0 );
	const int	wwLodLand = ( mesh->bslsp
		? int( mesh->bslsp->isST( ShaderFlags::ST_WorldMap4 ) ) : 0 );

	if ( nif == nullptr || nif->getBSVersion() == 0 ) {"""

OLD_CALL_A = "wwProgramCensus( nif, mesh, currentProgram, globalUniforms->lightSourcePosition[0] )"
NEW_CALL_A = ("wwProgramCensus( nif, mesh, wwMsn, wwLodLand, currentProgram,\n"
              "\t\tglobalUniforms->lightSourcePosition[0] )")
OLD_CALL_B = "wwProgramCensus( nif, mesh, program, globalUniforms->lightSourcePosition[0] )"
NEW_CALL_B = ("wwProgramCensus( nif, mesh, wwMsn, wwLodLand, program,\n"
              "\t\t\t\tglobalUniforms->lightSourcePosition[0] )")


def main():
    b = open(REND, "rb").read()
    assert b.count(b"\r\n") == 0
    s = b.decode("utf-8")
    for tag, old, n in (("sig", OLD_SIG, 1), ("flags", OLD_FLAGS, 1),
                        ("head", OLD_HEAD, 1), ("call A", OLD_CALL_A, 2),
                        ("call B", OLD_CALL_B, 4)):
        c = s.count(old)
        assert c == n, "%s: %d, wanted %d" % (tag, c, n)
        print("%-8s %d" % (tag, c))
    if "--check" in sys.argv:
        return 0
    s = (s.replace(OLD_SIG, NEW_SIG).replace(OLD_FLAGS, NEW_FLAGS)
          .replace(OLD_HEAD, NEW_HEAD).replace(OLD_CALL_A, NEW_CALL_A)
          .replace(OLD_CALL_B, NEW_CALL_B))
    open(REND, "wb").write(s.encode("utf-8"))
    b = open(REND, "rb").read()
    print("%s %d B  CRLF %d  LF %d" % (REND, len(b), b.count(b"\r\n"),
                                       b.count(b"\n") - b.count(b"\r\n")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
