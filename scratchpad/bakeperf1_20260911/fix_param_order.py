"""PowerShell requires param(...) to be the first STATEMENT in a script, so the
dot-source line has to sit after the param block, not before it."""
P = 'bake_run.ps1'
s = open(P, encoding='utf-8', newline='').read()
line = '. "$PSScriptRoot' + chr(92) + 'no_crash_dialog.ps1"   # BAKEPERF1: no crash dialog on his desktop\n'
assert s.startswith(line), repr(s[:90])
s = s[len(line):]
anchor = '  [string]$Log = ""\n)\n'
assert s.count(anchor) == 1, s.count(anchor)
s = s.replace(anchor, anchor + '\n' + line)
open(P, 'w', encoding='utf-8', newline='').write(s)
print('param order fixed')
