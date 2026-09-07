[CmdletBinding()]
param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\build'),
    [string]$AssetRoot = (Join-Path $PSScriptRoot '..\out\assets'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\out\render-validation'),
    [int]$Frames = 360,
    [int]$Width = 1920,
    [int]$Height = 1080,
    [int]$SamplesPerPixel = 2,
    [switch]$BenchmarkOnly,
    [switch]$DetailedScene,
    [switch]$WithoutDebugLayer
)
$ErrorActionPreference = 'Stop'
$BuildDirectory = [System.IO.Path]::GetFullPath($BuildDirectory)
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
$AssetRoot = [System.IO.Path]::GetFullPath($AssetRoot)
$executable = Join-Path $BuildDirectory 'apps\renderlab\Release\fury_renderlab.exe'
if (-not (Test-Path -LiteralPath $executable)) { throw "Missing build: $executable" }
if ($Frames -lt 60) { throw 'Use at least 60 frames for validation' }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$summaries = [System.Collections.Generic.List[object]]::new()
$expectedSource = & (Join-Path $PSScriptRoot 'source-fingerprint.ps1') -BuildDirectory $BuildDirectory

function Invoke-RenderCase {
    param([string]$Name, [string[]]$Options, [int]$FrameCount = $Frames)
    $report = Join-Path $OutputDirectory ($Name + '.json')
    $capture = Join-Path $OutputDirectory ($Name + '.ppm')
    $arguments = @('--hidden','--no-vsync','--frames',"$FrameCount",'--warmup','32','--report',$report,'--capture',$capture) + $Options
    if (-not $WithoutDebugLayer) { $arguments += '--debug' }
    # Paths may contain spaces; none of these arguments may contain a quote.
    foreach ($argument in $arguments) { if ($argument.Contains('"')) { throw 'Unexpected quote in renderer argument' } }
    $quoted = ($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $process = Start-Process -FilePath $executable -ArgumentList $quoted -WorkingDirectory (Split-Path -Parent $executable) `
        -WindowStyle Hidden -RedirectStandardOutput (Join-Path $OutputDirectory ($Name + '.log')) `
        -RedirectStandardError (Join-Path $OutputDirectory ($Name + '.err')) -PassThru
    $started = $process.StartTime
    Write-Host "$Name started (PID $($process.Id))"
    $deadline = [DateTime]::UtcNow.AddSeconds(120)
    while (-not $process.WaitForExit(5000)) {
        if ([DateTime]::UtcNow -gt $deadline) {
            $owned = Get-CimInstance Win32_Process -Filter "ProcessId=$($process.Id)"
            if ($owned -and $owned.ExecutablePath -eq $executable -and
                [Math]::Abs(($owned.CreationDate - $started).TotalSeconds) -lt 2) {
                Stop-Process -Id $process.Id -Force
                $process.WaitForExit()
            }
            throw "$Name timed out; inspected only its owned PID $($process.Id)"
        }
    }
    if ($process.ExitCode -ne 0) {
        Get-Content -LiteralPath (Join-Path $OutputDirectory ($Name + '.err')) -Tail 15 | Write-Host
        throw "$Name failed with exit code $($process.ExitCode)"
    }
    $result = Get-Content -Raw -LiteralPath $report | ConvertFrom-Json
    if ($result.source_sha256 -ne $expectedSource) { throw "$Name used a stale executable; rebuild before validation" }
    if ($result.frames -ne $FrameCount -or $result.validation_errors -ne 0 -or -not $result.hardware_ray_tracing) {
        throw "$Name did not meet the frame count / GPU validation gate"
    }
    if (-not $WithoutDebugLayer -and -not $result.debug_layer_active) { throw "$Name did not enable the debug layer" }
    if ((Get-Item -LiteralPath $capture).Length -lt $result.output_width * $result.output_height * 3) { throw "$Name capture is incomplete" }
    $summaries.Add([ordered]@{case=$Name;result=$result;capture_sha256=(Get-FileHash -LiteralPath $capture -Algorithm SHA256).Hash.ToLowerInvariant()})
    Write-Host "$Name passed: GPU median $($result.gpu_ms.median) ms, p99 $($result.gpu_ms.p99) ms"
}

$scene = @()
if ($DetailedScene) {
    $pier = Join-Path $AssetRoot 'modular_wooden_pier\modular_wooden_pier_2k.gltf'
    $trees = Join-Path $AssetRoot 'island_tree_01\island_tree_01_2k.gltf'
    if (-not (Test-Path -LiteralPath $pier) -or -not (Test-Path -LiteralPath $trees)) { throw 'Run fetch-render-assets.ps1 first' }
    $scene = @('--pier',$pier,'--trees',$trees)
}
foreach ($upscaler in @('native','fsr','xess')) {
    Invoke-RenderCase ($upscaler+'-still') (@('--width',"$Width",'--height',"$Height",'--spp',"$SamplesPerPixel",'--upscaler',$upscaler) + $scene)
    if ($BenchmarkOnly) {
        Invoke-RenderCase ($upscaler+'-flight') (@('--width',"$Width",'--height',"$Height",'--spp',"$SamplesPerPixel",'--upscaler',$upscaler,
            '--animate','--camera-motion') + $scene)
    } else {
        Invoke-RenderCase ($upscaler+'-motion-resize') @('--width','1280','--height','720','--upscaler',$upscaler,
            '--animate','--camera-motion','--camera-cut','--resize-test','--deform-test') 180
    }
}
if (-not $BenchmarkOnly) {
Invoke-RenderCase 'runtime-upscaler-switch' @('--width','960','--height','540','--upscaler-cycle-test') 180
Invoke-RenderCase 'ray-traced' (@('--width','1920','--height','1080','--mode','ray','--upscaler','fsr') + $scene)
Invoke-RenderCase 'motion-buffer-static' @('--width','640','--height','360','--debug-view','motion') 64
Invoke-RenderCase 'motion-buffer-moving' @('--width','640','--height','360','--debug-view','motion','--camera-motion') 64
Invoke-RenderCase 'depth-buffer' @('--width','640','--height','360','--debug-view','depth') 64
Invoke-RenderCase 'normal-buffer' @('--width','640','--height','360','--debug-view','normals') 64
Invoke-RenderCase 'alpha-visibility-change' @('--width','640','--height','360','--alpha-test') 64
}

$summary = [ordered]@{
    schema = 1
    timestamp_utc = [DateTime]::UtcNow.ToString('o')
    executable_sha256 = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant()
    detailed_scene = [bool]$DetailedScene
    frame_budget_ms = 8.3333333333
    benchmark_only = [bool]$BenchmarkOnly
    cases = $summaries.ToArray()
}
$finalSource = & (Join-Path $PSScriptRoot 'source-fingerprint.ps1') -BuildDirectory $BuildDirectory
if ($finalSource -ne $expectedSource) { throw 'Source changed during rendering validation; results are not for the current source' }
$summary | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'summary.json') -Encoding utf8
if (-not $BenchmarkOnly) {
    python (Join-Path $PSScriptRoot 'verify-render-output.py') $OutputDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Rendered pixel / geometry counter verification failed' }
}
Write-Host "Rendering validation passed: $OutputDirectory"
