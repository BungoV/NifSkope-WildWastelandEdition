"""One line of geometry counts for a .BTO / .BTR, read from the file's own bytes.

  python geomstats.py <file>  ->  "N shapes, V verts, T tris"

The reader is scratchpad/btr_spacing_20260909/btrparse.py, the same one the
triangulation plans are drawn with, so a caption number and a picture never come
from two different readers.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'btr_spacing_20260909'))
from btrparse import find_shapes

data = open(sys.argv[1], 'rb').read()
sh = find_shapes(data)
print('%d shapes, %s verts, %s tris'
      % (len(sh), format(sum(s['numVerts'] for s in sh), ','),
         format(sum(s['numTris'] for s in sh), ',')))
