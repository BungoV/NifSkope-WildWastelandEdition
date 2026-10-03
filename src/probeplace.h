#ifndef PROBEPLACE_H
#define PROBEPLACE_H

/*! PROBE PLACEMENT FOR THE PRTP BAKE (lane PRTPPLACE, 2026-09-30).
 *
 *  Where the transfer probes go, computed from the cell view's own geometry
 *  instead of the game's physics. Two halves:
 *
 *  1. FO4CS's rules, number for number (its TransportBakeRuntime.cpp B2f/B2n):
 *     a 280-unit lattice on world multiples, a column ray from the top that
 *     pierces every surface (step 4), a first-hit probe at eye height (120),
 *     one probe in every air gap of 140 or more under it (at floor +
 *     min(eye, gap/2), 6 levels), and wall stacks off each of those probes in
 *     the 4 axis directions (search 280, standoff 96, heights 240..960, each
 *     level verified by a vertical ray AND a horizontal ray that still finds
 *     the wall).
 *
 *  2. What FO4CS never built: probes in the OPENINGS between spaces --
 *     doorways, windows and breaches -- found without doors, from the air
 *     itself (the "gap between two volumes" ruling, docs/PRTP_PLAN.md 2b).
 *     The soup is voxelised; an opening is a straight-through passage in a
 *     thin wall that is narrower than the air on both sides of it, bounded on
 *     all four sides (jambs, lintel, sill or floor), with a roof on at least
 *     one side. A door that stands in it only TAGS it (the door's ref), since
 *     doors are baked open.
 *
 *  The soup is statics only (the caller filters: no doors, no clutter, no
 *  effect shaders, no glass). Everything is world units. */

#include <QString>
#include <QtGlobal>
#include <vector>

//! The triangles the probes see, plus the door boxes that only tag apertures.
struct ProbeSoup
{
	std::vector<float> tris;    //!< 9 floats per triangle, world units
	struct Door
	{
		quint32 ref = 0;
		float lo[3] = { 0, 0, 0 };
		float hi[3] = { 0, 0, 0 };
	};
	std::vector<Door> doors;
	/*! Lane PRTPBAKE: 3 bytes a triangle, LINEAR albedo x 255 (the texture's sRGB decoded,
	 *  times the vertex color), what the bake's surfels store. Empty = no albedo known
	 *  (the placer never reads it); the soup file carries it as an optional 'ALB1' tail. */
	std::vector<quint8> alb;
	void addTri( const float a[3], const float b[3], const float c[3] )
	{
		tris.insert( tris.end(), a, a + 3 );
		tris.insert( tris.end(), b, b + 3 );
		tris.insert( tris.end(), c, c + 3 );
		if ( !alb.empty() )
			alb.resize( tris.size() / 3, 128 );   // keep 3 bytes a triangle once any triangle has one
	}
	void addTri( const float a[3], const float b[3], const float c[3], const quint8 rgb[3] )
	{
		if ( alb.size() != tris.size() / 3 )
			alb.resize( tris.size() / 3, 128 );   // earlier triangles had none: mid grey
		tris.insert( tris.end(), a, a + 3 );
		tris.insert( tris.end(), b, b + 3 );
		tris.insert( tris.end(), c, c + 3 );
		alb.insert( alb.end(), rgb, rgb + 3 );
	}
	qint64 triCount() const { return qint64( tris.size() / 9 ); }
	/*! Lane BAKE4: blended glass (BGSM/BGEM bAlphaBlend, NiAlphaProperty blend), 9 floats a
	 *  triangle, kept apart from `tris`: the placer never sees it and it holds no surfel. The
	 *  bake tints the light that passes through it by glassT, 3 bytes a triangle, the
	 *  per-channel transmittance x 255 (1 - a (1 - c): the blend read as a filter). The
	 *  soup file carries it as an optional 'GLS1' tail after 'ALB1'. */
	std::vector<float> glass;
	std::vector<quint8> glassT;
	void addGlass( const float a[3], const float b[3], const float c[3], const quint8 t[3] )
	{
		glass.insert( glass.end(), a, a + 3 );
		glass.insert( glass.end(), b, b + 3 );
		glass.insert( glass.end(), c, c + 3 );
		glassT.insert( glassT.end(), t, t + 3 );
	}
	/*! Lane CAPTURE1: what the hit and cube albedo ways read at a point of a triangle (the tri way,
	 *  the default, never fills it). One per triangle of `tris` when filled; tex = -1 (the ground, a
	 *  shape with no map) keeps `alb` and the face normal. ~70 bytes a triangle. In memory only:
	 *  the soup file does not carry it. */
	struct TriMat
	{
		qint32 tex = -1;            //!< into matTex
		qint32 pal = -1;            //!< a palette map (Greyscale_To_PaletteColor), into matTex
		float row = -1.0f;          //!< the palette row; < 0 = rowScale x the vertex red
		float rowScale = 0.0f;
		float uv[6] = { 0, 0, 0, 0, 0, 0 };
		quint8 vc[9] = { 255, 255, 255, 255, 255, 255, 255, 255, 255 };   //!< vertex colors, gamma
		qint16 n[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };   //!< world vertex normals, snorm16
	};
	std::vector<TriMat> mat;
	std::vector<QString> matTex;
	std::vector<const void *> matTexPtr;   //!< the loaded maps (DDSTexture16), resolved before the bake
	//! Lane CAPTURE1 red "nofilter": the triangles the soup leaves out (effects, decals, blended,
	//! leaves), seen by the cube way only. Empty in every real run.
	std::vector<float> cubeExtra;
	std::vector<TriMat> cubeExtraMat;
	//! the material of the triangle just added (pads the triangles before it that had none)
	void setLastMat( const TriMat & m )
	{
		mat.resize( size_t( triCount() ) - 1 );
		mat.push_back( m );
	}
	/*! lane ROOMCLAMP1: 1 = the triangle's material is two-sided (SLSF2 Double_Sided or BGSM bTwoSided): it
	 *  has no back. Shorter than the triangle count = the rest are one-sided (empty: all one-sided; their
	 *  front is the stored winding). The soup file carries it as an optional 'TWO1' tail after 'GLS1'. */
	std::vector<quint8> twoSided;
	void markLastTwoSided()
	{
		twoSided.resize( size_t( triCount() ), 0 );
		twoSided.back() = 1;
	}
	bool isTwoSided( size_t tri ) const { return tri < twoSided.size() && twoSided[tri]; }
};

//! Lane BAKE4: one box of an enclosed room's air (world units), what `.tbk` v4 writes for the
//! point-in-room test. A room is many boxes (merged runs of its air columns).
struct ProbeRoomBox
{
	quint32 room = 0;
	float lo[3] = { 0, 0, 0 };
	float hi[3] = { 0, 0, 0 };
};
//! No room in this slot (a probe in one room, or none).
constexpr quint32 kProbeRoomNone = 0xFFFFFFFFu;

//! Room: the point of an enclosed room farthest from its walls. Cover: a spot no
//! other probe saw (lane PRTPPLACE's interior rule, bungo 2026-09-30).
enum class ProbeClass : int { FirstHit = 0, Interior = 1, Wall = 2, Aperture = 3, Room = 4, Cover = 5 };
enum class ApertureKind : int { None = 0, Doorway = 1, Window = 2, Breach = 3 };

struct ProbePoint
{
	float pos[3] = { 0, 0, 0 };
	ProbeClass cls = ProbeClass::FirstHit;
	int level = 0;              //!< interior level, the wall stack's level, or a room/cover probe's room
	int cellX = 0, cellY = 0;
	// apertures only
	ApertureKind kind = ApertureKind::None;
	float nrm[3] = { 0, 0, 0 }; //!< from the roofed side toward the open one (a wall probe: toward its wall)
	float width = 0, height = 0, sill = 0;
	quint32 doorRef = 0;        //!< the DOOR ref standing in it, 0 = none
	bool roomToRoom = false;    //!< both sides roofed: the normal is only the pass axis
	/*! Lane BAKE4: the enclosed room the probe stands in (0 = none: outdoors, a porch, a
	 *  ledge); an opening's probe gets the rooms on both sides (room[1] = kProbeRoomNone
	 *  for every other probe). Ids are this placement's: (rect hash << 16) | (n + 1). */
	quint32 room[2] = { 0, kProbeRoomNone };
};

struct ProbePlaceSpec
{
	//! The lattice rect, world units, inclusive (FO4CS: ceil(lo/s)*s .. x <= hi).
	float minX = 0, minY = 0, maxX = 0, maxY = 0;
	float spacing = 280.0f;
	float eye = 120.0f;
	float pierceStep = 4.0f;
	float minAirGap = 140.0f;
	int maxLevels = 6;
	float wallSearch = 280.0f;
	float wallStandoff = 96.0f;
	std::vector<float> wallHeights { 240.0f, 480.0f, 720.0f, 960.0f };
	bool apertures = true;
	float voxel = 35.0f;
	int maxVoxelLayers = 320;
	//! Wall angles the openings are searched at (voxel frames rotated about z).
	std::vector<float> apertureAngles { 0.0f, 22.5f, 45.0f, 67.5f };
	/*! THE INTERIOR RULE (needs the 0-degree frame above). A room is covered
	 *  walkable air (floor, minAirGap of headroom, a roof) cut at the openings;
	 *  one probe at its point farthest from the walls, then every walkable spot
	 *  must see a probe within coverRadius, hallRadius in a hallway (a room at
	 *  most hallWidth from wall to middle). A room whose edge is more than
	 *  roomOpenMax open ground or drop (no floor beyond) is outdoors -- a porch, a
	 *  strip under the eaves -- and left to the lattice; one whose edge is mostly
	 *  drop is a furniture top or a ledge; one with no opening, no open edge and
	 *  no drop is a sealed hollow (a foundation's crawlspace) and gets nothing. */
	bool coverage = true;
	float coverRadius = 200.0f;
	float hallRadius = 70.0f;    //!< no hallway spot more than 70 from a probe: they stand at most 140 apart
	float hallWidth = 105.0f;
	float roomOpenMax = 0.3f;
	int roomMinCells = 4;
	/*! A deliberate defect for the gates' refuters: "wall" drops the stack's
	 *  horizontal re-check, "aperture" drops the thin-jamb and straight-through
	 *  tests, "frames" searches openings at the first wall angle only,
	 *  "coverage" places room probes but skips the blind-spot fill. Empty in
	 *  every real run. */
	QString red;
};

struct ProbePlaceResult
{
	std::vector<ProbePoint> probes;
	int columns = 0, columnsEmpty = 0;
	int firstHit = 0, interior = 0, wall = 0;
	int gapsRejected = 0, levelsCapped = 0, wallRefused = 0, wallColumns = 0;
	int doorway = 0, window = 0, breach = 0, doored = 0, roomToRoom = 0;
	int apComponents = 0, apRejectedUnroofed = 0, apRejectedShape = 0, apRejectedPocket = 0, apMerged = 0;
	int gridX = 0, gridY = 0, gridZ = 0;
	int apFrames = 0;
	int rooms = 0, roomsOpen = 0, roomsLedge = 0, roomsSealed = 0, roomsTiny = 0, room = 0, cover = 0, roomNear = 0;
	int walkCells = 0, coverCells = 0, hallCells = 0, cutCells = 0, blindLeft = 0;
	int backFloors = 0;         //!< lane ROOMCLAMP1: air cells refused as walkable: only the back of one-sided faces below
	int backColumnHits = 0;     //!< lane ROOMCLAMP1: column levels (first hit or gap) on the back of a one-sided face: no probe
	int backPlaced = 0;         //!< lane ROOMCLAMP1: room / cover probes refused: the back of a one-sided face within 200 below
	bool gridClamped = false;
	//! lane BAKE4: the air of every enclosed room a probe stands in, and the probes given a room
	std::vector<ProbeRoomBox> roomBoxes;
	int roomIds = 0, probesInRoom = 0, apertureRooms = 0;
	qint64 soupTris = 0;
	double msBvh = 0, msColumns = 0, msVoxel = 0, msApertures = 0, msCoverage = 0;
	QString error;
};

//! Place every probe. False only on an unusable input (empty soup, bad rect).
bool probePlace( const ProbeSoup & soup, const ProbePlaceSpec & spec, ProbePlaceResult * out );

//! One row per probe; `#` lines carry the spec and the census.
bool probeWriteTsv( const QString & path, const ProbePlaceSpec & spec,
	const ProbePlaceResult & r, QString * error );

//! The census as notes lines (cell view notes, CLI stdout).
QString probeCensusText( const ProbePlaceResult & r );

//! Binary soup file ('PSP1'): the gates' input and the cell view's dump.
bool probeSoupWrite( const QString & path, const ProbeSoup & soup, QString * error );
bool probeSoupRead( const QString & path, ProbeSoup * soup, QString * error );

//! `nifskope -no-gui probeplace --soup <f> --rect minX,minY,maxX,maxY --out <tsv>
//!  [--spacing s] [--red wall|aperture|frames|coverage] [--no-openings] [--no-rooms]`
int probePlaceCli( const QStringList & args );

#endif // PROBEPLACE_H
