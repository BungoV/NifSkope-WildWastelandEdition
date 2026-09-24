# Lane UINOTES1b -- opening one nif after another INTO THE SAME WINDOW, which is
# what bungo does. Each open goes through the exe's own --port IPC, which is the
# File > Open code path (NifSkope::openFile). Paths with a SPACE cannot be used:
# the IPC command is space-separated (src/main.cpp), so such a file never arrives
# at all -- that is a separate, older defect and not what this measures.
param(
	[Parameter(Mandatory=$true)][string]$Exe,
	[Parameter(Mandatory=$true)][string[]]$Nifs,
	[Parameter(Mandatory=$true)][int]$Port,
	[Parameter(Mandatory=$true)][string]$Label,
	[int]$Cap = 90
)
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
if (-not $ready) { "{0}: the empty window never appeared" -f $Label; exit 1 }
"{0}: empty window at {1:N2} s" -f $Label, $ready
Start-Sleep -Seconds 2

foreach ($n in $Nifs) {
	$stem = [System.IO.Path]::GetFileNameWithoutExtension($n)
	$q.Refresh(); $cpu0 = $q.CPU
	$t1 = Get-Date
	$null = Start-Process -FilePath $Exe -ArgumentList @("--port", "$Port", $n) -PassThru
	$done = $null
	while (((Get-Date) - $t1).TotalSeconds -lt $Cap) {
		Start-Sleep -Milliseconds 250
		$q = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
		if (-not $q) { break }
		$q.Refresh()
		if ($q.MainWindowTitle -like "*$stem*") { $done = ((Get-Date) - $t1).TotalSeconds; break }
	}
	if (-not $q) { "{0}: the process died opening {1}" -f $Label, $stem; exit 1 }
	$q.Refresh()
	$cpu = [math]::Round($q.CPU - $cpu0, 2)
	if ($done) { "{0}: {1} opened in {2:N2} s, cpu {3} s, responding={4}" -f $Label, $stem, $done, $cpu, $q.Responding }
	else       { "{0}: {1} DID NOT OPEN inside {2} s, cpu {3} s, responding={4}" -f $Label, $stem, $Cap, $cpu, $q.Responding }
	Start-Sleep -Seconds 1
}
$q = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
if ($q) { Stop-Process -Id $p.Id -Force }
