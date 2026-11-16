// lane VKBUF1 (2026-11-16): the RT GI's texel buffers on the present device by name (vkrtbuf.h). Built into vkpresent.cpp
// after vkfarcards.inc (farTexelBuffer) and vkddgi.inc.
#include "features/vkframe/vkrtbuf.h"
#ifndef WW_VKPRESENT_NO_QT
#include "wwswitch.h"  // wwEnvInt (the red is live)
#include <QDebug>
#include <QString>
#endif
#include <chrono>
#include <cstdlib>

namespace
{
struct RtBufs
{
	char keys[WwVkPresent::kRtBufSlots] = {};  // the farTexelBuffer keys: one static char a slot (never a moving value's address)
	std::function<bool( std::string & )> fresh[WwVkPresent::kRtBufSlots];
	WwVkPresent::RtBufStat st;
	std::uint64_t gen = 0;
	std::string said;
	std::chrono::steady_clock::time_point saidAt {};
};

RtBufs & rtBufs()
{
	static RtBufs * r = new RtBufs;  // never destroyed: the keys outlive every owner
	return *r;
}

constexpr const char * kRtBufNames[WwVkPresent::kRtBufSlots] = { "cellDdgiAtlas", "cellRestir" };

bool rtBufSlotOk( int slot )
{
	return slot >= 0 && slot < WwVkPresent::kRtBufSlots;
}

const FcBuf * rtBufFind( int slot )
{
	const auto & m = farCards().bufs;
	const auto it = m.find( WwVkPresent::rtBufKey( slot ) );
	return it == m.end() || !it->second.buf ? nullptr : &it->second;
}
}  // namespace

namespace WwVkPresent
{

const char * rtBufName( int slot )
{
	return rtBufSlotOk( slot ) ? kRtBufNames[slot] : nullptr;
}

int rtBufSlotOf( const char * name )
{
	for ( int s = 0; name && s < kRtBufSlots; s++ )
		if ( std::strcmp( name, kRtBufNames[s] ) == 0 )
			return s;
	return -1;
}

std::uintptr_t rtBufKey( int slot )
{
	return rtBufSlotOk( slot ) ? reinterpret_cast<std::uintptr_t>( &rtBufs().keys[slot] ) : 0;
}

bool rtBufRed()
{
#ifdef WW_VKPRESENT_NO_QT
	const char * v = std::getenv( "WW_VKVIEW_RTBUF_RED" );
	return v && std::atoi( v ) == 1;
#else
	return wwEnvInt( "WW_VKVIEW_RTBUF_RED" ) == 1;
#endif
}

bool rtBufShare( int slot, unsigned glFmt, std::size_t bytes, unsigned & glBuf, unsigned & glTex, std::string & why )
{
	glBuf = glTex = 0;
	if ( !rtBufSlotOk( slot ) ) {
		why = "no rt buffer slot " + std::to_string( slot );
		return false;
	}
	RtBufs & R = rtBufs();
	if ( rtBufRed() ) {
		why = "red WW_VKVIEW_RTBUF_RED=1";
		R.st.why[slot] = why;
		return false;
	}
	// made / dropped read off farTexelBuffer's own counters: a remade buffer can take the old handle and GL name again
	const bool before = rtBufFind( slot ) != nullptr;
	const int made0 = farCards().st.made;
	if ( !farTexelBuffer( rtBufKey( slot ), glFmt, bytes, glBuf, glTex, why ) ) {
		R.st.why[slot] = why;
		R.st.bytes[slot] = 0;
		if ( before && !rtBufFind( slot ) ) {  // farTexelBuffer freed the old one before the make failed
			R.st.dropped[slot]++;
			R.gen++;
		}
		return false;
	}
	if ( farCards().st.made != made0 ) {
		if ( before )
			R.st.dropped[slot]++;
		R.st.made[slot]++;
		R.gen++;
	}
	R.st.bytes[slot] = bytes;
	return true;
}

bool rtBufCopy( int slot, unsigned srcGlBuf, std::size_t bytes, std::string & why )
{
	const FcBuf * b = rtBufSlotOk( slot ) ? rtBufFind( slot ) : nullptr;
	Gl & g = gl();
	if ( !b || b->glCtx != g.ctx ) {
		why = "no shared buffer of that slot in this context";
		return false;
	}
	if ( !g.copyNamedBufferSubData || !srcGlBuf ) {
		why = "no glCopyNamedBufferSubData or no source buffer";
		return false;
	}
	if ( bytes > b->bytes ) {
		why = "a copy of " + std::to_string( bytes ) + " bytes into a shared buffer of " + std::to_string( std::uint64_t( b->bytes ) );
		return false;
	}
	g.copyNamedBufferSubData( srcGlBuf, b->glBuf, 0, 0, std::intptr_t( bytes ) );
	RtBufs & R = rtBufs();
	R.st.copies[slot]++;
	R.st.copyBytes[slot] += bytes;
	return true;
}

void rtBufDrop( int slot )
{
	if ( !rtBufSlotOk( slot ) )
		return;
	RtBufs & R = rtBufs();
	if ( rtBufFind( slot ) ) {
		R.st.dropped[slot]++;
		R.gen++;
	}
	farTexelBufferDrop( rtBufKey( slot ) );
	R.st.bytes[slot] = 0;
}

void rtBufOwner( int slot, std::function<bool( std::string & why )> fresh )
{
	if ( rtBufSlotOk( slot ) )
		rtBufs().fresh[slot] = std::move( fresh );
}

bool rtBufOf( const char * name, RtBuf & out, std::string & why )
{
	out = RtBuf();
	const int slot = rtBufSlotOf( name );
	if ( slot < 0 ) {
		why = std::string( "no rt buffer named " ) + ( name ? name : "(null)" );
		return false;
	}
	RtBufs & R = rtBufs();
	R.st.lookups[slot]++;
	auto refuse = [&]( const std::string & w ) {
		why = w;
		R.st.refused[slot]++;
		R.st.why[slot] = w;
		rtBufSay();
		return false;
	};
	if ( rtBufRed() )
		return refuse( "red WW_VKVIEW_RTBUF_RED=1" );
	if ( !R.fresh[slot] )
		return refuse( "no owner" );
	std::string w;
	if ( !R.fresh[slot]( w ) )
		return refuse( w );
	const FcBuf * b = rtBufFind( slot );
	if ( !b || !b->view )
		return refuse( "not on the present device (never made, or dropped)" );
	if ( b->glCtx != gl().ctx )
		return refuse( "made in another GL context" );
	out.view = std::uint64_t( reinterpret_cast<std::uintptr_t>( b->view ) );
	out.bytes = std::uint64_t( b->bytes );
	out.texels = out.bytes / fcTexelBytes( b->glFmt );
	out.key = rtBufKey( slot );
	out.glBuf = b->glBuf;
	out.glFmt = b->glFmt;
	out.gen = R.gen;
	rtBufSay();
	return true;
}

std::uint64_t rtBufGen()
{
	return rtBufs().gen;
}

RtBufStat rtBufStat()
{
	return rtBufs().st;
}

std::string rtBufLine()
{
	const RtBufStat & s = rtBufs().st;
	std::string l = "vk rtbuf:";
	for ( int k = 0; k < kRtBufSlots; k++ ) {
		char b[256];
		std::snprintf( b, sizeof b, "%s %s %.1f MB (made %d, dropped %d), lookups %llu, refused %d, copies %llu (%.1f MB)", k ? ";" : "",
			kRtBufNames[k], double( s.bytes[k] ) / 1048576.0, s.made[k], s.dropped[k], static_cast<unsigned long long>( s.lookups[k] ),
			s.refused[k], static_cast<unsigned long long>( s.copies[k] ), double( s.copyBytes[k] ) / 1048576.0 );
		l += b;
		if ( !s.why[k].empty() )
			l += " [last refusal: " + s.why[k] + "]";
	}
	l += rtBufRed() ? "; RED WW_VKVIEW_RTBUF_RED=1" : "; red off";
	return l;
}

void rtBufSay()
{
	RtBufs & R = rtBufs();
	const auto now = std::chrono::steady_clock::now();
	if ( R.saidAt.time_since_epoch().count() != 0 && now - R.saidAt < std::chrono::seconds( 5 ) )
		return;
	std::string l = rtBufLine();
	if ( l == R.said )
		return;
	R.said = std::move( l );
	R.saidAt = now;
#ifdef WW_VKPRESENT_NO_QT
	std::fprintf( stderr, "%s\n", R.said.c_str() );
	std::fflush( stderr );
#else
	qInfo().noquote() << QString::fromStdString( R.said );   // the app's log, like the present's other vk lines
#endif
}

}  // namespace WwVkPresent
