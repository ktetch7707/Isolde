# Bounded process transport. PowerShell 7. No blocking ReadLine()/WaitForExit().
function Invoke-OracleProcess {
    param([string]$File, [string[]]$Arguments, [int]$TimeoutSeconds = 30,
          [string]$Log, [scriptblock]$OnLine, [switch]$AllowFailure)
    $start = [Diagnostics.ProcessStartInfo]::new($File)
    $start.UseShellExecute = $false; $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new(); $process.StartInfo = $start
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $tail = [Collections.Generic.Queue[string]]::new()
    $timedOut = $false; $started = $false
    try {
        $started = $process.Start()
        if (!$started) { throw "Could not start $File" }
        $outTask = $process.StandardOutput.ReadLineAsync(); $errTask = $process.StandardError.ReadLineAsync()
        $outDone = $false; $errDone = $false
        while (!$outDone -or !$errDone -or !$process.HasExited) {
            if ($clock.Elapsed.TotalSeconds -gt $TimeoutSeconds) {
                $timedOut = $true
                if (!$process.HasExited) { $process.Kill($true) }
                break
            }
            foreach ($stream in @('out','err')) {
                $task = if ($stream -eq 'out') { $outTask } else { $errTask }
                $done = if ($stream -eq 'out') { $outDone } else { $errDone }
                if (!$done -and $task.IsCompleted) {
                    $line = $task.GetAwaiter().GetResult()
                    if ($null -eq $line) {
                        if ($stream -eq 'out') { $outDone = $true } else { $errDone = $true }
                    } else {
                        if ($Log) { [IO.File]::AppendAllText($Log, "$line`n", [Text.UTF8Encoding]::new($false)) }
                        $tail.Enqueue($line); if ($tail.Count -gt 200) { [void]$tail.Dequeue() }
                        Write-Host $line
                        if ($OnLine -and $stream -eq 'out') { & $OnLine $line }
                        if ($stream -eq 'out') { $outTask = $process.StandardOutput.ReadLineAsync() }
                        else { $errTask = $process.StandardError.ReadLineAsync() }
                    }
                }
            }
            Start-Sleep -Milliseconds 20
        }
        if (!$process.HasExited -and !$process.WaitForExit(5000)) { throw "Could not stop owned process $File" }
        $code = if ($timedOut) { 124 } else { $process.ExitCode }
        $result = [pscustomobject]@{ ExitCode=$code; TimedOut=$timedOut; Lines=@($tail.ToArray()); Seconds=$clock.Elapsed.TotalSeconds }
        if ($code -ne 0 -and !$AllowFailure) { throw "$File exited $code$(if ($timedOut) {' (deadline exceeded)'}). Log: $Log. Stop here; do not retry blindly." }
        return $result
    } finally {
        if ($started -and !$process.HasExited) { $process.Kill($true); [void]$process.WaitForExit(5000) }
        $process.Dispose()
    }
}
function Get-OracleSshArguments {
    param([string]$IdentityFile)
    return @('-n','-i',$IdentityFile,'-p','2222','-o','BatchMode=yes','-o','ConnectTimeout=10',
             '-o','ServerAliveInterval=10','-o','ServerAliveCountMax=2','isolde-dev@127.0.0.1')
}
function Get-OracleScpArguments {
    param([string]$IdentityFile)
    return @('-i',$IdentityFile,'-P','2222','-o','BatchMode=yes','-o','ConnectTimeout=10',
             '-o','ServerAliveInterval=10','-o','ServerAliveCountMax=2')
}
