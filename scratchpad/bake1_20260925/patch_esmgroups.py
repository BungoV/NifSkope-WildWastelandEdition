"""BAKE1: join later plugins' world/cell child groups into EsmWorld (src/esmdata.h/.cpp). Refuses unless every
anchor matches exactly once; LF files, CR count asserted unchanged. --check writes nothing."""
import sys
ROOT = 'E:/Projects/NifskopeWWE-bake1/src/'
CHECK = '--check' in sys.argv

def patch(name, edits):
    path = ROOT + name
    data = open(path, 'rb').read()
    cr0 = data.count(b'\r')
    for old, new in edits:
        n = data.count(old)
        assert n == 1, '%s: anchor matched %d times: %r' % (name, n, old[:60])
        data = data.replace(old, new)
    assert data.count(b'\r') == cr0, name + ': CR count moved'
    if CHECK:
        print(name, 'ok (check only)')
        return
    with open(path, 'wb') as f:
        f.write(data)
    print(name, 'patched')

H_OLD1 = b"""		quint32 childGroup = 0;   //!< the cell's type-6 group (0 if none)
"""
H_NEW1 = b"""		quint32 childGroup = 0;   //!< the cell's type-6 group (0 if none)
		//! type-6 groups a LATER plugin opened under this cell (its new REFRs);
		//! libfo76utils links them to no record, see indexWorldspace (BAKE1)
		QVector<quint32> extraGroups;
"""
H_OLD2 = b"""	quint32 persistentCellGroup = 0;
"""
H_NEW2 = b"""	quint32 persistentCellGroup = 0;
	quint32 persistentCellForm = 0;
	QVector<quint32> persistentExtraGroups;   //!< later plugins' persistent-cell groups (BAKE1)
	int extraWorldGroups = 0;                 //!< later plugins' world-children groups walked (BAKE1)
	int extraCellGroups = 0;                  //!< later plugins' cell-children groups joined (BAKE1)
"""

C_OLD1 = b"""				if ( underWorldGroup ) {
					persistentCellGroup = group;
				} else if ( haveGrid ) {"""
C_NEW1 = b"""				if ( underWorldGroup ) {
					/* the FIRST file's persistent cell; a later plugin overrides
					 * the record, and its groups join below (BAKE1) */
					if ( !persistentCellGroup ) {
						persistentCellGroup = group;
						persistentCellForm = r->formID;
					}
				} else if ( haveGrid ) {"""

C_OLD2 = b"""			if ( r->children )
				walk( r->children );
			id = r->next;
		}
	};
	walk( wg->children );
}
"""
C_NEW2 = b"""			if ( r->children )
				walk( r->children );
			id = r->next;
		}
	};
	walk( wg->children );

	/* LATER PLUGINS' GROUPS (lane BAKE1, 2026-09-25). libfo76utils merges a
	 * comma list record by record: an override replaces the record's DATA in
	 * place and is never linked into the later file's chain, so a plugin's own
	 * world-children group, and every cell-children group it opens under an
	 * OVERRIDDEN cell, hang from no record's `next`. The walk above therefore
	 * saw the first file's tree only: BNS Trees.esp's 22,827 new REFRs (21,073
	 * in the persistent cell, the rest under 755 overridden exterior cells)
	 * never reached a bake. Group ids are 0x80000000 | k, k dense from 0 across
	 * every file, so enumerate them once: a type-1 group labelled with this
	 * worldspace other than the one walked is walked the same way (its NEW
	 * cells index themselves), and every type-6 group labelled with an indexed
	 * cell, or the persistent cell, that is not that cell's own child group is
	 * joined to it in load order. A one-file load has no such group, so it is
	 * unchanged to the byte. An overriding REFR stays in the FIRST file's chain
	 * with the last file's data, so no REFR is read twice. */
	QHash<quint32, QVector<quint32>> cellGroups;
	QVector<quint32> worldGroups;
	for ( quint32 k = 0; k < 0x7FFFFFFFU; k++ ) {
		const quint32 id = 0x80000000U | k;
		const ESMFile::ESMRecord * g = esm->findRecord( id );
		if ( !g )
			break;
		if ( g->type != GRUP )
			continue;
		if ( g->formID == 1 && g->flags == wsForm && id != w.next )
			worldGroups.append( id );
		else if ( g->formID == 6 )
			cellGroups[g->flags].append( id );
	}
	for ( quint32 id : worldGroups ) {
		const ESMFile::ESMRecord * g = esm->findRecord( id );
		if ( g && g->children )
			walk( g->children );
	}
	extraWorldGroups = int( worldGroups.size() );
	extraCellGroups = 0;
	for ( auto it = cellIndex.begin(); it != cellIndex.end(); ++it ) {
		const auto gl = cellGroups.constFind( it->cellForm );
		if ( gl == cellGroups.constEnd() )
			continue;
		for ( quint32 gid : *gl )
			if ( gid != it->childGroup ) {
				it->extraGroups.append( gid );
				extraCellGroups++;
			}
	}
	if ( persistentCellForm ) {
		for ( quint32 gid : cellGroups.value( persistentCellForm ) )
			if ( gid != persistentCellGroup ) {
				persistentExtraGroups.append( gid );
				extraCellGroups++;
			}
	}
	if ( qEnvironmentVariableIsSet( "WW_ESM_TRACE" ) )
		std::fprintf( stderr, "index: %d later world group(s), %d later cell group(s) joined, %d persistent\\n",
			extraWorldGroups, extraCellGroups, int( persistentExtraGroups.size() ) );
}
"""

C_OLD3 = b"""	if ( it == cellIndex.constEnd() )
		return {};
	return refrsInGroup( it->childGroup );
}
"""
C_NEW3 = b"""	if ( it == cellIndex.constEnd() )
		return {};
	QVector<EsmRefr> out = refrsInGroup( it->childGroup );
	for ( quint32 g : it->extraGroups )
		out += refrsInGroup( g );
	return out;
}
"""
C_OLD4 = b"""		persistentCache = refrsInGroup( persistentCellGroup );
"""
C_NEW4 = b"""		persistentCache = refrsInGroup( persistentCellGroup );
		for ( quint32 g : persistentExtraGroups )
			persistentCache += refrsInGroup( g );
"""

patch('esmdata.h', [(H_OLD1, H_NEW1), (H_OLD2, H_NEW2)])
patch('esmdata.cpp', [(C_OLD1, C_NEW1), (C_OLD2, C_NEW2), (C_OLD3, C_NEW3), (C_OLD4, C_NEW4)])
