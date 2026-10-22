#ifndef PROBEGIFINE_H
#define PROBEGIFINE_H

/* lane EMISSIVEGRID1: a finer GI grid (a third of the coarse voxel) in a box round each bright cluster of glowing
 * surfels. The coarse grid (about 100 units, probes about 280 apart) smears a sign's splash into a faint wash; the
 * fine box holds the glow's direct irradiance D (each glowing surfel a small area light, shadowed by the bake's
 * geometry) and the coarse grid's own share of the same surfels C (their links blended as the coarse grid blends its
 * probes), so the shader trades C for D inside the box and fades back to the coarse grid at its edge. Built from the
 * relight's surfels and links at cell load: no rebake. Off (WW_CELL_EMISSIVEGRID=0, the default) nothing is built. */

#include <QString>

#include <functional>
#include <vector>

struct ProbeRooms;

struct ProbeGiFineEmitter
{
	double p[3], n[3], Le[3];
	double area;		//!< the surfel's lit area: the largest omega x d^2 its links saw
};

struct ProbeGiFineProbe
{
	double p[3];
	int room[2];
	int linkStart, linkEnd;	//!< into ProbeGiFineInput::link*
};

struct ProbeGiFineInput
{
	std::vector<ProbeGiFineEmitter> glow;
	std::vector<ProbeGiFineProbe> probes;
	std::vector<int> linkEmit;		//!< per probe link to a glowing surfel: its index into glow
	std::vector<double> linkW;		//!< 7 a link: omega x kUnl x cosA on the six axes, then 0
	std::vector<double> linkTint;	//!< 3 a link
	double coarseVoxel = 100.0, radius = 300.0;
	int coarseDims[3] = { 0, 0, 0 };
	const ProbeRooms * rooms = nullptr;
	std::function<bool( const double p[3], const double q[3] )> blocked;
	QString red;	//!< "1" / "old": nothing built (the red)
};

struct ProbeGiFine
{
	int boxes = 0, clusters = 0, emitters = 0;
	qint64 voxels = 0, voxelsAir = 0, rays = 0, raysBlocked = 0;
	double ms = 0.0, eth = 0.0;
	/*! per box 8 floats: origin xyz, voxel; nx ny nz, the box's first layer. data: the atlas, coarseDims[0] x
	 *  coarseDims[1] a layer, `layers` layers, rgba; a box takes 12 x nz layers: D on +X -X +Y -Y +Z -Z (a = air
	 *  1/0), then C on the same six */
	std::vector<float> box;
	std::vector<float> data;
	int layers = 0, width = 0, height = 0;
	QString note;
};

void probeGiFineBuild( const ProbeGiFineInput & in, ProbeGiFine * out );
QString probeGiFineCensusText( const ProbeGiFine & f );

#endif // PROBEGIFINE_H
