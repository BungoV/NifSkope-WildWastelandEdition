h = 'src/lodinative.h'
s = open(h, encoding='utf-8', newline='').read()


def rep(o, n, c=1):
    global s
    assert s.count(o) == c, (s.count(o), o[:90])
    s = s.replace(o, n)


rep("""	Identity,       //!< hashed colour per placement identity (the stock channel 1 look)
	IdentityRaw,    //!< the identity's low byte as grey""",
    """	/*! v7: the GROUP, hashed (bungo 2026-09-18: "The houses should be one
	 *  object each though, for identity"). On a version-6 file there is no
	 *  group table and this falls back to the placement identity, which the
	 *  note line SAYS by name rather than drawing it silently. */
	Identity,
	Placement,      //!< the unique per-placement identity, hashed -- what `identity` drew before v7
	IdentityRaw,    //!< the identity's low byte as grey""")
open(h, 'w', encoding='utf-8', newline='').write(s)

p = 'src/lodinative.cpp'
s = open(p, encoding='utf-8', newline='').read()

rep("""	{ "identity", LodlChannel::Identity },
	{ "identityraw", LodlChannel::IdentityRaw },""",
    """	{ "identity", LodlChannel::Identity },
	{ "placement", LodlChannel::Placement },
	{ "identityraw", LodlChannel::IdentityRaw },""")

rep("""	const bool objectChannel = channel == LodlChannel::Identity
		|| channel == LodlChannel::IdentityRaw || channel == LodlChannel::Sky
		|| channel == LodlChannel::Ground || channel == LodlChannel::Seed
		|| channel == LodlChannel::Sway || channel == LodlChannel::SelfAo;""",
    """	/* v7: `sky` is a PER-VERTEX channel when the file carries the stream and a
	 * per-placement one when it does not. The decision is taken from what was
	 * READ, never from the version word alone, and the note line says which of
	 * the two it drew (root MISTAKES: name a channel from the WRITER of the file
	 * in front of you). */
	const bool skyPerVertex = ( channel == LodlChannel::Sky ) && !table.vertexSkyFirst.empty()
		&& !table.vertexSky.empty();
	const bool objectChannel = channel == LodlChannel::Identity
		|| channel == LodlChannel::Placement
		|| channel == LodlChannel::IdentityRaw || ( channel == LodlChannel::Sky && !skyPerVertex )
		|| channel == LodlChannel::Ground || channel == LodlChannel::Seed
		|| channel == LodlChannel::Sway || channel == LodlChannel::SelfAo;
	//! v7 read-back: how many placements `identity` had to fall back on, and how many drew a group
	qint64 groupDrawn = 0, groupFellBack = 0, skyVertSlices = 0, skyVertBytes = 0, skyVertMismatch = 0;""")

rep("""				case LodlChannel::Identity:
					hashColour( quint32( cold.identity ), placeChan );
					chanSeen( int( cold.identity ) );
					break;""",
    """				case LodlChannel::Identity:
					/* v7: the GROUP. A v6 file has no group table, so this
					 * FALLS BACK to the placement identity and the note line
					 * names the arm that served it. */
					if ( ii < table.group.size() ) {
						hashColour( quint32( table.group[ii] ) + 1u, placeChan );
						chanSeen( int( table.group[ii] ) );
						groupDrawn++;
					} else {
						hashColour( quint32( cold.identity ), placeChan );
						chanSeen( int( cold.identity ) );
						groupFellBack++;
					}
					break;
				case LodlChannel::Placement:
					hashColour( quint32( cold.identity ), placeChan );
					chanSeen( int( cold.identity ) );
					break;""")

# the per-vertex sky slice, beside the AO one
rep("""			const Vector3 pos( xyz[0] - origin[0], xyz[1] - origin[1], xyz[2] - origin[2] );
""",
    """			/* v7: the per-vertex SKY slice, found exactly as the AO slice above
			 * is -- same offsets shape, same "does its length match the drawn
			 * mesh" gate, so a slice cast for another slot's mesh is refused
			 * here rather than drawn as somebody else's numbers. */
			const quint8 * vskSlice = nullptr;
			quint32 vskFirstVertex = 0;
			if ( skyPerVertex && ii + 1 < table.vertexSkyFirst.size() ) {
				const quint32 f = table.vertexSkyFirst[ii], l = table.vertexSkyFirst[ii + 1];
				if ( l > f && l <= table.vertexSky.size() ) {
					auto rit = meshRange.find( meshId );
					if ( rit == meshRange.end() ) {
						quint32 lo = 0xFFFFFFFFu, hi = 0;
						const LodoMesh & mr = lib.meshes[meshId];
						for ( quint32 c = mr.clusterFirst; c < mr.clusterFirst + mr.clusterCount && c < lib.clusters.size(); c++ ) {
							lo = qMin( lo, lib.clusters[c].vertexBase );
							hi = qMax( hi, lib.clusters[c].vertexBase + quint32( lib.clusters[c].vertexCount ) );
						}
						rit = meshRange.insert( meshId, qMakePair( lo, hi > lo ? hi - lo : 0u ) );
					}
					if ( rit.value().second == l - f ) {
						vskSlice = table.vertexSky.data() + f;
						vskFirstVertex = rit.value().first;
						skyVertSlices++;
						skyVertBytes += qint64( l - f );
					} else {
						skyVertMismatch++;
					}
				}
			}
			const Vector3 pos( xyz[0] - origin[0], xyz[1] - origin[1], xyz[2] - origin[2] );
""")

rep("""					nb.withColour = wantAo || objectChannel;""",
    """					nb.withColour = wantAo || objectChannel || skyPerVertex;""")

rep("""					} else if ( objectChannel ) {
						for ( int k = 0; k < 3; k++ )
							o.chan[k] = placeChan[k];
					} else if ( vaoSlice ) {""",
    """					} else if ( objectChannel ) {
						for ( int k = 0; k < 3; k++ )
							o.chan[k] = placeChan[k];
					} else if ( skyPerVertex ) {
						/* v7: one byte a library vertex. A placement with no
						 * slice draws its flat 0x11 byte rather than white, so
						 * an absent slice reads as "no stream here", not as
						 * "fully open sky". */
						const quint8 sk = vskSlice ? vskSlice[sv.libIndex - vskFirstVertex] : inst.sky;
						o.chan[0] = o.chan[1] = o.chan[2] = float( sk ) / 255.0f;
						chanSeen( int( sk ) );
					} else if ( vaoSlice ) {""")

# the note lines
rep("""	if ( objectChannel )
		note << QString( "WW_LODL_CHANNEL=%1: %2 from %3, %L4 %5 read; %6" )""",
    """	if ( skyPerVertex )
		note << QString( "WW_LODL_CHANNEL=sky: the PER-VERTEX SKY STREAM (.lodi v7 0x110) from %1, "
				"%L2 bytes over %L3 slices, %L4 values read; %5%6" )
			.arg( QFileInfo( lodiPath ).fileName() )
			.arg( skyVertBytes ).arg( skyVertSlices ).arg( chanCount )
			.arg( chanCount == 0 ? QStringLiteral( "nothing was drawn" )
				: chanLo == chanHi
					? QString( "constant %1" ).arg( chanLo )
					: QString( "min %1, max %2, mean %3" ).arg( chanLo ).arg( chanHi )
						.arg( chanSum / double( chanCount ), 0, 'f', 3 ) )
			.arg( skyVertMismatch ? QString( "; %1 slice(s) did not match the drawn mesh and drew the 0x11 byte" )
				.arg( skyVertMismatch ) : QString() );
	if ( channel == LodlChannel::Sky && !skyPerVertex )
		note << QString( "WW_LODL_CHANNEL=sky: no per-vertex stream in %1 (a version-%2 file), so the "
				"PLACEMENT BYTE (.lodi 0x11) served it" )
			.arg( QFileInfo( lodiPath ).fileName() ).arg( header.version );
	if ( channel == LodlChannel::Identity )
		note << ( groupDrawn
			? QString( "WW_LODL_CHANNEL=identity: the GROUP (.lodi v7 0x100) on %L1 placements, "
					"%L2 groups in the file" ).arg( groupDrawn ).arg( header.groupCount )
			: QString( "WW_LODL_CHANNEL=identity: no group table in %1 (a version-%2 file), so the "
					"PLACEMENT IDENTITY served it on %L3 placements" )
				.arg( QFileInfo( lodiPath ).fileName() ).arg( header.version ).arg( groupFellBack ) );
	if ( objectChannel )
		note << QString( "WW_LODL_CHANNEL=%1: %2 from %3, %L4 %5 read; %6" )""")

rep("""				: channel == LodlChannel::Identity
					? QStringLiteral( "the placement identity, hashed to colour (the stock channel 1 palette)" )
				: channel == LodlChannel::IdentityRaw""",
    """				: channel == LodlChannel::Identity
					? QStringLiteral( "the group, hashed to colour (the stock channel 1 palette)" )
				: channel == LodlChannel::Placement
					? QStringLiteral( "the placement identity, hashed to colour (the stock channel 1 palette)" )
				: channel == LodlChannel::IdentityRaw""")

open(p, 'w', encoding='utf-8', newline='').write(s)
print('viewer spliced')
