#pragma once
// lane PERFLOAD1 (11-10): what a cell load saved, the main thread's step mark and the log drain.
// Header-only (inline variables), so a test that compiles one of the users without main.cpp still links.
#include <atomic>
#include <cstdio>
#include <cstring>

// P1: counted by lodgen.cpp, printed and reset by cellspeed.cpp as the `load speed:` line of one cell load
inline std::atomic<int> wwLoadWalkSkipped{ 0 }, wwLoadPathBuilt{ 0 }, wwLoadPathReused{ 0 };

// P2: set by main.cpp when the watch thread / the stderr pump run
inline void ( *wwMainStepFn )( const char * ) = nullptr;
inline void ( *wwStderrDrainFn )() = nullptr;

// P6: set by nifskope.cpp; parks (true) or attaches (false) the block list and tree of the window showing `nif`
class NifModel;
inline bool ( *wwParkViewsFn )( NifModel *, bool ) = nullptr;

//! A bulk edit of a shown document (the live far LOD rebuild) with its views parked: true when this call parked or
//! attached them. Every inserted block otherwise signals the block list proxy and the tree.
inline bool wwParkViews( NifModel * nif, bool park )
{
	return wwParkViewsFn && wwParkViewsFn( nif, park );
}

//! The main thread reached a named step (a string literal: the pointer is kept); a step over 1 s prints its own line.
inline void wwMainStep( const char * stage )
{
	if ( wwMainStepFn )
		wwMainStepFn( stage );
}

//! Before a hard exit (std::_Exit, a fatal message): the log text reaches its file.
inline void wwStderrPumpDrain()
{
	if ( wwStderrDrainFn )
		wwStderrDrainFn();
	else
		std::fflush( stderr );
}
