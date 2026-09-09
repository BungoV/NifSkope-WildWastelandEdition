#!/usr/bin/env python
"""BUILD1: render_shot.sh could not build its dirty fixture.

`-no-gui set -f Name -v Cube_L1` is refused: a BSTriShape's Name is a
tStringIndex and NifValue::setFromString parses that as a NUMBER
(src/data/nifvalue.cpp:715), so the CLI can only address the header string
table by index and cannot introduce a new string. `-no-gui get -f Name`
prints that index too ("1"), so the two fixture assertions were reading a
number and comparing it against a `_L1` suffix.

Minimal fix, harness-side only (no C++ change, no rebuild): the shape is
renamed by rewriting its entry in the header string table -- the file is
still derived from the application's own `new --cube`, nothing is
hand-authored -- and both names are read back out of `-no-gui list`, which
prints the resolved string.

LF-only file; the CR count must stay 0.
"""
import io
import os

P = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                 '..', '..', 'tests', 'spells', 'render_shot.sh')
P = os.path.normpath(P)

b = open(P, 'rb').read()
cr_before = b.count(b'\r')
assert cr_before == 0, cr_before

OLD = (b'nget() { cli get -b "$1" -f "$2" "$3" | tail -1; }\n')
NEW = (b'nget() { cli get -b "$1" -f "$2" "$3" | tail -1; }\n'
       b'# A block\'s Name is a tStringIndex: `get -f Name` prints the INDEX, not the\n'
       b'# string, and `set -f Name -v <text>` is refused for the same reason\n'
       b'# (NifValue::setFromString parses tStringIndex as a number). The resolved\n'
       b'# name is what `list` prints, so that is what is read here.\n'
       b'bname() { cli list "$2" | grep "^\\[$1\\]" | sed "s/.*\'\\(.*\\)\'.*/\\1/"; }\n')
assert b.count(OLD) == 1, b.count(OLD)
b = b.replace(OLD, NEW)

OLD2 = (b'SNAME=$(nget "$SHAPE" "Name" "$PLAIN")\n'
        b'cli set -b "$SHAPE" -f "Name" -v "${SNAME}_L1" -o "$(winpath "$LODF")" "$PLAIN" '
        b'>/dev/null 2>&1\n'
        b'[ -s "$LODF" ] || { echo "FAIL  could not rename the shape"; exit 1; }\n')
NEW2 = (b'SNAME=$(bname "$SHAPE" "$PLAIN")\n'
        b'[ -n "$SNAME" ] || { echo "FAIL  could not read the shape name"; exit 1; }\n'
        b'# The rename goes through the header string table because the CLI cannot\n'
        b'# introduce a new string (see bname above). The bytes still come from the\n'
        b'# app\'s own `new --cube`; only the one length-prefixed entry moves.\n'
        b'"$PY" - "$(winpath "$PLAIN")" "$(winpath "$LODF")" "$SNAME" <<\'RENEOF\'\n'
        b'import struct, sys\n'
        b'src, dst, name = sys.argv[1], sys.argv[2], sys.argv[3]\n'
        b'b = open(src, "rb").read()\n'
        b'old = struct.pack("<I", len(name)) + name.encode()\n'
        b'new = struct.pack("<I", len(name) + 3) + (name + "_L1").encode()\n'
        b'if b.count(old) != 1:\n'
        b'    sys.stderr.write("string table: %d matches for %r\\n" % (b.count(old), name))\n'
        b'    sys.exit(1)\n'
        b'open(dst, "wb").write(b.replace(old, new))\n'
        b'RENEOF\n'
        b'[ -s "$LODF" ] || { echo "FAIL  could not rename the shape"; exit 1; }\n')
assert b.count(OLD2) == 1, b.count(OLD2)
b = b.replace(OLD2, NEW2)

OLD3 = (b'PLAIN_NAME=$(nget "$SHAPE" "Name" "$PLAIN")\n'
        b'LOD_NAME=$(nget "$SHAPE" "Name" "$LODF")\n')
NEW3 = (b'PLAIN_NAME=$(bname "$SHAPE" "$PLAIN")\n'
        b'LOD_NAME=$(bname "$SHAPE" "$LODF")\n')
assert b.count(OLD3) == 1, b.count(OLD3)
b = b.replace(OLD3, NEW3)

# PY is used by the rename above and the script never defined it.
OLD4 = b'PORT="${WW_RENDER_SHOT_PORT:-42327}"\n'
NEW4 = (b'PORT="${WW_RENDER_SHOT_PORT:-42327}"\n'
        b'PY="${PY:-$(command -v python || echo /c/Windows/py)}"\n')
assert b.count(OLD4) == 1, b.count(OLD4)
b = b.replace(OLD4, NEW4)

assert b.count(b'\r') == 0
open(P, 'wb').write(b)
print('patched %s, %d bytes, CR %d' % (P, len(b), b.count(b'\r')))
