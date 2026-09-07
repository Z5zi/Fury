param([Parameter(Mandatory=$true)][string]$BuildDirectory)
$ErrorActionPreference='Stop'
$root=[System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$list=Join-Path $BuildDirectory 'generated\source-files.txt'
if(-not (Test-Path -LiteralPath $list)) { throw 'Build source list missing; reconfigure and build Fury' }
$payload=[System.Text.StringBuilder]::new()
foreach($relative in Get-Content -LiteralPath $list) {
    if(-not $relative) { continue }
    $file=Join-Path $root $relative
    $hash=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
    [void]$payload.Append($relative).Append(':').Append($hash).Append("`n")
}
$sha=[System.Security.Cryptography.SHA256]::Create()
try {
    $bytes=[System.Text.Encoding]::UTF8.GetBytes($payload.ToString())
    [System.BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-','').ToLowerInvariant()
} finally { $sha.Dispose() }
