"""fo4esm.py - read-only structural reader for a Fallout 4 .esm/.esp.

Lane ESPWRITE. This is the READER half; make_landfix_esp.py is the writer.

Nothing here writes. The master at
X:\\Programs\\Steam\\steamapps\\common\\Fallout 4\\Data\\Fallout4.esm is
opened 'rb' only, and every function takes an immutable bytes buffer.

Layout facts used here and where each came from:

  Record header, 24 bytes:
      type[4] dataSize:u32 flags:u32 formID:u32 timestamp:u16 vcs:u16
      version:u16 unknown:u16
    - esp_lib.read_record_header + read_record_header_tail (the tail being
      8 bytes, not 4, is called out there as a mistake that once silently
      corrupted a file).
  GRUP header, 24 bytes:
      'GRUP' groupSize:u32 (INCLUDES the 24-byte header) label[4]
      groupType:i32 timestamp:u16 vcs:u16 version:u16 unknown:u16
    - esp_lib.read_group_header, and laneb/esmland.py which already walks
      Fallout4.esm's WRLD tree with this and asserts sizes.
  Subrecord:
      type[4] size:u16 payload[size]
    - esp_lib.read_subrecords. NOTE esp_lib does NOT handle the XXXX
      oversize escape; this module does (see subrecords()). Reported as a
      defect in report_espwrite.md.
  Compressed record: flags & 0x00040000 -> payload is
      u32 uncompressedSize + raw zlib stream.
    - esp_lib.decompress_record_payload flags this wrapper as UNVALIDATED
      because that project had no compressed records to test on. This lane
      does: Fallout4.esm's exterior CELLs are compressed, and every one of
      them round-trips, which validates the wrapper. See report.
"""

import struct
import zlib

COMPRESSED_FLAG = 0x00040000

# Group type enum. Values 0..10 are the Bethesda GRUP types; the names are
# the conventional ones. Which of these FO4 actually uses under WRLD is
# MEASURED by group_census() rather than assumed.
GT_TOP = 0
GT_WORLD_CHILDREN = 1
GT_INTERIOR_BLOCK = 2
GT_INTERIOR_SUBBLOCK = 3
GT_EXTERIOR_BLOCK = 4
GT_EXTERIOR_SUBBLOCK = 5
GT_CELL_CHILDREN = 6
GT_TOPIC_CHILDREN = 7
GT_CELL_PERSISTENT = 8
GT_CELL_TEMPORARY = 9
GT_CELL_VISIBLE_DISTANT = 10

GT_NAME = {
    0: 'Top', 1: 'WorldChildren', 2: 'InteriorBlock', 3: 'InteriorSubBlock',
    4: 'ExteriorBlock', 5: 'ExteriorSubBlock', 6: 'CellChildren',
    7: 'TopicChildren', 8: 'CellPersistent', 9: 'CellTemporary',
    10: 'CellVisibleDistant',
}

COMMONWEALTH = 0x0000003C


# ---------------------------------------------------------------- headers

def read_record_header(buf, off):
    """(sig:bytes4, dataSize, flags, formid, tail:bytes8) for the record at off.

    Same field order as esp_lib.read_record_header, but returns the 8-byte
    tail alongside instead of needing a second call, and keeps sig as bytes
    so a comparison against b'LAND' cannot be defeated by a decode error.
    """
    sig = buf[off:off + 4]
    data_size, flags, formid = struct.unpack_from('<IiI', buf, off + 4)
    return sig, data_size, flags & 0xFFFFFFFF, formid, bytes(buf[off + 16:off + 24])


def read_group_header(buf, off):
    """(sig, groupSize, label:bytes4, groupType, tail:bytes8).

    groupSize INCLUDES the 24-byte header, per esp_lib's own note.
    """
    sig = buf[off:off + 4]
    gsize, label, gtype = struct.unpack_from('<I4si', buf, off + 4)
    return sig, gsize, label, gtype, bytes(buf[off + 16:off + 24])


def subrecords(payload):
    """Yield (tag:bytes4, content:bytes) over a decompressed record payload.

    Handles the XXXX escape: a subrecord tagged XXXX whose 4-byte content is
    a u32 giving the real length of the FOLLOWING subrecord, whose own u16
    length field is then meaningless (written as 0). esp_lib.read_subrecords
    lacks this and would desynchronise on any record carrying one.
    """
    i = 0
    n = len(payload)
    override = None
    while i + 6 <= n:
        tag = payload[i:i + 4]
        size = struct.unpack_from('<H', payload, i + 4)[0]
        i += 6
        if tag == b'XXXX':
            override = struct.unpack_from('<I', payload, i)[0]
            i += size
            continue
        if override is not None:
            size = override
            override = None
        yield tag, bytes(payload[i:i + size])
        i += size


def decompress_payload(raw):
    """raw = the on-disk dataSize bytes of a record with COMPRESSED_FLAG set.

    Wrapper is [u32 uncompressed size][zlib stream]. The assert makes a wrong
    wrapper assumption loud instead of silent - esp_lib's version has the
    same assert and the same reasoning.
    """
    declared = struct.unpack_from('<I', raw, 0)[0]
    out = zlib.decompress(raw[4:])
    assert len(out) == declared, (
        'compressed wrapper mismatch: got %d, header declared %d' % (len(out), declared))
    return out


def record_payload(buf, off, data_size, flags):
    """The record's LOGICAL payload: decompressed if the flag is set."""
    raw = bytes(buf[off + 24:off + 24 + data_size])
    if flags & COMPRESSED_FLAG:
        return decompress_payload(raw)
    return raw


# ------------------------------------------------------------------ walk

class Node(object):
    """One entry in the group stack: how we got to a record."""
    __slots__ = ('offset', 'gsize', 'label', 'gtype', 'tail')

    def __init__(self, offset, gsize, label, gtype, tail):
        self.offset = offset
        self.gsize = gsize
        self.label = label
        self.gtype = gtype
        self.tail = tail

    def __repr__(self):
        return 'GRUP(type=%d %s label=%s size=%d @0x%X)' % (
            self.gtype, GT_NAME.get(self.gtype, '?'), self.label.hex(), self.gsize,
            self.offset)


def walk(buf, start, end, stack, on_record, on_group=None):
    """Depth-first walk of [start,end). Calls on_record(off, sig, dsize, flags,
    formid, tail, stack) for every record and on_group(node, stack) for every
    GRUP, with `stack` the list of enclosing Nodes (outermost first).

    Asserts that each group's children consume EXACTLY its declared size, so a
    layout mistake is loud. That assert is exercised deliberately in
    test_roundtrip.py.
    """
    off = start
    while off + 24 <= end:
        if buf[off:off + 4] == b'GRUP':
            sig, gsize, label, gtype, tail = read_group_header(buf, off)
            assert gsize >= 24, 'GRUP at 0x%X declares size %d' % (off, gsize)
            assert off + gsize <= end, (
                'GRUP at 0x%X size %d overruns parent end 0x%X' % (off, gsize, end))
            node = Node(off, gsize, label, gtype, tail)
            if on_group is not None:
                on_group(node, stack)
            stack.append(node)
            walk(buf, off + 24, off + gsize, stack, on_record, on_group)
            stack.pop()
            off += gsize
            continue
        sig, dsize, flags, formid, tail = read_record_header(buf, off)
        on_record(off, sig, dsize, flags, formid, tail, stack)
        off += 24 + dsize
    assert off == end, 'group children ended at 0x%X, expected 0x%X' % (off, end)


def top_level_groups(buf):
    """Yield Nodes for the file's top-level GRUPs (after the TES4 record)."""
    tes4_size = struct.unpack_from('<I', buf, 4)[0]
    off = 24 + tes4_size
    n = len(buf)
    while off + 24 <= n:
        sig, gsize, label, gtype, tail = read_group_header(buf, off)
        assert sig == b'GRUP', 'expected GRUP at 0x%X, got %r' % (off, sig)
        yield Node(off, gsize, label, gtype, tail)
        off += gsize
    assert off == n, 'top level ended at 0x%X, file is 0x%X' % (off, n)


def load(path):
    with open(path, 'rb') as f:
        return f.read()
