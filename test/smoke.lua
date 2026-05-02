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
  assert(warn == nil)
  return
end

assert(_VERSION == "Lua 5.4", _VERSION)
assert(jit.lua54compat == true)
assert(type(warn) == "function")

do
  local co, ismain = coroutine.running()
  assert(type(co) == "thread")
  assert(ismain == true)
end

do
  local t = setmetatable({ 1, 2, 3 }, { __len = function() return 54 end })
  assert(#t == 54)
end

do
  warn("@off")
  warn("ignored warning")
  warn("@on")
  warn("lua54 ", "warning smoke")
  local ok, err = pcall(warn, 1)
  assert(ok == false)
  assert(type(err) == "string" and err:match("string expected"))
  warn("@off")
end
