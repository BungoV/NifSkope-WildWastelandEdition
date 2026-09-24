/* WW_GLTF_EXPORT_DIALOG: the in-application gate of the glTF export options
   dialog. Lane GLTFEXPORT1, 2026-09-19.

   First run 2026-09-19 09:4x: 17 checks, 0 failed. Rewritten the same hour for
   bungo's ruling on the defaults (09:45) -- see (a).

   IT FORCES THE STATE IT MEASURES. The dialog is constructed here and driven
   through setOptions(); nothing is read out of QSettings, so this measures the
   code and not the machine it runs on (feedback_harness_isolate_settings).

   WHAT IT MEASURES:
     (a) the dialog opens on the RULED defaults -- skeleton auto, joints whole,
         units game, animation on, bones body, textures copy -- asserted both
         against GltfExportOptions's own initialisers and field by field, so a
         default that moves without a ruling cannot hide behind the dialog
         agreeing with it. Then the way back: `--legacy-defaults` parses into
         exactly gltfExportLegacyOptions(), a flag after it still wins, and a
         freshly opened dialog is NOT the pre-ruling export.
         FLOOR: flipping one row of the legacy struct makes it not-legacy.
     (b) the grid is FLAT: one label and one control per row, ten rows, and the
         label list is exactly the ten names below in this order.
     (c) round trip: setOptions(x) then options() == x, for five states -- the
         ruled defaults, legacy, whole-skeleton game-units, body-build-cycle,
         and one with two parts and a skeleton path.
     (d) dialog and CLI agree. For each of those five states the flags -- the
         command line the CLI would need, written ABSOLUTELY as
         `--legacy-defaults` plus every departure from it -- parse back through
         gltfExportParseFlag() into the SAME struct. This is the check that
         stops the two front ends drifting.
     (e) the PNG row exists only when gltfExportPngAvailable() says a converter
         does (it says false in this tree, and says why).
   ENVIRONMENT (tests/spells/gltf_export_options.sh sets it):
     WW_GLTF_EXPORT_DIALOG=1
   Log: release/ww_gltf_export_dialog.log, ending PASS / FAIL then done. */

#include "gltfexportdialog.h"
#include "nifskope.h"

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QTextStream>
#include <QTimer>

namespace {

struct WwGltfDlgState
{
	QTextStream * out = nullptr;
	int checks = 0, fails = 0;
	void check( bool ok, const QString & what )
	{
		checks++;
		if ( !ok )
			fails++;
		*out << ( ok ? "  ok   " : "  FAIL " ) << what << "\n";
	}
	void say( const QString & s ) { *out << s << "\n"; }
};

//! Field-by-field equality. Written out longhand rather than memcmp because the
//! struct holds QStrings; and written here rather than as operator== on the
//! struct so that adding a field to GltfExportOptions without teaching this
//! function about it leaves the new field UNCHECKED rather than silently
//! compared -- which is why the count below is asserted too.
bool sameOptions( const GltfExportOptions & a, const GltfExportOptions & b )
{
	return a.skeleton == b.skeleton
		&& a.skeletonPath == b.skeletonPath
		&& a.joints == b.joints
		&& a.units == b.units
		&& a.parts == b.parts
		&& a.bones == b.bones
		&& a.textures == b.textures
		&& a.rootMotion == b.rootMotion
		&& a.includeClip == b.includeClip
		&& a.bakeBodyBuild == b.bakeBodyBuild
		&& a.buildCycleClip == b.buildCycleClip;
}

/*! The command line that states `o`, as the CLI would take it.
 *
 *  It opens with `--legacy-defaults` and then names every field that differs
 *  from the PRE-RULING values, which makes the list an ABSOLUTE statement of
 *  `o` rather than a delta against whatever the defaults happen to be today.
 *  Before bungo's ruling of 2026-09-19 09:45 the two were the same thing and
 *  the empty list meant "today"; they are not the same thing any more, and a
 *  delta-against-defaults encoding would have quietly started passing by
 *  agreeing with itself. This also means the gate exercises the way back. */
QStringList flagsFor( const GltfExportOptions & o )
{
	QStringList f{ QStringLiteral( "--legacy-defaults" ) };
	if ( o.skeleton == GltfExportOptions::Skeleton::Auto )
		f << QStringLiteral( "--skeleton" ) << QStringLiteral( "auto" );
	else if ( o.skeleton == GltfExportOptions::Skeleton::Path )
		f << QStringLiteral( "--skeleton" ) << o.skeletonPath;
	if ( o.joints == GltfExportOptions::Joints::Whole )
		f << QStringLiteral( "--joints" ) << QStringLiteral( "whole" );
	if ( o.units == GltfExportOptions::Units::GameUnits )
		f << QStringLiteral( "--units" ) << QStringLiteral( "game" );
	for ( const QString & p : o.parts )
		f << QStringLiteral( "--part" ) << p;
	if ( o.bones == GltfExportOptions::Bones::Body )
		f << QStringLiteral( "--bones-only" ) << QStringLiteral( "body" );
	if ( o.textures == GltfExportOptions::Textures::Copy )
		f << QStringLiteral( "--textures" ) << QStringLiteral( "copy" );
	else if ( o.textures == GltfExportOptions::Textures::Png )
		f << QStringLiteral( "--textures" ) << QStringLiteral( "png" );
	if ( o.rootMotion == GltfExportOptions::RootMotion::Root )
		f << QStringLiteral( "--root-motion-mode" ) << QStringLiteral( "root" );
	else if ( o.rootMotion == GltfExportOptions::RootMotion::Object )
		f << QStringLiteral( "--root-motion-mode" ) << QStringLiteral( "object" );
	if ( !o.includeClip )
		f << QStringLiteral( "--no-clip" );
	if ( o.bakeBodyBuild )
		f << QStringLiteral( "--body-build" )
		  << QStringLiteral( "%1,%2,%3" ).arg( o.buildThin ).arg( o.buildMuscular ).arg( o.buildFat );
	if ( o.buildCycleClip )
		f << QStringLiteral( "--body-build-cycle" );
	return f;
}

//! Parse a flag list back with the CLI's own parser.
bool parseFlags( const QStringList & f, GltfExportOptions & o, QString & error )
{
	for ( int i = 0; i < f.size(); ) {
		bool usedNext = false;
		const int n = gltfExportParseFlag( f.at( i ), f.value( i + 1 ), o, usedNext, error );
		if ( n <= 0 ) {
			if ( error.isEmpty() )
				error = QStringLiteral( "'%1' is not a glTF export flag" ).arg( f.at( i ) );
			return false;
		}
		i += usedNext ? 2 : 1;
	}
	return true;
}

void runOne( WwGltfDlgState * st, const QString & name, const GltfExportOptions & want,
			 GltfExportDialog * dlg )
{
	dlg->setOptions( want );
	const GltfExportOptions back = dlg->options();
	st->check( sameOptions( want, back ), QStringLiteral( "(c) %1: the dialog reads back what it was set to" ).arg( name ) );

	GltfExportOptions viaCli;
	QString err;
	const QStringList f = flagsFor( want );
	const bool parsed = parseFlags( f, viaCli, err );
	st->check( parsed, QStringLiteral( "(d) %1: the CLI parses its own flags (%2)" )
			   .arg( name, parsed ? f.join( QLatin1Char( ' ' ) ) : err ) );
	if ( parsed )
		st->check( sameOptions( want, viaCli ),
				   QStringLiteral( "(d) %1: the flags state the same options as the dialog" ).arg( name ) );
}

} // namespace

void wwGltfExportDialogHarness( NifSkope * skope )
{
	if ( !qEnvironmentVariableIsSet( "WW_GLTF_EXPORT_DIALOG" ) )
		return;

	QTimer::singleShot( 800, skope ? static_cast<QObject *>( skope ) : qApp, [skope]() {
		QFile logf( QApplication::applicationDirPath() + QLatin1String( "/ww_gltf_export_dialog.log" ) );
		if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
			return;
		QTextStream log( &logf );
		WwGltfDlgState state;
		WwGltfDlgState * st = &state;
		st->out = &log;
		st->say( QStringLiteral( "WW_GLTF_EXPORT_DIALOG" ) );

		GltfExportDialog dlg( QString(), true, QStringLiteral( "jog" ), nullptr );

		/* (a) the dialog opens on the RULED defaults (bungo, 2026-09-19 09:45:
		 * skeleton auto, joints whole, units game, animation on, bones body,
		 * textures copy), and `--legacy-defaults` is the way back to the
		 * pre-ruling export. Both are asserted, because "the dialog agrees
		 * with the struct" is worth nothing if the struct moved unnoticed. */
		const GltfExportOptions fresh = dlg.options();
		const GltfExportOptions ruled;          // the defaults, as the struct states them
		st->check( sameOptions( fresh, ruled ),
				   QStringLiteral( "(a) a freshly opened dialog states the RULED defaults" ) );
		st->check( fresh.skeleton == GltfExportOptions::Skeleton::Auto
				   && fresh.joints == GltfExportOptions::Joints::Whole
				   && fresh.units == GltfExportOptions::Units::GameUnits
				   && fresh.includeClip
				   && fresh.bones == GltfExportOptions::Bones::Body
				   && fresh.textures == GltfExportOptions::Textures::Copy,
				   QStringLiteral( "(a) and they are the six bungo ruled, field by field" ) );
		st->check( !gltfExportOptionsAreLegacy( fresh ),
				   QStringLiteral( "(a) the ruled defaults are NOT the pre-ruling export" ) );

		// the way back, through the CLI parser the dialog shares
		GltfExportOptions back;
		QString backErr;
		const bool backOk = parseFlags( QStringList{ QStringLiteral( "--legacy-defaults" ) }, back, backErr );
		st->check( backOk && gltfExportOptionsAreLegacy( back ),
				   QStringLiteral( "(a) --legacy-defaults restores the pre-ruling export (%1)" )
				   .arg( backOk ? QStringLiteral( "parsed" ) : backErr ) );
		st->check( sameOptions( back, gltfExportLegacyOptions() ),
				   QStringLiteral( "(a) --legacy-defaults == gltfExportLegacyOptions(), field by field" ) );
		// a flag AFTER it still wins: "legacy except for X" has to be sayable
		GltfExportOptions backThenGame;
		st->check( parseFlags( QStringList{ QStringLiteral( "--legacy-defaults" ),
										    QStringLiteral( "--units" ), QStringLiteral( "game" ) },
							   backThenGame, backErr )
				   && backThenGame.units == GltfExportOptions::Units::GameUnits
				   && backThenGame.joints == GltfExportOptions::Joints::Weighted,
				   QStringLiteral( "(a) a flag after --legacy-defaults still wins" ) );
		GltfExportOptions flipped = gltfExportLegacyOptions();
		flipped.units = GltfExportOptions::Units::GameUnits;
		st->check( !gltfExportOptionsAreLegacy( flipped ),
				   QStringLiteral( "(a) FLOOR: one flipped row makes it not-legacy" ) );

		// (b) the grid is flat
		const QStringList labels = dlg.rowLabels();
		const QStringList expect{
			QStringLiteral( "Skeleton" ), QStringLiteral( "Joints" ), QStringLiteral( "Units" ),
			QStringLiteral( "Parts" ), QStringLiteral( "Bones" ), QStringLiteral( "Textures" ),
			QStringLiteral( "Root motion" ), QStringLiteral( "Animation" ),
			QStringLiteral( "Bake body build" ), QStringLiteral( "Body build cycle clip" ) };
		st->check( labels == expect, QStringLiteral( "(b) ten rows, in order: %1" ).arg( labels.join( QLatin1String( ", " ) ) ) );
		st->check( dlg.rowCount() == dlg.controlCount(),
				   QStringLiteral( "(b) one control per label (%1 / %2)" ).arg( dlg.rowCount() ).arg( dlg.controlCount() ) );

		// (c) + (d)
		runOne( st, QStringLiteral( "the ruled defaults" ), GltfExportOptions(), &dlg );
		runOne( st, QStringLiteral( "legacy" ), gltfExportLegacyOptions(), &dlg );

		GltfExportOptions whole;
		whole.joints = GltfExportOptions::Joints::Whole;
		whole.units = GltfExportOptions::Units::GameUnits;
		whole.skeleton = GltfExportOptions::Skeleton::Auto;
		runOne( st, QStringLiteral( "whole-skeleton game-units" ), whole, &dlg );

		GltfExportOptions build;
		build.bakeBodyBuild = true;
		build.buildCycleClip = true;
		build.buildThin = 1.0f;
		runOne( st, QStringLiteral( "body build cycle" ), build, &dlg );

		GltfExportOptions parts;
		parts.skeleton = GltfExportOptions::Skeleton::Path;
		parts.skeletonPath = QStringLiteral( "E:/skeleton.nif" );
		parts.parts << QStringLiteral( "E:/hands.nif" ) << QStringLiteral( "E:/head.nif" );
		parts.bones = GltfExportOptions::Bones::Body;
		parts.textures = GltfExportOptions::Textures::Copy;
		parts.rootMotion = GltfExportOptions::RootMotion::Object;
		runOne( st, QStringLiteral( "parts + body bones + copied textures" ), parts, &dlg );

		// (e) the PNG row
		const QList<QComboBox *> combos = dlg.findChildren<QComboBox *>();
		int pngEntries = 0;
		for ( QComboBox * c : combos )
			for ( int i = 0; i < c->count(); i++ )
				if ( c->itemText( i ).contains( QLatin1String( "PNG" ) ) )
					pngEntries++;
		st->check( pngEntries == ( gltfExportPngAvailable() ? 1 : 0 ),
				   QStringLiteral( "(e) the PNG row is offered only when a converter exists (available=%1, rows=%2)" )
				   .arg( gltfExportPngAvailable() ? QLatin1String( "yes" ) : QLatin1String( "no" ) ).arg( pngEntries ) );

		st->say( QStringLiteral( "%1 checks, %2 failed" ).arg( st->checks ).arg( st->fails ) );
		st->say( st->fails == 0 ? QStringLiteral( "PASS" ) : QStringLiteral( "FAIL" ) );
		st->say( QStringLiteral( "done" ) );
		log.flush();
		logf.close();
		qApp->quit();
	} );
}
