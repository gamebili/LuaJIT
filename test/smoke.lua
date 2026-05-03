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
do
  assert(collectgarbage("generational") == "generational")
  assert(collectgarbage("incremental") == "generational")
  assert(collectgarbage("incremental") == "incremental")
  assert(collectgarbage("generational") == "incremental")
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
  assert(assert(load("local x <close> = false; return x"))() == false)
end
do
  assert(assert(load("local _ENV = { x = 42 }; return x"))() == 42)
  assert(assert(load("local _ENV = {}; x = 7; return _ENV.x"))() == 7)
  assert(assert(load("local _ENV = { f = function() return 3 end }; return f()"))() == 3)
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
  assert(select(1, pcall(assert(load("return 3.5 & 1")))) == false)
  assert(select(1, pcall(assert(load([[return "3" & 1]])))) == false)
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
assert(math.type() == nil)
assert(math.type(nil) == nil)
assert(math.type("1") == nil)
assert(math.type(1.5) == "float")
assert(math.type(1) == "integer" or math.type(1) == "float")
assert(math.maxinteger == 2147483647)
assert(math.mininteger == -2147483648)
assert(math.maxinteger > 0 and math.mininteger < 0)
assert(type(math.tointeger) == "function")
assert(type(math.ult) == "function")
assert(select(1, pcall(math.tointeger)) == false)
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
assert(math.type(math.floor(1.2)) == "integer")
assert(math.floor("1.2") == 1)
assert(math.type(math.ceil(1.2)) == "integer")
do
  local intpart, fracpart = math.modf(1.2)
  assert(intpart == 1 and math.type(intpart) == "integer")
  assert(fracpart > 0 and fracpart < 1 and math.type(fracpart) == "float")
end

do
  assert(tostring(setmetatable({}, { __name = "Lua54Smoke" })):match("^Lua54Smoke: ") ~= nil)
  local ok, err = pcall(math.abs, setmetatable({}, { __name = "Lua54Number" }))
  assert(ok == false and err:match("Lua54Number") ~= nil)
end
do
  assert(tonumber("0x10", 16) == nil)
  assert(tonumber("10", 16) == 16)
  assert(tonumber("0x10", 34) == 38182)
end
do
  assert(select(1, pcall(string.format, "%d", 1.2)) == false)
  assert(string.format("%d", 12.0) == "12")
  assert(string.format("%q", 1.5) == "0x1.8p+0")
  assert(string.format("%p", nil) == "(null)")
end
do
  assert(debug.getuservalue(io.stdout) == nil)
  assert(debug.getuservalue(io.stdout, 1) == nil)
  assert(debug.setuservalue(io.stdout, {}, 1) == nil)
end
do
  local t = setmetatable({ 1 }, { __len = function() return 3 end })
  local ok, err = pcall(table.concat, t, ",")
  assert(ok == false and err:match("index 2") ~= nil)
end
assert(load("return 0b1010") == nil)
assert(load("return 1LL") == nil)
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
  assert(utf8.len(s) == 4)
  assert(cps[1] == 97 and cps[2] == 162 and cps[3] == 8364 and cps[4] == 66376)
  assert(utf8.char(97, 162, 8364, 66376) == s)
  assert(#utf8.char(0x110000) == 4)
  assert(#utf8.char(0x200000) == 5)
  assert(#utf8.char(0x7fffffff) == 6)
  assert(select(1, pcall(utf8.char, 0x80000000)) == false)
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
  assert(select(1, pcall(string.pack, "b", 128)) == false)
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
  local first = string.gmatch("abcabc", "a", 2)
  local second = string.gmatch("abcabc", "a", 4)
  local none = string.gmatch("abcabc", "a", -2)
  assert(first() == "a")
  assert(second() == "a")
  assert(none() == nil)
end
