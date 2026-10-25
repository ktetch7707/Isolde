param(
    [ValidateSet('pages','pastel','lounge','studio','desktop','repairs','apps','clients','vpn','shell')][string]$Case = 'pages',
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [string]$Executable = '/home/isolde-dev/.local/share/isolde-appliance/current/bin/isolde',
    [string]$GuestRunner,
    [string]$RunId,
    [switch]$Installed,
    [ValidateRange(30,1800)][int]$TimeoutSeconds = 1000,
    [string]$IdentityFile = 'C:/Users/kketc/VirtualBox VMs/Isolde-Test/automation/id_ed25519_automation',
    [string]$VmName = 'Isolde-Test'
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/vm-process.ps1"
if (!$GuestRunner) { throw 'Use tools/vm-test.ps1 Visual -RunId <passing-run>. Direct unguarded acceptance is retired.' }
foreach ($path in @($SourceRoot,$Executable)) {
    if ($path -notmatch '^/[A-Za-z0-9/_.-]+$') { throw 'Use absolute Linux paths without shell metacharacters.' }
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$outputPath = (Resolve-Path -LiteralPath $OutputDirectory).Path
$sshArgs = Get-OracleSshArguments $IdentityFile
if ($GuestRunner) {
    if ($GuestRunner -notmatch '^/[A-Za-z0-9/_.-]+$' -or $RunId -notmatch '^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$') { throw 'Invalid oracle runner/run ID.' }
    $command = "python3 $GuestRunner visual --run-id $RunId --case $Case" + $(if ($Installed) { ' --installed' } else { '' })
} elseif ($Case -in @('studio','desktop')) {
    $desktopFlag = if ($Case -eq 'desktop') { ' --desktop' } else { '' }
    $command = "python3 $SourceRoot/tools/home-components-acceptance.py $Executable --studio --inspect$desktopFlag"
} elseif ($Case -eq 'pages') {
    $command = "python3 $SourceRoot/tools/tv-library-acceptance.py $Executable --pages --inspect"
} else {
    $fixture = if ($Case -eq 'pastel') { 'home-pastel-reference' } else { 'home-reference' }
    $command = "python3 $SourceRoot/tools/home-reference-acceptance.py $Executable --case $SourceRoot/tests/visual/$fixture --skin-from-settings --inspect"
}
$phases = [Collections.Generic.HashSet[string]]::new()
$captureMeta = @{ executable=$Executable }
$captureLine = {
    param($line)
    if ($line -match '^EXECUTABLE (/[A-Za-z0-9/_.-]+)$') { $captureMeta.executable=$Matches[1] }
    if ($line -match '^([A-Z]+)_READY (/tmp/isolde-(?:tv-acceptance|reference-config|components-acceptance)-[A-Za-z0-9_-]+)$') {
        $phase = $Matches[1].ToLowerInvariant(); $evidence = $Matches[2]
        if (!$phases.Add($phase)) { throw "Duplicate capture phase: $phase" }
        $capture = Join-Path $outputPath "$Case-$phase.png"
        if (Test-Path -LiteralPath $capture) { throw "Refusing to overwrite evidence: $capture" }
        Start-Sleep -Seconds 2
        $null = Invoke-OracleProcess -File 'C:/Program Files/Oracle/VirtualBox/VBoxManage.exe' `
            -Arguments @('controlvm',$VmName,'screenshotpng',$capture) -TimeoutSeconds 20 `
            -Log (Join-Path $outputPath 'capture.log')
        if (!(Test-Path -LiteralPath $capture) -or (Get-Item -LiteralPath $capture).Length -eq 0) { throw "Screenshot missing: $phase" }
        $vpnInput = $Case -eq 'vpn' -and $phase -in @('vpnnavigation','vpnpermission','vpnprovider','vpnsettings','vpnprotocol','vpnprotocolconfirm','vpnconnection','vpnproviderstate','vpnassign','vpnuspermission','vpnusprovider','vpnussearch','vpnusconnect','cbcvpnpermission')
        $shellInput = $Case -eq 'shell' -and $phase -in @('shellnavigation','shellgallery','shellstore','shellreturn','shellambient','shellwake')
        $nativePermission = ($Case -in @('apps','clients') -and $phase -in @('steampermission','freetubepermission','streamingpermission')) -or $vpnInput -or $shellInput
        $nativeReturn = ($Case -in @('apps','clients') -and $phase -in @('steam','freetube','streaming')) -or ($Case -eq 'vpn' -and $phase -in @('vpnproviderreturn','vpnusreturn','cbcvpn'))
        if (($Case -eq 'repairs' -and $phase -in @('desktoppermission','desktop')) -or ($Case -eq 'apps' -and $phase -eq 'steaminstall') -or $nativePermission -or $nativeReturn) {
            if ($VmName -ne 'Isolde-Test') { throw 'Compositor input is restricted to the Isolde-Test guest.' }
            $keys = @('Home')
            if ($phase -in @('desktoppermission','steaminstall') -or $nativePermission) {
                $instructions = Join-Path $outputPath $(if ($phase -eq 'desktoppermission') {'desktop-permission-keys.json'} elseif($phase -eq 'steaminstall') {'steam-install-keys.json'} else {$phase+'-keys.json'})
                Write-Host "Review $capture then provide bounded guest keys in $instructions"
                $deadline = [DateTime]::UtcNow.AddSeconds(110)
                while (!(Test-Path -LiteralPath $instructions)) {
                    if ([DateTime]::UtcNow -gt $deadline) { throw 'Guest dialog review timed out; no consent input sent.' }
                    Start-Sleep -Milliseconds 250
                }
                if ((Get-Item -LiteralPath $instructions).Length -gt 2048) { throw 'Oversized guest key instruction.' }
                $keyDocument = [IO.File]::ReadAllText($instructions) | ConvertFrom-Json
                $keys = @($keyDocument.keys)
            }
            $scancodes = @{Home=@('e0','47','e0','c7');Return=@('1c','9c');Tab=@('0f','8f');Space=@('39','b9');Escape=@('01','81');Left=@('e0','4b','e0','cb');Right=@('e0','4d','e0','cd');Up=@('e0','48','e0','c8');Down=@('e0','50','e0','d0');ShiftTab=@('2a','0f','8f','aa');CtrlF=@('1d','21','a1','9d')}
            if ($shellInput) { $scancodes.A=@('1e','9e') }
            if ((!$keys.Count -and !$nativePermission -and $phase -ne 'steaminstall') -or $keys.Count -gt 20) { throw 'Invalid bounded guest keystroke count.' }
            foreach ($key in $keys) {
                if (!$scancodes.ContainsKey([string]$key)) { throw "Unsupported guest key: $key" }
            }
            foreach ($key in $keys) {
                $null = Invoke-OracleProcess -File 'C:/Program Files/Oracle/VirtualBox/VBoxManage.exe' -Arguments (@('controlvm',$VmName,'keyboardputscancode') + $scancodes[[string]$key]) -TimeoutSeconds 20 -Log (Join-Path $outputPath 'guest-keys.log')
                Start-Sleep -Milliseconds 300
            }
            if ($vpnInput -and $keyDocument.text) {
                if ($phase -notin @('vpnprovider','vpnproviderstate','vpnusprovider','vpnussearch','vpnusconnect') -or $keyDocument.text -notin @('Canada','United States')) { throw 'Only reviewed country names may be typed into the guest provider UI.' }
                $null = Invoke-OracleProcess -File 'C:/Program Files/Oracle/VirtualBox/VBoxManage.exe' -Arguments @('controlvm',$VmName,'keyboardputstring',[string]$keyDocument.text) -TimeoutSeconds 20 -Log (Join-Path $outputPath 'guest-keys.log')
            }
            if ($vpnInput -and $keyDocument.click) {
                if ($phase -notin @('vpnpermission','vpnprovider','vpnsettings','vpnprotocol','vpnprotocolconfirm','vpnconnection','vpnproviderstate','vpnusprovider','vpnussearch','vpnusconnect')) { throw 'Pointer input is restricted to reviewed native VPN-provider frames.' }
                $point = $keyDocument.click
                foreach ($axis in @('x','y')) {
                    if (($point.$axis -isnot [int] -and $point.$axis -isnot [long]) -or $point.$axis -lt 0 -or $point.$axis -gt 3839) { throw 'Invalid bounded guest point.' }
                }
                $pointFile = Join-Path $outputPath "$phase-click.json"
                @{x=[int]$point.x;y=[int]$point.y} | ConvertTo-Json -Compress | Set-Content -LiteralPath $pointFile -Encoding utf8NoBOM
                $scpArgs = Get-OracleScpArguments $IdentityFile
                $null = Invoke-OracleProcess -File scp.exe -Arguments ($scpArgs + @($pointFile,"isolde-dev@127.0.0.1:$evidence/$phase-click.json")) -TimeoutSeconds 20 -Log (Join-Path $outputPath 'guest-pointer.log')
            }
        }
        $null = Invoke-OracleProcess -File ssh.exe -Arguments ($sshArgs + "touch $evidence/$phase-captured") `
            -TimeoutSeconds 20 -Log (Join-Path $outputPath 'acknowledgements.log')
        Write-Host "Captured $capture (visual review still required)"
    }
}
try {
    $null = Invoke-OracleProcess -File ssh.exe -Arguments ($sshArgs + $command) -TimeoutSeconds $TimeoutSeconds `
        -Log (Join-Path $outputPath "$Case-output.log") -OnLine $captureLine
} catch {
    $null = Invoke-OracleProcess -File ssh.exe -Arguments ($sshArgs + "python3 $GuestRunner cancel --run-id $RunId") `
        -TimeoutSeconds 30 -Log (Join-Path $outputPath 'cleanup.log') -AllowFailure
    throw
}
if (!$phases.Count) { throw 'No real framebuffer captures were produced; this is not visual acceptance.' }
@{ case=$Case; captured=@($phases | Sort-Object); visualReview='pending'; source=$SourceRoot; executable=$captureMeta.executable } |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $outputPath 'captures.json')
