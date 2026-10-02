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
 * NAMED DIFFERENCES from the game: bind pose, no idle; dead-on-start actors
 * stand where they were placed (the game drops them as ragdolls); no body
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
	RedOff,         //!< the red control switched actors off
	Count
};

struct CellActorRow
{
	quint32 form = 0, base = 0;
	CellActorFate fate = CellActorFate::Hidden;
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

	/*! Resolve and build one placed actor. True when it is drawn: `key` is what
	 *  shapes() answers to and `scale` the whole scale to place it with. False
	 *  = not shown (deleted, no base, initially disabled) or a named refusal,
	 *  counted either way. */
	bool place( const EsmRefr & r, bool showDisabled, QString & key, float & scale );

	//! The built actor, actor space, bind pose.
	bool shapes( const QString & key, std::vector<NativeSrcShape> * out ) const;

	/*! `  placed actors: R read, drawn D (...), not shown H, refused F: <reason> <count>, ...`.
	 *  Every placed actor is in exactly one of drawn, not shown or a named refusal. */
	QString censusLine() const;
	//! One line per placed actor, tab separated (tests/spells/cell_actor_check.py reads it).
	void dump( const QString & path ) const;

	static QString fateName( CellActorFate f );

private:
	struct Impl;
	Impl * d;
	QHash<quint32, int> rowOf;
};

#endif // CELLACTOR_H
