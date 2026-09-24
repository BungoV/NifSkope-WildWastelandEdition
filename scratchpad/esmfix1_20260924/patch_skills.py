S = 'E:/Projects/Claude/.claude/skills/'
def patch(rel, a, b):
    p = S + rel
    with open(p, 'rb') as f:
        src = f.read()
    cr = src.count(b'\r')
    assert src.count(a) == 1, (rel, src.count(a))
    src = src.replace(a, b)
    assert src.count(b'\r') == cr
    with open(p, 'wb') as f:
        f.write(src)
    print('ok', rel)
patch('mo2-mod-content-census/SKILL.md',
b'''  483 records under top byte FF; libfo76utils refuses raw form IDs above 0x0FFFFFFF. Probe a plugin's form-ID
  top bytes with NifSkope `scratchpad/loadorder1_20260924/formid_probe.py <plugin>` before blaming the load order.
''',
b'''  483 records under top byte FF; libfo76utils refused raw form IDs above 0x0FFFFFFF. FIXED by lane ESMFIX1
  (2026-09-24, branch esmfix1-20260924 until merged): the game's and xEdit's rule is "a file index at or beyond the
  plugin's master count names the plugin itself" (xEdit `TwbFile.FileFileIDtoLoadOrderFileID`), and esmfile.cpp
  now maps it so for form versions < 0xC0. On an exe without the fix, probe a plugin's top bytes with NifSkope
  `scratchpad/esmfix1_20260924/tes4probe.py <plugin>` (header flags, HEDR, masters, top bytes) or `census.py
  <print-source.txt>` (every plugin of a profile, records whose top byte exceeds the master count).
''')
patch('nifskope-ww-lodgen/SKILL.md',
b'''  profile has no `ModOrganizer.ini`, so the CLI needs `--data-root` for it. The panel writes no `.lodb`
''',
b'''  profile has no `ModOrganizer.ini`, so the CLI needs `--data-root` for it. LOAD-ALL-PLUGINS CHECK without a
  bake (ESMFIX1): `lodgen --mo2-profile <p> --list-worldspaces` constructs the ESM reader over every plugin
  (seconds; `--print-source` never opens a plugin, so it cannot prove a plugin loads). The reader resolves a
  form-ID file index >= the plugin's master count to the plugin itself (game/xEdit rule, form version < 0xC0).
  The panel writes no `.lodb`
''')
patch('nifskope-ww-worktree-build/SKILL.md',
b'''## 6. What lane CARDLINK1 added''',
b'''## 5b. A sibling worktree at your exact commit is a better object source than main (ESMFIX1, 2026-09-24)
When your branch starts at ANOTHER lane's head (not main), main's objects do not match it, but that lane's
worktree may: `git -C <sibling> log -1` = your branch point, `git -C <sibling> status --short -uno` empty,
`make -n -f Makefile.Release | grep -c "g++ "` = 0 run IN the sibling. Then copy `.qmake.stash`,
`GeneratedFiles` and the release runtime (add `qt.conf`, `hkclasses_fo4.json`, `hkx_annotation_vocabulary.txt`)
from the sibling instead of main; the NIFSKOPE_REVISION objects need no delete (same commit). First make: 57
objects (moc + the path-named ones), about 3 minutes; the rung is that exe.

## 6. What lane CARDLINK1 added''')
