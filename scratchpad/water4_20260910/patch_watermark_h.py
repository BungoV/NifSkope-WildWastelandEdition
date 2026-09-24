# -*- coding: utf-8 -*-
"""patch_watermark_h.py -- src/watermark.h: the dye kinds, the solver core, the dye API."""
import sys
sys.path.insert(0, 'scratchpad/water4_20260910')
from splice import splice   # noqa

H = 'src/watermark.h'
splice(H, [
    ('\tenum Kind { Stroke = 0, Pin = 1, Barrier = 2, Merge = 3, SourcePin = 4, OutletPin = 5,\n\t\tZeroFlow = 6 };\n',
     '''\t/* Lane WATER4's three: DyePin (7) is one point with a colour and a strength,
\t * whose plume runs DOWNSTREAM along the solved flow; DyeMouth (9) is a
\t * one-point mark on a river saying "this river's water tints the body it
\t * drains into" (the source is then the river's own id, so the consumer
\t * takes the river's colour); DyeKnob (8) is the document's one dye knob,
\t * the half-distance in world units, in `width`, at most one per file. A
\t * DyePin record carries 4 more bytes (RGBA) after its points; every other
\t * kind is the 20 + 8n of spec 3.7. */
\tenum Kind { Stroke = 0, Pin = 1, Barrier = 2, Merge = 3, SourcePin = 4, OutletPin = 5,
\t\tZeroFlow = 6, DyePin = 7, DyeKnob = 8, DyeMouth = 9 };
''', 'replace'),
    ('\tQVector<WaterStrokePoint> pts;\n\n\tbool enabled() const { return !( flags & Disabled ); }\n',
     '\tquint8 colour[4] = { 0, 0, 0, 0 };   //!< DyePin only: RGBA of the dye\n', 'before'),
    ('\tqint64 outsideBody = 0;    //!< stroke points that fell outside their own body\n',
     '''\tdouble solveSeconds = 0.0; //!< wall time of the potential solves, all bodies
\tint dyeBodies = 0;         //!< bodies that received a plume
\tqint64 dyeTexels = 0;      //!< texels with a dye weight > 0
\tdouble strokeAgreement = 1.0; //!< mean cosine between the strokes' tangents and the solved flow under them
''', 'after'),
    ('/*! An open `.lodl` version-3 file, its body table, its strokes, and the solve.\n',
     '''/*! THE SOLVER CORE (lane WATER4): potential flow on a texel mask.
 *
 *  bungo, 2026-09-10: *"Could this maybe use a bit of some simulation
 *  though?"* -- after seeing the harmonic fill's held discs (*"it's like
 *  overlapping circles more like"*).  On a body's mask, `div( k grad phi ) =
 *  S`: `k` the conductance (the water depth, so flux prefers deep water), `S`
 *  the sources (+) and sinks (-), NO-FLUX at every bank face because the
 *  five-point stencil only ever reaches a neighbour inside the mask, and the
 *  velocity `u = -grad phi` from the face fluxes.  Continuity is then a
 *  property, not a rule: a channel that halves its width doubles its speed.
 *
 *  Solved by Jacobi-preconditioned conjugate gradient on the COMPACTED wet
 *  set (the bounding box of the Charles is 202,752 texels; the river is
 *  25,114).  The pure-Neumann system is singular and consistent once the
 *  sources balance, which `solve` enforces by subtracting the mean; a
 *  `dirichlet` set (cells held at 0) makes it regular and is how a WINDOW cut
 *  out of a sea gets an open far field.  Every method reports the number
 *  the harness gates on: iterations, the relative residual, the divergence.
 *
 *  Dye rides on it: `dye()` is the steady advection-decay `u . grad c =
 *  -|u| c / L` solved EXACTLY in one pass by visiting cells in descending
 *  potential -- every upwind neighbour has a higher potential, so it is
 *  already known -- with `L` the HALF-distance (weight 1/2 at L, 1/8 at 3L)
 *  and the chord through a cell along the flow as the distance a cell costs.
 *  `trace()` is Pollock's semi-analytic streamline on the same face fluxes,
 *  which cannot leave through a bank because a wall face's velocity is 0.
 */
struct WaterFlowGrid
{
\tint w = 0, h = 0;
\tint n = 0;                         //!< wet cells
\tstd::vector<int> idx;              //!< w*h -> cell index, or -1
\tstd::vector<int> cx, cy;           //!< cell -> texel
\tstd::vector<int> fi, fj;           //!< faces: cell i | cell j, the first nE are east faces
\tstd::vector<double> kf;            //!< face conductance, the harmonic mean
\tint nE = 0;
\tstd::vector<double> diag;

\t//! Build from a mask (1 = wet) and a conductance per texel (ignored where dry).
\tvoid build( int width, int height, const std::vector<quint8> & wet,
\t\tconst std::vector<float> & k );
\t/*! `A phi = b` with `b` > 0 a source.  `dirichlet` (may be empty) marks
\t *  cells held at 0.  False only when the system is empty. */
\tbool solve( const std::vector<double> & b, const std::vector<quint8> & dirichlet,
\t\tstd::vector<double> & phi, int & iterations, double & residual,
\t\tdouble tol = 1e-9, int cap = 20000 ) const;
\t//! Flux on every face, positive from i to j.
\tvoid faceFlux( const std::vector<double> & phi, std::vector<double> & F ) const;
\t//! Cell velocity: each axis the mean of its two face velocities, a wall face 0.
\tvoid velocity( const std::vector<double> & phi, std::vector<double> & ux,
\t\tstd::vector<double> & uy ) const;
\t//! Net outflow per cell -- equals `b` where the solve converged.
\tvoid divergence( const std::vector<double> & phi, std::vector<double> & div ) const;
\t/*! The dye pass.  `held` >= 0 fixes a cell's weight (a source); < 0 is free.
\t *  `halfTexels` is L in texels.  `c` comes back in 0..1. */
\tvoid dye( const std::vector<double> & phi, const std::vector<double> & held,
\t\tdouble halfTexels, std::vector<double> & c ) const;
\t/*! Pollock's streamline from texel-space (x, y); true when it enters a
\t *  `stop` cell (indexed like `idx`, over w*h) within `maxCells` crossings. */
\tbool trace( const std::vector<double> & phi, double x, double y,
\t\tconst std::vector<quint8> & stop, int maxCells ) const;
};

''', 'before'),
    ('\tvoid setBodyLockZero( int id, bool on );\n\tbool bodyLockZero( int id ) const;\n',
     '''\t/*! bungo's dye: *"river flowing into an ocean and the river and the ocean
\t *  may have slightly different color"*.  A DyeMouth mark on `id` carries
\t *  its water past its mouth into the body it drains to, as a plume that
\t *  fades; `strength` 0..1 is the weight at the mouth. */
\tvoid setBodyDyeMouth( int id, bool on, float strength );
\tbool bodyDyeMouth( int id, float * strength = nullptr ) const;
\t/*! The one dye knob: the half-distance in world units (weight 1/2 after
\t *  L, 1/8 after 3 L).  Stored as a DyeKnob mark when it is not the default. */
\tdouble dyeHalfDistance() const;
\tvoid setDyeHalfDistance( double worldUnits );
\tstatic constexpr double kDyeHalfDistanceDefault = 8192.0;   //!< two cells
\t//! True when the store carries any dye mark, i.e. when a dye plane is written.
\tbool hasDye() const;
\t/*! The dye word this document would write at a plane texel: bits 0..15 the
\t *  source (a body id, or 0x8000 | the dye pin's index), bits 16..23 the
\t *  weight.  0 where there is no dye. */
\tquint32 dyeWordAt( int px, int py ) const;
''', 'after'),
    ('\tPlane idPlane, flowPlane, shorePlane;\n',
     '\tPlane dyePlane;\n', 'after'),
    ('\tQByteArray packFlowPlane( quint64 base, QString * error ) const;\n',
     '''\t//! The dye plane, packed at the flow plane's rate, 4 bytes a sample.
\tQByteArray packDyePlane( quint64 base, QString * error ) const;
\tquint32 dyeWordOf( int px, int py, quint16 id ) const;
\t//! One body's field: its window, mask, depth and constraints -> solved.
\tbool solveBody( int id, WaterMarkSolve & st, QString * note );
\t//! The plumes: every DyeMouth and DyePin, in the store's order.
\tvoid solveDye( WaterMarkSolve & st );
''', 'after'),
    ('\tquint64 oBody = 0, oName = 0, oId = 0, oFlow = 0, oShore = 0, oStroke = 0;\n',
     '\tquint64 oDye = 0;\n', 'after'),
])
