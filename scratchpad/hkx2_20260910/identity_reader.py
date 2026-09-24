import io

root = 'E:/Projects/NifskopeWildWastelandEdition/'

# ---------------------------------------------------------------- header ----
p = root + 'src/hkxanim.h'
s = io.open(p, encoding='utf-8', newline='').read()
old = ('\t//! track i drives bone trackToBone[i] of the skeleton named above\n'
       '\t//! (hkaAnimationBinding::transformTrackToBoneIndices)\n'
       '\tQVector<int> trackToBone;\n')
new = ('\t//! track i drives bone trackToBone[i] of the skeleton named above\n'
       '\t//! (hkaAnimationBinding::transformTrackToBoneIndices)\n'
       '\tQVector<int> trackToBone;\n'
       '\t//! true when the file gave no mapping and the identity map was used:\n'
       '\t//! either there is no hkaAnimationBinding at all, or its\n'
       '\t//! transformTrackToBoneIndices is EMPTY (third-party clips). Names the\n'
       '\t//! serving arm, so a consumer never reports a derived map as a stored one.\n'
       '\tbool trackToBoneIsIdentity = false;\n')
if new not in s:
    assert s.count(old) == 1, ('header anchor', s.count(old))
    s = s.replace(old, new, 1)
    io.open(p, 'w', encoding='utf-8', newline='').write(s)
    print('hkxanim.h patched')
else:
    print('hkxanim.h already patched')

# ---------------------------------------------------------------- reader ----
p = root + 'src/hkxanim.cpp'
s = io.open(p, encoding='utf-8', newline='').read()

old_v = ('\t\tif ( b.trackToBone.size() != anim->numTransformTracks )\n'
         '\t\t\trefuse( QString( "binding maps %1 tracks, the animation has %2" )'
         '.arg( b.trackToBone.size() ).arg( anim->numTransformTracks ) );\n')
new_v = ('\t\t/* AN EMPTY transformTrackToBoneIndices IS THE IDENTITY MAP, not a\n'
         '\t\t * missing one (lane FIXTURE, 2026-09-10, measured on the Mixamo\n'
         '\t\t * Collection clip Running_To_Slide_And_Back_To_Running.hkx: 95\n'
         '\t\t * tracks, binding count 0 with no local fixup -- the array really is\n'
         '\t\t * absent, it is not a parse failure). The engine\'s own consumers read\n'
         '\t\t * track i as bone i in that case, and frame 0\'s per-track translation\n'
         '\t\t * against skeleton.hkx\'s reference pose confirms it: 75 of 95 within\n'
         '\t\t * 1e-3 at shift 0, only 18 at any other shift.\n'
         '\t\t *\n'
         '\t\t * The reader therefore ACCEPTS an empty vector and fills the identity\n'
         '\t\t * map in decodeClip(), flagging HkxAnimClip::trackToBoneIsIdentity so\n'
         '\t\t * the derived map is never reported as a stored one. A non-empty\n'
         '\t\t * vector of the wrong length is still refused by name. Whether the\n'
         '\t\t * skeleton is big enough for an identity map -- at least numTracks\n'
         '\t\t * bones -- is the CONSUMER\'s gate (HkxPlayback::bind), because a clip\n'
         '\t\t * file usually carries no skeleton at all. */\n'
         '\t\tif ( !b.trackToBone.isEmpty() && b.trackToBone.size() != anim->numTransformTracks )\n'
         '\t\t\trefuse( QString( "binding maps %1 tracks, the animation has %2" )'
         '.arg( b.trackToBone.size() ).arg( anim->numTransformTracks ) );\n')
if new_v not in s:
    assert s.count(old_v) == 1, ('validate anchor', s.count(old_v))
    s = s.replace(old_v, new_v, 1)

old_d = ('\tif ( b ) {\n'
         '\t\tclip.originalSkeletonName = b->originalSkeletonName;\n'
         '\t\tclip.blendHint = b->blendHint;\n'
         '\t\tclip.trackToBone = b->trackToBone;\n'
         '\t} else {\n'
         '\t\tclip.blendHint = "NORMAL";\n'
         '\t\tfor ( int t = 0; t < a.numTransformTracks; t++ )\n'
         '\t\t\tclip.trackToBone.append( t );\n'
         '\t}\n')
new_d = ('\tif ( b ) {\n'
         '\t\tclip.originalSkeletonName = b->originalSkeletonName;\n'
         '\t\tclip.blendHint = b->blendHint;\n'
         '\t\tclip.trackToBone = b->trackToBone;\n'
         '\t} else {\n'
         '\t\tclip.blendHint = "NORMAL";\n'
         '\t}\n'
         '\t// The identity fallback, for both arms: no binding at all, or a binding\n'
         '\t// whose transformTrackToBoneIndices is empty. Named in the clip, never\n'
         '\t// silent (CONSTITUTION 10, "a fallback is never a silent downgrade").\n'
         '\tif ( clip.trackToBone.isEmpty() && a.numTransformTracks > 0 ) {\n'
         '\t\tclip.trackToBoneIsIdentity = true;\n'
         '\t\tfor ( int t = 0; t < a.numTransformTracks; t++ )\n'
         '\t\t\tclip.trackToBone.append( t );\n'
         '\t}\n')
if new_d not in s:
    assert s.count(old_d) == 1, ('decodeClip anchor', s.count(old_d))
    s = s.replace(old_d, new_d, 1)

io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('hkxanim.cpp CR =', open(p, 'rb').read().count(b'\r'))
print('hkxanim.h  CR =', open(root + 'src/hkxanim.h', 'rb').read().count(b'\r'))
