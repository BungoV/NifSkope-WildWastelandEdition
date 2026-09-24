p = 'E:/Projects/NifskopeWWE-loadorder1/tests/spells/lodgen_loadorder.sh'
s = open(p, encoding='utf-8').read()
old = 'lg() { "$NS" -no-gui lodgen "$@" 2>&1 | tr -d \'\\r\'; }'
assert s.count(old) == 1, 'lg'
s = s.replace(old, "# lg <args>: the exe's output, CR stripped; returns the EXE's exit code\n"
              'lg() { "$NS" -no-gui lodgen "$@" > "$W/.lg.raw" 2>&1; local r=$?; tr -d \'\\r\' < "$W/.lg.raw"; return $r; }')
for a, b in (('--print-source > "$W/g1.txt"; echo "  rc=${PIPESTATUS[0]}"', '--print-source > "$W/g1.txt"; echo "  rc=$?"'),
             ('--print-source > "$W/g1_ptxt.txt"; rc=${PIPESTATUS[0]}', '--print-source > "$W/g1_ptxt.txt"; rc=$?'),
             ('--native "$WA/g5" > "$W/g5.log"; rc=${PIPESTATUS[0]}', '--native "$WA/g5" > "$W/g5.log"; rc=$?')):
    assert s.count(a) == 1, a
    s = s.replace(a, b)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('ok; PIPESTATUS left', s.count('PIPESTATUS'))
