# CARDFIX1: DONE.md step-4 sections (text only)
P = 'E:/Projects/NifskopeWWE-cardfix1/scratchpad/cardfix1_20260924/DONE.md'
b = open(P, 'rb').read()
cr = b.count(b'\r')
s = b.decode('utf-8')


def rep(old, new):
    global s
    assert s.count(old) == 1, old[:60]
    s = s.replace(old, new)


rep('\n# 3. Gates (numbers; red runs)\n',
    '\n## Step 4 -- R5 defaults: N8, the crisp cut, the slider at the crisp end\n'
    '- Checked first: N8 (the bake driver\'s OCT default, the panel\'s Card frames default) and the slider at\n'
    '  the crisp end (frameCount 1, flat) were ALREADY the shipped defaults (IMPOSTORDEPTH2). Nothing moved there.\n'
    '- The crisp cut is now the default BY NAME: `Resolved::cutRule`, which is 2 (the strongest frame) whenever\n'
    '  one frame is drawn, and the Options cut otherwise; the shader\'s `cutRule` uniform reads the resolved rule\n'
    '  (src/gl/impostordraw.h/.cpp). Before, the crisp end drew under the stipple rule and was crisp only because\n'
    '  one frame at weight 1 happens to cut where that frame does. No pixel moves (D5, D7).\n'
    '- The preview harness names the resolved cut in its log (src/impostorpreviewtest.cpp).\n'
    '- New gate tests/spells/impostor_defaults.sh, rows D1-D7.\n'
    '\n# 3. Gates (numbers; red runs)\n')

rep('Output: gates/cardres_test.out (pictures under cardres/, not committed).\n',
    'Output: gates/cardres_test.out (pictures under cardres/, not committed).\n'
    '\n## Step 4 (exe 56724fa6)\n'
    '- impostor_defaults.sh: 7 checks / 0 failures (gates/impostor_defaults.new.out). D7 = the default picture is\n'
    '  byte-identical to the rung\'s default at all 16 views. RED on the rung exe 97716e49: 6 / 1, D4 (the rung\n'
    '  names no strongest-frame cut; gates/impostor_defaults.rung.out). D6 is the floor: the smooth end differs\n'
    '  from the default at 8 of 16 views, so D5/D7 can see a change.\n'
    '- kept green on 56724fa6: impostor_trunk 38/3 (the three named: flat-snap tear el 0 and el 20 = bungo\'s\n'
    '  13:1x ruling, smooth end el 20), impostor_draw 33/1 (row 5, the known red), impostor_aa 7/0.\n'
    '  Outputs gates/*.s4.out.\n')

rep('- commits: step 1 19c0347; step 2 (evidence only) see git log.\n',
    '- step 4 build: 56724fa6332362467884619efe19667e158024fc (22:31:49), 24,717,312 B.\n'
    '- commits: step 1 19c0347; step 2 91ddd41 (evidence only); step 3 d8302c9; step 4 see git log.\n')

out = s.encode('utf-8')
assert out.count(b'\r') == cr
open(P, 'wb').write(out)
print('DONE.md step 4 written')
