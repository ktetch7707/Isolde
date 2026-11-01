# Accounting invariants only. No application build or VM.
$ErrorActionPreference='Stop'
$calculate=Join-Path $PSScriptRoot 'backlog-progress.ps1'
$current=(& $calculate -Json -CheckTable | ConvertFrom-Json)
if($current.Active -ne 27){throw 'Active task count mismatch'}
$folder=Join-Path $PSScriptRoot ('../build/progress-contract-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $folder -Force | Out-Null
$path=Join-Path $folder 'milestones.md'
$fixture=Join-Path $folder 'backlog.md'
$active=@('T01')+(3..5 | ForEach-Object {'T{0:00}' -f $_})+(9..31 | ForEach-Object {'T{0:00}' -f $_})
$lines=foreach($task in $active){
    if($task -eq 'T20'){1..4 | ForEach-Object {"- [x] T20.$_ Accepted fixture group"}; '- [ ] T20.5 Pending fixture group'}
    else{"- [ ] $task.1 Pending fixture group"}
}
$lines | Set-Content -LiteralPath $fixture -Encoding utf8
$ledger=(1..4 | ForEach-Object {"| T20.5.$([char](96+$_)) | open | Pending fixture milestone | Pending evidence |"}) -join "`n"
Set-Content -LiteralPath $path -Value $ledger -Encoding utf8
$result=(& $calculate -Json -BacklogPath $fixture -MilestonePath $path | ConvertFrom-Json)
if($result.Sum -ne 80 -or $result.Overall -ne 3){throw 'Fixture baseline or rounding mismatch'}
# Duplicating an equivalent open outcome subdivides the same parent weight,
# rather than increasing this parent's weight relative to other parents.
$extra='| T20.5.e | open | Additional unverified movement requirement | Pending evidence |'
Set-Content -LiteralPath $path -Value ($ledger+"`n"+$extra) -Encoding utf8
$same=(& $calculate -Json -BacklogPath $fixture -MilestonePath $path | ConvertFrom-Json)
if($same.Sum -ne $result.Sum){throw 'Splitting an entirely open parent created credit'}
# Finishing one of five children earns one fifth of one fifth of T20: 4 points.
$extra='| T20.5.e | done | Fixture-only accounting proof | docs/progress-milestones.md |'
Set-Content -LiteralPath $path -Value ($ledger+"`n"+$extra) -Encoding utf8
$changed=(& $calculate -Json -BacklogPath $fixture -MilestonePath $path | ConvertFrom-Json)
if(($changed.Tasks | Where-Object Task -eq T20).Percent -ne 84){throw 'Original parent weight changed'}
Set-Content -LiteralPath $path -Value ($ledger+"`n"+$extra+"`n"+$extra) -Encoding utf8
$rejected=$false
try{ & $calculate -Json -BacklogPath $fixture -MilestonePath $path | Out-Null }catch{ $rejected=$_.Exception.Message -like 'Duplicate milestone*' }
if(!$rejected){throw 'Duplicate milestones must fail'}
Set-Content -LiteralPath $path -Value ($ledger+"`n| T99.1.a | open | Unknown group | Pending evidence |") -Encoding utf8
$rejected=$false
try{ & $calculate -Json -BacklogPath $fixture -MilestonePath $path | Out-Null }catch{ $rejected=$_.Exception.Message -like 'Unknown Scope parent*' }
if(!$rejected){throw 'Unknown parents must fail'}
Write-Output "PASS: table reconciliation, rounding, fixed parent weights, no split credit, duplicate/unknown rejection; evidence=$folder"
