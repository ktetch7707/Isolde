param(
    [ValidateSet('Check','Test','Visual','Deploy','Provision','Configure','Diagnose','Status','Recover','Cancel')][string]$Action = 'Test',
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$')][string]$RunId,
    [ValidateSet('all','guide','guide_contracts','guide_remote_ui','games','store','store_remote_ui','audio_backend','vpn','studio','studio_quick','studio_selection','studio_gestures','studio_grid','studio_destinations','studio_ordered','studio_policies','studio_overlays','studio_unit','studio_behavior','component_contracts','home_component_navigation','studio_navigation','kodi_media_client','desktop_mode','profile_browser_isolation')][string]$Suite = 'all',
    [ValidateSet('studio','desktop','lounge','pastel','pages','guide','guide-surfaces','guide-lifecycle','guide-features','guide-soak','guide-reopen','movieart','repairs','apps','clients','vpn','shell','playlists','android','androidapps','store','games','emulators')][string[]]$Cases = @('studio','lounge','pastel'),
    [switch]$Installed,
    [string]$GuideReplayPlan,
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$')][string]$ReuseFullRun,
    [switch]$Capture,
    [switch]$Storage,
    [switch]$GuideDiagnostics,
    [ValidateRange(0,900)][int]$WaitSeconds=0,
    [ValidateSet('freetube','cbc-gem','steam')][string]$NativeApp = 'freetube',
    [ValidateSet('native','surfshark','android','android-repair','games','guide','artifacts')][string]$ConfigureTarget = 'native',
    [string]$IdentityFile = 'C:/Users/kketc/VirtualBox VMs/Isolde-Test/automation/id_ed25519_automation'
)
$ErrorActionPreference = 'Stop'
if ($ReuseFullRun -and ($Action -ne 'Test' -or $Suite -ne 'guide_remote_ui')) { throw 'ReuseFullRun requires Test -Suite guide_remote_ui and supports only bounded Guide QML changes.' }
if ($Storage -and $Action -ne 'Status') { throw 'Storage inventory is restricted to Status.' }
if ($GuideDiagnostics -and $Action -ne 'Status') { throw 'GuideDiagnostics is Status-only.' }
if ($GuideReplayPlan -and ($Action -ne 'Visual' -or $Cases.Count -ne 1 -or $Cases[0] -notin @('guide-surfaces','guide-lifecycle','guide-reopen','guide-features'))) { throw 'GuideReplayPlan requires one supported Guide Visual case.' }
if ($WaitSeconds -and $Action -notin @('Test','Configure','Visual')) { throw 'Bounded lock waiting is available for Test/Configure/Visual only.' }
if ($Capture -and $Action -ne 'Status') { throw 'Capture is restricted to Status of an active guest setup guardian.' }
if ($PSVersionTable.PSVersion.Major -lt 7) { throw 'Use PowerShell 7 (pwsh), not Windows PowerShell.' }
. "$PSScriptRoot/vm-process.ps1"
$repo = Split-Path $PSScriptRoot -Parent
if (!$RunId) {
    if ($Action -notin @('Check','Test')) { throw "$Action requires -RunId from a previous Test result. No implicit build or deployment." }
    $RunId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,6)
}
$local = Join-Path $repo "build/vm-oracle/$RunId"
if ($Action -eq 'Test' -and (Test-Path -LiteralPath $local)) { throw 'Run ID already exists. Use Status/Visual/Deploy; do not refreeze over evidence.' }
New-Item -ItemType Directory -Force -Path $local | Out-Null
Write-Host "RUN_ID=$RunId"
Write-Host "EVIDENCE=$local"
$sshArgs = Get-OracleSshArguments $IdentityFile
$scpArgs = Get-OracleScpArguments $IdentityFile
$transport = Join-Path $local 'transport.log'
$guestRun = $null; $exitCode = 0
$launched = $false
try {
    $homeResult = Invoke-OracleProcess -File ssh.exe -Arguments ($sshArgs + "python3 -c 'from pathlib import Path; print(Path.home())'") -TimeoutSeconds 25 -Log $transport
    $guestHome = $homeResult.Lines | Where-Object { $_ -match '^/[A-Za-z0-9/_.-]+$' } | Select-Object -Last 1
    if (!$guestHome) { throw 'Could not determine a safe guest home path.' }
    $guestRun = "$guestHome/isolde-oracle/runs/$RunId"
    $driverHash = (Get-FileHash -LiteralPath "$PSScriptRoot/vm-oracle.py" -Algorithm SHA256).Hash.ToLowerInvariant()
    $guestDriver = "$guestHome/isolde-oracle/bin/driver-$driverHash.py"
    $null = Invoke-OracleProcess -File ssh.exe -Arguments ($sshArgs + "mkdir -p $guestHome/isolde-oracle/bin $guestRun") -TimeoutSeconds 25 -Log $transport
    $null = Invoke-OracleProcess -File scp.exe -Arguments ($scpArgs + @("$PSScriptRoot/vm-oracle.py","isolde-dev@127.0.0.1:$guestDriver")) -TimeoutSeconds 30 -Log $transport
    $null = Invoke-OracleProcess -File ssh.exe -Arguments ($sshArgs + "echo '$driverHash  $guestDriver' | sha256sum -c -") -TimeoutSeconds 25 -Log $transport
    if ($Action -eq 'Test') {
        $unknown = & git -C $repo ls-files --others --exclude-standard -- src qml config tests CMakeLists.txt
        if ($unknown) { throw "Track required new application/test files before testing: $($unknown -join ', '). Untracked artifacts are never uploaded automatically." }
        Push-Location $repo
        try { & "$PSScriptRoot/package-vm-source.ps1" -OutputDirectory (Join-Path $local 'snapshot') }
        finally { Pop-Location }
        $files = @('source.tar.gz','archive.sha256','files.sha256') | ForEach-Object { Join-Path $local "snapshot/$_" }
        $null = Invoke-OracleProcess -File scp.exe -Arguments ($scpArgs + $files + "isolde-dev@127.0.0.1:$guestRun/") -TimeoutSeconds 90 -Log $transport
    }
    if ($Action -eq 'Visual') {
        foreach ($case in $Cases) {
            $reopenDigest = ''
            $featureDigest = ''
            $studioDigest = ''
            if ($case -eq 'studio') {
                $helper = Join-Path $PSScriptRoot 'home-components-acceptance.py'
                $studioDigest = (Get-FileHash -LiteralPath $helper -Algorithm SHA256).Hash.ToLowerInvariant()
                $null = Invoke-OracleProcess -File scp.exe -Arguments ($scpArgs + @($helper,"isolde-dev@127.0.0.1:$guestHome/isolde-oracle/bin/studio-helper-$studioDigest.py")) -TimeoutSeconds 30 -Log $transport
            }
            if ($case -eq 'guide-features') {
                $helper = Join-Path $PSScriptRoot 'guide_features.py'
                $featureDigest = (Get-FileHash -LiteralPath $helper -Algorithm SHA256).Hash.ToLowerInvariant()
                $null = Invoke-OracleProcess -File scp.exe -Arguments ($scpArgs + @($helper,"isolde-dev@127.0.0.1:$guestHome/isolde-oracle/bin/guide-features-$featureDigest.py")) -TimeoutSeconds 30 -Log $transport
            }
            if ($case -eq 'guide-reopen') {
                $helper = Join-Path $PSScriptRoot 'guide-soak-acceptance.py'
                $reopenDigest = (Get-FileHash -LiteralPath $helper -Algorithm SHA256).Hash.ToLowerInvariant()
                $null = Invoke-OracleProcess -File scp.exe -Arguments ($scpArgs + @($helper,"isolde-dev@127.0.0.1:$guestHome/isolde-oracle/bin/guide-reopen-$reopenDigest.py")) -TimeoutSeconds 30 -Log $transport
            }
            $attempt = 'attempt-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
            & "$PSScriptRoot/run-visual-acceptance.ps1" -Case $case -SourceRoot "$guestRun/source" `
                -GuestRunner $guestDriver -RunId $RunId -Installed:$Installed -IdentityFile $IdentityFile `
                -Executable "$guestRun/payload/bin/isolde" -OutputDirectory (Join-Path $local "visual/$case/$attempt") -WaitSeconds $WaitSeconds -GuideReplayPlan $GuideReplayPlan -ReopenHelperDigest $reopenDigest -FeatureHelperDigest $featureDigest -StudioHelperDigest $studioDigest -TimeoutSeconds $(if($case -in @('guide-lifecycle','guide-features')){1800}else{1000})
        }
    } else {
        $remoteAction = $Action.ToLowerInvariant()
        $command = "python3 $guestDriver $remoteAction --run-id $RunId"
        if ($Storage) { $command += ' --storage' }
        if ($GuideDiagnostics) { $command += ' --guide-diagnostics' }
        if ($Action -eq 'Test') { $command += " --suite $Suite" }
        if ($ReuseFullRun) { $command += " --reuse-full-run $ReuseFullRun" }
        if ($Action -eq 'Diagnose') { $command += " --native-app $NativeApp" }
        if ($Action -eq 'Configure') { $command += " --configure-target $ConfigureTarget" }
        if ($Action -eq 'Configure' -and $ConfigureTarget -eq 'artifacts') {
            $maintenanceFiles = @('tools/vm-oracle.py','tests/test_vm_oracle.py','tools/home-reference-acceptance.py','tests/tst_home_navigation.cpp','tools/guide_features.py','tools/guide_lifecycle.py','tools/guide-soak-acceptance.py','qml/OnScreenKeyboard.qml','CMakeLists.txt')
            $maintenanceLines = foreach ($relative in $maintenanceFiles) {
                $hash = (Get-FileHash -LiteralPath (Join-Path $repo $relative) -Algorithm SHA256).Hash.ToLowerInvariant()
                "$hash  $relative"
            }
            $maintenanceManifest = Join-Path $local 'maintenance-files.sha256'
            [IO.File]::WriteAllText($maintenanceManifest,($maintenanceLines -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
            $maintenanceHash = (Get-FileHash -LiteralPath $maintenanceManifest -Algorithm SHA256).Hash.ToLowerInvariant()
            $bundle = "$guestHome/isolde-oracle/bin/maintenance-$driverHash-$maintenanceHash"
            $null = Invoke-OracleProcess -File ssh.exe -Arguments ($sshArgs + "mkdir -p $bundle/tools $bundle/tests $bundle/qml") -TimeoutSeconds 25 -Log $transport
            foreach ($relative in $maintenanceFiles) {
                $null = Invoke-OracleProcess -File scp.exe -Arguments ($scpArgs + @((Join-Path $repo $relative),"isolde-dev@127.0.0.1:$bundle/$relative")) -TimeoutSeconds 30 -Log $transport
            }
            $null = Invoke-OracleProcess -File scp.exe -Arguments ($scpArgs + @($maintenanceManifest,"isolde-dev@127.0.0.1:$bundle/files.sha256")) -TimeoutSeconds 30 -Log $transport
            $command += " --maintenance-tests-digest $maintenanceHash"
        }
        if ($WaitSeconds) { $command += " --wait-seconds $WaitSeconds" }
        $seconds = if ($Action -eq 'Provision') { 2400 } elseif ($Action -eq 'Test') { 1800 } elseif ($Action -eq 'Deploy') { 180 } elseif ($Action -eq 'Configure' -and $ConfigureTarget -eq 'android') { 3900 } elseif ($Action -eq 'Configure' -and $ConfigureTarget -eq 'games') { 2500 } elseif ($Action -eq 'Configure') { 1100 } elseif ($Action -eq 'Diagnose') { 90 } else { 45 }
        $launched = $Action -in @('Test','Deploy')
        $seconds += $WaitSeconds
        $actionResult = Invoke-OracleProcess -File ssh.exe -Arguments ($sshArgs + $command) -TimeoutSeconds $seconds -Log (Join-Path $local "$remoteAction.log")
        if ($Capture) {
            $stateLine = $actionResult.Lines | Where-Object { $_.StartsWith('{') } | Select-Object -Last 1
            if (!$stateLine) { throw 'No guest guardian status returned.' }
            $captureState = $stateLine | ConvertFrom-Json
            if ($captureState.runId -ne $RunId -or $captureState.state -ne 'running' -or $captureState.action -ne 'configure' -or $captureState.stage -ne 'native-configure-policy' -or !$captureState.guardianAlive) { throw 'Capture requires this live setup guardian.' }
            $capturePath = Join-Path $local ('setup-status-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.png')
            $null = Invoke-OracleProcess -File 'C:/Program Files/Oracle/VirtualBox/VBoxManage.exe' -Arguments @('controlvm','Isolde-Test','screenshotpng',$capturePath) -TimeoutSeconds 20 -Log (Join-Path $local 'setup-capture.log')
            Write-Host "Captured $capturePath"
        }
    }
} catch {
    $exitCode = 1
    @{ runId=$RunId; action=$Action; passed=$false; error=$_.Exception.Message;
       next='Stop. Read this result and Status. Do not loop, reset the VM, delete locks or launch another build.' } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $local 'host-failure.json')
    Write-Host "FAIL: $($_.Exception.Message)"
    if ($launched) {
        $null = Invoke-OracleProcess -File ssh.exe -Arguments ($sshArgs + "python3 $guestDriver cancel --run-id $RunId") `
            -TimeoutSeconds 30 -Log $transport -AllowFailure
    }
} finally {
    if ($guestRun -and $Action -ne 'Status') {
        try {
            $report = Join-Path $local 'report.tar.gz'
            $fetch = Invoke-OracleProcess -File scp.exe -Arguments ($scpArgs + @("isolde-dev@127.0.0.1:$guestRun/report.tar.gz",$report)) `
                -TimeoutSeconds 45 -Log $transport -AllowFailure
            if ($fetch.ExitCode -eq 0) {
                $reportDir = Join-Path $local 'report'
                New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
                $null = Invoke-OracleProcess -File tar.exe -Arguments @('-xzf',$report,'-C',$reportDir) -TimeoutSeconds 30 -Log $transport
                foreach ($bundle in Get-ChildItem -LiteralPath $reportDir -Filter 'evidence-*.tar.gz') {
                    $destination = Join-Path $reportDir $bundle.Name.Replace('.tar.gz','')
                    New-Item -ItemType Directory -Force -Path $destination | Out-Null
                    $null = Invoke-OracleProcess -File tar.exe -Arguments @('-xzf',$bundle.FullName,'-C',$destination) -TimeoutSeconds 30 -Log $transport
                }
            }
        } catch { Write-Host "Evidence retrieval failed: $($_.Exception.Message). Guest evidence retained at $guestRun" }
    }
    Write-Host "RUN_ID=$RunId"
    Write-Host "EVIDENCE=$local"
}
if ($exitCode) { throw "VM $Action failed. See $local/host-failure.json. Do not retry without diagnosing the named failure." }
