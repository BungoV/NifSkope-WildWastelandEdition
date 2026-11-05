/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENSE BLOCK *****/

#ifndef AOVOLUME_H
#define AOVOLUME_H

/* BAKED AO DECALS (lane AODECAL1, 2026-10-04; design docs/cloud/AODECAL1_DESIGN.md, twin
 * tests/spells/aodecal1_check.py). One model's ambient-occlusion volume: a box around the model (its bbox plus
 * a margin of twice the median extent), about 32 x 32 x 16 voxels shared out by the box's extents, each voxel
 * the L2 spherical harmonics of "a ray from here hits the model" (1024 Fibonacci rays), divided by 2 sqrt(pi)
 * so coefficient 0 is the blocked share of the sphere. Past the box an equivalent sphere per direction (the far
 * field) carries it to where it blocks under 1%. A placed copy darkens the probe term of what stands around
 * it: AO(x, n) looked up in the copy's model space; several copies multiply (his call, 10-03).
 *
 * Files (all little endian): `<hash>.ao` per model, 128-byte header + 9 bytes a voxel; `index.aoi` per folder,
 * a 64-byte record per model sorted by the FNV-1a 64 of its normalised path. Layouts in the design's section 5. */

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>

#include <cstdint>
#include <vector>

namespace aovol {

constexpr int kBudget = 32 * 32 * 16;
constexpr int kRays = 1024;
constexpr double kMarginK = 2.0;
constexpr double kFadeK = 0.25;
constexpr double kFarCut = 0.01;
constexpr double kGFloor = 0.1;	//!< a probe octant the copies block beyond 90% is not divided further
constexpr std::uint32_t kAoMagic = 0x4F415757u, kAoiMagic = 0x49415757u;
constexpr int kAoHead = 128;

struct Volume
{
	float lo[3] = { 0, 0, 0 }, hi[3] = { 0, 0, 0 };	//!< model space
	int dims[3] = { 0, 0, 0 };
	float fade = 1.0f;
	float cmax = 1e-6f;
	std::uint32_t rays = 0;
	float c[3] = { 0, 0, 0 };	//!< the far field's centre (the model's bbox centre)
	float coef[9] = {};		//!< rho^2(u) as L2 SH
	float rcut = 0.0f;
	std::uint64_t fp = 0;		//!< the model's fingerprint (first 8 bytes of its SHA-256)
	std::vector<std::uint8_t> payload;	//!< z, y, x, 9 bytes (u8 share, 8 x s8)
	std::vector<float> k;		//!< the payload dequantised: (z dy + y) dx + x, 9 floats
	// the bake's census (not in the file)
	int tris = 0, insideVoxels = 0;
	double msBake = 0.0;

	float cell( int a ) const { return ( hi[a] - lo[a] ) / float( dims[a] ); }
	size_t voxels() const { return size_t( dims[0] ) * size_t( dims[1] ) * size_t( dims[2] ); }
	//! the model-space box a copy can darken: the volume box and the far field's cut sphere
	void footprint( float flo[3], float fhi[3] ) const;
};

//! L2 real SH at a unit direction (the twin's constants)
void sh9( const double d[3], double y[9] );
//! the Fibonacci sphere's direction i of n (unrotated)
void fib( int i, int n, double d[3] );

/*! The bake from the model's render triangles (model space, 9 floats a triangle). The inside test is a back-face
 *  share of the hits above 0.5. Deterministic: fixed directions; voxels run on threads, each its own sums. */
bool bake( const std::vector<float> & tris, Volume * out, QString * why = nullptr, int rays = kRays );

//! the .ao file's bytes
QByteArray aoBytes( const Volume & v );
//! read back; refuses a wrong magic, payload size or CRC, and a fingerprint other than `expectFp` (unless 0)
bool readAo( const QByteArray & b, std::uint64_t expectFp, Volume * out, QString * why );

struct IndexRec
{
	std::uint64_t hash = 0, fp = 0;
	QString path;			//!< normalised
	float lo[3] = { 0, 0, 0 }, hi[3] = { 0, 0, 0 };	//!< the footprint, model space
	std::uint32_t size = 0, crc = 0;	//!< of the .ao file
};
QByteArray indexBytes( QVector<IndexRec> recs );
bool readIndex( const QByteArray & b, QHash<QString, IndexRec> * out, QString * why );

std::uint64_t fnv1a64( const QByteArray & b );
QString normPath( const QString & p );			//!< lower case, backslashes, no leading separator
std::uint64_t fingerprint( const QByteArray & modelBytes );
std::uint32_t crc32( const char * p, size_t n );
QString aoFileName( const QString & normalisedPath );	//!< "<16 hex digits>.ao"

//! AO over the receiver's hemisphere at m with normal n, both in the copy's model space (1 open, 0 closed)
double lookup( const Volume & v, const double m[3], const double n[3] );

/*! The copy's blocked share of each WORLD octant at m (model space), from the volume and past it the far
 *  sphere's cap. Q = the octant integrals of the SH turned into model space by the copy's rotation R
 *  (model -> world, row-major), from octantBasis(). Octant bits: x < 0 | (y < 0) << 1 | (z < 0) << 2. */
struct OctantBasis
{
	double Q[8][9];
	std::vector<float> dm;	//!< the 4096 world directions in model space
	std::vector<std::uint8_t> oc;	//!< their world octant
	int count[8];
};
void octantBasis( const double R[9], OctantBasis * out );
void octantOcclusion( const Volume & v, const OctantBasis & B, const double m[3], double occ[8] );

}	// namespace aovol

#endif // AOVOLUME_H
