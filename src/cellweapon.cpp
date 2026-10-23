/* lane MISS1 (GAPLOOK2): placed weapons with their default mod parts. See cellweapon.h for the rule. Record
 * layouts follow the published plugin format notes (the xEdit definitions: OBTS, OMOD DATA). */

#include "cellweapon.h"

#include "esmdata.h"
#include "esmfile.hpp"
#include "lodgen.h"
#include "nativeemit.h"
#include "wwswitch.h"
#include "data/niftypes.h"
#include "gl/celllooks.h"
#include "model/nifmodel.h"

#include <QBuffer>
#include <QSet>

#include <cstring>

namespace
{

quint32 u32At( const unsigned char * p )
{
	quint32 v;
	std::memcpy( &v, p, 4 );
	return v;
}

QString zString( const ESMFile::ESMField & f )
{
	const char * p = reinterpret_cast<const char *>( f.data() );
	size_t n = 0;
	while ( n < f.size() && p[n] )
		n++;
	return QString::fromLatin1( p, qsizetype( n ) );
}

bool loadNif( const QString & dataRoot, const QString & model, NifModel & nif )
{
	QString path = model;
	path.replace( QChar( '\\' ), QChar( '/' ) );
	if ( !path.startsWith( QStringLiteral( "meshes/" ), Qt::CaseInsensitive ) )
		path.prepend( QStringLiteral( "meshes/" ) );
	QByteArray bytes;
	if ( !lodgenProbeAsset( dataRoot, path, nullptr, nullptr, nullptr, &bytes ) || bytes.isEmpty() )
		return false;
	QBuffer dev( &bytes );
	if ( !dev.open( QIODevice::ReadOnly ) || !nif.load( dev, path.toLocal8Bit().constData() ) )
		return false;
	nif.resetState();
	return true;
}

//! A node's frame in its mesh's root space, the root's own transform left out (a placed load drops it too).
Transform frameOf( const NifModel & nif, int block )
{
	Transform t;
	for ( int b = block, hop = 0; b >= 0 && hop < 256; hop++ ) {
		const int p = nif.getParent( b );
		const QModelIndex ib = nif.getBlockIndex( b );
		if ( p < 0 || !nif.blockInherits( ib, "NiAVObject" ) )
			break;
		t = Transform( &nif, ib ) * t;
		b = p;
	}
	return t;
}

struct Point
{
	QString slot;
	Transform at;	// the point in the weapon's space
};

//! The P- points a mesh publishes, placed by `frame` (that mesh's root in the weapon's space).
void offered( const NifModel & nif, const Transform & frame, QVector<Point> & out )
{
	QHash<QString, int> byName;
	for ( int b = 0; b < nif.getBlockCount(); b++ ) {
		const QModelIndex i = nif.getBlockIndex( b );
		if ( nif.blockInherits( i, "NiAVObject" ) )
			byName.insert( nif.get<QString>( i, "Name" ), b );
	}
	for ( int b = 0; b < nif.getBlockCount(); b++ ) {
		const QModelIndex i = nif.getBlockIndex( b );
		if ( !nif.isNiBlock( i, "BSConnectPoint::Parents" ) )
			continue;
		const QModelIndex iPoints = nif.getIndex( i, "Connect Points" );
		if ( !iPoints.isValid() )
			continue;
		const int owner = nif.getParent( b );
		for ( int r = 0; r < nif.rowCount( iPoints ); r++ ) {
			const QModelIndex iPoint = nif.getIndex( iPoints, r );
			const QString name = nif.get<QString>( iPoint, "Name" );
			if ( name.size() <= 2 || !name.startsWith( QLatin1String( "P-" ), Qt::CaseInsensitive ) )
				continue;
			const QString rides = nif.get<QString>( iPoint, "Parent" );
			const int node = rides.isEmpty() ? owner : byName.value( rides, owner );
			Transform cp;
			cp.translation = nif.get<Vector3>( iPoint, "Translation" );
			cp.rotation.fromQuat( nif.get<Quat>( iPoint, "Rotation" ) );
			const float s = nif.get<float>( iPoint, "Scale" );
			cp.scale = s > 0.0f ? s : 1.0f;
			out.append( Point{ name.mid( 2 ), frame * ( node >= 0 ? frameOf( nif, node ) : Transform() ) * cp } );
		}
	}
}

QStringList declared( const NifModel & nif )
{
	QStringList asked;
	for ( int b = 0; b < nif.getBlockCount(); b++ ) {
		const QModelIndex i = nif.getBlockIndex( b );
		if ( !nif.isNiBlock( i, "BSConnectPoint::Children" ) )
			continue;
		const QModelIndex iNames = nif.getIndex( i, "Point Name" );
		if ( !iNames.isValid() )
			continue;
		for ( int r = 0; r < nif.rowCount( iNames ); r++ ) {
			const QString p = nif.get<QString>( nif.getIndex( iNames, r ) );
			if ( p.size() > 2 && p.startsWith( QLatin1String( "C-" ), Qt::CaseInsensitive ) )
				asked << p.mid( 2 );
		}
	}
	return asked;
}

void moveShapes( std::vector<NativeSrcShape> & shapes, const Transform & t )
{
	for ( NativeSrcShape & s : shapes ) {
		for ( size_t v = 0; v + 2 < s.geom.pos.size(); v += 3 ) {
			const Vector3 q = t * Vector3( s.geom.pos[v], s.geom.pos[v + 1], s.geom.pos[v + 2] );
			for ( int k = 0; k < 3; k++ )
				s.geom.pos[v + size_t( k )] = q[k];
		}
		for ( std::vector<float> * dir : { &s.geom.nrm, &s.geom.tan } )
			for ( size_t v = 0; v + 2 < dir->size(); v += 3 ) {
				const Vector3 q = t.rotation * Vector3( ( *dir )[v], ( *dir )[v + 1], ( *dir )[v + 2] );
				for ( int k = 0; k < 3; k++ )
					( *dir )[v + size_t( k )] = q[k];
			}
	}
}

} // namespace

struct CellWeapons::Impl
{
	const EsmWorld & world;
	QString dataRoot;
	ESMFile * esm = nullptr;
	QHash<quint32, QStringList> partsOf;	// base -> part meshes (empty: no assembly)
	QHash<quint32, std::vector<NativeSrcShape>> built;
	QSet<quint32> failed;
	int weapons = 0, assembled = 0, placed = 0, atRoot = 0, unresolved = 0, noMesh = 0;

	Impl( const EsmWorld & w, const QString & root ) : world( w ), dataRoot( root ) { esm = world.plugin(); }

	//! An OMOD's mesh, then the meshes of the OMODs its DATA includes (depth-limited).
	void addMod( quint32 form, QStringList & meshes, QSet<quint32> & seen, int depth )
	{
		if ( !form || depth > 4 || seen.contains( form ) )
			return;
		seen.insert( form );
		const ESMFile::ESMRecord * r = esm->findRecord( form );
		if ( !r || !( *r == "OMOD" ) )
			return;
		QVector<quint32> inner;
		bool mesh = false;
		ESMFile::ESMField f( *esm, *r );
		while ( f.next() ) {
			if ( f == "MODL" && !mesh ) {
				const QString m = zString( f );
				if ( !m.isEmpty() ) {
					meshes << m;
					mesh = true;
				}
			} else if ( f == "DATA" && f.size() >= 24 ) {
				// include count @0, property count @4, 2 bools, form type u32 @10, rank + tier, attach point @16
				const unsigned char * p = f.data();
				const quint32 includes = u32At( p );
				size_t at = 20;
				const quint32 parentSlots = u32At( p + at );	// not "slots": Qt takes that word
				at += 4 + size_t( parentSlots ) * 4;
				if ( at + 4 > f.size() )
					continue;
				const quint32 items = u32At( p + at );
				at += 4 + size_t( items ) * 8;
				for ( quint32 k = 0; k < includes && at + 7 <= f.size(); k++, at += 7 )
					inner.append( esm->mapFormID( *r, u32At( p + at ) ) );
			}
		}
		if ( !mesh )
			noMesh++;
		for ( quint32 m : inner )
			addMod( m, meshes, seen, depth + 1 );
	}

	//! The default object template's parts: the OBTS with Default set, else the first.
	QStringList readParts( quint32 base )
	{
		QStringList meshes;
		const ESMFile::ESMRecord * r = esm ? esm->findRecord( base ) : nullptr;
		if ( !r || !( *r == "WEAP" ) )
			return meshes;
		QVector<quint32> first, chosen;
		bool haveDefault = false;
		ESMFile::ESMField f( *esm, *r );
		while ( f.next() ) {
			if ( !( f == "OBTS" ) || f.size() < 18 )
				continue;
			// include count @0, property count @4, levels @8..11, parent @12, Default @14, keyword count u8 @15
			const unsigned char * p = f.data();
			const quint32 includes = u32At( p );
			const bool isDefault = p[14] != 0;
			size_t at = 16 + size_t( p[15] ) * 4 + 2;
			QVector<quint32> mods;
			for ( quint32 k = 0; k < includes && at + 7 <= f.size(); k++, at += 7 )
				mods.append( esm->mapFormID( *r, u32At( p + at ) ) );
			if ( first.isEmpty() && !haveDefault )
				first = mods;
			if ( isDefault && !haveDefault ) {
				chosen = mods;
				haveDefault = true;
			}
		}
		QSet<quint32> seen;
		for ( quint32 m : haveDefault ? chosen : first )
			addMod( m, meshes, seen, 0 );
		return meshes;
	}
};

CellWeapons::CellWeapons( const EsmWorld & world, const QString & dataRoot ) : d( new Impl( world, dataRoot ) )
{
}

CellWeapons::~CellWeapons()
{
	delete d;
}

bool CellWeapons::wants( quint32 base )
{
	if ( !base || !d->esm || !wwLookOn( WwLook::WeaponParts ) )
		return false;
	auto it = d->partsOf.constFind( base );
	if ( it == d->partsOf.constEnd() ) {
		const QStringList parts = d->readParts( base );
		const ESMFile::ESMRecord * r = d->esm->findRecord( base );
		d->weapons += ( r && *r == "WEAP" ) ? 1 : 0;
		it = d->partsOf.insert( base, parts );
	}
	return !it->isEmpty();
}

bool CellWeapons::shapes( quint32 base, std::vector<NativeSrcShape> * out )
{
	if ( !out || d->failed.contains( base ) )
		return false;
	auto bit = d->built.constFind( base );
	if ( bit != d->built.constEnd() ) {
		*out = *bit;
		return true;
	}
	const EsmLodBase & lb = d->world.lodBase( base );
	std::vector<NativeSrcShape> all;
	NifModel baseNif;
	if ( lb.model.isEmpty() || !lodgenNativeLoadModelPlaced( &d->dataRoot, lb.model, nullptr, &all )
		|| !loadNif( d->dataRoot, lb.model, baseNif ) ) {
		d->failed.insert( base );
		return false;
	}
	const bool redRoot = wwEnvStr( "WW_CELL_WEAP_PARTS_RED" ) == QLatin1String( "root" );
	QVector<Point> points;
	offered( baseNif, Transform(), points );
	struct Part
	{
		QString mesh;
		QStringList asks;
		bool done = false;
	};
	QVector<Part> parts;
	QHash<QString, NifModel *> nifs;
	for ( const QString & m : d->partsOf.value( base ) ) {
		NifModel * nif = new NifModel;
		if ( !loadNif( d->dataRoot, m, *nif ) ) {
			delete nif;
			d->noMesh++;
			continue;
		}
		nifs.insert( m, nif );
		parts.append( Part{ m, declared( *nif ) } );
	}
	auto attach = [&]( Part & part, const Transform & frame ) {
		std::vector<NativeSrcShape> shapes;
		part.done = true;
		if ( !lodgenNativeLoadModelPlaced( &d->dataRoot, part.mesh, nullptr, &shapes ) ) {
			d->noMesh++;
			return;
		}
		moveShapes( shapes, frame );
		all.insert( all.end(), shapes.begin(), shapes.end() );
		offered( *nifs.value( part.mesh ), frame, points );
		d->placed++;
	};
	// chains (receiver -> barrel -> muzzle): attach whatever resolves, until a pass attaches nothing
	for ( bool moved = true; moved; ) {
		moved = false;
		for ( Part & part : parts ) {
			if ( part.done )
				continue;
			if ( redRoot ) {
				attach( part, Transform() );
				moved = true;
				continue;
			}
			int found = -1;
			for ( const QString & slot : part.asks )
				for ( int i = 0; i < points.size(); i++ )
					if ( points.at( i ).slot.compare( slot, Qt::CaseInsensitive ) == 0 )
						found = i;	// the last publisher wins (the deepest in the chain)
			if ( found >= 0 ) {
				attach( part, points.at( found ).at );
				moved = true;
			}
		}
	}
	for ( Part & part : parts ) {
		if ( part.done )
			continue;
		if ( part.asks.isEmpty() ) {	// declares no point (a flash card, a sight's glass): the root, as nifmerge does
			attach( part, Transform() );
			d->atRoot++;
		} else {
			d->unresolved++;
		}
	}
	qDeleteAll( nifs );
	d->assembled++;
	d->built.insert( base, all );
	*out = all;
	return true;
}

QString CellWeapons::censusLine() const
{
	if ( !d->weapons )
		return QString();
	return QStringLiteral( "  weapon parts: %1 weapons, %2 assembled, parts %3 placed (%4 at the root: no point), "
						   "%5 unresolved, %6 no mesh%7\n" )
		.arg( d->weapons ).arg( d->assembled ).arg( d->placed ).arg( d->atRoot ).arg( d->unresolved ).arg( d->noMesh )
		.arg( wwEnvStr( "WW_CELL_WEAP_PARTS_RED" ) == QLatin1String( "root" ) ? QStringLiteral( " [red root]" ) : QString() );
}
