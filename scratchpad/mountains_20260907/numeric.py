"""Two numeric claims in the LODGEN packing contract, checked rather than
recalled. Lane IDENTITY, read-only."""
import struct

def h(x):
    return struct.unpack('<e', struct.pack('<e', x))[0]

bad = None
for i in range(0, 70000):
    if h(float(i)) != float(i):
        bad = i
        break
print('first integer NOT exactly representable in IEEE binary16: %s' % bad)
print('   h(2047)=%s h(2048)=%s h(2049)=%s h(2050)=%s'
      % (h(2047.0), h(2048.0), h(2049.0), h(2050.0)))
print('   h(65504)=%s  (65504 is the largest finite binary16)' % h(65504.0))

worst = 0
for v in range(256):
    f = v / 255.0
    worst = max(worst, abs(round(f * 255.0) - v))
print('8-bit UNORM, v/255 then round(f*255): max error over 0..255 = %d' % worst)

worstf = 0
for v in range(256):
    f = struct.unpack('<f', struct.pack('<f', v / 255.0))[0]
    worstf = max(worstf, abs(int(f * 255.0 + 0.5) - v))
print('   same but stored through a float32: max error = %d' % worstf)

# what the far-ring simplifier does: qRound(col.red()*255) on a Color4 that was
# itself built from a byte
worstq = 0
for v in range(256):
    f = struct.unpack('<f', struct.pack('<f', v / 255.0))[0]
    back = min(255, max(0, int(f * 255.0 + 0.5)))
    worstq = max(worstq, abs(back - v))
print('   lodgen.cpp:7635 path (float32 store, qRound, qBound): max error = %d'
      % worstq)
