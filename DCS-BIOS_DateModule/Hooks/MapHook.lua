
-- MapHook.lua
-- Hook-side companion for the DCS-BIOS MAP extension.
--
-- Install in:
--   Saved Games\DCS\Scripts\Hooks
--
-- Writes the numeric map ID to:
--   Saved Games\DCS\Scripts\DCS-BIOS-Extensions\MAP.txt

local MAP_FILE = lfs.writedir() .. [[Scripts\DCS-BIOS-Extensions\MAP.txt]]
local TEMP_FILE = MAP_FILE .. ".tmp"

local MAP_IDS = {
    Caucasus       = 1,
    Nevada         = 2,
    Normandy       = 3,
    PersianGulf     = 4,
    TheChannel      = 5,
    Syria           = 6,
    MarianaIslands  = 7,
    Falklands       = 8,
    SinaiMap        = 9,
    Kola            = 10,
    Afghanistan     = 11,
    Iraq            = 12,
    GermanyCW       = 13
}

local function logMessage(message)
    if log and log.write then
        log.write("Map", log.INFO, tostring(message))
    end
end

local function writeMap()
    local ok, current = pcall(function()
        return DCS.getCurrentMission()
    end)

    if not ok then
        logMessage("DCS.getCurrentMission failed: " .. tostring(current))
        os.remove(MAP_FILE)
        return
    end

    if type(current) ~= "table"
        or type(current.mission) ~= "table" then
        logMessage("Mission information is not available")
        os.remove(MAP_FILE)
        return
    end

    local theatre = current.mission.theatre

    if type(theatre) ~= "string" then
        logMessage("Mission theatre is not available")
        os.remove(MAP_FILE)
        return
    end

    local id = MAP_IDS[theatre] or 0

    -- Write to a temporary file first.
    local file, err = io.open(TEMP_FILE, "w")

    if not file then
        logMessage("Cannot open temporary MAP file: " .. tostring(err))
        return
    end

    file:write(string.format("%d\n", id))
    file:close()

    os.remove(MAP_FILE)

    local renamed, renameErr = os.rename(TEMP_FILE, MAP_FILE)

    if not renamed then
        logMessage("Cannot publish MAP file: " .. tostring(renameErr))
        return
    end

    logMessage(string.format(
        "MAP_ID=%d, THEATRE=%s",
        id,
        theatre
    ))
end

local callbacks = {}

function callbacks.onMissionLoadBegin()
    local ok, err = pcall(writeMap)

    if not ok then
        logMessage("Error exporting MAP: " .. tostring(err))
    end
end

DCS.setUserCallbacks(callbacks)

logMessage("Hook loaded")
