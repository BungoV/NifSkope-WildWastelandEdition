"""Read or set hknpBodyCinfo::flags -- the word that carries
RAISE_CONTACT_IMPULSE_EVENTS -- in a NIF's compiled collision.

    python tools/hkbodyflags.py <file.nif> ...            report
    python tools/hkbodyflags.py --set <file.nif> ...      patch dynamic bodies

WHY THIS EXISTS

Fallout 4 only plays a collision impact sound for a body that RAISES the
contact-impulse event, and that is a bit in hknpBody::m_flags:

    FOCollisionListener::OnContactImpulseEvent   1.10.155 @0x630c60
        matA = bhkUtilFunctions::GetMaterialForShape(bodyA->m_shape, keyA)
        matB = ... bodyB ...
        BGSImpactManager::ProcessEvent({matA, matB, position, velocity})

    hknpBody::initialize                         1.10.155 @0x14daaf0
        body->m_flags = (cinfo->flags & ~0xF) | 0x400
        (initializeStaticBody / initializeDynamicBody then OR in IS_STATIC /
         IS_DYNAMIC, which is why the low four bits are dropped -- every OTHER
         bit of cinfo.flags reaches the live body verbatim)

    NVFlex::printHknpBodyInfo                    1.10.155 @0x27afa4
        the engine's own debug printer, naming each bit:
          0x0001 IS_STATIC   0x0002 IS_DYNAMIC   0x0004 IS_KEYFRAMED
          0x0008 IS_ACTIVE   0x0010 RAISE_TRIGGER_EVENTS
          0x0020 RAISE_MANIFOLD_STATUS_EVENTS
          0x0040 RAISE_MANIFOLD_PROCESSED_EVENTS
          0x0080 RAISE_CONTACT_IMPULSE_EVENTS      <-- this one
          0x0100 DONT_COLLIDE      0x0200 DONT_BUILD_CONTACT_JACOBIANS
          0x0400 TEMP_REBUILD_COLLISION_CACHES     (engine always sets it)
          0x4000 IS_NON_RUNTIME    0x8000 IS_BREAKABLE
          0x10000..0x80000 USER_FLAG_0..3          0x100000 ENABLE_RESTITUTION
          0x200000 ENABLE_TRIGGER_MODIFIER  0x400000 ENABLE_IMPULSE_CLIPPING
          0x800000 ENABLE_MASS_CHANGER  0x1000000 ENABLE_SOFT_CONTACTS
          0x2000000 ENABLE_SURFACE_VELOCITY  0x4000000.. USER_FLAG_4..6

MEASURED ON VANILLA (13,889 bodies in 11,820 files under Meshes\Interiors,
SetDressing and Furniture) cinfo.flags takes exactly four values:

    0x00000000  12,456   every static (11,305) and every keyframed (1,151)
    0x00000080   1,060   dynamic
    0x00010080     348   dynamic
    0x00000010      25   the statics of PrydwenDestruction.nif

So RAISE_CONTACT_IMPULSE_EVENTS is set on 1,408 of 1,408 dynamic bodies and on
none of the 12,456 that are not -- derivable, exactly. USER_FLAG_0 is NOT:
it leans towards destructibles (287 of its 348 are *Dest.nif) but 56 non-Dest
layer-4 bodies carry it and 745 do not, so it has to be carried, not computed.

WHAT IS CHECKED

  * a dynamic body (one with a motionProperties entry) has
    RAISE_CONTACT_IMPULSE_EVENTS
  * a static or keyframed body has flags 0, which is what all 12,456 vanilla
    ones carry
  * no body carries a bit outside {RAISE_CONTACT_IMPULSE_EVENTS, USER_FLAG_0,
    RAISE_TRIGGER_EVENTS} -- the only three the 13,889-body corpus uses

USER_FLAG_0 is ALLOWED but never REQUIRED: it is real data that has to be
carried through a round trip, not something a checker can derive. Requiring it
would fail every plain dynamic prop -- Kickball01, Shovel01, AlarmClock all
carry 0x80 alone -- and demanding it is how a check starts lying.

Exit 0 if every body holds up, 1 if any does not, 2 if the file has no bodies.

`--set` repairs rather than stamps: a dynamic body gets
RAISE_CONTACT_IMPULSE_EVENTS plus whatever USER_FLAG_0 it already had, and a
static or keyframed one gets 0. The word is patched in place inside the
packfile's __data__ section, so no size and no fixup change. Writes a .bak
beside the file the first time.
"""
import os
import shutil
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hkmatrun import nif_blobs, Pack

RAISE_TRIGGER_EVENTS = 0x00000010
RAISE_CONTACT_IMPULSE_EVENTS = 0x00000080
USER_FLAG_0 = 0x00010000
# carried through the round trip rather than derived from anything modelled
CARRIED = USER_FLAG_0 | RAISE_TRIGGER_EVENTS
KNOWN = RAISE_CONTACT_IMPULSE_EVENTS | CARRIED


def wanted(kind, value):
    """What this body's flags SHOULD be, given what it already carries.

    Only RAISE_CONTACT_IMPULSE_EVENTS is derived. The carried bits are preserved
    where they are found and never demanded where they are not -- requiring
    USER_FLAG_0 would fail every plain dynamic prop, and requiring
    RAISE_TRIGGER_EVENTS would fail 11,305 of 11,330 statics.
    """
    derived = RAISE_CONTACT_IMPULSE_EVENTS if kind == 'dynamic' else 0
    return derived | (value & CARRIED)

BITS = [
    (0x00000001, 'IS_STATIC'), (0x00000002, 'IS_DYNAMIC'),
    (0x00000004, 'IS_KEYFRAMED'), (0x00000008, 'IS_ACTIVE'),
    (0x00000010, 'RAISE_TRIGGER_EVENTS'),
    (0x00000020, 'RAISE_MANIFOLD_STATUS_EVENTS'),
    (0x00000040, 'RAISE_MANIFOLD_PROCESSED_EVENTS'),
    (0x00000080, 'RAISE_CONTACT_IMPULSE_EVENTS'),
    (0x00000100, 'DONT_COLLIDE'), (0x00000200, 'DONT_BUILD_CONTACT_JACOBIANS'),
    (0x00004000, 'IS_NON_RUNTIME'), (0x00008000, 'IS_BREAKABLE'),
    (0x00010000, 'USER_FLAG_0'), (0x00020000, 'USER_FLAG_1'),
    (0x00040000, 'USER_FLAG_2'), (0x00080000, 'USER_FLAG_3'),
    (0x00100000, 'ENABLE_RESTITUTION'), (0x00200000, 'ENABLE_TRIGGER_MODIFIER'),
    (0x00400000, 'ENABLE_IMPULSE_CLIPPING'), (0x00800000, 'ENABLE_MASS_CHANGER'),
    (0x01000000, 'ENABLE_SOFT_CONTACTS'), (0x02000000, 'ENABLE_SURFACE_VELOCITY'),
]


def decode(value):
    names = [n for b, n in BITS if value & b]
    rest = value & ~sum(b for b, _ in BITS)
    if rest:
        names.append('unknown %#x' % rest)
    return ', '.join(names) or 'none'


def bodies(pk):
    """(cinfoOffset, kind) for every body, kind in static/keyframed/dynamic."""
    out = []
    for psd in pk.of_class('hknpPhysicsSystemData'):
        cin = pk.local.get(psd + 0x40)
        if cin is None:
            continue
        motions = pk.local.get(psd + 0x30)
        nmotions = pk.u32(psd + 0x38)
        for k in range(pk.u32(psd + 0x48)):
            off = cin + k * 0x60
            idx = pk.u32(off + 0x0c)
            if idx == 0x7fffffff:
                kind = 'static'
            elif motions is not None and idx < nmotions:
                # motionPropertiesId, the first u16 of the hknpMotionCinfo.
                # 0xffff means "no motionProperties record" -- the KEYFRAMED
                # state, a door, and vanilla leaves those flags at 0 too.
                mp = struct.unpack_from('<H', pk.data, motions + idx * 0x70)[0]
                kind = 'keyframed' if mp == 0xffff else 'dynamic'
            else:
                kind = 'no-motion-cinfo'
            out.append((k, off, kind))
    return out


def run(path, write, quiet=False):
    """(violations, bodies, patched) for one file."""
    raw = open(path, 'rb').read()
    edits = []
    lines = []
    bad = 0
    for _, at, blob in nif_blobs(path):
        try:
            pk = Pack(blob)
        except ValueError:
            continue
        for k, off, kind in bodies(pk):
            value = pk.u32(off + 0x18)
            want = wanted(kind, value)
            ok = (value == want) and not (value & ~KNOWN)
            mark = '' if ok else '   <- want %#010x' % want
            lines.append('  body %d %-10s flags=%#010x [%s]%s'
                         % (k, kind, value, decode(value), mark))
            if not ok:
                bad += 1
            if write and value != want:
                edits.append((at + pk.base + off + 0x18, want))
    if not quiet or bad:
        print(os.path.basename(path))
        for line in lines:
            print(line)
    if not lines:
        return 0, 0, 0
    if not edits:
        return bad, len(lines), 0
    if not os.path.exists(path + '.bak'):
        shutil.copy2(path, path + '.bak')
    data = bytearray(raw)
    for at, want in edits:
        struct.pack_into('<I', data, at, want)
    with open(path, 'wb') as fh:
        fh.write(bytes(data))
    print('  patched %d bodies' % len(edits))
    return 0, len(lines), len(edits)


def main(argv):
    write = '--set' in argv
    quiet = '--quiet' in argv
    paths = [a for a in argv if not a.startswith('--')]
    if not paths:
        print(__doc__)
        return 2
    bad = seen = patched = 0
    for p in paths:
        b, n, w = run(p, write, quiet)
        bad += b
        seen += n
        patched += w
    if write and not quiet:
        print('patched %d bodies in total' % patched)
    if not seen:
        return 2
    if bad:
        if not quiet:
            print('%d of %d bodies carry the wrong flags' % (bad, seen))
        return 1
    if not quiet:
        print('%d bodies, all flags as vanilla writes them' % seen)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
