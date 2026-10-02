#ifndef CELLDECAL_H
#define CELLDECAL_H

/* lane PLACED1: PLACED DECALS in the cell view.
 *
 * A cell places a decal as a reference whose base is a texture set carrying
 * decal data. The game draws each one as an oriented box, late, onto whatever
 * opaque surface lies inside the box, and lights the result like that surface.
 * This view welds its scene on the CPU, so it does the same thing on the CPU:
 * the welded opaque triangles are clipped to each decal's box and the clipped
 * pieces are welded into decal buckets that the lit program draws as an
 * ordinary blended surface.
 *
 * THE BOX (the placed reference's frame; world = R * local, R the cell view's
 * placement rotation):
 *   projection axis = local +Y, width axis = local +X, height axis = local -Z.
 *   With a box primitive on the reference: centre = the reference position,
 *     width = 2 * bounds.x, height = 2 * bounds.z, depth = 2 * bounds.y (the
 *     stored bounds are half extents); the base's sizes are not used.
 *   Without one: a ray from the reference position along +Y, 1000 units; no
 *     hit, no decal; the box is centred on the hit, width and height are the
 *     base's (min..max) times the reference's own scale pair, depth is the
 *     base's depth.
 *   u = dot(X, P - C) / width + 0.5, v = dot(-Z, P - C) / height + 0.5,
 *   t = dot(Y, P - C) / depth + 0.5; nothing is drawn outside [0,1]^3.
 * THE ANGLE RULE: with D = -Y, a surface whose face normal has dot(G, D) >= 0.3
 *   takes the decal whole; otherwise it fades by saturate((dot(N, D) - 0.3) / 0.25)
 *   with N the shading normal, and is dropped at 0.
 * THE DICE: a base whose min and max sizes differ, or that does not take the
 *   whole texture (it picks one quarter of a 2x2 sheet at random), is decided
 *   by a random roll in game. Those are refused by name, not guessed.
 *
 * NAMED DIFFERENCES from the game: the ray meets the DRAWN opaque triangles,
 * not the collision, and starts 1 unit behind the reference so that a decal
 * placed exactly on its surface still meets it; the angle rule is applied per triangle and per vertex,
 * not per pixel; cutout (alpha-tested) surfaces do not receive; no distance
 * fade and no parallax. */

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
	RedOff          //!< the red control switched decals off
};

struct CellDecalResult
{
	std::vector<CellDecalMesh> meshes;
	std::vector<CellDecalFate> fates;   //!< one per reference, same order
	int count[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };   //!< by fate
	qint64 triangles = 0;
	qint64 receiverTris = 0;
	int withPrimitive = 0, byRay = 0;
	QString red;                        //!< the red control in force, empty when none
};

/*! Project every decal reference onto the receivers. `origin` is what the
 *  receivers' positions were shifted by. `dumpPath`, when not empty, takes one
 *  line per reference (form, fate, box centre, axes, sizes, ray distance). */
void cellProjectDecals( const EsmWorld & world, const QString & dataRoot,
	const std::vector<CellDecalRef> & refs, const std::vector<CellDecalReceiver> & receivers,
	const float origin[3], CellDecalResult & out, const QString & dumpPath );

//! The census name of a fate.
QString cellDecalFateName( CellDecalFate f );

/*! The census line: `  placed decals: N read, drawn D (...); refused R: <reason> <count>, ...`.
 *  Every reference with decal data is in exactly one of drawn or a named refusal. */
QString cellDecalCensusLine( const CellDecalResult & r );

#endif // CELLDECAL_H
