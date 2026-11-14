"""lane FO4MATCH1 (2026-11-14): the BC7 encoder's GLSL as the runtime compiles it, for tests/hlslmatch/cullmatch.sh bc7.
src/features/far-lod/lodgengpu.cpp bc7Source() replaces %TABLES% in res/shaders/features/far-lod/lodgen_bc7.glsl with the
vendored detex partition tables (bptc-tables.c under lib/); this writes the same text, and checks that the HLSL
twin (res/shaders/hlsl/far-lod/lodgen_bc7_cs.hlsl) holds the same tables.
  python -I -X utf8 bc7tables.py <bptc-tables.c> <lodgen_bc7.glsl> <out .comp> <lodgen_bc7_cs.hlsl>
Last line: BC7TABLES OK | BC7TABLES FAIL <why> (exit 1)."""
import re
import sys

tab, glsl, out, twin = sys.argv[1:5]
c = open(tab, encoding='utf-8').read()


def table(name):
    m = re.search(r'\b' + name + r'\[[^\]]*\]\s*=\s*\{(.*?)\};', c, re.S)
    return [int(x) for x in re.findall(r'\d+', m.group(1))]


P2 = table('detex_bptc_table_P2')
AN = table('detex_bptc_table_anchor_index_second_subset')
if len(P2) != 1024 or len(AN) != 64:
    sys.exit('BC7TABLES FAIL table sizes %d %d' % (len(P2), len(AN)))
p2sub = [sum(1 << i for i in range(16) if P2[pt * 16 + i]) for pt in range(64)]
# bc7Source(): "const uint P2SUB[64] = uint[64](" "%1%2u"... ");\nconst int ANCHOR2[64] = int[64](" ... ");\n"
t = 'const uint P2SUB[64] = uint[64](' + ','.join('%du' % b for b in p2sub) + ');\n'
t += 'const int ANCHOR2[64] = int[64](' + ','.join('%d' % a for a in AN) + ');\n'
g = open(glsl, encoding='utf-8', newline='').read()
if g.count('%TABLES%') != 1:
    sys.exit('BC7TABLES FAIL %TABLES% not once in ' + glsl)
open(out, 'w', encoding='utf-8', newline='').write(g.replace('%TABLES%', t))
h = open(twin, encoding='utf-8').read()


def hl(name):
    m = re.search(r'static const \w+ ' + name + r'\[64\] = \{([^}]*)\};', h)
    return [int(x) for x in re.findall(r'\d+', m.group(1))] if m else None


if hl('P2SUB') != p2sub or hl('ANCHOR2') != AN:
    sys.exit('BC7TABLES FAIL the twin\'s P2SUB / ANCHOR2 differ from the detex tables')
print('BC7TABLES OK')
