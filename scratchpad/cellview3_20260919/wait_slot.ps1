# Wait until the exe slot is free: no NifSkope process, and no gate driver.
# A gate driver is a bash.exe whose command line is JUST a script path ending
# .sh -- the watcher's own shell always carries 'snapshot-bash' and is excluded,
# which is the whole point (a wait condition that matches itself never fires).
while ($true) {
  $nif = @(Get-CimInstance Win32_Process -Filter "Name='NifSkope.exe'")
  $drv = @(Get-CimInstance Win32_Process -Filter "Name='bash.exe'" |
           Where-Object { $_.CommandLine -notmatch 'snapshot-bash' -and
                          $_.CommandLine -match '\.sh\s*$' })
  if ($nif.Count -eq 0 -and $drv.Count -eq 0) { Write-Output "SLOT-FREE"; break }
  Start-Sleep -Seconds 10
}
