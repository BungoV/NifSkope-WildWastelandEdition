"""AUDIT1 step 7: splice the top summary, section 6 and section 7 into the report.

Every placeholder is filled from a file this lane MEASURED, not from text typed
here, so a number in the report cannot differ from the number on disk. The
script refuses if any placeholder survives, and refuses if the report already
carries a section 6 -- running it twice must not produce two.
"""
import io
import os
import sys

S = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916'
REPORT = os.path.join(S, 'lane_audit1_report.md')
NL = chr(10)

ORDER = ['sec6_head.md', 'sec6_63_head.md', 'sec6_64.md', 'sec6_65_head.md',
         'sec6_66_head.md', 'sec6_66_body.md', 'sec6_notfixed.md',
         'sec6_68_head.md']

FILL = {
    'STEP6-VERBATIM': 'fill_step6_verbatim.md',
    'BOARD-TABLE-HERE': 'fill_board_table.md',
    'INVARIANTS-HERE': 'fill_invariants.md',
    'PICTURES-TABLE': 'fill_pictures.md',
    'BOARD-LINE': 'fill_board_line.md',
    'WALL-LINE': 'fill_wall_line.md',
    'PLACEHOLDER-BOARD': 'fill_changelog_board.md',
}


def rd(p):
    return io.open(p, encoding='utf-8', newline='').read()


def main():
    rep = rd(REPORT)
    if rep.count(chr(13)):
        print('ABORT: the report carries CR')
        return 1
    if NL + '## 6.' in rep:
        print('ABORT: the report already has a section 6')
        return 1

    need = list(ORDER) + ['summary_draft.md', 'sec7_head.md',
                          'changelog_text.md', 'sec7_mistakes.md']
    for name in need + list(FILL.values()):
        if not os.path.exists(os.path.join(S, name)):
            print('ABORT: missing %s' % name)
            return 1

    sec6 = NL.join(rd(os.path.join(S, n)) for n in ORDER)
    summary = rd(os.path.join(S, 'summary_draft.md'))
    changelog = rd(os.path.join(S, 'changelog_text.md')).strip(NL)
    sec7 = (rd(os.path.join(S, 'sec7_head.md')).strip(NL) + NL * 2
            + '```markdown' + NL + changelog + NL + '```' + NL
            + rd(os.path.join(S, 'sec7_mistakes.md')).rstrip(NL) + NL)

    for key, fn in FILL.items():
        v = rd(os.path.join(S, fn)).strip(NL)
        sec6 = sec6.replace(key, v)
        summary = summary.replace(key, v)
        sec7 = sec7.replace(key, v)

    for key in FILL:
        if key in sec6 or key in summary or key in sec7:
            print('ABORT: %s survived the substitution' % key)
            return 1

    lines = rep.split(NL)
    at = None
    for i, l in enumerate(lines):
        if l.startswith('## 0.'):
            at = i
            break
    if at is None:
        print('ABORT: no section 0 to sit above')
        return 1
    nsum = len([x for x in summary.strip(NL).split(NL) if x.strip()])
    if nsum > 20:
        print('ABORT: the summary is %d non-blank lines, the cap is 20' % nsum)
        return 1

    out = (NL.join(lines[:at]) + summary.strip(NL) + NL * 2
           + NL.join(lines[at:]))
    if not out.endswith(NL):
        out = out + NL
    out = out + NL + sec6.strip(NL) + NL * 2 + sec7.strip(NL) + NL

    if out.count(chr(13)):
        print('ABORT: CR appeared')
        return 1
    tmp = REPORT + '.tmp'
    f = io.open(tmp, 'w', encoding='utf-8', newline='')
    f.write(out)
    f.close()
    os.replace(tmp, REPORT)
    print('report: %d -> %d bytes, %d lines, CR 0; summary %d lines, '
          'section 6 %d bytes, section 7 %d bytes'
          % (len(rep), len(out), out.count(NL), nsum, len(sec6), len(sec7)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
