# Copyright (c) 2026 MNZ Sistemas. All rights reserved.
<#
.SYNOPSIS
  Sync -> BuildPlugin -> (optionally) install into a test project, printing only errors and totals.

.EXAMPLE
  powershell -NoProfile -File Tools\build_plugin.ps1 -Engine 5.8 -NoInstall
  powershell -NoProfile -File Tools\build_plugin.ps1 -Engine 4.27            # also installs into D:\GameEngineProjects\Unreal\_ueabo_test427
  powershell -NoProfile -File Tools\build_plugin.ps1 -Engine 5.8 -Package    # also writes the Fab source zip

Run one engine at a time. On a hang, kill UnrealHeaderTool/UnrealBuildTool/AutomationTool and rerun.
#>
param(
    [Parameter(Mandatory = $true)][ValidateSet("5.8", "5.7", "5.6", "5.5", "5.0", "4.27")][string]$Engine,
    [switch]$NoInstall,
    [switch]$Package,
    [string]$ScratchRoot = "D:\UEABuild\ueabo",
    [string]$ZipRoot = "D:\UEABuild\FabUEABO"
)

$ErrorActionPreference = "Stop"
$Name = "UnrealEngineAbsoluteBlueprintOrganizer"
$Repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$IsUE4 = $Engine -eq "4.27"
$EngineRoot = "C:\Program Files\Epic Games\UE_$Engine"
$Tag = $Engine -replace "\.", ""
$Src = Join-Path $ScratchRoot "src$Tag\$Name"
$Out = Join-Path $ScratchRoot "out$Tag"
$Log = Join-Path $ScratchRoot "build_$Tag.log"

function Mirror($From, $To) {
    & robocopy $From $To /MIR /NFL /NDL /NJH /NJS | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE): $From -> $To" }
}

# Descriptor for the target engine: version stamp, and the platform list key 4.27 understands.
function Write-Descriptor($Dest) {
    $u = Get-Content (Join-Path $Repo "$Name.uplugin") -Raw
    $u = $u -replace '"EngineVersion":\s*"[^"]*"', ('"EngineVersion": "' + $Engine + '.0"')
    if ($IsUE4) { $u = $u -replace 'PlatformAllowList', 'WhitelistPlatforms' }
    [IO.File]::WriteAllText($Dest, $u, (New-Object System.Text.UTF8Encoding($false)))
}

# 1. Sync the scratch copy and verify it by hash.
New-Item -ItemType Directory -Force $Src | Out-Null
foreach ($d in @("Source", "Resources", "Config")) { if (Test-Path (Join-Path $Repo $d)) { Mirror (Join-Path $Repo $d) (Join-Path $Src $d) } }
foreach ($f in @("README.md", "LICENSE")) { if (Test-Path (Join-Path $Repo $f)) { Copy-Item -Force (Join-Path $Repo $f) $Src } }
Write-Descriptor (Join-Path $Src "$Name.uplugin")
$mismatch = 0
Get-ChildItem (Join-Path $Repo "Source") -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring((Join-Path $Repo "Source").Length)
    $other = Join-Path (Join-Path $Src "Source") $rel
    if (-not (Test-Path $other) -or (Get-FileHash $_.FullName).Hash -ne (Get-FileHash $other).Hash) { $mismatch++ }
}
if ($mismatch -gt 0) { throw "scratch copy differs from the repo in $mismatch file(s)" }
Write-Host "sync ok ($Src)"

# 2. Build.
if (Test-Path $Out) { [IO.Directory]::Delete($Out, $true) }
$PluginFile = Join-Path $Src "$Name.uplugin"
$Extra = @()
if ($Engine -eq "5.0") {
    # UE 5.0's RunUAT needs the engine's bundled .NET runtime, and -StrictIncludes avoids its shared PCHs on a current MSVC.
    $Bundled = "$EngineRoot\Engine\Binaries\ThirdParty\DotNet\Windows"
    $env:PATH = "$Bundled;$env:PATH"; $env:DOTNET_ROOT = $Bundled; $env:DOTNET_MULTILEVEL_LOOKUP = "0"
    $Extra += "-StrictIncludes"
}
$ok = $false
for ($attempt = 1; $attempt -le 40; $attempt++) {
    $ErrorActionPreference = "Continue"
    if ($IsUE4) {
        & "$EngineRoot\Engine\Binaries\DotNET\AutomationTool.exe" BuildPlugin -Plugin="$PluginFile" -Package="$Out" -TargetPlatforms=Win64 -Rocket > $Log 2>&1
    } else {
        & "$EngineRoot\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="$PluginFile" -Package="$Out" -TargetPlatforms=Win64 -Rocket @Extra > $Log 2>&1
    }
    $ErrorActionPreference = "Stop"
    $ok = Select-String -Path $Log -Pattern "BUILD SUCCESSFUL" -Quiet
    $busy = Select-String -Path $Log -Pattern "conflicting instance of AutomationTool|already running" -Quiet
    if ($ok -or -not $busy) { break }
    Write-Host "another AutomationTool is running; waiting 30 s (attempt $attempt)"
    Start-Sleep 30
}
if (-not $ok) {
    Write-Host "BUILD FAILED (UE $Engine) - log: $Log"
    Select-String -Path $Log -Pattern "error C\d+|error LNK|Error:|fatal error" | Select-Object -First 40 | ForEach-Object { $_.Line.Substring([Math]::Max(0, $_.Line.IndexOf("Source\"))) }
    exit 1
}
$warn = (Select-String -Path $Log -Pattern "warning C\d+" | Measure-Object).Count
Write-Host "BUILD SUCCESSFUL (UE $Engine), compiler warnings: $warn"

# 3. Fab source zip (Source, Resources, Config, descriptor; no binaries).
if ($Package) {
    New-Item -ItemType Directory -Force $ZipRoot | Out-Null
    $Stage = Join-Path $ScratchRoot "zip$Tag\$Name"
    if (Test-Path (Split-Path $Stage)) { [IO.Directory]::Delete((Split-Path $Stage), $true) }
    New-Item -ItemType Directory -Force $Stage | Out-Null
    foreach ($d in @("Source", "Resources", "Config")) { if (Test-Path (Join-Path $Src $d)) { Mirror (Join-Path $Src $d) (Join-Path $Stage $d) } }
    foreach ($f in @("README.md", "LICENSE")) { if (Test-Path (Join-Path $Src $f)) { Copy-Item -Force (Join-Path $Src $f) $Stage } }
    Copy-Item -Force $PluginFile $Stage
    $Zip = Join-Path $ZipRoot "${Name}_1.0.0_UE$Engine.zip"
    if (Test-Path $Zip) { [IO.File]::Delete($Zip) }
    Compress-Archive -Path $Stage -DestinationPath $Zip
    Write-Host "zip -> $Zip"
}
if ($NoInstall) { exit 0 }

# 4. Install into the per-engine test project's Plugins folder.
$Project = "D:\GameEngineProjects\Unreal\_ueabo_test$Tag"
$EditorProc = if ($IsUE4) { "UE4Editor*" } else { "UnrealEditor*" }
Get-Process | Where-Object { $_.ProcessName -like $EditorProc -and $_.Path -and $_.Path.StartsWith("$EngineRoot\", [StringComparison]::OrdinalIgnoreCase) } | Stop-Process -Force -Confirm:$false -ErrorAction SilentlyContinue
$Dest = Join-Path $Project "Plugins\$Name"
New-Item -ItemType Directory -Force $Dest | Out-Null
& robocopy $Out $Dest /MIR /NFL /NDL /NJH /NJS /XD HostProject | Out-Null
if ($LASTEXITCODE -ge 8) { throw "install failed" }
Copy-Item -Force $PluginFile (Join-Path $Dest "$Name.uplugin")
Write-Host "installed -> $Dest"
exit 0
