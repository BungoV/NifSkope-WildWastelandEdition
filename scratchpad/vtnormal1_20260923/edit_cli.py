import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/src/nifcli.cpp'
s = open(P, 'rb').read().decode('utf-8')


def sub(old, new, count=1):
	global s
	n = s.count(old)
	if n != count:
		sys.exit('anchor found %d times, want %d:\n%s' % (n, count, old[:300]))
	s = s.replace(old, new)


sub("""	LodgenVtOptions lgVt;
	QString lgVtDir;
	int lgVtBtr = -1;""", """	LodgenVtOptions lgVt;
	QString lgVtDir;
	/* THE FINEST TEXEL DENSITY AS ONE WORD (lane VTNORMAL1, bungo's ruling
	 * 2026-09-23 09:4x): 32, 16 or 8 world units a texel. It NAMES existing
	 * pairs and adds nothing underneath -- 32 = finest 2 / content 256 (the
	 * default), 16 = finest 2 / content 512, 8 = finest 1 / content 512 (his
	 * sheets' own density) -- so it is refused beside either of them. */
	int lgVtDensity = 0;
	bool lgVtFinestGiven = false, lgVtContentGiven = false;
	int lgVtBtr = -1;""")
sub("""		else if ( t == QLatin1String( "--vt-finest" ) ) lgVt.finestDim = next().toInt();
		else if ( t == QLatin1String( "--vt-content" ) ) lgVt.content = next().toInt();""",
"""		else if ( t == QLatin1String( "--vt-finest" ) ) {
			lgVt.finestDim = next().toInt();
			lgVtFinestGiven = true;
		}
		else if ( t == QLatin1String( "--vt-content" ) ) {
			lgVt.content = next().toInt();
			lgVtContentGiven = true;
		}
		else if ( t == QLatin1String( "--vt-density" ) ) lgVtDensity = qMax( -1, next().toInt() );
		else if ( t == QLatin1String( "--vt-half-aux" ) ) lgVt.halfAux = true;""")
sub("""		rc = cmdLodgen( file, lgListWorldspaces, lgWorldspace,
			lgHaveCell, lgCell[0], lgCell[1],""", """		if ( lgVtDensity ) {
			if ( lgVtFinestGiven || lgVtContentGiven ) {
				err() << "error: --vt-density names a --vt-finest / --vt-content pair; give "
						 "one or the other, not both" << Qt::endl;
				return 2;
			}
			if ( lgVtDensity == 32 ) {
				lgVt.finestDim = 2;
				lgVt.content = 256;
			} else if ( lgVtDensity == 16 ) {
				lgVt.finestDim = 2;
				lgVt.content = 512;
			} else if ( lgVtDensity == 8 ) {
				lgVt.finestDim = 1;
				lgVt.content = 512;
			} else {
				err() << "error: --vt-density must be 32, 16 or 8 (world units a texel at the "
						 "finest level)" << Qt::endl;
				return 2;
			}
		}
		rc = cmdLodgen( file, lgListWorldspaces, lgWorldspace,
			lgHaveCell, lgCell[0], lgCell[1],""")
sub("""		if ( vtOpts.compression < 0 || vtOpts.compression > 1 ) {
			err() << "error: --vt-compress must be none or zlib" << Qt::endl;
			return 2;
		}""", """		if ( vtOpts.compression < 0 || vtOpts.compression > 1 ) {
			err() << "error: --vt-compress must be none or zlib" << Qt::endl;
			return 2;
		}
		if ( vtOpts.halfAux && vtOpts.mips < 2 ) {
			err() << "error: --vt-half-aux drops each aux sheet's top mip and keeps the rest, so "
					 "it needs --vt-mips 2 or more" << Qt::endl;
			return 2;
		}""")
sub("""		  << "         [--vt-finest 2] [--vt-content 256] [--vt-border 8] [--vt-mips 2]\\n"
""", """		  << "         [--vt-finest 2] [--vt-content 256] [--vt-border 8] [--vt-mips 2]\\n"
		  << "         [--vt-density 32|16|8]           the finest level's texel size in\\n"
		  << "                                          world units, as one word: 32 =\\n"
		  << "                                          --vt-finest 2 --vt-content 256 (the\\n"
		  << "                                          CLI default, ~1.7 GB for the whole\\n"
		  << "                                          Commonwealth), 16 = finest 2 content\\n"
		  << "                                          512 (the panel's default, ~6.4 GB),\\n"
		  << "                                          8 = finest 1 content 512 (the upscaled\\n"
		  << "                                          normal sheets' own density, ~26 GB).\\n"
		  << "                                          Refused beside --vt-finest/--vt-content\\n"
		  << "         [--vt-half-aux]                  normal, mask, height and emissive\\n"
		  << "                                          tiles at HALF the texels a side (their\\n"
		  << "                                          top mip is not stored; the header's\\n"
		  << "                                          sheet descriptor byte 6 says so); the\\n"
		  << "                                          colour keeps the full density. OFF by\\n"
		  << "                                          default; needs --vt-mips 2 or more.\\n"
		  << "                                          With --msn-cache set, the pyramid's\\n"
		  << "                                          NORMAL is that folder's sheets, box-\\n"
		  << "                                          filtered as vectors to each level;\\n"
		  << "                                          a chunk with no sheet keeps the\\n"
		  << "                                          heights normal.\\n"
""")
open(P, 'wb').write(s.encode('utf-8'))
print('cli ok')
