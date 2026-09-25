# CARDFIX1 step 6 (IMPOSTORWIND1 job 3, sway A), part 1: the RAW vertex alpha reaches the bake.
# renderer.cpp forces the colour's alpha to 1 on a tree-animation shape (vertexColorOverride), so C.a
# is 1 exactly where the wind weight lives. The vertex stage passes the attribute's own alpha on
# separately; channel 11 (the bake's mask source, only the bake renders it) writes
# vec3( leaf, treeAnim ? rawA : 0, 0 ). The mask reads only red, so the mask is unchanged; the
# lit render never reads the new varying. Both shaders are LF-only.
ROOT = 'E:/Projects/NifskopeWWE-cardfix1/res/shaders/'


def patch(name, edits):
    b = open(ROOT + name, 'rb').read()
    assert b.count(b'\r') == 0, name
    s = b.decode('utf-8')
    for old, new in edits:
        c = s.count(old)
        assert c == 1, (name, old[:60], c)
        s = s.replace(old, new)
    out = s.encode('utf-8')
    assert out.count(b'\r') == 0
    open(ROOT + name, 'wb').write(out)
    print('patched', name)


patch('fo4_default.vert', [
    ('out vec4 C;\nflat out vec4 D;\n',
     'out vec4 C;\nflat out vec4 D;\n'
     '// the vertex colour\'s OWN alpha, before vertexColorOverride forces it to 1 on a tree-\n'
     '// animation shape: the wind weight W the impostor bake reads (channel 11 G, CARDFIX1 step 6)\n'
     'out float rawVertexAlpha;\n'),
    ('\tC = mix( vertexColor, vertexColorOverride, greaterThan( vertexColorOverride, vec4( 0.0 ) ) );\n',
     '\tC = mix( vertexColor, vertexColorOverride, greaterThan( vertexColorOverride, vec4( 0.0 ) ) );\n'
     '\trawVertexAlpha = vertexColor.a;\n'),
])

patch('fo4_default.frag', [
    ('in vec4 C;\nflat in vec4 D;\n',
     'in vec4 C;\nflat in vec4 D;\n'
     'in float rawVertexAlpha;\t// the vertex colour\'s own alpha (channel 11 G only)\n'),
    ('\t\t\tfloat leaf = lodMaskByTree ? ( lodTreeAnim ? 1.0 : 0.0 ) : ( alphaFlags > 0 ? 1.0 : 0.0 );\n'
     '\t\t\tv = vec3( leaf );\n',
     '\t\t\tfloat leaf = lodMaskByTree ? ( lodTreeAnim ? 1.0 : 0.0 ) : ( alphaFlags > 0 ? 1.0 : 0.0 );\n'
     '\t\t\t/* G: the WIND WEIGHT W (IMPOSTORWIND1, sway A): the raw vertex alpha on a\n'
     '\t\t\t * tree-animation shape -- the only wind input the game\'s tree vertex\n'
     '\t\t\t * shader reads -- and 0 on any other shape, which the game never moves.\n'
     '\t\t\t * The mask reads only R. */\n'
     '\t\t\tv = vec3( leaf, lodTreeAnim ? rawVertexAlpha : 0.0, 0.0 );\n'),
])
