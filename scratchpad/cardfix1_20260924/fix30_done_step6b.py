# CARDFIX1: DONE.md step 6b (bungo's N8-default ruling), its gate numbers, the commits line. LF-only.
P = 'E:/Projects/NifskopeWWE-cardfix1/scratchpad/cardfix1_20260924/DONE.md'
b = open(P, 'rb').read(); assert b.count(b'\r') == 0
s = b.decode('utf-8')


def rep(o, n):
    global s
    assert s.count(o) == 1, (o[:60], s.count(o))
    s = s.replace(o, n)


rep('step 7, IMPOSTORPBRM1 (brief_impostorpbrm1.md), not started: the brief forbids a step on a red one.\n',
    'step 7, IMPOSTORPBRM1 (brief_impostorpbrm1.md), not started: the brief forbids a step on a red one.\n'
    'Step 6b (the director\'s relay of bungo\'s 2026-09-25 ruling, the N8 grid as the bake default) landed after 6.\n')
rep('\n# 3. Gates (numbers; red runs)\n',
    '\n## Step 6b -- the N8 grid is the bake default (bungo RULED 2026-09-25)\n'
    '- bungo, 2026-09-25, verbatim (relayed by the director): **"Yes, 8x8 is the default choice for a bake"**.\n'
    '  It replaces his 2026-09-23 ring default for tree bakes, after step 5 measured N8 better than the ring at\n'
    '  every elevation, the horizon included.\n'
    '- tools/bake_impostor_cards.sh: RING defaults to 0 (the N8 grid) for every run, trees included; RING=16\n'
    '  still bakes the ring, anything else is refused by name. The panel has no ring path (the only\n'
    '  WW_IMPOSTOR_RING reader is the bake hook), so nothing changed there. docs/LODGEN_LODM_FORMAT.md 3.2 and\n'
    '  docs/LODGEN_CARD_SHEETS.md say the ring is an option. No exe change (script and docs only).\n'
    '- New gate row R7 in tests/spells/impostor_ring.sh: it runs the driver itself with MAX=0 (nothing is\n'
    '  photographed) and reads its library.txt.\n'
    '\n# 3. Gates (numbers; red runs)\n')
rep('\n# 4. Exe sha1 + commits\n',
    '\n## Step 6b (exe 309f3aa9, unchanged)\n'
    '- tests/spells/impostor_ring.sh 17 / 0 (gates/impostor_ring.s6n.out): R1-R6 as before, plus R7: the tree\n'
    '  run with no RING writes `ring 0`; RING=16 writes `ring 16`; RING=5 exits 2 naming the rule. RED: the\n'
    '  step-5 driver (git 1303334, pulled out beside the real one and removed afterwards) wrote `ring 16`.\n'
    '\n# 4. Exe sha1 + commits\n')
rep('  step 5 1303334 (code) + 6c5f5f8 (DONE); step 6 = the commit carrying this text (code, gate, DONE together).\n',
    '  step 5 1303334 (code) + 6c5f5f8 (DONE); step 6 24e7835 (code, gate, DONE); step 6b = the commit carrying\n'
    '  this text.\n')
rep('# 5. What the final bake needs\n(filled at the end)\n',
    '# 5. What the final bake needs\n'
    '- A card bake from THIS branch: the N8 grid by default (no RING), TILE 256, the crisp cut (steps 4, 6b).\n'
    '- Sway A is on for every model with a tree-animation shape (no switch; ruled). Those cards and their card\n'
    '  arrays are `lodm` 2: an exe from before step 6 and FO4CS\'s current reader refuse them BY NAME. So the\n'
    '  in-game test needs the FO4CS reader to learn `lodm` 2 (owed, FO4CS built last by standing order); the\n'
    '  NifSkope side reads and draws them.\n'
    '- The G4 ruling (accept 3.573 / 13, or raise the BC7 alpha weight) comes BEFORE the final bake, because\n'
    '  option (b) changes every model-sway card\'s bytes.\n'
    '- lodgenaggregate does not know ring sets or `lodm` 2 (not this lane\'s file): with the N8 default no ring\n'
    '  set is made, and the aggregate composites the weight but writes lodm 1 with no `sway` key.\n'
    '- Previewing a loose card set needs a `textures\\` tree beside the .lodm (the harness says so by name).\n')
rep('# 6. Skill review\n(filled at the end)\n',
    '# 6. Skill review\n'
    '- Written: E:\\Projects\\Claude\\.claude\\skills\\ww-preregister-bar-from-the-subject\\SKILL.md -- measure the\n'
    '  reference\'s own ceiling, the subject\'s population and the REAL input before writing a bar; print the\n'
    '  codec\'s error on an untouched channel beside a lossy-codec bar; a pre-registered bar that turns red on\n'
    '  correct code is reported with options, not re-pinned in the same step. It folds this lane\'s four\n'
    '  MISTAKES entries into one procedure.\n'
    '- Candidates for the director (DELIVERABLE_TEXT.md, Skill review): staging one step\'s hunks of a file that\n'
    '  already holds the next step\'s (stage6_docs.py: undo the later patch\'s pairs in memory, write the INDEX\n'
    '  only); D7 in impostor_defaults.sh needs RUNG=; the preview\'s `textures\\` tree for a loose set; the\n'
    '  pbr_shade_ab OLD arm needs the whole runtime (DLLs, qt.conf), and an OLD arm that cannot start reads as\n'
    '  "NO PICTURE" on every case, not as a refusal.\n')
out = s.encode('utf-8'); assert out.count(b'\r') == 0
open(P, 'wb').write(out); print('patched')
