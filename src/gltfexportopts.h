/* The ONE options struct behind File > Export > glTF and the `gltf` CLI verb.
   Lane GLTFEXPORT1, 2026-09-19.

   WHY IT IS ITS OWN FILE. bungo asked for "a few toggles in there before the
   export" (2026-09-19 04:xx). A dialog and a command line that each grew their
   own booleans would drift the first time one of them gained an option, so the
   dialog writes this struct, the CLI parses into this struct, and
   gltfExportNifWriteOpts() is the only thing either of them calls. Nothing in
   this header knows what a NifModel or a QWidget is -- QtCore only, so the
   gate links it without NifSkope.exe (skill ww-standalone-writer-gate).

   DEFAULTS: BLENDER-READY, RULED BY BUNGO 2026-09-19 09:45. Until that hour
   every field shipped at the value that reproduced the old export byte for
   byte, because an owed ruling never ships as a default (CONSTITUTION 7). He
   then ruled: skeleton AUTO, joints WHOLE, units GAME, animation ON, bones
   BODY only, textures COPY. Parts, root motion, and both body-build fields
   were not part of the ruling and are untouched.

   HE ALSO CHOSE GAME UNITS over this lane's metres recommendation. That is
   recorded here because the recommendation is in the tree and a later reader
   must not mistake it for the ruling.

   THE WAY BACK IS `--legacy-defaults` (CLI only, by his instruction: no
   dialog row). It assigns gltfExportLegacyOptions(), so the old bytes are one
   flag away and the byte-identity gate row still has something to prove.
   `gltfExportOptionsAreLegacy()` is the predicate both use, and
   gltfExportNifWriteOpts() still takes the OLD code path unchanged when it
   holds. The loaded clip (finding 1) is a DEFECT REPAIR and not an option, so
   it is in neither -- see gltfexportnif.h.

   The recommendations, which are commentary and never the defaults, are in
   gltfExportOptionRecommendation(). */

#ifndef GLTFEXPORTOPTS_H
#define GLTFEXPORTOPTS_H

#include <QString>
#include <QStringList>

//! metres per NIF unit: 0.9144 / 64, one yard per 64 units. The same constant
//! as GltfExportScene::unitScale (src/gltfexport.h:114) and
//! src/lib/importex/gltf.cpp. Its reciprocal 69.99125... is what PyNifly calls
//! "game units" and what E:/Projects/Claude/tools/gltf_units/gltf_to_game_units.py
//! multiplied by after the fact.
constexpr double GLTF_METRES_PER_UNIT = 0.9144 / 64.0;
constexpr double GLTF_UNITS_PER_METRE = 64.0 / 0.9144;   // 69.991251...

struct GltfExportOptions
{
	/*! (2) Where the node HIERARCHY comes from.
	 *
	 *  None    -- the open NIF alone. A body NIF carries a FLAT list of its
	 *             *_skin bones and nothing else, which is why a clip drove 13
	 *             of 95 tracks (bungo's Blender session, 2026-09-19 03:xx).
	 *  Auto    -- look for skeleton.nif beside the NIF and in
	 *             Meshes/Actors/Character/CharacterAssets under the data root.
	 *  Path    -- `skeletonPath`.
	 */
	enum class Skeleton { None, Auto, Path };
	Skeleton skeleton = Skeleton::Auto;          //!< RULED 2026-09-19 09:45
	QString skeletonPath;

	/*! (3) What goes in the glTF `skins[].joints` list.
	 *
	 *  Weighted -- only the bones the skin actually weights. Blender then makes
	 *              one armature per skin and hangs every unweighted bone in the
	 *              file beside it as a loose EMPTY (130 of them on skeleton.nif).
	 *  Whole    -- every bone node of the character, one joint list shared by
	 *              every skin, so Blender builds exactly ONE armature and no
	 *              empties. Inverse binds for a bone that carries no weight are
	 *              derived from its world matrix (the arithmetic
	 *              gltf_unify_skin.py did after the fact).
	 */
	enum class Joints { Weighted, Whole };
	Joints joints = Joints::Whole;               //!< RULED 2026-09-19 09:45

	//! (4) metres (today) or FO4 game units. PyNifly keeps game units.
	enum class Units { Metres, GameUnits };
	Units units = Units::GameUnits;              //!< RULED 2026-09-19 09:45

	/*! (5) Extra NIFs exported onto the SAME skeleton and into the same file:
	 *  hands, head, rear head, eyes, mouth. One glTF skin per part, every skin
	 *  sharing one joint list. Empty = the open NIF alone, which is today. */
	QStringList parts;

	/*! (6) bungo's own toggle. `Body` keeps only the bones a character's mesh
	 *  can be driven by; `All` keeps the helper rig too (Camera, Camera
	 *  Control, CamTarget, CamTargetParent, CharacterBumper, AnimObject*,
	 *  Weapon* ...). See gltfExportIsHelperBone() for the exact rule.
	 *  A helper bone that a skin WEIGHTS is never dropped -- dropping it would
	 *  break the mesh -- and the report names any that were kept for that. */
	enum class Bones { All, Body };
	Bones bones = Bones::Body;                   //!< RULED 2026-09-19 09:45

	/*! (7) Textures. Reference = write the game-relative .dds path into the
	 *  image uri and write no file (today). Copy = copy the .dds next to the
	 *  .gltf and make the uri relative to it. Png = only offered when a DDS
	 *  decoder is reachable from this translation unit; see
	 *  gltfExportPngAvailable(). */
	enum class Textures { Reference, Copy, Png };
	Textures textures = Textures::Copy;          //!< RULED 2026-09-19 09:45
	//! Where copied textures are looked for when the NIF path does not resolve.
	QString dataRoot;

	/*! (8) Root motion (hkaDefaultAnimatedReferenceFrame).
	 *
	 *  Strip  -- not written as channels, recorded in the animation's extras,
	 *            the clip plays in place. TODAY (gltfanim.cpp passed false).
	 *  Root   -- composed onto the root BONE's channels (today's --root-motion).
	 *  Object -- written onto the mesh/armature OBJECT node instead, so in
	 *            Blender the armature object travels and the bones do not.
	 */
	enum class RootMotion { Strip, Root, Object };
	RootMotion rootMotion = RootMotion::Strip;   //!< TODAY

	/*! (1) The loaded clip. This is NOT a toggle over old behaviour -- the menu
	 *  export could never carry a clip at all (gltfExportSetClipProvider had no
	 *  caller). It is on, and off only so a user can export the mesh alone. */
	bool includeClip = true;

	// ---- Part 2, the body build triangle ---------------------------------
	/*! Bake the current Body Build panel position into the exported bone
	 *  SCALES (static). Off unless the panel asks for it. */
	bool bakeBodyBuild = false;
	float buildThin = 0.0f, buildMuscular = 0.0f, buildFat = 0.0f;
	/*! Also write the 6-second THIN -> MUSCULAR -> FAT -> THIN cycle as a glTF
	 *  animation (absorbs build_build_cycle.py). Off by default. NEVER offered
	 *  as an .hkx: skeleton.hkx has none of the *_skin bones, so no game-valid
	 *  Havok clip of this animation can exist. */
	bool buildCycleClip = false;
	QString buildRaceEditorId;                   //!< "" = HumanRace (00013746)
	int buildGender = 0;                         //!< 0 male, 1 female

	//! metres per unit for the chosen unit mode. GameUnits writes 1.0, i.e.
	//! the .gltf carries raw NIF units, which is what PyNifly reads.
	double metresPerUnit() const
	{ return units == Units::GameUnits ? 1.0 : GLTF_METRES_PER_UNIT; }
};

/*! True when every field still holds the value that reproduces the export of
 *  2026-09-19 (before the 09:45 ruling) exactly. The byte-identity gate row
 *  asserts this of a `--legacy-defaults` command line, and
 *  gltfExportNifWriteOpts() takes the OLD code path unchanged when it is true,
 *  so the old bytes stay reachable and stay proven.
 *  includeClip is not part of the predicate: it is the defect repair. */
bool gltfExportOptionsAreLegacy( const GltfExportOptions & o );

/*! The pre-ruling values, as an options struct. `--legacy-defaults` assigns
 *  this over the defaults, which is why the flag must be parsed BEFORE the
 *  other flags on the same command line -- see gltfExportParseFlag(). Fields
 *  the ruling did not touch (parts, root motion, the body build) are left at
 *  their normal defaults here too, so the two structs differ in exactly the
 *  six ruled fields and nothing else. */
GltfExportOptions gltfExportLegacyOptions();

/*! The helper-bone rule for option (6), as one predicate so the dialog, the
 *  CLI, the report and the gate cannot disagree about it.
 *
 *  A bone is a HELPER when its name, case-folded, is one of
 *    "camera", "camera control", "camtarget", "camtargetparent",
 *    "charbumper", "characterbumper"
 *  or begins with one of
 *    "animobject", "weapon", "cam", "prop", "loot", "ladder", "bumper".
 *  Everything else is a body bone, INCLUDING the *_skin bones (they are what
 *  the build triangle scales) and the face bones.
 *
 *  MEASURED on the shipped human skeleton
 *  (E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Actors/Character/
 *  CharacterAssets/skeleton.nif, BSVersion 130, 166 blocks), by this predicate,
 *  in scratchpad/gltfexport1_20260919/count_helpers.py:
 *      129 NiNode, 15 helpers, 114 kept with "body bones only", 48 *_skin.
 *  The 15: WeaponLeft, WEAPON, AnimObjectL1..L3, AnimObjectR1..R3,
 *  AnimObjectA, AnimObjectB, Camera, Camera Control, CamTarget,
 *  CamTargetParent, CharacterBumper.
 *
 *  "skeleton.nif" is NOT in the list, although an earlier draft of this rule
 *  had it: that string is the name of the ROOT NiNode of the shipped file
 *  (block 0, measured in the same run), so calling it a helper would have
 *  proposed dropping the whole hierarchy. dropHelpers() keeps any helper with
 *  a kept descendant and so would have survived it, but a rule that is only
 *  saved by a later rule is a rule that is wrong.
 *
 *  The rule is by NAME because a helper carries no other mark: it is an
 *  ordinary NiNode in skeleton.nif. The refuter is a name that ought to be
 *  kept and begins with one of those prefixes -- "Weapon" is the one real
 *  risk, and a weighted helper is kept anyway. */
bool gltfExportIsHelperBone( const QString & name );

//! True when a PNG path exists in this build; today it is false and the
//! dialog leaves the Png row out rather than offering a dead choice.
bool gltfExportPngAvailable();

/*! Fills `o` from one CLI token. Returns:
 *   1  consumed the token and, when `takesValue`, the value that followed;
 *   0  not one of ours;
 *  -1  ours but the value is wrong -- `error` names the flag and the value.
 *  `next` is a callable-free convenience: the caller passes the following
 *  token (or a null QString when there is none). */
int gltfExportParseFlag( const QString & token, const QString & next,
						 GltfExportOptions & o, bool & usedNext, QString & error );

//! The `--help` block for the `gltf` verb, exactly the options above.
QString gltfExportOptionsHelp();

//! One line per option: what it is set to, in words, for the export summary
//! and for the report. Every line is also what the gate greps for.
QStringList gltfExportOptionsSummary( const GltfExportOptions & o );

//! This lane's RECOMMENDED value per option, in words, for the ROWS FOR BUNGO
//! table. Advisory only: nothing reads it to choose a default.
QStringList gltfExportOptionRecommendation();

#endif // GLTFEXPORTOPTS_H
