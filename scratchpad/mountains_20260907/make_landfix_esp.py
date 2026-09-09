"""make_landfix_esp.py - build a Fallout 4 plugin of LAND texture overrides.

Lane ESPWRITE. Takes an assignment file (the RECOVER lane's `recovered.txt`)
saying which LTEX belongs in which quadrant of which Commonwealth cell, and
emits a valid .esp of LAND record overrides carrying those assignments, so a
LOD generator (xLODGen, DynDOLOD, or ours) can bake far terrain from it.

    python make_landfix_esp.py --recovered recovered.txt --out LandFix.esp

    # develop / prove the machinery with one texture everywhere:
    python make_landfix_esp.py --placeholder 00021336 --ring 24 --out test.esp

    # measure the size question without writing anything:
    python make_landfix_esp.py --placeholder 00021336 --all --stats-only


THE RULE THIS WHOLE TOOL EXISTS TO OBEY
---------------------------------------
A Bethesda record override REPLACES THE WHOLE RECORD. You cannot ship "just
the BTXT". So every LAND emitted here starts as the master's own decompressed
payload and has ONLY its BTXT subrecords touched: VNML, VHGT, VCLR, ATXT,
VTXT, DATA, MPCD and anything unrecognised are copied through byte for byte,
in their original order. Likewise every CELL is re-emitted from the master's
raw on-disk record bytes without being parsed at all, which is what protects
XCRI (the precombined-refs list, present on 4,160 Commonwealth cells) from
being dropped and costing the player his frame rate.

test_roundtrip.py is the gate for that claim and must pass before this tool is
believed. It rebuilds all 36 Commonwealth exterior block groups from parsed
structure and compares 215,518,075 bytes against the master's own, then runs
the identity splice over all 36,864 LAND payloads.


THE ASSIGNMENT FILE
-------------------
One cell per line; blank lines and lines starting with '#' ignored.

    <cellX> <cellY> <q0> <q1> <q2> <q3>      four quadrant textures
    <cellX> <cellY> <ltex>                   the same texture in all four

Each quadrant field is:
    8 hex digits  a FormID, master-relative (00xxxxxx = Fallout4.esm)
    '-'           leave this quadrant exactly as the master has it
    '0'           delete this quadrant's base texture

Quadrants are 0 BL, 1 BR, 2 TL, 3 TR (docs/LODGEN_ESM_LAYOUTS.md).

FormID note: Fallout4.esm declares no masters of its own, so every FormID
inside it is 00xxxxxx. With Fallout4.esm as this plugin's master #0, a
verbatim payload copy needs NO FormID remapping - 00xxxxxx still means
Fallout4.esm. The tool asserts the master really has zero MAST entries rather
than trusting that.
"""

from __future__ import print_function

import argparse
import collections
import os
import struct
import sys
import time

import fo4esm as E
import esp_lib_land as L
import ba2strings

DEFAULT_ESM = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm'
DEFAULT_DATA = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data'
COMMONWEALTH = 0x0000003C


# ==========================================================================
# reading the master
# ==========================================================================

class CellEntry(object):
    __slots__ = ('x', 'y', 'formid', 'cell_bytes', 'land_bytes', 'land_payload',
                 'land_formid', 'land_tail', 'land_flags',
                 'cc_tail', 'ct_tail')

    def __init__(self):
        self.land_bytes = None
        self.land_payload = None
        self.cc_tail = b'\x00' * 8
        self.ct_tail = b'\x00' * 8


class Master(object):
    """Everything the writer needs out of Fallout4.esm, read once."""

    def __init__(self, path, worldspace=COMMONWEALTH, verbose=True):
        self.path = path
        self.worldspace = worldspace
        self.buf = E.load(path)
        if verbose:
            print('master %s (%s bytes)' % (path, '{:,}'.format(len(self.buf))))

        # No masters of its own => every FormID inside is 00xxxxxx and a
        # verbatim copy needs no remapping. Assert rather than assume.
        sig, dsize, flags, formid, tail = E.read_record_header(self.buf, 0)
        assert sig == b'TES4', 'not a plugin: %r' % (sig,)
        self.tes4_payload = E.record_payload(self.buf, 0, dsize, flags)
        masts = [c for t, c in E.subrecords(self.tes4_payload) if t == b'MAST']
        assert not masts, (
            'the master declares %d masters of its own; FormIDs inside it are '
            'not all 00xxxxxx and a verbatim payload copy would be wrong' % len(masts))

        self.top_tail = None
        self.wc_tail = None
        self.wrld_record = None          # (dsize, flags, tail, payload)
        self.block_tail = {}             # (bx,by) -> 8 bytes
        self.subblock_tail = {}          # (sx,sy) -> 8 bytes
        self.cells = {}                  # (x,y) -> CellEntry
        self._scan(verbose)

    def _scan(self, verbose):
        buf = self.buf
        top = None
        for node in E.top_level_groups(buf):
            if node.label == b'WRLD' and node.gtype == E.GT_TOP:
                top = node
                break
        assert top is not None, 'no top-level WRLD group'
        self.top_tail = top.tail

        # the WRLD record itself
        off, end = top.offset + 24, top.offset + top.gsize
        wc = None
        while off + 24 <= end:
            if buf[off:off + 4] == b'GRUP':
                sig, gsize, label, gtype, tail = E.read_group_header(buf, off)
                if (gtype == E.GT_WORLD_CHILDREN
                        and struct.unpack('<I', label)[0] == self.worldspace):
                    wc = E.Node(off, gsize, label, gtype, tail)
                    break
                off += gsize
                continue
            sig, dsize, flags, formid, tail = E.read_record_header(buf, off)
            if sig == b'WRLD' and formid == self.worldspace:
                self.wrld_record = (dsize, flags, tail,
                                    E.record_payload(buf, off, dsize, flags))
            off += 24 + dsize
        assert wc is not None, 'World Children group for 0x%08X not found' % self.worldspace
        assert self.wrld_record is not None, 'WRLD record 0x%08X not found' % self.worldspace
        self.wc_tail = wc.tail
        self.wc = wc

        cur = {'cell': None}
        stack_tails = {}

        def on_grp(node, stack):
            if node.gtype == E.GT_EXTERIOR_BLOCK:
                y, x = struct.unpack('<hh', node.label)
                self.block_tail[(x, y)] = node.tail
            elif node.gtype == E.GT_EXTERIOR_SUBBLOCK:
                y, x = struct.unpack('<hh', node.label)
                self.subblock_tail[(x, y)] = node.tail
            elif node.gtype == E.GT_CELL_CHILDREN:
                stack_tails['cc'] = node.tail
            elif node.gtype == E.GT_CELL_TEMPORARY:
                stack_tails['ct'] = node.tail

        def on_rec(off, sig, dsize, flags, formid, tail, stack):
            if sig == b'CELL' and stack[-1].gtype == E.GT_EXTERIOR_SUBBLOCK:
                payload = E.record_payload(buf, off, dsize, flags)
                xy = None
                for t, c in E.subrecords(payload):
                    if t == b'XCLC' and len(c) >= 8:
                        xy = struct.unpack_from('<ii', c, 0)
                        break
                assert xy is not None, 'CELL %08X has no XCLC' % formid
                ent = CellEntry()
                ent.x, ent.y = xy
                ent.formid = formid
                # raw on-disk record bytes, header included, never parsed again
                ent.cell_bytes = bytes(buf[off:off + 24 + dsize])
                self.cells[xy] = ent
                cur['cell'] = ent
            elif sig == b'LAND':
                ent = cur['cell']
                if ent is None:
                    return
                ent.land_formid = formid
                ent.land_tail = tail
                ent.land_flags = flags
                ent.land_bytes = bytes(buf[off:off + 24 + dsize])
                ent.land_payload = E.record_payload(buf, off, dsize, flags)
                ent.cc_tail = stack_tails.get('cc', b'\x00' * 8)
                ent.ct_tail = stack_tails.get('ct', b'\x00' * 8)

        E.walk(buf, wc.offset + 24, wc.offset + wc.gsize, [wc], on_rec, on_grp)
        n_land = sum(1 for e in self.cells.values() if e.land_bytes)
        if verbose:
            print('  %d cells, %d with a LAND, %d blocks, %d sub-blocks'
                  % (len(self.cells), n_land, len(self.block_tail),
                     len(self.subblock_tail)))


# ==========================================================================
# assignments
# ==========================================================================

def parse_formid_field(tok):
    """'-' -> None (leave alone); '0' -> 0 (delete); else an 8-hex FormID."""
    if tok == '-':
        return None
    v = int(tok, 16)
    return v


def _parse_recover_quad(field):
    """RECOVER lane's quadrant field: 'q0=<ltex>[:<weight>][;<ltex>:<weight>...]'.

    The header of their recovered.txt documents it as q0=<ltex:w;...>, a
    weighted candidate list. The base texture can only be one LTEX, so take
    the first (highest-weight) candidate. '-' and '0' keep the meanings this
    tool's own simpler format gives them.
    """
    val = field.split('=', 1)[1] if '=' in field else field
    if val in ('-', ''):
        return None
    first = val.split(';')[0]
    tok = first.split(':')[0]
    if tok == '-':
        return None
    return int(tok, 16)


def read_assignments(path, min_conf=None):
    """-> ({(x,y): {quadrant: formid-or-0}}, info dict)

    Accepts two formats, decided per line:

    (a) this tool's own, terse:
            <cellX> <cellY> <q0> <q1> <q2> <q3>
            <cellX> <cellY> <ltex>

    (b) the RECOVER lane's, as its recovered.txt actually writes it:
            R <cx> <cy> q0=<ltex>[:w][;...] q1=... q2=... q3=... conf=<0..1>

    RECOVER's own header says its far-field accuracy collapses with distance
    from painted terrain and warns against shipping the file as a material
    recovery. That is their call to make, not this tool's - but the `conf`
    column is there to be used, so --min-conf is honoured here and the number
    of cells it drops is reported.
    """
    out = {}
    info = {'lines': 0, 'dropped_conf': 0, 'format_a': 0, 'format_b': 0,
            'conf_seen': False}
    with open(path, 'r') as f:
        for lineno, line in enumerate(f, 1):
            line = line.split('#', 1)[0].strip()
            if not line:
                continue
            parts = line.split()
            info['lines'] += 1
            try:
                if parts[0] == 'R':
                    # RECOVER format
                    info['format_b'] += 1
                    x, y = int(parts[1]), int(parts[2])
                    quads = {}
                    conf = None
                    for tok in parts[3:]:
                        if tok.startswith('conf='):
                            info['conf_seen'] = True
                            conf = float(tok.split('=', 1)[1])
                        elif len(tok) > 2 and tok[0] == 'q' and tok[1].isdigit():
                            q = int(tok[1])
                            v = _parse_recover_quad(tok)
                            if v is not None:
                                quads[q] = v
                    if min_conf is not None and (conf is None or conf < min_conf):
                        info['dropped_conf'] += 1
                        continue
                elif len(parts) == 3:
                    info['format_a'] += 1
                    x, y = int(parts[0]), int(parts[1])
                    v = parse_formid_field(parts[2])
                    quads = {} if v is None else {q: v for q in range(4)}
                elif len(parts) == 6:
                    info['format_a'] += 1
                    x, y = int(parts[0]), int(parts[1])
                    quads = {}
                    for q in range(4):
                        v = parse_formid_field(parts[2 + q])
                        if v is not None:
                            quads[q] = v
                else:
                    raise ValueError('unrecognised line: expected "R x y q0=... ", '
                                     '3 fields or 6 fields, got %d' % len(parts))
            except (ValueError, IndexError) as exc:
                raise SystemExit('%s:%d: %s' % (path, lineno, exc))
            if quads:
                out.setdefault((x, y), {}).update(quads)
    return out, info


def land_relief(payload):
    """max - min terrain height in game units, from VHGT.

    Decode per src/esmdata.cpp:334 and docs/LODGEN_ESM_LAYOUTS.md: float base,
    then 33x33 SIGNED byte deltas, column 0 of each row offsetting the PREVIOUS
    row's column 0, everything x8. Returns None if the LAND has no VHGT.

    Why the writer cares: 18,890 of the 32,909 untextured Commonwealth cells
    are dead flat (relief exactly 0), so a texture assignment on them shows the
    player nothing while costing plugin bytes. Everything with real relief sits
    inside cell ring +-80.
    """
    for t, c in E.subrecords(payload):
        if t != b'VHGT' or len(c) < 4 + 33 * 33:
            continue
        base = struct.unpack_from('<f', c, 0)[0]
        d = struct.unpack_from('<1089b', c, 4)
        lo = hi = None
        row_start = base
        for row in range(33):
            row_start += d[row * 33]
            v = row_start
            for col in range(33):
                if col:
                    v += d[row * 33 + col]
                if lo is None or v < lo:
                    lo = v
                if hi is None or v > hi:
                    hi = v
        return (hi - lo) * 8.0
    return None


def filter_by_relief(master, assignments, min_relief):
    """Drop assignments on cells whose terrain is flatter than min_relief."""
    if not min_relief:
        return assignments, 0
    out = {}
    dropped = 0
    for xy, quads in assignments.items():
        ent = master.cells.get(xy)
        if ent is None or ent.land_bytes is None:
            continue
        rel = land_relief(ent.land_payload)
        if rel is None or rel < min_relief:
            dropped += 1
            continue
        out[xy] = quads
    return out, dropped


def synth_assignments(master, ltex, ring=None, box=None, only_untextured=True):
    """A placeholder assignment for development and for the size measurement:
    one LTEX in all four quadrants of every selected cell."""
    out = {}
    for (x, y), ent in master.cells.items():
        if ent.land_bytes is None:
            continue
        if ring is not None and (abs(x) > ring or abs(y) > ring):
            continue
        if box is not None and not (box[0] <= x <= box[2] and box[1] <= y <= box[3]):
            continue
        if only_untextured:
            base, has_alpha = L.land_quadrant_state(ent.land_payload)
            if base or has_alpha:
                continue
        out[(x, y)] = {q: ltex for q in range(4)}
    return out


# ==========================================================================
# the WRLD record
# ==========================================================================

def decode_rnam(content):
    gx, gy, count = struct.unpack_from('<hhI', content, 0)
    assert len(content) == 8 + count * 8, 'bad RNAM length %d' % len(content)
    return (gx, gy), [struct.unpack_from('<Ihh', content, 8 + i * 8)
                      for i in range(count)]


def encode_rnam(grid, refs):
    return (struct.pack('<hhI', grid[0], grid[1], len(refs))
            + b''.join(struct.pack('<Ihh', *r) for r in refs))


def collect_other_wrld_rnams(data_dir, master_path, worldspace):
    """Every OTHER plugin's large-reference entries for this worldspace.

    Needed because our plugin loads last, so a WRLD record copied from the
    master alone would DELETE the large references the DLCs add. Measured:
    DLCCoast adds 167 refs and 65 grids the master does not have,
    DLCNukaWorld adds 75.
    """
    extra = collections.OrderedDict()
    sources = {}
    if not os.path.isdir(data_dir):
        return extra, sources
    for n in sorted(os.listdir(data_dir)):
        if not n.lower().endswith(('.esm', '.esp', '.esl')):
            continue
        p = os.path.join(data_dir, n)
        if n.lower() == os.path.basename(master_path).lower():
            continue
        if os.path.getsize(p) < 200:
            continue
        try:
            buf = E.load(p)
            for node in E.top_level_groups(buf):
                if node.label != b'WRLD' or node.gtype != E.GT_TOP:
                    continue
                off, end = node.offset + 24, node.offset + node.gsize
                while off + 24 <= end:
                    if buf[off:off + 4] == b'GRUP':
                        off += E.read_group_header(buf, off)[1]
                        continue
                    sig, dsize, flags, fid, tail = E.read_record_header(buf, off)
                    if sig == b'WRLD' and fid == worldspace:
                        for t, c in E.subrecords(
                                E.record_payload(buf, off, dsize, flags)):
                            if t != b'RNAM':
                                continue
                            grid, refs = decode_rnam(c)
                            extra.setdefault(grid, [])
                            for r in refs:
                                if r not in extra[grid]:
                                    extra[grid].append(r)
                                    sources.setdefault(n, 0)
                                    sources[n] += 1
                    off += 24 + dsize
        except Exception as exc:
            print('  warning: could not read %s for large references: %s' % (n, exc))
    return extra, sources


def build_wrld_payload(master, mode, data_dir, lang='en', verbose=True):
    """Return the WRLD record payload to emit, or None for 'do not emit one'.

    mode 'master'     : the master's payload verbatim, FULL rewritten inline
    mode 'merge-rnam' : as above, plus the union of every other plugin's
                        large references, so nothing in the load order is lost
    mode 'none'       : no WRLD record at all
    """
    if mode == 'none':
        return None
    dsize, flags, tail, payload = master.wrld_record
    subs = list(E.subrecords(payload))

    # --- FULL: the master is Localized, so FULL is a u32 string ID. This
    # plugin is NOT localized, so it must carry the name inline instead -
    # which is exactly what the Creation Kit does saving a non-localized esp.
    name = None
    for i, (t, c) in enumerate(subs):
        if t == b'FULL':
            sid = struct.unpack('<I', c)[0] if len(c) == 4 else None
            if sid is not None:
                name = ba2strings.lookup(sid, lang)
            assert name, ('could not resolve WRLD FULL string id 0x%08X; refusing '
                          'to emit a plugin whose worldspace name is a raw id'
                          % (sid or 0))
            subs[i] = (b'FULL', name.encode('utf-8') + b'\x00')
            if verbose:
                print('  WRLD FULL: string id 0x%08X -> %r, written inline'
                      % (sid, name))
            break

    if mode == 'merge-rnam':
        extra, sources = collect_other_wrld_rnams(data_dir, master.path,
                                                  master.worldspace)
        have = collections.OrderedDict()
        idx_of = {}
        for i, (t, c) in enumerate(subs):
            if t == b'RNAM':
                grid, refs = decode_rnam(c)
                have[grid] = refs
                idx_of[grid] = i
        added_refs = 0
        added_grids = 0
        for grid, refs in extra.items():
            if grid in have:
                cur = have[grid]
                for r in refs:
                    if r not in cur:
                        cur.append(r)
                        added_refs += 1
                subs[idx_of[grid]] = (b'RNAM', encode_rnam(grid, cur))
            else:
                added_grids += 1
                added_refs += len(refs)
                # append after the last existing RNAM so the run stays contiguous
                last = max(idx_of.values()) if idx_of else 0
                subs.insert(last + 1, (b'RNAM', encode_rnam(grid, refs)))
                idx_of = {g: (i if i <= last else i + 1) for g, i in idx_of.items()}
                idx_of[grid] = last + 1
                have[grid] = refs
        if verbose:
            print('  WRLD large references: merged in %d refs across %d new grids '
                  'from %s' % (added_refs, added_grids,
                               ', '.join('%s(%d)' % kv for kv in sorted(sources.items()))
                               or 'nothing'))
        # self-check: the result must be a superset of the master's own set
        m_refs = set()
        for t, c in E.subrecords(payload):
            if t == b'RNAM':
                g, rr = decode_rnam(c)
                m_refs |= set((g,) + tuple([r]) for r in rr)
        o_refs = set()
        for t, c in subs:
            if t == b'RNAM':
                g, rr = decode_rnam(c)
                o_refs |= set((g,) + tuple([r]) for r in rr)
        assert m_refs <= o_refs, (
            'the merged WRLD lost %d of the master\'s own large references'
            % len(m_refs - o_refs))

    # XXXX-safe: the Commonwealth WRLD's OFST subrecord is 148,996 bytes.
    out = L.serialize_subrecords(subs)
    # round trip immediately - a WRLD emitted through the XXXX escape that
    # does not read back is the kind of thing nothing downstream would notice
    back = list(E.subrecords(out))
    assert back == subs, 'the emitted WRLD payload does not read back identically'
    return out


# ==========================================================================
# emitting
# ==========================================================================

def build_plugin(master, assignments, compress_land=True, compress_cell=False,
                 wrld_mode='merge-rnam', data_dir=DEFAULT_DATA,
                 masters=('Fallout4.esm',), author='landfix',
                 description=None, esl=False, esm=False, verbose=True):
    """-> (plugin bytes, stats dict)"""
    stats = collections.Counter()

    # ---- per-cell records ------------------------------------------------
    per_cell = {}
    missing = []
    for (x, y), quads in sorted(assignments.items()):
        ent = master.cells.get((x, y))
        if ent is None or ent.land_bytes is None:
            missing.append((x, y))
            continue
        new_payload, n_ins, at = L.splice_land_base_textures(ent.land_payload, quads)
        stats['btxt_inserted'] += n_ins
        stats['btxt_replaced'] += sum(1 for q in quads if quads[q]) - n_ins
        if compress_land:
            land_rec = L.build_record_compressed(
                b'LAND', ent.land_formid, ent.land_tail, new_payload,
                ent.land_flags & ~E.COMPRESSED_FLAG)
        else:
            land_rec = L.build_record_raw(
                b'LAND', ent.land_formid, ent.land_tail, new_payload,
                ent.land_flags & ~E.COMPRESSED_FLAG)
        stats['land_payload_bytes'] += len(new_payload)
        stats['land_record_bytes'] += len(land_rec)
        # what the same record weighs in the master, so the cost of the edit
        # is a measured delta rather than a claim
        stats['land_master_bytes'] += len(ent.land_bytes)
        stats['land_master_payload_bytes'] += len(ent.land_payload)

        temp = L.build_group(E.GT_CELL_TEMPORARY,
                             struct.pack('<I', ent.formid), land_rec, ent.ct_tail)
        children = L.build_group(E.GT_CELL_CHILDREN,
                                 struct.pack('<I', ent.formid), temp, ent.cc_tail)
        # CELL: the master's raw on-disk record bytes, never re-parsed.
        cell_rec = ent.cell_bytes
        stats['cell_record_bytes'] += len(cell_rec)
        stats['cells'] += 1
        stats['groups'] += 2
        stats['records'] += 2
        per_cell[(x, y)] = (ent.formid, cell_rec + children)

    if missing and verbose:
        print('  warning: %d requested cells have no LAND in the master '
              '(first few: %s)' % (len(missing), missing[:5]))

    # ---- group into sub-blocks and blocks, in the MASTER'S order ---------
    subblocks = collections.defaultdict(list)
    for (x, y), (formid, blob) in per_cell.items():
        subblocks[L.subblock_coords(x, y)].append((formid, blob))

    blocks = collections.defaultdict(list)
    for (sx, sy), items in subblocks.items():
        # cells ascending by FormID - measured, 576 of 576 sub-blocks
        items.sort(key=lambda t: t[0])
        body = b''.join(b for _, b in items)
        label = L.exterior_label(sx, sy)
        grp = L.build_group(E.GT_EXTERIOR_SUBBLOCK, label, body,
                            master.subblock_tail.get((sx, sy), b'\x00' * 8))
        stats['groups'] += 1
        # a sub-block's block is the block of any cell in it
        bx, by = sx >> 2, sy >> 2      # 8-cell sub-blocks, 4 per block axis
        blocks[(bx, by)].append((label, grp))

    block_blobs = []
    for (bx, by), items in blocks.items():
        # sub-blocks ascending by label-as-u32 - measured
        items.sort(key=lambda t: L.label_sort_key(t[0]))
        body = b''.join(g for _, g in items)
        label = L.exterior_label(bx, by)
        grp = L.build_group(E.GT_EXTERIOR_BLOCK, label, body,
                            master.block_tail.get((bx, by), b'\x00' * 8))
        stats['groups'] += 1
        block_blobs.append((label, grp))
    block_blobs.sort(key=lambda t: L.label_sort_key(t[0]))

    world_children = L.build_group(E.GT_WORLD_CHILDREN,
                                   struct.pack('<I', master.worldspace),
                                   b''.join(g for _, g in block_blobs),
                                   master.wc_tail)
    stats['groups'] += 1

    # ---- the WRLD record -------------------------------------------------
    wrld_payload = build_wrld_payload(master, wrld_mode, data_dir, verbose=verbose)
    top_body = b''
    if wrld_payload is not None:
        _, wflags, wtail, _ = master.wrld_record
        wrld_rec = L.build_record_raw(b'WRLD', master.worldspace, wtail,
                                      wrld_payload, wflags & ~E.COMPRESSED_FLAG)
        top_body += wrld_rec
        stats['records'] += 1
        stats['wrld_record_bytes'] = len(wrld_rec)
    top_body += world_children
    top = L.build_group(E.GT_TOP, b'WRLD', top_body, master.top_tail)
    stats['groups'] += 1

    # ---- TES4 ------------------------------------------------------------
    flags = 0
    if esm:
        flags |= L.TES4_FLAG_ESM
    if esl:
        flags |= L.TES4_FLAG_ESL
    # deliberately NOT TES4_FLAG_LOCALIZED: measured, 0 of 36,864 Commonwealth
    # CELLs carry a localizable subrecord, and the one WRLD FULL is written
    # inline above.
    n_records = stats['records']
    n_groups = stats['groups']
    tes4, _ = L.build_tes4(list(masters), n_records + n_groups,
                           author=author, description=description)
    tes4 = tes4[:8] + struct.pack('<i', flags) + tes4[12:]

    plugin = tes4 + top
    stats['plugin_bytes'] = len(plugin)
    stats['hedr_num_records'] = n_records + n_groups
    return plugin, stats


# ==========================================================================
# verification of what we just built
# ==========================================================================

def verify_plugin(blob, master, assignments, wrld_mode, verbose=True):
    """Re-read the emitted plugin with the same reader used on the master and
    check it against the master and the assignments. Every failure raises.

    This is not a substitute for xEdit or the game - see the report's Open
    Risks - but it does catch every structural mistake this tool could make.
    """
    problems = []
    buf = blob

    sig, dsize, flags, formid, tail = E.read_record_header(buf, 0)
    assert sig == b'TES4', 'emitted file does not start with TES4'
    tes4 = E.record_payload(buf, 0, dsize, flags)
    hedr = None
    masts = []
    for t, c in E.subrecords(tes4):
        if t == b'HEDR':
            hedr = struct.unpack('<fiI', c[:12])
        elif t == b'MAST':
            masts.append(c.rstrip(b'\x00').decode('latin1'))
    assert masts and masts[0].lower() == 'fallout4.esm', (
        'Fallout4.esm must be master #0 or the copied 00xxxxxx FormIDs are wrong; '
        'got %r' % (masts,))
    assert not (flags & L.TES4_FLAG_LOCALIZED), (
        'the Localized flag is set but no strings table ships with this plugin')

    # O(1) lookups; a linear scan per record would be O(n^2) at 32,909 cells.
    by_cell_formid = {e.formid: e for e in master.cells.values()}
    by_land_formid = {e.land_formid: e for e in master.cells.values()
                      if e.land_bytes is not None}

    n_rec = [0]
    n_grp = [0]
    seen = {}
    paths = collections.Counter()
    cell_bytes_ok = [0]
    cell_bytes_bad = []
    cur = {'cell': None}

    def on_grp(node, stack):
        n_grp[0] += 1

    def on_rec(off, sig, dsize, flags, formid, tail, stack):
        n_rec[0] += 1
        if sig == b'CELL':
            cur['cell'] = formid
            raw = bytes(buf[off:off + 24 + dsize])
            ent = by_cell_formid.get(formid)
            if ent is None or raw != ent.cell_bytes:
                cell_bytes_bad.append(formid)
            else:
                cell_bytes_ok[0] += 1
        elif sig == b'LAND':
            paths[tuple(n.gtype for n in stack)] += 1
            # list(stack), not stack: the walker mutates the same list object
            # as it unwinds, so a stored reference would be empty by now.
            seen[formid] = (E.record_payload(buf, off, dsize, flags),
                            tuple(n.gtype for n in stack), list(stack))

    for node in E.top_level_groups(buf):
        n_grp[0] += 1
        E.walk(buf, node.offset + 24, node.offset + node.gsize, [node], on_rec, on_grp)

    if hedr[1] != n_rec[0] + n_grp[0]:
        problems.append('HEDR numRecords %d != records+GRUPs %d'
                        % (hedr[1], n_rec[0] + n_grp[0]))

    want_path = (E.GT_TOP, E.GT_WORLD_CHILDREN, E.GT_EXTERIOR_BLOCK,
                 E.GT_EXTERIOR_SUBBLOCK, E.GT_CELL_CHILDREN, E.GT_CELL_TEMPORARY)
    for p, n in paths.items():
        if p != want_path:
            problems.append('%d LAND records sit on the wrong group path %s' % (n, p))

    if cell_bytes_bad:
        problems.append('%d CELL records are not byte-identical to the master '
                        '(first: %08X)' % (len(cell_bytes_bad), cell_bytes_bad[0]))

    # every requested cell present, and its LAND is the master's payload with
    # exactly the requested BTXT change and nothing else
    nchecked = 0
    for (x, y), quads in assignments.items():
        ent = master.cells.get((x, y))
        if ent is None or ent.land_bytes is None:
            continue
        got = seen.get(ent.land_formid)
        if got is None:
            problems.append('cell (%d,%d) LAND %08X missing from the plugin'
                            % (x, y, ent.land_formid))
            continue
        expect, _n_ins, _at = L.splice_land_base_textures(ent.land_payload, quads)
        if got[0] != expect:
            problems.append('cell (%d,%d) LAND %08X payload differs from the '
                            'expected splice' % (x, y, ent.land_formid))
            continue
        # and the non-BTXT subrecords are exactly the master's
        a = [s for s in E.subrecords(ent.land_payload) if s[0] != b'BTXT']
        b = [s for s in E.subrecords(got[0]) if s[0] != b'BTXT']
        if a != b:
            problems.append('cell (%d,%d) LAND %08X: a non-BTXT subrecord was '
                            'disturbed' % (x, y, ent.land_formid))
            continue
        base, _ = L.land_quadrant_state(got[0])
        for q, want in quads.items():
            if want and base.get(q) != want:
                problems.append('cell (%d,%d) quadrant %d is %s, expected %08X'
                                % (x, y, q, base.get(q), want))
        nchecked += 1

    # block / sub-block membership
    for formid, (payload, path, stack) in seen.items():
        ent = by_land_formid.get(formid)
        if ent is None:
            problems.append('LAND %08X is not a Commonwealth LAND' % formid)
            continue
        blk = next(n for n in stack if n.gtype == E.GT_EXTERIOR_BLOCK)
        sub_ = next(n for n in stack if n.gtype == E.GT_EXTERIOR_SUBBLOCK)
        by, bx = struct.unpack('<hh', blk.label)
        sy, sx = struct.unpack('<hh', sub_.label)
        if (bx, by) != L.block_coords(ent.x, ent.y):
            problems.append('cell (%d,%d) is in block (%d,%d), should be (%d,%d)'
                            % (ent.x, ent.y, bx, by) + str(L.block_coords(ent.x, ent.y)))
        if (sx, sy) != L.subblock_coords(ent.x, ent.y):
            problems.append('cell (%d,%d) is in sub-block (%d,%d), should be %s'
                            % (ent.x, ent.y, sx, sy, L.subblock_coords(ent.x, ent.y)))

    if verbose:
        print('  verify: %d records, %d GRUPs, HEDR=%d' % (n_rec[0], n_grp[0], hedr[1]))
        print('  verify: %d CELL records byte-identical to the master' % cell_bytes_ok[0])
        print('  verify: %d LAND records checked against the expected splice' % nchecked)
        print('  verify: every LAND on the path %s'
              % ' -> '.join(E.GT_NAME[g] for g in want_path))
    if problems:
        for p in problems[:20]:
            print('  VERIFY FAILED: %s' % p)
        raise SystemExit('verification failed with %d problems' % len(problems))
    return {'records': n_rec[0], 'groups': n_grp[0]}


# ==========================================================================
# main
# ==========================================================================

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--esm', default=DEFAULT_ESM, help='the master to read')
    ap.add_argument('--data', default=DEFAULT_DATA,
                    help='Data folder, scanned for other plugins large references')
    ap.add_argument('--recovered', help='assignment file from the RECOVER lane')
    ap.add_argument('--placeholder', metavar='HEX',
                    help='development mode: this LTEX FormID in every quadrant '
                         'of every selected cell')
    ap.add_argument('--ring', type=int, help='with --placeholder: |x|,|y| <= N')
    ap.add_argument('--box', help='with --placeholder: x0,y0,x1,y1')
    ap.add_argument('--all', action='store_true',
                    help='with --placeholder: every untextured cell')
    ap.add_argument('--include-textured', action='store_true',
                    help='with --placeholder: also cells that already have textures')
    ap.add_argument('--out', help='output .esp path')
    ap.add_argument('--no-compress', action='store_true',
                    help='emit LAND records uncompressed')
    ap.add_argument('--wrld', choices=('merge-rnam', 'master', 'none'),
                    default='merge-rnam',
                    help='how to emit the WRLD record (default merge-rnam)')
    ap.add_argument('--esl-flag', action='store_true', help='set the ESL flag (0x200)')
    ap.add_argument('--esm-flag', action='store_true', help='set the ESM flag (0x001)')
    ap.add_argument('--author', default='landfix')
    ap.add_argument('--stats-only', action='store_true',
                    help='build in memory and report sizes, write nothing')
    ap.add_argument('--min-relief', type=float, default=0.0, metavar='UNITS',
                    help='skip cells whose terrain relief (max-min VHGT height, '
                         'game units) is below this. 18,890 of the 32,909 '
                         'untextured Commonwealth cells are dead flat.')
    ap.add_argument('--min-conf', type=float, default=None, metavar='0..1',
                    help='with --recovered: skip cells whose conf column is '
                         'below this. RECOVER\'s own header reports its far-field '
                         'top-1 accuracy at 0.8%% beyond 16 cells from painted '
                         'terrain, so this is the knob that decides what ships.')
    ap.add_argument('--no-verify', action='store_true')
    args = ap.parse_args(argv)

    if not args.recovered and not args.placeholder:
        ap.error('give --recovered or --placeholder')
    if not args.out and not args.stats_only:
        ap.error('give --out or --stats-only')

    t0 = time.time()
    master = Master(args.esm)

    if args.recovered:
        assignments, info = read_assignments(args.recovered, args.min_conf)
        print('assignments: %d cells from %s' % (len(assignments), args.recovered))
        print('  %d lines read (%d in RECOVER format, %d in the terse format)'
              % (info['lines'], info['format_b'], info['format_a']))
        if args.min_conf is not None:
            if not info['conf_seen']:
                print('  warning: --min-conf %g given but no line carried a conf '
                      'field; nothing was filtered on confidence'
                      % args.min_conf)
            else:
                print('  --min-conf %g dropped %d cells'
                      % (args.min_conf, info['dropped_conf']))
    else:
        box = None
        if args.box:
            box = tuple(int(v) for v in args.box.split(','))
        assignments = synth_assignments(master, int(args.placeholder, 16),
                                        ring=args.ring, box=box,
                                        only_untextured=not args.include_textured)
        print('assignments: %d cells, placeholder LTEX %s'
              % (len(assignments), args.placeholder))

    if args.min_relief:
        assignments, dropped = filter_by_relief(master, assignments, args.min_relief)
        print('  --min-relief %g dropped %d flat cells, %d remain'
              % (args.min_relief, dropped, len(assignments)))

    blob, stats = build_plugin(
        master, assignments,
        compress_land=not args.no_compress,
        wrld_mode=args.wrld, data_dir=args.data,
        author=args.author, esl=args.esl_flag, esm=args.esm_flag)

    print('built: %s bytes  (%d cells, %d records, %d GRUPs)'
          % ('{:,}'.format(len(blob)), stats['cells'], stats['records'],
             stats['groups']))
    print('  LAND records %s bytes, CELL records %s bytes, WRLD record %s bytes'
          % ('{:,}'.format(stats['land_record_bytes']),
             '{:,}'.format(stats['cell_record_bytes']),
             '{:,}'.format(stats.get('wrld_record_bytes', 0))))
    print('  BTXT inserted %d, replaced %d'
          % (stats['btxt_inserted'], stats['btxt_replaced']))
    print('  the same LAND records weigh %s bytes in the master '
          '(payload %s -> %s, +%s)'
          % ('{:,}'.format(stats['land_master_bytes']),
             '{:,}'.format(stats['land_master_payload_bytes']),
             '{:,}'.format(stats['land_payload_bytes']),
             '{:,}'.format(stats['land_payload_bytes']
                           - stats['land_master_payload_bytes'])))
    if stats['cells']:
        print('  per cell: %.1f bytes total, %.1f bytes of LAND record'
              % (float(len(blob)) / stats['cells'],
                 float(stats['land_record_bytes']) / stats['cells']))
        print('  per cell without the one-off WRLD record: %.1f bytes'
              % (float(len(blob) - stats.get('wrld_record_bytes', 0))
                 / stats['cells']))

    if not args.no_verify:
        verify_plugin(blob, master, assignments, args.wrld)
        print('  VERIFY OK')

    if args.out and not args.stats_only:
        outp = os.path.abspath(args.out)
        with open(outp, 'wb') as f:
            f.write(blob)
        print('wrote %s (%s bytes)' % (outp, '{:,}'.format(len(blob))))

    print('done in %.1f s' % (time.time() - t0))
    return 0


if __name__ == '__main__':
    sys.exit(main())
