local mode = ...
assert(mode == "default" or mode == "lua54compat", "unknown smoke test mode")

local jit = require("jit")

assert(type(jit.version) == "string")
assert(type(jit.version_num) == "number")
assert(type(jit.os) == "string")
assert(type(jit.arch) == "string")

if mode == "default" then
  assert(_VERSION == "Lua 5.1", _VERSION)
  assert(jit.lua54compat == false)
  return
end

assert(_VERSION == "Lua 5.4", _VERSION)
assert(jit.lua54compat == true)

do
  local co, ismain = coroutine.running()
  assert(type(co) == "thread")
  assert(ismain == true)
end

do
  local t = setmetatable({ 1, 2, 3 }, { __len = function() return 54 end })
  assert(#t == 54)
end
