<#
.SYNOPSIS
	Build the shippable zip from release\, and prove it is the build you meant.

.DESCRIPTION
	release\ is a working directory as well as a deploy target: the .pro copies
	Qt's DLLs and plugins there after every link, and every WW_* harness drops its
	logs, framebuffer grabs and dialog screenshots beside them. The 0.3 package was
	assembled by hand, which is why this exists -- eighty-three files of test
	output sat one careless "copy the folder" away from a public download.

	So the manifest is an ALLOW-list, not a list of things to skip. A new harness
	writing a new kind of file lands outside it by default, which is the failure
	direction that costs nothing.

	It also checks the binary's own version string rather than trusting the
	argument. NIFSKOPE_VERSION comes from a DEFINE, and object files do not depend
	on the Makefile that carries it: bump WW_VER, rebuild, and main.o is not
	recompiled unless it is deleted. The 0.3 package shipped a binary announcing
	itself as 0.2 for exactly that reason.

.EXAMPLE
	pwsh tools\package.ps1 -Version 0.3.1
#>
param(
	[Parameter(Mandatory = $true)][string]$Version,
	[string]$Root = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'

$src  = Join-Path $Root 'release'
$dist = Join-Path $Root 'dist'
$name = "NifSkope-WildWastelandEdition-$Version-win64"
$tree = Join-Path $dist $name
$zip  = "$tree.zip"

if (-not (Test-Path (Join-Path $src 'NifSkope.exe'))) { throw "no NifSkope.exe in $src" }

# The binary's word, not the caller's. strings is not available everywhere, so
# read the UTF-16 literal Qt stores for the display name straight out of the file.
$bytes = [System.IO.File]::ReadAllBytes((Join-Path $src 'NifSkope.exe'))
$utf16 = [System.Text.Encoding]::Unicode.GetString($bytes)
$m = [regex]::Match($utf16, 'Wild Wasteland Edition ([0-9][0-9.]*)')
if (-not $m.Success) { throw "no version string found in NifSkope.exe" }
$built = $m.Groups[1].Value
if ($built -ne $Version) {
	throw "NifSkope.exe says $built, you asked for $Version. Objects do not depend on the Makefile: run qmake6, delete GeneratedFiles/.obj/main.o and about_dialog.o, and rebuild."
}
Write-Host "binary announces itself as $built"

# What ships. Everything else in release\ is build scratch or harness output.
$rootExt  = @('.dll', '.txt', '.xml', '.qss', '.conf', '.cmd')
$subDirs  = @('imageformats', 'platforms', 'shaders', 'styles')

if (Test-Path $tree) { Remove-Item $tree -Recurse -Force }
New-Item -ItemType Directory -Path $tree -Force | Out-Null

$skipped = @()
foreach ($f in Get-ChildItem $src -File) {
	$keep = $false
	if ($f.Name -eq 'NifSkope.exe') { $keep = $true }
	elseif ($rootExt -contains $f.Extension.ToLower()) { $keep = $true }
	# Harness output and scratch never ship, whatever extension they wear.
	#
	# -clike, and the case is load-bearing: every harness writes ww_ in lower
	# case and the shipped feature list is WW_FEATURES.txt. PowerShell's -like
	# is case-INSENSITIVE, so the first version of this line quietly dropped the
	# one document the fork exists to advertise -- caught only by diffing the
	# result against the previous package, which is why that diff is now a step.
	if ($f.Name -clike 'ww_*' -or $f.Name -clike '_*') { $keep = $false }
	if ($keep) { Copy-Item $f.FullName -Destination $tree }
	else { $skipped += $f.Name }
}
foreach ($d in $subDirs) {
	$from = Join-Path $src $d
	if (Test-Path $from) { Copy-Item $from -Destination $tree -Recurse }
}

# Present, not merely not-excluded. An allow-list can only drop things silently;
# this is the half that notices.
$required = @('NifSkope.exe', 'nif.xml', 'kfm.xml', 'style.qss', 'qt.conf',
	'nifskope-cli.cmd', 'LICENSE.txt', 'README.txt', 'WW_FEATURES.txt',
	'CHANGELOG.txt', 'CLI.txt', 'README_GLTF.txt', 'TROUBLESHOOTING.txt')
$missing = $required | Where-Object { -not (Test-Path (Join-Path $tree $_)) }
foreach ($d in $subDirs) {
	if (-not (Get-ChildItem (Join-Path $tree $d) -File -ErrorAction SilentlyContinue)) {
		$missing += "$d\ (empty or absent)"
	}
}
if ($missing) { throw "package is missing: $($missing -join ', ')" }

$files = Get-ChildItem $tree -Recurse -File
Write-Host ("packaged {0} files, left {1} behind" -f $files.Count, $skipped.Count)

if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $tree '*') -DestinationPath $zip -CompressionLevel Optimal

$hash = (Get-FileHash $zip -Algorithm SHA256).Hash
$size = [math]::Round((Get-Item $zip).Length / 1MB, 1)
Write-Host ""
Write-Host "$zip"
Write-Host ("{0} MB   SHA-256 {1}" -f $size, $hash)
