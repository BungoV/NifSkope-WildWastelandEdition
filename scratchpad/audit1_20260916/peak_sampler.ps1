param([string]$OutFile, [string]$StopFile)
# AUDIT1: sample the peak working set of every NifSkope process until <StopFile>
# appears, then write the maximum (bytes) to <OutFile>. Independent of the exe's
# own "peak working set:" census clause, so the two can be compared.
$peak = 0
$samples = 0
while (-not (Test-Path $StopFile)) {
    $ps = Get-Process NifSkope -ErrorAction SilentlyContinue
    if ($ps) {
        foreach ($p in $ps) {
            $samples++
            if ($p.WorkingSet64 -gt $peak) { $peak = $p.WorkingSet64 }
        }
    }
    Start-Sleep -Milliseconds 400
}
"$peak $samples" | Out-File -FilePath $OutFile -Encoding ascii
