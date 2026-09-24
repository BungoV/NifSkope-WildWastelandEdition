/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

/* WW_LIGHTANGLES_TEST=1 -- lane LIGHTANGLES1's gate (2026-09-24), run INSIDE the
   running application. tests/spells/light_angles.sh is the driver.

   THE DEFECT IT REFUTES

     The viewer's world-fixed light has two angles, GLView::declination and
     GLView::planarAngle (degrees, wrapped to [-180, 180] by rotateLight). The
     Save Lighting action (aSaveLighting -> LightingWidget::saveSettings) wrote
     them as quarter-degree integers under "Settings/Render/Lighting/Declination"
     and ".../Planar Angle"; LightingWidget's constructor read them back from
     "Lighting/Declination" and "Lighting/Planar Angle", a key nothing writes.
     So the angles never came back after a restart. And the load folded the value
     with `tmp % 720`, which turns a legal +-180 degrees (+-720 stored) into 0:
     the light from the opposite side.

   ONE LAUNCH = one READ half and/or one SAVE half, so the spell chains
   launches: save pair 1 -> restart -> read pair 1, save pair 2 -> restart ...
   Every launch shares ONE scratch settings scope, which is the "restart".

     WW_LIGHTANGLES_EXPECT=d,p   read half: the angles this launch must have
                                 LOADED at startup, within one slider step
                                 (0.25 degree, the stored unit)
     WW_LIGHTANGLES_SAVE=d,p     save half: put these angles on the view, fire
                                 the real Save Lighting action, read the stored
                                 keys back
     WW_LIGHTANGLES_LOG=<path>   the log (default release/ww_lightangles_test.log)

   REFUSAL: the harness writes settings, so it runs ONLY inside a scratch
   WW_SETTINGS_SCOPE. Without one it saves nothing and FAILS by name; bungo's own
   settings key is never reached.

   FLOORS
     - the lighting panel and the Save Lighting action exist (a missing one
       would make the save half a no-op and the read half would read defaults)
     - the stored integer is read back from QSettings BEFORE the load check, so
       a red says which half broke: the key was never written, or the load did
       not read it
     - every expected pair has a non-zero angle, so a load that ignores the key
       (and so leaves 0, 0) cannot pass
     - ONE key family: after a save, the legacy "Lighting/..." keys are absent
     - the loaded angles are read off the GLView (what paintGL uses), never off
       the widget's intent */

#include "nifskope.h"
#include "glview.h"
#include "harnesswindow.h"
#include "model/nifmodel.h"
#include "ui/widgets/lightingwidget.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QSettings>
#include <QTextStream>
#include <QTimer>
#include <QUndoStack>

#include <cmath>

namespace
{

struct WwLaState
{
	int checks = 0;
	int fails = 0;
	QString logPath;
	bool done = false;
};

void wwLaSay( WwLaState & st, const QString & line )
{
	QFile f( st.logPath );
	if ( f.open( QIODevice::Append | QIODevice::Text ) ) {
		QTextStream out( &f );
		out << line << "\n";
		out.flush();
		f.close();
	}
}

void wwLaCheck( WwLaState & st, const QString & what, bool pass )
{
	st.checks++;
	if ( !pass )
		st.fails++;
	wwLaSay( st, ( pass ? QStringLiteral( "  ok   " ) : QStringLiteral( "  FAIL " ) ) + what );
}

//! "d,p" -> two angles; false when the variable is not exactly two numbers
bool wwLaPair( const char * var, float & d, float & p )
{
	const QStringList l = qEnvironmentVariable( var ).split( QLatin1Char( ',' ), Qt::SkipEmptyParts );
	if ( l.size() != 2 )
		return false;
	bool okD = false, okP = false;
	d = float( l[0].trimmed().toDouble( &okD ) );
	p = float( l[1].trimmed().toDouble( &okP ) );
	return okD && okP;
}

const char * const kDecl = "Settings/Render/Lighting/Declination";
const char * const kPlan = "Settings/Render/Lighting/Planar Angle";
const char * const kOldDecl = "Lighting/Declination";
const char * const kOldPlan = "Lighting/Planar Angle";

QString wwLaStored( QSettings & s, const char * key )
{
	return s.contains( QLatin1String( key ) ) ? s.value( QLatin1String( key ) ).toString()
											  : QStringLiteral( "<absent>" );
}

void wwLaFinish( NifSkope * skope, WwLaState * st )
{
	if ( st->done )
		return;
	st->done = true;
	wwLaSay( *st, QStringLiteral( "%1 checks, %2 failures" ).arg( st->checks ).arg( st->fails ) );
	wwLaSay( *st, st->fails == 0 ? QStringLiteral( "PASS" ) : QStringLiteral( "FAIL" ) );
	wwLaSay( *st, QStringLiteral( "done" ) );
	if ( skope ) {
		if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
			n->undoStack->setClean();
		skope->setWindowModified( false );
	}
	delete st;
	// the archlocktest.cpp exit: close gracefully, then end the loop ourselves
	QTimer::singleShot( 0, qApp, [skope]() {
		if ( skope )
			skope->close();
		QTimer::singleShot( 200, qApp, []() { QCoreApplication::exit( 0 ); } );
	} );
}

void wwLaRun( NifSkope * skope, WwLaState * st )
{
	wwLaSay( *st, QStringLiteral( "--- WW_LIGHTANGLES_TEST: light angles across a restart ---" ) );

	const QString suffix = wwHarnessSettingsSuffix();
	wwLaSay( *st, QStringLiteral( "  settings scope: '%1' (applicationName '%2')" )
		.arg( suffix.trimmed(), QCoreApplication::applicationName() ) );
	const bool scoped = !suffix.isEmpty();
	wwLaCheck( *st, QStringLiteral( "(floor) the run is inside a scratch WW_SETTINGS_SCOPE" ), scoped );
	if ( !scoped ) {
		wwLaSay( *st, QStringLiteral( "  REFUSED: no scratch scope, nothing read or written" ) );
		return;
	}

	GLView * ogl = skope->getGLView();
	auto * panel = skope->findChild<LightingWidget *>( QStringLiteral( "ShadingLightingPanel" ) );
	QAction * save = skope->findChild<QAction *>( QStringLiteral( "aSaveLighting" ) );
	wwLaCheck( *st, QStringLiteral( "(floor) the viewport exists" ), ogl != nullptr );
	wwLaCheck( *st, QStringLiteral( "(floor) the lighting panel exists" ), panel != nullptr );
	wwLaCheck( *st, QStringLiteral( "(floor) the Save Lighting action exists" ), save != nullptr );
	if ( !ogl )
		return;

	constexpr float kStep = 0.25f;	// 180 / LightingWidget::POS, the stored unit
	QSettings settings;

	/* THE READ HALF: what did THIS process load at startup? */
	float ed = 0.0f, ep = 0.0f;
	if ( wwLaPair( "WW_LIGHTANGLES_EXPECT", ed, ep ) ) {
		wwLaSay( *st, QStringLiteral( "  read half: stored %1 = %2, %3 = %4; legacy %5 = %6, %7 = %8" )
			.arg( QLatin1String( kDecl ), wwLaStored( settings, kDecl ) )
			.arg( QLatin1String( kPlan ), wwLaStored( settings, kPlan ) )
			.arg( QLatin1String( kOldDecl ), wwLaStored( settings, kOldDecl ) )
			.arg( QLatin1String( kOldPlan ), wwLaStored( settings, kOldPlan ) ) );
		wwLaCheck( *st, QStringLiteral( "(floor) the expected pair has a non-zero angle" ),
				   ed != 0.0f || ep != 0.0f );
		wwLaCheck( *st, QStringLiteral( "(floor) the previous launch's save is in the store" ),
				   settings.contains( QLatin1String( kDecl ) ) && settings.contains( QLatin1String( kPlan ) ) );
		const float dd = std::fabs( ogl->declination - ed );
		const float dp = std::fabs( ogl->planarAngle - ep );
		wwLaSay( *st, QStringLiteral( "  loaded declination %1 (expected %2, off by %3), planar %4 (expected %5, off by %6)" )
			.arg( double( ogl->declination ), 0, 'f', 3 ).arg( double( ed ), 0, 'f', 3 ).arg( double( dd ), 0, 'f', 3 )
			.arg( double( ogl->planarAngle ), 0, 'f', 3 ).arg( double( ep ), 0, 'f', 3 ).arg( double( dp ), 0, 'f', 3 ) );
		wwLaCheck( *st, QStringLiteral( "(a) the declination came back after the restart, within one step (%1 deg)" )
			.arg( double( kStep ) ), dd <= kStep );
		wwLaCheck( *st, QStringLiteral( "(a) the planar angle came back after the restart, within one step (%1 deg)" )
			.arg( double( kStep ) ), dp <= kStep );
	}

	/* THE SAVE HALF: the real action, the real keys */
	float sd = 0.0f, sp = 0.0f;
	if ( wwLaPair( "WW_LIGHTANGLES_SAVE", sd, sp ) && save ) {
		ogl->declination = sd;
		ogl->planarAngle = sp;
		save->trigger();
		settings.sync();
		const int wantD = int( std::lround( sd / kStep ) );
		const int wantP = int( std::lround( sp / kStep ) );
		wwLaSay( *st, QStringLiteral( "  save half: put %1, %2 on the view, fired Save Lighting; stored %3 = %4, %5 = %6"
									  " (want %7, %8)" )
			.arg( double( sd ), 0, 'f', 3 ).arg( double( sp ), 0, 'f', 3 )
			.arg( QLatin1String( kDecl ), wwLaStored( settings, kDecl ) )
			.arg( QLatin1String( kPlan ), wwLaStored( settings, kPlan ) )
			.arg( wantD ).arg( wantP ) );
		wwLaCheck( *st, QStringLiteral( "(save) the declination key holds the saved angle" ),
				   settings.contains( QLatin1String( kDecl ) ) && settings.value( QLatin1String( kDecl ) ).toInt() == wantD );
		wwLaCheck( *st, QStringLiteral( "(save) the planar angle key holds the saved angle" ),
				   settings.contains( QLatin1String( kPlan ) ) && settings.value( QLatin1String( kPlan ) ).toInt() == wantP );
		wwLaCheck( *st, QStringLiteral( "(one family) no legacy Lighting/ key exists after the save" ),
				   !settings.contains( QLatin1String( kOldDecl ) ) && !settings.contains( QLatin1String( kOldPlan ) ) );
	}
}

} // namespace

void wwLightAnglesHarness( NifSkope * skope )
{
	if ( !skope || !qEnvironmentVariableIsSet( "WW_LIGHTANGLES_TEST" ) )
		return;
	auto * st = new WwLaState;
	st->logPath = qEnvironmentVariable( "WW_LIGHTANGLES_LOG" );
	if ( st->logPath.isEmpty() )
		st->logPath = QCoreApplication::applicationDirPath() + QStringLiteral( "/ww_lightangles_test.log" );
	QFile::remove( st->logPath );
	// no file is opened: 1.5 s after the window exists is after the widget's
	// constructor (the load) and after the first paint
	QTimer::singleShot( 1500, skope, [skope, st]() {
		wwLaRun( skope, st );
		wwLaFinish( skope, st );
	} );
}
