#!/usr/bin/env python
# lodb_read.py -- the one Python reader for the bake record (.lodb); harnesses import it.
# A second, independent reader of the v2 text format (docs/LODGEN_BAKE_RECORD.md); v1 binary is refused by name.
# Usage: lodb_read.read(path), outputs(rec), normalise(text); CLI: lodb_read.py <ws.lodb> [key]

import json
import sys

V1_MAGIC = b'LODB'

#: The volatile fields, named here and in src/lodbfile.h; masked, never dropped, so a lost line still shows.
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
        'products': [], 'unknown': [], 'lineCount': 0, 'bytes': len(raw),
    }
    at = {}          # (cx,cy) -> index into rec['chunks']
    saw_version = False

    for line in raw.decode('utf-8').split(chr(10)):
        if line.endswith(chr(13)):
            # A CR is not written by us; tolerated so an editor-mangled record still reads.
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
        elif k == 'product' and len(f) > 2:
            # a region product -- a file under the record folder no chunk row claims -- with its sha1
            rec['products'].append({'path': f[1], 'sha1': f[2]})
        elif k == 'census' and len(f) > 1:
            rec['census'].append(f[1])
        elif k == 'end' and len(f) > 2:
            rec['endFiles'] = int(f[1])
            rec['endBytes'] = int(f[2])
        else:
            # An unknown kind is kept so a harness can report a line it does not understand.
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
            # `stage times:` is a wall clock: recorded and masked, not dropped (see lodbNormalise).
            keep.append('census' + TAB + 'stage times: <volatile>')
        elif f[0] == 'census' and 'peak working set: ' in line:
            # The peak working set is the one inline volatile clause: mask from `peak working set: ` to the next comma or EOL.
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
        # Bytes, not text: a Windows text-mode stdout turns LF into CRLF and changes the sha1.
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
