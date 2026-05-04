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
  assert(math.type == nil)
  assert(math.maxinteger == nil)
  assert(math.mininteger == nil)
  assert(math.tointeger == nil)
  assert(math.ult == nil)
  assert(utf8 == nil)
  assert(debug.setcstacklimit == nil)
  assert(coroutine.close == nil)
  assert(package.searchers == nil)
  assert(type(package.loaders) == "table")
  assert(string.pack == nil)
  assert(string.unpack == nil)
  assert(string.packsize == nil)
  assert(type(getfenv) == "function")
  assert(type(setfenv) == "function")
  assert(type(module) == "function")
  assert(type(newproxy) == "function")
  assert(type(loadstring) == "function")
  assert(type(unpack) == "function")
  assert(type(gcinfo) == "function")
  assert(type(bit) == "table")
  assert(type(package.seeall) == "function")
  assert(type(table.foreach) == "function")
  assert(type(table.foreachi) == "function")
  assert(type(table.getn) == "function")
  assert(type(table.maxn) == "function")
  assert(type(math.atan2) == "function")
  assert(type(math.pow) == "function")
  assert(type(math.log10) == "function")
  assert(type(debug.getfenv) == "function")
  assert(type(debug.setfenv) == "function")
  assert(_ENV == nil)
  assert(select(1, pcall(collectgarbage, "generational")) == false)
  assert(select(1, pcall(collectgarbage, "incremental")) == false)
  assert(load("local x <const> = 1; return x") == nil)
  assert(load("local x <close> = false; return x") == nil)
  assert(load("return 5 // 2") == nil)
  assert(load("return 6 & 3") == nil)
  assert(load("return 4 | 1") == nil)
  assert(load("return 7 ~ 3") == nil)
  assert(load("return ~7") == nil)
  assert(load("return 1 << 3") == nil)
  assert(load("return 8 >> 1") == nil)
  return
end

assert(_VERSION == "Lua 5.4", _VERSION)
assert(jit.lua54compat == true)
assert(getfenv == nil)
assert(setfenv == nil)
assert(module == nil)
assert(newproxy == nil)
assert(loadstring == nil)
assert(unpack == nil)
assert(gcinfo == nil)
assert(bit == nil)
assert(package.seeall == nil)
assert(table.foreach == nil)
assert(table.foreachi == nil)
assert(table.getn == nil)
assert(table.maxn == nil)
assert(type(table.unpack) == "function")
do
  local a, b, c = table.unpack({ "a", "b", "c" }, 2)
  assert(a == "b" and b == "c" and c == nil)
  local n
  n, a, b, c = select("#", table.unpack({ "a", nil, "c" })),
	       table.unpack({ "a", nil, "c" })
  assert(n == 3 and a == "a" and b == nil and c == "c")
  n, a, b, c = select("#", table.unpack(setmetatable({ "a" }, {
    __len = function() return 3 end
  }))), table.unpack(setmetatable({ "a" }, { __len = function() return 3 end }))
  assert(n == 3 and a == "a" and b == nil and c == nil)
  local proxy = setmetatable({}, {
    __len = function() return 3 end,
    __index = function(_, k) return tostring(k) end,
  })
  n, a, b, c = select("#", table.unpack(proxy)), table.unpack(proxy)
  assert(n == 3 and a == "1" and b == "2" and c == "3")
  assert(select(1, pcall(table.unpack, {}, 1.2)) == false)
  assert(select(1, pcall(table.unpack, {}, 1, 1.2)) == false)
  local ok, err = pcall(table.unpack)
  assert(ok == false and err:match("attempt to get length") ~= nil)
  assert(select("#", table.unpack(nil, 1, 0)) == 0)
  ok, err = pcall(table.unpack, nil, 1, 1)
  assert(ok == false and err:match("attempt to index a nil value") ~= nil)
  assert(select("#", table.unpack(1, 1, 0)) == 0)
end
assert(math.atan2 == nil)
assert(math.pow == nil)
assert(math.log10 == nil)
assert(math.sinh == nil)
assert(math.cosh == nil)
assert(math.tanh == nil)
assert(math.frexp == nil)
assert(math.ldexp == nil)
assert(debug.getfenv == nil)
assert(debug.setfenv == nil)
assert(type(package.searchers) == "table")
assert(package.loaders == nil)
assert(type(debug.setcstacklimit) == "function")
assert(type(coroutine.close) == "function")
assert(type(string.pack) == "function")
assert(type(string.unpack) == "function")
assert(type(string.packsize) == "function")
assert(type(warn) == "function")
assert(type(math.type) == "function")
assert(_ENV == _G)
assert(rawget(_G, "_ENV") == nil)
do
  local k = nil
  repeat
    k = next(_G, k)
    assert(k ~= "_ENV")
  until k == nil
  for name in pairs(_G) do
    assert(name ~= "_ENV")
  end
end
assert(select(1, pcall(error, "lua54 error level", 1.2)) == false)
assert(select(1, pcall(getmetatable)) == false)
assert(select(1, pcall(select, 1.2, "a", "b")) == false)
assert(select(1, pcall(select, -1.2, "a", "b")) == false)
assert(select("1", "a", "b") == "a")
do
  local ok, err = pcall(assert)
  assert(ok == false and err:match("to 'assert'") ~= nil)
  ok, err = pcall(next)
  assert(ok == false and err:match("to 'next'") ~= nil)
  ok, err = pcall(pairs)
  assert(ok == false and err:match("to 'pairs'") ~= nil)
  ok, err = pcall(ipairs)
  assert(ok == false and err:match("to 'ipairs'") ~= nil)
  ok, err = pcall(setmetatable)
  assert(ok == false and err:match("to 'setmetatable'") ~= nil)
  ok, err = pcall(getmetatable)
  assert(ok == false and err:match("to 'getmetatable'") ~= nil)
  local iter, state, key = pairs(1)
  assert(iter == next and state == 1 and key == nil)
  ok, err = pcall(iter, state, key)
  assert(ok == false and err:match("to 'next'") ~= nil)
  for _, name in ipairs({ "create", "resume", "status", "wrap", "close" }) do
    ok, err = pcall(coroutine[name])
    assert(ok == false and err:match("coroutine%."..name) ~= nil)
  end
  ok, err = pcall(coroutine.yield)
  assert(ok == false and err:match("outside a coroutine") ~= nil)
  for _, item in ipairs({
    { "type", function() return type() end },
    { "tostring", function() return tostring() end },
    { "pcall", function() return pcall() end },
    { "xpcall", function() return xpcall(function() end) end },
    { "select", function() return select() end },
    { "error", function() return error(nil, 1.2) end },
    { "tonumber", function() return tonumber("10", 2.5) end },
    { "load", function() return load(true) end },
    { "load", function() return load("return 1", true) end },
    { "load", function() return load("return 1", nil, true) end },
    { "loadfile", function() return loadfile(nil, true) end },
    { "dofile", function() return dofile(true) end },
    { "collectgarbage", function() return collectgarbage(true) end },
    { "collectgarbage", function() return collectgarbage(1) end },
    { "collectgarbage", function() return collectgarbage("step", true) end },
    { "collectgarbage", function() return collectgarbage("step", 1.2) end },
  }) do
    ok, err = pcall(item[2])
    assert(ok == false and err:match("to '"..item[1].."'") ~= nil)
  end
end
do
  local ok, err = pcall(rawget)
  assert(ok == false and err:match("to 'rawget'") ~= nil)
  ok, err = pcall(rawset, {}, "k")
  assert(ok == false and err:match("to 'rawset'") ~= nil)
  ok, err = pcall(rawequal)
  assert(ok == false and err:match("to 'rawequal'") ~= nil)
  ok, err = pcall(rawlen, 1)
  assert(ok == false and err:match("to 'rawlen'") ~= nil and
         err:match("table or string expected") ~= nil)
end
do
  local locked = setmetatable({}, { __metatable = "locked" })
  assert(getmetatable(locked) == "locked")
end
do
  local f = assert(load("return x"))
  local name, env = debug.getupvalue(f, 1)
  assert(name == "_ENV" and env == _G)
  assert(debug.getinfo(f, "u").nups == 1)
  local repl = { x = 54 }
  assert(debug.setupvalue(f, 1, repl) == "_ENV")
  assert(f() == 54)

  local no_global = assert(load("local y = 1; return y"))
  assert(debug.getupvalue(no_global, 1) == "_ENV")
  assert(debug.getinfo(no_global, "u").nups == 1)

  local function outer()
    local y = 7
    return function() return _G, y end
  end
  local h = outer()
  assert(debug.getinfo(h, "u").nups == 2)
  local n1, v1 = debug.getupvalue(h, 1)
  local n2, v2 = debug.getupvalue(h, 2)
  assert(n1 == "_ENV" and v1 == _G)
  assert(n2 == "y" and v2 == 7)
end
do
  for _, item in ipairs({
    { "string.byte", function() return pcall(string.byte, nil) end },
    { "string.char", function() return pcall(string.char, nil) end },
    { "string.dump", function() return pcall(string.dump, nil) end },
    { "string.find", function() return pcall(string.find, nil, "x") end },
    { "string.format", function() return pcall(string.format, nil) end },
    { "string.gmatch", function() return pcall(string.gmatch, nil, "x") end },
    { "string.gsub", function() return pcall(string.gsub, nil, "x", "y") end },
    { "string.len", function() return pcall(string.len, nil) end },
    { "string.lower", function() return pcall(string.lower, nil) end },
    { "string.match", function() return pcall(string.match, nil, "x") end },
    { "string.rep", function() return pcall(string.rep, nil, 2) end },
    { "string.reverse", function() return pcall(string.reverse, nil) end },
    { "string.sub", function() return pcall(string.sub, nil, 1) end },
    { "string.upper", function() return pcall(string.upper, nil) end },
    { "string.pack", function() return pcall(string.pack, nil) end },
    { "string.unpack", function() return pcall(string.unpack, nil, "") end },
    { "string.packsize", function() return pcall(string.packsize, nil) end },
  }) do
    local ok, err = item[2]()
    assert(ok == false and err:match("to '"..item[1].."'") ~= nil)
  end
end
do
  local function expect_bad_integer(f, ...)
    local ok, err = pcall(f, ...)
    assert(ok == false and type(err) == "string" and
           err:match("integer representation") ~= nil)
  end
  local function with_local()
    local x = 1
    expect_bad_integer(debug.getinfo, 1.2)
    expect_bad_integer(debug.getlocal, 1, 1.2)
    expect_bad_integer(debug.getlocal, 1.2, 1)
    expect_bad_integer(debug.setlocal, 1, 1.2, x)
    expect_bad_integer(debug.setlocal, 1.2, 1, x)
  end
  local function with_upvalue()
    local y = 1
    return function() return y end
  end
  local f = with_upvalue()
  with_local()
  expect_bad_integer(debug.getupvalue, f, 1.2)
  expect_bad_integer(debug.setupvalue, f, 1.2, 2)
  expect_bad_integer(debug.upvalueid, f, 1.2)
  expect_bad_integer(debug.upvaluejoin, f, 1.2, f, 1)
  expect_bad_integer(debug.sethook, function() end, "", 1.2)
  expect_bad_integer(debug.traceback, "lua54 traceback", 1.2)
  expect_bad_integer(debug.getuservalue, io.stdout, 1.2)
  expect_bad_integer(debug.setuservalue, io.stdout, {}, 1.2)
  expect_bad_integer(debug.setcstacklimit, 1.2)
  assert(debug.getinfo("1", "n") ~= nil)
end
do
  assert(collectgarbage("generational") == "generational")
  assert(collectgarbage("incremental") == "generational")
  assert(collectgarbage("incremental") == "incremental")
  assert(collectgarbage("generational") == "incremental")
  assert(select(1, pcall(collectgarbage, "minor")) == false)
  assert(select(1, pcall(collectgarbage, "major")) == false)
  do
    local oldpause = collectgarbage("setpause", 123)
    assert(oldpause == 200)
    assert(collectgarbage("setpause", oldpause) == 120)
    assert(collectgarbage("setpause", -1) == 200)
    assert(collectgarbage("setpause", 1001) == 0)
    assert(collectgarbage("setpause", 200) == 1000)
    local oldmul = collectgarbage("setstepmul", 321)
    assert(oldmul == 100)
    assert(collectgarbage("setstepmul", oldmul) == 320)
    assert(collectgarbage("setstepmul", -1) == 100)
    assert(collectgarbage("setstepmul", 1001) == 0)
    assert(collectgarbage("setstepmul", 100) == 1000)
    assert(select(1, pcall(collectgarbage, "step", 1.2)) == false)
    assert(select(1, pcall(collectgarbage, "setpause", 123.5)) == false)
    assert(select(1, pcall(collectgarbage, "setstepmul", 123.5)) == false)
    assert(collectgarbage("setpause", "123") == 200)
    assert(collectgarbage("setstepmul", "123") == 100)
  end
end
do
  assert(assert(load("local x <const> = 1; return x"))() == 1)
  do
    local a, b = assert(load("local x <const>, y = 1, 2; return x, y"))()
    assert(a == 1 and b == 2)
  end
  assert(load("local x <const> = 1; x = 2") == nil)
  assert(load("local x <const> = 1; local function f() x = 2 end") == nil)
  assert(load("local x <unknown> = 1") == nil)
  assert(load("local x <close>, y <close> = false, false") == nil)
  do
    local ok, err = pcall(assert(load("local x <close> = 1")))
    assert(ok == false and err:match("variable 'x' got a non%-closable value") ~= nil)
    assert(assert(load("local x <close>; return x"))() == nil)
    assert(assert(load("local x <close> = nil; return x"))() == nil)
    local closable = setmetatable({}, { __close = function() end })
    _G.__lua54_closable_decl = closable
    assert(assert(load("local x <close> = __lua54_closable_decl; return x"))() == closable)
    _G.__lua54_closable_decl = nil
  end
  assert(assert(load("local x <close> = false; return x"))() == false)
  do
    assert(assert(load([[
      local log = {}
      local mt = {
        __close = function(self, err)
          assert(err == nil)
          log[#log + 1] = self.name
        end,
      }
      do
        local a <close> = setmetatable({ name = "a" }, mt)
        local b <close> = setmetatable({ name = "b" }, mt)
        assert(a.name == "a" and b.name == "b")
      end
      assert(table.concat(log, ",") == "b,a")

      local n = 0
      do
        local x <close> = false
        local y <close> = nil
        assert(x == false and y == nil)
      end
      assert(n == 0)
      return true
    ]]))())
  end
  do
    local setlocal_const = assert(load([[
    return function()
      local x <const> = {}
      local repl = { changed = 1 }
      assert(debug.setlocal(1, 1, repl) == "x")
      assert(x == repl and x.changed == 1)
    end
    ]]))()
    setlocal_const()
    local f = assert(load([[local x <const> = {}; return function() return x end]]))()
    local repl = { changed = 2 }
    assert(debug.getupvalue(f, 1) == "x")
    assert(debug.setupvalue(f, 1, repl) == "x")
    assert(f() == repl and f().changed == 2)
    do
      local y = { joined = 3 }
      local g = function() return y end
      debug.upvaluejoin(f, 1, g, 1)
      assert(f() == y and f().joined == 3)
    end
  end
end
do
  assert(assert(load("local _ENV = { x = 42 }; return x"))() == 42)
  assert(assert(load("local _ENV = {}; x = 7; return _ENV.x"))() == 7)
  assert(assert(load("local _ENV = { f = function() return 3 end }; return f()"))() == 3)
end
do
  local dbg, assert = debug, assert
  local e1, e2 = { x = 1 }, { x = 2 }
  local _ENV = e1
  local function f() return x end
  assert(f() == 1)
  do
    local name, value = dbg.getupvalue(f, 1)
    assert(name == "_ENV" and value == e1)
  end
  do
    local _ENV = e2
    local function g() return x end
    assert(g() == 2)
    assert(dbg.upvalueid(f, 1) ~= dbg.upvalueid(g, 1))
    dbg.upvaluejoin(f, 1, g, 1)
    assert(f() == 2 and g() == 2)
    assert(dbg.upvalueid(f, 1) == dbg.upvalueid(g, 1))
  end
end
do
  local dbg, pcall, type, assert = debug, pcall, type, assert
  local _ENV = 5
  local function f() return missing_global end
  local name, value = dbg.getupvalue(f, 1)
  assert(name == "_ENV" and type(value) == "number" and value == 5)
  local ok, err = pcall(f)
  assert(ok == false and err:match("_ENV") ~= nil and
         err:match("number") ~= nil)
end
do
  local f, err = load("for i = 1, 3, 0 do end")
  assert(f == nil and err:match("'for' step is zero") ~= nil)
  f, err = load("for i = 1, 3, 0.0 do end")
  assert(f == nil and err:match("'for' step is zero") ~= nil)
  local ok
  ok, err = pcall(assert(load("local z = 0; for i = 1, 3, z do end")))
  assert(ok == false and err:match("'for' step is zero") ~= nil)
  ok, err = pcall(assert(load([[for i = 1, 3, "0" do end]])))
  assert(ok == false and err:match("'for' step is zero") ~= nil)
end
do
  local function eval(src)
    return assert(load("return "..src))()
  end
  assert(eval("5 // 2") == 2)
  assert(eval("-5 // 2") == -3)
  assert(eval("5.5 // 2") == 2)
  assert(eval([["5" // 2]]) == 2)
  assert(eval([["5.5" // 2]]) == 2)
  assert(assert(load("local a, b = 6, 3; return a & b"))() == 2)
  assert(eval("4 | 1") == 5)
  assert(eval("7 ~ 3") == 4)
  assert(eval("~7") == -8)
  assert(eval("1 << 3") == 8)
  assert(eval("8 >> 1") == 4)
  assert(assert(load("local a, b = 6, 3; return (a + 1) & (b + 1)"))() == 4)
  assert(eval("((1 << 4) | 3) ~ 5") == 22)
  assert(eval("8 >> -1") == 16)
  assert(eval("8 << -1") == 4)
  do
    assert(assert(load([[return "1" + "2"]]))() == 3)
    local string_mt = debug.getmetatable("")
    debug.setmetatable("", {
      __add = function(a, b) return "add:"..tostring(a)..":"..tostring(b) end,
      __mul = function(a, b) return "mul:"..tostring(a)..":"..tostring(b) end,
      __unm = function(a, b) return "unm:"..tostring(a)..":"..tostring(b) end,
    })
    assert(assert(load([[return "1" + 2]]))() == "add:1:2")
    assert(assert(load([[return 2 + "1"]]))() == "add:2:1")
    assert(assert(load([[return "1" * 2]]))() == "mul:1:2")
    assert(assert(load([[return -"1"]]))() == "unm:1:1")
    debug.setmetatable("", string_mt)
    local ok, err = pcall(assert(load([[return "x" + 1]])))
    assert(ok == false and err:match("attempt to add") ~= nil and
           err:match("'string'") ~= nil and err:match("'number'") ~= nil)
    ok, err = pcall(assert(load([[return 1 + "x"]])))
    assert(ok == false and err:match("attempt to add") ~= nil and
           err:match("'number'") ~= nil and err:match("'string'") ~= nil)
    ok, err = pcall(assert(load([[return "x" * true]])))
    assert(ok == false and err:match("attempt to mul") ~= nil and
           err:match("'string'") ~= nil and err:match("'boolean'") ~= nil)
    ok, err = pcall(assert(load([[return true * "x"]])))
    assert(ok == false and err:match("attempt to mul") ~= nil and
           err:match("'boolean'") ~= nil and err:match("'string'") ~= nil)
    ok, err = pcall(assert(load([[return -"x"]])))
    assert(ok == false and err:match("attempt to unm") ~= nil and
           err:match("'string'") ~= nil)
  end
  do
    local ok, err = pcall(assert(load("return 3.5 & 1")))
    assert(ok == false and err:match("integer representation") ~= nil)
    ok, err = pcall(assert(load([[return "3" & 1]])))
    assert(ok == false and err:match("bitwise operation") ~= nil and
           err:match("string value") ~= nil)
    ok, err = pcall(assert(load("return true & 1")))
    assert(ok == false and err:match("bitwise operation") ~= nil and
           err:match("boolean value") ~= nil)
    _G.__lua54_named_bitwise = setmetatable({}, { __name = "Lua54Bitwise" })
    ok, err = pcall(assert(load("return __lua54_named_bitwise & 1")))
    assert(ok == false and err:match("bitwise operation") ~= nil and
           err:match("Lua54Bitwise value") ~= nil)
    _G.__lua54_named_bitwise = nil
  end
  do
    local ok, err = pcall(assert(load([[return "x" // 1]])))
    assert(ok == false and err:match("idiv") ~= nil and
           err:match("'string'") ~= nil and err:match("'number'") ~= nil)
    ok, err = pcall(assert(load([[return 1 // "x"]])))
    assert(ok == false and err:match("idiv") ~= nil and
           err:match("'number'") ~= nil and err:match("'string'") ~= nil)
    _G.__lua54_named_idiv = setmetatable({}, { __name = "Lua54Idiv" })
    ok, err = pcall(assert(load("return __lua54_named_idiv // 1")))
    assert(ok == false and err:match("idiv") ~= nil and
           err:match("'Lua54Idiv'") ~= nil)
    _G.__lua54_named_idiv = nil
  end
  do
    local only_lt = setmetatable({}, { __lt = function() return true end })
    local with_le = setmetatable({}, { __le = function() return true end })
    assert(only_lt < only_lt)
    assert(select(1, pcall(function() return only_lt <= only_lt end)) == false)
    assert((with_le <= with_le) == true)
  end
  do
    local lhs = setmetatable({}, {
      __idiv = function(a, b) return { "idiv", a, b } end,
      __band = function(a, b) return { "band", a, b } end,
      __bor = function(a, b) return { "bor", a, b } end,
      __bxor = function(a, b) return { "bxor", a, b } end,
      __bnot = function(a, b) return { "bnot", a, b } end,
      __shl = function(a, b) return { "shl", a, b } end,
      __shr = function(a, b) return { "shr", a, b } end,
    })
    local rhs = {}
    _G.__lua54_meta_lhs = lhs
    _G.__lua54_meta_rhs = rhs
    assert(assert(load("return __lua54_meta_lhs // __lua54_meta_rhs"))()[1] == "idiv")
    assert(assert(load("return __lua54_meta_lhs & __lua54_meta_rhs"))()[1] == "band")
    assert(assert(load("return __lua54_meta_lhs | __lua54_meta_rhs"))()[1] == "bor")
    assert(assert(load("return __lua54_meta_lhs ~ __lua54_meta_rhs"))()[1] == "bxor")
    do
      local r = assert(load("return ~__lua54_meta_lhs"))()
      assert(r[1] == "bnot" and r[2] == lhs and r[3] == lhs)
    end
    assert(assert(load("return __lua54_meta_lhs << 2"))()[1] == "shl")
    assert(assert(load("return __lua54_meta_lhs >> 2"))()[1] == "shr")
    do
      local reverse = setmetatable({}, { __band = function(a, b) return { a, b } end })
      _G.__lua54_meta_reverse = reverse
      local r = assert(load("return 7 & __lua54_meta_reverse"))()
      assert(r[1] == 7 and r[2] == reverse)
    end
    _G.__lua54_meta_lhs = nil
    _G.__lua54_meta_rhs = nil
    _G.__lua54_meta_reverse = nil
  end
end
do
  for _, name in ipairs({ "type", "tointeger", "ult", "min", "max" }) do
    local ok, err = pcall(math[name])
    assert(ok == false and err:match("math%."..name) ~= nil)
  end
end
assert(math.type(nil) == nil)
assert(math.type("1") == nil)
assert(math.type(1.5) == "float")
assert(math.type(1) == "integer" or math.type(1) == "float")
assert(math.maxinteger == 2147483647)
assert(math.mininteger == -2147483648)
assert(math.maxinteger > 0 and math.mininteger < 0)
assert(type(math.tointeger) == "function")
assert(type(math.ult) == "function")
assert(math.tointeger(nil) == nil)
assert(math.tointeger("12") == 12)
assert(math.tointeger(12.0) == 12)
assert(math.tointeger(12.5) == nil)
assert(math.tointeger(2147483648) == nil)
assert(math.ult(1, 2) == true)
assert(math.ult(2, 1) == false)
assert(math.ult(1, -1) == true)
assert(math.ult(-1, 1) == false)
assert(select(1, pcall(math.ult, 1.5, 2)) == false)
assert(math.max("a", "b") == "b")
assert(math.min("a", "b") == "a")
assert(math.max(false) == false)
do
  local a = setmetatable({ v = 1 }, { __lt = function(x, y) return x.v < y.v end })
  local b = setmetatable({ v = 2 }, { __lt = function(x, y) return x.v < y.v end })
  assert(math.max(a, b) == b)
  assert(math.min(a, b) == a)
end
assert(select(1, pcall(math.max)) == false)
assert(select(1, pcall(math.max, 1, "b")) == false)
assert(math.type(math.floor(1.2)) == "integer")
assert(math.floor("1.2") == 1)
assert(math.type(math.ceil(1.2)) == "integer")
do
  local ok, err = pcall(math.deg)
  assert(ok == false and err:match("math%.deg") ~= nil and
         err:match("number expected") ~= nil)
  ok, err = pcall(math.rad, {})
  assert(ok == false and err:match("math%.rad") ~= nil and
         err:match("number expected") ~= nil)
  assert(math.deg(tostring(math.pi)) > 179.999)
  assert(math.rad("180") > 3.141 and math.rad("180") < 3.142)
end
do
  local intpart, fracpart = math.modf(1.2)
  assert(intpart == 1 and math.type(intpart) == "integer")
  assert(fracpart > 0 and fracpart < 1 and math.type(fracpart) == "float")
end

do
  local named = setmetatable({}, { __name = "Lua54Smoke" })
  local nonstr = setmetatable({}, { __name = 123 })
  assert(type(named) == "table")
  assert(tostring(named):match("^Lua54Smoke: ") ~= nil)
  assert(tostring(nonstr):match("^table: ") ~= nil)
  local ok, err = pcall(math.abs, setmetatable({}, { __name = "Lua54Number" }))
  assert(ok == false and err:match("Lua54Number") ~= nil)
  ok, err = pcall(coroutine.resume, named)
  assert(ok == false and err:match("thread expected") and err:match("Lua54Smoke"))
  ok, err = pcall(coroutine.close, named)
  assert(ok == false and err:match("thread expected") and err:match("Lua54Smoke"))
  ok, err = pcall(coroutine.isyieldable, named)
  assert(ok == false and err:match("thread expected") and err:match("Lua54Smoke"))
end
do
  local co = coroutine.create(function()
    assert(coroutine.isyieldable() == true)
    coroutine.yield("paused")
  end)
  assert(coroutine.isyieldable() == false)
  assert(coroutine.isyieldable(co) == true)
  assert(select(1, coroutine.resume(co)) == true)
  assert(coroutine.status(co) == "suspended")
  assert(coroutine.isyieldable(co) == true)
  assert(select(1, coroutine.resume(co)) == true)
  assert(coroutine.status(co) == "dead")
  assert(coroutine.isyieldable(co) == true)
end
do
  for _, s in ipairs({
    "inf", "Inf", "INF", "+inf", "-inf", "infinity",
    "nan", "NaN", "NAN", "+nan", "-nan", "  inf  "
  }) do
    assert(tonumber(s) == nil)
  end
  assert(tonumber("0b10") == nil)
  assert(tonumber("0B10") == nil)
  assert(tonumber("+0b10") == nil)
  assert(tonumber("-0B10") == nil)
  assert(tonumber("  0b10  ") == nil)
  assert(tonumber("0x10", 16) == nil)
  assert(tonumber("10", 16) == 16)
  assert(tonumber("0x10", 34) == 38182)
  local ok, err = pcall(tonumber, "10", 2.5)
  assert(ok == false and err:match("integer representation") ~= nil)
  assert(tonumber("10", "2") == 2)
end
do
  for _, s in ipairs({ "inf", "NaN", "0b10" }) do
    local ok, err = pcall(math.abs, s)
    assert(ok == false and err:match("number expected") ~= nil)
    assert(math.tointeger(s) == nil)
    assert(math.type(s) == nil)
  end
  assert(math.abs("1e9999") == math.huge)
end
do
  assert(select(1, pcall(string.format, "%d", 1.2)) == false)
  local ok, err = pcall(string.format, "%d", "1.2")
  assert(ok == false and err:match("string%.format") ~= nil and
         err:match("integer representation") ~= nil)
  assert(string.format("%d", 12.0) == "12")
  assert(string.format("%q", nil) == "nil")
  assert(string.format("%q", true) == "true")
  assert(string.format("%q", 1) == "1")
  assert(string.format("%q", 1.5) == "0x1.8p+0")
  assert(string.format("%q", -0.0) == "-0x0p+0")
  assert(string.format("%q", 0 / 0) == "(0/0)")
  assert(string.format("%q", math.huge) == "1e9999")
  assert(string.format("%q", -math.huge) == "-1e9999")
  assert(select(1, pcall(string.format, "%q", {})) == false)
  assert(select(1, pcall(string.format, "%q",
    setmetatable({}, { __tostring = function() return "x" end }))) == false)
  assert(select(1, pcall(string.format, "%c", 65.5)) == false)
  assert(string.format("%c", "65") == "A")
  assert(string.format("%p", nil) == "(null)")
  assert(string.format("%p", false) == "(null)")
  assert(string.format("%p", true) == "(null)")
  assert(string.format("%p", 1) == "(null)")
  assert(string.format("%p", "x") ~= "(null)")
end
do
  local function expect_bad_integer(f, ...)
    local ok, err = pcall(f, ...)
    assert(ok == false and type(err) == "string" and
           err:match("integer representation") ~= nil)
  end
  expect_bad_integer(string.byte, "abc", 1.2)
  expect_bad_integer(string.byte, "abc", 1, 2.2)
  assert(string.byte("abc", "2") == 98)
  expect_bad_integer(string.char, 65.2)
  local ok, err = pcall(string.char, 256)
  assert(ok == false and err:match("value out of range") ~= nil)
  expect_bad_integer(string.sub, "abc", 1.2)
  expect_bad_integer(string.sub, "abc", 1, 2.2)
  expect_bad_integer(string.rep, "a", 1.2)
  expect_bad_integer(string.find, "abc", "b", 1.2)
  expect_bad_integer(string.match, "abc", "b", 1.2)
  expect_bad_integer(string.gmatch, "abc", "b", 1.2)
  expect_bad_integer(string.gsub, "aaa", "a", "b", 1.2)
end
do
  assert(debug.getuservalue(io.stdout) == nil)
  assert(debug.getuservalue(io.stdout, 1) == nil)
  assert(debug.setuservalue(io.stdout, {}, 1) == nil)
end
do
  for _, item in ipairs({
    { "debug.getinfo", function() return pcall(debug.getinfo, nil) end },
    { "debug.getinfo", function() return pcall(debug.getinfo, 1, true) end },
    { "debug.getlocal", function() return pcall(debug.getlocal, nil, 1) end },
    { "debug.setlocal", function() return pcall(debug.setlocal, nil, 1, true) end },
    { "debug.getupvalue", function() return pcall(debug.getupvalue, nil, 1) end },
    { "debug.setupvalue", function() return pcall(debug.setupvalue, nil, 1, true) end },
    { "debug.upvalueid", function() return pcall(debug.upvalueid, nil, 1) end },
    { "debug.upvaluejoin", function() return pcall(debug.upvaluejoin, nil, 1, nil, 1) end },
    { "debug.sethook", function() return pcall(debug.sethook, true) end },
    { "debug.getuservalue", function() return pcall(debug.getuservalue, nil) end },
    { "debug.setuservalue", function() return pcall(debug.setuservalue, nil, {}) end },
    { "debug.setcstacklimit", function() return pcall(debug.setcstacklimit, nil) end },
  }) do
    local ok, err = item[2]()
    assert(ok == false and err:match("to '"..item[1].."'") ~= nil)
  end
end
do
  local ok, err = pcall(table.concat, nil)
  assert(ok == false and err:match("to 'table%.concat'") ~= nil)
  ok, err = pcall(table.concat, {}, true)
  assert(ok == false and err:match("to 'table%.concat'") ~= nil)
  ok, err = pcall(table.insert, nil, 1)
  assert(ok == false and err:match("to 'table%.insert'") ~= nil)
  ok, err = pcall(table.remove, nil)
  assert(ok == false and err:match("to 'table%.remove'") ~= nil)
  ok, err = pcall(table.sort, nil)
  assert(ok == false and err:match("to 'table%.sort'") ~= nil)
  ok, err = pcall(table.sort, {}, true)
  assert(ok == false and err:match("to 'table%.sort'") ~= nil)
  ok, err = pcall(table.move)
  assert(ok == false and err:match("to 'table%.move'") ~= nil)
end
do
  local t = setmetatable({ 1 }, { __len = function() return 3 end })
  local ok, err = pcall(table.concat, t, ",")
  assert(ok == false and err:match("index 2") ~= nil)
  t = setmetatable({ 1 }, { __len = function() return 1.2 end })
  ok, err = pcall(table.concat, t, ",")
  assert(ok == false and err:match("object length is not an integer") ~= nil)
end
do
  local ok, err = pcall(table.concat, { 1, nil, 3 }, ",")
  assert(ok == false and err:match("index 2") ~= nil)
  local proxy = setmetatable({}, {
    __len = function() return 3 end,
    __index = function(_, k) return tostring(k) end,
  })
  assert(table.concat(proxy, ",") == "1,2,3")
  ok, err = pcall(table.concat, { 1, 2, 3 }, ",", 1.2, 2)
  assert(ok == false and err:match("number has no integer representation") ~= nil)
  ok, err = pcall(table.concat, { 1, 2, 3 }, ",", 1, 2.2)
  assert(ok == false and err:match("number has no integer representation") ~= nil)
  assert(table.concat({}, ",", 1, 0) == "")
  assert(table.concat({ 1, 2 }, ",", 2, 1) == "")
end
do
  local ok, err = pcall(table.insert, { 1, 2 }, 1.2, "x")
  assert(ok == false and err:match("integer representation") ~= nil)
  ok, err = pcall(table.insert, { 1, 2 }, 0, "x")
  assert(ok == false and err:match("position out of bounds") ~= nil)
  ok, err = pcall(table.insert, { 1, 2 }, 5, "x")
  assert(ok == false and err:match("position out of bounds") ~= nil)
  local t = setmetatable({ 1, nil, 3 }, { __len = function() return 3 end })
  table.insert(t, "x")
  assert(t[1] == 1 and t[2] == nil and t[3] == 3 and t[4] == "x")
  local with_hole = { 1, nil, 3 }
  table.insert(with_hole, "x")
  assert(with_hole[1] == 1 and with_hole[2] == nil and
         with_hole[3] == 3 and with_hole[4] == "x")
  local base = { 1, 2, 3 }
  local proxy = setmetatable({}, {
    __len = function() return 3 end,
    __index = function(_, k) return base[k] end,
    __newindex = function(_, k, v) base[k] = v end,
  })
  table.insert(proxy, 2, "x")
  assert(base[1] == 1 and base[2] == "x" and base[3] == 2 and base[4] == 3)
end
do
  local ok, err = pcall(table.remove, { 1, 2 }, 1.2)
  assert(ok == false and err:match("integer representation") ~= nil)
  ok, err = pcall(table.remove, { 1, 2 }, 0)
  assert(ok == false and err:match("position out of bounds") ~= nil)
  ok, err = pcall(table.remove, { 1, 2 }, 4)
  assert(ok == false and err:match("position out of bounds") ~= nil)
  local t = { 1, 2 }
  assert(table.remove(t, 3) == nil and t[1] == 1 and t[2] == 2)
  assert(table.remove({}, 0) == nil)
  local with_hole = { 1, nil, 3 }
  assert(table.remove(with_hole) == 3)
  assert(with_hole[1] == 1 and with_hole[2] == nil and with_hole[3] == nil)
  local base = { 1, 2, 3 }
  local proxy = setmetatable({}, {
    __len = function() return 3 end,
    __index = function(_, k) return base[k] end,
    __newindex = function(_, k, v) base[k] = v end,
  })
  assert(table.remove(proxy, 2) == 2)
  assert(base[1] == 1 and base[2] == 3 and base[3] == nil)
end
do
  local ok, err = pcall(table.move, { 1, 2 }, 1.2, 2, 1, {})
  assert(ok == false and err:match("integer representation") ~= nil)
  ok, err = pcall(table.move, { 1, 2 }, 1, 2.2, 1, {})
  assert(ok == false and err:match("integer representation") ~= nil)
  ok, err = pcall(table.move, { 1, 2 }, 1, 2, 1.2, {})
  assert(ok == false and err:match("integer representation") ~= nil)
  ok, err = pcall(table.move)
  assert(ok == false and err:match("bad argument #2") ~= nil)
  ok, err = pcall(table.move, nil, 1, 0, 1)
  assert(ok == false and err:match("bad argument #1") ~= nil)
  local dst = {}
  table.move({ 1, 2 }, "1", 2, "2", dst)
  assert(dst[2] == 1 and dst[3] == 2)
  local src = setmetatable({}, {
    __index = function(_, k) return k * 10 end
  })
  dst = setmetatable({}, {
    __newindex = function(t, k, v) rawset(t, k, v + 1) end
  })
  table.move(src, 1, 2, 3, dst)
  assert(dst[3] == 11 and dst[4] == 21)
end
do
  local t = setmetatable({ 3, 2, 1 }, { __len = function() return 2 end })
  table.sort(t)
  assert(t[1] == 2 and t[2] == 3 and t[3] == 1)
  t = setmetatable({ 3, 2, 1 }, { __len = function() return 1.2 end })
  local ok, err = pcall(table.sort, t)
  assert(ok == false and err:match("object length is not an integer") ~= nil)
  ok, err = pcall(table.sort, { 3, 2, 1, 0 },
                  function(a, b) return a <= b end)
  assert(ok == false and err:match("invalid order function for sorting") ~= nil)
  ok, err = pcall(table.sort, { 1, 2, 3, 4 },
                  function(a, b) return a >= b end)
  assert(ok == false and err:match("invalid order function for sorting") ~= nil)
  ok, err = pcall(table.sort, { 2, 1 }, function()
    error("sort cmp boom", 0)
  end)
  assert(ok == false and err == "sort cmp boom")
  local always_true = { 1, 2, 3 }
  table.sort(always_true, function() return true end)
  assert(table.concat(always_true, ",") == "2,3,1")
  local always_false = { 3, 2, 1 }
  table.sort(always_false, function() return false end)
  assert(table.concat(always_false, ",") == "3,2,1")
  local reads, writes, base = {}, {}, { 3, 2, 1 }
  local proxy = setmetatable({}, {
    __len = function() return 3 end,
    __index = function(_, k) reads[#reads+1] = k; return base[k] end,
    __newindex = function(_, k, v) writes[#writes+1] = k; base[k] = v end,
  })
  table.sort(proxy)
  assert(base[1] == 1 and base[2] == 2 and base[3] == 3)
  assert(#reads > 0 and #writes > 0)
end
assert(load("return 0b1010") == nil)
assert(load("return 1L") == nil)
assert(load("return 1LL") == nil)
assert(load("return 1UL") == nil)
assert(load("return 1ULL") == nil)
assert(load("return 1uLL") == nil)
assert(load("return 1i") == nil)

do
  local s = string.char(0x61, 0xc2, 0xa2, 0xe2, 0x82, 0xac,
                        0xf0, 0x90, 0x8d, 0x88)
  local bad = string.char(0xc0, 0x80)
  local extended = utf8.char(0x200000)
  local surrogate = string.char(0xed, 0xa0, 0x80)
  local cps = { utf8.codepoint(s, 1, -1) }
  local seen = {}
  local lax_seen = {}
  assert(type(utf8) == "table")
  assert(type(utf8.charpattern) == "string")
  for _, item in ipairs({
    { "utf8.char", function() return pcall(utf8.char, nil) end },
    { "utf8.codes", function() return pcall(utf8.codes, nil) end },
    { "utf8.codepoint", function() return pcall(utf8.codepoint, nil) end },
    { "utf8.len", function() return pcall(utf8.len, nil) end },
    { "utf8.offset", function() return pcall(utf8.offset, nil, 1) end },
    { "utf8.offset", function() return pcall(utf8.offset, "a", nil) end },
  }) do
    local ok, err = item[2]()
    assert(ok == false and err:match("to '"..item[1].."'") ~= nil)
  end
  assert(utf8.len(s) == 4)
  assert(cps[1] == 97 and cps[2] == 162 and cps[3] == 8364 and cps[4] == 66376)
  assert(utf8.char(97, 162, 8364, 66376) == s)
  assert(#utf8.char(0x110000) == 4)
  assert(#utf8.char(0x200000) == 5)
  assert(#utf8.char(0x7fffffff) == 6)
  local ok, err = pcall(utf8.char, 0x80000000)
  assert(ok == false and err:match("value out of range") ~= nil)
  assert(select(1, pcall(utf8.char, 97.2)) == false)
  assert(select(1, pcall(utf8.codepoint, s, 1.2)) == false)
  assert(select(1, pcall(utf8.len, s, 1.2)) == false)
  assert(select(1, pcall(utf8.offset, s, 1.2)) == false)
  assert(select(1, pcall(utf8.offset, s, 1, 1.2)) == false)
  assert(utf8.charpattern:find("\253", 1, true) ~= nil)
  for p, c in utf8.codes(s) do
    seen[#seen+1] = p..":"..c
  end
  assert(table.concat(seen, ",") == "1:97,2:162,4:8364,7:66376")
  assert(utf8.offset(s, 1) == 1)
  assert(utf8.offset(s, 2) == 2)
  assert(utf8.offset(s, 3) == 4)
  assert(utf8.offset(s, 4) == 7)
  assert(utf8.offset(s, 5) == #s + 1)
  assert(utf8.offset(s, 6) == nil)
  assert(utf8.offset(s, 0, #s + 1) == #s + 1)
  assert(utf8.offset(s, -1) == 7)
  do
    local n, badpos = utf8.len(bad)
    assert(n == nil and badpos == 1)
  end
  do
    local n, badpos = utf8.len(extended)
    assert(n == nil and badpos == 1)
    assert(utf8.len(extended, 1, -1, true) == 1)
    assert(utf8.codepoint(extended, 1, 1, true) == 0x200000)
    assert(utf8.len(surrogate, 1, -1, true) == 1)
    for p, c in utf8.codes(extended, true) do
      lax_seen[#lax_seen+1] = p..":"..c
    end
    assert(table.concat(lax_seen, ",") == "1:2097152")
  end
  assert(select(1, pcall(function()
    for _ in utf8.codes(bad) do end
  end)) == false)
end

do
  local old = debug.setcstacklimit(200)
  assert(type(old) == "number")
  assert(debug.setcstacklimit(0) == old)
  assert(debug.setcstacklimit(-1) == old)
  assert(select(1, pcall(debug.setcstacklimit, "x")) == false)
  assert(debug.getinfo(function() end, "t").istailcall == false)
  do
    local info = debug.getinfo(function() end, "r")
    assert(info.ftransfer == 0 and info.ntransfer == 0)
  end
end

do
  local function bytes(s)
    local t = {}
    for i = 1, #s do t[#t+1] = string.byte(s, i) end
    return table.concat(t, ",")
  end
  assert(bytes(string.pack("bBhH", -2, 254, -300, 65000)) ==
         "254,254,212,254,232,253")
  do
    local a, b, c, d, pos = string.unpack("bBhH",
      string.pack("bBhH", -2, 254, -300, 65000))
    assert(a == -2 and b == 254 and c == -300 and d == 65000 and pos == 7)
  end
  assert(bytes(string.pack("<i2I2", -2, 65534)) == "254,255,254,255")
  assert(bytes(string.pack(">i2I2", -2, 65534)) == "255,254,255,254")
  do
    local a, b, pos = string.unpack(">i2I2", string.pack(">i2I2", -2, 65534))
    assert(a == -2 and b == 65534 and pos == 5)
  end
  do
    local a, b, pos = string.unpack("<i4I4", string.pack("<i4I4", -123456, 4000000000))
    assert(a == -123456 and b == 4000000000 and pos == 9)
  end
  do
    local s = string.pack("fd", 1.5, -2.25)
    local a, b, pos = string.unpack("fd", s)
    assert(a == 1.5 and b == -2.25 and pos == #s + 1)
  end
  assert(string.pack("c4", "abcd") == "abcd")
  assert(string.unpack("c4", "abcd") == "abcd")
  assert(bytes(string.pack("c4", "a")) == "97,0,0,0")
  assert(string.pack("z", "hi") == "hi\0")
  do
    local a, pos = string.unpack("z", "hi\0tail")
    assert(a == "hi" and pos == 4)
  end
  assert(bytes(string.pack("s1", "abc")) == "3,97,98,99")
  do
    local a, pos = string.unpack("s1", "\3abcx")
    assert(a == "abc" and pos == 5)
  end
  assert(string.packsize("jT") == 16)
  do
    local a, b, pos = string.unpack("<jT", string.pack("<jT", -2, 5))
    assert(a == -2 and b == 5 and pos == 17)
  end
  assert(string.packsize("!8bi8") == 16)
  assert(bytes(string.pack("!8bi8", 1, 2)) ==
         "1,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0")
  do
    local a, b, pos = string.unpack("!8bi8", string.pack("!8bi8", 1, 2))
    assert(a == 1 and b == 2 and pos == 17)
  end
  assert(string.packsize("!8bXi8") == 8)
  assert(bytes(string.pack("!8bXi8", 1)) == "1,0,0,0,0,0,0,0")
  do
    local a, pos = string.unpack("!8bXi8", string.pack("!8bXi8", 1))
    assert(a == 1 and pos == 9)
  end
  assert(string.packsize("lL") == 8)
  do
    local a, b, pos = string.unpack("<lL", string.pack("<lL", -2, 5))
    assert(a == -2 and b == 5 and pos == 9)
  end
  assert(bytes(string.pack("x b", 7)) == "0,7")
  assert(string.unpack("x b", "\0\7") == 7)
  assert(string.packsize("bBhH<i2I2<i4I4fdc4x") == 35)
  assert(select(1, pcall(string.packsize, "z")) == false)
  assert(select(1, pcall(string.pack, "b", 1.2)) == false)
  assert(select(1, pcall(string.pack, "b", 128)) == false)
  assert(select(1, pcall(string.unpack, "b", "a", 1.2)) == false)
  assert(select(1, pcall(string.unpack, "I4", "\1")) == false)
end

do
  package.preload.__lua54_smoke_module = function()
    return { ok = true }
  end
  local m, loaderdata = require("__lua54_smoke_module")
  assert(m.ok == true)
  assert(loaderdata == ":preload:")
  package.loaded.__lua54_smoke_module = nil
  package.preload.__lua54_smoke_module = nil
end

do
  local found, searcherr = package.searchpath("__lua54_missing__", "nope/?.lua;none/?.lua")
  assert(found == nil)
  assert(not searcherr:match("^\n\t"))
  assert(searcherr:match("\n\tno file") ~= nil)
  local preloaderr = package.searchers[1]("__lua54_missing_preload__")
  assert(type(preloaderr) == "string" and not preloaderr:match("^\n\t"))
  local old_searchers = package.searchers
  package.searchers = {
    function() return "custom missing" end
  }
  local ok, err = pcall(require, "__lua54_missing_custom__")
  assert(ok == false and err:match("module '__lua54_missing_custom__' not found:\n\tcustom missing"))
  package.searchers = old_searchers
end

do
  local old_searchers = package.searchers
  package.searchers = {
    function()
      return function(name, loaderdata)
        return { name = name, loaderdata = loaderdata }
      end, "loader-data"
    end
  }
  local m, loaderdata = require("__lua54_loader_data")
  assert(m.name == "__lua54_loader_data")
  assert(m.loaderdata == "loader-data")
  assert(loaderdata == "loader-data")
  do
    local again, again_data = require("__lua54_loader_data")
    assert(again == m and again_data == nil)
  end
  package.loaded.__lua54_loader_data = nil
  package.searchers = old_searchers
end

do
  local co, ismain = coroutine.running()
  assert(type(co) == "thread")
  assert(ismain == true)
end

do
  local co = coroutine.create(function()
    coroutine.yield("paused")
    return "done"
  end)
  local ok, value = coroutine.resume(co)
  assert(ok == true and value == "paused")
  assert(coroutine.status(co) == "suspended")
  assert(coroutine.close(co) == true)
  assert(coroutine.status(co) == "dead")
  assert(select(1, coroutine.resume(co)) == false)
  assert(select(1, pcall(coroutine.close, coroutine.running())) == false)
  do
    local done = coroutine.create(function() return "done" end)
    assert(select(1, coroutine.resume(done)) == true)
    assert(coroutine.status(done) == "dead")
    assert(coroutine.close(done) == true)
  end
  do
    local bad = coroutine.create(function() error("lua54 close error") end)
    assert(select(1, coroutine.resume(bad)) == false)
    local closed, err = coroutine.close(bad)
    assert(closed == false)
    assert(type(err) == "string" and err:match("lua54 close error"))
  end
end

do
  local t = setmetatable({}, {
    __index = function(_, k)
      if k == 1 then return "via-index" end
    end
  })
  local i, v = ipairs(t)(t, 0)
  assert(i == 1 and v == "via-index")
end

do
  local a, b = math.randomseed(1, 2)
  assert(a == 1 and b == 2)
  local s1, s2 = math.randomseed()
  assert(math.tointeger(s1) == s1 and math.tointeger(s2) == s2)
  assert(s1 ~= 0 or s2 ~= 0)
  local full = math.random(0)
  if math.type(1) == "integer" then
    assert(math.type(full) == "integer")
  end
  assert(math.tointeger(full) == full)
  assert(full >= math.mininteger and full <= math.maxinteger)
  assert(math.random(5, 5) == 5)
  assert(select(1, pcall(math.random, -1)) == false)
  assert(select(1, pcall(math.random, 10, 5)) == false)
  assert(select(1, pcall(math.random, 1.5)) == false)
  assert(select(1, pcall(math.random, 1, 2, 3)) == false)
end

do
  local t = setmetatable({ 1, 2, 3 }, { __len = function() return 54 end })
  assert(#t == 54)
end
do
  local log = {}
  do
    local a = setmetatable({}, { __gc = function() log[#log+1] = "a" end })
    local b = setmetatable({}, { __gc = function() log[#log+1] = "b" end })
    assert(a and b)
    a, b = nil, nil
  end
  collectgarbage()
  collectgarbage()
  assert(table.concat(log, ",") == "b,a")

  log = {}
  do
    local mt = {}
    local t = setmetatable({}, mt)
    mt.__gc = function() log[#log+1] = "late" end
    t = nil
  end
  collectgarbage()
  collectgarbage()
  assert(#log == 0)

  do
    local mt = { __gc = function() log[#log+1] = "old" end }
    local t = setmetatable({}, mt)
    mt.__gc = function() log[#log+1] = "new" end
    t = nil
  end
  collectgarbage()
  collectgarbage()
  assert(log[1] == "new" and log[2] == nil)

  log = {}
  do
    local mt = { __gc = function() log[#log+1] = "removed" end }
    local t = setmetatable({}, mt)
    mt.__gc = nil
    t = nil
  end
  collectgarbage()
  collectgarbage()
  assert(#log == 0)
end
do
  local function count(t)
    local n = 0
    for _ in pairs(t) do n = n + 1 end
    return n
  end

  local weak_keys = setmetatable({}, { __mode = "k" })
  do
    local key = {}
    weak_keys[key] = { key = key }
    key = nil
  end
  for _ = 1, 6 do collectgarbage() end
  assert(count(weak_keys) == 0)

  local live_keys = setmetatable({}, { __mode = "k" })
  local live = {}
  live_keys[live] = { key = live }
  for _ = 1, 6 do collectgarbage() end
  assert(count(live_keys) == 1 and live_keys[live] ~= nil)
end

do
  local ok, err = pcall(os.date, true)
  assert(ok == false and err:match("to 'os%.date'") ~= nil)
  ok, err = pcall(os.date, "%c", true)
  assert(ok == false and err:match("to 'os%.date'") ~= nil)
  ok, err = pcall(os.difftime, 1, true)
  assert(ok == false and err:match("to 'os%.difftime'") ~= nil)
  ok, err = pcall(os.execute, true)
  assert(ok == false and err:match("to 'os%.execute'") ~= nil)
  ok, err = pcall(os.getenv, true)
  assert(ok == false and err:match("to 'os%.getenv'") ~= nil)
  ok, err = pcall(os.remove, true)
  assert(ok == false and err:match("to 'os%.remove'") ~= nil)
  ok, err = pcall(os.rename, true, false)
  assert(ok == false and err:match("to 'os%.rename'") ~= nil)
  ok, err = pcall(os.setlocale, true)
  assert(ok == false and err:match("to 'os%.setlocale'") ~= nil)
  ok, err = pcall(os.setlocale, nil, true)
  assert(ok == false and err:match("to 'os%.setlocale'") ~= nil)
  ok, err = pcall(os.time, true)
  assert(ok == false and err:match("to 'os%.time'") ~= nil)
end

do
  local missing = "__lua54_rename_missing__"
  local ok, msg, code = os.rename(missing, "__lua54_rename_target__")
  assert(ok == nil and type(msg) == "string" and type(code) == "number")
  assert(msg:match(missing) == nil)
end

do
  warn("@off")
  warn("ignored warning")
  warn("@on")
  warn("@off")
  local ok, err = pcall(warn, 1)
  assert(ok == true)
  ok, err = pcall(warn, true)
  assert(ok == false)
  assert(type(err) == "string" and err:match("string expected"))
  ok, err = pcall(warn)
  assert(ok == false)
  assert(type(err) == "string" and err:match("string expected"))
  warn("@off")
end

do
  local fname = "lua54_lines_smoke.tmp"
  local f = assert(io.open(fname, "w"))
  f:write("line\n")
  f:close()
  local iter, state, ctrl, closing = io.lines(fname)
  assert(type(iter) == "function" and state == nil and ctrl == nil)
  assert(io.type(closing) == "file")
  assert(iter() == "line")
  assert(iter() == nil)
  assert(io.type(closing) == "closed file")
  os.remove(fname)
end

do
  local first = string.gmatch("abcabc", "a", 2)
  local second = string.gmatch("abcabc", "a", 4)
  local none = string.gmatch("abcabc", "a", -2)
  assert(first() == "a")
  assert(second() == "a")
  assert(none() == nil)
end

do
  local function plus(a)
    return a + 1
  end
  local full = string.dump(plus, false)
  local stripped = string.dump(plus, true)
  assert(type(full) == "string" and #full > 0)
  assert(type(stripped) == "string" and #stripped > 0 and #stripped <= #full)
  local loaded = assert(load(stripped, "=dumped", "b"))
  assert(loaded(41) == 42)
  local upvalue = 42
  local with_upvalue = string.dump(function() return upvalue end, true)
  loaded = assert(load(with_upvalue, "=dumped-upvalue", "b"))
  assert(loaded() == _G)
  local dump_env = { marker = "dump-env" }
  loaded = assert(load(with_upvalue, "=dumped-upvalue", "b", dump_env))
  assert(loaded() == dump_env)
  local n, v = debug.getupvalue(loaded, 1)
  assert(n == "" and v == dump_env)
  assert(debug.getupvalue(loaded, 2) == nil)
  loaded = assert(load(string.dump(function() return 54 end, true), "=dumped-plain", "b"))
  assert(debug.getupvalue(loaded, 1) == nil)
  loaded = assert(load(string.dump(function() return math.type(1) end, true), "=dumped-global", "b"))
  n, v = debug.getupvalue(loaded, 1)
  assert(n == "_ENV" and v == _G)
  assert(debug.getupvalue(loaded, 2) == nil)
  local f, err = load(stripped, "=dumped", "t")
  assert(f == nil and err:match("attempt to load a binary chunk %(mode is 't'%)"))
  f, err = load("return 1", "=text", "b")
  assert(f == nil and err:match("attempt to load a text chunk %(mode is 'b'%)"))
  -- Prebuilt Lua 5.4.8 dump of: function() return 54 end.
  -- The compat mode supports LuaJIT bytecode only, so reject official chunks
  -- explicitly instead of silently accepting an incompatible format.
  local official54 = string.char(
    27,76,117,97,84,0,25,147,13,10,26,10,4,8,8,120,
    86,0,0,0,0,0,0,0,0,0,0,0,40,119,64,0,
    128,129,129,0,0,2,131,1,128,26,128,72,0,2,
    0,71,0,1,0,128,128,128,128,128,128,128)
  f, err = load(official54, "=official54", "b")
  assert(f == nil and err:match("cannot load incompatible bytecode"))
  f, err = load(official54, "=official54", "t")
  assert(f == nil and err:match("attempt to load a binary chunk %(mode is 't'%)"))
end

do
  local ok_util, jutil = pcall(require, "jit.util")
  local ok_opt, jitopt = pcall(require, "jit.opt")
  if ok_util and ok_opt and jit.status() then
    local function trace_highwater()
      local n = 0
      for i = 1, 1000 do
        if jutil.traceinfo(i) then n = i end
      end
      return n
    end
    local lua54_loop = assert(load([[
      return function(n)
        local _ENV = { bias = 3 }
        local sum = 0
        for i = 1, n do
          sum = sum + (i // 2) + ((i & 3) | bias)
        end
        return sum
      end
    ]]))()
    local function random_loop(n)
      math.randomseed(123, 456)
      local sum = 0
      for i = 1, n do
        local v = math.random(1, 4)
        if math.tointeger(v) ~= v or v < 1 or v > 4 then return false end
        sum = sum + v
      end
      return sum >= n and sum <= 4*n
    end
    local function reject_number_string_loop(n)
      -- Keep scanner-only number spellings rejected after this path is traced.
      local bad = { "inf", "nan", "0b10" }
      local c = 0
      for i = 1, n do
        if tonumber(bad[(i % 3) + 1]) == nil then
          c = c + 1
        end
      end
      return c
    end
    local function string_meta_arith_loop(n)
      local sum = 0
      for i = 1, n do
        sum = sum + ("1" + i)
      end
      return sum
    end
    jit.flush()
    jit.on()
    jitopt.start("hotloop=1")
    local before = trace_highwater()
    assert(lua54_loop(80) == 1840)
    assert(lua54_loop(80) == 1840)
    assert(trace_highwater() > before)
    jit.flush()
    before = trace_highwater()
    assert(random_loop(120) == true)
    assert(random_loop(120) == true)
    assert(trace_highwater() > before)
    jit.flush()
    before = trace_highwater()
    assert(reject_number_string_loop(80) == 80)
    assert(reject_number_string_loop(80) == 80)
    assert(trace_highwater() > before)
    jit.flush()
    local string_mt = debug.getmetatable("")
    debug.setmetatable("", {
      __add = function(a, b) return tonumber(a) + b + 100 end,
    })
    before = trace_highwater()
    assert(string_meta_arith_loop(80) == 11320)
    assert(string_meta_arith_loop(80) == 11320)
    assert(trace_highwater() > before)
    debug.setmetatable("", string_mt)
    jit.flush()
    jitopt.start("hotloop=56")
  end
end
