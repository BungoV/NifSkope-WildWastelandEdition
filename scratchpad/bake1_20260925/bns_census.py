"""BNS Trees.esp record census: which record types, which STAT/TREE bases carry MODL under bns, how many REFRs
place them, and whether those REFRs sit in the Commonwealth. Read only. Usage: python bns_census.py <esp>"""
import sys, collections, struct
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/mountains_20260907')
import fo4esm

buf = fo4esm.load(sys.argv[1])
cnt = collections.Counter()
new_forms = collections.Counter()
bases = {}          # formid -> (sig, modl, has_mnam)
refr_names = collections.Counter()
refr_in_ws = collections.Counter()
tes4 = fo4esm.read_record_header(buf, 0)
nmasters = 0
for sub in fo4esm.subrecords(bytes(buf[24:24 + tes4[1]])):
    if sub[0] == b'MAST':
        nmasters += 1

def on_record(off, sig, dsize, flags, formid, tail, stack):
    cnt[sig] += 1
    if (formid >> 24) >= nmasters:
        new_forms[sig] += 1
    if sig in (b'STAT', b'TREE', b'SCOL', b'REFR'):
        p = fo4esm.record_payload(buf, off, dsize, flags)
        subs = {}
        for s in fo4esm.subrecords(p):
            subs.setdefault(s[0], s[1])
        if sig == b'REFR':
            nm = subs.get(b'NAME')
            if nm is not None:
                refr_names[struct.unpack('<I', nm[:4])[0]] += 1
                ws = [n for n in stack if n.gtype == 1]
                if ws:
                    refr_in_ws[struct.unpack('<I', ws[0].label)[0]] += 1
        else:
            modl = subs.get(b'MODL', b'').split(b'\0')[0].decode('latin1')
            bases[formid] = (sig.decode(), modl, b'MNAM' in subs)

fo4esm.walk(buf, 24 + tes4[1], len(buf), [], on_record)
print('masters', nmasters)
print('records', dict((k.decode(), v) for k, v in cnt.most_common(20)))
print('new forms', dict((k.decode(), v) for k, v in new_forms.most_common(20)))
bnsb = {f: b for f, b in bases.items() if 'bns' in b[1].lower()}
print('bases total', len(bases), 'with bns MODL', len(bnsb), 'with MNAM', sum(1 for b in bases.values() if b[2]))
print('by type', collections.Counter(b[0] for b in bases.values()))
print('REFR total', sum(refr_names.values()), 'placing bns bases', sum(refr_names[f] for f in bnsb))
print('REFR by worldspace', {('%08X' % k): v for k, v in refr_in_ws.items()})
for f, b in sorted(bnsb.items())[:12]:
    print('  %08X %s %s mnam=%d refs=%d' % (f, b[0], b[1], b[2], refr_names[f]))
top = refr_names.most_common(10)
print('most placed bases', [('%08X' % f, n, bases.get(f, ('?', '?'))[1]) for f, n in top])
