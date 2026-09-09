/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "esmdata.h"

#include <cstdio>

#include "esmfile.hpp"

#include <climits>
#include <cstring>
#include <functional>
#include <algorithm>
#include <QSet>

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
	/* WW_ESM_TRACE=1: where load() is, on stderr, unbuffered. Put in for a
	 * FO76 plugin that blocked with zero CPU while listWorldspaces() on the
	 * same file returned at once; the trace says which step never returns. */
	const bool trace = qEnvironmentVariableIsSet( "WW_ESM_TRACE" );
	auto tr = [trace]( const char * what ) {
		if ( trace ) {
			std::fputs( what, stderr );
			std::fputc( 10, stderr );
			std::fflush( stderr );
		}
	};
	try {
		tr( "load: opening the plugin" );
		esm = std::make_unique<ESMFile>( esmPath.toLocal8Bit().constData() );
		tr( "load: plugin opened, looking up the worldspace" );
		wsForm = worldspaceFormID;
		const ESMFile::ESMRecord & w = esm->getRecord( wsForm );
		tr( "load: worldspace record found" );
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
				} else if ( f == "NAM2" && f.size() >= 4 ) {
					// the worldspace's default water type; cells override it
					// with XCWT. Commonwealth's is ExtOceanWater.
					defWaterType = f.readUInt32();
				}
			}
		}
		tr( "load: header fields read, indexing cells" );
		indexWorldspace();
		tr( "load: cells indexed" );
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

bool EsmWorld::cellWater( int cx, int cy, float & height,
	quint32 * typeForm ) const
{
	/* Only cells with an EXPLICIT water height make LOD water quads —
	 * measured on Commonwealth.4.-20.24.BTR: the chunk's two water shapes
	 * sit exactly at the explicit XCLW heights (7250, 10000), and its
	 * has-water-at-default cells get NO quads — but chunk 0,0 (the harbor,
	 * all 16 cells sentinel-XCLW + hasWater, default water 450) DOES get
	 * vanilla quads at exactly 450. The reconciling rule: hasWater with the
	 * resolved height (explicit XCLW, else the worldspace default), and the
	 * BUILDER emits a quad only where the water is exposed above the cell's
	 * terrain minimum — Sanctuary's default-height cells sit under 3000+
	 * terrain, the harbor's above the seabed. */
	height = defWaterH;
	if ( typeForm )
		*typeForm = defWaterType;
	auto it = cellIndex.constFind( qMakePair( cx, cy ) );
	if ( it == cellIndex.constEnd() )
		return false;
	const ESMFile::ESMRecord * cr = esm->findRecord( it->cellForm );
	if ( !cr )
		return false;
	bool hasWater = false;
	ESMFile::ESMField f( *esm, *cr );
	while ( f.next() ) {
		if ( f == "DATA" && f.size() >= 2 ) {
			hasWater = ( f.readUInt16() & 0x0002 ) != 0;
		} else if ( f == "XCWT" && f.size() >= 4 ) {
			if ( typeForm )
				*typeForm = f.readUInt32();
		} else if ( f == "XCLW" && f.size() >= 4 ) {
			const quint32 raw = f.readUInt32();
			if ( raw != 0xFF7FFFFFU && raw != 0x7F7FFFFFU && raw != 0x4F7FFFC9U ) {
				float v;
				std::memcpy( &v, &raw, 4 );
				height = v;
			}
		}
	}
	return hasWater;
}

/* Reproduces FO4CS FarFieldPluginReader.h WalkGroup / FarFieldHeightmapBake.h
 * exactly, because the loader compares the result against a constant:
 *   - only this worldspace's world-children GRUP, walked in FILE order
 *     (pre-order over children/next is the file order: a GRUP's children are
 *     stored contiguously after its header);
 *   - the first VHGT of each LAND only, its raw payload bytes post-inflate;
 *   - a LAND whose owning CELL carried no XCLC contributes nothing;
 *   - no de-duplication of repeated cells.
 * ESMField inflates compressed records itself and its window after next() is
 * the field payload, which is what land() already reads its floats from. */
quint64 EsmWorld::vhgtCorpusHash( int * landsHashed ) const
{
	quint64 h = Q_UINT64_C( 0xCBF29CE484222325 );
	int n = 0;
	const ESMFile::ESMRecord & w = esm->getRecord( wsForm );
	const ESMFile::ESMRecord * wg = w.next ? esm->findRecord( w.next ) : nullptr;
	if ( wg && wg->type == GRUP && wg->flags == wsForm ) {
		bool cellHasCoords = false;
		std::function<void( unsigned int )> walk = [&]( unsigned int id ) {
			while ( id ) {
				const ESMFile::ESMRecord * r = esm->findRecord( id );
				if ( !r )
					return;
				if ( r->type != GRUP && *r == "CELL" ) {
					cellHasCoords = false;
					ESMFile::ESMField f( *esm, *r );
					while ( f.next() )
						if ( f == "XCLC" && f.size() >= 8 )
							cellHasCoords = true;
				} else if ( r->type != GRUP && *r == "LAND" && cellHasCoords ) {
					ESMFile::ESMField f( *esm, *r );
					while ( f.next() ) {
						if ( f == "VHGT" ) {
							const unsigned char * p = f.getDataPtr();
							for ( size_t i = 0, sz = f.size(); i < sz; i++ ) {
								h ^= p[i];
								h *= Q_UINT64_C( 0x100000001B3 );
							}
							n++;
							break;   // first VHGT only, as the loader does
						}
					}
				}
				if ( r->children )
					walk( r->children );
				id = r->next;
			}
		};
		walk( wg->children );
	}
	if ( landsHashed )
		*landsHashed = n;
	return h;
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
			const quint32 ltex = esm->mapFormID( lr, f.readUInt32() );
			const int quadrant = int( f.readUInt8() );
			if ( quadrant >= 0 && quadrant < 4 )
				out.baseTex[quadrant] = ltex;
		} else if ( f == "ATXT" && f.size() >= 8 ) {
			const quint32 ltex = esm->mapFormID( lr, f.readUInt32() );
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
		} else if ( f == "VCLR" && f.size() >= 33 * 33 * 3 ) {
			// hand-painted vertex colours, multiplied into the ground by the
			// landscape shader; same 33x33 SW-origin grid as VHGT
			const unsigned char * d = f.data();
			for ( int row = 0; row < 33; row++ )
				for ( int col = 0; col < 33; col++ )
					for ( int k = 0; k < 3; k++ )
						out.colors[row][col][k] = d[( row * 33 + col ) * 3 + k];
			out.hasColors = true;
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
						ref.base = esm->mapFormID( *r, f.readUInt32() );
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
			} else if ( f == "MODL" && ( *br == "TREE" || *br == "STAT" ) ) {
				/* The base's own near model: what the impostor bake photographs
				 * (bungo, 2026-09-06: "the base is more detailed"). A TREE has
				 * no MNAM; its near model stands at level 0, as before. */
				b.model = fieldString( f );
				if ( *br == "TREE" )
					b.models[0] = b.model;
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
					txst = esm->mapFormID( *lr, f.readUInt32() );
		}
		const ESMFile::ESMRecord * tr = txst ? esm->findRecord( txst ) : nullptr;
		if ( tr && *tr == "TXST" ) {
			QString material;
			ESMFile::ESMField f( *esm, *tr );
			while ( f.next() ) {
				if ( f == "TX00" )
					diffuse = fieldString( f );
				else if ( f == "TX01" )
					normal = fieldString( f );
				else if ( f == "MNAM" )
					material = fieldString( f );
			}
			/* Material-backed TXSTs (common on landscape sets) carry no TX00
			 * — the textures live inside the referenced .bgsm. Hand the
			 * material path through; the texture loader resolves it. */
			if ( diffuse.isEmpty() && !material.isEmpty() )
				diffuse = material;
		}
	}
	ltexCache.insert( ltexForm, qMakePair( diffuse, normal ) );
}

static void esmFnvBytes( quint64 & h, const unsigned char * p, size_t n )
{
	for ( size_t i = 0; i < n; i++ ) {
		h ^= p[i];
		h *= Q_UINT64_C( 0x100000001B3 );
	}
}

static quint32 esmLeUInt32( const unsigned char * p )
{
	return quint32( p[0] ) | ( quint32( p[1] ) << 8 )
		| ( quint32( p[2] ) << 16 ) | ( quint32( p[3] ) << 24 );
}

//! Every indexed exterior cell, ascending y then x. Both corpus hashes below
//! walk this order so a load-order permutation that changes nothing effective
//! cannot change a hash and hard-refuse a good bake.
QVector<QPair<int, int>> EsmWorld::cellsAscending() const
{
	QVector<QPair<int, int>> keys;
	keys.reserve( cellIndex.size() );
	for ( auto it = cellIndex.constBegin(); it != cellIndex.constEnd(); ++it )
		keys.append( it.key() );
	std::sort( keys.begin(), keys.end(),
		[]( const QPair<int, int> & a, const QPair<int, int> & b ) {
			return a.second != b.second ? a.second < b.second : a.first < b.first;
		} );
	return keys;
}

//! The LAND record of one indexed cell, 0 when the cell carries none.
quint32 EsmWorld::landFormOf( int cx, int cy ) const
{
	auto it = cellIndex.constFind( qMakePair( cx, cy ) );
	if ( it == cellIndex.constEnd() || !it->childGroup )
		return 0;
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
	if ( cg )
		walk( cg->children );
	return landForm;
}

quint64 EsmWorld::vhgtCorpusHashSorted( int * landsHashed ) const
{
	quint64 h = Q_UINT64_C( 0xCBF29CE484222325 );
	int n = 0;
	for ( const QPair<int, int> & k : cellsAscending() ) {
		const quint32 landForm = landFormOf( k.first, k.second );
		if ( !landForm )
			continue;
		const ESMFile::ESMRecord & lr = esm->getRecord( landForm );
		ESMFile::ESMField f( *esm, lr );
		while ( f.next() ) {
			if ( f == "VHGT" ) {
				esmFnvBytes( h, f.getDataPtr(), f.size() );
				n++;
				break;      // first VHGT only, as vhgtCorpusHash() does
			}
		}
	}
	if ( landsHashed )
		*landsHashed = n;
	return h;
}

quint64 EsmWorld::paintCorpusHash() const
{
	quint64 h = Q_UINT64_C( 0xCBF29CE484222325 );
	QSet<quint32> ltexSeen;
	for ( const QPair<int, int> & k : cellsAscending() ) {
		const quint32 landForm = landFormOf( k.first, k.second );
		if ( !landForm )
			continue;
		const ESMFile::ESMRecord & lr = esm->getRecord( landForm );
		ESMFile::ESMField f( *esm, lr );
		while ( f.next() ) {
			const bool isBase = ( f == "BTXT" );
			if ( !isBase && !( f == "ATXT" ) && !( f == "VTXT" ) )
				continue;
			esmFnvBytes( h, f.getDataPtr(), f.size() );
			if ( ( isBase || f == "ATXT" ) && f.size() >= 4 ) {
				const quint32 id = esm->mapFormID( lr, esmLeUInt32( f.getDataPtr() ) );
				if ( id )
					ltexSeen.insert( id );
			}
		}
	}
	QVector<quint32> ltexIds( ltexSeen.constBegin(), ltexSeen.constEnd() );
	std::sort( ltexIds.begin(), ltexIds.end() );
	QSet<quint32> grasSeen;
	for ( quint32 id : ltexIds ) {
		const ESMFile::ESMRecord * lr = esm->findRecord( id );
		if ( !lr || lr->type == GRUP || !( *lr == "LTEX" ) )
			continue;
		ESMFile::ESMField f( *esm, *lr );
		while ( f.next() ) {
			const bool isGnam = ( f == "GNAM" );
			if ( !isGnam && !( f == "TNAM" ) )
				continue;
			esmFnvBytes( h, f.getDataPtr(), f.size() );
			if ( isGnam && f.size() >= 4 ) {
				const quint32 g = esm->mapFormID( *lr, esmLeUInt32( f.getDataPtr() ) );
				if ( g )
					grasSeen.insert( g );
			}
		}
	}
	QVector<quint32> grasIds( grasSeen.constBegin(), grasSeen.constEnd() );
	std::sort( grasIds.begin(), grasIds.end() );
	for ( quint32 id : grasIds ) {
		const ESMFile::ESMRecord * gr = esm->findRecord( id );
		if ( !gr || gr->type == GRUP || !( *gr == "GRAS" ) )
			continue;
		ESMFile::ESMField f( *esm, *gr );
		while ( f.next() )
			if ( f == "DATA" || f == "MODL" )
				esmFnvBytes( h, f.getDataPtr(), f.size() );
	}
	return h;
}

void EsmWorld::setGrassTintResolver( EsmGrassTintFn fn, void * user ) const
{
	/* The tint is folded into the cached T(L), so a DIFFERENT resolver
	 * invalidates it -- and installing the same one again must not, or a
	 * per-tile call would drop the cache 9,216 times over one worldspace. */
	if ( tintFn == fn && tintUser == user )
		return;
	tintFn = fn;
	tintUser = user;
	ltexCoverCache.clear();
}

bool EsmWorld::grass( quint32 grasForm, EsmGrass & out ) const
{
	auto it = grasCache.constFind( grasForm );
	if ( it != grasCache.constEnd() ) {
		out = *it;
		return out.form != 0;
	}
	EsmGrass g;
	const ESMFile::ESMRecord * r = esm->findRecord( grasForm );
	if ( r && r->type != GRUP && *r == "GRAS" ) {
		g.form = grasForm;
		ESMFile::ESMField f( *esm, *r );
		while ( f.next() ) {
			if ( f == "DATA" && f.size() >= 32 ) {
				/* GRAS DATA, 32 bytes in all 107 records of the shipped corpus.
				 * Offsets 3, 6..7 and 29..31 are stale slots (they co-vary in
				 * three fixed groups across the corpus — 4-byte fields now
				 * written 3, 2 and 1 bytes deep) and are read past, not used. */
				g.density = f.readUInt8();
				g.minSlope = f.readUInt8();
				g.maxSlope = f.readUInt8();
				(void) f.readUInt8();
				g.unitsFromWater = f.readUInt16();
				(void) f.readUInt16();
				g.waterType = f.readUInt32();
				g.positionRange = f.readFloat();
				g.heightRange = f.readFloat();
				g.colourRange = f.readFloat();
				g.wavePeriod = f.readFloat();
				g.flags = f.readUInt8();
			} else if ( f == "MODL" ) {
				g.model = fieldString( f );
			}
		}
		grasReads++;
	}
	grasCache.insert( grasForm, g );
	out = g;
	return g.form != 0;
}

/* LTEX -> GNAM -> GRAS, the chain the Creation Kit grows grass from, read here
 * for the first time. D is the density SUM over the links (an LTEX with four
 * grasses is four times as grassy, not the average); S weights each grass's
 * Max Slope by its density; T weights each grass's average colour the same way
 * but over the tint-BEARING density only, so a grass whose mesh or texture
 * cannot be resolved loses its vote on the colour and keeps its vote on D.
 *
 * The returned reference lives in a QHash and a later resolve can rehash it:
 * callers copy the value out (the paint loop fills a per-quadrant array once
 * on quadrant entry) rather than holding the reference across another call. */
const EsmLtexCover & EsmWorld::ltexCover( quint32 ltexForm, const QString & dataRoot ) const
{
	auto it = ltexCoverCache.constFind( ltexForm );
	if ( it != ltexCoverCache.constEnd() )
		return *it;
	EsmLtexCover c;
	c.resolved = true;
	const ESMFile::ESMRecord * lr = esm->findRecord( ltexForm );
	if ( lr && lr->type != GRUP && *lr == "LTEX" ) {
		c.exists = true;
		QVector<quint32> gnams;
		{
			ESMFile::ESMField f( *esm, *lr );
			while ( f.next() )
				if ( f == "GNAM" && f.size() >= 4 )
					gnams.append( esm->mapFormID( *lr, f.readUInt32() ) );
		}
		double dsum = 0.0, ssum = 0.0, tw = 0.0;
		double tsum[3] = { 0.0, 0.0, 0.0 };
		for ( quint32 gid : gnams ) {
			EsmGrass g;
			if ( !grass( gid, g ) ) {
				c.danglingGnam++;
				continue;
			}
			c.grasses++;
			dsum += double( g.density );
			ssum += double( g.density ) * double( g.maxSlope );
			float rgb[3] = { 0.0f, 0.0f, 0.0f };
			if ( tintFn && !g.model.isEmpty() && tintFn( g.model, dataRoot, tintUser, rgb ) ) {
				tw += double( g.density );
				for ( int k = 0; k < 3; k++ )
					tsum[k] += double( g.density ) * double( rgb[k] );
			} else {
				c.grassesWithoutTint++;
			}
		}
		c.density = quint16( qMin( dsum, 65535.0 ) );
		c.tintDensity = quint16( qMin( tw, 65535.0 ) );
		if ( dsum > 0.0 )
			c.maxSlope = float( ssum / dsum );
		if ( tw > 0.0 ) {
			for ( int k = 0; k < 3; k++ )
				c.tint[k] = float( tsum[k] / tw );
			c.hasTint = true;
		}
	}
	return *ltexCoverCache.insert( ltexForm, c );
}

const EsmCoverCensus & EsmWorld::coverCensus() const
{
	if ( censusBuilt )
		return census;
	censusBuilt = true;
	EsmCoverCensus c;
	std::function<void( unsigned int )> walk = [&]( unsigned int id ) {
		while ( id ) {
			const ESMFile::ESMRecord * r = esm->findRecord( id );
			if ( !r )
				return;
			if ( r->type != GRUP && *r == "LTEX" ) {
				c.ltexTotal++;
				int links = 0;
				ESMFile::ESMField f( *esm, *r );
				while ( f.next() )
					if ( f == "GNAM" )
						links++;
				c.gnamLinks += links;
				if ( links )
					c.ltexWithGnam++;
			} else if ( r->type != GRUP && *r == "GRAS" ) {
				c.grasTotal++;
				int dataSize = -1;
				ESMFile::ESMField f( *esm, *r );
				while ( f.next() )
					if ( f == "DATA" )
						dataSize = int( f.size() );
				if ( dataSize < 0 ) {
					c.grasWithoutData++;
				} else {
					if ( c.grasDataMin < 0 || dataSize < c.grasDataMin )
						c.grasDataMin = dataSize;
					if ( dataSize > c.grasDataMax )
						c.grasDataMax = dataSize;
				}
			}
			if ( r->children )
				walk( r->children );
			id = r->next;
		}
	};
	const ESMFile::ESMRecord * r0 = esm->findRecord( 0U );
	if ( r0 )
		walk( r0->next );
	census = c;
	return census;
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
				p.base = esm->mapFormID( *r, f.readUInt32() );
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
