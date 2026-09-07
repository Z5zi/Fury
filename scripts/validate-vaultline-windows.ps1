[CmdletBinding()]
param([string]$BuildDirectory = (Join-Path $PSScriptRoot '..\build'))
$ErrorActionPreference = 'Stop'
$BuildDirectory = [System.IO.Path]::GetFullPath($BuildDirectory)
$exe = Join-Path $BuildDirectory 'apps\vaultline\Release\vaultline.exe'
$output = Join-Path $BuildDirectory 'vaultline-validation'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$cases = @(
    @{ Name='opengl'; Backend='opengl'; Upscaler='native'; Arguments='--smoke' },
    @{ Name='software-overrides-dx12'; Backend='dx12'; Upscaler='native'; Arguments='--soft --smoke' },
    @{ Name='dx12-native'; Backend='dx12'; Upscaler='native'; Arguments='--smoke' },
    @{ Name='dx12-fsr'; Backend='dx12'; Upscaler='fsr'; Arguments='--smoke' },
    @{ Name='dx12-xess'; Backend='dx12'; Upscaler='xess'; Arguments='--smoke' }
)
$saved = @{}
foreach ($name in @('FURY_RENDERER','FURY_UPSCALER','FURY_TRACE_MODE','FURY_DX12_DEBUG','FURY_SMOKE')) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
try {
    foreach ($case in $cases) {
        $env:FURY_RENDERER=$case.Backend
        $env:FURY_UPSCALER=$case.Upscaler
        $env:FURY_TRACE_MODE='path'
        $env:FURY_DX12_DEBUG='1'
        $env:FURY_SMOKE='1'
        $log=Join-Path $output ($case.Name+'.log')
        $process=Start-Process -FilePath $exe -ArgumentList $case.Arguments -WorkingDirectory (Split-Path -Parent $exe) `
            -WindowStyle Hidden -RedirectStandardOutput $log -RedirectStandardError (Join-Path $output ($case.Name+'.err')) -PassThru
        $started=$process.StartTime
        Write-Host "$($case.Name) started (PID $($process.Id))"
        if (-not $process.WaitForExit(30000)) {
            $owned=Get-CimInstance Win32_Process -Filter "ProcessId=$($process.Id)"
            if($owned -and $owned.ExecutablePath -eq $exe -and [Math]::Abs(($owned.CreationDate-$started).TotalSeconds)-lt 2) {
                Stop-Process -Id $process.Id -Force
                $process.WaitForExit()
            }
            throw "$($case.Name) exceeded its 30-second smoke limit"
        }
        if($process.ExitCode -ne 0) { throw "$($case.Name) failed: exit $($process.ExitCode)" }
        $text=Get-Content -Raw -LiteralPath $log
        if($text -match '\[ERROR\]') { throw "$($case.Name) logged an error" }
        if($case.Name -eq 'software-overrides-dx12') {
            if($text -notmatch 'Active renderer: Software') { throw 'Explicit software selection did not override DX12 environment' }
        } elseif($case.Backend -eq 'dx12') {
            if($text -notmatch 'Rendering validation: frames=\d+ provider=.* errors=0') { throw 'Missing successful DX12 runtime summary' }
        }
        Write-Host "$($case.Name) passed"
    }
} finally {
    foreach($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name,$saved[$name],'Process') }
}
Write-Host "Vaultline backend validation passed: $output"
