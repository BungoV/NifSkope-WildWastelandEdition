# Render-regression capture + compare.
#
# Guards the renderer against changes to the lighting path — specifically the
# CPU particle simulation and the screen-space refraction preview, which are
# easy to break from a shader edit and have no other automated check.
#
#   .\capture.ps1 -Mode baseline      # capture the reference set (before a change)
#   .\capture.ps1 -Mode compare       # re-render and diff against the baseline
#
# Each NIF is rendered by its own NifSkope process via WW_RENDER_SHOT, which
# pins the camera (setOrientation) and the scene clock (setSceneTime) so the
# pixels are reproducible — the particle sim is time-driven, so without a pinned
# clock nothing would ever match.

param(
    [ValidateSet('baseline','compare')] [string] $Mode = 'compare',
    [int] $Tolerance = 0          # allowed differing pixels per image
)

$ErrorActionPreference = 'Stop'
$exe   = 'E:\Projects\ClaudeNifskope\release\NifSkope.exe'
$root  = 'E:\Projects\ClaudeNifskope\tools\render_regression'
$fo4   = 'E:\Tools\Fallout 4\DataUnpacked\Data'
$repo  = 'E:\Projects\ClaudeNifskope'

# Corpus: each entry must exercise a distinct renderer path. Keep the reasons
# written down — a case whose purpose is unclear gets dropped the first time it
# is inconvenient, and that is exactly how the particle sim loses its cover.
$corpus = @(
    @{ name = 'particles_mist';   path = "$fo4\Meshes\Effects\AttachFXMist01.nif";              why = 'NiPSys particle sim + flipbook' }
    @{ name = 'particles_glow';   path = "$fo4\Meshes\Effects\BlackGlowFill01.nif";             why = 'BSEffectShaderProperty particle shading' }
    @{ name = 'glass_visor';      path = "$fo4\Meshes\Effects\CA-PowerArmorVisorGlass01.nif";   why = 'glass / alpha-blended lighting shader' }
    @{ name = 'glass_shader';     path = "$fo4\Meshes\Effects\GlassShader01.nif";               why = 'glass effect shader' }
    @{ name = 'refraction_fixed'; path = "$repo\tests\render\refraction_fixture.nif";           why = 'SLSF1_Refraction forced on — screen-space refraction path' }
    @{ name = 'lit_setdressing';  path = "$fo4\Meshes\SetDressing\ACDucts\ACDuctConnector01.nif"; why = 'standard FO4 spec/gloss lighting (_s map)' }
    @{ name = 'lit_head';         path = "$repo\tests\rigging\fixtures\donor.nif";              why = 'skinned mesh, subsurface/softlight path' }
)

$outDir  = Join-Path $root $(if ($Mode -eq 'baseline') { 'baseline' } else { 'current' })
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;using System.Drawing;using System.Drawing.Imaging;using System.Runtime.InteropServices;
public class ImgDiff {
  // Returns differing pixel count, or -1 if the two images are not comparable.
  public static long Compare(string a, string b, out int maxChannelDelta) {
    maxChannelDelta = 0;
    using (var ia = new Bitmap(a)) using (var ib = new Bitmap(b)) {
      if (ia.Width != ib.Width || ia.Height != ib.Height) return -1;
      var r = new Rectangle(0,0,ia.Width,ia.Height);
      var da = ia.LockBits(r, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
      var db = ib.LockBits(r, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
      long n = ia.Width * (long)ia.Height * 4;
      byte[] ba = new byte[n], bb = new byte[n];
      Marshal.Copy(da.Scan0, ba, 0, (int)n); Marshal.Copy(db.Scan0, bb, 0, (int)n);
      ia.UnlockBits(da); ib.UnlockBits(db);
      long diff = 0; int mx = 0;
      for (long i = 0; i < n; i += 4) {
        int d = Math.Abs(ba[i]-bb[i]) + Math.Abs(ba[i+1]-bb[i+1]) + Math.Abs(ba[i+2]-bb[i+2]);
        if (d != 0) { diff++; int per = Math.Max(Math.Abs(ba[i]-bb[i]), Math.Max(Math.Abs(ba[i+1]-bb[i+1]), Math.Abs(ba[i+2]-bb[i+2]))); if (per > mx) mx = per; }
      }
      maxChannelDelta = mx;
      return diff;
    }
  }
}
'@ -ReferencedAssemblies System.Drawing

$results = @()
foreach ($c in $corpus) {
    $out = Join-Path $outDir "$($c.name).png"
    if (-not (Test-Path $c.path)) {
        Write-Host ("SKIP  {0,-16} missing asset: {1}" -f $c.name, $c.path) -ForegroundColor DarkYellow
        $results += @{ name = $c.name; status = 'missing' }
        continue
    }
    Remove-Item $out -ErrorAction SilentlyContinue
    $env:WW_RENDER_SHOT = $out
    $env:WW_RENDER_TIME = '1.0'
    $p = Start-Process $exe -ArgumentList "`"$($c.path)`"" -PassThru -Wait
    Remove-Item Env:\WW_RENDER_SHOT, Env:\WW_RENDER_TIME -ErrorAction SilentlyContinue

    if (-not (Test-Path $out)) {
        Write-Host ("FAIL  {0,-16} no framebuffer written (exit {1})" -f $c.name, $p.ExitCode) -ForegroundColor Red
        $results += @{ name = $c.name; status = 'no-output' }
        continue
    }

    if ($Mode -eq 'baseline') {
        Write-Host ("BASE  {0,-16} {1,8:N0} bytes   {2}" -f $c.name, (Get-Item $out).Length, $c.why)
        $results += @{ name = $c.name; status = 'captured' }
        continue
    }

    $ref = Join-Path $root "baseline\$($c.name).png"
    if (-not (Test-Path $ref)) {
        Write-Host ("NEW   {0,-16} no baseline to compare against" -f $c.name) -ForegroundColor DarkYellow
        $results += @{ name = $c.name; status = 'no-baseline' }
        continue
    }
    $mx = 0
    $d = [ImgDiff]::Compare($ref, $out, [ref]$mx)
    if ($d -lt 0) {
        Write-Host ("FAIL  {0,-16} size mismatch vs baseline" -f $c.name) -ForegroundColor Red
        $results += @{ name = $c.name; status = 'size-mismatch' }
    } elseif ($d -le $Tolerance) {
        Write-Host ("OK    {0,-16} identical" -f $c.name) -ForegroundColor Green
        $results += @{ name = $c.name; status = 'ok' }
    } else {
        Write-Host ("DIFF  {0,-16} {1,10:N0} px differ, max channel delta {2}   ({3})" -f $c.name, $d, $mx, $c.why) -ForegroundColor Yellow
        $results += @{ name = $c.name; status = 'diff'; pixels = $d; max = $mx }
    }
}

Write-Host ""
# The @() around each filter is load-bearing. Where-Object returning exactly ONE
# hashtable hands back the hashtable itself, not a 1-element array, and .Count on
# a Hashtable is its KEY count — so a single DIFF row reported "diff=4" (name,
# status, pixels, max) and a single OK row would report "ok=2". The totals only
# looked correct while every status happened to match more than one case, which
# is why this went unnoticed until 07-27.
Write-Host ("mode={0}  ok={1} diff={2} failed={3} missing={4}" -f $Mode,
    @($results | Where-Object { $_.status -eq 'ok' }).Count,
    @($results | Where-Object { $_.status -eq 'diff' }).Count,
    @($results | Where-Object { $_.status -in @('no-output','size-mismatch') }).Count,
    @($results | Where-Object { $_.status -in @('missing','no-baseline') }).Count)
