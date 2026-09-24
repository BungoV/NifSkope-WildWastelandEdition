import io
p = 'src/nativeemit.cpp'
s = open(p, encoding='utf-8', newline='').read()
BS = chr(92)


def rep(o, n, c=1):
    global s
    assert s.count(o) == c, (s.count(o), o[:90])
    s = s.replace(o, n)


old_head = """		/* v7: the path test, taken HERE because this is the only place the
		 * placement's own model string is in hand.
		 *
		 * A COMPONENT test and not the prefix test the brief names, because the
		 * prefix test catches NOTHING and that was measured before this line was
		 * written: `p.model` is the model the chunk builder DREW, which for a far
		 * placement is the authored LOD -- `LOD""" + BS + """Architecture""" + BS + """Airport""" + BS + """...` -- so
		 * "starts with architecture""" + BS + """" matched 0 of 2,449 placements on chunk
		 * 4.4.-12 while 2,026 of the .lodo's 5,956 model strings sit under an
		 * `Architecture` folder. Asking whether any path COMPONENT is
		 * `architecture` catches the LOD path and the source path alike. Both
		 * separators, because a path from an ESM and a path from a loose file do
		 * not agree on which one Bethesda used. */
		{
			bool arch = false;
			int from = 0;
			while ( from <= p.model.size() ) {
				int sep = from;
				while ( sep < p.model.size() && p.model[sep] != QLatin1Char( '""" + BS + BS + """' ) && p.model[sep] != QLatin1Char( '/' ) )
					sep++;
				if ( QStringView( p.model ).mid( from, sep - from ).compare( QLatin1String( "architecture" ), Qt::CaseInsensitive ) == 0 ) {
					arch = true;
					break;
				}
				from = sep + 1;
			}
			instArch.push_back( arch ? 1 : 0 );
			if ( arch )
				archPlacements++;
		}"""

new_head = """		/* v7: the path test, taken HERE because this is the only place both the
		 * placement and its base row are in hand.
		 *
		 * The string asked is the BASE ROW'S model -- `lb.model`, the near MODL,
		 * the one `lib.addString` already put in the .lodo -- and not `p.model`,
		 * the model this placement DREW. Two measurements forced that, both on
		 * chunk 4.4.-12's own bake and both before this line was written:
		 *
		 *   - the brief's literal "starts with architecture""" + BS + """" caught 0 of 2,449;
		 *   - a path-COMPONENT test on the DRAWN model caught 238 of 2,449,
		 *     because the drawn model of a far placement is the authored LOD and
		 *     Bethesda files those by NEIGHBOURHOOD, not by kind:
		 *     `LOD""" + BS + """Neighborhoods""" + BS + """Cambridge""" + BS + """Cambridge10_Bld01LOD.nif`. Exactly 1 of
		 *     the 314 LOD-rooted paths in this .lodo carries an `architecture`
		 *     component; the other 313 are houses filed under a place name.
		 *
		 * The base's SOURCE path does carry the kind -- `Architecture""" + BS + """Buildings""" + BS + """
		 * BldgBrick7Story3x5FreeComEntA.nif` -- and catches 1,877 of 2,449. It is
		 * also the string the .lodo SHIPS (`bases[].modelStringOffset`), so a
		 * refuter reading the file judges the identical bytes this rule judged
		 * rather than a paraphrase of them. Both separators, because a path from
		 * an ESM and a path from a loose file do not agree on which one Bethesda
		 * used. */
		{
			const QString & bp = lib.stringAt( lib.bases[br.value()].modelStringOffset );
			bool arch = false;
			int from = 0;
			while ( from <= bp.size() ) {
				int sep = from;
				while ( sep < bp.size() && bp[sep] != QLatin1Char( '""" + BS + BS + """' ) && bp[sep] != QLatin1Char( '/' ) )
					sep++;
				if ( QStringView( bp ).mid( from, sep - from ).compare( QLatin1String( KNOB_ARCH_COMPONENT ), Qt::CaseInsensitive ) == 0 ) {
					arch = true;
					break;
				}
				from = sep + 1;
			}
			instArch.push_back( arch ? 1 : 0 );
			if ( arch )
				archPlacements++;
		}"""

rep(old_head, new_head)
open(p, 'w', encoding='utf-8', newline='').write(s)
print('arch source-path rule spliced')
