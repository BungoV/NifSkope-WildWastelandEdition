import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/src/lodtsheets.cpp'


def sub(s, old, new, count=1):
	n = s.count(old)
	if n != count:
		sys.exit('anchor found %d times, want %d:\n%s' % (n, count, old[:200]))
	return s.replace(old, new)


s = open(P, 'rb').read().decode('utf-8')
s = sub(s, """	const quint32 bytes = lodvSheetMipBytes( d->h, sheet, 0, cover );
	const int dim = int( d->h.storedTexels );
	const int blocks = ( dim + 3 ) / 4;""", """	const quint32 bytes = lodvSheetMipBytes( d->h, sheet, 0, cover );
	/* A HALF-RESOLUTION sheet (descriptor byte 6, mipSkip 1) stores its first
	 * mip at storedTexels / 2. It is decoded at its own size and handed back at
	 * storedTexels by nearest texel, so every caller keeps one grid. */
	const int full = int( d->h.storedTexels );
	const int dim = lodvSheetSide( d->h, sheet, 0 );
	const int blocks = ( dim + 3 ) / 4;""")
s = sub(s, """			for ( int i = 0; i < 16; i++ ) {
				const int x = bx * 4 + ( i & 3 ), y = by * 4 + ( i >> 2 );
				if ( x < dim && y < dim )
					out[size_t( y ) * size_t( dim ) + size_t( x )] = blk[i];
			}
		}
	}
	return true;
}""", """			for ( int i = 0; i < 16; i++ ) {
				const int x = bx * 4 + ( i & 3 ), y = by * 4 + ( i >> 2 );
				if ( x < dim && y < dim )
					out[size_t( y ) * size_t( dim ) + size_t( x )] = blk[i];
			}
		}
	}
	if ( dim != full && dim > 0 ) {
		std::vector<quint8> small;
		small.swap( out );
		out.assign( size_t( full ) * size_t( full ), 255 );
		for ( int y = 0; y < full; y++ )
			for ( int x = 0; x < full; x++ )
				out[size_t( y ) * size_t( full ) + size_t( x )] =
					small[size_t( y * dim / full ) * size_t( dim ) + size_t( x * dim / full )];
	}
	return true;
}""")
s = sub(s, """		const QByteArray head = ddsDx10Header( int( d->h.storedTexels ),
			int( d->h.storedTexels ), fmt, bytes );""", """		// a half-resolution sheet (mipSkip 1) is its own, smaller picture; the
		// border and content fractions are the same, so the UVs do not move
		const QByteArray head = ddsDx10Header( lodvSheetSide( d->h, sheet, 0 ),
			lodvSheetSide( d->h, sheet, 0 ), fmt, bytes );""")
open(P, 'wb').write(s.encode('utf-8'))
print('sheets ok')
