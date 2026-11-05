/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "watercurves.h"

#include "watermark.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>

#include <cmath>
#include <cstring>

/* =========================================================================
 *  The model behind the water window.  No widget lives here; the window
 *  edits a WaterCurveDoc and the harness measures one.
 * ========================================================================= */

namespace {

constexpr double kTwoPi = 6.283185307179586;

quint16 rd16( const char * p )
{
	return quint16( quint8( p[0] ) | ( quint16( quint8( p[1] ) ) << 8 ) );
}

quint32 rd32( const char * p )
{
	return quint32( quint8( p[0] ) ) | ( quint32( quint8( p[1] ) ) << 8 )
		| ( quint32( quint8( p[2] ) ) << 16 ) | ( quint32( quint8( p[3] ) ) << 24 );
}

float rdf32( const char * p )
{
	const quint32 u = rd32( p );
	float f = 0.0f;
	std::memcpy( &f, &u, 4 );
	return f;
}

struct Buf
{
	QByteArray b;
	void u8( quint8 v ) { b.append( char( v ) ); }
	void u16( quint16 v ) { u8( quint8( v ) ); u8( quint8( v >> 8 ) ); }
	void u32( quint32 v ) { u16( quint16( v ) ); u16( quint16( v >> 16 ) ); }
	void i32( qint32 v ) { u32( quint32( v ) ); }
	void f32( float v ) { quint32 u = 0; std::memcpy( &u, &v, 4 ); u32( u ); }
};

/*! One record of the raw stroke store, the parts the store's own codec does
 *  not (yet) carry: the trailing per-point floats of a curve, the payload of
 *  a raster.  Index-aligned with WaterMarkDoc::strokes(), because that codec
 *  appends every record it reads in order. */
struct StoreExtra
{
	quint8 kind = 0;
	QVector<float> weights;
	QByteArray payload;
};

QVector<StoreExtra> parseStoreExtras( const QByteArray & raw )
{
	QVector<StoreExtra> out;
	if ( raw.size() < 4 )
		return out;
	const quint32 count = rd32( raw.constData() );
	qsizetype at = 4;
	for ( quint32 i = 0; i < count; i++ ) {
		if ( at + 4 > raw.size() )
			break;
		const quint32 recBytes = rd32( raw.constData() + at );
		if ( recBytes < 20 || at + qsizetype( recBytes ) > raw.size() )
			break;
		const char * p = raw.constData() + at;
		StoreExtra e;
		e.kind = quint8( p[6] );
		const quint16 n = rd16( p + 16 );
		/* LANE WATER7, red 5a of BUILD10, the reading half. A DyePin's record
		 * carries FOUR COLOUR BYTES between its points and its trailing bytes
		 * (WaterMarkDoc::encodeStrokes / decodeStrokes both say so); this
		 * parser used one base for every kind, so a dye pin's weights would
		 * have been read four bytes early even if the kind test had let them
		 * through -- which it did not. Both halves are fixed together, or the
		 * writer below would start emitting weights nothing reads. */
		const qsizetype base = 20 + qsizetype( n ) * 8
			+ ( e.kind == WaterStroke::DyePin ? 4 : 0 );
		if ( ( e.kind == WaterStroke::Stroke || e.kind == WaterStroke::Pin
				|| e.kind == WaterStroke::DyePin )
			&& qsizetype( recBytes ) >= base + qsizetype( n ) * 4 && n > 0 ) {
			e.weights.reserve( n );
			for ( int k = 0; k < n; k++ )
				e.weights.append( rdf32( p + base + k * 4 ) );
		} else if ( e.kind == WaterCurveDoc::kRasterKind && qsizetype( recBytes ) > base ) {
			e.payload = QByteArray( p + base, int( qsizetype( recBytes ) - base ) );
		}
		out.append( e );
		at += qsizetype( recBytes );
	}
	return out;
}

//! 9 significant digits: a float survives the round trip exactly, and the text is stable.
QString num( double v )
{
	if ( std::isnan( v ) || std::isinf( v ) )
		return QStringLiteral( "0" );
	QString s = QString::number( v, 'g', 9 );
	if ( s == QLatin1String( "-0" ) )
		s = QStringLiteral( "0" );
	return s;
}

QString jsonString( const QString & s )
{
	QString out = QStringLiteral( "\"" );
	for ( QChar c : s ) {
		if ( c == QLatin1Char( '"' ) )
			out += QLatin1String( "\\\"" );
		else if ( c == QLatin1Char( '\\' ) )
			out += QLatin1String( "\\\\" );
		else if ( c == QLatin1Char( '\n' ) )
			out += QLatin1String( "\\n" );
		else if ( c == QLatin1Char( '\r' ) )
			out += QLatin1String( "\\r" );
		else if ( c == QLatin1Char( '\t' ) )
			out += QLatin1String( "\\t" );
		else if ( c.unicode() < 0x20 )
			out += QStringLiteral( "\\u%1" ).arg( int( c.unicode() ), 4, 16, QLatin1Char( '0' ) );
		else
			out += c;
	}
	out += QLatin1Char( '"' );
	return out;
}

const char * kindName( quint8 k )
{
	switch ( k ) {
	case WaterCurve::Curve: return "curve";
	case WaterCurve::Pin: return "pin";
	case WaterCurve::SourcePin: return "sourcePin";
	case WaterCurve::OutletPin: return "outletPin";
	case WaterCurve::DyePin: return "dyePin";
	default: return "curve";
	}
}

int kindFromName( const QString & s )
{
	if ( s == QLatin1String( "curve" ) ) return WaterCurve::Curve;
	if ( s == QLatin1String( "pin" ) ) return WaterCurve::Pin;
	if ( s == QLatin1String( "sourcePin" ) ) return WaterCurve::SourcePin;
	if ( s == QLatin1String( "outletPin" ) ) return WaterCurve::OutletPin;
	if ( s == QLatin1String( "dyePin" ) ) return WaterCurve::DyePin;
	return -1;
}

const char * className( int cls )
{
	switch ( cls ) {
	case 0: return "sea";
	case 1: return "river";
	case 2: return "lake";
	default: return "auto";
	}
}

int classFromName( const QString & s )
{
	if ( s == QLatin1String( "sea" ) ) return 0;
	if ( s == QLatin1String( "river" ) ) return 1;
	if ( s == QLatin1String( "lake" ) ) return 2;
	return -1;
}

//! The body-plane grid of an open document, in texels.
void gridOf( const WaterMarkDoc & doc, int & tw, int & th )
{
	double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	doc.worldBounds( x0, y0, x1, y1 );
	const double u = doc.worldPerTexel();
	tw = int( ( x1 - x0 ) / u + 0.5 );
	th = int( ( y1 - y0 ) / u + 0.5 );
}

} // namespace

// =========================================================================
//  WaterCurve
// =========================================================================

void WaterCurve::reverse()
{
	std::reverse( pts.begin(), pts.end() );
}

double WaterCurve::length() const
{
	double L = 0.0;
	for ( int i = 0; i + 1 < pts.size(); i++ )
		L += std::hypot( double( pts[i + 1].x ) - pts[i].x, double( pts[i + 1].y ) - pts[i].y );
	return L;
}

// =========================================================================
//  WaterRasterLayer
// =========================================================================

qint64 WaterRasterLayer::paintedCount() const
{
	qint64 n = 0;
	for ( quint8 p : painted )
		n += p ? 1 : 0;
	return n;
}

bool WaterRasterLayer::wordAt( int px, int py, quint16 & word ) const
{
	const int lx = px - px0, ly = py - py0;
	if ( lx < 0 || ly < 0 || lx >= w || ly >= h )
		return false;
	const qsizetype i = qsizetype( ly ) * w + lx;
	if ( i >= painted.size() || !painted[i] )
		return false;
	word = words[i];
	return true;
}

QByteArray WaterRasterLayer::payload() const
{
	Buf b;
	b.i32( px0 );
	b.i32( py0 );
	b.i32( w );
	b.i32( h );
	QByteArray wordBytes( reinterpret_cast<const char *>( words.constData() ),
		int( words.size() * qsizetype( sizeof( quint16 ) ) ) );
	QByteArray paintBytes( reinterpret_cast<const char *>( painted.constData() ), int( painted.size() ) );
	const QByteArray zw = qCompress( wordBytes, 6 );
	const QByteArray zp = qCompress( paintBytes, 6 );
	b.u32( quint32( zw.size() ) );
	b.u32( quint32( zp.size() ) );
	b.b.append( zw );
	b.b.append( zp );
	return b.b;
}

bool WaterRasterLayer::fromPayload( const QByteArray & p, QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	if ( p.size() < 24 )
		return fail( QStringLiteral( "a raster record is %1 bytes; its header alone is 24" ).arg( p.size() ) );
	px0 = qint32( rd32( p.constData() ) );
	py0 = qint32( rd32( p.constData() + 4 ) );
	w = qint32( rd32( p.constData() + 8 ) );
	h = qint32( rd32( p.constData() + 12 ) );
	const quint32 zwN = rd32( p.constData() + 16 );
	const quint32 zpN = rd32( p.constData() + 20 );
	if ( w <= 0 || h <= 0 || w > 65536 || h > 65536 )
		return fail( QStringLiteral( "a raster record says %1 x %2 texels" ).arg( w ).arg( h ) );
	if ( 24 + qsizetype( zwN ) + qsizetype( zpN ) > p.size() )
		return fail( QStringLiteral( "a raster record's two stores (%1 + %2 bytes) do not fit its %3" )
			.arg( zwN ).arg( zpN ).arg( p.size() ) );
	const QByteArray wordBytes = qUncompress( QByteArray( p.constData() + 24, int( zwN ) ) );
	const QByteArray paintBytes = qUncompress( QByteArray( p.constData() + 24 + zwN, int( zpN ) ) );
	const qsizetype n = qsizetype( w ) * h;
	if ( wordBytes.size() != n * 2 || paintBytes.size() != n )
		return fail( QStringLiteral( "a raster record inflates to %1 + %2 bytes for %3 texels" )
			.arg( wordBytes.size() ).arg( paintBytes.size() ).arg( n ) );
	words.resize( n );
	painted.resize( n );
	std::memcpy( words.data(), wordBytes.constData(), size_t( n * 2 ) );
	std::memcpy( painted.data(), paintBytes.constData(), size_t( n ) );
	return true;
}

// =========================================================================
//  WaterCurveDoc: the document
// =========================================================================

void WaterCurveDoc::clear()
{
	curves.clear();
	bodies.clear();
	rasters.clear();
	dyeHalfDistance = 8192.0;
}

WaterBodyOverride * WaterCurveDoc::overrideFor( int id, bool create )
{
	if ( id <= 0 )
		return nullptr;
	for ( WaterBodyOverride & o : bodies )
		if ( o.id == id )
			return &o;
	if ( !create )
		return nullptr;
	WaterBodyOverride o;
	o.id = id;
	bodies.append( o );
	return &bodies.last();
}

const WaterBodyOverride * WaterCurveDoc::overrideOf( int id ) const
{
	for ( const WaterBodyOverride & o : bodies )
		if ( o.id == id )
			return &o;
	return nullptr;
}

void WaterCurveDoc::dropEmptyOverrides()
{
	for ( int i = bodies.size() - 1; i >= 0; i-- )
		if ( bodies[i].isEmpty() )
			bodies.removeAt( i );
}

// ---- json -----------------------------------------------------------------

QString WaterCurveDoc::jsonPathFor( const QString & lodlPath )
{
	const QFileInfo fi( lodlPath );
	return fi.dir().filePath( fi.completeBaseName() + QStringLiteral( ".water.json" ) );
}

QByteArray WaterCurveDoc::toJson() const
{
	/* Hand-written on purpose: QJsonDocument sorts keys and prints doubles its
	 * own way, and this file is read by people and diffed by the gate, so the
	 * order is the order a person would want and the numbers are stable. */
	QString s;
	s.reserve( 4096 + curves.size() * 256 );
	s += QLatin1String( "{\n" );
	s += QStringLiteral( "  \"format\": \"ww-water-curves\",\n" );
	s += QStringLiteral( "  \"version\": %1,\n" ).arg( kJsonVersion );
	s += QStringLiteral( "  \"worldspace\": %1,\n" ).arg( jsonString( worldspace ) );
	s += QStringLiteral( "  \"landFile\": %1,\n" ).arg( jsonString( landFile ) );
	s += QStringLiteral( "  \"cells\": [%1, %2, %3, %4],\n" )
		.arg( cellMinX ).arg( cellMinY ).arg( cellMaxX ).arg( cellMaxY );
	s += QStringLiteral( "  \"bodySamples\": %1,\n" ).arg( bodySamples );
	s += QStringLiteral( "  \"units\": \"world\",\n" );
	s += QStringLiteral( "  \"dye\": { \"halfDistance\": %1 },\n" ).arg( num( dyeHalfDistance ) );
	s += QStringLiteral( "  \"curves\": [" );
	for ( int i = 0; i < curves.size(); i++ ) {
		const WaterCurve & c = curves[i];
		s += ( i ? QLatin1String( ",\n    {" ) : QLatin1String( "\n    {" ) );
		s += QStringLiteral( " \"kind\": \"%1\", \"body\": %2, \"enabled\": %3, \"speed\": %4, \"width\": %5" )
			.arg( QLatin1String( kindName( c.kind ) ) ).arg( c.body )
			.arg( c.enabled ? QLatin1String( "true" ) : QLatin1String( "false" ) )
			.arg( num( c.speed ) ).arg( num( c.width ) );
		if ( c.kind == WaterCurve::DyePin )
			s += QStringLiteral( ", \"colour\": [%1, %2, %3, %4]" )
				.arg( c.colour[0] ).arg( c.colour[1] ).arg( c.colour[2] ).arg( c.colour[3] );
		s += QLatin1String( ",\n      \"points\": [" );
		for ( int k = 0; k < c.pts.size(); k++ ) {
			const WaterCurvePoint & p = c.pts[k];
			s += ( k ? QLatin1String( ", " ) : QLatin1String( "" ) );
			if ( k && ( k % 6 ) == 0 )
				s += QLatin1String( "\n        " );
			s += QStringLiteral( "[%1, %2, %3]" ).arg( num( p.x ) ).arg( num( p.y ) ).arg( num( p.w ) );
		}
		s += QLatin1String( "] }" );
	}
	s += ( curves.isEmpty() ? QLatin1String( "],\n" ) : QLatin1String( "\n  ],\n" ) );
	s += QStringLiteral( "  \"bodies\": [" );
	int nb = 0;
	for ( const WaterBodyOverride & o : bodies ) {
		if ( o.isEmpty() )
			continue;
		s += ( nb++ ? QLatin1String( ",\n    {" ) : QLatin1String( "\n    {" ) );
		s += QStringLiteral( " \"id\": %1, \"name\": %2, \"class\": \"%3\", \"form\": %4, \"colour\": %5, "
			"\"still\": %6, \"dyeMouth\": %7, \"dyeStrength\": %8 }" )
			.arg( o.id ).arg( jsonString( o.name ) ).arg( QLatin1String( className( o.cls ) ) )
			.arg( o.form ? QStringLiteral( "\"0x%1\"" ).arg( o.form, 8, 16, QLatin1Char( '0' ) )
				: QStringLiteral( "null" ) )
			.arg( o.colour[3] ? QStringLiteral( "[%1, %2, %3]" ).arg( o.colour[0] ).arg( o.colour[1] ).arg( o.colour[2] )
				: QStringLiteral( "null" ) )
			.arg( o.still ? QLatin1String( "true" ) : QLatin1String( "false" ) )
			.arg( o.dyeMouth ? QLatin1String( "true" ) : QLatin1String( "false" ) )
			.arg( num( o.dyeStrength ) );
	}
	s += ( nb ? QLatin1String( "\n  ],\n" ) : QLatin1String( "],\n" ) );
	s += QStringLiteral( "  \"rasters\": [" );
	for ( int i = 0; i < rasters.size(); i++ ) {
		const WaterRasterLayer & r = rasters[i];
		s += ( i ? QLatin1String( ",\n    {" ) : QLatin1String( "\n    {" ) );
		s += QStringLiteral( " \"file\": %1, \"sha256\": %2, \"origin\": [%3, %4], \"size\": [%5, %6], \"painted\": %7 }" )
			.arg( jsonString( r.sourceFile ) ).arg( jsonString( QString::fromLatin1( r.sha256 ) ) )
			.arg( r.px0 ).arg( r.py0 ).arg( r.w ).arg( r.h ).arg( r.paintedCount() );
	}
	s += ( rasters.isEmpty() ? QLatin1String( "]\n" ) : QLatin1String( "\n  ]\n" ) );
	s += QLatin1String( "}\n" );
	return s.toUtf8();
}

bool WaterCurveDoc::fromJson( const QByteArray & bytes, QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	QJsonParseError pe;
	const QJsonDocument jd = QJsonDocument::fromJson( bytes, &pe );
	if ( jd.isNull() || !jd.isObject() )
		return fail( QStringLiteral( "not a json object: %1 at offset %2" )
			.arg( pe.errorString() ).arg( pe.offset ) );
	const QJsonObject o = jd.object();
	if ( o.value( QLatin1String( "format" ) ).toString() != QLatin1String( "ww-water-curves" ) )
		return fail( QStringLiteral( "the file's \"format\" is \"%1\", not \"ww-water-curves\"" )
			.arg( o.value( QLatin1String( "format" ) ).toString() ) );
	const int ver = o.value( QLatin1String( "version" ) ).toInt( -1 );
	if ( ver < 1 || ver > kJsonVersion )
		return fail( QStringLiteral( "the file is version %1; this reader knows 1..%2" )
			.arg( ver ).arg( kJsonVersion ) );
	WaterCurveDoc d;
	d.worldspace = o.value( QLatin1String( "worldspace" ) ).toString();
	d.landFile = o.value( QLatin1String( "landFile" ) ).toString();
	const QJsonArray cells = o.value( QLatin1String( "cells" ) ).toArray();
	if ( cells.size() == 4 ) {
		d.cellMinX = cells[0].toInt();
		d.cellMinY = cells[1].toInt();
		d.cellMaxX = cells[2].toInt();
		d.cellMaxY = cells[3].toInt();
	}
	d.bodySamples = o.value( QLatin1String( "bodySamples" ) ).toInt();
	d.dyeHalfDistance = o.value( QLatin1String( "dye" ) ).toObject()
		.value( QLatin1String( "halfDistance" ) ).toDouble( 8192.0 );
	const QJsonArray cs = o.value( QLatin1String( "curves" ) ).toArray();
	for ( int i = 0; i < cs.size(); i++ ) {
		const QJsonObject co = cs[i].toObject();
		WaterCurve c;
		const int k = kindFromName( co.value( QLatin1String( "kind" ) ).toString() );
		if ( k < 0 )
			return fail( QStringLiteral( "curve %1 has kind \"%2\", which this reader does not know" )
				.arg( i + 1 ).arg( co.value( QLatin1String( "kind" ) ).toString() ) );
		c.kind = quint8( k );
		c.body = quint16( co.value( QLatin1String( "body" ) ).toInt() );
		c.enabled = co.value( QLatin1String( "enabled" ) ).toBool( true );
		c.speed = float( co.value( QLatin1String( "speed" ) ).toDouble( 0.25 ) );
		c.width = float( co.value( QLatin1String( "width" ) ).toDouble( 4096.0 ) );
		const QJsonArray col = co.value( QLatin1String( "colour" ) ).toArray();
		if ( col.size() == 4 )
			for ( int j = 0; j < 4; j++ )
				c.colour[j] = quint8( qBound( 0, col[j].toInt(), 255 ) );
		const QJsonArray pts = co.value( QLatin1String( "points" ) ).toArray();
		if ( pts.isEmpty() )
			return fail( QStringLiteral( "curve %1 has no points" ).arg( i + 1 ) );
		for ( int j = 0; j < pts.size(); j++ ) {
			const QJsonArray p = pts[j].toArray();
			if ( p.size() < 2 )
				return fail( QStringLiteral( "curve %1 point %2 is not [x, y, w]" ).arg( i + 1 ).arg( j + 1 ) );
			WaterCurvePoint q;
			q.x = float( p[0].toDouble() );
			q.y = float( p[1].toDouble() );
			q.w = p.size() >= 3 ? float( p[2].toDouble( 1.0 ) ) : 1.0f;
			c.pts.append( q );
		}
		d.curves.append( c );
	}
	const QJsonArray bs = o.value( QLatin1String( "bodies" ) ).toArray();
	for ( int i = 0; i < bs.size(); i++ ) {
		const QJsonObject bo = bs[i].toObject();
		WaterBodyOverride ov;
		ov.id = bo.value( QLatin1String( "id" ) ).toInt();
		if ( ov.id <= 0 )
			return fail( QStringLiteral( "body override %1 has no id" ).arg( i + 1 ) );
		ov.name = bo.value( QLatin1String( "name" ) ).toString();
		ov.cls = classFromName( bo.value( QLatin1String( "class" ) ).toString() );
		const QJsonValue fv = bo.value( QLatin1String( "form" ) );
		if ( fv.isString() ) {
			bool ok = false;
			const quint32 f = fv.toString().toUInt( &ok, 16 );
			ov.form = ok ? f : 0;
		}
		const QJsonArray col = bo.value( QLatin1String( "colour" ) ).toArray();
		if ( col.size() == 3 ) {
			for ( int j = 0; j < 3; j++ )
				ov.colour[j] = quint8( qBound( 0, col[j].toInt(), 255 ) );
			ov.colour[3] = 255;
		}
		ov.still = bo.value( QLatin1String( "still" ) ).toBool( false );
		ov.dyeMouth = bo.value( QLatin1String( "dyeMouth" ) ).toBool( false );
		ov.dyeStrength = float( bo.value( QLatin1String( "dyeStrength" ) ).toDouble( 1.0 ) );
		d.bodies.append( ov );
	}
	const QJsonArray rs = o.value( QLatin1String( "rasters" ) ).toArray();
	for ( int i = 0; i < rs.size(); i++ ) {
		const QJsonObject ro = rs[i].toObject();
		WaterRasterLayer r;
		r.sourceFile = ro.value( QLatin1String( "file" ) ).toString();
		r.sha256 = ro.value( QLatin1String( "sha256" ) ).toString().toLatin1();
		const QJsonArray org = ro.value( QLatin1String( "origin" ) ).toArray();
		const QJsonArray sz = ro.value( QLatin1String( "size" ) ).toArray();
		if ( org.size() == 2 ) {
			r.px0 = org[0].toInt();
			r.py0 = org[1].toInt();
		}
		if ( sz.size() == 2 ) {
			r.w = sz[0].toInt();
			r.h = sz[1].toInt();
		}
		/* The words are NOT in the json -- the PNG is their source; the
		 * window re-imports it from beside the json when it exists, and says
		 * so when it does not.  The layer arrives here empty of texels. */
		d.rasters.append( r );
	}
	*this = d;
	return true;
}

bool WaterCurveDoc::saveJson( const QString & path, QString * error ) const
{
	QSaveFile f( path );
	if ( !f.open( QIODevice::WriteOnly ) ) {
		if ( error )
			*error = QStringLiteral( "cannot write %1: %2" ).arg( path, f.errorString() );
		return false;
	}
	const QByteArray b = toJson();
	if ( f.write( b ) != b.size() || !f.commit() ) {
		if ( error )
			*error = QStringLiteral( "writing %1 failed: %2" ).arg( path, f.errorString() );
		return false;
	}
	return true;
}

bool WaterCurveDoc::loadJson( const QString & path, QString * error )
{
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		if ( error )
			*error = QStringLiteral( "cannot read %1: %2" ).arg( path, f.errorString() );
		return false;
	}
	return fromJson( f.readAll(), error );
}

// ---- the .lodl ------------------------------------------------------------

QVector<QVector<float>> WaterCurveDoc::weightsFromStore( const QByteArray & rawStore )
{
	QVector<QVector<float>> out;
	for ( const StoreExtra & e : parseStoreExtras( rawStore ) )
		out.append( e.weights );
	return out;
}

QVector<QByteArray> WaterCurveDoc::rastersFromStore( const QByteArray & rawStore )
{
	QVector<QByteArray> out;
	for ( const StoreExtra & e : parseStoreExtras( rawStore ) )
		if ( e.kind == kRasterKind )
			out.append( e.payload );
	return out;
}

bool WaterCurveDoc::readFrom( const WaterMarkDoc & doc, QString * error )
{
	if ( !doc.isOpen() ) {
		if ( error )
			*error = QStringLiteral( "no landscape file is open" );
		return false;
	}
	clear();
	const QFileInfo fi( doc.path() );
	worldspace = fi.completeBaseName();
	landFile = fi.fileName();
	if ( const LodtFile * f = doc.file() ) {
		cellMinX = f->cellMinX();
		cellMinY = f->cellMinY();
		cellMaxX = f->cellMaxX();
		cellMaxY = f->cellMaxY();
		bodySamples = f->bodyIdSamples();
	}
	dyeHalfDistance = doc.dyeHalfDistance();

	const QVector<StoreExtra> extras = doc.file()
		? parseStoreExtras( doc.file()->strokeStore() ) : QVector<StoreExtra>();
	const QVector<WaterStroke> & ss = doc.strokes();
	for ( int i = 0; i < ss.size(); i++ ) {
		const WaterStroke & s = ss[i];
		const StoreExtra * e = i < extras.size() ? &extras[i] : nullptr;
		if ( s.kind == WaterStroke::Stroke || s.kind == WaterStroke::Pin
			|| s.kind == WaterStroke::SourcePin || s.kind == WaterStroke::OutletPin
			|| s.kind == WaterStroke::DyePin ) {
			WaterCurve c;
			c.kind = ( s.kind == WaterStroke::Pin ) ? quint8( WaterCurve::Curve ) : s.kind;
			c.body = s.body;
			c.enabled = s.enabled();
			c.speed = s.speed;
			c.width = s.width;
			for ( int j = 0; j < 4; j++ )
				c.colour[j] = s.colour[j];
			for ( int k = 0; k < s.pts.size(); k++ ) {
				WaterCurvePoint p;
				p.x = s.pts[k].x;
				p.y = s.pts[k].y;
				p.w = ( e && k < e->weights.size() ) ? e->weights[k] : 1.0f;
				c.pts.append( p );
			}
			curves.append( c );
		} else if ( s.kind == kRasterKind && e && !e->payload.isEmpty() ) {
			WaterRasterLayer r;
			QString why;
			if ( r.fromPayload( e->payload, &why ) )
				rasters.append( r );
			else if ( error )
				*error = why;
		}
	}
	for ( int id = 1; id <= doc.bodyCount(); id++ ) {
		LodtWaterBody b;
		if ( !doc.body( id, b ) )
			continue;
		WaterBodyOverride o;
		o.id = id;
		o.name = doc.bodyName( id );
		o.cls = ( b.flags & ( 1u << 5 ) ) ? int( b.cls ) : -1;
		if ( b.colour[3] )
			for ( int j = 0; j < 4; j++ )
				o.colour[j] = b.colour[j];
		o.still = doc.bodyLockZero( id );
		float strength = 1.0f;
		o.dyeMouth = doc.bodyDyeMouth( id, &strength );
		o.dyeStrength = strength;
		if ( !o.isEmpty() )
			bodies.append( o );
	}
	return true;
}

bool WaterCurveDoc::writeTo( WaterMarkDoc & doc, int * refused, QString * error ) const
{
	if ( !doc.isOpen() ) {
		if ( error )
			*error = QStringLiteral( "no landscape file is open" );
		return false;
	}
	// the kinds this model owns leave; the doc's own marks (6, 8, 9) stay
	{
		const QVector<WaterStroke> & ss = doc.strokes();
		for ( int i = ss.size() - 1; i >= 0; i-- ) {
			const quint8 k = ss[i].kind;
			if ( k == WaterStroke::Stroke || k == WaterStroke::Pin || k == WaterStroke::SourcePin
				|| k == WaterStroke::OutletPin || k == WaterStroke::DyePin || k == kRasterKind )
				doc.removeStroke( i );
		}
	}
	int nRefused = 0;
	QString first;
	for ( const WaterCurve & c : curves ) {
		if ( c.pts.isEmpty() )
			continue;
		WaterStroke s;
		s.kind = ( c.kind == WaterCurve::Curve && c.pts.size() == 1 )
			? quint8( WaterStroke::Pin ) : c.kind;
		s.body = c.body;
		s.flags = quint8( WaterStroke::SetsDirection | WaterStroke::SetsSpeed
			| ( c.enabled ? 0 : WaterStroke::Disabled ) );
		s.speed = c.speed;
		s.width = c.width;
		for ( int j = 0; j < 4; j++ )
			s.colour[j] = c.colour[j];
		for ( const WaterCurvePoint & p : c.pts ) {
			WaterStrokePoint q;
			q.x = p.x;
			q.y = p.y;
			s.pts.append( q );
		}
#ifdef WATERMARK_STROKE_EXTRA
		/* Hook-up H2 (scratchpad/water5_20260910/PENDING.md): the store's
		 * codec keeps a record's trailing bytes in `extra`; a curve's are one
		 * float a point, the speed weights.
		 *
		 * LANE WATER7, red 5a of BUILD10: DyePin was missing from this list
		 * while readFrom() READS a dye pin's weights back (the `e->weights`
		 * branch above covers Stroke, Pin, SourcePin, OutletPin and DyePin
		 * alike), so a dye pin's per-point weight was read, shown, edited --
		 * and then dropped on the way out, silently, reading 1.0 on the next
		 * open. Writing it makes read and write the same set. The record's
		 * codec already puts a DyePin's trailing bytes AFTER its four colour
		 * bytes, on both sides (WaterMarkDoc::encodeStrokes / decodeStrokes),
		 * so nothing about the layout has to change.
		 *
		 * SourcePin and OutletPin are deliberately NOT added here: they are
		 * single points whose weight the solver never reads, so writing four
		 * bytes for them would move the file's bytes for no behaviour. That is
		 * a candidate, not a fix, and section 3 of the lane report names the
		 * gate that would settle it. */
		if ( s.kind == WaterStroke::Stroke || s.kind == WaterStroke::Pin
			|| s.kind == WaterStroke::DyePin ) {
			Buf w;
			for ( const WaterCurvePoint & p : c.pts )
				w.f32( p.w );
			s.extra = w.b;
		}
#endif
		QString msg;
		if ( !doc.addStroke( s, &msg ) ) {
			nRefused++;
			if ( first.isEmpty() )
				first = msg;
		}
	}
#ifdef WATERMARK_STROKE_EXTRA
	for ( const WaterRasterLayer & r : rasters ) {
		WaterStroke s;
		s.kind = kRasterKind;
		s.flags = WaterStroke::SetsDirection | WaterStroke::SetsSpeed;
		s.extra = r.payload();
		QString msg;
		if ( !doc.addStroke( s, &msg ) ) {
			nRefused++;
			if ( first.isEmpty() )
				first = msg;
		}
	}
#endif
	for ( const WaterBodyOverride & o : bodies ) {
		if ( o.id < 1 || o.id > doc.bodyCount() )
			continue;
		doc.setBodyName( o.id, o.name );
		doc.setBodyClass( o.id, o.cls );
		doc.setBodyColour( o.id, o.colour[0], o.colour[1], o.colour[2], o.colour[3] != 0 );
		if ( o.form )
			doc.setBodyForm( o.id, o.form );
		doc.setBodyLockZero( o.id, o.still );
		doc.setBodyDyeMouth( o.id, o.dyeMouth, o.dyeStrength );
	}
	doc.setDyeHalfDistance( dyeHalfDistance );
	if ( refused )
		*refused = nRefused;
	if ( nRefused && error )
		*error = QStringLiteral( "%1 curve(s) refused by the land file; the first: %2" )
			.arg( nRefused ).arg( first );
	return true;
}

bool WaterCurveDoc::sameCurves( const WaterCurveDoc & a, const WaterCurveDoc & b, QString * why )
{
	auto no = [why]( const QString & m ) {
		if ( why )
			*why = m;
		return false;
	};
	if ( a.curves.size() != b.curves.size() )
		return no( QStringLiteral( "%1 curves against %2" ).arg( a.curves.size() ).arg( b.curves.size() ) );
	for ( int i = 0; i < a.curves.size(); i++ ) {
		const WaterCurve & x = a.curves[i], & y = b.curves[i];
		if ( x.kind != y.kind || x.body != y.body || x.enabled != y.enabled )
			return no( QStringLiteral( "curve %1: kind/body/enabled differ" ).arg( i + 1 ) );
		if ( x.speed != y.speed || x.width != y.width )
			return no( QStringLiteral( "curve %1: speed %2/%3 width %4/%5" ).arg( i + 1 )
				.arg( double( x.speed ) ).arg( double( y.speed ) ).arg( double( x.width ) ).arg( double( y.width ) ) );
		if ( x.pts.size() != y.pts.size() )
			return no( QStringLiteral( "curve %1: %2 points against %3" ).arg( i + 1 )
				.arg( x.pts.size() ).arg( y.pts.size() ) );
		for ( int k = 0; k < x.pts.size(); k++ ) {
			if ( x.pts[k].x != y.pts[k].x || x.pts[k].y != y.pts[k].y )
				return no( QStringLiteral( "curve %1 point %2: (%3, %4) against (%5, %6)" ).arg( i + 1 ).arg( k + 1 )
					.arg( double( x.pts[k].x ) ).arg( double( x.pts[k].y ) )
					.arg( double( y.pts[k].x ) ).arg( double( y.pts[k].y ) ) );
			if ( x.pts[k].w != y.pts[k].w )
				return no( QStringLiteral( "curve %1 point %2: weight %3 against %4" ).arg( i + 1 ).arg( k + 1 )
					.arg( double( x.pts[k].w ) ).arg( double( y.pts[k].w ) ) );
		}
		if ( x.kind == WaterCurve::DyePin )
			for ( int j = 0; j < 4; j++ )
				if ( x.colour[j] != y.colour[j] )
					return no( QStringLiteral( "curve %1: dye colour differs" ).arg( i + 1 ) );
	}
	return true;
}

// ---- PNG ------------------------------------------------------------------

/* THE CONVENTION (lane WATER6).  R and G carry the direction as a DirectX
 * normal map does: both channels are centred on 128 with a scale of 127,
 * R is +X (east), and G is +Y TOWARD THE IMAGE BOTTOM -- so a texel whose
 * water runs north is DARK green (1) and one whose water runs south is
 * bright (255).  Up to 2026-09-10 this tool wrote +green = north, which is
 * the OpenGL convention and the opposite of what Substance, Houdini and
 * every DirectX-era engine exporter write.  B is the speed nibble x 17 and
 * A the confidence nibble x 17.
 *
 * The mapping round-trips every one of the 256 directions exactly (gate
 * X5b, 65,536 words), and `tests/fixtures/flowmap_directx_4x4.png` is the
 * same rule written by a script that shares no code with this one (X5c). */
quint16 WaterCurveDoc::wordFromRgba( quint8 r, quint8 g, quint8 b, quint8 a )
{
	if ( !a && !r && !g && !b )
		return 0;
	const double cx = ( double( r ) - 128.0 ) / 127.0;
	const double cy = -( ( double( g ) - 128.0 ) / 127.0 );   // +G is SOUTH
	double ang = std::atan2( cy, cx );
	if ( ang < 0.0 )
		ang += kTwoPi;
	const int dir = int( std::lround( ang / kTwoPi * 256.0 ) ) & 0xFF;
	const int speed = qBound( 0, int( std::lround( double( b ) / 17.0 ) ), 15 );
	const int conf = qBound( 0, int( std::lround( double( a ) / 17.0 ) ), 15 );
	return quint16( dir | ( speed << 8 ) | ( conf << 12 ) );
}

void WaterCurveDoc::rgbaFromWord( quint16 word, quint8 & r, quint8 & g, quint8 & b, quint8 & a )
{
	const double ang = double( word & 0xFF ) / 256.0 * kTwoPi;
	r = quint8( qBound( 0, int( 128 + std::lround( 127.0 * std::cos( ang ) ) ), 255 ) );
	g = quint8( qBound( 0, int( 128 + std::lround( 127.0 * -std::sin( ang ) ) ), 255 ) );
	b = quint8( ( ( word >> 8 ) & 0xF ) * 17 );
	a = quint8( ( ( word >> 12 ) & 0xF ) * 17 );
}

bool WaterCurveDoc::exportFlowPng( const WaterMarkDoc & doc, const QString & flowPng,
	const QString & maskPng, qint64 * wetTexels, QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	if ( !doc.isOpen() )
		return fail( QStringLiteral( "no landscape file is open" ) );
	int tw = 0, th = 0;
	gridOf( doc, tw, th );
	if ( tw <= 0 || th <= 0 )
		return fail( QStringLiteral( "the file's grid is %1 x %2" ).arg( tw ).arg( th ) );
	QImage flow( tw, th, QImage::Format_RGBA8888 );
	QImage mask( tw, th, QImage::Format_Grayscale16 );
	if ( flow.isNull() || mask.isNull() )
		return fail( QStringLiteral( "cannot allocate two %1 x %2 images" ).arg( tw ).arg( th ) );
	flow.fill( Qt::transparent );
	mask.fill( 0 );
	qint64 wet = 0;
	QString err;
	/* WET TEXELS CARRY A = confidence x 17, which for a body nobody marked is
	 * 0 -- the same 0,0,0,0 a dry texel writes.  So a wet texel's alpha is
	 * floored at 1: painted, confidence 0.  wordFromRgba rounds 1 / 17 back
	 * to 0, so the round trip stays exact, and an importer can tell water
	 * from land by alpha alone. */
	const bool ok = doc.sweep( [&]( int px, int py, quint16 id, quint16, quint16 wordNow ) {
		if ( !id || px < 0 || py < 0 || px >= tw || py >= th )
			return;
		wet++;
		quint8 r = 0, g = 0, b = 0, a = 0;
		rgbaFromWord( wordNow, r, g, b, a );
		if ( a == 0 )
			a = 1;
		const int row = th - 1 - py;
		uchar * line = flow.scanLine( row ) + qsizetype( px ) * 4;
		line[0] = r;
		line[1] = g;
		line[2] = b;
		line[3] = a;
		reinterpret_cast<quint16 *>( mask.scanLine( row ) )[px] = id;
	}, &err );
	if ( !ok )
		return fail( err );
	if ( wetTexels )
		*wetTexels = wet;
	if ( !flow.save( flowPng, "PNG" ) )
		return fail( QStringLiteral( "cannot write %1" ).arg( flowPng ) );
	if ( !maskPng.isEmpty() && !mask.save( maskPng, "PNG" ) )
		return fail( QStringLiteral( "cannot write %1" ).arg( maskPng ) );
	return true;
}

bool WaterCurveDoc::importFlowPng( const WaterMarkDoc & doc, const QString & flowPng,
	WaterRasterLayer & out, double * agreeAsIs, double * agreeFlipped, QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	if ( agreeAsIs )
		*agreeAsIs = 0.0;
	if ( agreeFlipped )
		*agreeFlipped = 0.0;
	if ( !doc.isOpen() )
		return fail( QStringLiteral( "no landscape file is open" ) );
	QFile f( flowPng );
	if ( !f.open( QIODevice::ReadOnly ) )
		return fail( QStringLiteral( "cannot read %1: %2" ).arg( flowPng, f.errorString() ) );
	const QByteArray bytes = f.readAll();
	f.close();
	QImage img;
	if ( !img.loadFromData( bytes, "PNG" ) || img.isNull() )
		return fail( QStringLiteral( "%1 is not a PNG this reader can decode" ).arg( flowPng ) );
	int tw = 0, th = 0;
	gridOf( doc, tw, th );
	if ( img.width() != tw || img.height() != th )
		return fail( QStringLiteral( "%1 is %2 x %3; the file's body-plane grid is %4 x %5, and a flow "
			"map is imported at that grid" ).arg( QFileInfo( flowPng ).fileName() )
			.arg( img.width() ).arg( img.height() ).arg( tw ).arg( th ) );
	if ( img.format() != QImage::Format_RGBA8888 )
		img = img.convertToFormat( QImage::Format_RGBA8888 );
	WaterRasterLayer r;
	r.px0 = 0;
	r.py0 = 0;
	r.w = tw;
	r.h = th;
	r.words.resize( qsizetype( tw ) * th );
	r.painted.resize( qsizetype( tw ) * th );
	r.sourceFile = QFileInfo( flowPng ).fileName();
	r.sha256 = QCryptographicHash::hash( bytes, QCryptographicHash::Sha256 ).toHex();
	for ( int py = 0; py < th; py++ ) {
		const uchar * line = img.constScanLine( th - 1 - py );
		for ( int px = 0; px < tw; px++ ) {
			const uchar * q = line + qsizetype( px ) * 4;
			const qsizetype i = qsizetype( py ) * tw + px;
			r.painted[i] = q[3] ? 1 : 0;
			r.words[i] = q[3] ? wordFromRgba( q[0], q[1], q[2], q[3] ) : 0;
		}
	}
	/* THE FLIPPED-GREEN TEST.  A flow map from another tool with +G = NORTH
	 * (the OpenGL convention, and what this tool itself wrote before lane
	 * WATER6) would import as a field mirrored about east-west, and nothing
	 * in the
	 * PNG says which way it was written.  The file itself does: over the
	 * painted wet texels, the mean cosine between the map's direction and
	 * the document's own is taken as-is and with the green mirrored (theta ->
	 * -theta); when the mirrored reading agrees better and the as-is reading
	 * is poor, the map is refused with both numbers.  A map of a body nobody
	 * has marked is compared against the automatic word, which is the body's
	 * mean direction, so the test still has teeth there. */
	double sumAs = 0.0, sumFl = 0.0;
	qint64 n = 0;
	QString err;
	const bool ok = doc.sweep( [&]( int px, int py, quint16 id, quint16, quint16 wordNow ) {
		if ( !id || !wordNow || px < 0 || py < 0 || px >= tw || py >= th )
			return;
		const qsizetype i = qsizetype( py ) * tw + px;
		if ( !r.painted[i] || !r.words[i] )
			return;
		const double a = double( r.words[i] & 0xFF ) / 256.0 * kTwoPi;
		const double d = double( wordNow & 0xFF ) / 256.0 * kTwoPi;
		sumAs += std::cos( a - d );
		sumFl += std::cos( -a - d );
		n++;
	}, &err );
	if ( !ok )
		return fail( err );
	const double asIs = n ? sumAs / double( n ) : 0.0;
	const double flipped = n ? sumFl / double( n ) : 0.0;
	if ( agreeAsIs )
		*agreeAsIs = asIs;
	if ( agreeFlipped )
		*agreeFlipped = flipped;
	if ( n > 0 && flipped > asIs && asIs < 0.9 )
		return fail( QStringLiteral( "the green channel of %1 looks flipped: over %2 painted texels the "
			"map agrees with this file's flow at %3 as it is and at %4 with green mirrored. This tool "
			"writes +green = south, the DirectX convention (green grows toward the image bottom); "
			"flip the channel and import again" )
			.arg( QFileInfo( flowPng ).fileName() ).arg( n )
			.arg( asIs, 0, 'f', 3 ).arg( flipped, 0, 'f', 3 ) );
	out = r;
	return true;
}

bool WaterCurveDoc::flipGreen( const QString & inPng, const QString & outPng, QString * error )
{
	QImage img( inPng );
	if ( img.isNull() ) {
		if ( error )
			*error = QStringLiteral( "cannot read %1" ).arg( inPng );
		return false;
	}
	img = img.convertToFormat( QImage::Format_RGBA8888 );
	for ( int y = 0; y < img.height(); y++ ) {
		uchar * line = img.scanLine( y );
		for ( int x = 0; x < img.width(); x++ )
			line[qsizetype( x ) * 4 + 1] = uchar( 255 - line[qsizetype( x ) * 4 + 1] );
	}
	if ( !img.save( outPng, "PNG" ) ) {
		if ( error )
			*error = QStringLiteral( "cannot write %1" ).arg( outPng );
		return false;
	}
	return true;
}

bool WaterCurveDoc::rasterWordAt( int px, int py, quint16 & word ) const
{
	// the last layer painted wins, as in any image editor's layer stack
	for ( int i = rasters.size() - 1; i >= 0; i-- )
		if ( rasters[i].wordAt( px, py, word ) )
			return true;
	return false;
}
