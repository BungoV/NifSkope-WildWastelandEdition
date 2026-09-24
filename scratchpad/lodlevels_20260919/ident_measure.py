#!/usr/bin/env python3
"""IDENT lane -- the measurement.  Reads esm.pkl (read-only, another lane's)
and ident_scan.pkl (this lane's, in the session scratchpad) and answers, for the
downtown Boston ARCHITECTURE REFRs, which record-level mechanism (if any) says
"these REFRs are one building".

Prints a plain-text report on stdout; ident_notes.md is written by hand from it.
"""
import math
import os
import pickle
import struct
import sys
from collections import Counter, defaultdict

LANE = os.path.dirname(os.path.abspath(__file__))
SCRATCH = ('C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/'
           '392777f8-9016-4913-858d-16d6eec4c01a/scratchpad')

# ------------------------------------------------------------------ regions
# chunk 4.4.-12 == cells x 4..7, y -12..-9 == world x 16384..32767, y -49152..-32769
PRIMARY = (4, 7, -12, -9)
WIDE = (0, 11, -16, -5)


def cell(x, y):
    return int(math.floor(x / 4096.0)), int(math.floor(y / 4096.0))


def inbox(x, y, box):
    cx, cy = cell(x, y)
    return box[0] <= cx <= box[1] and box[2] <= cy <= box[3]


# -------------------------------------------------------- the architecture rule
def is_arch(modl):
    """The base's near MODL path has a path COMPONENT that is 'architecture' or
    'buildings', or a component that ENDS IN 'kit'.  Case-insensitive; both
    slashes are separators."""
    if not modl:
        return False
    parts = modl.lower().replace('\\', '/').split('/')
    for p in parts[:-1]:                 # directory components only
        if p in ('architecture', 'buildings') or p.endswith('kit'):
            return True
    return False


def diag(pts):
    """World-bound diagonal of a set of (x,y,z) points, in units."""
    if not pts:
        return 0.0
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    zs = [p[2] for p in pts]
    return math.sqrt((max(xs) - min(xs)) ** 2 + (max(ys) - min(ys)) ** 2
                     + (max(zs) - min(zs)) ** 2)


def med(v):
    if not v:
        return 0
    s = sorted(v)
    n = len(s)
    return s[n // 2] if n % 2 else 0.5 * (s[n // 2 - 1] + s[n // 2])


def main():
    E = pickle.load(open(os.path.join(LANE, 'esm.pkl'), 'rb'))
    S = pickle.load(open(os.path.join(SCRATCH, 'ident_scan.pkl'), 'rb'))
    bases, refs = E['bases'], E['refs']
    refsub, layr, scol, pkin, rfgp = (S['refsub'], S['layr'], S['scol'],
                                      S['pkin'], S['rfgp'])
    out = []

    def say(s=''):
        out.append(s)
        print(s)

    # ---------------------------------------------------------- selection
    sel = {}
    for name, box in (('PRIMARY', PRIMARY), ('WIDE', WIDE)):
        allr = [r for r in refs if inbox(r[2], r[3], box)]
        arch = [r for r in allr if is_arch(bases.get(r[1], {}).get('modl', ''))]
        sel[name] = (allr, arch)
        say('%-8s cells x %d..%d y %d..%d : %d REFRs, %d architecture (%.1f%%)'
            % (name, box[0], box[1], box[2], box[3], len(allr), len(arch),
               100.0 * len(arch) / max(1, len(allr))))
    # base-type breakdown of the architecture selection
    for name in ('PRIMARY', 'WIDE'):
        c = Counter(bases.get(r[1], {}).get('type', '?') for r in sel[name][1])
        say('  %s architecture base types: %s' % (name, dict(c.most_common())))
    say()

    # what the rule threw away, to show it is not silently dropping buildings
    c = Counter()
    for r in sel['PRIMARY'][0]:
        m = bases.get(r[1], {}).get('modl', '')
        if not is_arch(m):
            c[m.lower().replace('\\', '/').split('/')[0]] += 1
    say('PRIMARY non-architecture REFRs by top folder: %s'
        % dict(c.most_common(12)))
    say()

    # ------------------------------------------------- mechanism tallies
    table = []

    def bounds_of_groups(groups, pos):
        """groups: {key: [refForm...]}.  Returns (sizes, diags) for size>=2."""
        sizes, diags = [], []
        for k, v in groups.items():
            sizes.append(len(v))
            if len(v) >= 2:
                diags.append(diag([pos[f] for f in v if f in pos]))
        return sizes, diags

    for region in ('PRIMARY', 'WIDE'):
        allr, arch = sel[region]
        pos = {r[0]: (r[2], r[3], r[4]) for r in allr}
        archset = set(r[0] for r in arch)
        N = len(arch)
        say('=' * 72)
        say('REGION %s  --  %d architecture REFRs' % (region, N))
        say('=' * 72)

        # ---- (a) SCOL base
        scolrefs = [r for r in arch if bases.get(r[1], {}).get('type') == 'SCOL']
        say('(a) SCOL base: %d of %d = %.2f%%'
            % (len(scolrefs), N, 100.0 * len(scolrefs) / max(1, N)))
        if scolrefs:
            pc, dg = [], []
            for r in scolrefs:
                s = scol.get(r[1])
                if not s:
                    continue
                pc.append(sum(max(1, n // 28) for _, n in s['parts']))
                o = s['obnd']
                if o:
                    dg.append(math.sqrt((o[3] - o[0]) ** 2 + (o[4] - o[1]) ** 2
                                        + (o[5] - o[2]) ** 2))
            say('    median part placements per SCOL instance: %s' % med(pc))
            say('    median OBND diagonal of a SCOL instance : %.0f units (%.1f m)'
                % (med(dg), med(dg) * 0.0142875))
            ex = Counter((r[1]) for r in scolrefs)
            say('    five most-placed SCOL bases here:')
            for f, n in ex.most_common(5):
                s = scol.get(f, {})
                o = s.get('obnd')
                d = (math.sqrt((o[3] - o[0]) ** 2 + (o[4] - o[1]) ** 2
                               + (o[5] - o[2]) ** 2) if o else 0)
                say('      %08X %-44s parts=%3d obndDiag=%6.0f  x%d'
                    % (f, s.get('edid', '?'),
                       sum(max(1, n2 // 28) for _, n2 in s.get('parts', [])), d, n))
            # group = one SCOL instance
            table.append((region, 'SCOL base', len(scolrefs),
                          med([1] * len(scolrefs)), med(dg)))
        else:
            table.append((region, 'SCOL base', 0, 0, 0))

        # ---- (b) XLYR
        g = defaultdict(list)
        for r in arch:
            s = refsub.get(r[0], {})
            if b'XLYR' in s:
                g[struct.unpack_from('<I', s[b'XLYR'][0], 0)[0]].append(r[0])
        n = sum(len(v) for v in g.values())
        say('(b) XLYR layer: %d of %d = %.2f%%  in %d distinct layers'
            % (n, N, 100.0 * n / max(1, N), len(g)))
        if g:
            sizes, diags = bounds_of_groups(g, pos)
            say('    median layer group size %s, median group diagonal %.0f units (%.1f m)'
                % (med(sizes), med(diags), med(diags) * 0.0142875))
            say('    twenty most-used layers here:')
            for f, v in sorted(g.items(), key=lambda kv: -len(kv[1]))[:20]:
                ed, par = layr.get(f, ('?', 0))
                d = diag([pos[x] for x in v if x in pos])
                say('      %08X %-46s n=%5d diag=%8.0f  parent=%08X'
                    % (f, ed, len(v), d, par))
            table.append((region, 'XLYR layer', n, med(sizes), med(diags)))
        else:
            table.append((region, 'XLYR layer', 0, 0, 0))

        # ---- (c) XESP
        g = defaultdict(list)
        for r in arch:
            s = refsub.get(r[0], {})
            if b'XESP' in s:
                g[struct.unpack_from('<I', s[b'XESP'][0], 0)[0]].append(r[0])
        n = sum(len(v) for v in g.values())
        say('(c) XESP enable parent: %d of %d = %.2f%%  in %d parent groups'
            % (n, N, 100.0 * n / max(1, N), len(g)))
        if g:
            sizes, diags = bounds_of_groups(g, pos)
            hist = Counter(sizes)
            say('    group-size histogram: %s'
                % dict(sorted(hist.items())[:14]))
            say('    median group size %s, median diagonal of a >=2 group %.0f units'
                % (med(sizes), med(diags)))
            table.append((region, 'XESP parent', n, med(sizes), med(diags)))
        else:
            table.append((region, 'XESP parent', 0, 0, 0))

        # ---- (d) PKIN / XRFG
        g = defaultdict(list)
        for r in arch:
            s = refsub.get(r[0], {})
            if b'XRFG' in s:
                g[struct.unpack_from('<I', s[b'XRFG'][0], 0)[0]].append(r[0])
        n = sum(len(v) for v in g.values())
        say('(d) XRFG reference group: %d of %d = %.2f%%  in %d groups'
            % (n, N, 100.0 * n / max(1, N), len(g)))
        if g:
            sizes, diags = bounds_of_groups(g, pos)
            say('    median group size %s, median diagonal %.0f units'
                % (med(sizes), med(diags)))
            for f, v in sorted(g.items(), key=lambda kv: -len(kv[1]))[:10]:
                e = rfgp.get(f, ('?', '?', 0, 0))
                say('      %08X edid=%-30s name=%-22s pkin=%08X n=%d diag=%.0f'
                    % (f, e[0], e[1], e[3], len(v),
                       diag([pos[x] for x in v if x in pos])))
            table.append((region, 'XRFG refgroup', n, med(sizes), med(diags)))
        else:
            table.append((region, 'XRFG refgroup', 0, 0, 0))
        npk = sum(1 for r in arch if b'XPRD' in refsub.get(r[0], {})
                  or b'XPPA' in refsub.get(r[0], {}))
        say('    XPRD/XPPA (patrol, NOT a pack-in mark) on architecture REFRs: %d' % npk)

        # ---- (e) XLKR / XLRT / XMBR
        for sig, label in ((b'XLKR', 'XLKR linked-ref'), (b'XLRT', 'XLRT loc-ref-type'),
                           (b'XMBR', 'XMBR multibound')):
            g = defaultdict(list)
            carriers = 0
            for r in arch:
                s = refsub.get(r[0], {})
                if sig not in s:
                    continue
                carriers += 1
                if sig == b'XLKR':
                    for pay in s[sig]:
                        if len(pay) >= 8:
                            tgt = struct.unpack_from('<I', pay, 4)[0]
                        elif len(pay) >= 4:
                            tgt = struct.unpack_from('<I', pay, 0)[0]
                        else:
                            continue
                        g[tgt].append(r[0])
                elif sig == b'XLRT':
                    for pay in s[sig]:
                        for k in range(len(pay) // 4):
                            g[struct.unpack_from('<I', pay, k * 4)[0]].append(r[0])
                else:
                    g[struct.unpack_from('<I', s[sig][0], 0)[0]].append(r[0])
            say('(e) %s: %d of %d = %.2f%%  in %d groups'
                % (label, carriers, N, 100.0 * carriers / max(1, N), len(g)))
            if g:
                sizes, diags = bounds_of_groups(g, pos)
                say('    median group size %s, median diagonal of a >=2 group %.0f units'
                    % (med(sizes), med(diags)))
                say('    largest groups: %s'
                    % sorted((len(v) for v in g.values()), reverse=True)[:8])
                table.append((region, label, carriers, med(sizes), med(diags)))
            else:
                table.append((region, label, 0, 0, 0))

        # ---- (f) residual
        anysig = (b'XLYR', b'XESP', b'XLKR', b'XLRT', b'XMBR', b'XRFG')
        bare = [r for r in arch
                if bases.get(r[1], {}).get('type') != 'SCOL'
                and not any(s in refsub.get(r[0], {}) for s in anysig)]
        say('(f) NOTHING AT ALL (no SCOL base, none of %s): %d of %d = %.2f%%'
            % ('/'.join(s.decode() for s in anysig), len(bare), N,
               100.0 * len(bare) / max(1, N)))
        # and the stricter residual: nothing but XLYR
        onlylayer = [r for r in arch
                     if bases.get(r[1], {}).get('type') != 'SCOL'
                     and set(refsub.get(r[0], {})) <= {b'XLYR'}]
        say('    of which "XLYR only or nothing": %d = %.2f%%'
            % (len(onlylayer), 100.0 * len(onlylayer) / max(1, N)))
        say()

    say('=' * 72)
    say('SUMMARY TABLE (rows: region | mechanism | REFRs | median group | median diag)')
    for row in table:
        allr, arch = sel[row[0]]
        say('%-8s %-18s %6d  %6.2f%%  %8s  %10.0f'
            % (row[0], row[1], row[2], 100.0 * row[2] / max(1, len(arch)),
               row[3], row[4]))

    # ------------------------------------------------- PKIN survival, globally
    say()
    say('PKIN records in the shipped Fallout4.esm: %d' % len(pkin))
    say('RFGP records in the shipped Fallout4.esm: %d' % len(rfgp))
    say('LAYR records in the shipped Fallout4.esm: %d' % len(layr))
    say('SCOL records in the shipped Fallout4.esm: %d' % len(scol))
    # do any RFGP in the region point at a PKIN?
    pk = Counter()
    for f, (ed, nm, rf, p) in rfgp.items():
        pk['haspkin' if p else 'nopkin'] += 1
    say('RFGP with a PNAM pack-in pointer: %s' % dict(pk))

    with open(os.path.join(SCRATCH, 'ident_measure.txt'), 'w') as f:
        f.write('\n'.join(out))


if __name__ == '__main__':
    main()
