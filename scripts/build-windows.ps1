[CmdletBinding()]
param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\build'),
    [string]$DependencyRoot = (Join-Path $PSScriptRoot '..\out\deps'),
    [ValidateSet('Release','Debug','RelWithDebInfo')][string]$Configuration = 'Release',
    [int]$Workers = 12
)
$ErrorActionPreference = 'Stop'
$source = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$BuildDirectory = [System.IO.Path]::GetFullPath($BuildDirectory)
$DependencyRoot = [System.IO.Path]::GetFullPath($DependencyRoot)
& (Join-Path $PSScriptRoot 'bootstrap-windows.ps1') -DependencyRoot $DependencyRoot
$deps = Get-Content -Raw -LiteralPath (Join-Path $DependencyRoot 'dependency-paths.json') | ConvertFrom-Json
cmake -S $source -B $BuildDirectory -A x64 `
    "-DSDL2_DIR=$($deps.SDL2)/cmake" -DFURY_ENABLE_DX12=ON `
    "-DFURY_DXC_ROOT=$($deps.DXC)" "-DFURY_FSR_ROOT=$($deps.FSR)" "-DFURY_XESS_ROOT=$($deps.XeSS)"
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
cmake --build $BuildDirectory --config $Configuration --parallel $Workers
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
ctest --test-dir $BuildDirectory -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'CTest failed' }
Write-Host "Build and unit tests passed: $BuildDirectory"
