#!/usr/bin/env python
"""IMPOSTORFIX1 fix05: `$PY` must be the interpreter that was FOUND, not a name
resolved again later against a PATH the script itself changed.

Step 0 picks `python` off PATH. Sixty lines later step 2 prepends
`/c/msys64/ucrt64/bin` so that g++ can be found -- and MSYS2 ships its own
`python.exe` there, which shadows the one step 0 measured. Every `$PY` after
that line runs a DIFFERENT interpreter than the one the row announced.

It stayed invisible while the rows only needed the standard library. Step 14
imports numpy, and the gate reported
`ModuleNotFoundError: No module named 'numpy'` against an interpreter that has
numpy 1.26.0 -- a row failing for a reason that is not in the thing it tests.

`command -v` already returns the absolute path; keep it.
"""
import io

P = 'tests/spells/impostor_draw.sh'
b = io.open(P, 'rb').read()
cr = b.count(b'\r')
assert cr == 0, cr

OLD = b'''	if command -v "$c" >/dev/null 2>&1; then PY="$c"; break; fi
'''
NEW = b'''	# THE ABSOLUTE PATH, not the name. Step 2 below prepends
	# /c/msys64/ucrt64/bin to PATH so g++ can be found, and MSYS2 ships its
	# own python.exe there; a bare "python" would be resolved again after
	# that line and quietly become a different interpreter with a different
	# set of modules. Step 14 found this by failing on a missing numpy that
	# the announced interpreter has.
	if command -v "$c" >/dev/null 2>&1; then PY="$( command -v "$c" )"; break; fi
'''
assert b.count(OLD) == 1, b.count(OLD)
b = b.replace(OLD, NEW)

assert b.count(b'\r') == cr
io.open(P, 'wb').write(b)
print('fix05 applied, %d bytes, CR=%d' % (len(b), b.count(b'\r')))
