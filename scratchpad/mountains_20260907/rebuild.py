import io, os
HERE = os.path.dirname(os.path.abspath(__file__))
rep = os.path.join(HERE, 'report_mountains.md')
body = io.open(rep, encoding='utf-8').read()
head = io.open(os.path.join(HERE, 'head.md'), encoding='utf-8').read()
anchor = body.find('## 0. TOOLING')
assert anchor > 0, 'anchor not found'
body = '\n'.join(['---', '', '# THE MEASUREMENTS', '', '']) + body[anchor:]
io.open(rep, 'w', encoding='utf-8').write(head + body)
print('report rebuilt:', os.path.getsize(rep), 'bytes')
