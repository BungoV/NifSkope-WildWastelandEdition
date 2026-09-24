"""INCR1 step 3c -- document the two new `--bake-record` lines (section 4) and
say in section 3 that the two normalisers are now held against each other."""
import io
import os
import sys

NL = chr(10)
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '..', '..'))
CHECK = '--check' in sys.argv[1:]
rel = 'docs/LODGEN_BAKE_RECORD.md'

with io.open(os.path.join(ROOT, rel), 'rb') as fh:
    b = fh.read()
assert b.count(chr(13).encode()) == 0
t = b.decode('utf-8')
if 'normalisedSha1' in t:
    raise SystemExit(rel + ' already documents it')


def sub1(text, old, new, what):
    n = text.count(old)
    if n != 1:
        raise SystemExit('ANCHOR %s: %d times, wanted 1' % (what, n))
    return text.replace(old, new, 1)


t = sub1(t,
         'bake-record endAgrees 0|1              ← the end line, re-counted off the disk',
         'bake-record endAgrees 0|1              ← the end line, re-counted off the disk' + NL
         + 'bake-record normalisedLines <n>        bake-record normalisedSha1 <sha1>',
         'section 4 listing')

t = sub1(t,
         '`lodbNormalise()` (`src/lodbfile.cpp`) and `lodb_read.normalise()`' + NL
         + '(`tests/spells/lodb_read.py`) replace each with the literal `<volatile>`. They',
         '`lodbNormalise()` (`src/lodbfile.cpp`) and `lodb_read.normalise()`' + NL
         + '(`tests/spells/lodb_read.py`) replace each with the literal `<volatile>`. They',
         'noop')

t = sub1(t,
         'end into the bto disposition or the layout counts that follow it.',
         'end into the bto disposition or the layout counts that follow it.' + NL
         + NL
         + '### The two normalisers are now held against each other' + NL
         + NL
         + 'The law says the mask is written twice, in two languages, because two' + NL
         + 'implementations of one rule is the only way a wrong mask is caught. Until' + NL
         + '2026-09-17 it was written twice and **never compared**: `lodbNormalise()` had' + NL
         + 'no caller anywhere in `src/`, and every gate used the Python half alone, so a' + NL
         + 'field masked in one half and not in the other would have gone unnoticed for as' + NL
         + 'long as nobody read both.' + NL
         + NL
         + '`--bake-record` now prints the C++ half\'s answer as a digest:' + NL
         + NL
         + '```' + NL
         + 'bake-record normalisedLines <n>' + NL
         + 'bake-record normalisedSha1  <sha1 of the normalised text, UTF-8, LF>' + NL
         + '```' + NL
         + NL
         + 'Empty lines are dropped before masking and the join ends in exactly one' + NL
         + 'newline, which is what `lodb_read.normalise()` does, so the two agree on the' + NL
         + 'TEXT and not merely on the rule. A gate normalises the same file with the' + NL
         + 'Python reader, hashes it the same way, and compares one hex string; they' + NL
         + 'disagreeing is a defect in whichever half was changed alone.',
         'section 3 tail')

if CHECK:
    print('  would write %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
else:
    with io.open(os.path.join(ROOT, rel), 'wb') as fh:
        fh.write(t.encode('utf-8'))
    print('  wrote %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
