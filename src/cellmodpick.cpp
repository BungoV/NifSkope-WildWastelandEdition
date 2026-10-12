/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENSE BLOCK *****/

#include "cellmodpick.h"

#include "lodbfile.h"
#include "lodgen.h"
#include "lodgenloadorder.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSettings>

// lane MODPICK1 -- see cellmodpick.h

namespace {

QHash<QString, QString> & shownBake()    { static QHash<QString, QString> h; return h; }	// ws (lower) -> .lodb
QHash<QString, QString> & loadedStack()  { static QHash<QString, QString> h; return h; }	// ws (lower) -> comma list
QHash<const void *, QString> & viewNotes() { static QHash<const void *, QString> h; return h; }

QString key( const QString & ws )
{
	return QStringLiteral( "CellView/ModPick/%1" ).arg( ws.toLower() );
}

QString fileOf( const QString & path )
{
	return QFileInfo( path.trimmed() ).fileName();
}

QStringList namesOf( const QStringList & paths )
{
	QStringList n;
	for ( const QString & p : paths )
		n << fileOf( p ).toLower();
	return n;
}

//! The mod folder a plugin path lives in: <mods>/<name>/..., MO2's overwrite, else the game's Data.
QString modOfPath( const QString & path )
{
	const QString p = QDir::fromNativeSeparators( path );
	const int m = p.indexOf( QStringLiteral( "/mods/" ), 0, Qt::CaseInsensitive );
	if ( m >= 0 ) {
		const int s = m + 6;
		const int e = p.indexOf( QLatin1Char( '/' ), s );
		if ( e > s )
			return p.mid( s, e - s );
	}
	if ( p.contains( QStringLiteral( "/overwrite/" ), Qt::CaseInsensitive ) )
		return QStringLiteral( "overwrite" );
	return QStringLiteral( "Data" );
}

//! His MO2 profile: the LOD panel's field, else the profile this machine keeps (as the LOD panel defaults).
QString mo2Profile()
{
	QSettings s;
	QString p = s.value( QStringLiteral( "LodGeneration/mo2Profile" ) ).toString().trimmed();
	if ( p.isEmpty() && QFileInfo( QStringLiteral( "E:/Projects/Fallout 4 Mods/profiles/Default/modlist.txt" ) ).isFile() )
		p = QStringLiteral( "E:/Projects/Fallout 4 Mods/profiles/Default" );
	return p;
}

bool readMo2( LodgenLoadOrder * lo, QString * err )
{
	const QString profile = mo2Profile();
	if ( profile.isEmpty() ) {
		*err = QStringLiteral( "no MO2 profile set (the LOD panel's Profile field)" );
		return false;
	}
	const QString mods = QSettings().value( QStringLiteral( "LodGeneration/mo2Mods" ) ).toString().trimmed();
	return lodgenLoadOrderFromMo2( profile, mods, QString(), lo, err );
}

bool redInert()
{
	return qgetenv( "WW_MODPICK_RED" ) == "inert";
}

} // namespace

QString cellModPickShownBake( const QString & ws )
{
	return shownBake().value( ws.toLower() );
}

QString cellModPickLoaded( const QString & ws )
{
	return loadedStack().value( ws.toLower() );
}

bool cellModPickTouched( const QString & ws )
{
	return !ws.isEmpty() && QSettings().contains( key( ws ) );
}

QStringList cellModPickOff( const QString & ws )
{
	QStringList off;
	for ( const QString & n : QSettings().value( key( ws ) ).toStringList() )
		if ( !n.trimmed().isEmpty() )
			off << n.trimmed().toLower();
	return off;
}

void cellModPickSave( const QString & ws, const QStringList & off )
{
	QStringList o;
	for ( const QString & n : off )
		o << n.toLower();
	QSettings().setValue( key( ws ), o );
}

void cellModPickReset( const QString & ws )
{
	QSettings().remove( key( ws ) );
}

CellModList cellModPickList( const QString & ws, const QString & lodb )
{
	CellModList L;
	L.ws = ws;
	if ( !lodb.isEmpty() ) {
		LodgenLedger led;
		QString err;
		if ( lodgenReadLedger( lodb, &led, &err ) && !led.plugins.isEmpty() ) {
			L.source = QStringLiteral( "bake" );
			L.from = lodb;
			for ( const LodbPlugin & p : std::as_const( led.plugins ) ) {
				CellModRow r;
				r.path = p.path;
				r.file = fileOf( p.path );
				if ( r.file.isEmpty() )
					r.file = p.name;
				r.mod = modOfPath( p.path );
				r.present = QFileInfo( p.path ).isFile();
				L.rows << r;
			}
		} else {
			L.error = QStringLiteral( "the far LOD bake's record %1 could not be read%2" )
				.arg( QDir::toNativeSeparators( lodb ) ).arg( err.isEmpty() ? QString() : QStringLiteral( ": " ) + err );
		}
	}
	if ( L.source.isEmpty() ) {
		LodgenLoadOrder lo;
		QString err;
		if ( readMo2( &lo, &err ) ) {
			L.source = QStringLiteral( "mo2" );
			L.from = lo.profileDir;
			L.error.clear();
			for ( int i = 0; i < lo.plugins.size(); i++ ) {
				CellModRow r;
				r.path = lo.plugins.at( i );
				r.file = fileOf( r.path );
				const QString from = i < lo.pluginFrom.size() ? lo.pluginFrom.at( i ) : QString();
				r.mod = from.isEmpty() || from == QLatin1String( "data" ) ? QStringLiteral( "Data" ) : from;
				r.present = QFileInfo( r.path ).isFile();
				L.rows << r;
			}
		} else if ( L.error.isEmpty() ) {
			L.error = QStringLiteral( "MO2 profile: %1" ).arg( err );
		}
	}
	// the ticks: the saved choice, else what the last build of this worldspace loaded
	const bool touched = cellModPickTouched( ws );
	const QStringList off = cellModPickOff( ws );
	const QStringList loaded = namesOf( cellModPickLoaded( ws ).split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) );
	for ( CellModRow & r : L.rows ) {
		const QString n = r.file.toLower();
		r.on = r.present && ( touched ? !off.contains( n ) : ( loaded.isEmpty() || loaded.contains( n ) ) );
	}
	return L;
}

CellModApplied cellModPickApply( const QString & ws, const QString & plugins, const QString & lodb,
	const QStringList & bakePresent )
{
	CellModApplied r;
	r.plugins = plugins;
	const QStringList asked = plugins.split( QLatin1Char( ',' ), Qt::SkipEmptyParts );
	shownBake().insert( ws.toLower(), lodb );
	if ( !cellModPickTouched( ws ) ) {
		r.line = QStringLiteral( "mods: the picker is untouched -- %1 plugins as the cell asked" ).arg( asked.size() );
	} else if ( redInert() ) {
		r.line = QStringLiteral( "mods: the saved choice for %1 is NOT applied (RED inert) -- %2 plugins as the cell asked" )
			.arg( ws ).arg( asked.size() );
	} else {
		// the list the picker shows, its present files only
		QStringList base, resStack;
		QString source;
		if ( !lodb.isEmpty() ) {
			base = bakePresent;
			source = QStringLiteral( "the far LOD bake" );
		} else {
			LodgenLoadOrder lo;
			QString err;
			if ( readMo2( &lo, &err ) ) {
				for ( const QString & p : std::as_const( lo.plugins ) )
					if ( QFileInfo( p ).isFile() )
						base << p;
				source = QStringLiteral( "the MO2 profile" );
				resStack = lo.stack;	// its plugins' own assets: the profile's mods, not only Data
			}
		}
		if ( base.isEmpty() ) {
			base = asked;
			source = QStringLiteral( "the cell's own list" );
		}
		const QStringList off = cellModPickOff( ws );
		QStringList kept, dropped;
		for ( const QString & p : std::as_const( base ) ) {
			if ( off.contains( fileOf( p ).toLower() ) )
				dropped << fileOf( p );
			else
				kept << p.trimmed();
		}
		if ( kept.isEmpty() ) {
			r.line = QStringLiteral( "mods: the saved choice for %1 leaves no plugin -- NOT applied, %2 plugins as the cell asked" )
				.arg( ws ).arg( asked.size() );
		} else {
			r.plugins = kept.join( QLatin1Char( ',' ) );
			r.resources = resStack;
			r.line = QStringLiteral( "mods: the saved choice for %1 -- %2 of %3 plugins from %4%5" )
				.arg( ws ).arg( kept.size() ).arg( base.size() ).arg( source )
				.arg( dropped.isEmpty() ? QString() : QStringLiteral( "; unticked: " ) + dropped.join( QStringLiteral( ", " ) ) );
		}
	}
	loadedStack().insert( ws.toLower(), r.plugins );
	// one stack: say when the near cells' mods are not the shown bake's
	if ( !lodb.isEmpty() && !bakePresent.isEmpty() ) {
		const QStringList nearN = namesOf( r.plugins.split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) );
		const QStringList bakeN = namesOf( bakePresent );
		if ( nearN != bakeN ) {
			QStringList off, extra;
			for ( const QString & p : bakePresent )
				if ( !nearN.contains( fileOf( p ).toLower() ) )
					off << fileOf( p );
			for ( const QString & p : r.plugins.split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) )
				if ( !bakeN.contains( fileOf( p ).toLower() ) )
					extra << fileOf( p );
			QStringList what;
			if ( !off.isEmpty() )
				what << QStringLiteral( "%1 off" ).arg( QStringList( off.mid( 0, 3 ) ).join( QStringLiteral( ", " ) )
					+ ( off.size() > 3 ? QStringLiteral( " and %1 more" ).arg( off.size() - 3 ) : QString() ) );
			if ( !extra.isEmpty() )
				what << QStringLiteral( "%1 not in the bake" ).arg( QStringList( extra.mid( 0, 3 ) ).join( QStringLiteral( ", " ) )
					+ ( extra.size() > 3 ? QStringLiteral( " and %1 more" ).arg( extra.size() - 3 ) : QString() ) );
			if ( what.isEmpty() )
				what << QStringLiteral( "another order" );
			r.viewNote = QStringLiteral( "Mods differ from the far LOD bake: %1" ).arg( what.join( QStringLiteral( "; " ) ) );
			r.line += QStringLiteral( "\n  mods: DIFFER from the far LOD bake -- %1 (the view says so)" ).arg( what.join( QStringLiteral( "; " ) ) );
		} else {
			r.line += QStringLiteral( "\n  mods: same as the far LOD bake (%1 plugins)" ).arg( bakeN.size() );
		}
	}
	return r;
}

QString cellModPickViewNote( const void * doc )
{
	return viewNotes().value( doc );
}

void cellModPickSetViewNote( const void * doc, const QString & note )
{
	if ( note.isEmpty() )
		viewNotes().remove( doc );
	else
		viewNotes().insert( doc, note );
}
