# CARDFIX1: DONE.md step 6 sections, the PARTIAL head, the step-6 exes and commits. LF-only.
P = 'E:/Projects/NifskopeWWE-cardfix1/scratchpad/cardfix1_20260924/DONE.md'
b = open(P, 'rb').read(); assert b.count(b'\r') == 0
s = b.decode('utf-8')


def rep(o, n):
    global s
    assert s.count(o) == 1, (o[:60], s.count(o))
    s = s.replace(o, n)


rep('PARTIAL -- lane CARDFIX1 (LOD-D), chain of seven steps; this file grows one section per landed step.\n',
    'PARTIAL -- lane CARDFIX1 (LOD-D), chain of seven steps. Steps 1-5 landed; step 6 (sway A) built, committed and\n'
    'gated, RED on G4 only (the BC7 error bar, pre-registered from the synthetic; decision owed, section 3). NEXT:\n'
    'step 7, IMPOSTORPBRM1 (brief_impostorpbrm1.md), not started: the brief forbids a step on a red one.\n')

STEP6_BUILT = open('E:/Projects/NifskopeWWE-cardfix1/scratchpad/cardfix1_20260924/done_step6_built.txt',
                   encoding='utf-8').read()
STEP6_GATES = open('E:/Projects/NifskopeWWE-cardfix1/scratchpad/cardfix1_20260924/done_step6_gates.txt',
                   encoding='utf-8').read()
rep('\n# 3. Gates (numbers; red runs)\n', '\n' + STEP6_BUILT + '\n# 3. Gates (numbers; red runs)\n')
rep('\n# 4. Exe sha1 + commits\n', '\n' + STEP6_GATES + '\n# 4. Exe sha1 + commits\n')
rep('  step 5 1303334 (code) + this DONE commit.\n',
    '  step 5 1303334 (code) + 6c5f5f8 (DONE); step 6 = the commit carrying this text (code, gate, DONE together).\n'
    '- step 6 builds: 0eeade3a (23:59:42, first); 309f3aa9a09897da12c94db644ff70f57f33dc7b (2026-09-25 00:22:33,\n'
    '  24,737,280 B; + the ring array file name, fix24) = the exe every step-6 number is from.\n')
out = s.encode('utf-8'); assert out.count(b'\r') == 0
open(P, 'wb').write(out); print('patched')
