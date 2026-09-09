import sys, os, io
HERE = os.path.dirname(os.path.abspath(__file__))
rep = os.path.join(HERE, 'report_mountains.md')
with io.open(rep, 'a', encoding='utf-8') as r:
    for name in sys.argv[1:]:
        r.write(io.open(os.path.join(HERE, name), encoding='utf-8').read())
print('appended', sys.argv[1:], '-> report is', os.path.getsize(rep), 'bytes')
