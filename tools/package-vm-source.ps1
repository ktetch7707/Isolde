param(
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
# Run from the repository root. Package tracked working-tree bytes, including
# uncommitted edits, without collecting scratch files, credentials or .git.
$root = (& git rev-parse --show-toplevel)
if ($LASTEXITCODE -ne 0) { throw 'Run inside the Isolde repository.' }
if ((Get-Location).Path -ne (Resolve-Path -LiteralPath $root).Path) { throw 'Run from the repository root.' }
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Choose a new output directory; snapshots must not overwrite earlier evidence.' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$outputPath = (Resolve-Path -LiteralPath $OutputDirectory).Path
$utf8 = [Text.UTF8Encoding]::new($false)
$files = @(& git -c core.quotePath=false ls-files)
if ($LASTEXITCODE -ne 0) { throw 'Could not enumerate tracked files.' }
foreach ($file in $files) {
    if ($file -match '[\r\n]' -or $file.StartsWith('-') -or !(Test-Path -LiteralPath $file -PathType Leaf)) {
        throw "Cannot package tracked path: $file. Resolve deletions/path changes explicitly first."
    }
}
[IO.File]::WriteAllLines((Join-Path $outputPath 'tracked-files.txt'), $files, $utf8)
$head = & git rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot identify HEAD.' }
[IO.File]::WriteAllText((Join-Path $outputPath 'head.txt'), "$head`n", $utf8)
$status = @(& git status --short)
if ($LASTEXITCODE -ne 0) { throw 'Cannot record status.' }
[IO.File]::WriteAllLines((Join-Path $outputPath 'status.txt'), $status, $utf8)
$diffPath = Join-Path $outputPath 'working-tree.patch'
& git diff HEAD --binary "--output=$diffPath" 2> (Join-Path $outputPath 'git-diff.stderr.log')
if ($LASTEXITCODE -ne 0) { throw 'Cannot record working-tree diff.' }
$manifest = foreach ($file in $files) { "$((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant())  $file" }
[IO.File]::WriteAllLines((Join-Path $outputPath 'files.sha256'), $manifest, $utf8)
& tar.exe -czf (Join-Path $outputPath 'source.tar.gz') -T (Join-Path $outputPath 'tracked-files.txt')
if ($LASTEXITCODE -ne 0) { throw 'Source archive failed.' }
# Detect changes while archiving. Guest sha256sum -c checks actual extracted bytes.
foreach ($line in $manifest) {
    $file = $line.Substring(66)
    if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $line.Substring(0,64)) {
        throw "Source changed during packaging: $file. Use a new snapshot directory."
    }
}
$hash = (Get-FileHash -LiteralPath (Join-Path $outputPath 'source.tar.gz') -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $outputPath 'archive.sha256'), "$hash  source.tar.gz`n", $utf8)
Write-Output "Snapshot: $outputPath"
Write-Output "HEAD: $head; tracked files: $($files.Count); SHA256: $hash"
Write-Output 'Review status.txt: untracked files are excluded. Track required new source/assets before packaging.'
