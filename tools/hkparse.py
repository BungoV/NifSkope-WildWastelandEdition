import struct, sys

blob = open(sys.argv[1], 'rb').read()

# hkxHeader
magic0, magic1 = struct.unpack_from('<II', blob, 0)
assert magic0 == 0x57E0E057 and magic1 == 0x10C0C010, "not a Havok packfile"
userTag, fileVersion = struct.unpack_from('<II', blob, 8)
layout = blob[16:20]  # bytesInPointer, littleEndian, reusePaddingOpt, emptyBaseClassOpt
numSections, = struct.unpack_from('<i', blob, 20)
contentsSection, contentsOffset = struct.unpack_from('<ii', blob, 24)
print(f"packfile v{fileVersion} ptr{layout[0]*8} sections={numSections} contents@sec{contentsSection}+{contentsOffset}")
ver = blob[0x28:0x38].split(b'\x00')[0].decode()
print(f"sdk {ver}")

# section headers: 64 bytes each, start at 0x40 for v11 (flags+pad after version)
SECHDR = 0x40
sections = []
for s in range(numSections):
    off = SECHDR + s * 0x40
    tag = blob[off:off+19].split(b'\x00')[0].decode()
    absStart, localFix, globalFix, virtualFix, exports, imports, end = struct.unpack_from('<7i', blob, off+20)
    sections.append((tag, absStart, localFix, globalFix, virtualFix, exports, imports, end))
    print(f"section {s}: {tag:16} data[{absStart:#x}..+{localFix:#x}) local@{localFix:#x} global@{globalFix:#x} virt@{virtualFix:#x} end@{end:#x}")

# class names section: [u32 tag][u8 sig?][name\0]... actually [u32 crc][byte 0x09][name\0]
cn = sections[0]
cnames = {}
p = cn[1]
endp = cn[1] + cn[2]
while p < endp - 5:
    crc, = struct.unpack_from('<I', blob, p)
    if blob[p+4] != 0x09:
        break
    e = blob.index(b'\x00', p+5)
    # offset used by fixups = offset of the NAME within the section
    cnames[p + 5 - cn[1]] = blob[p+5:e].decode()
    p = e + 1
print("classnames:", cnames)

# data section
dt = [s for s in sections if s[0] == '__data__'][0]
base = dt[1]
data_len = dt[2]  # local fixup offset == end of payload
def rel(x): return base + x

# virtual fixups: (dataOffset, sectionIndex, classNameOffset)
virt = []
p = rel(dt[4])
while p + 12 <= min(rel(dt[5]), len(blob)):
    src, sec, cno = struct.unpack_from('<iii', blob, p)
    p += 12
    if src == -1:
        continue
    virt.append((src, cnames.get(cno, f'?{cno}')))
print("objects:")
for src, cls in virt:
    print(f"  @{src:#06x} {cls}")

# local fixups: (srcOffset, dstOffset) — pointer patch table
local = []
p = rel(dt[2])
while p + 8 <= min(rel(dt[3]), len(blob)):
    src, dst = struct.unpack_from('<ii', blob, p)
    p += 8
    if src != -1:
        local.append((src, dst))
print(f"local fixups: {len(local)}")
for s, d in local:
    print(f"  ptr@{s:#06x} -> {d:#06x}")

# dump object payloads as float32 rows
if len(sys.argv) > 2 and sys.argv[2] == '-d':
    virt_sorted = sorted(virt) + [(data_len, 'END')]
    for i in range(len(virt_sorted)-1):
        src, cls = virt_sorted[i]
        nxt = virt_sorted[i+1][0]
        print(f"--- {cls} @{src:#x} ({nxt-src} bytes)")
        chunk = blob[rel(src):rel(nxt)]
        for r in range(0, min(len(chunk), 4096), 16):
            hexs = ' '.join(f'{b:02x}' for b in chunk[r:r+16])
            fl = ''
            if r+16 <= len(chunk):
                fl = ' '.join(f'{v:.5g}' for v in struct.unpack_from('<4f', chunk, r))
            print(f"  {src+r:5x}  {hexs:<48}  {fl}")
