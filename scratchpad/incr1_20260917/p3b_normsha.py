"""INCR1 step 3b -- make the C++ normaliser REACHABLE so the two can be held
against each other, and fix two comments that count wrong.

`ww-volatile-field-law` step 3 says the mask is written twice because two
implementations of one rule is the only way a wrong mask is caught. In this tree
they were written twice and NEVER COMPARED: `lodbNormalise()` had no caller in
src/ at all (grep, 2026-09-17), and every gate used the Python half alone. Two
lines on the `--bake-record` read path fix that without dumping a file.

Anchors asserted count == 1. Escapes from chr(). LF preserved.
"""
import io
import os
import sys

NL = chr(10)
TAB = chr(9)
BS = chr(92)
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '..', '..'))
CHECK = '--check' in sys.argv[1:]


def load(rel):
    with io.open(os.path.join(ROOT, rel), 'rb') as fh:
        b = fh.read()
    assert b.count(chr(13).encode()) == 0, rel + ' is not LF-only'
    return b.decode('utf-8')


def save(rel, text):
    if CHECK:
        print('  would write %s (%d bytes)' % (rel, len(text.encode('utf-8'))))
        return
    with io.open(os.path.join(ROOT, rel), 'wb') as fh:
        fh.write(text.encode('utf-8'))
    print('  wrote %s (%d bytes)' % (rel, len(text.encode('utf-8'))))


def sub1(text, old, new, what):
    n = text.count(old)
    if n != 1:
        raise SystemExit('ANCHOR %s: found %d times, wanted exactly 1' % (what, n))
    return text.replace(old, new, 1)


rel = 'src/nifcli.cpp'
t = load(rel)
if 'normalisedSha1' in t:
    raise SystemExit(rel + ' already prints the normalised digest')

# ---- 1. the digest of the normalised record, on the --bake-record read path --
anchor = (TAB + TAB + '/* THE DIFF. `file` is the comma list this run was given -- positional,')
block = NL.join([
    TAB + TAB + '/* THE TWO NORMALISERS, HELD AGAINST EACH OTHER (lane INCR1,',
    TAB + TAB + ' * 2026-09-17). `ww-volatile-field-law` step 3 says the mask is',
    TAB + TAB + ' * written twice, in two languages, because two implementations of',
    TAB + TAB + ' * one rule is the only way a wrong mask is caught. In this tree it',
    TAB + TAB + ' * WAS written twice and never compared: `lodbNormalise()` had no',
    TAB + TAB + ' * caller anywhere in src/, and every gate used the Python half',
    TAB + TAB + ' * (`tests/spells/lodb_read.py`) alone, so a fifth volatile field',
    TAB + TAB + ' * masked in one half and not the other would have gone unnoticed.',
    TAB + TAB + ' *',
    TAB + TAB + ' * A DIGEST is enough: the gate normalises the same file with the',
    TAB + TAB + ' * Python reader, hashes it the same way and compares one hex string,',
    TAB + TAB + ' * with no 97-line dump on either side. Empty lines are dropped before',
    TAB + TAB + ' * masking and the join ends in one newline, which is exactly what',
    TAB + TAB + ' * `lodb_read.normalise()` does -- the two agree on the TEXT, not just',
    TAB + TAB + ' * on the rule. */',
    TAB + TAB + '{',
    TAB + TAB + TAB + 'QFile nf( gLgBakeRecord );',
    TAB + TAB + TAB + 'if ( nf.open( QIODevice::ReadOnly ) ) {',
    TAB + TAB + TAB + TAB + 'QStringList raw = QString::fromUtf8( nf.readAll() )',
    TAB + TAB + TAB + TAB + '                  .split( QChar( ' + chr(39) + BS + 'n' + chr(39) + ' ) );',
    TAB + TAB + TAB + TAB + 'raw.removeAll( QString() );',
    TAB + TAB + TAB + TAB + 'const QStringList norm = lodbNormalise( raw );',
    TAB + TAB + TAB + TAB + 'const QByteArray joined =',
    TAB + TAB + TAB + TAB + '    ( norm.join( QChar( ' + chr(39) + BS + 'n' + chr(39) + ' ) )',
    TAB + TAB + TAB + TAB + '      + QChar( ' + chr(39) + BS + 'n' + chr(39) + ' ) ).toUtf8();',
    TAB + TAB + TAB + TAB + 'out() << "bake-record normalisedLines " << norm.size() << Qt::endl;',
    TAB + TAB + TAB + TAB + 'out() << "bake-record normalisedSha1 "',
    TAB + TAB + TAB + TAB + '      << QString::fromLatin1( QCryptographicHash::hash( joined,',
    TAB + TAB + TAB + TAB + '            QCryptographicHash::Sha1 ).toHex() ) << Qt::endl;',
    TAB + TAB + TAB + '} else {',
    TAB + TAB + TAB + TAB + 'out() << "bake-record normalisedSha1 n/a (cannot reopen the record)"',
    TAB + TAB + TAB + TAB + '      << Qt::endl;',
    TAB + TAB + TAB + '}',
    TAB + TAB + '}',
    '',
]) + anchor
t = sub1(t, anchor, block, 'bake-record diff anchor')

# ---- 2. the two comments that count wrong ------------------------------------
t = sub1(t,
         TAB + TAB + ' * It is DETERMINISTIC except for the three things `src/lodbfile.h` names' + NL
         + TAB + TAB + ' * -- the `baked` line, a plugin\'s path field and the resource lines -- so' + NL
         + TAB + TAB + ' * two full bakes of the same tree write the same bytes everywhere else.',
         TAB + TAB + ' * It is DETERMINISTIC except for the FIVE things `src/lodbfile.h` names' + NL
         + TAB + TAB + ' * -- the `baked` line, a plugin\'s path field, the resource lines, the' + NL
         + TAB + TAB + ' * `stage times:` census line and the `peak working set:` clause of the' + NL
         + TAB + TAB + ' * `bake census:` one -- so two full bakes of the same tree write the' + NL
         + TAB + TAB + ' * same bytes everywhere else.',
         'three things comment')

t = sub1(t,
         TAB + TAB + TAB + ' * INFORMATIONAL -- nothing here is part of any hash -- and' + NL
         + TAB + TAB + TAB + ' * `lodbNormalise()` drops it, which is what makes "the mod folder was' + NL
         + TAB + TAB + TAB + ' * renamed" a no-op for the record. */',
         TAB + TAB + TAB + ' * INFORMATIONAL -- nothing here is part of any hash -- and' + NL
         + TAB + TAB + TAB + ' * `lodbNormalise()` MASKS it (the `kind` and the order stay, so a' + NL
         + TAB + TAB + TAB + ' * reordered stack still shows; masked, never dropped), which is what' + NL
         + TAB + TAB + TAB + ' * makes "the mod folder was renamed" a no-op for the record. */',
         'normalise drops comment')

save(rel, t)
print(NL + 'done')
