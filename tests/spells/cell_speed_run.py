"""Lane SPEED1 (2026-10-02): run ONE NifSkope cell open and measure it from outside.

Wall seconds, processor seconds (user + kernel) and the peak working set are read from the
operating system's own counters for the child process -- no NifSkope code and none of its timers.
The graphics card's load is sampled once a second from nvidia-smi when there is one (machine-wide:
another window on the card counts too).

usage: cell_speed_run.py <timeout_s> <results.tsv> <tag> <notes_file> -- <exe> <args...>
Appends one row to results.tsv:
  tag  rc  wall_s  cpu_s  cores_busy  peak_mb  gpu_mean_pct  gpu_max_pct
and prints it. The environment is passed through untouched.
"""
import ctypes
import ctypes.wintypes as wt
import shutil
import subprocess
import sys
import threading
import time


class PMC(ctypes.Structure):
    _fields_ = [('cb', wt.DWORD), ('PageFaultCount', wt.DWORD), ('PeakWorkingSetSize', ctypes.c_size_t),
                ('WorkingSetSize', ctypes.c_size_t), ('QuotaPeakPagedPoolUsage', ctypes.c_size_t),
                ('QuotaPagedPoolUsage', ctypes.c_size_t), ('QuotaPeakNonPagedPoolUsage', ctypes.c_size_t),
                ('QuotaNonPagedPoolUsage', ctypes.c_size_t), ('PagefileUsage', ctypes.c_size_t),
                ('PeakPagefileUsage', ctypes.c_size_t)]


def counters(handle):
    k32 = ctypes.WinDLL('kernel32', use_last_error=True)
    c, e, k, u = wt.FILETIME(), wt.FILETIME(), wt.FILETIME(), wt.FILETIME()
    h = wt.HANDLE(handle)
    cpu = 0.0
    if k32.GetProcessTimes(h, ctypes.byref(c), ctypes.byref(e), ctypes.byref(k), ctypes.byref(u)):
        ft = lambda f: ((f.dwHighDateTime << 32) | f.dwLowDateTime) / 1e7
        cpu = ft(k) + ft(u)
    pmc = PMC()
    pmc.cb = ctypes.sizeof(PMC)
    peak = 0.0
    if k32.K32GetProcessMemoryInfo(h, ctypes.byref(pmc), pmc.cb):
        peak = pmc.PeakWorkingSetSize / (1 << 20)
    return cpu, peak


def main():
    if '--' not in sys.argv or sys.argv.index('--') != 5:
        print(__doc__)
        return 2
    timeout_s, results, tag, notes = float(sys.argv[1]), sys.argv[2], sys.argv[3], sys.argv[4]
    cmd = sys.argv[6:]
    gpu = []
    stop = threading.Event()

    def sample():
        smi = shutil.which('nvidia-smi')
        while smi and not stop.wait(1.0):
            try:
                o = subprocess.run([smi, '--query-gpu=utilization.gpu', '--format=csv,noheader,nounits'],
                                   capture_output=True, text=True, timeout=5).stdout.split()
                if o:
                    gpu.append(float(o[0]))
            except Exception:
                pass

    th = threading.Thread(target=sample, daemon=True)
    th.start()
    t0 = time.perf_counter()
    rc = None
    with open(notes, 'wb') as nf:
        p = subprocess.Popen(cmd, stdout=nf, stderr=subprocess.STDOUT)
        try:
            rc = p.wait(timeout=timeout_s)
        except subprocess.TimeoutExpired:
            rc = 'timeout'   # never killed here: the caller's rule is that no process is killed
        wall = time.perf_counter() - t0
        cpu, peak = counters(int(p._handle))
    stop.set()
    row = [tag, str(rc), '%.2f' % wall, '%.2f' % cpu, '%.2f' % (cpu / wall if wall > 0 else 0.0), '%.0f' % peak,
           '%.0f' % (sum(gpu) / len(gpu)) if gpu else '-', '%.0f' % max(gpu) if gpu else '-']
    line = '\t'.join(row)
    with open(results, 'a') as f:
        f.write(line + '\n')
    print(line)
    return 0 if rc == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
