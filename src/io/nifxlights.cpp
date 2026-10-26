#include "io/nifxlights.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>

namespace nifxl {

namespace {

constexpr std::size_t kMaxMembers = 4096;
constexpr int kMaxDepth = 32;
constexpr int kMaxSections = 64;
constexpr int kStackDepth = 1000;	// deeper than this cannot be parsed safely: too-deep

const double kPi = 3.14159265358979323846;

bool isNote( const std::string & key )
{
	const std::string f = fold( key );
	return ( !f.empty() && f[0] == '_' ) || f == "comment";
}

std::string pointer( const std::string & parent, const std::string & key )
{
	std::string out = parent + "/";
	for ( char c : key ) {
		if ( c == '~' )
			out += "~0";
		else if ( c == '/' )
			out += "~1";
		else
			out += c;
	}
	return out;
}

// Code points of UTF-8 (lone surrogates kept as WTF-8 decode the same way).
std::vector<std::uint32_t> codePoints( const std::string & s )
{
	std::vector<std::uint32_t> out;
	for ( std::size_t i = 0; i < s.size(); ) {
		unsigned char c = (unsigned char)s[i];
		int n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
		std::uint32_t cp = n == 1 ? c : n == 2 ? ( c & 0x1F ) : n == 3 ? ( c & 0x0F ) : ( c & 0x07 );
		for ( int k = 1; k < n && i + k < s.size(); ++k )
			cp = ( cp << 6 ) | ( (unsigned char)s[i + k] & 0x3F );
		out.push_back( cp );
		i += n;
	}
	return out;
}

int editDistance( const std::string & a8, const std::string & b8 )
{
	const auto a = codePoints( a8 ), b = codePoints( b8 );
	std::vector<int> row( b.size() + 1 );
	for ( std::size_t j = 0; j <= b.size(); ++j )
		row[j] = int( j );
	for ( std::size_t i = 1; i <= a.size(); ++i ) {
		int prev = row[0];
		row[0] = int( i );
		for ( std::size_t j = 1; j <= b.size(); ++j ) {
			int cur = std::min( { row[j] + 1, row[j - 1] + 1, prev + ( a[i - 1] != b[j - 1] ) } );
			prev = row[j];
			row[j] = cur;
		}
	}
	return row[b.size()];
}

// ---- reading one file

bool skipComment( const std::string & d, std::size_t at, std::size_t & after )
{
	after = at;
	if ( d.compare( at, 2, "//" ) == 0 ) {
		std::size_t e = d.find( '\n', at );
		after = e == std::string::npos ? d.size() : e;
	} else if ( d.compare( at, 2, "/*" ) == 0 ) {
		std::size_t e = d.find( "*/", at + 2 );
		if ( e == std::string::npos )
			return false;	// a comment that never ends
		after = e + 2;
	}
	return true;
}

// Comments and trailing commas dropped outside strings (the reference reader's strict_json).
bool strictJson( const std::string & d, std::string & out )
{
	std::size_t at = 0, n = d.size();
	int last = -1;
	long comma = -1;
	while ( at < n ) {
		const char c = d[at];
		if ( c == '"' ) {
			std::size_t end = at + 1;
			while ( end < n && d[end] != '"' )
				end += d[end] == '\\' ? 2 : 1;
			out.append( d, at, end + 1 - at );
			at = end + 1;
			last = '"';
			comma = -1;
			continue;
		}
		std::size_t after;
		if ( !skipComment( d, at, after ) )
			return false;
		if ( after != at ) {
			if ( at + 1 < n && d[at + 1] == '*' )
				out += ' ';
			at = after;
			continue;
		}
		if ( c == ' ' || c == '\t' || c == '\r' || c == '\n' ) {
			out += c;
			++at;
			continue;
		}
		if ( ( c == ']' || c == '}' ) && comma >= 0 )
			out.erase( std::size_t( comma ), 1 );
		const bool afterValue = !( last == -1 || last == '[' || last == '{' || last == ',' || last == ':' );
		comma = ( c == ',' && afterValue ) ? long( out.size() ) : -1;
		out += c;
		last = (unsigned char)c;
		++at;
	}
	return true;
}

bool validUtf8( const std::string & s )
{
	const std::size_t n = s.size();
	for ( std::size_t i = 0; i < n; ) {
		unsigned char c = (unsigned char)s[i];
		if ( c < 0x80 ) {
			++i;
			continue;
		}
		int k;
		unsigned char lo = 0x80, hi = 0xBF;
		if ( c >= 0xC2 && c <= 0xDF )
			k = 1;
		else if ( c >= 0xE0 && c <= 0xEF ) {
			k = 2;
			if ( c == 0xE0 ) lo = 0xA0;
			if ( c == 0xED ) hi = 0x9F;
		} else if ( c >= 0xF0 && c <= 0xF4 ) {
			k = 3;
			if ( c == 0xF0 ) lo = 0x90;
			if ( c == 0xF4 ) hi = 0x8F;
		} else
			return false;
		for ( int j = 1; j <= k; ++j ) {
			if ( i + j >= n )
				return false;
			unsigned char t = (unsigned char)s[i + j];
			unsigned char l = j == 1 ? lo : 0x80, h = j == 1 ? hi : 0xBF;
			if ( t < l || t > h )
				return false;
		}
		i += k + 1;
	}
	return true;
}

void putUtf8( std::string & out, std::uint32_t cp )
{
	if ( cp < 0x80 )
		out += char( cp );
	else if ( cp < 0x800 ) {
		out += char( 0xC0 | ( cp >> 6 ) );
		out += char( 0x80 | ( cp & 0x3F ) );
	} else if ( cp < 0x10000 ) {
		out += char( 0xE0 | ( cp >> 12 ) );
		out += char( 0x80 | ( ( cp >> 6 ) & 0x3F ) );
		out += char( 0x80 | ( cp & 0x3F ) );
	} else {
		out += char( 0xF0 | ( cp >> 18 ) );
		out += char( 0x80 | ( ( cp >> 12 ) & 0x3F ) );
		out += char( 0x80 | ( ( cp >> 6 ) & 0x3F ) );
		out += char( 0x80 | ( cp & 0x3F ) );
	}
}

// Strict JSON as Python's json module reads it (first key wins case-free; NaN and Infinity refused).
struct Parser
{
	const std::string & s;
	std::size_t i = 0;
	bool tooDeep = false;

	void ws()
	{
		while ( i < s.size() && ( s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r' ) )
			++i;
	}

	static int hex( char c )
	{
		if ( c >= '0' && c <= '9' ) return c - '0';
		if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
		if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
		return -1;
	}

	bool u4( std::size_t at, std::uint32_t & v ) const
	{
		if ( at + 4 > s.size() )
			return false;
		v = 0;
		for ( int k = 0; k < 4; ++k ) {
			int h = hex( s[at + k] );
			if ( h < 0 )
				return false;
			v = v * 16 + std::uint32_t( h );
		}
		return true;
	}

	bool string( std::string & out )
	{
		++i;	// the opening quote
		while ( true ) {
			if ( i >= s.size() )
				return false;
			unsigned char c = (unsigned char)s[i];
			if ( c == '"' ) {
				++i;
				return true;
			}
			if ( c < 0x20 )
				return false;	// a control character
			if ( c != '\\' ) {
				out += char( c );
				++i;
				continue;
			}
			if ( i + 1 >= s.size() )
				return false;
			char e = s[i + 1];
			i += 2;
			switch ( e ) {
			case '"': out += '"'; break;
			case '\\': out += '\\'; break;
			case '/': out += '/'; break;
			case 'b': out += '\b'; break;
			case 'f': out += '\f'; break;
			case 'n': out += '\n'; break;
			case 'r': out += '\r'; break;
			case 't': out += '\t'; break;
			case 'u': {
				std::uint32_t v;
				if ( !u4( i, v ) )
					return false;
				i += 4;
				if ( v >= 0xD800 && v <= 0xDBFF && i + 1 < s.size() && s[i] == '\\' && s[i + 1] == 'u' ) {
					std::uint32_t w;
					if ( u4( i + 2, w ) && w >= 0xDC00 && w <= 0xDFFF ) {
						v = 0x10000 + ( ( v - 0xD800 ) << 10 ) + ( w - 0xDC00 );
						i += 6;
					}
				}
				putUtf8( out, v );	// a lone surrogate stays (WTF-8) and is refused later as bad-escape
				break;
			}
			default:
				return false;
			}
		}
	}

	bool number( double & out )
	{
		const std::size_t start = i;
		if ( i < s.size() && s[i] == '-' )
			++i;
		if ( i >= s.size() )
			return false;
		if ( s[i] == '0' )
			++i;
		else if ( s[i] >= '1' && s[i] <= '9' ) {
			while ( i < s.size() && s[i] >= '0' && s[i] <= '9' )
				++i;
		} else
			return false;
		if ( i + 1 < s.size() && s[i] == '.' && s[i + 1] >= '0' && s[i + 1] <= '9' ) {
			i += 1;
			while ( i < s.size() && s[i] >= '0' && s[i] <= '9' )
				++i;
		}
		if ( i < s.size() && ( s[i] == 'e' || s[i] == 'E' ) ) {
			std::size_t j = i + 1;
			if ( j < s.size() && ( s[j] == '+' || s[j] == '-' ) )
				++j;
			if ( j < s.size() && s[j] >= '0' && s[j] <= '9' ) {
				while ( j < s.size() && s[j] >= '0' && s[j] <= '9' )
					++j;
				i = j;
			}
		}
		const char * b = s.data() + start;
		const char * e = s.data() + i;
		auto r = std::from_chars( b, e, out );
		if ( r.ec == std::errc::result_out_of_range ) {
			const std::string t( b, e );
			const std::size_t ex = t.find_first_of( "eE" );
			const bool tiny = ex != std::string::npos && ex + 1 < t.size() && t[ex + 1] == '-';
			const bool neg = t[0] == '-';
			out = tiny ? ( neg ? -0.0 : 0.0 ) : ( neg ? -HUGE_VAL : HUGE_VAL );
		} else if ( r.ec != std::errc() )
			return false;
		return true;
	}

	bool literal( const char * word )
	{
		std::size_t n = std::char_traits<char>::length( word );
		if ( s.compare( i, n, word ) != 0 )
			return false;
		i += n;
		return true;
	}

	bool value( Json & v, int depth )
	{
		if ( depth > kStackDepth ) {
			tooDeep = true;
			return false;
		}
		if ( i >= s.size() )
			return false;
		const char c = s[i];
		if ( c == '{' ) {
			v.kind = Json::Object;
			++i;
			ws();
			if ( i < s.size() && s[i] == '}' ) {
				++i;
				return true;
			}
			while ( true ) {
				ws();
				if ( i >= s.size() || s[i] != '"' )
					return false;
				std::string key;
				if ( !string( key ) )
					return false;
				ws();
				if ( i >= s.size() || s[i] != ':' )
					return false;
				++i;
				ws();
				Json item;
				if ( !value( item, depth + 1 ) )
					return false;
				const std::string f = fold( key );
				bool seen = false;
				for ( const auto & m : v.obj )
					if ( fold( m.first ) == f ) {
						seen = true;
						break;
					}
				if ( seen )
					v.dups.push_back( key );
				else
					v.obj.emplace_back( std::move( key ), std::move( item ) );
				ws();
				if ( i < s.size() && s[i] == ',' ) {
					++i;
					continue;
				}
				if ( i < s.size() && s[i] == '}' ) {
					++i;
					return true;
				}
				return false;
			}
		}
		if ( c == '[' ) {
			v.kind = Json::Array;
			++i;
			ws();
			if ( i < s.size() && s[i] == ']' ) {
				++i;
				return true;
			}
			while ( true ) {
				ws();
				Json item;
				if ( !value( item, depth + 1 ) )
					return false;
				v.arr.push_back( std::move( item ) );
				ws();
				if ( i < s.size() && s[i] == ',' ) {
					++i;
					continue;
				}
				if ( i < s.size() && s[i] == ']' ) {
					++i;
					return true;
				}
				return false;
			}
		}
		if ( c == '"' ) {
			v.kind = Json::String;
			return string( v.str );
		}
		if ( c == '-' || ( c >= '0' && c <= '9' ) ) {
			v.kind = Json::Number;
			return number( v.num );
		}
		if ( literal( "true" ) ) {
			v.kind = Json::Bool;
			v.b = true;
			return true;
		}
		if ( literal( "false" ) ) {
			v.kind = Json::Bool;
			return true;
		}
		if ( literal( "null" ) )
			return true;
		return false;	// NaN, Infinity and anything else
	}
};

bool loneSurrogate( const std::string & s )
{
	for ( std::size_t i = 0; i + 1 < s.size(); ++i )
		if ( (unsigned char)s[i] == 0xED && (unsigned char)s[i + 1] >= 0xA0 )
			return true;
	return false;
}

bool badStrings( const Json & v )
{
	if ( v.kind == Json::String )
		return loneSurrogate( v.str );
	for ( const auto & m : v.obj )
		if ( loneSurrogate( m.first ) || badStrings( m.second ) )
			return true;
	for ( const auto & a : v.arr )
		if ( badStrings( a ) )
			return true;
	return false;
}

const char * checkLimits( const Json & v, int depth )
{
	if ( v.kind != Json::Object && v.kind != Json::Array )
		return nullptr;
	if ( depth > kMaxDepth )
		return "too-deep";
	if ( ( v.kind == Json::Object ? v.obj.size() : v.arr.size() ) > kMaxMembers )
		return "too-many";
	if ( v.kind == Json::Object ) {
		for ( const auto & m : v.obj )
			if ( const char * w = checkLimits( m.second, depth + 1 ) )
				return w;
	} else {
		for ( const auto & a : v.arr )
			if ( const char * w = checkLimits( a, depth + 1 ) )
				return w;
	}
	return nullptr;
}

// ---- warnings and the lights section

struct Warn
{
	std::vector<Warning> & items;
	void add( const std::string & at, const std::string & key, const char * warning, const std::string & sug = {} )
	{
		Warning w;
		w.at = at;
		w.key = key;
		w.warning = warning;
		w.suggest = sug;
		items.push_back( w );
	}
	void unknown( const std::string & at, const std::string & key, const std::vector<std::string> & known )
	{
		add( at, key, "unknown", suggest( key, known ) );
	}
	void value( const std::string & at, const std::string & key, const char * problem, const std::string & sug = {} )
	{
		add( at, key, "value", sug );
		items.back().problem = problem;
	}
};

void duplicateWarnings( const Json & v, const std::string & at, Warn & warn )
{
	if ( v.kind == Json::Object ) {
		for ( const auto & k : v.dups )
			warn.add( at, k, "duplicate" );
		for ( const auto & m : v.obj )
			duplicateWarnings( m.second, pointer( at, m.first ), warn );
	} else if ( v.kind == Json::Array ) {
		for ( std::size_t k = 0; k < v.arr.size(); ++k )
			duplicateWarnings( v.arr[k], pointer( at, std::to_string( k ) ), warn );
	}
}

bool finiteNumber( const Json & v ) { return v.kind == Json::Number && std::isfinite( v.num ); }

// The problem word for a value that is not [x, y, z] numbers, or nullptr.
const char * triple( const Json & v )
{
	if ( v.kind != Json::Array || v.arr.size() != 3 )
		return "length";
	for ( const auto & a : v.arr )
		if ( !finiteNumber( a ) )
			return "not-number";
	return nullptr;
}

const std::vector<std::string> kLightKeys = { "type", "lumens", "kelvin", "tint", "tier", "node", "follows",
	"position", "direction", "cone", "fog" };
const std::vector<std::string> kTypes = { "omni", "spot", "hemisphere" };
const std::vector<std::string> kTiers = { "hero", "normal", "bulb" };
const std::vector<std::string> kFogKeys = { "scale" };
// The sections a .nifx reader knows (the others warn as unknown); NifSkope reads only `lights` here.
const std::vector<std::string> kSections = { "material", "fakeVolume", "meleeTrail", "lights" };

int word( const Json & v, const std::vector<std::string> & words, const std::string & at, const std::string & key, Warn & warn )
{
	if ( v.kind != Json::String ) {
		warn.value( at, key, "not-string" );
		return -1;
	}
	for ( std::size_t k = 0; k < words.size(); ++k )
		if ( fold( words[k] ) == fold( v.str ) )
			return int( k );
	warn.value( at, key, "unknown-word", suggest( v.str, words ) );
	return -1;
}

bool readLight( const Json & entry, const std::string & at, Warn & warn, Light & out )
{
	bool hasLumens = false;
	for ( const auto & m : entry.obj ) {
		const std::string & key = m.first;
		const Json & v = m.second;
		if ( isNote( key ) )
			continue;
		std::string name;
		for ( const auto & k : kLightKeys )
			if ( fold( k ) == fold( key ) ) {
				name = k;
				break;
			}
		if ( name.empty() ) {
			warn.unknown( at, key, kLightKeys );
			continue;
		}
		if ( name == "type" || name == "tier" ) {
			int w = word( v, name == "type" ? kTypes : kTiers, at, key, warn );
			if ( w < 0 )
				continue;
			if ( name == "type" ) {
				out.type = w;
				out.hasType = true;
			} else {
				out.tier = w;
				out.hasTier = true;
			}
		} else if ( name == "lumens" || name == "kelvin" || name == "cone" ) {
			if ( !finiteNumber( v ) ) {
				warn.value( at, key, "not-number" );
				continue;
			}
			const double lo = name == "lumens" ? 0 : name == "kelvin" ? 1667 : 1;
			const double hi = name == "lumens" ? 1000000 : name == "kelvin" ? 25000 : 170;
			double x = v.num;
			if ( !( lo <= x && x <= hi ) ) {
				warn.value( at, key, "clamped" );
				x = std::min( hi, std::max( lo, x ) );
			}
			if ( name == "lumens" ) {
				out.lumens = x;
				hasLumens = true;
			} else if ( name == "kelvin" ) {
				out.kelvin = x;
				out.hasKelvin = true;
			} else {
				out.cone = x;
				out.hasCone = true;
			}
		} else if ( name == "tint" || name == "position" || name == "direction" ) {
			if ( const char * p = triple( v ) ) {
				warn.value( at, key, p );
				continue;
			}
			double t[3] = { v.arr[0].num, v.arr[1].num, v.arr[2].num };
			if ( name == "tint" ) {
				bool changed = false, any = false;
				for ( double & c : t ) {
					double k = std::min( 1.0, std::max( 0.0, c ) );
					changed |= k != c;
					c = k;
					any |= c != 0;
				}
				if ( changed )
					warn.value( at, key, "clamped" );
				if ( !any ) {
					warn.value( at, key, "black" );
					continue;
				}
				std::copy( t, t + 3, out.tint );
				out.hasTint = true;
			} else if ( name == "direction" ) {
				if ( t[0] == 0 && t[1] == 0 && t[2] == 0 ) {
					warn.value( at, key, "zero-length" );
					continue;
				}
				std::copy( t, t + 3, out.direction );
				out.hasDirection = true;
			} else {
				std::copy( t, t + 3, out.position );
				out.hasPosition = true;
			}
		} else if ( name == "node" || name == "follows" ) {
			if ( v.kind != Json::String ) {
				warn.value( at, key, "not-string" );
				continue;
			}
			if ( v.str.find_first_not_of( " \t\r\n\f\v\x1c\x1d\x1e\x1f" ) == std::string::npos ) {
				warn.value( at, key, "empty" );
				continue;
			}
			( name == "node" ? out.node : out.follows ) = v.str;
			( name == "node" ? out.hasNode : out.hasFollows ) = true;
		} else {	// fog
			if ( v.kind != Json::Object ) {
				warn.value( at, key, "not-object" );
				continue;
			}
			const std::string fat = pointer( at, key );
			bool any = false;
			for ( const auto & f : v.obj ) {
				if ( isNote( f.first ) )
					continue;
				if ( fold( f.first ) != "scale" ) {
					warn.unknown( fat, f.first, kFogKeys );
					continue;
				}
				if ( !finiteNumber( f.second ) ) {
					warn.value( fat, f.first, "not-number" );
					continue;
				}
				double x = f.second.num;
				if ( !( 0 <= x && x <= 8 ) ) {
					warn.value( fat, f.first, "clamped" );
					x = std::min( 8.0, std::max( 0.0, x ) );
				}
				out.fogScale = x;
				any = true;
			}
			if ( any )
				out.hasFog = true;
		}
	}
	if ( !hasLumens ) {
		warn.value( at, "lumens", "missing" );
		return false;
	}
	return true;
}

} // namespace

std::string fold( const std::string & s )
{
	std::string out = s;
	for ( char & c : out )
		if ( c >= 'A' && c <= 'Z' )
			c = char( c - 'A' + 'a' );
	return out;
}

std::string suggest( const std::string & key, std::vector<std::string> known )
{
	std::sort( known.begin(), known.end() );
	int best = 3;
	std::string out;
	for ( const auto & k : known ) {
		int d = editDistance( fold( key ), fold( k ) );
		if ( d <= 2 && d < best ) {
			best = d;
			out = k;
		}
	}
	return out;
}

const char * typeWord( int type ) { return type == Spot ? "spot" : type == Hemisphere ? "hemisphere" : "omni"; }
const char * tierWord( int tier ) { return tier == Hero ? "hero" : tier == Bulb ? "bulb" : "normal"; }

const Json * Json::find( const std::string & key ) const
{
	const std::string f = fold( key );
	for ( const auto & m : obj )
		if ( fold( m.first ) == f )
			return &m.second;
	return nullptr;
}

const Light * Lights::find( const std::string & name ) const
{
	const std::string f = fold( name );
	for ( const auto & l : lights )
		if ( fold( l.name ) == f )
			return &l;
	return nullptr;
}

std::string parse( const std::string & bytes, Json & out )
{
	if ( bytes.size() > kMaxBytes )
		return "too-large";
	std::string data = bytes;
	if ( data.compare( 0, 3, "\xEF\xBB\xBF" ) == 0 )
		data.erase( 0, 3 );
	std::size_t at = 0;
	while ( true ) {	// whitespace and comments may come before the first `{`
		while ( at < data.size() && ( data[at] == ' ' || data[at] == '\t' || data[at] == '\r' || data[at] == '\n' ) )
			++at;
		std::size_t after;
		if ( !skipComment( data, at, after ) )
			return "malformed";
		if ( after == at )
			break;
		at = after;
	}
	if ( at == data.size() )
		return "empty";
	if ( data[at] == '[' )
		return "not-object";
	if ( data[at] != '{' )
		return "binary";
	std::string text;
	if ( !strictJson( data, text ) || !validUtf8( text ) )
		return "malformed";
	Parser p{ text };
	p.ws();
	Json doc;
	if ( !p.value( doc, 1 ) )
		return p.tooDeep ? "too-deep" : "malformed";
	p.ws();
	if ( p.i != text.size() )
		return "malformed";
	if ( badStrings( doc ) )
		return "bad-escape";
	if ( const char * w = checkLimits( doc, 1 ) )
		return w;
	out = std::move( doc );
	return {};
}

Lights read( const std::string & bytes )
{
	Lights r;
	Json doc;
	std::string why = parse( bytes, doc );
	const Json * version = why.empty() ? doc.find( "version" ) : nullptr;
	if ( why.empty() && !version )
		why = "missing-version";
	if ( why.empty() && !( version->kind == Json::Number && version->num == 1 ) )
		why = "unsupported-version";
	if ( why.empty() ) {
		int sections = 0;
		for ( const auto & m : doc.obj )
			if ( &m.second != version && !isNote( m.first ) )
				++sections;
		if ( sections > kMaxSections )
			why = "too-many-sections";
	}
	if ( !why.empty() ) {
		r.status = "refused";
		r.reason = why;
		return r;
	}
	r.status = "ok";
	Warn warn{ r.warnings };
	duplicateWarnings( doc, "", warn );
	std::vector<std::string> known = { "version" };
	known.insert( known.end(), kSections.begin(), kSections.end() );
	for ( const auto & m : doc.obj ) {
		if ( &m.second == version || isNote( m.first ) )
			continue;
		std::string name;
		for ( const auto & s : kSections )
			if ( fold( s ) == fold( m.first ) )
				name = s;
		if ( name.empty() ) {
			r.unknownSections.push_back( m.first );
			warn.unknown( "", m.first, known );
			continue;
		}
		if ( name != "lights" )
			continue;	// other readers' sections
		if ( m.second.kind != Json::Object ) {
			warn.value( "", m.first, "not-object" );
			continue;
		}
		r.hasLights = true;
		const std::string at = pointer( "", m.first );
		for ( const auto & e : m.second.obj ) {
			if ( isNote( e.first ) )
				continue;
			if ( e.second.kind != Json::Object ) {
				warn.value( at, e.first, "not-object" );
				continue;
			}
			Light l;
			l.name = e.first;
			if ( readLight( e.second, pointer( at, e.first ), warn, l ) )
				r.lights.push_back( l );
		}
	}
	std::stable_sort( r.warnings.begin(), r.warnings.end(), []( const Warning & a, const Warning & b ) {
		if ( a.at != b.at ) return a.at < b.at;
		if ( a.key != b.key ) return a.key < b.key;
		if ( a.warning != b.warning ) return a.warning < b.warning;
		return a.problem < b.problem;
	} );
	std::sort( r.unknownSections.begin(), r.unknownSections.end() );
	return r;
}

// ---- converting (nifx/SPEC.md, Converting)

void kelvinRgb( double kelvin, double rgb[3] )
{
	const double t = std::min( 25000.0, std::max( 1667.0, kelvin ) );
	double x, y;
	if ( t <= 4000 )
		x = -0.2661239e9 / ( t * t * t ) - 0.2343589e6 / ( t * t ) + 0.8776956e3 / t + 0.179910;
	else
		x = -3.0258469e9 / ( t * t * t ) + 2.1070379e6 / ( t * t ) + 0.2226347e3 / t + 0.240390;
	if ( t <= 2222 )
		y = -1.1063814 * x * x * x - 1.34811020 * x * x + 2.18555832 * x - 0.20219683;
	else if ( t <= 4000 )
		y = -0.9549476 * x * x * x - 1.37418593 * x * x + 2.09137015 * x - 0.16748867;
	else
		y = 3.0817580 * x * x * x - 5.87338670 * x * x + 3.75112997 * x - 0.37001483;
	const double X = x / y, Y = 1.0, Z = ( 1.0 - x - y ) / y;
	rgb[0] = 3.2404542 * X - 1.5371385 * Y - 0.4985314 * Z;
	rgb[1] = -0.9692660 * X + 1.8760108 * Y + 0.0415560 * Z;
	rgb[2] = 0.0556434 * X - 0.2040259 * Y + 1.0572252 * Z;
	double m = 0;
	for ( int k = 0; k < 3; ++k ) {
		rgb[k] = std::max( 0.0, rgb[k] );
		m = std::max( m, rgb[k] );
	}
	for ( int k = 0; k < 3; ++k )
		rgb[k] /= m;
}

Converted convert( const Light & l, bool redTintDims )
{
	double omega;
	if ( l.type == Omni )
		omega = 4 * kPi;
	else if ( l.type == Hemisphere )
		omega = 2 * kPi;
	else
		omega = 2 * kPi * ( 1 - std::cos( l.cone * kPi / 180 / 2 ) );
	const double intensity = l.lumens / omega;
	Converted c;
	c.radius = std::min( 16384.0, std::max( 16.0, 69.99 * std::sqrt( intensity ) ) );
	double rgb[3] = { 1, 1, 1 };
	if ( l.hasKelvin )
		kelvinRgb( l.kelvin, rgb );
	if ( l.hasTint ) {
		const double tm = std::max( { l.tint[0], l.tint[1], l.tint[2] } );
		for ( int k = 0; k < 3; ++k )
			rgb[k] = rgb[k] * l.tint[k] / tm;
	}
	const double m = std::max( { rgb[0], rgb[1], rgb[2] } );
	for ( int k = 0; k < 3; ++k )
		rgb[k] = m > 0 ? rgb[k] / m : 1.0;
	double lin[3];
	for ( int k = 0; k < 3; ++k ) {
		c.color[k] = int( std::floor( 255 * std::pow( rgb[k], 1 / 2.2 ) + 0.5 ) );
		lin[k] = std::pow( c.color[k] / 255.0, 2.2 );
	}
	const double lum = 0.2126 * lin[0] + 0.7152 * lin[1] + 0.0722 * lin[2];
	c.fade = intensity / ( 800 / ( 4 * kPi ) );
	if ( !redTintDims )
		c.fade /= lum;	// a tint never dims: the same lumens whatever the colour
	return c;
}

} // namespace nifxl
