// Lane WATERUNDER5: the WATR INAM (underwater imagespace) read of EsmWorld::waterRecord.
// 1. the 7 vanilla masters: the 47 WATR name UnderwaterDefaultIS 19 / UnderwaterMurkyIS 18 / UnderwaterClearIS 10.
// 2. A plugin written here (master Fallout4.esm) adds an IMGS and overrides a WATR whose INAM names it; loaded third
//    (Fallout4.esm, DLCRobot.esm, plugin) the INAM must map to 02000800 = that IMGS. A raw read finds 01000800.
// 3. (WATERUNDER6) the same for WRLD NAM2 (worldspace default water) and CELL XCWT (cell water): a plugin overrides
//    Commonwealth and cell (0,0) with its own new WATR; both must read 02000801, a raw read finds 01000801.
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

// a group with a numeric label and a type (1 world children, 4 exterior block, 5 exterior sub-block)
static QByteArray subGroup( quint32 label, quint32 type, const QByteArray & body )
{
	Bytes h;
	h.b.append( "GRUP", 4 );
	h.u32( quint32( 24 + body.size() ) );
	h.u32( label );
	h.u32( type );
	h.u32( 0 );
	h.u32( 0 );
	return h.b + body;
}

static QByteArray f32( float v )
{
	quint32 u;
	std::memcpy( &u, &v, 4 );
	return le32( u );
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

	// 3. WRLD NAM2 and CELL XCWT: a plugin overrides Commonwealth (its default water) and cell (0,0) (its water)
	const quint32 ws = 0x3C, newWatr = 0x01000801;
	quint32 cell00 = 0, vanillaDefault = 0;
	float defWaterH = 0;
	{
		EsmWorld w;
		QString err;
		check( w.load( fo4, ws, &err ), "Commonwealth opens (Fallout4.esm) " + err );
		cell00 = w.cellForm( 0, 0 );
		vanillaDefault = w.defaultWaterType();
		defWaterH = w.defaultWaterHeight();
		check( cell00 != 0 && vanillaDefault != 0, QString( "vanilla: cell (0,0) %1, default water %2" )
			.arg( cell00, 8, 16, QChar( '0' ) ).arg( vanillaDefault, 8, 16, QChar( '0' ) ) );
	}
	const QString esp3 = scratch + "/WaterIdsTest.esp";
	{
		Bytes hedr;
		hedr.u32( 0x3F800000 );
		hedr.u32( 3 );
		hedr.u32( 0x802 );
		Bytes f;
		f.field( "HEDR", hedr.b );
		f.field( "MAST", zstr( "Fallout4.esm" ) );
		f.field( "DATA", QByteArray( 8, 0 ) );
		const QByteArray tes4 = record( "TES4", 0, f.b );
		Bytes wa;
		wa.field( "EDID", zstr( "UW6TestWater" ) );
		Bytes wr;	// the WRLD override: EDID, DNAM (land, water height), NAM2 = the plugin's own WATR (raw)
		wr.field( "EDID", zstr( "Commonwealth" ) );
		wr.field( "DNAM", f32( -2048.0f ) + f32( defWaterH ) );
		wr.field( "NAM2", le32( newWatr ) );
		Bytes ce;	// the CELL (0,0) override: has water, height 123, XCWT = the plugin's own WATR (raw)
		ce.field( "DATA", QByteArray( "\x02\x00", 2 ) );
		ce.field( "XCLC", le32( 0 ) + le32( 0 ) + le32( 0 ) );
		ce.field( "XCLW", f32( 123.0f ) );
		ce.field( "XCWT", le32( newWatr ) );
		const QByteArray cells = subGroup( ws, 1, subGroup( 0, 4, subGroup( 0, 5, record( "CELL", cell00, ce.b ) ) ) );
		QFile out( esp3 );
		if ( !out.open( QIODevice::WriteOnly ) ) {
			std::printf( "FAIL cannot write %s\n", esp3.toLocal8Bit().constData() );
			return 2;
		}
		out.write( tes4 + group( "WATR", record( "WATR", newWatr, wa.b ) )
			+ group( "WRLD", record( "WRLD", ws, wr.b ) + cells ) );
	}
	{
		EsmWorld w;
		QString err;
		check( cell00 && w.load( fo4 + "," + robot + "," + esp3, ws, &err ), "the 3-file Commonwealth opens " + err );
		const QString nam2 = QString::number( w.defaultWaterType(), 16 ).rightJustified( 8, '0' );
		check( w.defaultWaterType() == 0x02000801, QString( "WRLD NAM2 maps to 02000801 (read %1)" ).arg( nam2 ) );
		float h = 0;
		quint32 t = 0;
		const bool has = w.cellWater( 0, 0, h, &t );
		const QString xcwt = QString::number( t, 16 ).rightJustified( 8, '0' );
		check( has && h == 123.0f, QString( "cell (0,0) override read: has water %1, height %2" ).arg( int( has ) ).arg( h ) );
		check( t == 0x02000801, QString( "CELL XCWT maps to 02000801 (read %1)" ).arg( xcwt ) );
	}

	std::printf( "%s (%d failed)\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
