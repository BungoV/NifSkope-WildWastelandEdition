// Lane WATERUNDER5: the WATR INAM (underwater imagespace) read of EsmWorld::waterRecord.
// 1. the 7 vanilla masters: the 47 WATR name UnderwaterDefaultIS 19 / UnderwaterMurkyIS 18 / UnderwaterClearIS 10.
// 2. A plugin written here (master Fallout4.esm) adds an IMGS and overrides a WATR whose INAM names it; loaded third
//    (Fallout4.esm, DLCRobot.esm, plugin) the INAM must map to 02000800 = that IMGS. A raw read finds 01000800.
// Run: waterinam_test <Fallout 4 Data folder> <scratch folder>; red = build against the base esmdata.cpp (must fail).
#include "esmdata.h"
#include "esmwater.h"
#include "esmfile.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QSet>
#include <QString>
#include <cstdio>
#include <cstring>
#include <functional>

static int fails = 0;
static void check( bool ok, const QString & what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what.toLocal8Bit().constData() );
	if ( !ok )
		fails++;
}

static quint32 tag( const char * s )
{
	return quint32( quint8( s[0] ) ) | quint32( quint8( s[1] ) ) << 8 | quint32( quint8( s[2] ) ) << 16
		| quint32( quint8( s[3] ) ) << 24;
}

// every WATR of every loaded file: each file's top WATR group (group ids are dense from 0x80000000 across the files;
// an override lives in its first file's chain, so the set keeps each form once)
static QVector<quint32> watrForms( const ESMFile & esm )
{
	QVector<quint32> out;
	QSet<quint32> seen;
	for ( quint32 k = 0;; k++ ) {
		const ESMFile::ESMRecord * g = esm.findRecord( 0x80000000U | k );
		if ( !g )
			break;
		if ( g->type == tag( "GRUP" ) && g->formID == 0 && g->flags == tag( "WATR" ) )
			for ( unsigned int c = g->children; c; ) {
				const ESMFile::ESMRecord * r = esm.findRecord( c );
				if ( !r )
					break;
				if ( r->type == tag( "WATR" ) && !seen.contains( r->formID ) ) {
					seen.insert( r->formID );
					out.append( r->formID );
				}
				c = r->next;
			}
	}
	return out;
}

struct Bytes
{
	QByteArray b;
	void u16( quint16 v ) { b.append( char( v & 255 ) ).append( char( v >> 8 ) ); }
	void u32( quint32 v ) { for ( int i = 0; i < 4; i++ ) b.append( char( ( v >> ( 8 * i ) ) & 255 ) ); }
	void field( const char * t, const QByteArray & d ) { b.append( t, 4 ); u16( quint16( d.size() ) ); b.append( d ); }
};

static QByteArray zstr( const char * s ) { return QByteArray( s, int( std::strlen( s ) ) + 1 ); }
static QByteArray le32( quint32 v ) { Bytes x; x.u32( v ); return x.b; }

static QByteArray record( const char * type, quint32 form, const QByteArray & data )
{
	Bytes h;
	h.b.append( type, 4 );
	h.u32( quint32( data.size() ) );
	h.u32( 0 );	// flags
	h.u32( form );
	h.u32( 0 );	// version control
	h.u16( 131 );	// Fallout 4 form version
	h.u16( 0 );
	return h.b + data;
}

static QByteArray group( const char * label, const QByteArray & body )
{
	Bytes h;
	h.b.append( "GRUP", 4 );
	h.u32( quint32( 24 + body.size() ) );
	h.b.append( label, 4 );
	h.u32( 0 );	// top group
	h.u32( 0 );
	h.u32( 0 );
	return h.b + body;
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc < 3 ) {
		std::printf( "usage: waterinam_test <Fallout 4 Data> <scratch folder>\n" );
		return 2;
	}
	const QString data = QString::fromLocal8Bit( argv[1] ), scratch = QString::fromLocal8Bit( argv[2] );
	const QString fo4 = data + "/Fallout4.esm", robot = data + "/DLCRobot.esm";

	const auto interiors = EsmWorld::listInteriors( fo4, nullptr );
	if ( interiors.isEmpty() ) {
		std::printf( "FAIL no interior cell in %s\n", fo4.toLocal8Bit().constData() );
		return 2;
	}
	const QString cell = QString::number( interiors.first().first, 16 );

	// 1. the vanilla census
	quint32 victim = 0;
	QString victimEdid;
	{
		EsmWorld w;
		QString err;
		QString all = fo4;
		for ( const char * m : { "DLCRobot.esm", "DLCworkshop01.esm", "DLCCoast.esm", "DLCworkshop02.esm",
			"DLCworkshop03.esm", "DLCNukaWorld.esm" } )
			all += "," + data + "/" + m;
		check( w.loadInterior( all, cell, &err ), "the 7 vanilla masters open " + err );
		const QVector<quint32> forms = watrForms( *w.plugin() );
		QMap<QString, int> n;
		for ( quint32 f : forms ) {
			WwWaterRecord r;
			if ( w.waterRecord( f, r ) )
				n[r.uwIsName]++;
			if ( !victim && ( f >> 24 ) == 0 && r.uwIsName == QLatin1String( "UnderwaterDefaultIS" ) ) {
				victim = f;
				victimEdid = r.editorId;
			}
		}
		for ( auto it = n.constBegin(); it != n.constEnd(); ++it )
			std::printf( "     %-22s %d\n", it.key().toLocal8Bit().constData(), it.value() );
		check( forms.size() == 47, QString( "47 vanilla WATR (read %1)" ).arg( forms.size() ) );
		check( n.value( "UnderwaterDefaultIS" ) == 19 && n.value( "UnderwaterMurkyIS" ) == 18
			&& n.value( "UnderwaterClearIS" ) == 10, "INAM: Default 19 / Murky 18 / Clear 10" );
	}

	// 2. a plugin override, loaded third
	const char * isEdid = "UW5TestIS";
	const QString esp = scratch + "/WaterInamTest.esp";
	{
		Bytes hedr;
		hedr.u32( 0x3F800000 );	// 1.0
		hedr.u32( 2 );
		hedr.u32( 0x801 );
		QByteArray tes4;
		Bytes f;
		f.field( "HEDR", hedr.b );
		f.field( "MAST", zstr( "Fallout4.esm" ) );
		f.field( "DATA", QByteArray( 8, 0 ) );
		tes4 = record( "TES4", 0, f.b );
		Bytes is;
		is.field( "EDID", zstr( isEdid ) );
		Bytes wa;
		wa.field( "EDID", zstr( victimEdid.toLatin1().constData() ) );
		wa.field( "INAM", le32( 0x01000800 ) );	// raw: index 1 = the plugin itself (one master)
		QFile out( esp );
		QDir().mkpath( scratch );
		if ( !out.open( QIODevice::WriteOnly ) ) {
			std::printf( "FAIL cannot write %s\n", esp.toLocal8Bit().constData() );
			return 2;
		}
		out.write( tes4 + group( "IMGS", record( "IMGS", 0x01000800, is.b ) ) + group( "WATR", record( "WATR", victim, wa.b ) ) );
	}
	{
		EsmWorld w;
		QString err;
		check( victim && w.loadInterior( fo4 + "," + robot + "," + esp, cell, &err ), "the 3-file load order opens " + err );
		WwWaterRecord r;
		const bool got = w.waterRecord( victim, r );
		const QString hex = QString::number( r.imageSpace, 16 ).rightJustified( 8, '0' );
		check( got && r.imageSpace == 0x02000800, QString( "override %1: INAM maps to 02000800 (read %2)" )
			.arg( victimEdid, hex ) );
		check( got && r.uwIsName == QLatin1String( isEdid ), QString( "override %1: names %2 (read \"%3\")" )
			.arg( victimEdid, isEdid, r.uwIsName ) );
	}

	std::printf( "%s (%d failed)\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
