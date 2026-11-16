# DCS-BIOS DateModule

By Nov 2026, DCS-BIOS does currently not support exporting the current Mission Date natively. 
As a temporary workaround this additional DCS-BIOS module was created 

# How it works

There seems to be no ***"DCS Export API"*** call to get the current mission date. Instead the ***"Control API"** call `DCS.getCurrentMission()` is used to get the current missions date and writes it into a temporary log file.`Scripts\DCS-BIOS-Extensions\MissionDate.txtScripts\DCS-BIOS-Extensions\MissionDate.txt` (see \Hooks\MissionDateHook.lua). 
A custom DCS-BIOS module ***"MissionDate.lua"*** is injected into DCS-BIOS\lib\modules\common_modules\CommonData.lua which reads the current mission date from the log file and exports :
- MISSION_YEAR
- MISSION_MONTH
- MISSION_DAY

via DCS-BIOS. 

This values can be consumed by standard DCS-Bios Arduino Code. 

# How to install 

- Download repository
- Open PowerShell in <REPO_Path>\DCS-BIOS_DateModule
- execute .\Install-MissionDate.ps1
   - .EXAMPLE
     .\Install-MissionDate.ps1
     
     .\Install-MissionDate.ps1 -DcsSavedGamesPath "C:\Users\me\Saved Games\DCS.openbeta"
     
     .\Install-MissionDate.ps1 -Uninstall
- Start/Restart DCS World
- Optional:
   -   Check "DCS-BIOS PATH\doc\Addresses.h" for newly added Date attributes
   -   Check BORT und "Common Data" for newly added Date attributes

