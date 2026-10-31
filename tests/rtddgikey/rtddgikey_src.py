"""FIX1101A: the ray-traced GI volume's build key (src/rtddgicell.cpp, the key the frame tick compares to rebuild) must
hold the soup's ALBEDO. Manager hold h1 (10-31): a WW_SOUPALB_RED reopen rebuilt the soup grey (census "albedo grey 0.5")
but the volume kept running (no "rebuilding" line: same soup slot, same triangles, same key), so the after / soupred pair
compared one volume with itself (240 px). Rules read from the source text:
  1. the key block (from `quint64 key = 0xCBF29CE484222325ULL;` to `const int skyOn`) mixes soup->alb's size and bytes
  2. the red token noalbkey (WW_CELL_RTDDGI_RED) is parsed into the path bits and skips only that part
  3. the path bits are in the key (a set of the red rebuilds the volume)
usage: python -I rtddgikey_src.py <rtddgicell.cpp>   rc 0 = all rules hold, 1 = a rule fails"""
import re
import sys
s = open(sys.argv[1], encoding='utf-8', errors='replace').read()
a = s.find('quint64 key = 0xCBF29CE484222325ULL;')
b = s.find('const int skyOn', a)
blk = s[a:b] if a >= 0 and b > a else ''
r1 = bool(re.search(r'soup->alb\.size\(\)', blk)) and bool(re.search(r'fnv\(\s*key\s*,\s*&soup->alb\[', blk))
r2 = 'QLatin1String( "noalbkey" )' in s and bool(re.search(r'if\s*\(\s*!\s*\(\s*paths\s*&\s*kPathNoAlbKey\s*\)\s*\)', blk))
r3 = bool(re.search(r'key\s*=\s*fnv\(\s*key\s*,\s*&paths\s*,', s))
print('rtddgikey: key block %d chars; rule 1 (albedo in the key) %s, rule 2 (red noalbkey skips it) %s, rule 3 (path reds in the key) %s'
      % (len(blk), 'OK' if r1 else 'FAIL', 'OK' if r2 else 'FAIL', 'OK' if r3 else 'FAIL'))
ok = r1 and r2 and r3
print('RTDDGIKEY %s' % ('OK' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
