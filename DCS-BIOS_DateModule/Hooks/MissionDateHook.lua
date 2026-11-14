-- MissionDateHook.lua
-- DCS Hook-side companion for the DCS-BIOS MissionDate extension.
-- Install this file in Saved Games\DCS\Scripts\Hooks.
--
-- DCS.getCurrentMission() is accessed here, in the Hook environment.
-- The Export.lua-side DCS-BIOS module reads the resulting text file.

local DATE_FILE = lfs.writedir() .. [[Scripts\DCS-BIOS-Extensions\MissionDate.txt]]
local TEMP_FILE = DATE_FILE .. ".tmp"

local function logMessage(message)
    if log and log.write then
        log.write("MissionDate", log.INFO, tostring(message))
    end
end

local function writeMissionDate()
    local ok, current = pcall(function()
        return DCS.getCurrentMission()
    end)

    if not ok then
        logMessage("DCS.getCurrentMission failed: " .. tostring(current))
        os.remove(DATE_FILE)
        return
    end

    if type(current) ~= "table"
        or type(current.mission) ~= "table"
        or type(current.mission.date) ~= "table" then
        logMessage("Mission date is not available")
        os.remove(DATE_FILE)
        return
    end

    local date = current.mission.date
    local year = tonumber(date.Year)
    local month = tonumber(date.Month)
    local day = tonumber(date.Day)

    if not year or not month or not day
        or year < 1 or year > 65535
        or month < 1 or month > 12
        or day < 1 or day > 31 then
        logMessage("Mission date contains invalid values")
        os.remove(DATE_FILE)
        return
    end

    -- Write to a temporary file first so the Export environment never reads
    -- a partially-written date.
    local file, err = io.open(TEMP_FILE, "w")
    if not file then
        logMessage("Cannot open temporary date file: " .. tostring(err))
        return
    end

    file:write(string.format("%d,%d,%d\n",
        math.floor(year), math.floor(month), math.floor(day)))
    file:close()

    os.remove(DATE_FILE)
    local renamed, renameErr = os.rename(TEMP_FILE, DATE_FILE)
    if not renamed then
        logMessage("Cannot publish mission date file: " .. tostring(renameErr))
        return
    end

    logMessage(string.format("MISSION_DATE=%04d-%02d-%02d",
        year, month, day))
end

local callbacks = {}

function callbacks.onMissionLoadBegin()
    local ok, err = pcall(writeMissionDate)
    if not ok then
        logMessage("Error exporting mission date: " .. tostring(err))
    end
end

DCS.setUserCallbacks(callbacks)
logMessage("Hook loaded")
