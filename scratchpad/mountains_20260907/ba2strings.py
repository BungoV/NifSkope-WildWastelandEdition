"""ba2strings.py - pull Fallout4_en.STRINGS out of "Fallout4 - Interface.ba2"
and resolve a string ID.

Needed for exactly one thing: the Commonwealth WRLD record's FULL subrecord is
a u32 string ID (0x0003613F) because Fallout4.esm is flagged Localized. A
plugin that copies that record but is NOT flagged Localized must carry the
name INLINE as a zstring instead - which is what the Creation Kit itself does
when it saves a non-localized .esp. So the real string has to be read, not
guessed.

There is no loose Data\\Strings\\Fallout4_en.STRINGS on this machine (checked),
so it comes from the archive.

BA2 general-archive layout (the 'GNRL' flavour):
    header  : 'BTDX' u32 version, char[4] type, u32 fileCount,
              u64 nameTableOffset
    per file: u32 nameHash, char[4] ext, u32 dirHash, u32 flags,
              u64 dataOffset, u32 packedSize, u32 unpackedSize, u32 sentinel
              (packedSize 0 means stored, otherwise zlib)
    names   : at nameTableOffset, fileCount x (u16 length + bytes)

The reader asserts the 'BTDX'/'GNRL' magic, that the sentinel is 0xBAADF00D on
every record, and that each extracted file matches its declared unpackedSize,
so a wrong layout guess is loud rather than silent.

Read-only.
"""

import struct
import sys
import zlib

BA2 = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4 - Interface.ba2'
SENTINEL = 0xBAADF00D


def read_ba2_general(path):
    """-> dict {name.lower(): bytes}. Only unpacks what is asked for lazily?
    No - the Interface archive is small enough to take whole, and taking it
    whole means the sentinel/size asserts run on every entry, which is the
    point."""
    with open(path, 'rb') as f:
        blob = f.read()
    magic, version, atype, count, name_off = struct.unpack_from('<4sI4sIQ', blob, 0)
    assert magic == b'BTDX', 'not a BA2: %r' % (magic,)
    assert atype == b'GNRL', 'expected a general archive, got %r' % (atype,)
    entries = []
    p = 24
    for i in range(count):
        (nhash, ext, dhash, flags, off, packed, unpacked, sent) = \
            struct.unpack_from('<I4sIIQIII', blob, p)
        assert sent == SENTINEL, (
            'record %d sentinel is 0x%08X, not 0xBAADF00D - layout is wrong' % (i, sent))
        entries.append((off, packed, unpacked))
        p += 36
    # name table
    names = []
    q = name_off
    for i in range(count):
        ln = struct.unpack_from('<H', blob, q)[0]
        q += 2
        names.append(blob[q:q + ln].decode('latin1'))
        q += ln
    out = {}
    for name, (off, packed, unpacked) in zip(names, entries):
        if packed:
            data = zlib.decompress(blob[off:off + packed])
        else:
            data = blob[off:off + unpacked]
        assert len(data) == unpacked, (
            '%s: got %d bytes, header declared %d' % (name, len(data), unpacked))
        out[name.replace('/', '\\').lower()] = data
    return out


def parse_strings(data):
    """.STRINGS: u32 count, u32 dataSize, count x [u32 id][u32 offset], then a
    block of NUL-terminated UTF-8 at those offsets.
    (.DLSTRINGS/.ILSTRINGS prefix each entry with a u32 length instead.)"""
    count, data_size = struct.unpack_from('<II', data, 0)
    dir_end = 8 + count * 8
    assert dir_end + data_size == len(data), (
        'STRINGS size mismatch: 8 + %d*8 + %d != %d' % (count, data_size, len(data)))
    table = {}
    for i in range(count):
        sid, off = struct.unpack_from('<II', data, 8 + i * 8)
        p = dir_end + off
        q = data.index(b'\x00', p)
        table[sid] = data[p:q].decode('utf-8', 'replace')
    return table


_cache = {}


def lookup(string_id, lang='en'):
    key = lang
    if key not in _cache:
        files = read_ba2_general(BA2)
        want = ('strings\\fallout4_%s.strings' % lang)
        assert want in files, 'not in the archive: %s (have %s)' % (
            want, [n for n in files if n.endswith('.strings')][:5])
        _cache[key] = parse_strings(files[want])
    return _cache[key].get(string_id)


if __name__ == '__main__':
    files = read_ba2_general(BA2)
    strs = sorted(n for n in files if '.strings' in n)
    print('%d files in the archive; string tables: %s' % (len(files), strs))
    table = parse_strings(files['strings\\fallout4_en.strings'])
    print('Fallout4_en.STRINGS: %d entries' % len(table))
    for sid in (0x0003613F,):
        print('  0x%08X -> %r' % (sid, table.get(sid)))
