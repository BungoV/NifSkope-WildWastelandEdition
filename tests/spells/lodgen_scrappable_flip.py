#!/usr/bin/env python
"""The RED CONTROL for the workshop-scrappable bit.

A count that agrees with a rule proves nothing unless it can disagree. This
copies a `.lodi`, flips bit 6 of ONE instance's flags word in the copy, and
reads the count back out of both files. If the count does not move, the count
was never reading those bytes.

It does not open the plugin and it does not call NifSkope: one file in, one file
out, and the only things it knows are the 24-byte instance record's flags word
at offset 0x14 (`docs/LODGEN_NATIVE_LODO_LODI.md` s4.1) and where the three
CRC32 words live.

**THE CRCs ARE PART OF THE CONTROL.** A `.lodi` carries three integrity words
in a chain, and a single flipped flag byte breaks all three, so the file REFUSES
to open rather than opening and reading differently -- which is the right
behaviour and is itself worth proving:

  1. the owning CHUNK's CRC32 (0x18 of its 32-byte record) over that chunk's
     instance records followed by its cold records;
  2. `indexCrc32` (header 0x64) over the chunk table, the cell ranges, the
     occluder tables and every v5..v8 stream, in that order -- the chunk table
     is in it, so repairing (1) breaks (2);
  3. `headerCrc32` (header 0x0C) over 0x10 to the end of the header -- which
     holds `indexCrc32`, so repairing (2) breaks (3).

The blob (2) covers is taken from the DECODER's own `indexRanges`, never from a
second copy of the rule. The script repairs all three and requires that the
bytes which moved are exactly the flag byte and those three words: thirteen
bytes in four named windows. A flip that needed no CRC repair would mean the
CRCs do not cover the flags.

USAGE
  python tests/spells/lodgen_scrappable_flip.py <in.lodi> <out.lodi> [--index N]

EXIT
  0  only the flag byte and the three CRC words moved, and the count moved by one
  1  it did not
  2  an input is missing
"""
import os
import shutil
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import lodgen_native_decode as D  # noqa: E402

LODI_INST_SCRAPPABLE = 64       # bit 6, src/lodifile.h
FLAGS_IN_RECORD = 0x14          # u16 at 0x14 of the 24-byte instance record
CRC_IN_CHUNK = 0x18             # u32 at 0x18 of the 32-byte chunk record
OFF_INDEX_CRC = 0x64            # u32, header: over the chunk table and the streams
OFF_HEADER_CRC = 0x0C           # u32, header: over 0x10 .. the end of the header


def count(path):
	T = D.read_lodi(path)
	return sum(1 for r in T['instances'] if int(r['flags']) & LODI_INST_SCRAPPABLE), T


def main():
	args = [a for a in sys.argv[1:] if not a.startswith('--')]
	if len(args) < 2:
		sys.stderr.write(__doc__)
		return 2
	src, dst = args[0], args[1]
	idx = None
	if '--index' in sys.argv:
		idx = int(sys.argv[sys.argv.index('--index') + 1])
	if not os.path.isfile(src):
		print('SKIP: missing %s' % src)
		return 2

	before, T = count(src)
	h = T['header']
	if idx is None:
		# the first instance the file says IS scrappable, so the flip CLEARS a
		# real one; if the file has none, flip the first instance instead
		idx = next((i for i, r in enumerate(T['instances'])
					if int(r['flags']) & LODI_INST_SCRAPPABLE), 0)
	off = h['offInstances'] + idx * 24 + FLAGS_IN_RECORD

	# the chunk that owns this instance: ids and CRCs are PER CHUNK
	ci = None
	for k, c in enumerate(T['chunks']):
		if c['instanceCount'] and c['instanceFirst'] <= idx < c['instanceFirst'] + c['instanceCount']:
			ci = k
			break
	if ci is None:
		print('FAIL: no chunk claims instance %d' % idx)
		return 1
	c = T['chunks'][ci]

	shutil.copyfile(src, dst)
	b = bytearray(open(dst, 'rb').read())
	b[off] ^= LODI_INST_SCRAPPABLE

	# 1. the owning chunk's CRC32, computed the way the reader computes it
	s, e = c['instanceFirst'], c['instanceFirst'] + c['instanceCount']
	rec = bytes(b[h['offInstances'] + s * 24:h['offInstances'] + e * 24])
	cold = bytes(b[h['offCold'] + s * 8:h['offCold'] + e * 8])
	chunkCrc = zlib.crc32(cold, zlib.crc32(rec)) & 0xFFFFFFFF
	coff = h['offChunks'] + ci * 32 + CRC_IN_CHUNK
	oldChunk = int.from_bytes(bytes(b[coff:coff + 4]), 'little')
	b[coff:coff + 4] = chunkCrc.to_bytes(4, 'little')

	# 2. indexCrc32, over the blob the DECODER says it covers
	blob = b''.join(bytes(b[o:o + n]) for o, n in T['indexRanges'])
	indexCrc = zlib.crc32(blob) & 0xFFFFFFFF
	oldIndex = int.from_bytes(bytes(b[OFF_INDEX_CRC:OFF_INDEX_CRC + 4]), 'little')
	b[OFF_INDEX_CRC:OFF_INDEX_CRC + 4] = indexCrc.to_bytes(4, 'little')

	# 3. headerCrc32, over 0x10 .. the end of the header block
	hdr = T['headerBytes']
	headerCrc = zlib.crc32(bytes(b[0x10:hdr])) & 0xFFFFFFFF
	oldHeader = int.from_bytes(bytes(b[OFF_HEADER_CRC:OFF_HEADER_CRC + 4]), 'little')
	b[OFF_HEADER_CRC:OFF_HEADER_CRC + 4] = headerCrc.to_bytes(4, 'little')

	open(dst, 'wb').write(bytes(b))

	after, _ = count(dst)
	a = open(src, 'rb').read()
	d = open(dst, 'rb').read()
	moved = [i for i in range(len(a)) if a[i] != d[i]]
	allowed = (set([off]) | set(range(coff, coff + 4))
			   | set(range(OFF_INDEX_CRC, OFF_INDEX_CRC + 4))
			   | set(range(OFF_HEADER_CRC, OFF_HEADER_CRC + 4)))

	print('instance %d in chunk %d, flags byte at 0x%X' % (idx, ci, off))
	print('  bytes changed : %d %s' % (len(moved), ['0x%X' % m for m in moved]))
	print('  chunk  crc    : %08X -> %08X at 0x%X' % (oldChunk, chunkCrc, coff))
	print('  index  crc    : %08X -> %08X at 0x%X' % (oldIndex, indexCrc, OFF_INDEX_CRC))
	print('  header crc    : %08X -> %08X at 0x%X' % (oldHeader, headerCrc, OFF_HEADER_CRC))
	print('  count before  : %d' % before)
	print('  count after   : %d' % after)
	ok = (off in moved
		  and oldChunk != chunkCrc and oldIndex != indexCrc and oldHeader != headerCrc
		  and set(moved) <= allowed
		  and abs(after - before) == 1)
	print('RED CONTROL %s: the count %s when one flag byte moved, and all THREE CRC '
		  'words had to be repaired for the file to open at all'
		  % ('OK' if ok else 'FAILED', 'moved' if after != before else 'DID NOT MOVE'))
	return 0 if ok else 1


if __name__ == '__main__':
	sys.exit(main())
