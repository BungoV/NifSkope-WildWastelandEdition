#ifndef NIFITEMCACHE_H
#define NIFITEMCACHE_H

/* Lane SPEED1 (2026-10-02): A THREAD'S OWN RUN OF ITEM SLOTS.
 *
 * Every NifItem comes from one pool behind one mutex (nifitem.cpp). A thread that parses whole models takes
 * and returns hundreds of thousands of items per model, and several such threads queue on that mutex. While
 * one of these lives on a thread, that thread takes slots a whole run at a time (one lock per run) and keeps
 * what it frees; when the last one on the thread dies, everything it holds goes back to the shared list.
 * Threads that never make one are exactly as before. */
struct NifItemThreadCache
{
	NifItemThreadCache();
	~NifItemThreadCache();
	NifItemThreadCache( const NifItemThreadCache & ) = delete;
	NifItemThreadCache & operator=( const NifItemThreadCache & ) = delete;
};

#endif
