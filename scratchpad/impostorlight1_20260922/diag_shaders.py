# IMPOSTORLIGHT1 -- write INSTRUMENT variants of the mesh and card fragment
# shaders into a run folder's shaders/, from pristine sources.
#
#   python diag_shaders.py <src shaders dir> <dst shaders dir> <stage>
#
# stage (same number in both shaders, so one run photographs one stage):
#   0  pristine (copies the sources byte for byte)
#   1  NORMALS: VIEW-space normal as colour, n*0.5+0.5
#        mesh = the GEOMETRIC normal the bake photographs (btnMatrix_norm[2],
#               flipped for back faces, exactly channel 8 = what the sheet holds)
#        card = the blended card normal taken to view space with the card's own
#               modelViewMatrix (VARIANT picks the multiply order, see below)
#   2  albedo only
#   3  albedo x ambient (card: WITHOUT the baked AO)
#   4  albedo x (ambient + diffuse)          (card: without AO)
#   5  everything before the tonemap         (mesh: + spec/emissive; card: + AO)
#   6  normal output (tonemapped) -- identical to 0 except the define line
# VARIANT env (card, stage 1 only): "mv" = mat3(modelViewMatrix)*n (default),
#   "tmv" = n*mat3(modelViewMatrix), "model" = model-space normal (no transform)
#
# Anchors are asserted exactly once; nothing is written on a miss.
import os, sys, shutil

src, dst, stage = sys.argv[1], sys.argv[2], int(sys.argv[3])
variant = os.environ.get('VARIANT', 'mv')


def rd(p):
    with open(p, 'rb') as f:
        return f.read().decode('utf-8')


def once(s, anchor, new, name):
    n = s.count(anchor)
    if n != 1:
        sys.exit('REFUSED: anchor %r matches %d times in %s' % (anchor[:50], n, name))
    return s.replace(anchor, new)


os.makedirs(dst, exist_ok=True)
for f in os.listdir(src):
    shutil.copyfile(os.path.join(src, f), os.path.join(dst, f))
if stage == 0:
    print('stage 0: pristine copy')
    sys.exit(0)

# ---- mesh -------------------------------------------------------------------
m = rd(os.path.join(src, 'fo4_default.frag'))
m = once(m, '#version 410 core\n', '#version 410 core\n#define WW_DIAG %d\n' % stage, 'mesh')
mesh_block = '''#if WW_DIAG == 1
	fragColor = vec4( normalize( btnMatrix_norm[2] ) * ( gl_FrontFacing ? 1.0 : -1.0 ) * 0.5 + 0.5, color.a ); return;
#elif WW_DIAG == 2
	fragColor = vec4( albedo, color.a ); return;
#elif WW_DIAG == 3
	fragColor = vec4( A.rgb * albedo, color.a ); return;
#elif WW_DIAG == 4
	fragColor = vec4( diffuse * albedo * D.rgb * ( 1.0 - F ) + A.rgb * albedo, color.a ); return;
#elif WW_DIAG == 5
	fragColor = vec4( color.rgb, color.a ); return;
#endif
	color.rgb = tonemap( color.rgb );
'''
m = once(m, '\tcolor.rgb = tonemap( color.rgb );\n', mesh_block, 'mesh')

# ---- card -------------------------------------------------------------------
c = rd(os.path.join(src, 'impostor_oct.frag'))
c = once(c, '#version 410 core\n', '#version 410 core\n#define WW_DIAG %d\n' % stage, 'card')
nv = {'mv': 'normalize( mat3( modelViewMatrix ) * normal )',
      'tmv': 'normalize( normal * mat3( modelViewMatrix ) )',
      'model': 'normal',
      'lit': 'nView'}[variant]   # 'lit' = the FIXED shader's own lighting normal
card_block = '''	vec3 wwAmb0 = sqrt( max( lightSourceAmbient.rgb, vec3( 0.0 ) ) ) * 0.375;
#if WW_DIAG == 1
	fragColor = vec4( %s * 0.5 + 0.5, 1.0 ); return;
#elif WW_DIAG == 2
	fragColor = vec4( colour.rgb, 1.0 ); return;
#elif WW_DIAG == 3
	fragColor = vec4( colour.rgb * wwAmb0, 1.0 ); return;
#elif WW_DIAG == 4
	fragColor = vec4( colour.rgb * ( lit + wwAmb0 ), 1.0 ); return;
#elif WW_DIAG == 5
	fragColor = vec4( colour.rgb * ( lit + ambient ), 1.0 ); return;
#endif
	vec3 lc = colour.rgb * ( lit + ambient );
''' % nv
c = once(c, '\tvec3 lc = colour.rgb * ( lit + ambient );\n', card_block, 'card')

for name, s in (('fo4_default.frag', m), ('impostor_oct.frag', c)):
    with open(os.path.join(dst, name), 'wb') as f:
        f.write(s.encode('utf-8'))
print('stage %d written (card normal variant %s): CR mesh %d card %d' % (
    stage, variant, m.count('\r'), c.count('\r')))
