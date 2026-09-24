h = 'src/lodgen.h'
s = open(h, 'r', encoding='utf-8', newline='').read()


def rep(fname, a, b, n=1):
    global s
    assert s.count(a) == n, (fname, a[:70], s.count(a))
    s = s.replace(a, b)


rep(h, """/*! Resolve one material's mask contribution.
 *
 *  `matName` is a `.bgsm` / `.bgem` / `.pbrm` game path and may be empty;
 *  `specularTex` is the vanilla slot-7 `_s` map the shape or the TXST names and
 *  may be empty; `smoothness` is the shape's or the material's own constant.
 *  A same-name `.pbrm` beside a `.bgsm` wins, which is the discovery rule the
 *  renderer already uses (BSShaderLightingProperty::resolvePbrm), so an asset
 *  that previews as PBR in this application bakes as PBR too.
 *
 *  Never fails: with nothing to read it answers LODGEN_MASK_NONE, roughness
 *  1.0, metallic 0, and says so. */
void lodgenResolveMaterialMask( const QString & dataRoot, const QString & matName,
	const QString & specularTex, float smoothness, LodgenMaterialMask * out );""",
    """/*! Resolve one material's mask contribution.
 *
 *  `matName` is a `.bgsm` / `.bgem` / `.pbrm` game path and may be empty;
 *  `specularTex` is the vanilla slot-7 `_s` map the shape or the TXST names and
 *  may be empty; `smoothness` is the shape's or the material's own constant.
 *
 *  WHERE A PBRM IS LOOKED FOR, and why there are two places. A same-name `.pbrm`
 *  beside a `.bgsm` wins, which is the discovery rule the renderer already uses
 *  (`BSShaderLightingProperty::resolvePbrm`), so an asset that previews as PBR
 *  in this application bakes as PBR. But **more than half of Fallout 4's
 *  landscape textures name no material at all** -- their TXST carries TX00 and
 *  TX07 and nothing else -- and without a second place to look, those could
 *  never be given a PBRM by anybody. So when there is no material, `diffuseTex`
 *  supplies the stem, through `lodmSourceCandidate()`'s own convention: the
 *  diffuse path with `textures\\` swapped for `materials\\` and the extension
 *  replaced. That is the same rule a source `.lodm` is already found by, which
 *  is what keeps ONE convention rather than two.
 *
 *  Never fails: with nothing to read it answers LODGEN_MASK_NONE, roughness
 *  1.0, metallic 0, and says so. */
void lodgenResolveMaterialMask( const QString & dataRoot, const QString & matName,
	const QString & specularTex, float smoothness, LodgenMaterialMask * out,
	const QString & diffuseTex = QString() );""")

open(h, 'w', encoding='utf-8', newline='').write(s)

c = 'src/lodgen.cpp'
s = open(c, 'r', encoding='utf-8', newline='').read()

rep(c, """void lodgenResolveMaterialMask( const QString & dataRoot, const QString & matName,
	const QString & specularTex, float smoothness, LodgenMaterialMask * out )
{
	if ( !out )
		return;
	*out = LodgenMaterialMask();

	/* Arm 1: a PBRM. The candidate is the material itself when it already names
	 * one, else the same stem with `.pbrm` -- the renderer's own discovery rule
	 * (glproperty.cpp resolvePbrm), so an asset that previews as PBR here bakes
	 * as PBR. A malformed PBRM leaves the legacy material in charge, exactly as
	 * the renderer does, rather than failing the layer. */
	QString spec = specularTex;
	float smooth = smoothness;
	QString pbrmCand;
	if ( !matName.isEmpty() ) {
		QString mp = matName;
		mp.replace( QChar( '\\\\' ), QChar( '/' ) );
		if ( !mp.startsWith( QStringLiteral( "materials/" ), Qt::CaseInsensitive ) )
			mp.prepend( QStringLiteral( "materials/" ) );
		if ( mp.endsWith( QStringLiteral( ".pbrm" ), Qt::CaseInsensitive ) )
			pbrmCand = mp;
		else if ( mp.endsWith( QStringLiteral( ".bgsm" ), Qt::CaseInsensitive )
			|| mp.endsWith( QStringLiteral( ".bgem" ), Qt::CaseInsensitive ) )
			pbrmCand = mp.left( mp.length() - 5 ) + QStringLiteral( ".pbrm" );

		if ( !pbrmCand.isEmpty() ) {""",
    """void lodgenResolveMaterialMask( const QString & dataRoot, const QString & matName,
	const QString & specularTex, float smoothness, LodgenMaterialMask * out,
	const QString & diffuseTex )
{
	if ( !out )
		return;
	*out = LodgenMaterialMask();

	/* Arm 1: a PBRM. A malformed one leaves the legacy material in charge,
	 * exactly as the renderer does, rather than failing the layer. */
	QString spec = specularTex;
	float smooth = smoothness;
	QString pbrmCand;
	QString mp = matName;
	if ( !mp.isEmpty() ) {
		mp.replace( QChar( '\\\\' ), QChar( '/' ) );
		/* A TXST's MNAM is sometimes an ABSOLUTE authoring path -- measured in
		 * the shipped Commonwealth: `c:/projects/fallout4/build/pc/data/
		 * materials/landscape/rocks/rockriverstones_wet.bgsm`. Cut to the last
		 * `materials/` component, or the read is looked for under a folder that
		 * only ever existed on Bethesda's build machine. */
		const int mi = mp.lastIndexOf( QStringLiteral( "materials/" ), -1, Qt::CaseInsensitive );
		if ( mi > 0 )
			mp.remove( 0, mi );
		else if ( !mp.startsWith( QStringLiteral( "materials/" ), Qt::CaseInsensitive ) )
			mp.prepend( QStringLiteral( "materials/" ) );
		if ( mp.endsWith( QStringLiteral( ".pbrm" ), Qt::CaseInsensitive ) )
			pbrmCand = mp;
		else if ( mp.endsWith( QStringLiteral( ".bgsm" ), Qt::CaseInsensitive )
			|| mp.endsWith( QStringLiteral( ".bgem" ), Qt::CaseInsensitive ) )
			pbrmCand = mp.left( mp.length() - 5 ) + QStringLiteral( ".pbrm" );
	} else if ( !diffuseTex.isEmpty() ) {
		/* No material to hang a sibling off. `lodmSourceCandidate` already knows
		 * how to turn a diffuse into a material-folder stem, and it is the SAME
		 * convention a source .lodm is found by, so the two cannot disagree
		 * about where a user puts an override. */
		const QString lodm = lodmSourceCandidate( QString(), diffuseTex );
		if ( lodm.endsWith( QStringLiteral( ".lodm" ), Qt::CaseInsensitive ) ) {
			pbrmCand = lodm.left( lodm.length() - 5 ) + QStringLiteral( "pbrm" );
			pbrmCand.replace( QChar( '\\\\' ), QChar( '/' ) );
		}
	}
	{
		if ( !pbrmCand.isEmpty() ) {""")

rep(c, """					return;
				}
			}
		}

		/* No PBRM: the legacy material's OWN slots win over whatever the caller""",
    """					return;
				}
			}
		}
	}
	if ( !matName.isEmpty() ) {
		/* No PBRM: the legacy material's OWN slots win over whatever the caller""")

# the terrain resolver hands the diffuse through
rep(c, """			lodgenResolveMaterialMask( dataRoot, ts.material, ts.specular, 1.0f, &m.mat );""",
    """			lodgenResolveMaterialMask( dataRoot, ts.material, ts.specular, 1.0f, &m.mat,
				ts.diffuse );""")

open(c, 'w', encoding='utf-8', newline='').write(s)
for f in (h, c):
    b = open(f, 'rb').read()
    print(f, 'CR', b.count(b'\r'), 'LF', b.count(b'\n'))
