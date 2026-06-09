-- Differential probe battery for Lua 5.4 surface comparison.
--
-- Run this file with both the official Lua 5.4 interpreter and the LuaJIT
-- Lua 5.4 compat build, then diff the normalized outputs:
--
--   lua54.exe tools/lua54_diff_probe.lua > official.txt
--   luajit.exe tools/lua54_diff_probe.lua > compat.txt
--
-- Each line is "#<probe-id> <results>" where results are the pcall results
-- of calling one stdlib function with one battery argument combination.
-- Memory addresses and other nondeterministic payloads are normalized so
-- the two outputs are directly diffable. The probe ordering is fully
-- deterministic (sorted library and function names, fixed battery order).

if jit then jit.off() end
io.stdout:setvbuf("line")

local function norm(s)
  s = tostring(s)
  -- Normalize addresses / object identities.
  s = s:gsub("0[xX]%x+", "PTR")
  s = s:gsub("builtin#%d+", "PTR")
  s = s:gsub("(table: )%S+", "%1PTR")
  s = s:gsub("(function: )%S+", "%1PTR")
  s = s:gsub("(thread: )%S+", "%1PTR")
  s = s:gsub("(userdata: )%S+", "%1PTR")
  s = s:gsub("(file %()[^%)]+(%))", "%1PTR%2")
  -- Normalize this script's path prefix in error positions.
  s = s:gsub("[^%s:]*lua54_diff_probe%.lua", "PROBE")
  -- Strip nonprintable bytes so the output stays line-oriented.
  s = s:gsub("[%z\1-\9\11-\31\127-\255]", function(c)
    return ("\\%03d"):format(c:byte())
  end)
  return s
end

local function emit(id, ...)
  local n = select("#", ...)
  local parts = {}
  for i = 1, n do
    parts[i] = norm((select(i, ...)))
  end
  io.write("#", id, " ", table.concat(parts, " | "), "\n")
end

-- Battery of argument values. Functions returning fresh values keep probes
-- independent. Order is fixed; ids derive from it.
local battery = {
  { "noarg" },
  { "nil", function() return nil end },
  { "true", function() return true end },
  { "false", function() return false end },
  { "table", function() return {} end },
  { "seqtable", function() return { 10, 20, 30 } end },
  { "func", function() return function() end end },
  { "int0", function() return 0 end },
  { "negint", function() return -1 end },
  { "int3", function() return 3 end },
  { "frac", function() return 1.5 end },
  { "bigint", function() return 1 << 40 end },
  { "huge", function() return math.huge end },
  { "emptystr", function() return "" end },
  { "numstr", function() return "1.5" end },
  { "intstr", function() return "3" end },
  { "alphastr", function() return "x" end },
  { "thread", function() return coroutine.create(function() end) end },
}

-- Functions that must not run with arbitrary battery values because they
-- terminate the process, touch the filesystem/clock/PRNG, read stdin, or
-- mutate interpreter-global state shared with later probes.
local deny = {
  ["_G.dofile"] = true,        -- battery has no file args; noarg reads stdin
  ["_G.print"] = true,
  ["_G.require"] = true,       -- error text embeds host-specific package.path
  ["_G.collectgarbage"] = false,
  ["os.clock"] = true,
  ["os.date"] = true,
  ["os.exit"] = true,
  ["os.execute"] = true,
  ["os.getenv"] = true,
  ["os.remove"] = true,
  ["os.rename"] = true,
  ["os.time"] = true,
  ["os.tmpname"] = true,
  ["math.random"] = true,
  ["math.randomseed"] = true,
  ["io.close"] = true,
  ["io.flush"] = true,
  ["io.input"] = true,
  ["io.lines"] = true,         -- "" probes differ only by C strerror text
  ["io.open"] = true,          -- avoid creating files; covered separately
  ["io.output"] = true,
  ["io.popen"] = true,
  ["io.read"] = true,
  ["io.write"] = true,
  ["io.tmpfile"] = true,
  ["debug.debug"] = true,
  ["debug.sethook"] = true,
  ["debug.setmetatable"] = true,
  ["debug.getlocal"] = true,   -- exposes harness-internal frame layout
  ["debug.setlocal"] = true,   -- mutates harness-internal frame slots
  ["os.setlocale"] = true,     -- mutates process locale; host-specific names
  ["string.dump"] = true,      -- documented LuaJIT bytecode format boundary
  ["package.loadlib"] = true,  -- error text embeds host loader specifics
  ["package.searchpath"] = false,
}

-- Functions whose two-argument battery would request huge allocations
-- (e.g. string.rep("...", 1 << 40) commits terabytes before failing on
-- hosts with an expandable pagefile). Keep the one-argument battery only.
local deny2 = {
  ["string.rep"] = true,
}

local libs = {
  { "_G", _G, {
    "assert", "error", "getmetatable", "ipairs", "load", "next", "pairs",
    "pcall", "rawequal", "rawget", "rawlen", "rawset", "select",
    "setmetatable", "tonumber", "tostring", "type", "xpcall", "warn",
    "collectgarbage",
  } },
  { "string", string },
  { "table", table },
  { "math", math },
  { "os", os },
  { "io", io },
  { "utf8", utf8 },
  { "coroutine", coroutine },
  { "debug", debug },
  { "package", package },
}

local function fnames(libname, lib, explicit)
  local names = {}
  if explicit then
    for _, k in ipairs(explicit) do names[#names + 1] = k end
  else
    for k, v in pairs(lib) do
      if type(k) == "string" and type(v) == "function" then
        names[#names + 1] = k
      end
    end
  end
  table.sort(names)
  local out = {}
  for _, k in ipairs(names) do
    local full = libname .. "." .. k
    if not deny[full] then out[#out + 1] = k end
  end
  return out
end

-- Phase 1: surface inventory (which functions exist, value kinds).
for _, entry in ipairs(libs) do
  local libname, lib, explicit = entry[1], entry[2], entry[3]
  local names = {}
  if explicit then
    names = explicit
  else
    for k in pairs(lib) do
      if type(k) == "string" then names[#names + 1] = k end
    end
  end
  table.sort(names)
  for _, k in ipairs(names) do
    emit("inv:" .. libname .. "." .. k, type(lib[k]))
  end
end

-- Phase 2: one- and two-argument call battery.
local function probe_call(full, f, desc, ...)
  local id = "call:" .. full .. "(" .. desc .. ")"
  emit(id, pcall(f, ...))
end

for _, entry in ipairs(libs) do
  local libname, lib, explicit = entry[1], entry[2], entry[3]
  for _, k in ipairs(fnames(libname, lib, explicit)) do
    local full = libname .. "." .. k
    local f = lib[k]
    for _, a in ipairs(battery) do
      if a[2] then
        probe_call(full, f, a[1], a[2]())
      else
        probe_call(full, f, a[1])
      end
    end
    if not deny2[full] then
      for _, a in ipairs(battery) do
        for _, b in ipairs(battery) do
          if a[2] and b[2] then
            probe_call(full, f, a[1] .. "," .. b[1], a[2](), b[2]())
          end
        end
      end
    end
  end
end

io.write("#done\n")
