# Authoring tool: derive explicit SVG foreground resources without touching originals.
$ErrorActionPreference='Stop'
$iconRoot=Join-Path $PSScriptRoot '../config/app-icons'
$foregroundRoot=Join-Path $iconRoot 'foreground'
$null=New-Item -ItemType Directory -Force -Path $foregroundRoot
$roles=[ordered]@{}
foreach($file in Get-ChildItem -LiteralPath $iconRoot -Filter '*.svg'){
    $original=[IO.File]::ReadAllText($file.FullName).Replace("`r`n","`n")
    $plate=[regex]::Match($original,'^(<svg\b[^>]*>)<rect x="2" y="2" width="60" height="60" rx="13" fill="(#[0-9A-Fa-f]{6})"/>')
    $foreground=if($plate.Success){$original.Remove(0,$plate.Length).Insert(0,$plate.Groups[1].Value)}else{$original}
    $colour=[regex]::Match($foreground,'<path\b[^>]*\bfill="(#[0-9A-Fa-f]{3,6})"')
    if(-not $colour.Success){$colour=[regex]::Match($foreground,'<g\b[^>]*\bfill="(#[0-9A-Fa-f]{3,6})"')}
    if(-not $colour.Success){$colour=[regex]::Match($foreground,'\bstroke="(#[0-9A-Fa-f]{3,6})"')}
    if(-not $colour.Success){throw "No explicit ink colour in $($file.Name)"}
    $exclude=@();if($plate.Success){$exclude+= $plate.Groups[2].Value}
    # This source's separate white polygon fills an intentional play-shaped hole.
    # The same rule covers any future explicitly white polygon detail.
    if($foreground -match '<polygon\b[^>]*fill="#fff(?:fff)?"'){$exclude+='#FFFFFF'}
    $target=Join-Path $foregroundRoot $file.Name
    [IO.File]::WriteAllText($target,$foreground,[Text.UTF8Encoding]::new($false))
    $backdrop=if($colour.Groups[1].Value -match '^#(?:fff|(?:[DE-F][0-9A-F]){3})$' -and $plate.Success){$plate.Groups[2].Value}else{''}
    $sourceHash=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($original))).ToLowerInvariant()
    $roles[$file.BaseName]=[ordered]@{foreground='foreground/'+$file.Name;inkColour=$colour.Groups[1].Value;inkExclude=@($exclude|Select-Object -Unique);opticalScale=1.0;contrastBackdrop=$backdrop;foregroundSha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant();originalSha256=$sourceHash}
}
$manifest=[ordered]@{schemaVersion=1;derivation='isolde-icon-roles-v1';icons=$roles}
[IO.File]::WriteAllText((Join-Path $iconRoot 'roles.json'),($manifest|ConvertTo-Json -Depth 6)+[Environment]::NewLine,[Text.UTF8Encoding]::new($false))
