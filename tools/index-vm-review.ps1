# Reuse reviewed identical pixels; never turn that into semantic acceptance.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$')][string]$RunId,
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$')][string]$ReviewBaseRunId
)
$ErrorActionPreference='Stop'
$workspace=Split-Path $PSScriptRoot -Parent
$root=Join-Path $workspace "build/vm-oracle/$RunId"
if (!(Test-Path -LiteralPath $root -PathType Container)) { throw 'Missing VM evidence directory.' }
$known=@{}
if ($ReviewBaseRunId) {
    $ledger=Join-Path $workspace "build/vm-oracle/$ReviewBaseRunId/review/agent-visual-review.json"
    if (!(Test-Path -LiteralPath $ledger -PathType Leaf)) { throw 'Missing explicit agent review ledger.' }
    $previous=Get-Content -LiteralPath $ledger -Raw | ConvertFrom-Json
    if ($previous.entries.Count -gt 4096) { throw 'Unbounded previous visual ledger.' }
    foreach ($entry in $previous.entries) {
        if ($entry.sha256 -notmatch '^[a-f0-9]{64}$' -or !$entry.review.StartsWith('reviewed')) { continue }
        $known[$entry.sha256]=@{run=$ReviewBaseRunId;path=$entry.path;disposition=$entry.review}
    }
}
$files=@()
foreach ($part in @('report','visual')) {
    $directory=Join-Path $root $part
    if (Test-Path -LiteralPath $directory) { $files+=@(Get-ChildItem -LiteralPath $directory -Recurse -File -Filter '*.png') }
}
if ($files.Count -gt 4096) { throw 'Unbounded capture inventory.' }
$entries=@();$newImages=@();$reused=0
foreach ($file in $files) {
    if ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Redirected capture refused.' }
    $relative=[IO.Path]::GetRelativePath($root,$file.FullName).Replace('\','/')
    $sha=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $prior=$known[$sha]
    $entry=@{path=$relative;sha256=$sha;pixelReview=if($prior){'reused-identical'}else{'required'};semanticAcceptance='pending'}
    if ($prior) { $entry.previousReview=$prior;$reused++ }
    else { $newImages+=$relative }
    $entries+=$entry
}
$review=Join-Path $root 'review';New-Item -ItemType Directory -Force -Path $review | Out-Null
@{run=$RunId;reviewBase=$ReviewBaseRunId;reusedPixels=$reused;newPixels=$newImages.Count;entries=$entries} |
    ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $review 'capture-index.json') -Encoding utf8NoBOM
$figures=@($newImages | ForEach-Object {
    $safe=[Net.WebUtility]::HtmlEncode($_)
    "<figure><figcaption>$safe</figcaption><a href='../$safe'><img loading='lazy' src='../$safe'></a></figure>"
})
@"
<!doctype html><meta charset='utf-8'><title>Changed VM captures — $RunId</title>
<style>body{background:#17171c;color:#eee;font:16px system-ui;margin:24px}img{max-width:100%;height:auto}figure{margin:24px 0}figcaption{padding:8px}</style>
<h1>$RunId — changed captures</h1><p>$($newImages.Count) images need pixel review; $reused reuse identical reviewed pixels. Semantic acceptance remains separate. Previous exclusions remain exclusions.</p>
$($figures -join "`n")
"@ | Set-Content -LiteralPath (Join-Path $review 'changed-captures.html') -Encoding utf8NoBOM
Write-Host "VISUAL_INDEX new=$($newImages.Count) reused=$reused total=$($entries.Count)"
