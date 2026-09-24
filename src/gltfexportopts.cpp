/* The options struct's own behaviour: the legacy predicate, the helper-bone
   rule, the CLI parse and the words the dialog, the summary and the report
   all read. QtCore only -- see src/gltfexportopts.h for why.

   Lane GLTFEXPORT1, 2026-09-19. */

#include "gltfexportopts.h"

#include <QFileInfo>

bool gltfExportOptionsAreLegacy( const GltfExportOptions & o )
{
	// includeClip is deliberately NOT here: carrying the loaded clip is the
	// repair of a defect (the provider had no caller), not a toggle over a
	// behaviour anyone chose.
	return o.skeleton == GltfExportOptions::Skeleton::None
		&& o.joints == GltfExportOptions::Joints::Weighted
		&& o.units == GltfExportOptions::Units::Metres
		&& o.parts.isEmpty()
		&& o.bones == GltfExportOptions::Bones::All
		&& o.textures == GltfExportOptions::Textures::Reference
		&& !o.bakeBodyBuild
		&& !o.buildCycleClip;
	// rootMotion is not in the predicate either: Strip IS what the old call
	// passed, and Root IS what the old --root-motion passed, so both old
	// command lines keep their bytes. Object is new and moves bytes only when
	// it is asked for.
}

GltfExportOptions gltfExportLegacyOptions()
{
	// Written out field by field rather than by flipping the six the ruling
	// moved, so that a LATER ruling on a seventh cannot quietly take the legacy
	// struct with it. gltfExportOptionsAreLegacy() below is the check that this
	// and the predicate have not drifted apart, and the dialog gate calls it.
	GltfExportOptions o;
	o.skeleton = GltfExportOptions::Skeleton::None;
	o.skeletonPath.clear();
	o.joints = GltfExportOptions::Joints::Weighted;
	o.units = GltfExportOptions::Units::Metres;
	o.parts.clear();
	o.bones = GltfExportOptions::Bones::All;
	o.textures = GltfExportOptions::Textures::Reference;
	o.rootMotion = GltfExportOptions::RootMotion::Strip;
	o.bakeBodyBuild = false;
	o.buildCycleClip = false;
	return o;
}

namespace {

const char * const HELPER_EXACT[] = {
	"camera", "camera control", "camtarget", "camtargetparent",
	// NOT "skeleton.nif": measured 2026-09-19, that string is the name of the
	// ROOT NiNode of the shipped human skeleton (block 0), not a helper.
	"charbumper", "characterbumper", nullptr
};

const char * const HELPER_PREFIX[] = {
	"animobject", "weapon", "cam", "prop", "loot", "ladder", "bumper", nullptr
};

} // namespace

bool gltfExportIsHelperBone( const QString & name )
{
	const QString n = name.trimmed().toLower();
	if ( n.isEmpty() )
		return false;
	for ( int i = 0; HELPER_EXACT[i]; i++ )
		if ( n == QLatin1String( HELPER_EXACT[i] ) )
			return true;
	for ( int i = 0; HELPER_PREFIX[i]; i++ )
		if ( n.startsWith( QLatin1String( HELPER_PREFIX[i] ) ) )
			return true;
	return false;
}

bool gltfExportPngAvailable()
{
	// There is no DDS decoder reachable from a QtCore-only translation unit in
	// this tree, and the one the renderer uses (src/gl/gltex*) needs a GL
	// context. So the choice is not offered rather than offered and refused.
	return false;
}

int gltfExportParseFlag( const QString & token, const QString & next,
						 GltfExportOptions & o, bool & usedNext, QString & error )
{
	usedNext = false;
	error.clear();
	auto needValue = [&]( QString & into ) -> bool {
		if ( next.isNull() ) {
			error = QStringLiteral( "%1 needs a value" ).arg( token );
			return false;
		}
		into = next;
		usedNext = true;
		return true;
	};

	/* THE WAY BACK. bungo ruled Blender-ready defaults on 2026-09-19 09:45;
	 * this is the one flag that puts every ruled field back where it was, so
	 * the old bytes stay reachable and the byte-identity gate row still has
	 * something to prove.
	 *
	 * It moves the SIX RULED FIELDS ONLY and leaves everything else on the
	 * command line alone -- --data-root, --part, --root-motion-mode, the body
	 * build -- because those were never part of the ruling and silently
	 * dropping a value the caller typed would be worse than any tidiness.
	 * It is therefore order-dependent ON PURPOSE: a flag after it wins, which
	 * is what "legacy except for X" has to mean. */
	if ( token == QLatin1String( "--legacy-defaults" ) ) {
		const GltfExportOptions L = gltfExportLegacyOptions();
		o.skeleton = L.skeleton;
		o.skeletonPath = L.skeletonPath;
		o.joints = L.joints;
		o.units = L.units;
		o.bones = L.bones;
		o.textures = L.textures;
		return 1;
	}
	if ( token == QLatin1String( "--skeleton" ) ) {
		QString v;
		if ( !needValue( v ) )
			return -1;
		if ( v.compare( QLatin1String( "none" ), Qt::CaseInsensitive ) == 0 ) {
			o.skeleton = GltfExportOptions::Skeleton::None;
			o.skeletonPath.clear();
		} else if ( v.compare( QLatin1String( "auto" ), Qt::CaseInsensitive ) == 0 ) {
			o.skeleton = GltfExportOptions::Skeleton::Auto;
			o.skeletonPath.clear();
		} else {
			o.skeleton = GltfExportOptions::Skeleton::Path;
			o.skeletonPath = v;
		}
		return 1;
	}
	if ( token == QLatin1String( "--joints" ) ) {
		QString v;
		if ( !needValue( v ) )
			return -1;
		if ( v.compare( QLatin1String( "weighted" ), Qt::CaseInsensitive ) == 0 )
			o.joints = GltfExportOptions::Joints::Weighted;
		else if ( v.compare( QLatin1String( "whole" ), Qt::CaseInsensitive ) == 0
			   || v.compare( QLatin1String( "skeleton" ), Qt::CaseInsensitive ) == 0 )
			o.joints = GltfExportOptions::Joints::Whole;
		else {
			error = QStringLiteral( "--joints takes weighted or whole, not '%1'" ).arg( v );
			return -1;
		}
		return 1;
	}
	if ( token == QLatin1String( "--units" ) ) {
		QString v;
		if ( !needValue( v ) )
			return -1;
		if ( v.compare( QLatin1String( "metres" ), Qt::CaseInsensitive ) == 0
		  || v.compare( QLatin1String( "meters" ), Qt::CaseInsensitive ) == 0 )
			o.units = GltfExportOptions::Units::Metres;
		else if ( v.compare( QLatin1String( "game" ), Qt::CaseInsensitive ) == 0
			   || v.compare( QLatin1String( "gameunits" ), Qt::CaseInsensitive ) == 0 )
			o.units = GltfExportOptions::Units::GameUnits;
		else {
			error = QStringLiteral( "--units takes metres or game, not '%1'" ).arg( v );
			return -1;
		}
		return 1;
	}
	if ( token == QLatin1String( "--part" ) ) {
		QString v;
		if ( !needValue( v ) )
			return -1;
		o.parts << v;
		return 1;
	}
	if ( token == QLatin1String( "--bones-only" ) ) {
		// kept as a word, not a bare switch, so the two values are symmetric
		QString v;
		if ( !needValue( v ) )
			return -1;
		if ( v.compare( QLatin1String( "body" ), Qt::CaseInsensitive ) == 0 )
			o.bones = GltfExportOptions::Bones::Body;
		else if ( v.compare( QLatin1String( "all" ), Qt::CaseInsensitive ) == 0 )
			o.bones = GltfExportOptions::Bones::All;
		else {
			error = QStringLiteral( "--bones-only takes body or all, not '%1'" ).arg( v );
			return -1;
		}
		return 1;
	}
	if ( token == QLatin1String( "--textures" ) ) {
		QString v;
		if ( !needValue( v ) )
			return -1;
		if ( v.compare( QLatin1String( "reference" ), Qt::CaseInsensitive ) == 0 )
			o.textures = GltfExportOptions::Textures::Reference;
		else if ( v.compare( QLatin1String( "copy" ), Qt::CaseInsensitive ) == 0 )
			o.textures = GltfExportOptions::Textures::Copy;
		else if ( v.compare( QLatin1String( "png" ), Qt::CaseInsensitive ) == 0 ) {
			if ( !gltfExportPngAvailable() ) {
				error = QStringLiteral( "--textures png: this build has no DDS decoder "
										"outside the renderer, so PNG is not offered" );
				return -1;
			}
			o.textures = GltfExportOptions::Textures::Png;
		} else {
			error = QStringLiteral( "--textures takes reference or copy, not '%1'" ).arg( v );
			return -1;
		}
		return 1;
	}
	if ( token == QLatin1String( "--data-root" ) ) {
		QString v;
		if ( !needValue( v ) )
			return -1;
		o.dataRoot = v;
		return 1;
	}
	if ( token == QLatin1String( "--root-motion-mode" ) ) {
		QString v;
		if ( !needValue( v ) )
			return -1;
		if ( v.compare( QLatin1String( "strip" ), Qt::CaseInsensitive ) == 0 )
			o.rootMotion = GltfExportOptions::RootMotion::Strip;
		else if ( v.compare( QLatin1String( "root" ), Qt::CaseInsensitive ) == 0 )
			o.rootMotion = GltfExportOptions::RootMotion::Root;
		else if ( v.compare( QLatin1String( "object" ), Qt::CaseInsensitive ) == 0 )
			o.rootMotion = GltfExportOptions::RootMotion::Object;
		else {
			error = QStringLiteral( "--root-motion-mode takes strip, root or object, not '%1'" ).arg( v );
			return -1;
		}
		return 1;
	}
	if ( token == QLatin1String( "--no-clip" ) ) {
		o.includeClip = false;
		return 1;
	}
	if ( token == QLatin1String( "--body-build" ) ) {
		QString v;
		if ( !needValue( v ) )
			return -1;
		const QStringList p = v.split( QLatin1Char( ',' ) );
		bool ok = p.size() == 3;
		float t = 0, m = 0, f = 0;
		if ( ok ) t = p[0].toFloat( &ok );
		if ( ok ) m = p[1].toFloat( &ok );
		if ( ok ) f = p[2].toFloat( &ok );
		if ( !ok ) {
			error = QStringLiteral( "--body-build takes thin,muscular,fat, not '%1'" ).arg( v );
			return -1;
		}
		o.bakeBodyBuild = true;
		o.buildThin = t;
		o.buildMuscular = m;
		o.buildFat = f;
		return 1;
	}
	if ( token == QLatin1String( "--body-build-cycle" ) ) {
		o.buildCycleClip = true;
		return 1;
	}
	if ( token == QLatin1String( "--body-build-race" ) ) {
		QString v;
		if ( !needValue( v ) )
			return -1;
		o.buildRaceEditorId = v;
		return 1;
	}
	if ( token == QLatin1String( "--body-build-gender" ) ) {
		QString v;
		if ( !needValue( v ) )
			return -1;
		if ( v.compare( QLatin1String( "male" ), Qt::CaseInsensitive ) == 0 )
			o.buildGender = 0;
		else if ( v.compare( QLatin1String( "female" ), Qt::CaseInsensitive ) == 0 )
			o.buildGender = 1;
		else {
			error = QStringLiteral( "--body-build-gender takes male or female, not '%1'" ).arg( v );
			return -1;
		}
		return 1;
	}
	return 0;
}

QString gltfExportOptionsHelp()
{
	return QStringLiteral(
		"  --legacy-defaults                     the export as it was before 2026-09-19: skeleton\n"
		"                                        none, weighted joints, metres, all bones,\n"
		"                                        referenced textures. A flag after it still wins.\n"
		"  --skeleton none|auto|<skeleton.nif>   hierarchy source (default auto; auto finding\n"
		"                                        nothing falls back to the open NIF and says so)\n"
		"  --joints weighted|whole               glTF joint list (default whole = ONE armature)\n"
		"  --units metres|game                   default game (1 glTF unit = 1 NIF unit, PyNifly);\n"
		"                                        metres = 0.0142875 m per unit\n"
		"  --part <file.nif>                     repeatable: another NIF onto the same skeleton\n"
		"  --bones-only body|all                 drop the helper rig, or keep it (default body)\n"
		"  --textures reference|copy             default copy, the .dds beside the file; reference\n"
		"                                        writes the game path and no file\n"
		"  --data-root <dir>                     where copied textures are looked for\n"
		"  --root-motion-mode strip|root|object  default strip (--root-motion is still root)\n"
		"  --no-clip                             export the mesh without the clip\n"
		"  --body-build t,m,f                    bake the build triangle into the bone scales\n"
		"  --body-build-cycle                    also write the 6 s thin->muscular->fat->thin clip\n"
		"  --body-build-race <EDID>              default HumanRace\n"
		"  --body-build-gender male|female       default male\n" );
}

QStringList gltfExportOptionsSummary( const GltfExportOptions & o )
{
	QStringList s;
	switch ( o.skeleton ) {
	case GltfExportOptions::Skeleton::None:
		s << QStringLiteral( "skeleton: none (the file's own nodes)" ); break;
	case GltfExportOptions::Skeleton::Auto:
		s << QStringLiteral( "skeleton: auto%1" )
			.arg( o.skeletonPath.isEmpty() ? QString() : QStringLiteral( " -> " ) + o.skeletonPath ); break;
	case GltfExportOptions::Skeleton::Path:
		s << QStringLiteral( "skeleton: %1" ).arg( o.skeletonPath ); break;
	}
	s << QStringLiteral( "joints: %1" ).arg( o.joints == GltfExportOptions::Joints::Whole
		? QStringLiteral( "the whole skeleton, one list" ) : QStringLiteral( "weighted bones only" ) );
	// The sentence says what ONE glTF UNIT IS, because that is the only number a
	// reader can act on. "game units" is scale 1.0 -- one glTF unit is one NIF
	// unit, which is what PyNifly reads and writes -- and "metres" is the glTF
	// specification's own, 0.9144/64 m to the NIF unit. "(1 metres per unit)"
	// was the old wording and it read like a bug.
	s << ( o.units == GltfExportOptions::Units::GameUnits
		? QStringLiteral( "units: game units -- 1 glTF unit = 1 NIF unit (scale %1, "
						  "%2 game units to the metre), PyNifly-compatible" )
			.arg( o.metresPerUnit(), 0, 'g', 9 ).arg( GLTF_UNITS_PER_METRE, 0, 'g', 9 )
		: QStringLiteral( "units: metres -- 1 glTF unit = 1 m (scale %1 m per NIF unit)" )
			.arg( o.metresPerUnit(), 0, 'g', 9 ) );
	s << QStringLiteral( "parts: %1" ).arg( o.parts.isEmpty() ? QStringLiteral( "the open file only" )
		: QStringLiteral( "%1 extra (%2)" ).arg( o.parts.size() ).arg( o.parts.join( QStringLiteral( ", " ) ) ) );
	s << QStringLiteral( "bones: %1" ).arg( o.bones == GltfExportOptions::Bones::Body
		? QStringLiteral( "body bones only" ) : QStringLiteral( "body bones and helpers" ) );
	s << QStringLiteral( "textures: %1" ).arg(
		o.textures == GltfExportOptions::Textures::Copy ? QStringLiteral( "copied beside the file" )
		: o.textures == GltfExportOptions::Textures::Png ? QStringLiteral( "converted to PNG" )
		: QStringLiteral( "referenced by their game path" ) );
	s << QStringLiteral( "root motion: %1" ).arg(
		o.rootMotion == GltfExportOptions::RootMotion::Root ? QStringLiteral( "kept on the root bone" )
		: o.rootMotion == GltfExportOptions::RootMotion::Object ? QStringLiteral( "baked onto the object" )
		: QStringLiteral( "stripped, the clip plays in place" ) );
	s << QStringLiteral( "clip: %1" ).arg( o.includeClip ? QStringLiteral( "the loaded one, if any" )
														 : QStringLiteral( "none" ) );
	if ( o.bakeBodyBuild )
		s << QStringLiteral( "body build: thin %1, muscular %2, fat %3 (%4, %5)" )
			.arg( double( o.buildThin ), 0, 'f', 3 ).arg( double( o.buildMuscular ), 0, 'f', 3 )
			.arg( double( o.buildFat ), 0, 'f', 3 )
			.arg( o.buildRaceEditorId.isEmpty() ? QStringLiteral( "HumanRace" ) : o.buildRaceEditorId )
			.arg( o.buildGender == 1 ? QStringLiteral( "female" ) : QStringLiteral( "male" ) );
	if ( o.buildCycleClip )
		s << QStringLiteral( "body build cycle: written as a 6 s glTF animation "
							 "(NOT a game-valid .hkx -- skeleton.hkx has no _skin bones)" );
	return s;
}

QStringList gltfExportOptionRecommendation()
{
	/* THE RULING, 2026-09-19 09:45, as `option | what it does | pre-ruling |
	 * ruled`. Nothing reads this to choose a default -- the defaults are the
	 * initialisers in gltfexportopts.h and nothing else -- it is here so that a
	 * reader of the code sees what was asked for and what was answered, and so
	 * the two cannot drift without one of them looking wrong.
	 *
	 * On units he chose GAME over this lane's metres recommendation. That is
	 * written down because a recommendation left lying in a tree gets mistaken
	 * for a decision. */
	return QStringList()
		<< QStringLiteral( "skeleton | where the bone hierarchy comes from | none | auto (RULED)" )
		<< QStringLiteral( "joints | what goes in the glTF joint list | weighted | whole (RULED)" )
		<< QStringLiteral( "units | metres or Fallout 4 units | metres | game (RULED, over the metres recommendation)" )
		<< QStringLiteral( "parts | other body NIFs into the same file | none | none, unruled, per export" )
		<< QStringLiteral( "bones | keep the camera/weapon helper rig | all | body (RULED)" )
		<< QStringLiteral( "textures | copy the .dds beside the file | reference | copy (RULED)" )
		<< QStringLiteral( "root motion | where the travel goes | strip | strip, unruled, unchanged" )
		<< QStringLiteral( "clip | carry the clip playing in the viewer | never could | on (RULED; it is also the repair)" )
		<< QStringLiteral( "the way back | every ruled row at its pre-ruling value | - | --legacy-defaults, CLI only" );
}
