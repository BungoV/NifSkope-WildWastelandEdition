"""FIX1101A: Modern temporal AA ships ON (High) for the user, under a NEW settings key (an Off stored while it shipped
Off must not stick); harness runs without WW_TAA2 still pin Off (deterministic gates); WW_TAA2=off stays the red.
python -I taa2default_src.py <src/features/post/gametaa.cpp>   rc 0 = PASS
"""
import re
import sys

src = open(sys.argv[1], encoding='latin-1').read()
key = re.search(r'kTaa2Key = "([^"]+)"', src)
dflt = re.search(r'constexpr int kTaa2Default = (\d);', src)
reads = 'QSettings().value( QLatin1StringView( kTaa2Key ), kTaa2Default )' in src
harness = re.search(r'else if \( wwHarnessRun\(\) \) \{\s*s\.taa2 = 0;', src) is not None
off_pin = '"off", "low", "medium", "high", "ultra"' in src
checks = {
    'new key (not the shipped-Off one)': bool(key) and key.group(1) != 'Settings/Render/Modern temporal AA',
    'default tier High (3)': bool(dflt) and dflt.group(1) == '3',
    'the settings read takes the default': reads,
    'harness without WW_TAA2 keeps Off': harness,
    'WW_TAA2=off still pins the old resolve': off_pin,
}
for k, v in checks.items():
    print('%-40s %s' % (k, 'ok' if v else 'NO'))
ok = all(checks.values())
print('VERDICT %s' % ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
