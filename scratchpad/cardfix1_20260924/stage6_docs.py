# CARDFIX1: stage the two docs for the STEP 6 commit WITHOUT step 6b's hunks (fix29, the N8-default ruling),
# which landed in the working tree before step 6 was committed. Reads fix29's own (old, new) pairs, undoes them
# in memory, and writes that text to the INDEX only (git hash-object -w + update-index). The working tree is
# not touched; step 6b's commit then stages the files whole.
import subprocess
R = 'E:/Projects/NifskopeWWE-cardfix1/'
pairs = {}
src = open(R + 'scratchpad/cardfix1_20260924/fix29_n8_default.py', encoding='utf-8').read()
src = src.replace('\ndef patch(rel, pairs):', '\ndef _unused(rel, pairs):')
g = {'patch': lambda rel, ps: pairs.setdefault(rel, []).extend(ps), '__name__': 'stage'}
exec(compile(src, 'fix29', 'exec'), g)
for rel in ('docs/LODGEN_LODM_FORMAT.md', 'docs/LODGEN_CARD_SHEETS.md'):
    s = open(R + rel, 'rb').read().decode('utf-8')
    for old, new in pairs[rel]:
        assert s.count(new) == 1, (rel, new[:60])
        s = s.replace(new, old)
    b = s.encode('utf-8'); assert b.count(b'\r') == 0
    sha = subprocess.run(['git', '-C', R, 'hash-object', '-w', '--stdin'], input=b,
                         capture_output=True, check=True).stdout.decode().strip()
    subprocess.run(['git', '-C', R, 'update-index', '--cacheinfo', '100644,%s,%s' % (sha, rel)], check=True)
    print('staged step-6 text of', rel, sha[:10])
