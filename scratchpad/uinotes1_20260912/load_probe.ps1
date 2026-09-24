# Lane UINOTES1b -- LOOK at the stuck window instead of guessing. Launches the
# exe on a file with its own --port, waits, then writes down every top-level
# window the process owns (a modal box is a window with its own title) and saves
# a picture of the second monitor.
param(
	[Parameter(Mandatory=$true)][string]$Exe,
	[Parameter(Mandatory=$true)][string]$Nif,
	[Parameter(Mandatory=$true)][int]$Port,
	[Parameter(Mandatory=$true)][string]$Shot,
	[int]$Wait = 25
)
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
$src = @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class WinEnum {
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumWindowsProc cb, IntPtr l);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] static extern int GetWindowTextLength(IntPtr h);
  [DllImport("user32.dll")] static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  delegate bool EnumWindowsProc(IntPtr h, IntPtr l);
  public static List<string> ForPid(uint want) {
    var outp = new List<string>();
    EnumWindows(delegate(IntPtr h, IntPtr l) {
      uint pid; GetWindowThreadProcessId(h, out pid);
      if (pid != want) return true;
      var t = new StringBuilder(512); GetWindowText(h, t, 512);
      var c = new StringBuilder(256); GetClassName(h, c, 256);
      outp.Add((IsWindowVisible(h) ? "visible  " : "hidden   ") + "class=" + c.ToString() + "  title='" + t.ToString() + "'");
      return true;
    }, IntPtr.Zero);
    return outp;
  }
}
"@
Add-Type -TypeDefinition $src -ReferencedAssemblies System.Drawing

$env:WW_WINDOW_AT = "1960,40"
$p = Start-Process -FilePath $Exe -ArgumentList @("--port", "$Port", $Nif) -PassThru
Start-Sleep -Seconds $Wait
$q = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
if (-not $q) { "the process is gone after $Wait s"; exit 1 }
$q.Refresh()
"pid {0}, cpu {1:N2} s, responding={2}, main title '{3}'" -f $q.Id, $q.CPU, $q.Responding, $q.MainWindowTitle
"--- top-level windows ---"
[WinEnum]::ForPid([uint32]$q.Id) | ForEach-Object { $_ }

$b = [System.Windows.Forms.SystemInformation]::VirtualScreen
$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size)
$bmp.Save($Shot, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
"picture: $Shot"
Stop-Process -Id $p.Id -Force
