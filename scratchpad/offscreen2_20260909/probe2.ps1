param([string]$Out, [string]$Stop)
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Collections.Generic;
public class Ww2 {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetLayeredWindowAttributes(IntPtr h, out uint k, out byte a, out uint f);
  [DllImport("user32.dll")] public static extern int GetWindowLongW(IntPtr h, int i);
  public static List<string> Scan(uint want) {
    var found = new List<string>();
    EnumWindows(delegate(IntPtr h, IntPtr l) {
      uint pid; GetWindowThreadProcessId(h, out pid);
      if (pid != want) return true;
      RECT r; GetWindowRect(h, out r);
      uint k; byte a; uint f; int alpha = 255;
      if (GetLayeredWindowAttributes(h, out k, out a, out f)) { if ((f & 0x2) != 0) alpha = (int)a; }
      var cn = new StringBuilder(128); GetClassName(h, cn, 128);
      var tt = new StringBuilder(128); GetWindowTextW(h, tt, 128);
      found.Add(String.Format("vis={10} par=0x{11:X} hwnd=0x{12:X} rect={1},{2},{3}x{4} alpha={5} style=0x{6:X} ex=0x{7:X} cls={8} title={9}",
        (IsWindowVisible(h)?1:0), GetWindowLongW(h,-8), h.ToInt64(), r.Left, r.Top, r.Right - r.Left, r.Bottom - r.Top, alpha,
        GetWindowLongW(h, -16), GetWindowLongW(h, -20), cn.ToString(), tt.ToString()));
      return true;
    }, IntPtr.Zero);
    return found;
  }
}
"@
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$lines = New-Object System.Collections.Generic.List[string]
while (-not (Test-Path $Stop)) {
  foreach ($p in (Get-CimInstance Win32_Process -Filter "Name='NifSkope.exe'")) {
    if ($p.CommandLine -notmatch '--port') { continue }
    foreach ($w in [Ww2]::Scan([uint32]$p.ProcessId)) {
      $lines.Add(("{0,7} pid={1} {2}" -f $sw.ElapsedMilliseconds, $p.ProcessId, $w))
    }
  }
  Start-Sleep -Milliseconds 10
}
$lines | Out-File -Encoding ascii $Out
