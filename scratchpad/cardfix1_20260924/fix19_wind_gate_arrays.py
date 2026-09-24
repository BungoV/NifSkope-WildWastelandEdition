P = 'E:/Projects/NifskopeWWE-cardfix1/tests/spells/impostor_wind.sh'
b = open(P, 'rb').read(); assert b.count(b'\r') == 0
s = b.decode('utf-8')
def rep(o, n, c=1):
    global s
    assert s.count(o) == c, (o, s.count(o)); s = s.replace(o, n)
rep('compress elm "$EXE" --arrays\n',
    'compress elm "$EXE"\n'
    '# the card ARRAY: the same card set through the region route with --arrays (lodgen_card_arrays.sh\'s shape,\n'
    '# at the far ring where a card substitutes by default)\n'
    '"$EXE" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -32 16 -17 31 --dim 16 --no-ao \\n'
    '\t--out-dir "$WORK/elm/obj" --tex-dir "$WORK/elm/tex" --data-root "$DATA" --arrays --impostors "$WORK/elm/cards" \\n'
    '\t> "$WORK/elm/arrays.log" 2>&1\n'
    'say "arrays elm: lodgen rc $?; $( grep -a -m1 \'^card arrays written:\' "$WORK/elm/arrays.log" )"\n')
rep('''arr=$( grep -l '"cardArray"' $( find "$WORK/elm" -iname '*.lodm' -not -path '*/cards/*' ) 2>/dev/null | head -1 )''',
    '''arr=$( find "$WORK/elm/tex" -iname '*LodgenCards*.lodm' 2>/dev/null | head -1 )''')
out = s.encode('utf-8'); assert out.count(b'\r') == 0
open(P, 'wb').write(out); print('patched')
