import re, subprocess
t = open('pics_depth/A_depth.log', encoding='utf-8', errors='replace').read()
rx = re.compile(r'water depth probe (-?\d+),(-?\d+): body (\d+) water ([\d.-]+), ground ([\d.-]+) at full-rate sample (\d+),(\d+), depth ([\d.-]+) units, band \W*([A-Za-z0-9 ]+)')
rows = [m.groups() for m in rx.finditer(t)]
print(len(rows), 'probe lines parsed')
pick = {}
for r in rows:
    if r[2] == '1' and r[8] not in pick:
        pick[r[8]] = r
for r in list(pick.values())[:3]:
    out = subprocess.run(['python', '../../tests/spells/lodl_open_authority.py',
                          'bk_new_def/FO4CSLOD/Commonwealth/Commonwealth.lodl', 'height', r[5], r[6]],
                         capture_output=True, text=True)
    print('probe %s,%s body %s water %s ground %s depth %s band [%s] | authority: %s'
          % (r[0], r[1], r[2], r[3], r[4], r[7], r[8].strip(), (out.stdout.strip() or out.stderr.strip())[-200:]))
