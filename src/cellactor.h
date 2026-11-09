#ifndef CELLACTOR_H
#define CELLACTOR_H

/* Placed actors in the cell view: each one is built from its race skeleton, skin and outfit addons and face mesh,
 * posed (ragdoll data or a death pose when dead on start, else frame 0 of the race's idle) and skinned once on the
 * CPU into welded shapes, since the cell view's shadow pass does not skin. Interiors only. */

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

	//! MODULES1 item 16: the palette row of this actor's hair shape `shapeName` (its NPC_ HCLF), false when none.
	bool hairPaint( const QString & key, const QString & shapeName, float * row ) const;

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
