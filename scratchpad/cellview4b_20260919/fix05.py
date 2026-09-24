"""Lane CELLVIEW4B, fix 05 -- tests/spells/cell_open.sh went RED on this lane's
own marker repair, and the row it failed can be made STRONGER instead of just
made to agree.

WHAT WENT RED.  `cell_open_check.py:is_marker_model()` says in its own docstring
that it mirrors `src/cellview.cpp` "element for element".  The repair widened the
C++ rule with `m.startsWith("marker")`, because a marker sitting at the meshes
ROOT has no backslash in front of it and every earlier test needed one --
`markerxheading.nif` (REFR 00066245, the black arrow the director saw),
`markercocheading.nif`, and the whole `markers\\...` subtree were being drawn as
ordinary statics.  The Python copy was not widened, so the gate called three
now-hidden markers "references dropped although their model is present".  The
viewer is right and the gate's copy of the rule was stale.

WHY THE ROW IS REPLACED RATHER THAN PATCHED.  Mirroring a rule by hand is the
weak form -- two sources that LOOK alike, checked by eye, exactly the kind of
"cause stated without a measurement" this project forbids.  The run supplies a
real accounting identity instead:

    downtown 5,-11   plugin drawable 1438, scene 1426, missing 12
                     census: hidden: disabled 0, markers 12, deleted 0, no base 0

Every reference the plugin puts in the block and the scene does not draw must be
accounted for by a category the viewer NAMED in its own census.  That is a
closed sum, it fails when a reference goes missing for any unnamed reason, and
it cannot be satisfied by widening a marker rule -- widening the rule moves a
reference from one named category to another and the sum is unchanged.  The old
row's excuse ("9 more were dropped with no loose model, which is expected") is
exactly the kind of open-ended excuse that hides a real drop, and it goes.

The per-reference explanation is kept and the marker rule still corrected,
because the MISSING lines are what a reader uses to find out which reference.
"""
import io
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
CHECK = '--check' in sys.argv


def edit(rel, anchor, new):
    path = os.path.join(ROOT, rel)
    with io.open(path, 'rb') as fh:
        raw = fh.read()
    cr_before = raw.count(b'\r')
    txt = raw.decode('utf-8')
    n = txt.count(anchor)
    print('%-34s %d  %r' % (rel, n, anchor[:50]))
    assert n == 1, 'anchor matched %d times, not once' % n
    out = txt.replace(anchor, new)
    if CHECK:
        return
    data = out.encode('utf-8')
    assert data.count(b'\r') == cr_before, 'CR count moved'
    with io.open(path, 'wb') as fh:
        fh.write(data)


# ---------------------------------------------------------------- the rule
edit('tests/spells/cell_open_check.py',
'''    """src/cellview.cpp:125-132, element for element.

    Markers are hidden by default (the brief), so the plugin listing one is not
    evidence that the scene dropped it.
    """
    m = model.lower().replace('/', '\\\\')
    return ('\\\\marker' in m or 'marker_' in m
            or m.endswith('markerx.nif') or '\\\\editor\\\\' in m)''',
'''    """src/cellview.cpp isMarkerModel(), element for element.

    Markers are hidden by default (the brief), so the plugin listing one is not
    evidence that the scene dropped it.

    The `startswith('marker')` clause is the one lane CELLVIEW4B added on both
    sides: a marker at the meshes ROOT has no backslash before its name, so
    every other test here needs one and `markerxheading.nif`,
    `markercocheading.nif` and the `markers\\\\...` subtree were classed as
    ordinary statics. Do not trust this comment that the two rules still agree
    -- the accounting row below measures it, and that row is why a stale copy
    of this function can no longer pass quietly.
    """
    m = model.lower().replace('/', '\\\\')
    return ('\\\\marker' in m or 'marker_' in m
            or m.endswith('markerx.nif') or m.startswith('marker')
            or '\\\\editor\\\\' in m)''')

# ------------------------------------------------------------- the new row
edit('tests/spells/cell_open_check.py',
'''    if unexplained:
        problems.append('%d references dropped although their model is present '
                        '(%d more were dropped with no loose model, which is '
                        'expected)'
                        % (len(unexplained), len(missing) - len(unexplained)))
    elif missing:
        print('  %d references absent, every one with no loose model (BA2-only)'
              ' -- not counted against the scene' % len(missing))''',
'''    if unexplained:
        problems.append('%d references dropped although their model is present '
                        '(%d more were dropped with no loose model, which is '
                        'expected)'
                        % (len(unexplained), len(missing) - len(unexplained)))
    elif missing:
        print('  %d references absent, every one with no loose model (BA2-only)'
              ' -- not counted against the scene' % len(missing))

    # THE ACCOUNTING IDENTITY (lane CELLVIEW4B). The row above explains one
    # reference at a time and its last excuse -- "no loose model" -- is open
    # ended: a reference that vanished for a reason nobody has thought of also
    # has no loose model some of the time. The viewer publishes a census of
    # every reference it deliberately did not draw, by NAMED category:
    #
    #   hidden: disabled 0, markers 12, deleted 0, no base 0
    #
    # so the honest question is whether those named categories account for the
    # whole gap, exactly. They did on 2026-09-19 for downtown 5,-11: plugin
    # 1438 drawable, scene 1426, gap 12, census 12. A reference dropped for an
    # unnamed reason moves the two apart and this fails; widening or narrowing
    # the marker rule moves a reference between two NAMED categories and leaves
    # the sum alone, so this row cannot be satisfied by editing the rule.
    if getattr(a, 'notes', None) and os.path.isfile(a.notes):
        with open(a.notes, 'r', errors='replace') as fh:
            hid = re.search(r'hidden: disabled ([0-9]+), markers ([0-9]+), '
                            r'deleted ([0-9]+), no base ([0-9]+)', fh.read())
        if not hid:
            problems.append('the scene wrote no "hidden:" census line, so the '
                            'references it did not draw are unaccounted for')
        else:
            named = sum(int(g) for g in hid.groups())
            print('  accounting: %d references the plugin draws are not in the '
                  'scene; the census names %d (disabled %s, markers %s, '
                  'deleted %s, no base %s)'
                  % ((len(missing), named) + tuple(hid.groups())))
            if named != len(missing):
                problems.append('%d references are missing from the scene but '
                                'the census names only %d -- %d dropped for a '
                                'reason the viewer does not state'
                                % (len(missing), named, len(missing) - named))''')

# -------------------------------------------------------------- the plumbing
edit('tests/spells/cell_open_check.py',
'''import os
import struct''',
'''import os
import re
import struct''')

edit('tests/spells/cell_open_check.py',
'''    ap.add_argument('--dump', required=True)''',
'''    ap.add_argument('--dump', required=True)
    # the viewer's own census, for the accounting row (lane CELLVIEW4B)
    ap.add_argument('--notes')''')

edit('tests/spells/cell_open.sh',
'''			--cell "$x" "$y" --n "$n" --dump "$dump" ${RED:+--red} >> "$LOG" 2>&1''',
'''			--cell "$x" "$y" --n "$n" --dump "$dump" --notes "$notes" \\
			${RED:+--red} >> "$LOG" 2>&1''')

print()
print('CHECK ONLY, nothing written' if CHECK else 'APPLIED')
