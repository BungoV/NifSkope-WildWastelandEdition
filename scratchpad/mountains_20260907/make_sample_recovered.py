"""make_sample_recovered.py - emit a sample recovered.txt in the format
make_landfix_esp.py consumes, so the RECOVER lane has something concrete to
write against and so the --recovered code path is exercised end to end rather
than only the --placeholder development path.

The sample assigns plausible-looking real Commonwealth base textures by a
crude rule (coast/ocean near the map edge, dirt-gravel inland). It is NOT a
recovery - RECOVER's output replaces it. It exists to prove the file format
and the tool's parsing of it.
"""

import struct
import sys

import make_landfix_esp as M
import esp_lib_land as L

# real LTEX FormIDs, from find_ltex.py's census of what Bethesda actually uses
LDirtGravel01 = 0x00021336
LCoastSandWet01 = 0x000AB72E
LOceanFloor01 = 0x0014BF47
LBlastedForestDirt01 = 0x00125C6C


def main(out_path, min_relief=64.0):
    master = M.Master(M.DEFAULT_ESM)
    lines = [
        '# sample recovered.txt for make_landfix_esp.py',
        '#',
        '#   <cellX> <cellY> <q0> <q1> <q2> <q3>   four quadrant LTEX FormIDs',
        '#   <cellX> <cellY> <ltex>                the same texture in all four',
        '#',
        '# a quadrant field of "-" leaves that quadrant as the master has it;',
        '# "0" deletes its base texture. Quadrants are 0 BL, 1 BR, 2 TL, 3 TR.',
        '# FormIDs are master-relative: 00xxxxxx means Fallout4.esm.',
        '#',
        '# THIS FILE IS A PLACEHOLDER, not a recovery. The rule below is crude',
        '# on purpose - it exists to exercise the format.',
        '',
    ]
    n = 0
    for (x, y), ent in sorted(master.cells.items()):
        if ent.land_bytes is None:
            continue
        base, has_alpha = L.land_quadrant_state(ent.land_payload)
        if base or has_alpha:
            continue                      # already painted; leave it alone
        rel = M.land_relief(ent.land_payload)
        if rel is None or rel < min_relief:
            continue
        r = max(abs(x), abs(y))
        if r >= 70:
            tex = LOceanFloor01
        elif r >= 55:
            tex = LCoastSandWet01
        elif r >= 44:
            tex = LBlastedForestDirt01
        else:
            tex = LDirtGravel01
        lines.append('%d %d %08X' % (x, y, tex))
        n += 1
    with open(out_path, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    print('wrote %s: %d cell assignments' % (out_path, n))


if __name__ == '__main__':
    # NOT 'recovered.txt' by default: that name belongs to the RECOVER lane in
    # this shared folder, and an earlier run of this script overwrote their
    # file. Never take a filename another lane owns.
    main(sys.argv[1] if len(sys.argv) > 1 else 'sample_recovered.txt')
