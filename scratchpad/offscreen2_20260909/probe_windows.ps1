param([string]$Out, [string]$Stop)
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class WwEnum {
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
  public static System.Collections.Generic.List<string> Scan(uint want) {
    var outp = new System.Collections.Generic.List<string>();
    EnumWindows(delegate(IntPtr h, IntPtr l) {
      uint pid; GetWindowThreadProcessId(h, out pid);
      if (pid != want) return true;
      if (!IsWindowVisible(h)) return true;
      RECT r; GetWindowRect(h, out r);
      var cn = new StringBuilder(256); GetClassName(h, cn, 256);
      var tt = new StringBuilder(256); GetWindowTextW(h, tt, 256);
      uint k; byte a; uint f; int alpha = 255;
      if (GetLayeredWindowAttributes(h, out k, out a, out f)) { if ((f & 0x2) != 0) alpha = (int)a; }
      int ex = GetWindowLongW(h, -20);
      outp.Add(String.Format("cls={0} title=[{1}] rect={2},{3},{4},{5} alpha={6} exstyle=0x{7:X}",
        cn.ToString(), tt.ToString(), r.Left, r.Top, r.Right, r.Bottom, alpha, ex));
      return true;
    }, IntPtr.Zero);
    return outp;
  }
}
"@
$lines = New-Object System.Collections.Generic.List[string]
$t = 0
while (-not (Test-Path $Stop)) {
  foreach ($p in (Get-CimInstance Win32_Process -Filter "Name='NifSkope.exe'")) {
    if ($p.CommandLine -notmatch '--port') { continue }
    foreach ($s in [WwEnum]::Scan([uint32]$p.ProcessId)) {
      $lines.Add("t=$t pid=$($p.ProcessId) $s")
    }
  }
  $t++
  Start-Sleep -Milliseconds 100
}
$lines | Out-File -Encoding ascii $Out
