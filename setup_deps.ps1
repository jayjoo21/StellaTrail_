# Stella Trail - SDL2 Dependency Setup Script
# Run this script to download SDL2, SDL2_image, SDL2_ttf for Windows

param(
    [string]$depsDir = "$PSScriptRoot\deps"
)

$SDL2_VERSION      = "2.28.5"
$SDL2_IMAGE_VERSION = "2.8.2"
$SDL2_TTF_VERSION  = "2.22.0"

$urls = @{
    "SDL2"       = "https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VERSION/SDL2-devel-$SDL2_VERSION-VC.zip"
    "SDL2_image" = "https://github.com/libsdl-org/SDL_image/releases/download/release-$SDL2_IMAGE_VERSION/SDL2_image-devel-$SDL2_IMAGE_VERSION-VC.zip"
    "SDL2_ttf"   = "https://github.com/libsdl-org/SDL_ttf/releases/download/release-$SDL2_TTF_VERSION/SDL2_ttf-devel-$SDL2_TTF_VERSION-VC.zip"
}

New-Item -ItemType Directory -Force $depsDir | Out-Null

foreach ($name in $urls.Keys) {
    $url  = $urls[$name]
    $zip  = "$depsDir\$name.zip"
    Write-Host "Downloading $name..."
    Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
    Write-Host "Extracting $name..."
    Expand-Archive -Path $zip -DestinationPath $depsDir -Force
    Remove-Item $zip
}

# Rename directories to expected names
$renames = @{
    "SDL2-$SDL2_VERSION"            = "SDL2"
    "SDL2_image-$SDL2_IMAGE_VERSION" = "SDL2_image"
    "SDL2_ttf-$SDL2_TTF_VERSION"    = "SDL2_ttf"
}
foreach ($old in $renames.Keys) {
    $oldPath = "$depsDir\$old"
    $newPath = "$depsDir\$($renames[$old])"
    if (Test-Path $oldPath) {
        if (Test-Path $newPath) { Remove-Item $newPath -Recurse -Force }
        Rename-Item $oldPath $newPath
    }
}

# Download a free Korean-compatible font (Noto Sans KR)
$fontsDir = "$PSScriptRoot\assets\fonts"
New-Item -ItemType Directory -Force $fontsDir | Out-Null
$fontUrl = "https://github.com/notofonts/noto-cjk/raw/main/Sans/OTF/Korean/NotoSansCJKkr-Regular.otf"
Write-Host "Downloading Noto Sans KR font..."
try {
    Invoke-WebRequest -Uri $fontUrl -OutFile "$fontsDir\NotoSansKR.ttf" -UseBasicParsing
} catch {
    Write-Warning "Font download failed. Korean text will not render. Place any TTF font as assets/fonts/NotoSansKR.ttf"
}

Write-Host ""
Write-Host "=== Setup complete! ==="
Write-Host "Now run:"
Write-Host "  cmake -B build -DSDL2_DIR=deps/SDL2/cmake -DSDL2_image_DIR=deps/SDL2_image/cmake -DSDL2_ttf_DIR=deps/SDL2_ttf/cmake"
Write-Host "  cmake --build build --config Release"
