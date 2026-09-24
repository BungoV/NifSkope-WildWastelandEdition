# CARDFIX1 step 6: lodgen_octahedral.sh asserted "the .lodm is a lodm 1 <family> card". Its fixture is a tree,
# so since sway A (lodm 2, docs/LODGEN_LODM_FORMAT.md 3.3) its card is lodm 2 / sway "model" -- s6 run: 115 ok,
# 1 FAIL on exactly that line. The premise is stale, not the writer. The check now ties the version to the
# bake's OWN sidecar line: `sway model` -> lodm 2 with sway "model"; `sway synthetic` (or an older sidecar with
# no line) -> lodm 1 with no sway key. A lodm 2 without a model sidecar still fails, so does a lodm 1 on one.
# LF-only file.
P = 'E:/Projects/NifskopeWWE-cardfix1/tests/spells/lodgen_octahedral.sh'
b = open(P, 'rb').read(); assert b.count(b'\r') == 0
s = b.decode('utf-8')
old = ("check('the .lodm is a lodm 1 %s card' % fam, lm.get('lodm') == 1 and lm.get('family') == fam and lm.get('kind') == 'card')\n")
assert s.count(old) == 1, s.count(old)
new = ("swayLine = [l.split() for l in open(f'{d}/{ident}.txt').read().splitlines() if l.startswith('sway ')]\n"
       "swayWord = swayLine[0][1] if swayLine else 'synthetic'\n"
       "wantVer = 2 if swayWord == 'model' else 1\n"
       "check('the .lodm is a lodm %d %s card (the sidecar says sway %s; lodm 2 = model sway)' % (wantVer, fam, swayWord),\n"
       "      lm.get('lodm') == wantVer and lm.get('family') == fam and lm.get('kind') == 'card'\n"
       "      and ( lm.get('sway') == 'model' if wantVer == 2 else 'sway' not in lm ))\n")
s = s.replace(old, new)
out = s.encode('utf-8'); assert out.count(b'\r') == 0
open(P, 'wb').write(out); print('patched')
