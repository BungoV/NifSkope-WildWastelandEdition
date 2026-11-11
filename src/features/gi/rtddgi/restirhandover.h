// Lane AIRPERF1 round 2 (2026-11-11): when a ReSTIR step may wait on the device. Pure, no Qt, no Vulkan:
// restir_vk.inc restirGpuStep follows it, tests/features/gi/restir_handover simulates it (-DRESTIR_HANDOVER_RED fails).
//
// The interop submit waits on GL's glDone semaphore, so its fence fires only after the GPU has run every GL command
// queued before the signal: a CPU wait on that fence costs the GL frame's backlog (18-40 ms in a 104 ms frame), not the
// kernel (about 1.5 ms). The rule: the GL frame never waits. It submits and returns; GL orders its own read of the
// ratio on the GPU with the vkDone semaphore. A step that finds its last submit still on the device makes no new one
// (the host-written parameters and the one command buffer are in use): the frame keeps the last ratio.
#ifndef RESTIRHANDOVER_H
#define RESTIRHANDOVER_H

namespace restirhand
{

enum Act
{
	Submit = 0,   // the last submit is done: write the parameters, record, submit
	Late = 1,     // the last submit is still on the device and nobody may wait: no submit this frame
	Block = 2     // wait for the last submit, then as Submit (the CLI, a read-back, the red)
};

//! before a step. lastDone: the fence of this session's last submit is signalled; wait: the caller waits for results
inline Act before( bool lastDone, bool wait )
{
#ifdef RESTIR_HANDOVER_RED
	(void) wait;
	return lastDone ? Submit : Block;   // the old rule: a step always waits
#else
	return lastDone ? Submit : ( wait ? Block : Late );
#endif
}

//! after the submit: true = the CPU waits on this submit's fence
inline bool after( bool wait )
{
#ifdef RESTIR_HANDOVER_RED
	(void) wait;
	return true;
#else
	return wait;
#endif
}

} // namespace restirhand

#endif
