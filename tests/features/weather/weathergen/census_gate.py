"""Census gate (lane WEATHERRECX1): every weather record of the official masters resolves a .recx in the MO2 mod
"Fusion Shaders - Assets", read by the spec's reference reader (specwt/tests/reference_reader.py run_recx, untouched:
only its case-folder lookup is pointed at <mod>/Plugins). Then a dry dump of every input and where it comes from
(file#entry, or the generated rung / a default when the file does not name it -- the order of weatherrecx.cpp).
Refuters: a record no file names must be reported missing; a hidden file must make its record miss.
usage: python -I census_gate.py <specwt> <wthr_list.json> <mod root> <dump.tsv>"""
import json
import os
import sys

spec, listing, mod, dump = sys.argv[1:5]
sys.path.insert(0, os.path.join(spec, 'tests'))
import reference_reader as rr  # noqa: E402

REQUIRED = ['atmosphere', 'clouds', 'night', 'wind', 'temperature', 'transitions', 'groundFog']
INPUTS = [('atmosphere', k) for k in ('visibility', 'hazeHeight', 'ozone', 'hazeType', 'humidity', 'groundAlbedo',
                                      'angstrom', 'hazeAnisotropy', 'airglow')] + \
    [('clouds', k) for k in ('coverage', 'type', 'liquidWater', 'iceFraction', 'evolution')] + \
    [('night', k) for k in ('stars', 'moonlight')] + \
    [('wind', k) for k in ('speed', 'direction', 'gusts', 'turbulence')] + [('temperature', 'air')] + \
    [('lightning', k) for k in ('frequency', 'cloudToGround', 'distance', 'fadeIn', 'fadeOut')] + \
    [('transitions', 'duration')] + \
    [('precipitation', k) for k in ('wetting', 'drying', 'base', 'fadeIn', 'fadeOut', 'layers')] + \
    [('groundFog', 'thickness')]
# lane WEATHERRECX2 (spec 1.8): the reference reader answers in the 1.6 names and units (wetting / drying in minutes,
# lightning.distance in km); the fades and the shaft base sit in precipitation / lightning, the cloud water in liquidWater
# not written by the generated rung: the hazeType preset (OPAC) or the reader's own default supplies them
PRESET = {('atmosphere', 'angstrom'), ('atmosphere', 'hazeAnisotropy'), ('atmosphere', 'airglow')}

real_find = rr.find_case_free
hidden = set()


def find(base, parts):
    if [p.casefold() for p in parts] == ['data', 'plugins']:
        return os.path.join(mod, 'Plugins')
    return real_find(base, parts)


real_listdir = os.listdir


def listdir(p):
    return [n for n in real_listdir(p) if os.path.join(p, n).replace('\\', '/').casefold() not in hidden]


rr.find_case_free = find
rr.os.listdir = listdir
categories = rr.load_categories(spec)
wthrs = [w for w in json.load(open(listing, encoding='utf-8')) if not w.get('deleted')]


def run(extra=()):
    recs = [{'form': '%s|%s' % (w['owner'], w['lid']), 'category': 'Weathers', 'editorId': w['edid']} for w in wthrs]
    recs += [{'form': f, 'category': 'Weathers', 'editorId': e} for f, e in extra]
    case = {'plugins': ['Fallout4.esm', 'DLCRobot.esm', 'DLCworkshop01.esm', 'DLCCoast.esm', 'DLCworkshop02.esm',
                        'DLCworkshop03.esm', 'DLCNukaWorld.esm'], 'records': recs}
    return rr.run_recx(mod, case, categories)


def key_of(form):
    return rr.form_id(rr.parse_form(form))


out = run()
ok = True
missing, per_plugin, blocks_seen = [], {}, {}
lines = ['plugin\teditorId\tblock.key\tvalue\tsource']
for w in wthrs:
    k = key_of('%s|%s' % (w['owner'], w['lid']))
    got = out['records'].get(k, {})
    lack = [b for b in REQUIRED if b not in got]
    if lack:
        missing.append('%s %s lacks %s' % (w['owner'], w['edid'], ','.join(lack)))
    per_plugin[w['owner']] = per_plugin.get(w['owner'], 0) + (0 if lack else 1)
    for b in got:
        blocks_seen[b] = blocks_seen.get(b, 0) + 1
    for block, key in INPUTS:
        hit = got.get(block)
        val = hit['value'].get(key) if hit and isinstance(hit['value'], dict) else None
        if val is not None:
            src = 'file ' + hit['from']
        elif (block, key) in PRESET:
            src = 'default (hazeType preset)'
        elif block in ('lightning', 'precipitation'):
            src = 'none (the weather has no %s)' % block
        else:
            src = 'generated rung'
        lines.append('%s\t%s\t%s.%s\t%s\t%s' % (w['owner'], w['edid'], block, key,
                                                json.dumps(val, separators=(',', ':')) if val is not None else '-', src))
with open(dump, 'w', encoding='utf-8', newline='\n') as f:
    f.write('\n'.join(lines) + '\n')
print('records %d, resolved with every required block %d; per plugin: %s' % (
    len(wthrs), len(wthrs) - len(missing), ' '.join('%s=%d' % kv for kv in sorted(per_plugin.items()))))
print('blocks: ' + ' '.join('%s=%d' % kv for kv in sorted(blocks_seen.items())))
print('refused %d, conflicts %d, warnings %d' % (len(out['refused']), len(out['conflicts']), len(out['warnings'])))
for m in missing[:10]:
    print('  MISSING ' + m)
for r in (out['refused'] + out['conflicts'] + out['warnings'])[:10]:
    print('  ' + json.dumps(r))
ok = not missing and not out['refused'] and not out['conflicts'] and not out['warnings'] and len(wthrs) == 107

# refuter 1: a record nobody names
fake = run([('Fallout4.esm|0x00FFFFF0', 'NoSuchWeather')])
r1 = key_of('Fallout4.esm|0x00FFFFF0') not in fake['records']
print('refuter 1 (unnamed record reported missing): %s' % ('caught' if r1 else 'NOT CAUGHT'))
# refuter 2: hide one generated file; its record must lose its atmosphere
w0 = next(w for w in wthrs if w['edid'] == 'CommonwealthRain')
hidden.add(os.path.join(mod, 'Plugins', 'Fusion Shaders - Assets', 'Weathers', 'Fallout4',
                        'CommonwealthRain.recx').replace('\\', '/').casefold())
hid = run()
r2 = 'atmosphere' not in hid['records'].get(key_of('%s|%s' % (w0['owner'], w0['lid'])), {})
hidden.clear()
print('refuter 2 (hidden file loses its record): %s' % ('caught' if r2 else 'NOT CAUGHT'))
print('dump %d lines -> %s' % (len(lines) - 1, dump))
ok = ok and r1 and r2
print('CENSUS %s' % ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
