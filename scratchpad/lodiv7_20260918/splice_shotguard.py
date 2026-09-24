p = 'tests/spells/lodi_v7.sh'
s = open(p, encoding='utf-8', newline='').read()

O = """\t[ -s "$OUT/$1.png" ] || echo "  (no picture for $1, exit $?)"
}"""
assert s.count(O) == 1, s.count(O)

N = """\t[ -s "$OUT/$1.png" ] || echo "  (no picture for $1, exit $?)"
\t# PRECAUTION, not a fix for anything measured (MISTAKES 2026-09-18 18:3x):
\t# nothing this gate starts may outlive its own shot. The one-instance rule
\t# means a window left standing makes every later gate meaningless, and the
\t# gate should say so here rather than produce blank pictures afterwards.
\t# A NifSkope with no `--port` is bungo's window; this gate never starts one
\t# without a port, so anything still up after a shot is this gate's own.
\tif tasklist 2>/dev/null | grep -qi "NifSkope.exe"; then
\t\techo "  STOP: a NifSkope is still up after shot '$1' (this gate's port $PORT)."
\t\techo "        Unwedge it WITHOUT killing it by sending it the scene over its own"
\t\techo "        port -- a UTF-16LE UDP datagram 'NifSkope::open <win path>' to"
\t\techo "        127.0.0.1:$PORT (src/main.cpp IPCsocket). Then re-run this gate."
\t\treturn 9
\tfi
}"""

s = s.replace(O, N)
open(p, 'w', encoding='utf-8', newline='').write(s)
d = open(p, 'rb').read()
print('shot() guard added; CRLF', d.count(b'\r\n'))
