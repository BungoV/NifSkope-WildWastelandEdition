#!/usr/bin/env python3
"""PERF1 step 5 refuter -- write a minimal, VALID FO4 plugin.

    python mkesp.py <out.esp> <author string>

One TES4 record, one master (Fallout4.esm), no other records, so the plugin
contributes NOTHING to the object census or the VHGT corpus and the only hash it
can move is `loadOrderHash`, which folds each plugin's lower-cased file NAME and
its byte SIZE. Changing the author string by one character changes the size;
changing it to another string of the SAME length changes bytes but not size,
which is the hole the lane's report states outright.
"""
import struct
import sys


def field(sig, data):
    return sig + struct.pack('<H', len(data)) + data


def main(out, author):
    hedr = struct.pack('<fiI', 0.95, 0, 0x800)
    body = field(b'HEDR', hedr)
    body += field(b'CNAM', author.encode('ascii') + b'\0')
    body += field(b'MAST', b'Fallout4.esm\0')
    body += field(b'DATA', struct.pack('<Q', 0))
    rec = b'TES4' + struct.pack('<IIII', len(body), 0, 0, 0) + struct.pack('<HH', 131, 0) + body
    with open(out, 'wb') as fh:
        fh.write(rec)
    print('%s  %d bytes  author=%r' % (out, len(rec), author))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
