Add-Type -AssemblyName System.Drawing

$outDir = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\res\icon'))
[IO.Directory]::CreateDirectory($outDir) | Out-Null

$outline = [Drawing.Color]::FromArgb(238, 240, 244)
$muted = [Drawing.Color]::FromArgb(155, 163, 174)
$cyan = [Drawing.Color]::FromArgb(63, 199, 232)
$blue = [Drawing.Color]::FromArgb(70, 130, 255)
$purple = [Drawing.Color]::FromArgb(168, 85, 247)
$green = [Drawing.Color]::FromArgb(66, 210, 118)
$yellow = [Drawing.Color]::FromArgb(255, 211, 86)
$orange = [Drawing.Color]::FromArgb(255, 132, 62)
$red = [Drawing.Color]::FromArgb(255, 78, 96)

function Write-BlockIcon([string] $name, [scriptblock] $draw) {
	$bitmap = New-Object Drawing.Bitmap 36, 36, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
	$graphics = [Drawing.Graphics]::FromImage($bitmap)
	$graphics.Clear([Drawing.Color]::Transparent)
	$graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
	$graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
	& $draw $graphics
	$bitmap.Save((Join-Path $outDir $name), [Drawing.Imaging.ImageFormat]::Png)
	$graphics.Dispose()
	$bitmap.Dispose()
}

Write-BlockIcon 'block-node.png' {
	param($g)
	$fill = New-Object Drawing.SolidBrush $orange
	$pen = New-Object Drawing.Pen ([Drawing.Color]::FromArgb(210, 183, 71, 25)), 1.5
	$g.FillEllipse($fill, 11, 11, 14, 14)
	$g.DrawEllipse($pen, 11, 11, 14, 14)
	$pen.Dispose(); $fill.Dispose()
}

Write-BlockIcon 'block-geometry.png' {
	param($g)
	$back = [Drawing.Point[]]@([Drawing.Point]::new(7, 7), [Drawing.Point]::new(23, 7), [Drawing.Point]::new(23, 23), [Drawing.Point]::new(7, 23), [Drawing.Point]::new(7, 7))
	$front = [Drawing.Point[]]@([Drawing.Point]::new(13, 13), [Drawing.Point]::new(29, 13), [Drawing.Point]::new(29, 29), [Drawing.Point]::new(13, 29), [Drawing.Point]::new(13, 13))
	$rearPen = New-Object Drawing.Pen $muted, 1.6
	$frontPen = New-Object Drawing.Pen $orange, 1.8
	$edgePen = New-Object Drawing.Pen $outline, 1.6
	$g.DrawLines($rearPen, $back)
	$g.DrawLines($frontPen, $front)
	$g.DrawLine($edgePen, 7, 7, 13, 13); $g.DrawLine($edgePen, 23, 7, 29, 13)
	$g.DrawLine($edgePen, 23, 23, 29, 29); $g.DrawLine($edgePen, 7, 23, 13, 29)
	$rearPen.Dispose(); $frontPen.Dispose(); $edgePen.Dispose()
}

Write-BlockIcon 'block-material.png' {
	param($g)
	$rect = [Drawing.Rectangle]::new(5, 5, 26, 26)
	$fill = New-Object Drawing.Drawing2D.LinearGradientBrush $rect, $purple, $cyan, 45
	$rim = New-Object Drawing.Pen $outline, 1.5
	$g.FillEllipse($fill, $rect)
	$g.DrawEllipse($rim, $rect)
	$shade = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(105, 15, 18, 28))
	$g.FillPie($shade, 5, 5, 26, 26, 15, 155)
	$shine = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(205, 255, 255, 255))
	$g.FillEllipse($shine, 10, 9, 6, 5)
	$shade.Dispose(); $shine.Dispose(); $rim.Dispose(); $fill.Dispose()
}

Write-BlockIcon 'block-texture.png' {
	param($g)
	$frame = New-Object Drawing.Pen $outline, 1.7
	$sky = New-Object Drawing.SolidBrush $cyan
	$ground = New-Object Drawing.SolidBrush $green
	$mountain = New-Object Drawing.SolidBrush $purple
	$sun = New-Object Drawing.SolidBrush $yellow
	$g.FillRectangle($sky, 6, 7, 24, 22)
	$g.FillPolygon($ground, [Drawing.Point[]]@([Drawing.Point]::new(6,25),[Drawing.Point]::new(17,17),[Drawing.Point]::new(30,27),[Drawing.Point]::new(30,29),[Drawing.Point]::new(6,29)))
	$g.FillPolygon($mountain, [Drawing.Point[]]@([Drawing.Point]::new(12,24),[Drawing.Point]::new(21,14),[Drawing.Point]::new(30,24)))
	$g.FillEllipse($sun, 10, 10, 5, 5)
	$g.DrawRectangle($frame, 6, 7, 24, 22)
	$frame.Dispose(); $sky.Dispose(); $ground.Dispose(); $mountain.Dispose(); $sun.Dispose()
}

Write-BlockIcon 'block-animation.png' {
	param($g)
	$fill = New-Object Drawing.SolidBrush $green
	$rim = New-Object Drawing.Pen ([Drawing.Color]::FromArgb(29, 145, 75)), 1.8
	$points = [Drawing.Point[]]@([Drawing.Point]::new(11,7),[Drawing.Point]::new(29,18),[Drawing.Point]::new(11,29))
	$g.FillPolygon($fill, $points)
	$g.DrawPolygon($rim, $points)
	$fill.Dispose(); $rim.Dispose()
}

Write-BlockIcon 'block-particles.png' {
	param($g)
	$trail = New-Object Drawing.Pen ([Drawing.Color]::FromArgb(120, 185, 195, 210)), 1
	$g.DrawLine($trail, 8, 27, 13, 22); $g.DrawLine($trail, 17, 28, 19, 20); $g.DrawLine($trail, 24, 25, 25, 17)
	$dots = @(
		@($cyan, 7, 24, 5), @($outline, 13, 17, 4), @($orange, 17, 25, 6),
		@($green, 23, 13, 5), @($yellow, 27, 22, 4), @($purple, 10, 11, 4),
		@($red, 19, 7, 3), @($blue, 28, 8, 3)
	)
	foreach ($dot in $dots) {
		$brush = New-Object Drawing.SolidBrush $dot[0]
		$g.FillEllipse($brush, $dot[1], $dot[2], $dot[3], $dot[3])
		$brush.Dispose()
	}
	$trail.Dispose()
}
Write-BlockIcon 'block-camera.png' {
	param($g)
	$body = New-Object Drawing.SolidBrush $blue
	$top = New-Object Drawing.SolidBrush $muted
	$rim = New-Object Drawing.Pen $outline, 1.6
	$lens = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(35, 43, 56))
	$glass = New-Object Drawing.SolidBrush $cyan
	$g.FillRectangle($body, 5, 11, 26, 18)
	$g.FillPolygon($top, [Drawing.Point[]]@([Drawing.Point]::new(10,11),[Drawing.Point]::new(14,7),[Drawing.Point]::new(22,7),[Drawing.Point]::new(25,11)))
	$g.DrawRectangle($rim, 5, 11, 26, 18)
	$g.FillEllipse($lens, 11, 13, 14, 14); $g.DrawEllipse($rim, 11, 13, 14, 14)
	$g.FillEllipse($glass, 15, 17, 6, 6)
	$body.Dispose(); $top.Dispose(); $rim.Dispose(); $lens.Dispose(); $glass.Dispose()
}

Write-BlockIcon 'block-extra-data.png' {
	param($g)
	$paper = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(225, 235, 238, 244))
	$fold = New-Object Drawing.SolidBrush $muted
	$line = New-Object Drawing.Pen $cyan, 1.8
	$badge = New-Object Drawing.SolidBrush $orange
	$plus = New-Object Drawing.Pen ([Drawing.Color]::White), 1.8
	$g.FillPolygon($paper, [Drawing.Point[]]@([Drawing.Point]::new(7,5),[Drawing.Point]::new(23,5),[Drawing.Point]::new(29,11),[Drawing.Point]::new(29,31),[Drawing.Point]::new(7,31)))
	$g.FillPolygon($fold, [Drawing.Point[]]@([Drawing.Point]::new(23,5),[Drawing.Point]::new(29,11),[Drawing.Point]::new(23,11)))
	$g.DrawLine($line, 11,15,24,15); $g.DrawLine($line,11,20,21,20); $g.DrawLine($line,11,25,18,25)
	$g.FillEllipse($badge, 21, 21, 12, 12)
	$g.DrawLine($plus, 27,24,27,30); $g.DrawLine($plus,24,27,30,27)
	$paper.Dispose(); $fold.Dispose(); $line.Dispose(); $badge.Dispose(); $plus.Dispose()
}

Write-Host "Generated Block List icons in $outDir"
