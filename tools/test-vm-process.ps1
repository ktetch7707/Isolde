param([Parameter(Mandatory = $true)][string] $PythonExe)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/vm-process.ps1"

# Fail immediately so the contract report identifies the first broken process guarantee.
function Assert($condition, $message) {
    if (!$condition) { throw $message }
}

$result = Invoke-OracleProcess -File $PythonExe -TimeoutSeconds 5 -Arguments @(
    '-c', 'import sys; print("out"); print("err", file=sys.stderr)'
)
Assert ($result.ExitCode -eq 0 -and $result.Lines -contains 'out' -and $result.Lines -contains 'err') 'Both streams must drain'

$result = Invoke-OracleProcess -File $PythonExe -TimeoutSeconds 5 -AllowFailure -Arguments @(
    '-c', 'import sys; raise SystemExit(7)'
)
Assert ($result.ExitCode -eq 7) 'Nonzero status lost'

$result = Invoke-OracleProcess -File $PythonExe -TimeoutSeconds 1 -AllowFailure -Arguments @(
    '-c', 'import time; time.sleep(60)'
)
Assert ($result.TimedOut -and $result.ExitCode -eq 124 -and $result.Seconds -lt 4) 'Silent transport did not time out'

$literal = 'spaces $HOME $(must-not-run) `literal` "quotes"'
$result = Invoke-OracleProcess -File $PythonExe -TimeoutSeconds 5 -Arguments @(
    '-c', 'import sys; print(sys.argv[1])', $literal
)
Assert ($result.Lines -contains $literal) 'Argument quoting changed literal input'

$seen = [Collections.Generic.List[string]]::new()
$null = Invoke-OracleProcess -File $PythonExe -TimeoutSeconds 5 -Arguments @(
    '-c', 'print("PHASE_READY /tmp/test")'
) -OnLine { param($line) $seen.Add($line) }
Assert ($seen.Count -eq 1) 'Capture callback lost'

$threw = $false
try {
    $null = Invoke-OracleProcess -File $PythonExe -TimeoutSeconds 5 -Arguments @(
        '-c', 'import time; print("ready", flush=True); time.sleep(60)'
    ) -OnLine { throw 'capture failed' }
} catch {
    $threw = $_.Exception.Message -eq 'capture failed'
}
Assert $threw 'Capture failure must unwind and stop the owned SSH process'
Write-Host 'PASS: streams, exit status, silent deadline, literal arguments, callback and failure cleanup'
