#!/usr/bin/env pwsh
# Stella Trail - Windows build script
# Downloads SDL2 and builds with MinGW or MSVC

param(
    [switch]$Clean,
    [string]$Toolchain = "auto"  # auto, mingw, msvc
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Root    = $PSScriptRoot
$DepsDir = "$Root\deps"
$BuildDir= "$Root\build"

$SDL2_VER      = "2.28.5"
$SDL2_IMG_VER  = "2.8.2"
$SDL2_TTF_VER  = "2.22.0"

function Get-SDL2-MinGW {
    param($Name, $Ver, $Url)
    $zip  = "$DepsDir\$Name-mingw.zip"
    $dest = "$DepsDir\$Name-mingw"
    if (Test-Path $dest) { Write-Host "$Name already extracted."; return }
    Write-Host "Downloading $Name $Ver..."
    Invoke-WebRequest -Uri $Url -OutFile $zip -UseBasicParsing
    Expand-Archive -Path $zip -DestinationPath $DepsDir -Force
    $extracted = Get-ChildItem $DepsDir -Directory | Where-Object { $_.Name -like "$Name*" -and $_.Name -ne "$Name-mingw" } | Select-Object -First 1
    if ($extracted) { Rename-Item $extracted.FullName $dest }
    Remove-Item $zip -ErrorAction SilentlyContinue
}

New-Item -ItemType Directory -Force $DepsDir  | Out-Null
New-Item -ItemType Directory -Force $BuildDir | Out-Null

# Download SDL2 MinGW packages
Get-SDL2-MinGW "SDL2"       $SDL2_VER    "https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VER/SDL2-devel-$SDL2_VER-mingw.zip"
Get-SDL2-MinGW "SDL2_image" $SDL2_IMG_VER "https://github.com/libsdl-org/SDL_image/releases/download/release-$SDL2_IMG_VER/SDL2_image-devel-$SDL2_IMG_VER-mingw.zip"
Get-SDL2-MinGW "SDL2_ttf"   $SDL2_TTF_VER "https://github.com/libsdl-org/SDL_ttf/releases/download/release-$SDL2_TTF_VER/SDL2_ttf-devel-$SDL2_TTF_VER-mingw.zip"

# Find include/lib dirs (MinGW packages use x86_64-w64-mingw32)
$ARCH = "i686-w64-mingw32"  # Use 32-bit to match MinGW installation
$sdlInc  = "$DepsDir\SDL2-mingw\$ARCH\include\SDL2"
$sdlLib  = "$DepsDir\SDL2-mingw\$ARCH\lib"
$sdlBin  = "$DepsDir\SDL2-mingw\$ARCH\bin"
$imgInc  = "$DepsDir\SDL2_image-mingw\$ARCH\include\SDL2"
$imgLib  = "$DepsDir\SDL2_image-mingw\$ARCH\lib"
$imgBin  = "$DepsDir\SDL2_image-mingw\$ARCH\bin"
$ttfInc  = "$DepsDir\SDL2_ttf-mingw\$ARCH\include\SDL2"
$ttfLib  = "$DepsDir\SDL2_ttf-mingw\$ARCH\lib"
$ttfBin  = "$DepsDir\SDL2_ttf-mingw\$ARCH\bin"

$INCLUDES = "-I`"$DepsDir\SDL2-mingw\$ARCH\include\SDL2`" -I`"$DepsDir\SDL2-mingw\$ARCH\include`" -I`"$DepsDir\SDL2_image-mingw\$ARCH\include`" -I`"$DepsDir\SDL2_ttf-mingw\$ARCH\include`""
$LIBS     = "-L`"$sdlLib`" -L`"$imgLib`" -L`"$ttfLib`" -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf"

$SOURCES = Get-ChildItem "$Root\src" -Filter "*.cpp" | ForEach-Object { "`"$($_.FullName)`"" }
$SRC_STR = $SOURCES -join " "

Write-Host "Compiling Stella Trail..."
$CMD = "g++ -std=c++14 -O2 -Wall $INCLUDES $SRC_STR $LIBS -o `"$BuildDir\StellaTrail.exe`""
Invoke-Expression $CMD

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed!"
    exit 1
}

# Copy DLLs
Write-Host "Copying DLLs..."
Copy-Item "$sdlBin\SDL2.dll"       $BuildDir -Force
Copy-Item "$imgBin\SDL2_image.dll" $BuildDir -Force -ErrorAction SilentlyContinue
Copy-Item "$ttfBin\SDL2_ttf.dll"   $BuildDir -Force -ErrorAction SilentlyContinue
# Copy extra image DLLs (libpng, libjpeg, zlib)
Get-ChildItem $imgBin -Filter "*.dll" | Copy-Item -Destination $BuildDir -Force -ErrorAction SilentlyContinue
Get-ChildItem $ttfBin -Filter "*.dll" | Copy-Item -Destination $BuildDir -Force -ErrorAction SilentlyContinue

# Copy assets
Copy-Item "$Root\assets" $BuildDir -Recurse -Force

# Download font if missing
$fontPath = "$BuildDir\assets\fonts\NotoSansKR.ttf"
if (-not (Test-Path $fontPath)) {
    New-Item -ItemType Directory -Force "$BuildDir\assets\fonts" | Out-Null
    Write-Host "Downloading font..."
    try {
        Invoke-WebRequest "https://github.com/googlefonts/noto-cjk/raw/main/Sans/OTF/Korean/NotoSansCJKkr-Regular.otf" `
            -OutFile $fontPath -UseBasicParsing -TimeoutSec 30
    } catch {
        Write-Warning "Font download failed — Korean text won't show. Add NotoSansKR.ttf manually."
    }
}

Write-Host ""
Write-Host "Build complete! Run: .\build\StellaTrail.exe"
