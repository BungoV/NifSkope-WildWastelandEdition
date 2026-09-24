"""Repair the dot-source line the heredoc mangled (a backslash does not survive
a shell heredoc -- lodgen skill, editing traps). Built from chr(92)."""
import os
BS = chr(92)
bad = '. "$PSScriptRoot\no_crash_dialog.ps1"   # BAKEPERF1: no crash dialog on his desktop\n'
good = '. "$PSScriptRoot' + BS + 'no_crash_dialog.ps1"   # BAKEPERF1: no crash dialog on his desktop\n'
for p in ('bake_run.ps1', 'bisect.ps1', 'bisect2.ps1', 'gate_identity.ps1'):
    s = open(p, encoding='utf-8', newline='').read()
    if good in s:
        print('already ok', p)
        continue
    assert s.count(bad) == 1, (p, s.count(bad))
    s = s.replace(bad, good)
    open(p, 'w', encoding='utf-8', newline='').write(s)
    print('repaired', p)
