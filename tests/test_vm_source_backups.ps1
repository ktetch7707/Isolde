$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../tools/vm-process.ps1"
$fixture = Join-Path (Split-Path $PSScriptRoot -Parent) ("build/backup-contract-" + [guid]::NewGuid().ToString('N'))
$snapshot = Join-Path $fixture 'valid/snapshot'
New-Item -ItemType Directory -Force $snapshot | Out-Null
$archive = Join-Path $snapshot 'source.tar.gz'
[IO.File]::WriteAllText($archive, 'original archived bytes')
$hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $snapshot 'archive.sha256'), "$hash  source.tar.gz`n")
function Expect-Throws([scriptblock]$Action, [string]$Message) {
    $caught = $false
    try { & $Action | Out-Null } catch { if ($_.Exception.Message -notlike "*$Message*") { throw }; $caught = $true }
    if (!$caught) { throw "Expected refusal: $Message" }
}
$result = @(Get-OracleSourceBackups -Root $fixture)
if ($result.Count -ne 1 -or $result[0].sha256 -ne $hash -or $result[0].runId -ne 'valid') { throw 'Valid backup was not verified.' }
New-Item -ItemType Directory (Join-Path $fixture 'incomplete/snapshot') | Out-Null
if (@(Get-OracleSourceBackups -Root $fixture).Count -ne 1) { throw 'Incomplete snapshot must not authorize pruning.' }
Expect-Throws { Get-OracleSourceBackups -Root $fixture -RunIds @('../escape') } 'Unsafe backup run ID'
Expect-Throws { Get-OracleSourceBackups -Root $fixture -RunIds (@('valid') * 1025) } 'exceeds 1024'
[IO.File]::AppendAllText($archive, 'tampered')
Expect-Throws { Get-OracleSourceBackups -Root $fixture } 'checksum mismatch'
[IO.File]::WriteAllText($archive, 'original archived bytes')
if ($IsWindows) {
    New-Item -ItemType Directory (Join-Path $fixture 'redirected') | Out-Null
    New-Item -ItemType Junction -Path (Join-Path $fixture 'redirected/snapshot') -Target $snapshot | Out-Null
    Expect-Throws { Get-OracleSourceBackups -Root $fixture -RunIds @('redirected') } 'Redirected source-backup path'
    Expect-Throws { Get-OracleSourceBackups -Root (Join-Path $fixture 'redirected/snapshot') } 'Redirected source-backup ancestor'
}
if ([IO.File]::ReadAllText($archive) -ne 'original archived bytes') { throw 'Backup inventory changed host bytes.' }
Write-Output 'PASS source-backup contracts: valid, incomplete, traversal, bounds, corruption, redirected paths, read-only'
