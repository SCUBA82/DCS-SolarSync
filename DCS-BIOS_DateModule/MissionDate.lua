-- MissionDate.lua
-- DCS-BIOS extension: adds the mission date to the existing CommonData module as
--   MISSION_YEAR, MISSION_MONTH, MISSION_DAY
-- so that the controls are named CommonData_MISSION_YEAR etc. on the Arduino side and
-- no separate module (and no extra base address) is needed.
--
-- This file does NOT create a module. It returns a function that takes the CommonData
-- module object and defines the controls on it. Install-MissionDate.ps1 adds one call to
-- CommonData.lua, just before its final "return":
--
--   require("Scripts.DCS-BIOS-Extensions.MissionDate")(CommonData)
--
-- Data flow:
--   Hook (Saved Games\DCS\Scripts\Hooks\MissionDateHook.lua) writes
--   Saved Games\DCS\Scripts\DCS-BIOS-Extensions\MissionDate.txt  ("year,month,day")
--   and this file reads it in the Export.lua environment. DCS.getCurrentMission()
--   is not available there, which is why the file is needed.

local DATE_FILE = lfs.writedir() .. [[Scripts\DCS-BIOS-Extensions\MissionDate.txt]]

local missionYear = 0
local missionMonth = 0
local missionDay = 0
local lastReadSecond = nil

local function clear()
    missionYear, missionMonth, missionDay = 0, 0, 0
end

-- Called by the getters. Reads the file at most once per second, not on every export frame.
local function refresh()
    local now = (os and os.time and os.time()) or 0
    if lastReadSecond ~= nil and now == lastReadSecond then
        return
    end
    lastReadSecond = now

    local ok = pcall(function()
        local file = io.open(DATE_FILE, "r")
        if not file then
            clear()
            return
        end
        local contents = file:read("*l")
        file:close()

        local year, month, day
        if contents then
            year, month, day = string.match(contents, "^(%d+),(%d+),(%d+)$")
        end
        year, month, day = tonumber(year), tonumber(month), tonumber(day)

        if year and month and day
            and year >= 1 and year <= 65535
            and month >= 1 and month <= 12
            and day >= 1 and day <= 31 then
            missionYear = math.floor(year)
            missionMonth = math.floor(month)
            missionDay = math.floor(day)
        else
            clear()
        end
    end)
    if not ok then
        clear()
    end
end

return function(module)
    module:defineIntegerFromGetter(
        "MISSION_YEAR",
        function() refresh() return missionYear end,
        65535,
        "Metadata",
        "Mission date - year (0 = unavailable)"
    )

    module:defineIntegerFromGetter(
        "MISSION_MONTH",
        function() refresh() return missionMonth end,
        12,
        "Metadata",
        "Mission date - month (0 = unavailable)"
    )

    module:defineIntegerFromGetter(
        "MISSION_DAY",
        function() refresh() return missionDay end,
        31,
        "Metadata",
        "Mission date - day (0 = unavailable)"
    )
end
