#!/usr/bin/env python3
"""Prove hookup.py's --apply WITHOUT touching src/.

This lane may not edit existing src/ files in place -- another lane owns the
build slots -- but a hook-up script nobody has ever seen produce output is not
evidence of anything.  So: run the same table through the same apply_to(),
write the results to COPIES under this scratchpad folder, and check the copies.

Checks:
  * every edit landed exactly once (apply_to asserts this itself)
  * the copy's CR count obeys the same rule the real run asserts
  * braces and parens balance over the whole copied file, and the count of
    each is the SAME as the original plus what the inserted text carries
  * the inserted C++ is printed so a person can read its indentation
"""
import io
import sys

import hookup as H


def balance(text):
    """-> (brace delta, paren delta) ignoring nothing -- a crude but honest
    counter.  It is only meaningful as a DIFFERENCE between the original and
    the edited copy, which is why that is what the caller compares."""
    return text.count('{') - text.count('}'), text.count('(') - text.count(')')


def main():
    plan = H.build()
    byFile = {}
    for path, emode, prefix, textTmpl, real, n, ending in plan:
        if n != 1:
            print('REFUSED before starting: %s %r matches %d' % (path, prefix, n))
            return 1
        byFile.setdefault(path, []).append((emode, real, textTmpl, ending))

    for path in sorted(byFile):
        orig = io.open(H.ROOT + path, 'rb').read().decode('utf-8')
        text = orig
        added = 0
        ending = H.LF
        bodies = []
        for emode, real, textTmpl, ending in byFile[path]:
            body = textTmpl.replace('{A}', real)
            bodies.append(body)
            text = H.apply_to(text, emode, real, body, ending)
            added += body.count(H.LF) + (0 if emode == 'replace' else 1)

        cr0, cr1 = orig.count(H.CR), text.count(H.CR)
        want = cr0 + (added if ending == '\r\n' else 0)
        b0, p0 = balance(orig)
        b1, p1 = balance(text)
        # what the INSERTED text itself carries, minus what the replaced
        # anchors took away
        db = sum(b.count('{') - b.count('}') for b in bodies)
        dp = sum(b.count('(') - b.count(')') for b in bodies)
        for emode, real, _t, _e in byFile[path]:
            if emode == 'replace':
                db -= real.count('{') - real.count('}')
                dp -= real.count('(') - real.count(')')

        okCR = (cr1 == want)
        okB = (b1 - b0 == db)
        okP = (p1 - p0 == dp)
        print('%-18s lines %d -> %d (+%d)  CR %d -> %d %s  brace delta %+d '
              'expected %+d %s  paren delta %+d expected %+d %s'
              % (path, orig.count(H.LF) + 1, text.count(H.LF) + 1, added,
                 cr0, cr1, 'OK' if okCR else 'BAD',
                 b1 - b0, db, 'OK' if okB else 'BAD',
                 p1 - p0, dp, 'OK' if okP else 'BAD'))
        if not (okCR and okB and okP):
            return 1
        # the file's OWN balance, which for a whole C++ file must be zero
        if path.endswith(('.cpp', '.h')):
            print('%-18s whole-file braces %+d parens %+d  (was %+d / %+d)'
                  % ('', b1, p1, b0, p0))
        out = 'copy_' + path.replace('/', '_')
        with io.open(out, 'wb') as fh:
            fh.write(text.encode('utf-8'))
    print('copies written beside this script; src/ was not touched')
    return 0


if __name__ == '__main__':
    sys.exit(main())
