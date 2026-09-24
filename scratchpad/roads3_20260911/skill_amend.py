"""Append lane ROADS3's paragraph to ww-control-calibration, and mirror both
skills into the parallel tree E:\\Projects\\NifskopeWWE_ui.

Written as a file, not a heredoc: prose apostrophes.
"""
import io
import os
import shutil

A = r'E:\Projects\NifskopeWildWastelandEdition'
B = r'E:\Projects\NifskopeWWE_ui'
REL = os.path.join('.claude', 'skills')

PARA = """

## A floor that returns NaN is not a weak floor, it is NO floor (lane ROADS3, 2026-09-12)

Lane ROADS3 asked whether vanilla keeps any of the road diffuse's own detail.
The measurement was a correlation between the residual after the best wash and
our full-detail bake's departure from its flat average, and the floor was the
same signal TRANSLATED by a large random shift.

The signal is mask-shaped: it is zero everywhere off the road. Translate it and
it is zero everywhere ON the road, so it has no variance there and
`np.corrcoef` returns **NaN** for every draw. Printed into a table beside the
real number, NaN reads like a floor that was beaten. It is a floor that never
ran.

**Check every floor for NaN and for zero variance before you believe it**, and
print the floor's own standard deviation next to its value. When the signal is
confined to a mask, the honest floor keeps its amplitude and breaks only its
phase -- `splatlib.phase_twin(sig, seed=...)`, several seeds, report mean and
max. ROADS3's real numbers then came out as +0.0275 against a phase-twin floor
of 0.0270 mean / 0.0644 max, i.e. the correlation IS the floor, which is a
result. The NaN version of the same table was an empty claim.
"""


def main():
    p = os.path.join(A, REL, 'ww-control-calibration', 'SKILL.md')
    s = io.open(p, encoding='utf-8', newline='').read()
    if 'lane ROADS3, 2026-09-12' in s:
        print('calibration skill already amended')
    else:
        io.open(p, 'w', encoding='utf-8', newline='').write(s.rstrip('\n') + PARA)
        print('amended', p)
    if not os.path.isdir(B):
        print('MIRROR SKIPPED: %s is not on disk' % B)
        return
    # The parallel tree has lane UINOTES1 live in it, so nothing there is
    # deleted or overwritten wholesale: the new skill is copied only if it is
    # absent, and the amended one gets the same paragraph appended if it does
    # not already carry it.
    src = os.path.join(A, REL, 'ww-simulate-before-build')
    dst = os.path.join(B, REL, 'ww-simulate-before-build')
    if os.path.isdir(dst):
        print('mirror already has ww-simulate-before-build, left alone')
    else:
        shutil.copytree(src, dst)
        print('mirrored ww-simulate-before-build ->', dst)
    q = os.path.join(B, REL, 'ww-control-calibration', 'SKILL.md')
    if not os.path.isfile(q):
        print('MIRROR SKIPPED: no ww-control-calibration in the parallel tree')
        return
    t = io.open(q, encoding='utf-8', newline='').read()
    if 'lane ROADS3, 2026-09-12' in t:
        print('mirror calibration skill already amended')
    else:
        io.open(q, 'w', encoding='utf-8', newline='').write(t.rstrip('\n') + PARA)
        print('amended', q)


main()
