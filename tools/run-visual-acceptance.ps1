param(
    [ValidateSet('movieart','pages','guide','guide-surfaces','guide-lifecycle','guide-features','guide-soak','guide-reopen','pastel','lounge','studio','desktop','collections','repairs','apps','clients','vpn','shell','playlists','android','androidapps','store','games','emulators')][string]$Case = 'pages',
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [string]$Executable = '/home/isolde-dev/.local/share/isolde-appliance/current/bin/isolde',
    [string]$GuestRunner,
    [string]$RunId,
    [switch]$Installed,
    [string]$GuideReplayPlan,
    [string]$ReopenHelperDigest,
    [string]$FeatureHelperDigest,
    [string]$StudioHelperDigest,
    [ValidateRange(30,1800)][int]$TimeoutSeconds = 1000,
    [ValidateRange(0,900)][int]$WaitSeconds = 0,
    [string]$IdentityFile = 'C:/Users/kketc/VirtualBox VMs/Isolde-Test/automation/id_ed25519_automation',
    [string]$VmName = 'Isolde-Test'
)
$ErrorActionPreference = 'Stop'
$transportTimeout = $TimeoutSeconds + $WaitSeconds
. "$PSScriptRoot/vm-process.ps1"
if (!$GuestRunner) { throw 'Use tools/vm-test.ps1 Visual -RunId <passing-run>. Direct unguarded acceptance is retired.' }
foreach ($path in @($SourceRoot,$Executable)) {
    if ($path -notmatch '^/[A-Za-z0-9/_.-]+$') { throw 'Use absolute Linux paths without shell metacharacters.' }
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$outputPath = (Resolve-Path -LiteralPath $OutputDirectory).Path
$guidePlan = $null
if ($GuideReplayPlan) {
    . "$PSScriptRoot/guide-input-plan.ps1"
    $guidePlan = Import-GuideInputPlan -Path $GuideReplayPlan -Case $Case
    $guidePlan | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputPath 'reviewed-input-plan.json')
    Write-Host 'Replaying bounded fixture input; captured screenshots still require review.'
}
$sshArgs = Get-OracleSshArguments $IdentityFile
if ($GuestRunner) {
    if ($GuestRunner -notmatch '^/[A-Za-z0-9/_.-]+$' -or $RunId -notmatch '^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$') { throw 'Invalid oracle runner/run ID.' }
    $command = "python3 $GuestRunner visual --run-id $RunId --case $Case" + $(if ($Installed) { ' --installed' } else { '' })
    if ($ReopenHelperDigest) {
        if ($Case -ne 'guide-reopen' -or $ReopenHelperDigest -notmatch '^[0-9a-f]{64}$') { throw 'Invalid cold-reopen helper identity.' }
        $command += " --reopen-helper-digest $ReopenHelperDigest"
    }
    if($WaitSeconds){$command += " --wait-seconds $WaitSeconds"}
    if ($FeatureHelperDigest) {
        if ($Case -ne 'guide-features' -or $FeatureHelperDigest -notmatch '^[0-9a-f]{64}$') { throw 'Invalid Guide feature helper identity.' }
        $command += " --feature-helper-digest $FeatureHelperDigest"
    }
    if ($StudioHelperDigest) {
        if ($Case -notin @('studio','desktop','collections') -or $StudioHelperDigest -notmatch '^[0-9a-f]{64}$') { throw 'Invalid Studio helper identity.' }
        $command += " --studio-helper-digest $StudioHelperDigest"
    }
} elseif ($Case -in @('studio','desktop','collections')) {
    $desktopFlag = if ($Case -eq 'desktop') { ' --desktop' } elseif ($Case -eq 'collections') { ' --collections' } else { '' }
    $command = "python3 $SourceRoot/tools/home-components-acceptance.py $Executable --studio --inspect$desktopFlag"
} elseif ($Case -eq 'pages') {
    $command = "python3 $SourceRoot/tools/tv-library-acceptance.py $Executable --pages --inspect"
} elseif ($Case -eq 'playlists') {
    $command = "python3 $SourceRoot/tools/tv-library-acceptance.py $Executable --playlists --inspect"
} else {
    $fixture = if ($Case -eq 'pastel') { 'home-pastel-reference' } else { 'home-reference' }
    $command = "python3 $SourceRoot/tools/home-reference-acceptance.py $Executable --case $SourceRoot/tests/visual/$fixture --skin-from-settings --inspect"
}
$phases = [Collections.Generic.HashSet[string]]::new()
$captureMeta = @{ executable=$Executable }
$captureLine = {
    param($line)
    if ($Case -in @('guide-lifecycle','guide-features','guide-reopen') -and $line -match '^Inspect: /tmp/isolde-tv-acceptance-[A-Za-z0-9_-]+$') {
        if ($VmName -ne 'Isolde-Test') { throw 'Guide diagnostics are restricted to Isolde-Test.' }
        # Capture the failed state while the helper still owns its windows.
        # This sends no input and preserves the original failing exit status.
        $failureCapture = Join-Path $outputPath "$Case-failure.png"
        if (!(Test-Path -LiteralPath $failureCapture)) {
            $null = Invoke-OracleProcess -File 'C:/Program Files/Oracle/VirtualBox/VBoxManage.exe' -Arguments @('controlvm',$VmName,'screenshotpng',$failureCapture) -TimeoutSeconds 15 -Log (Join-Path $outputPath 'capture.log')
        }
    }
    if ($line -eq "STAGE visual-$Case" -and $Case -in @('guide-lifecycle','guide-features','guide-reopen')) {
        if ($VmName -ne 'Isolde-Test') { throw 'Guide input is restricted to Isolde-Test.' }
        # Clear compositor search/context overlays before the fresh test windows
        # exist. The guest guardian already owns the lock at this stage marker.
        $null = Invoke-OracleProcess -File 'C:/Program Files/Oracle/VirtualBox/VBoxManage.exe' -Arguments @('controlvm',$VmName,'keyboardputscancode','01','81','01','81','01','81') -TimeoutSeconds 20 -Log (Join-Path $outputPath 'guest-keys.log')
    }
    if ($Case -eq 'guide-reopen' -and $line -match '^GUIDE_REOPEN_TARGET ([0-9]{1,2})$') {
        $captureMeta.reopenTarget = [int]$Matches[1]
        if ($captureMeta.reopenTarget -gt 16) { throw 'Unbounded cold-reopen fixture target.' }
    }
    if ($line -match '^EXECUTABLE (/[A-Za-z0-9/_.-]+)$') { $captureMeta.executable=$Matches[1] }
    if ($line -match '^([A-Z]+)_READY (/tmp/isolde-(?:tv-acceptance|reference-config|components-acceptance)-[A-Za-z0-9_-]+)$') {
        $guideReviewDeadline = [DateTime]::UtcNow.AddSeconds(100)
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
        $androidInput = $Case -eq 'androidapps' -and $phase -in @('androidnavigation','androidgallery','androidlaunch','androidpermission','androidopen','androidremote','androidreturn')
        $gamesInput = ($Case -eq 'games' -and $phase -in @('gamespermission','gamesmenu')) -or ($Case -eq 'emulators' -and $phase -in @('emulatorspermission','emulatorsmenu','emulatorsplay'))
        $guideInput = $Case -in @('guide','guide-surfaces','guide-lifecycle','guide-features','guide-soak','guide-reopen') -and $phase -in @('guidestartup','guideorigin','guidehome','guideempty','guidesource','guidepvr','guidevideo','guidereturn','guidelocaldraft','guidelocalpreview','guidelocalreview','guidelocalcreated','guidelocalvideo','guidelocalosd','guidelocalreturn','guideduplicatedraft','guidelineup','guidewidgetentry','guidecompact','guideoverlay','guideexpanded','guideoverlayreturn','guidecompactreturn','guidecontinuousready','guidecontinuousvideo','guidecontinuousnext','guidesurfed','guidesurfreturn','guidedeletedready','guidedeletedfile','guidedeletedreturn','guiderepairselect','guiderepaircontext','guiderepairreview','guiderepairconfirm','guiderepaired','guidefeedsetup','guidefeedpreview','guidefeedmapped','guidefeedimported','guidefeednolistings','guidefeedreturn','guidefeedauth','guidefeedoff','guidestationselect','guidestationlogo','guidestationcolor','guideappearanceapplied','guideexport','guideimportdraft','guideimportpreview','guideimported','guidetemplatesaved','guidetemplatereuse','guidecartoonrules','guidecalendarblock','guidecalendarsaved','guidemixedrules','guidemixedpreview','guidemixedcreated','guidedirectdraft','guidedirectpreview','guidedirectimported','guidefeedrefreshfailed','guidesoakhome','guidesoakguide','guidesoakvideo','guidesoakreturn','guidesoaksearch','guidesoakresults','guidesoakrecovery')
        $nativePermission = $gamesInput -or $androidInput -or ($Case -in @('apps','clients') -and $phase -in @('steampermission','freetubepermission','streamingpermission')) -or $vpnInput -or $shellInput -or $guideInput
        $storeReturn = $Case -eq 'store' -and $phase -in @('storeandroidopen','storechromiumopen')
        $nativeReturn = ($Case -eq 'games' -and $phase -eq 'gamesplay') -or ($Case -eq 'emulators' -and $phase -eq 'emulatorsaction') -or $storeReturn -or ($Case -in @('apps','clients') -and $phase -in @('steam','freetube','streaming')) -or ($Case -eq 'vpn' -and $phase -in @('vpnproviderreturn','vpnusreturn','cbcvpn'))
        if (($Case -eq 'repairs' -and $phase -in @('desktoppermission','desktop')) -or ($Case -eq 'apps' -and $phase -eq 'steaminstall') -or $nativePermission -or $nativeReturn) {
            if ($VmName -ne 'Isolde-Test') { throw 'Compositor input is restricted to the Isolde-Test guest.' }
            $keys = @('Home')
            if ($phase -in @('desktoppermission','steaminstall') -or $nativePermission) {
                $instructions = Join-Path $outputPath $(if ($phase -eq 'desktoppermission') {'desktop-permission-keys.json'} elseif($phase -eq 'steaminstall') {'steam-install-keys.json'} else {$phase+'-keys.json'})
                if ($guideInput -and $guidePlan -and $guidePlan.phases.Contains($phase)) {
                    if (Test-Path -LiteralPath $instructions) { throw 'Refusing to replace an existing input instruction.' }
                    $replayKeys=@($guidePlan.phases[$phase])
                    if ($Case -eq 'guide-reopen' -and $phase -eq 'guidesoakguide') {
                        if (!$captureMeta.ContainsKey('reopenTarget')) { throw 'Missing verified cold-reopen fixture target.' }
                        $replayKeys=@((0..$captureMeta.reopenTarget | ForEach-Object {'Down'})+@('Return'))
                    }
                    @{keys=$replayKeys} | ConvertTo-Json -Compress | Set-Content -LiteralPath $instructions -Encoding utf8NoBOM
                    Write-Host "Replayed fixture phase $phase; screenshot review pending."
                }
                $deadline = if ($guideInput) { $guideReviewDeadline } else { [DateTime]::UtcNow.AddSeconds(110) }
                $refreshCount = 0
                while ($true) {
                    if (!(Test-Path -LiteralPath $instructions)) { Write-Host "Review $capture then provide bounded guest keys in $instructions" }
                    while (!(Test-Path -LiteralPath $instructions)) {
                        if ([DateTime]::UtcNow -gt $deadline) { throw 'Guest dialog review timed out; no consent input sent.' }
                        Start-Sleep -Milliseconds 250
                    }
                    if ([DateTime]::UtcNow -gt $deadline) { throw 'Guest dialog review timed out; no consent input sent.' }
                    if ((Get-Item -LiteralPath $instructions).Length -gt 2048) { throw 'Oversized guest key instruction.' }
                    $keyDocument = [IO.File]::ReadAllText($instructions) | ConvertFrom-Json
                    $keys = @($keyDocument.keys)
                    if ($guideInput) {
                        $fields = @($keyDocument.PSObject.Properties.Name)
                        if ('keys' -notin $fields -or $keyDocument.keys -isnot [array] -or @($fields | Where-Object { $_ -notin @('keys','recapture') }).Count) { throw 'Invalid Guide review instruction.' }
                        if ('recapture' -in $fields) {
                            if ($keyDocument.recapture -isnot [bool] -or !$keyDocument.recapture -or $keys.Count) { throw 'Guide recapture requires true and an empty keys array.' }
                            if ($refreshCount -ge 3) { throw 'Guide review permits at most three additional captures.' }
                            if (($deadline - [DateTime]::UtcNow).TotalSeconds -le 22) { throw 'Guide recapture would exceed the original review deadline.' }
                            Remove-Item -LiteralPath $instructions
                            $refreshCount++
                            $capture = Join-Path $outputPath "$Case-$phase-review$refreshCount.png"
                            if (Test-Path -LiteralPath $capture) { throw "Refusing to overwrite evidence: $capture" }
                            Start-Sleep -Seconds 2
                            $null = Invoke-OracleProcess -File 'C:/Program Files/Oracle/VirtualBox/VBoxManage.exe' `
                                -Arguments @('controlvm',$VmName,'screenshotpng',$capture) -TimeoutSeconds 20 `
                                -Log (Join-Path $outputPath 'capture.log')
                            if (!(Test-Path -LiteralPath $capture) -or (Get-Item -LiteralPath $capture).Length -eq 0) { throw "Screenshot missing: $phase review $refreshCount" }
                            Write-Host "Recaptured phase $phase review $refreshCount at $capture; no input or acknowledgement sent."
                            continue
                        }
                    }
                    break
                }
            }
            $scancodes = @{Home=@('e0','47','e0','c7');Return=@('1c','9c');Tab=@('0f','8f');Space=@('39','b9');Escape=@('01','81');Left=@('e0','4b','e0','cb');Right=@('e0','4d','e0','cd');Up=@('e0','48','e0','c8');Down=@('e0','50','e0','d0');ShiftTab=@('2a','0f','8f','aa');CtrlF=@('1d','21','a1','9d')}
            # Use Isolde's existing application Search shortcut; XF86Search is
            # intercepted by GNOME and opens the desktop overview instead.
            if ($guideInput) { $scancodes.F9=@('43','c3');$scancodes.Menu=@('e0','5d','e0','dd');$scancodes.Search=@('35','b5') }
            if ($shellInput) { $scancodes.A=@('1e','9e') }
            if ($Case -eq 'emulators') { $scancodes.X=@('2d','ad');$scancodes.Z=@('2c','ac');$scancodes.S=@('1f','9f') }
            if ((!$keys.Count -and !$nativePermission -and $phase -ne 'steaminstall') -or $keys.Count -gt 20) { throw 'Invalid bounded guest keystroke count.' }
            foreach ($key in $keys) {
                if (!$scancodes.ContainsKey([string]$key)) { throw "Unsupported guest key: $key" }
            }
            if ($guideInput) {
                $null = Invoke-OracleProcess -File 'C:/Program Files/Oracle/VirtualBox/VBoxManage.exe' -Arguments @('controlvm',$VmName,'keyboardputscancode','aa','b6','9d','e0','9d','b8','e0','b8','e0','db','e0','dc') -TimeoutSeconds 20 -Log (Join-Path $outputPath 'guest-keys.log')
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
        if ($guidePlan) { Write-Host "CAPTURE $phase review=pending" } else { Write-Host "Captured $capture (visual review still required)" }
    }
}
try {
    $null = Invoke-OracleProcess -File ssh.exe -Arguments ($sshArgs + $command) -TimeoutSeconds $transportTimeout `
        -Log (Join-Path $outputPath "$Case-output.log") -OnLine $captureLine
} catch {
    $null = Invoke-OracleProcess -File ssh.exe -Arguments ($sshArgs + "python3 $GuestRunner cancel --run-id $RunId") `
        -TimeoutSeconds 30 -Log (Join-Path $outputPath 'cleanup.log') -AllowFailure
    throw
}
if (!$phases.Count) { throw 'No real framebuffer captures were produced; this is not visual acceptance.' }
@{ case=$Case; captured=@($phases | Sort-Object); visualReview='pending'; source=$SourceRoot; executable=$captureMeta.executable } |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $outputPath 'captures.json')
