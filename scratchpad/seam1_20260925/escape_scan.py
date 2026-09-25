"""Scan the branch's added C++ lines for unknown escape sequences in string literals (g++ turns "\G" into "G")."""
import re, subprocess
BS = chr(92)
d = subprocess.run(['git', 'diff', 'ea0ca708', '--', 'src'], capture_output=True,
                   cwd='E:/Projects/NifskopeWWE-seam1').stdout.decode('utf-8', 'replace')
ok = set('ntr0"x' + "'" + BS)
bad = 0
for l in d.splitlines():
    if not l.startswith('+') or l[1:].strip().startswith(('*', '//', '/*')):
        continue
    i = 0
    for q in re.findall(r'"(?:[^"\\]|\\.)*"', l):
        for m in re.finditer(re.escape(BS) + '(.)', q):
            if m.group(1) not in ok:
                print(l.strip()[:140]); bad += 1; break
print('lines with unknown escapes:', bad)
