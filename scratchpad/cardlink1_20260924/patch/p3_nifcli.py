"""CARDLINK1 patch 3: src/nifcli.cpp -- move the native block after the card arrays, add the link call."""
import sys
WT = 'E:/Projects/NifskopeWWE-cardlink1'
P = WT + '/scratchpad/cardlink1_20260924/patch/'
path = WT + '/src/nifcli.cpp'
with open(path, 'rb') as f:
    b = f.read()
cr0 = b.count(b'\r')
START = b'\t\tif ( lodgenNativeActive() ) {\n\t\t\tif ( !nativeDir.isEmpty() && gLgNativeCache ) {\n'
END = b'\t\t\tlodgenNativeEnd();\n\t\t}\n\t\t/* WHERE THE OBJECT SHEETS GO, once,'
DEST = b'\t\t/* ===== THE SCRATCH TEARDOWN (lane BTOFREE1, 2026-09-16) ============\n'
for a in (START, END, DEST):
    c = b.count(a)
    if c != 1:
        sys.exit('anchor count %d for %r' % (c, a[:70]))
i = b.index(START)
j = b.index(END) + len(b'\t\t\tlodgenNativeEnd();\n\t\t}\n')
block = b[i:j]
rest = b[:i] + b[j:]
old_call = b'\t\t\tQString nrep, nerr;\n\t\t\tbool nativeOk = false;\n'
if block.count(old_call) != 1:
    sys.exit('link-call anchor not once in the block')
with open(P + 's_linkcall.txt', 'rb') as f:
    call = f.read().replace(b'\r\n', b'\n')
block = block.replace(old_call, call)
if rest.count(DEST) != 1:
    sys.exit('dest anchor lost')
k = rest.index(DEST)
out = rest[:k] + block + rest[k:]
if out.count(b'\r') != cr0:
    sys.exit('CR count moved')
with open(path, 'wb') as f:
    f.write(out)
print('moved native block (%d bytes) and added the link call' % len(block))
