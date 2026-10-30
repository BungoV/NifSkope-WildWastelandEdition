# lane FOGHEIGHT2: judge the run.sh arms. JUDGE_X = the arm set judged as the fix (new | base), JUDGE_LOOKS = the
# celllooks.cpp whose row default is judged. Exit 0 only when every check passes (the red, X = base, must fail).
import sys, os, math, re
O = sys.argv[1]
X = os.environ.get('JUDGE_X', 'new')
LOOKS = os.environ.get('JUDGE_LOOKS', '')
UPM = 69.93007  # game units per metre (cellvolair.cpp kUnitsPerKm / 1000)
fails, oks = [], []


def rd(n):
    p = os.path.join(O, n + '.txt')
    return open(p, 'rb').read() if os.path.isfile(p) else b''


def parse(b):
    out = []
    for line in b.decode().splitlines():
        m = re.match(r'(\S+) (\S+) \| med (\S+) (\S+) (\S+) \| layer (\S+) (\S+) (\S+) (\S+) \| ground (\S+) (\S+) \| recx (.*)$', line)
        if not m:
            continue
        v = [float.fromhex(x) for x in m.groups()[2:11]]
        out.append(dict(edid=m.group(1), hour=float(m.group(2)), s0=v[0], zb=v[1], invH=v[2], w=v[3], zn=v[4],
                        invHn=v[5], s=v[6], zg=v[7], invHg=v[8], recx=m.group(12)))
    return out


def B(z, z0, ih):
    return min(1.0, math.exp(-(z - z0) * ih)) if ih > 0 else 1.0


def sig(c, z):  # sigma_t per unit (the air's density law, cellvolair.cpp VaLayer comment)
    r = (1 - c['w']) * B(z, c['zb'], c['invH']) + c['w'] * B(z, c['zn'], c['invHn'])
    return c['s0'] * (r + c['s'] * B(z, c['zg'], c['invHg']))


def check(name, ok, why=''):
    (oks if ok else fails).append(name + ('' if ok else ': ' + why))


def key(c):
    return (c['edid'], c['hour'])


# 1. row OFF is today's code, bit for bit, on every root
for r in ('ship', 'empty', 'prec', 'thick'):
    a, b = rd('%s_%s_off' % (X, r)), rd('base_%s_off' % r)
    check('off==base %s (%d cases)' % (r, b.count(b'\n')), b.count(b'\n') > 0 and a == b, 'differs')

# 2. the shipped FO4CS files (4-12 m skins): with the row on every vanilla case climbs (derived over the thinner file)
game = {key(c): c for c in parse(rd('base_empty_off'))}      # the game's air alone (no file, no layer)
skin = {key(c): c for c in parse(rd('base_ship_off'))}       # today: the shipped skin
H = [0, 10, 50, 100, 200, 500]
ship = parse(rd('%s_ship_on' % X))
check('ship on cases', len(ship) == len(game) and len(ship) > 0, '%d vs %d' % (len(ship), len(game)))
print('density (row ON, shipped files), sigma per metre at metres above the anchor; skin = today (file wins)')
for c in ship:
    k = key(c)
    if k not in game or k not in skin:
        check('case %s %g present' % k, False, 'missing')
        continue
    g, s = game[k], skin[k]
    t = [sig(c, c['zg'] + h * UPM) * UPM for h in H]
    ts = [sig(s, s['zg'] + h * UPM) * UPM for h in H]
    print('%-26s h=%5.2f %s' % (k[0], k[1], c['recx'][:110]))
    print('   on   ' + '  '.join('%4dm %.3e' % (h, v) for h, v in zip(H, t)) + '  r500/r50 %.3f' % (t[-1] / t[2]))
    print('   skin ' + '  '.join('%4dm %.3e' % (h, v) for h, v in zip(H, ts)) + '  r500/r50 %.3f' % (ts[-1] / ts[2]))
    check('derived over the thinner file %s %g' % k, 'over a thinner file' in c['recx'] and c['s'] > 0, c['recx'][:80])
    g0 = sig(g, g['zg']) * UPM
    check('street = the game %s %g' % k, abs(t[0] - g0) <= 1e-5 * g0, '%.6e vs %.6e' % (t[0], g0))
    check('falls with height %s %g' % k, all(t[i + 1] <= t[i] for i in range(5)) and t[-1] < 0.9 * t[0],
          ' '.join('%.3e' % v for v in t))
    check('climbs: 500 m vs 50 m thinner than today %s %g' % k, t[-1] / t[2] < ts[-1] / ts[2] and t[-1] / t[2] < 0.9,
          '%.3f vs %.3f' % (t[-1] / t[2], ts[-1] / ts[2]))

# 3. a file at least as thick as the derived layer (a weather mod's own fog) still wins
check('thick file wins', rd('%s_thick_on' % X) == rd('%s_thick_off' % X) and rd('%s_thick_on' % X).count(b'\n') > 0,
      'ON moved the thick file')
for c in parse(rd('%s_thick_on' % X)):
    check('thick rung %s %g' % key(c), c['recx'].startswith('wildcard:') and 'derived' not in c['recx'], c['recx'][:60])

# 4. the precedence fixture: the rungs still name the file they replaced (form for Foggy, wildcard for the rest)
for c in parse(rd('%s_prec_on' % X)):
    want = 'form:' if c['edid'] == 'CommonwealthFoggy' else 'wildcard:'
    check('prec rung %s %g %s' % (key(c) + (want,)), want in c['recx'] and 'over a thinner file' in c['recx'], c['recx'][:90])

# 5. no file: the derived rung on every case (FOGHEIGHT1's law, unchanged)
for c in parse(rd('%s_empty_on' % X)):
    check('empty derived %s %g' % key(c), c['recx'].startswith('derived(') and c['s'] > 0, c['recx'][:60])
check('empty on == base empty on', rd('%s_empty_on' % X) == rd('base_empty_on'), 'the no-file law moved')

# 6. burn-off reaches the derived layer: noon burnt away
for c in parse(rd('%s_ship_onburn' % X)):
    if c['hour'] == 12.0:
        check('burn-off noon %s' % c['edid'], c['s'] == 0.0 or 'derived' not in c['recx'], '%g %s' % (c['s'], c['recx'][:70]))

# 7. the red keeps FOGHEIGHT1's precedence (any file wins): red == off on the shipped files
if X == 'new':
    check('red filewins == off', rd('new_ship_onred') == rd('new_ship_off') and rd('new_ship_off').count(b'\n') > 0,
          'red does not restore the old precedence')

# 8. the row ships ON (its new settings key, so an old stored OFF does not stick)
src = open(LOOKS, encoding='utf-8').read() if LOOKS and os.path.isfile(LOOKS) else ''
m = re.search(r'\{ "Ground fog from weather", "([^"]+)"(, (true|false))? \}', src)
check('row default on', bool(m) and m.group(3) == 'true', m.group(0) if m else 'row not found')

print('PASS %d FAIL %d (judged %s)' % (len(oks), len(fails), X))
for f in fails[:200]:
    print('FAIL', f)
print('VERDICT', 'PASS' if not fails else 'FAIL')
sys.exit(0 if not fails else 1)
