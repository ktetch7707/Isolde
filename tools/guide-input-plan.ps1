# Declarative fixture keys only; no text, pointer, shell or native-app permission replay.
function Import-GuideInputPlan {
    param([string]$Path, [string]$Case)
    if ($Case -notin @('guide-lifecycle','guide-reopen','guide-features')) { throw 'Fixture input replay is restricted to Guide acceptance cases.' }
    $file = Get-Item -LiteralPath $Path
    if ($file.Length -gt 65536) { throw 'Oversized Guide input plan.' }
    $plan = Get-Content -Raw -LiteralPath $file.FullName | ConvertFrom-Json -AsHashtable
    if ($plan.Count -ne 3 -or ($plan.schemaVersion -isnot [int] -and $plan.schemaVersion -isnot [long]) -or $plan.schemaVersion -ne 1 -or $plan.case -ne $Case -or $plan.phases -isnot [System.Collections.IDictionary]) { throw 'Invalid Guide input plan.' }
    if ($plan.phases.Count -eq 0 -or $plan.phases.Count -gt 64) { throw 'Invalid Guide replay phase count.' }
    $allowed = @('Return','Escape','Left','Right','Up','Down','Menu','F9','Search')
    foreach ($phase in $plan.phases.Keys) {
        $keys = $plan.phases[$phase]
        if ($phase -notmatch '^guide[a-z]{1,40}$' -or $keys -isnot [array] -or $keys.Count -gt 20) { throw 'Invalid Guide replay phase/keys.' }
        foreach ($key in $keys) { if ($key -isnot [string] -or $key -notin $allowed) { throw 'Unsupported Guide replay key.' } }
    }
    return $plan
}
