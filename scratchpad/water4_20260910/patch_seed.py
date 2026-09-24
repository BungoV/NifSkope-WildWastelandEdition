# -*- coding: utf-8 -*-
"""patch_seed.py -- the continuation is SEEDED by a breadth-first walk from
the fixed texels before the SOR, in the C++ and in the prototype."""
import sys
sys.path.insert(0, 'scratchpad/water4_20260910')
from splice import splice   # noqa

splice('src/watermark.cpp', [
 ("\t\t// the continuation: red-black SOR on the free texels\n\t\t{\n",
  '''\t\t/* the continuation is SEEDED by a breadth-first walk from the fixed
\t\t * texels -- every free texel takes the vector of the texel it was reached
\t\t * from -- so a pond behind a one-texel neck starts with a direction
\t\t * instead of waiting for a relaxation whose changes fall under the
\t\t * tolerance before they reach it (the prototype left two such ponds at
\t\t * zero); the SOR then smooths what the walk laid down */
\t\t{
\t\t\tstd::vector<int> queue;
\t\t\tstd::vector<quint8> seen( n, 0 );
\t\t\tqueue.reserve( n );
\t\t\tfor ( size_t at = 0; at < n; at++ )
\t\t\t\tif ( fixed[at] ) {
\t\t\t\t\tseen[at] = 1;
\t\t\t\t\tqueue.push_back( int( at ) );
\t\t\t\t}
\t\t\tfor ( size_t q = 0; q < queue.size(); q++ ) {
\t\t\t\tconst int at = queue[q];
\t\t\t\tconst int x = at % W, y = at / W;
\t\t\t\tconst int dx4[4] = { -1, 1, 0, 0 }, dy4[4] = { 0, 0, -1, 1 };
\t\t\t\tfor ( int k4 = 0; k4 < 4; k4++ ) {
\t\t\t\t\tconst int nx = x + dx4[k4], ny = y + dy4[k4];
\t\t\t\t\tif ( nx < 0 || ny < 0 || nx >= W || ny >= H )
\t\t\t\t\t\tcontinue;
\t\t\t\t\tconst size_t nat = size_t( ny ) * size_t( W ) + size_t( nx );
\t\t\t\t\tif ( !F->mask[nat] || seen[nat] )
\t\t\t\t\t\tcontinue;
\t\t\t\t\tseen[nat] = 1;
\t\t\t\t\tdxv[nat] = dxv[size_t( at )];
\t\t\t\t\tdyv[nat] = dyv[size_t( at )];
\t\t\t\t\tqueue.push_back( int( nat ) );
\t\t\t\t}
\t\t\t}
\t\t}
\t\t// the continuation: red-black SOR on the free texels
\t\t{
''', 'replace'),
])
splice('scratchpad/water4_20260910/smooth_probe.py', [
 ("    wm = wet.astype(float)\n",
  """    # seed every free texel from its nearest fixed one by a breadth-first walk
    from collections import deque
    fixedM = np.zeros_like(wet); fixedM[g.ys[moving], g.xs[moving]] = True
    seen = fixedM.copy(); dq = deque(zip(*np.nonzero(fixedM)))
    while dq:
        y, x = dq.popleft()
        for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < W and 0 <= ny < H and wet[ny, nx] and not seen[ny, nx]:
                seen[ny, nx] = True; cx[ny, nx] = cx[y, x]; cy[ny, nx] = cy[y, x]; dq.append((ny, nx))
    wm = wet.astype(float)
""", 'replace'),
])
