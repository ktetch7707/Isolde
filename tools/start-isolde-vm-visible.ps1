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

# Check the existing VM before launching so a running graphical session stays intact.
$info = & $vboxManage showvminfo $vmName --machinereadable
if ($LASTEXITCODE -ne 0) {
    throw "Could not read the state of $vmName."
}
$stateLine = $info | Where-Object { $_ -like 'VMState=*' } | Select-Object -First 1
if ($stateLine -match 'VMState="running"') {
    exit 0
}

Start-Process -FilePath $virtualBoxVm -ArgumentList @("--startvm", $vmName) -WindowStyle Normal
