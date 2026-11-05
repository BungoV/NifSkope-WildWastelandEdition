"""lane PERFFIX2 b (item 8): the sun-cascades zone's four parts each have a GPU sub-timer and a CPU sub, named in
cellpasstime.cpp's tables in enum order, and sunshadow.cpp's wwSunShadowPass brackets each call with them.
python -I tests/suntimersplit/check.py <tree> [--red]   (--red: the check on a doctored copy that drops one table name:
must FAIL)."""
import re, sys

root = sys.argv[1]
red = '--red' in sys.argv
h = open(root + '/src/features/shared/cellpasstime.h', encoding='utf-8').read()
c = open(root + '/src/features/shared/cellpasstime.cpp', encoding='utf-8').read()
s = open(root + '/src/features/shadows/sunshadow.cpp', encoding='utf-8').read()
if red:
    c = c.replace('"cloud-shadow", ', '', 1)	# the GPU table loses one name: the names after it shift
fails = 0


def check(ok, what):
    global fails
    print(('ok ' if ok else 'FAIL ') + what)
    fails += 0 if ok else 1


def enum_names(name):
    m = re.search(r'enum ' + name + r' \{(.*?)\};', h, re.S)
    body = re.sub(r'//[^\n]*', '', m.group(1))
    return [x.strip() for x in body.replace('\n', ' ').split(',') if x.strip()]


def table(var):
    m = re.search(r'static const char \* const ' + var + r'\[[A-Za-z]+\] = \{(.*?)\};', c, re.S)
    return re.findall(r'"([^"]+)"', m.group(1))


sub = [x for x in enum_names('Sub') if x != 'SubCount']
cpu = [x for x in enum_names('CpuSub') if x != 'CpuSubCount']
ts, tc = table('subName'), table('cpuSubName')
check(len(sub) == len(ts), 'GPU subs: %d enum names, %d table names' % (len(sub), len(ts)))
check(len(cpu) == len(tc), 'CPU subs: %d enum names, %d table names' % (len(cpu), len(tc)))
new = [('SunCasters', 'sun-casters'), ('SunTerrStep', 'sun-terrstep'), ('CloudShadow', 'cloud-shadow'),
       ('FarGroundPass', 'far-ground-pass')]
for e, n in new:
    check(e in sub and sub.index(e) < len(ts) and ts[sub.index(e)] == n, 'GPU sub %s is named %s' % (e, n))
    ce = 'Cpu' + e
    check(ce in cpu and cpu.index(ce) < len(tc) and tc[cpu.index(ce)] == n, 'CPU sub %s is named %s' % (ce, n))
body = s[s.index('void wwSunShadowPass( Scene * scene )'):]
body = body[:body.index('\n}\n')]
for e, f in (('SunCasters', 'sunShadowPassImpl'), ('SunTerrStep', 'wwShadowSetFrame'), ('CloudShadow', 'wwCloudShadowPass'),
             ('FarGroundPass', 'CellFarGround::pass')):
    check(re.search(r'part\( PassTime::' + e + r', PassTime::Cpu' + e + r', [^\n]*' + re.escape(f), body) is not None,
          'wwSunShadowPass times %s under %s' % (f, e))
check('PassTime::subBegin( gpu )' in body and 'PassTime::subEnd( gpu )' in body and 'DrawCpuScope t( cpu )' in body,
      'each part opens and closes its sub-timers')
print('SUNTIMERSPLIT %s (%d failed)' % ('FAIL' if fails else 'PASS', fails))
sys.exit(1 if fails else 0)
