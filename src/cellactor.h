#ifndef CELLACTOR_H
#define CELLACTOR_H

/* Placed actors in the cell view: each one is built at rest (race skeleton, skin and outfit
 * addons, face mesh) and skinned once on the CPU into welded shapes, since the cell view's
 * shadow pass does not skin. A bone the skeleton names takes the skeleton's rest transform. */

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

/*! Resolve and build one placed actor. True when drawn (`key` for shapes(), `scale` the whole
 *  scale); false when not shown or refused, counted either way. */
	bool place( const EsmRefr & r, bool hidden, QString & key, float & scale );

	//! The built actor, actor space, bind pose.
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
