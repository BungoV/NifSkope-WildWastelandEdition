

# ############################################################################
# ############################################################################
#
#                    LANE ESPWRITE ADDITIONS  (2026-09-07)
#
# Everything ABOVE this line is E:\Projects\Claude\esp_lib.py copied verbatim.
# The original is untouched. Everything BELOW is new, and exists to WRITE a
# Fallout 4 plugin made entirely of LAND (and owning CELL) overrides.
#
# Why a new layer at all, rather than just calling the functions above:
#
#   * esp_lib.read_subrecords does not implement the XXXX oversize escape, so
#     it desynchronises on any record carrying a subrecord longer than 65535
#     bytes. Not fatal for LAND (largest is VNML/VCLR at 3267) but fatal for
#     the walk in general. fo4esm.subrecords handles it. REPORTED, not
#     silently worked around - see report_espwrite.md "Defects found in
#     esp_lib.py".
#   * esp_lib.walk_all_records advances past a GRUP by 24 bytes and treats a
#     compressed record's dataSize as its on-disk size (correct), but it has
#     an off-by-one: `while pos < len(data) - 24` stops 24 bytes early and
#     silently drops a final record that ends exactly at EOF. Also REPORTED.
#   * esp_lib has no group WRITER at all. insert_records patches an existing
#     group's size in place; this lane builds group trees from nothing, where
#     every size must be computed bottom-up.
#   * esp_lib.compress_record_payload/decompress_record_payload carry an
#     explicit "wrapper format NOT validated, no compressed records here to
#     test against" caveat. This lane HAS them - all 36,864 Commonwealth LAND
#     records are compressed - and validates the wrapper. See the report.
#
# ############################################################################

import fo4esm as _E


# ------------------------------------------------------------------ flags

TES4_FLAG_ESM = 0x00000001
TES4_FLAG_LOCALIZED = 0x00000080
TES4_FLAG_ESL = 0x00000200

# Group types, re-exported so callers need only this module.
GT_TOP = _E.GT_TOP
GT_WORLD_CHILDREN = _E.GT_WORLD_CHILDREN
GT_EXTERIOR_BLOCK = _E.GT_EXTERIOR_BLOCK
GT_EXTERIOR_SUBBLOCK = _E.GT_EXTERIOR_SUBBLOCK
GT_CELL_CHILDREN = _E.GT_CELL_CHILDREN
GT_CELL_TEMPORARY = _E.GT_CELL_TEMPORARY


# ------------------------------------------------------- record / group IO

def build_record_raw(sig, formid, header_tail, payload, flags=0):
    """A complete record: 24-byte header + payload, dataSize = len(payload).

    Same contract as esp_lib.build_record but takes sig as bytes-or-str and
    does not re-encode a bytes signature. The 8-byte header_tail assert is
    kept from esp_lib deliberately: that module records a 4-byte-short header
    as a mistake that once silently corrupted a file.
    """
    if isinstance(sig, str):
        sig = sig.encode('ascii')
    assert len(sig) == 4, 'signature must be 4 bytes, got %r' % (sig,)
    assert len(header_tail) == 8, 'header_tail must be 8 bytes, got %d' % len(header_tail)
    return (sig + struct.pack('<IiI', len(payload), flags, formid)
            + bytes(header_tail) + bytes(payload))


def build_record_compressed(sig, formid, header_tail, payload, flags=0, level=9):
    """Emit the record with its payload zlib-compressed and the COMPRESSED
    flag set. On-disk payload is [u32 uncompressed size][zlib stream], the
    wrapper esp_lib flagged as unvalidated and this lane validates.

    Falls back to uncompressed if compression does not actually save bytes -
    which is what Bethesda does too (the median 46-byte Commonwealth CELL is
    left uncompressed in the master; zlib inflates it).
    """
    import zlib
    blob = struct.pack('<I', len(payload)) + zlib.compress(bytes(payload), level)
    if len(blob) >= len(payload):
        return build_record_raw(sig, formid, header_tail, payload,
                                flags & ~COMPRESSED_FLAG)
    return build_record_raw(sig, formid, header_tail, blob, flags | COMPRESSED_FLAG)


def build_group(gtype, label, children, tail=b'\x00' * 8):
    """A GRUP: 'GRUP' + u32 size + label[4] + i32 type + tail[8] + children.

    size INCLUDES the 24-byte header - the convention esp_lib.read_group_header
    documents, and the one thing here that a reader would notice immediately if
    it were wrong. test_roundtrip.py proves it by rebuilding the master's own
    groups byte-for-byte.
    """
    assert len(label) == 4, 'group label must be 4 bytes, got %r' % (label,)
    assert len(tail) == 8, 'group tail must be 8 bytes, got %d' % len(tail)
    body = b''.join(children) if not isinstance(children, (bytes, bytearray)) else bytes(children)
    return (b'GRUP' + struct.pack('<I', 24 + len(body)) + bytes(label)
            + struct.pack('<i', gtype) + bytes(tail) + body)


# -------------------------------------------------- exterior group labels

def _floordiv(v, d):
    """Floor division, explicitly. Python's // already floors for ints; this
    exists so the intent is stated at the call site, because truncation toward
    zero is the plausible wrong answer and it agrees with floor on every cell
    with x,y >= 0. Measured: truncation matches only 9,801 of 36,864 cells."""
    return v // d


def block_coords(cx, cy):
    """Exterior BLOCK coordinates for cell (cx, cy). 32x32 cells per block."""
    return _floordiv(cx, 32), _floordiv(cy, 32)


def subblock_coords(cx, cy):
    """Exterior SUB-BLOCK coordinates for cell (cx, cy). 8x8 cells per block."""
    return _floordiv(cx, 8), _floordiv(cy, 8)


def exterior_label(bx, by):
    """The 4-byte GRUP label for an exterior block or sub-block: two int16s,
    **Y FIRST, then X**, little-endian.

    MEASURED, not assumed: tested against all 36,864 Commonwealth cells in
    Fallout4.esm. (y,x) matched 36,864/36,864 for both blocks and sub-blocks;
    (x,y) matched 6,144/36,864 and 1,536/36,864 respectively.
    """
    return struct.pack('<hh', by, bx)


def label_sort_key(label):
    """Master ordering for block and sub-block groups: ascending value of the
    4-byte label read as a little-endian u32.

    MEASURED: the master writes blocks (x=0,y=0) (0,1) (0,2) (0,-3) (0,-2)
    (0,-1) (1,0)... which is neither signed (x,y) nor signed (y,x) order, but
    is exactly ascending u32 of the label (0,1,2,0xFFFD,0xFFFE,0xFFFF,0x10000).
    """
    return struct.unpack('<I', label)[0]


# -------------------------------------------------------- LAND texture ops

BTXT_LAYER_BASE = -1   # MEASURED: -1 in all 12,471 BTXTs in the Commonwealth.

# Byte 5 of BTXT/ATXT. esmdata.cpp and LODGEN_ESM_LAYOUTS.md both call it a
# pad; it is not (DLCRobot rewrites exactly this byte in one BTXT of LAND
# 0000F124, 0xC7 -> 0x4E, changing nothing else). Across the master it takes
# 95 distinct values, 0 in 56% of cases - uninitialised Creation Kit memory
# written to disk. 0 is both the most common real value and the only
# defensible one to synthesise.
BTXT_UNKNOWN_BYTE = 0


def build_btxt(ltex_formid, quadrant, unknown=BTXT_UNKNOWN_BYTE,
               layer=BTXT_LAYER_BASE):
    """An 8-byte BTXT body: u32 LTEX FormID, u8 quadrant, u8 unknown,
    int16 layer. Quadrants are 0 BL, 1 BR, 2 TL, 3 TR (docs/LODGEN_ESM_LAYOUTS.md).
    """
    assert 0 <= quadrant <= 3, 'quadrant must be 0..3, got %r' % (quadrant,)
    return struct.pack('<IBBh', ltex_formid, quadrant, unknown, layer)


def parse_btxt(content):
    """-> (ltex_formid, quadrant, unknown, layer)"""
    return struct.unpack('<IBBh', content[:8])


# Subrecords that must stay ahead of the texture block, in this order. The
# master always writes DATA, VNML, VHGT, VCLR (those that exist) before any
# BTXT/ATXT/VTXT; verified by subrecord-shape census over all 36,864 LANDs.
LAND_HEAD_TAGS = (b'DATA', b'VNML', b'VHGT', b'VCLR')


def splice_land_base_textures(payload, quad_ltex, unknown=BTXT_UNKNOWN_BYTE):
    """THE CORE OPERATION. Return a new LAND payload with quadrant base
    textures set, and EVERYTHING ELSE COPIED THROUGH BYTE-FOR-BYTE.

    payload    -- the master LAND record's decompressed payload
    quad_ltex  -- dict {quadrant:int 0..3 -> LTEX FormID:int}. A quadrant
                  absent from the dict is left exactly as the master had it.
                  A quadrant mapped to None or 0 has its BTXT REMOVED.

    Rules, all following from "an override replaces the whole record":
      * every non-BTXT subrecord is emitted in its original order with its
        original bytes - VNML, VHGT, VCLR, ATXT, VTXT, DATA, MPCD, anything
        unknown. Nothing is reinterpreted, nothing is regenerated.
      * a quadrant that already has a BTXT gets its FormID replaced IN PLACE,
        preserving the subrecord's position and its unknown byte and layer.
      * a quadrant that has no BTXT gets one INSERTED immediately after the
        last head subrecord (DATA/VNML/VHGT/VCLR) and before the first
        ATXT/VTXT, which is where the master puts a quadrant's BTXT relative
        to that quadrant's alpha layers.
      * inserted BTXTs go in ascending quadrant order, which is the master's
        own order for the ones it writes contiguously.
    """
    subs = list(_E.subrecords(payload))
    seen = set()
    out = []
    inserted_at = None

    for idx, (tag, content) in enumerate(subs):
        if tag == b'BTXT' and len(content) >= 8:
            ltex, quad, unk, layer = parse_btxt(content)
            if quad in quad_ltex:
                seen.add(quad)
                new = quad_ltex[quad]
                if not new:
                    continue                      # requested removal
                content = struct.pack('<IBBh', new, quad, unk, layer)
        out.append((tag, content))

    missing = [q for q in sorted(quad_ltex) if q not in seen and quad_ltex[q]]
    if missing:
        # insertion point: after the last leading head subrecord
        pos = 0
        for i, (tag, _) in enumerate(out):
            if tag in LAND_HEAD_TAGS:
                pos = i + 1
            else:
                break
        new_subs = [(b'BTXT', build_btxt(quad_ltex[q], q, unknown))
                    for q in missing]
        out[pos:pos] = new_subs
        inserted_at = pos

    blob = b''.join(sub(t.decode('ascii'), c) for t, c in out)
    return blob, len(missing), inserted_at


def land_quadrant_state(payload):
    """-> {quadrant -> ltex formid} for the BTXTs a LAND already has, plus a
    bool saying whether it has any ATXT alpha layers at all."""
    base = {}
    has_alpha = False
    for tag, content in _E.subrecords(payload):
        if tag == b'BTXT' and len(content) >= 8:
            ltex, quad, unk, layer = parse_btxt(content)
            base[quad] = ltex
        elif tag == b'ATXT':
            has_alpha = True
    return base, has_alpha


# ------------------------------------------------------------------- TES4

def build_tes4(masters, num_records, next_object_id=0x800, author=None,
               description=None, flags=TES4_FLAG_ESM, tail=None,
               hedr_version=1.0):
    """Build the plugin header record.

    masters        -- list of filenames in load order, e.g. ['Fallout4.esm'].
                      Each becomes MAST + a 8-byte DATA, which is what every
                      shipped plugin does.
    num_records    -- HEDR record count. Bethesda counts every record in the
                      file EXCLUDING the TES4 itself; make_landfix_esp.py
                      computes it by counting what it actually emitted rather
                      than predicting it.
    next_object_id -- HEDR next object id. This plugin creates NO new records,
                      so nothing ever allocates from it. 0x800 is the first id
                      a light plugin may use and is a safe floor.
    flags          -- see TES4_FLAG_*. Deliberately NOT setting
                      TES4_FLAG_LOCALIZED: measured, 0 of 36,864 Commonwealth
                      CELLs carry a localizable subrecord, so nothing copied
                      through needs a strings table.
    """
    payload = sub('HEDR', struct.pack('<fiI', hedr_version, num_records,
                                      next_object_id))
    payload += sub('CNAM', (author or 'landfix').encode('ascii') + b'\x00')
    if description:
        payload += sub('SNAM', description.encode('ascii') + b'\x00')
    for m in masters:
        payload += sub('MAST', m.encode('ascii') + b'\x00')
        payload += sub('DATA', struct.pack('<Q', 0))
    # ONAM (overridden-forms list) is only meaningful for an ESM flagged
    # plugin that overrides persistent references; LAND/CELL temporary-child
    # overrides do not go in it. Omitted deliberately.
    payload += sub('INTV', struct.pack('<I', 1))
    return build_record_raw(b'TES4', 0, tail or (b'\x00' * 8), payload, 0) , payload


def hedr_patch_num_records(record_bytes, num_records):
    """Rewrite HEDR.numRecords inside an already-built TES4 record. Used
    because the true count is only known after the whole tree is emitted."""
    buf = bytearray(record_bytes)
    p = 24
    end = 24 + struct.unpack_from('<I', buf, 4)[0]
    while p < end:
        tag = bytes(buf[p:p + 4])
        size = struct.unpack_from('<H', buf, p + 4)[0]
        if tag == b'HEDR':
            struct.pack_into('<i', buf, p + 6 + 4, num_records)
            return bytes(buf)
        p += 6 + size
    raise ValueError('HEDR not found in TES4 record')
