#ifndef CELLACTOR_H
#define CELLACTOR_H

/* lane PLACED1: PLACED ACTORS in the cell view.
 *
 * A cell places an actor as its own reference record type; the cell view read
 * only the ordinary references, so no actor, living or dead, was in it. This
 * reads them and builds each one AT REST: the race's skeleton, the skin's
 * armor addons, the outfit's armor addons, the pre-built face mesh, all posed
 * on the skeleton's own bind pose and handed to the cell view as ordinary
 * welded shapes at the placed transform. No animation, no ragdoll.
 *
 * WHY ON THE CPU, NOT AS RIGS. The cell view is one welded document. Its
 * shadow pass does not skin, and merging a rig per actor rescans the whole
 * document per merge. So each part is skinned once here, with the same rule
 * the character merge uses: A BONE THE SKELETON NAMES TAKES THE SKELETON'S
 * REST TRANSFORM; a bone only the part has keeps the part's own.
 *   v' = sum_k w_k * ( W_k * B_k ) * v      W_k the bone's rest transform in
 *   actor space, B_k the part's skin-to-bone transform for that bone.
 *
 * THE CHAIN (published record layouts, the xEdit definitions):
 *   looks   = the actor record its Traits come from (template chain; a leveled
 *             list on the way is a dice roll unless it holds exactly one entry
 *             that always spawns)
 *   race    -> skeleton by sex, skin, height by sex, "has a face mesh" flag
 *   skin    = the actor's own, else the race's; each of its addons made for
 *             the race is drawn UNLESS the outfit wears one of its body slots
 *   outfit  = the default outfit of the record the Inventory comes from; each
 *             armor's addons made for the race; a leveled item is taken when it
 *             is not a dice roll (use-all, or one entry), else left out, counted
 *   head    = the pre-built face mesh of the looks record, when the race has
 *             the flag and the file exists; its gore caps are never drawn, its
 *             hair not under headgear
 *   scale   = the reference's scale * the race's height for the sex * the
 *             actor's own height
 *
 * THE POSE (lane ACTORPOSE1). The skeleton's bind pose is arms out: every
 * actor drawn on it stood in a T pose. Now the skeleton is posed first:
 *   dead on start, with ragdoll data (the reference's XRGD: one entry per
 *             ragdoll body, in a depth-first walk of skeleton.hkx's ragdoll
 *             skeleton; each body is the animation bone at its bind position;
 *             the rotation is the bone's local one, in the reference rotation's
 *             own convention, entry 0 places the root): lying where the game
 *             left its ragdoll. Refused, by name, when the entries' lengths do
 *             not fit the skeleton's bones, or when 4 or more bodies sit at
 *             their bind rotation (a ragdoll the Creation Kit never settled).
 *   dead on start otherwise: frame 0 of the race's death pose
 *             (animations\deathpose1.hkx), flat on the reference's heading, its
 *             tilt taken out: the game drops such a body as a ragdoll.
 *   otherwise  frame 0 of the race's idle clip, the first that exists of
 *             <actor folder>\animations: mt\neutral\posea_idle1.hkx (human),
 *             idle.hkx, idle_mt.hkx, mt\idle.hkx, <folder>_idle.hkx; tracks
 *             named through <actor folder>\characterassets\skeleton.hkx.
 *   neither    the bind pose, counted with its reason.
 * A bone only a part has (hair cloth bones) rides its nearest ancestor the
 * skeleton names, by that ancestor's move from the part's own node.
 *
 * NAMED DIFFERENCES from the game: no package idle (furniture, a leaning
 * marker: the actor stands in its race's idle instead), frame 0 of the idle,
 * not a moment of it; a dead-on-start actor without usable ragdoll data lies
 * in one fixed death pose (the game simulates its fall); no body
 * weight morph, no face tint, no hair color, no weapons; a creature's gore
 * caps are drawn (they sit inside the whole body); an actor with a height
 * range stands at the middle of it (the game rolls it); interiors only.
 * Actors are not REFRs: they stay out of the reference list, the REFR counts
 * and the placement dump, and have their own census line and dump. */

#include "esmdata.h"
#include "nativeemit.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <vector>

enum class CellActorFate
{
	Drawn = 0,
	Hidden,         //!< deleted, without a base, or initially disabled: never asked for
	LeveledList,    //!< its looks come through a leveled list: a dice roll in game
	NotAnActor,     //!< the base is no actor record
	NoRace,         //!< the actor names no race record
	NoSkeleton,     //!< the race's skeleton file does not load
	NoBodyModel,    //!< no addon of the skin carries a model (a robot built from parts)
	NoGeometry,     //!< models were named and none gave a triangle
	RedOff,         //!< a debug switch turned actors off
	Count
};

//! lane ACTORPOSE1: the pose a drawn actor stands (or lies) in
enum class CellActorPose
{
	Bind = 0,       //!< the skeleton's bind pose (arms out): no idle clip, or the red control
	Idle,           //!< frame 0 of the race's idle clip
	Ragdoll,        //!< dead on start: the reference's own ragdoll data
	Death,          //!< dead on start, ragdoll data none, refused or never settled: the race's death pose
	Count
};

struct CellActorRow
{
	quint32 form = 0, base = 0;
	CellActorFate fate = CellActorFate::Hidden;
	CellActorPose pose = CellActorPose::Bind;
	QString poseWhy;            //!< the clip read, or why the pose is not the one asked for
	QByteArray ragdoll;         //!< the reference's XRGD (dead-on-start actors)
	bool dead = false;          //!< starts dead
	quint32 looks = 0;          //!< the actor record the Traits come from
	QString raceEdid;
	bool female = false;
	float scale = 1.0f;
	float pos[3] = { 0.0f, 0.0f, 0.0f };
	float rot[3] = { 0.0f, 0.0f, 0.0f };
	QString skeleton;
	QStringList parts;          //!< models drawn, in order: skin, outfit, face mesh
	QStringList hidden;         //!< skin models the outfit hides
	QString faceMesh;           //!< empty when none is drawn
	bool headMissing = false;   //!< the race wants a face mesh and no file exists
	bool outfitUnknown = false; //!< the Inventory comes through a dice roll: drawn in its skin
	int outfitDice = 0;         //!< outfit entries left out because they are a dice roll
	QString key;                //!< what the cell view asks shapes() for
};

class CellActors
{
public:
	CellActors( const EsmWorld & world, const QString & dataRoot );
	~CellActors();

	/*! The open interior's placed actors as references (empty in a worldspace).
	 *  Every one gets a row; a row keeps the fate Hidden until place() is asked. */
	QVector<EsmRefr> references();
	/*! lane GAPLOOK2: the placed actors of these cell groups (an exterior cell's, the persistent cell's);
	 *  `box` (minX, minY, maxX, maxY, game units) keeps only the actors standing inside it. */
	QVector<EsmRefr> referencesIn( const QVector<quint32> & groups, const float * box = nullptr );

/*! Resolve and build one placed actor. True when drawn (`key` for shapes(), `scale` the whole
 *  scale); false when not shown or refused, counted either way. */
	bool place( const EsmRefr & r, bool hidden, QString & key, float & scale );

	//! The built actor, actor space, posed (lane ACTORPOSE1).
	bool shapes( const QString & key, std::vector<NativeSrcShape> * out ) const;

	/*! `  placed actors: R read, drawn D (...), not shown H, refused F: <reason> <count>, ...`.
	 *  Every placed actor is in exactly one of drawn, not shown or a named refusal. */
	QString censusLine() const;
//! One line per placed actor, tab separated.
	void dump( const QString & path ) const;

	static QString fateName( CellActorFate f );

private:
	struct Impl;
	Impl * d;
	QHash<quint32, int> rowOf;
};

#endif // CELLACTOR_H
