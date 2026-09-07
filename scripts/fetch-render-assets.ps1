[CmdletBinding()]
param([string]$Destination = (Join-Path $PSScriptRoot '..\out\assets'))
$ErrorActionPreference = 'Stop'
$Destination = [System.IO.Path]::GetFullPath($Destination)
$lock = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot '..\assets\render-assets.lock.json') | ConvertFrom-Json
$headers = @{ 'User-Agent' = 'Fury-Asset-Preparation/1.0 (https://github.com/Z5zi/Fury)' }
Write-Host 'Powered by Poly Haven — CC0 assets: https://polyhaven.com'
foreach ($asset in $lock.assets) {
    foreach ($file in $asset.files) {
        $target = [System.IO.Path]::GetFullPath((Join-Path $Destination $file.path))
        if (-not $target.StartsWith($Destination.TrimEnd('\','/') + [System.IO.Path]::DirectorySeparatorChar,
                                   [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Asset path leaves destination: $($file.path)"
        }
        if (-not (Test-Path -LiteralPath $target)) {
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
            Write-Host "Downloading $($file.path)"
            Invoke-WebRequest -Uri $file.url -Headers $headers -OutFile $target
        }
        if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $file.sha256) {
            throw "Asset hash mismatch: $target"
        }
    }
}
Write-Host "Verified rendering assets: $Destination"
