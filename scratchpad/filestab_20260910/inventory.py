#!/usr/bin/env python3
"""Inventory: every tr("...") string containing "nif" (any case) in the two
source files that build the left-column NIFs page and its dock."""
import re
import sys

STR = re.compile(r'tr\(\s*"((?:[^"\\]|\\.)*)"')
NIF = re.compile(r'nif', re.I)

for f in ['src/nifskope.cpp', 'src/nifskope_ui.cpp']:
    lines = open(f, encoding='utf-8', errors='replace').read().split('\n')
    for i, l in enumerate(lines, 1):
        for m in STR.finditer(l):
            s = m.group(1)
            if NIF.search(s):
                print('%s:%d: "%s"' % (f, i, s))
