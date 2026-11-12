# One invocation, one frozen build, one report. VM operations stay in vm-test.ps1.
[CmdletBinding()]
param(
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$')][string]$RunId = ((Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,6)),
    [ValidateSet('studio','studio-designs','studio-search','themes','themecustom','themefaults','themecontrols','sources','desktop','collections','catalog','history','lounge','pastel','pages','guide','guide-surfaces','guide-lifecycle','guide-features','movieart','repairs','apps','clients','vpn','shell','playlists','android','androidapps','store','games','emulators')]
    [string[]]$Cases = @('studio','lounge','pastel'),
    [switch]$NativeOnly,
    [ValidateSet('all','studio_cutover','studio_closure_regressions','sources','kodi_media_client','shell_appearance','campaign_repairs')][string]$Suite='all',
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$')][string]$ReviewBaseRunId,
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$')][string]$ResumeRunId,
    [switch]$ReportOnly,
    [switch]$NoOpen
)
$ErrorActionPreference = 'Stop'
if ($PSVersionTable.PSVersion.Major -lt 7) { throw 'Use PowerShell 7 (pwsh).' }
if ($Suite -ne 'all' -and !$NativeOnly -and !$ReportOnly) { throw 'Focused batches require -NativeOnly; installed visual acceptance uses a passing full candidate.' }
$root = Join-Path (Split-Path $PSScriptRoot -Parent) "build/vm-oracle/$RunId"
if ($ReportOnly -and !(Test-Path -LiteralPath $root)) { throw "No evidence for $RunId" }
if (!$ReportOnly -and (Test-Path -LiteralPath $root)) { throw 'Run exists; use -ReportOnly to view it without rerunning.' }
$failure = $null
try {
    if (!$ReportOnly) {
        $testArguments = @{ RunId=$RunId; Suite=$Suite }
        if ($ResumeRunId) { $testArguments.ResumeRunId=$ResumeRunId }
        & "$PSScriptRoot/vm-test.ps1" Test @testArguments
        if (!$NativeOnly) { & "$PSScriptRoot/vm-test.ps1" Visual -RunId $RunId -Cases $Cases }
    }
} catch { $failure = $_.Exception.Message }
finally {
    New-Item -ItemType Directory -Force -Path $root | Out-Null
    function Escape([object]$value) { [Net.WebUtility]::HtmlEncode([string]$value) }
    function Relative([string]$path) { [IO.Path]::GetRelativePath($root, $path).Replace('\','/') }
    $rows = [Collections.Generic.List[string]]::new()
    $passed = 0; $failed = 0; $skipped = 0
    $xmlPath = Join-Path $root 'report/ctest.xml'
    if (Test-Path -LiteralPath $xmlPath) {
        try {
            # Do not resolve entities from result files.
            $settings = [Xml.XmlReaderSettings]::new(); $settings.DtdProcessing = 'Prohibit'
            $reader = [Xml.XmlReader]::Create($xmlPath, $settings)
            try { $xml = [Xml.XmlDocument]::new(); $xml.Load($reader) } finally { $reader.Dispose() }
            foreach ($test in $xml.SelectNodes('//testcase')) {
                $state = if ($test.SelectSingleNode('failure|error')) { $failed++; 'FAIL' } elseif ($test.SelectSingleNode('skipped') -or $test.GetAttribute('status') -in @('notrun','disabled')) { $skipped++; 'SKIP' } else { $passed++; 'PASS' }
                $detail = $test.SelectSingleNode('failure|error|skipped|system-out')
                $rows.Add("<tr><td>$state</td><td>$(Escape $test.GetAttribute('name'))</td><td>$(Escape $test.GetAttribute('time'))</td><td><details><summary>Output</summary><pre>$(Escape $detail.InnerText)</pre></details></td></tr>")
            }
        } catch { $failure = "Cannot read CTest results: $($_.Exception.Message)" }
    }
    $hostFailure = Join-Path $root 'host-failure.json'
    if (!$failure -and (Test-Path -LiteralPath $hostFailure)) { $failure = (Get-Content -Raw -LiteralPath $hostFailure | ConvertFrom-Json).error }
    $state = if ($failure -or $failed -or $skipped -or !$passed) { 'INCOMPLETE / FAILED' } else { 'CTest PASS — visual review is separate' }
    $links = @(Get-ChildItem -LiteralPath $root -Recurse -File | Where-Object { $_.Extension -in @('.log','.json','.xml') } | ForEach-Object {
        $relative = Relative $_.FullName
        "<li><a href='$(Escape $relative)'>$(Escape $relative)</a></li>"
    })
    $report = Join-Path $root 'index.html'
    @"
<!doctype html><html lang="en"><meta charset="utf-8"><title>Isolde test results — $(Escape $RunId)</title>
<style>body{font:16px system-ui;margin:2rem;max-width:1200px;background:#101824;color:#edf2fa}a{color:#8dccff}table{border-collapse:collapse;width:100%}td,th{padding:.6rem;text-align:left;border-bottom:1px solid #526070}pre{white-space:pre-wrap;overflow-wrap:anywhere}img{max-width:100%}figure{margin:1rem 0}h1{font-size:1.8rem}</style>
<h1>$(Escape $state)</h1><p>Run: $(Escape $RunId) · Passed: $passed · Failed: $failed · Skipped: $skipped</p>
<p>$(Escape $failure)</p><p>Requested suite: $(Escape $Suite). Unattempted tests are not passes. Selected graphical cases run afterward against the same frozen payload; execution stops at the first named failure. Screenshots require agent review. Required software acceptance follows docs/vm-software-completion.md, including the supported offline journey. Hardware and external accounts are post-completion handoff checks; the twelve-hour Guide soak is optional.</p>
<h2>CTest results</h2><table><tr><th>Result</th><th>Test</th><th>Seconds</th><th>Details</th></tr>$($rows -join "`n")</table>
<h2>Evidence and failure logs</h2><ul>$($links -join "`n")</ul>
<h2>Visual review</h2><p><a href='review/changed-captures.html'>Review changed captures</a> · <a href='review/capture-index.json'>Exact-hash reuse and exclusion receipts</a>. Unchanged reviewed pixels are reused automatically; semantic acceptance remains separate.</p>
</html>
"@ | Set-Content -LiteralPath $report -Encoding utf8
    Write-Host "RESULT=$state; PASS=$passed FAIL=$failed SKIP=$skipped"
    Write-Host "REPORT=$report"
    $reviewArguments=@{RunId=$RunId}
    if ($ReviewBaseRunId) { $reviewArguments.ReviewBaseRunId=$ReviewBaseRunId }
    & "$PSScriptRoot/index-vm-review.ps1" @reviewArguments
    if (!$NoOpen) { Start-Process -FilePath $report }
}
if ($failure -or $failed -or $skipped -or !$passed) { throw "Test run incomplete or failed. See $report" }
