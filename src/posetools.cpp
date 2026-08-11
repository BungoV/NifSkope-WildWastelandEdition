/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "nifskope.h"
#include "glview.h"
#include "nifmerge.h"
#include "model/nifmodel.h"
#include "spells/animationsetup.h"
#include "ui/settingsdialog.h"
#include "wwskin.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QMenu>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QDir>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTextStream>

#include <cmath>

/*! \file posetools.cpp
 * \brief Pose Manager workspace dock.
 *
 * Blender's Pose Mode + Pose Library, in NifSkope's flat visual language. The
 * posing ENGINE already exists — Shape::updateBoneTransforms reads the live
 * bone node transform, so selecting a bone and pressing G/R/S deforms the mesh
 * — so this dock is two things over the shared AnimSetup pose API:
 *
 *   - a **bone list** that drives block-list / viewport selection, so a bone is
 *     one click away from G/R/S (this is the practical form of "bone picking";
 *     clickable octahedral bones in the 3D view are Skeleton Manager work),
 *   - a **pose library**: save the current pose, apply one with a blend slider,
 *     delete. Poses are ordinary one-key NiControllerSequences, so they also
 *     show up in the Timeline.
 */

namespace
{

//! Section-heading label, matching the other manager docks.
QLabel * heading( const QString & text, QWidget * parent )
{
	// the shared one, so all eight docks cannot drift apart again
	return wwHeading( text, parent );
}

//! Settings key for the NifSkope library root. Shared with the General →
//! NifSkope Library settings pane (its "libraryFolder" line edit humanizes to
//! this exact key), so the dock's Folder... button and the settings dialog stay
//! in sync.
static const char * const kLibraryKey = "Settings/Library/Library Folder";

//! The NifSkope library root — a single folder where NifSkope keeps user files
//! (the pose library today, other things later, each in its own subfolder).
//! Defaults to a "NifSkope" folder under the user's Documents, overridable via
//! QSettings, and created on use.
QString nifskopeLibraryFolder()
{
	QSettings settings;
	QString dir = settings.value( QLatin1String( kLibraryKey ) ).toString();
	if ( dir.isEmpty() ) {
		const QString docs = QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation );
		dir = QDir( docs.isEmpty() ? QDir::homePath() : docs ).filePath( QStringLiteral( "NifSkope" ) );
	}
	QDir().mkpath( dir );
	return dir;
}

void setNifskopeLibraryFolder( const QString & dir )
{
	QSettings settings;
	settings.setValue( QLatin1String( kLibraryKey ), dir );
}

//! Where pose files live: a "Poses" subfolder of the library root, so other
//! features can claim their own subfolders without colliding.
QString poseLibraryFolder()
{
	const QString dir = QDir( nifskopeLibraryFolder() ).filePath( QStringLiteral( "Poses" ) );
	QDir().mkpath( dir );
	return dir;
}

/* TEST HARNESS (WW_SAMEXPORT_TEST=1): the Screen Archer Menu pose EXPORT.
 *
 * WHY IT LIVES HERE and not with the other harnesses in nifskope_ui.cpp: the
 * thing under test is this dock's export, and the dock is created before the
 * document finishes loading, so its own completeLoading connection is the exact
 * hook the harness needs. It writes release/ww_samexport_test.log and quits.
 *
 * WHAT IS MEASURED. The import harness (WW_SAMPOSE_TEST) proves the CONVENTION
 * against numbers worked out by hand; a round trip cannot do that, because a
 * convention agrees with itself however wrong it is. So the round trip is only
 * one of five things here, and none of the others compares the exporter with the
 * importer:
 *
 *   1. THE BONE SET. Every key resolves to a named NiAVObject, none is a `_skin`
 *      proxy, the file root and its own children (`Root`, `CharacterBumper`) are
 *      absent — and every bone of the committed 89-bone corpus pose is PRESENT.
 *      Coverage is the requirement; extra keys are not a failure (SAM's own node
 *      map for this skeleton lists 140).
 *   2. REST VALUES VERBATIM, on a skeleton nothing has posed: each exported
 *      x/y/z/scale parses back to the node's own value, and the angles rebuilt
 *      through the harness's OWN Rx(yaw)*Ry(pitch)*Rz(roll) — doubles, written
 *      out by hand, sharing no code with Matrix::toEuler — reproduce the node's
 *      rotation matrix. A self-agreeing exporter fails this.
 *   3. THE SHAPE OF THE FILE: version is the NUMBER 2, name/skeleton are
 *      strings, and every one of the 7 channels is a STRING, as SAM writes them.
 *   4. THE ROUND TRIP: import the corpus pose, export it, import that, and every
 *      bone's local transform must come back within 1e-5. Plus the exported
 *      values against the FIXTURE'S OWN, which is the check that the export is
 *      the import's inverse rather than merely repeatable.
 *   5. THE GIMBAL BRANCH, which is the half of toEuler an ordinary pose never
 *      reaches: the fixture's Back_Armor is at pitch 90 exactly (its matrix
 *      element m02 is 1), so asin() saturates and yaw/roll come out of the
 *      degenerate branch. It has to round-trip too, and it is asserted by name.
 */
void wwSamExportHarness( NifSkope * skope, GLView * ogl, QWidget * panel )
{
	QObject::connect( skope, &NifSkope::completeLoading, panel,
		[skope, ogl, panel]( bool ok, QString & ) {
		QTimer::singleShot( 800, panel, [skope, ogl, panel, ok]() {
			QFile logf( QApplication::applicationDirPath() + "/ww_samexport_test.log" );
			if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
				return;
			QTextStream log( &logf );
			QStringList fails;
			NifModel * nif = skope ? skope->getNifModel() : nullptr;

			const QString dir = QApplication::applicationDirPath();
			const QString restPath = dir + QStringLiteral( "/ww_samexport_rest.json" );
			const QString posedPath = dir + QStringLiteral( "/ww_samexport_posed.json" );
			const QString againPath = dir + QStringLiteral( "/ww_samexport_again.json" );
			const QString fixture = QString::fromLocal8Bit( qgetenv( "WW_SAMEXPORT_POSE" ) );

			auto readJson = []( const QString & p, QJsonObject & root ) -> bool {
				QFile f( p );
				if ( !f.open( QIODevice::ReadOnly ) )
					return false;
				const QJsonDocument d = QJsonDocument::fromJson( f.readAll() );
				f.close();
				root = d.object();
				return d.isObject();
			};
			// a channel as the number it spells, and whether it was a STRING
			auto chan = []( const QJsonObject & o, const char * key, bool * isStr = nullptr ) {
				const QJsonValue v = o.value( QLatin1String( key ) );
				if ( isStr ) *isStr = v.isString();
				return v.isString() ? v.toString().trimmed().toDouble() : v.toDouble();
			};
			/* The harness's OWN rotation, from degrees, in doubles, multiplied out
			 * by hand — Rx(yaw)*Ry(pitch)*Rz(roll) in NIF row order. Nothing here
			 * calls Matrix::fromEuler or Matrix::toEuler, so agreement with the
			 * exported angles is evidence rather than a tautology. */
			auto handRot = []( double yawDeg, double pitchDeg, double rollDeg ) {
				const double rad = 3.14159265358979 / 180.0;
				const double cx = std::cos( yawDeg * rad ), sx = std::sin( yawDeg * rad );
				const double cy = std::cos( pitchDeg * rad ), sy = std::sin( pitchDeg * rad );
				const double cz = std::cos( rollDeg * rad ), sz = std::sin( rollDeg * rad );
				Matrix m;
				m( 0, 0 ) = float( cy * cz );
				m( 0, 1 ) = float( -cy * sz );
				m( 0, 2 ) = float( sy );
				m( 1, 0 ) = float( cx * sz + sx * sy * cz );
				m( 1, 1 ) = float( cx * cz - sx * sy * sz );
				m( 1, 2 ) = float( -sx * cy );
				m( 2, 0 ) = float( sx * sz - cx * sy * cz );
				m( 2, 1 ) = float( sx * cz + cx * sy * sz );
				m( 2, 2 ) = float( cx * cy );
				return m;
			};
			// degrees are periodic: -180 and 180 are one angle, and the gimbal
			// branch legitimately returns whichever end it lands on
			auto angleDiff = []( double a, double b ) {
				double d = std::fabs( a - b );
				while ( d > 180.0 )
					d = std::fabs( d - 360.0 );
				return d;
			};
			auto matMax = []( const Matrix & a, const Matrix & b ) {
				float d = 0;
				for ( int r = 0; r < 3; r++ )
					for ( int c = 0; c < 3; c++ )
						d = qMax( d, float( std::fabs( a( r, c ) - b( r, c ) ) ) );
				return d;
			};

			do {
				if ( !ok || !nif || nif->getBlockCount() == 0 ) {
					fails << QStringLiteral( "load failed" );
					break;
				}

				// ---- the UI half: the button exists, beside the import -------
				auto * exportBtn = panel->findChild<QPushButton *>(
					QStringLiteral( "PoseExportSamButton" ) );
				auto * importBtn = panel->findChild<QPushButton *>(
					QStringLiteral( "PoseImportSamButton" ) );
				if ( !exportBtn || !importBtn )
					fails << QStringLiteral( "the Pose Manager has no SAM export button beside its import" );
				else {
					log << "buttons: '" << importBtn->text() << "' / '" << exportBtn->text() << "'\n";
					if ( !exportBtn->isEnabled() || !exportBtn->text().contains(
							QStringLiteral( "SAM" ) ) )
						fails << QStringLiteral( "the SAM export button is disabled or misnamed" );
				}

				// ---- 1/2/3: the REST export, before anything is posed --------
				int written = 0;
				QString err;
				if ( !ogl->poseExportSam( restPath, QStringLiteral( "rest" ), &written, &err ) ) {
					fails << ( QStringLiteral( "the rest export failed: " ) + err );
					break;
				}
				QJsonObject rest;
				if ( !readJson( restPath, rest ) ) {
					fails << QStringLiteral( "the rest export is not readable JSON" );
					break;
				}
				const QJsonObject restT = rest.value( QStringLiteral( "transforms" ) ).toObject();
				log << "rest export: " << written << " bone(s) written, "
					<< restT.size() << " key(s) in the file\n";
				log << "skeleton field: '" << rest.value( QStringLiteral( "skeleton" ) ).toString()
					<< "', name field: '" << rest.value( QStringLiteral( "name" ) ).toString() << "'\n";

				if ( !rest.value( QStringLiteral( "version" ) ).isDouble()
					 || rest.value( QStringLiteral( "version" ) ).toInt() != 2 )
					fails << QStringLiteral( "version is not the NUMBER 2" );
				if ( !rest.value( QStringLiteral( "name" ) ).isString()
					 || !rest.value( QStringLiteral( "skeleton" ) ).isString() )
					fails << QStringLiteral( "name / skeleton are not strings" );
				if ( restT.isEmpty() ) {
					fails << QStringLiteral( "the rest export has no transforms" );
					break;
				}

				// every channel of every bone must be a STRING, as SAM writes them
				int nonString = 0;
				for ( auto it = restT.constBegin(); it != restT.constEnd(); ++it ) {
					const QJsonObject o = it.value().toObject();
					for ( const char * k : { "yaw", "pitch", "roll", "x", "y", "z", "scale" } )
						if ( !o.value( QLatin1String( k ) ).isString() )
							nonString++;
				}
				log << "channels written as bare numbers instead of strings: " << nonString << "\n";
				if ( nonString != 0 )
					fails << QStringLiteral( "%1 channel(s) are JSON numbers, not strings" ).arg( nonString );

				// the bone-set rule, asserted rather than described
				QHash<QString, int> byName;
				for ( int b = 0; b < nif->getBlockCount(); b++ ) {
					const QModelIndex i = nif->getBlockIndex( b );
					if ( !nif->blockInherits( i, "NiAVObject" ) )
						continue;
					const QString nm = nif->resolveString( i, "Name" );
					if ( !nm.isEmpty() && !byName.contains( nm ) )
						byName.insert( nm, b );
				}
				QStringList rootNames;
				for ( int r : nif->getRootLinks() ) {
					rootNames << nif->resolveString( nif->getBlockIndex( r ), "Name" );
					for ( int c : nif->getChildLinks( r ) )
						if ( nif->blockInherits( nif->getBlockIndex( c ), "NiAVObject" ) )
							rootNames << nif->resolveString( nif->getBlockIndex( c ), "Name" );
				}
				int unresolved = 0, skinKeys = 0, rootKeys = 0;
				for ( auto it = restT.constBegin(); it != restT.constEnd(); ++it ) {
					if ( !byName.contains( it.key() ) )
						unresolved++;
					if ( it.key().endsWith( QLatin1String( "_skin" ), Qt::CaseInsensitive ) )
						skinKeys++;
					if ( rootNames.contains( it.key() ) )
						rootKeys++;
				}
				log << "keys that are not named NiAVObjects: " << unresolved
					<< ", _skin proxies: " << skinKeys
					<< ", the root or a child of it: " << rootKeys
					<< " (root chain: " << rootNames.join( QStringLiteral( ", " ) ) << ")\n";
				if ( unresolved || skinKeys || rootKeys )
					fails << QStringLiteral( "the exported key set breaks its own rule "
						"(%1 unresolved, %2 _skin, %3 root-level)" )
						.arg( unresolved ).arg( skinKeys ).arg( rootKeys );

				/* REST VALUES VERBATIM. Every channel back against the node it came
				 * from, and the rotation against the harness's own composition. */
				double worstT = 0, worstS = 0;
				float worstR = 0;
				QString worstRName;
				for ( auto it = restT.constBegin(); it != restT.constEnd(); ++it ) {
					auto b = byName.constFind( it.key() );
					if ( b == byName.constEnd() )
						continue;
					const Transform t( nif, nif->getBlockIndex( *b ) );
					const QJsonObject o = it.value().toObject();
					worstT = qMax( worstT, std::fabs( chan( o, "x" ) - double( t.translation[0] ) ) );
					worstT = qMax( worstT, std::fabs( chan( o, "y" ) - double( t.translation[1] ) ) );
					worstT = qMax( worstT, std::fabs( chan( o, "z" ) - double( t.translation[2] ) ) );
					worstS = qMax( worstS, std::fabs( chan( o, "scale" ) - double( t.scale ) ) );
					const float d = matMax( handRot( chan( o, "yaw" ), chan( o, "pitch" ),
						chan( o, "roll" ) ), t.rotation );
					if ( d > worstR ) { worstR = d; worstRName = it.key(); }
				}
				log << "rest verbatim: worst translation " << worstT << ", worst scale " << worstS
					<< ", worst rotation vs the harness's own Rx*Ry*Rz " << worstR
					<< " (" << worstRName << ")\n";
				if ( worstT > 1e-5 || worstS > 1e-5 )
					fails << QStringLiteral( "the rest export does not reproduce the node values" );
				if ( worstR > 1e-5f )
					fails << QStringLiteral( "the exported angles do not rebuild the node rotation "
						"(worst %1 on %2)" ).arg( worstR ).arg( worstRName );

				if ( fixture.isEmpty() || !QFile::exists( fixture ) ) {
					log << "no corpus pose in WW_SAMEXPORT_POSE — coverage and round trip skipped\n";
					break;
				}

				// ---- COVERAGE: every bone the corpus pose names --------------
				QJsonObject corpus;
				if ( !readJson( fixture, corpus ) ) {
					fails << QStringLiteral( "the corpus pose is not readable JSON" );
					break;
				}
				const QJsonObject corpusT = corpus.value( QStringLiteral( "transforms" ) ).toObject();
				QStringList missingKeys;
				for ( auto it = corpusT.constBegin(); it != corpusT.constEnd(); ++it )
					if ( !restT.contains( it.key() ) )
						missingKeys << it.key();
				QStringList extraKeys;
				for ( auto it = restT.constBegin(); it != restT.constEnd(); ++it )
					if ( !corpusT.contains( it.key() ) )
						extraKeys << it.key();
				extraKeys.sort();
				log << "corpus pose: " << corpusT.size() << " bone(s); export covers all but "
					<< missingKeys.size() << "\n";
				log << "keys the export adds beyond the corpus (" << extraKeys.size() << "): "
					<< extraKeys.join( QStringLiteral( ", " ) ) << "\n";
				if ( !missingKeys.isEmpty() )
					fails << QStringLiteral( "the export omits %1 corpus bone(s): %2" )
						.arg( missingKeys.size() )
						.arg( missingKeys.mid( 0, 10 ).join( QStringLiteral( ", " ) ) );

				// ---- 4: THE ROUND TRIP ---------------------------------------
				QString importErr;
				int missing = 0;
				const int applied = ogl->poseImportSam( fixture, 1.0f, &importErr, &missing );
				QApplication::processEvents();
				log << "corpus pose imported: " << applied << " bone(s), " << missing
					<< " not in this skeleton\n";
				if ( applied <= 0 ) {
					fails << ( QStringLiteral( "the corpus pose would not import: " ) + importErr );
					break;
				}

				// what the bones hold now, by name, before anything is written out
				QHash<QString, Transform> posed;
				for ( int b : AnimSetup::samPoseBones( nif ) )
					posed.insert( nif->resolveString( nif->getBlockIndex( b ), "Name" ),
						Transform( nif, nif->getBlockIndex( b ) ) );

				if ( !ogl->poseExportSam( posedPath, QStringLiteral( "posed" ), &written, &err ) ) {
					fails << ( QStringLiteral( "the posed export failed: " ) + err );
					break;
				}
				QJsonObject posedJson;
				if ( !readJson( posedPath, posedJson ) ) {
					fails << QStringLiteral( "the posed export is not readable JSON" );
					break;
				}
				const QJsonObject posedT = posedJson.value( QStringLiteral( "transforms" ) ).toObject();

				/* THE EXPORTED VALUES AGAINST THE FIXTURE'S OWN. Not against what
				 * this build produced: the numbers on the right are the ones
				 * Screen Archer Menu wrote. */
				double worstAngle = 0, worstPos = 0;
				QString worstAngleName;
				int compared = 0;
				for ( auto it = corpusT.constBegin(); it != corpusT.constEnd(); ++it ) {
					if ( !posedT.contains( it.key() ) || !byName.contains( it.key() ) )
						continue;
					const QJsonObject want = it.value().toObject();
					const QJsonObject got = posedT.value( it.key() ).toObject();
					compared++;
					for ( const char * k : { "yaw", "pitch", "roll" } ) {
						const double d = angleDiff( chan( got, k ), chan( want, k ) );
						if ( d > worstAngle ) { worstAngle = d; worstAngleName = it.key(); }
					}
					for ( const char * k : { "x", "y", "z", "scale" } )
						worstPos = qMax( worstPos, std::fabs( chan( got, k ) - chan( want, k ) ) );
				}
				log << "export vs the fixture over " << compared << " bone(s): worst angle "
					<< worstAngle << " deg (" << worstAngleName << "), worst position/scale "
					<< worstPos << "\n";
				if ( compared < corpusT.size() )
					fails << QStringLiteral( "only %1 of %2 corpus bones could be compared" )
						.arg( compared ).arg( corpusT.size() );
				if ( worstAngle > 1e-3 )
					fails << QStringLiteral( "the exported angles differ from the fixture's by %1 deg "
						"on %2" ).arg( worstAngle ).arg( worstAngleName );
				if ( worstPos > 1e-4 )
					fails << QStringLiteral( "the exported translations differ from the fixture's by %1" )
						.arg( worstPos );

				/* THE GIMBAL BRANCH. Back_Armor is at pitch 90 exactly, so the
				 * node's m02 saturates at 1 and toEuler leaves its main branch —
				 * the half of the inverse an ordinary bone never exercises. */
				const QString gimbal = QStringLiteral( "Back_Armor" );
				if ( posedT.contains( gimbal ) && byName.contains( gimbal ) ) {
					const Transform t( nif, nif->getBlockIndex( byName.value( gimbal ) ) );
					const QJsonObject got = posedT.value( gimbal ).toObject();
					const QJsonObject want = corpusT.value( gimbal ).toObject();
					log << gimbal << ": m02 " << t.rotation( 0, 2 ) << ", exported yaw/pitch/roll "
						<< chan( got, "yaw" ) << " / " << chan( got, "pitch" ) << " / "
						<< chan( got, "roll" ) << " (fixture " << chan( want, "yaw" ) << " / "
						<< chan( want, "pitch" ) << " / " << chan( want, "roll" ) << ")\n";
					if ( std::fabs( double( t.rotation( 0, 2 ) ) ) < 0.999999 )
						fails << QStringLiteral( "%1 is not on the gimbal branch after all — "
							"that check measures nothing" ).arg( gimbal );
					/* Its yaw comes back as -180 where the fixture wrote 180 — the
					 * same angle, and the branch's own atan2 is free to return
					 * either end of it. What has to hold is the ROTATION, so the
					 * angles are compared periodically and the matrix the harness
					 * rebuilds from them is compared to the node's outright. */
					if ( angleDiff( chan( got, "pitch" ), chan( want, "pitch" ) ) > 1e-3
						 || angleDiff( chan( got, "yaw" ), chan( want, "yaw" ) ) > 1e-3
						 || angleDiff( chan( got, "roll" ), chan( want, "roll" ) ) > 1e-3 )
						fails << QStringLiteral( "the gimbal-locked bone does not round-trip" );
					const float gd = matMax( handRot( chan( got, "yaw" ), chan( got, "pitch" ),
						chan( got, "roll" ) ), t.rotation );
					log << gimbal << ": the harness rebuilds the node rotation from the exported "
						<< "angles to " << gd << "\n";
					if ( gd > 1e-5f )
						fails << QStringLiteral( "the gimbal-locked bone's exported angles do not "
							"rebuild its rotation (%1)" ).arg( gd );
				} else {
					fails << QStringLiteral( "the fixture no longer carries %1 — the gimbal branch "
						"is untested" ).arg( gimbal );
				}

				// ...and back in: every local transform must survive the trip
				const int again = ogl->poseImportSam( posedPath, 1.0f, &importErr, &missing );
				QApplication::processEvents();
				log << "re-imported the export: " << again << " bone(s)\n";
				if ( again < applied )
					fails << QStringLiteral( "the re-import posed %1 bone(s) where the original "
						"posed %2" ).arg( again ).arg( applied );
				double worstRtT = 0, worstRtS = 0;
				float worstRtR = 0;
				QString worstRtName;
				int checked = 0;
				for ( auto it = posed.constBegin(); it != posed.constEnd(); ++it ) {
					if ( !byName.contains( it.key() ) )
						continue;
					const Transform now( nif, nif->getBlockIndex( byName.value( it.key() ) ) );
					checked++;
					worstRtT = qMax( worstRtT, double( ( now.translation - it.value().translation ).length() ) );
					worstRtS = qMax( worstRtS, std::fabs( double( now.scale - it.value().scale ) ) );
					const float d = matMax( now.rotation, it.value().rotation );
					if ( d > worstRtR ) { worstRtR = d; worstRtName = it.key(); }
				}
				log << "round trip over " << checked << " bone(s): worst translation " << worstRtT
					<< ", worst scale " << worstRtS << ", worst rotation " << worstRtR
					<< " (" << worstRtName << ")\n";
				if ( checked < applied )
					fails << QStringLiteral( "only %1 of %2 posed bones were re-checked" )
						.arg( checked ).arg( applied );
				if ( worstRtT > 1e-5 || worstRtS > 1e-5 || worstRtR > 1e-5f )
					fails << QStringLiteral( "the round trip moved a bone (t %1, s %2, r %3 on %4)" )
						.arg( worstRtT ).arg( worstRtS ).arg( worstRtR ).arg( worstRtName );

				// a third export, to say whether the file has settled exactly
				int again2 = 0;
				if ( ogl->poseExportSam( againPath, QStringLiteral( "posed" ), &again2, &err ) ) {
					QFile a( posedPath ), b( againPath );
					if ( a.open( QIODevice::ReadOnly ) && b.open( QIODevice::ReadOnly ) ) {
						const bool same = ( a.readAll() == b.readAll() );
						log << "export -> import -> export is byte-identical: "
							<< ( same ? "yes" : "no" ) << "\n";
					}
				}
			} while ( false );

			for ( const QString & f : fails )
				log << "  FAIL: " << f << "\n";
			log << ( fails.isEmpty() ? "PASS\n" : "FAILED\n" );
			log << "done\n";
			logf.close();

			// the document is modified; click through the save prompt on the way out
			QTimer * qd = new QTimer( qApp );
			QObject::connect( qd, &QTimer::timeout, qApp, []() {
				auto * mb = qobject_cast<QMessageBox *>( QApplication::activeModalWidget() );
				if ( !mb )
					return;
				QAbstractButton * bn = mb->button( QMessageBox::Discard );
				if ( !bn ) bn = mb->button( QMessageBox::No );
				if ( !bn && !mb->buttons().isEmpty() ) bn = mb->buttons().first();
				if ( bn ) bn->click();
			} );
			qd->start( 100 );
			QTimer::singleShot( 0, qApp, &QApplication::quit );
		} );
	} );
}

} // namespace

QDockWidget * tlCreatePoseManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl )
{
	auto * skope = qobject_cast<NifSkope *>( mw );

	auto * dock = new QDockWidget( QObject::tr( "Pose Manager" ), mw );
	dock->setObjectName( QStringLiteral( "PoseManagerDock" ) );
	dock->setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea );

	auto * panel = new QWidget( dock );
	auto * layout = new QVBoxLayout( panel );
	layout->setContentsMargins( 6, 6, 6, 6 );
	layout->setSpacing( 5 );

	// search bar at the very top (Blender's bone search)
	auto * boneSearch = new QLineEdit( panel );
	boneSearch->setObjectName( QStringLiteral( "PoseBoneSearch" ) );
	boneSearch->setClearButtonEnabled( true );
	boneSearch->setPlaceholderText( QObject::tr( "Search bones..." ) );
	layout->addWidget( boneSearch );

	auto * status = new QLabel( QObject::tr( "Open a skinned mesh to pose its skeleton." ), panel );
	status->setWordWrap( true );
	layout->addWidget( status );

	// bring in a skeleton (or another armour piece) from a file OR a game archive
	// (the archive path reuses the existing NIF Browser)
	/* A QToolButton with InstantPopup, not a QPushButton with setMenu.
	 *
	 * A QPushButton cannot take the shared boxed-button sheet at all -- it
	 * styles QToolButton. And InstantPopup rather than MenuButtonPopup for a
	 * specific reason: this button has NO clicked() connection, only actions on
	 * its menu, so splitting it would leave the main segment dead.
	 */
	auto * loadSkelBtn = new QToolButton( panel );
	loadSkelBtn->setText( QObject::tr( "Load skeleton..." ) );
	loadSkelBtn->setPopupMode( QToolButton::InstantPopup );
	loadSkelBtn->setToolButtonStyle( Qt::ToolButtonTextOnly );
	loadSkelBtn->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "3px 8px" ) ) );
	loadSkelBtn->setObjectName( QStringLiteral( "PoseLoadSkeletonButton" ) );
	loadSkelBtn->setToolTip( QObject::tr(
		"Merge a skeleton (or armour piece) from another NIF — from a file or a "
		"game archive. Bones with matching names are shared, so the merged pieces "
		"pose as one skeleton." ) );
	auto * loadSkelMenu = new QMenu( loadSkelBtn );
	QAction * loadFromFileAct = loadSkelMenu->addAction( QObject::tr( "From file..." ) );
	QAction * loadFromArchiveAct = loadSkelMenu->addAction( QObject::tr( "From game archive..." ) );
	loadFromArchiveAct->setObjectName( QStringLiteral( "PoseLoadSkeletonArchiveAction" ) );
	loadSkelBtn->setMenu( loadSkelMenu );
	layout->addWidget( loadSkelBtn );

	// ---- bones ------------------------------------------------------------
	layout->addWidget( heading( QObject::tr( "Bones" ), panel ) );

	// display: which bones, names, relationship lines, weight overlay
	auto * dispRow = new QHBoxLayout;
	auto * filterCombo = new QComboBox( panel );
	filterCombo->setObjectName( QStringLiteral( "PoseBoneFilter" ) );
	filterCombo->addItems( { QObject::tr( "All bones" ), QObject::tr( "Deforming" ),
		QObject::tr( "Face sculpt" ) } );
	filterCombo->setToolTip( QObject::tr( "Which bones are drawn and pickable" ) );
	auto * namesChk = new QCheckBox( QObject::tr( "Names" ), panel );
	namesChk->setToolTip( QObject::tr( "Show bone names in the viewport" ) );
	auto * relChk = new QCheckBox( QObject::tr( "Links" ), panel );
	relChk->setObjectName( QStringLiteral( "PoseRelationsToggle" ) );
	relChk->setChecked( true );
	relChk->setToolTip( QObject::tr( "Show dashed parent-relationship lines" ) );
	auto * weightsChk = new QCheckBox( QObject::tr( "Weights" ), panel );
	weightsChk->setObjectName( QStringLiteral( "PoseWeightsToggle" ) );
	weightsChk->setToolTip( QObject::tr( "Highlight the vertices the hovered / selected bone affects (heat by weight)" ) );
	dispRow->addWidget( filterCombo, 1 );
	dispRow->addWidget( namesChk );
	dispRow->addWidget( relChk );
	dispRow->addWidget( weightsChk );
	layout->addLayout( dispRow );

	auto * boneList = new QListWidget( panel );
	boneList->setObjectName( QStringLiteral( "PoseBoneList" ) );
	boneList->setToolTip( QObject::tr( "Click a bone to select it, then G / R / S in the viewport to pose it.\n"
		"Ctrl / Shift-click selects several bones to transform together." ) );
	boneList->setSelectionMode( QAbstractItemView::ExtendedSelection );
	boneList->setMinimumHeight( 140 );
	layout->addWidget( boneList, 1 );

	// reset the selected bone (or all) to the pose captured on entering Pose Mode
	auto * resetRow = new QHBoxLayout;
	auto * resetBtn = new QPushButton( QObject::tr( "Reset bone" ), panel );
	resetBtn->setObjectName( QStringLiteral( "PoseResetBoneButton" ) );
	resetBtn->setToolTip( QObject::tr( "Restore the selected bone to its rest transform" ) );
	auto * resetAllBtn = new QPushButton( QObject::tr( "Reset all" ), panel );
	resetAllBtn->setToolTip( QObject::tr( "Restore every bone to the rest pose" ) );
	auto * resetWhat = new QComboBox( panel );
	resetWhat->setObjectName( QStringLiteral( "PoseResetChannel" ) );
	resetWhat->addItems( { QObject::tr( "All" ), QObject::tr( "Rotation" ),
		QObject::tr( "Location" ), QObject::tr( "Scale" ) } );
	resetWhat->setToolTip( QObject::tr( "Which transform channels a reset clears" ) );
	resetRow->addWidget( resetBtn );
	resetRow->addWidget( resetAllBtn );
	resetRow->addWidget( resetWhat );
	layout->addLayout( resetRow );

	auto * mirrorBtn = new QPushButton( QObject::tr( "Mirror to other side (X)" ), panel );
	mirrorBtn->setObjectName( QStringLiteral( "PoseMirrorButton" ) );
	mirrorBtn->setToolTip( QObject::tr( "Copy the selected bone's pose to its L/R counterpart, mirrored across X" ) );
	layout->addWidget( mirrorBtn );

	// pin: lock the selected bone(s) so other transforms / mirror / proportional
	// editing can't move them
	auto * pinRow = new QHBoxLayout;
	auto * pinBtn = new QPushButton( QObject::tr( "Pin / Unpin" ), panel );
	pinBtn->setObjectName( QStringLiteral( "PosePinButton" ) );
	pinBtn->setToolTip( QObject::tr( "Lock the selected bone(s) so they don't move or get affected by other transforms" ) );
	auto * unpinAllBtn = new QPushButton( QObject::tr( "Unpin all" ), panel );
	pinRow->addWidget( pinBtn );
	pinRow->addWidget( unpinAllBtn );
	layout->addLayout( pinRow );

	// non-destructive: the real bone nodes are restored on exit; the pose lives
	// in a file / the library. "Bake" commits it into the bones.
	auto * ndRow = new QHBoxLayout;
	auto * ndChk = new QCheckBox( QObject::tr( "Keep original bones (non-destructive)" ), panel );
	ndChk->setObjectName( QStringLiteral( "PoseNonDestructive" ) );
	ndChk->setChecked( true );
	ndChk->setToolTip( QObject::tr( "Leaving Pose Mode restores the real bone transforms; the pose persists only in files / the library. Turn off, or Bake, to keep it in the bones." ) );
	auto * bakeBtn = new QPushButton( QObject::tr( "Bake to bones" ), panel );
	bakeBtn->setToolTip( QObject::tr( "Commit the current pose into the bone transforms (exiting won't restore them)" ) );
	ndRow->addWidget( ndChk, 1 );
	ndRow->addWidget( bakeBtn );
	layout->addLayout( ndRow );

	// ---- pose library (folder of Outfit Studio .xml pose files) -----------
	auto * libGroup = new QGroupBox( QObject::tr( "Pose library" ), panel );
	auto * libLayout = new QVBoxLayout( libGroup );
	libLayout->setContentsMargins( 6, 8, 6, 6 );
	libLayout->setSpacing( 5 );

	// folder row: the NifSkope library root (poses live in its Poses subfolder)
	auto * folderRow = new QHBoxLayout;
	auto * folderLabel = new QLabel( libGroup );
	folderLabel->setObjectName( QStringLiteral( "PoseLibraryFolderLabel" ) );
	folderLabel->setStyleSheet( QStringLiteral( "QLabel { color: %1; }" ).arg( wwSkinColor( "textMuted" ) ) );
	auto * folderBtn = new QPushButton( QObject::tr( "Folder..." ), libGroup );
	folderBtn->setToolTip( QObject::tr( "Choose the NifSkope library folder — poses are stored in its Poses subfolder. "
		"Also settable in Settings → General → NifSkope Library." ) );
	folderBtn->setMaximumWidth( 70 );
	folderRow->addWidget( folderLabel, 1 );
	folderRow->addWidget( folderBtn );
	libLayout->addLayout( folderRow );

	auto * poseList = new QListWidget( libGroup );
	poseList->setObjectName( QStringLiteral( "PosePoseList" ) );
	poseList->setToolTip( QObject::tr( "Pose files in the library folder. Select one and Apply (with Blend), or Save the current pose here." ) );
	poseList->setMinimumHeight( 90 );
	libLayout->addWidget( poseList );

	auto * blendRow = new QHBoxLayout;
	blendRow->addWidget( new QLabel( QObject::tr( "Blend" ), libGroup ) );
	auto * blend = new QSlider( Qt::Horizontal, libGroup );
	blend->setObjectName( QStringLiteral( "PoseBlendSlider" ) );
	blend->setRange( 0, 100 );
	blend->setValue( 100 );
	blend->setToolTip( QObject::tr( "Apply strength: 100%% replaces the current pose, less blends toward it." ) );
	blendRow->addWidget( blend, 1 );
	auto * blendVal = new QLabel( QStringLiteral( "100%" ), libGroup );
	blendVal->setMinimumWidth( 36 );
	blendRow->addWidget( blendVal );
	libLayout->addLayout( blendRow );

	auto * btnRow = new QHBoxLayout;
	auto * applyBtn = new QPushButton( QObject::tr( "Apply" ), libGroup );
	auto * saveBtn = new QPushButton( QObject::tr( "Save current..." ), libGroup );
	auto * delBtn = new QPushButton( QObject::tr( "Delete" ), libGroup );
	btnRow->addWidget( applyBtn );
	btnRow->addWidget( saveBtn );
	btnRow->addWidget( delBtn );
	libLayout->addLayout( btnRow );

	// Outfit Studio pose XML files (BodySlide) — the blend slider applies here too
	auto * osRow = new QHBoxLayout;
	auto * importOsBtn = new QPushButton( QObject::tr( "Import pose file..." ), libGroup );
	importOsBtn->setObjectName( QStringLiteral( "PoseImportOsButton" ) );
	importOsBtn->setToolTip( QObject::tr( "Load an Outfit Studio pose (.xml) and apply it (uses the Blend strength)" ) );
	auto * exportOsBtn = new QPushButton( QObject::tr( "Export pose file..." ), libGroup );
	exportOsBtn->setToolTip( QObject::tr( "Save the current pose as an Outfit Studio pose (.xml)" ) );
	osRow->addWidget( importOsBtn );
	osRow->addWidget( exportOsBtn );
	libLayout->addLayout( osRow );

	/* Screen Archer Menu pose JSON, its own row because it is a different KIND of
	 * file: SAM transforms are absolute per-bone locals, so an import is a
	 * straight replacement and an export is the bones read back as they stand —
	 * neither one needs the rest capture the Outfit Studio pair is built on, and
	 * both work outside Pose Mode. */
	auto * samRow = new QHBoxLayout;
	auto * importSamBtn = new QPushButton( QObject::tr( "Import SAM pose..." ), libGroup );
	importSamBtn->setObjectName( QStringLiteral( "PoseImportSamButton" ) );
	importSamBtn->setToolTip( QObject::tr( "Load a Screen Archer Menu pose (.json) and apply it (uses the Blend strength)" ) );
	auto * exportSamBtn = new QPushButton( QObject::tr( "Export SAM pose..." ), libGroup );
	exportSamBtn->setObjectName( QStringLiteral( "PoseExportSamButton" ) );
	exportSamBtn->setToolTip( QObject::tr( "Save the skeleton's current bone transforms as a Screen "
		"Archer Menu pose (.json). Absolute local transforms, six decimals — no rest pose needed." ) );
	samRow->addWidget( importSamBtn );
	samRow->addWidget( exportSamBtn );
	libLayout->addLayout( samRow );

	layout->addWidget( libGroup );

	// ---- behaviour --------------------------------------------------------

	// Guard against the list<->viewport selection feedback loop: while we push a
	// selection from one side to the other, the return signal must not bounce.
	auto syncing = std::make_shared<bool>( false );

	// A refresh needs to survive block-renumbering (save/apply insert or remove
	/* The ACTIVE pose. A pose is written into the bone transforms, so it survives
	 * on its own for bones that already exist — but a piece merged in afterwards
	 * brings bones that were never posed, and the rig would come apart at exactly
	 * the seam you just added. Remembering which pose is on lets it be re-applied
	 * over the whole rig after every merge, so "load a pose and it stays loaded"
	 * holds no matter what arrives later. Cleared only by picking Default (rest).
	 */
	auto activePose = std::make_shared<QString>();
	auto activeBlend = std::make_shared<float>( 1.0f );

	// blocks), so the list carries node NAMES and resolves them per action.
	auto refresh = [=]() {
		NifModel * m = skope ? skope->getNifModel() : nif;
		boneList->clear();
		poseList->clear();
		if ( !m || m->getBlockCount() == 0 ) {
			status->setText( QObject::tr( "Open a skinned mesh to pose its skeleton." ) );
			return;
		}

		// remember which library pose was selected so a refresh (fired on every
		// model edit) doesn't drop the user's selection out from under them
		const QString selectedPose = poseList->currentItem()
			? poseList->currentItem()->data( Qt::UserRole ).toString() : QString();

		/* ...and which BONE was selected, which had no equivalent.
		 *
		 * Picking a bone in the viewport emits poseBonePicked and then clicked;
		 * clicked reaches NifSkope::select -> currentNifIndexChanged -> this
		 * refresh(), which opens with boneList->clear(). So picking a bone in the
		 * 3D view un-selected it in the panel, every time. The pose list directly
		 * above was already stashed and restored by name; the bone list never was.
		 */
		const QString selectedBone = boneList->currentItem()
			? boneList->currentItem()->data( Qt::UserRole ).toString() : QString();

		const QVector<int> bones = AnimSetup::poseBoneNodes( m );
		const QString filter = boneSearch->text().trimmed();
		for ( int b : bones ) {
			const QString name = m->get<QString>( m->getBlockIndex( b ), "Name" );
			if ( name.isEmpty() )
				continue;
			if ( !filter.isEmpty() && !name.contains( filter, Qt::CaseInsensitive ) )
				continue;
			// a pinned bone shows a lock marker; UserRole keeps the raw name
			const bool pinned = ogl && ogl->poseIsPinned( b );
			auto * item = new QListWidgetItem( pinned ? ( QStringLiteral( "🔒 " ) + name ) : name, boneList );
			item->setData( Qt::UserRole, name );	// resolve to a block on click
			if ( pinned )
				item->setForeground( QBrush( QColor( 0x9a, 0x9a, 0xa2 ) ) );
			if ( !selectedBone.isEmpty() && name == selectedBone )
				boneList->setCurrentItem( item );
		}

		// library = the .xml pose files in <library>/Poses (base name shown, full
		// path in UserRole). The label shows the library root's name; the tooltip
		// gives the actual Poses folder.
		const QString folder = poseLibraryFolder();
		folderLabel->setText( QDir( nifskopeLibraryFolder() ).dirName() );
		folderLabel->setToolTip( QObject::tr( "Poses stored in: %1" ).arg( folder ) );
		QDir dir( folder );
		int nposes = 0;
		// Default sits above the files and is not one: it is how you turn the
		// active pose OFF. Without a row for it there is no way back to rest
		// short of Reset all, which says nothing about the pose still being on.
		auto * defaultItem = new QListWidgetItem( QObject::tr( "Default (rest pose)" ), poseList );
		defaultItem->setData( Qt::UserRole, QString() );
		defaultItem->setForeground( QBrush( QColor( 0x9a, 0x9a, 0xa2 ) ) );
		for ( const QFileInfo & fi : dir.entryInfoList( { QStringLiteral( "*.xml" ) },
				QDir::Files, QDir::Name ) ) {
			const bool isActive = ( fi.absoluteFilePath() == *activePose );
			auto * it = new QListWidgetItem(
				isActive ? QStringLiteral( "● " ) + fi.completeBaseName() : fi.completeBaseName(),
				poseList );
			it->setData( Qt::UserRole, fi.absoluteFilePath() );
			if ( isActive )
				it->setForeground( QBrush( QColor( 0xFF, 0x9D, 0x00 ) ) );
			if ( !selectedPose.isEmpty() && fi.absoluteFilePath() == selectedPose )
				poseList->setCurrentItem( it );   // restore the selection by path
			nposes++;
		}
		if ( activePose->isEmpty() )
			poseList->setCurrentItem( defaultItem );

		status->setText( bones.isEmpty()
			? QObject::tr( "No skinned geometry — nothing to pose." )
			: QObject::tr( "%1 bone(s), %2 pose file(s).%3" ).arg( bones.size() ).arg( nposes )
				.arg( activePose->isEmpty() ? QString()
					: QObject::tr( " Pose: %1." ).arg( QFileInfo( *activePose ).completeBaseName() ) ) );
	};

	// resolve a node name to its current block (block numbers shift under edits)
	auto blockForName = [=]( const QString & name ) -> int {
		NifModel * m = skope ? skope->getNifModel() : nif;
		if ( !m )
			return -1;
		for ( int b = 0; b < m->getBlockCount(); b++ ) {
			QModelIndex i = m->getBlockIndex( b );
			if ( m->blockInherits( i, "NiAVObject" ) && m->get<QString>( i, "Name" ) == name )
				return b;
		}
		return -1;
	};

	// select bone(s) in the list -> select them in the viewport, ready for
	// G/R/S. Multiple rows select multiple bones (transformed together).
	QObject::connect( boneList, &QListWidget::itemSelectionChanged, panel, [=]() {
		NifModel * m = skope ? skope->getNifModel() : nif;
		if ( !m || !ogl || *syncing )
			return;
		const QList<QListWidgetItem *> sel = boneList->selectedItems();
		if ( sel.isEmpty() )
			return;
		QSet<int> blocks;
		for ( QListWidgetItem * it : sel ) {
			const int b = blockForName( it->data( Qt::UserRole ).toString() );
			if ( b >= 0 )
				blocks.insert( b );
		}
		if ( blocks.isEmpty() )
			return;
		const int active = blockForName( boneList->currentItem()
			? boneList->currentItem()->data( Qt::UserRole ).toString() : sel.last()->data( Qt::UserRole ).toString() );
		ogl->setObjectSelection( blocks, active >= 0 ? active : *blocks.constBegin() );

		// colour like the Block List: active = orange text, other selected =
		// blue row background (the rest cleared)
		// the blue here had drifted to #2b3b5c against everyone else's #2b425f,
		// while the comment above claimed it was copying the Block List
		const QColor orange = QColor::fromString( wwSkinColor( "selTextActive" ) );
		const QColor blueRow = QColor::fromString( wwSkinColor( "selBgInactive" ) );
		QSet<QListWidgetItem *> selSet( sel.constBegin(), sel.constEnd() );
		for ( int r = 0; r < boneList->count(); r++ ) {
			QListWidgetItem * it = boneList->item( r );
			const int b = blockForName( it->data( Qt::UserRole ).toString() );
			const bool isActive = ( b == active );
			const bool isSel = selSet.contains( it );
			it->setForeground( isActive ? QBrush( orange ) : QBrush() );
			it->setBackground( ( isSel && !isActive ) ? QBrush( blueRow ) : QBrush() );
		}
	} );

	// bring in a skeleton / armour piece from another NIF (the merge that shares
	// same-named bones, so the pieces pose as one skeleton). Both the file and
	// archive paths funnel through this: check there's a document, then report.
	auto currentTarget = [=]() -> NifModel * {
		NifModel * m = skope ? skope->getNifModel() : nif;
		if ( !m || m->getBlockCount() == 0 ) {
			QMessageBox::information( panel, QObject::tr( "Load skeleton" ),
				QObject::tr( "Open a NIF first, then merge a skeleton into it." ) );
			return nullptr;
		}
		return m;
	};
	auto reportMerge = [=]( const QString & label, bool ok, const NifMergeResult & r ) {
		if ( !ok ) {
			QMessageBox::warning( panel, QObject::tr( "Load skeleton" ), r.error );
			return;
		}
		status->setText( QObject::tr( "Merged %1: %2 bone(s) shared, %3 added, %4 shape(s)." )
			.arg( label ).arg( r.nodesReused ).arg( r.nodesAdded ).arg( r.shapesAdded ) );
		if ( r.nodesReused == 0 )
			QMessageBox::information( panel, QObject::tr( "Load skeleton" ),
				QObject::tr( "No bones matched by name, so the merged pieces do not share a "
					"skeleton — posing them as one rig will not work." ) );
		// A rig binds bones by NAME, so a repeated name silently sends a pose to
		// the wrong node and the rig folds up. It should never happen; say so
		// loudly if it does, because the symptom names no cause.
		else if ( !r.duplicateNames.isEmpty() )
			QMessageBox::warning( panel, QObject::tr( "Load skeleton" ),
				QObject::tr( "%1 bone name(s) now appear on more than one node:\n\n%2\n\n"
					"Posing will address only one of each pair. Undo the merge." )
					.arg( r.duplicateNames.size() )
					.arg( r.duplicateNames.mid( 0, 12 ).join( QStringLiteral( ", " ) )
						+ ( r.duplicateNames.size() > 12 ? QObject::tr( ", ..." ) : QString() ) ) );
		/* Put the active pose back over the whole rig. The bones that were
		 * already here kept it — it lives in their transforms — but everything
		 * that just arrived is at bind, and a half-posed rig is worse than an
		 * unposed one. Re-applying costs nothing when nothing new matched.
		 */
		if ( ogl && !activePose->isEmpty() ) {
			QString poseError;
			// the active pose may be either flavour of file; dispatch on suffix
			const int n = activePose->endsWith( QStringLiteral( ".json" ), Qt::CaseInsensitive )
				? ogl->poseImportSam( *activePose, *activeBlend, &poseError )
				: ogl->poseImportOutfitStudio( *activePose, *activeBlend, &poseError );
			if ( n > 0 )
				status->setText( status->text() + QObject::tr( " Re-applied pose '%1' over %2 bone(s)." )
					.arg( QFileInfo( *activePose ).completeBaseName() ).arg( n ) );
		}
		refresh();
	};

	// From file: the classic file-dialog path
	QObject::connect( loadFromFileAct, &QAction::triggered, panel, [=]() {
		NifModel * m = currentTarget();
		if ( !m )
			return;
		const QString path = QFileDialog::getOpenFileName( panel,
			QObject::tr( "Load skeleton / armour piece" ), m->getFolder(),
			QObject::tr( "NIF files (*.nif)" ) );
		if ( path.isEmpty() )
			return;
		NifMergeResult r;
		const bool ok = nifMergeFile( m, path, true, r );
		reportMerge( QFileInfo( path ).fileName(), ok, r );
	} );

	// From game archive: reuse the existing NIF Browser to pick a NIF from a
	// BSA/BA2, extract its bytes, and merge them (no unpack to disk needed)
	QObject::connect( loadFromArchiveAct, &QAction::triggered, panel, [=]() {
		NifModel * m = currentTarget();
		if ( !m )
			return;
		if ( !skope ) {
			QMessageBox::information( panel, QObject::tr( "Load skeleton" ),
				QObject::tr( "The NIF Browser is only available from the main window." ) );
			return;
		}
		QByteArray bytes;
		QString label;
		if ( !skope->pickNifFromBrowser( panel, bytes, label ) )
			return;
		NifMergeResult r;
		const bool ok = nifMergeData( m, bytes, label, true, r );
		reportMerge( label, ok, r );
	} );

	// picking a bone in the viewport highlights it in the list
	if ( ogl )
		QObject::connect( ogl, &GLView::poseBonePicked, panel, [=]( int block ) {
			NifModel * m = skope ? skope->getNifModel() : nif;
			if ( !m || block < 0 )
				return;
			const QString name = m->get<QString>( m->getBlockIndex( block ), "Name" );
			// reflect the viewport pick in the list without bouncing back
			*syncing = true;
			for ( int r = 0; r < boneList->count(); r++ )
				if ( boneList->item( r )->data( Qt::UserRole ).toString() == name ) {
					boneList->setCurrentRow( r );
					break;
				}
			*syncing = false;
		} );

	QObject::connect( boneSearch, &QLineEdit::textChanged, panel, [=]() { refresh(); } );

	// channel bitmask from the combo: All=7, Rotation=1, Location=2, Scale=4
	auto resetChannels = [resetWhat]() {
		switch ( resetWhat->currentIndex() ) {
		case 1: return 1;
		case 2: return 2;
		case 3: return 4;
		default: return 7;
		}
	};
	QObject::connect( resetBtn, &QPushButton::clicked, panel, [=]() {
		if ( !ogl ) return;
		QListWidgetItem * sel = boneList->currentItem();
		if ( !sel ) return;
		const int b = blockForName( sel->data( Qt::UserRole ).toString() );
		if ( b >= 0 )
			ogl->poseResetBone( b, resetChannels() );
	} );
	QObject::connect( resetAllBtn, &QPushButton::clicked, panel, [=]() {
		if ( ogl )
			ogl->poseResetBone( -1, resetChannels() );
	} );
	QObject::connect( mirrorBtn, &QPushButton::clicked, panel, [=]() {
		if ( !ogl ) return;
		QListWidgetItem * sel = boneList->currentItem();
		if ( !sel ) return;
		const int b = blockForName( sel->data( Qt::UserRole ).toString() );
		if ( b >= 0 && ogl->poseMirrorBone( b ) < 0 )
			status->setText( QObject::tr( "No L/R counterpart found for that bone." ) );
	} );

	// display toggles drive the viewport; the filter also changes the bone list
	QObject::connect( filterCombo, QOverload<int>::of( &QComboBox::currentIndexChanged ),
		panel, [=]( int idx ) {
			if ( ogl ) ogl->setPoseBoneFilter( idx );
			refresh();
		} );
	QObject::connect( namesChk, &QCheckBox::toggled, panel,
		[=]( bool on ) { if ( ogl ) ogl->setPoseShowBoneNames( on ); } );
	QObject::connect( relChk, &QCheckBox::toggled, panel,
		[=]( bool on ) { if ( ogl ) ogl->setPoseShowRelations( on ); } );
	QObject::connect( weightsChk, &QCheckBox::toggled, panel,
		[=]( bool on ) { if ( ogl ) ogl->setPoseShowWeights( on ); } );

	// pin / unpin the selected bone(s)
	QObject::connect( pinBtn, &QPushButton::clicked, panel, [=]() {
		if ( !ogl ) return;
		for ( QListWidgetItem * it : boneList->selectedItems() ) {
			const int b = blockForName( it->data( Qt::UserRole ).toString() );
			if ( b >= 0 )
				ogl->poseTogglePin( b );
		}
		refresh();
	} );
	QObject::connect( unpinAllBtn, &QPushButton::clicked, panel, [=]() {
		if ( ogl ) ogl->poseClearPins();
		refresh();
	} );

	// non-destructive toggle + bake
	QObject::connect( ndChk, &QCheckBox::toggled, panel,
		[=]( bool on ) { if ( ogl ) ogl->setPoseNonDestructive( on ); } );
	QObject::connect( bakeBtn, &QPushButton::clicked, panel, [=]() {
		if ( ogl ) ogl->poseBakeToBones();
	} );

	QObject::connect( blend, &QSlider::valueChanged, blendVal, [=]( int v ) {
		blendVal->setText( QStringLiteral( "%1%" ).arg( v ) );
	} );

	// choose the NifSkope library root (poses live in its Poses subfolder)
	QObject::connect( folderBtn, &QPushButton::clicked, panel, [=]() {
		const QString dir = QFileDialog::getExistingDirectory( panel,
			QObject::tr( "NifSkope library folder" ), nifskopeLibraryFolder() );
		if ( !dir.isEmpty() ) {
			setNifskopeLibraryFolder( dir );
			refresh();
		}
	} );

	// Save current pose as a new .xml file in the library folder
	QObject::connect( saveBtn, &QPushButton::clicked, panel, [=]() {
		if ( !ogl )
			return;
		bool ok = false;
		const QString name = QInputDialog::getText( panel, QObject::tr( "Save pose" ),
			QObject::tr( "Pose name:" ), QLineEdit::Normal,
			QStringLiteral( "Pose" ), &ok ).trimmed();
		if ( !ok || name.isEmpty() )
			return;
		QString safe = name;
		safe.replace( QRegularExpression( QStringLiteral( "[\\\\/:*?\"<>|]" ) ), QStringLiteral( "_" ) );
		const QString path = QDir( poseLibraryFolder() ).filePath( safe + QStringLiteral( ".xml" ) );
		QString error;
		if ( !ogl->poseExportOutfitStudio( path, name, &error ) )
			QMessageBox::warning( panel, QObject::tr( "Save pose" ), error );
		else
			status->setText( QObject::tr( "Saved pose '%1' to the library." ).arg( name ) );
		refresh();
	} );

	// Apply the selected library pose file (with the blend strength). Double-
	// clicking a pose applies it too.
	auto applyLibraryPose = [=]( QListWidgetItem * sel ) {
		if ( !ogl || !sel )
			return;
		const QString path = sel->data( Qt::UserRole ).toString();
		// Default (rest): the only way to turn the active pose off
		if ( path.isEmpty() ) {
			activePose->clear();
			ogl->poseResetBone( -1, 7 );		// every channel, every bone
			status->setText( QObject::tr( "Back to the rest pose." ) );
			refresh();
			return;
		}
		QString error;
		const int n = ogl->poseImportOutfitStudio( path, blend->value() / 100.0f, &error );
		if ( n <= 0 ) {
			QMessageBox::warning( panel, QObject::tr( "Apply pose" ), error );
			return;
		}
		*activePose = path;
		*activeBlend = blend->value() / 100.0f;
		status->setText( QObject::tr( "Applied '%1': %2 bone(s) posed.%3" )
			.arg( QFileInfo( path ).completeBaseName() ).arg( n )
			.arg( error.isEmpty() ? QString() : QStringLiteral( " " ) + error ) );
		refresh();
	};
	QObject::connect( applyBtn, &QPushButton::clicked, panel,
		[=]() { applyLibraryPose( poseList->currentItem() ); } );
	QObject::connect( poseList, &QListWidget::itemDoubleClicked, panel,
		[=]( QListWidgetItem * it ) { applyLibraryPose( it ); } );

	// Delete the selected library pose file (with confirmation)
	QObject::connect( delBtn, &QPushButton::clicked, panel, [=]() {
		QListWidgetItem * sel = poseList->currentItem();
		if ( !sel )
			return;
		const QString path = sel->data( Qt::UserRole ).toString();
		if ( QMessageBox::question( panel, QObject::tr( "Delete pose" ),
				QObject::tr( "Delete the pose file '%1'?" ).arg( sel->text() ) ) != QMessageBox::Yes )
			return;
		QFile::remove( path );
		refresh();
	} );

	// Outfit Studio pose XML import/export (uses the blend slider on import)
	QObject::connect( importOsBtn, &QPushButton::clicked, panel, [=]() {
		if ( !ogl ) return;
		const QString path = QFileDialog::getOpenFileName( panel,
			QObject::tr( "Import Outfit Studio pose" ), poseLibraryFolder(),
			QObject::tr( "Pose XML (*.xml)" ) );
		if ( path.isEmpty() ) return;
		QString error;
		const int n = ogl->poseImportOutfitStudio( path, blend->value() / 100.0f, &error );
		if ( n <= 0 )
			QMessageBox::warning( panel, QObject::tr( "Import pose" ), error );
		else {
			// an imported pose is the active one too, so it survives later merges
			*activePose = path;
			*activeBlend = blend->value() / 100.0f;
			status->setText( QObject::tr( "Imported %1: %2 bone(s) posed." )
				.arg( QFileInfo( path ).fileName() ).arg( n ) );
			if ( !error.isEmpty() )   // partial: some bones not in this skeleton
				status->setText( status->text() + QStringLiteral( " " ) + error );
			refresh();
		}
	} );
	// Screen Archer Menu pose JSON import (uses the blend slider too)
	QObject::connect( importSamBtn, &QPushButton::clicked, panel, [=]() {
		if ( !ogl ) return;
		const QString path = QFileDialog::getOpenFileName( panel,
			QObject::tr( "Import SAM pose" ), poseLibraryFolder(),
			QObject::tr( "SAM Pose JSON (*.json)" ) );
		if ( path.isEmpty() ) return;
		QString error;
		const int n = ogl->poseImportSam( path, blend->value() / 100.0f, &error );
		if ( n <= 0 )
			QMessageBox::warning( panel, QObject::tr( "Import SAM pose" ), error );
		else {
			// an imported pose is the active one too, so it survives later merges
			*activePose = path;
			*activeBlend = blend->value() / 100.0f;
			status->setText( QObject::tr( "Imported %1: %2 bone(s) posed." )
				.arg( QFileInfo( path ).fileName() ).arg( n ) );
			if ( !error.isEmpty() )   // partial: some bones not in this skeleton
				status->setText( status->text() + QStringLiteral( " " ) + error );
			refresh();
		}
	} );
	// Screen Archer Menu pose JSON export. Same split as everything else here: the
	// dialog picks the path, GLView::poseExportSam does the work and is callable
	// with no GUI at all (which is what WW_SAMEXPORT_TEST drives).
	QObject::connect( exportSamBtn, &QPushButton::clicked, panel, [=]() {
		if ( !ogl ) return;
		QString path = QFileDialog::getSaveFileName( panel,
			QObject::tr( "Export SAM pose" ),
			QDir( poseLibraryFolder() ).filePath( QStringLiteral( "pose.json" ) ),
			QObject::tr( "SAM Pose JSON (*.json)" ) );
		if ( path.isEmpty() ) return;
		if ( !path.endsWith( QStringLiteral( ".json" ), Qt::CaseInsensitive ) )
			path += QStringLiteral( ".json" );
		QString error;
		int written = 0;
		if ( !ogl->poseExportSam( path, QFileInfo( path ).completeBaseName(), &written, &error ) )
			QMessageBox::warning( panel, QObject::tr( "Export SAM pose" ), error );
		else
			status->setText( QObject::tr( "Exported %1 bone(s) to %2.%3" ).arg( written )
				.arg( QFileInfo( path ).fileName() )
				.arg( error.isEmpty() ? QString() : QStringLiteral( " " ) + error ) );
	} );
	QObject::connect( exportOsBtn, &QPushButton::clicked, panel, [=]() {
		if ( !ogl ) return;
		QString path = QFileDialog::getSaveFileName( panel,
			QObject::tr( "Export Outfit Studio pose" ),
			QDir( poseLibraryFolder() ).filePath( QStringLiteral( "pose.xml" ) ),
			QObject::tr( "Pose XML (*.xml)" ) );
		if ( path.isEmpty() ) return;
		if ( !path.endsWith( QStringLiteral( ".xml" ), Qt::CaseInsensitive ) )
			path += QStringLiteral( ".xml" );
		QString error;
		if ( !ogl->poseExportOutfitStudio( path, QFileInfo( path ).completeBaseName(), &error ) )
			QMessageBox::warning( panel, QObject::tr( "Export pose" ), error );
		else
			status->setText( QObject::tr( "Exported pose to %1" ).arg( QFileInfo( path ).fileName() ) );
	} );

	// keep in step with the document, like the other manager docks
	if ( skope ) {
		QObject::connect( skope, &NifSkope::currentNifIndexChanged, panel, [=]( const QModelIndex & ) { refresh(); } );
		QObject::connect( skope, &NifSkope::completeLoading, panel,
			[=]( bool, QString & ) { QTimer::singleShot( 0, panel, [=]() { refresh(); } ); } );
		// live-update the library when the folder is changed in Settings
		// (General -> Poses); both write the same Pose Library Folder key
		if ( SettingsDialog * opt = skope->getOptions() )
			QObject::connect( opt, &SettingsDialog::saveSettings, panel,
				[=]() { QTimer::singleShot( 0, panel, [=]() { refresh(); } ); } );
	}
	QObject::connect( nif, &QAbstractItemModel::modelReset, panel,
		[=]() { QTimer::singleShot( 0, panel, [=]() { refresh(); } ); } );
	if ( nif->undoStack )
		QObject::connect( nif->undoStack, &QUndoStack::indexChanged, panel,
			[=]() { QTimer::singleShot( 0, panel, [=]() { refresh(); } ); } );
	// Showing the Pose workspace enters Pose Mode (skeleton drawn, bones
	// clickable); hiding it leaves. Matches how the paint docks drive their
	// viewport modes.
	QObject::connect( dock, &QDockWidget::visibilityChanged, panel, [=]( bool visible ) {
		if ( ogl ) {
			if ( visible && !ogl->poseModeActive() )
				ogl->setPoseMode( true );
			else if ( !visible && ogl->poseModeActive() )
				ogl->setPoseMode( false );
		}
		if ( visible )
			refresh();
	} );

	// the export harness drives the callables above with no dialog in sight;
	// see wwSamExportHarness for what it measures and why it lives in this file
	if ( skope && ogl && qEnvironmentVariableIsSet( "WW_SAMEXPORT_TEST" ) )
		wwSamExportHarness( skope, ogl, panel );

	refresh();
	dock->setWidget( panel );
	/* Docked and hidden, like every sibling. This was returning an UNDOCKED
	 * QDockWidget; skeletontools.cpp documents the startup bug that same
	 * omission caused there.
	 */
	mw->addDockWidget( Qt::RightDockWidgetArea, dock );
	dock->hide();
	return dock;
}
