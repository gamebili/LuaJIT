local function readfile(path)
  local f, err = io.open(path, "rb")
  assert(f, err)
  local data = f:read("*a")
  f:close()
  return data
end

local luaconf = readfile("src/luaconf.h")
assert(luaconf:find("#define%s+LUAMOD_API%s+LUA_API"),
       "src/luaconf.h: Lua 5.4 LUAMOD_API must be defined as LUA_API")

local lualib = readfile("src/lualib.h")
local openlibs = {
  "base",
  "coroutine",
  "table",
  "io",
  "os",
  "string",
  "utf8",
  "math",
  "debug",
  "package",
}

for _, lib in ipairs(openlibs) do
  -- Lua 5.4 lets embedders mark standard library open functions separately
  -- from auxiliary-library APIs, so lualib.h must use LUAMOD_API here.
  local pat = "LUAMOD_API%s+int%s+luaopen_" .. lib .. "%s*%("
  assert(lualib:find(pat), "src/lualib.h: luaopen_" .. lib ..
         " must be declared with LUAMOD_API")
end

assert(lualib:find("LUALIB_API%s+void%s+luaL_openlibs%s*%("),
       "src/lualib.h: luaL_openlibs must remain declared with LUALIB_API")

local lauxlib = readfile("src/lauxlib.h")
assert(lauxlib:find("typedef%s+struct%s+luaL_Buffer%s+luaL_Buffer%s*;"),
       "src/lauxlib.h: Lua 5.4 must forward typedef luaL_Buffer")
assert(lauxlib:find("struct%s+luaL_Buffer%s*{"),
       "src/lauxlib.h: Lua 5.4 must define struct luaL_Buffer by tag")
assert(not lauxlib:find("typedef%s+struct%s+luaL_Buffer%s*{"),
       "src/lauxlib.h: Lua 5.4 luaL_Buffer definition must not repeat typedef")
assert(lauxlib:find("#define%s+luaL_argcheck%b()%s*\\%s*\r?\n%s*%(%(void%)%s*%(%s*luai_likely%b()%s*||%s*luaL_argerror"),
       "src/lauxlib.h: Lua 5.4 luaL_argcheck must use luai_likely")
assert(lauxlib:find("#define%s+luaL_argexpected%b()%s*\\%s*\r?\n%s*%(%(void%)%s*%(%s*luai_likely%b()%s*||%s*luaL_typeerror"),
       "src/lauxlib.h: Lua 5.4 luaL_argexpected must use luai_likely")

print("lua54_header_static.lua OK")
