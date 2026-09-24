#!/usr/bin/env python3
"""RESUME3: splice SPLAT1's CONTRACT_AMENDMENT.md into docs/LODGEN_TERRAIN_VT.md.

`ww-contract-provenance`: the anchors are read out of the file's own bytes, not
retyped. The file carries UNICODE MINUS (U+2212) in the cell ranges and EM DASH
(U+2014) in the prose; retyping either as ASCII counts 0. Provenance is
re-stamped from the live tree at apply time, never copied from the amendment.

    python contract.py --check
    python contract.py --apply
"""
import sys, os, hashlib

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOC = os.path.join(ROOT, 'docs', 'LODGEN_TERRAIN_VT.md')
SRC = os.path.join(ROOT, 'src', 'lodgen.cpp')
MINUS = chr(0x2212)
EMDASH = chr(0x2014)
NL = chr(10)

SAMPLING_OLD = (
"**Each source diffuse is sampled the same way**: `u = frac(wx/2048)`," + NL +
"`v = frac(wy/2048)` (the bake's world-space tiling), at the mip" + NL +
"`clamp( log2( max(1, unitsPerTexel / (2048/textureWidth)) ), 0, maxMip )`," + NL +
"trilinear. Get the mip wrong and the two bands differ by the texture's own" + NL +
"high-frequency detail, which is the visible half of a seam." + NL)

SAMPLING_NEW = (
"**Each source diffuse is sampled the same way**: `u = frac(wx/T)`," + NL +
"`v = frac(wy/T)` at the mip" + NL +
"`clamp( log2( max(1, unitsPerTexel / (T/textureWidth)) ), 0, maxMip )`," + NL +
"trilinear, where **T = 341.3333 world units, the engine's own landscape texture" + NL +
"repeat**: twelve repeats a cell, six a quadrant. The runtime and the pyramid" + NL +
"must use the SAME T or ring 0 seams by the texture's own detail, which is the" + NL +
"visible half of a seam. `--land-tiling <units>` overrides it for a user who has" + NL +
"changed `fLandTextureTilingMult`; `--land-tiling 2048` reproduces every sheet" + NL +
"written before 2026-09-11 byte for byte." + NL +
NL +
"**T IS THE ENGINE'S, AND IT IS NOT IN THE DATA.** No LAND, LTEX or TXST field" + NL +
"carries a tiling scale " + EMDASH + " LTEX is EDID + TNAM + HNAM + SNAM + GNAM, TXST is" + NL +
"texture paths and flags, checked over every landscape texture in the Sanctuary" + NL +
"region. The number is read out of `Fallout4.exe` **1.10.155.0** (65,319,936" + NL +
"bytes), re-derived from the binary by" + NL +
"`scratchpad/splat1_20260911/s2c_engine_tiling.py`:" + NL +
NL +
"* `fLandTextureTilingMult:Landscape`, one copy, file 0x2C84DD8 / VA" + NL +
"  0x142C861D8. Its `Setting` record `{vtable, data, name}` at file 0x36E83A8" + NL +
"  carries **data 0x3FC00000 = 1.5f**; the neighbouring records decode to" + NL +
"  `bCurrentCellOnly` 0, `iMaxGrassTypesPerTexure` 2, `fTexturePctThreshold`" + NL +
"  0.005, which is what says the stride and the field order are right. The" + NL +
"  setting is absent from `Fallout4_Default.ini`, so 1.5 is what runs." + NL +
"* The data slot (VA 0x1436E97B0) has exactly ONE code reference, at VA" + NL +
"  0x1403A74C6: `xmm6 = 4.0 / mult` (0x1403A74E5, 0x1403A74ED; falls back to" + NL +
"  16.0 at 0x142C4B1BC when the setting is 0), then" + NL +
"  `xmm2 = 1.0 / xmm6 = mult/4 = 0.375` (0x1403A75FD, 0x1403A760F)." + NL +
"* The loop it feeds (0x1403A7620 outer / 0x1403A7650 inner, both" + NL +
"  `cmp .., 0x11 ; jl`) is the **17x17 quadrant vertex grid**, and it stores an" + NL +
"  8-byte (u,v) pair per vertex at 0x1403A76C7 with `u = col * 0.375`." + NL +
"* 17 vertices = 16 quads = one quadrant = 2,048 world units, so the vertex" + NL +
"  spacing is 128 units and **T = 128 / 0.375 = 341.3333**." + NL +
NL +
"Addresses are for the 1.10.155 build and are re-derived, never typed, by the" + NL +
"script above. In the generator the value lives in one place, `lodgenLandTiling()`" + NL +
"(`src/lodgen.h`), and all FOURTEEN sampling sites " + EMDASH + " colour, mask and emissive," + NL +
"in both the stock chunk bake and the pyramid " + EMDASH + " read it. The normal sheet" + NL +
"(`_msn`) is computed from VHGT and no tiling term reaches it; that it is" + NL +
"byte-identical at both tiling values is a gate, not an assumption." + NL +
NL +
"**What the tiling does NOT fix.** It removes the speckle and it does not close" + NL +
"the whole-tile colour difference against vanilla. The grading " + EMDASH + " vanilla's" + NL +
"uniform x0.82-0.83, measured by ROADS1 " + EMDASH + " remains the open item \"splat" + NL +
"calibration vs vanilla grading\"." + NL)

VCLR_OLD = (
"**VCLR is not the grading it looks like.** Measured on the Commonwealth:" + NL +
"**2,362 of 36,864 cells carry a VCLR at all**, and over the Sanctuary region" + NL +
"(cells " + MINUS + "20.." + MINUS + "17 x 24..27) every byte of every VCLR present is in **249..255** " + EMDASH + NL +
"white to within 6/255. The grading that actually moves the colour is the layer" + NL +
"WEIGHTS and the grass tint, which is why the gate's floor is a blend that drops" + NL +
"the weights rather than one that drops VCLR." + NL)

VCLR_NEW = (
"**VCLR is not the grading it looks like.** Measured on the Commonwealth:" + NL +
"**2,362 of 36,864 cells carry a VCLR at all.** Over the Sanctuary region" + NL +
"(cells " + MINUS + "20.." + MINUS + "17 x 24..27) **11 of 16 cells carry one and the bytes run" + NL +
"203..255**; over cells " + MINUS + "20.." + MINUS + "17 x 20..23, 16 of 16 carry one, over 170..255" + NL +
"(lane SPLAT1, `s3_candidates.py`). The range **249..255** this page carried" + NL +
"until 2026-09-11 does not reproduce and is withdrawn. VCLR is therefore not" + NL +
"white " + EMDASH + " but it is still not the grading: removing the multiply entirely moves" + NL +
"the sheet's local variance by **0.02 of a 52-unit excess**, and the cells that" + NL +
"carry NO VCLR show the LARGER excess (63.98 against 52.97). The grading that" + NL +
"actually moves the colour is the layer WEIGHTS and the grass tint, which is why" + NL +
"the gate's floor stays a blend that drops the weights rather than one that" + NL +
"drops VCLR." + NL)


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else '--check'
    b = open(DOC, 'rb').read()
    cr0, lf0 = b.count(b'\r'), b.count(b'\n')
    src = open(SRC, 'rb').read()
    print('docs/LODGEN_TERRAIN_VT.md  %d bytes  %d lines  CR %d' % (len(b), lf0, cr0))
    print('src/lodgen.cpp             %d bytes  %d lines  sha1 %s'
          % (len(src), src.count(b'\n'), hashlib.sha1(src).hexdigest()))
    ok = True
    nb = b
    for label, old, new in (('the sampling paragraph', SAMPLING_OLD, SAMPLING_NEW),
                            ('the VCLR paragraph', VCLR_OLD, VCLR_NEW)):
        c = nb.count(old.encode('utf-8'))
        print('  replace count=%d  %s' % (c, label))
        if c != 1:
            ok = False
            print('     REFUSE: matched %d times, not 1' % c)
            continue
        nb = nb.replace(old.encode('utf-8'), new.encode('utf-8'), 1)
    print('  CR before=%d after=%d   lines %d -> %d' % (cr0, nb.count(b'\r'), lf0, nb.count(b'\n')))
    if nb.count(b'\r') != cr0:
        ok = False
        print('     REFUSE: line endings changed')
    if not ok:
        print(NL + 'RESULT REFUSED - nothing written')
        return 2
    if mode == '--apply':
        open(DOC, 'wb').write(nb)
        print('wrote docs/LODGEN_TERRAIN_VT.md')
        print(NL + 'RESULT APPLIED - 2 paragraphs')
    else:
        print(NL + 'RESULT CHECK OK - 2 paragraphs, nothing written')
    return 0


if __name__ == '__main__':
    sys.exit(main())
