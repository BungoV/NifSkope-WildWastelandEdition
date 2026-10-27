// lane OPENCELLFIX2 unit test: a Display group's master (gl/celllooks.h WwGroup) gates its rows' readers.
// Prints one line per check, then PASS or FAIL; exit code 0 only on PASS.
// Reds, each must FAIL: WW_CELLGROUP_RED=<a group name> (that master gates nothing), WW_CELLGROUP_RED=ssr (the one
// Screen-Space Reflections row sets indoors only).

#include "celllooks.h"

#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>

#include <cstdio>

namespace {
int checks = 0, fails = 0;
void check( bool ok, const QByteArray & what )
{
	checks++;
	fails += !ok;
	std::printf( "  %s %s\n", ok ? "ok  " : "FAIL", what.constData() );
}
bool ssrIn = false, ssrOut = false;
}	// namespace

// gl/cellssr.cpp's setters, stood in
void wwCellSsrSetOn( bool on ) { ssrIn = on; }
void wwCellSsrExteriorSetOn( bool on ) { ssrOut = on; }

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	QStandardPaths::setTestModeEnabled( true );
	QCoreApplication app( argc, argv );
	QCoreApplication::setOrganizationName( QStringLiteral( "cellgroupstest" ) );
	QCoreApplication::setApplicationName( QStringLiteral( "cellgroupstest" ) );
	// the test's own settings scope, an ini file beside the exe (never the app's key)
	QSettings::setDefaultFormat( QSettings::IniFormat );
	QSettings::setPath( QSettings::IniFormat, QSettings::UserScope, QCoreApplication::applicationDirPath() + QStringLiteral( "/settings" ) );
	QSettings().clear();

	std::printf( "every master ships on; off, its rows read off and keep their own saved state\n" );
	for ( int k = 0; k < int( WwLook::Count ); k++ )	// every row on, so a master is the only thing that can turn one off
		wwLookSet( WwLook( k ), true );
	for ( int gi = 0; gi < int( WwGroup::Count ); gi++ ) {
		const WwGroup g = WwGroup( gi );
		const QByteArray name = wwGroupName( g );
		check( wwGroupOn( g ) && wwGroupLets( g ), name + ": the master ships on" );
		int rows = 0, off = 0;
		wwGroupSet( g, false );
		for ( int k = 0; k < int( WwLook::Count ); k++ ) {
			const WwLook l = WwLook( k );
			if ( wwLookGroup( l ) != g || !wwLookHasRow( l ) || wwLookPinned( l ) )
				continue;
			rows++;
			off += !wwLookOn( l );
		}
		if ( rows )
			check( off == rows, name + ": off, its " + QByteArray::number( rows ) + " rows read off (" + QByteArray::number( off ) + ")" );
		check( !wwGroupOn( g ), name + ": the master reads off" );
		wwGroupSet( g, true );
		int back = 0;
		for ( int k = 0; k < int( WwLook::Count ); k++ ) {
			const WwLook l = WwLook( k );
			if ( wwLookGroup( l ) == g && wwLookHasRow( l ) && !wwLookPinned( l ) )
				back += wwLookOn( l );
		}
		check( back == rows, name + ": on again, its rows come back as they were" );
	}
	wwGroupSet( WwGroup::Water, false );
	check( QSettings().value( QStringLiteral( "WW/CellWaterCaustics" ) ).toBool(), "a master off leaves its rows' own keys alone" );
	check( !QSettings().value( QStringLiteral( "WW/CellGroupWater" ), true ).toBool(), "the master's key is saved off" );
	check( wwLookOn( WwLook::BloomMip ), "a row with no master is not gated by one" );
	wwGroupSet( WwGroup::Water, true );

	std::printf( "the light curve and lamp shadow budget read neutral with their master off\n" );
	wwCellLightCurveSet( 1.5f );
	wwCellShadowBudgetSet( 8 );
	check( wwCellLightCurve() == 1.5f && wwCellShadowBudget() == 8, "on: the rows' values" );
	wwGroupSet( WwGroup::CellLights, false );
	wwGroupSet( WwGroup::Shadows, false );
	check( wwCellLightCurve() == 1.0f, "Cell lights off: the curve reads 1 (off)" );
	check( wwCellShadowBudget() == 0, "Shadows off: the budget reads 0 (off)" );
	wwGroupSet( WwGroup::CellLights, true );
	wwGroupSet( WwGroup::Shadows, true );
	check( wwCellLightCurve() == 1.5f && wwCellShadowBudget() == 8, "on again: the rows' values are back" );

	std::printf( "note 20: one Screen-Space Reflections row sets indoors and outdoors\n" );
	wwCellSsrRowSet( true );
	check( ssrIn && ssrOut, "ticked: both halves on" );
	wwCellSsrRowSet( false );
	check( !ssrIn && !ssrOut, "unticked: both halves off" );

	std::printf( "%d checks, %d failed\n%s\n", checks, fails, fails ? "FAIL" : "PASS" );
	return fails ? 1 : 0;
}
