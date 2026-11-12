#ifndef WW_FEATURES_SHARED_GLSTATEMEMO_H
#define WW_FEATURES_SHARED_GLSTATEMEMO_H

/* lane EMPTY240 R11-2: a pass's GL state reads (glGetIntegerv / glIsEnabled / glGetBooleanv of what it saves and puts
 * back) kept from the last read, one memo a pass and a renderer. The frame before such a pass is the same every frame
 * (the passes before it restore what they change), so the reads run on the first call, 1 paint in 64 (a check), after
 * any live set, under WW_SETUP_MEMO_CHECK=1, and every time under the red WW_GLSTATE_MEMO_RED=1 (live). A check that
 * reads another state than the kept one (with no live set between) prints `gl state memo: <name> read another state`
 * once and that memo reads every time from then on. */

#include "wwswitch.h"

#include <QHash>
#include <QtGlobal>

#include <cstdio>
#include <cstring>

quint64 wwGlPaintCount();	// glview.cpp
bool wwSetupMemoChecking();	// renderer.cpp

//! WW_GLSTATE_MEMO_RED=1 (live): every memo reads the driver every time, as before
inline bool wwGlStateMemoRed()
{
	WW_SWITCH_STATIC( bool, red, wwEnvInt( "WW_GLSTATE_MEMO_RED" ) == 1 );
	return red;
}

template <typename T, int N> struct WwGlMemo
{
	T v[N] = {};
	quint64 gen = 0;
	bool valid = false, off = false;

	//! true: ask the driver now and keep() what it said; false: v holds the state
	bool due() const
	{
		if ( !valid || off || wwGlStateMemoRed() || wwSetupMemoChecking() || gen != wwSwitchGen() )
			return true;
		return wwGlPaintCount() % 64u == 0u;
	}
	void keep( const T ( &now )[N], const char * name )
	{
		const quint64 g = wwSwitchGen();
		if ( valid && !off && gen == g && std::memcmp( now, v, sizeof v ) != 0 ) {
			off = true;
			std::fprintf( stderr, "gl state memo: %s read another state at its check: read every time from now (EMPTY240 R11-2)\n",
				name );
		}
		std::memcpy( v, now, sizeof v );
		gen = g;
		valid = true;
	}
};

#endif
