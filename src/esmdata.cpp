/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "esmdata.h"

#include "esmfile.hpp"

#include <climits>
#include <cstring>
#include <functional>

/* Group-tree conventions, measured against Fallout4.esm (2026-08-31):
 * only GRUP entries carry `children`; a RECORD's child group is its NEXT
 * SIBLING — a GRUP whose label (the `flags` field) equals the record's form
 * ID. A group entry's `formID` field holds the GROUP TYPE: 1 = world
 * children, 4/5 = exterior block/sub-block, 6 = cell children. The
 * worldspace's persistent cell sits DIRECTLY under the type-1 group (its
 * XCLC still says (0,0)); real exterior cells sit under sub-blocks.
 */

namespace
{

constexpr unsigned int GRUP = 0x50555247U;

QString fieldString( const ESMFile::ESMField & f )
{
	// zero-terminated within the field
	const char * p = reinterpret_cast<const char *>( f.data() );
	size_t n = f.size();
	while ( n > 0 && p[n - 1] == '\0' )
		n--;
	return QString::fromLatin1( p, qsizetype( n ) );
}

} // namespace

EsmWorld::EsmWorld() = default;
EsmWorld::~EsmWorld() = default;

bool EsmWorld::load( const QString & esmPath, quint32 worldspaceFormID, QString * error )
{
	try {
		esm = std::make_unique<ESMFile>( esmPath.toLocal8Bit().constData() );
		wsForm = worldspaceFormID;
		const ESMFile::ESMRecord & w = esm->getRecord( wsForm );
		if ( !( w == "WRLD" ) ) {
			if ( error )
				*error = QString( "form %1 is not a WRLD record" ).arg( wsForm, 8, 16, QChar( '0' ) );
			return false;
		}
		{
			ESMFile::ESMField f( *esm, w );
			while ( f.next() ) {
				if ( f == "EDID" ) {
					wsEdid = fieldString( f );
				} else if ( f == "DNAM" && f.size() >= 8 ) {
					defLandH = f.readFloat();
					defWaterH = f.readFloat();
				}
			}
		}
		indexWorldspace();
		if ( cellIndex.isEmpty() ) {
			if ( error )
				*error = QStringLiteral( "worldspace has no indexed exterior cells" );
			return false;
		}
		if ( error )
			error->clear();
		return true;
	} catch ( std::exception & e ) {
		if ( error )
			*error = QString::fromLatin1( e.what() );
		return false;
	}
}

void EsmWorld::indexWorldspace()
{
	const ESMFile::ESMRecord & w = esm->getRecord( wsForm );
	const ESMFile::ESMRecord * wg = w.next ? esm->findRecord( w.next ) : nullptr;
	if ( !wg || wg->type != GRUP || wg->flags != wsForm )
		return;

	// walk the whole world-children subtree; classify each CELL by the group
	// TYPE of its immediate parent group
	std::function<void( unsigned int )> walk = [&]( unsigned int id ) {
		while ( id ) {
			const ESMFile::ESMRecord * r = esm->findRecord( id );
			if ( !r )
				return;
			if ( r->type != GRUP && *r == "CELL" ) {
				int cx = 0, cy = 0;
				bool haveGrid = false;
				{
					ESMFile::ESMField f( *esm, *r );
					while ( f.next() ) {
						if ( f == "XCLC" && f.size() >= 8 ) {
							cx = int( f.readInt32() );
							cy = int( f.readInt32() );
							haveGrid = true;
						}
					}
				}
				// the cell's own child group, if any, follows as a sibling
				quint32 group = 0;
				const ESMFile::ESMRecord * cg = r->next ? esm->findRecord( r->next ) : nullptr;
				if ( cg && cg->type == GRUP && cg->flags == r->formID )
					group = r->next;
				// classify: parent group type 1 = the persistent cell
				const ESMFile::ESMRecord * pg = esm->findRecord( r->parent );
				const bool underWorldGroup = pg && pg->type == GRUP && pg->formID == 1;
				if ( underWorldGroup ) {
					persistentCellGroup = group;
				} else if ( haveGrid ) {
					CellEntry e;
					e.cellForm = r->formID;
					e.childGroup = group;
					cellIndex.insert( qMakePair( cx, cy ), e );
				}
			}
			if ( r->children )
				walk( r->children );
			id = r->next;
		}
	};
	walk( wg->children );
}

bool EsmWorld::cellWater( int cx, int cy, float & height ) const
{
	/* Only cells with an EXPLICIT water height make LOD water quads —
	 * measured on Commonwealth.4.-20.24.BTR: the chunk's two water shapes
	 * sit exactly at the explicit XCLW heights (7250, 10000), and its
	 * has-water-at-default cells get NO quads (the worldspace's NAM4 LOD
	 * water plane serves those). Treating sentinel XCLW as the default
	 * height produced a third shape vanilla does not have. */
	height = defWaterH;
	auto it = cellIndex.constFind( qMakePair( cx, cy ) );
	if ( it == cellIndex.constEnd() )
		return false;
	const ESMFile::ESMRecord * cr = esm->findRecord( it->cellForm );
	if ( !cr )
		return false;
	bool hasWater = false;
	bool explicitHeight = false;
	ESMFile::ESMField f( *esm, *cr );
	while ( f.next() ) {
		if ( f == "DATA" && f.size() >= 2 ) {
			hasWater = ( f.readUInt16() & 0x0002 ) != 0;
		} else if ( f == "XCLW" && f.size() >= 4 ) {
			const quint32 raw = f.readUInt32();
			if ( raw != 0xFF7FFFFFU && raw != 0x7F7FFFFFU && raw != 0x4F7FFFC9U ) {
				float v;
				std::memcpy( &v, &raw, 4 );
				height = v;
				explicitHeight = true;
			}
		}
	}
	return hasWater && explicitHeight;
}

void EsmWorld::cellBounds( int & minX, int & minY, int & maxX, int & maxY ) const
{
	minX = minY = INT_MAX;
	maxX = maxY = INT_MIN;
	for ( auto it = cellIndex.constBegin(); it != cellIndex.constEnd(); ++it ) {
		minX = qMin( minX, it.key().first );
		maxX = qMax( maxX, it.key().first );
		minY = qMin( minY, it.key().second );
		maxY = qMax( maxY, it.key().second );
	}
}

bool EsmWorld::hasCell( int cx, int cy ) const
{
	return cellIndex.contains( qMakePair( cx, cy ) );
}

bool EsmWorld::land( int cx, int cy, EsmLand & out ) const
{
	out = EsmLand();
	out.cellX = cx;
	out.cellY = cy;
	auto it = cellIndex.constFind( qMakePair( cx, cy ) );
	if ( it == cellIndex.constEnd() || !it->childGroup )
		return false;
	// find the LAND record in the cell's child group subtree
	quint32 landForm = 0;
	std::function<void( unsigned int )> walk = [&]( unsigned int id ) {
		while ( id && !landForm ) {
			const ESMFile::ESMRecord * r = esm->findRecord( id );
			if ( !r )
				return;
			if ( r->type != GRUP && *r == "LAND" )
				landForm = r->formID;
			if ( r->children )
				walk( r->children );
			id = r->next;
		}
	};
	const ESMFile::ESMRecord * cg = esm->findRecord( it->childGroup );
	if ( !cg )
		return false;
	walk( cg->children );
	if ( !landForm )
		return false;

	const ESMFile::ESMRecord & lr = esm->getRecord( landForm );
	ESMFile::ESMField f( *esm, lr );
	int pendingQuadrant = -1;       // set by ATXT, consumed by the next VTXT
	while ( f.next() ) {
		if ( f == "BTXT" && f.size() >= 8 ) {
			const quint32 ltex = f.readUInt32();
			const int quadrant = int( f.readUInt8() );
			if ( quadrant >= 0 && quadrant < 4 )
				out.baseTex[quadrant] = ltex;
		} else if ( f == "ATXT" && f.size() >= 8 ) {
			const quint32 ltex = f.readUInt32();
			const int quadrant = int( f.readUInt8() );
			if ( quadrant >= 0 && quadrant < 4 ) {
				EsmLandLayer layer;
				layer.ltex = ltex;
				std::memset( layer.opacity, 0, sizeof( layer.opacity ) );
				out.layers[quadrant].append( layer );
				pendingQuadrant = quadrant;
			} else {
				pendingQuadrant = -1;
			}
		} else if ( f == "VTXT" && pendingQuadrant >= 0
			&& !out.layers[pendingQuadrant].isEmpty() ) {
			EsmLandLayer & layer = out.layers[pendingQuadrant].last();
			const size_t entries = f.size() / 8;
			for ( size_t e = 0; e < entries; e++ ) {
				const quint16 posn = f.readUInt16();
				(void) f.readUInt16();
				const float opacity = f.readFloat();
				if ( posn <= 288 )
					layer.opacity[posn / 17][posn % 17] = opacity;
			}
			pendingQuadrant = -1;
		} else if ( f == "VHGT" && f.size() >= 4 + 33 * 33 ) {
			/* VHGT: float base + 33x33 signed byte deltas, times 8 game
			 * units. Column 0 of each row offsets from the PREVIOUS row's
			 * column 0; other columns accumulate along the row. */
			const float base = f.readFloat();
			const signed char * d = reinterpret_cast<const signed char *>( f.data() + 4 );
			float rowStart = base;
			for ( int row = 0; row < 33; row++ ) {
				rowStart += float( d[row * 33] );
				float v = rowStart;
				out.heights[row][0] = v * 8.0f;
				for ( int col = 1; col < 33; col++ ) {
					v += float( d[row * 33 + col] );
					out.heights[row][col] = v * 8.0f;
				}
			}
			out.valid = true;
		}
	}
	return out.valid;
}

QVector<EsmRefr> EsmWorld::refrsInGroup( quint32 groupID ) const
{
	QVector<EsmRefr> out;
	if ( !groupID )
		return out;
	std::function<void( unsigned int )> walk = [&]( unsigned int id ) {
		while ( id ) {
			const ESMFile::ESMRecord * r = esm->findRecord( id );
			if ( !r )
				return;
			if ( r->type != GRUP && *r == "REFR" ) {
				EsmRefr ref;
				ref.formID = r->formID;
				ref.initiallyDisabled = ( r->flags & 0x00000800 ) != 0;
				ref.deleted = ( r->flags & 0x00000020 ) != 0;
				ESMFile::ESMField f( *esm, *r );
				while ( f.next() ) {
					if ( f == "NAME" && f.size() >= 4 ) {
						ref.base = f.readUInt32();
					} else if ( f == "DATA" && f.size() >= 24 ) {
						for ( int i = 0; i < 3; i++ )
							ref.pos[i] = f.readFloat();
						for ( int i = 0; i < 3; i++ )
							ref.rot[i] = f.readFloat();
					} else if ( f == "XSCL" && f.size() >= 4 ) {
						ref.scale = f.readFloat();
					}
				}
				if ( ref.base ) {
					const ESMFile::ESMRecord * br = esm->findRecord( ref.base );
					if ( br )
						ref.baseType = br->type;
				}
				out.append( ref );
			}
			if ( r->children )
				walk( r->children );
			id = r->next;
		}
	};
	const ESMFile::ESMRecord * g = esm->findRecord( groupID );
	if ( g )
		walk( g->children );
	return out;
}

QVector<EsmRefr> EsmWorld::refrs( int cx, int cy ) const
{
	auto it = cellIndex.constFind( qMakePair( cx, cy ) );
	if ( it == cellIndex.constEnd() )
		return {};
	return refrsInGroup( it->childGroup );
}

const QVector<EsmRefr> & EsmWorld::persistentRefrs() const
{
	if ( !persistentCacheBuilt ) {
		persistentCache = refrsInGroup( persistentCellGroup );
		persistentCacheBuilt = true;
	}
	return persistentCache;
}

QVector<EsmRefr> EsmWorld::persistentRefrsIn( float minX, float minY, float maxX, float maxY ) const
{
	QVector<EsmRefr> out;
	for ( const EsmRefr & r : persistentRefrs() ) {
		if ( r.pos[0] >= minX && r.pos[0] < maxX && r.pos[1] >= minY && r.pos[1] < maxY )
			out.append( r );
	}
	return out;
}

const EsmLodBase & EsmWorld::lodBase( quint32 baseFormID ) const
{
	auto it = lodBaseCache.constFind( baseFormID );
	if ( it != lodBaseCache.constEnd() )
		return *it;

	EsmLodBase b;
	b.formID = baseFormID;
	const ESMFile::ESMRecord * br = esm->findRecord( baseFormID );
	if ( br && br->type != GRUP ) {
		b.type = br->type;
		ESMFile::ESMField f( *esm, *br );
		while ( f.next() ) {
			if ( f == "MNAM" && *br == "STAT" ) {
				/* STAT MNAM: 4 x 260-byte entries, each a zero-terminated
				 * mesh path followed by junk (wbDefinitionsFO4). */
				const size_t n = f.size();
				for ( int level = 0; level < 4; level++ ) {
					const size_t o = size_t( level ) * 260;
					if ( o >= n )
						break;
					const char * p = reinterpret_cast<const char *>( f.data() + o );
					const size_t maxLen = qMin<size_t>( 260, n - o );
					size_t len = 0;
					while ( len < maxLen && p[len] )
						len++;
					if ( len ) {
						b.models[level] = QString::fromLatin1( p, qsizetype( len ) );
						b.hasLod = true;
					}
				}
			} else if ( f == "DNAM" && *br == "STAT" && f.size() >= 16 ) {
				(void) f.readFloat();       // max angle
				(void) f.readUInt32();      // direction material
				b.leafAmplitude = f.readFloat();
				b.leafFrequency = f.readFloat();
			} else if ( f == "CNAM" && *br == "TREE" && f.size() >= 48 ) {
				b.trunkFlexibility = f.readFloat();
				b.branchFlexibility = f.readFloat();
				// 8 unknown floats, then leaf amplitude + frequency
				for ( int i = 0; i < 8; i++ )
					(void) f.readFloat();
				b.leafAmplitude = f.readFloat();
				b.leafFrequency = f.readFloat();
			} else if ( f == "MODL" && *br == "TREE" ) {
				// a TREE's LOD comes from the model naming convention; keep
				// the model path so the generator can derive _lod variants
				b.models[0] = fieldString( f );
			}
		}
		// TREE records always participate in LOD when flagged Has Distant LOD
		if ( *br == "TREE" && ( br->flags & 0x00008000 ) )
			b.hasLod = true;
	}
	auto ins = lodBaseCache.insert( baseFormID, b );
	return *ins;
}

void EsmWorld::ltexTextures( quint32 ltexForm, QString & diffuse, QString & normal ) const
{
	auto it = ltexCache.constFind( ltexForm );
	if ( it != ltexCache.constEnd() ) {
		diffuse = it->first;
		normal = it->second;
		return;
	}
	diffuse.clear();
	normal.clear();
	const ESMFile::ESMRecord * lr = esm->findRecord( ltexForm );
	if ( lr && *lr == "LTEX" ) {
		quint32 txst = 0;
		{
			ESMFile::ESMField f( *esm, *lr );
			while ( f.next() )
				if ( f == "TNAM" && f.size() >= 4 )
					txst = f.readUInt32();
		}
		const ESMFile::ESMRecord * tr = txst ? esm->findRecord( txst ) : nullptr;
		if ( tr && *tr == "TXST" ) {
			ESMFile::ESMField f( *esm, *tr );
			while ( f.next() ) {
				if ( f == "TX00" )
					diffuse = fieldString( f );
				else if ( f == "TX01" )
					normal = fieldString( f );
			}
		}
	}
	ltexCache.insert( ltexForm, qMakePair( diffuse, normal ) );
}

const QVector<EsmScolPart> & EsmWorld::scolParts( quint32 formID ) const
{
	auto it = scolCache.constFind( formID );
	if ( it != scolCache.constEnd() )
		return *it;

	QVector<EsmScolPart> parts;
	const ESMFile::ESMRecord * r = esm->findRecord( formID );
	if ( r && r->type != GRUP && *r == "SCOL" ) {
		/* wbDefinitionsFO4 SCOL: repeating [ONAM part base, DATA placement
		 * array], each placement 28 bytes: pos XYZ, rot XYZ (radians),
		 * scale — the part's copies in the collection's local space. */
		ESMFile::ESMField f( *esm, *r );
		while ( f.next() ) {
			if ( f == "ONAM" && f.size() >= 4 ) {
				EsmScolPart p;
				p.base = f.readUInt32();
				parts.append( p );
			} else if ( f == "DATA" && !parts.isEmpty() ) {
				const size_t n = f.size() / 28;
				for ( size_t i = 0; i < n; i++ ) {
					EsmScolPlacement pl;
					for ( int k = 0; k < 3; k++ )
						pl.pos[k] = f.readFloat();
					for ( int k = 0; k < 3; k++ )
						pl.rot[k] = f.readFloat();
					pl.scale = f.readFloat();
					parts.last().placements.append( pl );
				}
			}
		}
	}
	return *scolCache.insert( formID, parts );
}

QVector<QPair<quint32, QString>> EsmWorld::listWorldspaces( const QString & esmPath, QString * error )
{
	QVector<QPair<quint32, QString>> out;
	try {
		ESMFile esm( esmPath.toLocal8Bit().constData() );
		const ESMFile::ESMRecord * r0 = esm.findRecord( 0U );
		std::function<void( unsigned int )> walk = [&]( unsigned int id ) {
			while ( id ) {
				const ESMFile::ESMRecord * r = esm.findRecord( id );
				if ( !r )
					return;
				if ( r->type != GRUP && *r == "WRLD" ) {
					QString edid;
					ESMFile::ESMField f( esm, *r );
					while ( f.next() )
						if ( f == "EDID" )
							edid = fieldString( f );
					out.append( qMakePair( quint32( r->formID ), edid ) );
				}
				if ( r->children )
					walk( r->children );
				id = r->next;
			}
		};
		if ( r0 )
			walk( r0->next );
		if ( error )
			error->clear();
	} catch ( std::exception & e ) {
		if ( error )
			*error = QString::fromLatin1( e.what() );
	}
	return out;
}
