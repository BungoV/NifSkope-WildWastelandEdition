/* lane PLACED1: see esmplaced.h. Record layouts follow the published plugin
 * format notes (the xEdit definitions); nothing here is engine-derived. */

#include "esmplaced.h"

#include "esmdata.h"
#include "esmfile.hpp"

#include <cstring>

namespace
{

constexpr unsigned int GRUP_TYPE = 0x50555247U;

QString zString( const ESMFile::ESMField & f )
{
	const char * p = reinterpret_cast<const char *>( f.data() );
	size_t n = f.size();
	while ( n > 0 && p[n - 1] == '\0' )
		n--;
	return QString::fromLatin1( p, qsizetype( n ) );
}

float f32At( const unsigned char * p )
{
	float v;
	std::memcpy( &v, p, 4 );
	return v;
}

} // namespace

bool esmDecalBase( const EsmWorld & world, quint32 txstForm, EsmDecalBase & out )
{
	out = EsmDecalBase();
	ESMFile * esm = world.plugin();
	if ( !esm || !txstForm )
		return false;
	const ESMFile::ESMRecord * r = esm->findRecord( txstForm );
	if ( !r || r->type == GRUP_TYPE || !( *r == "TXST" ) )
		return false;
	out.exists = true;
	ESMFile::ESMField f( *esm, *r );
	while ( f.next() ) {
		if ( f == "EDID" ) {
			out.edid = zString( f );
		} else if ( f == "TX00" ) {
			out.diffuse = zString( f );
		} else if ( f == "TX01" ) {
			out.normal = zString( f );
		} else if ( f == "TX07" ) {
			out.spec = zString( f );
		} else if ( f == "MNAM" ) {
			out.material = zString( f );
		} else if ( f == "DODT" && f.size() >= 36 ) {
			const unsigned char * p = f.data();
			out.hasDecalData = true;
			out.minWidth = f32At( p );
			out.maxWidth = f32At( p + 4 );
			out.minHeight = f32At( p + 8 );
			out.maxHeight = f32At( p + 12 );
			out.depth = f32At( p + 16 );
			out.shininess = f32At( p + 20 );
			out.parallaxScale = f32At( p + 24 );
			out.parallaxPasses = p[28];
			out.flags = p[29];
			for ( int k = 0; k < 4; k++ )
				out.color[k] = p[32 + k];
		}
	}
	return true;
}

bool esmRefrDecal( const EsmWorld & world, quint32 refrForm, EsmRefrDecal & out )
{
	out = EsmRefrDecal();
	ESMFile * esm = world.plugin();
	if ( !esm || !refrForm )
		return false;
	const ESMFile::ESMRecord * r = esm->findRecord( refrForm );
	if ( !r || r->type == GRUP_TYPE || !( *r == "REFR" ) )
		return false;
	ESMFile::ESMField f( *esm, *r );
	while ( f.next() ) {
		if ( f == "XPRM" && f.size() >= 32 ) {
			const unsigned char * p = f.data();
			out.hasPrimitive = true;
			for ( int k = 0; k < 3; k++ )
				out.half[k] = f32At( p + 4 * k );
			std::memcpy( &out.primType, p + 28, 4 );
		} else if ( f == "XPDD" && f.size() >= 8 ) {
			const unsigned char * p = f.data();
			out.hasScale = true;
			out.widthScale = f32At( p );
			out.heightScale = f32At( p + 4 );
		}
	}
	return true;
}
