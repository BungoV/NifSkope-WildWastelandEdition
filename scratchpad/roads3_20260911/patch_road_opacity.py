"""ROADS3 -- add `--road-opacity` to the road pass.

Written as a FILE and run, never as a shell heredoc: lane ROADS2's MISTAKES
entry 4 says a heredoc carrying prose apostrophes dies with
`unexpected EOF while looking for matching '''`, and this lane repeated that
mistake once already.

Every replacement asserts its own count before it is written, so a patch that
matched the wrong number of sites refuses instead of writing.
"""
import io
import os
import sys

ROOT = r'E:\Projects\NifskopeWildWastelandEdition'


def read(p):
    with io.open(os.path.join(ROOT, p), 'r', encoding='utf-8', newline='') as f:
        return f.read()


def write(p, s):
    with io.open(os.path.join(ROOT, p), 'w', encoding='utf-8', newline='') as f:
        f.write(s)


def sub(s, old, new, want):
    n = s.count(old)
    if n != want:
        sys.exit('REFUSED: expected %d of %r, found %d' % (want, old[:60], n))
    return s.replace(old, new)


# --------------------------------------------------------------- lodgen.h
h = read('src/lodgen.h')
OLD_H = "\tfloat roadCoverSuppress = 1.0f;\n"
NEW_H = OLD_H + """
\t/*! HOW STRONGLY the road paint is mixed into the ground it lies on --
\t *  the alpha of the road plane is scaled by this before the composite
\t *  `colour = ground + ( roadColour - ground ) * a * roadOpacity`.
\t *
\t *  1.0 is the shipped default and is the bake as it has always been: the
\t *  multiply is branched over entirely at 1.0, so off is the previous
\t *  bake's BYTES and not a float argument about 1.0f. 0.0 leaves the
\t *  ground untouched by the paint while the road GEOMETRY is still there
\t *  -- the ground-cover suppression below deliberately keeps using the
\t *  UNSCALED coverage, because a faintly painted road is still a road and
\t *  grass still does not grow through it.
\t *
\t *  WHY THE KNOB EXISTS (lane ROADS3, measured on the rung's own sheets,
\t *  every number beside the floor it was read against):
\t *
\t *    vanilla's far road stands +4.29 luminance levels over the ground
\t *    around it on chunk (-20,20) and +4.40 on (-8,8) -- two tiles a whole
\t *    biome apart -- while ours stands +29.96 and +3.84. So ours is right
\t *    on one tile (0.56 of a level) and 25.67 levels too contrasty on the
\t *    other, which is the one bungo looked at.
\t *
\t *    the reason is that our paint is a FIXED material colour while
\t *    vanilla's road follows the ground under it. Road luminance regressed
\t *    on the mean luminance of the non-road texels within 8 texels:
\t *    vanilla +0.714 and +0.755, our own unpainted ground +0.637 and
\t *    +0.565, ours +0.339 and +0.209. Floors, both sides: the same field
\t *    translated reads -0.009 mean and 0.298 worst over 5 draws, a
\t *    known-answer fixed paint reads +0.000, a known-answer ground+4 reads
\t *    +1.000.
\t *
\t *  It is NOT set away from 1.0 here, and that is a refusal with numbers,
\t *  not an omission. No single opacity meets this lane's gates at once: on
\t *  (-20,20) vanilla's absolute level wants 0.83, vanilla's rise over the
\t *  ground wants 0.326, the two-tone step wants 0.25 or less and the local
\t *  detail SD wants 0.75 or more; on (-8,8) NO opacity can reach vanilla's
\t *  road at all, because the composite can only land between our ground
\t *  (102.12) and our paint (106.68) and vanilla's road is at 94.59, 7.53
\t *  levels outside that interval. The gap is the GROUND's, which lane
\t *  TILING2 measured and told this lane in writing not to chase with the
\t *  road pass. The default is bungo's call; the priced table and the
\t *  pictures are in scratchpad/lane_roads3_report.md.
\t *
\t *  NOT a hue knob, and that is measured too: road minus surround on the
\t *  opponent axes reads vanilla +2.30 / -2.14 and ours +3.35 / -3.25 on
\t *  (-20,20), vanilla +4.10 / -3.07 and ours +1.96 / -1.24 on (-8,8) --
\t *  same sign, same direction, every gap under 3 levels. The difference
\t *  bungo sees is brightness, not colour. */
\tfloat roadOpacity = 1.0f;
"""
h = sub(h, OLD_H, NEW_H, 1)
write('src/lodgen.h', h)
print('lodgen.h: roadOpacity field added')

# ------------------------------------------------------------- lodgen.cpp
c = read('src/lodgen.cpp')

SITE1_KEY = 'const quint32 rp = roadPlane[size_t( py ) * RES + px];'
SITE2_KEY = 'const quint32 rp = roadPlane[size_t( j ) * S + i];'

OLD_RA = """\t\t\t\t\tconst float ra = float( rp >> 24 ) / 255.0f;
\t\t\t\t\tif ( ra > 0.0f ) {
"""

NEW_RA_1 = """\t\t\t\t\t/* THE ROAD OPACITY (lane ROADS3). Two different things
\t\t\t\t\t * come out of the road plane's alpha and they must not be
\t\t\t\t\t * confused:
\t\t\t\t\t *
\t\t\t\t\t *   `raGeom` is COVERAGE -- how much of this texel the road
\t\t\t\t\t *   mesh actually covers. The ground-cover suppression below
\t\t\t\t\t *   keeps using it unscaled, because a faintly painted road
\t\t\t\t\t *   is still a road and grass still does not grow through it.
\t\t\t\t\t *
\t\t\t\t\t *   `ra` is how strongly the paint is mixed in. At the
\t\t\t\t\t *   shipped default of 1.0 the multiply is not done at all
\t\t\t\t\t *   and the colour branch is entered on exactly the same
\t\t\t\t\t *   condition as before, so the off value is the previous
\t\t\t\t\t *   bake's BYTES by construction rather than by a float
\t\t\t\t\t *   argument about 1.0f.
\t\t\t\t\t *
\t\t\t\t\t * Why the knob exists, and why it is not set away from 1.0
\t\t\t\t\t * by default, is in LodgenCoverOptions::roadOpacity with the
\t\t\t\t\t * numbers and the floors. */
\t\t\t\t\tconst float raGeom = float( rp >> 24 ) / 255.0f;
\t\t\t\t\tconst float ra = ( coverOpts.roadOpacity == 1.0f )
\t\t\t\t\t\t? raGeom : raGeom * coverOpts.roadOpacity;
\t\t\t\t\tif ( raGeom > 0.0f ) {
"""

NEW_RA_2 = """\t\t\t\t\t/* Coverage and paint strength, the same two things and
\t\t\t\t\t * the same rule as the chunk path's -- see there. */
\t\t\t\t\tconst float raGeom = float( rp >> 24 ) / 255.0f;
\t\t\t\t\tconst float ra = ( coverOpts.roadOpacity == 1.0f )
\t\t\t\t\t\t? raGeom : raGeom * coverOpts.roadOpacity;
\t\t\t\t\tif ( raGeom > 0.0f ) {
"""

OLD_MIX = """\t\t\t\t\t\tfor ( int k = 0; k < 3; k++ )
\t\t\t\t\t\t\tcolor[k] = color[k] + ( rc[k] - color[k] ) * ra;
"""
NEW_MIX = """\t\t\t\t\t\tif ( ra > 0.0f )
\t\t\t\t\t\t\tfor ( int k = 0; k < 3; k++ )
\t\t\t\t\t\t\t\tcolor[k] = color[k] + ( rc[k] - color[k] ) * ra;
"""

OLD_KEEP = """\t\t\t\t\t\t\tconst float keep = qBound( 0.0f,
\t\t\t\t\t\t\t\t1.0f - ra * coverOpts.roadCoverSuppress, 1.0f );
"""
NEW_KEEP = """\t\t\t\t\t\t\tconst float keep = qBound( 0.0f,
\t\t\t\t\t\t\t\t1.0f - raGeom * coverOpts.roadCoverSuppress, 1.0f );
"""

for key, new_ra in ((SITE1_KEY, NEW_RA_1), (SITE2_KEY, NEW_RA_2)):
    at = c.find(key)
    if at < 0:
        sys.exit('REFUSED: composite site not found: %s' % key)
    seg_start = at + len(key) + 1
    seg = c[seg_start:seg_start + len(OLD_RA)]
    if seg != OLD_RA:
        sys.exit('REFUSED: unexpected text after %s:\n%r' % (key, seg))
    c = c[:seg_start] + new_ra + c[seg_start + len(OLD_RA):]

c = sub(c, OLD_MIX, NEW_MIX, 2)
c = sub(c, OLD_KEEP, NEW_KEEP, 2)

OLD_CENSUS = ("\t\tr << QString( \"roadDetail %1\" ).arg"
              "( double( opts.cover.roadDetail ), 0, 'f', 3 );\n")
NEW_CENSUS = OLD_CENSUS + (
    "\t\tr << QString( \"roadOpacity %1\" ).arg"
    "( double( opts.cover.roadOpacity ), 0, 'f', 3 );\n")
c = sub(c, OLD_CENSUS, NEW_CENSUS, 1)
write('src/lodgen.cpp', c)
print('lodgen.cpp: both composite sites + census line patched')

# -------------------------------------------------------------- nifcli.cpp
n = read('src/nifcli.cpp')

OLD_DECL = """\tbool lgRoadDetailSet = false, lgRoadRaisedSet = false,
\t\tlgRoadSidewalksSet = false, lgRoadsLegacy = false;
"""
NEW_DECL = """\tbool lgRoadDetailSet = false, lgRoadRaisedSet = false,
\t\tlgRoadSidewalksSet = false, lgRoadsLegacy = false,
\t\tlgRoadOpacitySet = false;
"""
n = sub(n, OLD_DECL, NEW_DECL, 1)

OLD_PARSE = """\t\telse if ( t == QLatin1String( "--road-cover-suppress" ) )
\t\t\tlgCover.roadCoverSuppress = next().toFloat();
"""
NEW_PARSE = OLD_PARSE + """\t\t/* HOW STRONGLY the road paint is mixed into the ground under it
\t\t * (lane ROADS3). 1 is the default and is branched over, so the off
\t\t * value is the previous bake's bytes; 0 paints nothing while the
\t\t * road geometry still suppresses the ground cover. The numbers that
\t\t * decided the default are on LodgenCoverOptions::roadOpacity. */
\t\telse if ( t == QLatin1String( "--road-opacity" ) ) {
\t\t\tlgCover.roadOpacity = qBound( 0.0f, next().toFloat(), 1.0f );
\t\t\tlgRoadOpacitySet = true;
\t\t}
"""
n = sub(n, OLD_PARSE, NEW_PARSE, 1)

OLD_LEGACY = """\tif ( lgRoadsLegacy ) {
\t\tif ( !lgRoadDetailSet )
\t\t\tlgCover.roadDetail = 1.0f;
"""
NEW_LEGACY = """\tif ( lgRoadsLegacy ) {
\t\tif ( !lgRoadDetailSet )
\t\t\tlgCover.roadDetail = 1.0f;
\t\t/* A no-op while the default is 1.0, and written anyway so that the
\t\t * way back stays the way back if the default is ever moved. */
\t\tif ( !lgRoadOpacitySet )
\t\t\tlgCover.roadOpacity = 1.0f;
"""
n = sub(n, OLD_LEGACY, NEW_LEGACY, 1)

OLD_USAGE = ('\t\t  << "  lodgen ... [--road-composite max-z|blend] '
             '[--road-detail 0..1]\\n"\n')
NEW_USAGE = (
    '\t\t  << "                                          --road-opacity A '
    '(default 1)\\n"\n'
    '\t\t  << "                                          scales how strongly '
    'the road\\n"\n'
    '\t\t  << "                                          paint is mixed into '
    'the ground\\n"\n'
    '\t\t  << "                                          under it: 1 is the '
    'bake as it\\n"\n'
    '\t\t  << "                                          has always been and '
    'the multiply\\n"\n'
    '\t\t  << "                                          is branched over, 0 '
    'paints\\n"\n'
    '\t\t  << "                                          nothing while the '
    'road still\\n"\n'
    '\t\t  << "                                          suppresses the ground '
    'cover\\n"\n'
    '\t\t  << "                                          under it. Vanilla\'s '
    'far road\\n"\n'
    '\t\t  << "                                          stands +4.29 and '
    '+4.40 levels\\n"\n'
    '\t\t  << "                                          over its surround on '
    'two tiles;\\n"\n'
    '\t\t  << "                                          ours stands +29.96 '
    'and +3.84, so\\n"\n'
    '\t\t  << "                                          the error is not the '
    'same on\\n"\n'
    '\t\t  << "                                          every chunk and no '
    'one value\\n"\n'
    '\t\t  << "                                          fixes both (lane '
    'ROADS3).\\n"\n'
) + OLD_USAGE
n = sub(n, OLD_USAGE, NEW_USAGE, 1)
write('src/nifcli.cpp', n)
print('nifcli.cpp: declaration, parse, legacy and usage patched')
print('DONE')
