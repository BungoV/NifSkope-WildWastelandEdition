p = 'src/lodgen.cpp'
s = open(p, 'r', encoding='utf-8', newline='').read()


def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (a[:80], s.count(a))
    s = s.replace(a, b)


rep("""	{
		QJsonObject root;
		root.insert( QStringLiteral( "lodm" ), 1 );
		root.insert( QStringLiteral( "family" ), QStringLiteral( "legacy" ) );
		root.insert( QStringLiteral( "kind" ), QStringLiteral( "terrainVT" ) );""",
"""	{
		QJsonObject root;
		root.insert( QStringLiteral( "lodm" ), 1 );
		/* THE FAMILY WORD IS REAL NOW, and it MEANS it (bungo, 2026-09-11
		 * 09:5x: "you can mirror how it is set up for the .lodm").
		 *
		 * It said "legacy" until today and the contract called it vestigial,
		 * because the sheets were terrain's own invention and neither family
		 * described them. They are the OBJECT family's now: the mask sheet is
		 * the `rmaos` slot's channels in the `rmaos` slot's order, so a
		 * consumer that knows `.lodm` 2.1 knows this pyramid without a second
		 * table. Legacy materials are CONVERTED at bake -- gloss inverted into
		 * roughness, metallic 0 -- so what ships is PBR whatever the source
		 * was, and `terrain.maskRules` below says how many layers came by which
		 * road rather than leaving the word to be taken on trust. */
		root.insert( QStringLiteral( "family" ), QStringLiteral( "pbr" ) );
		root.insert( QStringLiteral( "kind" ), QStringLiteral( "terrainVT" ) );""")

rep("""		sheets.append( sheet( "color", 71, 71, "sRGB", "RGB albedo, grass tint folded in" ) );
		sheets.append( sheet( "msn", 71, 71, "linear", "model-space normal, 0.5+0.5 encoded" ) );
		sheets.append( sheet( "data", 71, 77, "linear",
			"R sky-free AO, G flow wetness, B shore proximity, A ground cover" ) );
		sheets.append( sheet( "height", 56, 56, "linear",
			"R16_UNORM, height/8 + 32767, the shadow heightmap's own encoding" ) );
		t.insert( QStringLiteral( "sheets" ), sheets );""",
"""		const int colorCover = opts.coverInColor ? 77 : 71;
		const int maskCover = opts.coverInColor ? 71 : 77;
		sheets.append( sheet( "color", 71, colorCover, "sRGB",
			opts.coverInColor
				? "RGB albedo, grass tint folded in, A ground cover (the object family's coverage slot)"
				: "RGB albedo, grass tint folded in" ) );
		sheets.append( sheet( "msn", 71, 71, "linear", "model-space normal, 0.5+0.5 encoded" ) );
		sheets.append( sheet( "mask", 71, maskCover, "linear",
			opts.coverInColor
				? "rmaos: R roughness, G metallic, B sky-free AO, A unused (0)"
				: "rmaos: R roughness, G metallic, B sky-free AO, A ground cover" ) );
		if ( opts.height )
			sheets.append( sheet( "height", 56, 56, "linear",
				"R16_UNORM, height/8 + 32767, the shadow heightmap's own encoding" ) );
		if ( wantEmissive )
			sheets.append( sheet( "emissive", 71, 71, "linear",
				"RGB emissive colour, no alpha" ) );
		t.insert( QStringLiteral( "sheets" ), sheets );
		/* ABSENCE, SAID IN WORDS. A consumer must be able to tell "this
		 * worldspace emits nothing" from "the writer forgot"; a black sheet
		 * says neither. */
		t.insert( QStringLiteral( "emissive" ), wantEmissive
			? QStringLiteral( "present" ) : QStringLiteral( "none" ) );
		/* WHAT WAS DROPPED, and where the consumer gets it instead. Naming it
		 * here costs one key and saves a reader looking for a channel that was
		 * removed on purpose. */
		{
			QJsonObject dropped;
			dropped.insert( QStringLiteral( "shoreProximity" ),
				QStringLiteral( "runtime: subtract the .lodl water body plane from the height "
					"at the sample (docs/LODGEN_BTD_FORMAT.md, \\"What is NOT in this file\\")" ) );
			dropped.insert( QStringLiteral( "wetness" ),
				QStringLiteral( "not baked: a close-up effect; far wetness is a weather state "
					"the runtime owns" ) );
			t.insert( QStringLiteral( "dropped" ), dropped );
		}
		/* THE PER-LAYER RULE CENSUS. Every landscape texture the bake's own
		 * rectangle paints, counted by the rule that served its mask -- so the
		 * `family: pbr` above is auditable instead of asserted, and a
		 * worldspace served entirely by `none-default` cannot pass for a
		 * measurement. */
		{
			QJsonObject rules;
			rules.insert( QStringLiteral( "pbrm" ), maskCache.ruleCounts[LODGEN_MASK_PBRM] );
			rules.insert( QStringLiteral( "legacyInverted" ),
				maskCache.ruleCounts[LODGEN_MASK_LEGACY_INVERTED] );
			rules.insert( QStringLiteral( "noneDefault" ), maskCache.ruleCounts[LODGEN_MASK_NONE] );
			rules.insert( QStringLiteral( "withRoughnessMap" ), maskCache.withRoughnessMap );
			rules.insert( QStringLiteral( "withMetallicMap" ), maskCache.withMetallicMap );
			rules.insert( QStringLiteral( "withEmissiveMap" ), maskCache.withEmissive );
			rules.insert( QStringLiteral( "distinctLtex" ), maskCache.byForm.size() );
			rules.insert( QStringLiteral( "roughnessDefault" ), 1.0 );
			rules.insert( QStringLiteral( "metallicDefault" ), 0.0 );
			t.insert( QStringLiteral( "maskRules" ), rules );
		}""")

# the census line the driver prints
rep("""		r << QString( "cover %1" ).arg( opts.cover.cover ? 1 : 0 );""",
"""		r << QString( "cover %1" ).arg( opts.cover.cover ? 1 : 0 );
		r << QString( "coverIn %1" ).arg( opts.coverInColor
			? QStringLiteral( "color" ) : QStringLiteral( "mask" ) );
		r << QString( "sheets %1" ).arg( 3 + ( opts.height ? 1 : 0 ) + ( wantEmissive ? 1 : 0 ) );
		r << QString( "emissive %1" ).arg( wantEmissive
			? QStringLiteral( "present" ) : QStringLiteral( "none" ) );
		/* THE RULE CENSUS, on one physical line of key=value tokens, because
		 * report lines in this tree are parsed by keyword and never by field
		 * position. Every word is WRITTEN and every one of them MOVES with the
		 * corpus (the three rules of 2026-09-04 21:33). */
		r << QString( "maskPbrm %1" ).arg( maskCache.ruleCounts[LODGEN_MASK_PBRM] );
		r << QString( "maskLegacyInverted %1" ).arg( maskCache.ruleCounts[LODGEN_MASK_LEGACY_INVERTED] );
		r << QString( "maskNoneDefault %1" ).arg( maskCache.ruleCounts[LODGEN_MASK_NONE] );
		r << QString( "maskRoughMaps %1" ).arg( maskCache.withRoughnessMap );
		r << QString( "maskMetalMaps %1" ).arg( maskCache.withMetallicMap );
		r << QString( "maskEmissiveMaps %1" ).arg( maskCache.withEmissive );
		r << QString( "maskDistinctLtex %1" ).arg( maskCache.byForm.size() );
		r << QString( "maskLayerRefs %1" ).arg( layerFormsSeen );""")

open(p, 'w', encoding='utf-8', newline='').write(s)
b = open(p, 'rb').read()
print('CR', b.count(b'\r'), 'LF', b.count(b'\n'))
