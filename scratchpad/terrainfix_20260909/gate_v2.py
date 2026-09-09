import struct, sys
b = open(sys.argv[1], 'rb').read()
fails = []


def check(name, cond):
    print("  %s %s" % ("ok  " if cond else "FAIL", name))
    if not cond:
        fails.append(name)


check("the written header says version 2", struct.unpack_from('<I', b, 4)[0] == 2)
off = struct.unpack_from('<QQQQQQQQQQ', b, 0x48)
check("the first section starts at 0xA0, so the header really grew",
      off[0] == 0xA0)
dh = struct.unpack_from('<f', b, 0x98)[0]
dt = struct.unpack_from('<I', b, 0x9C)[0]
print("  file says default water height %g type %08x" % (dh, dt))
# TELEMETRY ECHOES TRUTH: the census line is compared against the BYTES, not
# trusted as a statement about them.
sh, st = sys.argv[3], sys.argv[4]
check("the file's default water height is what the writer reported (%s)" % sh,
      abs(dh - float(sh)) < 1e-3)
check("the file's default water form is what the writer reported (%s)" % st,
      ("%08x" % dt) == st.lower())
check("the Commonwealth has a default water type (WRLD NAM2)", dt != 0)

# the water plane's two cases must BOTH occur, or 0xFFFF is untested
minX, minY, maxX, maxY = struct.unpack_from('<iiii', b, 8)
watrN = struct.unpack_from('<I', b, 0x34)[0]
cellsX, cellsY = maxX - minX + 1, maxY - minY + 1
oCell = off[4]
inherit = explicit = nowater = 0
bad = 0
for i in range(cellsX * cellsY):
    wh, wt, fl = struct.unpack_from('<fHH', b, oCell + i * 16 + 8)
    if not (fl & 1):
        nowater += 1
        continue
    if wt == 0xFFFF:
        inherit += 1
    else:
        explicit += 1
        if wt >= watrN:
            bad += 1
print("  water cells: %d inherit the worldspace type, %d name their own, "
      "%d have no water" % (inherit, explicit, nowater))
check("cells that INHERIT the worldspace water exist", inherit > 0)
check("cells that name their OWN water type exist", explicit > 0)
check("every explicit water type indexes the WATR table (%d entries)" % watrN,
      bad == 0)

# the version 1 fallback, exact at its off value
try:
    v1 = open(sys.argv[2], 'rb').read()
except IOError:
    v1 = b''
if v1:
    o1 = struct.unpack_from('<QQQQQQQQQQ', v1, 0x48)
    check("version 1 still writes a 152-byte header (first section at 0x98)",
          struct.unpack_from('<I', v1, 4)[0] == 1 and o1[0] == 0x98)
    check("every section offset moved by exactly the 8 bytes appended",
          all(off[k] == o1[k] + 8 for k in range(10)))
    check("and NOTHING ELSE moved: the two files are identical past the header",
          b[0xA0:] == v1[0x98:])
else:
    check("the version 1 fallback file exists", False)
print("RESULT %s" % ("PASS" if not fails else "FAIL"))
sys.exit(1 if fails else 0)
