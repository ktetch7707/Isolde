$ErrorActionPreference='Stop'
. "$PSScriptRoot/guide-input-plan.ps1"
$folder=Join-Path $PSScriptRoot ('../build/guide-plan-contract-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $folder -Force | Out-Null
$path=Join-Path $folder 'plan.json'
function Check-Plan($document,$valid) {
    $document | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $path
    $accepted=$false
    try { $null=Import-GuideInputPlan -Path $path -Case guide-lifecycle; $accepted=$true } catch { if($valid){throw} }
    if($accepted -ne $valid){throw 'Input-plan boundary regression'}
}
Check-Plan @{schemaVersion=1;case='guide-lifecycle';phases=@{guidestartup=@('Left','Return');guidevideo=@()}} $true
Check-Plan @{schemaVersion=1;case='guide-lifecycle';phases=@{guidevideo=@('Home')}} $false
Check-Plan @{schemaVersion=1;case='guide-lifecycle';phases=@{guidevideo='Return'}} $false
Check-Plan @{schemaVersion=1;case='guide-lifecycle';phases=@{guidevideo=@('Return')};text='unsafe'} $false
Check-Plan @{schemaVersion=$true;case='guide-lifecycle';phases=@{guidevideo=@()}} $false
Check-Plan @{schemaVersion=1;case='guide-lifecycle';phases=@{guidevideo=@('Return')*21}} $false
$rejected=$false
try{$null=Import-GuideInputPlan -Path $path -Case apps}catch{$rejected=$true}
if(!$rejected){throw 'Native permissions must never replay'}
@{schemaVersion=1;case='guide-surfaces';phases=@{guidevideo=@('Escape')}} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $path
$null=Import-GuideInputPlan -Path $path -Case guide-surfaces
Write-Output "PASS: Guide replay bounds, keys, schema, empty keys and native-case refusal; evidence=$folder"
