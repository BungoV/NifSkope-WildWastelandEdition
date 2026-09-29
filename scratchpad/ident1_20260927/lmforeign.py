"""IDENT1: name the non-landmark pieces inside a landmark's top group, at a tolerance and cap.
usage: python lmforeign.py <dump> <tol> <cap> <landmark key>"""
import sys, os, collections
import groups

h, P, C = groups.load(sys.argv[1])
roots, gb, ref = groups.run(P, C, float(sys.argv[2]), float(sys.argv[3]))
mem, hist = groups.summary(P, roots, gb)
lm = groups.landmarks(P, roots, mem, gb)[sys.argv[4]]
top = lm['top'][0]['root']
words, centre, rad = groups.LANDMARKS[sys.argv[4]]
cnt = collections.Counter()
for p in mem[top]:
    nm = p['name'].lower()
    inside = any(w in nm for w in words) and (not centre or ((p['x'] - centre[0]) ** 2 + (p['y'] - centre[1]) ** 2) ** 0.5 <= rad)
    if not inside:
        cnt[(os.path.basename(p['name'].replace(chr(92), '/').split('(')[-1].rstrip(')')), round(p['x'], -2), round(p['y'], -2))] += 1
for k, v in cnt.most_common(20):
    print(v, k)
