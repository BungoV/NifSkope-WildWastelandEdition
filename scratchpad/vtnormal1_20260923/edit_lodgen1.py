"""VTNORMAL1 edit 1: cache reader returns vectors; the pyramid's normal from
his sheets; the heights-normal plane for the chunk-sheet assembly; half-aux
encode; census. Every anchor asserted unique."""
import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/src/lodgen.cpp'
s = open(P, 'rb').read().decode('utf-8')


def sub(old, new, count=1):
	global s
	n = s.count(old)
	if n != count:
		sys.exit('anchor found %d times, want %d:\n%s' % (n, count, old[:300]))
	s = s.replace(old, new)


# ---- 1. the DDS reader: vectors on request ---------------------------------
sub("""static bool lodgenMsnFromAssembledDds( const QString & path, std::vector<quint32> & out,
	int & w, int & h )
{""", """static bool lodgenMsnFromAssembledDds( const QString & path, std::vector<quint32> * out,
	int & w, int & h, std::vector<float> * vec = nullptr )
{""")
sub("""	out.assign( size_t( w ) * size_t( h ), LODGEN_MSN_FLAT );
	const uchar * src = reinterpret_cast<const uchar *>( px.constData() );
	for ( size_t i = 0; i < size_t( w ) * size_t( h ); i++, src += 4 ) {
		float e = float( src[0] ) / 255.0f * 2.0f - 1.0f;   // R east
		float up = float( src[1] ) / 255.0f * 2.0f - 1.0f;  // G up, his slope
		float n = float( src[2] ) / 255.0f * 2.0f - 1.0f;   // B north
		const float inv = 1.0f / qMax( std::sqrt( e * e + n * n + up * up ), 1e-6f );
		out[i] = lodgenTerrainMsnPixel( Vector3( e * inv, n * inv, up * inv ) );
	}
	return true;
}""", """	/* `vec` (lane VTNORMAL1): the same unit vectors, east north up, handed back
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
}""")

# ---- 2. the cache lookup: one reader, two callers ---------------------------
sub("""static bool lodgenMsnFromCache( const QString & base, std::vector<quint32> & out,
	int & w, int & h )
{
	if ( g_msnCacheDir.isEmpty() )
		return false;
	const QString dds = QDir( g_msnCacheDir ).filePath(
		QFileInfo( base ).fileName() + QStringLiteral( "_msn.DDS" ) );
	if ( QFileInfo( dds ).exists() && lodgenMsnFromAssembledDds( dds, out, w, h ) )
		return true;""", """/*! One cache lookup, `<dir>/<name>_msn.DDS` then `<dir>/<name>.png`, for two
 *  callers: the chunk-sheet writer wants encoded pixels (`out`), the pyramid
 *  wants unit vectors (`vec`, lane VTNORMAL1). Exactly one of the two is set.
 *  The off-circle census belongs to the chunk-sheet writer and only it moves it. */
static bool lodgenMsnCacheRead( const QString & base, std::vector<quint32> * out,
	std::vector<float> * vec, int & w, int & h )
{
	if ( g_msnCacheDir.isEmpty() )
		return false;
	const QString dds = QDir( g_msnCacheDir ).filePath(
		QFileInfo( base ).fileName() + QStringLiteral( "_msn.DDS" ) );
	if ( QFileInfo( dds ).exists() && lodgenMsnFromAssembledDds( dds, out, w, h, vec ) )
		return true;""")
sub("""	w = img.width();
	h = img.height();
	if ( w <= 0 || h <= 0 )
		return false;
	out.assign( size_t( w ) * size_t( h ), LODGEN_MSN_FLAT );
	qint64 offCircle = 0;""", """	w = img.width();
	h = img.height();
	if ( w <= 0 || h <= 0 )
		return false;
	if ( vec )
		vec->assign( size_t( w ) * size_t( h ) * 3, 0.0f );
	else
		out->assign( size_t( w ) * size_t( h ), LODGEN_MSN_FLAT );
	qint64 offCircle = 0;""")
sub("""			e *= inv;
			n *= inv;
			up *= inv;
			out[size_t( y ) * size_t( w ) + size_t( x )] =
				lodgenTerrainMsnPixel( Vector3( e, n, up ) );
		}
	}
	if ( offCircle )
		g_vrMsnCacheRenorm++;
	return true;
}""", """			e *= inv;
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
}

static bool lodgenMsnFromCache( const QString & base, std::vector<quint32> & out,
	int & w, int & h )
{
	return lodgenMsnCacheRead( base, &out, nullptr, w, h );
}""")

# ---- 3. the stage carries the heights normal when the sheets replace it -----
sub("""	std::vector<quint16> height;
	bool cover = false;
};

struct LodgenVtLevel""", """	std::vector<quint16> height;
	/*! The HEIGHTS normal, kept only while bungo's upscaled sheets replace
	 *  `msn` (lane VTNORMAL1) AND the .btr chunk sheets are assembled from this
	 *  staging: the chunk-sheet path reads this plane, so its bytes do not move
	 *  when the pyramid's normal does. Empty otherwise. */
	std::vector<quint32> msnHeights;
	bool cover = false;
};

struct LodgenVtLevel""")

# ---- 4. half-resolution aux sheets in the tile encoder ----------------------
sub("""QByteArray lodgenVtEncodeTile( const LodgenVtStage & st, int stored, int mips, bool withHeight,
	bool withEmissive, bool coverInColor )
{""", """QByteArray lodgenVtEncodeTile( const LodgenVtStage & st, int stored, int mips, bool withHeight,
	bool withEmissive, bool coverInColor, bool halfAux = false )
{
	/* HALF-RESOLUTION AUX SHEETS (lane VTNORMAL1, `--vt-half-aux`): the msn,
	 * mask, height and emissive sheets drop their mip 0 and store the rest,
	 * which is exactly the full sheet's mips 1.. -- the staging and the halving
	 * are untouched, only the top mip is not written. The colour sheet keeps
	 * every mip; the header says which sheets skipped one (descriptor byte 6). */
	auto skip = [halfAux]( int sheet, int m ) { return halfAux && sheet != 0 && m == 0; };""")
sub("""				img = lodgenVtHalve( img, w, h, sheets[s].alpha );
				w /= 2;
				h /= 2;
			}
			lodgenVtEncodeBlocks( img, w, h, sheets[s].bc3, out );""", """				img = lodgenVtHalve( img, w, h, sheets[s].alpha );
				w /= 2;
				h /= 2;
			}
			if ( !skip( s, m ) )
				lodgenVtEncodeBlocks( img, w, h, sheets[s].bc3, out );""")
sub("""				img = lodgenVtHalve16( img, w, h );
				w /= 2;
				h /= 2;
			}
			lodgenVtEncodeR16( img, out );""", """				img = lodgenVtHalve16( img, w, h );
				w /= 2;
				h /= 2;
			}
			if ( !skip( 3, m ) )
				lodgenVtEncodeR16( img, out );""")
sub("""				img = lodgenVtHalve( img, w, h, false );
				w /= 2;
				h /= 2;
			}
			lodgenVtEncodeBlocks( img, w, h, false, out );
		}
	}
	return out;
}""", """				img = lodgenVtHalve( img, w, h, false );
				w /= 2;
				h /= 2;
			}
			if ( !skip( 4, m ) )
				lodgenVtEncodeBlocks( img, w, h, false, out );
		}
	}
	return out;
}""")

# ---- 5. the filter carries the heights normal alongside ---------------------
sub("""		if ( row == childRows.constEnd() || ctx >= int( row->size() ) ) {
			bgra[0] = 0xFF808080U;
			bgra[1] = LODGEN_MSN_FLAT;
			bgra[2] = 0x00FFFFFFU;
			bgra[3] = 0x00FF00FFU;
			bgra[4] = 0xFF000000U;
			*h16 = 32767;
			return;
		}""", """		if ( row == childRows.constEnd() || ctx >= int( row->size() ) ) {
			bgra[0] = 0xFF808080U;
			bgra[1] = LODGEN_MSN_FLAT;
			bgra[2] = 0x00FFFFFFU;
			bgra[3] = 0x00FF00FFU;
			bgra[4] = 0xFF000000U;
			bgra[5] = LODGEN_MSN_FLAT;
			*h16 = 32767;
			return;
		}""")
sub("""		bgra[4] = idx < st.emissive.size() ? st.emissive[idx] : 0xFF000000U;
		*h16 = st.height[idx];
	};""", """		bgra[4] = idx < st.emissive.size() ? st.emissive[idx] : 0xFF000000U;
		bgra[5] = idx < st.msnHeights.size() ? st.msnHeights[idx] : st.msn[idx];
		*h16 = st.height[idx];
	};
	/* The heights normal (lane VTNORMAL1) is a SIXTH plane only while the
	 * children carry one; it filters by the msn's own law below. */
	bool withHeights = false;
	for ( auto it = childRows.constBegin(); it != childRows.constEnd() && !withHeights; ++it )
		for ( const LodgenVtStage & c : *it )
			if ( !c.msnHeights.empty() ) {
				withHeights = true;
				break;
			}
	const int planes = withHeights ? 6 : 5;""")
sub("""	out.height.assign( size_t( stored ) * stored, 32767 );
	out.cover = false;
""", """	out.height.assign( size_t( stored ) * stored, 32767 );
	if ( withHeights )
		out.msnHeights.assign( size_t( stored ) * stored, LODGEN_MSN_FLAT );
	else
		out.msnHeights.clear();
	out.cover = false;
""")
sub("""			quint32 acc[5][4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
				{ 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
			quint32 hAcc = 0;
			for ( int dv = 0; dv < 2; dv++ ) {
				for ( int du = 0; du < 2; du++ ) {
					quint32 px[5];
					quint16 hv = 0;
					sample( u0 + du, v0 + dv, px, &hv );
					for ( int s = 0; s < 5; s++ ) {""", """			quint32 acc[6][4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
				{ 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
			quint32 hAcc = 0;
			for ( int dv = 0; dv < 2; dv++ ) {
				for ( int du = 0; du < 2; du++ ) {
					quint32 px[6];
					quint16 hv = 0;
					sample( u0 + du, v0 + dv, px, &hv );
					for ( int s = 0; s < planes; s++ ) {""")
sub("""				out.msn[o] = lodgenTerrainMsnPixel( Vector3( n[0], n[2], n[1] ) );
			}""", """				out.msn[o] = lodgenTerrainMsnPixel( Vector3( n[0], n[2], n[1] ) );
			}
			if ( withHeights ) {
				float n[3];
				for ( int k = 0; k < 3; k++ )
					n[k] = float( ( acc[5][k] + 2 ) >> 2 ) / 255.0f * 2.0f - 1.0f;
				const float len = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
				if ( len > 1e-6f )
					for ( int k = 0; k < 3; k++ )
						n[k] /= len;
				out.msnHeights[o] = lodgenTerrainMsnPixel( Vector3( n[0], n[2], n[1] ) );
			}""")

# ---- 6. tile bytes for the estimator ----------------------------------------
sub("""static qint64 lodgenVtTileBytes( int stored, int mips, bool coverTile, bool withHeight,
	bool withEmissive = false, bool coverInColor = false )
{
	qint64 n = 0;
	for ( int m = 0; m < mips; m++ ) {
		const qint64 s = stored >> m;
		const qint64 blocks = ( s / 4 ) * ( s / 4 );""", """static qint64 lodgenVtTileBytes( int stored, int mips, bool coverTile, bool withHeight,
	bool withEmissive = false, bool coverInColor = false, bool halfAux = false )
{
	qint64 n = 0;
	for ( int m = 0; m < mips; m++ ) {
		const qint64 s = stored >> m;
		const qint64 blocks = ( s / 4 ) * ( s / 4 );
		if ( halfAux && m == 0 ) {
			// the half-resolution aux sheets store no mip 0; colour does
			n += blocks * ( ( coverTile && coverInColor ) ? 16 : 8 );
			continue;
		}""")
sub("""	const qint64 rawNoCover = lodgenVtTileBytes( stored, opts.mips, false, opts.height,
		false, opts.coverInColor );
	const qint64 rawCover = lodgenVtTileBytes( stored, opts.mips, true, opts.height,
		false, opts.coverInColor );""", """	const qint64 rawNoCover = lodgenVtTileBytes( stored, opts.mips, false, opts.height,
		false, opts.coverInColor, opts.halfAux );
	const qint64 rawCover = lodgenVtTileBytes( stored, opts.mips, true, opts.height,
		false, opts.coverInColor, opts.halfAux );""")

open(P, 'wb').write(s.encode('utf-8'))
print('lodgen edit 1 ok')
