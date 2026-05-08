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

  local macros = {}
  for line in out:gmatch("[^\r\n]+") do
    local name = line:match("^#define%s+([A-Za-z_][A-Za-z0-9_]*)")
    if name and wanted_prefix(name) then
      macros[name] = true
    end
  end
  assert(next(macros), "macro audit collected no macros from " .. include_dir)
  return macros
end

local official = arg[1] or os.getenv("LUA54_SRC_DIR") or
                 "D:/p4_gl2/pristine/tools/lua/lua-5.4.8-src/lua-5.4.8"
local current = "src"

local official_macros = collect_macros(official, "")
local current_macros = collect_macros(current, "-DLUAJIT_ENABLE_LUA54COMPAT")

local allowed_extra = {
  LUAJIT_ENABLE_LUA54COMPAT = true,
  -- These names are official Lua 5.4 functions, but the compatibility header
  -- exposes them as macros so external modules get wrapper ABI entrypoints
  -- while LuaJIT internals keep their historical 5.1 ABI.
  lua_dump = true,
  lua_getfield = true,
  lua_getglobal = true,
  lua_geti = true,
  lua_gettable = true,
  lua_load = true,
  lua_pushlstring = true,
  lua_pushstring = true,
  lua_rawget = true,
  lua_rawgeti = true,
  lua_rawgetp = true,
  lua_rawlen = true,
  lua_rawseti = true,
  lua_resume = true,
  lua_setglobal = true,
  lua_sethook = true,
  lua_version = true,
}

local missing = {}
for name in pairs(official_macros) do
  if not current_macros[name] then missing[#missing + 1] = name end
end
table.sort(missing)

local extra = {}
for name in pairs(current_macros) do
  if not official_macros[name] and not allowed_extra[name] then
    extra[#extra + 1] = name
  end
end
table.sort(extra)

assert(#missing == 0, "missing official Lua 5.4 macro names: " .. table.concat(missing, ", "))
assert(#extra == 0, "unexpected extra Lua 5.4 macro names: " .. table.concat(extra, ", "))

print("lua54_header_macro_audit.lua OK")
