#ifndef CELLDECAL_H
#define CELLDECAL_H

/* Placed decals in the cell view: each decal's oriented box (projection local +Y, width +X, height -Z)
 * clips the welded opaque triangles on the CPU into blended decal buckets. A decal whose size or sheet
 * quarter is a random roll in game is refused, not guessed. */

#include <QString>
#include <QVector>
#include <vector>

class EsmWorld;

struct CellDecalRef
{
	quint32 form = 0;
	quint32 base = 0;
	float pos[3] = { 0.0f, 0.0f, 0.0f };
	float rot[3] = { 0.0f, 0.0f, 0.0f };
	int refRow = -1;
};

//! One welded bucket offered as a receiver; positions are (world - origin).
struct CellDecalReceiver
{
	const float * pos = nullptr;    //!< first vertex position, 3 floats
	const float * nrm = nullptr;    //!< first vertex normal, 3 floats
	size_t stride = 0;              //!< bytes from one vertex to the next
	size_t numVerts = 0;
	const quint32 * tris = nullptr; //!< 3 indices per triangle
	size_t numTris = 0;
};

struct CellDecalVert
{
	float pos[3];
	float nrm[3];
	float tan[3];   //!< along +v
	float bit[3];   //!< along +u
	float uv[2];
	float alpha;    //!< the angle fade
};

//! The decals of one texture set, welded.
struct CellDecalMesh
{
	QString name;
	QString diffuse, normal, spec;
	std::vector<CellDecalVert> verts;
	std::vector<quint32> tris;
	int decals = 0;
};

enum class CellDecalFate
{
	Drawn = 0,
	NoDecalData,    //!< the texture set carries no decal data: not a decal
	DiceRoll,       //!< its size or its quarter of the sheet is a random roll in game
	NotABox,        //!< a primitive that is not a box
	RayMissed,      //!< no surface within 1000 units along its axis
	NoSurface,      //!< nothing opaque inside its box at an angle it takes
	NoTexture,      //!< no diffuse texture could be named
	RedOff          //!< a debug switch turned decals off
};

struct CellDecalResult
{
	std::vector<CellDecalMesh> meshes;
	std::vector<CellDecalFate> fates;   //!< one per reference, same order
	int count[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };   //!< by fate
	qint64 triangles = 0;
	qint64 receiverTris = 0;
	int withPrimitive = 0, byRay = 0;
	QString red;                        //!< the WW_CELL_DECAL_RED debug mode in force, empty when none
};

/*! Project every decal reference onto the receivers. `origin` is what the receivers' positions were
 *  shifted by; `dumpPath`, when not empty, takes one line per reference. */
void cellProjectDecals( const EsmWorld & world, const QString & dataRoot,
	const std::vector<CellDecalRef> & refs, const std::vector<CellDecalReceiver> & receivers,
	const float origin[3], CellDecalResult & out, const QString & dumpPath );

//! The census name of a fate.
QString cellDecalFateName( CellDecalFate f );

/*! The census line: `  placed decals: N read, drawn D (...); refused R: <reason> <count>, ...`.
 *  Every reference with decal data is in exactly one of drawn or a named refusal. */
QString cellDecalCensusLine( const CellDecalResult & r );

#endif // CELLDECAL_H
