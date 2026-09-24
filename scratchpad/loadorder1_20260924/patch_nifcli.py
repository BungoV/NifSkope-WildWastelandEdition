"""Lane LOADORDER1: the nifcli.cpp hunks for --mo2-profile / --mo2-mods and
--plugins-txt keeping its masters. Every anchor must match exactly once; the
file is LF-only and must stay so. --check writes nothing."""
import sys

PATH = 'E:/Projects/NifskopeWWE-loadorder1/src/nifcli.cpp'
check = '--check' in sys.argv

with open(PATH, 'rb') as f:
    src = f.read()
cr0 = src.count(b'\r')

EDITS = [
    # 1. the header
    (b'#include "lodgenchunkpass.h"\n',
     b'#include "lodgenchunkpass.h"\n#include "lodgenloadorder.h"\n'),
    # 2. the switch digest: both name WHICH FILE an asset is read from
    (b'\t"--resource", "--plugins-txt",\n',
     b'\t"--resource", "--plugins-txt", "--mo2-profile", "--mo2-mods",\n'),
    # 3. the variables
    (b'\tQString lgPluginsTxt;\n\tbool lgMo2 = false;\n',
     b'\tQString lgPluginsTxt;\n\tbool lgMo2 = false;\n'
     b'\tQString lgMo2Profile;             // lane LOADORDER1: his MO2 profile off disk\n'
     b'\tQString lgMo2Mods;\n'),
    # 4. the parse
    (b'\t\telse if ( t == QLatin1String( "--mo2" ) ) lgMo2 = true;\n',
     b'\t\telse if ( t == QLatin1String( "--mo2" ) ) lgMo2 = true;\n'
     b'\t\telse if ( t == QLatin1String( "--mo2-profile" ) ) lgMo2Profile = next();\n'
     b'\t\telse if ( t == QLatin1String( "--mo2-mods" ) ) lgMo2Mods = next();\n'),
    # 5. a profile IS the plugin list, so no <file> is needed with it
    (b'\t\t\t\t|| lgPrintSource || lgListFiles > 0\n',
     b'\t\t\t\t|| lgPrintSource || lgListFiles > 0 || !lgMo2Profile.isEmpty()\n'),
    # 6a. the MO2 profile branch, ahead of --plugins-txt / --mo2
    (b'\t\tQStringList mo2Plugins;\n\t\tif ( lgMo2 || !lgPluginsTxt.isEmpty() ) {\n',
     b'\t\tQStringList mo2Plugins;\n'
     b'\t\tif ( !lgMo2Profile.isEmpty() ) {\n'
     b'\t\t\t/* His MO2 load order read off disk, no usvfs (lane LOADORDER1,\n'
     b'\t\t\t * src/lodgenloadorder.h): the plugins as full paths, masters first,\n'
     b'\t\t\t * and the stack Data -> mods bottom-up -> overwrite, --resource above. */\n'
     b'\t\t\tif ( !lodgenApplyMo2Profile( lgMo2Profile, lgMo2Mods, lgDataRoot, lgResources,\n'
     b'\t\t\t\t\tlgMo2 || !lgPluginsTxt.isEmpty(), &stack, &file, out(), err() ) )\n'
     b'\t\t\t\treturn 2;\n'
     b'\t\t} else if ( lgMo2 || !lgPluginsTxt.isEmpty() ) {\n'),
    # 6b. --plugins-txt keeps the masters plugins.txt never lists
    (b'\t\t\t// the plugin list becomes the comma list EsmFile merges, in load order\n'
     b'\t\t\tQStringList resolved;\n'
     b'\t\t\tfor ( const QString & p : mo2Plugins ) {\n'
     b'\t\t\t\tconst QString full = QDir( dataDir ).filePath( p );\n'
     b'\t\t\t\tresolved << ( QFileInfo( full ).isFile() ? QDir::cleanPath( full ) : p );\n'
     b'\t\t\t}\n',
     b'\t\t\t/* the plugin list becomes the comma list EsmFile merges, in load order,\n'
     b'\t\t\t * led by the masters plugins.txt never lists (Fallout4.esm, DLC, CC);\n'
     b'\t\t\t * a plugin not in Data is refused by name (lane LOADORDER1) */\n'
     b'\t\t\tQStringList resolved;\n'
     b'\t\t\tQString rerr;\n'
     b'\t\t\tif ( !mo2Plugins.isEmpty()\n'
     b'\t\t\t\t&& !lodgenLoadOrderFromPluginsTxt( dataDir, mo2Plugins, &resolved, &rerr ) ) {\n'
     b'\t\t\t\terr() << "error: --plugins-txt refused: " << rerr << Qt::endl;\n'
     b'\t\t\t\treturn 2;\n'
     b'\t\t\t}\n'),
    # 7. what --print-source calls the source
    (b'\t\t\tout() << "source: " << ( lgMo2 ? "mo2" : "specified" ) << Qt::endl;\n',
     b'\t\t\tout() << "source: " << ( !lgMo2Profile.isEmpty() ? "mo2-profile" : lgMo2 ? "mo2" : "specified" )\n'
     b'\t\t\t\t  << Qt::endl;\n'),
    # 8. the help text
    (b'\t\t  << "                                          archives in that order\\n"\n',
     b'\t\t  << "                                          archives in that order\\n"\n'
     b'\t\t  << "  lodgen [--mo2-profile DIR] [--mo2-mods DIR]  his MO2 load order read off\\n"\n'
     b'\t\t  << "                                          disk, MO2 not running: Fallout4.esm,\\n"\n'
     b'\t\t  << "                                          the DLC and CC masters in Data, then\\n"\n'
     b'\t\t  << "                                          plugins.txt\'s enabled plugins as full\\n"\n'
     b'\t\t  << "                                          paths (overwrite, the enabled mods\\n"\n'
     b'\t\t  << "                                          top-down, then Data); the stack Data,\\n"\n'
     b'\t\t  << "                                          then modlist.txt bottom-up, then\\n"\n'
     b'\t\t  << "                                          overwrite. Mods default to\\n"\n'
     b'\t\t  << "                                          <profile>/../../mods, Data to\\n"\n'
     b'\t\t  << "                                          --data-root or ModOrganizer.ini\\n"\n'),
]

out = src
for i, (a, b) in enumerate(EDITS, 1):
    n = out.count(a)
    if n != 1:
        print('edit %d: anchor count %d, refusing' % (i, n))
        sys.exit(1)
    out = out.replace(a, b)
added = sum(b.count(b'\n') - a.count(b'\n') for a, b in EDITS)
assert out.count(b'\r') == cr0 == 0, 'CR count moved'
assert out.count(b'\n') == src.count(b'\n') + added
print('all %d anchors once; +%d lines; CR %d' % (len(EDITS), added, cr0))
if not check:
    with open(PATH, 'wb') as f:
        f.write(out)
    print('written')
