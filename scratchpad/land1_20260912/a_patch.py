"""Lane LAND1 Part A (TILING5) -- terrain-guided land sampling, as a REFUSING
patch script rather than a set of hand edits.

Every anchor is asserted exact-once (or exact-twice where the edit belongs at
BOTH sampling sites -- the reason this is a script at all: `src/lodgen.cpp`'s
own comments record that lane TILING2 lost a relink by editing one `sampleLtex`
and not the other).  Every target file is asserted LF-only before and after.
`--check` counts the anchors and writes nothing.

    python a_patch.py --check
    python a_patch.py
"""
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '..', '..'))
CPP = os.path.join(ROOT, 'src', 'lodgen.cpp')
HDR = os.path.join(ROOT, 'src', 'lodgen.h')
CLI = os.path.join(ROOT, 'src', 'nifcli.cpp')

T = '\t'

# ---------------------------------------------------------------- lodgen.cpp

WARP_HEAD_OLD = (
    'void lodgenLandWarp( float wx, float wy, float * wxOut, float * wyOut )\n'
    '{\n'
    + T + '/* OFF IS A RETURN, not a multiply by zero: the rung\'s bytes are reached by\n'
    + T + ' * not touching the coordinate at all. */\n'
    + T + 'if ( g_landWarpAmp <= 0.0f || g_landWarpOctaves < 1 ) {\n'
    + T + T + '*wxOut = wx;\n'
    + T + T + '*wyOut = wy;\n'
    + T + T + 'return;\n'
    + T + '}\n'
    + T + 'double ox = 0.0, oy = 0.0;\n'
    + T + 'double a = double( g_landWarpAmp );\n'
    + T + 'double l = double( g_landWarpLattice );\n'
)

WARP_HEAD_NEW = (
    '/*! The warp at an EXPLICIT amplitude.\n'
    ' *\n'
    ' *  Lane LAND1 lifted the amplitude out of the global so the terrain-guided\n'
    ' *  rules below can MODULATE it by the macro slope without a second copy of\n'
    ' *  the octave loop.  `lodgenLandWarp` is this function at the configured\n'
    ' *  amplitude, term for term, so the shipped behaviour is unchanged and the\n'
    ' *  default is still a return. */\n'
    'static void lodgenLandWarpAt( float wx, float wy, double amp,\n'
    '                              float * wxOut, float * wyOut )\n'
    '{\n'
    + T + '/* OFF IS A RETURN, not a multiply by zero: the rung\'s bytes are reached by\n'
    + T + ' * not touching the coordinate at all. */\n'
    + T + 'if ( amp <= 0.0 || g_landWarpOctaves < 1 ) {\n'
    + T + T + '*wxOut = wx;\n'
    + T + T + '*wyOut = wy;\n'
    + T + T + 'return;\n'
    + T + '}\n'
    + T + 'double ox = 0.0, oy = 0.0;\n'
    + T + 'double a = amp;\n'
    + T + 'double l = double( g_landWarpLattice );\n'
)

WARP_TAIL_OLD = (
    T + '*wxOut = float( double( wx ) + ox );\n'
    + T + '*wyOut = float( double( wy ) + oy );\n'
    '}\n'
    '\n'
    'float lodgenLandWarpAmp()\n'
)

WARP_TAIL_NEW = (
    T + '*wxOut = float( double( wx ) + ox );\n'
    + T + '*wyOut = float( double( wy ) + oy );\n'
    '}\n'
    '\n'
    'void lodgenLandWarp( float wx, float wy, float * wxOut, float * wyOut )\n'
    '{\n'
    + T + 'lodgenLandWarpAt( wx, wy, double( g_landWarpAmp ), wxOut, wyOut );\n'
    '}\n'
    '\n'
    'float lodgenLandWarpAmp()\n'
)

HEX_ANCHOR = ('/* --- THE HISTOGRAM-PRESERVING HEX TILING (lane TILING4) '
              '---------------------\n')

GUIDE_BLOCK = '''/* --- TERRAIN-GUIDED LAND SAMPLING (lane LAND1, bungo 2026-09-12) -----------
 *
 * His words: "since we're reusing vanilla terain normals and slope maps, might
 * as well use them to guide this a bit", after asking "what is used for the
 * land sample warp? the normal or slope map?" -- the answer being neither: a
 * hashed value-noise lattice on world X/Y, which knows nothing about the ground
 * it is decorating.
 *
 * THE INPUT IS THE HEIGHTMAP, NOT THE SHEET.  The macro gradient below is a
 * Sobel difference of the SAME ring height grid the `_msn` normal a few lines
 * above the land lookup is built from, at a half-step of `--land-guide-scale`/2
 * world units.  Reading it off the heightmap rather than off vanilla's `_msn`
 * sheet is what makes it continuous across a chunk, a tile and a region border:
 * the grid is filled over a ONE-CELL ring (4,096 units) by the shared filler,
 * the deepest stencil this file asks of it reaches scale/2 + the tile's own
 * border (256 units at the shipped geometry), and nothing inside a tile can
 * therefore reach the grid's clamped edge.  Two adjacent chunks baked apart and
 * baked together are the same bytes because every term is a pure function of
 * WORLD position.
 *
 * FIVE RULES, each a switch, all off by default and all off by RETURN:
 *
 *   drag       the sample slides DOWNHILL by k * the macro normal's xy, so the
 *              texture lags the slope.  A smooth field, so it has strain, and
 *              the strain is what the swirl instrument (lane TILING4) reads.
 *   aspect     the sampling frame is ROTATED by the downhill azimuth, about the
 *              centre of the macro lattice cell the texel is in, blended toward
 *              identity by the macro slope.  A blend of the identity and a
 *              rotation about one anchor is a SIMILARITY -- uniform scale and
 *              rotation, zero shear -- so inside a cell the strain is
 *              isotropic and the orientation instrument cannot see it.  The
 *              price is a discontinuity at the lattice lines, and it is a real
 *              one: it is what the gates have to price.
 *   aspecthex  the same rotation, but carried by the HEX lattice's three taps
 *              instead of a square one: each of `lodgenLandHexTap`'s vertices
 *              rotates the plane about ITSELF by the macro azimuth there, and
 *              the barycentric variance-preserving blend that already joins the
 *              three offsets joins the three rotations.  Seamless AND
 *              shear-free by construction.  Needs `--land-hex`.
 *   slopewarp  TILING3's hash warp with its amplitude scaled by the macro
 *              slope -- weak on the flats, full on the slopes.
 *   flatwarp   the same, the other way round.  Both exist because which
 *              direction helps is a measurement, not an opinion.
 *
 * WHY THE ROTATION IS NOT WEIGHTED ON THE ANGLE.  `atan2` has a branch cut at
 * due west; multiplying the ANGLE by a weight below 1 turns that cut into a
 * visible discontinuity of up to 2*pi*weight.  The weight is applied to the
 * MAP instead -- p + w * ( R(p) - p ) -- which is continuous across the cut
 * because cos and sin are, and which stays shear-free (it is (1-w)I + wR, a
 * complex number times the plane).
 *
 * The rule NUMBERS live in lodgen.h beside the declarations, each with its own
 * one-line description; there is no second copy of them here.
 */
'''

GUIDE_BLOCK += (
    'static int   g_landGuideRule     = LODGEN_LANDGUIDE_OFF;\n'
    'static float g_landGuideStrength = 1.0f;\n'
    'static float g_landGuideScale    = 1024.0f;   // world units\n'
    'static float g_landGuideSlopeRef = 0.5f;      // tangent; 0.5 == 26.6 degrees\n'
    '\n'
    '/*! Everything the guide needs to ask the ring height grid a question at a\n'
    ' *  WORLD position: the grid, its size, and the affine map from world units\n'
    ' *  to the grid coordinates both bakers already compute for the normal.\n'
    ' *\n'
    ' *  `ngx = wx / 128 + ngOffX`.  The chunk baker\'s offset is\n'
    ' *  ( RING_UNITS - chunkWorldX ) / 128 and the tile baker\'s is -ringWestX /\n'
    ' *  128; that one line is the whole difference between the two sites, which\n'
    ' *  is why the rules themselves need no second copy. */\n'
    'struct LodgenLandGuideCtx\n'
    '{\n'
    + T + 'const std::vector<float> * hgt = nullptr;\n'
    + T + 'int hn = 0;\n'
    + T + 'float ngOffX = 0.0f;\n'
    + T + 'float ngOffY = 0.0f;\n'
    '};\n'
    '\n'
    '/*! The terrain\'s LOW-PASS gradient at the guide scale, at a world position.\n'
    ' *\n'
    ' *  A Sobel 3x3 over the ring height grid at a half-step of scale/2 world\n'
    ' *  units: eight taps, and the perpendicular smoothing is what keeps a single\n'
    ' *  128-unit VHGT step out of the answer.  Returns dz/dx and dz/dy as a\n'
    ' *  TANGENT -- 1.0 is 45 degrees -- so DOWNHILL is ( -dzdx, -dzdy ), which is\n'
    ' *  the surface normal\'s own xy up to its length.\n'
    ' *\n'
    ' *  In double for the same reason the warp and the hex lattice are: the grid\n'
    ' *  coordinate comes from a world coordinate that reaches +-2,000,000 units. */\n'
    'static void lodgenLandMacroGradient( const LodgenLandGuideCtx & ctx,\n'
    '                                     double wx, double wy,\n'
    '                                     double * dzdx, double * dzdy )\n'
    '{\n'
    + T + '*dzdx = 0.0;\n'
    + T + '*dzdy = 0.0;\n'
    + T + 'if ( !ctx.hgt || ctx.hn < 2 )\n'
    + T + T + 'return;\n'
    + T + 'const double s = double( g_landGuideScale ) * 0.5;      // world units\n'
    + T + 'const double gs = s / 128.0;                            // grid steps\n'
    + T + 'const double cx = wx / 128.0 + double( ctx.ngOffX );\n'
    + T + 'const double cy = wy / 128.0 + double( ctx.ngOffY );\n'
    + T + 'double h[3][3];\n'
    + T + 'for ( int j = -1; j <= 1; j++ )\n'
    + T + T + 'for ( int i = -1; i <= 1; i++ )\n'
    + T + T + T + 'h[j + 1][i + 1] = double( lodgenTerrainHeightAt( *ctx.hgt, ctx.hn,\n'
    + T + T + T + T + 'float( cx + double( i ) * gs ), float( cy + double( j ) * gs ) ) );\n'
    + T + 'const double gx = ( h[0][2] + 2.0 * h[1][2] + h[2][2] )\n'
    + T + T + '- ( h[0][0] + 2.0 * h[1][0] + h[2][0] );\n'
    + T + 'const double gy = ( h[2][0] + 2.0 * h[2][1] + h[2][2] )\n'
    + T + T + '- ( h[0][0] + 2.0 * h[0][1] + h[0][2] );\n'
    + T + '*dzdx = gx / ( 8.0 * s );\n'
    + T + '*dzdy = gy / ( 8.0 * s );\n'
    '}\n'
    '\n'
    '/*! The macro slope\'s weight, 0 on the flat and 1 at the reference slope. */\n'
    'static inline double lodgenLandGuideWeight( double tangent )\n'
    '{\n'
    + T + 'const double ref = double( g_landGuideSlopeRef ) > 1e-6\n'
    + T + T + '? double( g_landGuideSlopeRef ) : 1e-6;\n'
    + T + 'const double w = tangent / ref;\n'
    + T + 'return w >= 1.0 ? 1.0 : w;\n'
    '}\n'
    '\n'
    '/*! The guided rotation of one point about one anchor.\n'
    ' *\n'
    ' *  `( ax, ay )` is the anchor the rotation is rigid about; the azimuth and\n'
    ' *  the weight are read at the ANCHOR, not at the point, which is what makes\n'
    ' *  the map inside one lattice cell a similarity rather than a shear. */\n'
    'static void lodgenLandGuideRotate( const LodgenLandGuideCtx & ctx,\n'
    '                                   double ax, double ay,\n'
    '                                   double * px, double * py )\n'
    '{\n'
    + T + 'double dzdx = 0.0, dzdy = 0.0;\n'
    + T + 'lodgenLandMacroGradient( ctx, ax, ay, &dzdx, &dzdy );\n'
    + T + 'const double gx = -dzdx, gy = -dzdy;                    // downhill\n'
    + T + 'const double t = std::sqrt( gx * gx + gy * gy );\n'
    + T + 'if ( t <= 1e-9 )\n'
    + T + T + 'return;\n'
    + T + 'double w = lodgenLandGuideWeight( t ) * double( g_landGuideStrength );\n'
    + T + 'if ( w <= 0.0 )\n'
    + T + T + 'return;\n'
    + T + 'if ( w > 1.0 )\n'
    + T + T + 'w = 1.0;\n'
    + T + '/* cos/sin of the RAW azimuth: continuous across atan2\'s branch cut,\n'
    + T + ' * which weighting the angle instead would have turned into a seam. */\n'
    + T + 'const double inv = 1.0 / t;\n'
    + T + 'const double cs = gx * inv, sn = gy * inv;\n'
    + T + 'const double rx = *px - ax, ry = *py - ay;\n'
    + T + 'const double qx = ax + rx * cs - ry * sn;\n'
    + T + 'const double qy = ay + rx * sn + ry * cs;\n'
    + T + '*px += w * ( qx - *px );\n'
    + T + '*py += w * ( qy - *py );\n'
    '}\n'
    '\n'
    '/*! The land diffuse lookup\'s coordinate: the terrain-guided rules, then the\n'
    ' *  hash warp, exactly as the two lines this replaces did.\n'
    ' *\n'
    ' *  OFF IS A RETURN twice over: with no rule this is `lodgenLandWarp`, which\n'
    ' *  at amplitude 0 does not touch the coordinate -- so the default bake at\n'
    ' *  both sampling sites is the rung\'s bytes. */\n'
    'static void lodgenLandGuidedWarp( const LodgenLandGuideCtx & ctx,\n'
    '                                  float wx, float wy, float mdzdx, float mdzdy,\n'
    '                                  float * wxOut, float * wyOut )\n'
    '{\n'
    + T + 'if ( g_landGuideRule == LODGEN_LANDGUIDE_OFF ) {\n'
    + T + T + 'lodgenLandWarp( wx, wy, wxOut, wyOut );\n'
    + T + T + 'return;\n'
    + T + '}\n'
    + T + 'const double gx = -double( mdzdx ), gy = -double( mdzdy );   // downhill\n'
    + T + 'const double t = std::sqrt( gx * gx + gy * gy );\n'
    + T + 'double sx = double( wx ), sy = double( wy );\n'
    + T + 'double amp = double( g_landWarpAmp );\n'
    + T + 'switch ( g_landGuideRule ) {\n'
    + T + 'case LODGEN_LANDGUIDE_DRAG: {\n'
    + T + T + '/* the macro NORMAL\'s xy, which is the downhill unit vector times the\n'
    + T + T + ' * sine of the slope angle -- k is therefore world units at a vertical\n'
    + T + T + ' * face and about k * tangent on gentle ground, and it is bounded. */\n'
    + T + T + 'const double len = std::sqrt( 1.0 + t * t );\n'
    + T + T + 'sx += double( g_landGuideStrength ) * gx / len;\n'
    + T + T + 'sy += double( g_landGuideStrength ) * gy / len;\n'
    + T + T + 'break;\n'
    + T + '}\n'
    + T + 'case LODGEN_LANDGUIDE_ASPECT: {\n'
    + T + T + '/* the macro lattice cell CENTRE, anchored on world coordinates and\n'
    + T + T + ' * not on the tile, or two tiles would disagree at their border */\n'
    + T + T + 'const double L = double( g_landGuideScale ) > 1.0\n'
    + T + T + T + '? double( g_landGuideScale ) : 1.0;\n'
    + T + T + 'const double ax = ( std::floor( sx / L ) + 0.5 ) * L;\n'
    + T + T + 'const double ay = ( std::floor( sy / L ) + 0.5 ) * L;\n'
    + T + T + 'lodgenLandGuideRotate( ctx, ax, ay, &sx, &sy );\n'
    + T + T + 'break;\n'
    + T + '}\n'
    + T + 'case LODGEN_LANDGUIDE_ASPECTHEX:\n'
    + T + T + '/* carried by the hex tap, per lattice vertex; nothing to do to the\n'
    + T + T + ' * coordinate here */\n'
    + T + T + 'break;\n'
    + T + 'case LODGEN_LANDGUIDE_SLOPEWARP:\n'
    + T + T + 'amp *= lodgenLandGuideWeight( t ) * double( g_landGuideStrength );\n'
    + T + T + 'break;\n'
    + T + 'case LODGEN_LANDGUIDE_FLATWARP:\n'
    + T + T + 'amp *= ( 1.0 - lodgenLandGuideWeight( t ) )\n'
    + T + T + T + '* double( g_landGuideStrength );\n'
    + T + T + 'break;\n'
    + T + 'default:\n'
    + T + T + 'break;\n'
    + T + '}\n'
    + T + 'lodgenLandWarpAt( float( sx ), float( sy ), amp, wxOut, wyOut );\n'
    '}\n'
    '\n'
    'int lodgenLandGuideRule()\n'
    '{\n'
    + T + 'return g_landGuideRule;\n'
    '}\n'
    '\n'
    'void lodgenSetLandGuideRule( int rule )\n'
    '{\n'
    + T + '/* Refused, not clamped: an unknown rule number is a typo and must not\n'
    + T + ' * silently become one of the five. */\n'
    + T + 'if ( rule >= LODGEN_LANDGUIDE_OFF && rule <= LODGEN_LANDGUIDE_FLATWARP )\n'
    + T + T + 'g_landGuideRule = rule;\n'
    '}\n'
    '\n'
    'float lodgenLandGuideStrength()\n'
    '{\n'
    + T + 'return g_landGuideStrength;\n'
    '}\n'
    '\n'
    'void lodgenSetLandGuideStrength( float k )\n'
    '{\n'
    + T + 'g_landGuideStrength = k;\n'
    '}\n'
    '\n'
    'float lodgenLandGuideScale()\n'
    '{\n'
    + T + 'return g_landGuideScale;\n'
    '}\n'
    '\n'
    'void lodgenSetLandGuideScale( float units )\n'
    '{\n'
    + T + '/* Bounded above by the RING, not by taste: the height grid carries one\n'
    + T + ' * cell (4,096 units) outside the tile and the tile\'s own border eats 256\n'
    + T + ' * of it at the shipped geometry, so a Sobel half-step past 2,048 units\n'
    + T + ' * would read the grid\'s clamped edge and the answer would depend on\n'
    + T + ' * which tile asked. Below 128 it is finer than the VHGT grid itself. */\n'
    + T + 'if ( units >= 128.0f && units <= 2048.0f )\n'
    + T + T + 'g_landGuideScale = units;\n'
    '}\n'
    '\n'
    'float lodgenLandGuideSlopeRef()\n'
    '{\n'
    + T + 'return g_landGuideSlopeRef;\n'
    '}\n'
    '\n'
    'void lodgenSetLandGuideSlopeRef( float tangent )\n'
    '{\n'
    + T + 'if ( tangent > 0.0f )\n'
    + T + T + 'g_landGuideSlopeRef = tangent;\n'
    '}\n'
    '\n'
)

# ---- the hex tap: the guide context parameter and the per-vertex rotation

HEXSIG_OLD = (
    'static FloatVector4 lodgenLandHexTap( const DDSTexture16 * tex,\n'
    '                                      float swx, float swy, float tile,\n'
    '                                      float u, float v, float mip, float maxMip )\n'
    '{\n'
    + T + 'if ( g_landHexSize <= 0.0f )\n'
)

HEXSIG_NEW = (
    'static FloatVector4 lodgenLandHexTap( const DDSTexture16 * tex,\n'
    '                                      float swx, float swy, float tile,\n'
    '                                      float u, float v, float mip, float maxMip,\n'
    '                                      const LodgenLandGuideCtx * guide )\n'
    '{\n'
    + T + 'if ( g_landHexSize <= 0.0f )\n'
)

HEXTAP_OLD = (
    T + T + 'const double ox = lodgenLandHexOffset( vi[k], vj[k], 0 ) * double( tile );\n'
    + T + T + 'const double oy = lodgenLandHexOffset( vi[k], vj[k], 1 ) * double( tile );\n'
    + T + T + '/* wrap by hand exactly as the caller does: getPixelT clamps and the\n'
    + T + T + ' * tiling is ours */\n'
    + T + T + 'double tu = std::fmod( ( double( swx ) + ox ) / double( tile ), 1.0 );\n'
    + T + T + 'double tv = std::fmod( ( double( swy ) + oy ) / double( tile ), 1.0 );\n'
)

HEXTAP_NEW = (
    T + T + 'const double ox = lodgenLandHexOffset( vi[k], vj[k], 0 ) * double( tile );\n'
    + T + T + 'const double oy = lodgenLandHexOffset( vi[k], vj[k], 1 ) * double( tile );\n'
    + T + T + '/* THE TERRAIN-GUIDED ROTATION (lane LAND1), carried per lattice\n'
    + T + T + ' * VERTEX: each tap rotates the plane about its own vertex by the\n'
    + T + T + ' * macro azimuth measured THERE, so every tap is a similarity and\n'
    + T + T + ' * the shear is exactly zero, and the barycentric blend below joins\n'
    + T + T + ' * the three rotations the same way it already joins the three\n'
    + T + T + ' * offsets -- no seam at a lattice edge.  Off, `px`/`py` are the\n'
    + T + T + ' * caller\'s own coordinate and the line is what it was. */\n'
    + T + T + 'double px = double( swx ), py = double( swy );\n'
    + T + T + 'if ( guide && g_landGuideRule == LODGEN_LANDGUIDE_ASPECTHEX ) {\n'
    + T + T + T + 'double vx = 0.0, vy = 0.0;\n'
    + T + T + T + 'lodgenLandHexVertexPos( vi[k], vj[k], double( g_landHexSize ),\n'
    + T + T + T + T + '&vx, &vy );\n'
    + T + T + T + 'lodgenLandGuideRotate( *guide, vx, vy, &px, &py );\n'
    + T + T + '}\n'
    + T + T + '/* wrap by hand exactly as the caller does: getPixelT clamps and the\n'
    + T + T + ' * tiling is ours */\n'
    + T + T + 'double tu = std::fmod( ( px + ox ) / double( tile ), 1.0 );\n'
    + T + T + 'double tv = std::fmod( ( py + oy ) / double( tile ), 1.0 );\n'
)

# the lattice vertex -> world position, inserted beside lodgenLandHexOffset
HEXOFF_OLD = (
    'static inline double lodgenLandHexOffset( qint32 i, qint32 j, quint32 k )\n'
    '{\n'
    + T + 'return double( lodgenWarpHash( i, j, k ) ) / 4294967296.0;\n'
    '}\n'
)

HEXOFF_NEW = (
    'static inline double lodgenLandHexOffset( qint32 i, qint32 j, quint32 k )\n'
    '{\n'
    + T + 'return double( lodgenWarpHash( i, j, k ) ) / 4294967296.0;\n'
    '}\n'
    '\n'
    '/*! The WORLD position of one lattice vertex -- the inverse of the skew in\n'
    ' *  `lodgenLandHexCell`, spelled out rather than re-derived at the call site\n'
    ' *  (lane LAND1 needs it to anchor a per-vertex rotation).\n'
    ' *\n'
    ' *      sx = px - SKEW * py,  sy = SCALE * py   =>   py = sy / SCALE,\n'
    ' *      px = sx + SKEW * sy / SCALE,            and world = ( px, py ) * size. */\n'
    'static inline void lodgenLandHexVertexPos( qint32 i, qint32 j, double size,\n'
    '                                           double * wx, double * wy )\n'
    '{\n'
    + T + 'const double py = double( j ) / LODGEN_HEX_SCALE;\n'
    + T + 'const double px = double( i ) + LODGEN_HEX_SKEW * py;\n'
    + T + '*wx = px * size;\n'
    + T + '*wy = py * size;\n'
    '}\n'
)

# ---- the two sampling sites

SITEA_OLD = (
    T + T + T + 'msn[size_t( py ) * RES + px] = lodgenTerrainMsnPixel( nrm );\n'
)

SITEA_NEW = (
    T + T + T + 'msn[size_t( py ) * RES + px] = lodgenTerrainMsnPixel( nrm );\n'
    '\n'
    + T + T + T + '/* THE MACRO GRADIENT (lane LAND1): the terrain\'s own low-pass\n'
    + T + T + T + ' * slope at --land-guide-scale, measured ONCE a texel on the same\n'
    + T + T + T + ' * ring height grid the normal above comes from, and handed to the\n'
    + T + T + T + ' * land diffuse lookup below. Off, nothing is computed. */\n'
    + T + T + T + 'LodgenLandGuideCtx lguide;\n'
    + T + T + T + 'lguide.hgt = &hgt;\n'
    + T + T + T + 'lguide.hn = hn;\n'
    + T + T + T + 'lguide.ngOffX = ( LODGEN_TERRAIN_RING_UNITS - cwX ) / 128.0f;\n'
    + T + T + T + 'lguide.ngOffY = ( LODGEN_TERRAIN_RING_UNITS - cwY ) / 128.0f;\n'
    + T + T + T + 'float mgx = 0.0f, mgy = 0.0f;\n'
    + T + T + T + 'if ( lodgenLandGuideRule() != LODGEN_LANDGUIDE_OFF ) {\n'
    + T + T + T + T + 'double gdx = 0.0, gdy = 0.0;\n'
    + T + T + T + T + 'lodgenLandMacroGradient( lguide, double( wx ), double( wy ),\n'
    + T + T + T + T + T + '&gdx, &gdy );\n'
    + T + T + T + T + 'mgx = float( gdx );\n'
    + T + T + T + T + 'mgy = float( gdy );\n'
    + T + T + T + '}\n'
)

SITEB_OLD = (
    T + T + T + 'out.msn[size_t( j ) * S + i] = lodgenTerrainMsnPixel( nrm );\n'
)

SITEB_NEW = (
    T + T + T + 'out.msn[size_t( j ) * S + i] = lodgenTerrainMsnPixel( nrm );\n'
    '\n'
    + T + T + T + '/* THE MACRO GRADIENT (lane LAND1). THIS is the site that writes\n'
    + T + T + T + ' * the shipped sheet with --vt on; the chunk site above carries the\n'
    + T + T + T + ' * same five lines, which is why the patch that made them is a\n'
    + T + T + T + ' * script. */\n'
    + T + T + T + 'LodgenLandGuideCtx lguide;\n'
    + T + T + T + 'lguide.hgt = &hgt;\n'
    + T + T + T + 'lguide.hn = hn;\n'
    + T + T + T + 'lguide.ngOffX = -ringW / 128.0f;\n'
    + T + T + T + 'lguide.ngOffY = -ringS / 128.0f;\n'
    + T + T + T + 'float mgx = 0.0f, mgy = 0.0f;\n'
    + T + T + T + 'if ( lodgenLandGuideRule() != LODGEN_LANDGUIDE_OFF ) {\n'
    + T + T + T + T + 'double gdx = 0.0, gdy = 0.0;\n'
    + T + T + T + T + 'lodgenLandMacroGradient( lguide, double( wx ), double( wy ),\n'
    + T + T + T + T + T + '&gdx, &gdy );\n'
    + T + T + T + T + 'mgx = float( gdx );\n'
    + T + T + T + T + 'mgy = float( gdy );\n'
    + T + T + T + '}\n'
)

WARPCALL_OLD = (
    T * 5 + 'float swx = wx, swy = wy;\n'
    + T * 5 + 'lodgenLandWarp( wx, wy, &swx, &swy );\n'
)

WARPCALL_NEW = (
    T * 5 + 'float swx = wx, swy = wy;\n'
    + T * 5 + 'lodgenLandGuidedWarp( lguide, wx, wy, mgx, mgy, &swx, &swy );\n'
)

HEXCALL_OLD = (
    T * 5 + 'const FloatVector4 fp =\n'
    + T * 6 + 'lodgenLandHexTap( tex, swx, swy, TILE, u, v, mip, maxMip );\n'
)

HEXCALL_NEW = (
    T * 5 + 'const FloatVector4 fp =\n'
    + T * 6 + 'lodgenLandHexTap( tex, swx, swy, TILE, u, v, mip, maxMip,\n'
    + T * 7 + '&lguide );\n'
)

# ------------------------------------------------------------------ lodgen.h

HDR_OLD = (
    'float lodgenLandHexSize();\n'
    'void lodgenSetLandHexSize( float units );        // 0 = off = the rung\'s bytes\n'
)

HDR_NEW = (
    'float lodgenLandHexSize();\n'
    'void lodgenSetLandHexSize( float units );        // 0 = off = the rung\'s bytes\n'
    '\n'
    '/* --- TERRAIN-GUIDED LAND SAMPLING (lane LAND1, bungo 2026-09-12) ----------\n'
    ' *\n'
    ' * bungo, after the warp sweep picture: "what is used for the land sample\n'
    ' * warp? the normal or slope map?" -- neither, a hashed lattice on world X/Y\n'
    ' * -- and then "since we\'re reusing vanilla terain normals and slope maps,\n'
    ' * might as well use them to guide this a bit".\n'
    ' *\n'
    ' * The guide is the TERRAIN\'S OWN LOW-PASS SLOPE, read off the ring height\n'
    ' * grid the `_msn` normal is built from (a Sobel 3x3 at a half-step of\n'
    ' * --land-guide-scale/2), NOT off vanilla\'s `_msn` sheet: the heightmap is\n'
    ' * continuous across chunk, tile and region borders and the sheet is not, and\n'
    ' * at the scales these rules use the two agree -- see the lane report\'s\n'
    ' * section 1 for the number.\n'
    ' *\n'
    ' * Five rules, all off by default and all off by RETURN, so the default bake\n'
    ' * is the rung\'s bytes at both sampling sites:\n'
    ' *   drag       the sample slides downhill by k * the macro normal\'s xy;\n'
    ' *   aspect     the sampling frame is rotated by the downhill azimuth about\n'
    ' *              the macro lattice cell\'s centre, weighted by slope;\n'
    ' *   aspecthex  the same rotation carried by the hex lattice\'s three taps,\n'
    ' *              which is seamless and shear-free by construction;\n'
    ' *   slopewarp  TILING3\'s hash warp, amplitude scaled by the macro slope;\n'
    ' *   flatwarp   the same, scaled by 1 - that.\n'
    ' *\n'
    ' * WHAT THE LANE MEASURED is in the report and in WW_CHANGES.md; nothing here\n'
    ' * becomes a default without bungo. */\n'
    'enum {\n'
    + T + 'LODGEN_LANDGUIDE_OFF       = 0,\n'
    + T + 'LODGEN_LANDGUIDE_DRAG      = 1,\n'
    + T + 'LODGEN_LANDGUIDE_ASPECT    = 2,\n'
    + T + 'LODGEN_LANDGUIDE_ASPECTHEX = 3,\n'
    + T + 'LODGEN_LANDGUIDE_SLOPEWARP = 4,\n'
    + T + 'LODGEN_LANDGUIDE_FLATWARP  = 5\n'
    '};\n'
    'int lodgenLandGuideRule();\n'
    'void lodgenSetLandGuideRule( int rule );         // 0 = off = the rung\'s bytes\n'
    'float lodgenLandGuideStrength();\n'
    'void lodgenSetLandGuideStrength( float k );\n'
    'float lodgenLandGuideScale();\n'
    'void lodgenSetLandGuideScale( float units );     // 128..2048, ring-bounded\n'
    'float lodgenLandGuideSlopeRef();\n'
    'void lodgenSetLandGuideSlopeRef( float tangent );\n'
)

# ----------------------------------------------------------------- nifcli.cpp

CLI_OLD = (
    T + T + 'else if ( t == QLatin1String( "--land-hex" ) ) lodgenSetLandHexSize( next().toFloat() );\n'
)

CLI_NEW = (
    T + T + 'else if ( t == QLatin1String( "--land-hex" ) ) lodgenSetLandHexSize( next().toFloat() );\n'
    + T + T + '/* TERRAIN-GUIDED LAND SAMPLING (lane LAND1), bungo 2026-09-12:\n'
    + T + T + ' * "since we\'re reusing vanilla terain normals and slope maps,\n'
    + T + T + ' * might as well use them to guide this a bit". The guide is the\n'
    + T + T + ' * HEIGHTMAP\'s own low-pass slope, not the `_msn` sheet, so it is\n'
    + T + T + ' * continuous across every border. `off` is the default and is the\n'
    + T + T + ' * rung\'s bytes; the strength after the colon means world units for\n'
    + T + T + ' * `drag`, a 0..1 fraction of the rotation for `aspect`/`aspecthex`,\n'
    + T + T + ' * and a multiplier on --land-warp\'s amplitude for the two warps. */\n'
    + T + T + 'else if ( t == QLatin1String( "--land-guide" ) ) {\n'
    + T + T + T + 'QString v = next().toLower();\n'
    + T + T + T + 'const int colon = v.indexOf( QLatin1Char( \':\' ) );\n'
    + T + T + T + 'if ( colon >= 0 ) {\n'
    + T + T + T + T + 'bool ok = false;\n'
    + T + T + T + T + 'const float k = v.mid( colon + 1 ).toFloat( &ok );\n'
    + T + T + T + T + 'if ( ok )\n'
    + T + T + T + T + T + 'lodgenSetLandGuideStrength( k );\n'
    + T + T + T + T + 'else\n'
    + T + T + T + T + T + 'fprintf( stderr, "lodgen: --land-guide %s has no readable '
    'strength after the colon; the default stands\\n",\n'
    + T + T + T + T + T + T + 'v.toLatin1().constData() );\n'
    + T + T + T + T + 'v = v.left( colon );\n'
    + T + T + T + '}\n'
    + T + T + T + 'if ( v == QLatin1String( "off" ) )\n'
    + T + T + T + T + 'lodgenSetLandGuideRule( LODGEN_LANDGUIDE_OFF );\n'
    + T + T + T + 'else if ( v == QLatin1String( "drag" ) )\n'
    + T + T + T + T + 'lodgenSetLandGuideRule( LODGEN_LANDGUIDE_DRAG );\n'
    + T + T + T + 'else if ( v == QLatin1String( "aspect" ) )\n'
    + T + T + T + T + 'lodgenSetLandGuideRule( LODGEN_LANDGUIDE_ASPECT );\n'
    + T + T + T + 'else if ( v == QLatin1String( "aspecthex" ) )\n'
    + T + T + T + T + 'lodgenSetLandGuideRule( LODGEN_LANDGUIDE_ASPECTHEX );\n'
    + T + T + T + 'else if ( v == QLatin1String( "slopewarp" ) )\n'
    + T + T + T + T + 'lodgenSetLandGuideRule( LODGEN_LANDGUIDE_SLOPEWARP );\n'
    + T + T + T + 'else if ( v == QLatin1String( "flatwarp" ) )\n'
    + T + T + T + T + 'lodgenSetLandGuideRule( LODGEN_LANDGUIDE_FLATWARP );\n'
    + T + T + T + 'else\n'
    + T + T + T + T + 'fprintf( stderr, "lodgen: --land-guide %s is not one of '
    'off|drag|aspect|aspecthex|slopewarp|flatwarp; off stands\\n",\n'
    + T + T + T + T + T + 'v.toLatin1().constData() );\n'
    + T + T + '}\n'
    + T + T + 'else if ( t == QLatin1String( "--land-guide-scale" ) ) '
    'lodgenSetLandGuideScale( next().toFloat() );\n'
    + T + T + 'else if ( t == QLatin1String( "--land-guide-slope" ) ) '
    'lodgenSetLandGuideSlopeRef( next().toFloat() );\n'
)

USAGE_OLD = (
    T + T + '  << "  lodgen ... --terrain-region ... [--land-tiling UNITS]\\n"\n'
)

USAGE_NEW = (
    T + T + '  << "  lodgen ... --terrain-region ... [--land-guide RULE[:K]]\\n"\n'
    + T + T + '  << "                                 [--land-guide-scale UNITS] '
    '[--land-guide-slope TAN]\\n"\n'
    + T + T + '  << "                                          terrain-guided land sampling '
    '(lane\\n"\n'
    + T + T + '  << "                                          LAND1): the land texture\'s '
    'phase is\\n"\n'
    + T + T + '  << "                                          steered by the HEIGHTMAP\'s own '
    'low-pass\\n"\n'
    + T + T + '  << "                                          slope at --land-guide-scale '
    '(128..2048,\\n"\n'
    + T + T + '  << "                                          default 1024). RULE is '
    'off (the\\n"\n'
    + T + T + '  << "                                          default, and the rung\'s bytes), '
    'drag,\\n"\n'
    + T + T + '  << "                                          aspect, aspecthex, slopewarp or '
    'flatwarp.\\n"\n'
    + T + T + '  << "                                          K is world units for drag, a '
    '0..1\\n"\n'
    + T + T + '  << "                                          fraction for the two aspects, and '
    'a\\n"\n'
    + T + T + '  << "                                          multiplier on --land-warp for the '
    'warps.\\n"\n'
    + T + T + '  << "  lodgen ... --terrain-region ... [--land-tiling UNITS]\\n"\n'
)


EDITS = [
    (CPP, WARP_HEAD_OLD, WARP_HEAD_NEW, 1),
    (CPP, WARP_TAIL_OLD, WARP_TAIL_NEW, 1),
    (CPP, HEX_ANCHOR, GUIDE_BLOCK + HEX_ANCHOR, 1),
    (CPP, HEXOFF_OLD, HEXOFF_NEW, 1),
    (CPP, HEXSIG_OLD, HEXSIG_NEW, 1),
    (CPP, HEXTAP_OLD, HEXTAP_NEW, 1),
    (CPP, SITEA_OLD, SITEA_NEW, 1),
    (CPP, SITEB_OLD, SITEB_NEW, 1),
    (CPP, WARPCALL_OLD, WARPCALL_NEW, 2),
    (CPP, HEXCALL_OLD, HEXCALL_NEW, 2),
    (HDR, HDR_OLD, HDR_NEW, 1),
    (CLI, CLI_OLD, CLI_NEW, 1),
    (CLI, USAGE_OLD, USAGE_NEW, 1),
]


def main():
    check = '--check' in sys.argv
    bufs = {}
    for p in (CPP, HDR, CLI):
        b = open(p, 'rb').read()
        if b.count(b'\r'):
            print('REFUSED: %s carries %d CR bytes; this script writes LF only'
                  % (p, b.count(b'\r')))
            return 2
        bufs[p] = b.decode('utf-8')
    bad = 0
    for path, old, new, want in EDITS:
        n = bufs[path].count(old)
        head = old.strip().split('\n')[0][:64]
        print('%-14s want %d found %d   %s'
              % (os.path.basename(path), want, n, head))
        if n != want:
            bad += 1
        elif not check:
            bufs[path] = bufs[path].replace(old, new)
    if bad:
        print('REFUSED: %d anchors did not match exactly' % bad)
        return 3
    if check:
        print('CHECK ONLY: %d anchors all matched, nothing written' % len(EDITS))
        return 0
    for p in (CPP, HDR, CLI):
        b = bufs[p].encode('utf-8')
        if b.count(b'\r'):
            print('REFUSED: the patched %s would carry CR bytes' % p)
            return 4
        with open(p, 'wb') as f:
            f.write(b)
        print('wrote %s  %d bytes  %d LF  %d CR'
              % (p, len(b), b.count(b'\n'), b.count(b'\r')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
