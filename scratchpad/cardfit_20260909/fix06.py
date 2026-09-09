#!/usr/bin/env python3
"""CARDFIT3 fix 06 -- amend the nifskope-ww-lodgen skill with the syntax gate a
broken harness block cost this lane, and sharpen the heredoc trap it already
carries. Live tree only: the repo's own .claude/skills does not hold this skill.

Written as a FILE and not a heredoc, which is the trap being written up.
"""
BS = chr(92)
P = r'E:\Projects\Claude\.claude\skills\nifskope-ww-lodgen\SKILL.md'
b = open(P, 'rb').read()
cr = b.count(b'\r')
s = b.decode('utf-8')

old = ("## Editing traps (each cost a build)\n"
       "* Heredocs halve backslashes and mangle `" + BS + "n` inside Python; write patch scripts with the Write\n"
       "  tool and run them, or build escapes from `bytes([92])`.\n")

new = (
"## Compile a harness's embedded Python BEFORE running the harness (2026-09-09)\n"
"A `tests/spells/*.sh` gate is mostly Python inside `<<'PYEOF'` heredocs. A SyntaxError in one of\n"
"those blocks does NOT stop the script: the block dies, its checks never run, the shell's `fails`\n"
"counter never moves for them, and the suite prints `RESULT FAIL` that reads exactly like a failing\n"
"check. One stray apostrophe cost a ten-minute run this way. Two seconds of gate first:\n"
"\n"
"```python\n"
"import re, sys\n"
"s = open('tests/spells/<name>.sh', encoding='utf-8').read()\n"
"for i, blk in enumerate(re.findall(r\"<<'PYEOF'" + BS + "n(.*?)" + BS + "nPYEOF\", s, re.S)):\n"
"    try:\n"
"        compile(blk, '<block %d>' % i, 'exec'); print('block %d ok' % i)\n"
"    except SyntaxError as e:\n"
"        print('block %d line %s: %s' % (i, e.lineno, e.msg)); sys.exit(1)\n"
"```\n"
"\n"
"Run it after every edit to a harness, and treat a suite whose `ok` COUNT DROPPED as a broken block\n"
"until proved otherwise -- the count is the tell, not the verdict. (47 ok where the run before had\n"
"68 was the signal; `RESULT FAIL` was not.)\n"
"\n"
"## Editing traps (each cost a build)\n"
"* Heredocs halve backslashes and mangle `" + BS + "n` inside Python; write patch scripts with the Write\n"
"  tool and run them, or build escapes from `bytes([92])` / `chr(92)`. **This includes an apostrophe\n"
"  escaped as `" + BS + "'` inside a single-quoted Python string** -- the heredoc eats the backslash and the\n"
"  string ends early, twenty lines from where the error is reported. Four occurrences in one lane,\n"
"  2026-09-09; the rule is that no text carrying a backslash or an apostrophe goes through a heredoc\n"
"  at all.\n")

assert s.count(old) == 1, s.count(old)
s = s.replace(old, new)
out = s.encode('utf-8')
assert out.count(b'\r') == cr
open(P, 'wb').write(out)
print('skill amended: CR %d unchanged, %d bytes' % (cr, len(out)))
