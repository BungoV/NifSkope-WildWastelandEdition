import re, glob, sys
rows = []
for f in sorted(glob.glob('pop_*.txt')):
    n = f[4:-4]
    t = open(f).read()
    r = re.findall(r'(\w+_n4)\s+(az|el):\s+(\d+) vs\s+(\d+) x1.5 =\s+[\d.]+\s+ratio ([\d.]+)', t)
    tear = re.findall(r'(\w+_n4)\s+az\d+ (\S+)\s+IoU ([\d.]+).*?torn share ([\d.]+)%', t)
    iou = re.findall(r'^(\S+)\s+(\w+_n4)\s+az: .*IoU mean ([\d.]+)', t, re.M)
    s = '%-16s pop ' % n + ' '.join('%s/%s %s' % (a[:5], b, c) for a, b, _, _, c in r)
    s += ' | tear ' + ' '.join('%s %s %s%%' % (a[:5], i, ts) for a, rr, i, ts in tear if rr != 'run_ship')
    s += ' | azIoU ' + ' '.join('%s %s' % (b[:5], m) for a, b, m in iou if a != 'run_ship')
    print(s)
