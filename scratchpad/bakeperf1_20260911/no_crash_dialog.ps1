# BAKEPERF1, 2026-09-11 14:2x: SIX WINDOWS CRASH DIALOGS REACHED BUNGO'S DESKTOP.
#
# Dot-source this BEFORE launching NifSkope from any script in this lane.
#
# Windows gives a child process the error mode of the process that created it,
# so setting it here silences the "Application Error" box for every NifSkope
# this shell starts. It is the stop-gap; the real fix is the -no-gui path
# calling SetErrorMode for itself at CLI start, which no wrapper can guarantee
# for a run somebody else launches.
#
#   SEM_FAILCRITICALERRORS   0x0001
#   SEM_NOGPFAULTERRORBOX    0x0002   <- the crash dialog
#   SEM_NOALIGNMENTFAULTEXCEPT 0x0004
#   SEM_NOOPENFILEERRORBOX   0x8000
if (-not ('WWErr.Native' -as [type])) {
  Add-Type -Namespace WWErr -Name Native -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("kernel32.dll")]
public static extern uint SetErrorMode(uint uMode);
'@
}
[void][WWErr.Native]::SetErrorMode(0x0001 -bor 0x0002 -bor 0x0004 -bor 0x8000)
