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

$scriptsPath   = Join-Path $DcsSavedGamesPath "Scripts"
$biosPath      = Join-Path $scriptsPath "DCS-BIOS\BIOS.lua"
$commonPath    = Join-Path $scriptsPath "DCS-BIOS\lib\modules\common_modules\CommonData.lua"
$extensionPath = Join-Path $scriptsPath "DCS-BIOS-Extensions"
$hooksPath     = Join-Path $scriptsPath "Hooks"
$modulePath    = Join-Path $extensionPath "MissionDate.lua"
$hookPath      = Join-Path $hooksPath "MissionDateHook.lua"

$startMarker = '-- MissionDate extension (auto-added by Install-MissionDate.ps1)'
$endMarker   = '-- end MissionDate extension'
$utf8NoBom   = New-Object System.Text.UTF8Encoding($false)

function Backup-File([string]$path) {
    $backup = "$path.backup.MissionDate.$(Get-Date -Format yyyyMMdd-HHmmss)"
    Copy-Item $path $backup
    return $backup
}

# Removes the registration that an earlier version of this extension added to BIOS.lua.
function Remove-LegacyRegistration {
    if (-not (Test-Path $biosPath)) { return }
    $text = [System.IO.File]::ReadAllText($biosPath)
    $pattern = '(?m)^[ \t]*require\("Scripts\.DCS-BIOS-Extensions\.MissionDate"\),[ \t]*\r?\n'
    if ([regex]::IsMatch($text, $pattern)) {
        $backup = Backup-File $biosPath
        [System.IO.File]::WriteAllText($biosPath, [regex]::Replace($text, $pattern, ''), $utf8NoBom)
        Write-Host "Removed the old MissionDate registration from BIOS.lua. Backup: $backup"
    }
}

if ($Uninstall) {
    Remove-LegacyRegistration

    if (Test-Path $commonPath) {
        $text = [System.IO.File]::ReadAllText($commonPath)
        $pattern = '(?s)[ \t]*' + [regex]::Escape($startMarker) + '.*?' + [regex]::Escape($endMarker) + '[ \t]*\r?\n?'
        if ([regex]::IsMatch($text, $pattern)) {
            $backup = Backup-File $commonPath
            [System.IO.File]::WriteAllText($commonPath, [regex]::Replace($text, $pattern, ''), $utf8NoBom)
            Write-Host "Removed the MissionDate block from CommonData.lua. Backup: $backup"
        } else {
            Write-Host "No MissionDate block found in CommonData.lua."
        }
    }

    Remove-Item $modulePath -Force -ErrorAction SilentlyContinue
    Remove-Item $hookPath -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $extensionPath "MissionDate.txt") -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $extensionPath "MissionDate.txt.tmp") -Force -ErrorAction SilentlyContinue
    Write-Host "Removed MissionDate extension files. DCS-BIOS itself was not removed."
    exit 0
}

# ---- Install ----------------------------------------------------------------

if (-not (Test-Path $commonPath)) {
    throw "CommonData.lua not found: $commonPath`nThis extension needs the module layout (lib\modules\common_modules). Pass the correct Saved Games folder with -DcsSavedGamesPath."
}
if (-not (Test-Path (Join-Path $PSScriptRoot "MissionDate.lua"))) {
    throw "MissionDate.lua must be in the same folder as this installer."
}
if (-not (Test-Path (Join-Path $PSScriptRoot "Hooks\MissionDateHook.lua"))) {
    throw "Hooks\MissionDateHook.lua is missing from the package."
}

$text = [System.IO.File]::ReadAllText($commonPath)
$alreadyPatched = $text.Contains($startMarker)

# Find the module variable in the final "return <name>" of CommonData.lua before changing anything.
$moduleVar = $null
$returnMatch = $null
if (-not $alreadyPatched) {
    $returnMatches = [regex]::Matches($text, '(?m)^return[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]*\r?$')
    if ($returnMatches.Count -eq 0) {
        throw "Could not find the final 'return <module>' line in CommonData.lua. No files were changed."
    }
    $returnMatch = $returnMatches[$returnMatches.Count - 1]
    $moduleVar = $returnMatch.Groups[1].Value
}

New-Item -ItemType Directory -Force -Path $extensionPath | Out-Null
New-Item -ItemType Directory -Force -Path $hooksPath | Out-Null
Copy-Item (Join-Path $PSScriptRoot "MissionDate.lua") $modulePath -Force
Copy-Item (Join-Path $PSScriptRoot "Hooks\MissionDateHook.lua") $hookPath -Force

Remove-LegacyRegistration

if ($alreadyPatched) {
    Write-Host "CommonData.lua already contains the MissionDate block."
} else {
    $nl = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $blockLines = @(
        $startMarker,
        'do',
        '  local ok, err = pcall(function() require("Scripts.DCS-BIOS-Extensions.MissionDate")(' + $moduleVar + ') end)',
        '  if not ok and log and log.write then log.write("MissionDate", log.ERROR, tostring(err)) end',
        'end',
        $endMarker
    )
    $block = ($blockLines -join $nl) + $nl

    $backup = Backup-File $commonPath
    $patched = $text.Substring(0, $returnMatch.Index) + $block + $text.Substring($returnMatch.Index)
    [System.IO.File]::WriteAllText($commonPath, $patched, $utf8NoBom)
    Write-Host "Added the MissionDate block to CommonData.lua (module variable: $moduleVar)."
    Write-Host "Backup created: $backup"
}

Write-Host ""
Write-Host "MissionDate installed."
Write-Host "Hook:      $hookPath"
Write-Host "Extension: $modulePath"
Write-Host ""
Write-Host "Next: restart DCS and load a mission. Then look up CommonData -> MISSION_YEAR / MISSION_MONTH / MISSION_DAY"
Write-Host "in the DCS-BIOS Reference Tool and enter address, mask and shift in the firmware."
Write-Host "Check dcs.log for [MissionDate] messages and DCS-BIOS.log for errors."
