#ifndef GPUSTAGE_H
#define GPUSTAGE_H

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

#include "wwswitch.h"

#include <algorithm>
#include <atomic>
#include <cstdint>

/*! MODULES1 (11-06, bungo "fix the wild ram usage"): static GL buffers are filled by a GPU copy from a small staging
 *  buffer, so the driver keeps no system-RAM copy of them. Measured offline (notes/modules1/ram/unit glmirror, NVIDIA):
 *  a buffer filled from the CPU (glBufferData with data, glBufferSubData, glBufferStorage) keeps a resident copy in the
 *  process working set (+2.0 GB for 2 GB of buffers); the same buffers filled through glCopyBufferSubData from a reused,
 *  orphaned staging buffer add +0.13 GB. The game hands mesh data to the GPU and keeps no copy; this is the same rule.
 *
 *  wwStageUpload( target, offset, bytes, data ) writes `bytes` at `offset` of the buffer bound to `target` (the buffer
 *  must already have its storage, e.g. glBufferData( target, size, nullptr, GL_STATIC_DRAW )), in chunks of at most
 *  kGpuStageChunk through one staging name per context, re-specified per chunk (orphaned, so no wait on the GPU).
 *  GL_COPY_READ_BUFFER is left bound to 0. Red WW_GPUSTAGE_RED=1: plain glBufferSubData (the old path). */
inline constexpr std::intptr_t kGpuStageChunk = 8 << 20;

struct GpuStageCounts
{
	std::atomic<std::int64_t> stagedBytes { 0 };	// written through the staging copy
	std::atomic<std::int64_t> directBytes { 0 };	// written straight (the red)
};

inline GpuStageCounts & gpuStageCounts()
{
	static GpuStageCounts c;
	return c;
}

inline bool gpuStageRed()
{
	WW_SWITCH_STATIC( bool, red, wwEnvIsSet( "WW_GPUSTAGE_RED" ) && wwEnv( "WW_GPUSTAGE_RED" ) != "0" );
	return red;
}

inline void wwStageUpload( GLenum target, GLintptr offset, GLsizeiptr bytes, const void * data )
{
	QOpenGLContext * ctx = QOpenGLContext::currentContext();
	if ( !ctx || bytes <= 0 || !data )
		return;
	QOpenGLExtraFunctions * f = ctx->extraFunctions();
	if ( gpuStageRed() ) {
		f->glBufferSubData( target, offset, bytes, data );
		gpuStageCounts().directBytes += bytes;
		return;
	}
	// one staging name per context (names are not shared between unshared contexts)
	struct Ring { QOpenGLContext * ctx = nullptr; GLuint name = 0; };
	static thread_local Ring ring;
	if ( ring.ctx != ctx || !ring.name ) {
		ring.ctx = ctx;
		ring.name = 0;
		f->glGenBuffers( 1, &ring.name );
	}
	f->glBindBuffer( GL_COPY_READ_BUFFER, ring.name );
	const unsigned char * p = static_cast<const unsigned char *>( data );
	for ( GLsizeiptr o = 0; o < bytes; o += kGpuStageChunk ) {
		const GLsizeiptr n = std::min<GLsizeiptr>( kGpuStageChunk, bytes - o );
		f->glBufferData( GL_COPY_READ_BUFFER, n, p + o, GL_STREAM_COPY );
		f->glCopyBufferSubData( GL_COPY_READ_BUFFER, target, 0, offset + o, n );
	}
	f->glBindBuffer( GL_COPY_READ_BUFFER, 0 );
	gpuStageCounts().stagedBytes += bytes;
}

//! glBufferData( target, bytes, data, usage ) for a static buffer: storage first, then the staged copy
inline void wwStageBufferData( GLenum target, GLsizeiptr bytes, const void * data, GLenum usage )
{
	QOpenGLContext * ctx = QOpenGLContext::currentContext();
	if ( !ctx )
		return;
	QOpenGLExtraFunctions * f = ctx->extraFunctions();
	if ( gpuStageRed() || !data ) {
		f->glBufferData( target, bytes, data, usage );
		if ( data )
			gpuStageCounts().directBytes += bytes;
		return;
	}
	f->glBufferData( target, bytes, nullptr, usage );
	wwStageUpload( target, 0, bytes, data );
}

#endif // GPUSTAGE_H
