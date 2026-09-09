import struct, sys
b = open(sys.argv[1], 'rb').read()
h, w = struct.unpack_from('<II', b, 12)
fcc = b[84:88]
# block size from the HEADER: ours is BC1 and vanilla's is BC3, and assuming
# 16 read past the end of our own file once (2026-09-07)
bs = 8 if fcc == b'DXT1' else 16
col = 0 if bs == 8 else 8          # BC3 puts its alpha block first
off = 148 if fcc == b'DX10' else 128
bw, bh = (w + 3) // 4, (h + 3) // 4


def block(o):
    c0, c1 = struct.unpack_from('<HH', b, o)
    bits = struct.unpack_from('<I', b, o + 4)[0]

    def rgb(c):
        return (((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63,
                (c & 31) * 255 // 31)
    e0, e1 = rgb(c0), rgb(c1)
    if c0 > c1 or bs == 16:
        t = [e0, e1, tuple((2 * e0[k] + e1[k]) // 3 for k in range(3)),
             tuple((e0[k] + 2 * e1[k]) // 3 for k in range(3))]
    else:
        t = [e0, e1, tuple((e0[k] + e1[k]) // 2 for k in range(3)), (0, 0, 0)]
    return [t[(bits >> (2 * k)) & 3] for k in range(16)]


px = [[(0, 0, 0)] * w for _ in range(h)]
for by in range(bh):
    for bx in range(bw):
        t = block(off + (by * bw + bx) * bs + col)
        for k in range(16):
            y, x = by * 4 + k // 4, bx * 4 + k % 4
            if y < h and x < w:
                px[y][x] = t[k]
res = [0.0, 0.0, 0.0]
neg = [0, 0, 0]
diff = [0, 0, 0, 0]
tot = [0, 0, 0, 0]
for y in range(h):
    row = px[y]
    for x in range(w):
        c = row[x]
        if x:
            tot[x % 4] += 1
            if c != row[x - 1]:
                diff[x % 4] += 1
    if y % 8:                      # the channel verdict needs a sample, not all
        continue
    for c in row:
        v = [c[k] / 255.0 * 2 - 1 for k in range(3)]
        for k in range(3):
            a, bb = v[(k + 1) % 3], v[(k + 2) % 3]
            s = 1.0 - a * a - bb * bb
            res[k] += abs((s ** 0.5 if s > 0 else 0.0) - abs(v[k]))
            if c[k] < 128:
                neg[k] += 1
up = min(range(3), key=lambda k: (neg[k], res[k]))
print('UP=%s %s' % ('RGB'[up], ' '.join(
    'D%d=%d' % (i, 100 * diff[i] // max(1, tot[i])) for i in range(4))))
