local function readall(cmd)
  local f = assert(io.popen(cmd .. " 2>&1", "r"))
  local data = f:read("*a")
  local ok, _, code = f:close()
  assert(ok, data ~= "" and data or ("command failed: " .. cmd .. " exit=" .. tostring(code)))
  return data
end

local function quote(path)
  return '"' .. path:gsub('"', '\\"') .. '"'
end

local function write_input(path)
  local f = assert(io.open(path, "wb"))
  f:write('#include "lua.h"\n#include "lauxlib.h"\n#include "lualib.h"\n')
  f:close()
end

local function collect_macros(include_dir, defs)
  -- Keep the temporary input outside src/. For quoted includes, GCC searches
  -- the input file directory before -I paths; putting this under src would
  -- make the "official" side accidentally include the local compatibility
  -- headers instead of the Lua 5.4.8 reference headers.
  local tmp = "lua54_header_macro_audit_input.c"
  write_input(tmp)
  local cmd = table.concat({
    "gcc",
    defs or "",
    "-std=c99",
    "-I", quote(include_dir),
    "-E -dM -x c",
    quote(tmp),
  }, " ")
  local out = readall(cmd)
  os.remove(tmp)

  local function wanted_prefix(name)
    return name:sub(1, 3) == "LUA" or
           name:sub(1, 3) == "lua" or
           name:sub(1, 4) == "LUAI" or
           name:sub(1, 4) == "LUAL" or
           name:sub(1, 4) == "luaL" or
           name:sub(1, 4) == "luai" or
           name:sub(1, 2) == "l_"
  end

  local function has_macro(blob, name)
    return blob:find("\n" .. name .. "\n", 1, true) ~= nil
  end

  local macros = "\n"
  for line in out:gmatch("[^\r\n]+") do
    local name = line:match("^#define%s+([A-Za-z_][A-Za-z0-9_]*)")
    if name and wanted_prefix(name) then
      if not has_macro(macros, name) then
        macros = macros .. name .. "\n"
      end
    end
  end
  assert(macros ~= "\n", "macro audit collected no macros from " .. include_dir)
  return macros
end

local official = arg[1] or os.getenv("LUA54_SRC_DIR") or
                 "D:/p4_gl2/pristine/tools/lua/lua-5.4.8-src/lua-5.4.8"
local current = "src"

local official_macros = collect_macros(official, "")
local current_macros = collect_macros(current, "-DLUAJIT_ENABLE_LUA54COMPAT")

local allowed_extra = "\n" .. table.concat({
  "LUAJIT_ENABLE_LUA54COMPAT",
  -- These names are official Lua 5.4 functions, but the compatibility header
  -- exposes them as macros so external modules get wrapper ABI entrypoints
  -- while LuaJIT internals keep their historical 5.1 ABI.
  "lua_dump",
  "lua_getfield",
  "lua_getglobal",
  "lua_geti",
  "lua_gettable",
  "lua_load",
  "lua_pushlstring",
  "lua_pushstring",
  "lua_rawget",
  "lua_rawgeti",
  "lua_rawgetp",
  "lua_rawlen",
  "lua_rawseti",
  "lua_resume",
  "lua_setglobal",
  "lua_sethook",
  "lua_version",
}, "\n") .. "\n"

local function has_macro(blob, name)
  return blob:find("\n" .. name .. "\n", 1, true) ~= nil
end

local missing = ""
local missing_count = 0
for name in official_macros:gmatch("[^\n]+") do
  if not has_macro(current_macros, name) then
    missing_count = missing_count + 1
    missing = missing .. (missing_count == 1 and name or ", " .. name)
  end
end

local extra = ""
local extra_count = 0
for name in current_macros:gmatch("[^\n]+") do
  if not has_macro(official_macros, name) and not has_macro(allowed_extra, name) then
    extra_count = extra_count + 1
    extra = extra .. (extra_count == 1 and name or ", " .. name)
  end
end

assert(missing_count == 0, "missing official Lua 5.4 macro names: " .. missing)
assert(extra_count == 0, "unexpected extra Lua 5.4 macro names: " .. extra)

print("lua54_header_macro_audit.lua OK")
