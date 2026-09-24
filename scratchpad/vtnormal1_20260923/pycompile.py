"""Compile every <<'PYEOF' block of the named harnesses before running them."""
import re, sys
bad = 0
for name in sys.argv[1:]:
	s = open(name, encoding='utf-8').read()
	blocks = re.findall(r"<<'PYEOF'\n(.*?)\nPYEOF", s, re.S)
	for i, blk in enumerate(blocks):
		try:
			compile(blk, '<%s block %d>' % (name, i), 'exec')
		except SyntaxError as e:
			print('%s block %d line %s: %s' % (name, i, e.lineno, e.msg))
			bad += 1
	print('%s: %d blocks' % (name.split('/')[-1], len(blocks)))
sys.exit(1 if bad else 0)
