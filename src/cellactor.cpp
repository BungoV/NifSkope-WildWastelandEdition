/* lane PLACED1: placed actors, built at rest on the CPU. See cellactor.h for the rule. Record
 * layouts follow the published plugin format notes (the xEdit definitions). */

#include "cellactor.h"

#include "esmfile.hpp"
#include "lodgen.h"
#include "data/niftypes.h"
#include "model/nifmodel.h"

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>

namespace
{

constexpr unsigned int GRUP_TYPE = 0x50555247U;
constexpr int CHAIN_DEPTH = 12;           // template / leveled list hops before giving up
constexpr float RED_SHIFT = 96.0f;        // the "shift" red control moves every actor this far along +X

QString zString( const ESMFile::ESMField & f )
{
	const char * p = reinterpret_cast<const char *>( f.data() );
	size_t n = f.size();
	size_t len = 0;
	while ( len < n && p[len] )
		len++;
	return QString::fromLatin1( p, qsizetype( len ) );
}

quint32 u32At( const unsigned char * p )
{
	quint32 v;
	std::memcpy( &v, p, 4 );
	return v;
}

quint16 u16At( const unsigned char * p )
{
	quint16 v;
	std::memcpy( &v, p, 2 );
	return v;
}

float f32At( const unsigned char * p )
{
	float v;
	std::memcpy( &v, p, 4 );
	return v;
}

struct NpcRec
{
	bool exists = false;
	bool female = false;
	quint16 templateFlags = 0;      // bit 0 Traits, bit 8 Inventory
	quint32 tplt = 0;
	quint32 tpta[13] = { 0 };
	quint32 race = 0, skin = 0, outfit = 0;
	float heightMin = 1.0f, heightMax = 1.0f;
};

struct RaceRec
{
	bool exists = false;
	QString edid;
	QString skeleton[2];            // male, female
	quint32 skin = 0, armorRace = 0;
	float height[2] = { 1.0f, 1.0f };
	bool faceMesh = false;          // the "FaceGen Head" flag
};

struct ArmaRec
{
	bool exists = false;
	quint32 wears = 0, race = 0;    // wears: the body-slot bits
	QVector<quint32> moreRaces;
	QString model[2];               // male, female
};

struct ArmoRec
{
	bool exists = false;
	quint32 wears = 0;
	QVector<quint32> addons;
};

//! A leveled list as far as "is it a dice roll" goes.
struct ListRec
{
	bool exists = false;
	bool dice = true;               // false: every entry below is always taken
	QVector<quint32> entries;
};

Transform worldOf( const NifModel & nif, int block )
{
	Transform t( &nif, nif.getBlockIndex( block ) );
	int p = nif.getParent( block );
	for ( int hop = 0; p >= 0 && hop < 256; hop++ ) {
		const QModelIndex ip = nif.getBlockIndex( p );
		if ( !nif.blockInherits( ip, "NiAVObject" ) )
			break;
		t = Transform( &nif, ip ) * t;
		p = nif.getParent( p );
	}
	return t;
}

QString meshPath( const QString & model )
{
	QString path = model;
	path.replace( QChar( '\\' ), QChar( '/' ) );
	if ( !path.startsWith( QStringLiteral( "meshes/" ), Qt::CaseInsensitive ) )
		path.prepend( QStringLiteral( "meshes/" ) );
	return path;
}

bool loadNif( const QString & dataRoot, const QString & model, NifModel & nif )
{
	const QString path = meshPath( model );
	QByteArray bytes;
	if ( !lodgenProbeAsset( dataRoot, path, nullptr, nullptr, nullptr, &bytes ) || bytes.isEmpty() )
		return false;
	QBuffer dev( &bytes );
	if ( !dev.open( QIODevice::ReadOnly ) || !nif.load( dev, path.toLocal8Bit().constData() ) )
		return false;
	nif.resetState();
	return true;
}

} // namespace

struct CellActors::Impl
{
	const EsmWorld & world;
	QString dataRoot;
	ESMFile * esm = nullptr;
	QString red;
	QVector<CellActorRow> rows;
	QStringList plugins;

	struct Built
	{
		std::vector<NativeSrcShape> shapes;
		qint64 triangles = 0;
		float lo[3] = { 0, 0, 0 }, hi[3] = { 0, 0, 0 };
	};
	QHash<QString, Built> built;
	QSet<QString> builtEmpty;
	QHash<QString, QHash<QString, Transform>> restCache;     // skeleton -> bone name -> rest transform
	QHash<QString, std::vector<NativeSrcShape>> partCache;   // skeleton|model -> skinned shapes
	QHash<QString, int> headPartType;                        // head part editor id (lower) -> its type
	bool headPartsRead = false;

	Impl( const EsmWorld & w, const QString & root ) : world( w ), dataRoot( root )
	{
		esm = world.plugin();
		red = QString::fromLocal8Bit( qgetenv( "WW_CELL_ACTOR_RED" ) ).trimmed().toLower();
		for ( const QString & p : world.pluginList().split( QChar( ',' ), Qt::SkipEmptyParts ) )
			plugins.append( QFileInfo( p.trimmed() ).fileName() );
	}

	const ESMFile::ESMRecord * record( quint32 form, const char * type ) const
	{
		if ( !esm || !form )
			return nullptr;
		const ESMFile::ESMRecord * r = esm->findRecord( form );
		if ( !r || r->type == GRUP_TYPE || !( *r == type ) )
			return nullptr;
		return r;
	}

	NpcRec npc( quint32 form ) const
	{
		NpcRec out;
		const ESMFile::ESMRecord * r = record( form, "NPC_" );
		if ( !r )
			return out;
		out.exists = true;
		bool haveMax = false;
		ESMFile::ESMField f( *esm, *r );
		while ( f.next() ) {
			const unsigned char * p = f.data();
			if ( f == "ACBS" && f.size() >= 16 ) {
				out.female = ( u32At( p ) & 1U ) != 0;
				out.templateFlags = u16At( p + 14 );
			} else if ( f == "TPLT" && f.size() >= 4 ) {
				out.tplt = esm->mapFormID( *r, u32At( p ) );
			} else if ( f == "TPTA" && f.size() >= 52 ) {
				for ( int k = 0; k < 13; k++ ) {
					const quint32 raw = u32At( p + 4 * k );
					out.tpta[k] = raw ? esm->mapFormID( *r, raw ) : 0;
				}
			} else if ( f == "RNAM" && f.size() >= 4 ) {
				out.race = esm->mapFormID( *r, u32At( p ) );
			} else if ( f == "WNAM" && f.size() >= 4 ) {
				out.skin = esm->mapFormID( *r, u32At( p ) );
			} else if ( f == "DOFT" && f.size() >= 4 ) {
				out.outfit = esm->mapFormID( *r, u32At( p ) );
			} else if ( f == "NAM6" && f.size() >= 4 ) {
				out.heightMin = f32At( p );
				if ( !haveMax )
					out.heightMax = out.heightMin;
			} else if ( f == "NAM4" && f.size() >= 4 ) {
				out.heightMax = f32At( p );
				haveMax = true;
			}
		}
		return out;
	}

	RaceRec race( quint32 form ) const
	{
		RaceRec out;
		const ESMFile::ESMRecord * r = record( form, "RACE" );
		if ( !r )
			return out;
		out.exists = true;
		const unsigned version = esm->getRecordFormVersion( *r );
		int skeletons = 0;
		ESMFile::ESMField f( *esm, *r );
		while ( f.next() ) {
			const unsigned char * p = f.data();
			if ( f == "EDID" ) {
				out.edid = zString( f );
			} else if ( f == "WNAM" && f.size() >= 4 && !out.skin ) {
				out.skin = esm->mapFormID( *r, u32At( p ) );
			} else if ( f == "DATA" && f.size() >= 12 ) {
				out.height[0] = f32At( p );
				out.height[1] = f32At( p + 4 );
				const size_t at = version >= 109 ? 32 : 8;
				if ( f.size() >= at + 4 )
					out.faceMesh = ( u32At( p + at ) & 0x02U ) != 0;
			} else if ( f == "ANAM" && skeletons < 2 ) {
				out.skeleton[skeletons++] = zString( f );   // the first two: male, female skeletal model
			} else if ( f == "RNAM" && f.size() == 4 ) {
				out.armorRace = esm->mapFormID( *r, u32At( p ) );
			}
		}
		return out;
	}

	ArmoRec armo( quint32 form ) const
	{
		ArmoRec out;
		const ESMFile::ESMRecord * r = record( form, "ARMO" );
		if ( !r )
			return out;
		out.exists = true;
		ESMFile::ESMField f( *esm, *r );
		while ( f.next() ) {
			if ( f == "BOD2" && f.size() >= 4 )
				out.wears = u32At( f.data() );
			else if ( f == "MODL" && f.size() == 4 )
				out.addons.append( esm->mapFormID( *r, u32At( f.data() ) ) );
		}
		return out;
	}

	ArmaRec arma( quint32 form ) const
	{
		ArmaRec out;
		const ESMFile::ESMRecord * r = record( form, "ARMA" );
		if ( !r )
			return out;
		out.exists = true;
		ESMFile::ESMField f( *esm, *r );
		while ( f.next() ) {
			if ( f == "BOD2" && f.size() >= 4 )
				out.wears = u32At( f.data() );
			else if ( f == "RNAM" && f.size() >= 4 )
				out.race = esm->mapFormID( *r, u32At( f.data() ) );
			else if ( f == "MOD2" )
				out.model[0] = zString( f );
			else if ( f == "MOD3" )
				out.model[1] = zString( f );
			else if ( f == "MODL" && f.size() == 4 )
				out.moreRaces.append( esm->mapFormID( *r, u32At( f.data() ) ) );
		}
		return out;
	}

	/* A leveled list is NOT a dice roll when nothing in it is left to chance: no chance of
	 * nothing (its own, a global's, an entry's), every entry from level 1, and either one entry
	 * or (items only) the use-all flag. */
	ListRec list( quint32 form, const char * type, bool useAllCounts ) const
	{
		ListRec out;
		const ESMFile::ESMRecord * r = record( form, type );
		if ( !r )
			return out;
		out.exists = true;
		bool chance = false, useAll = false;
		ESMFile::ESMField f( *esm, *r );
		while ( f.next() ) {
			const unsigned char * p = f.data();
			if ( f == "LVLD" && f.size() >= 1 ) {
				chance = chance || p[0] != 0;
			} else if ( f == "LVLG" ) {
				chance = true;
			} else if ( f == "LVLF" && f.size() >= 1 ) {
				useAll = ( p[0] & 0x04 ) != 0;
			} else if ( f == "LVLO" && f.size() >= 12 ) {
				if ( u16At( p ) > 1 || p[10] != 0 )
					chance = true;
				out.entries.append( esm->mapFormID( *r, u32At( p + 4 ) ) );
			}
		}
		out.dice = chance || out.entries.isEmpty()
			|| !( out.entries.size() == 1 || ( useAllCounts && useAll ) );
		return out;
	}

	/*! The actor record that owns template category `bit` for `form` (0 Traits, 8 Inventory).
	 *  0 with `dice` set when a leveled list on the way is a dice roll; 0 without when the chain
	 *  breaks. */
	quint32 owner( quint32 form, int bit, bool & dice ) const
	{
		dice = false;
		for ( int hop = 0; hop < CHAIN_DEPTH && form; hop++ ) {
			const ESMFile::ESMRecord * r = esm ? esm->findRecord( form ) : nullptr;
			if ( !r || r->type == GRUP_TYPE )
				return 0;
			if ( *r == "LVLN" ) {
				const ListRec l = list( form, "LVLN", false );
				if ( l.dice ) {
					dice = true;
					return 0;
				}
				form = l.entries.first();
				continue;
			}
			if ( !( *r == "NPC_" ) )
				return 0;
			const NpcRec n = npc( form );
			if ( !( n.templateFlags & ( 1U << bit ) ) )
				return form;
			const quint32 next = n.tpta[bit] ? n.tpta[bit] : n.tplt;
			if ( !next )
				return form;
			form = next;
		}
		return 0;
	}

	//! The armors an outfit entry stands for; a dice roll is counted, not taken.
	void outfitItem( quint32 form, QVector<quint32> & armors, int & dice, int depth ) const
	{
		const ESMFile::ESMRecord * r = esm ? esm->findRecord( form ) : nullptr;
		if ( !r || r->type == GRUP_TYPE )
			return;
		if ( *r == "ARMO" ) {
			armors.append( form );
		} else if ( *r == "LVLI" ) {
			const ListRec l = list( form, "LVLI", true );
			if ( l.dice || depth >= 6 ) {
				dice++;
				return;
			}
			for ( quint32 e : l.entries )
				outfitItem( e, armors, dice, depth + 1 );
		}
	}

	void readHeadParts()
	{
		headPartsRead = true;
		const ESMFile::ESMRecord * r0 = esm ? esm->findRecord( 0U ) : nullptr;
		if ( !r0 )
			return;
		for ( unsigned int id = r0->next; id; ) {
			const ESMFile::ESMRecord * g = esm->findRecord( id );
			if ( !g )
				break;
			if ( g->type == GRUP_TYPE && std::memcmp( &g->flags, "HDPT", 4 ) == 0 ) {
				for ( unsigned int c = g->children; c; ) {
					const ESMFile::ESMRecord * h = esm->findRecord( c );
					if ( !h )
						break;
					if ( h->type != GRUP_TYPE && *h == "HDPT" ) {
						QString edid;
						int type = -1;
						ESMFile::ESMField f( *esm, *h );
						while ( f.next() ) {
							if ( f == "EDID" )
								edid = zString( f );
							else if ( f == "PNAM" && f.size() >= 4 )
								type = int( u32At( f.data() ) );
						}
						if ( !edid.isEmpty() )
							headPartType.insert( edid.toLower(), type );
					}
					c = h->next;
				}
			}
			id = g->next;
		}
	}

	const QHash<QString, Transform> & restOf( const QString & skeleton )
	{
		const QString key = skeleton.toLower();
		auto it = restCache.constFind( key );
		if ( it != restCache.constEnd() )
			return *it;
		QHash<QString, Transform> rest;
		NifModel nif;
		if ( loadNif( dataRoot, skeleton, nif ) ) {
			for ( int b = 0; b < nif.getBlockCount(); b++ ) {
				const QModelIndex ib = nif.getBlockIndex( b );
				if ( !nif.blockInherits( ib, "NiNode" ) )
					continue;
				const QString name = nif.get<QString>( ib, "Name" );
				if ( !name.isEmpty() && !rest.contains( name ) )
					rest.insert( name, worldOf( nif, b ) );
			}
		}
		return *restCache.insert( key, rest );
	}

	/*! One part, posed on the skeleton's rest pose. The loader's shapes carry the materials
	 *  and the triangles; the positions, normals and tangents are replaced here. */
	const std::vector<NativeSrcShape> & part( const QString & skeleton, const QString & model )
	{
		const QString key = skeleton.toLower() + QChar( '|' ) + model.toLower();
		auto it = partCache.constFind( key );
		if ( it != partCache.constEnd() )
			return *it;
		std::vector<NativeSrcShape> shapes;
		NifModel nif;
		const QHash<QString, Transform> & rest = restOf( skeleton );
		if ( !lodgenNativeLoadModelOnce( &dataRoot, model, nullptr, &shapes ) || !loadNif( dataRoot, model, nif ) )
			shapes.clear();
		for ( NativeSrcShape & s : shapes ) {
			const int block = s.nearFacts.block;
			const QModelIndex iShape = nif.getBlockIndex( block );
			const size_t nv = s.geom.pos.size() / 3;
			if ( !iShape.isValid() || !nv )
				continue;
			const QModelIndex iSkin = nif.getBlockIndex( nif.getLink( iShape, "Skin" ), "BSSkin::Instance" );
			const QModelIndex iData = iSkin.isValid()
				? nif.getBlockIndex( nif.getLink( iSkin, "Data" ), "BSSkin::BoneData" ) : QModelIndex();
			const QModelIndex iVD = nif.getIndex( iShape, "Vertex Data" );
			const BSVertexDesc desc = nif.get<BSVertexDesc>( iShape, "Vertex Desc" );
			const quint16 vf = quint16( ( desc.Value() >> 44 ) & 0xFFFF );
			const bool skinned = iSkin.isValid() && iData.isValid() && iVD.isValid() && ( vf & 0x40 ) != 0
				&& quint32( nv ) == nif.get<quint32>( iShape, "Num Vertices" );
			if ( skinned ) {
				const QVector<qint32> bones = nif.getLinkArray( iSkin, "Bones" );
				const QModelIndex iList = nif.getIndex( iData, "Bone List" );
				std::vector<Transform> mats( size_t( bones.size() ) );
				for ( int k = 0; k < bones.size(); k++ ) {
					const QString name = nif.get<QString>( nif.getBlockIndex( bones[k] ), "Name" );
					auto rit = rest.constFind( name );
					// a bone the skeleton names takes the skeleton's rest; one only the part has keeps its own
					const Transform w = rit != rest.constEnd() ? *rit
						: ( bones[k] >= 0 ? worldOf( nif, bones[k] ) : Transform() );
					mats[size_t( k )] = w * Transform( &nif, nif.index( k, 0, iList ) );
				}
				const bool fullPrec = ( vf & 0x400 ) != 0;
				for ( size_t v = 0; v < nv; v++ ) {
					const QModelIndex row = nif.index( int( v ), 0, iVD );
					const Vector3 p = fullPrec ? nif.get<Vector3>( row, "Vertex" )
						: Vector3( nif.get<HalfVector3>( row, "Vertex" ) );
					const Vector3 n( nif.get<ByteVector3>( row, "Normal" ) );
					const Vector3 t( nif.get<ByteVector3>( row, "Tangent" ) );
					const QVector<float> wts = nif.getArray<float>( row, "Bone Weights" );
					const QVector<quint8> bns = nif.getArray<quint8>( row, "Bone Indices" );
					Vector3 sp, sn, st;
					float total = 0.0f;
					for ( int j = 0; j < 4 && j < wts.size() && j < bns.size(); j++ ) {
						const float w = wts[j];
						if ( w <= 0.0f || int( bns[j] ) >= bones.size() )
							continue;
						const Transform & m = mats[size_t( bns[j] )];
						sp += ( m * p ) * w;
						sn += ( m.rotation * n ) * w;
						st += ( m.rotation * t ) * w;
						total += w;
					}
					if ( total <= 1.0e-6f )
						continue;   // unweighted: stays where the loader put it
					sp = sp * ( 1.0f / total );
					sn.normalize();
					st.normalize();
					for ( int k = 0; k < 3; k++ ) {
						s.geom.pos[v * 3 + size_t( k )] = sp[k];
						if ( s.geom.nrm.size() >= ( v + 1 ) * 3 )
							s.geom.nrm[v * 3 + size_t( k )] = sn[k];
						if ( s.geom.tan.size() >= ( v + 1 ) * 3 )
							s.geom.tan[v * 3 + size_t( k )] = st[k];
					}
				}
			} else {
				// rigid: it rides the nearest ancestor the skeleton names
				for ( int up = nif.getParent( block ), hop = 0; up >= 0 && hop < 256; up = nif.getParent( up ), hop++ ) {
					auto rit = rest.constFind( nif.get<QString>( nif.getBlockIndex( up ), "Name" ) );
					if ( rit == rest.constEnd() )
						continue;
					const Transform m = *rit * worldOf( nif, up ).inverted();
					for ( size_t v = 0; v < nv; v++ ) {
						const Vector3 p = m * Vector3( s.geom.pos[v * 3], s.geom.pos[v * 3 + 1], s.geom.pos[v * 3 + 2] );
						Vector3 n, t;
						if ( s.geom.nrm.size() >= ( v + 1 ) * 3 )
							n = m.rotation * Vector3( s.geom.nrm[v * 3], s.geom.nrm[v * 3 + 1], s.geom.nrm[v * 3 + 2] );
						if ( s.geom.tan.size() >= ( v + 1 ) * 3 )
							t = m.rotation * Vector3( s.geom.tan[v * 3], s.geom.tan[v * 3 + 1], s.geom.tan[v * 3 + 2] );
						for ( int k = 0; k < 3; k++ ) {
							s.geom.pos[v * 3 + size_t( k )] = p[k];
							if ( s.geom.nrm.size() >= ( v + 1 ) * 3 )
								s.geom.nrm[v * 3 + size_t( k )] = n[k];
							if ( s.geom.tan.size() >= ( v + 1 ) * 3 )
								s.geom.tan[v * 3 + size_t( k )] = t[k];
						}
					}
					break;
				}
			}
		}
		return *partCache.insert( key, shapes );
	}
};

CellActors::CellActors( const EsmWorld & world, const QString & dataRoot )
	: d( new Impl( world, dataRoot ) )
{
}

CellActors::~CellActors()
{
	delete d;
}

QVector<EsmRefr> CellActors::references()
{
	QVector<EsmRefr> out;
	ESMFile * esm = d->esm;
	if ( !esm )
		return out;
	std::function<void( unsigned int )> walk = [&]( unsigned int id ) {
		while ( id ) {
			const ESMFile::ESMRecord * r = esm->findRecord( id );
			if ( !r )
				return;
			if ( r->type != GRUP_TYPE && *r == "ACHR" && !rowOf.contains( r->formID ) ) {
				EsmRefr ref;
				ref.formID = r->formID;
				ref.initiallyDisabled = ( r->flags & 0x00000800 ) != 0;
				ref.deleted = ( r->flags & 0x00000020 ) != 0;
				ESMFile::ESMField f( *esm, *r );
				while ( f.next() ) {
					const unsigned char * p = f.data();
					if ( f == "NAME" && f.size() >= 4 ) {
						ref.base = esm->mapFormID( *r, u32At( p ) );
					} else if ( f == "DATA" && f.size() >= 24 ) {
						for ( int k = 0; k < 3; k++ ) {
							ref.pos[k] = f32At( p + 4 * k );
							ref.rot[k] = f32At( p + 12 + 4 * k );
						}
					} else if ( f == "XSCL" && f.size() >= 4 ) {
						ref.scale = f32At( p );
					} else if ( f == "XESP" && f.size() >= 8 ) {
						ref.enableParent = esm->mapFormID( *r, u32At( p ) );
						ref.enableParentOpposite = ( u32At( p + 4 ) & 1U ) != 0;
					} else if ( f == "XLYR" && f.size() >= 4 ) {
						ref.layer = esm->mapFormID( *r, u32At( p ) );
					}
				}
				if ( ref.base ) {
					const ESMFile::ESMRecord * br = esm->findRecord( ref.base );
					if ( br )
						ref.baseType = br->type;
				}
				CellActorRow row;
				row.form = ref.formID;
				row.base = ref.base;
				row.dead = ( r->flags & 0x00000200 ) != 0;
				for ( int k = 0; k < 3; k++ ) {
					row.pos[k] = ref.pos[k];
					row.rot[k] = ref.rot[k];
				}
				row.scale = ref.scale;
				rowOf.insert( ref.formID, int( d->rows.size() ) );
				d->rows.append( row );
				out.append( ref );
			}
			if ( r->children )
				walk( r->children );
			id = r->next;
		}
	};
	for ( quint32 g : d->world.interiorChildGroups() ) {
		const ESMFile::ESMRecord * gr = esm->findRecord( g );
		if ( gr )
			walk( gr->children );
	}
	return out;
}

bool CellActors::place( const EsmRefr & r, bool hidden, QString & key, float & scale )
{
	auto rit = rowOf.constFind( r.formID );
	if ( rit == rowOf.constEnd() )
		return false;
	CellActorRow & row = d->rows[*rit];
	// `hidden`: the cell view's own verdict on the start state (the enable-parent chain, as for any reference)
	if ( r.deleted || !r.base || hidden )
		return false;   // the row keeps the fate Hidden
	auto refuse = [&]( CellActorFate f ) {
		row.fate = f;
		return false;
	};
	if ( d->red == QLatin1String( "none" ) )
		return refuse( CellActorFate::RedOff );

	bool dice = false;
	const quint32 looks = d->owner( r.base, 0, dice );
	if ( !looks )
		return refuse( dice ? CellActorFate::LeveledList : CellActorFate::NotAnActor );
	const NpcRec n = d->npc( looks );
	row.looks = looks;
	row.female = n.female;
	const RaceRec race = d->race( n.race );
	if ( !race.exists )
		return refuse( CellActorFate::NoRace );
	row.raceEdid = race.edid;
	const int sex = n.female ? 1 : 0;

	QString skeleton = race.skeleton[sex];
	if ( skeleton.isEmpty() || d->restOf( skeleton ).isEmpty() )
		skeleton = race.skeleton[0];
	if ( skeleton.isEmpty() || d->restOf( skeleton ).isEmpty() )
		return refuse( CellActorFate::NoSkeleton );
	row.skeleton = skeleton;

	QSet<quint32> races;
	races.insert( n.race );
	if ( race.armorRace )
		races.insert( race.armorRace );
	struct Piece
	{
		quint32 wears;
		QString model;
	};
	auto addonsOf = [&]( quint32 armoForm, QVector<Piece> & pieces ) -> quint32 {
		const ArmoRec a = d->armo( armoForm );
		for ( quint32 af : a.addons ) {
			const ArmaRec ad = d->arma( af );
			if ( !ad.exists )
				continue;
			bool fits = races.contains( ad.race );
			for ( quint32 mr : ad.moreRaces )
				fits = fits || races.contains( mr );
			if ( !fits )
				continue;
			const QString model = !ad.model[sex].isEmpty() ? ad.model[sex] : ad.model[0];
			if ( !model.isEmpty() )
				pieces.append( Piece{ ad.wears, model } );
		}
		return a.wears;
	};

	// the outfit: the default outfit of the record the Inventory comes from
	quint32 worn = 0;
	QVector<Piece> outfitPieces;
	{
		bool invDice = false;
		const quint32 inv = d->owner( r.base, 8, invDice );
		if ( !inv ) {
			row.outfitUnknown = true;
		} else {
			const quint32 outfit = d->npc( inv ).outfit;
			const ESMFile::ESMRecord * o = d->record( outfit, "OTFT" );
			if ( o ) {
				QVector<quint32> armors;
				ESMFile::ESMField f( *d->esm, *o );
				while ( f.next() ) {
					if ( !( f == "INAM" ) )
						continue;
					for ( size_t at = 0; at + 4 <= f.size(); at += 4 )
						d->outfitItem( d->esm->mapFormID( *o, u32At( f.data() + at ) ), armors, row.outfitDice, 0 );
				}
				for ( quint32 a : armors )
					worn |= addonsOf( a, outfitPieces );
			}
		}
	}

	// the skin: the actor's own, else the race's; an addon is hidden when the outfit wears one of its slots
	QVector<Piece> skinPieces;
	addonsOf( n.skin ? n.skin : race.skin, skinPieces );
	if ( skinPieces.isEmpty() )
		return refuse( CellActorFate::NoBodyModel );
	const quint32 hides = d->red == QLatin1String( "nohide" ) ? 0U : worn;
	for ( const Piece & p : skinPieces ) {
		if ( p.wears & hides )
			row.hidden.append( p.model );
		else
			row.parts.append( p.model );
	}
	for ( const Piece & p : outfitPieces )
		if ( !row.parts.contains( p.model, Qt::CaseInsensitive ) )
			row.parts.append( p.model );

	// the head: the pre-built face mesh of the looks record; slot 32 worn hides it
	if ( race.faceMesh && !( worn & ( 1U << 2 ) ) ) {
		const int file = int( looks >> 24 );
		const QString plugin = file < d->plugins.size() ? d->plugins[file] : QString();
		const QString face = QStringLiteral( "actors\\character\\facegendata\\facegeom\\%1\\%2.nif" )
			.arg( plugin, QStringLiteral( "%1" ).arg( looks & 0x00FFFFFFU, 8, 16, QChar( '0' ) ).toUpper() );
		QByteArray probe;
		if ( !plugin.isEmpty() && lodgenProbeAsset( d->dataRoot, meshPath( face ), nullptr, nullptr, nullptr, &probe )
			&& !probe.isEmpty() )
			row.faceMesh = face;
		else
			row.headMissing = true;
	}

	// height: the race's for the sex, times the actor's own (the middle of its range when it has one)
	row.scale = r.scale * race.height[sex] * 0.5f * ( n.heightMin + n.heightMax );

	key = QStringLiteral( "actor|%1|%2|%3|%4" ).arg( looks, 8, 16, QChar( '0' ) ).arg( skeleton.toLower() )
		.arg( row.parts.join( QChar( ';' ) ).toLower() ).arg( worn, 8, 16, QChar( '0' ) );
	if ( d->builtEmpty.contains( key ) )
		return refuse( CellActorFate::NoGeometry );
	if ( !d->built.contains( key ) ) {
		if ( !d->headPartsRead )
			d->readHeadParts();
		Impl::Built b;
		auto take = [&]( const std::vector<NativeSrcShape> & shapes, bool face ) {
			for ( const NativeSrcShape & s : shapes ) {
				if ( s.geom.pos.empty() || s.geom.tris.empty() )
					continue;
				if ( face ) {
					// the face mesh names each shape after its head part: gore caps never, hair not under headgear
					const int type = d->headPartType.value( s.nearFacts.name.toLower(), -1 );
					if ( type == 7 || ( type == 3 && ( worn & 0x3U ) ) || ( type == 4 && ( worn & ( 1U << 18 ) ) ) )
						continue;
				}
				b.shapes.push_back( s );
			}
		};
		for ( const QString & model : row.parts )
			take( d->part( skeleton, model ), false );
		if ( !row.faceMesh.isEmpty() )
			take( d->part( skeleton, row.faceMesh ), true );
		bool first = true;
		for ( NativeSrcShape & s : b.shapes ) {
			b.triangles += qint64( s.geom.tris.size() / 3 );
			for ( size_t v = 0; v + 2 < s.geom.pos.size(); v += 3 ) {
				if ( d->red == QLatin1String( "shift" ) )
					s.geom.pos[v] += RED_SHIFT;
				for ( int k = 0; k < 3; k++ ) {
					const float c = s.geom.pos[v + size_t( k )];
					b.lo[k] = first ? c : qMin( b.lo[k], c );
					b.hi[k] = first ? c : qMax( b.hi[k], c );
				}
				first = false;
			}
		}
		if ( b.shapes.empty() || !b.triangles ) {
			d->builtEmpty.insert( key );
			return refuse( CellActorFate::NoGeometry );
		}
		d->built.insert( key, b );
	}
	row.key = key;
	row.fate = CellActorFate::Drawn;
	scale = row.scale;
	return true;
}

bool CellActors::shapes( const QString & key, std::vector<NativeSrcShape> * out ) const
{
	auto it = d->built.constFind( key );
	if ( it == d->built.constEnd() || !out )
		return false;
	*out = it->shapes;
	return true;
}

QString CellActors::fateName( CellActorFate f )
{
	switch ( f ) {
	case CellActorFate::Drawn: return QStringLiteral( "drawn" );
	case CellActorFate::Hidden: return QStringLiteral( "not shown" );
	case CellActorFate::LeveledList: return QStringLiteral( "leveled list (a dice roll)" );
	case CellActorFate::NotAnActor: return QStringLiteral( "base is no actor record" );
	case CellActorFate::NoRace: return QStringLiteral( "no race record" );
	case CellActorFate::NoSkeleton: return QStringLiteral( "skeleton file does not load" );
	case CellActorFate::NoBodyModel: return QStringLiteral( "no body model (built from parts)" );
	case CellActorFate::NoGeometry: return QStringLiteral( "no geometry in its models" );
	case CellActorFate::RedOff: return QStringLiteral( "switched off by the red control" );
	default: break;
	}
	return QStringLiteral( "?" );
}

QString CellActors::censusLine() const
{
	if ( d->rows.isEmpty() )
		return QString();
	int count[int( CellActorFate::Count )] = { 0 };
	int dead = 0, noHead = 0, noOutfit = 0, outfitDice = 0;
	qint64 tris = 0;
	for ( const CellActorRow & row : d->rows ) {
		count[int( row.fate )]++;
		if ( row.fate != CellActorFate::Drawn )
			continue;
		dead += row.dead ? 1 : 0;
		noHead += row.headMissing ? 1 : 0;
		noOutfit += row.outfitUnknown ? 1 : 0;
		outfitDice += row.outfitDice ? 1 : 0;
		auto it = d->built.constFind( row.key );
		if ( it != d->built.constEnd() )
			tris += it->triangles;
	}
	QString s;
	QTextStream t( &s );
	t << "  placed actors: " << d->rows.size() << " read, drawn " << count[int( CellActorFate::Drawn )]
	  << " in the skeleton's bind pose (" << tris << " triangles; " << dead
	  << " dead on start, drawn standing: no ragdoll; " << noHead << " without a head: no pre-built face mesh; "
	  << noOutfit << " in their skin and " << outfitDice << " short of outfit pieces: a dice roll), not shown "
	  << count[int( CellActorFate::Hidden )] << " (deleted, no base or initially disabled)";
	QStringList refused;
	int total = 0;
	for ( int f = int( CellActorFate::LeveledList ); f < int( CellActorFate::Count ); f++ ) {
		if ( !count[f] )
			continue;
		total += count[f];
		refused.append( QStringLiteral( "%1 %2" ).arg( fateName( CellActorFate( f ) ) ).arg( count[f] ) );
	}
	t << ", refused " << total;
	if ( total )
		t << ": " << refused.join( QLatin1String( ", " ) );
	if ( !d->red.isEmpty() )
		t << " [RED CONTROL " << d->red << "]";
	t << "\n";
	return s;
}

void CellActors::dump( const QString & path ) const
{
	if ( path.isEmpty() )
		return;
	QFile file( path );
	if ( !file.open( QIODevice::WriteOnly | QIODevice::Text ) )
		return;
	static const char * const words[int( CellActorFate::Count )] = {
		"drawn", "hidden", "leveled", "notactor", "norace", "noskeleton", "nobody", "nogeometry", "redoff"
	};
	QTextStream t( &file );
	t << "# form\tbase\tfate\tdead\tlooks\trace\tsex\tscale\tpos\trot\ttriangles\tlo\thi\tskeleton\tparts\thidden\tface\toutfit\n";
	for ( const CellActorRow & row : d->rows ) {
		qint64 tris = 0;
		float lo[3] = { 0, 0, 0 }, hi[3] = { 0, 0, 0 };
		auto it = d->built.constFind( row.key );
		if ( row.fate == CellActorFate::Drawn && it != d->built.constEnd() ) {
			tris = it->triangles;
			for ( int k = 0; k < 3; k++ ) {
				lo[k] = it->lo[k];
				hi[k] = it->hi[k];
			}
		}
		auto dash = []( const QString & v ) { return v.isEmpty() ? QStringLiteral( "-" ) : v; };
		t << QStringLiteral( "%1" ).arg( row.form, 8, 16, QChar( '0' ) ) << '\t'
		  << QStringLiteral( "%1" ).arg( row.base, 8, 16, QChar( '0' ) ) << '\t'
		  << words[int( row.fate )] << '\t' << ( row.dead ? 1 : 0 ) << '\t'
		  << QStringLiteral( "%1" ).arg( row.looks, 8, 16, QChar( '0' ) ) << '\t'
		  << dash( row.raceEdid ) << '\t' << ( row.female ? 'F' : 'M' ) << '\t'
		  << QString::number( double( row.scale ), 'f', 6 ) << '\t'
		  << QStringLiteral( "%1 %2 %3" ).arg( double( row.pos[0] ), 0, 'f', 4 ).arg( double( row.pos[1] ), 0, 'f', 4 )
				.arg( double( row.pos[2] ), 0, 'f', 4 ) << '\t'
		  << QStringLiteral( "%1 %2 %3" ).arg( double( row.rot[0] ), 0, 'f', 6 ).arg( double( row.rot[1] ), 0, 'f', 6 )
				.arg( double( row.rot[2] ), 0, 'f', 6 ) << '\t'
		  << tris << '\t'
		  << QStringLiteral( "%1 %2 %3" ).arg( double( lo[0] ), 0, 'f', 3 ).arg( double( lo[1] ), 0, 'f', 3 )
				.arg( double( lo[2] ), 0, 'f', 3 ) << '\t'
		  << QStringLiteral( "%1 %2 %3" ).arg( double( hi[0] ), 0, 'f', 3 ).arg( double( hi[1] ), 0, 'f', 3 )
				.arg( double( hi[2] ), 0, 'f', 3 ) << '\t'
		  << dash( row.skeleton ) << '\t' << dash( row.parts.join( QChar( ';' ) ) ) << '\t'
		  << dash( row.hidden.join( QChar( ';' ) ) ) << '\t' << dash( row.faceMesh ) << '\t'
		  << ( row.outfitUnknown ? "unknown" : ( row.outfitDice ? "short" : "whole" ) ) << '\n';
	}
}
