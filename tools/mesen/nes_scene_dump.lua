-- One-shot NES scene dumper for Mesen 2.
-- Load this script in Mesen's script window while Zelda is running on the target scene.

-- This folder must already exist (the script does not create folders in sandboxed Lua).
local OUTPUT_DIR = "C:/ProjectsPersonal/Zelda/dumps"
local PREFIX = "first_room"

local dumped = false

local function logInfo(msg)
  emu.log("[scene-dump] " .. msg)
end

local function writeBytes(path, bytes)
  local file = io.open(path, "w+b")
  if file == nil then
    error("Unable to open file for writing: " .. path)
  end

  local chunk = {}
  for i = 1, #bytes do
    chunk[#chunk + 1] = string.char(bytes[i])
  end
  file:write(table.concat(chunk))
  file:close()
end

local function readMemoryBlock(memType, startAddr, length)
  local result = {}
  for i = 0, length - 1 do
    result[#result + 1] = emu.read(startAddr + i, memType)
  end
  return result
end

local function writeMetadata(path, spriteHeight, chrMemType, chrSize, ppuctrl)
  local frameCount = emu.getState().ppu.frameCount
  local cpuState = emu.getState().cpu
  local bgPatternBase = ((ppuctrl & 0x10) ~= 0) and 4096 or 0
  local spritePatternBase = ((ppuctrl & 0x08) ~= 0) and 4096 or 0

  local json = {
    "{",
    '  "frame": ' .. tostring(frameCount) .. ',',
    '  "pc": ' .. tostring(cpuState.pc) .. ',',
    '  "a": ' .. tostring(cpuState.a) .. ',',
    '  "x": ' .. tostring(cpuState.x) .. ',',
    '  "y": ' .. tostring(cpuState.y) .. ',',
    '  "ppuctrl": ' .. tostring(ppuctrl) .. ',',
    '  "sprite_height": ' .. tostring(spriteHeight) .. ',',
    '  "bg_pattern_base": ' .. tostring(bgPatternBase) .. ',',
    '  "sprite_pattern_base": ' .. tostring(spritePatternBase) .. ',',
    '  "chr_memory_type": "' .. chrMemType .. '",',
    '  "chr_size": ' .. tostring(chrSize),
    "}"
  }

  local file = io.open(path, "w+b")
  if file == nil then
    error("Unable to open metadata file for writing: " .. path)
  end
  file:write(table.concat(json, "\n"))
  file:close()
end

local function chooseOutputDir()
  if OUTPUT_DIR ~= nil and OUTPUT_DIR ~= "" then
    return OUTPUT_DIR
  end

  local scriptData = emu.getScriptDataFolder()
  if scriptData ~= nil and scriptData ~= "" then
    return scriptData
  end

  return "./dumps"
end

local function dumpScene()
  local outputDir = chooseOutputDir()

  local nametable = readMemoryBlock(emu.memType.nesNametableRam, 0x000, 0x400)
  local attribute = readMemoryBlock(emu.memType.nesNametableRam, 0x3C0, 0x40)
  local palette = readMemoryBlock(emu.memType.nesPaletteRam, 0x00, 0x20)
  local oam = readMemoryBlock(emu.memType.nesSpriteRam, 0x00, 0x100)

  local chrMemType = emu.memType.nesChrRam
  local chrSize = emu.getMemorySize(chrMemType)
  local chrLabel = "nesChrRam"
  if chrSize <= 0 then
    chrMemType = emu.memType.nesChrRom
    chrSize = emu.getMemorySize(chrMemType)
    chrLabel = "nesChrRom"
  end

  if chrSize > 0x2000 then
    chrSize = 0x2000
  end
  local chr = readMemoryBlock(chrMemType, 0x0000, chrSize)

  local ppuctrl = emu.read(0x2000, emu.memType.nesMemory)
  local spriteHeight = ((ppuctrl & 0x20) ~= 0) and 16 or 8

  writeBytes(outputDir .. "/" .. PREFIX .. "_nametable.bin", nametable)
  writeBytes(outputDir .. "/" .. PREFIX .. "_attribute.bin", attribute)
  writeBytes(outputDir .. "/" .. PREFIX .. "_palette.bin", palette)
  writeBytes(outputDir .. "/" .. PREFIX .. "_palette32.bin", palette)
  writeBytes(outputDir .. "/" .. PREFIX .. "_oam.bin", oam)
  writeBytes(outputDir .. "/" .. PREFIX .. "_chr.bin", chr)
  writeMetadata(outputDir .. "/" .. PREFIX .. "_metadata.json", spriteHeight, chrLabel, chrSize, ppuctrl)

  logInfo("Scene dumped to " .. outputDir)
  emu.displayMessage("Scene dump complete", PREFIX .. "_* files written to: " .. outputDir)
end

local function onEndFrame()
  if dumped then
    return
  end

  local ok, err = pcall(dumpScene)
  if ok then
    dumped = true
    return
  end

  local ioPath = emu.getScriptDataFolder()
  if ioPath == nil or ioPath == "" then
    emu.displayMessage("Scene dump failed", "Enable script I/O in Debugger settings")
    logInfo("Dump failed (script I/O disabled). Enable: Debugger -> Script Window -> Allow access to I/O and OS functions")
  else
    emu.displayMessage("Scene dump failed", "See Script Log for details")
    logInfo("Dump failed: " .. tostring(err))
  end
end

emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
emu.displayMessage("Scene dump", "Script loaded. Unpause for 1 frame to dump.")
logInfo("Script loaded. Waiting for endFrame event.")