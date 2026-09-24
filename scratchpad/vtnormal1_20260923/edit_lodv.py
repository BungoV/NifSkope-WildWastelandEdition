import sys
R = 'E:/Projects/NifskopeWildWastelandEdition/src/io/'


def sub(path, old, new, count=1):
	b = open(path, 'rb').read()
	s = b.decode('utf-8')
	n = s.count(old)
	if n != count:
		sys.exit('%s: anchor found %d times, want %d:\n%s' % (path, n, count, old[:200]))
	s = s.replace(old, new)
	open(path, 'wb').write(s.encode('utf-8'))


h = R + 'lodvfile.h'
sub(h, """	quint8 role = LODV_ROLE_UNUSED;
	quint8 colorSpace = 0;          //!< 0 linear, 1 sRGB
};""", """	quint8 role = LODV_ROLE_UNUSED;
	quint8 colorSpace = 0;          //!< 0 linear, 1 sRGB
	/*! Descriptor byte 6 (lane VTNORMAL1, 2026-09-23; zero in every file
	 *  before it). How many of the tile's mips this sheet does NOT store, from
	 *  the top: 0 = all `mipCount` of them at `storedTexels`, 1 = a HALF-
	 *  resolution sheet whose first stored mip is `storedTexels / 2` and which
	 *  stores `mipCount - 1` of them. It is exactly the full sheet with mip 0
	 *  dropped, so the border stays a multiple of 4 by rule 12 and the colour
	 *  sheet (always 0) still sets the texel density. */
	quint8 mipSkip = 0;
};""")
sub(h, """//! One sheet's bytes at one mip, for a reader that unpacks a payload.
quint32 lodvSheetMipBytes( const LodvHeaderFields & h, int sheet, int mip, bool cover );""",
"""//! One sheet's bytes at one of ITS stored mips (0 = its first stored mip, which
//! is `storedTexels >> mipSkip` on a side); 0 past its last stored mip.
quint32 lodvSheetMipBytes( const LodvHeaderFields & h, int sheet, int mip, bool cover );
//! The side in texels of one sheet's stored mip `mip` (0 = its first stored one).
int lodvSheetSide( const LodvHeaderFields & h, int sheet, int mip );""")

c = R + 'lodvfile.cpp'
sub(c, """		put8( s + 4, f.sheets[i].role );
		put8( s + 5, f.sheets[i].colorSpace );
	}""", """		put8( s + 4, f.sheets[i].role );
		put8( s + 5, f.sheets[i].colorSpace );
		put8( s + 6, f.sheets[i].mipSkip );
	}""")
sub(c, """		f.sheets[i].role = s[4];
		f.sheets[i].colorSpace = s[5];
	}""", """		f.sheets[i].role = s[4];
		f.sheets[i].colorSpace = s[5];
		f.sheets[i].mipSkip = s[6];
	}""")
sub(c, """	if ( sheet < 0 || sheet >= int( h.sheetCount ) || mip < 0 || mip >= int( h.mipCount ) )
		return 0;
	const quint32 s = quint32( h.storedTexels ) >> mip;""",
"""	if ( sheet < 0 || sheet >= int( h.sheetCount ) || mip < 0
		|| mip + int( h.sheets[sheet].mipSkip ) >= int( h.mipCount ) )
		return 0;
	const quint32 s = quint32( h.storedTexels ) >> ( mip + int( h.sheets[sheet].mipSkip ) );""")
sub(c, """quint32 lodvTileRawBytes( const LodvHeaderFields & h, bool cover )
{""", """int lodvSheetSide( const LodvHeaderFields & h, int sheet, int mip )
{
	if ( sheet < 0 || sheet >= int( h.sheetCount ) || mip < 0 )
		return 0;
	return int( h.storedTexels ) >> ( mip + int( h.sheets[sheet].mipSkip ) );
}

quint32 lodvTileRawBytes( const LodvHeaderFields & h, bool cover )
{""")
sub(c, """			if ( s.colorSpace > 1 )
				return fail( QString( "refused: sheet %1 has colorSpace %2" ).arg( i ).arg( s.colorSpace ) );""",
"""			if ( s.colorSpace > 1 )
				return fail( QString( "refused: sheet %1 has colorSpace %2" ).arg( i ).arg( s.colorSpace ) );
			/* DESCRIPTOR BYTE 6, mipSkip: 0, or 1 on a half-resolution sheet. The
			 * colour sheet sets the texel density and is never halved; a sheet may
			 * not skip every mip the tile has. */
			if ( s.mipSkip > 1 || int( s.mipSkip ) >= int( fd.mipCount ) )
				return fail( QString( "refused: sheet %1 has mipSkip %2 with mipCount %3; it is 0, "
					"or 1 when the tile has at least two mips" ).arg( i ).arg( s.mipSkip ).arg( fd.mipCount ) );
			if ( s.mipSkip && s.role == LODV_ROLE_COLOR )
				return fail( QString( "refused: sheet %1 is the colour sheet and has mipSkip %2; the "
					"colour sheet is always stored at full resolution" ).arg( i ).arg( s.mipSkip ) );""")
sub(c, """			if ( fd.sheets[i].role != 0 || fd.sheets[i].dxgiFormat != 0
				|| fd.sheets[i].dxgiFormatCover != 0 || fd.sheets[i].colorSpace != 0 )""",
"""			if ( fd.sheets[i].role != 0 || fd.sheets[i].dxgiFormat != 0
				|| fd.sheets[i].dxgiFormatCover != 0 || fd.sheets[i].colorSpace != 0
				|| fd.sheets[i].mipSkip != 0 )""")
sub(c, """		out << QString( "sheet %1 role %2 dxgi %3 dxgiWithCover %4 colorSpace %5" )
			.arg( i ).arg( h.sheets[i].role ).arg( h.sheets[i].dxgiFormat )
			.arg( h.sheets[i].dxgiFormatCover ).arg( h.sheets[i].colorSpace );""",
"""		out << QString( "sheet %1 role %2 dxgi %3 dxgiWithCover %4 colorSpace %5" )
			.arg( i ).arg( h.sheets[i].role ).arg( h.sheets[i].dxgiFormat )
			.arg( h.sheets[i].dxgiFormatCover ).arg( h.sheets[i].colorSpace )
			+ ( h.sheets[i].mipSkip ? QString( " mipSkip %1" ).arg( h.sheets[i].mipSkip ) : QString() );""")
print('lodv ok')
