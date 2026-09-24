# BAKEPERF1: one region bake, timed, with the process's PEAK WORKING SET.
#
#   bake_run.ps1 -Exe <exe> -Out <dir> -Region "x0 y0 x1 y1" [-Threads N] [-Log <file>]
#
# Every path handed to -no-gui is ABSOLUTE (the CLI resolves a relative output
# path against release/, not the shell's cwd -- lodgen skill, lane NATIVE1a).
param(
  [Parameter(Mandatory=$true)][string]$Exe,
  [Parameter(Mandatory=$true)][string]$Out,
  [Parameter(Mandatory=$true)][string]$Region,
  [int]$Threads = 0,
  [int]$ChunkThreads = 0,
  [string]$Log = ""
)

. "$PSScriptRoot\no_crash_dialog.ps1"   # BAKEPERF1: no crash dialog on his desktop

$esm = "X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm"
if (Test-Path $Out) { Remove-Item -Recurse -Force $Out }
New-Item -ItemType Directory -Force -Path $Out | Out-Null
New-Item -ItemType Directory -Force -Path "$Out\tex" | Out-Null
if ($Log -eq "") { $Log = "$Out\bake.log" }

$r = $Region -split '\s+'
$argv = @('-no-gui','lodgen',$esm,'--worldspace','3C',
          '--terrain-region',$r[0],$r[1],$r[2],$r[3],'--dim','4',
          '--out-dir',$Out,'--tex-dir',"$Out\tex",'--native',$Out,
          '--arrays','--merge')
if ($Threads -gt 0) { $argv += @('--threads', "$Threads") }
if ($ChunkThreads -ne 0) { $argv += @('--chunk-threads', "$ChunkThreads") }
# PowerShell 5.1 joins -ArgumentList with plain spaces and quotes nothing:
# every argument that can hold a space is quoted here.
$quoted = $argv | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } }

$sw = [System.Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $Exe -ArgumentList $quoted -NoNewWindow -PassThru `
       -RedirectStandardOutput $Log -RedirectStandardError "$Log.err"
$h = $p.Handle   # keep the handle open so the exit code survives exit
# PeakWorkingSet64 is not readable once the process has exited, so it is
# sampled WHILE it runs and the maximum kept. 100 ms; a bake is tens of
# seconds, and the peak is reached inside a stage, not between two.
$peak = 0
while (-not $p.HasExited) {
  try { $p.Refresh(); if ($p.PeakWorkingSet64 -gt $peak) { $peak = $p.PeakWorkingSet64 } } catch {}
  Start-Sleep -Milliseconds 100
}
$p.WaitForExit()
$sw.Stop()
$rc = $p.ExitCode

"RUN exe=$Exe region=$Region threads=$Threads chunkThreads=$ChunkThreads"
"RC=$rc"
"WALL_MS=$($sw.ElapsedMilliseconds)"
"PEAK_WS_BYTES=$peak"
"PEAK_WS_MB=$([math]::Round($peak/1MB,1))"
Get-Content $Log | Select-String -Pattern 'stage times:' | ForEach-Object { "STAGES $_" }
$n = (Get-ChildItem -Recurse -File $Out | Where-Object { $_.Name -ne 'bake.log' -and $_.Name -ne 'bake.log.err' }).Count
"FILES=$n"
