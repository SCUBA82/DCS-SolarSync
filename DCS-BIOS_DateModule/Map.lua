
-- Map.lua
-- DCS-BIOS extension: exports the current DCS map as
--   CommonData_MAP_ID
--
-- The Hook-side companion writes the numeric map ID to MAP.txt.
-- The file is read at most once per second.

local MAP_FILE = lfs.writedir() .. [[Scripts\DCS-BIOS-Extensions\MAP.txt]]

local mapId = 0
local lastReadSecond = nil

local function refresh()
    local now = (os and os.time and os.time()) or 0

    if lastReadSecond ~= nil and now == lastReadSecond then
        return
    end

    lastReadSecond = now

    local ok = pcall(function()
        local file = io.open(MAP_FILE, "r")

        if not file then
            mapId = 0
            return
        end

        local contents = file:read("*l")
        file:close()

        local value = tonumber(contents)

        if value and value >= 0 and value <= 13 then
            mapId = math.floor(value)
        else
            mapId = 0
        end
    end)

    if not ok then
        mapId = 0
    end
end

return function(module)
    module:defineIntegerFromGetter(
        "MAP_ID",
        function()
            refresh()
            return mapId
        end,
        13,
        "Metadata",
        "Current DCS map ID (0 = unknown)"
    )
end
