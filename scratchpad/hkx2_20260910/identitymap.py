import io

p = 'src/hkxplayback.cpp'
s = io.open(p, encoding='utf-8', newline='').read()

old = """	if ( skeletons.isEmpty() || e.clip.trackToBone.isEmpty() )
		return false;

	int need = 0;
	for ( int b : e.clip.trackToBone )
		need = qMax( need, b + 1 );
"""
new = """	if ( skeletons.isEmpty() )
		return false;

	/* AN EMPTY BINDING IS THE IDENTITY MAP, not a missing one.
	 *
	 * hkaAnimationBinding::transformTrackToBoneIndices is empty in every clip
	 * Bethesda ships (HKX1's census: identity in 13,322 of 13,514, a permutation
	 * in 192) and in third-party clips exported without a binding at all; track
	 * i then drives bone i. Refusing an empty vector here would have refused
	 * almost the whole archive. Accepted only when the skeleton has at least
	 * numTracks bones -- the same rule the fixture lane wrote into the
	 * ww-hkx-animation skill -- and the loop below reads it as
	 * trackToBone.value(t, t), so a short vector falls through to identity too.
	 */
	int need = e.clip.numTracks;
	for ( int b : e.clip.trackToBone )
		need = qMax( need, b + 1 );
"""
assert s.count(old) == 1, s.count(old)
s = s.replace(old, new, 1)
io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('CR=', open(p, 'rb').read().count(b'\r'))
