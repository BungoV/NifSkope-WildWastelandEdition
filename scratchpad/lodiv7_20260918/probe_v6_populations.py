#!/usr/bin/env python3
"""LODIV7 step 0 control: on the SHIPPED v6 pair, does the per-vertex AO stream's
mean over a placement's vertices equal the instance record's 0x10 `ao` byte?

The v7 brief pre-registers the same test for the NEW sky stream against the 0x11
`sky` byte ("they are the same cast"). The two v6 quantities already on disk are
the SAME PAIR OF POPULATIONS, so this answers, before a line of v7 is written,
whether that gate is reachable at all -- and it reads bytes the shipped writer
produced, not bytes this lane produced.

Usage: python probe_v6_populations.py <Commonwealth.lodi> [chunkX chunkY]
"""
import struct, sys, collections

def le(fmt, b, off):
    return struct.unpack_from('<' + fmt, b, off)

p = sys.argv[1]
b = open(p, 'rb').read()
assert b[:4] == b'LODI', b[:4]
ver = le('I', b, 4)[0]
n = le('I', b, 0x58)[0]
chunkCount = le('I', b, 0x54)[0]
west, south, east, north = le('hhhh', b, 0x48)
offChunks = le('Q', b, 0x68)[0]
offInst = le('Q', b, 0x78)[0]
offCold = le('Q', b, 0x80)[0]
offPao = le('Q', b, 0xE4)[0]
offVao = le('Q', b, 0xF4)[0]
vaoBytes = le('I', b, 0xFC)[0]
print('version %d instances %d chunks %d extent W%d S%d E%d N%d' % (ver, n, chunkCount, west, south, east, north))
w = east - west + 1
first = list(le('%dI' % (n + 1), b, offVao))
data = b[offVao + 4 * (n + 1): offVao + vaoBytes]
assert first[-1] == len(data), (first[-1], len(data))

# chunk table -> per-instance chunk
inst_chunk = [None] * n
for ci in range(chunkCount):
    iFirst, iCount = le('II', b, offChunks + 32 * ci)
    if iCount == 0:
        continue
    cx = west + (ci % w)
    cy = north - (ci // w)
    for i in range(iFirst, iFirst + iCount):
        inst_chunk[i] = (cx, cy)

sel = None
if len(sys.argv) > 3:
    sel = (int(sys.argv[2]), int(sys.argv[3]))

rows = []
empties = 0
for i in range(n):
    if sel and inst_chunk[i] != sel:
        continue
    ao, sky, ground, seed = b[offInst + 24 * i + 0x10: offInst + 24 * i + 0x14]
    lo, hi = first[i], first[i + 1]
    if hi == lo:
        empties += 1
        continue
    sl = data[lo:hi]
    m = sum(sl) / float(len(sl))
    rows.append((i, len(sl), m, ao, sky, abs(m - ao)))

rows.sort(key=lambda r: -r[5])
tot = len(rows)
print('placements with a stream slice: %d   empty slices: %d' % (tot, empties))
if tot:
    within2 = sum(1 for r in rows if r[5] <= 2.0)
    print('|mean(vertexAo) - ao byte| <= 2 on %d of %d = %.2f%%' % (within2, tot, 100.0 * within2 / tot))
    print('worst 10 (index, verts, mean(vertexAo), ao byte 0x10, sky byte 0x11, |diff|):')
    for r in rows[:10]:
        print('  %7d  %5d  %8.2f  %4d  %4d  %8.2f' % r)
    print('mean over placements of mean(vertexAo) = %.2f ; mean of ao byte = %.2f'
          % (sum(r[2] for r in rows) / tot, sum(r[3] for r in rows) / float(tot)))
    dist = sum(1 for r in rows if len(set(data[first[r[0]]:first[r[0] + 1]])) > 1)
    print('placements whose stream has >1 distinct value: %d of %d' % (dist, tot))
