"""Lane SPEED1 (2026-10-02): the independent checker of tests/spells/cell_speed.sh.

No NifSkope code and none of its timers. Three verdict lines for one cell:

  PICTURE  every "new" picture against reference 1, pixel by pixel. The bound is MEASURED: the two
           reference runs' own difference (the count of pixels that differ at all, and the largest
           channel step), plus the stated margin (MARGIN_* below): the count AND the step must hold.
  COUNTS   the cell's own census lines (references, placements, shapes, vertices, triangles, lights ...)
           printed by the window, line for line the same in every run; lines that carry a time are left out.
  GAIN     wall seconds and peak working set as the operating system counted them (cell_speed_run.py):
           the median new run against the fastest / leanest reference run, each better by the floor.

  SAVE     (--save) the document saved by the new path against the old path's file: the same bytes, and
           the new run had shapes whose rows were written late: half when a reader asked for them by name,
           the rest by the save itself.

usage: cell_speed_check.py <out_dir> <cell_id> <ref_runs> <new_runs> [--floor-s F] [--floor-mb F]
       cell_speed_check.py --save <ref.nif> <new.nif> <new.notes>
"""
import os
import re
import statistics
import sys

import numpy as np
from PIL import Image

# The floors the gate holds (fractions of the reference). Measured 2026-10-02 on three cells (the lane's
# table in WW_CHANGES.md): 63-70% fewer seconds, 41-49% less peak memory. Set under the measured gain so
# machine noise does not trip them and far over zero so the old path (--red slow, 0%) cannot pass.
FLOOR_S = 0.50
FLOOR_MB = 0.33

# The picture's margin over the two reference runs' own difference. Measured 2026-10-02 with exes that
# draw the same thing: Vault111Cryo references differ by 30-47 pixels of step 1, a third run by 42; the 3x3
# block's by 24, a third run by 67 of step 2; the shelter's by 0. Two samples under-read a range, so the
# count is allowed 4 x the references' own + 64 and the step 2 x + 2. A fault is far outside both: one
# model's placements moved = tens of thousands of pixels at steps over 100.
MARGIN_PX_TIMES, MARGIN_PX = 4, 64
MARGIN_STEP_TIMES, MARGIN_STEP = 2, 2

TIMED = re.compile(r'\b\d+(\.\d+)?\s*(ms|s|sec|seconds|MB|GB)\b')


def picture(path):
    return np.asarray(Image.open(path).convert('RGB'), dtype=np.int16)


def differ(a, b):
    if a.shape != b.shape:
        return a.shape[0] * a.shape[1], 255
    d = np.abs(a - b).max(axis=2)
    return int((d > 0).sum()), int(d.max())


def census(path):
    """The window's own "cell view:" block, without the lines that carry a time."""
    out, on = [], False
    with open(path, 'r', errors='replace') as f:
        for line in f:
            line = line.rstrip('\r\n')
            if line.endswith('cell view:'):   # "[Info] cell view:"
                on = True
                continue
            if on:
                if line.startswith(' ') or line.startswith('cell view '):
                    if not TIMED.search(line):
                        out.append(line.strip())
                elif line.strip():
                    on = False
    return out


def saved(a):
    """--save <ref.nif> <new.nif> <new.notes>: the file a save writes is the old path's file, byte for byte,
    and the new run really had shapes waiting beside the document (else nothing was tested)."""
    import hashlib
    ref, new, notes = a
    try:
        hr = hashlib.sha256(open(ref, 'rb').read()).hexdigest()
        hn = hashlib.sha256(open(new, 'rb').read()).hexdigest()
        size_r, size_n = os.path.getsize(ref), os.path.getsize(new)
        text = open(notes, encoding='utf-8', errors='replace').read()
    except OSError as e:
        print('SAVE FAIL  %s' % e)
        return 1
    m = re.search(r'cell speed save: (\d+) shapes waited beside the document, (\d+) after the save, file written', text)
    waited, left = (int(m.group(1)), int(m.group(2))) if m else (0, -1)
    # the header's block sizes WHILE the rows wait: the old path's sum (its notes sit beside the new run's)
    stated = []
    for t in (notes.replace('.savenew.', '.saveref.'), notes):
        try:
            h = re.search(r'cell speed header: (\d+) bytes of blocks stated before the save',
                          open(t, encoding='utf-8', errors='replace').read())
        except OSError:
            h = None
        stated.append(int(h.group(1)) if h else -1)
    # the net under by-name readers: before the save, every other waiting shape was asked for its rows the
    # way a mesh tool asks (a lookup by name); each must have had them, and the file is still the old one
    n = re.search(r'cell speed net: (\d+) shapes asked for their rows by name, (\d+) had them', text)
    asked, had = (int(n.group(1)), int(n.group(2))) if n else (0, -1)
    ok = (hr == hn and size_r > 100000 and waited > 0 and left == 0 and stated[0] > 100000
          and stated[0] == stated[1] and asked > 0 and had == asked)
    print('SAVE %s  old path %d bytes, new path %d bytes, %s; %d shapes waited beside the document, %d of %d asked '
          'by name had their rows, the save wrote the rest, %d left; header block sizes before the save %d vs %d'
          % ('PASS' if ok else 'FAIL', size_r, size_n, 'the same bytes' if hr == hn else 'DIFFERENT bytes',
             waited, had, asked, left, stated[0], stated[1]))
    return 0


def main():
    a = sys.argv[1:]
    if a and a[0] == '--save':
        return saved(a[1:4])
    floor_s, floor_mb = FLOOR_S, FLOOR_MB
    if '--floor-s' in a:
        floor_s = float(a[a.index('--floor-s') + 1])
    if '--floor-mb' in a:
        floor_mb = float(a[a.index('--floor-mb') + 1])
    out, cell, nref, nnew = a[0], a[1], int(a[2]), int(a[3])
    refs = ['ref%d' % i for i in range(1, nref + 1)]
    news = ['new%d' % i for i in range(1, nnew + 1)]
    p = lambda tag, ext: os.path.join(out, '%s.%s.%s' % (cell, tag, ext))

    # ---- PICTURE
    missing = [t for t in refs + news if not os.path.isfile(p(t, 'png'))]
    if missing or nref < 2:
        print('PICTURE FAIL  missing pictures: %s' % (', '.join(missing) or 'a second reference run'))
    else:
        base = picture(p(refs[0], 'png'))
        lit = int((base.max(axis=2) > 8).sum())
        bound_px, bound_step = 0, 0
        for t in refs[1:]:
            n, s = differ(base, picture(p(t, 'png')))
            bound_px, bound_step = max(bound_px, n), max(bound_step, s)
        allow_px, allow_step = MARGIN_PX_TIMES * bound_px + MARGIN_PX, MARGIN_STEP_TIMES * bound_step + MARGIN_STEP
        worst_px, worst_step = 0, 0
        for t in news:
            n, s = differ(base, picture(p(t, 'png')))
            worst_px, worst_step = max(worst_px, n), max(worst_step, s)
        ok = worst_px <= allow_px and worst_step <= allow_step and lit > base.shape[0] * base.shape[1] // 50
        print('PICTURE %s  new vs reference: %d pixels differ at most (largest step %d); the references differ '
              'from each other by %d (step %d); allowed %d pixels of step %d; %d of %d pixels drawn'
              % ('PASS' if ok else 'FAIL', worst_px, worst_step, bound_px, bound_step, allow_px, allow_step, lit,
                 base.shape[0] * base.shape[1]))

    # ---- COUNTS
    try:
        want = census(p(refs[0], 'notes'))
        bad = []
        for t in refs[1:] + news:
            got = census(p(t, 'notes'))
            if got != want:
                first = next((i for i in range(min(len(got), len(want))) if got[i] != want[i]), min(len(got), len(want)))
                bad.append('%s line %d: "%s" vs "%s"' % (t, first, (got[first] if first < len(got) else '(none)')[:90],
                                                       (want[first] if first < len(want) else '(none)')[:90]))
        ok = bool(want) and not bad
        print('COUNTS %s  %d census lines the same in %d runs%s'
              % ('PASS' if ok else 'FAIL', len(want), nref + nnew, ('; ' + bad[0]) if bad else ''))
    except OSError as e:
        print('COUNTS FAIL  %s' % e)

    # ---- GAIN
    rows = {}
    try:
        with open(os.path.join(out, 'results.tsv')) as f:
            for line in f:
                c = line.rstrip('\n').split('\t')
                if len(c) >= 6 and c[0].startswith(cell + '.'):
                    rows[c[0][len(cell) + 1:]] = c
    except OSError:
        pass
    if not refs or not news or any(t not in rows or rows[t][1] != '0' for t in refs + news):
        print('GAIN FAIL  a run is missing from results.tsv or did not exit 0')
        return 1
    ref_s = min(float(rows[t][2]) for t in refs)
    ref_mb = min(float(rows[t][5]) for t in refs)
    new_s = statistics.median(float(rows[t][2]) for t in news)
    new_mb = statistics.median(float(rows[t][5]) for t in news)
    cores_ref = statistics.median(float(rows[t][4]) for t in refs)
    cores_new = statistics.median(float(rows[t][4]) for t in news)
    gain_s, gain_mb = 1.0 - new_s / ref_s, 1.0 - new_mb / ref_mb
    ok = gain_s >= floor_s and gain_mb >= floor_mb
    print('GAIN %s  %.1f s -> %.1f s (%.0f%% less, floor %.0f%%); peak %.0f MB -> %.0f MB (%.0f%% less, floor %.0f%%); '
          'cores busy %.2f -> %.2f'
          % ('PASS' if ok else 'FAIL', ref_s, new_s, 100 * gain_s, 100 * floor_s, ref_mb, new_mb, 100 * gain_mb,
             100 * floor_mb, cores_ref, cores_new))
    return 0


if __name__ == '__main__':
    sys.exit(main())
