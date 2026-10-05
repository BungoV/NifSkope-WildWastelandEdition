#ifndef FARLIGHT_H
#define FARLIGHT_H

/*! DISTANT LIGHT FROM THE SURFELS, IN SWITCHABLE LAYERS (lane FARVIEW1, 2026-10-04; docs/cloud/FARVIEW1_DESIGN.md,
 *  docs/cloud/FARVIEW1b_DESIGN.md, docs/PRTP_PLAN.md 2h).
 *
 *  Past the distance where the game still has a cell's lights, a surface takes the light the bake's surfels hold:
 *  each lit surfel's irradiance from the PLACED lights (direct + the bounce the relight settles on; no sun, no sky,
 *  no glow), looked up by the 27 cells around the point on the same side, w = (1 - d^2 / R^2)^2, R = 1.5 cells.
 *
 *  THE BAKE (farLightBake): from the relight's recorded operators (src/proberelight.h). The relight is linear in the
 *  placed lights once the pass count is FIXED, so the light splits into layers, one per switch group (FARVIEW1b 3.2:
 *  ALWAYS 0, PARENT 1 = enable-parent root + parity, SELF 2): E(state) = E_always + sum of the groups that are on.
 *  The pass count = the count the all-on relight settles on. Gate L: sum of the layers against one all-on relight
 *  at that count, in double (bar 1e-6 of the p99); reds "settle" (each layer stops by BOUNCE2's own rule) and
 *  "drop" (the largest switched layer left out).
 *
 *  FILES, one set per 4096-unit sector, beside the relight's lights_X_Y.wlt (whose light-set hash they carry):
 *    farlight_X_Y.fvl  version 3 (FARVIEW1b 3.6; 3 = FARVIEW1): header 64 B, then 16 B records sorted by key:
 *                      u16 cx, cy, cz | u8 side | u8 pos[3] | u32 E always-on (RGB9E5) | u8 pad | u8 flags (1: only
 *                      layers light it). cx, cy, cz count from the header's bases (i32 kz0 at 36, kx0 at 52, ky0 at
 *                      56: the smallest key of the sector's records). Version 2 counted cx, cy from
 *                      floor(4096 X / cell) - 1, but a sector's surfels are the ones its probes linked, and a bake
 *                      with the far soup links surfels cells away (the airport: a record below the sector's corner).
 *    farlight_X_Y.fvg  the switched layers (absent when none reaches the sector): header 32 B, groups (u64 key, u32
 *                      first entry, u32 count), entries 8 B (u32 record index, u32 E RGB9E5).
 *
 *  THE RUNTIME SET (farLightLoad): every sector of a folder, refused where the three hashes differ; the summed E for
 *  a group state (farLightSum), and the GPU tables (farLightTables): an open-addressed hash on (world cell, side)
 *  -> record, and the records (position, E). The bulb dots come from the .wlt's lights with flag 32. */

#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <functional>
#include <vector>

struct ProbeRelightOps;
struct ProbeSoup;

struct FarLightBakeSpec
{
	QString red;                  //!< gate L's reds: "settle" | "drop" (the files are written from the green layers)
	float cell = 70.0f;           //!< the .tbk surfel cell
	double trimAlways = 1e-4;     //!< x the sector's all-on p99 (FARVIEW1b 3.5)
	double trimSwitched = 3e-5;
	bool dump = false;            //!< farcheck_X_Y.bin beside: per record the all-on and start-state E (the file checker)
};

struct FarLightBakeOut
{
	int passes = 0;               //!< the fixed pass count (the all-on relight's settled count)
	int groups = 0, switched = 0;
	double gateL = 0.0;           //!< max |sum of layers - all-on| / the all-on p99 (the red's when a red is set)
	double gateLmax = 0.0;        //!< the same / the all-on max
	int records = 0, entries = 0, sectors = 0;
	qint64 bytes = 0;
	double ms = 0.0;
	QString census;
};

bool farLightBake( const ProbeRelightOps & ops, const QString & dir, const FarLightBakeSpec & spec, FarLightBakeOut * out,
	QString * err );

/*! The bulb dots' intensity (FARVIEW1b 5.1): for each light, the glowing soup triangles within `clear` units of it,
 *  I_dot = sum Le x area / 4 (a closed bulb's mean projected area is a quarter of its surface). Writes Light::dot;
 *  returns the lights that got one. */
int farLightDots( const ProbeSoup & soup, ProbeRelightOps & ops, float clear, QString * census );

struct FarLightSet
{
	float cell = 70.0f;
	struct Rec
	{
		float pos[3];
		int k[3];                 //!< the world cell
		quint8 side;
		float Ea[3];              //!< always-on
	};
	std::vector<Rec> recs;
	struct Group
	{
		quint64 key = 0;
		bool onAtStart = true;
		std::vector<int> rec;     //!< into recs
		std::vector<float> E;     //!< 3 an entry
	};
	std::vector<Group> groups;    //!< switched layers, all sectors (one entry list per sector and group, appended)
	struct Dot
	{
		float pos[3];
		float I[3];
		quint64 group;
		bool onAtStart;
	};
	std::vector<Dot> dots;
	int sectors = 0, refused = 0;
	QString census;
};

bool farLightLoad( const QString & dir, FarLightSet * set, QString * err );

//! E per record (3 a record) for a group state: always-on + the groups `on` says
void farLightSum( const FarLightSet & set, const std::function<bool( quint64 key, bool onAtStart )> & on, std::vector<float> & E );

/*! The GPU tables. slotTab: 4 floats a slot (x, y, z x 8 + side, record + 1; 0 empty), exact integers in float so one
 *  sampler type reads both tables; 2^bits slots, load factor <= 0.5; recs: 8 floats a record (pos, 0, E, 0). maxProbe: the longest chain walked
 *  to find every record of one key (the shader walks to the first empty slot, never further than this). */
void farLightTables( const FarLightSet & set, const std::vector<float> & E, std::vector<float> & slotTab, std::vector<float> & recs,
	int * bits, int * maxProbe );

//! the hash both sides use (the shader's cellFarHash)
inline quint32 farLightHash( int x, int y, int z, int side )
{
	return ( quint32( x ) * 73856093u ) ^ ( quint32( y ) * 19349663u ) ^ ( quint32( z ) * 83492791u ) ^ ( quint32( side ) * 2654435761u );
}

#endif // FARLIGHT_H
