<#
.SYNOPSIS
  Installs (or removes) the MissionDate extension for DCS-BIOS.

.DESCRIPTION
  - copies MissionDate.lua  to Saved Games\DCS\Scripts\DCS-BIOS-Extensions\
  - copies the hook         to Saved Games\DCS\Scripts\Hooks\
  - adds ONE guarded call to DCS-BIOS\lib\modules\common_modules\CommonData.lua (just before its
    final "return"), which defines MISSION_YEAR / MISSION_MONTH / MISSION_DAY on the CommonData
    module. No new module and no new base address are needed, so the Arduino names are
    CommonData_MISSION_YEAR etc.
  - a backup of CommonData.lua is made before it is changed.

  A DCS-BIOS update overwrites CommonData.lua. Run this script again afterwards. It is safe to
  run repeatedly.

  If an earlier version of this extension registered a separate module in BIOS.lua, that entry
  is removed automatically.

.EXAMPLE
  .\Install-MissionDate.ps1
  .\Install-MissionDate.ps1 -DcsSavedGamesPath "C:\Users\me\Saved Games\DCS.openbeta"
  .\Install-MissionDate.ps1 -Uninstall
#>

param(
    [string]$DcsSavedGamesPath = (Join-Path $env:USERPROFILE "Saved Games\DCS"),
    [switch]$Uninstall
)

$ErrorActionPreference = "Stop"

$scriptsPath = Join-Path $DcsSavedGamesPath "Scripts"
$commonDataPath = Join-Path $scriptsPath "DCS-BIOS\lib\modules\common_modules\CommonData.lua"
$extensionPath = Join-Path $scriptsPath "DCS-BIOS-Extensions"
$hooksPath = Join-Path $scriptsPath "Hooks"

$modules = @(
    @{
        Name = "MissionDate"
        Source = "MissionDate.lua"
        HookSource = "Hooks\MissionDateHook.lua"
    },
    @{
        Name = "Map"
        Source = "Map.lua"
        HookSource = "Hooks\MapHook.lua"
    }
)

if (-not (Test-Path $commonDataPath)) {
    throw "CommonData.lua not found: $commonDataPath`nPass the correct Saved Games path with -DcsSavedGamesPath."
}

$commonDataText = [System.IO.File]::ReadAllText($commonDataPath)

if ($Uninstall) {
    $updated = $commonDataText

    foreach ($module in $modules) {
        $requireLine = 'require("Scripts.DCS-BIOS-Extensions.' + $module.Name + '")(CommonData)'

        $updated = $updated.Replace("    " + $requireLine + "`r`n", "")
        $updated = $updated.Replace("    " + $requireLine + "`n", "")
        $updated = $updated.Replace($requireLine + "`r`n", "")
        $updated = $updated.Replace($requireLine + "`n", "")
    }

    if ($updated -ne $commonDataText) {
        $backupPath = "$commonDataPath.backup.DCSBIOS-Extensions.$(Get-Date -Format yyyyMMdd-HHmmss)"
        Copy-Item $commonDataPath $backupPath

        [System.IO.File]::WriteAllText(
            $commonDataPath,
            $updated,
            [System.Text.UTF8Encoding]::new($false)
        )

        Write-Host "Removed extension registrations from CommonData.lua."
        Write-Host "Backup created: $backupPath"
    } else {
        Write-Host "No extension registrations found in CommonData.lua."
    }

    foreach ($module in $modules) {
        Remove-Item (Join-Path $extensionPath $module.Source) -Force -ErrorAction SilentlyContinue
        Remove-Item (Join-Path $hooksPath ($module.Name + "Hook.lua")) -Force -ErrorAction SilentlyContinue
        Remove-Item (Join-Path $extensionPath ($module.Name + ".txt")) -Force -ErrorAction SilentlyContinue
        Remove-Item (Join-Path $extensionPath ($module.Name + ".txt.tmp")) -Force -ErrorAction SilentlyContinue

        Write-Host "Removed $($module.Name) extension files."
    }

    Write-Host "DCS-BIOS itself was not modified or removed."
    exit 0
}

foreach ($module in $modules) {
    if (-not (Test-Path (Join-Path $PSScriptRoot $module.Source))) {
        throw "$($module.Source) must be in the same folder as this installer."
    }

    if (-not (Test-Path (Join-Path $PSScriptRoot $module.HookSource))) {
        throw "$($module.HookSource) is missing from the package."
    }
}

New-Item -ItemType Directory -Force -Path $extensionPath | Out-Null
New-Item -ItemType Directory -Force -Path $hooksPath | Out-Null

foreach ($module in $modules) {
    Copy-Item (Join-Path $PSScriptRoot $module.Source) `
        (Join-Path $extensionPath $module.Source) -Force

    Copy-Item (Join-Path $PSScriptRoot $module.HookSource) `
        (Join-Path $hooksPath ($module.Name + "Hook.lua")) -Force

    Write-Host "Installed $($module.Name) files."
}

$updated = $commonDataText
$registrationChanged = $false

foreach ($module in $modules) {
    $requireLine = '    require("Scripts.DCS-BIOS-Extensions.' + $module.Name + '")(CommonData)'

    if ($updated -notmatch [regex]::Escape($requireLine.Trim())) {
        # Insert immediately before the final return statement.
        $pattern = '(?m)^([ \t]*return[ \t]+CommonData[ \t]*)$'

        if (-not [regex]::IsMatch($updated, $pattern)) {
            throw "Could not find the final 'return CommonData' in CommonData.lua. No CommonData.lua changes were made."
        }

        $updated = [regex]::Replace(
            $updated,
            $pattern,
            ($requireLine + "`r`n" + '$1'),
            1
        )

        $registrationChanged = $true
        Write-Host "Registered $($module.Name) in CommonData.lua."
    } else {
        Write-Host "$($module.Name) is already registered in CommonData.lua."
    }
}

if ($registrationChanged) {
    $backupPath = "$commonDataPath.backup.DCSBIOS-Extensions.$(Get-Date -Format yyyyMMdd-HHmmss)"
    Copy-Item $commonDataPath $backupPath

    [System.IO.File]::WriteAllText(
        $commonDataPath,
        $updated,
        [System.Text.UTF8Encoding]::new($false)
    )

    Write-Host "Backup created: $backupPath"
}

Write-Host ""
Write-Host "DCS-BIOS extensions installed."
Write-Host "Hooks:   $hooksPath"
Write-Host "Modules: $extensionPath"
Write-Host ""
Write-Host "Restart DCS and load a mission."
Write-Host "Check dcs.log for [MissionDate] and [Map] messages."
