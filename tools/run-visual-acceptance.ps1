param(
    [ValidateSet('pages','pastel','lounge')][string]$Case = 'pages',
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$SourceRoot = '/tmp/isolde-order-vsDgGY',
    [string]$IdentityFile = 'C:/Users/kketc/VirtualBox VMs/Isolde-Test/automation/id_ed25519_automation',
    [string]$VmName = 'Isolde-Test'
)
$ErrorActionPreference = 'Stop'
if ($SourceRoot -notmatch '^/[A-Za-z0-9/_.-]+$') { throw 'Use an absolute Linux source directory without shell metacharacters.' }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$outputPath = (Resolve-Path -LiteralPath $OutputDirectory).Path
$executable = '/home/isolde-dev/.local/share/isolde-appliance/current/bin/isolde'
$command = if ($Case -eq 'pages') {
    "python3 $SourceRoot/tools/tv-library-acceptance.py $executable --pages --inspect"
} else {
    $fixture = if ($Case -eq 'pastel') { 'home-pastel-reference' } else { 'home-reference' }
    "python3 $SourceRoot/tools/home-reference-acceptance.py $executable --case $SourceRoot/tests/visual/$fixture --skin-from-settings --inspect"
}
$start = [Diagnostics.ProcessStartInfo]::new('ssh.exe')
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
foreach ($argument in @('-i',$IdentityFile,'-p','2222','-o','BatchMode=yes','isolde-dev@127.0.0.1',$command)) { $start.ArgumentList.Add($argument) }
$process = [Diagnostics.Process]::new()
$process.StartInfo = $start
if (!$process.Start()) { throw 'Could not start the acceptance connection.' }
$errors = $process.StandardError.ReadToEndAsync()
try {
    while (($line = $process.StandardOutput.ReadLine()) -ne $null) {
        Write-Output $line
        Add-Content -LiteralPath (Join-Path $outputPath "$Case-output.log") -Value $line
        if ($line -match '^([A-Z]+)_READY (/tmp/isolde-(?:tv-acceptance|reference-config)-[A-Za-z0-9_-]+)$') {
            $phase = $Matches[1].ToLowerInvariant()
            $evidence = $Matches[2]
            $capture = Join-Path $outputPath "$Case-$phase.png"
            Start-Sleep -Seconds 2 # Let the destination frame reach the VM display before reading its framebuffer.
            & 'C:/Program Files/Oracle/VirtualBox/VBoxManage.exe' controlvm $VmName screenshotpng $capture
            if ($LASTEXITCODE -ne 0 -or (Get-Item -LiteralPath $capture).Length -eq 0) { throw "Screenshot failed: $phase" }
            # This acknowledges file capture only. Visual review remains a separate step.
            & ssh.exe -i $IdentityFile -p 2222 -o BatchMode=yes isolde-dev@127.0.0.1 "touch $evidence/$phase-captured"
            if ($LASTEXITCODE -ne 0) { throw "Could not acknowledge capture: $phase" }
            Write-Output "Captured $capture"
        }
    }
    $process.WaitForExit()
    $errorText = $errors.GetAwaiter().GetResult()
    if ($errorText) { Write-Output $errorText }
    if ($process.ExitCode -ne 0) { throw "Acceptance failed with exit code $($process.ExitCode)." }
} finally {
    $process.Dispose()
}
