"""Same rule for gate_identity.ps1: param(...) first, dot-source after it."""
P = 'gate_identity.ps1'
s = open(P, encoding='utf-8', newline='').read()
line = '. "$PSScriptRoot' + chr(92) + 'no_crash_dialog.ps1"   # BAKEPERF1: no crash dialog on his desktop\n'
assert s.startswith(line), repr(s[:90])
s = s[len(line):]
anchor = 'param([switch]$SkipBakes)\n'
assert s.count(anchor) == 1
s = s.replace(anchor, anchor + '\n' + line)
open(P, 'w', encoding='utf-8', newline='').write(s)
print('gate_identity param order fixed')
