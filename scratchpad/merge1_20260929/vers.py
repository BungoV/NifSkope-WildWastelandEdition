import struct, glob, sys
d = sys.argv[1]
for f in sorted(glob.glob(d + '/*.lodl') + glob.glob(d + '/*.lodi') + glob.glob(d + '/*.lodt')):
    b = open(f, 'rb').read(16)
    print(f.replace(chr(92), '/').rsplit('/', 1)[-1], b[:4], struct.unpack('<I', b[4:8])[0])
