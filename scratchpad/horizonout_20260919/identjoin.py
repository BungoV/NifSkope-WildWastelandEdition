#!/usr/bin/env python3
"""HORIZONOUT step 9: THE PROXIMITY IDENTITY JOIN (bungo's ruling 2026-09-19).

The v7 group table's join rule stops being "an `architecture`-pathed placement
whose WORLD AXIS-ALIGNED BOX is within 16 u of another's" and becomes "any
NON-TREE placement whose drawn LOD MESH is within 64 u of another's". Lane
IDENTPROX measured both (scratchpad/identprox_20260919/RECOMMENDATION.txt):
the box measure at any gap welds a street, because an elevated highway deck's
box hangs over the buildings underneath it; the mesh measure does not, and 128 u
is the last gap at which no identity holds two different reference buildings.

`--identity-join legacy` is the way back, exactly the shipped rule.
"""
import sys

CHECK = "--check" in sys.argv
ROOT = r"E:/Projects/NifskopeWildWastelandEdition/"


class F:
    def __init__(self, rel):
        self.path = ROOT + rel
        with open(self.path, "rb") as f:
            raw = f.read()
        self.cr = raw.count(b"\r")
        self.d = raw.decode("utf-8")
        self.orig = self.d
        self.n = 0

    def sub(self, old, new, what):
        c = self.d.count(old)
        if c != 1:
            sys.exit("REFUSED [%s]: anchor appears %d times, want 1" % (what, c))
        self.d = self.d.replace(old, new)
        self.n += 1

    def save(self):
        name = self.path.rsplit("/", 1)[-1]
        print("%-16s %2d edits, %d -> %d chars, %d -> %d lines (CR %d -> %d)"
              % (name, self.n, len(self.orig), len(self.d),
                 self.orig.count("\n"), self.d.count("\n"),
                 self.cr, self.d.count("\r")))
        if self.d.count("\r") != self.cr:
            sys.exit("REFUSED: %s CR count moved" % name)
        if not CHECK:
            with open(self.path, "wb") as f:
                f.write(self.d.encode("utf-8"))


# ===========================================================================
# src/nativeemit.h -- the option
# ===========================================================================
h = F("src/nativeemit.h")
h.sub("""void lodgenNativeScrappableOption( bool scrappable );""",
      """void lodgenNativeScrappableOption( bool scrappable );

/*! v7 grouping, THE PROXIMITY JOIN (bungo's ruling of 2026-09-19, measured by
 *  lane IDENTPROX). `legacy` restores the shipped rule exactly -- only a
 *  placement whose BASE model path carries an `architecture` component may
 *  join, and it joins when the two WORLD AXIS-ALIGNED boxes are within 16 u on
 *  every axis. False (the default) runs the ruled rule: every NON-TREE
 *  placement with a drawn LOD mesh joins when the two MESHES are within
 *  `gapWorld` world units. `gapWorld <= 0` leaves the default 64 alone.
 *
 *  The gap is a knob and the measure is not: lane IDENTPROX measured that a
 *  BOX gap of any size bridges a street (an elevated highway deck's box hangs
 *  over the buildings under it) while the mesh gap does not until 256 u. */
void lodgenNativeIdentityJoinOption( bool legacy, float gapWorld );""",
      "nativeemit.h declaration")
h.save()

# ===========================================================================
# src/nativeemit.cpp
# ===========================================================================
c = F("src/nativeemit.cpp")

# ---- the sticky options --------------------------------------------------
c.sub("""\tbool scrappable = false;            //!< v9: mark workshop-scrappable placements; off = .lodi v7
};""",
      """\tbool scrappable = false;            //!< v9: mark workshop-scrappable placements; off = .lodi v7
\tbool identityJoinLegacy = false;    //!< v7 grouping: the pre-2026-09-19 architecture/box rule
\tfloat identityJoinGap = 64.0f;      //!< v7 grouping: the MESH-to-MESH gap two placements join at
};""",
      "NativeLadderOptions fields")

c.sub("""void lodgenNativeScrappableOption( bool scrappable )
{
\tladderOpts().scrappable = scrappable;
}""",
      """void lodgenNativeScrappableOption( bool scrappable )
{
\tladderOpts().scrappable = scrappable;
}

void lodgenNativeIdentityJoinOption( bool legacy, float gapWorld )
{
\tladderOpts().identityJoinLegacy = legacy;
\tif ( gapWorld > 0.0f )
\t\tladderOpts().identityJoinGap = gapWorld;
}""",
      "lodgenNativeIdentityJoinOption")

c.sub("""\ts.scrappable = o.scrappable;""",
      """\ts.scrappable = o.scrappable;
\ts.identityJoinLegacy = o.identityJoinLegacy;
\ts.identityJoinGap = o.identityJoinGap;""",
      "options copy")

c.sub("""\tbool scrappable = false;
\tQHash<QPair<quint32, int>, int> byKey;        //!< (ref, part) -> arrival index""",
      """\tbool scrappable = false;
\t/*! v7 grouping, the PROXIMITY JOIN (bungo's ruling 2026-09-19). `legacy`
\t *  is the exact way back to the shipped architecture/box rule. */
\tbool identityJoinLegacy = false;
\tfloat identityJoinGap = 64.0f;
\tQHash<QPair<quint32, int>, int> byKey;        //!< (ref, part) -> arrival index""",
      "State fields")

# ---- the eligibility -----------------------------------------------------
c.sub("""\t\t/* (ii) Architecture placements join a connected component over their
\t\t * world boxes. Everything else is its own group and stays so. */
\t\tstruct Box { float lo[3], hi[3]; };
\t\tstd::vector<Box> box( ni );
\t\tstd::vector<quint32> arch;
\t\tarch.reserve( archPlacements );
\t\tfor ( size_t i = 0; i < ni; i++ ) {
\t\t\tconst LodiSrcInstance & r = set.instances[i];
\t\t\tBox & b = box[i];
\t\t\tfor ( int k = 0; k < 3; k++ ) { b.lo[k] = r.pos[k]; b.hi[k] = r.pos[k]; }
\t\t\tif ( i >= instArch.size() || !instArch[i] )
\t\t\t\tcontinue;
\t\t\tquint16 mid = LODO_NO_MESH;
\t\t\tif ( r.baseId < lib.bases.size() && r.mnamSlot < 4 )
\t\t\t\tmid = lib.bases[r.baseId].rep[r.mnamSlot];
\t\t\tif ( KNOB.useBox && mid != LODO_NO_MESH && mid < lib.meshes.size() ) {""",
      """\t\t/* (ii) THE JOIN. Two rules live here and the command line picks one.
\t\t *
\t\t * PROXIMITY (the default since bungo's ruling of 2026-09-19): every
\t\t * placement that is NOT a tree and HAS a drawn LOD mesh joins a connected
\t\t * component with every other one whose MESH comes within
\t\t * `--identity-join-gap` world units (64 by default). Trees and card-only
\t\t * placements stay singletons.
\t\t *
\t\t * LEGACY (`--identity-join legacy`): the shipped rule -- only a placement
\t\t * whose BASE model path carries an `architecture` component joins, and it
\t\t * joins when the two WORLD AXIS-ALIGNED boxes are within 16 u on every
\t\t * axis. It is kept because it is the exact way back and because it is the
\t\t * red control of the gate: it must reproduce 588 groups on chunk 4.4.-12.
\t\t *
\t\t * WHY THE MEASURE CHANGED AND NOT ONLY THE NUMBER (lane IDENTPROX,
\t\t * scratchpad/identprox_20260919/RECOMMENDATION.txt): the box rule at 16 u
\t\t * ALREADY welds 286 placements across 9 Creation Kit layers into one
\t\t * 18,121-unit identity, because the elevated highway deck's axis-aligned
\t\t * box hangs over four South Boston city blocks. No gap fixes that; only
\t\t * the measure does. With the mesh measure the only identity above the
\t\t * 6,110 u threshold anywhere in the chunk, at every gap from 16 to 128 u,
\t\t * is the highway itself. */
\t\tconst bool legacyJoin = s.identityJoinLegacy;
\t\tstruct Box { float lo[3], hi[3]; };
\t\tstd::vector<Box> box( ni );
\t\tstd::vector<quint32> arch;          //!< the placements THIS rule lets join
\t\tstd::vector<quint16> meshOf( ni, quint16( LODO_NO_MESH ) );
\t\tarch.reserve( archPlacements );
\t\tfor ( size_t i = 0; i < ni; i++ ) {
\t\t\tconst LodiSrcInstance & r = set.instances[i];
\t\t\tBox & b = box[i];
\t\t\tfor ( int k = 0; k < 3; k++ ) { b.lo[k] = r.pos[k]; b.hi[k] = r.pos[k]; }
\t\t\tquint16 mid = LODO_NO_MESH;
\t\t\tif ( r.baseId < lib.bases.size() && r.mnamSlot < 4 )
\t\t\t\tmid = lib.bases[r.baseId].rep[r.mnamSlot];
\t\t\tmeshOf[i] = mid;
\t\t\tconst bool hasMesh = ( mid != LODO_NO_MESH && mid < lib.meshes.size() );
\t\t\t/* A TREE NEVER JOINS. Not a taste: a tree's far shadow is cast from its
\t\t\t * own authored LOD mesh with alpha test, and an identity shared with the
\t\t\t * wall it grows against would exclude that wall from the tree's shadow.
\t\t\t * The flag is the library's own (`LODO_BASE_TREE`), which is what the
\t\t\t * card baker and the foliage refusal already read. */
\t\t\tconst bool isTree = ( r.baseId < lib.bases.size() )
\t\t\t\t&& ( lib.bases[r.baseId].flags & LODO_BASE_TREE ) != 0;
\t\t\tconst bool eligible = legacyJoin
\t\t\t\t? ( i < instArch.size() && instArch[i] != 0 )
\t\t\t\t: ( !isTree && hasMesh );
\t\t\tif ( !eligible )
\t\t\t\tcontinue;
\t\t\tif ( KNOB.useBox && mid != LODO_NO_MESH && mid < lib.meshes.size() ) {""",
      "the eligibility loop")

# ---- the two walks -------------------------------------------------------
c.sub("""\tquint32 archPlacements = 0;     //!< v7 census: how many placements the path prefix catches""",
      """\tquint32 archPlacements = 0;     //!< v7 census: how many placements the path prefix catches
\t/*! v7 grouping, the PROXIMITY JOIN's own census: what it was given, what it
\t *  measured and what it cost. Declared here, beside `archPlacements`, because
\t *  the census line that prints them is written long after the block that
\t *  fills them has closed. */
\tquint32 joinEligible = 0;
\tquint64 joinSamples = 0, joinPairs = 0;
\tqint64 joinMs = 0;""",
      "join counters")

c.sub("""\t\t{
\t\t\tconst float T = KNOB.touchTolerance, G = KNOB.gridCell;
\t\t\tstd::unordered_map<quint64, std::vector<quint32>> grid;""",
      """\t\tif ( legacyJoin ) {
\t\t\tconst float T = KNOB.touchTolerance, G = KNOB.gridCell;
\t\t\tstd::unordered_map<quint64, std::vector<quint32>> grid;""",
      "legacy walk guard")

c.sub("""\t\t\t\t\t\tif ( touch )
\t\t\t\t\t\t\tjoin( v[a], v[b2] );
\t\t\t\t\t}
\t\t\t}
\t\t}
\t\t/* The key the writer sees.""",
      """\t\t\t\t\t\tif ( touch )
\t\t\t\t\t\t\tjoin( v[a], v[b2] );
\t\t\t\t\t}
\t\t\t}
\t\t} else {
\t\t\t/* THE MESH-TO-MESH JOIN, in three steps and no approximation of the
\t\t\t * third one.
\t\t\t *
\t\t\t * THE MEASURE is the one lane IDENTPROX measured and named `mesh`: the
\t\t\t * minimum distance between two SAMPLE SETS, each the placed level-0 LOD
\t\t\t * vertices PLUS every level-0 triangle's three edge midpoints and its
\t\t\t * centroid. It is a point-set measure, so it NEVER reports less than the
\t\t\t * true surface-to-surface distance and reports at most that distance
\t\t\t * plus the two sample sets' covering radii -- and the covering radius of
\t\t\t * {vertices, edge midpoints, centroid} over a triangle is at most half
\t\t\t * its longest edge. It therefore joins CONSERVATIVELY: a pair it joins
\t\t\t * is genuinely within the gap, and the only error it can make is to
\t\t\t * leave a pair apart that a point-to-triangle measure would join. That
\t\t\t * is the same arithmetic the Python sweep ran, which is why the two
\t\t\t * counts are comparable at all (root MISTAKES: a refuter that shares the
\t\t\t * producer's code measures agreement, not correctness -- here the
\t\t\t * sharing is a DEFINITION and the two implementations are independent).
\t\t\t *
\t\t\t * THE COST is one pass over the samples, not a pair walk: every sample
\t\t\t * of every eligible placement goes into one uniform grid whose cell IS
\t\t\t * the gap, and each sample then looks at its own cell and the 26 around
\t\t\t * it. Two placements already in one set cost a `find` and nothing else,
\t\t\t * which is what keeps a 205-placement building cheap. */
\t\t\tQElapsedTimer joinClock;
\t\t\tjoinClock.start();
\t\t\tconst float T = ( s.identityJoinGap > 0.0f ) ? s.identityJoinGap : 64.0f;
\t\t\t/* One sample set per MESH, in LOCAL units, built on first use: a region
\t\t\t * draws a few hundred meshes and tens of thousands of placements. */
\t\t\tstd::unordered_map<quint32, std::vector<float>> meshPts;
\t\t\tauto samplesOf = [&]( quint16 mid ) -> const std::vector<float> & {
\t\t\t\tauto it = meshPts.find( quint32( mid ) );
\t\t\t\tif ( it != meshPts.end() )
\t\t\t\t\treturn it->second;
\t\t\t\tstd::vector<float> out;
\t\t\t\tconst LodoMesh & me = lib.meshes[mid];
\t\t\t\tconst quint32 c1 = me.clusterFirst + me.clusterCount;
\t\t\t\tfor ( quint32 cc = me.clusterFirst; cc < c1 && cc < lib.clusters.size(); cc++ ) {
\t\t\t\t\tif ( cc >= lib.clusterLods.size() || lib.clusterLods[cc].level != 0 )
\t\t\t\t\t\tcontinue;
\t\t\t\t\tconst LodoCluster & cl = lib.clusters[cc];
\t\t\t\t\tfloat lv[48][3];
\t\t\t\t\tint nv = 0;
\t\t\t\t\tfor ( int n = 0; n < int( cl.vertexCount ) && n < 48; n++ ) {
\t\t\t\t\t\tconst size_t vi = size_t( cl.vertexBase ) + size_t( n );
\t\t\t\t\t\tif ( vi >= lib.vertices.size() )
\t\t\t\t\t\t\tbreak;
\t\t\t\t\t\tconst LodoVertex & vv = lib.vertices[vi];
\t\t\t\t\t\tfor ( int k = 0; k < 3; k++ )
\t\t\t\t\t\t\tlv[n][k] = me.aabbMin[k] + float( vv.pos[k] ) / 65535.0f * me.aabbExtent[k];
\t\t\t\t\t\tout.push_back( lv[n][0] ); out.push_back( lv[n][1] ); out.push_back( lv[n][2] );
\t\t\t\t\t\tnv++;
\t\t\t\t\t}
\t\t\t\t\tconst size_t li = size_t( cc ) * 48;
\t\t\t\t\tfor ( int t = 0; t < int( cl.triangleCount ); t++ ) {
\t\t\t\t\t\tif ( li + size_t( t ) * 3 + 2 >= lib.localIndices.size() )
\t\t\t\t\t\t\tbreak;
\t\t\t\t\t\tconst int ia = lib.localIndices[li + size_t( t ) * 3 + 0];
\t\t\t\t\t\tconst int ib = lib.localIndices[li + size_t( t ) * 3 + 1];
\t\t\t\t\t\tconst int ic = lib.localIndices[li + size_t( t ) * 3 + 2];
\t\t\t\t\t\tif ( ia >= nv || ib >= nv || ic >= nv )
\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\tfor ( int k = 0; k < 3; k++ ) out.push_back( 0.5f * ( lv[ia][k] + lv[ib][k] ) );
\t\t\t\t\t\tfor ( int k = 0; k < 3; k++ ) out.push_back( 0.5f * ( lv[ib][k] + lv[ic][k] ) );
\t\t\t\t\t\tfor ( int k = 0; k < 3; k++ ) out.push_back( 0.5f * ( lv[ic][k] + lv[ia][k] ) );
\t\t\t\t\t\tfor ( int k = 0; k < 3; k++ )
\t\t\t\t\t\t\tout.push_back( ( lv[ia][k] + lv[ib][k] + lv[ic][k] ) / 3.0f );
\t\t\t\t\t}
\t\t\t\t}
\t\t\t\t/* A mesh whose level-0 clusters this reader could not walk keeps ONE
\t\t\t\t * sample -- the centre of its own AABB -- rather than none, so it is
\t\t\t\t * still a placement that can join something instead of silently
\t\t\t\t * becoming a singleton for a reason no line of the census states. */
\t\t\t\tif ( out.empty() )
\t\t\t\t\tfor ( int k = 0; k < 3; k++ )
\t\t\t\t\t\tout.push_back( me.aabbMin[k] + me.aabbExtent[k] * 0.5f );
\t\t\t\treturn meshPts.emplace( quint32( mid ), std::move( out ) ).first->second;
\t\t\t};
\t\t\tstd::vector<float> pts;
\t\t\tstd::vector<quint32> owner;
\t\t\tfor ( quint32 i : arch ) {
\t\t\t\tif ( meshOf[i] == LODO_NO_MESH || meshOf[i] >= lib.meshes.size() )
\t\t\t\t\tcontinue;
\t\t\t\tconst LodiSrcInstance & r = set.instances[i];
\t\t\t\tconst std::vector<float> & lp = samplesOf( meshOf[i] );
\t\t\t\tpts.reserve( pts.size() + lp.size() );
\t\t\t\towner.reserve( owner.size() + lp.size() / 3 );
\t\t\t\tfor ( size_t p = 0; p + 2 < lp.size(); p += 3 ) {
\t\t\t\t\t/* The SAME placement arithmetic the box above uses: scale on the
\t\t\t\t\t * local point, then the row-major rotation, then the position. */
\t\t\t\t\tconst float x = lp[p] * r.scale, y = lp[p + 1] * r.scale, z = lp[p + 2] * r.scale;
\t\t\t\t\tpts.push_back( r.rot[0] * x + r.rot[1] * y + r.rot[2] * z + r.pos[0] );
\t\t\t\t\tpts.push_back( r.rot[3] * x + r.rot[4] * y + r.rot[5] * z + r.pos[1] );
\t\t\t\t\tpts.push_back( r.rot[6] * x + r.rot[7] * y + r.rot[8] * z + r.pos[2] );
\t\t\t\t\towner.push_back( i );
\t\t\t\t}
\t\t\t}
\t\t\tjoinEligible = quint32( arch.size() );
\t\t\tjoinSamples = quint64( owner.size() );
\t\t\t/* The cell key packs three signed cell indices into 21 bits each. At a
\t\t\t * 64-unit cell that is +/- 67 million world units an axis, five hundred
\t\t\t * times the widest Bethesda worldspace, so two different cells cannot
\t\t\t * share a key and no join can come from an aliased bucket. */
\t\t\tauto cellOf = [T]( float v ) { return int( std::floor( double( v ) / double( T ) ) ); };
\t\t\tauto keyOf3 = []( int gx, int gy, int gz ) {
\t\t\t\treturn ( ( quint64( quint32( gx ) ) & 0x1FFFFFull ) << 42 )
\t\t\t\t\t| ( ( quint64( quint32( gy ) ) & 0x1FFFFFull ) << 21 )
\t\t\t\t\t| ( quint64( quint32( gz ) ) & 0x1FFFFFull );
\t\t\t};
\t\t\tstd::unordered_map<quint64, std::vector<quint32>> pgrid;
\t\t\tpgrid.reserve( owner.size() / 4 + 16 );
\t\t\tfor ( size_t p = 0; p < owner.size(); p++ )
\t\t\t\tpgrid[keyOf3( cellOf( pts[p * 3] ), cellOf( pts[p * 3 + 1] ), cellOf( pts[p * 3 + 2] ) )]
\t\t\t\t\t.push_back( quint32( p ) );
\t\t\tconst float T2 = T * T;
\t\t\tfor ( size_t p = 0; p < owner.size(); p++ ) {
\t\t\t\tconst int gx = cellOf( pts[p * 3] ), gy = cellOf( pts[p * 3 + 1] ), gz = cellOf( pts[p * 3 + 2] );
\t\t\t\tfor ( int dz = -1; dz <= 1; dz++ )
\t\t\t\t\tfor ( int dy = -1; dy <= 1; dy++ )
\t\t\t\t\t\tfor ( int dx = -1; dx <= 1; dx++ ) {
\t\t\t\t\t\t\tauto cit = pgrid.find( keyOf3( gx + dx, gy + dy, gz + dz ) );
\t\t\t\t\t\t\tif ( cit == pgrid.end() )
\t\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\t\tfor ( quint32 q : cit->second ) {
\t\t\t\t\t\t\t\tif ( owner[q] == owner[p] )
\t\t\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\t\t\tif ( find( owner[p] ) == find( owner[q] ) )
\t\t\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\t\t\tconst float ex = pts[p * 3] - pts[size_t( q ) * 3];
\t\t\t\t\t\t\t\tconst float ey = pts[p * 3 + 1] - pts[size_t( q ) * 3 + 1];
\t\t\t\t\t\t\t\tconst float ez = pts[p * 3 + 2] - pts[size_t( q ) * 3 + 2];
\t\t\t\t\t\t\t\tif ( ex * ex + ey * ey + ez * ez <= T2 ) {
\t\t\t\t\t\t\t\t\tjoin( owner[p], owner[q] );
\t\t\t\t\t\t\t\t\tjoinPairs++;
\t\t\t\t\t\t\t\t}
\t\t\t\t\t\t\t}
\t\t\t\t\t\t}
\t\t\t}
\t\t\tjoinMs = joinClock.elapsed();
\t\t}
\t\t/* The key the writer sees.""",
      "the proximity walk")

# ---- the census ----------------------------------------------------------
c.sub("""\t\tladderLine += QString( "; groups %1" )
\t\t\t.arg( s.lodiV7
\t\t\t\t? QString( "%1 over %2 placements (%3 grouped, largest %4, %5 singleton), %6 placement(s) whose BASE model path has a `%7` component" )
\t\t\t\t\t.arg( stats.groups ).arg( stats.instances ).arg( stats.groupedPlacements )
\t\t\t\t\t.arg( stats.largestGroup ).arg( stats.singletonGroups ).arg( archPlacements )
\t\t\t\t\t.arg( QLatin1String( KNOB.archComponent ) )
\t\t\t\t: QStringLiteral( "OFF (--lodi-v6)" ) );""",
      """\t\tladderLine += QString( "; groups %1" )
\t\t\t.arg( s.lodiV7
\t\t\t\t? QString( "%1 over %2 placements (%3 grouped, largest %4, %5 singleton), %6 placement(s) whose BASE model path has a `%7` component" )
\t\t\t\t\t.arg( stats.groups ).arg( stats.instances ).arg( stats.groupedPlacements )
\t\t\t\t\t.arg( stats.largestGroup ).arg( stats.singletonGroups ).arg( archPlacements )
\t\t\t\t\t.arg( QLatin1String( KNOB.archComponent ) )
\t\t\t\t: QStringLiteral( "OFF (--lodi-v6)" ) );
\t\t/* v7 grouping, THE JOIN RULE, on its own words and with its cost, because
\t\t * the rule is bungo's ruling of 2026-09-19 and a bake that silently ran
\t\t * the other one would be indistinguishable from a bake that ran this one
\t\t * badly. The gate reads `groups` above against lane IDENTPROX's Python
\t\t * count (chunk 4.4.-12: 588 under `legacy`, 167 under the ruled rule) and
\t\t * the legacy switch is its red control. */
\t\tladderLine += QString( "\\n  native-identity-join: %1" )
\t\t\t.arg( !s.lodiV7
\t\t\t\t? QStringLiteral( "OFF (--lodi-v6); no group table is written" )
\t\t\t\t: ( s.identityJoinLegacy
\t\t\t\t\t? QString( "LEGACY (--identity-join legacy): the pre-2026-09-19 rule -- an `%1` "
\t\t\t\t\t\t"path component and a WORLD AXIS-ALIGNED BOX gap of %2 u. %3 eligible placement(s)." )
\t\t\t\t\t\t.arg( QLatin1String( KNOB.archComponent ) )
\t\t\t\t\t\t.arg( double( KNOB.touchTolerance ), 0, 'f', 1 ).arg( archPlacements )
\t\t\t\t\t: QString( "PROXIMITY (the default; --identity-join legacy is the way back): every "
\t\t\t\t\t\t"NON-TREE placement with a drawn LOD mesh, MESH-TO-MESH gap %1 u "
\t\t\t\t\t\t"(--identity-join-gap). %2 eligible placement(s), %L3 mesh sample point(s) "
\t\t\t\t\t\t"(level-0 vertices + edge midpoints + centroids, placed), %L4 sample pair(s) "
\t\t\t\t\t\t"inside the gap, %5 ms. The measure never reports LESS than the true "
\t\t\t\t\t\t"surface distance, so every join it made is real." )
\t\t\t\t\t\t.arg( double( s.identityJoinGap ), 0, 'f', 1 ).arg( joinEligible )
\t\t\t\t\t\t.arg( joinSamples ).arg( joinPairs ).arg( joinMs ) ) );""",
      "the join census line")

c.save()
print("--check: nothing written" if CHECK else "OK")
