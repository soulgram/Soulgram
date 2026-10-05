# Soulgram: generates the application icons (a ghost on a violet circle)
# into Telegram/Resources/art, replacing the official Telegram artwork.
# Usage: powershell -ExecutionPolicy Bypass -File soulgram\tools\make_icons.ps1

Add-Type -AssemblyName System.Drawing

$art = Join-Path $PSScriptRoot '..\..\Telegram\Resources\art'
$art = (Resolve-Path $art).Path

function New-SoulgramBitmap([int]$size, [double]$margin) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)

    $r = $size / 2.0 - $margin
    $cx = $size / 2.0
    $cy = $size / 2.0

    # Background circle with a violet -> pink gradient.
    $rect = New-Object System.Drawing.RectangleF(($cx - $r), ($cy - $r), (2 * $r), (2 * $r))
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        $rect,
        [System.Drawing.Color]::FromArgb(255, 110, 75, 255),
        [System.Drawing.Color]::FromArgb(255, 225, 80, 180),
        [System.Drawing.Drawing2D.LinearGradientMode]::ForwardDiagonal)
    $g.FillEllipse($brush, $rect)
    $brush.Dispose()

    # Ghost body: round head, straight sides, wavy bottom.
    $w = 1.00 * $r
    $left = $cx - $w / 2
    $right = $cx + $w / 2
    $top = $cy - 0.60 * $r
    $bottom = $cy + 0.40 * $r
    $wave = 0.12 * $r
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddArc([single]$left, [single]$top, [single]$w, [single]$w, 180, 180)
    $path.AddLine([single]$right, [single]($top + $w / 2), [single]$right, [single]$bottom)
    $bumps = 3
    $step = $w / $bumps
    for ($i = 0; $i -lt $bumps; $i++) {
        $x1 = $right - $i * $step
        $x2 = $x1 - $step
        $path.AddBezier(
            [single]$x1, [single]$bottom,
            [single]($x1 - $step * 0.15), [single]($bottom + $wave * 1.5),
            [single]($x2 + $step * 0.15), [single]($bottom + $wave * 1.5),
            [single]$x2, [single]$bottom)
    }
    $path.CloseFigure()
    $white = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    $g.FillPath($white, $path)
    $white.Dispose()
    $path.Dispose()

    # Eyes.
    $eye = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 70, 40, 150))
    $ew = 0.13 * $r
    $eh = 0.19 * $r
    $ey = $cy - 0.16 * $r
    $g.FillEllipse($eye, [single]($cx - 0.19 * $r - $ew / 2), [single]$ey, [single]$ew, [single]$eh)
    $g.FillEllipse($eye, [single]($cx + 0.19 * $r - $ew / 2), [single]$ey, [single]$ew, [single]$eh)
    $eye.Dispose()

    $g.Dispose()
    return $bmp
}

function Save-Png([int]$size, [double]$margin, [string]$name) {
    $bmp = New-SoulgramBitmap $size $margin
    $bmp.Save((Join-Path $art $name), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

function Get-PngBytes([int]$size, [double]$margin) {
    $bmp = New-SoulgramBitmap $size $margin
    $stream = New-Object System.IO.MemoryStream
    $bmp.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    return ,$stream.ToArray()
}

# Same proportions as the original artwork.
$ratio = 13.0 / 256.0
Save-Png 256 13 'logo_256.png'
Save-Png 256 3 'logo_256_no_margin.png'
Save-Png 1024 60 'icon_round512@2x.png'
foreach ($s in 16, 32, 48, 64, 128, 256, 512) {
    Save-Png $s ([Math]::Max(1, $s * $ratio)) "icon$s.png"
    Save-Png (2 * $s) ([Math]::Max(1, 2 * $s * $ratio)) "icon$s@2x.png"
}

# Windows .ico with PNG-compressed entries.
$sizes = 16, 24, 32, 48, 256
$images = @()
foreach ($s in $sizes) {
    $images += ,(Get-PngBytes $s ([Math]::Max(1, $s * $ratio)))
}
$out = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter($out)
$writer.Write([UInt16]0)
$writer.Write([UInt16]1)
$writer.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    $dim = if ($s -ge 256) { 0 } else { $s }
    $writer.Write([Byte]$dim)
    $writer.Write([Byte]$dim)
    $writer.Write([Byte]0)
    $writer.Write([Byte]0)
    $writer.Write([UInt16]1)
    $writer.Write([UInt16]32)
    $writer.Write([UInt32]$images[$i].Length)
    $writer.Write([UInt32]$offset)
    $offset += $images[$i].Length
}
foreach ($image in $images) {
    $writer.Write($image)
}
$writer.Flush()
[System.IO.File]::WriteAllBytes((Join-Path $art 'icon256.ico'), $out.ToArray())
$writer.Dispose()

Write-Output "Soulgram icons written to $art"
