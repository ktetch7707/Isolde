# Wrapper contract tests; no VM connection or application build.
$ErrorActionPreference = 'Stop'
$temp = Join-Path (Split-Path $PSScriptRoot -Parent) ('build/test-all-contract-' + [guid]::NewGuid().ToString('N'))
$tools = New-Item -ItemType Directory -Path "$temp/tools" -Force
Copy-Item -LiteralPath "$PSScriptRoot/test-all.ps1" -Destination $tools.FullName
@'
param($Action,$RunId,$Suite,$Cases)
$root = Join-Path (Split-Path $PSScriptRoot -Parent) "build/vm-oracle/$RunId"
New-Item -ItemType Directory -Force -Path "$root/report" | Out-Null
Add-Content -LiteralPath "$root/calls.log" -Value $Action
if ($Action -eq 'Test') {
    if ($Suite -ne 'all') { throw 'Expected full suite' }
    '<testsuite><testcase name="safe &amp; sound" time="1"><system-out>&lt;script&gt;unsafe&lt;/script&gt;</system-out></testcase></testsuite>' | Set-Content "$root/report/ctest.xml"
    if ($RunId -eq 'fail') { throw 'named build failure' }
}
'@ | Set-Content "$tools/vm-test.ps1"
& "$tools/test-all.ps1" -RunId pass -NoOpen
$root = "$temp/build/vm-oracle/pass"
if ((Get-Content "$root/calls.log") -join ',' -ne 'Test,Visual') { throw 'Must test once then visualize once' }
$html = Get-Content -Raw "$root/index.html"
if (!$html.Contains('safe &amp; sound') -or $html.Contains('<script>unsafe')) { throw 'Report must escape test output' }
& "$tools/test-all.ps1" -RunId pass -ReportOnly -NoOpen
if (@(Get-Content "$root/calls.log").Count -ne 2) { throw 'ReportOnly must not run tests' }
$caught = $false
try { & "$tools/test-all.ps1" -RunId fail -NoOpen } catch { $caught = $true }
if (!$caught) { throw 'Failure must propagate' }
if ((Get-Content "$temp/build/vm-oracle/fail/calls.log") -ne 'Test') { throw 'Must stop before visuals on failure' }
if (!(Get-Content -Raw "$temp/build/vm-oracle/fail/index.html").Contains('named build failure')) { throw 'Failure report missing diagnostics' }
Write-Host "PASS: test-all orchestration, report-only, escaping, first-failure stop; evidence=$temp"
