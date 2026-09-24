"""INCR1 step 3 -- the FIFTH volatile field (the peak working set) and leg (f).

Every anchor is asserted `count == 1` before it is replaced (root MISTAKES.md:
the sed/heredoc trap, recorded four times). Every escape is built from chr().
Re-runnable: it refuses on a file that already carries the patch.

    python p3_volatile5.py            # patch
    python p3_volatile5.py --check    # report only
"""
import io
import os
import sys

NL = chr(10)
TAB = chr(9)
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '..', '..'))

CHECK = '--check' in sys.argv[1:]
changed = []


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
    changed.append(rel)
    print('  wrote %s (%d bytes)' % (rel, len(text.encode('utf-8'))))


def sub1(text, old, new, what):
    n = text.count(old)
    if n != 1:
        raise SystemExit('ANCHOR %s: found %d times, wanted exactly 1' % (what, n))
    return text.replace(old, new, 1)


# ---------------------------------------------------------------- lodbfile.cpp
rel = 'src/lodbfile.cpp'
t = load(rel)
if 'peak working set' in t:
    raise SystemExit(rel + ' already carries the fifth mask')

t = sub1(t,
         '/* ---- normalising away the four volatile things '
         '-------------------------- */',
         '/* ---- normalising away the five volatile things '
         '-------------------------- */',
         'cpp banner')

anchor = (TAB + TAB + 'if ( l.startsWith( QLatin1String( "plugin' + chr(92) + 't" ) ) ) {')
fifth = NL.join([
    TAB + TAB + 'if ( l.startsWith( QLatin1String( "census' + chr(92) + 't" ) )',
    TAB + TAB + '      && l.contains( QLatin1String( "peak working set: " ) ) ) {',
    TAB + TAB + TAB + '/* THE FIFTH VOLATILE THING, handed over by lane ARCHLOCK1 and',
    TAB + TAB + TAB + ' * masked by lane INCR1 (2026-09-17): the chunk-pass census line',
    TAB + TAB + TAB + ' * carries this process\'s PEAK WORKING SET',
    TAB + TAB + TAB + ' * (`lodgenPeakWorkingSetLine()`, src/lodgenparallel.cpp). That is a',
    TAB + TAB + TAB + ' * measurement of THIS MACHINE AT THIS MOMENT, not of the bake\'s',
    TAB + TAB + TAB + ' * inputs: two bakes of one tree differ in it by megabytes, and four',
    TAB + TAB + TAB + ' * gates were red on this one clause.',
    TAB + TAB + TAB + ' *',
    TAB + TAB + TAB + ' * It sits INLINE in a line whose other facts -- thread counts, the',
    TAB + TAB + TAB + ' * chunk-thread bound, chunk jobs and workers, the bto disposition,',
    TAB + TAB + TAB + ' * the layout root and its file counts -- are CONTENT and must keep',
    TAB + TAB + TAB + ' * comparing. (`ww-volatile-field-law` step 1 would have had it on a',
    TAB + TAB + TAB + ' * line of its own; moving it would move a census line bungo reads,',
    TAB + TAB + TAB + ' * so the clause is masked where it stands and the divergence is',
    TAB + TAB + TAB + ' * stated in docs/LODGEN_BAKE_RECORD.md section 3.)',
    TAB + TAB + TAB + ' *',
    TAB + TAB + TAB + ' * Exactly the one clause goes: from `peak working set: ` to the next',
    TAB + TAB + TAB + ' * comma, or to the end of the line when there is none. NEITHER',
    TAB + TAB + TAB + ' * spelling lodgenPeakWorkingSetLine() produces ("... GB (N bytes)" /',
    TAB + TAB + TAB + ' * "not available on this platform") contains a comma, so the clause',
    TAB + TAB + TAB + ' * cannot run past its own end and eat a fact that does not move. */',
    TAB + TAB + TAB + 'const int at = l.indexOf( QLatin1String( "peak working set: " ) )',
    TAB + TAB + TAB + '               + 18;   /* strlen( "peak working set: " ) */',
    TAB + TAB + TAB + 'int to = l.indexOf( QChar( \',\' ), at );',
    TAB + TAB + TAB + 'if ( to < 0 )',
    TAB + TAB + TAB + TAB + 'to = l.size();',
    TAB + TAB + TAB + 'out << l.left( at ) + QStringLiteral( "<volatile>" ) + l.mid( to );',
    TAB + TAB + TAB + 'continue;',
    TAB + TAB + '}',
    '',
]) + anchor
t = sub1(t, anchor, fifth, 'cpp plugin branch')
save(rel, t)

# ------------------------------------------------------------------ lodbfile.h
rel = 'src/lodbfile.h'
t = load(rel)
if 'peak working set' in t:
    raise SystemExit(rel + ' already names the fifth')

t = sub1(t,
         ' * holds for every line but four, and those four are named here and nowhere',
         ' * holds for every line but five, and those five are named here and nowhere',
         'h count')
t = sub1(t,
         ' *   `census<TAB>stage times: ...`  a WALL CLOCK -- two bakes of one tree' + NL
         + ' *                                 differ by tenths of a second. Found by' + NL
         + ' *                                 gate (h) measuring, not by thinking.' + NL,
         ' *   `census<TAB>stage times: ...`  a WALL CLOCK -- two bakes of one tree' + NL
         + ' *                                 differ by tenths of a second. Found by' + NL
         + ' *                                 gate (h) measuring, not by thinking.' + NL
         + ' *   the `peak working set: ...`   THIS MACHINE AT THIS MOMENT, not the' + NL
         + ' *   clause of the `census` line    inputs. The only one that is INLINE:' + NL
         + ' *   that begins `bake census:`     the rest of that line is content, so' + NL
         + ' *                                 exactly the clause up to the next comma' + NL
         + ' *                                 is masked. Handed over by ARCHLOCK1.' + NL,
         'h volatile list')
t = sub1(t,
         '/*! Strip the three lines/fields the record declares VOLATILE (see the block',
         '/*! Strip the FIVE lines/fields the record declares VOLATILE (see the block',
         'h normalise doc')
save(rel, t)

# ------------------------------------------------------------- lodb_read.py
rel = 'tests/spells/lodb_read.py'
t = load(rel)
if 'peak working set' in t:
    raise SystemExit(rel + ' already carries the fifth mask')

t = sub1(t,
         "VOLATILE_FIELDS = ('baked value', 'resource path/size/mtime', 'plugin path'," + NL
         + "                   'the census stage-times line')",
         "VOLATILE_FIELDS = ('baked value', 'resource path/size/mtime', 'plugin path'," + NL
         + "                   'the census stage-times line'," + NL
         + "                   'the peak-working-set clause of the census bake-census line')",
         'py VOLATILE_FIELDS')

anchor = ("        elif f[0] == 'plugin':" + NL
          + "            keep.append(TAB.join(f[:5] + ['<volatile>']))")
fifth = NL.join([
    "        elif f[0] == 'census' and 'peak working set: ' in line:",
    "            # THE FIFTH VOLATILE THING (lane INCR1, 2026-09-17, handed over by",
    "            # ARCHLOCK1): the chunk-pass census line carries this process's",
    "            # PEAK WORKING SET, a measurement of the machine at that moment",
    "            # rather than of the inputs. It is the only one that is INLINE --",
    "            # the rest of that line (thread counts, chunk jobs, the bto",
    "            # disposition, the layout root and its counts) is CONTENT -- so",
    "            # exactly the clause is masked, from `peak working set: ` to the",
    "            # next comma or the end of the line. Neither spelling",
    "            # lodgenPeakWorkingSetLine() produces contains a comma.",
    "            # Mirrors lodbNormalise() in src/lodbfile.cpp.",
    "            at = line.index('peak working set: ') + len('peak working set: ')",
    "            to = line.find(',', at)",
    "            if to < 0:",
    "                to = len(line)",
    "            keep.append(line[:at] + '<volatile>' + line[to:])",
]) + NL + anchor
t = sub1(t, anchor, fifth, 'py plugin branch')

t = sub1(t,
         "    line's value, every `resource` line's path/size/mtime (the KIND and the" + NL
         + "    ORDER stay, so a reordered stack still shows) and the `plugin` line's last" + NL
         + "    field (the absolute path) become the literal `<volatile>`. Masked, never",
         "    line's value, every `resource` line's path/size/mtime (the KIND and the" + NL
         + "    ORDER stay, so a reordered stack still shows), the `plugin` line's last" + NL
         + "    field (the absolute path), the `stage times:` census line and the" + NL
         + "    `peak working set:` clause of the `bake census:` one become the literal" + NL
         + "    `<volatile>` -- five things, the count in VOLATILE_FIELDS. Masked, never",
         'py normalise docstring')
save(rel, t)

# ------------------------------------------------------------ lodgen_layout.sh
rel = 'tests/spells/lodgen_layout.sh'
t = load(rel)
if 'the record itself excepted' in t:
    raise SystemExit(rel + ' already excepts the record')

t = sub1(t,
         'ON_DISK="$(find "$W/new/FO4CSLOD" -type f 2>/dev/null | wc -l)"' + NL
         + 'if [ "$CN" = "$ON_DISK" ]; then' + NL
         + TAB + 'note "(f) it counts what is on disk ($CN file(s))"' + NL
         + 'else' + NL
         + TAB + 'bad "(f) it counts what is on disk (census $CN, on disk $ON_DISK)"' + NL
         + 'fi' + NL,
         '# The census line is composed BEFORE the bake record is written, so the' + NL
         + '# record cannot be in the number it carries. That is the law the format' + NL
         + '# states for the same count on the `end` line (docs/LODGEN_BAKE_RECORD.md' + NL
         + '# section 2.8: "Counted by walking the record\'s own directory tree at write' + NL
         + '# time, the record itself excepted (it is not written yet)"), so the count on' + NL
         + '# THIS side of the comparison excepts it too -- and says how many it took' + NL
         + '# out, so the exception cannot quietly swallow a second file.' + NL
         + 'ON_DISK_ALL="$(find "$W/new/FO4CSLOD" -type f 2>/dev/null | wc -l)"' + NL
         + 'RECS="$(find "$W/new/FO4CSLOD" -type f -name \'*.lodb\' 2>/dev/null | wc -l)"' + NL
         + 'ON_DISK=$((ON_DISK_ALL - RECS))' + NL
         + 'if [ "$RECS" -gt 1 ]; then' + NL
         + TAB + 'bad "(f) exactly one bake record under the root (found $RECS)"' + NL
         + 'else' + NL
         + TAB + 'note "(f) $RECS bake record excepted from the disk count '
         + '(section 2.8: it is not written when the census line is composed)"' + NL
         + 'fi' + NL
         + 'if [ "$CN" = "$ON_DISK" ]; then' + NL
         + TAB + 'note "(f) it counts what is on disk ($CN file(s), '
         + '$ON_DISK_ALL on disk less $RECS record)"' + NL
         + 'else' + NL
         + TAB + 'bad "(f) it counts what is on disk (census $CN, '
         + 'on disk $ON_DISK_ALL less $RECS record = $ON_DISK)"' + NL
         + 'fi' + NL,
         'layout leg (f) count')
save(rel, t)

# ------------------------------------------------------ LODGEN_BAKE_RECORD.md
rel = 'docs/LODGEN_BAKE_RECORD.md'
t = load(rel)
if 'peak working set' in t:
    raise SystemExit(rel + ' already names the fifth')

t = sub1(t, '## 3. Determinism, and the four volatile things',
         '## 3. Determinism, and the five volatile things', 'doc heading')
t = sub1(t,
         'except for exactly four things, each isolated on a named line or a named field' + NL
         + 'so a comparison can mask precisely them and nothing else:',
         'except for exactly five things, four of them isolated on a named line or a' + NL
         + 'named field so a comparison can mask precisely them and nothing else -- and' + NL
         + 'the fifth a named CLAUSE inside a line that is otherwise content:',
         'doc lead')
t = sub1(t,
         '| the stage times | the `census` line beginning `stage times:` | wall clock '
         'again -- two bakes of one tree differed by 0.1 s |',
         '| the stage times | the `census` line beginning `stage times:` | wall clock '
         'again -- two bakes of one tree differed by 0.1 s |' + NL
         + '| the peak working set | the clause `peak working set: ...` up to the next '
         'comma, inside the `census` line beginning `bake census:` | this machine at '
         'this moment -- two bakes of one tree differed by megabytes |',
         'doc table row')
t = sub1(t,
         'bake took a minute, and dropping it would put the census completeness floor' + NL
         + '(section 2.7) permanently out of step with the census the bake printed.',
         'bake took a minute, and dropping it would put the census completeness floor' + NL
         + '(section 2.7) permanently out of step with the census the bake printed.' + NL
         + NL
         + 'The fifth was **carried over from another lane\'s red**: ARCHLOCK1 (2026-09-17)' + NL
         + 'found `lodgen_bakerec.sh` (e)(h), `lodgen_layout.sh`, `lodgen_native.sh`' + NL
         + 'section 5 and `lodgen_btofree.sh` red with identical counts on two different' + NL
         + 'exes, all of them on one file and one clause: `peak working set: 2.28 GB' + NL
         + '(2451947520 bytes)`, printed inside the chunk-pass census line by' + NL
         + '`lodgenPeakWorkingSetLine()` (`src/lodgenparallel.cpp`). It measures the' + NL
         + 'machine, not the inputs.' + NL
         + NL
         + '**A stated divergence from `ww-volatile-field-law` step 1**, which says a' + NL
         + 'volatile thing goes on a line of its own and never inline among facts that do' + NL
         + 'not move. This one stays inline: the chunk-pass census line is a line bungo' + NL
         + 'reads at the end of every bake, and splitting it would change a census wording' + NL
         + 'for the benefit of a comparison. Instead exactly the CLAUSE is masked, from' + NL
         + '`peak working set: ` to the next comma or the end of the line, and the mask is' + NL
         + 'bounded by a fact about the writer: neither spelling' + NL
         + '`lodgenPeakWorkingSetLine()` produces -- `N.NN GB (N bytes)` or `not available' + NL
         + 'on this platform` -- contains a comma, so the clause cannot run past its own' + NL
         + 'end into the bto disposition or the layout counts that follow it.',
         'doc fifth paragraph')
save(rel, t)

print(NL + 'patched %d file(s)' % len(changed))
