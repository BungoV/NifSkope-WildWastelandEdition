P = 'E:/Projects/NifskopeWWE-cardfix1/tests/spells/impostor_wind.sh'
b = open(P, 'rb').read(); assert b.count(b'\r') == 0
s = b.decode('utf-8')
def rep(o, n, c=1):
    global s
    assert s.count(o) == c, (o, s.count(o)); s = s.replace(o, n)
rep('W="$( "$PY" "$here/impostor_wind.py" 2>/dev/null; true )"\n', '')
rep("ge() { \"$PY\" -c \"import sys; sys.exit(0 if float('$1') >= float('$2') else 1)\"; }\n",
    "ge() { \"$PY\" -c \"import sys; sys.exit(0 if float('$1') >= float('$2') else 1)\"; }\n"
    "le() { \"$PY\" -c \"import sys; sys.exit(0 if float('$1') <= float('$2') else 1)\"; }\n")
rep('! ge "${e:-999}" "$half.001"', 'le "${e:-999}" "$half"')
rep('! ge "${ec:-999}" "$half.001"', 'le "${ec:-999}" "$half"')
rep('! ge "${ep:-999}" "$half.001"', 'le "${ep:-999}" "$half"')
rep('! ge "${m:-99}" 3.001 && ! ge "${p:-99}" 12.001', 'le "${m:-99}" 3.0 && le "${p:-99}" 12')
rep('if ge "${ms:-0}" 3.001; then', 'if ! le "${ms:-0}" 3.0; then')
out = s.encode('utf-8'); assert out.count(b'\r') == 0
open(P, 'wb').write(out); print('patched')
