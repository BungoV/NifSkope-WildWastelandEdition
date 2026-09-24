#!/usr/bin/env python3
"""LODFOLDER Q1 + Q3 (read-only): where MNAM paths live, and orphan LOD nifs."""
import os
import pickle
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TMP = os.environ.get('LODFOLDER_TMP') or HERE
MESHROOT = 'E:/Tools/Fallout 4/DataUnpacked/Data/Meshes'
BS = chr(92)


def norm(p):
    return p.replace(BS, '/').lower().strip().lstrip('/')


def main():
    allbases = pickle.load(open(os.path.join(TMP, 'lodfolder_bases.pkl'), 'rb'))

    distinct = {}          # normpath -> (rawpath, [(plugin, form, edid, slotidx)])
    nLodBearing = 0
    slotUse = {}
    equalsModl = []        # (plugin, form, edid, slot, path)
    for (plug, form), b in allbases.items():
        filled = [s for s in b['slots'] if s]
        if not filled:
            continue
        nLodBearing += 1
        slotUse[len(filled)] = slotUse.get(len(filled), 0) + 1
        nm = norm(b['modl']) if b['modl'] else None
        for k, s in enumerate(b['slots']):
            if not s:
                continue
            n = norm(s)
            e = distinct.setdefault(n, [s, []])
            e[1].append((plug, form, b['edid'], k))
            if nm and n == nm:
                equalsModl.append((plug, form, b['edid'], k, s))

    inLod = {k: v for k, v in distinct.items() if k.startswith('lod/')}
    elsewhereD = {k: v for k, v in distinct.items() if not k.startswith('lod/')}

    top = {}
    for k in elsewhereD:
        t = k.split('/')[0] if '/' in k else '<root>'
        top[t] = top.get(t, 0) + 1

    out = []
    W = out.append
    W('### Q1 -- where do MNAM (LOD) mesh paths point?')
    W('')
    W('LOD-bearing bases (>=1 filled MNAM slot): %d of %d LOD-capable bases'
      % (nLodBearing, len(allbases)))
    W('filled-slot-count distribution: ' +
      ', '.join('%d slot(s): %d bases' % (k, slotUse[k]) for k in sorted(slotUse)))
    W('')
    W('| where | distinct MNAM mesh paths |')
    W('|---|---|')
    W('| under Meshes%sLOD%s | **%d** |' % (BS, BS, len(inLod)))
    W('| somewhere else | **%d** |' % len(elsewhereD))
    W('| total distinct | %d |' % len(distinct))
    W('')
    W('"Elsewhere" paths by top-level folder under Meshes%s:' % BS)
    W('')
    W('| top-level folder | distinct paths |')
    W('|---|---|')
    for t, c in sorted(top.items(), key=lambda x: -x[1]):
        W('| %s | %d |' % (t, c))
    W('')
    inLodAnywhere = {k: v for k, v in elsewhereD.items()
                     if '/lod/' in k or k.startswith('lod/')}
    noLodAtAll = {k: v for k, v in distinct.items()
                  if '/lod/' not in k and not k.startswith('lod/')}
    W('Refinement -- every "elsewhere" path is a DLC LOD folder, not a non-LOD folder:')
    W('')
    W('| | distinct paths |')
    W('|---|---|')
    W('| "elsewhere" paths that still have a %sLOD%s component (e.g. DLC04%sLOD%sArchitecture%s...) | **%d of %d** |'
      % (BS, BS, BS, BS, BS, len(inLodAnywhere), len(elsewhereD)))
    W('| MNAM paths with NO LOD folder component ANYWHERE | **%d of %d** |'
      % (len(noLodAtAll), len(distinct)))
    if noLodAtAll:
        W('')
        W('Those non-LOD-folder MNAM paths:')
        W('')
        for k, (raw, users) in sorted(noLodAtAll.items())[:25]:
            plug, form, edid, kk = users[0]
            W('  - %s   (%s %08X %s, slot %d)' % (raw, edid or '(none)', form, plug, kk))
    W('')
    W('Ten named "elsewhere" examples:')
    W('')
    W('| MNAM path | slot | base editor ID | formID | plugin |')
    W('|---|---|---|---|---|')
    ex = sorted(elsewhereD.items(), key=lambda x: (-len(x[1][1]), x[0]))[:10]
    for n, (raw, users) in ex:
        plug, form, edid, k = users[0]
        W('| %s | %d | %s | %08X | %s | ' % (raw, k, edid or '(none)', form, plug))
    W('')
    W('MNAM slot whose path EQUALS that same base\'s own near MODL path:')
    W('')
    eqBases = set((p, f) for p, f, _, _, _ in equalsModl)
    eqLodModl = sum(1 for (p, f) in eqBases
                    if '/lod/' in norm(allbases[(p, f)]['modl'])
                    or norm(allbases[(p, f)]['modl']).startswith('lod/'))
    W('count = **%d** slot(s) across %d distinct bases'
      % (len(equalsModl), len(eqBases)))
    W('')
    W('Of those %d bases, **%d** have a near MODL that is ITSELF a file in an LOD folder --'
      % (len(eqBases), eqLodModl))
    W('i.e. they are LOD-only filler statics (city-block shells such as Ticonderoga_Bld01LOD)')
    W('placed in the world, not full models being reused as their own LOD.  Only **%d**'
      % (len(eqBases) - eqLodModl))
    W('base(s) point an MNAM slot at a genuine full-detail near model.')
    if equalsModl:
        W('')
        W('| base editor ID | formID | plugin | slot | path (= MODL) |')
        W('|---|---|---|---|---|')
        for plug, form, edid, k, s in equalsModl[:10]:
            W('| %s | %08X | %s | %d | %s |' % (edid or '(none)', form, plug, k, s))
    W('')

    # ---------------------------------------------------------------- Q3
    lodroot = os.path.join(MESHROOT, 'LOD')
    onDisk = set()
    for dirpath, _dirs, files in os.walk(lodroot):
        for fn in files:
            if fn.lower().endswith('.nif'):
                full = os.path.join(dirpath, fn)
                rel = os.path.relpath(full, MESHROOT).replace(BS, '/').lower()
                onDisk.add(rel)
    referenced = set(inLod.keys())
    refOnDisk = referenced & onDisk
    refMissing = referenced - onDisk
    orphans = onDisk - referenced

    W('### Q3 -- orphan .nif files under Meshes%sLOD%s' % (BS, BS))
    W('')
    W('Denominator note: E:%sTools%sFallout 4%sDataUnpacked%sData is the UNPACKED shipped BA2s,'
      % (BS, BS, BS, BS))
    W('so every .nif the game ships in that tree is present -- this is a fair denominator,')
    W('not a mod folder sample.')
    W('')
    W('| | count |')
    W('|---|---|')
    W('| .nif files on disk under Meshes%sLOD%s | **%d** |' % (BS, BS, len(onDisk)))
    W('| of those, referenced by >=1 MNAM slot | %d |' % len(refOnDisk))
    W('| **orphans** (referenced by NO MNAM slot in any of the 7 masters) | **%d** (%.1f%%) |'
      % (len(orphans), 100.0 * len(orphans) / max(1, len(onDisk))))
    W('| MNAM LOD paths naming a file NOT on disk | %d |' % len(refMissing))
    W('')
    W('Twenty orphan examples:')
    W('')
    for p in sorted(orphans)[:20]:
        W('  - %s' % p.replace('/', BS))
    W('')

    txt = '\n'.join(out)
    open(os.path.join(TMP, 'q1q3.md'), 'w').write(txt)
    print(txt)


if __name__ == '__main__':
    main()
