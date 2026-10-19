$ErrorActionPreference = "Stop"

$vmName = "Isolde-Test"
$virtualBoxVm = "C:\Program Files\Oracle\VirtualBox\VirtualBoxVM.exe"
$vboxManage = "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe"

if (-not (Test-Path -LiteralPath $virtualBoxVm)) {
    throw "VirtualBoxVM.exe was not found at $virtualBoxVm"
}

if (-not (Test-Path -LiteralPath $vboxManage)) {
    throw "VBoxManage.exe was not found at $vboxManage"
}

$info = & $vboxManage showvminfo $vmName --machinereadable 2>$null
$stateLine = $info | Where-Object { $_ -like 'VMState=*' } | Select-Object -First 1

if ($stateLine -match 'VMState="running"') {
    exit 0
}

Start-Process -FilePath $virtualBoxVm -ArgumentList @("--startvm", $vmName) -WindowStyle Normal
