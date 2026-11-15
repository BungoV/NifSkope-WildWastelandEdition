"""lane WEATHERRECX3 (bungo 11-14 "Old recx need an adjustment"): carry the ORIGINAL weather .recx (SPEC 1.4 per-plugin
files written by WEATHERRECX1, and the 1.7 Precip type files) into the 1.8 format with every authored value kept.

Allowed: key renames and unit conversions that keep the physical value. No value key the old files lacked is added
(the 1.8 reader leaves those unset for a weather a file sets, as the 1.4 reader did: weatherrecx.cpp rungLater), no
hours.* keyframes. Nothing of the old file is dropped: unknown keys pass through as they are. --record-from carries the
WEATHERRECX2 files' vanillaRecord block (the WTHR record as stored: data for later, nothing resolves from it).

usage: python -I migrate.py <original Weathers root> <out Weathers root> [--record-from <WEATHERRECX2 Weathers root>]
"""
import json, os, sys

VERSION = '1.8'
HEAD = ('Fusion Shaders weather file (SPEC.md 1.8): every condition in physical units; the values are the 1.4 file\'s, '
        'carried to the 1.8 names and units (lane WEATHERRECX3, tests/features/weather/weatherrecx3/migrate.py). Edit freely.')

# old key (block, key) -> new path inside the entry; same unit unless SCALE names it
MOVES = {
    ('atmosphere', 'hazeHeight'): ('atmosphere', 'haze', 'height'),
    ('atmosphere', 'hazeType'): ('atmosphere', 'haze', 'type'),
    ('atmosphere', 'hazeAlbedo'): ('atmosphere', 'haze', 'albedo'),
    ('atmosphere', 'hazeAnisotropy'): ('atmosphere', 'haze', 'anisotropy'),
    ('transitions', 'precipitationStart'): ('precipitation', 'fadeIn'),
    ('transitions', 'precipitationEnd'): ('precipitation', 'fadeOut'),
    ('transitions', 'lightningStart'): ('lightning', 'fadeIn'),
    ('transitions', 'lightningEnd'): ('lightning', 'fadeOut'),
}
# 1.8 units: precipitation.wetting / drying hours (were minutes), lightning.distance metres (was km)
SCALE = {('precipitation', 'wetting'): 1.0 / 60.0, ('precipitation', 'drying'): 1.0 / 60.0,
         ('lightning', 'distance'): 1000.0}


def put(d, path, v):
    for k in path[:-1]:
        d = d.setdefault(k, {})
    d[path[-1]] = v


def scaled(block, key, v):
    f = SCALE.get((block, key))
    if f is None or not isinstance(v, (int, float)) or isinstance(v, bool):
        return v
    return v * f


def migrate_block(block, src, out, top):
    """src: one block (or one of its hours); out: where its keys go; top: the entry (moves across blocks land there)"""
    for k, v in src.items():
        if k == 'hours' and isinstance(v, dict):
            hrs = out.setdefault('hours', {})
            for h, hv in v.items():
                ho = hrs.setdefault(h, {})
                # an hour's cross-block move stays inside this block's hour (none in the shipped files)
                migrate_block(block, hv, ho, None)
            continue
        mv = MOVES.get((block, k))
        if mv is not None and mv[0] != block:
            if top is None:
                raise SystemExit('cross-block move inside hours: %s.%s' % (block, k))
            put(top, mv, scaled(mv[0], mv[-1], v))
        elif mv is not None:
            put(out, mv[1:], scaled(block, mv[-1], v))
        else:
            out[k] = scaled(block, k, v)


def migrate_entry(e):
    out = {}
    for k, v in e.items():
        if k == 'physicalWeather':
            out['fromRecord'] = not v	# 1.8: physicalWeather true = fromRecord false (recxphysical.h)
        elif isinstance(v, dict) and k not in ('_derivedFrom', 'type'):
            blk = out.setdefault(k, {})
            migrate_block(k, v, blk, out)
        else:
            out[k] = v
    # an emptied block (transitions holding only fades) stays as {} only if it was the old file's: drop none
    return out


def migrate_file(j):
    out = {}
    if '_' in j:
        out['_'] = HEAD if 'entries' in j else j['_']
    out['version'] = VERSION
    for k, v in j.items():
        if k in ('_', 'version'):
            continue
        if k == 'entries':
            out['entries'] = [migrate_entry(e) for e in v]
        else:
            out[k] = v
    return out


def carry_record(rel, m, recfrom):
    """the WEATHERRECX2 file's vanillaRecord block (every WTHR field as stored, spec part 8: data for later, not a value
    the reader resolves) is carried onto the same weather's entry; the regenerated values themselves are not"""
    p = os.path.join(recfrom, rel)
    if not os.path.isfile(p):
        return 0
    got = {e.get('weather'): e.get('vanillaRecord') for e in json.load(open(p, encoding='utf-8')).get('entries', [])}
    n = 0
    for e in m.get('entries', []):
        vr = got.get(e.get('weather'))
        if vr is not None and 'vanillaRecord' not in e:
            e['vanillaRecord'] = vr
            n += 1
    return n


def main():
    args = sys.argv[1:]
    recfrom = None	# --record-from <Weathers>: carry each entry's vanillaRecord from that (the WEATHERRECX2) set
    if '--record-from' in args:
        i = args.index('--record-from')
        recfrom = args[i + 1]
        del args[i:i + 2]
    src, dst = args[0], args[1]
    n = nrec = 0
    for dp, dn, fn in os.walk(src):
        for f in sorted(fn):
            if not f.endswith('.recx'):
                continue
            rel = os.path.relpath(os.path.join(dp, f), src)
            j = json.load(open(os.path.join(dp, f), encoding='utf-8'))
            if not any('precipitationType' in e for e in j.get('entries', [])):
                m = migrate_file(j)
                if recfrom:
                    nrec += carry_record(rel, m, recfrom)
                txt = json.dumps(m, indent=1, ensure_ascii=False) + '\n'
            else:	# a Precip type file (1.7): only the version moves; its layout (tabs) is kept
                raw = open(os.path.join(dp, f), encoding='utf-8', newline='').read()
                txt = raw.replace('"version": "1.7"', '"version": "%s"' % VERSION, 1)
            p = os.path.join(dst, rel)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, 'w', encoding='utf-8', newline='') as o:
                o.write(txt)
            n += 1
    print('migrated', n, 'files ->', dst, '; vanillaRecord carried on', nrec, 'entries')


if __name__ == '__main__':
    main()
