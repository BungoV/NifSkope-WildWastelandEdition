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

#ifndef CELLAODECAL_H
#define CELLAODECAL_H

/* BAKED AO DECALS IN THE CELL VIEW (lane AODECAL1, 2026-10-04; src/aovolume.h is the volume itself).
 *
 * Which models get one (his call, 10-03): only big, movable statics -- cars, dumpsters, furniture, crates.
 * Small clutter is skipped. A model qualifies when its base is STAT, MSTT, FURN or CONT, its path names one
 * of those kinds (aoDecalKind) and its render triangles are at least kMinSide long on their longest side and
 * at most kMaxSide long and kMaxHeight tall (a building is not movable). WW_CELL_AODECAL_RED=clutter is the
 * gate's red: the size floor and the path rule are dropped.
 *
 * What it does: every placed copy darkens the GI probe term (only that: sun, lamps, emissive and the cube's
 * specular stay) by its volume's AO in the copy's own model space; copies multiply (his call). The probes
 * already saw the copies as blocked sky, so the probe grid is rebuilt with the copies taken out first
 * (design section 4: each probe octant's sky share divided by what the copies' volumes say they blocked).
 * The row ships OFF: WW_CELL_AODECAL=1 pins it on, else the setting WW/CellAoDecal. */

#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

#include "aovolume.h"

struct NativeSrcShape;
struct ProbeSoup;
class Matrix;
class Vector3;

struct AoDecalSet
{
	struct Model
	{
		QString model;		//!< as the base names it (data-relative, no "meshes\")
		QString norm;		//!< "meshes\..." normalised: the index's key
		aovol::Volume vol;
		std::vector<float> maskTris;	//!< every drawn shape's triangles (model space): the copy's own pixels
		bool baked = false;	//!< this run baked it (else read from the folder)
	};
	struct Copy
	{
		int model = -1;
		double R[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };	//!< model -> world rotation, row-major
		double t[3] = { 0, 0, 0 };
		double s = 1.0;
		//! the placement as placed, kept apart from R/t/s (which the red "frozen" moves): the GPU gate's truth
		double placedR[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 }, placedT[3] = { 0, 0, 0 }, placedS = 1.0;
		unsigned ref = 0;
		size_t soupTri0 = 0, soupTri1 = 0;	//!< its triangles in the probe soup
		float wlo[3] = { 0, 0, 0 }, whi[3] = { 0, 0, 0 };	//!< the footprint's world AABB
		std::shared_ptr<aovol::OctantBasis> basis;
	};
	std::vector<Model> models;
	std::vector<Copy> copies;
	QString red;			//!< WW_CELL_AODECAL_RED
	QString dir;			//!< the .ao folder
	QString census;
	int modelsBaked = 0, modelsRead = 0, modelsRefused = 0;
	double msBake = 0.0;

	//! world point -> the copy's model space
	static void toModel( const Copy & c, const double w[3], double m[3] );
	/*! The copy-free sky share of each octant at the world point p: s / max(1 - occ, 0.1), 0 where s = 0, with
	 *  occ = 1 - product over the copies whose footprint holds p of (1 - their blocked share). False when no
	 *  copy reaches p (out = vis). The red nodivide returns vis unchanged. */
	bool skyFree( const double p[3], const float vis[8], float out[8] ) const;
};

bool aoDecalOn();
void aoDecalSetOn( bool on );
//! "car", "dumpster", "furniture", "crate" or empty (not one of his kinds)
QString aoDecalKind( const QString & model );

//! collects the qualifying copies while the cell view places its references, then loads or bakes their volumes
class AoDecalBuilder
{
public:
	AoDecalBuilder();
	bool on() const { return on_; }
	//! one placed reference (its loaded shapes, model space); soup tris [tri0, tri1) are its probe-soup share
	void consider( const QString & baseType, const QString & model, const std::vector<NativeSrcShape> & shapes,
		const Vector3 & pos, const Matrix & rot, float scale, unsigned ref, size_t tri0, size_t tri1 );
	//! the volumes: read from `dir` (index.aoi) when the model's fingerprint matches, else baked and written
	std::shared_ptr<const AoDecalSet> finish( const QString & dataRoot );
	QString census() const;

private:
	bool on_ = false;
	std::shared_ptr<AoDecalSet> set_;
	struct Seen { int model = -1; QString why; };
	std::vector<std::pair<QString, Seen>> seenOrder_;
	std::vector<std::vector<float>> bakeTris_;
	int considered_ = 0, skippedKind_ = 0, skippedSmall_ = 0, skippedBig_ = 0, skippedType_ = 0;
	QString censusFile_;
	QString censusRows_;
};

/*! lane AODECAL1 gate D on a real cell: per probe near a copy, the bake's sky share per octant; then the same
 *  probe traced twice by this tracer (the soup with the copies, the soup without them) and the volume route's
 *  copy-free share rebuilt from the traced-with share. Written as a table to `file`. */
bool aoDecalProbeGate( const AoDecalSet & set, const ProbeSoup & soup, const std::vector<float> & probePos,
	const std::vector<float> & probeVis, const QString & file, QString * summary );

/*! nifcli `aobake` (lane AODECAL1 gate A on a real model): one model's volume read or baked into --dir, and with
 *  --receivers each receiver's lookup beside a 4096-ray brute force of the placed model (--copy "R9 t3 s") */
int aoDecalCli( const QStringList & args );

#endif // CELLAODECAL_H
