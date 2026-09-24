import sys
for p in sys.argv[1:]:
    b = open(p, 'rb').read()
    crlf = b.count(b'\r\n')
    lf = b.count(b'\n') - crlf
    print(f"{p}: crlf={crlf} lf={lf} bytes={len(b)}")
