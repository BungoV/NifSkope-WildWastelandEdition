// Lane ESPWRITE1 gate 2: read a written plugin back through the reader NifSkope WW uses (EsmWorld, load order
// "Fallout4.esm,<ours>"). Every plan ref must come back with its base, pose and flags, and a disabled override must
// REPLACE the master ref (one copy, disabled). Usage: roundtrip <Data dir> <plan.json> <ours.esp>
#include "esmdata.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>
#include <cstdio>

static int checks = 0, failures = 0;

static void check( bool ok, const QString & what )
{
	++checks;
	if ( !ok ) {
		++failures;
		std::printf( "FAIL %s\n", qPrintable( what ) );
	}
}

static quint32 hexForm( const QJsonValue & v )
{
	return v.toString().toUInt( nullptr, 16 );
}

static const EsmRefr * findRef( const QVector<EsmRefr> & v, quint32 form, int * copies )
{
	const EsmRefr * hit = nullptr;
	*copies = 0;
	for ( const EsmRefr & r : v )
		if ( r.formID == form ) {
			hit = &r;
			++*copies;
		}
	return hit;
}

int main( int argc, char ** argv )
{
	QCoreApplication app( argc, argv );
	QStringList a = app.arguments();
	if ( a.size() < 4 ) {
		std::printf( "usage: roundtrip <Data dir> <plan.json> <ours.esp>\n" );
		return 2;
	}
	QFile pf( a[2] );
	if ( !pf.open( QIODevice::ReadOnly ) )
		return 2;
	QJsonObject j = QJsonDocument::fromJson( pf.readAll() ).object();
	QString order = a[1] + "/" + j.value( "masters" ).toArray().first().toString() + "," + a[3];
	QString ours = QFileInfo( a[3] ).fileName();
	// the writer numbers own records in plan order from 0x800 under master index 1; the reader (esmfile.cpp, as the
	// game) loads a light plugin as FE + light slot (0 here) + the 12-bit local id
	quint32 next = j.value( "light" ).toBool() ? 0xFE000800 : 0x01000800;
	for ( const QJsonValue & cv : j.value( "cells" ).toArray() ) {
		QJsonObject c = cv.toObject();
		quint32 cell = hexForm( c.value( "cell" ) );
		bool exterior = c.value( "world" ).isString();
		EsmWorld w;
		QString err;
		QVector<EsmRefr> temp;
		if ( exterior ) {
			if ( !w.load( order, hexForm( c.value( "world" ) ), &err ) ) {
				std::printf( "FAIL load: %s\n", qPrintable( err ) );
				return 2;
			}
		} else {
			if ( !w.loadInterior( order, QString::number( cell, 16 ).rightJustified( 8, '0' ), &err ) ) {
				std::printf( "FAIL loadInterior: %s\n", qPrintable( err ) );
				return 2;
			}
			temp = w.interiorRefrs();
		}
		for ( const QJsonValue & av : c.value( "adds" ).toArray() ) {
			QJsonObject ad = av.toObject();
			quint32 form = next++;
			QJsonArray p = ad.value( "pos" ).toArray(), q = ad.value( "rot" ).toArray();
			bool persistent = ad.value( "persistent" ).toBool();
			QVector<EsmRefr> pool = temp;
			if ( exterior ) {
				int cx = int( std::floor( p.at( 0 ).toDouble() / 4096.0 ) ), cy = int( std::floor( p.at( 1 ).toDouble() / 4096.0 ) );
				float x = float( p.at( 0 ).toDouble() ), y = float( p.at( 1 ).toDouble() );
				pool = persistent ? w.persistentRefrsIn( x - 64, y - 64, x + 64, y + 64 ) : w.refrs( cx, cy );
			}
			int copies = 0;
			const EsmRefr * r = findRef( pool, form, &copies );
			QString tag = QStringLiteral( "%1 ref %2" ).arg( exterior ? ( persistent ? "exterior persistent" : "exterior temporary" )
				: ( persistent ? "interior persistent" : "interior temporary" ) ).arg( form, 8, 16, QLatin1Char( '0' ) );
			check( r != nullptr && copies == 1, QStringLiteral( "missing: %1 read back %2 times" ).arg( tag ).arg( copies ) );
			if ( !r )
				continue;
			check( r->base == hexForm( ad.value( "base" ) ), QStringLiteral( "base: %1 base %2" ).arg( tag ).arg( r->base, 8, 16 ) );
			double ep = 0, er = 0;
			for ( int i = 0; i < 3; ++i ) {
				ep = std::max( ep, double( std::fabs( r->pos[i] - float( p.at( i ).toDouble() ) ) ) );
				er = std::max( er, double( std::fabs( r->rot[i] - float( q.at( i ).toDouble() ) ) ) );
			}
			check( ep <= 1e-6 && er <= 1e-6, QStringLiteral( "pose: %1 off by pos %2 rot %3" ).arg( tag ).arg( ep ).arg( er ) );
			check( std::fabs( r->scale - float( ad.value( "scale" ).toDouble( 1.0 ) ) ) <= 1e-6,
				QStringLiteral( "scale: %1 %2" ).arg( tag ).arg( r->scale ) );
			check( !r->initiallyDisabled, QStringLiteral( "flags: %1 reads disabled" ).arg( tag ) );
			check( w.recordPlugin( form ) == ours, QStringLiteral( "owner: %1 is from '%2'" ).arg( tag, w.recordPlugin( form ) ) );
			std::printf( "  %s: base %08X pos (%g, %g, %g) rot z %g\n", qPrintable( tag ), r->base, r->pos[0], r->pos[1],
				r->pos[2], r->rot[2] );
		}
		for ( const QJsonValue & dv : c.value( "disable" ).toArray() ) {
			quint32 form = hexForm( dv );
			QVector<EsmRefr> pool = temp;
			if ( exterior ) {
				pool.clear();
				int cx = 0, cy = 0;
				// a disabled exterior temporary ref: search the 3x3 around the plan's first add
				QJsonArray p0 = c.value( "adds" ).toArray().first().toObject().value( "pos" ).toArray();
				cx = int( std::floor( p0.at( 0 ).toDouble() / 4096.0 ) );
				cy = int( std::floor( p0.at( 1 ).toDouble() / 4096.0 ) );
				for ( int dy = -1; dy <= 1; ++dy )
					for ( int dx = -1; dx <= 1; ++dx )
						pool += w.refrs( cx + dx, cy + dy );
			}
			int copies = 0;
			const EsmRefr * r = findRef( pool, form, &copies );
			QString tag = QStringLiteral( "override %1" ).arg( form, 8, 16, QLatin1Char( '0' ) );
			check( r != nullptr && copies == 1, QStringLiteral( "override: %1 read back %2 times (want 1, replacing the master's)" )
				.arg( tag ).arg( copies ) );
			if ( r ) {
				check( r->initiallyDisabled, QStringLiteral( "override: %1 is not disabled" ).arg( tag ) );
				check( w.recordPlugin( form ) == ours, QStringLiteral( "override: %1 wins from '%2'" ).arg( tag, w.recordPlugin( form ) ) );
				std::printf( "  %s: disabled %d, winner %s\n", qPrintable( tag ), int( r->initiallyDisabled ),
					qPrintable( w.recordPlugin( form ) ) );
			}
		}
	}
	std::printf( "gate 2 %s: %d checks, %d failures\n", qPrintable( ours ), checks, failures );
	return failures ? 1 : 0;
}
