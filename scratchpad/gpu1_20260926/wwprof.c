/* wwprof -- a sampling CPU profiler for a running Windows process (lane GPU1, 2026-09-26).
 *
 *   wwprof.exe <pid> <out.txt> [interval_ms=100] [max_frames=64]
 *
 * Every interval: enumerate the process's threads; for each, read its CPU time (kernel+user) and
 * the delta since the last sample. A thread that consumed CPU in the interval (and thread 0, the
 * first one seen = the main thread, always) is suspended, its context read, its stack walked with
 * StackWalk64 (x64 unwind data from .pdata, no symbols needed), resumed. One line per thread:
 *   S <t_ms> <tid> <dcpu_100ns> <main?> <addr> <addr> ...      (hex absolute addresses, leaf first)
 * plus one line per interval: T <t_ms> <sum_dcpu_100ns> <threads> <busy_threads>
 * and the module table at the start: M <base> <size> <path>.
 * Symbolise offline with wwprof_report.py against an UNSTRIPPED link of the same objects.
 * Ends when the process exits. Never kills, never writes into the target.
 */
#include <windows.h>
#include <tlhelp32.h>
#include <dbghelp.h>
#include <psapi.h>
#include <stdio.h>
#include <stdlib.h>

#define MAXT 512
static DWORD tids[MAXT]; static ULONGLONG lastcpu[MAXT]; static int nt = 0;
static DWORD mainTid = 0;

static ULONGLONG ft(FILETIME f) { return ((ULONGLONG)f.dwHighDateTime << 32) | f.dwLowDateTime; }
static int slot(DWORD tid) {
	for (int i = 0; i < nt; i++) if (tids[i] == tid) return i;
	if (nt < MAXT) { tids[nt] = tid; lastcpu[nt] = 0; return nt++; }
	return -1;
}

int main(int argc, char ** argv) {
	if (argc < 3) { fprintf(stderr, "usage: wwprof <pid> <out> [interval_ms] [max_frames]\n"); return 2; }
	DWORD pid = (DWORD)atoi(argv[1]);
	int iv = argc > 3 ? atoi(argv[3]) : 100;
	int maxf = argc > 4 ? atoi(argv[4]) : 64;
	FILE * o = fopen(argv[2], "w");
	if (!o) { fprintf(stderr, "cannot open %s\n", argv[2]); return 2; }
	HANDLE hp = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE, FALSE, pid);
	if (!hp) { fprintf(stderr, "OpenProcess %lu failed %lu\n", pid, GetLastError()); return 1; }
	/* No symbol search at all: invade=TRUE with the default path stalled for minutes (symbol server / big
	 * COFF table). Unwinding only needs each module's .pdata, which dbghelp reads from the image once the
	 * module is registered; names come offline from nm. */
	SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS | 0x00001000 /* IGNORE_NT_SYMPATH */
		| 0x02000000 /* DISABLE_SYMSRV_AUTODETECT */);
	if (!SymInitialize(hp, "C:\\wwprof_no_symbols", FALSE)) fprintf(stderr, "SymInitialize failed %lu (walking anyway)\n", GetLastError());
	HMODULE mods[1024]; DWORD need = 0;
	if (EnumProcessModules(hp, mods, sizeof(mods), &need)) {
		for (unsigned i = 0; i < need / sizeof(HMODULE); i++) {
			MODULEINFO mi; char name[MAX_PATH];
			if (GetModuleInformation(hp, mods[i], &mi, sizeof(mi)) && GetModuleFileNameExA(hp, mods[i], name, MAX_PATH)) {
				fprintf(o, "M %llx %lx %s\n", (unsigned long long)(ULONG_PTR)mi.lpBaseOfDll, mi.SizeOfImage, name);
				SymLoadModuleEx(hp, NULL, name, NULL, (DWORD64)(ULONG_PTR)mi.lpBaseOfDll, mi.SizeOfImage, NULL, 0);
			}
		}
	}
	fflush(o);
	fprintf(stderr, "wwprof: attached to %lu, %lu modules\n", pid, (unsigned long)(need / sizeof(HMODULE)));
	LARGE_INTEGER fq, t0, tn; QueryPerformanceFrequency(&fq); QueryPerformanceCounter(&t0);
	int refreshMods = 0;
	for (;;) {
		if (WaitForSingleObject(hp, 0) == WAIT_OBJECT_0) break;
		QueryPerformanceCounter(&tn);
		long long tms = (tn.QuadPart - t0.QuadPart) * 1000 / fq.QuadPart;
		HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
		THREADENTRY32 te; te.dwSize = sizeof(te);
		ULONGLONG sum = 0; int count = 0, busy = 0;
		if (snap != INVALID_HANDLE_VALUE && Thread32First(snap, &te)) {
			do {
				if (te.th32OwnerProcessID != pid) continue;
				if (!mainTid) mainTid = te.th32ThreadID;
				count++;
				HANDLE ht = OpenThread(THREAD_QUERY_INFORMATION | THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, te.th32ThreadID);
				if (!ht) continue;
				FILETIME c, e, k, u;
				ULONGLONG cpu = 0;
				if (GetThreadTimes(ht, &c, &e, &k, &u)) cpu = ft(k) + ft(u);
				int s = slot(te.th32ThreadID);
				ULONGLONG d = 0;
				if (s >= 0) { d = cpu >= lastcpu[s] ? cpu - lastcpu[s] : 0; if (!refreshMods) d = 0; /* first round: CPU spent before we attached */ lastcpu[s] = cpu; }
				sum += d;
				int isMain = te.th32ThreadID == mainTid;
				if (d > 0) busy++;
				if (d > 0 || isMain) {
					if (SuspendThread(ht) != (DWORD)-1) {
						CONTEXT ctx; memset(&ctx, 0, sizeof(ctx)); ctx.ContextFlags = CONTEXT_FULL;
						if (GetThreadContext(ht, &ctx)) {
							STACKFRAME64 sf; memset(&sf, 0, sizeof(sf));
							sf.AddrPC.Offset = ctx.Rip; sf.AddrPC.Mode = AddrModeFlat;
							sf.AddrFrame.Offset = ctx.Rbp; sf.AddrFrame.Mode = AddrModeFlat;
							sf.AddrStack.Offset = ctx.Rsp; sf.AddrStack.Mode = AddrModeFlat;
							fprintf(o, "S %lld %lu %llu %d", tms, te.th32ThreadID, (unsigned long long)d, isMain);
							for (int f = 0; f < maxf; f++) {
								if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, hp, ht, &sf, &ctx, NULL,
										SymFunctionTableAccess64, SymGetModuleBase64, NULL)) break;
								if (!sf.AddrPC.Offset) break;
								fprintf(o, " %llx", (unsigned long long)sf.AddrPC.Offset);
							}
							fputc('\n', o);
						}
						ResumeThread(ht);
					}
				}
				CloseHandle(ht);
			} while (Thread32Next(snap, &te));
		}
		if (snap != INVALID_HANDLE_VALUE) CloseHandle(snap);
		refreshMods = 1;
		fprintf(o, "T %lld %llu %d %d\n", tms, (unsigned long long)sum, count, busy);
		fflush(o);
		Sleep(iv);
	}
	fprintf(o, "E\n");
	fclose(o);
	return 0;
}
