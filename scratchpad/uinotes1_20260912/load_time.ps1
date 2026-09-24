# Lane UINOTES1b -- how long a PLAIN NIF takes to open in the GUI, measured the
# same way on two exes. bungo, 2026-09-12 05:06: "the merged exe freezes when he
# opens ANY nif".
#
#   powershell -File load_time.ps1 -Exe <path> -Nif <path> -Port <n> -Label <name>
#
# It launches the exe WITH ITS OWN --port (never his window, which has none),
# puts it on the second monitor, and polls the main window title every 250 ms.
# Three stamps come out: the process is up, a window exists, and the title
# carries the file's name (which is set by onLoadComplete). A run that never
# reaches the third inside -Cap seconds is reported as NOT LOADED, with the last
# title seen and the process's own CPU seconds beside it.
param(
	[Parameter(Mandatory=$true)][string]$Exe,
	[Parameter(Mandatory=$true)][string]$Nif,
	[Parameter(Mandatory=$true)][int]$Port,
	[Parameter(Mandatory=$true)][string]$Label,
	[int]$Cap = 90
)

$stem = [System.IO.Path]::GetFileNameWithoutExtension($Nif)
$env:WW_WINDOW_AT = "1960,40"
$t0 = Get-Date
$p = Start-Process -FilePath $Exe -ArgumentList @("--port", "$Port", $Nif) -PassThru
$tUp = $null; $tWin = $null; $tTitle = $null; $last = ""
while (((Get-Date) - $t0).TotalSeconds -lt $Cap) {
	Start-Sleep -Milliseconds 250
	$q = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
	if (-not $q) { break }
	if (-not $tUp) { $tUp = ((Get-Date) - $t0).TotalSeconds }
	$q.Refresh()
	$t = $q.MainWindowTitle
	if ($t) { $last = $t; if (-not $tWin) { $tWin = ((Get-Date) - $t0).TotalSeconds } }
	if ($t -and $t -like "*$stem*") { $tTitle = ((Get-Date) - $t0).TotalSeconds; break }
}
$q = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
$cpu = if ($q) { [math]::Round($q.CPU, 2) } else { "gone" }
if ($tTitle) {
	"{0}: LOADED in {1:N2} s (window at {2:N2} s), cpu {3} s, title '{4}'" -f $Label, $tTitle, $tWin, $cpu, $last
} else {
	"{0}: NOT LOADED inside {1} s (window at {2} s), cpu {3} s, last title '{4}'" -f $Label, $Cap, $tWin, $cpu, $last
}
if ($q) {
	$null = $q.CloseMainWindow()
	Start-Sleep -Seconds 3
	$q = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
	if ($q) { Stop-Process -Id $p.Id -Force; "{0}: had to be stopped, it did not close" -f $Label }
}
