#ifndef ALPHABGSM_H
#define ALPHABGSM_H

/* lane ALPHABGSM1 (2026-10-29): the census of the side passes that take their alpha test from
 * Shape::wwDrawAlpha (the colour draw's rule: a material file's test wins over the NiAlphaProperty).
 * Each pass tallies the shapes it takes and prints one line when the numbers move:
 *   alphabgsm <pass>: shapes N, alpha-tested NIF K (before) drawn M (after), material only A, NIF only B
 * "before" is what the pass tested until this lane (the NIF's property), "after" is what it tests now. */

#include <QtGlobal>

#include <cstdio>
#include <map>
#include <string>

struct AlphaBgsmTally
{
	qint64 nif = 0, drawn = 0, matOnly = 0, nifOnly = 0;

	static void note( AlphaBgsmTally & t, bool nifTest, bool drawnTest )
	{
		t.nif += nifTest ? 1 : 0;
		t.drawn += drawnTest ? 1 : 0;
		t.matOnly += ( drawnTest && !nifTest ) ? 1 : 0;
		t.nifOnly += ( nifTest && !drawnTest ) ? 1 : 0;
	}

	static void report( const AlphaBgsmTally & t, const char * pass, qint64 shapes )
	{
		static std::map<std::string, std::string> last;
		char line[256];
		std::snprintf( line, sizeof line, "alphabgsm %s: shapes %lld, alpha-tested NIF %lld (before) drawn %lld (after), "
			"material only %lld, NIF only %lld", pass, (long long) shapes, (long long) t.nif, (long long) t.drawn,
			(long long) t.matOnly, (long long) t.nifOnly );
		std::string & was = last[pass];
		if ( was == line )
			return;
		was = line;
		std::fprintf( stderr, "%s\n", line );
	}
};

#endif // ALPHABGSM_H
