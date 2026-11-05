#ifndef LODGENBAKEMEM_H
#define LODGENBAKEMEM_H

// CARDMEM2: the whole-map bake's memory rules, pure so tests/cardmem can check them without a bake.
#include <cstdint>

//! The texture LRU counts DDSTexture16::size(): texels, 8 bytes each (16-bit RGBA).
constexpr std::int64_t kLodgenTexBytesPerUnit = 8;
//! The bake LRU's default budget in real MB. The old default, 512 Mi units, was 4 GiB real.
constexpr std::int64_t kLodgenTexBudgetMB = 2048;

//! The bake LRU's budget in its units: `mb` > 0 overrides the default; red = the old 512 Mi units.
inline std::int64_t lodgenTexBudgetUnits( std::int64_t mb, bool red )
{
	if ( red )
		return std::int64_t( 512 ) << 20;
	const std::int64_t m = mb > 0 ? mb : kLodgenTexBudgetMB;
	return ( m << 20 ) / kLodgenTexBytesPerUnit;
}

//! The VT land caches keep a cell row only while a later tile row can reach it. Rows run NORTH to south; after
//! row `ty` the next row's north edge is north - (ty+1)*dim and its border reaches one cell past it.
//! True when cell row `cellY` can go. Red: the old rule, which dropped the cells SOUTH of the next row (the ones
//! still to come) and kept every passed one.
inline bool lodgenVtLandDrop( int cellY, int ty, int north, int dim, bool red )
{
	if ( red )
		return cellY < north - ( ty + 2 ) * dim;
	return cellY > north - ( ty + 1 ) * dim + 1;
}

#endif
