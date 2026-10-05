# This file is part of Soulgram,
# an unofficial fork of Telegram Desktop.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

# Generates the wizard images of the installer from the Soulgram icon.
# Usage: make_installer_images.ps1 -OutDir <folder>
param([Parameter(Mandatory = $true)][string]$OutDir)

Add-Type -AssemblyName System.Drawing

$art = Join-Path $PSScriptRoot '..\..\..\Resources\art'
$icon = [System.Drawing.Image]::FromFile((Resolve-Path (Join-Path $art 'icon512@2x.png')))
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

function New-Canvas([int]$width, [int]$height) {
	$bitmap = New-Object System.Drawing.Bitmap($width, $height, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
	$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
	$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
	$graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
	$graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
	return @($bitmap, $graphics)
}

# The big image on the first and the last page: the gradient of the icon,
# the icon and the name.
function Save-Big([int]$width, [int]$height, [string]$path) {
	$bitmap, $graphics = New-Canvas $width $height
	$rect = New-Object System.Drawing.Rectangle(0, 0, $width, $height)
	$brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
		$rect,
		[System.Drawing.Color]::FromArgb(124, 77, 245),
		[System.Drawing.Color]::FromArgb(202, 80, 192),
		60.0)
	$graphics.FillRectangle($brush, $rect)
	$size = [int]($width * 0.62)
	$left = [int](($width - $size) / 2)
	$top = [int]($height * 0.30)
	$graphics.DrawImage($icon, $left, $top, $size, $size)
	$font = New-Object System.Drawing.Font('Segoe UI Semibold', [single]($width * 0.12), [System.Drawing.GraphicsUnit]::Pixel)
	$format = New-Object System.Drawing.StringFormat
	$format.Alignment = [System.Drawing.StringAlignment]::Center
	$textRect = New-Object System.Drawing.RectangleF(0, ($top + $size + $width * 0.06), $width, ($width * 0.2))
	$graphics.DrawString('Soulgram', $font, [System.Drawing.Brushes]::White, $textRect, $format)
	$bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Bmp)
	$graphics.Dispose()
	$bitmap.Dispose()
}

# The small image in the corner of the other pages: the icon on white.
function Save-Small([int]$size, [string]$path) {
	$bitmap, $graphics = New-Canvas $size $size
	$graphics.Clear([System.Drawing.Color]::White)
	$graphics.DrawImage($icon, 0, 0, $size, $size)
	$bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Bmp)
	$graphics.Dispose()
	$bitmap.Dispose()
}

# Sizes for 100%, 150% and 200% scale.
Save-Big 164 314 (Join-Path $OutDir 'wizard_100.bmp')
Save-Big 246 471 (Join-Path $OutDir 'wizard_150.bmp')
Save-Big 328 628 (Join-Path $OutDir 'wizard_200.bmp')
Save-Small 58 (Join-Path $OutDir 'wizard_small_100.bmp')
Save-Small 87 (Join-Path $OutDir 'wizard_small_150.bmp')
Save-Small 116 (Join-Path $OutDir 'wizard_small_200.bmp')
$icon.Dispose()
Write-Output "Images saved to $OutDir"
