#!/usr/bin/env python3
"""Two numbers the C++ reader also prints, read by the independent Python one.
A file two readers disagree about is a file neither of them has read."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lodgen_native_decode as D
T = D.read_lodi(sys.argv[1])
print('groups=%d vertexSkyBytesTotal=%d' % (T['header']['groupCount'], len(T['vertexSky'])))
