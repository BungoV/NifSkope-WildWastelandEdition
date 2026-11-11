#ifndef RTFARTRACE_H
#define RTFARTRACE_H

/*! lane AIRPERF1 round 5: the far field as the ray-traced GI traces it.
 *
 *  Round 4 traced the far terrain alone, in one dirt tone. A far shape (a document shape named "FarLOD ...": the far
 *  terrain, the far objects' side arrays, the instanced far rows, the game's own far chunks) is read here ONCE into an
 *  immutable RtFarShape: its triangles, each with the colour of its diffuse map at the triangle's middle, in runs a
 *  cell (and a placed object) long. The stream picks the runs within the traced ring that no loaded cell traces already
 *  (cellstream.cpp traceSync) and the traced set's worker copies them into the merged soup (rttraceset.cpp).
 *
 *  An instanced bucket keeps its meshes once, in the mesh's own frame, and a 3 x 4 placement a run: the worker places
 *  them, so the main thread never expands a row.
 *
 *  Not in it: far water and the sea (the trace has no water), and far trees drawn as cards (they have no mesh). */

#include <QtGlobal>

#include <memory>
#include <string>
#include <vector>

class NifModel;
class Shape;
class ProbeAlbedo;

struct RtFarShape
{
	//! a placement: world = m (rows of a 3 x 4) x (mesh vertex, 1)
	struct Xf
	{
		float m[12] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
		float uMid = 0.0f;       //!< mirrorU: u = uMid - u (fo4_default.vert, the instanced rows)
		bool mirrorU = false;
		bool flip = false;       //!< a mirrored placement: the winding turns
	};
	struct Run
	{
		qint64 cell = 0;         //!< CellFarHide::cellKey of the cell it stands in
		quint32 ref = 0;         //!< the placed object (0: none)
		int first = 0, count = 0;   //!< triangles of `tris`
		int xf = -1;             //!< -1: the triangles are in the world's frame already
	};
	int src = 0;                 //!< 0 the far terrain, 1 far objects
	quint64 sig = 0;             //!< what it was read from (rtFarShapeSig): a rebuilt far field gives another
	int span[4] = { 0, 0, -1, -1 };   //!< the cells its runs stand in (x0, y0, x1, y1)
	std::vector<float> tris;     //!< 9 floats a triangle
	std::vector<unsigned char> alb;   //!< 3 bytes a triangle, linear x 255
	std::vector<float> uv;       //!< 6 floats a triangle; only with a mask
	std::vector<Xf> xf;
	std::vector<Run> runs;
	bool twoSided = false;
	std::string maskName;        //!< the alpha-tested map (lower case); empty: none
	int maskW = 0, maskH = 0, thr = 128;
	std::shared_ptr<const std::vector<unsigned char>> mask;
	qint64 flat = 0;             //!< triangles that took the stated tone (no map read)
	double ms = 0.0;             //!< what the read cost on the main thread

	double bytes() const
	{
		return double( tris.size() ) * 4.0 + double( alb.size() ) + double( uv.size() ) * 4.0 + double( xf.size() * sizeof( Xf ) )
			+ double( runs.size() * sizeof( Run ) ) + ( mask ? double( mask->size() ) : 0.0 );
	}
};

//! what the shape is made of now (0: nothing to trace)
quint64 rtFarShapeSig( const NifModel * nif, Shape * sh );
/*! the shape read (main thread). `src` 0 the terrain, 1 objects. `flatRed` = the stated tone on every triangle (round
 *  4's far colour, red WW_CELL_RTSET_RED=fardirt). Vertex colours are not read: on the far buckets they are bake
 *  channels (the far AO, the wind weight), not paint */
std::shared_ptr<const RtFarShape> rtFarShapeBuild( const NifModel * nif, Shape * sh, int src, ProbeAlbedo & alb, bool flatRed );

#endif
