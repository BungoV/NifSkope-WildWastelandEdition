"""The weather .recx generated rung (lane WEATHERRECX1, 2026-11-06): one WTHR record's class -> physical values.

The SAME rule as src/features/weather/weathergen.h (the C++ reader's rung 2); tests/features/weather/weathergen checks
the two agree on every vanilla + DLC weather. Spec: FO4-Extension-Formats recx/SPEC.md "The generated rung".
It reads only the record's EditorID, defining plugin, DATA flags/wind/transition bytes, its precipitation shader's
type and density and whether it has a thunder sound: never a colour, never a fog distance (bungo 11-06).

usage:
  python -I weathergen.py <wthr_list.json> <out root>        write <out root>/<plugin stem>/<EDID>.recx for every weather
  python -I weathergen.py <wthr_list.json> --table <out.tsv>  the rung's values, one row per weather (the gate's fixture)
wthr_list.json is the record list tools/wthr_list.py writes from the masters.
"""
import json
import os
import sys

# class -> visibility m, hazeType, humidity %, coverage, cloud type, density, wind m/s, deg C offset, lightning /min
CLASSES = {
    'clearest':  (80000, 'clean',   45, 0.05, 'cumulus',       0.8,  3,   1, 0),
    'clear':     (30000, 'clean',   55, 0.2,  'cumulus',       1.0,  4,   0, 0),
    'polluted':  (12000, 'urban',   60, 0.3,  'stratocumulus', 1.0,  3,   0, 0),
    'overcast':  (15000, 'clean',   75, 0.95, 'stratus',       1.2,  6,  -2, 0),
    'mist':      (3000,  'clean',   93, 0.7,  'stratus',       1.0,  2,  -3, 0),
    'fog':       (500,   'clean',  100, 0.9,  'stratus',       1.0,  1.5, -4, 0),
    'drizzle':   (2500,  'clean',   97, 1.0,  'stratus',       1.5,  3,  -3, 0),
    'rain':      (5000,  'clean',   95, 1.0,  'stratus',       2.0,  7,  -3, 0),
    'storm':     (4000,  'clean',   95, 1.0,  'cumulonimbus',  2.5, 11,  -3, 4),
    'snow':      (2000,  'clean',   90, 1.0,  'stratus',       1.5,  5, -14, 0),
    'dust':      (8000,  'dust',    30, 0.2,  'cumulus',       1.0,  8,   4, 0),
    'duststorm': (800,   'dust',    20, 0.4,  'cumulus',       1.0, 15,   5, 0),
    'smoke':     (2000,  'smoke',   40, 0.6,  'stratocumulus', 1.5, 10,   6, 0),
    'radstorm':  (2500,  'glowing', 60, 1.0,  'cumulonimbus',  2.5, 14,   3, 6),
}
# region -> base deg C, the hazeType that replaces "clean", groundAlbedo
REGIONS = {
    'commonwealth': (12, 'clean',    0.15),
    'coast':        (7,  'maritime', 0.12),
    'nukaworld':    (14, 'clean',    0.20),
    'glowingsea':   (16, 'glowing',  0.25),
}
HAZE_HEIGHT = {'clear': 1200, 'clearest': 1200, 'polluted': 1200, 'overcast': 1200, 'dust': 1200,
               'duststorm': 2500, 'smoke': 2500, 'radstorm': 2500,
               'rain': 600, 'drizzle': 600, 'storm': 600, 'snow': 600, 'mist': 200, 'fog': 120}
PRECIP_RATE = {'drizzle': 1.0, 'rain': 4.0, 'storm': 8.0, 'snow': 1.0}	# mm/h at the vanilla density 7
WETTING = {'storm': 15, 'rain': 15, 'drizzle': 30, 'snow': 30}
DIM_NIGHT = {'overcast': (0.3, 0.5), 'rain': (0.3, 0.5), 'storm': (0.3, 0.5), 'snow': (0.3, 0.5),
             'radstorm': (0.3, 0.5), 'drizzle': (0.3, 0.5), 'fog': (0.1, 0.3)}
CLEAR_NIGHTS = ('clear', 'clearest', 'dust')
TRANS_MIN, TRANS_MAX = 0.01, 3.0	# fWeatherTransMin (exe), fWeatherTransMax (Fallout4.esm GMST)


def classify(edid, flags, precip_type, thunder):
    """the first rule that holds (SPEC.md "The generated rung" 1)"""
    e = edid.lower().replace('vbfog', '')	# CommonwealthClear_VBFog: a clear weather with volumetric fog, not a fog
    if 'radstorm' in e:
        return 'radstorm'
    if precip_type == 'rain':
        if thunder:
            return 'storm'
        return 'drizzle' if 'mist' in e else 'rain'
    if precip_type == 'snow':
        return 'snow'
    for word, cls in (('nuke', 'smoke'), ('fog', 'fog'), ('mist', 'mist')):
        if word in e:
            return cls
    if 'dust' in e:
        return 'dust' if 'dusty' in e else 'duststorm'	# CommonwealthDusty: a dusty day; DLC04NukaWorldDust: the storm
    for word, cls in (('pollut', 'polluted'), ('overcast', 'overcast'), ('dark', 'overcast'), ('clearest', 'clearest')):
        if word in e:
            return cls
    if flags & 2:
        return 'overcast'
    if flags & (4 | 8):
        return 'overcast'
    return 'clear'


def region_of(edid, owner):
    o = owner.lower()
    if o == 'dlccoast.esm':
        return 'coast'
    if o == 'dlcnukaworld.esm':
        return 'nukaworld'
    if 'commonwealthgs' in edid.lower():
        return 'glowingsea'
    return 'commonwealth'


def r3(x):
    return float('%.3f' % x)


def generate(w):
    """w: one wthr_list.json row -> (class, region, the rung's values as a flat dict)"""
    d = w.get('data', {})
    flags = d.get('flags', 0)
    pr = w.get('precip') or {}
    ptype = pr.get('type')
    pdens = pr.get('density', 7.0)
    thunder = 'thunder' in w.get('sounds', {})
    edid = w.get('edid', '')
    cls = classify(edid, flags, ptype, thunder)
    reg = region_of(edid, w['owner'])
    vis, haze, hum, cov, ctype, dens, wind, toff, freq = CLASSES[cls]
    base, regionHaze, albedo = REGIONS[reg]
    if haze == 'clean':
        haze = regionHaze
    if 'rad' in edid.lower() and cls in ('fog', 'mist'):
        haze = 'glowing'	# Far Harbor's radioactive fog
    v = {'class': cls, 'region': reg}
    v['visibility'] = float(vis)
    v['visibilityNight'] = r3(0.7 * vis) if cls in ('mist', 'fog') else None
    v['hazeHeight'] = float(HAZE_HEIGHT[cls])
    v['ozone'] = 1.0
    v['hazeType'] = haze
    v['humidity'] = float(hum)
    v['groundAlbedo'] = albedo
    v['coverage'] = cov
    v['cloudType'] = ctype
    v['density'] = dens
    speed = wind * (0.5 + d.get('windSpeed', 0) / 255.0)
    turb = (d.get('windTurb') or 0) / 255.0
    a = d.get('windDir', 0) / 255.0 * 360.0
    v['windSpeed'] = r3(speed)
    v['windDirection'] = r3((270.0 - a) % 360.0)
    v['turbulence'] = r3(turb)
    v['gusts'] = r3(speed * (1.0 + turb))
    v['tempDay'] = float(base + toff)
    v['tempDawn'] = float(base + toff - (6 if cls in CLEAR_NIGHTS else 2))
    v['lightning'] = float(freq) if thunder else 0.0
    if ptype in ('rain', 'snow') and cls in PRECIP_RATE:
        v['precipType'] = 'rain' if ptype == 'rain' else 'snow_fine'
        v['precipRate'] = r3(PRECIP_RATE[cls] * pdens / 7.0)
        v['wetting'] = float(WETTING[cls])
    else:
        v['precipType'] = None
        v['precipRate'] = 0.0
        v['wetting'] = 20.0
    v['drying'] = 90.0
    v['transHours'] = r3(d.get('transDelta', 0) / 255.0 * (TRANS_MAX - TRANS_MIN) + TRANS_MIN)
    v['precipStart'] = r3(d.get('precipFadeIn', 0) / 255.0)
    v['precipEnd'] = r3(d.get('precipFadeOut', 0) / 255.0)
    v['lightningStart'] = r3(d.get('thunderFadeIn', 0) / 255.0)
    v['lightningEnd'] = r3(d.get('thunderFadeOut', 0) / 255.0)
    v['stars'], v['moonlight'] = DIM_NIGHT.get(cls, (1.0, 1.0))
    return v


def entry_of(w, v):
    atmo = {'visibility': v['visibility'], 'hazeHeight': v['hazeHeight'], 'ozone': v['ozone'],
            'hazeType': v['hazeType'], 'humidity': v['humidity'], 'groundAlbedo': v['groundAlbedo']}
    if v['visibilityNight'] is not None:
        atmo['hours'] = {'2': {'visibility': v['visibilityNight']}, '14': {'visibility': v['visibility']}}
    e = {'weather': '%s|%s' % (w['owner'], w['lid']), '_editorId': w.get('edid', ''),	# one match key per entry
         '_derivedFrom': derived_note(w, v),
         'atmosphere': atmo,
         'clouds': {'coverage': v['coverage'], 'type': v['cloudType'], 'density': v['density']},
         'night': {'stars': v['stars'], 'moonlight': v['moonlight']},
         'wind': {'speed': v['windSpeed'], 'direction': v['windDirection'], 'gusts': v['gusts'],
                  'turbulence': v['turbulence']},
         'temperature': {'air': v['tempDay'], 'hours': {'5': {'air': v['tempDawn']}, '14': {'air': v['tempDay']}}},
         'lightning': {'frequency': v['lightning']},
         'transitions': {'duration': v['transHours'], 'precipitationStart': v['precipStart'],
                         'precipitationEnd': v['precipEnd'], 'lightningStart': v['lightningStart'],
                         'lightningEnd': v['lightningEnd']}}
    p = {'wetting': v['wetting'], 'drying': v['drying']}
    if v['precipType']:
        p['layers'] = [{'type': v['precipType'], 'rate': v['precipRate']}]
    e['precipitation'] = p
    return e


def derived_note(w, v):
    d = w.get('data', {})
    fl = d.get('flags', 0)
    cls = ''.join(c for c, b in (('Pleasant', 1), ('Cloudy', 2), ('Rainy', 4), ('Snow', 8)) if fl & b) or 'none'
    pr = w.get('precip')
    bits = ['generated rung: class %s, region %s' % (v['class'], v['region']),
            'EditorID %s (%s)' % (w.get('edid', ''), w['owner']), 'DATA flags %s' % cls,
            'MNAM %s %s density %s' % (pr.get('edid'), pr.get('type'), pr.get('density')) if pr else 'no MNAM',
            'thunder sound' if 'thunder' in w.get('sounds', {}) else 'no thunder sound',
            'DATA wind %d dir %d turb %s trans %d' % (d.get('windSpeed', 0), d.get('windDir', 0), d.get('windTurb'),
                                                      d.get('transDelta', 0)),
            'no colour or fog distance read (src/features/weather/weathergen.py)']
    return '; '.join(bits)


def main():
    rows = json.load(open(sys.argv[1]))
    if sys.argv[2] == '--table':
        keys = None
        with open(sys.argv[3], 'w', newline='\n') as f:
            for w in rows:
                v = generate(w)
                if keys is None:
                    keys = list(v.keys())
                    f.write('\t'.join(['owner', 'lid', 'edid', 'flags', 'windSpeed', 'windDir', 'windTurb', 'transDelta',
                                       'precipIn', 'precipOut', 'thunderIn', 'thunderOut', 'precipType', 'precipDensity',
                                       'thunder'] + keys) + '\n')
                d = w.get('data', {})
                pr = w.get('precip') or {}
                ins = [w['owner'], w['lid'], w.get('edid', ''), d.get('flags', 0), d.get('windSpeed', 0),
                       d.get('windDir', 0), d.get('windTurb') or 0, d.get('transDelta', 0), d.get('precipFadeIn', 0),
                       d.get('precipFadeOut', 0), d.get('thunderFadeIn', 0), d.get('thunderFadeOut', 0),
                       pr.get('type') or '-', pr.get('density', 7.0), int('thunder' in w.get('sounds', {}))]
                outs = ['-' if v[k] is None else v[k] for k in keys]
                f.write('\t'.join(str(x) for x in ins + outs) + '\n')
        print('table: %d weathers -> %s' % (len(rows), sys.argv[3]))
        return
    out_root = sys.argv[2]
    made, kept, per = 0, 0, {}
    for w in rows:
        v = generate(w)
        stem = os.path.splitext(w['owner'])[0]
        folder = os.path.join(out_root, stem)
        os.makedirs(folder, exist_ok=True)
        path = os.path.join(folder, w.get('edid', w['lid']) + '.recx')
        doc = {'_': 'Fusion Shaders weather file (SPEC.md 1.3): every condition in physical units. Edit freely; '
                    'the values below are the generated rung written out (lane WEATHERRECX1).',
               'entries': [entry_of(w, v)]}
        text = json.dumps(doc, indent=1) + '\n'
        if os.path.exists(path):
            if open(path, encoding='utf-8').read() != text:
                kept += 1
                print('kept (exists, differs): %s' % path)
            continue
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)
        made += 1
        per[stem] = per.get(stem, 0) + 1
    print('written %d, kept %d: %s' % (made, kept, ', '.join('%s %d' % kv for kv in sorted(per.items()))))


if __name__ == '__main__':
    main()
