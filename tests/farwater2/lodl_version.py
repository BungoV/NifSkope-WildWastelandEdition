"""Lane FARWATER2: one line about a .lodl -- the header version word and the body count (own read, 0x04 / 0xA8)."""
import struct
import sys

b = open(sys.argv[1], 'rb').read(0x100)
ver = struct.unpack_from('<I', b, 4)[0]
n_body = struct.unpack_from('<I', b, 0xA8)[0]
print(f'  {sys.argv[1]}: version {ver}, {n_body} water bodies')
