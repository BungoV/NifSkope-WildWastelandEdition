"""Put the evidence sections into numeric order and unwrap the two headings
that got split across lines."""
import io, os, re
HERE = os.path.dirname(os.path.abspath(__file__))
rep = os.path.join(HERE, 'report_mountains.md')
t = io.open(rep, encoding='utf-8').read()

t = t.replace('## 4. LENS 4 — TOOLS (overseer, after the four lens agents were killed by the\n'
              '## session limit; done personally)',
              '## 4. LENS 4 — TOOLS\n\n*(Done personally: all four lens agents were killed mid-flight by the session limit.)*')
t = t.replace('## 3. LENS 3 — SHADING (done personally; the lens agent was killed at the\n'
              '## session limit before it reported)',
              '## 3. LENS 3 — SHADING\n\n*(Done personally; the lens agent was killed before it reported.)*')

# split on top-level '## N.' evidence sections and reorder 1,2,3,4,5,6
parts = re.split(r'(?m)^(?=## \d+\. )', t)
head, sections = parts[0], parts[1:]


def num(s):
    return int(re.match(r'## (\d+)\.', s).group(1))


sections.sort(key=num)
io.open(rep, 'w', encoding='utf-8').write(head + ''.join(sections))
print('reordered:', [num(s) for s in sections], os.path.getsize(rep), 'bytes')
