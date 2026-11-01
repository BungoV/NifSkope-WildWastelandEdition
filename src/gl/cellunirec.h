#pragma once

/* lane PERFFIX1 (item 1): the cell-uniform class record (celllights.cpp wwCellLightsUniforms). The first call of a
 * program class in an armed pass records every set and bind in order; a later call of that class replays it onto its
 * own program through the same setters, and runs the program-specific calls (Live / Cluster ops) at their places.
 * A template over the program and vector types, so tests/cellunirec drives it with a fake program. */

#include <cstdint>
#include <cstring>
#include <vector>

namespace WwUniRec
{
struct Op
{
	enum Kind : std::uint8_t { B, I, F, F2, F3, F4, F4L, Bind, Live, Cluster } kind = B;
	const char * name = nullptr;	// a literal of the body, or the format of an indexed name
	int arg = -1;	// F4L: the index (-1 none); Bind: the unit; Live: which call
	int i = 0;	// B / I: the value; Bind: the target; Live: its flag
	unsigned tex = 0;	// Bind: the texture
	float f[4] = { 0, 0, 0, 0 };
	bool same( const Op & o ) const
	{
		return kind == o.kind && arg == o.arg && i == o.i && tex == o.tex && std::memcmp( f, o.f, sizeof f ) == 0
			&& ( name == o.name || ( name && o.name && std::strcmp( name, o.name ) == 0 ) );
	}
};

template <class P, class V4> struct Rec
{
	std::vector<Op> ops;
	bool on = false;	// recording
	bool valid = false;
	const std::vector<float> * clTexels = nullptr;
	int clCount = 0;
	std::uint64_t clStamp = 0;
	void push( const Op & o )
	{
		if ( on )
			ops.push_back( o );
	}
	void b( P * p, const char * n, bool x )
	{
		p->uni1b( n, x );
		Op o; o.kind = Op::B; o.name = n; o.i = x ? 1 : 0; push( o );
	}
	void i( P * p, const char * n, int x )
	{
		p->uni1i( n, x );
		Op o; o.kind = Op::I; o.name = n; o.i = x; push( o );
	}
	void f( P * p, const char * n, float x )
	{
		p->uni1f( n, x );
		Op o; o.kind = Op::F; o.name = n; o.f[0] = x; push( o );
	}
	void f2( P * p, const char * n, float x, float y )
	{
		p->uni2f( n, x, y );
		Op o; o.kind = Op::F2; o.name = n; o.f[0] = x; o.f[1] = y; push( o );
	}
	void f3( P * p, const char * n, float x, float y, float z )
	{
		p->uni3f( n, x, y, z );
		Op o; o.kind = Op::F3; o.name = n; o.f[0] = x; o.f[1] = y; o.f[2] = z; push( o );
	}
	void f4( P * p, const char * n, V4 v )
	{
		p->uni4f( n, v );
		Op o; o.kind = Op::F4; o.name = n; std::memcpy( o.f, &v, sizeof o.f ); push( o );
	}
	void f4l( P * p, const char * n, int k, V4 v )
	{
		p->uni4f_l( k < 0 ? p->uniLocation( n ) : p->uniLocation( n, k ), v );
		Op o; o.kind = Op::F4L; o.name = n; o.arg = k; std::memcpy( o.f, &v, sizeof o.f ); push( o );
	}
	template <class U> void bind( U & u, int unit, unsigned target, unsigned tex )
	{
		u.bind( unit, target, tex );
		Op o; o.kind = Op::Bind; o.arg = unit; o.i = int( target ); o.tex = tex; push( o );
	}
	void live( int which, bool flag = false )
	{
		Op o; o.kind = Op::Live; o.arg = which; o.i = flag ? 1 : 0; push( o );
	}
	void cluster( const std::vector<float> * t, int n, std::uint64_t st )
	{
		clTexels = t; clCount = n; clStamp = st;
		Op o; o.kind = Op::Cluster; o.i = n; o.tex = unsigned( st ); push( o );
	}
};

//! the record onto p: each set through p's own setter, binds through u, live( op ) for the Live and Cluster ops
template <class P, class V4, class U, class L> void replay( const Rec<P, V4> & c, P * p, U & u, L live )
{
	for ( const Op & o : c.ops ) {
		switch ( o.kind ) {
		case Op::B: p->uni1b( o.name, o.i != 0 ); break;
		case Op::I: p->uni1i( o.name, o.i ); break;
		case Op::F: p->uni1f( o.name, o.f[0] ); break;
		case Op::F2: p->uni2f( o.name, o.f[0], o.f[1] ); break;
		case Op::F3: p->uni3f( o.name, o.f[0], o.f[1], o.f[2] ); break;
		case Op::F4: p->uni4f( o.name, V4( o.f[0], o.f[1], o.f[2], o.f[3] ) ); break;
		case Op::F4L:
			p->uni4f_l( o.arg < 0 ? p->uniLocation( o.name ) : p->uniLocation( o.name, o.arg ), V4( o.f[0], o.f[1], o.f[2], o.f[3] ) );
			break;
		case Op::Bind: u.bind( o.arg, unsigned( o.i ), o.tex ); break;
		case Op::Live:
		case Op::Cluster: live( o ); break;
		}
	}
}

//! ops of a and b that differ (a length difference counts once)
inline long long differ( const std::vector<Op> & a, const std::vector<Op> & b )
{
	long long d = a.size() == b.size() ? 0 : 1;
	for ( size_t k = 0; k < a.size() && k < b.size(); k++ )
		d += a[k].same( b[k] ) ? 0 : 1;
	return d;
}
}
