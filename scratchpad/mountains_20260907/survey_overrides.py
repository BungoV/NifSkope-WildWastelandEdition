"""survey_overrides.py - EVIDENCE for "does an override plugin have to carry the
CELL record too, or only the LAND?"

Method: Bethesda's own DLC plugins are override plugins that edit Commonwealth
terrain. Whatever THEY do is the shape the engine is known to read, because
these ship in the retail game. So: for every plugin in Data, find LAND records
whose FormID belongs to Fallout4.esm (index 00 after master remapping - i.e.
the record is an OVERRIDE, not a new record), and report whether the CELL
that owns them is also present in that plugin, and whether that CELL is itself
an override or a new record.

Also dumps each plugin's TES4 flags/masters so the ESL question can be answered
from real ESL files rather than from recollection.

Read-only.
"""

import collections
import os
import struct
import sys

import fo4esm as E

DATA = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data'


def tes4_info(buf):
    sig, dsize, flags, formid, tail = E.read_record_header(buf, 0)
    assert sig == b'TES4'
    payload = E.record_payload(buf, 0, dsize, flags)
    masters = []
    hedr = None
    cnam = None
    for tag, content in E.subrecords(payload):
        if tag == b'HEDR':
            hedr = struct.unpack('<fII', content[:12])
        elif tag == b'MAST':
            masters.append(content.rstrip(b'\x00').decode('latin1'))
        elif tag == b'CNAM':
            cnam = content.rstrip(b'\x00').decode('latin1')
    return flags, hedr, masters, cnam, dsize, tail, payload


def scan(path):
    buf = E.load(path)
    flags, hedr, masters, cnam, tes4_dsize, tes4_tail, tes4_payload = tes4_info(buf)
    name = os.path.basename(path)
    print('--- %s  (%d bytes)' % (name, len(buf)))
    print('    TES4 flags=0x%08X  ESM=%s ESL=%s' % (
        flags, bool(flags & 0x00000001), bool(flags & 0x00000200)))
    print('    HEDR version=%r numRecords=%d nextObjectID=0x%X' % hedr)
    print('    masters=%r author=%r' % (masters, cnam))
    print('    TES4 header tail=%s  dataSize=%d' % (tes4_tail.hex(), tes4_dsize))

    # master index -> which file a formid's top byte refers to.
    # Overrides of Fallout4.esm content have top byte == index of Fallout4.esm.
    try:
        f4_index = masters.index('Fallout4.esm')
    except ValueError:
        f4_index = None

    lands = []
    cells_present = {}   # formid -> (is_override, xy)
    cell_stack = {'formid': None, 'xy': None}

    def on_rec(off, sig, dsize, rflags, formid, tail, stack):
        if sig == b'CELL':
            payload = E.record_payload(buf, off, dsize, rflags)
            xy = None
            for tag, content in E.subrecords(payload):
                if tag == b'XCLC' and len(content) >= 8:
                    xy = struct.unpack_from('<ii', content, 0)
            cell_stack['formid'] = formid
            cell_stack['xy'] = xy
            cells_present[formid] = ((formid >> 24) == f4_index, xy, len(payload))
        elif sig == b'LAND':
            lands.append((formid, cell_stack['formid'], cell_stack['xy'],
                          dsize, rflags, tuple(n.gtype for n in stack)))

    for node in E.top_level_groups(buf):
        if node.label == b'WRLD' and node.gtype == E.GT_TOP:
            E.walk(buf, node.offset + 24, node.offset + node.gsize, [node], on_rec)

    if not lands:
        print('    no LAND records')
        print()
        return

    ovr = [l for l in lands if f4_index is not None and (l[0] >> 24) == f4_index]
    print('    LAND records: %d total, %d are overrides of Fallout4.esm formids'
          % (len(lands), len(ovr)))
    paths = collections.Counter(l[5] for l in lands)
    for p, n in paths.most_common():
        print('    path: %s x%d' % (' -> '.join('%d:%s' % (g, E.GT_NAME.get(g, '?'))
                                                for g in p), n))
    # the key question
    with_cell = sum(1 for l in lands if l[1] in cells_present)
    print('    LANDs whose owning CELL record is ALSO present in this plugin: %d / %d'
          % (with_cell, len(lands)))
    if ovr:
        sample = ovr[:5]
        for formid, cformid, xy, dsize, rflags, path in sample:
            c = cells_present.get(cformid)
            print('      LAND %08X in CELL %08X %s  cell-present=%s cell-is-F4-override=%s'
                  % (formid, cformid or 0, xy, c is not None,
                     c[0] if c else None))
    print()


def main():
    names = sorted(n for n in os.listdir(DATA)
                   if n.lower().endswith(('.esm', '.esp', '.esl'))
                   and n != 'Fallout4.esm')
    for n in names:
        p = os.path.join(DATA, n)
        if os.path.getsize(p) < 200:
            continue
        try:
            scan(p)
        except Exception as exc:
            print('--- %s FAILED: %s: %s' % (n, type(exc).__name__, exc))
            print()


if __name__ == '__main__':
    main()
