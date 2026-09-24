"""The report was written incrementally (CONSTITUTION 1), so its 3.x
subsections landed in the order they were measured rather than in reading
order. Reassemble: preamble, 0, 1, 2, 3 (+3.1..3.14 sorted), 4, 5, 6.
Nothing is edited -- only moved -- and the line count must not change except
for the separators this rewrites."""
import re

P = 'scratchpad/lane_bakeperf1_report.md'
src = open(P, encoding='utf-8', newline='').read()
before = len(src)

lines = src.split('\n')
heads = [(i, l) for i, l in enumerate(lines) if re.match(r'^#{2,3} ', l)]

blocks = []           # (key, text)
first = heads[0][0]
preamble = '\n'.join(lines[:first])

for n, (i, l) in enumerate(heads):
    end = heads[n + 1][0] if n + 1 < len(heads) else len(lines)
    text = '\n'.join(lines[i:end])
    m = re.match(r'^#{2,3} (\d+)(?:\.(\d+))?([a-z]?)', l)
    major = int(m.group(1))
    minor = int(m.group(2)) if m.group(2) else -1     # the ## heading sorts first
    suffix = m.group(3) or ''
    blocks.append(((major, minor, suffix), text))

blocks.sort(key=lambda b: b[0])

# strip any trailing "---" separators the incremental appends left mid-block,
# then join with one separator between MAJOR sections only
out = [preamble.rstrip('\n')]
prev_major = None
for key, text in blocks:
    text = text.rstrip('\n')
    # a block that ENDS with a horizontal rule was an append seam
    text = re.sub(r'\n+---\s*$', '', text)
    if key[1] == -1:
        out.append('\n---\n')
    out.append(text)
    prev_major = key[0]

res = '\n\n'.join(x for x in out if x != '') + '\n'
res = res.replace('\n\n\n---\n\n\n', '\n\n---\n\n')
open(P, 'w', encoding='utf-8', newline='').write(res)
print('reordered: %d -> %d bytes, %d blocks' % (before, len(res), len(blocks)))
for key, _ in blocks:
    print('  ', key)
