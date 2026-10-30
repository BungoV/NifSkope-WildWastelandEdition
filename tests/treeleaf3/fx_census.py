"""Lane TREELEAF3 (FIX1030C, 2026-10-30): which alpha-tested BSEffectShaderProperty shapes reach a side pass, and does
their colour pass test a different alpha from the raw base alpha the side pass tests? Offline: the game's BA2s by
bytes (tests/spells/near_texture_stack.py), no NifSkope, no window.

THE SIDE PASSES (code read, src/gl at the lane's base):
  sun cascades (sunshadow.cpp), cell-light cubes (celllights.cpp, both caster loops) and reflection probes
  (cellreflprobe.cpp) take only Shape::wwCastsSunShadow() shapes, which is `... && !bsesp`: no effect shape.
  Hi-Z (cellgpucull.cpp vertexProgram): only fo4_cell/fo4_default-family programs are candidates or occluders; an
  effect shape draws through fo4_effectshader/fo4_effectcell*: never.
  TAA object vectors (taainputs.cpp movers): EVERY visible shape that moved this step (skinned + moved, the grass wind,
  or its world transform changed) -- effect shapes included. taa_objvel.frag discards `base.a * alpha < threshold`
  with alpha = 1 for an effect shape (lsp is null) and the threshold of Shape::wwDrawAlpha( "taa" ).

THE COLOUR PASS (fo4_effectshader.frag) tests color.a = base.a (1 then the palette's alpha under greyscaleAlpha)
  x falloff (useFalloff) x the vertex alpha (FO4: whenever the mesh has vertex colours, unless Tree_Anim)
  x baseColor.a^2 x (cell) the soft fade, against the material's ref with `>` (alphaFlags 4), else the NIF function.

So a shape is a MISMATCH candidate when it is effect + alpha-tested as drawn, can MOVE (skinned, or its NIF animates
transforms), and its composed alpha can differ from base.a (any factor above != 1).

usage: python -I tests/treeleaf3/fx_census.py <out.txt> [<Data dir> [<resource root> ...]]
       env MAXNIF=<n> caps the NIF walk (0 = all).
Prints `FXCENSUS ...` lines; rc 0 (a census, not a gate).
"""
import os
import struct
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(REPO, 'tests', 'spells'))
from near_texture_stack import TexStack  # noqa: E402

DATA = 'X:/Programs/Steam/steamapps/common/Fallout 4/Data'
BS = chr(92)
SHAPES = ('BSTriShape', 'BSSubIndexTriShape', 'BSMeshLODTriShape', 'BSDynamicTriShape')
ANIM = ('NiTransformController', 'NiMultiTargetTransformController', 'NiControllerManager', 'NiKeyframeController',
        'BSLagBoneController', 'NiLookAtController', 'BSProceduralLightningController')


def read_bgem(b):
    """src/io/materialfile.cpp Material::readFile + EffectMaterial::readFile (the fields this census needs)."""
    o = 4
    ver, = struct.unpack_from('<I', b, o); o += 4
    o += 4 + 16	# tile flags, uv
    alpha, = struct.unpack_from('<f', b, o); o += 4
    blend = b[o]; o += 1 + 8
    ref = b[o]; test = b[o + 1]; o += 2
    o += 2 + 2 + 4	# zwrite ztest, ssr wetssr, decal twosided decalnofade nonoccluder
    o += 2 + 4	# refraction, falloff, power
    o += 1	# env mapping
    if ver < 10:
        o += 4
    o += 1	# grayscale to palette colour
    if ver >= 6:
        o += 1
    ntex = (8 if ver <= 20 else 10) if ver >= 10 else 5
    for _ in range(ntex):
        n, = struct.unpack_from('<I', b, o); o += 4 + n
    if ver >= 10:
        if ver > 20:
            glass = b[o]; o += 1
            if glass:
                o += 12 + 8 + (4 if ver > 21 else 0)
        o += 1 + 4
    blood, efl, fall, fallc, gsa, soft = b[o:o + 6]; o += 6
    o += 12 + 4	# base colour, scale
    fsa, fso, fsto, fstp = struct.unpack_from('<4f', b, o); o += 16
    return dict(ver=ver, alpha=alpha, blend=blend, ref=ref, test=test, falloff=fall, fstart=fsto, fstop=fstp,
                gsa=gsa, soft=soft)


def walk_nif(b):
    """[(shape dict)] for every BSTriShape-family shape whose shader property is a BSEffectShaderProperty."""
    o = b.index(b'\n') + 1
    ver, = struct.unpack_from('<I', b, o); o += 4
    o += 1 + 4
    nblocks, bsver = struct.unpack_from('<II', b, o); o += 8
    if ver != 0x14020007 or bsver != 130:
        return [], set()
    for _ in range(4):
        o += 1 + b[o]
    ntypes, = struct.unpack_from('<H', b, o); o += 2
    types = []
    for _ in range(ntypes):
        n, = struct.unpack_from('<I', b, o)
        types.append(b[o + 4:o + 4 + n].decode('latin1')); o += 4 + n
    tidx = struct.unpack_from('<%dH' % nblocks, b, o); o += 2 * nblocks
    bsize = struct.unpack_from('<%dI' % nblocks, b, o); o += 4 * nblocks
    nstr, = struct.unpack_from('<I', b, o); o += 8
    strings = []
    for _ in range(nstr):
        n, = struct.unpack_from('<I', b, o)
        strings.append(b[o + 4:o + 4 + n].decode('latin1')); o += 4 + n
    ngroups, = struct.unpack_from('<I', b, o); o += 4 + 4 * ngroups
    starts = []
    for i in range(nblocks):
        starts.append(o)
        o += bsize[i]
    tn = [types[t] for t in tidx]
    present = set(tn)

    def net(p):	# NiObjectNET: name, extra data list, controller
        name, = struct.unpack_from('<i', b, p); p += 4
        ne, = struct.unpack_from('<I', b, p); p += 4 + 4 * ne
        ctrl, = struct.unpack_from('<i', b, p); p += 4
        return p, (strings[name] if 0 <= name < len(strings) else ''), ctrl

    out = []
    for i in range(nblocks):
        if tn[i] not in SHAPES:
            continue
        p, sname, _ = net(starts[i])
        p += 4 + 12 + 36 + 4 + 4	# flags, translation, rotation, scale, collision
        p += 16	# bounding sphere
        skin, shader, alphap = struct.unpack_from('<iii', b, p); p += 12
        desc, = struct.unpack_from('<Q', b, p); p += 8
        if not (0 <= shader < nblocks) or tn[shader] != 'BSEffectShaderProperty':
            continue
        ntri, = struct.unpack_from('<I', b, p); p += 4
        nv, = struct.unpack_from('<H', b, p); p += 2
        dsize, = struct.unpack_from('<I', b, p); p += 4
        attr = desc >> 44
        vsize = (desc & 0xF) * 4
        coff = ((desc >> 24) & 0xF) * 4
        vmin = 255
        if (attr & 0x20) and dsize > 0 and vsize > 0:
            for v in range(nv):
                a = b[p + v * vsize + coff + 3]
                if a < vmin:
                    vmin = a
        s = dict(shape=tn[i], name=sname, skinned=skin >= 0 or bool(attr & 0x40), colors=bool(attr & 0x20),
                 vmin=vmin, nv=nv)
        # the effect shader property
        q, mat, _ = net(starts[shader])
        f1, f2 = struct.unpack_from('<II', b, q); q += 8
        q += 16
        n, = struct.unpack_from('<I', b, q); q += 4 + n	# source texture
        q += 4	# clamp, lighting influence, env min lod, unused
        fsa, fso, fsto, fstp = struct.unpack_from('<4f', b, q); q += 16
        bc = struct.unpack_from('<4f', b, q); q += 16
        s.update(mat=mat, f1=f1, f2=f2, fstart=fsto, fstop=fstp, bca=bc[3])
        s['alpha'] = None
        if 0 <= alphap < nblocks and tn[alphap] == 'NiAlphaProperty':
            q, _, _ = net(starts[alphap])
            fl, = struct.unpack_from('<H', b, q)
            s['alpha'] = (fl, b[q + 2])
        out.append(s)
    return out, present


def mat_key(p):
    low = p.lower().replace('/', BS)
    if ('materials' + BS) in low:
        low = low[low.index('materials' + BS):]
    elif not low.startswith('materials' + BS):
        low = 'materials' + BS + low.lstrip(BS)
    return low


def main(a):
    out_path = a[0]
    roots = [a[1] if len(a) > 1 else DATA] + a[2:]
    t0 = time.time()
    st = TexStack(roots, 'stack')
    lines = []

    def say(s):
        print(s)
        lines.append(s)
        sys.stdout.flush()

    nifs = sorted(k for k in st.arch if k.startswith('meshes' + BS) and k.endswith('.nif'))
    maxnif = int(os.environ.get('MAXNIF', '0') or 0)
    if maxnif:
        nifs = nifs[::max(1, len(nifs) // maxnif)][:maxnif]
    say('fx_census: %d root(s), %d NIFs' % (len(roots), len(nifs)))
    mats = {}
    bad = 0
    tot = dict(fx=0, tested=0, tested_opaque=0, tested_blend=0, mat=0, mat_unread=0)
    cand = []	# tested + can move
    by_dir = {}
    for k in nifs:
        b = st.read(k)
        try:
            shapes, present = walk_nif(b)
        except (struct.error, ValueError, IndexError):
            bad += 1
            continue
        anim = sorted(present.intersection(ANIM))
        for s in shapes:
            tot['fx'] += 1
            m = None
            if s['mat'].lower().endswith('.bgem'):
                mk = mat_key(s['mat'])
                if mk not in mats:
                    mb = st.read(mk)
                    try:
                        mats[mk] = read_bgem(mb) if mb else None
                    except (struct.error, IndexError):
                        mats[mk] = None
                m = mats[mk]
                tot['mat' if m else 'mat_unread'] += 1
            # the drawn test (Shape::wwDrawAlpha / setupProgramCE1): a readable material wins
            if m:
                test, blend, ref, func = bool(m['test']), bool(m['blend']), m['ref'], 4
            elif s['alpha']:
                fl, thr = s['alpha']
                test, blend, ref, func = bool(fl >> 9 & 1), bool(fl & 1), thr, (fl >> 10) & 7
            else:
                test = blend = False
                ref, func = 0, -1
            if not test:
                continue
            tot['tested'] += 1
            tot['tested_blend' if blend else 'tested_opaque'] += 1
            gsa = bool(m['gsa']) if m else bool(s['f1'] >> 5 & 1)
            fall = bool(m['falloff']) if m else bool(s['f1'] >> 6 & 1)
            fst, fsp = (m['fstart'], m['fstop']) if m else (s['fstart'], s['fstop'])
            soft = bool(m['soft']) if m else bool(s['f1'] >> 30 & 1)
            ea = m['alpha'] if m else s['bca']
            treeanim = bool(s['f2'] >> 29 & 1)
            factors = []
            if gsa:
                factors.append('greyscaleAlpha')
            if fall and not (fst == 1.0 and fsp == 1.0):
                factors.append('falloff(%.2f..%.2f)' % (fst, fsp))
            if s['colors'] and not treeanim and s['vmin'] < 255:
                factors.append('vertexAlpha(min %d)' % s['vmin'])
            if ea < 1.0:
                factors.append('baseColorAlpha^2(%.3f)' % (ea * ea))
            if soft:
                factors.append('soft')
            moves = s['skinned'] or bool(anim)
            d = BS.join(k.split(BS)[1:2])
            r = by_dir.setdefault(d, [0, 0, 0])
            r[0] += 1
            r[1] += moves
            r[2] += moves and bool(factors)
            if moves:
                cand.append((k, s, m, blend, ref, func, factors, anim))
    say('FXCENSUS NIFs unreadable %d; effect-shader shapes %d (BGEM read %d, BGEM named but unread %d)' % (
        bad, tot['fx'], tot['mat'], tot['mat_unread']))
    say('FXCENSUS alpha-tested as drawn: %d (not blended %d, blended %d)' % (
        tot['tested'], tot['tested_opaque'], tot['tested_blend']))
    nm = len(cand)
    nf = sum(1 for c in cand if c[6])
    nfo = sum(1 for c in cand if c[6] and not c[3])
    say('FXCENSUS tested AND can move (skinned, or the NIF animates transforms) = TAA mover candidates: %d; '
        'of them composed alpha != base alpha: %d (not blended %d)' % (nm, nf, nfo))
    fc = {}
    for c in cand:
        for f in c[6]:
            key = f.split('(')[0]
            fc[key] = fc.get(key, 0) + 1
    say('FXCENSUS factors over the mover candidates: %s' % ', '.join('%s %d' % kv for kv in sorted(fc.items())))
    # what TREELEAF3's TAA rule reproduces exactly: baseColor.a^2, the vertex alpha, the palette alpha and the strict
    # threshold; falloff (the per-pixel normal-map normal) and the soft fade (the frame's depth) stay residual
    exact = sum(1 for c in cand if c[6] and all(f.split('(')[0] in ('baseColorAlpha^2', 'vertexAlpha', 'greyscaleAlpha')
                                              for f in c[6]))
    say('FXCENSUS of the %d composed != base: every factor reproduced by the TAA rule %d; with a residual falloff or '
        'soft factor %d' % (nf, exact, nf - exact))
    funcs = {}
    for c in cand:
        kk = ('mat >' if c[2] else 'nif func %d' % c[5])
        funcs[kk] = funcs.get(kk, 0) + 1
    say('FXCENSUS test function over the mover candidates (side pass keeps a >= t): %s' % funcs)
    for d, (n, mv, f) in sorted(by_dir.items(), key=lambda kv: -kv[1][0])[:12]:
        say('FXCENSUS dir %-14s tested %5d, can move %5d, + composed != base %5d' % (d, n, mv, f))
    say('FXCENSUS examples (tested, moves, composed != base):')
    shown = 0
    for k, s, m, blend, ref, func, factors, anim in cand:
        if not factors or shown >= 25:
            continue
        shown += 1
        say('  %s :%s %s ref %d %s skinned %d anim %s %s' % (k[len('meshes' + BS):], s['name'][:24],
            s['mat'][-40:] if s['mat'] else '(no bgem)', ref, 'blend' if blend else 'opaque', s['skinned'],
            ','.join(a_[:12] for a_ in anim) or '-', ' '.join(factors)))
    say('time %.0f s' % (time.time() - t0))
    with open(out_path, 'w') as fo:
        fo.write('\n'.join(lines) + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
