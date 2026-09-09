#!/usr/bin/env python
"""Measure — not compute — the wall time and PEAK WORKING SET of one whole-worldspace
`.lodt` open in NifSkope WW.

psutil is absent on this machine's python 3.9, so the peak comes straight from the
Win32 `PROCESS_MEMORY_COUNTERS.PeakWorkingSetSize` field through ctypes. That field
is MONOTONIC inside the kernel, so the last successful poll before the process exits
is the true peak; the poll rate only decides how close to exit that last read lands.

Usage:  python measure_open.py <exe> <lodt> <shot.png> <port> [region] [plane]
Prints one line:  wall <s> s  peak working set <MB> MB  rc <n>
"""
import ctypes, ctypes.wintypes as wt, os, subprocess, sys, time

PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ           = 0x0010


class PROCESS_MEMORY_COUNTERS( ctypes.Structure ):
	_fields_ = [
		( "cb", wt.DWORD ),
		( "PageFaultCount", wt.DWORD ),
		( "PeakWorkingSetSize", ctypes.c_size_t ),
		( "WorkingSetSize", ctypes.c_size_t ),
		( "QuotaPeakPagedPoolUsage", ctypes.c_size_t ),
		( "QuotaPagedPoolUsage", ctypes.c_size_t ),
		( "QuotaPeakNonPagedPoolUsage", ctypes.c_size_t ),
		( "QuotaNonPagedPoolUsage", ctypes.c_size_t ),
		( "PagefileUsage", ctypes.c_size_t ),
		( "PeakPagefileUsage", ctypes.c_size_t ),
	]


def main():
	exe, lodt, shot, port = sys.argv[1:5]
	region = sys.argv[5] if len( sys.argv ) > 5 else ""
	plane  = sys.argv[6] if len( sys.argv ) > 6 else ""

	env = dict( os.environ )
	env["WW_WINDOW_AT"]    = "1960,40"
	env["WW_RENDER_SHOT"]  = shot
	env["WW_RENDER_VIEW"]  = "1"
	env["WW_RENDER_SIZE"]  = "1400x1400"
	if region:
		env["WW_LODT_REGION"] = region
	if plane:
		env["WW_LODT_PLANE"] = plane

	t0 = time.time()
	p = subprocess.Popen( [exe, "--port", port, lodt], env=env )

	k32  = ctypes.WinDLL( "kernel32", use_last_error=True )
	psapi = ctypes.WinDLL( "psapi", use_last_error=True )
	h = k32.OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, p.pid )
	peak = 0
	pmc = PROCESS_MEMORY_COUNTERS()
	pmc.cb = ctypes.sizeof( pmc )
	while p.poll() is None:
		if h and psapi.GetProcessMemoryInfo( h, ctypes.byref( pmc ), pmc.cb ):
			if pmc.PeakWorkingSetSize > peak:
				peak = pmc.PeakWorkingSetSize
		time.sleep( 0.05 )
	rc = p.wait()
	wall = time.time() - t0
	if h:
		k32.CloseHandle( h )
	print( "wall %.1f s  peak working set %.1f MB  rc %d" % ( wall, peak / 1048576.0, rc ) )
	return 0 if peak > 0 else 9


if __name__ == "__main__":
	sys.exit( main() )
