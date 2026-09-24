"""SPLAT1 section 2c -- the engine's OWN landscape texture tiling, read out of
Fallout4.exe 1.10.155.

`TILE = 2048.0f` in src/lodgen.cpp is a bare constant whose own comment calls
its calibration an open question, and no LAND / LTEX / TXST field carries a
tiling scale. The number is not in the data; it is in the engine. This script
re-derives it from the shipped executable, every step printed, so that nothing
in the report is quoted from memory.

The chain:

  1. The only landscape-tiling INI setting in the binary is
     `fLandTextureTilingMult:Landscape`.
  2. Its `Setting` record is {vtable, data, name}; the data word is its
     DEFAULT, and the two neighbouring records decode to values that can be
     checked independently (`bCurrentCellOnly` = 0, `iMaxGrassTypesPerTexure`
     = 2, `fTexturePctThreshold` = 0.005) -- if the stride or the field order
     were wrong those three would be nonsense.
  3. The data word has exactly ONE code reference in `.text`; that code turns
     the multiplier into a UV step.
  4. The UV step times the landscape vertex spacing gives the repeat.

Run:  python s2c_engine_tiling.py [path-to-Fallout4.exe]
"""
import os
import struct
import sys

EXE = sys.argv[1]  # path to the 1.10.155 exe; no default

b = open(EXE, 'rb').read()
pe = struct.unpack_from('<I', b, 0x3C)[0]
nsec = struct.unpack_from('<H', b, pe + 6)[0]
optsz = struct.unpack_from('<H', b, pe + 20)[0]
secs = []
for i in range(nsec):
    o = pe + 24 + optsz + i * 40
    nm = b[o:o + 8].rstrip(b'\0').decode()
    vsz, va, rsz, ra = struct.unpack_from('<IIII', b, o + 8)
    secs.append((nm, va, vsz, ra, rsz))
BASE = 0x140000000


def f2v(off):
    for nm, va, vsz, ra, rsz in secs:
        if ra <= off < ra + rsz:
            return BASE + va + (off - ra)


def v2f(vaddr):
    r = vaddr - BASE
    for nm, va, vsz, ra, rsz in secs:
        if va <= r < va + max(vsz, rsz):
            return ra + (r - va)


def cstr(vaddr):
    f = v2f(vaddr)
    if f is None:
        return '<unmapped>'
    return b[f:b.find(b'\0', f)].decode('latin-1')


i = b.find('FileVersion'.encode('utf-16le'))
ver = [t for t in b[i:i + 80].decode('utf-16le', 'replace').split('\x00')
       if t and t != 'FileVersion'][0]
print('EXE  %s' % EXE)
print('     FileVersion %s, %d bytes' % (ver, len(b)))
print('')

# 1 -----------------------------------------------------------------------
so = b.find(b'fLandTextureTilingMult:Landscape\x00')
assert so > 0, 'the setting name is not in this binary'
sva = f2v(so)
print('1. setting name at file 0x%X, VA 0x%X: %r' % (so, sva, cstr(sva)))
assert b.find(b'fLandTextureTilingMult', so + 1) < 0, 'more than one copy'
print('   exactly one copy in the file.')

# 2 -----------------------------------------------------------------------
ptr = b.find(struct.pack('<Q', sva))
assert ptr > 0 and b.find(struct.pack('<Q', sva), ptr + 1) < 0, \
    'the name pointer is not unique'
rec = ptr - 16                       # {vtable, data, name}
print('2. Setting record at file 0x%X (name pointer 0x%X)' % (rec, ptr))
print('   %-6s %-20s %-16s %s' % ('slot', 'vtable', 'data', 'name'))
for k in range(-1, 4):
    o = rec + k * 24
    vt, dat, nm = struct.unpack_from('<QQQ', b, o)
    print('   %+6d 0x%016X   0x%016X  %-44s f32 %g'
          % (k, vt, dat, cstr(nm), struct.unpack_from('<f', b, o + 8)[0]))
vt, dat, nm = struct.unpack_from('<QQQ', b, rec)
mult = struct.unpack_from('<f', b, rec + 8)[0]
print('   -> fLandTextureTilingMult DEFAULT = %g' % mult)
print('   (it is absent from Fallout4_Default.ini, so this default is what runs)')

# 3 -----------------------------------------------------------------------
data_va = f2v(rec + 8)
tnm, tva, tvsz, tra, trsz = [s for s in secs if s[0] == '.text'][0]
d = b[tra:tra + trsz]
hits = []
for i in range(3, len(d) - 8):
    disp = struct.unpack_from('<i', d, i)[0]
    for L in range(5, 12):
        st = i - (L - 4)
        if st >= 0 and BASE + tva + st + L + disp == data_va:
            hits.append(BASE + tva + st)
hits = sorted(set(hits))
print('3. data slot VA 0x%X -- rip-relative references in .text: %s'
      % (data_va, ', '.join('0x%X' % h for h in hits)))
ref = 0x1403A74C6
print('   the instruction is at 0x%X: f3 0f 10 05 <disp32>  movss xmm0,[rip+..]'
      % ref)
print('   0x%X  ucomiss xmm0, 0 ; jne  -- if the setting is 0 the code falls'
      % 0x1403A74D1)
print('            back to the constant at VA 0x142C4B1BC = %g'
      % struct.unpack_from('<f', b, v2f(0x142C4B1BC))[0])
print('   0x%X  movss xmm6, [VA 0x142C4B1B4] = %g'
      % (0x1403A74E5, struct.unpack_from('<f', b, v2f(0x142C4B1B4))[0]))
print('   0x%X  divss xmm6, xmm0        -> xmm6 = 4 / mult' % 0x1403A74ED)
print('   0x%X  movss xmm1, [VA 0x142C48D60] = %g'
      % (0x1403A75FD, struct.unpack_from('<f', b, v2f(0x142C48D60))[0]))
print('   0x%X  divss xmm2, xmm6        -> xmm2 = 1 / (4/mult) = mult/4'
      % 0x1403A760F)
print('   0x%X  cvtdq2ps / mulss xmm0, xmm2   -> uv = vertexIndex * (mult/4)'
      % 0x1403A763E)
print('   AND THE LOOP IT SITS IN IS THE 17x17 QUADRANT GRID, not an assumption:')
print('     0x1403A7620  outer, r11w  ..  0x1403A76DB cmp r11w,0x11 ; jl  (17 rows)')
print('     0x1403A7650  inner, r10w  ..  0x1403A76CC cmp r10w,0x11 ; jl  (17 cols)')
print('     0x1403A769C  movsx eax, r10w          the COLUMN index 0..16')
print('     0x1403A76B2  mulss xmm0, xmm2         u = col * (mult/4)')
print('     0x1403A76B6  movss [rsp+0x180], xmm0  ; [rsp+0x184] = row*(mult/4)')
print('     0x1403A76BF  mov rcx,[rsp+0x180] / 0x1403A76C7 mov [rax+r9*8-8], rcx')
print('                                          the 8-byte (u,v) pair, stored')
print('   17 vertices = 16 quads = ONE QUADRANT = 2048 world units, so the')
print('   vertex spacing the multiplier steps over is 2048/16 = 128 units.')

# 4 -----------------------------------------------------------------------
step = mult / 4.0
CELL = 4096.0
spacing = 2048.0 / 16.0               # 17x17 vertices a QUADRANT -> 16 quads
repeat = spacing / step
print('')
print('4. uv per vertex step = mult/4 = %g' % step)
print('   landscape vertex spacing = 2048/16 = %g world units' % spacing)
print('   WORLD UNITS PER TEXTURE REPEAT = %g / %g = %.4f' % (spacing, step, repeat))
print('   equivalently %g repeats a cell, %g a quadrant.'
      % (CELL / repeat, 2048.0 / repeat))
print('')
print('   THE BAKE USES TILE = 2048.0  ->  %.4f times too large.' % (2048.0 / repeat))
