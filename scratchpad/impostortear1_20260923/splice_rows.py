p = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/impostor_draw.sh'
s = open(p, 'rb').read().decode('utf-8')
assert '\r' not in s
a = 'say "done  $steps steps, $fails failures"'
assert 'IMPOSTORTEAR1' not in s, 'already spliced'
i = s.rindex(a)
assert s[i:].count(chr(10)) <= 4, 'the last done line is not the tail'
r = open('E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostortear1_20260923/rows17_18.sh', 'rb').read().decode('utf-8')
assert '\r' not in r
s = s[:i] + r + s[i:]
open(p, 'wb').write(s.encode('utf-8'))
print('spliced at', i)
