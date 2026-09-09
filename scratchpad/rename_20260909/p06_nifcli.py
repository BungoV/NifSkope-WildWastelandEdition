# Patch 6 -- src/nifcli.cpp
#   (a) every command, flag and printed key follows the 2026-09-09 rename:
#       the LAND file is .lodl (command `lodl`, flag --lodl), the terrain
#       TEXTURE sheets are .lodt (flag --lodt-check). The retired spellings
#       (`lodt` the command, --lodt, --lodv-check) are REFUSED BY NAME so an
#       old command line fails loudly instead of doing the wrong thing;
#   (b) `--candidates trees` means TREE ONLY (it meant `tree || missing`,
#       which put 14 shacks and rock cliffs in a 33-candidate "trees" run).
import os
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")
P = "src/nifcli.cpp"
b = open(P, "rb").read()
assert b.count(b"\r") == 0
s = b.decode("utf-8")


def sub(old, new, n=1):
    global s
    c = s.count(old)
    assert c == n, "anchor count %d != %d for %r" % (c, n, old[:80])
    s = s.replace(old, new)


# ---------------------------------------------------------------- (b) trees
sub("""	 *   missing  far MNAM slots empty: these fall back to heavy near
		 *            meshes at dim16/32 without a card (the default)
		 *   trees    every tree, and the missing
		 *   all      every base with LOD""",
    """	 *   missing  far MNAM slots empty: these fall back to heavy near
		 *            meshes at dim16/32 without a card (the default)
		 *   trees    every tree, and ONLY trees. It meant `tree || missing`
		 *            until 2026-09-09, which is a SUPERSET of the default and
		 *            not a tree list at all: 14 of a 33-candidate Sanctuary
		 *            run were shacks and rock cliffs
		 *   all      every base with LOD""")
sub("""			else if ( candidateKind == QLatin1String( "trees" ) )
				want = tree || missing;""",
    """			else if ( candidateKind == QLatin1String( "trees" ) )
				want = tree;""")

# ------------------------------------------------------- (a) the land route
sub("""/*! `lodt <file.lodt>` — our own whole-worldspace landscape file to terrain""",
    """/*! `lodl <file.lodl>` — our own whole-worldspace landscape file to terrain""")
sub("""		  << "  lodt <file.lodt> --info                 our landscape file: extent, height\\n\"""",
    """		  << "  lodl <file.lodl> --info                 our landscape file: extent, height\\n\"""")
sub("""		  << "  lodt <file.lodt> [--region X0 Y0 X1 Y1] [--lod N] [--plane KEY] [-o OUT.nif]\\n\"""",
    """		  << "  lodl <file.lodl> [--region X0 Y0 X1 Y1] [--lod N] [--plane KEY] [-o OUT.nif]\\n\"""")

# the two written paths and the --from-btd path
sub("""			err() << "error: --from-btd needs --lodt <dir>" << Qt::endl;""",
    """			err() << "error: --from-btd needs --lodl <dir>" << Qt::endl;""")
assert s.count('+ QFileInfo( btdPath ).completeBaseName() + QStringLiteral( ".lodt" );') == 1
s = s.replace('+ QFileInfo( btdPath ).completeBaseName() + QStringLiteral( ".lodt" );',
              '+ QFileInfo( btdPath ).completeBaseName() + QStringLiteral( ".lodl" );')
assert s.count('+ world.worldspaceEdid() + QStringLiteral( ".lodt" );') == 2
s = s.replace('+ world.worldspaceEdid() + QStringLiteral( ".lodt" );',
              '+ world.worldspaceEdid() + QStringLiteral( ".lodl" );')
assert s.count('out() << "lodt: " << written << Qt::endl;') == 2
s = s.replace('out() << "lodt: " << written << Qt::endl;',
              'out() << "lodl: " << written << Qt::endl;')

# ------------------------------------------------- (a) the texture container
sub("""	/* Read a .lodm or a .lodv through the repo's OWN parser/validator, so a""",
    """	/* Read a .lodm or a .lodt (the terrain texture sheets) through the repo's
	 * OWN parser/validator, so a""")
sub("""			out() << "lodv ok 0" << Qt::endl;
			out() << "lodv " << verr << Qt::endl;
			return 1;
		}
		out() << "lodv ok 1" << Qt::endl;
		for ( const QString & l : lodvDescribe( hf, table ) )
			out() << "lodv " << l << Qt::endl;""",
    """			out() << "lodt ok 0" << Qt::endl;
			out() << "lodt " << verr << Qt::endl;
			return 1;
		}
		out() << "lodt ok 1" << Qt::endl;
		for ( const QString & l : lodvDescribe( hf, table ) )
			out() << "lodt " << l << Qt::endl;""")
sub("""		  << "                                          8-texel border, one .lodv per level\\n\"""",
    """		  << "                                          8-texel border, one .lodt per level\\n\"""")
sub("""		  << "  lodgen --lodv-check FILE.lodv            validate a .lodv by every rule of\\n\"""",
    """		  << "  lodgen --lodt-check FILE.lodt            validate a terrain texture level by\\n"
		  << "                                          every rule of\\n\"""")

# --------------------------------------------------------- the flag parsing
sub("""		else if ( t == QLatin1String( "--lodt" ) ) lgLodtDir = next();""",
    """		else if ( t == QLatin1String( "--lodl" ) ) lgLodtDir = next();
		/* The retired spellings NAME their replacement instead of falling into
		 * the generic "unknown option": these two are the commands a harness,
		 * a script or bungo's own shell history will still be carrying, and a
		 * bare "unknown option --lodt" does not say that .lodt now means
		 * something else. */
		else if ( t == QLatin1String( "--lodt" ) ) {
			err() << "error: --lodt is retired: the landscape file is .lodl now "
					 "(.lodt names the terrain texture sheets) -- use --lodl <dir>" << Qt::endl;
			err().flush();
			return 2;
		}""")
sub("""		else if ( t == QLatin1String( "--lodv-check" ) ) lgLodvCheck = next();""",
    """		else if ( t == QLatin1String( "--lodt-check" ) ) lgLodvCheck = next();
		else if ( t == QLatin1String( "--lodv-check" ) ) {
			err() << "error: --lodv-check is retired: the terrain texture sheets are "
					 ".lodt now -- use --lodt-check <file.lodt>" << Qt::endl;
			err().flush();
			return 2;
		}""")

# ------------------------------------------------------------- the dispatch
sub("""	else if ( cmd == QLatin1String( "lodt" ) )
		rc = cmdLodt( file, btdInfo, btdHaveRegion,
			btdRegion[0], btdRegion[1], btdRegion[2], btdRegion[3], btdLod,
			lodtPlaneName, outFile );""",
    """	else if ( cmd == QLatin1String( "lodl" ) )
		rc = cmdLodt( file, btdInfo, btdHaveRegion,
			btdRegion[0], btdRegion[1], btdRegion[2], btdRegion[3], btdLod,
			lodtPlaneName, outFile );
	else if ( cmd == QLatin1String( "lodt" ) ) {
		err() << "error: the 'lodt' command is retired: the landscape file is .lodl "
				 "now (.lodt names the terrain texture sheets) -- use "
				 "'lodl <file.lodl>'" << Qt::endl;
		rc = 2;
	}""")

# the questions-that-need-no-file comment mentions --lodv-check by name
sub("""	 * forgotten so far (--dump-geometry, --lodm-check, --lodv-check) and the""",
    """	 * forgotten so far (--dump-geometry, --lodm-check, --lodt-check) and the""")

out = s.encode("utf-8")
assert out.count(b"\r") == 0
open(P, "wb").write(out)
print("OK", P, len(out), "bytes, CR", out.count(b"\r"), "LF", out.count(b"\n"))
for i, ln in enumerate(s.split("\n"), 1):
    low = ln.lower()
    if (".lodv" in low or "lodv-check" in low
            or ('"lodt' in low and "retired" not in low and "names the terrain" not in low)):
        print("  check @%d: %s" % (i, ln.strip()))
