p = 'scratchpad/lodiv7_20260918/lane_lodiv7_report.md'
s = open(p, encoding='utf-8', newline='').read()
A = "which re-runs G1..G3 and the two neighbours whose failures are already understood or already repaired. | The attempt at 19:11:54 returned `SKIP: Fallout4 is up` -- the harness's rule about the game holding the files. Game up during run; nothing was re-run in a loop to get around it. |"
N = ("which re-runs G1..G3 and carries the one neighbour not yet re-run, `lodgen_native.sh`, whose two failures are "
     "already repaired test-side. | The attempt at 19:11:54 returned `SKIP: Fallout4 is up` -- the harness's rule about "
     "the game holding the files, and `lodgen_native.sh` is a BAKE that reads the archives the game is holding. "
     "`render_shot.sh`, the other neighbour that failed on the stale binary, HAS since been re-run on its own and "
     "returns its standing 82/0 (19:19:24..19:21:45, game up during run). Nothing was re-run in a loop to get around "
     "the guard. |")
assert s.count(A) == 1
s = s.replace(A, N)
open(p, 'w', encoding='utf-8', newline='').write(s)
d = open(p, 'rb').read()
print('row 8 updated; CRLF %d; bytes %d' % (d.count(b'\r\n'), len(d)))
