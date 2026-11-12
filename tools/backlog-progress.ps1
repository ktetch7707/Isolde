# Read-only progress accounting. No build, VM access or automatic evidence mutation.
[CmdletBinding()]
param([switch]$Json, [switch]$CheckTable,
    [string]$BacklogPath = (Join-Path $PSScriptRoot '../docs/agent-backlog.md'),
    [string]$MilestonePath = (Join-Path $PSScriptRoot '../docs/progress-milestones.md'))
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$backlog = Get-Content -Raw -LiteralPath $BacklogPath
$ledger = Get-Content -Raw -LiteralPath $MilestonePath
$excluded = @('T13', 'T31')
$active = @('T01') + @(3..5 | ForEach-Object { 'T{0:00}' -f $_ }) + @(9..30 | ForEach-Object { 'T{0:00}' -f $_ })
$active = @($active | Where-Object { $_ -notin $excluded })
$parents = @{}; $children = @{}; $seen = @{}
foreach ($task in $active) {
    foreach ($m in [regex]::Matches($backlog, '(?m)^- \[([ x])\] (' + $task + '\.\d+) (.+)\r?$')) {
        $id = $m.Groups[2].Value
        if ($parents.ContainsKey($id)) { throw "Duplicate Scope ID $id" }
        $parents[$id] = @{ task=$task; accepted=($m.Groups[1].Value -eq 'x') }
    }
}
foreach ($m in [regex]::Matches($ledger, '(?m)^\| (T\d{2}\.\d+\.[a-z]+) \| (done|open) \| ([^\r\n]+) \| ([^\r\n]+) \|\r?$')) {
    $id = $m.Groups[1].Value; $parent = $id.Substring(0,$id.LastIndexOf('.'))
    if ($parent.Substring(0,3) -in $excluded) { continue }
    if ($seen.ContainsKey($id)) { throw "Duplicate milestone $id" }; $seen[$id]=$true
    if (!$parents.ContainsKey($parent)) { throw "Unknown Scope parent $parent" }
    if (!$children.ContainsKey($parent)) { $children[$parent]=[Collections.Generic.List[object]]::new() }
    $children[$parent].Add(@{ id=$id; done=($m.Groups[2].Value -eq 'done') })
    if ($m.Groups[2].Value -eq 'done' -and $m.Groups[4].Value -notmatch 'J-native|J-runtime|C-runtime|Q-inventory|\.(log|json|md)|/tmp/') { throw "Missing evidence reference for $id" }
}
if (!$seen.Count) { throw 'No milestone rows found' }
$weights=@{}
foreach ($m in [regex]::Matches($ledger,'(?m)^<!-- milestone-weights (T\d{2}\.\d+): ([a-z0-9=.,]+) -->\r?$')) {
    $parent=$m.Groups[1].Value
    if (!$children.ContainsKey($parent) -or $weights.ContainsKey($parent)) { throw "Invalid weighted group $parent" }
    $group=@{};[decimal]$total=0
    foreach ($part in $m.Groups[2].Value.Split(',')) {
        $pair=$part.Split('=');$id=$parent+'.'+$pair[0]
        if ($pair.Count -ne 2 -or !$seen.ContainsKey($id) -or $group.ContainsKey($id)) { throw "Invalid milestone weight $part" }
        $value=[decimal]::Parse($pair[1],[Globalization.CultureInfo]::InvariantCulture)
        if ($value -le 0 -or $value -gt 1) { throw "Invalid weight for $id" }
        $group[$id]=$value;$total+=$value
    }
    if ($total -ne 1 -or $group.Count -ne $children[$parent].Count) { throw "Weights must cover exactly one original group: $parent" }
    $weights[$parent]=$group
}
$results = foreach ($task in $active) {
    $ids = @($parents.Keys | Where-Object { $parents[$_].task -eq $task })
    if (!$ids.Count) { throw "Missing active task $task" }
    [decimal]$earned=0; $accepted=0
    foreach ($id in $ids) {
        if ($children.ContainsKey($id)) {
            $done = @($children[$id] | Where-Object { $_.done }).Count
            if ($weights.ContainsKey($id)) {
                foreach ($child in $children[$id]) { if ($child.done) { $earned += $weights[$id][$child.id] } }
            } else { $earned += [decimal]$done / $children[$id].Count }
            if ($parents[$id].accepted -and $done -ne $children[$id].Count) { throw "Parent $id checked despite incomplete milestones" }
        } elseif ($parents[$id].accepted) { $earned++ }
        if ($parents[$id].accepted) { $accepted++ }
    }
    $percent = [int][math]::Round(100*$earned/$ids.Count,0,[MidpointRounding]::AwayFromZero)
    if ($accepted -ne $ids.Count) { $percent=[math]::Min(99,$percent) }
    [pscustomobject]@{ Task=$task; Percent=$percent; AcceptedGroups=$accepted; Groups=$ids.Count }
}
$sum = ($results | Measure-Object -Property Percent -Sum).Sum
$overall = [int][math]::Round($sum/$active.Count,0,[MidpointRounding]::AwayFromZero)
if (@($results | Where-Object { $_.AcceptedGroups -ne $_.Groups }).Count) { $overall=[math]::Min(99,$overall) }
$output = [pscustomobject]@{ Tasks=@($results); Sum=$sum; Overall=$overall; Active=$active.Count; Milestones=$seen.Count; DoneMilestones=@($children.Values | ForEach-Object { $_ } | Where-Object { $_.done }).Count }
if ($CheckTable) {
    foreach ($result in $results) {
        $row = [regex]::Match($backlog,'(?m)^\| '+$result.Task+' \|[^\r\n]*?\| (\d+)% \|')
        if (!$row.Success -or [int]$row.Groups[1].Value -ne $result.Percent) { throw "Backlog table mismatch for $($result.Task): expected $($result.Percent)%" }
    }
    $row=[regex]::Match($backlog,'(?m)^\| \*\*Overall\*\* \|[^\r\n]*?\*\*(\d+)%\*\*')
    if (!$row.Success -or [int]$row.Groups[1].Value -ne $overall) { throw "Overall table mismatch: expected $overall%" }
}
if ($Json) { $output | ConvertTo-Json -Depth 5 } else { $results | Format-Table; Write-Output "Sum=$sum; Overall=$overall%; Active=$($active.Count); milestones=$($output.DoneMilestones)/$($output.Milestones)" }
