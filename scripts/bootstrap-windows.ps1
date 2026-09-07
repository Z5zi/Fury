[CmdletBinding()]
param([string]$DependencyRoot = (Join-Path $PSScriptRoot '..\out\deps'))
$ErrorActionPreference = 'Stop'
$DependencyRoot = [System.IO.Path]::GetFullPath($DependencyRoot)
New-Item -ItemType Directory -Force -Path $DependencyRoot | Out-Null

function Install-VerifiedZip {
    param([string]$Url, [string]$ArchiveName, [string]$Hash, [string]$Destination, [string]$Marker)
    $archive = Join-Path $DependencyRoot $ArchiveName
    if (-not (Test-Path -LiteralPath $archive)) {
        Write-Host "Downloading $ArchiveName"
        Invoke-WebRequest -Uri $Url -OutFile $archive
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $Hash) {
        throw "SHA-256 mismatch: $archive"
    }
    if (-not (Test-Path -LiteralPath $Marker)) {
        Expand-Archive -LiteralPath $archive -DestinationPath $Destination -Force
    }
    if (-not (Test-Path -LiteralPath $Marker)) { throw "SDK extraction did not produce $Marker" }
}

$sdl = Join-Path $DependencyRoot 'SDL2-2.30.9'
$dxc = Join-Path $DependencyRoot 'dxc'
$xess = Join-Path $DependencyRoot 'xess'
$fsr = Join-Path $DependencyRoot 'fsr'
Install-VerifiedZip 'https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-devel-2.30.9-VC.zip' `
    'SDL2-devel-2.30.9-VC.zip' '8C91D91E5BCB997D062EC2B553C53832EBF95654D4AA35E8C02A954D4CE752AE' `
    $DependencyRoot (Join-Path $sdl 'cmake\sdl2-config.cmake')
Install-VerifiedZip 'https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2607/dxc_2026_07_29.zip' `
    'dxc.zip' 'A1DFB116BA3EEAE6A1582291B53A8E7BF65AD760676BD3194685C8F7367CD241' `
    $dxc (Join-Path $dxc 'bin\x64\dxc.exe')
Install-VerifiedZip 'https://github.com/intel/xess/releases/download/v3.0.2/XeSS_SDK_3.0.2.zip' `
    'xess.zip' '88B8A373F30E33F3558A77A93E634F11B8132FC3047EA1A8EDEEAD32B8471990' `
    $xess (Join-Path $xess 'inc\xess\xess_d3d12.h')

$fsrCommit = '60f4ea81909200d8542eca14dccb2628b763a9a3'
if (-not (Test-Path -LiteralPath $fsr)) {
    git clone --depth 1 --branch v2.3.0 --filter=blob:none --sparse `
        https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK.git $fsr
    if ($LASTEXITCODE -ne 0) { throw 'FSR SDK clone failed' }
}
$actualCommit = (git -C $fsr rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actualCommit -ne $fsrCommit) { throw 'Unexpected FSR SDK commit' }
if (git -C $fsr status --porcelain) { throw 'FSR SDK has local modifications; use a clean dependency directory' }
git -C $fsr sparse-checkout set Kits/FidelityFX/api Kits/FidelityFX/upscalers/include Kits/FidelityFX/signedbin docs
if ($LASTEXITCODE -ne 0) { throw 'FSR SDK checkout failed' }

$runtimeHashes = @{
    (Join-Path $fsr 'Kits\FidelityFX\signedbin\amd_fidelityfx_loader_dx12.dll') = 'E2D85AA05A9BD9ED8B38935FDF5199372CCA6F74C12015143BB6F945EE1608AA'
    (Join-Path $fsr 'Kits\FidelityFX\signedbin\amd_fidelityfx_upscaler_dx12.dll') = 'D0DCCCC74A43C44BA435B7A369B456E0970D8A4464E4BD683119B374F2C9FB46'
    (Join-Path $xess 'bin\libxess.dll') = '251659DD84A3E84DE67C886A4186E01F3ECA49B00641906FE38BB6B807E5D5B7'
}
foreach ($runtime in $runtimeHashes.GetEnumerator()) {
    if ((Get-FileHash -LiteralPath $runtime.Key -Algorithm SHA256).Hash -ne $runtime.Value) {
        throw "Runtime integrity check failed: $($runtime.Key)"
    }
}
$paths = [ordered]@{ SDL2 = $sdl; DXC = $dxc; FSR = $fsr; XeSS = $xess; FSRCommit = $fsrCommit }
$paths | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $DependencyRoot 'dependency-paths.json') -Encoding utf8
Write-Host "Verified SDKs: $DependencyRoot"
Write-Host 'SDL 2.30.9; DXC 1.9.2607; AMD FSR SDK 2.3.0; Intel XeSS SDK 3.0.2'
