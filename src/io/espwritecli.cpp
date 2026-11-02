/* Lane ESPWRITE2 (2026-11-02): `NifSkope -no-gui espwrite <mode> ...`, the plugin writer's gates on the built exe.
 *   build <plan.json> <out.esp> <Data dir> | yaw <niftypes.cpp> <cellview.cpp> | reemit <plugin> [--drop]
 *       gates 1, 3 and 4 (src/io/espwritegates.cpp; reds WW_ESPWRITE_RED as in ESPWRITE1)
 *   roundtrip <Data dir> <plan.json> <ours.esp>
 *       gate 2 (src/io/espwriteroundtrip.cpp)
 *   place <cell.wwcell> <out.esp> [--append] [--full] [--specdir <dir>] <EDID> <x> <y> <z> <yaw deg> [more items]
 *       the Edits panel's save without a window (src/celledit.h): resolve each EditorID in the cell's load order,
 *       write the plugin, write the reopen .wwcell, read the cell back through EsmWorld on the NEW load order and
 *       print each item's ghost line (gate 5), its saved form and the read-back pose.
 *   edit <cell.wwcell> <out.esp> [--append] [--full] [--specdir <dir>] <ref hex> [move x y z] [turn <yaw deg>] [disable] ...
 *       ESPWRITE3: override references already in an interior cell (the winner's record, DATA / 0x800 changed), save,
 *       read back on the new load order: each changed ref once, won by the new plugin, at the new pose, and every
 *       other ref of the cell unchanged. Reds WW_ESPWRITE_RED=editkeep | editflag.
 */

#include "celledit.h"
#include "cellview.h"
#include "esmdata.h"

#include <QFileInfo>
#include <QSet>
#include <QStringList>

#include <algorithm>

#include <cmath>
#include <cstdio>

int espwriteGatesMain( const QStringList & a );
int espwriteRoundtripMain( const QStringList & a );

namespace {

int cmdPlace( const QStringList & a )
{
	if ( a.size() < 2 ) {
		std::printf( "espwrite place: needs <cell.wwcell> <out.esp> [--append] [--full] [--specdir D] <EDID> x y z yaw...\n" );
		return 2;
	}
	CellSceneSpec spec;
	QString e;
	if ( !cellSpecFromFile( a[0], spec, &e ) ) {
		std::printf( "espwrite place: %s: %s\n", qPrintable( a[0] ), qPrintable( e ) );
		return 2;
	}
	CellEditTarget target;
	target.path = a[1];
	QString specDir = QFileInfo( a[1] ).absolutePath();
	QStringList rest;
	for ( int i = 2; i < a.size(); i++ ) {
		if ( a[i] == QLatin1String( "--append" ) )
			target.append = true;
		else if ( a[i] == QLatin1String( "--full" ) )
			target.light = false;
		else if ( a[i] == QLatin1String( "--specdir" ) && i + 1 < a.size() )
			specDir = a[++i];
		else
			rest << a[i];
	}
	if ( rest.isEmpty() || rest.size() % 5 ) {
		std::printf( "espwrite place: items come as <EDID> x y z yaw, %lld words given\n", (long long)rest.size() );
		return 2;
	}
	const QStringList order = spec.plugins.split( QLatin1Char( ',' ), Qt::SkipEmptyParts );
	QVector<CellEditItem> items;
	for ( int i = 0; i < rest.size(); i += 5 ) {
		CellEditItem it;
		QString sig;
		if ( !cellEditResolveEdid( order, rest[i], it.base, sig, it.lo, it.hi, &e ) ) {
			std::printf( "espwrite place: REFUSED %s\n", qPrintable( e ) );
			return 1;
		}
		it.baseEdid = rest[i];
		for ( int k = 0; k < 3; k++ )
			it.pos[k] = rest[i + 1 + k].toFloat();
		const float n[3] = { 0, 0, 1 };
		cellEditPose( rest[i + 4].toFloat() * 3.14159265358979323846f / 180.0f, false, n, it.rot );
		it.persistent = sig == QLatin1String( "STAT" ) && ( it.base == 0x34 || it.base == 0x3B );
		items.append( it );
	}
	QString reopen, line;
	if ( !cellEditSave( spec, items, target, specDir, &reopen, &line, &e ) ) {
		std::printf( "espwrite place: REFUSED %s\n", qPrintable( e ) );
		return 1;
	}
	std::printf( "saved %s\n", qPrintable( line ) );
	std::printf( "reopen %s\n", qPrintable( reopen ) );
	for ( const CellEditItem & it : std::as_const( items ) )
		std::printf( "item form %08X %s\n", it.formOut, qPrintable( cellEditGhostLine( it ) ) );

	// read the cell back on the new load order, the way the cell view will
	CellSceneSpec back;
	if ( !cellSpecFromFile( reopen, back, &e ) ) {
		std::printf( "readback FAIL %s\n", qPrintable( e ) );
		return 1;
	}
	EsmWorld w;
	QVector<EsmRefr> refs;
	if ( back.interior ) {
		if ( !w.loadInterior( back.plugins, back.interiorCell, &e ) ) {
			std::printf( "readback FAIL %s\n", qPrintable( e ) );
			return 1;
		}
		refs = w.interiorRefrs();
	} else {
		quint32 ws = 0;
		for ( const auto & p : EsmWorld::listWorldspaces( back.plugins, &e ) )
			if ( p.second.compare( back.world, Qt::CaseInsensitive ) == 0 )
				ws = p.first;
		if ( !ws || !w.load( back.plugins, ws, &e ) ) {
			std::printf( "readback FAIL %s\n", qPrintable( e ) );
			return 1;
		}
		QSet<quint32> seen;
		for ( const CellEditItem & it : std::as_const( items ) ) {
			const int cx = int( std::floor( it.pos[0] / 4096.0f ) ), cy = int( std::floor( it.pos[1] / 4096.0f ) );
			QVector<EsmRefr> got = w.refrs( cx, cy );
			got += w.persistentRefrsIn( cx * 4096.0f, cy * 4096.0f, ( cx + 1 ) * 4096.0f, ( cy + 1 ) * 4096.0f );
			for ( const EsmRefr & r : std::as_const( got ) )
				if ( !seen.contains( r.formID ) ) {
					seen.insert( r.formID );
					refs.append( r );
				}
		}
	}
	const QString tname = QFileInfo( target.path ).fileName();
	int ok = 0, bad = 0;
	for ( const CellEditItem & it : std::as_const( items ) ) {
		const EsmRefr * hit = nullptr;
		for ( const EsmRefr & r : std::as_const( refs ) )
			if ( r.base == it.base && std::fabs( r.pos[0] - it.pos[0] ) < 0.01f && std::fabs( r.pos[1] - it.pos[1] ) < 0.01f
				&& std::fabs( r.pos[2] - it.pos[2] ) < 0.01f && w.recordPlugin( r.formID ).compare( tname, Qt::CaseInsensitive ) == 0 )
				hit = &r;
		if ( !hit ) {
			bad++;
			std::printf( "readback MISSING %s at %.3f %.3f %.3f\n", qPrintable( it.baseEdid ), it.pos[0], it.pos[1], it.pos[2] );
			continue;
		}
		float dr = 0.0f, dp = 0.0f;
		for ( int k = 0; k < 3; k++ ) {
			dr = std::max( dr, std::fabs( hit->rot[k] - it.rot[k] ) );
			dp = std::max( dp, std::fabs( hit->pos[k] - it.pos[k] ) );
		}
		( dr < 1.0e-6f ? ok : bad )++;
		std::printf( "readback %s form %08X plugin %s pos %.3f %.3f %.3f rot %.6f %.6f %.6f dpos %.3g drot %.3g\n",
			dr < 1.0e-6f ? "ok" : "MISMATCH", hit->formID, qPrintable( w.recordPlugin( hit->formID ) ), hit->pos[0],
			hit->pos[1], hit->pos[2], hit->rot[0], hit->rot[1], hit->rot[2], dp, dr );
	}
	int own = 0;
	for ( const EsmRefr & r : std::as_const( refs ) )
		if ( w.recordPlugin( r.formID ).compare( tname, Qt::CaseInsensitive ) == 0 )
			own++;
	std::printf( "readback summary %d ok %d bad of %lld; %d ref(s) of %s in the read cell\n", ok, bad,
		(long long)items.size(), own, qPrintable( tname ) );
	return bad ? 1 : 0;
}

// ESPWRITE3: the refs of an interior cell through EsmWorld
bool interiorRefs( const QString & plugins, const QString & cell, EsmWorld & w, QVector<EsmRefr> & refs, QString * e )
{
	if ( !w.loadInterior( plugins, cell, e ) )
		return false;
	refs = w.interiorRefrs();
	return true;
}

const EsmRefr * refOf( const QVector<EsmRefr> & refs, quint32 form, int * count = nullptr )
{
	const EsmRefr * hit = nullptr;
	int n = 0;
	for ( const EsmRefr & r : refs )
		if ( r.formID == form ) {
			hit = &r;
			n++;
		}
	if ( count )
		*count = n;
	return hit;
}

int cmdEdit( const QStringList & a )
{
	if ( a.size() < 3 ) {
		std::printf( "espwrite edit: needs <cell.wwcell> <out.esp> [--append] [--full] [--specdir D] <ref hex> "
			"[move x y z] [turn yawDeg] [disable] ...\n" );
		return 2;
	}
	CellSceneSpec spec;
	QString e;
	if ( !cellSpecFromFile( a[0], spec, &e ) || !spec.interior ) {
		std::printf( "espwrite edit: %s: %s\n", qPrintable( a[0] ), e.isEmpty() ? "not an interior cell" : qPrintable( e ) );
		return 2;
	}
	QVector<EsmRefr> refs0;
	{	// closed before the save: an open EsmWorld maps the plugins, and "Add to existing" rewrites one of them
		EsmWorld before;
		if ( !interiorRefs( spec.plugins, spec.interiorCell, before, refs0, &e ) ) {
			std::printf( "espwrite edit: %s\n", qPrintable( e ) );
			return 2;
		}
	}
	CellEditTarget target;
	target.path = a[1];
	QString specDir = QFileInfo( a[1] ).absolutePath();
	QVector<CellEditChange> changes;
	for ( int i = 2; i < a.size(); i++ ) {
		const QString t = a[i];
		auto need = [&]( int n ) {
			if ( changes.isEmpty() || i + n >= a.size() ) {
				std::printf( "espwrite edit: %s needs a ref before it and %d number(s) after it\n", qPrintable( t ), n );
				return false;
			}
			return true;
		};
		if ( t == QLatin1String( "--append" ) )
			target.append = true;
		else if ( t == QLatin1String( "--full" ) )
			target.light = false;
		else if ( t == QLatin1String( "--specdir" ) && i + 1 < a.size() )
			specDir = a[++i];
		else if ( t == QLatin1String( "move" ) ) {
			if ( !need( 3 ) )
				return 2;
			changes.last().move = true;
			for ( int k = 0; k < 3; k++ )
				changes.last().pos[k] = a[++i].toFloat();
		} else if ( t == QLatin1String( "turn" ) ) {
			if ( !need( 1 ) )
				return 2;
			changes.last().turn = true;
			changes.last().rot[2] = a[++i].toFloat() * 3.14159265358979323846f / 180.0f;
		} else if ( t == QLatin1String( "disable" ) ) {
			if ( changes.isEmpty() ) {
				std::printf( "espwrite edit: disable needs a ref before it\n" );
				return 2;
			}
			changes.last().disable = true;
		} else {
			bool ok = false;
			CellEditChange c;
			c.ref = t.toUInt( &ok, 16 );
			const EsmRefr * r = ok ? refOf( refs0, c.ref ) : nullptr;
			if ( !r ) {
				std::printf( "espwrite edit: REFUSED %s is not a reference of %s; its first refs:\n", qPrintable( t ),
					qPrintable( spec.interiorCell ) );
				for ( int k = 0; k < std::min<int>( 12, refs0.size() ); k++ )
					std::printf( "  %08X base %08X pos %.3f %.3f %.3f disabled %d\n", refs0[k].formID, refs0[k].base,
						refs0[k].pos[0], refs0[k].pos[1], refs0[k].pos[2], int( refs0[k].initiallyDisabled ) );
				return 1;
			}
			for ( int k = 0; k < 3; k++ ) {
				c.pos[k] = r->pos[k];
				c.rot[k] = r->rot[k];
			}
			changes.append( c );
		}
	}
	QVector<CellEditItem> items;
	QString reopen, line;
	if ( !cellEditSave( spec, items, changes, target, specDir, &reopen, &line, &e ) ) {
		std::printf( "espwrite edit: REFUSED %s\n", qPrintable( e ) );
		return 1;
	}
	std::printf( "saved %s\n", qPrintable( line ) );
	std::printf( "reopen %s\n", qPrintable( reopen ) );

	CellSceneSpec back;
	EsmWorld w;
	QVector<EsmRefr> refs1;
	if ( !cellSpecFromFile( reopen, back, &e ) || !interiorRefs( back.plugins, back.interiorCell, w, refs1, &e ) ) {
		std::printf( "readback FAIL %s\n", qPrintable( e ) );
		return 1;
	}
	const QString tname = QFileInfo( target.path ).fileName();
	int ok = 0, bad = 0;
	QSet<quint32> changedRefs;
	for ( const CellEditChange & c : std::as_const( changes ) ) {
		changedRefs.insert( c.ref );
		int n = 0;
		const EsmRefr * r0 = refOf( refs0, c.ref );
		const EsmRefr * r = refOf( refs1, c.ref, &n );
		if ( !r ) {
			bad++;
			std::printf( "readback MISSING %08X\n", c.ref );
			continue;
		}
		float dp = 0.0f, dr = 0.0f;
		for ( int k = 0; k < 3; k++ ) {
			dp = std::max( dp, std::fabs( r->pos[k] - c.pos[k] ) );
			dr = std::max( dr, std::fabs( r->rot[k] - c.rot[k] ) );
		}
		const QString wn = w.recordPlugin( c.ref );
		const bool wins = wn.compare( tname, Qt::CaseInsensitive ) == 0;
		const bool dis = r->initiallyDisabled == ( c.disable || r0->initiallyDisabled );
		const bool good = n == 1 && wins && dp < 0.01f && dr < 1.0e-6f && dis && r->base == r0->base;
		( good ? ok : bad )++;
		std::printf( "readback %s ref %08X copies %d plugin %s pos %.3f %.3f %.3f rot %.6f %.6f %.6f dpos %.3g drot %.3g "
			"disabled %d (was %d) persistent %d base %08X\n", good ? "ok" : "MISMATCH", c.ref, n, qPrintable( wn ),
			r->pos[0], r->pos[1], r->pos[2], r->rot[0], r->rot[1], r->rot[2], dp, dr, int( r->initiallyDisabled ),
			int( r0->initiallyDisabled ), int( c.persistent ), r->base );
	}
	// nothing else in the cell changed, and no ref was added
	int others = 0;
	for ( const EsmRefr & r0 : std::as_const( refs0 ) ) {
		if ( changedRefs.contains( r0.formID ) )
			continue;
		const EsmRefr * r = refOf( refs1, r0.formID );
		if ( !r || r->initiallyDisabled != r0.initiallyDisabled || r->pos[0] != r0.pos[0] || r->pos[1] != r0.pos[1]
			|| r->pos[2] != r0.pos[2] || r->rot[2] != r0.rot[2] )
			others++;
	}
	std::printf( "readback summary %d ok %d bad of %lld; refs %lld -> %lld; others changed %d\n", ok, bad,
		(long long)changes.size(), (long long)refs0.size(), (long long)refs1.size(), others );
	return ( bad || others || refs0.size() != refs1.size() ) ? 1 : 0;
}

} // namespace

int cmdEspWrite( const QStringList & args )
{
	const QString mode = args.value( 0 );
	int rc = 2;
	if ( mode == QLatin1String( "build" ) || mode == QLatin1String( "yaw" ) || mode == QLatin1String( "reemit" ) ) {
		QStringList a = args.mid( 1 );
		a.prepend( QStringLiteral( "--" ) + mode );
		a.prepend( QStringLiteral( "espwrite" ) );
		rc = espwriteGatesMain( a );
	} else if ( mode == QLatin1String( "roundtrip" ) ) {
		QStringList a = args.mid( 1 );
		a.prepend( QStringLiteral( "espwrite" ) );
		rc = espwriteRoundtripMain( a );
	} else if ( mode == QLatin1String( "place" ) ) {
		rc = cmdPlace( args.mid( 1 ) );
	} else if ( mode == QLatin1String( "edit" ) ) {
		rc = cmdEdit( args.mid( 1 ) );
	} else {
		std::printf( "espwrite: build|yaw|reemit|roundtrip|place|edit (src/io/espwritecli.cpp)\n" );
	}
	std::fflush( stdout );
	return rc;
}
