"""
esp_lib.py - Validated FO4 .esp binary read/write helpers.

Everything in this module was cross-validated against real data in
X01Remastered.esp during the 2026-06-27 and 2026-06-29 sessions - either
against known values from prior CK/xEdit screenshots, or against a second
independent example, or both. Anything NOT validated this rigorously is
explicitly flagged in its own docstring rather than presented with equal
confidence.

Confirmed-solid: generic record/group walking, EFSH DNAM schema (full),
VMAD script-property encoding for Object/Float/Bool/String/Array types,
AVIF/Paralysis-Spell/Script-Effect record templates, MGEF archetype field
offset (48) and confirmed value for script-running MGEFs (0 in all cases,
regardless of whether applied via ENCH to wearer or RemoteCast to target),
GRUP size update requirements (patching a record inside a GRUP requires
updating the GRUP size field or the engine stops reading at the old boundary).

Explicitly NOT validated (flagged at point of use, not solved here):
PERK record structure beyond the ACHS entry pattern, OMOD's full property-
block semantics beyond the ENCH-reference sub-pattern.

Key gotchas confirmed 2026-06-29:
- MGEF archetype field is at offset 64 (not 48 as previously assumed).
- MGEF archetype field is at offset 64 (not 48 as previously assumed).
- Archetype 1 = Script self-applied (ENCH on armor → wearer). Archetype 45 =
  Script target-applied (RemoteCast at enemy). VMAD scripts appear to fire
  regardless of archetype — the archetype controls engine-native math behavior,
  not script execution.
- Records inserted anywhere in the file must be inside the appropriate top-level
  GRUP or the engine silently ignores them. Patching a record inside a GRUP
  requires updating that GRUP's size field.
"""

import struct


# ============================================================
# Generic record/group walking - confirmed reliable across every
# record type encountered this session, including records with VMAD,
# nested subrecords, and GRUP groups. Does NOT handle compressed
# records (flag bit 0x00040000) - those are detected and skipped/
# flagged, never silently misparsed.
# ============================================================

COMPRESSED_FLAG = 0x00040000


def read_record_header(buf, offset):
    """Returns (signature, data_size, flags, formid) for the record at offset."""
    sig = buf[offset:offset+4].decode('ascii', errors='replace')
    data_size = struct.unpack_from('<I', buf, offset+4)[0]
    flags = struct.unpack_from('<I', buf, offset+8)[0]
    formid = struct.unpack_from('<I', buf, offset+12)[0]
    return sig, data_size, flags, formid


def read_record_header_tail(buf, offset):
    """Returns the 8 bytes after FormID (revision + version/unknown) -
    needed when cloning a record's header. Confirmed via direct byte
    inspection that this is 8 bytes, not 4 - an earlier attempt at this
    session built a 4-byte-short header from copying only the last 4,
    which silently corrupted the file (caught only by full re-verification,
    not by the write itself succeeding)."""
    return bytes(buf[offset+16:offset+24])


def read_group_header(buf, offset):
    """Returns (signature, group_size, label, group_type) for the GRUP at offset.
    group_size INCLUDES the 24-byte GRUP header itself."""
    sig = buf[offset:offset+4].decode('ascii', errors='replace')
    group_size = struct.unpack_from('<I', buf, offset+4)[0]
    label = buf[offset+8:offset+12]
    group_type = struct.unpack_from('<I', buf, offset+12)[0]
    return sig, group_size, label, group_type


def read_subrecords(buf, start, length):
    """Walk subrecords within a record's payload. Returns list of (tag, content_bytes)."""
    subs = []
    p = start
    end = start + length
    while p < end:
        tag = buf[p:p+4].decode('ascii', errors='replace')
        size = struct.unpack_from('<H', buf, p+4)[0]
        content = buf[p+6:p+6+size]
        subs.append((tag, content))
        p += 6 + size
    return subs


def sub(tag, content):
    """Build a single subrecord: 4-char tag + u16 length + content."""
    return tag.encode('ascii') + struct.pack('<H', len(content)) + content


def build_record(sig, formid, header_tail, payload_bytes, flags=0):
    """Build a complete record: 24-byte header + payload. header_tail must
    be 8 bytes (see read_record_header_tail)."""
    assert len(header_tail) == 8, f"header_tail must be 8 bytes, got {len(header_tail)}"
    header = struct.pack('<4sIII', sig.encode('ascii'), len(payload_bytes), flags, formid) + header_tail
    return header + payload_bytes


def walk_all_records(data, include_groups=False):
    """Walk the entire file. Yields (offset, sig, data_size, flags, formid)
    for every non-GRUP record (and GRUP headers too if include_groups=True).
    This is the SAME loop used throughout this session's verification
    scripts - confirmed to reach EOF exactly on a clean file."""
    pos = 0
    while pos < len(data) - 24:
        sig = data[pos:pos+4].decode('ascii', errors='replace')
        if sig == 'GRUP':
            if include_groups:
                yield (pos,) + read_group_header(data, pos)
            pos += 24
            continue
        sig, dsize, flags, formid = read_record_header(data, pos)
        yield pos, sig, dsize, flags, formid
        pos += 24 + dsize


def build_formid_index(data):
    """FormID -> (record_type, EditorID) for every uncompressed record with
    an EDID in the file. Used for cross-reference validation when decoding
    unfamiliar blocks (e.g. spotting a real FormList/ENCH/AVIF reference
    inside a record you haven't fully reverse-engineered yet)."""
    index = {}
    for pos, sig, dsize, flags, formid in walk_all_records(data):
        if flags & COMPRESSED_FLAG:
            continue
        subs = read_subrecords(data, pos+24, dsize)
        edid = next((c.rstrip(b'\x00').decode('ascii', 'replace') for t, c in subs if t == 'EDID'), None)
        if edid:
            index[formid] = (sig, edid)
    return index


def find_record_by_edid(data, record_type, target_edid):
    """Returns (offset, formid, data_size, subrecords) or None.
    subrecords is the list from read_subrecords (NOT a dict - duplicate
    tags, e.g. multiple Color Key subrecords sharing a name in some
    record types, would collide in a dict)."""
    for pos, sig, dsize, flags, formid in walk_all_records(data):
        if sig != record_type or (flags & COMPRESSED_FLAG):
            continue
        subs = read_subrecords(data, pos+24, dsize)
        edid = next((c.rstrip(b'\x00').decode('ascii', 'replace') for t, c in subs if t == 'EDID'), None)
        if edid == target_edid:
            return pos, formid, dsize, subs
    return None


def find_group(data, label_str):
    """Returns (group_offset, group_size) for the top-level GRUP with this
    4-char label (group_type 0). Raises if not found."""
    for pos, sig, dsize, label, gtype in walk_all_records(data, include_groups=True):
        if sig == 'GRUP' and label == label_str.encode('ascii') and gtype == 0:
            return pos, dsize
    raise ValueError(f"group not found: {label_str}")


# ============================================================
# EFSH DNAM schema - FULLY validated, every field cross-checked against
# real known values (see session notes: Break shader's exact Falloff/
# Color/Alpha/ColorKeyScale/Time values, all confirmed bit-for-bit).
# ============================================================

EFSH_DNAM_FIELDS = [
    # (name, kind, byte_offset) - kind is 'f'=float, 'i'=int32, 'rgb'=3 bytes+pad
    ('src_blend', 'i', 0), ('blend_op', 'i', 4), ('z_test', 'i', 8),
    ('fill_ck1', 'rgb', 12),
    ('fill_alpha_fadein', 'f', 16), ('fill_full_time', 'f', 20),
    ('fill_alpha_fadeout', 'f', 24), ('fill_persist_ratio', 'f', 28),
    ('fill_pulse_amp', 'f', 32), ('fill_pulse_freq', 'f', 36),
    ('fill_anim_u', 'f', 40), ('fill_anim_v', 'f', 44),
    ('edge_falloff', 'f', 48), ('edge_color', 'rgb', 52),
    ('edge_alpha_fadein', 'f', 56), ('edge_full_time', 'f', 60),
    ('edge_alpha_fadeout', 'f', 64), ('edge_persist_ratio', 'f', 68),
    ('edge_pulse_amp', 'f', 72), ('edge_pulse_freq', 'f', 76),
    ('fill_full_ratio', 'f', 80), ('edge_full_ratio', 'f', 84),
    ('dest_blend', 'i', 88),
    ('holes_start_time', 'f', 92), ('holes_end_time', 'f', 96),
    ('holes_start_value', 'f', 100), ('holes_end_value', 'f', 104),
    ('ambient_sound', 'i', 108),
    ('fill_ck2', 'rgb', 112), ('fill_ck3', 'rgb', 116),
    # byte 120: 'unknown' field is 1 BYTE, not 4 - confirmed by exact
    # remaining-length math (37 bytes left, only fits if this is 1 byte)
]
EFSH_DNAM_TAIL_FIELDS = [
    # relative to offset 121 (after the 1-byte unknown at 120)
    ('ck1_scale', 'f', 0), ('ck2_scale', 'f', 4), ('ck3_scale', 'f', 8),
    ('ck1_time', 'f', 12), ('ck2_time', 'f', 16), ('ck3_time', 'f', 20),
    ('flags', 'i', 24), ('tex_scale_u', 'f', 28), ('tex_scale_v', 'f', 32),
]
EFSH_DNAM_TOTAL_LEN = 157


def decode_efsh_dnam(dnam):
    """Decode a full 157-byte EFSH DNAM block into a dict of named values."""
    assert len(dnam) == EFSH_DNAM_TOTAL_LEN, f"expected {EFSH_DNAM_TOTAL_LEN} bytes, got {len(dnam)}"
    result = {}
    for name, kind, off in EFSH_DNAM_FIELDS:
        if kind == 'f':
            result[name] = struct.unpack_from('<f', dnam, off)[0]
        elif kind == 'i':
            result[name] = struct.unpack_from('<i', dnam, off)[0]
        elif kind == 'rgb':
            result[name] = tuple(dnam[off:off+3])
    result['unknown_byte'] = dnam[120]
    for name, kind, rel_off in EFSH_DNAM_TAIL_FIELDS:
        off = 121 + rel_off
        if kind == 'f':
            result[name] = struct.unpack_from('<f', dnam, off)[0]
        elif kind == 'i':
            result[name] = struct.unpack_from('<i', dnam, off)[0]
    return result


def encode_efsh_dnam(values, defaults=None):
    """Build a 157-byte EFSH DNAM block from a dict of named values
    (same keys as decode_efsh_dnam produces). Missing keys fall back to
    `defaults` dict if given, else 0/0.0/(0,0,0)."""
    defaults = defaults or {}

    def get(name, fallback):
        if name in values:
            return values[name]
        return defaults.get(name, fallback)

    buf = bytearray(EFSH_DNAM_TOTAL_LEN)
    for name, kind, off in EFSH_DNAM_FIELDS:
        if kind == 'f':
            struct.pack_into('<f', buf, off, get(name, 0.0))
        elif kind == 'i':
            struct.pack_into('<i', buf, off, get(name, 0))
        elif kind == 'rgb':
            r, g, b = get(name, (0, 0, 0))
            buf[off:off+4] = bytes([r, g, b, 0])
    buf[120] = get('unknown_byte', 4)
    for name, kind, rel_off in EFSH_DNAM_TAIL_FIELDS:
        off = 121 + rel_off
        if kind == 'f':
            struct.pack_into('<f', buf, off, get(name, 0.0))
        elif kind == 'i':
            struct.pack_into('<I', buf, off, get(name, 0))
    return bytes(buf)


# ============================================================
# VMAD script-property encoding - Object/Float confirmed via TWO
# internal cross-checks (AVIF_Health -> real vanilla FormID 0x2D4,
# SETTING_ShieldBreakSpellLingerTime -> real known 0.4 value) plus full
# external round-trip validation (the melee-shock build's 19-property
# VMAD decoded back byte-perfect via xEdit... wait, via direct re-parse).
# Bool confirmed via a real external documented example only - NOT yet
# cross-checked against a second example inside this specific file.
# ============================================================

def vmad_prop_object(name, formid):
    """Object-type property (type=01): for ActorValue/Spell/FormList/etc.
    references - any Form-derived type encodes identically here, type
    safety is enforced by the compiled script's own declarations, not
    stored separately in VMAD."""
    name_b = name.encode('ascii')
    return struct.pack('<H', len(name_b)) + name_b + bytes([0x01, 0x01]) + b'\x00\x00\xff\xff' + struct.pack('<I', formid)


def vmad_prop_float(name, value):
    """Float-type property (type=04)."""
    name_b = name.encode('ascii')
    return struct.pack('<H', len(name_b)) + name_b + bytes([0x04, 0x01]) + struct.pack('<f', value)


def vmad_prop_bool(name, value):
    """Bool-type property (type=05). NOTE: confirmed via external
    documented example only (a real community reverse-engineering thread),
    not yet cross-validated against a second example inside this specific
    project's own file - treat with slightly less confidence than Object/Float."""
    name_b = name.encode('ascii')
    return struct.pack('<H', len(name_b)) + name_b + bytes([0x05, 0x01]) + bytes([1 if value else 0])


def vmad_prop_int(name, value):
    """Int-type property (type=03). Confirmed via external documented
    example only, same caveat as Bool - not yet cross-validated inside
    this project's own file."""
    name_b = name.encode('ascii')
    return struct.pack('<H', len(name_b)) + name_b + bytes([0x03, 0x01]) + struct.pack('<i', value)


def build_vmad(script_name, property_blocks):
    """Assemble a complete single-script VMAD field.
    property_blocks: list of already-encoded property bytes (from the
    vmad_prop_* functions above), in the EXACT order the .psc declares them."""
    header = struct.pack('<HHH', 6, 2, 1)  # version, objformat, scriptCount - confirmed values
    name_b = script_name.encode('ascii')
    script_block = (struct.pack('<H', len(name_b)) + name_b + bytes([0x00])
                     + struct.pack('<H', len(property_blocks)) + b''.join(property_blocks))
    return header + script_block


def decode_vmad_properties(vmad):
    """Decode a VMAD back into a list of (script_name, [(name, type, value), ...])
    tuples - one entry per attached script. Confirmed this session against
    a real vanilla multi-script example (PerkPainTrainStaggerEffect,
    scriptCount=2) that multiple scripts are simply sequential blocks with
    no separator - each one's own namelen+name+status+propcount+properties,
    immediately followed by the next script's same structure.

    Stops and returns what it has so far if it hits an unrecognized
    property type code, rather than raising - so partial/unfamiliar VMADs
    are still inspectable. Returns (scripts, bytes_consumed) where scripts
    is the list described above."""
    version, objfmt, scriptcount = struct.unpack_from('<HHH', vmad, 0)
    p = 6
    scripts = []

    for _ in range(scriptcount):
        namelen = struct.unpack_from('<H', vmad, p)[0]; p += 2
        script_name = vmad[p:p+namelen].decode('ascii'); p += namelen
        p += 1  # status byte
        propcount = struct.unpack_from('<H', vmad, p)[0]; p += 2

        props = []
        stopped_early = False
        for _ in range(propcount):
            pnamelen = struct.unpack_from('<H', vmad, p)[0]; p += 2
            pname = vmad[p:p+pnamelen].decode('ascii'); p += pnamelen
            ptype = vmad[p]; p += 2  # skip status byte too
            if ptype == 0:   # None
                props.append((pname, 'none', None))
            elif ptype == 1: # Object
                formid = struct.unpack_from('<I', vmad, p+4)[0]
                props.append((pname, 'object', formid)); p += 8
            elif ptype == 2: # Int32 (alternate int encoding)
                val = struct.unpack_from('<i', vmad, p)[0]
                props.append((pname, 'int', val)); p += 4
            elif ptype == 3: # Int32
                val = struct.unpack_from('<i', vmad, p)[0]
                props.append((pname, 'int', val)); p += 4
            elif ptype == 4: # Float
                val = struct.unpack_from('<f', vmad, p)[0]
                props.append((pname, 'float', val)); p += 4
            elif ptype == 5: # Bool
                val = bool(vmad[p])
                props.append((pname, 'bool', val)); p += 1
            elif ptype == 6: # String
                slen = struct.unpack_from('<H', vmad, p)[0]; p += 2
                val = vmad[p:p+slen].decode('ascii', errors='replace'); p += slen
                props.append((pname, 'string', val))
            elif ptype in (11, 12, 13, 14, 15, 16): # Array types (elem type = ptype - 10)
                arr_len = struct.unpack_from('<I', vmad, p)[0]; p += 4
                elem_type = ptype - 10
                arr = []
                for _ in range(arr_len):
                    if elem_type == 1:
                        formid = struct.unpack_from('<I', vmad, p+4)[0]; p += 8
                        arr.append(('object', formid))
                    elif elem_type in (2, 3):
                        val = struct.unpack_from('<i', vmad, p)[0]; p += 4
                        arr.append(('int', val))
                    elif elem_type == 4:
                        val = struct.unpack_from('<f', vmad, p)[0]; p += 4
                        arr.append(('float', val))
                    elif elem_type == 5:
                        arr.append(('bool', bool(vmad[p]))); p += 1
                    elif elem_type == 6:
                        slen = struct.unpack_from('<H', vmad, p)[0]; p += 2
                        arr.append(('string', vmad[p:p+slen].decode('ascii','replace'))); p += slen
                    else:
                        props.append((pname, f'UNKNOWN array elem type={elem_type}', None))
                        stopped_early = True
                        break
                if not stopped_early:
                    props.append((pname, 'array', arr))
            else:
                props.append((pname, f'UNKNOWN type={ptype}', None))
                stopped_early = True
                break
        scripts.append((script_name, props))
        if stopped_early:
            break

    return scripts, p  # p = bytes consumed, compare to len(vmad)


# ============================================================
# Record insertion - handles the multi-step process this session got
# wrong once (4-byte-short header) and incomplete once (forgot to update
# next_object_id) before getting fully right. Centralizing it here so
# those two specific mistakes structurally can't happen again.
# ============================================================

def insert_records(data, insertions, hedr_offset, num_new_records, new_next_object_id=None):
    """
    data: bytearray of the full file (mutated in place)
    insertions: list of (insertion_point, record_bytes, group_header_offset)
                - sort by insertion_point descending BEFORE calling, so
                  earlier insertions don't invalidate offsets for later
                  ones still to be applied
    hedr_offset: byte offset of the TES4 record's HEDR content start
                 (the float version field - num_records is hedr_offset+4,
                 next_object_id is hedr_offset+8)
    num_new_records: total records being added, for the num_records bump
    new_next_object_id: if given, sets next_object_id to exactly this
                         value (recommended: continue the file's own
                         existing counter, NOT an arbitrary higher gap -
                         this session's thermal-vision test deliberately
                         left a gap and that was flagged as the wrong
                         instinct, not a safe convention to repeat)
    """
    for insertion_point, record_bytes, group_header_offset in sorted(insertions, key=lambda x: -x[0]):
        data[insertion_point:insertion_point] = record_bytes
        old_size = struct.unpack_from('<I', data, group_header_offset+4)[0]
        struct.pack_into('<I', data, group_header_offset+4, old_size + len(record_bytes))

    num_records_offset = hedr_offset + 4
    old_count = struct.unpack_from('<I', data, num_records_offset)[0]
    struct.pack_into('<I', data, num_records_offset, old_count + num_new_records)

    if new_next_object_id is not None:
        next_id_offset = hedr_offset + 8
        struct.pack_into('<I', data, next_id_offset, new_next_object_id)


def delete_records(data, deletions, hedr_offset, num_deleted_records, new_next_object_id=None):
    """
    The structural inverse of insert_records - removes record bytes
    entirely rather than adding them.

    data: bytearray of the full file (mutated in place)
    deletions: list of (record_start_offset, total_record_length, group_header_offset)
               - total_record_length = 24 + data_size (the full record,
                 header included)
               - sort by record_start_offset DESCENDING before calling, same
                 reasoning as insert_records but more critical here: removing
                 bytes shifts everything AFTER the deletion point backward,
                 so deletions must be applied from the highest offset down
                 or every subsequent offset still to be processed is wrong
    hedr_offset: same meaning as insert_records
    num_deleted_records: total records being removed, for the num_records
                          decrement
    new_next_object_id: generally NOT recommended to change on deletion -
                         unlike insertion, freeing up a FormID for reuse is
                         only safe if you're certain nothing else in the
                         file or any dependent plugin references it by
                         FormID. Leave None (default) unless you've
                         specifically confirmed that.
    """
    for record_start, total_length, group_header_offset in sorted(deletions, key=lambda x: -x[0]):
        del data[record_start:record_start+total_length]
        old_size = struct.unpack_from('<I', data, group_header_offset+4)[0]
        struct.pack_into('<I', data, group_header_offset+4, old_size - total_length)

    num_records_offset = hedr_offset + 4
    old_count = struct.unpack_from('<I', data, num_records_offset)[0]
    struct.pack_into('<I', data, num_records_offset, old_count - num_deleted_records)

    if new_next_object_id is not None:
        next_id_offset = hedr_offset + 8
        struct.pack_into('<I', data, next_id_offset, new_next_object_id)


def find_record_by_formid(data, target_formid):
    """Returns (offset, sig, data_size, flags) for a record by FormID, or
    None. Useful for deletion, where you have a FormID but need the exact
    offset/length to remove."""
    for pos, sig, dsize, flags, formid in walk_all_records(data):
        if formid == target_formid:
            return pos, sig, dsize, flags
    return None


def decompress_record_payload(buf, record_start, data_size):
    """Decompress a record whose COMPRESSED_FLAG is set. FO4's compressed
    record payload is documented as [4-byte decompressed size][zlib-
    compressed data]. The zlib round-trip itself is tested directly (see
    session notes) - but this specific 4-byte-size-prefix WRAPPER format
    has NOT been cross-validated against a real compressed record from
    this project's own file, since none exist here to test against. Treat
    with appropriately lower confidence than the rest of this module until
    confirmed against a real example."""
    import zlib
    raw = buf[record_start:record_start+data_size]
    declared_size = struct.unpack_from('<I', raw, 0)[0]
    decompressed = zlib.decompress(raw[4:])
    assert len(decompressed) == declared_size, (
        f"decompressed size {len(decompressed)} != declared {declared_size} - "
        "wrapper format assumption may be wrong for this record"
    )
    return decompressed


def compress_record_payload(payload):
    """Inverse of decompress_record_payload - same untested-wrapper-format
    caveat applies."""
    import zlib
    compressed = zlib.compress(payload)
    return struct.pack('<I', len(payload)) + compressed



def find_hedr_offset(data):
    """Locate the TES4 record's HEDR content start offset, for use with insert_records."""
    tes4_sig, tes4_size, tes4_flags, tes4_formid = read_record_header(data, 0)
    p = 24
    while p < 24 + tes4_size:
        tag = data[p:p+4].decode('ascii', errors='replace')
        size = struct.unpack_from('<H', data, p+4)[0]
        if tag == 'HEDR':
            return p + 6
        p += 6 + size
    raise ValueError("HEDR not found in TES4 record")


# MGEF DATA layout constants moved to consolidated block below.

# ============================================================
# MGEF Archetype enum - confirmed via statistical cross-check against all
# uncompressed MGEFs in Fallout4.esm plus direct EDID name verification.
# ============================================================
# Script archetypes: MGEFs whose primary purpose is running a Papyrus script.
# VMAD scripts appear to execute regardless of archetype — scripts have been
# observed running on archetype 0 (Value Modifier) MGEFs as well. The archetype
# controls the engine-native math behaviour layered on top of (or instead of) the
# script, not whether the script fires. Use these values when building a pure-
# script MGEF to match the pattern used in this project.
MGEF_ARCHETYPE_SCRIPT_SELF   = 1   # confirmed: ShieldMaster (ENCH-applied to wearer via armor OMOD)
MGEF_ARCHETYPE_SCRIPT_TARGET = 45  # confirmed: MeleeRetaliation (RemoteCast at aggressor)

# Engine-native archetypes
MGEF_ARCHETYPE_VALUE_MODIFIER      = 0   # damage/restore any AV
MGEF_ARCHETYPE_DUAL_VALUE_MODIFIER = 5   # e.g. FortifyResistRads
MGEF_ARCHETYPE_CALM                = 6
MGEF_ARCHETYPE_DEMORALIZE          = 7
MGEF_ARCHETYPE_FRENZY              = 8
MGEF_ARCHETYPE_DISARM              = 9
MGEF_ARCHETYPE_INVISIBILITY        = 11
MGEF_ARCHETYPE_LIGHT               = 12
MGEF_ARCHETYPE_SUMMON_CREATURE     = 18
MGEF_ARCHETYPE_PARALYSIS           = 21  # confirmed via ParalyzeEffect
MGEF_ARCHETYPE_GUIDE               = 25  # VANS effect
MGEF_ARCHETYPE_CURE_ADDICTION      = 28
MGEF_ARCHETYPE_VALUE_AND_PARTS     = 31  # RestoreHealthStimpak etc.
MGEF_ARCHETYPE_STAGGER             = 33  # confirmed via PerkPainTrainStaggerEffect
MGEF_ARCHETYPE_PEAK_VALUE_MODIFIER = 34  # persistent buffs/debuffs (resist, carry weight, etc.)
MGEF_ARCHETYPE_CLOAK               = 35  # confirmed via PA_ImpactLandingAggroCloak
MGEF_ARCHETYPE_ACCUMULATE_MAGNITUDE = 36 # e.g. ArmorReducedPowerAttackEffect
MGEF_ARCHETYPE_SLOW_TIME           = 37  # confirmed via SlowTimeJet
MGEF_ARCHETYPE_REANIMATE           = 47  # confirmed via GlowingOneReanimate
MGEF_ARCHETYPE_JETPACK             = 48  # confirmed via jetpackEFFECT
MGEF_ARCHETYPE_CHAMELEON           = 49  # confirmed via AssaultronStealthEFFECT


# ============================================================
# Apply Combat Hit Spell Perk Entry - FULLY CONFIRMED, from a clean,
# freshly-built CK example (not the earlier test perk, which had a messy
# edit history and likely-stale EPFT data). Byte accounting verified
# exact: every subrecord here sums to precisely the real observed diff
# when this was added to an empty-entries Perk.
# ============================================================
ACHS_ENTRY_POINT_ID = 51

def build_achs_perk_entry(spell_formid, owner_condition_ctda=None):
    """
    Build a complete "Apply Combat Hit Spell" Perk Entry block, ready to
    append to an empty-entries Perk record's payload (after its base
    EDID/FULL/DESC/DATA subrecords).

    spell_formid: the Spell this entry casts on a combat hit
    owner_condition_ctda: optional pre-built 32-byte CTDA block for the
                          Perk Owner condition tab (see EFSH/VMAD-style
                          construction elsewhere in this module) - pass
                          None to omit (no Perk Owner condition).

    NOTE: this only includes the Perk Owner condition tab. The Weapon and
    Target condition tabs are NOT covered here - that structure hasn't
    been confirmed from a real example yet.
    """
    payload = sub('PRKE', bytes([2, 0, 1]))  # Type=2(EntryPoint), Rank=0, Priority=1
    payload += sub('DATA', bytes([ACHS_ENTRY_POINT_ID, 2, 3]))  # EntryID=51, Function=2, CondTabCount=3
    if owner_condition_ctda:
        payload += sub('PRKC', bytes([0]))  # tab marker: Perk Owner
        payload += sub('CTDA', owner_condition_ctda)
    payload += sub('EPFT', bytes([5]))
    payload += sub('EPFB', bytes([1, 0]))
    payload += sub('EPF3', bytes([0, 0]))
    payload += sub('EPFD', struct.pack('<I', spell_formid))
    payload += sub('PRKF', b'')
    return payload


def build_armor_pieces_ctda(required_pieces, avif_armorpieces_formid):
    """Build the confirmed AVIF_ArmorPieces >= threshold CTDA block (32
    bytes) - Type byte 0x00 confirmed this session to mean >=, not ==,
    via real SPECIAL-perk cross-validation against vanilla Fallout4.esm."""
    return (bytes([0, 0, 0, 0])  # Type=0(>=), Unused
            + struct.pack('<f', float(required_pieces))  # ComparisonValue
            + struct.pack('<i', 14)  # FunctionIndex = GetActorValue (confirmed)
            + struct.pack('<I', avif_armorpieces_formid)  # Param1
            + struct.pack('<I', 0)  # Param2
            + struct.pack('<I', 0)  # RunOnType
            + struct.pack('<I', 0)  # Reference
            + bytes([0xFF, 0xFF, 0xFF, 0xFF]))  # final field - confirmed 0xFFFFFFFF in this context (Perk), was 0 on the ENCH context


# ============================================================
# Weapon keyword CTDA - CONFIRMED via real vanilla BigLeagues/IronFist
# perks. func=560 checks whether the equipped weapon carries a given
# keyword. Type=0x01 confirmed used by vanilla for OR-chained keyword
# checks (melee1H OR melee2H) - the exact OR-vs-AND bit semantics are NOT
# fully decoded, this is a direct structural clone of a proven-working
# vanilla example, not derived from a fully-understood flag table.
# ============================================================
WEAPON_KEYWORD_FUNCTION_INDEX = 560

def build_weapon_keyword_ctda(keyword_formid):
    """Build a CTDA block checking 'equipped weapon has this keyword',
    structurally identical to vanilla BigLeagues/IronFist's own weapon
    checks. Chain multiple of these together (as in vanilla) to OR
    several keywords - e.g. melee1H + melee2H + unarmed for 'melee or
    unarmed', the exact pattern this project needed."""
    return (bytes([0x01, 0, 0, 0])  # Type=0x01 (confirmed vanilla pattern), Unused
            + struct.pack('<f', 1.0)  # ComparisonValue = 1.0 (true)
            + struct.pack('<i', WEAPON_KEYWORD_FUNCTION_INDEX)
            + struct.pack('<I', keyword_formid)  # Param1
            + struct.pack('<I', 0) + struct.pack('<I', 0) + struct.pack('<I', 0)
            + bytes([0xFF, 0xFF, 0xFF, 0xFF]))

KYWD_WEAPON_TYPE_MELEE_1H = 0x0004A0A4
KYWD_WEAPON_TYPE_MELEE_2H = 0x0004A0A5
KYWD_WEAPON_TYPE_UNARMED = 0x0005240E


# ============================================================
# CTDA Type byte - CORRECTED this session. Earlier inferred (wrongly)
# from SPECIAL-perk gating that low bits = operator and 0x00 = ">=".
# That was built on a bad assumption about what those perks were actually
# checking. The authoritative answer, confirmed against xEdit's own
# source-level documentation (Starfield wiki, citing wbDefinitionsSF1.pas):
#
#   Compare operator = UPPER 3 bits (bits 5-7)
#     0=EqualTo, 1=NotEqualTo, 2=GreaterThan, 3=GreaterOrEqual,
#     4=LessThan, 5=LessOrEqual
#   Flags = LOWER 5 bits (bits 0-4)
#     0x01=OR (default is AND), 0x02=UseAliases, 0x04=UseGlobal,
#     0x08=UsePackData, 0x10=SwapSubjectTarget
#
# Re-verified against this project's own AVIF_ArmorPieces CTDA (Type=0x00
# = EqualTo, no flags - functionally identical to >=6 in practice since
# 6 is armor's hard ceiling, which is exactly why one example couldn't
# distinguish them) and against BigLeagues' confirmed OR-chain (Type=0x01
# = EqualTo + OR flag, matching "HasKeyword==true, OR next condition").
# ============================================================
CTDA_OP_EQUAL = 0
CTDA_OP_NOT_EQUAL = 1
CTDA_OP_GREATER = 2
CTDA_OP_GREATER_OR_EQUAL = 3
CTDA_OP_LESS = 4
CTDA_OP_LESS_OR_EQUAL = 5

CTDA_FLAG_OR = 0x01
CTDA_FLAG_USE_ALIASES = 0x02
CTDA_FLAG_USE_GLOBAL = 0x04
CTDA_FLAG_USE_PACK_DATA = 0x08
CTDA_FLAG_SWAP_SUBJECT_TARGET = 0x10

def build_ctda_type_byte(operator, or_with_next=False):
    """Build the Type byte correctly: operator in the upper 3 bits, OR
    flag in the lowest bit if this condition should chain via OR with
    whatever comes immediately after it (default is AND)."""
    type_byte = (operator & 0x07) << 5
    if or_with_next:
        type_byte |= CTDA_FLAG_OR
    return type_byte


# ============================================================
# WEAP DNAM damage field - CONFIRMED via real vanilla weapons spanning a
# wide damage range, checked by relative ordering (no need to recall
# exact published numbers): Knife=20, CombatRifle=25, BaseballBat=35,
# Sledgehammer=40, SuperSledge=45 - monotonic, sensible real-world tiers.
# This is NOT accessible from Papyrus at all (confirmed earlier this
# project: vanilla Weapon script has zero damage-reading functions, F4SE
# doesn't add one either) - but IS directly readable via binary parsing,
# which is new capability this project didn't have before.
# ============================================================
WEAP_DNAM_DAMAGE_OFFSET = 112

def get_weapon_base_damage(weap_dnam):
    """Returns the base damage float from a WEAP record's DNAM subrecord."""
    return struct.unpack_from('<f', weap_dnam, WEAP_DNAM_DAMAGE_OFFSET)[0]


# ============================================================
# ARMO stats - CONFIRMED via Raider vs Combat torso armor, real relative
# tiers (Value 18->60, Weight 7.0->8.0, both sensible cost/weight jumps).
# DAMA is a repeating per-damage-type resistance entry, NOT part of DATA -
# confirmed both pieces specifically carry a dtEnergy entry with sensible
# relative ratings (2 vs 15). Base DATA only covers Value/Weight here -
# the primary Damage Resistance rating likely lives in a DAMA entry too
# (tagged with the Physical damage type), not separately confirmed this
# session since neither example checked had one.
# ============================================================
ARMO_DATA_VALUE_OFFSET = 0   # int
ARMO_DATA_WEIGHT_OFFSET = 4  # float

def decode_armo_dama(dama_content):
    """Returns (damage_type_formid, resistance_rating) for one DAMA entry.
    ARMO records can have multiple DAMA subrecords, one per damage type
    they specially resist - read_subrecords already returns each as a
    separate (tag, content) pair, don't dict() them (same duplicate-tag
    trap as VMAD's multiple DATA subrecords caught earlier this session)."""
    dt = struct.unpack_from('<i', dama_content, 0)[0]
    rating = struct.unpack_from('<i', dama_content, 4)[0]
    return dt, rating


# ============================================================
# MGEF DATA Delivery field - CONFIRMED via cross-reference against the
# already-confirmed SPIT.TargetType field across 326 real vanilla Spells.
# Near-perfect match (TargetType N -> Delivery N for N in 0,1,2,3, >99%
# consistency) - Delivery and TargetType share the same enum space, which
# makes sense since the Spell and its Effect must agree on this for the
# effect to actually apply (confirmed earlier this project: a mismatch
# here is exactly the kind of thing that makes a Spell silently do
# nothing). Combined with the already-confirmed Self=0/TargetActor=3 from
# the SPIT field, this gives Delivery's full known mapping too.
# ============================================================
# MGEF_DATA_DELIVERY_OFFSET = 84  # moved to consolidated MGEF DATA block below


# ============================================================
# OMOD type=2 block - CONFIRMED via real vanilla "ImprovedCarryCapacity2"
# armor linings: [type=2][subtype=10][AVIF FormID][value][0] = 20 bytes,
# modifies the given Actor Value by a flat amount. index=732 resolved to
# the real CarryWeight AV; values (10.0 for Limb pieces, 20.0 for Torso)
# match the expected relative tier (torso = bigger bonus). A separate,
# consistently-paired subtype=3 entry with a fixed FormID/tiny float
# appears alongside every example - looks like a standard companion/
# categorization tag, not independently meaningful, not decoded further.
# ============================================================
OMOD_BLOCK_TYPE_MOD_AVIF_FLAT = (2, 10)  # (type, subtype) -> [FormID][value]


# ============================================================
# EXPL DATA fields - CONFIRMED via the FO4 Creation Kit wiki's own
# documented field names ("Damage... Inner Radius... Outer Radius...
# IS Radius"), cross-checked against real examples: our own cosmetic
# shield-break explosion correctly shows Damage=0 (it's not meant to hurt
# anyone), and grenade/child-grenade explosions show consistent, sensible
# Damage values (100) with appropriately smaller radii on the child.
# ============================================================
EXPL_DATA_FORCE_OFFSET = 24
EXPL_DATA_DAMAGE_OFFSET = 28
EXPL_DATA_INNER_RADIUS_OFFSET = 32
EXPL_DATA_OUTER_RADIUS_OFFSET = 36
EXPL_DATA_IS_RADIUS_OFFSET = 40


# MGEF_DATA_CASTINGTYPE_OFFSET, MGEF_DATA_PROJECTILE_OFFSET, MGEF_DATA_EXPLOSION_OFFSET,
# MGEF_DATA_HITSHADER_OFFSET — moved to consolidated MGEF DATA block below.


# ============================================================
# QUST - minimal "system quest" template, CONFIRMED via real vanilla
# example (SystemPowerArmorQuest, 0x0001FA20) - genuinely new territory
# this session, last item investigated. Critically: VMAD on QUST uses
# the EXACT SAME schema already confirmed on MGEF/PERK - no differences
# at all, the existing build_vmad/vmad_prop_* functions work unchanged.
# This is the template needed for a future "shared settings holder"
# Quest, if the bash/weapon-shock category-setting duplication ever
# needs to become true single-source-of-truth sharing instead of
# convention-based consistency.
#
# DNAM (12 bytes) and ANAM (4 bytes) byte LAYOUT confirmed from this one
# clean example, but NOT every bit's semantic meaning - e.g. DNAM byte 0
# (0x11 in this example) is almost certainly a flags byte (this is a
# "Start Game Enabled"-style always-running system quest) but the exact
# bit-to-flag mapping isn't decoded. Safe to clone this exact structure
# wholesale (EDID + VMAD + this exact DNAM/NEXT/ANAM) for a new minimal
# settings-holder quest, same "clone what's proven, change only what
# must differ" strategy used throughout this project.
# ============================================================
QUST_MINIMAL_TEMPLATE_DNAM = bytes.fromhex('110000000000000000000000')  # 12 bytes, from SystemPowerArmorQuest
QUST_MINIMAL_TEMPLATE_NEXT = b''   # 0 bytes
QUST_MINIMAL_TEMPLATE_ANAM = bytes.fromhex('00000000')  # 4 bytes, 0 aliases


# ============================================================
# HAZD DNAM layout - AUTHORITATIVE, sourced directly from
# wbDefinitionsFO4.pas (TES5Edit/TES5Edit, line 7262).
# Cross-validated: DLC01 lightning HAZDs use flags=0x00 and
# flags=0x04 (AlignToImpact), never 0x01 or 0x05.
# Previous incorrect assumption (0x01=AffectsPlayer, 0x04=AffectsActors)
# was wrong - there is NO "affects actors" flag; all actors are always
# affected unless 0x01 restricts to player-only.
# ============================================================
HAZD_DNAM_SIZE = 52
HAZD_DNAM_LIMIT_OFFSET          = 0   # uint32
HAZD_DNAM_RADIUS_OFFSET         = 4   # float
HAZD_DNAM_LIFETIME_OFFSET       = 8   # float
HAZD_DNAM_IMAGESPACE_RADIUS_OFFSET = 12  # float
HAZD_DNAM_TARGET_INTERVAL_OFFSET = 16  # float, default 0.3
HAZD_DNAM_FLAGS_OFFSET          = 20  # uint32
HAZD_DNAM_EFFECT_FORMID_OFFSET  = 24  # FormID: SPEL/ENCH/NULL
HAZD_DNAM_LIGHT_FORMID_OFFSET   = 28  # FormID: LIGH/NULL
HAZD_DNAM_IMPACT_DATASET_OFFSET = 32  # FormID: IPDS/NULL
HAZD_DNAM_SOUND_FORMID_OFFSET   = 36  # FormID: SNDR/NULL
HAZD_DNAM_TAPER_RADIUS_OFFSET   = 40  # float (Full Effect Radius)
HAZD_DNAM_TAPER_WEIGHT_OFFSET   = 44  # float
HAZD_DNAM_TAPER_CURSE_OFFSET    = 48  # float

# HAZD flags (DNAM offset 20) - from wbDefinitionsFO4.pas
HAZD_FLAG_AFFECTS_PLAYER_ONLY       = 0x01  # restricts to player; 0x00 = all actors
HAZD_FLAG_INHERIT_DURATION          = 0x02
HAZD_FLAG_ALIGN_TO_IMPACT_NORMAL    = 0x04
HAZD_FLAG_INHERIT_RADIUS            = 0x08
HAZD_FLAG_DROP_TO_GROUND            = 0x10
HAZD_FLAG_TAPER_BY_PROXIMITY        = 0x20


def build_hazd_dnam(limit=0, radius=128.0, lifetime=10.0, imagespace_radius=0.0,
                    target_interval=0.3, flags=0, effect_formid=0,
                    light_formid=0, impact_dataset_formid=0, sound_formid=0,
                    taper_radius=0.0, taper_weight=0.0, taper_curse=0.0):
    """Build a 52-byte HAZD DNAM subrecord body. Layout from wbDefinitionsFO4.pas."""
    d = bytearray(HAZD_DNAM_SIZE)
    struct.pack_into('<I', d, HAZD_DNAM_LIMIT_OFFSET,           limit)
    struct.pack_into('<f', d, HAZD_DNAM_RADIUS_OFFSET,          radius)
    struct.pack_into('<f', d, HAZD_DNAM_LIFETIME_OFFSET,        lifetime)
    struct.pack_into('<f', d, HAZD_DNAM_IMAGESPACE_RADIUS_OFFSET, imagespace_radius)
    struct.pack_into('<f', d, HAZD_DNAM_TARGET_INTERVAL_OFFSET, target_interval)
    struct.pack_into('<I', d, HAZD_DNAM_FLAGS_OFFSET,           flags)
    struct.pack_into('<I', d, HAZD_DNAM_EFFECT_FORMID_OFFSET,   effect_formid)
    struct.pack_into('<I', d, HAZD_DNAM_LIGHT_FORMID_OFFSET,    light_formid)
    struct.pack_into('<I', d, HAZD_DNAM_IMPACT_DATASET_OFFSET,  impact_dataset_formid)
    struct.pack_into('<I', d, HAZD_DNAM_SOUND_FORMID_OFFSET,    sound_formid)
    struct.pack_into('<f', d, HAZD_DNAM_TAPER_RADIUS_OFFSET,    taper_radius)
    struct.pack_into('<f', d, HAZD_DNAM_TAPER_WEIGHT_OFFSET,    taper_weight)
    struct.pack_into('<f', d, HAZD_DNAM_TAPER_CURSE_OFFSET,     taper_curse)
    return bytes(d)


# ============================================================
# MGEF DATA layout - AUTHORITATIVE offsets from wbDefinitionsFO4.pas,
# confirmed against empirical byte analysis done earlier this project.
# ============================================================
MGEF_DATA_SIZE = 152
MGEF_DATA_FLAGS_OFFSET          = 0    # uint32
MGEF_DATA_BASE_COST_OFFSET      = 4    # float
MGEF_DATA_ASSOC_ITEM_OFFSET     = 8    # FormID (depends on archetype)
MGEF_DATA_MAGIC_SKILL_OFFSET    = 12   # int32
MGEF_DATA_RESIST_AV_OFFSET      = 16   # int32 (ActorValue)
MGEF_DATA_COUNTER_EFFECT_COUNT_OFFSET = 20  # uint16
MGEF_DATA_CASTING_LIGHT_OFFSET  = 22   # uint16 (pad)
MGEF_DATA_TAPER_WEIGHT_OFFSET   = 24   # float  (NOT HitShader - confirmed)
MGEF_DATA_HIT_SHADER_OFFSET     = 32   # FormID: EFSH - confirmed via FormID resolution
MGEF_DATA_ENCHANT_SHADER_OFFSET = 36   # FormID: EFSH (no nonzero example seen)
MGEF_DATA_MIN_SKILL_LEVEL_OFFSET = 40  # int32
MGEF_DATA_SPELLMAKING_AREA_OFFSET = 44 # int32
MGEF_DATA_SPELLMAKING_CAST_TIME_OFFSET = 48  # float
MGEF_DATA_TAPER_CURVE_OFFSET    = 52   # float
MGEF_DATA_TAPER_DURATION_OFFSET = 56   # float
MGEF_DATA_SECOND_AV_WEIGHT_OFFSET = 60 # float
MGEF_DATA_ARCHETYPE_OFFSET      = 64   # uint32 - CONFIRMED (was wrongly 48 earlier)
MGEF_DATA_PRIMARY_AV_OFFSET     = 68   # uint32 (ActorValue)
MGEF_DATA_PROJECTILE_OFFSET     = 72   # FormID: PROJ
MGEF_DATA_EXPLOSION_OFFSET      = 76   # FormID: EXPL
MGEF_DATA_CASTINGTYPE_OFFSET    = 80   # uint32: 0=ConstantEffect,1=FireForget,2=Concentration,3=Scroll
MGEF_DATA_DELIVERY_OFFSET       = 84   # uint32: 0=Self,1=Contact,2=Aimed,3=TargetActor,4=TargetLocation
MGEF_DATA_SECOND_AV_OFFSET      = 88   # uint32 (ActorValue)
MGEF_DATA_CASTING_ART_OFFSET    = 92   # FormID: ARTO
MGEF_DATA_HIT_EFFECT_ART_OFFSET = 96   # FormID: ARTO
MGEF_DATA_IMPACT_DATA_OFFSET    = 100  # FormID: IPDS
MGEF_DATA_SKILL_USAGE_MULT_OFFSET = 104 # float
MGEF_DATA_DUAL_CAST_ART_OFFSET  = 108  # FormID: DUAL
MGEF_DATA_DUAL_CAST_SCALE_OFFSET = 112 # float
MGEF_DATA_ENCHANT_ART_OFFSET    = 116  # FormID: ARTO
MGEF_DATA_HIT_VISUALS_OFFSET    = 120  # FormID: RFCT
MGEF_DATA_ENCHANT_VISUALS_OFFSET = 124 # FormID: RFCT
MGEF_DATA_EQUIP_ABILITY_OFFSET  = 128  # FormID: SPEL
MGEF_DATA_IMAGESPACE_MOD_OFFSET = 132  # FormID: IMAD
MGEF_DATA_PERK_OFFSET           = 136  # FormID: PERK
MGEF_DATA_CASTING_SOUND_LEVEL_OFFSET = 140  # uint32
MGEF_DATA_SCRIPT_AI_SCORE_OFFSET = 144 # float
MGEF_DATA_SCRIPT_AI_DELAY_OFFSET = 148 # float


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


def sub_any(tag, content):
    """A subrecord of ANY length, using the XXXX escape when it does not fit
    the u16 length field.

    esp_lib.sub() packs the length as u16 and raises struct.error past 65535.
    That is not theoretical: the Commonwealth WRLD record's OFST subrecord is
    148,996 bytes, and the first attempt to emit a WRLD override died on it.

    The escape, as the master itself encodes it and as fo4esm.subrecords()
    reads it back: a subrecord tagged XXXX whose 4-byte content is the u32
    real length, immediately followed by the real subrecord with its own u16
    length field written as 0.
    """
    if isinstance(tag, bytes):
        tag = tag.decode('ascii')
    if len(content) <= 0xFFFF:
        return sub(tag, content)
    return (b'XXXX' + struct.pack('<H', 4) + struct.pack('<I', len(content))
            + tag.encode('ascii') + struct.pack('<H', 0) + bytes(content))


def serialize_subrecords(subs):
    """[(tag, content), ...] -> bytes, XXXX-safe."""
    return b''.join(sub_any(t, c) for t, c in subs)


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

    blob = serialize_subrecords(out)
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
