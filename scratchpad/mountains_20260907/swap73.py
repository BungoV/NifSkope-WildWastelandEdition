"""Replace the [RUNNING] placeholder for section 7.3 with the landed result."""
import io, os
HERE = os.path.dirname(os.path.abspath(__file__))
rep = os.path.join(HERE, 'report_mountains.md')
t = io.open(rep, encoding='utf-8').read()
start = t.find('### 7.3 Full-corpus level-4 sweep — [RUNNING]')
assert start > 0, 'placeholder not found'
new = io.open(os.path.join(HERE, 'sec7c.md'), encoding='utf-8').read()
t = t[:start] + new
io.open(rep, 'w', encoding='utf-8').write(t)
print('section 7.3 replaced;', os.path.getsize(rep), 'bytes')
