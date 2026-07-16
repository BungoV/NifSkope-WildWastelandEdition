param(
    [switch]$Build,
    [int]$Jobs = 4,
    [string]$RealTarget,
    [string]$RealDonor,
    [string]$RealReference,
    [string]$RealOutput
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$release = Join-Path $root 'release'
$executable = Join-Path $release 'RiggingIntegration.exe'
$out = Join-Path $PSScriptRoot 'out'
$target = Join-Path $PSScriptRoot 'fixtures\target.nif'
$donor = Join-Path $PSScriptRoot 'fixtures\donor.nif'
$result = Join-Path $out 'result.nif'
$stdout = Join-Path $out 'stdout.log'
$stderr = Join-Path $out 'stderr.log'
$realMode = -not [string]::IsNullOrWhiteSpace($RealTarget) -or
    -not [string]::IsNullOrWhiteSpace($RealDonor) -or
    -not [string]::IsNullOrWhiteSpace($RealReference) -or
    -not [string]::IsNullOrWhiteSpace($RealOutput)

if ($realMode) {
    if ([string]::IsNullOrWhiteSpace($RealTarget) -or
        [string]::IsNullOrWhiteSpace($RealDonor) -or
        [string]::IsNullOrWhiteSpace($RealReference)) {
        throw 'Real-asset mode requires -RealTarget, -RealDonor, and -RealReference.'
    }
    $target = (Resolve-Path -LiteralPath $RealTarget).Path
    $donor = (Resolve-Path -LiteralPath $RealDonor).Path
    $reference = (Resolve-Path -LiteralPath $RealReference).Path
    $result = if ([string]::IsNullOrWhiteSpace($RealOutput)) {
        Join-Path $out 'real_result.nif'
    } else {
        [System.IO.Path]::GetFullPath($RealOutput)
    }
    $stdout = Join-Path $out 'real_stdout.log'
    $stderr = Join-Path $out 'real_stderr.log'
}

$msys = 'C:\msys64'
if (Test-Path (Join-Path $msys 'ucrt64\bin')) {
    $env:PATH = (Join-Path $msys 'ucrt64\bin') + ';' +
        (Join-Path $msys 'usr\bin') + ';' + $env:PATH
}

if ($Build) {
    $qmake = Get-Command qmake6, qmake -ErrorAction SilentlyContinue | Select-Object -First 1
    $make = Get-Command mingw32-make, make -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $qmake -or -not $make) {
        throw 'qmake6/qmake and mingw32-make/make must be available on PATH.'
    }

    Push-Location $root
    try {
        & $qmake.Source -o Makefile.RiggingIntegration RiggingIntegration.pro 'CONFIG+=release'
        if ($LASTEXITCODE -ne 0) { throw "qmake failed with exit code $LASTEXITCODE." }
        & $make.Source -f Makefile.RiggingIntegration -j ([Math]::Max(1, $Jobs))
        if ($LASTEXITCODE -ne 0) { throw "RiggingIntegration build failed with exit code $LASTEXITCODE." }
    }
    finally {
        Pop-Location
    }
}

if (-not (Test-Path $executable)) {
    throw "Missing $executable. Run this script with -Build first."
}
if (-not (Test-Path $target) -or -not (Test-Path $donor)) {
    throw 'The Rigging integration fixtures are missing.'
}

New-Item -ItemType Directory -Force -Path $out | Out-Null
New-Item -ItemType Directory -Force -Path ([System.IO.Path]::GetDirectoryName($result)) | Out-Null
$oldPlatform = $env:QT_QPA_PLATFORM
Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue
try {
    $arguments = if ($realMode) {
        @('--real', ('"' + $target + '"'), ('"' + $donor + '"'),
            ('"' + $result + '"'), ('"' + $reference + '"'))
    } else {
        @($target, $donor, $result)
    }
    $process = Start-Process -FilePath $executable `
        -ArgumentList $arguments `
        -WorkingDirectory $release `
        -RedirectStandardOutput $stdout `
        -RedirectStandardError $stderr `
        -WindowStyle Hidden -Wait -PassThru
}
finally {
    if ($null -ne $oldPlatform) { $env:QT_QPA_PLATFORM = $oldPlatform }
}

if ($process.ExitCode -ne 0) {
    Get-Content -LiteralPath $stderr -Tail 80 -ErrorAction SilentlyContinue
    throw "Rigging integration test failed with exit code $($process.ExitCode)."
}

$passPattern = if ($realMode) { '^REAL_PASS ' } else { '^PASS ' }
$pass = Select-String -LiteralPath $stderr -Pattern $passPattern | Select-Object -Last 1
if (-not $pass) {
    Get-Content -LiteralPath $stderr -Tail 80 -ErrorAction SilentlyContinue
    throw 'Rigging integration executable exited successfully without a PASS record.'
}

$pass.Line
