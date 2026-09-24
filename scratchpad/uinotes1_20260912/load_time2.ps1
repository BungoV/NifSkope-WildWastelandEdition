# Lane UINOTES1b -- the SECOND open, the one bungo actually does: the window is
# already up and a file is opened INTO it. The exe's own --port IPC carries
# exactly that ("NifSkope::open <file>", src/main.cpp), so a second launch on the
# same port drives NifSkope::openFile the way File > Open does.
#
#   powershell -File load_time2.ps1 -Exe <path> -Nif <path> -Port <n> -Label <name>
param(
	[Parameter(Mandatory=$true)][string]$Exe,
	[Parameter(Mandatory=$true)][string]$Nif,
	[Parameter(Mandatory=$true)][int]$Port,
	[Parameter(Mandatory=$true)][string]$Label,
	[int]$Cap = 120
)

$stem = [System.IO.Path]::GetFileNameWithoutExtension($Nif)
$env:WW_WINDOW_AT = "1960,40"
$p = Start-Process -FilePath $Exe -ArgumentList @("--port", "$Port") -PassThru
$t0 = Get-Date
$ready = $null
while (((Get-Date) - $t0).TotalSeconds -lt 60) {
	Start-Sleep -Milliseconds 250
	$q = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
	if (-not $q) { break }
	$q.Refresh()
	if ($q.MainWindowTitle) { $ready = ((Get-Date) - $t0).TotalSeconds; break }
}
if (-not $ready) { "{0}: the empty window never appeared" -f $Label; if ($q) { Stop-Process -Id $p.Id -Force }; exit 1 }
Start-Sleep -Seconds 2
$q.Refresh()
$before = $q.MainWindowTitle
$cpu0 = $q.CPU

# the open itself
$t1 = Get-Date
$null = Start-Process -FilePath $Exe -ArgumentList @("--port", "$Port", $Nif) -PassThru
$tOpen = $null; $last = $before
while (((Get-Date) - $t1).TotalSeconds -lt $Cap) {
	Start-Sleep -Milliseconds 250
	$q = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
	if (-not $q) { break }
	$q.Refresh()
	$t = $q.MainWindowTitle
	if ($t) { $last = $t }
	if ($t -and $t -like "*$stem*") { $tOpen = ((Get-Date) - $t1).TotalSeconds; break }
}
$q = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
$cpu = if ($q) { [math]::Round($q.CPU - $cpu0, 2) } else { "gone" }
if ($tOpen) {
	"{0}: empty window at {1:N2} s; the open took {2:N2} s, cpu {3} s" -f $Label, $ready, $tOpen, $cpu
} else {
	"{0}: empty window at {1:N2} s; the open DID NOT FINISH inside {2} s, cpu {3} s, title still '{4}'" -f $Label, $ready, $Cap, $cpu, $last
}
if ($q) {
	$null = $q.CloseMainWindow()
	Start-Sleep -Seconds 3
	$q = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
	if ($q) { Stop-Process -Id $p.Id -Force; "{0}: had to be stopped, it did not close" -f $Label }
}
