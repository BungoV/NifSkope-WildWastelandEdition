#!/usr/bin/env python
"""lane TESTDRIVER1: send live-driver steps to a running NifSkope (--port PORT, WW_LIVE_SERVE=1) and print each reply.

    python ww_live.py PORT "set WW_CELL_GI=0" "shot E:/out/a.png" "unset WW_CELL_GI"
    python ww_live.py PORT --file steps.txt
Steps are the WW_LIVE_STEPS lines (row/set/unset/apply/wait/settle/shot/path/gpums/status/quit). A step that redoes
a stage (shaders, a reopen) can take minutes: the reply wait is --timeout seconds (default 900).
"""
import argparse
import socket
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('port', type=int)
    ap.add_argument('steps', nargs='*')
    ap.add_argument('--file')
    ap.add_argument('--timeout', type=float, default=900.0)
    a = ap.parse_args()
    steps = list(a.steps)
    if a.file:
        steps += [l.strip() for l in open(a.file, encoding='utf-8') if l.strip() and not l.startswith('#')]
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(('127.0.0.1', 0))
    s.settimeout(a.timeout)
    bad = 0
    for st in steps:
        s.sendto(('NifSkope::live ' + st).encode('utf-16-le'), ('127.0.0.1', a.port))
        try:
            data, _ = s.recvfrom(65536)
        except socket.timeout:
            print('%s -> NO REPLY in %d s' % (st, a.timeout))
            return 2
        reply = data.decode('utf-16-le', 'replace')
        print(reply)
        if ' -> refused' in reply:
            bad += 1
        if st == 'quit':
            break
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
