"""measure_sizes.py - the size table, built by actually emitting each plugin
rather than extrapolating from one.

Loads the master once and builds every configuration in memory. Nothing is
written to disk unless --write is given.
"""

from __future__ import print_function

import sys

import make_landfix_esp as M
import esp_lib_land as L


def main():
    master = M.Master(M.DEFAULT_ESM)
    ltex = 0x00021336            # LDirtGravel01, the 2nd most-used real base
    rows = []

    configs = []
    for r in (16, 24, 32, 40, 48, 64, 96):
        configs.append(('ring +-%d' % r, dict(ring=r)))
    configs.append(('all untextured cells', dict()))

    for name, sel in configs:
        assigns = M.synth_assignments(master, ltex, only_untextured=True, **sel)
        if not assigns:
            rows.append((name, 0, 0, 0, 0))
            continue
        comp, cs = M.build_plugin(master, assigns, compress_land=True,
                                  wrld_mode='merge-rnam', verbose=False)
        raw, rs = M.build_plugin(master, assigns, compress_land=False,
                                 wrld_mode='merge-rnam', verbose=False)
        nowrld, ns = M.build_plugin(master, assigns, compress_land=True,
                                    wrld_mode='none', verbose=False)
        rows.append((name, len(assigns), len(comp), len(raw), len(nowrld)))
        print('  %-24s %6d cells  compressed %12s  uncompressed %12s  '
              'compressed-no-WRLD %12s'
              % (name, len(assigns), '{:,}'.format(len(comp)),
                 '{:,}'.format(len(raw)), '{:,}'.format(len(nowrld))))
        sys.stdout.flush()

    print()
    print('| selection | cells | compressed | uncompressed | compressed, no WRLD record |')
    print('| --- | ---: | ---: | ---: | ---: |')
    for name, n, c, r, nw in rows:
        print('| %s | %s | %s | %s | %s |'
              % (name, '{:,}'.format(n), '{:,}'.format(c), '{:,}'.format(r),
                 '{:,}'.format(nw)))

    # the ESL question, measured rather than asserted
    print()
    print('=== ESL flag ===')
    assigns = M.synth_assignments(master, ltex, ring=24, only_untextured=True)
    esl, _ = M.build_plugin(master, assigns, wrld_mode='merge-rnam',
                            esl=True, verbose=False)
    import fo4esm as E
    import struct
    sig, dsize, flags, formid, tail = E.read_record_header(esl, 0)
    print('  TES4 flags with --esl-flag: 0x%08X (ESL bit 0x200 set: %s)'
          % (flags, bool(flags & L.TES4_FLAG_ESL)))
    newrecs = []
    def on(off, s, ds, fl, fid, tl, st):
        if (fid >> 24) != 0:
            newrecs.append(fid)
    for node in E.top_level_groups(esl):
        E.walk(esl, node.offset + 24, node.offset + node.gsize, [node], on)
    print('  records in the plugin whose FormID is NOT master-index 00: %d'
          % len(newrecs))
    print('  (ESL restricts NEW records to 0x800-0xFFF; this plugin creates none, '
          'so the restriction cannot be violated)')


if __name__ == '__main__':
    main()
