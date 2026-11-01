param([Parameter(Mandatory=$true)][string]$PythonExe)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/vm-process.ps1"
function Assert($condition,$message) { if (!$condition) { throw $message } }
$r = Invoke-OracleProcess -File $PythonExe -Arguments @('-c','import sys;print("out");print("err",file=sys.stderr)') -TimeoutSeconds 5
Assert ($r.ExitCode -eq 0 -and $r.Lines -contains 'out' -and $r.Lines -contains 'err') 'Both streams must drain'
$r = Invoke-OracleProcess -File $PythonExe -Arguments @('-c','import sys;raise SystemExit(7)') -AllowFailure -TimeoutSeconds 5
Assert ($r.ExitCode -eq 7) 'Nonzero status lost'
$r = Invoke-OracleProcess -File $PythonExe -Arguments @('-c','import time;time.sleep(60)') -AllowFailure -TimeoutSeconds 1
Assert ($r.TimedOut -and $r.ExitCode -eq 124 -and $r.Seconds -lt 4) 'Silent transport did not time out'
$literal = 'spaces $HOME $(must-not-run) `literal` "quotes"'
$r = Invoke-OracleProcess -File $PythonExe -Arguments @('-c','import sys;print(sys.argv[1])',$literal) -TimeoutSeconds 5
Assert ($r.Lines -contains $literal) 'Argument quoting changed literal input'
$seen = [Collections.Generic.List[string]]::new()
$r = Invoke-OracleProcess -File $PythonExe -Arguments @('-c','print("PHASE_READY /tmp/test")') -TimeoutSeconds 5 -OnLine { param($line) $seen.Add($line) }
Assert ($seen.Count -eq 1) 'Capture callback lost'
$threw = $false
try { $null = Invoke-OracleProcess -File $PythonExe -Arguments @('-c','import time;print("ready",flush=True);time.sleep(60)') -TimeoutSeconds 5 -OnLine { throw 'capture failed' } }
catch { $threw = $_.Exception.Message -eq 'capture failed' }
Assert $threw 'Capture failure must unwind and stop the owned SSH process'
Write-Host 'PASS: streams, exit status, silent deadline, literal arguments, callback and failure cleanup'
