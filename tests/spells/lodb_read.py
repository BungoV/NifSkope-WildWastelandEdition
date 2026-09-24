#!/usr/bin/env python
# lodb_read.py -- THE ONE PYTHON READER FOR THE BAKE RECORD (.lodb)
#
# Lane BAKEREC1, 2026-09-17.
#
# WHY THIS FILE EXISTS
#   Before this lane four harnesses each carried their own parser for the
#   record, and every one of them parsed the v1 BINARY container by hand:
#
#     tests/spells/lodgen_defaults.sh      b[b.index(b'{'):]      -> json.loads
#     tests/spells/lodgen_layout.sh        json.loads(raw[16:])
#     tests/spells/lodgen_btofree_ledger.py
#     tests/spells/lodgen_native.sh        check 5
#
#   Four parsers is four places a format change has to be found, and the lane
#   that changed the format would have discovered the fourth one by watching a
#   harness fail on a Tuesday. There is one reader now and the harnesses import
#   it. A format change lands here and nowhere else.
#
# WHAT IT READS
#   The version 2 record: plain text, UTF-8, LF, `key<TAB>fields`. The full
#   format is docs/LODGEN_BAKE_RECORD.md; the writer is `lodgenWriteLedger` in
#   src/lodbfile.cpp and this reader is deliberately a SECOND implementation of
#   it -- an independent reader is the only thing that can catch a writer that
#   agrees with itself.
#
#   It also recognises the v1 binary container ('LODB' + a 16-byte header +
#   compact JSON) and refuses it BY NAME, exactly the way the C++ reader does,
#   rather than limping on with half the fields missing.
#
# WHAT IT RETURNS
#   A dict whose v1 keys are spelled exactly as the old JSON spelled them, so a
#   harness that used to do `json.loads(...)` keeps working after swapping one
#   line:
#
#     worldspace  int        the worldspace form id
#     worldEdid   str
#     dim         int
#     region      [x0,y0,x1,y1]
#     switches    str        sha1 hex over the argument vector
#     loadOrder   str        16 hex digits
#     chunks      [ {cx, cy, dim, inputs, out:["<rel> <sha1>"],
#                    outFiles:[...], outDigests:[...]} ]
#
#   plus the v2 block:
#
#     version     int        2
#     exe         str        the WW edition stamp the bake ran from
#     exeBytes    int
#     baked       str        ISO-8601 UTC -- THE ONE VOLATILE FIELD
#     target      'fo4cs'|'stock'
#     alg         {chunk,file,plugin,switches}
#     hashes      {loadOrderHash, pluginCorpusHash, objectCorpusHash,
#                  modelCorpusHash, cardCorpusHash}   (absent keys mean the
#                                                      bake wrote no pair)
#     plugins     [ {index, name, bytes, hash, hashHex, path} ]
#     resources   [ {kind, path, bytes, mtime} ]
#     switchTokens [str]     the argument vector, verbatim
#     census      [str]      every census line the bake printed
#     endFiles    int
#     endBytes    int
#     unknown     [str]      every line kind this reader did not know
#
# USAGE
#   as a module:   import lodb_read; rec = lodb_read.read(path)
#   from a shell:  python tests/spells/lodb_read.py <ws.lodb> [key]
#                  -- with no key it prints the whole record as JSON; with a
#                     key it prints that one field, which is what a `.sh`
#                     wants inside a `$( )`.
#
#   Two more helpers the harnesses need:
#     outputs(rec)     -> {relative path: sha1}  over every chunk
#     normalise(text)  -> the record's text with the volatile FIELDS masked to
#                         the literal <volatile>, matching the C++
#                         `lodbNormalise` line for line, so two bakes of one
#                         tree can be compared with `cmp`.

import json
import sys

V1_MAGIC = b'LODB'

#: The four volatile things, named here and in src/lodbfile.h and nowhere
#: else. They are MASKED, never dropped: a dropped line would also hide a
#: record that lost one.
VOLATILE_FIELDS = ('baked value', 'resource path/size/mtime', 'plugin path',
                   'the census stage-times line',
                   'the peak-working-set clause of the census bake-census line')


class LodbRefused(Exception):
    """The record cannot be read, with the reason the C++ reader would give."""


def read(path):
    with open(path, 'rb') as fh:
        raw = fh.read()
    return reads(raw, path)


def reads(raw, path='<bytes>'):
    if raw[:4] == V1_MAGIC:
        raise LodbRefused(
            '%s is a version 1 BINARY bake record and this build writes '
            'version 2 (plain text). Re-bake once without --incremental; '
            'every bake writes it' % path)

    rec = {
        'version': 0, 'worldspace': 0, 'worldEdid': '', 'dim': 0,
        'region': [0, 0, 0, 0], 'switches': '', 'loadOrder': '',
        'chunks': [], 'exe': '', 'exeBytes': 0, 'baked': '', 'target': '',
        'alg': {}, 'hashes': {}, 'plugins': [], 'resources': [],
        'switchTokens': [], 'census': [], 'endFiles': -1, 'endBytes': -1,
        'unknown': [], 'lineCount': 0, 'bytes': len(raw),
    }
    at = {}          # (cx,cy) -> index into rec['chunks']
    saw_version = False

    for line in raw.decode('utf-8').split(chr(10)):
        if line.endswith(chr(13)):
            # A CR is not written by us. It is tolerated here so that a record
            # mangled by an editor still reads, and the gate that counts CR
            # bytes stays the one thing that reports it.
            line = line[:-1]
        if not line:
            continue
        rec['lineCount'] += 1
        f = line.split(chr(9))
        k = f[0]
        if k == 'lodb':
            if len(f) < 3:
                raise LodbRefused('%s has a short version line' % path)
            rec['version'] = int(f[1])
            if rec['version'] != 2:
                raise LodbRefused(
                    '%s is bake record version %d and this reader knows 2'
                    % (path, rec['version']))
            saw_version = True
            rec['worldEdid'] = f[2]
            if len(f) > 3:
                rec['exe'] = f[3]
            if len(f) > 4:
                rec['exeBytes'] = int(f[4])
        elif k == 'baked' and len(f) > 1:
            rec['baked'] = f[1]
        elif k == 'alg':
            for pair in f[1:]:
                name, _, val = pair.partition('=')
                rec['alg'][name] = val
        elif k == 'shape' and len(f) > 4:
            rec['worldspace'] = int(f[1])
            rec['dim'] = int(f[2])
            rec['region'] = [int(v) for v in f[3].split(',')][:4]
            rec['target'] = f[4]
        elif k == 'hash' and len(f) > 2:
            rec['hashes'][f[1]] = f[2]
        elif k == 'loadorder' and len(f) > 1:
            rec['loadOrder'] = f[1]
        elif k == 'plugin' and len(f) > 4:
            rec['plugins'].append({
                'index': int(f[1]), 'name': f[2], 'bytes': int(f[3]),
                'hash': int(f[4], 16), 'hashHex': f[4],
                'path': f[5] if len(f) > 5 else '',
            })
        elif k == 'resource' and len(f) > 2:
            rec['resources'].append({
                'kind': f[1], 'path': f[2],
                'bytes': int(f[3]) if len(f) > 3 and f[3] else 0,
                'mtime': f[4] if len(f) > 4 else '',
            })
        elif k == 'switch' and len(f) > 1:
            rec['switchTokens'].append(f[1])
        elif k == 'switches' and len(f) > 1:
            rec['switches'] = f[1]
        elif k == 'chunk' and len(f) > 4:
            at[(f[1], f[2])] = len(rec['chunks'])
            rec['chunks'].append({
                'cx': int(f[1]), 'cy': int(f[2]), 'dim': int(f[3]),
                'inputs': f[4], 'out': [], 'outFiles': [], 'outDigests': [],
            })
        elif k == 'out' and len(f) > 4:
            i = at.get((f[1], f[2]))
            if i is None:
                rec['unknown'].append(line)
                continue
            ch = rec['chunks'][i]
            ch['outFiles'].append(f[3])
            ch['outDigests'].append(f[4])
            ch['out'].append('%s %s' % (f[3], f[4]))
        elif k == 'census' and len(f) > 1:
            rec['census'].append(f[1])
        elif k == 'end' and len(f) > 2:
            rec['endFiles'] = int(f[1])
            rec['endBytes'] = int(f[2])
        else:
            # An unknown kind is KEPT rather than dropped, so a harness can say
            # "this record has a line I do not understand" instead of quietly
            # measuring a record it only half read.
            rec['unknown'].append(line)

    if not saw_version:
        raise LodbRefused('%s has no lodb version line' % path)
    return rec


def outputs(rec, skip_manifest=False):
    """{relative path: sha1 hex} over every chunk of a record."""
    files = {}
    for ch in rec['chunks']:
        for name, dig in zip(ch['outFiles'], ch['outDigests']):
            if skip_manifest and 'manifest' in name:
                continue
            files[name] = dig
    return files


def normalise(text):
    """The record's text with exactly the volatile parts MASKED.

    A second implementation of `lodbNormalise` (src/lodbfile.cpp): the `baked`
    line's value, every `resource` line's path/size/mtime (the KIND and the
    ORDER stay, so a reordered stack still shows), the `plugin` line's last
    field (the absolute path), the `stage times:` census line and the
    `peak working set:` clause of the `bake census:` one become the literal
    `<volatile>` -- five things, the count in VOLATILE_FIELDS. Masked, never
    dropped, so the line count and the order survive and a record that lost a
    line still differs.

    Two bakes of one tree, normalised, must be byte-equal -- that is gate (h)
    in tests/spells/lodgen_bakerec.sh and the reason the volatile things sit on
    named lines at all.
    """
    TAB = chr(9)
    keep = []
    for line in text.split(chr(10)):
        if not line:
            continue
        f = line.split(TAB)
        if f[0] == 'baked':
            keep.append('baked' + TAB + '<volatile>')
        elif f[0] == 'resource':
            keep.append('resource' + TAB + (f[1] if len(f) > 1 else '')
                        + TAB + '<volatile>')
        elif f[0] == 'census' and len(f) > 1 and f[1].startswith('stage times:'):
            # THE FOURTH VOLATILE THING: `stage times:` is a wall clock, so it
            # is recorded and masked rather than dropped. See lodbNormalise in
            # src/lodbfile.cpp for why it is not simply left out of the record.
            keep.append('census' + TAB + 'stage times: <volatile>')
        elif f[0] == 'census' and 'peak working set: ' in line:
            # THE FIFTH VOLATILE THING (lane INCR1, 2026-09-17, handed over by
            # ARCHLOCK1): the chunk-pass census line carries this process's
            # PEAK WORKING SET, a measurement of the machine at that moment
            # rather than of the inputs. It is the only one that is INLINE --
            # the rest of that line (thread counts, chunk jobs, the bto
            # disposition, the layout root and its counts) is CONTENT -- so
            # exactly the clause is masked, from `peak working set: ` to the
            # next comma or the end of the line. Neither spelling
            # lodgenPeakWorkingSetLine() produces contains a comma.
            # Mirrors lodbNormalise() in src/lodbfile.cpp.
            at = line.index('peak working set: ') + len('peak working set: ')
            to = line.find(',', at)
            if to < 0:
                to = len(line)
            keep.append(line[:at] + '<volatile>' + line[to:])
        elif f[0] == 'plugin':
            keep.append(TAB.join(f[:5] + ['<volatile>']))
        else:
            keep.append(line)
    return chr(10).join(keep) + chr(10)


def normalise_file(path):
    with open(path, 'rb') as fh:
        return normalise(fh.read().decode('utf-8'))


def main(argv):
    if len(argv) < 2:
        sys.stderr.write('usage: lodb_read.py <ws.lodb> [key|--normalise]\n')
        return 2
    if len(argv) > 2 and argv[2] == '--normalise':
        # BYTES, not text. On Windows a text-mode stdout turns every LF into
        # CRLF, so a caller that hashes this pipe hashes a DIFFERENT file than
        # the one lodbNormalise() hashed -- same 77 lines, different sha1. That
        # is exactly how this cross-check first came up red (lane INCR1).
        sys.stdout.buffer.write(normalise_file(argv[1]).encode('utf-8'))
        return 0
    try:
        rec = read(argv[1])
    except LodbRefused as e:
        sys.stderr.write('REFUSED %s\n' % e)
        return 1
    if len(argv) > 2:
        v = rec.get(argv[2])
        if isinstance(v, (dict, list)):
            print(json.dumps(v, sort_keys=True))
        else:
            print(v)
        return 0
    print(json.dumps(rec, sort_keys=True, indent=1))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
