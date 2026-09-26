"""FLAT1 part 5: the option plumbing -- the chunk-pass digest, the command line, the panel row, the self-test row."""
import sys

W = 'E:/Projects/NifskopeWWE-flat1/src/'


def patch(name, pairs):
    p = W + name
    s = open(p, newline='').read()
    for a, b in pairs:
        c = s.count(a)
        if c != 1:
            sys.exit('%s: anchor count %d: %r' % (name, c, a[:80]))
        s = s.replace(a, b)
    open(p, 'w', newline='').write(s)
    print('patched', name)


patch('lodgenchunkpass.cpp', [(
    '''		add( p + "roadSidewalks", b( c.roadSidewalks ) );
''',
    '''		add( p + "roadSidewalks", b( c.roadSidewalks ) );
		add( p + "flatObjects", b( c.flatObjects ) );
		/* The override file's rules move the output, so their digest is an input
		 * (the rule lines only: a comment edit moves nothing). */
		add( p + "flatObjectsRules", c.roads && c.flatObjects
			? lodgenFlatObjectsRulesDigest( c.flatObjectsFile ) : QStringLiteral( "-" ) );
''')])

patch('nifcli.cpp', [
    ('''		lgRoadSidewalksSet = false, lgRoadsLegacy = false,
		lgRoadOpacitySet = false;
''',
     '''		lgRoadSidewalksSet = false, lgRoadsLegacy = false,
		lgRoadOpacitySet = false, lgFlatObjectsSet = false;
'''),
    ('''		else if ( t == QLatin1String( "--no-road-sidewalks" ) ) {
			lgCover.roadSidewalks = false;
			lgRoadSidewalksSet = true;
		}
''',
     '''		else if ( t == QLatin1String( "--no-road-sidewalks" ) ) {
			lgCover.roadSidewalks = false;
			lgRoadSidewalksSet = true;
		}
		else if ( t == QLatin1String( "--flat-objects" ) ) {
			lgCover.flatObjects = true;
			lgFlatObjectsSet = true;
		}
		else if ( t == QLatin1String( "--no-flat-objects" ) ) {
			lgCover.flatObjects = false;
			lgFlatObjectsSet = true;
		}
		else if ( t == QLatin1String( "--flat-objects-file" ) ) lgCover.flatObjectsFile = next();
'''),
    ('''		if ( !lgRoadSidewalksSet )
			lgCover.roadSidewalks = true;
	}
''',
     '''		if ( !lgRoadSidewalksSet )
			lgCover.roadSidewalks = true;
		/* ROADS1 painted no flat ground objects. */
		if ( !lgFlatObjectsSet )
			lgCover.flatObjects = false;
	}
'''),
    ('''		  << "             [--road-sidewalks] [--no-road-sidewalks] [--roads-legacy]\\n"
''',
     '''		  << "             [--road-sidewalks] [--no-road-sidewalks] [--roads-legacy]\\n"
		  << "             [--flat-objects] [--no-flat-objects]\\n"
		  << "             [--flat-objects-file FILE]\\n"
'''),
    ('''		  << "                                          the bake before this existed.\\n"
		  << "  lodgen ... [--terrain-object-ao]\\n"
''',
     '''		  << "                                          the bake before this existed.\\n"
		  << "                                          FLAT GROUND OBJECTS (on with the\\n"
		  << "                                          roads, --no-flat-objects turns them\\n"
		  << "                                          off): every other placed static\\n"
		  << "                                          that is low and lies on the ground\\n"
		  << "                                          (measured from its mesh: top at most\\n"
		  << "                                          64 above the ground, underside at\\n"
		  << "                                          most 16 above it, not under water,\\n"
		  << "                                          more top than side) is painted with\\n"
		  << "                                          its in-game texture; decals and\\n"
		  << "                                          alpha shapes go over, lowest first.\\n"
		  << "                                          The override file (default\\n"
		  << "                                          lodgen_flat_objects.txt beside the\\n"
		  << "                                          exe, --flat-objects-file names\\n"
		  << "                                          another) takes lines `bake PATH` /\\n"
		  << "                                          `nobake PATH` (a .nif or a folder)\\n"
		  << "                                          that win over the rule. The bake\\n"
		  << "                                          writes <ws>.flat_objects_report.txt\\n"
		  << "                                          beside the sheets (terrain VT bake).\\n"
		  << "                                          --roads-legacy turns them off.\\n"
		  << "  lodgen ... [--terrain-object-ao]\\n"
'''),
])

patch('lodgenmanager.cpp', [
    ('''					"Command line: --no-road-sidewalks turns them off" ) );
			roadsSection->body()->setLayout( f.g );
''',
     '''					"Command line: --no-road-sidewalks turns them off" ) );
			xB( f, "LodgenFlatObjectsCheck", QStringLiteral( "flatObjects" ),
				tr( "Paint flat ground objects" ), true,
				tr( "Low objects lying on the ground (pads, tracks, paths, decals, flat debris)\\n"
					"are painted with the texture they wear in game. Standing things stay out.\\n"
					"The file lodgen_flat_objects.txt beside NifSkope overrides the rule per model\\n"
					"or folder (bake / nobake lines); each bake writes a report beside its output.\\n"
					"Command line: --no-flat-objects turns them off" ) );
			roadsSection->body()->setLayout( f.g );
'''),
    ('''						"roadComposite", "roadRaised", "roadSidewalks" } )
''',
     '''						"roadComposite", "roadRaised", "roadSidewalks", "flatObjects" } )
'''),
    ('''		o.roadSidewalks = xb( "roadSidewalks" );
''',
     '''		o.roadSidewalks = xb( "roadSidewalks" );
		o.flatObjects = xb( "flatObjects" );
'''),
])

patch('nifskope_ui.cpp', [(
    '''								"the road pass writes nothing on this chunk -- the command line own --no-roads moves only the ledger here (measured 2026-09-12); tests/spells/lodgen_roads.sh reads the road rows", nullptr, nullptr, nullptr, nullptr, "1" },
							{ "LodgenTerrainObjectAoCheck",''',
    '''								"the road pass writes nothing on this chunk -- the command line own --no-roads moves only the ledger here (measured 2026-09-12); tests/spells/lodgen_roads.sh reads the road rows", nullptr, nullptr, nullptr, nullptr, "1" },
							{ "LodgenFlatObjectsCheck", "flatObjects", nullptr,
								"the road pass writes nothing on this chunk -- the command line own --no-roads moves only the ledger here (measured 2026-09-12); tests/spells/lodgen_roads.sh reads the road rows", nullptr, nullptr, nullptr, nullptr, "1" },
							{ "LodgenTerrainObjectAoCheck",''')])
