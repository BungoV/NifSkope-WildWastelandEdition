param([string]$Out, [string]$Stop)
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Collections.Generic;
public class WwWin {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetLayeredWindowAttributes(
      IntPtr h, out uint crKey, out byte bAlpha, out uint dwFlags);
  public static List<string> Scan(uint want) {
    var found = new List<string>();
    EnumWindows(delegate(IntPtr h, IntPtr l) {
      uint pid; GetWindowThreadProcessId(h, out pid);
      if (pid != want) return true;
      if (!IsWindowVisible(h)) return true;
      RECT r; GetWindowRect(h, out r);
      // LWA_ALPHA is 0x2. Not layered, or layered without LWA_ALPHA, means the
      // window is fully opaque as far as an eye is concerned.
      uint k; byte a; uint f; int alpha = 255;
      if (GetLayeredWindowAttributes(h, out k, out a, out f)) { if ((f & 0x2) != 0) alpha = (int)a; }
      var cn = new StringBuilder(128); GetClassName(h, cn, 128);
      var tt = new StringBuilder(128); GetWindowTextW(h, tt, 128);
      found.Add(String.Format("{0},{1},{2},{3}\t{4}\tcls={5}\ttitle={6}",
        r.Left, r.Top, r.Right, r.Bottom, alpha, cn.ToString(), tt.ToString()));
      return true;
    }, IntPtr.Zero);
    return found;
  }
}
"@
$lines = New-Object System.Collections.Generic.List[string]
while (-not (Test-Path $Stop)) {
  foreach ($p in (Get-CimInstance Win32_Process -Filter "Name='NifSkope.exe'")) {
    if ($p.CommandLine -notmatch '--port') { continue }
    foreach ($w in [WwWin]::Scan([uint32]$p.ProcessId)) {
      $f = $w -split "`t"
      $xy = $f[0] -split ','
      $L = [int]$xy[0]; $T = [int]$xy[1]; $R = [int]$xy[2]; $B = [int]$xy[3]
      foreach ($s in [System.Windows.Forms.Screen]::AllScreens) {
        # $scr, NOT $b: PowerShell variables are case-insensitive, so $b would
        # be the same variable as $B, the window's bottom edge.
        $scr = $s.Bounds
        if (($L -lt ($scr.X + $scr.Width)) -and ($R -gt $scr.X) -and
            ($T -lt ($scr.Y + $scr.Height)) -and ($B -gt $scr.Y)) {
          $prim = 0; if ($s.Primary) { $prim = 1 }
          $lines.Add("$($p.ProcessId) $($f[0]) on $($s.DeviceName) primary=$prim alpha=$($f[1]) $($f[2]) $($f[3])")
          break
        }
      }
    }
  }
  Start-Sleep -Milliseconds 200
}
$lines | Out-File -Encoding ascii $Out
