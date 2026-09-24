# -*- coding: utf-8 -*-
"""patch_solver2.py -- what the numpy prototype taught, ported into
src/watermark.cpp:
  1. the solver balances its sources PER CONNECTED PIECE (a body is often
     several: the bridge rule joins pieces up to 2 texels apart) and an
     isolated texel is not divided by zero;
  2. the stroke's conductance preference is a SMOOTH quartic bump (x4 on the
     stroke, x1 at its half-width), not a step -- a step in k refracts the
     flow at its edge and that edge was a seam;
  3. a stroke's end is every wet texel within its half-width of the end
     point, not one texel -- a point sink is a singularity whose neighbours
     all point at it;
  4. the DIRECTION written to the plane is the solved direction continued
     into slack water and bank texels (whose face-averaged velocity is biased
     by the staircase) and low-passed by kSmoothPasses in-mask 3x3 vector
     averages; the SPEED and the flux are the solve's own, untouched.
"""
import sys
sys.path.insert(0, 'scratchpad/water4_20260910')
from splice import splice   # noqa

C = 'src/watermark.cpp'
H = 'src/watermark.h'

splice(H, [
    ('\tint nE = 0;\n\tstd::vector<double> diag;\n',
     '\tstd::vector<int> comp;             //!< cell -> connected piece (faces only)\n\tint nComp = 0;\n', 'after'),
])

splice(C, [
    # ---- 1. components in build(), per-piece balance in solve() ----
    ('\tdiag.assign( size_t( n ), 0.0 );\n\tfor ( size_t f = 0; f < fi.size(); f++ ) {\n\t\tdiag[size_t( fi[f] )] += kf[f];\n\t\tdiag[size_t( fj[f] )] += kf[f];\n\t}\n}\n',
     '''\tdiag.assign( size_t( n ), 0.0 );
\tfor ( size_t f = 0; f < fi.size(); f++ ) {
\t\tdiag[size_t( fi[f] )] += kf[f];
\t\tdiag[size_t( fj[f] )] += kf[f];
\t}
\t/* The connected pieces, over the faces.  A body is often several: the
\t * writer's bridge rule joins pieces up to two texels apart, so the mask
\t * has islets and coves that touch nothing, and each is a pure-Neumann
\t * system of its own whose sources must balance on their own. */
\tstd::vector<int> parent( size_t( n ), 0 );
\tfor ( int i = 0; i < n; i++ )
\t\tparent[size_t( i )] = i;
\tauto find = [&]( int i ) {
\t\twhile ( parent[size_t( i )] != i ) {
\t\t\tparent[size_t( i )] = parent[size_t( parent[size_t( i )] )];
\t\t\ti = parent[size_t( i )];
\t\t}
\t\treturn i;
\t};
\tfor ( size_t f = 0; f < fi.size(); f++ ) {
\t\tconst int a = find( fi[f] ), b = find( fj[f] );
\t\tif ( a != b )
\t\t\tparent[size_t( a )] = b;
\t}
\tcomp.assign( size_t( n ), -1 );
\tnComp = 0;
\tstd::vector<int> label( size_t( n ), -1 );
\tfor ( int i = 0; i < n; i++ ) {
\t\tconst int r = find( i );
\t\tif ( label[size_t( r )] < 0 )
\t\t\tlabel[size_t( r )] = nComp++;
\t\tcomp[size_t( i )] = label[size_t( r )];
\t}
}
''', 'replace'),
    ('''\tif ( !anyD ) {
\t\t// the pure-Neumann system: consistent only when the sources balance
\t\tdouble mean = 0.0;
\t\tfor ( int i = 0; i < n; i++ )
\t\t\tmean += b[size_t( i )];
\t\tmean /= double( n );
\t\tfor ( int i = 0; i < n; i++ )
\t\t\tb[size_t( i )] -= mean;
\t}
''', '''\t{
\t\t/* consistency PER PIECE: a piece with no Dirichlet cell is a pure-
\t\t * Neumann system of its own, singular, and solvable only when its
\t\t * sources balance -- so its mean is taken out of it */
\t\tstd::vector<double> sum( size_t( std::max( 1, nComp ) ), 0.0 );
\t\tstd::vector<int> cnt( size_t( std::max( 1, nComp ) ), 0 );
\t\tstd::vector<quint8> hasD( size_t( std::max( 1, nComp ) ), 0 );
\t\tfor ( int i = 0; i < n; i++ ) {
\t\t\tconst int c = comp.empty() ? 0 : comp[size_t( i )];
\t\t\tsum[size_t( c )] += b[size_t( i )];
\t\t\tcnt[size_t( c )]++;
\t\t\tif ( anyD && dirichlet[size_t( i )] )
\t\t\t\thasD[size_t( c )] = 1;
\t\t}
\t\tfor ( int i = 0; i < n; i++ ) {
\t\t\tconst int c = comp.empty() ? 0 : comp[size_t( i )];
\t\t\tif ( !hasD[size_t( c )] && cnt[size_t( c )] > 0 )
\t\t\t\tb[size_t( i )] -= sum[size_t( c )] / double( cnt[size_t( c )] );
\t\t}
\t}
''', 'replace'),
    ('\tfor ( int i = 0; i < n; i++ ) {\n\t\tz[size_t( i )] = r[size_t( i )] / diag[size_t( i )];\n\t\tp[size_t( i )] = z[size_t( i )];\n\t\trz += r[size_t( i )] * z[size_t( i )];\n\t}\n',
     '\t// an isolated texel has no face and no equation: its preconditioner is 0\n'
     '\tstd::vector<double> Minv( size_t( n ), 0.0 );\n'
     '\tfor ( int i = 0; i < n; i++ )\n'
     '\t\tMinv[size_t( i )] = diag[size_t( i )] > 0.0 ? 1.0 / diag[size_t( i )] : 0.0;\n'
     '\tfor ( int i = 0; i < n; i++ ) {\n\t\tz[size_t( i )] = r[size_t( i )] * Minv[size_t( i )];\n\t\tp[size_t( i )] = z[size_t( i )];\n\t\trz += r[size_t( i )] * z[size_t( i )];\n\t}\n', 'replace'),
    ('\t\tfor ( int i = 0; i < n; i++ ) {\n\t\t\tz[size_t( i )] = r[size_t( i )] / diag[size_t( i )];\n\t\t\trz2 += r[size_t( i )] * z[size_t( i )];\n\t\t}\n',
     '\t\tfor ( int i = 0; i < n; i++ ) {\n\t\t\tz[size_t( i )] = r[size_t( i )] * Minv[size_t( i )];\n\t\t\trz2 += r[size_t( i )] * z[size_t( i )];\n\t\t}\n', 'replace'),
    # ---- 2. the smooth bump ----
    ('//! Conductance boost inside a stroke\'s half-width: the soft preference.\nconstexpr double kStrokeBoost = 4.0;\n',
     '//! Conductance boost ON a stroke: a quartic bump to x1 at its half-width.\n'
     'constexpr double kStrokeBoost = 4.0;\n'
     '//! Passes of the in-mask 3x3 vector average over the written DIRECTION.\n'
     'constexpr int kSmoothPasses = 8;\n'
     '//! Below this fraction of the mean speed the water is slack and its direction is continued.\n'
     'constexpr double kSlackFraction = 0.02;\n', 'replace'),
    ('''\tstd::vector<quint8> held( n, 0 );
\tauto texelOf = [&]( double wx, double wy, int & x, int & y ) {
''', '''\tstd::vector<quint8> held( n, 0 );
\tstd::vector<float> boost( n, 1.0f );
\tauto texelOf = [&]( double wx, double wy, int & x, int & y ) {
''', 'replace'),
    ('''\t\t\t\tdouble tx = 0, ty = 0;
\t\t\t\tif ( distToSegment( wx, wy, seg.ax, seg.ay, seg.bx, seg.by, tx, ty ) > seg.halfW )
\t\t\t\t\tcontinue;
\t\t\t\tif ( !held[at] ) {
\t\t\t\t\theld[at] = 1;
\t\t\t\t\tk[at] = float( k[at] * kStrokeBoost );
\t\t\t\t}
''', '''\t\t\t\tdouble tx = 0, ty = 0;
\t\t\t\tconst double dd = distToSegment( wx, wy, seg.ax, seg.ay, seg.bx, seg.by, tx, ty );
\t\t\t\tif ( dd > seg.halfW )
\t\t\t\t\tcontinue;
\t\t\t\theld[at] = 1;
\t\t\t\t/* a SMOOTH preference: a step in k refracts the flow at its edge,
\t\t\t\t * and that edge was a seam in the prototype's picture */
\t\t\t\tconst double q = 1.0 - ( dd / seg.halfW ) * ( dd / seg.halfW );
\t\t\t\tboost[at] = float( std::max( double( boost[at] ), 1.0 + ( kStrokeBoost - 1.0 ) * q * q ) );
''', 'replace'),
    ('''\tint heldHere = 0;
\tfor ( size_t i = 0; i < n; i++ )
\t\tif ( held[i] )
\t\t\theldHere++;
''', '''\tint heldHere = 0;
\tfor ( size_t i = 0; i < n; i++ ) {
\t\tk[i] = float( k[i] * boost[i] );
\t\tif ( held[i] )
\t\t\theldHere++;
\t}
''', 'replace'),
    # ---- 3. distributed ends and pins ----
    ('''\t// nearest wet texel of this body to a world point, within `reach` texels
\tauto nearestWet = [&]( double wx, double wy, int reach ) -> long {
''', '''\t// every wet texel of this body within `radius` texels of a world point --
\t// a point sink is a singularity whose neighbours all point at it, so an
\t// end or a pin is a DISC, and the nearest texel only when the disc is empty
\tauto discOf = [&]( double wx, double wy, double radius, std::vector<size_t> & out ) {
\t\tint x = 0, y = 0;
\t\ttexelOf( wx, wy, x, y );
\t\tconst int r = std::max( 1, int( radius + 0.5 ) );
\t\tfor ( int dy = -r; dy <= r; dy++ )
\t\t\tfor ( int dx = -r; dx <= r; dx++ ) {
\t\t\t\tif ( dx * dx + dy * dy > r * r )
\t\t\t\t\tcontinue;
\t\t\t\tconst int nx = x + dx, ny = y + dy;
\t\t\t\tif ( nx < 0 || ny < 0 || nx >= W || ny >= H )
\t\t\t\t\tcontinue;
\t\t\t\tconst size_t at = size_t( ny ) * size_t( W ) + size_t( nx );
\t\t\t\tif ( F->mask[at] )
\t\t\t\t\tout.push_back( at );
\t\t\t}
\t};
\tconst double endRadius = std::max( 2.0, ( maxWidth > 0.0 ? maxWidth * 0.5 : 4.0 * u ) / u );
\t// nearest wet texel of this body to a world point, within `reach` texels
\tauto nearestWet = [&]( double wx, double wy, int reach ) -> long {
''', 'replace'),
    ('''\tfor ( const QPointF & p : srcPins ) {
\t\tconst long at = nearestWet( p.x(), p.y(), 4 );
\t\tif ( at >= 0 )
\t\t\tsources.push_back( size_t( at ) );
\t}
\tfor ( const QPointF & p : snkPins ) {
\t\tconst long at = nearestWet( p.x(), p.y(), 4 );
\t\tif ( at >= 0 )
\t\t\tsinks.push_back( size_t( at ) );
\t}
''', '''\tfor ( const QPointF & p : srcPins ) {
\t\tdiscOf( p.x(), p.y(), endRadius, sources );
\t\tconst long at = sources.empty() ? nearestWet( p.x(), p.y(), 4 ) : -1;
\t\tif ( at >= 0 )
\t\t\tsources.push_back( size_t( at ) );
\t}
\tfor ( const QPointF & p : snkPins ) {
\t\tdiscOf( p.x(), p.y(), endRadius, sinks );
\t\tconst long at = sinks.empty() ? nearestWet( p.x(), p.y(), 4 ) : -1;
\t\tif ( at >= 0 )
\t\t\tsinks.push_back( size_t( at ) );
\t}
''', 'replace'),
    ('''\t\tfor ( const QPointF & p : firsts ) {
\t\t\tconst long at = nearestWet( p.x(), p.y(), 8 );
\t\t\tif ( at >= 0 )
\t\t\t\ts.push_back( size_t( at ) );
\t\t}
\t\tfor ( const QPointF & p : lasts ) {
\t\t\tconst long at = nearestWet( p.x(), p.y(), 8 );
\t\t\tif ( at >= 0 )
\t\t\t\tt.push_back( size_t( at ) );
\t\t}
''', '''\t\tfor ( const QPointF & p : firsts ) {
\t\t\tconst size_t before = s.size();
\t\t\tdiscOf( p.x(), p.y(), endRadius, s );
\t\t\tconst long at = s.size() == before ? nearestWet( p.x(), p.y(), 8 ) : -1;
\t\t\tif ( at >= 0 )
\t\t\t\ts.push_back( size_t( at ) );
\t\t}
\t\tfor ( const QPointF & p : lasts ) {
\t\t\tconst size_t before = t.size();
\t\t\tdiscOf( p.x(), p.y(), endRadius, t );
\t\t\tconst long at = t.size() == before ? nearestWet( p.x(), p.y(), 8 ) : -1;
\t\t\tif ( at >= 0 )
\t\t\t\tt.push_back( size_t( at ) );
\t\t}
''', 'replace'),
    # ---- 4. the written direction: continued and low-passed ----
    ('''\tdouble mx = 0.0, my = 0.0;
\tfor ( int i = 0; i < G.n; i++ ) {
\t\tconst size_t at = size_t( G.cy[size_t( i )] ) * size_t( W ) + size_t( G.cx[size_t( i )] );
\t\tconst double sp = std::hypot( ux[size_t( i )], uy[size_t( i )] );
\t\tif ( sp > 1e-12 ) {
\t\t\tF->vx[at] = float( ux[size_t( i )] / sp );
\t\t\tF->vy[at] = float( uy[size_t( i )] / sp );
\t\t\tmx += ux[size_t( i )] / sp;
\t\t\tmy += uy[size_t( i )] / sp;
\t\t} else {
\t\t\tF->vx[at] = 0.0f;
\t\t\tF->vy[at] = 0.0f;
\t\t}
''', '''\t/* THE WRITTEN DIRECTION.  The solve's direction stands where the water
\t * moves and is not a bank texel; slack water (below kSlackFraction of the
\t * mean speed: dead-end coves, the water past the sink) and the bank
\t * texels -- whose face-averaged velocity is biased by the staircase, 12
\t * degrees on the synthetic island -- are CONTINUED from it by the
\t * harmonic fill (tangent to the banks by construction); then the whole
\t * unit field is low-passed by kSmoothPasses in-mask 3x3 vector averages.
\t * The prototype measured why: within three texels of a staircase bank
\t * the raw direction jumps 22-25 degrees at its 99th percentile, six
\t * texels in it is 5.6.  The SPEED and the flux are the solve's own and
\t * are not touched by any of this. */
\t{
\t\tstd::vector<float> dxv( n, 0.0f ), dyv( n, 0.0f );
\t\tstd::vector<quint8> fixed( n, 0 );
\t\tfor ( int i = 0; i < G.n; i++ ) {
\t\t\tconst int x = G.cx[size_t( i )], y = G.cy[size_t( i )];
\t\t\tconst size_t at = size_t( y ) * size_t( W ) + size_t( x );
\t\t\tconst double sp = std::hypot( ux[size_t( i )], uy[size_t( i )] );
\t\t\tbool bank = x == 0 || y == 0 || x == W - 1 || y == H - 1;
\t\t\tif ( !bank )
\t\t\t\tbank = !F->mask[at - 1] || !F->mask[at + 1] || !F->mask[at - size_t( W )]
\t\t\t\t\t|| !F->mask[at + size_t( W )];
\t\t\tif ( sp >= kSlackFraction * meanSpeed && sp > 1e-300 && !bank ) {
\t\t\t\tdxv[at] = float( ux[size_t( i )] / sp );
\t\t\t\tdyv[at] = float( uy[size_t( i )] / sp );
\t\t\t\tfixed[at] = 1;
\t\t\t}
\t\t}
\t\t// the continuation: red-black SOR on the free texels
\t\t{
\t\t\tconst double omega = 1.9, tol = 1e-4;
\t\t\tfor ( int it2 = 0; it2 < 4000; it2++ ) {
\t\t\t\tdouble worst = 0.0;
\t\t\t\tfor ( int phase = 0; phase < 2; phase++ )
\t\t\t\t\tfor ( int y = 0; y < H; y++ )
\t\t\t\t\t\tfor ( int x = ( y + phase ) & 1; x < W; x += 2 ) {
\t\t\t\t\t\t\tconst size_t at = size_t( y ) * size_t( W ) + size_t( x );
\t\t\t\t\t\t\tif ( !F->mask[at] || fixed[at] )
\t\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\t\tdouble sx = 0.0, sy = 0.0;
\t\t\t\t\t\t\tint kk = 0;
\t\t\t\t\t\t\tconst int dx4[4] = { -1, 1, 0, 0 }, dy4[4] = { 0, 0, -1, 1 };
\t\t\t\t\t\t\tfor ( int q = 0; q < 4; q++ ) {
\t\t\t\t\t\t\t\tconst int nx = x + dx4[q], ny = y + dy4[q];
\t\t\t\t\t\t\t\tif ( nx < 0 || ny < 0 || nx >= W || ny >= H )
\t\t\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\t\t\tconst size_t nat = size_t( ny ) * size_t( W ) + size_t( nx );
\t\t\t\t\t\t\t\tif ( !F->mask[nat] )
\t\t\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\t\t\tsx += dxv[nat];
\t\t\t\t\t\t\t\tsy += dyv[nat];
\t\t\t\t\t\t\t\tkk++;
\t\t\t\t\t\t\t}
\t\t\t\t\t\t\tif ( !kk )
\t\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\t\tconst double nvx = dxv[at] + omega * ( sx / kk - dxv[at] );
\t\t\t\t\t\t\tconst double nvy = dyv[at] + omega * ( sy / kk - dyv[at] );
\t\t\t\t\t\t\tworst = std::max( worst, std::max( std::fabs( nvx - dxv[at] ), std::fabs( nvy - dyv[at] ) ) );
\t\t\t\t\t\t\tdxv[at] = float( nvx );
\t\t\t\t\t\t\tdyv[at] = float( nvy );
\t\t\t\t\t\t}
\t\t\t\tif ( worst < tol )
\t\t\t\t\tbreak;
\t\t\t}
\t\t}
\t\t// the low-pass: kSmoothPasses in-mask 3x3 vector averages, corners at half weight
\t\tstd::vector<float> tx2( n ), ty2( n );
\t\tfor ( int pass = 0; pass < kSmoothPasses; pass++ ) {
\t\t\tfor ( int y = 0; y < H; y++ )
\t\t\t\tfor ( int x = 0; x < W; x++ ) {
\t\t\t\t\tconst size_t at = size_t( y ) * size_t( W ) + size_t( x );
\t\t\t\t\tif ( !F->mask[at] ) {
\t\t\t\t\t\ttx2[at] = 0.0f;
\t\t\t\t\t\tty2[at] = 0.0f;
\t\t\t\t\t\tcontinue;
\t\t\t\t\t}
\t\t\t\t\tdouble sx = 0.0, sy = 0.0, wsum = 0.0;
\t\t\t\t\tfor ( int dy = -1; dy <= 1; dy++ )
\t\t\t\t\t\tfor ( int dx = -1; dx <= 1; dx++ ) {
\t\t\t\t\t\t\tconst int nx = x + dx, ny = y + dy;
\t\t\t\t\t\t\tif ( nx < 0 || ny < 0 || nx >= W || ny >= H )
\t\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\t\tconst size_t nat = size_t( ny ) * size_t( W ) + size_t( nx );
\t\t\t\t\t\t\tif ( !F->mask[nat] )
\t\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\t\tconst double wgt = ( dx == 0 || dy == 0 ) ? 1.0 : 0.5;
\t\t\t\t\t\t\tsx += wgt * dxv[nat];
\t\t\t\t\t\t\tsy += wgt * dyv[nat];
\t\t\t\t\t\t\twsum += wgt;
\t\t\t\t\t\t}
\t\t\t\t\ttx2[at] = float( wsum > 0.0 ? sx / wsum : 0.0 );
\t\t\t\t\tty2[at] = float( wsum > 0.0 ? sy / wsum : 0.0 );
\t\t\t\t}
\t\t\tdxv.swap( tx2 );
\t\t\tdyv.swap( ty2 );
\t\t}
\t\tfor ( size_t at = 0; at < n; at++ ) {
\t\t\tif ( !F->mask[at] )
\t\t\t\tcontinue;
\t\t\tconst double m = std::hypot( double( dxv[at] ), double( dyv[at] ) );
\t\t\tF->vx[at] = float( m > 1e-9 ? dxv[at] / m : 0.0 );
\t\t\tF->vy[at] = float( m > 1e-9 ? dyv[at] / m : 0.0 );
\t\t}
\t}
\tdouble mx = 0.0, my = 0.0;
\tfor ( int i = 0; i < G.n; i++ ) {
\t\tconst size_t at = size_t( G.cy[size_t( i )] ) * size_t( W ) + size_t( G.cx[size_t( i )] );
\t\tconst double sp = std::hypot( ux[size_t( i )], uy[size_t( i )] );
\t\tmx += F->vx[at];
\t\tmy += F->vy[at];
''', 'replace'),
])
