import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/src/nifcli.cpp'
s = open(P, 'rb').read().decode('utf-8')
assert s.count('\r') == 0
pairs = [
	('[--msn-cache DIR]\\n"', '[--msn-cache DIR|auto]\\n"'),
	('''		  << "                                          heights normal.\\n"
		  << "         [--vt-compress none|zlib]''',
	 '''		  << "                                          heights normal. DIR may be the\\n"
		  << "                                          sheets' own folder or a mod / Data\\n"
		  << "                                          folder holding them under\\n"
		  << "                                          Textures/Terrain/<world>/. auto: the\\n"
		  << "                                          last --resource folder with sheets\\n"
		  << "                                          wider than vanilla's 512, or none.\\n"
		  << "         [--vt-compress none|zlib]'''),
]
for o, n in pairs:
	c = s.count(o)
	if c != 1:
		sys.exit('anchor count %d: %r' % (c, o[:50]))
	s = s.replace(o, n)
open(P, 'wb').write(s.encode('utf-8'))
print('clihelp ok')
