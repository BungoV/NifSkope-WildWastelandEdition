# Lane LODIV7. Two TEST-SIDE repairs that the version bump owes its neighbours.
# Both are measured failures from tests/spells/lodgen_native.sh run under G5:
#   j0          "the .lodi at 3, 4, 5 or 6 (4 / 7)"      -- the list never learned 7
#   (lodi-wrap) "refused, but the message never says ..." -- the doctored file was
#               refused for headerCrc32 instead, because the re-sign covers the
#               256-byte header and a v7 header BLOCK is 512 bytes. A control that
#               is refused for the wrong reason is not a control.
import io

# ---- 1. the version list ----------------------------------------------------
p = 'tests/spells/lodgen_native_fields.py'
s = open(p, encoding='utf-8', newline='').read()
O = ("    ck.check('j0 the .lodo is at version 4 and the .lodi at 3, 4, 5 or 6 (%d / %d)'\n"
     "             % (h['version'], ih['version']), h['version'] == 4 and ih['version'] in (3, 4, 5, 6))")
N = ("    ck.check('j0 the .lodo is at version 4 and the .lodi at 3, 4, 5, 6 or 7 (%d / %d)'\n"
     "             % (h['version'], ih['version']),\n"
     "             h['version'] == 4 and ih['version'] in (3, 4, 5, 6, 7))")
assert s.count(O) == 1, s.count(O)
s = s.replace(O, N)
open(p, 'w', encoding='utf-8', newline='').write(s)

# ---- 2. the header re-sign, which has to know how big the header is ---------
p2 = 'tests/spells/lodgen_native_mutate.py'
m = open(p2, encoding='utf-8', newline='').read()
O2 = """def resign_header_lodi(b):
    put(b, 0x0C, 'I', crc32(bytes(b[0x10:0x100])))"""
N2 = '''def resign_header_lodi(b):
    """headerCrc32 covers the header BLOCK, and v7 made that block 512 bytes
    (docs s4.9). Signing 256 of a 512-byte header leaves the file refused for a
    CRC mismatch, which is not the refusal any of these cases is testing for --
    it is a control that goes red for the wrong reason, which is worse than one
    that does not go red at all."""
    hdr = 0x200 if get(b, 0x04, 'I')[0] >= 7 else 0x100
    put(b, 0x0C, 'I', crc32(bytes(b[0x10:hdr])))'''
assert m.count(O2) == 1, m.count(O2)
m = m.replace(O2, N2)
open(p2, 'w', encoding='utf-8', newline='').write(m)

for f in (p, p2):
    d = open(f, 'rb').read()
    print('%s  CRLF %d  bytes %d' % (f, d.count(b'\r\n'), len(d)))
