import io

p = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/hkxanim_decode.py'
s = io.open(p, encoding='utf-8', newline='').read()

old_v = ('        if len(b["transformTrackToBoneIndices"]) != anim[0]["numberOfTransformTracks"]:\n'
         '            raise Refusal("binding maps %d tracks, the animation has %d"\n'
         '                          % (len(b["transformTrackToBoneIndices"]), anim[0]["numberOfTransformTracks"]))\n')
new_v = ('        # An EMPTY transformTrackToBoneIndices is the IDENTITY map, not a\n'
         '        # missing one (lane FIXTURE, 2026-09-10, measured on the Mixamo clip\n'
         '        # Running_To_Slide_And_Back_To_Running.hkx: 95 tracks, binding count 0\n'
         '        # with no local fixup). src/hkxanim.cpp holds the same rule, so the two\n'
         '        # decoders stay a matched pair. A non-empty vector of the wrong length\n'
         '        # is still refused by name.\n'
         '        if b["transformTrackToBoneIndices"] and \\\n'
         '                len(b["transformTrackToBoneIndices"]) != anim[0]["numberOfTransformTracks"]:\n'
         '            raise Refusal("binding maps %d tracks, the animation has %d"\n'
         '                          % (len(b["transformTrackToBoneIndices"]), anim[0]["numberOfTransformTracks"]))\n')

old_t = '                    bone = bind["transformTrackToBoneIndices"][t] if bind else t\n'
new_t = ('                    # identity when there is no binding, or an empty one\n'
         '                    tb = bind["transformTrackToBoneIndices"] if bind else []\n'
         '                    bone = tb[t] if t < len(tb) else t\n')

for old, new, what in ((old_v, new_v, 'validate'), (old_t, new_t, 'tsv track->bone')):
    if new in s:
        print(what, 'already patched')
        continue
    assert s.count(old) == 1, (what, s.count(old))
    s = s.replace(old, new, 1)
    print(what, 'patched')

io.open(p, 'w', encoding='utf-8', newline='').write(s)
d = open(p, 'rb').read()
print('CR =', d.count(b'\r'))
