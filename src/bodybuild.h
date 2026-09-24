/* The Fallout 4 character-creator BODY BUILD triangle: the game's own numbers,
   and the game's own arithmetic for combining them.
   Lane GLTFEXPORT1, 2026-09-19. bungo, 2026-09-19 05:0x: "Fallout 4 has 'skin'
   bones, those are not used for animations, but only for the character's
   build? ... could you build a hkx animation that basically is the build
   slider ... thin to muscular to fat to thin".

   WHERE THE DATA IS. RACE record, subrecord group "Bone Scale Data". Layout
   from xEdit's wbDefinitionsFO4.pas (wbBoneDataItem, ~line 5534), not guessed:

     BSMP  u32      Weight Scale Target Gender   (0 Male, 1 Female)
     then repeated:  BSMB zstring name, BSMS 36 B = thin(xyz) musc(xyz) fat(xyz)
     BMMP  u32      Range Modifier Target Gender
     then repeated:  BSMB zstring name, BSMS 16 B = minY minZ maxY maxZ

   HumanRace (00013746) carries two sets, 48 bones and 48 range modifiers each.
   Every X is exactly 1.0 in all three corners of both genders: the triangle
   changes a bone's CROSS SECTION, never its length, which is why a character
   gets thicker and not taller. Cross-checked against the independent Python
   dump E:/Projects/Claude/tools/body_build_anim/race_bone_data.json (48/48).

   Bethesda also ships the same table as loose JSON beside the meshes --
   Meshes/Actors/Character/CharacterAssets/HumanRaceBoneScales{Male,Female}
   {Thin,Muscular,Fat,Default}.txt -- which is what TESRace::
   ImportBodyMorphBoneBaseScales reads at CK time. The DEFAULT file's Scale is
   exactly (1,1,1) for all 48 bones, and that is the fact the combination
   formula below has to reproduce at the centre of the triangle.

   ALL OF THIS IS A PREVIEW. Nothing here writes a file. And the cycle clip it
   can export is NOT a game-valid .hkx: skeleton.hkx (95 bones) contains no
   *_skin bone at all -- they live only in skeleton.nif -- so no correct FO4
   Havok clip of this animation can exist. Never offer it as one. */

#ifndef BODYBUILD_H
#define BODYBUILD_H

#include "data/niftypes.h"

#include <QString>
#include <QStringList>
#include <QVector>

//! One bone's three corners. Absolute per-axis multipliers, 1.0 = unmodified.
struct BodyBuildBone
{
	QString name;
	Vector3 thin = Vector3( 1, 1, 1 );
	Vector3 muscular = Vector3( 1, 1, 1 );
	Vector3 fat = Vector3( 1, 1, 1 );
	//! True when all three corners are (1,1,1) -- the game stores nothing for
	//! such a bone either (Fallout4.exe 1.10.155 @ 0x5b1cd9 skips them).
	bool neutral() const;
};

//! BMMP. NOT a clamp: the engine ADDS these, scaled by the body-region slider.
struct BodyBuildRangeMod
{
	QString name;
	float minY = 0, minZ = 0, maxY = 0, maxZ = 0;
};

struct BodyBuildSet
{
	int gender = 0;                       //!< 0 male, 1 female
	QVector<BodyBuildBone> bones;
	QVector<BodyBuildRangeMod> rangeMods;
	const BodyBuildBone * find( const QString & boneName ) const;
};

struct BodyBuildTable
{
	quint32 formID = 0;
	QString editorId;
	QVector<BodyBuildSet> sets;
	const BodyBuildSet * set( int gender ) const;
	bool isEmpty() const { return sets.isEmpty(); }
};

//! HumanRace.
constexpr quint32 BODYBUILD_HUMAN_RACE = 0x00013746u;

/*! Read one RACE's Bone Scale Data out of an .esm/.esp.
 *  `error` carries a sentence on false; a RACE that simply has no bone scale
 *  data is NOT an error -- the table comes back empty and says so in `error`
 *  as a refusal sentence the panel shows. */
bool bodyBuildLoadRace( const QString & esmPath, quint32 formID,
						BodyBuildTable & out, QString & error );

//! Every RACE in the file that carries bone scale data: formID -> EDID, for
//! the panel's race list. bungo asked for any RACE to be selectable.
bool bodyBuildListRaces( const QString & esmPath,
						 QVector<QPair<quint32, QString>> & out, QString & error );

/*! ---------------------------------------------------------------------------
 *  THE COMBINATION, MEASURED -- Fallout4.exe 1.10.155 (Todd's treat).
 *
 *
 *  TESNPC::FillBoneScaleMap @ RVA 0x5b18f0, its per-bone lambda @ 0x5c24e0:
 *
 *      s = w.x*thin + w.y*muscular + w.z*fat  -  k * ( (thin+musc+fat)/3 - 1 )
 *
 *  quoted from 0x5c2597..0x5c264c:
 *      movss xmm0,[rip+0x26f8e7d]  ; 0.333333343
 *      mulss xmm10,xmm0            ; (V0.x+V1.x+V2.x)/3
 *      subss xmm10,[rip+0x328621a] ; NiPoint3::UNIT_ALL  -> mean - 1
 *      mulss xmm13,xmm0 / mulss xmm7,xmm1 / mulss xmm4,xmm0   ; the three w*V
 *      mulss xmm10,xmm0            ; k * (mean - 1)
 *      subss xmm13,xmm10           ; the correction
 *
 *  The weights are BARYCENTRIC and sum to 1 (TESNPC::ConvertFloatToMorphWeight
 *  @ 0x5bac60 maps one 0..100 slider to a two-corner lerp (1-t,0,t) or
 *  (0,t,1-t); kDefaultMorphWeight @ 0x3715478 is (1/3,1/3,1/3)). Because they
 *  sum to 1, "weighted sum of the corners" and "1 + sum of the corners'
 *  deviations from 1" are the SAME expression -- the two candidates the brief
 *  named are algebraically identical, and the corpus below refutes BOTH: at
 *  the centre they give the mean of the three corners, and the game gives 1.
 *
 *  k is not a weight. The triangle is the unit equilateral one
 *  P0=(0,h) P1=(0.5,0) P2=(1,h), h = sqrt(3)/2, and
 *      k = ( R - |P - centroid| ) / R ,   P = w.x*P0 + w.y*P1 + w.z*P2
 *  with R = 1/sqrt(3), the CIRCUMRADIUS -- the centroid-to-corner distance --
 *  so k = 1 at the centre and 0 at every corner.
 *
 *  ---- WHAT IS PROVEN, AND BY WHAT ---------------------------------------
 *
 *  Not by the disassembly. Bethesda ships the game's own answer as loose JSON:
 *  HumanRaceBoneScales{Male,Female}{Thin,Muscular,Fat,Default}.txt beside the
 *  meshes, each a list of {Name, Position, Rotation, Scale} -- which is what
 *  TESRace::ImportBodyMorphBoneBaseScales reads at CK time to FILL the RACE
 *  record this header parses. Evaluating the expression above at the four
 *  states and comparing, bone for bone and axis for axis, against those eight
 *  files (scratchpad/gltfexport1_20260919/body_build_table.py and
 *  corner_control.py, run 2026-09-19):
 *
 *      male   thin      k=0  48 bones   max |diff|  Y 1.11e-16  Z 1.11e-16
 *      male   muscular  k=0  48 bones               Y 0         Z 2.22e-16
 *      male   fat       k=0  48 bones               Y 0         Z 0
 *      male   Default   k=1  48 bones               Y 2.22e-16  Z 2.22e-16
 *      female thin      k=0  48 bones               Y 1.11e-16  Z 1.11e-16
 *      female muscular  k=0  48 bones               Y 0         Z 0
 *      female fat       k=0  48 bones               Y 0         Z 0
 *      female Default   k=1  48 bones               Y 2.22e-16  Z 2.22e-16
 *
 *  That is double epsilon: at a corner the answer IS the raw stored corner
 *  vector, and at the centre it is exactly 1. The FLOOR that shows the
 *  comparison can fail is the cross pairing -- the male raw fat vector against
 *  the FEMALE fat file is off by 0.744 on RButtFat_skin.
 *
 *  X IS A KNOWN DIVERGENCE, and it is in the data, not the arithmetic. The
 *  RACE record stores X = 1.0 for all 48 bones of both genders, while the
 *  shipped .txt carries an authored X on about ten of them (RBreast_skin
 *  1.0278; female fat LArm_Collarbone_skin 1.1840). The .txt is the CK-time
 *  INPUT and the RACE record is what it was imported into, so the import drops
 *  X; the engine reads the RACE record, and so does this file. Y and Z -- the
 *  cross section, which is the whole of what the build slider changes -- are
 *  exact.
 *
 *  ---- WHAT IS NOT PROVEN -------------------------------------------------
 *
 *  UNPROVEN: the SHAPE of k between the centre and a corner. The corpus pins
 *  k at both ends and says nothing in between, and any function that is 1 at
 *  the centre and 0 at the corners fits it. The one above is the disassembly's
 *  reading; a rival that fits the same two ends exactly as well is
 *  k = 3 * min(w.x, w.y, w.z). The two separate immediately off the corners:
 *  at the midpoint of an edge, w = (0.5, 0, 0.5), this file says k = 0.5 and
 *  the rival says k = 0.
 *
 *  THE REFUTER, and it is cheap: one character built at a half-way slider
 *  position, its Belly_skin scale read in game (or a .txt exported from the CK
 *  at that position), against this file's number for the same weights. Until
 *  someone does that, the panel is exact at the three corners and at the
 *  centre and is an interpolation everywhere else, and it says so.
 *  ------------------------------------------------------------------------ */

//! The k of the formula above, from barycentric weights. Clamped to [0,1].
float bodyBuildCentroidK( float wThin, float wMuscular, float wFat );

//! The final per-axis scale for one bone. Weights are normalised first; a
//! zero-sum triple is the centroid, the game's own default.
Vector3 bodyBuildScale( const BodyBuildBone & b, float wThin, float wMuscular, float wFat );

//! The corner weights that put the point exactly at a named corner / centre.
//! 0 thin, 1 muscular, 2 fat, 3 centroid.
void bodyBuildCornerWeights( int which, float & wThin, float & wMuscular, float & wFat );

//! The bone names the panel and the exporter touch: every bone in the set.
QStringList bodyBuildBoneNames( const BodyBuildSet & s );

//! The director's 6-second cycle, as key times and the corner at each key:
//! 0 s THIN, 2 s MUSCULAR, 4 s FAT, 6 s THIN. Linear. Absorbs
//! E:/Projects/Claude/tools/body_build_anim/build_build_cycle.py.
struct BodyBuildCycleKey { float time; int corner; };
QVector<BodyBuildCycleKey> bodyBuildCycleKeys();

#endif // BODYBUILD_H
