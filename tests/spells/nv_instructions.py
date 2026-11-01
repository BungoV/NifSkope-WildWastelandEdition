"""The fragment instruction count of an NVIDIA program binary (tests/spells/shader_link_check.py --save-binary).

    python -I nv_instructions.py <bin> [--limit 65536]
Prints `fp N headroom H (P %)` for the !!NVfp section. The NV driver keeps each stage's NV_gpu_program5 assembly in
the binary; an instruction = a statement line ending ';' that is not a declaration (flow ops count). The limit is
GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB (65536 on 610.74). Lane PBRMCSMHEADROOM, after SHADOWSHIMMER4's nvasm.py.
"""
import re, sys

b = open(sys.argv[1], 'rb').read()
limit = int(sys.argv[sys.argv.index('--limit') + 1]) if '--limit' in sys.argv else 65536
DECL = re.compile(rb'^\s*(?:FLAT\s+|CENTROID\s+|NOPERSPECTIVE\s+|SHORT\s+|LONG\s+)*'
                  rb'(?:OPTION|PARAM|TEMP|ATTRIB|OUTPUT|ADDRESS|BUFFER|CBUFFER|TEXTURE|SHARED|IMAGE)\b')
fp = None
for m in re.finditer(rb'!!NVfp', b):
    e = b.find(b'\x00', m.start())
    lines = [l.strip() for l in b[m.start():e if e > 0 else len(b)].split(b'\n')]
    fp = len([l for l in lines if l.endswith(b';') and not DECL.match(l)])
if fp is None:
    print('fp none')
    sys.exit(2)
print('fp %d headroom %d (%.3f %%)' % (fp, limit - fp, 100.0 * (limit - fp) / limit))
