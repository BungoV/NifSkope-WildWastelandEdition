"""The weather .recx as a translation of its vanilla WTHR record (lane MODULES1 part 24, 2026-11-07; SPEC.md 1.5).

bungo 11-07: "you need to translate that weather record file from vanilla to recx". Every field of the record goes
either to the physical key it means or, as stored, to the entry's `vanillaRecord` block (SPEC.md 1.5); the class table of
weathergen.py (the generated rung) is only the fallback for a key the record does not give.

The models (each one is the whole conversion; nothing else is read from the record into a physical key):
  visibility   FO4 fog: f(d) = max * clamp((d - near) / (far - near), 0, 1) ^ power (FNAM, game units; 1 u =
               0.0142875 m). The physical haze is f(d) = 1 - exp(-s d), visibility V = 3.912 / s (Koschmieder, 2 %).
               s is the least-squares fit of the two curves over d in [0, far] (64 samples, golden search on ln s),
               day and night each; night goes to `hours` 2, day to 14 (the generator's hours), when they differ > 5 %.
  hazeHeight   FO4 height fog: full density to `mid`, a linear ramp to none over `range` above it. The exponential
               haze with the same vertical column: H = max(mid, 0) + range / 2 (day near pair).
  hazeAlbedo   the record's day fogFar colour (sRGB -> linear), normalised to its largest channel, times the largest
               single-scatter albedo of the haze type at the file's humidity (weathergen.h humidityGrowth): the sun of
               the physical sky is white, so the record's tint lives in what the haze gives back. No drawn colour.
  precipitation, wind, lightning, transitions: the generated rung already reads these from the record's MNAM/SPGD and
               DATA bytes (weathergen.py generate); they are noted per key in `_derivedFrom`.
Each block names the keys the translation set in `_translated` (the reader's red WW_WEATHER_TRANSLATE_RED=1 drops
them, so the generated rung draws: the files as they were before 1.5).

usage:
  python -I weathertranslate.py <wthr_list.json> <wthr_full.json> <out root> [--old <old root> <diff.tsv>]
wthr_list.json: notes/weatherrecx1/wthr_list.json (the rung's input); wthr_full.json: notes/modules1/tools/wthr_full.py.
"""
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import weathergen as G  # noqa: E402

UNIT = 0.0142875	# m per game unit
VIS_MIN, VIS_MAX = 100.0, 400000.0	# weatherrecx.cpp kVisMin / kVisMax
HH_MIN, HH_MAX = 20.0, 20000.0
HAZE_ALBEDO = {'clean': (0.97, 0.97, 0.97), 'maritime': (0.99, 0.99, 0.99), 'urban': (0.82, 0.82, 0.82),
               'dust': (0.97, 0.92, 0.80), 'smoke': (0.86, 0.84, 0.80), 'glowing': (0.88, 0.97, 0.82)}	# weathergen.h


def r3(x):
    return float('%.3f' % x)


def vanilla_fog(d, near, far, power, mx):
    if far <= near:
        return mx if d >= far else 0.0
    t = min(1.0, max(0.0, (d - near) / (far - near)))
    return mx * t ** max(power, 1e-4)


def fit_visibility(near_u, far_u, power, mx):
    """least-squares s of 1 - exp(-s d) against the vanilla curve over [0, far]; -> visibility m"""
    near, far = near_u * UNIT, far_u * UNIT
    if far <= 0.0 or mx <= 0.0:
        return VIS_MAX
    ds = [far * (i + 0.5) / 64.0 for i in range(64)]
    fv = [vanilla_fog(d, near, far, power, mx) for d in ds]

    def err(ls):
        s = math.exp(ls)
        return sum((1.0 - math.exp(-s * d) - f) ** 2 for d, f in zip(ds, fv))
    a, b = math.log(1e-6), math.log(1.0)
    g = (math.sqrt(5.0) - 1.0) / 2.0
    for _ in range(80):
        c, e = b - g * (b - a), a + g * (b - a)
        if err(c) < err(e):
            b = e
        else:
            a = c
    s = math.exp((a + b) / 2.0)
    return min(VIS_MAX, max(VIS_MIN, 3.912 / s))


def haze_height(mid_u, range_u):
    return min(HH_MAX, max(HH_MIN, (max(mid_u, 0.0) + max(range_u, 0.0) / 2.0) * UNIT))


def srgb_lin(c):
    c = c / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def grown_albedo(haze, humidity):
    rh = min(99.0, max(0.0, humidity)) / 100.0
    f = ((1.0 - rh) / 0.5) ** -0.25
    return [1.0 - (1.0 - a) / f for a in HAZE_ALBEDO.get(haze, HAZE_ALBEDO['clean'])]


def haze_albedo(rgb, haze, humidity):
    lin = [srgb_lin(c) for c in rgb]
    m = max(lin)
    if m <= 0.0:
        return None
    top = max(grown_albedo(haze, humidity))
    return [r3(min(1.0, max(0.0, top * c / m))) for c in lin]


def metres(fog):
    return {k: (r3(v * UNIT) if ('Near' in k or 'Far' in k or 'Height' in k) and 'Scale' not in k else v)
            for k, v in fog.items()}


def record_block(f):
    """every field of the record that no physical key takes, as stored (distances in m; SPEC.md 1.5 `vanillaRecord`)"""
    rec = {'formVersion': f.get('formVersion'), 'definedBy': f.get('owner'), 'winner': f.get('winner')}
    if 'fog' in f:
        rec['fog'] = metres(f['fog'])
    for k in ('fogScale', 'colors', 'cloudTextures', 'cloudLayersMax', 'cloudDisabledMask', 'cloudColors',
              'cloudSpeed', 'directionalAmbient', 'imageSpaces', 'precipitationForm', 'visualEffect', 'aurora',
              'sounds', 'skyStatics', 'godRays', 'visibilityMult', 'volatilityMult'):
        if k in f:
            rec[k] = f[k]
    d = dict(f.get('data', {}))
    d.pop('flags', None)
    rec['data'] = d
    return rec


def translate(w, f):
    """w: the wthr_list row (rung input), f: the wthr_full row -> (entry, diag)"""
    v = G.generate(w)
    e = G.entry_of(w, v)
    fog = f.get('fog', {})
    der = {}
    atmo = e['atmosphere']
    tr_atmo = []
    diag = {'class': v['class']}
    if fog and fog.get('dayFar', 0) > 0:
        vd = fit_visibility(fog['dayNear'], fog['dayFar'], fog['dayPower'], fog['dayMax'])
        vn = fit_visibility(fog['nightNear'], fog['nightFar'], fog['nightPower'], fog['nightMax'])
        vd, vn = float('%.0f' % vd), float('%.0f' % vn)
        atmo['visibility'] = vd
        tr_atmo.append('visibility')
        der['atmosphere.visibility'] = ('FNAM day near %.0f far %.0f u power %.3g max %.3g -> least-squares '
                                        'Koschmieder fit' % (fog['dayNear'], fog['dayFar'], fog['dayPower'],
                                                             fog['dayMax']))
        atmo.pop('hours', None)
        if abs(vn - vd) > 0.05 * vd:
            atmo['hours'] = {'2': {'visibility': vn}, '14': {'visibility': vd}}
            tr_atmo.append('hours')
            der['atmosphere.hours'] = ('FNAM night near %.0f far %.0f u power %.3g max %.3g -> hour 2; day -> hour 14'
                                       % (fog['nightNear'], fog['nightFar'], fog['nightPower'], fog['nightMax']))
        hh = float('%.0f' % haze_height(fog['dayNearHeightMid'], fog['dayNearHeightRange']))
        atmo['hazeHeight'] = hh
        tr_atmo.append('hazeHeight')
        der['atmosphere.hazeHeight'] = ('FNAM day near-height mid %.0f range %.0f u -> column-equal scale height'
                                        % (fog['dayNearHeightMid'], fog['dayNearHeightRange']))
        diag.update(visDay=vd, visNight=vn, hazeHeight=hh)
    ff = f.get('colors', {}).get('fogFar', {}).get('day')
    if ff:
        ha = haze_albedo(ff, atmo['hazeType'], atmo['humidity'])
        if ha:
            atmo['hazeAlbedo'] = ha
            tr_atmo.append('hazeAlbedo')
            der['atmosphere.hazeAlbedo'] = ('NAM0 fogFar day sRGB %s -> linear tint x the %s haze albedo at %g %%'
                                            % (ff, atmo['hazeType'], atmo['humidity']))
            diag['hazeAlbedo'] = ha
    if tr_atmo:
        atmo['_translated'] = tr_atmo
    for k in ('hazeType', 'humidity', 'groundAlbedo', 'ozone'):
        der.setdefault('atmosphere.' + k, 'generated rung (class %s, region %s): the record has no such field'
                       % (v['class'], v['region']))
    if 'hours' not in atmo:
        der['atmosphere.hours'] = 'none: the record night fog is within 5 % of the day'
    pf = f.get('precipitationForm')
    der['precipitation'] = ('record MNAM %s %s density %s (generated rung rate)' % (
        pf.get('edid'), pf.get('typeName'), pf.get('particleDensity')) if pf else
        'record MNAM NULL: the record has no precipitation form, so none is drawn')
    d = f.get('data', {})
    der['wind'] = 'record DATA wind speed %s dir %s turbulence %s (generated rung speeds)' % (
        d.get('windSpeed'), d.get('windDir'), d.get('windTurb'))
    der['transitions'] = 'record DATA trans %s, precip fade %s/%s, thunder fade %s/%s' % (
        d.get('transDelta'), d.get('precipFadeIn'), d.get('precipFadeOut'), d.get('thunderFadeIn'),
        d.get('thunderFadeOut'))
    der['lightning'] = ('record SNAM thunder sound -> class frequency' if 'thunder' in f.get('sounds', {}) else
                        'record has no thunder sound -> none')
    for k in ('clouds', 'night', 'temperature'):
        der[k] = 'generated rung (class %s, region %s); the record cloud art and colours are in `vanillaRecord`' % (
            v['class'], v['region']) if k == 'clouds' else 'generated rung (class %s, region %s)' % (
            v['class'], v['region'])
    der['vanillaRecord'] = 'every other WTHR field as stored (NAM0, FNAM in m, NAM4, PNAM/JNAM, QNAM/RNAM, DALC, IMSP, ...)'
    e['_derivedFrom'] = der
    e['vanillaRecord'] = record_block(f)
    return e, diag


def flat(o, p=''):
    if isinstance(o, dict):
        out = {}
        for k, x in o.items():
            out.update(flat(x, p + '.' + k if p else k))
        return out
    return {p: o}


def main():
    a = sys.argv[1:]
    rows = json.load(open(a[0], encoding='utf-8'))
    full = {(r['owner'], r['lid']): r for r in json.load(open(a[1], encoding='utf-8'))}
    out_root = a[2]
    old = a[a.index('--old') + 1] if '--old' in a else None
    diff_path = a[a.index('--old') + 2] if '--old' in a else None
    made, per, diffs, missing = 0, {}, [], []
    for w in rows:
        f = full.get((w['owner'], w['lid']))
        if f is None:
            missing.append(w.get('edid'))
            continue
        e, diag = translate(w, f)
        stem = os.path.splitext(w['owner'])[0]
        folder = os.path.join(out_root, stem)
        os.makedirs(folder, exist_ok=True)
        name = w.get('edid', w['lid']) + '.recx'
        doc = {'_': 'Fusion Shaders weather file (SPEC.md 1.5): the vanilla WTHR record translated to physical units; '
                    '`vanillaRecord` keeps every field as stored. Edit freely (lane MODULES1, '
                    'src/features/weather/weathertranslate.py).',
               'entries': [e]}
        with open(os.path.join(folder, name), 'w', encoding='utf-8', newline='\n') as fh:
            fh.write(json.dumps(doc, indent=1) + '\n')
        made += 1
        per[stem] = per.get(stem, 0) + 1
        if old:
            op = os.path.join(old, stem, name)
            o = json.load(open(op, encoding='utf-8'))['entries'][0] if os.path.exists(op) else {}
            fo, fn = flat({k: x for k, x in o.items() if not k.startswith('_')}), \
                flat({k: x for k, x in e.items() if not k.startswith('_') and k != 'vanillaRecord'})
            ch = ['%s %s->%s' % (k, fo.get(k), fn.get(k)) for k in sorted(set(fo) | set(fn))
                  if fo.get(k) != fn.get(k) and not k.endswith('_translated')]
            diffs.append((stem, w.get('edid'), diag.get('class'), len(ch), '; '.join(ch)))
    if diff_path:
        with open(diff_path, 'w', encoding='utf-8', newline='\n') as fh:
            fh.write('plugin\tedid\tclass\tchanged\tfields old->new (record block not counted: all new)\n')
            for r in diffs:
                fh.write('\t'.join(str(x) for x in r) + '\n')
    print('translated %d (%s); no full record: %s' % (made, ', '.join('%s %d' % kv for kv in sorted(per.items())),
                                                       missing or 'none'))


if __name__ == '__main__':
    main()
