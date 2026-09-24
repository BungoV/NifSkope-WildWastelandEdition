#!/usr/bin/env python3
"""Lane PANEL1: count the lodgen sub-command's switches, by SETTING, out of
src/nifcli.cpp itself, so the audit table in the report cannot drift from a
hand tally.  Run from the repo root.

A `--x` / `--no-x` pair is ONE setting.  The two retired spellings are excluded
(they only print an error naming their replacement).  The class of each setting
is stated here, once, and the report quotes these counts.
"""
import re
import sys

FIRST, LAST = 6535, 7014          # the lodgen switch block in src/nifcli.cpp

PAIRS = {
    '--no-cover': '--cover', '--no-shore-denser': '--shore-denser',
    '--no-roads': '--roads', '--no-terrain-object-ao': '--terrain-object-ao',
    '--no-road-raised': '--road-raised', '--no-road-sidewalks': '--road-sidewalks',
    '--no-vt': '--vt', '--no-vt-btr': '--vt-btr',
    '--no-terrain-identity': '--terrain-identity', '--no-trees-only': '--trees-only',
    '--no-atlas': '--atlas', '--no-arrays': '--arrays', '--no-merge': '--merge',
    '--no-aggregate': '--aggregate', '--vt-cover-in-mask': '--vt-cover-in-color',
}
RETIRED = {'--lodt', '--lodv-check'}

DIAG = set("""--list-worldspaces --print-source --list-files --probe --probe-out --dump-land
--dump-layers --dump-shapes --dump-geometry --dump-cover --dump-object-ao --vt-estimate
--lodm-check --lodt-check --corpus-hash --verify-only --btd-probe --from-btd --native-verify
--native-verify-corpus --native-fixture --native-mesh-report --list-impostor-candidates
--candidates --stress-file --stress-threads --stress-reps --stress-sabotage --water-report
--ao-grey --cell""".split())

PATH = set("""--out-dir --tex-dir --data-root --resource --plugins-txt --impostors
--vanilla-lod-root --msn-cache --water-velocities""".split())

# rows the panel already had before this lane
HAVE = set("""--worldspace --terrain-region --terrain --objects --dim --out-dir --vt --native
--lodl --heightmap --impostors --resource --mo2 --refresh-ao --no-identity --no-ao --ao-skirt
--cull-buried --cull-margin --slot-fallback --trees-only --impostors-from-level --card-half-aux
--atlas --arrays --no-simplify --simplify8 --simplify16 --simplify32 --simplify-error
--target-tris --shore-denser --shore-density --geomorph --terrain-identity --cover --grass-tint
--vt-finest --vt-btr --heightmap-size""".split())

# PATH switches the panel deliberately does not offer, each with its reason
CLI_ONLY_PATH = {
    '--tex-dir': 'the panel derives it from the output mod folder',
    '--data-root': 'house rule: assets come from the game\'s own folders and archives',
    '--plugins-txt': 'the MO2 source reads Mod Organizer\'s own plugins.txt',
}


def main(path='src/nifcli.cpp'):
    lines = open(path, encoding='utf-8', errors='replace').read().split('\n')
    order = []
    for i in range(FIRST - 1, LAST):
        for m in re.finditer(r'QLatin1String\( "(--[a-z0-9-]+)" \)', lines[i]):
            name = PAIRS.get(m.group(1), m.group(1))
            if name not in order:
                order.append(name)
    spellings = sum(
        len(re.findall(r'QLatin1String\( "(--[a-z0-9-]+)" \)', lines[i]))
        for i in range(FIRST - 1, LAST))
    settings = [s for s in order if s not in RETIRED]

    cli_only = set(CLI_ONLY_PATH) | DIAG
    rows = []
    for cls, members in (
            ('BAKE', [s for s in settings if s not in DIAG and s not in PATH]),
            ('PATH', [s for s in settings if s in PATH]),
            ('DIAG', [s for s in settings if s in DIAG])):
        have = [s for s in members if s in HAVE]
        only = [s for s in members if s in cli_only]
        new = [s for s in members if s not in HAVE and s not in cli_only]
        rows.append((cls, len(members), len(have), len(new), len(only)))
        if new:
            print('%s NEW (%d): %s' % (cls, len(new), ' '.join(new)))
    print()
    print('spellings %d, pairs %d, retired %d -> %d settings'
          % (spellings, len(PAIRS), len(RETIRED), len(settings)))
    print('%-6s %8s %14s %8s %9s' % ('class', 'settings', 'already a row', 'NEW', 'CLI-ONLY'))
    for cls, tot, have, new, only in rows:
        print('%-6s %8d %14d %8d %9d' % (cls, tot, have, new, only))
    t = [sum(c) for c in zip(*[r[1:] for r in rows])]
    print('%-6s %8d %14d %8d %9d' % ('total', *t))
    return 0


if __name__ == '__main__':
    sys.exit(main(*sys.argv[1:]))
