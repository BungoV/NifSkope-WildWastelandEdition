"""lane VKFRAME1 round 11: the GL tone program's sources (res/shaders/cell_hdr.vert + features/exposure/cell_hdr.frag) with
the loader's #include expansion (tests/spells/shader_link_check.py), for tests/vkframe/toneview.cpp's GL reference.

    python -I tests/vkframe/toneview_expand.py <out dir>
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

out = sys.argv[1]
# lane VKBLOOM1: + the Fusion Shaders bloom's chain and the exposure measure (the BLOOM arm's GL reference)
POST = ('res', 'shaders', 'features', 'post')
for name, rel in (('cell_hdr.vert.glsl', ('res', 'shaders', 'cell_hdr.vert')),
                  ('cell_hdr.frag.glsl', ('res', 'shaders', 'features', 'exposure', 'cell_hdr.frag')),
                  ('cell_ao.vert.glsl', ('res', 'shaders', 'cell_ao.vert')),
                  ('cell_bloom_src.frag.glsl', POST + ('cell_bloom_src.frag',)),
                  ('cell_bloom_down.frag.glsl', POST + ('cell_bloom_down.frag',)),
                  ('cell_bloom_hold.frag.glsl', POST + ('cell_bloom_hold.frag',)),
                  ('cell_bloom_up.frag.glsl', POST + ('cell_bloom_up.frag',)),
                  ('cell_bloom_comb.frag.glsl', POST + ('cell_bloom_comb.frag',)),
                  ('cell_is_measure_q.frag.glsl', ('res', 'shaders', 'features', 'exposure', 'cell_is_measure_q.frag')),
                  # lane VKFAR2 P9: the lens-flare sprites (the FLARE arm's GL reference)
                  ('cell_lensflare.vert.glsl', POST + ('cell_lensflare.vert',)),
                  ('cell_lensflare.frag.glsl', POST + ('cell_lensflare.frag',))) + tuple(
                  # lane VKFAR2 P7: cellpost.cpp's passes (the POST arm's GL reference)
                  (n + '.frag.glsl', POST + (n + '.frag',)) for n in (
                      'game_dof_down', 'game_dof_blur', 'game_dof', 'game_doflens_prep', 'game_doflens_tile',
                      'game_doflens_gather', 'game_doflens', 'game_taa_mv', 'game_mblur', 'game_dofmblur',
                      'game_doflensmblur', 'cell_objblur_vec', 'cell_objblur_tile', 'cell_objblur', 'game_taa_copy')):
    src = S.expand(os.path.join(TREE, *rel))
    with open(os.path.join(out, name), 'w', newline='\n', encoding='latin-1') as f:
        f.write(src)
    print('expanded %s: %d lines' % (name, src.count('\n') + 1))
