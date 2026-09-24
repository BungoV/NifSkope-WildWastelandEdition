/* The menu entry for the animated glTF export (lane HKX4b, 2026-09-10;
   options dialog and the clip repair, lane GLTFEXPORT1, 2026-09-19).

   `File > Export > .glTF (skeleton, skin and animation)`. The thin GUI half:
   it shows the options dialog, picks the file name, calls
   src/gltfexportchar.cpp, and reports in words what went out and what did not.
   All of the reading is there, none of it is here.

   WHY IT IS NOT src/lib/importex/gltf.cpp. That exporter stays the one for a
   static scene: it carries materials, embedded PNG textures, LODs and
   Starfield, and it goes through the GL Scene. It cannot write an animation
   (its `animations` array is never populated) and nothing it does can be
   gated without building the whole application. This one writes the clip.
   The two agree on the conventions a user can see -- 1 unit = 0.9144/64 m and
   the Z-up -> Y-up rotation at the root -- so a mesh from either lands in the
   same place in Blender. See docs/GLTF_INTERCHANGE.md.

   THE DEFECT THIS FILE CARRIED until 2026-09-19, and the whole of bungo's
   "the export had no animation in it": the clip was fetched through
   gltfExportClipProvider(), and NOTHING IN THE TREE EVER CALLED
   gltfExportSetClipProvider(). `grep -rn gltfExportSetClipProvider src`
   returned one line, its own definition (src/gltfexportnif.cpp:459). So the
   menu export ALWAYS took the no-clip branch and always printed "NO ANIMATION
   was written: no clip is loaded", however many clips were playing. It needed
   no new plumbing to repair: src/lib/importex/importex.cpp:140 already passes
   `ogl->scene` into this function, and this function threw it away with
   Q_UNUSED. The Scene owns the clip list (`Scene::hkx`), so the clip was one
   dereference away the entire time.

   The provider seam is KEPT and is now actually registered, against the scene
   this call was handed, so an in-process caller that has no Scene of its own
   still reaches the playing clip.

   NOT HOOKED UP: scratchpad/hkx4_20260910/hookup.py adds the row in
   src/lib/importex/importex.cpp and the three NifSkope.pro lines. */

#include "gltfexportchar.h"
#include "gltfexportdialog.h"
#include "gltfexportnif.h"

#include "gl/glscene.h"
#include "hkxplayback.h"
#include "model/nifmodel.h"

#include <QApplication>
#include <QMessageBox>
#include <QString>
#include <QStringList>

#define tr( x ) QApplication::tr( x )

// defined in importex.cpp
QString getImportexFileName( const NifModel * nif, const char * fileType, bool isImport );

namespace {

//! The scene the current export is running against. A file static because the
//! provider seam is a C function pointer with no user data; set for the
//! duration of one export and cleared after, so nothing here outlives the call.
const Scene * s_exportScene = nullptr;

/*! Turn the active clip entry into the pair gltfExportNifClip() wants.
 *
 *  HkxClipEntry::trackBone is indexed by TRACK. gltfExportNifClip() looks up
 *  `boneNames[ clip.trackToBone[t] ]`, which is indexed by BONE. The two are
 *  the same list only when trackToBone is the identity, and on a Fallout 4
 *  body it is not -- which is exactly the kind of quiet off-by-a-permutation
 *  that would have put the wrong bone's curve on a node. So the list is
 *  SCATTERED here, by the clip's own map, and any bone index no track names
 *  keeps an empty string (gltfExportNifClip treats that as unmatched and
 *  reports it).
 */
bool clipFromScene( const Scene * scene, HkxAnimClip & clip, QStringList & boneNames )
{
	if ( !scene || !scene->hkx )
		return false;
	const HkxClipEntry * e = scene->hkx->activeEntry();
	if ( !e )
		return false;

	clip = e->clip;

	int highest = -1;
	for ( int t = 0; t < e->trackBone.size(); t++ ) {
		const int bone = t < clip.trackToBone.size() ? clip.trackToBone[t] : t;
		if ( bone > highest )
			highest = bone;
	}
	boneNames.clear();
	if ( highest < 0 )
		return true;
	for ( int i = 0; i <= highest; i++ )
		boneNames << QString();
	for ( int t = 0; t < e->trackBone.size(); t++ ) {
		const int bone = t < clip.trackToBone.size() ? clip.trackToBone[t] : t;
		if ( bone >= 0 && bone < boneNames.size() && boneNames[bone].isEmpty() )
			boneNames[bone] = e->trackBone[t];
	}
	return true;
}

bool sceneClipProvider( HkxAnimClip & clip, QStringList & boneNames )
{
	return clipFromScene( s_exportScene, clip, boneNames );
}

QString rootMotionWord( const GltfExportOptions & o )
{
	switch ( o.rootMotion ) {
	case GltfExportOptions::RootMotion::Root:
		return QApplication::tr( "kept on the root bone" );
	case GltfExportOptions::RootMotion::Object:
		return QApplication::tr( "baked onto the object" );
	default:
		return QApplication::tr( "stripped" );
	}
}

} // namespace

void exportGltfAnimated( const NifModel * nif, const Scene * scene, const QModelIndex & index )
{
	if ( !nif )
		return;
	if ( nif->getBSVersion() < 130 ) {
		QMessageBox::critical( nullptr, "NifSkope",
			tr( "The animated glTF export reads Fallout 4 files (BS version 130 and up). "
				"This file is BS version %1; use Export .glTF for it." ).arg( nif->getBSVersion() ) );
		return;
	}
	if ( index.isValid() && !nif->blockInherits( index, { "NiNode", "BSTriShape" } ) ) {
		QMessageBox::critical( nullptr, "NifSkope",
			tr( "Select a node or a shape to export, or nothing at all to export the whole file." ) );
		return;
	}

	// The clip is read BEFORE the dialog so the dialog's Animation row can name
	// it, and so "no clip loaded" is a statement about the viewer rather than
	// about a provider that was never registered.
	s_exportScene = scene;
	gltfExportSetClipProvider( sceneClipProvider );

	HkxAnimClip clip;
	QStringList boneNames;
	bool haveClip = false;
	if ( GltfExportClipProvider provider = gltfExportClipProvider() )
		haveClip = provider( clip, boneNames );

	GltfExportDialog dlg( nif->getFolder(), haveClip, clip.name, nullptr );
	if ( dlg.exec() != QDialog::Accepted ) {
		s_exportScene = nullptr;
		return;
	}
	GltfExportOptions opts = dlg.options();
	if ( !haveClip )
		opts.includeClip = false;

	const QString filename = getImportexFileName( nif, "glTF", false );
	if ( filename.isEmpty() ) {
		s_exportScene = nullptr;
		return;
	}

	GltfExportReport report;
	GltfExportCharReport charReport;
	QString error;
	const bool wrote = gltfExportCharacter( nif, index, opts.includeClip ? &clip : nullptr,
											boneNames, opts, filename, report, charReport, error );
	s_exportScene = nullptr;
	if ( !wrote ) {
		QMessageBox::critical( nullptr, "NifSkope", tr( "glTF export refused: %1" ).arg( error ) );
		return;
	}

	QString msg = tr( "Wrote %1 and its .bin: %2 nodes, %3 shapes (%4 skinned), %5 vertices, %6 triangles." )
		.arg( filename ).arg( report.nodes ).arg( report.shapes ).arg( report.skinnedShapes )
		.arg( report.vertices ).arg( report.triangles );

	if ( opts.includeClip )
		msg += tr( "\n\nAnimation '%1': %2 of %3 tracks matched a node, %4 did not." )
			.arg( clip.name ).arg( report.tracksMatched )
			.arg( report.tracksMatched + report.tracksUnmatched ).arg( report.tracksUnmatched );
	else if ( haveClip )
		msg += tr( "\n\nNO ANIMATION was written: the Animation row was turned off in the export options." );
	else
		msg += tr( "\n\nNO ANIMATION was written: no clip is playing in the Animation workspace. "
				   "Load one there, select it, and export again." );
	msg += tr( "\nRoot motion was %1." ).arg( rootMotionWord( opts ) );

	// THE UNIT SENTENCE. It is written every time, because "why is my character
	// 1/70th of the size I expected" is the single question this export has
	// produced most often, and the answer is a number the file itself does not
	// carry in words.
	if ( opts.units == GltfExportOptions::Units::GameUnits )
		msg += tr( "\n\nUnits: GAME UNITS. 1 glTF unit = 1 Fallout 4 unit, so the file is "
				   "%1x larger than a metric one. This is what PyNifly expects." )
			.arg( GLTF_UNITS_PER_METRE, 0, 'f', 4 );
	else
		msg += tr( "\n\nUnits: METRES. 1 Fallout 4 unit = %1 m (0.9144/64), so a character is "
				   "about 1.8 m tall in Blender. For PyNifly, or to work in game units, "
				   "set Units to Game units in the export options (x%2)." )
			.arg( GLTF_METRES_PER_UNIT, 0, 'f', 7 ).arg( GLTF_UNITS_PER_METRE, 0, 'f', 4 );

	// Everything the character export decided, in its own numbers.
	QStringList did;
	if ( charReport.skeletonNodes )
		did << tr( "skeleton: %1 nodes" ).arg( charReport.skeletonNodes );
	if ( charReport.partsMerged )
		did << tr( "%1 extra part(s): %2 nodes matched by name, %3 added" )
			.arg( charReport.partsMerged ).arg( charReport.nodesMatchedByName ).arg( charReport.nodesAdded );
	if ( charReport.skinsUnified )
		did << tr( "%1 skin(s) share one joint list of %2 (%3 inverse binds derived)" )
			.arg( charReport.skinsUnified ).arg( charReport.jointsPerSkin ).arg( charReport.inverseBindsDerived );
	if ( charReport.helpersDropped )
		did << tr( "%1 helper bone(s) dropped, %2 kept because a skin weights them" )
			.arg( charReport.helpersDropped ).arg( charReport.helpersKeptWeighted );
	if ( charReport.texturesCopied || charReport.texturesMissing )
		did << tr( "%1 texture(s) copied, %2 not found" )
			.arg( charReport.texturesCopied ).arg( charReport.texturesMissing );
	if ( charReport.buildBonesScaled )
		did << tr( "body build applied to %1 bone(s)%2" ).arg( charReport.buildBonesScaled )
			.arg( charReport.buildCycleChannels
				  ? tr( ", cycle clip on %1 channel(s)" ).arg( charReport.buildCycleChannels )
				  : QString() );
	if ( !did.isEmpty() )
		msg += QStringLiteral( "\n\n" ) + did.join( QStringLiteral( "\n" ) );

	// A bone nothing matched is NAMED. Never dropped in silence.
	if ( !charReport.unmatchedBones.isEmpty() ) {
		QStringList show = charReport.unmatchedBones.mid( 0, 12 );
		msg += tr( "\n\n%1 bone(s) had no node anywhere: %2%3" )
			.arg( charReport.unmatchedBones.size() ).arg( show.join( QStringLiteral( ", " ) ) )
			.arg( charReport.unmatchedBones.size() > show.size() ? QStringLiteral( ", ..." ) : QString() );
	}

	if ( !report.notes.isEmpty() )
		msg += QStringLiteral( "\n\n" ) + report.notes.join( QStringLiteral( "\n" ) );
	if ( !charReport.notes.isEmpty() )
		msg += QStringLiteral( "\n\n" ) + charReport.notes.join( QStringLiteral( "\n" ) );

	if ( opts.textures == GltfExportOptions::Textures::Reference )
		msg += tr( "\n\nTextures are referenced by their game-relative .dds path and are not written; "
				   "Blender will show the materials without them. Set Textures to \"Copy beside the "
				   "file\" in the export options to write them out." );

	QMessageBox::information( nullptr, "NifSkope", msg );
}
