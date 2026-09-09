import sys, struct, os

for p in sys.argv[1:]:
    with open(p, 'rb') as f:
        b = f.read(148)
    magic = b[0:4]
    h = struct.unpack('<31I', b[4:128])
    # h[0]=size h[1]=flags h[2]=height h[3]=width h[4]=pitch h[5]=depth h[6]=mipcount
    pf = struct.unpack('<8I', b[76:108])
    fourcc = b[84:88]
    print(os.path.basename(p), 'w', h[3], 'h', h[2], 'mips', h[6],
          'pfflags', hex(pf[1]), 'fourCC', fourcc, 'bytes', os.path.getsize(p))
    if fourcc == b'DX10':
        dx = struct.unpack('<5I', b[128:148])
        print('   dxgi', dx[0], 'dim', dx[1], 'arraySize', dx[3])
