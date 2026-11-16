param(
    [Parameter(Mandatory=$false)]
    [string[]]$Paths = @("src"),
    [Parameter(Mandatory=$false)]
    [int]$CyclomaticComplexity = 10,
    [Parameter(Mandatory=$false)]
    [int]$FunctionLength = 60,
    [Parameter(Mandatory=$false)]
    [int]$Parameters = 5,
    [Parameter(Mandatory=$false)]
    [switch]$FailOnViolation = $true
)

$ErrorActionPreference = "Stop"

$lizardExe = Get-Command "python.exe" -ErrorAction SilentlyContinue
if (-not $lizardExe) {
    Write-Error "python.exe not found on PATH. Install Python to run Lizard."
    exit 1
}

$lizardModule = "lizard"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot = Resolve-Path (Join-Path $scriptDir "..")

$allPaths = @()
foreach ($p in $Paths) {
    $resolved = Resolve-Path (Join-Path $repoRoot $p) -ErrorAction SilentlyContinue
    if ($resolved) {
        $allPaths += $resolved.Path
    } else {
        Write-Warning "Path not found: $p"
    }
}

if ($allPaths.Count -eq 0) {
    Write-Error "No valid source paths found."
    exit 1
}

$args = @("-l", "cpp", "-C", $CyclomaticComplexity, "-L", $FunctionLength, "-a", $Parameters) + $allPaths

Write-Host "Running: python -m lizard $($args -join ' ')"
$output = python -m lizard $args 2>&1
Write-Output $output

$exitCode = $LASTEXITCODE
if ($exitCode -ne 0 -and $FailOnViolation) {
    Write-Host "Lizard check FAILED with exit code $exitCode (complexity/function-length/parameter limits exceeded)."
    exit 1
}

Write-Host "Lizard check passed."
exit 0
