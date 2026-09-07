[CmdletBinding()]
param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\build'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\out\packages'),
    [string]$AssetRoot = (Join-Path $PSScriptRoot '..\out\assets'),
    [switch]$IncludeDetailedAssets
)
$ErrorActionPreference = 'Stop'
$source = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$BuildDirectory = [System.IO.Path]::GetFullPath($BuildDirectory)
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
$AssetRoot = [System.IO.Path]::GetFullPath($AssetRoot)
$versionMatch = [regex]::Match((Get-Content -Raw -LiteralPath (Join-Path $source 'CMakeLists.txt')), 'project\(Fury VERSION ([0-9.]+)')
if (-not $versionMatch.Success) { throw 'Cannot read Fury version' }
$version = $versionMatch.Groups[1].Value
$suffix = if ($IncludeDetailedAssets) { '-coastal' } else { '' }
$name = "Fury-$version-windows-x64$suffix"
$stage = Join-Path $OutputDirectory ($name + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Force -Path $stage | Out-Null
$lab = Join-Path $BuildDirectory 'apps\renderlab\Release'
$game = Join-Path $BuildDirectory 'apps\vaultline\Release'
foreach ($file in @('fury_renderlab.exe','SDL2.dll','amd_fidelityfx_loader_dx12.dll','amd_fidelityfx_upscaler_dx12.dll',
                    'libxess.dll','LICENSE-AMD-FSR.txt','LICENSE-Intel-XeSS.txt')) {
    $path = Join-Path $lab $file
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing package input: $path" }
    Copy-Item -LiteralPath $path -Destination $stage
}
Copy-Item -LiteralPath (Join-Path $game 'vaultline.exe') -Destination $stage
Copy-Item -LiteralPath (Join-Path $source 'assets') -Destination $stage -Recurse
Copy-Item -LiteralPath (Join-Path $source 'LICENSE') -Destination $stage
Copy-Item -LiteralPath (Join-Path $source 'docs\RENDERING.md') -Destination (Join-Path $stage 'RENDERING.md')
Copy-Item -LiteralPath (Join-Path $source 'engine\third_party\cgltf\LICENSE') -Destination (Join-Path $stage 'LICENSE-cgltf.txt')
Copy-Item -LiteralPath (Join-Path $source 'engine\third_party\stb\stb_image.h') -Destination (Join-Path $stage 'LICENSE-stb-image.h')
@'
Fury rendering development build

Requirements: Windows 10/11 x64, a DXR 1.1-capable GPU and current graphics driver,
and the Microsoft Visual C++ v14 x64 runtime at least as new as the build toolchain.
Official runtime downloads: https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist

Start a Coastal Lab launcher for the 1440p rendering fixture, or Play Vaultline for
the original game prototype. Native, FSR and XeSS launchers select real separate
reconstruction paths. In the lab: right mouse look, WASD/QE fly, Shift fast,
F1 lighting, F2 reconstruction, F3 quality, F4 debug, Space water, F11 fullscreen.
In Vaultline, O / Start opens settings.

See RENDERING.md for exact capabilities and unfinished work. This development
milestone is not a claim that the engine or its art meets the GTA 6 visual goal.
'@ | Set-Content -LiteralPath (Join-Path $stage 'START-HERE.txt') -Encoding utf8
$sdlConfig = Get-Content -LiteralPath (Join-Path $BuildDirectory 'CMakeCache.txt') | Select-String '^SDL2_DIR:[^=]*=(.*)$'
if ($sdlConfig) {
    $sdlLicense = Join-Path ([System.IO.Directory]::GetParent($sdlConfig.Matches[0].Groups[1].Value).FullName) 'COPYING.txt'
    if (Test-Path -LiteralPath $sdlLicense) { Copy-Item -LiteralPath $sdlLicense -Destination (Join-Path $stage 'LICENSE-SDL2.txt') }
}
if (-not (Test-Path -LiteralPath (Join-Path $stage 'LICENSE-SDL2.txt'))) { throw 'SDL license missing from package' }

$assetArguments = ''
if ($IncludeDetailedAssets) {
    & (Join-Path $PSScriptRoot 'fetch-render-assets.ps1') -Destination $AssetRoot
    $detail = Join-Path $stage 'assets\photoreal'
    New-Item -ItemType Directory -Force -Path $detail | Out-Null
    foreach ($asset in @('modular_wooden_pier','island_tree_01')) {
        Copy-Item -LiteralPath (Join-Path $AssetRoot $asset) -Destination $detail -Recurse
    }
    $assetArguments = ' --pier "%~dp0assets\photoreal\modular_wooden_pier\modular_wooden_pier_2k.gltf" --trees "%~dp0assets\photoreal\island_tree_01\island_tree_01_2k.gltf"'
}
@'
@echo off
setlocal
cd /d "%~dp0"
set "FURY_RENDERER=dx12"
vaultline.exe
if errorlevel 1 pause
'@ | Set-Content -LiteralPath (Join-Path $stage 'Play Vaultline.cmd') -Encoding ascii
foreach ($mode in @('native','fsr','xess')) {
    $launcher = "@echo off`r`nsetlocal`r`ncd /d `"%~dp0`"`r`nfury_renderlab.exe --width 2560 --height 1440 --upscaler $mode --quality quality --no-vsync$assetArguments`r`nif errorlevel 1 pause`r`n"
    $launcher | Set-Content -LiteralPath (Join-Path $stage "Coastal Lab - $mode.cmd") -Encoding ascii
}
$revision = (git -C $source rev-parse HEAD).Trim()
$dirty = [bool](git -C $source status --porcelain)
$files = Get-ChildItem -LiteralPath $stage -File -Recurse | ForEach-Object {
    [ordered]@{path=$_.FullName.Substring($stage.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
}
[ordered]@{version=$version;git_revision=$revision;source_dirty=$dirty;detailed_assets=[bool]$IncludeDetailedAssets;files=@($files)} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stage 'manifest.json') -Encoding utf8
$archive = Join-Path $OutputDirectory ($name + '.zip')
if (Test-Path -LiteralPath $archive) { throw "Archive already exists: $archive" }
Compress-Archive -LiteralPath $stage -DestinationPath $archive -CompressionLevel Optimal
Get-FileHash -LiteralPath $archive -Algorithm SHA256 | Format-List
Write-Host "Package directory: $stage"
Write-Host "Package archive: $archive"
