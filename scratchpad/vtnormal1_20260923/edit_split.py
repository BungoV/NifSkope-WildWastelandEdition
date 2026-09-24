"""VTNORMAL1 edit: the chunk-sheet cache loops back to their rung shape.
Gate (b) found 3 bytes of a 22 MB chunk _msn one LSB off the rung: the shared
loop (vec branch inside) compiles differently under -march=haswell. The encoded
path gets its original loop verbatim; the vector path gets its own loop."""
import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/src/lodgen.cpp'
s = open(P, 'rb').read().decode('utf-8')
assert s.count('\r') == 0


def sub(old, new):
	global s
	n = s.count(old)
	if n != 1:
		sys.exit('anchor found %d times:\n%s' % (n, old[:300]))
	s = s.replace(old, new)


sub("""	/* `vec` (lane VTNORMAL1): the same unit vectors, east north up, handed back
	 * as floats for the pyramid, which box-filters them before it encodes. The
	 * arithmetic is the one below, so the chunk sheet (encoded here) and the
	 * pyramid (encoded after its filter) start from the same vector. */
	if ( vec )
		vec->assign( size_t( w ) * size_t( h ) * 3, 0.0f );
	else
		out->assign( size_t( w ) * size_t( h ), LODGEN_MSN_FLAT );
	const uchar * src = reinterpret_cast<const uchar *>( px.constData() );
	for ( size_t i = 0; i < size_t( w ) * size_t( h ); i++, src += 4 ) {
		float e = float( src[0] ) / 255.0f * 2.0f - 1.0f;   // R east
		float up = float( src[1] ) / 255.0f * 2.0f - 1.0f;  // G up, his slope
		float n = float( src[2] ) / 255.0f * 2.0f - 1.0f;   // B north
		const float inv = 1.0f / qMax( std::sqrt( e * e + n * n + up * up ), 1e-6f );
		if ( vec ) {
			float * o = vec->data() + 3 * i;
			o[0] = e * inv;
			o[1] = n * inv;
			o[2] = up * inv;
		} else {
			( *out )[i] = lodgenTerrainMsnPixel( Vector3( e * inv, n * inv, up * inv ) );
		}
	}
	return true;
}""", """	/* `vec` (lane VTNORMAL1): the same unit vectors, east north up, handed back
	 * as floats for the pyramid, which box-filters them before it encodes. It is
	 * its OWN loop on purpose: one loop with a branch inside compiled the
	 * encoded path differently (-march=haswell) and moved 3 bytes of a 22 MB
	 * chunk sheet by one step, so the chunk-sheet loop below is the rung's,
	 * character for character. */
	if ( vec ) {
		vec->assign( size_t( w ) * size_t( h ) * 3, 0.0f );
		const uchar * vs = reinterpret_cast<const uchar *>( px.constData() );
		for ( size_t i = 0; i < size_t( w ) * size_t( h ); i++, vs += 4 ) {
			const float e = float( vs[0] ) / 255.0f * 2.0f - 1.0f;
			const float up = float( vs[1] ) / 255.0f * 2.0f - 1.0f;
			const float n = float( vs[2] ) / 255.0f * 2.0f - 1.0f;
			const float inv = 1.0f / qMax( std::sqrt( e * e + n * n + up * up ), 1e-6f );
			float * o = vec->data() + 3 * i;
			o[0] = e * inv;
			o[1] = n * inv;
			o[2] = up * inv;
		}
		return true;
	}
	return lodgenMsnEncodeAssembled( px, *out, w, h );
}""")

# the rung's loop, verbatim, as its own function (the reference `out` too)
sub("""static bool lodgenMsnFromAssembledDds( const QString & path, std::vector<quint32> * out,
	int & w, int & h, std::vector<float> * vec = nullptr )
{""", """static bool lodgenMsnEncodeAssembled( const QByteArray & px, std::vector<quint32> & out,
	int w, int h )
{
	out.assign( size_t( w ) * size_t( h ), LODGEN_MSN_FLAT );
	const uchar * src = reinterpret_cast<const uchar *>( px.constData() );
	for ( size_t i = 0; i < size_t( w ) * size_t( h ); i++, src += 4 ) {
		float e = float( src[0] ) / 255.0f * 2.0f - 1.0f;   // R east
		float up = float( src[1] ) / 255.0f * 2.0f - 1.0f;  // G up, his slope
		float n = float( src[2] ) / 255.0f * 2.0f - 1.0f;   // B north
		const float inv = 1.0f / qMax( std::sqrt( e * e + n * n + up * up ), 1e-6f );
		out[i] = lodgenTerrainMsnPixel( Vector3( e * inv, n * inv, up * inv ) );
	}
	return true;
}

static bool lodgenMsnFromAssembledDds( const QString & path, std::vector<quint32> * out,
	int & w, int & h, std::vector<float> * vec = nullptr )
{""")

# PNG path: the rung's loop verbatim for the encoded path, a twin for vectors
sub("""	if ( vec )
		vec->assign( size_t( w ) * size_t( h ) * 3, 0.0f );
	else
		out->assign( size_t( w ) * size_t( h ), LODGEN_MSN_FLAT );
	qint64 offCircle = 0;
	for ( int y = 0; y < h; y++ ) {
		const quint32 * src = reinterpret_cast<const quint32 *>( img.constScanLine( y ) );
		for ( int x = 0; x < w; x++ ) {
			const quint32 p = src[x];
			float e = float( ( p >> 16 ) & 0xFFU ) / 255.0f * 2.0f - 1.0f;   // cache R
			float n = float( ( p >> 8 ) & 0xFFU ) / 255.0f * 2.0f - 1.0f;    // cache G
			const float s = e * e + n * n;
			float up = std::sqrt( qMax( 0.0f, 1.0f - s ) );
			if ( s > 1.0f )
				offCircle++;
			const float len = std::sqrt( e * e + n * n + up * up );
			const float inv = 1.0f / qMax( len, 1e-6f );
			e *= inv;
			n *= inv;
			up *= inv;
			const size_t at = size_t( y ) * size_t( w ) + size_t( x );
			if ( vec ) {
				float * o = vec->data() + 3 * at;
				o[0] = e;
				o[1] = n;
				o[2] = up;
			} else {
				( *out )[at] = lodgenTerrainMsnPixel( Vector3( e, n, up ) );
			}
		}
	}
	if ( offCircle && !vec )
		g_vrMsnCacheRenorm++;
	return true;
}""", """	if ( vec ) {
		// the pyramid's vectors (lane VTNORMAL1): its own loop, see the DDS reader
		vec->assign( size_t( w ) * size_t( h ) * 3, 0.0f );
		for ( int y = 0; y < h; y++ ) {
			const quint32 * vs = reinterpret_cast<const quint32 *>( img.constScanLine( y ) );
			for ( int x = 0; x < w; x++ ) {
				const quint32 p = vs[x];
				const float e = float( ( p >> 16 ) & 0xFFU ) / 255.0f * 2.0f - 1.0f;
				const float n = float( ( p >> 8 ) & 0xFFU ) / 255.0f * 2.0f - 1.0f;
				const float up = std::sqrt( qMax( 0.0f, 1.0f - ( e * e + n * n ) ) );
				const float inv = 1.0f / qMax( std::sqrt( e * e + n * n + up * up ), 1e-6f );
				float * o = vec->data() + 3 * ( size_t( y ) * size_t( w ) + size_t( x ) );
				o[0] = e * inv;
				o[1] = n * inv;
				o[2] = up * inv;
			}
		}
		return true;
	}
	return lodgenMsnEncodePng( img, *out, w, h );
}""")

sub("""static bool lodgenMsnCacheRead( const QString & base, std::vector<quint32> * out,
	std::vector<float> * vec, int & w, int & h )
{""", """/*! The rung's PNG loop, verbatim (the chunk-sheet path; its bytes are gated). */
static bool lodgenMsnEncodePng( const QImage & img, std::vector<quint32> & out, int w, int h )
{
	out.assign( size_t( w ) * size_t( h ), LODGEN_MSN_FLAT );
	qint64 offCircle = 0;
	for ( int y = 0; y < h; y++ ) {
		const quint32 * src = reinterpret_cast<const quint32 *>( img.constScanLine( y ) );
		for ( int x = 0; x < w; x++ ) {
			const quint32 p = src[x];
			float e = float( ( p >> 16 ) & 0xFFU ) / 255.0f * 2.0f - 1.0f;   // cache R
			float n = float( ( p >> 8 ) & 0xFFU ) / 255.0f * 2.0f - 1.0f;    // cache G
			const float s = e * e + n * n;
			float up = std::sqrt( qMax( 0.0f, 1.0f - s ) );
			if ( s > 1.0f )
				offCircle++;
			const float len = std::sqrt( e * e + n * n + up * up );
			const float inv = 1.0f / qMax( len, 1e-6f );
			e *= inv;
			n *= inv;
			up *= inv;
			out[size_t( y ) * size_t( w ) + size_t( x )] =
				lodgenTerrainMsnPixel( Vector3( e, n, up ) );
		}
	}
	if ( offCircle )
		g_vrMsnCacheRenorm++;
	return true;
}

static bool lodgenMsnCacheRead( const QString & base, std::vector<quint32> * out,
	std::vector<float> * vec, int & w, int & h )
{""")
open(P, 'wb').write(s.encode('utf-8'))
print('split ok, CR', s.count('\r'))
