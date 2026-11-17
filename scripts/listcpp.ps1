param([string]$Root="..")
$exts = @(".cpp",".h")
$all = Get-ChildItem -Path $Root -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $exts -contains $_.Extension }
$all | Group-Object Directory | Sort-Object Count -Descending | ForEach-Object {
  $d = $_.Name
  $name = Split-Path $d -Leaf
  Write-Output ("$($_.Count)P $name")
}
Write-Output ("TOTAL " + ($all.Count))
