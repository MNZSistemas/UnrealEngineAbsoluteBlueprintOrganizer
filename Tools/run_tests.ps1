# Copyright (c) 2026 MNZ Sistemas. All rights reserved.
<#
.SYNOPSIS
  Runs the "Ueabo" automation tests headless in the per-engine test project and prints the totals.
  Build and install first: Tools\build_plugin.ps1 -Engine <v>

.EXAMPLE
  powershell -NoProfile -File Tools\run_tests.ps1 -Engine 5.8
#>
param(
    [Parameter(Mandatory = $true)][ValidateSet("5.8", "5.7", "5.6", "5.5", "5.0", "4.27")][string]$Engine,
    [string]$Filter = "Ueabo"
)
$ErrorActionPreference = "Stop"
$Tag = $Engine -replace "\.", ""
$EngineRoot = "C:\Program Files\Epic Games\UE_$Engine"
$ProjectDir = "D:\GameEngineProjects\Unreal\_ueabo_test$Tag"
$Project = Join-Path $ProjectDir "UeaboTest.uproject"
if (-not (Test-Path $Project)) {
    New-Item -ItemType Directory -Force $ProjectDir | Out-Null
    $assoc = if ($Engine -eq "4.27") { "4.27" } else { $Engine }
    $json = '{ "FileVersion": 3, "EngineAssociation": "' + $assoc + '", "Category": "", "Description": "", "Plugins": [ { "Name": "UnrealEngineAbsoluteBlueprintOrganizer", "Enabled": true } ] }'
    [IO.File]::WriteAllText($Project, $json, (New-Object System.Text.UTF8Encoding($false)))
}
$Exe = if ($Engine -eq "4.27") { "$EngineRoot\Engine\Binaries\Win64\UE4Editor-Cmd.exe" } else { "$EngineRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" }
$Log = "D:\UEABuild\ueabo\test_$Tag.log"
$ErrorActionPreference = "Continue"
& $Exe "$Project" "-ExecCmds=Automation RunTests $Filter" -unattended -nopause -nosplash -nullrhi "-testexit=Automation Test Queue Empty" "-abslog=$Log" -log > $null 2>&1
$ErrorActionPreference = "Stop"
$lines = Select-String -Path $Log -Pattern "Test Completed\. Result=\{(\w+)\}.*Name=\{([^}]*)\}"
$pass = ($lines | Where-Object { $_.Matches[0].Groups[1].Value -match "^(Success|Passed)$" } | Measure-Object).Count
$fail = ($lines | Where-Object { $_.Matches[0].Groups[1].Value -notmatch "^(Success|Passed)$" } | Measure-Object).Count
Write-Host "UE $Engine tests: passed $pass, failed $fail (log: $Log)"
$pluginIssues = Select-String -Path $Log -Pattern "(LogPluginManager|LogModuleManager|LogUeabo\w*): (Error|Warning)"
Write-Host "plugin/module log errors+warnings: $(($pluginIssues | Measure-Object).Count)"
$pluginIssues | Select-Object -First 10 | ForEach-Object { $_.Line }
if ($fail -gt 0 -or $pass -eq 0) {
    Select-String -Path $Log -Pattern "LogAutomationController: Error|Error: .*Ueabo|LogUeabo.*Error|Fatal error" | Select-Object -First 40 | ForEach-Object { $_.Line }
    exit 1
}
exit 0
