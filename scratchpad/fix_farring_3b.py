"""FARRING1 step 3b: the single --objects path takes --slot-fallback too, so
the flag means the same thing on both ways into the chunk builder."""

P = 'src/nifcli.cpp'
b = open(P, 'rb').read()
assert b.count(b'\r') == 0
s = b.decode('utf-8')

A = ("\t\topts.impostorDir = impostors;\n"
     "\t\topts.impostorFromLevel = impostorFromLevel;\n"
     "\t\topts.dataRoot = dataRoot.isEmpty()\n")
n = s.count(A)
assert n == 1, 'anchor matched %d times' % n
s = s.replace(A, ("\t\topts.impostorDir = impostors;\n"
                  "\t\topts.impostorFromLevel = impostorFromLevel;\n"
                  "\t\topts.slotFallback = slotFallback;\n"
                  "\t\topts.dataRoot = dataRoot.isEmpty()\n"))

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('nifcli.cpp: %d -> %d bytes' % (len(b), len(out)))
