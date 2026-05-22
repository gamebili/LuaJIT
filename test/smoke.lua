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
do
  local f, err = load("a")
  assert(f == nil and err:find("syntax error near <eof>", 1, true), err)
end
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
  local wide_proxy = setmetatable({}, {
    __index = function(_, k) return k end,
  })
  n, a, b = select("#", table.unpack(wide_proxy, -1099511627776,
				     -1099511627775)),
	    table.unpack(wide_proxy, -1099511627776, -1099511627775)
  assert(n == 2 and a == -1099511627776 and b == -1099511627775)
  assert(math.type(a) == "integer" and math.type(b) == "integer")
  assert(select("#", table.unpack({}, math.maxinteger, math.mininteger)) == 0)
  local ok, err = pcall(table.unpack, {}, 1, math.maxinteger)
  assert(ok == false and err:find("too many results to unpack", 1, true))
  ok, err = pcall(table.unpack, {}, math.mininteger, math.maxinteger)
  assert(ok == false and err:find("too many results to unpack", 1, true))
  assert(select(1, pcall(table.unpack, {}, 1.2)) == false)
  assert(select(1, pcall(table.unpack, {}, 1, 1.2)) == false)
  ok, err = pcall(table.unpack)
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
  assert(bit == nil)
  local ok, mod = pcall(require, "jit.dump")
  assert(ok == true and type(mod) == "table")
  package.loaded["jit.dump"] = nil
  package.loaded.bit = nil
  _G.bit = nil
end
do
  local function result_count(...)
    return select("#", ...), ...
  end
  local n, empty_key = result_count(next({}))
  assert(n == 1 and empty_key == nil)
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
do
  local ok_wide_level, err_wide_level =
    pcall(error, "lua54 wide level", 1099511627776)
  assert(ok_wide_level == false and err_wide_level == "lua54 wide level")
end
do
  local ok, err = pcall(function() error(101) end)
  assert(ok == false and err == 101)
  local marker = { tag = "lua54-error-object" }
  ok, err = pcall(function() error(marker) end)
  assert(ok == false and err == marker)
  ok, err = pcall(assert, false, 123)
  assert(ok == false and err == 123)
  ok, err = pcall(assert, false, marker)
  assert(ok == false and err == marker)
end
assert(select(1, pcall(getmetatable)) == false)
assert(select(1, pcall(select, 1.2, "a", "b")) == false)
assert(select(1, pcall(select, -1.2, "a", "b")) == false)
assert(select("#", select(1099511627776, "a", "b")) == 0)
assert(select("#", select("1099511627776", "a", "b")) == 0)
assert(select("#", select(math.maxinteger, "a", "b")) == 0)
ok, err = pcall(select, -1099511627776, "a", "b")
assert(ok == false and err:match("index out of range") ~= nil)
do
  local ok, err = pcall(select, 0, "a")
  assert(ok == false and err:find("bad argument #1 to 'select'", 1, true) and
	 err:find("index out of range", 1, true))
end
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
  assert(type(ipairs({})) == "function" and ipairs({}) == ipairs({}))
  do
    local function result_count(...)
      return select("#", ...), ...
    end
    local iter = ipairs({})
    local n, empty_key = result_count(iter({}, 0))
    assert(n == 1 and empty_key == nil)
    local k, v = iter({ [math.mininteger] = 10 }, math.maxinteger)
    assert(k == math.mininteger and v == 10)
    n, k, v = result_count(iter({ [math.mininteger] = 10 }, k))
    assert(n == 1 and k == nil and v == nil)
  end
  do
    local i64 = 2147483648
    assert(i64 == 2147483647 + 1)
    assert(math.mininteger == math.mininteger + 0)
    assert(rawequal(i64, 2147483647 + 1))
  end
  ok, err = pcall(setmetatable)
  assert(ok == false and err:match("to 'setmetatable'") ~= nil)
  ok, err = pcall(setmetatable, {})
  assert(ok == false and
         err:find("to 'setmetatable' (nil or table expected, got no value)",
                  1, true) ~= nil)
  assert(pcall(setmetatable, {}, nil) == true)
  ok, err = pcall(function() return setmetatable({}) end)
  assert(ok == false and
         err:find("to 'setmetatable' (nil or table expected, got no value)",
                  1, true) ~= nil)
  do
    local f = setmetatable
    ok, err = pcall(function() return f({}) end)
    assert(ok == false and
           err:find("to 'f' (nil or table expected, got no value)",
                    1, true) ~= nil)
    assert(pcall(function() return f({}, nil) end) == true)
  end
  ok, err = pcall(getmetatable)
  assert(ok == false and err:match("to 'getmetatable'") ~= nil)
  local iter, state, key = pairs(1)
  assert(iter == next and state == 1 and key == nil)
  ok, err = pcall(iter, state, key)
  assert(ok == false and err:match("to 'next'") ~= nil)
  do
    local function check_pairs_iter_name(name, fn)
      local ok, err = pcall(fn)
      assert(ok == false and
	     tostring(err):find("bad argument #1 to '"..name.."'", 1, true))
    end
    check_pairs_iter_name("iter", function()
      local iter, state, key = pairs(nil)
      return iter(state, key)
    end)
    check_pairs_iter_name("iter", function()
      local p = pairs
      local iter, state, key = p(nil)
      return iter(state, key)
    end)
    do
      local p = pairs
      check_pairs_iter_name("iter", function()
	local iter, state, key = p(nil)
	return iter(state, key)
      end)
    end
    check_pairs_iter_name("f", function()
      local iter, state, key = pairs(nil)
      local f = iter
      return f(state, key)
    end)
    lua54_pairs_iter = nil
    check_pairs_iter_name("lua54_pairs_iter", function()
      lua54_pairs_iter = pairs(nil)
      return lua54_pairs_iter(nil, nil)
    end)
    lua54_pairs_iter = nil
    check_pairs_iter_name("iter", function()
      local holder = {}
      holder.iter = pairs(nil)
      return holder.iter(nil, nil)
    end)
  end
  do
    local t = setmetatable({ 10, 20, 30 }, { __pairs = function(obj)
      local inc = coroutine.yield("lua54-pairs-yield")
      return function(state, i)
        if i > 1 then return i - inc, state[i - inc] end
      end, obj, #obj + 1
    end })
    local seen = {}
    local co = coroutine.wrap(function()
      for _, v in pairs(t) do seen[#seen + 1] = v end
      return "done"
    end)
    -- Lua 5.4 runs __pairs through the VM call path, so it can yield here.
    assert(co() == "lua54-pairs-yield")
    assert(co(1) == "done")
    assert(seen[1] == 30 and seen[2] == 20 and seen[3] == 10 and #seen == 3)
  end
  do
    ok, err = pcall(function() for _ in nil do end end)
    assert(ok == false and
	   err:find("attempt to call a nil value (for iterator 'for iterator')",
		    1, true))
    ok, err = pcall(function()
      for _ in pairs(setmetatable({}, { __pairs = function() return true end })) do
      end
    end)
    assert(ok == false and
	   err:find("attempt to call a boolean value (for iterator 'for iterator')",
		    1, true))
  end
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
  do
    ok, err = pcall(xpcall, function() end)
    assert(ok == false and err:find("to 'xpcall' (function expected, got no value)",
                                    1, true) ~= nil)
    ok, err = pcall(xpcall, function() end, nil)
    assert(ok == false and err:find("to 'xpcall' (function expected, got nil)",
                                    1, true) ~= nil)
    ok, err = pcall(function() return xpcall(function() end) end)
    assert(ok == false and err:find("to 'xpcall' (function expected, got no value)",
                                    1, true) ~= nil)
    ok, err = pcall(function() return xpcall(function() end, nil) end)
    assert(ok == false and err:find("to 'xpcall' (function expected, got nil)",
                                    1, true) ~= nil)
    local f = xpcall
    ok, err = pcall(function() return f(function() end) end)
    assert(ok == false and err:find("to 'f' (function expected, got no value)",
                                    1, true) ~= nil)
    ok, err = pcall(function() return f(function() end, nil) end)
    assert(ok == false and err:find("to 'f' (function expected, got nil)",
                                    1, true) ~= nil)
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
  local n = 20
  local callable = function()
    return 54
  end
  for _ = 1, n do
    callable = setmetatable({}, { __call = callable })
  end
  assert(callable() == 54)
end
do
  local function check(name, fn)
    local ok, err = pcall(fn)
    assert(ok == false and
           err:match("attempt to call a boolean value", 1, true) and
           err:match(name, 1, true))
  end
  check("local 'f'", function()
    local f = setmetatable({}, { __call = true })
    return f()
  end)
  check("local 'g'", function()
    local f = setmetatable({}, { __call = true })
    local g = f
    return g()
  end)
  check("field 'f'", function()
    local t = { f = setmetatable({}, { __call = true }) }
    return t.f()
  end)
  check("field '?'", function()
    local t = { f = setmetatable({}, { __call = true }) }
    local k = "f"
    return t[k]()
  end)
  check("global '?'", function()
    _G.lua54_dynamic_call_chain_bad = setmetatable({}, { __call = true })
    local k = "lua54_dynamic_call_chain_bad"
    local ok, res = pcall(function()
      return _ENV[k]()
    end)
    _G.lua54_dynamic_call_chain_bad = nil
    if not ok then error(res, 0) end
    return res
  end)
  check("local 'f'", function()
    local f = setmetatable({}, {
      __call = setmetatable({}, { __call = true }),
    })
    return f()
  end)
  do
    local function run(label, fn)
      local ok, err = pcall(fn)
      assert(label == "preserve" and ok == false and type(err) == "string")
      return err
    end
    local err = run("preserve", function()
      local t = { f = setmetatable({}, { __call = true }) }
      local k = "f"
      return t[k]()
    end)
    assert(err:find("attempt to call a boolean value (field '?')", 1, true))
  end
  do
    local f = setmetatable({}, { __call = true })
    local ok, err = pcall(f)
    assert(ok == false and
           err:match("attempt to call a boolean value", 1, true))
    ok, err = xpcall(f, function(e) return tostring(e) end)
    assert(ok == false and
           err:match("attempt to call a boolean value", 1, true))
  end
end
do
  local n = 10000
  local callable
  callable = function()
    if n == 0 then
      return 1023
    end
    n = n - 1
    return callable()
  end
  for _ = 1, 100 do
    callable = setmetatable({}, { __call = callable })
  end
  -- A long Lua 5.4 callable chain can add many implicit self arguments. The
  -- tail-call path must grow the coroutine stack and keep CALLT after growth.
  assert(coroutine.wrap(function() return callable() end)() == 1023)
end
do
  local f = assert(load("return x"))
  local name, env = debug.getupvalue(f, 1)
  assert(name == "_ENV" and env == _G)
  assert(debug.getinfo(f, "u").nups == 1)
  local repl = { x = 54 }
  assert(debug.setupvalue(f, 1, repl) == "_ENV")
  assert(f() == 54)
  assert(debug.setupvalue(f, 1, 5) == "_ENV")
  local ok, err = pcall(f)
  assert(ok == false and err:match("_ENV") ~= nil and
         err:match("number") ~= nil)

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
  -- Lua 5.4's debug.upvalueid is a query: missing upvalues return nil.
  -- debug.upvaluejoin remains the mutating API and must still reject them.
  assert(debug.upvalueid(h, 0) == nil)
  assert(debug.upvalueid(h, 3) == nil)
  assert(debug.upvalueid(h, -1) == nil)
  assert(select("#", debug.upvalueid(h, 0)) == 1)
  assert(select("#", debug.upvalueid(h, 3)) == 1)
  assert(select("#", debug.upvalueid(h, -1)) == 1)
  ok, err = pcall(debug.upvaluejoin, h, 3, h, 2)
  assert(ok == false and err:find("bad argument #2 to 'debug.upvaluejoin'",
				  1, true) and
	 err:find("invalid upvalue index", 1, true))
  ok, err = pcall(debug.upvaluejoin, print, 1, print, 1)
  assert(ok == false and err:find("bad argument #2 to 'debug.upvaluejoin'",
				  1, true) and
	 err:find("invalid upvalue index", 1, true))
  ok, err = pcall(debug.upvaluejoin, h, 2, function() end, 1)
  assert(ok == false and err:find("bad argument #4 to 'debug.upvaluejoin'",
				  1, true) and
	 err:find("invalid upvalue index", 1, true))
  ok, err = pcall(debug.upvaluejoin, h, 2, print, 1)
  assert(ok == false and err:find("bad argument #4 to 'debug.upvaluejoin'",
				  1, true) and
	 err:find("invalid upvalue index", 1, true))

  local source = assert(load("local _ENV = 5; return function() return x end"))()
  local target = assert(load("return x"))
  debug.upvaluejoin(target, 1, source, 1)
  n1, v1 = debug.getupvalue(target, 1)
  assert(n1 == "_ENV" and v1 == 5)
  assert(debug.upvalueid(target, 1) == debug.upvalueid(source, 1))
  ok, err = pcall(target)
  assert(ok == false and err:match("_ENV") ~= nil and
         err:match("number") ~= nil)

  local function expect_no_internal_c_upvalue(fn)
    -- LuaJIT keeps some standard-library helpers as C closure upvalues for
    -- speed, but Lua 5.4 debug APIs must not expose those implementation slots.
    assert(select("#", debug.getupvalue(fn, 1)) == 0)
    assert(select("#", debug.getupvalue(fn, 2)) == 0)
    assert(select("#", debug.upvalueid(fn, 1)) == 1)
    assert(debug.upvalueid(fn, 1) == nil)
    assert(select("#", debug.setupvalue(fn, 1, "hidden")) == 0)
  end
  expect_no_internal_c_upvalue(print)
  expect_no_internal_c_upvalue(pairs)
  expect_no_internal_c_upvalue(ipairs)
  local ipairs_aux1 = ipairs({})
  local ipairs_aux2 = ipairs({})
  assert(ipairs_aux1 == ipairs_aux2)
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
  local function expect_string_callname(fn, name)
    local ok, err = pcall(fn)
    assert(ok == false and
           err:find("bad argument #1 to '" .. name .. "'", 1, true) ~= nil)
  end
  expect_string_callname(function() string.dump(true) end, "dump")
  expect_string_callname(function() return string.dump(true) end, "dump")
  do
    local f = string.dump
    expect_string_callname(function() f(true) end, "f")
    expect_string_callname(function() return f(true) end, "f")
  end
  expect_string_callname(function() string.pack(nil) end, "pack")
  expect_string_callname(function() return string.pack(nil) end, "pack")
  do
    local f = string.pack
    expect_string_callname(function() f(nil) end, "f")
    expect_string_callname(function() return f(nil) end, "f")
  end
  expect_string_callname(function() string.unpack(nil, "") end, "unpack")
  expect_string_callname(function() return string.unpack(nil, "") end, "unpack")
  expect_string_callname(function() string.packsize(nil) end, "packsize")
  expect_string_callname(function() return string.packsize(nil) end, "packsize")
end
do
  local function expect_public_tail_callname(fn, name)
    local ok, err = pcall(fn)
    assert(ok == false and
           err:find("test/smoke.lua:", 1, true) ~= nil and
           err:find("to '" .. name .. "'", 1, true) ~= nil)
  end
  do
    local ok, err = pcall(math.random, true)
    assert(ok == false and err:find("to 'math.random'", 1, true) ~= nil)
    ok, err = pcall(math.randomseed, true)
    assert(ok == false and err:find("to 'math.randomseed'", 1, true) ~= nil)
  end
  expect_public_tail_callname(function() return math.random(true) end, "random")
  expect_public_tail_callname(function() return math.randomseed(true) end,
                              "randomseed")
  expect_public_tail_callname(function() return math.fmod(1, 0) end, "fmod")
  expect_public_tail_callname(function() return math.min() end, "min")
  do
    local f = math.max
    expect_public_tail_callname(function() return f() end, "f")
  end
  do
    local function set_global_alias()
      lua54_global_abs_alias = math.abs
    end
    set_global_alias()
    expect_public_tail_callname(function()
      return lua54_global_abs_alias(true)
    end, "lua54_global_abs_alias")
    lua54_global_abs_alias = nil
  end
  do
    local alias_table = {}
    local function set_table_alias()
      alias_table.f = math.abs
    end
    set_table_alias()
    expect_public_tail_callname(function()
      return alias_table.f(true)
    end, "f")
  end
  do
    local alias_table = {}
    lua54_table_global_alias = alias_table
    lua54_table_global_alias.h = math.abs
    expect_public_tail_callname(function()
      return alias_table.h(true)
    end, "h")
    lua54_table_global_alias = nil
  end
  do
    local alias_table = {}
    alias_table.j = math.abs
    lua54_table_global_alias = alias_table
    expect_public_tail_callname(function()
      return lua54_table_global_alias.j(true)
    end, "j")
    lua54_table_global_alias = nil
  end
  do
    local alias_table = {}
    local holder = {}
    holder.inner = alias_table
    holder.inner.k = math.abs
    expect_public_tail_callname(function()
      return alias_table.k(true)
    end, "k")
    expect_public_tail_callname(function()
      return holder.inner.k(true)
    end, "k")
  end
  do
    local alias_table = {}
    local holder = { inner = alias_table }
    holder.inner.l = math.abs
    expect_public_tail_callname(function()
      return alias_table.l(true)
    end, "l")
    expect_public_tail_callname(function()
      return holder.inner.l(true)
    end, "l")
  end
  do
    local alias_table = {}
    local base = {}
    base.inner = alias_table
    local holder = { alias = base.inner }
    holder.alias.m = math.abs
    expect_public_tail_callname(function()
      return alias_table.m(true)
    end, "m")
    expect_public_tail_callname(function()
      return holder.alias.m(true)
    end, "m")
  end
  do
    local alias_table = {}
    local base = {}
    base.inner = alias_table
    local holder = {}
    holder.alias = base.inner
    holder.alias.n = math.abs
    expect_public_tail_callname(function()
      return alias_table.n(true)
    end, "n")
    expect_public_tail_callname(function()
      return holder.alias.n(true)
    end, "n")
  end
  expect_public_tail_callname(function() return os.exit("x") end, "exit")
  do
    local f = os.exit
    expect_public_tail_callname(function() return f("x") end, "f")
  end
  expect_public_tail_callname(function() return io.open(true) end, "open")
  expect_public_tail_callname(function() return io.input(true) end, "input")
  expect_public_tail_callname(function() return io.read({}) end, "read")
  do
    local f = io.open
    expect_public_tail_callname(function() return f(true) end, "f")
  end
  expect_public_tail_callname(function() return debug.getinfo(true) end,
                              "getinfo")
  do
    local f = debug.getinfo
    expect_public_tail_callname(function() return f(true) end, "f")
  end
  expect_public_tail_callname(function() return utf8.len(true) end, "len")
  do
    local f = utf8.len
    expect_public_tail_callname(function() return f(true) end, "f")
  end
  expect_public_tail_callname(function()
    return package.searchpath({}, "?.lua")
  end, "searchpath")
  do
    local f = package.searchpath
    expect_public_tail_callname(function() return f({}, "?.lua") end, "f")
  end
  expect_public_tail_callname(function() return rawget() end, "rawget")
  expect_public_tail_callname(function() return rawset({}, "k") end, "rawset")
  expect_public_tail_callname(function() return rawequal() end, "rawequal")
  expect_public_tail_callname(function() return rawlen(1) end, "rawlen")
  expect_public_tail_callname(function() return next() end, "next")
  expect_public_tail_callname(function() return pairs() end, "pairs")
  expect_public_tail_callname(function() return ipairs() end, "ipairs")
  expect_public_tail_callname(function() return getmetatable() end,
                              "getmetatable")
  expect_public_tail_callname(function() return setmetatable() end,
                              "setmetatable")
  expect_public_tail_callname(function() return tonumber("10", 1) end,
                              "tonumber")
  expect_public_tail_callname(function() return select(1.2) end, "select")
  expect_public_tail_callname(function() return assert() end, "assert")
  do
    local f = rawget
    expect_public_tail_callname(function() return f() end, "f")
  end
  do
    local f = next
    expect_public_tail_callname(function() return f() end, "f")
  end
  do
    local f = tonumber
    expect_public_tail_callname(function() return f("10", 1) end, "f")
  end
  do
    local f = select
    expect_public_tail_callname(function() return f(1.2) end, "f")
  end
  do
    local f = assert
    expect_public_tail_callname(function() return f() end, "f")
  end
  do
    local function expect_dynamic_callname(fn)
      local ok, err = pcall(fn)
      assert(ok == false and
             err:find("test/smoke.lua:", 1, true) ~= nil and
             err:find("to '?'", 1, true) ~= nil)
    end
    expect_dynamic_callname(function()
      local k = "rawget"
      return _G[k]()
    end)
    expect_dynamic_callname(function()
      local k = "concat"
      return table[k](nil)
    end)
    expect_dynamic_callname(function()
      local k = "byte"
      return string[k](nil)
    end)
    expect_dynamic_callname(function()
      local k = "format"
      return string[k](nil)
    end)
    expect_dynamic_callname(function()
      local k = "abs"
      return math[k](true)
    end)
    expect_dynamic_callname(function()
      local k = "date"
      return os[k]({})
    end)
    expect_dynamic_callname(function()
      local k = "getinfo"
      return debug[k](true)
    end)
    expect_dynamic_callname(function()
      local k = "len"
      return utf8[k](true)
    end)
    expect_dynamic_callname(function()
      local k = "searchpath"
      return package[k]({}, "?.lua")
    end)
    do
      local ok, err = pcall(function()
        local t = { f = true }
        local k = "f"
        return t[k]()
      end)
      assert(ok == false and
             err:find("attempt to call a boolean value (field '?')",
                      1, true) ~= nil)
    end
    do
      local ok, err = pcall(function()
        _G.lua54_dynamic_call_bad = true
        local k = "lua54_dynamic_call_bad"
        return _ENV[k]()
      end)
      _G.lua54_dynamic_call_bad = nil
      assert(ok == false and
             err:find("attempt to call a boolean value (global '?')",
                      1, true) ~= nil)
    end
    do
      local ok, err = pcall(function()
        local k = "abs"
        local f = math[k]
        return f(true)
      end)
      assert(ok == false and
             err:find("bad argument #1 to 'f'", 1, true) ~= nil)
    end
    do
      local k = "abs"
      local ok, inner_ok, err = pcall(function()
        return pcall(math[k], true)
      end)
      assert(ok == true and inner_ok == false and
             err:find("bad argument #1 to 'math.abs'", 1, true) ~= nil)
    end
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
  assert(debug.getinfo(1099511627776, "S").what == "C")
  assert(debug.getinfo("1099511627776", "S").what == "C")
  assert(debug.getinfo(math.maxinteger) == nil)
  assert(debug.traceback("lua54 traceback wide", 1099511627776):
	 find("stack traceback", 1, true) ~= nil)
  assert(debug.getinfo("1", "n") ~= nil)

  local function expect_level_error(fn, fname, narg, ...)
    local ok, err = pcall(fn, ...)
    assert(ok == false and type(err) == "string" and
           err:find("bad argument #" .. narg .. " to '" .. fname ..
                    "' (level out of range)", 1, true) ~= nil)
  end
  expect_level_error(debug.getlocal, "debug.getlocal", 1, 999, 1)
  expect_level_error(debug.setlocal, "debug.setlocal", 1, 999, 1, true)
  do
    local co = coroutine.create(function()
      local x = 1
      coroutine.yield()
      return x
    end)
    assert(coroutine.resume(co) == true)
    expect_level_error(debug.getlocal, "debug.getlocal", 2, co, 999, 1)
    expect_level_error(debug.setlocal, "debug.setlocal", 2, co, 999, 1, true)
  end
end
do
  local function result_count(...)
    return select("#", ...), ...
  end
  local n, hook = result_count(debug.gethook())
  assert(n == 1 and hook == nil)
  local co = coroutine.create(function() end)
  n, hook = result_count(debug.gethook(co))
  assert(n == 1 and hook == nil)
  -- Lua 5.4 treats an empty mask with count 0 as "no hook"; keeping the
  -- function in the registry would make gethook() return function, "", 0.
  debug.sethook(function() end, "", 0)
  n, hook = result_count(debug.gethook())
  assert(n == 1 and hook == nil)
  debug.sethook(co, function() end, "", 0)
  n, hook = result_count(debug.gethook(co))
  assert(n == 1 and hook == nil)
  debug.sethook(function() end, "", 1)
  local active, mask, count = debug.gethook()
  assert(type(active) == "function" and mask == "" and count == 1)
  debug.sethook()
end
do
  assert(collectgarbage("generational") == "generational")
  assert(collectgarbage("incremental") == "generational")
  assert(collectgarbage("incremental") == "incremental")
  assert(collectgarbage("generational") == "incremental")
  do
    local n = 0
    local mt = { __gc = function() n = n + 1 end }
    collectgarbage("incremental")
    do
      local x = setmetatable({}, mt)
      x = nil
    end
    collectgarbage("stop")
    assert(collectgarbage("generational") == "incremental")
    assert(n == 1)
    assert(collectgarbage("isrunning") == false)
    do
      local x = setmetatable({}, mt)
      x = nil
    end
    assert(collectgarbage("incremental") == "generational")
    assert(n == 1)
    collectgarbage("restart")
    collectgarbage("collect")
    assert(n == 2)
  end
  assert(type(collectgarbage("count", true, true)) == "number")
  assert(type(collectgarbage(nil, true)) == "number")
  assert(collectgarbage("collect", true, true) == 0)
  collectgarbage("stop", true, true)
  assert(collectgarbage("isrunning", true) == false)
  collectgarbage("restart", true, true)
  assert(collectgarbage("isrunning", true) == true)
  do
    local function expect_gc_int_error(opt, narg, msg, ...)
      local ok, err = pcall(collectgarbage, opt, ...)
      assert(ok == false)
      assert(err:find("bad argument #" .. narg .. " to 'collectgarbage'",
		      1, true))
      assert(err:find(msg, 1, true))
    end
    collectgarbage("incremental")
    assert(collectgarbage("generational", 20, 30) == "incremental")
    assert(collectgarbage("incremental", "200", "100", "13") ==
	   "generational")
    assert(collectgarbage("generational", 20, 30, 40) == "incremental")
    assert(collectgarbage("incremental", 200, 300, 12, 9) == "generational")
    assert(collectgarbage("incremental", 321, 432, 13) == "incremental")
    assert(collectgarbage("setpause", 200) == 320)
    assert(collectgarbage("setstepmul", 100) == 432)
    expect_gc_int_error("generational", 2, "integer representation", 20.5)
    expect_gc_int_error("generational", 3, "integer representation", 20, 30.5)
    expect_gc_int_error("generational", 2, "number expected, got boolean",
			true)
    expect_gc_int_error("generational", 3, "number expected, got boolean",
			20, true)
    expect_gc_int_error("incremental", 2, "integer representation", 200.5)
    expect_gc_int_error("incremental", 3, "integer representation", 200, 300.5)
    expect_gc_int_error("incremental", 4, "integer representation",
			200, 300, 12.5)
    expect_gc_int_error("incremental", 4, "number expected, got boolean",
			200, 300, true)
  end
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
    assert(select(1, pcall(collectgarbage, "step", 1099511627776)) == true)
    assert(collectgarbage("setpause", "123") == 200)
    assert(collectgarbage("setstepmul", "123") == 100)
    collectgarbage("incremental", 200, 100, 13)
    collectgarbage("collect")
    assert(collectgarbage("step", -1) == false)
    assert(collectgarbage("step", 0) == true)
    collectgarbage("collect")
    assert(collectgarbage("step", 1099511627776) == true)
    collectgarbage("incremental", 200, 100, 1)
    collectgarbage("collect")
    local t = {}
    for i = 1, 20000 do t[i] = { i } end
    t = nil
    assert(collectgarbage("step", 0) == false)
    collectgarbage("incremental", 200, 100, 24)
    collectgarbage("collect")
    t = {}
    for i = 1, 20000 do t[i] = { i } end
    t = nil
    assert(collectgarbage("step", 0) == true)
    collectgarbage("incremental", 200, 100, 13)
    collectgarbage("stop")
    assert(collectgarbage("isrunning") == false)
    collectgarbage("collect")
    assert(collectgarbage("isrunning") == false)
    assert(collectgarbage("step", 20000) == true)
    assert(collectgarbage("isrunning") == false)
    collectgarbage("restart")
    assert(collectgarbage("isrunning") == true)
    do
      local oldhot
      if jit and jit.opt and jit.opt.start then
        jit.flush()
        jit.opt.start("hotloop=1", "hotexit=1")
        oldhot = true
      end
      local mt = { __mode = "k" }
      local roots = setmetatable({}, mt)
      local x
      for i = 1, 100 do
        local n = {}
        roots[n] = { k = { x } }
        x = n
      end
      collectgarbage("collect")
      local done = false
      local u = setmetatable({}, { __gc = function() done = true end })
      local n = 0
      repeat
        n = n + 1
        u = {}
      until done or n > 20000
      -- A full collection must not leave such a large threshold that a hot
      -- allocation loop traces and sinks allocations before GC can run.
      assert(done)
      if oldhot then
        jit.flush()
        jit.opt.start("hotloop=56", "hotexit=10")
      end
      assert(x ~= nil and roots ~= nil and u ~= nil)
    end
    do
      local function bounded_table_finalizer(limit)
        local done = false
        local u = setmetatable({}, { __gc = function() done = true end })
        local keep = {34}
        local n = 0
        repeat
          n = n + 1
          u = {}
        until done or n > limit
        assert(done and keep[1] == 34)
        return u
      end

      -- Match the official gc.lua sequence that leaves a large weak/ephemeron
      -- graph before relying on allocation-triggered table finalization.
      local lim = 15
      local a = setmetatable({}, { __mode = "k" })
      for i = 1, lim do a[{}] = i end
      for i = 1, lim do a[i] = i end
      for i = 1, lim do local s = string.rep("@", i); a[s] = s.."#" end
      collectgarbage("collect")
      a = setmetatable({}, { __mode = "v" })
      for i = 1, lim do a[i] = {} end
      for i = 1, lim do a[i.."x"] = {} end
      for i = 1, lim do local t = {}; a[t] = t end
      for i = 1, lim do a[i+lim] = i.."x" end
      collectgarbage("collect")
      a = setmetatable({}, { __mode = "kv" })
      local x, y, z = {}, {}, {}
      a[1], a[2], a[3] = x, y, z
      a[string.rep("$", 11)] = string.rep("$", 11)
      for i = 4, lim do a[i] = {} end
      for i = 1, lim do a[{}] = i end
      for i = 1, lim do local t = {}; a[t] = t end
      collectgarbage("collect")
      x, y, z = nil, nil, nil
      collectgarbage("collect")
      local mt = { __mode = "k" }
      a = {{10}, {20}, {30}, {40}}
      setmetatable(a, mt)
      x = nil
      for i = 1, 100 do
        local n = {}
        a[n] = { k = { x } }
        x = n
      end
      bounded_table_finalizer(50000)
      assert(a ~= nil and x ~= nil)
    end
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
  assert(load("local x <close> = nil; x = false") == nil)
  assert(load("local x <close> = nil; local function f() x = false end") == nil)
  do
    local _, err = load([[
      local x <close> = nil
      x = false
    ]])
    assert(err:match(":2: attempt to assign to const variable 'x'", 1, true))
  end
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
    local function lexnear(src, near)
      local fn, err = load("return " .. src, "")
      assert(fn == nil and err:match(near, 1, true) ~= nil)
    end
    lexnear([["abc\x"]], [[\x"]])
    lexnear([["\999"]], [[\999"]])
    lexnear([["abc\u{11r"]], [[\u{11r]])
    assert(load("#=1", "") == nil)
    do
      local _, err = load("a" .. string.char(1) .. "a = 1", "")
      assert(err:match("'<\\1>'", 1, true) ~= nil)
      _, err = load(string.char(255) .. "a = 1", "")
      assert(err:match("'<\\255>'", 1, true) ~= nil)
    end
    assert(load("a" .. string.char(128) .. "1 = 1", "") == nil)
    do
      local _, err = load("local a = {4\n")
      assert(err:match("near <eof>", 1, true) ~= nil)
    end
    do
      local _, err = load("::A:: a = 1\n::A::")
      assert(err:match("label 'A' already defined on line 1", 1, true))
      _, err = load("goto A\ndo ::A:: end")
      assert(err:match("no visible label 'A' for <goto> at line 1", 1, true))
      _, err = load("break")
      assert(err:match("break outside loop at line 1", 1, true))
    end
    do
      local ok, err = pcall(assert(load("bbbb = 2; return bbbb()")))
      assert(ok == false and err:match("global 'bbbb'", 1, true) ~= nil)
      ok, err = pcall(assert(load("aaa = {}; return (aaa or aaa) + (aaa and aaa)")))
      assert(ok == false and err:match("'aaa'", 1, true) == nil)
      ok, err = pcall(assert(load("aaa = {}; return (aaa or aaa)()")))
      assert(ok == false and err:match("'aaa'", 1, true) == nil)
      local chunk = {}
      for i = 1, 300 do chunk[i] = "aaa = x" .. i end
      ok, err = pcall(assert(load(table.concat(chunk, "; ") ..
        "; local t = {}; t:bbb()")))
      assert(ok == false and err:match("method 'bbb'", 1, true) ~= nil)
      ok, err = pcall(assert(load([[return math.sin("a")]])))
      assert(ok == false and err:match("'sin'", 1, true) ~= nil and
	     err:match("'math.sin'", 1, true) == nil, tostring(err))
      ok, err = pcall(assert(load([[return math.abs(true)]])))
      assert(ok == false and err:match("'abs'", 1, true) ~= nil and
	     err:match("'math.abs'", 1, true) == nil, tostring(err))
      ok, err = pcall(assert(load([[return (math.abs)(true)]])))
      assert(ok == false and err:match("'abs'", 1, true) ~= nil and
	     err:match("'math.abs'", 1, true) == nil, tostring(err))
      ok, err = pcall(assert(load([[local f = math.abs; return f(true)]])))
      assert(ok == false and err:match("'f'", 1, true) ~= nil and
	     err:match("'math.abs'", 1, true) == nil, tostring(err))
      ok, err = pcall(assert(load("a\n=\n-\n\nprint\n;")))
      assert(ok == false and err:match(":3:", 1, true) ~= nil)
      _G.__lua54_string_self = setmetatable({}, { __index = string })
      ok, err = pcall(assert(load("__lua54_string_self:sub()")))
      _G.__lua54_string_self = nil
      assert(ok == false and err:match("bad self", 1, true) ~= nil)
      ok, err = pcall(assert(load([[("a"):sub{}]])))
      assert(ok == false and err:match("bad argument #1", 1, true) ~= nil)
      _, err = load("local a; a" .. string.rep(",a", 500))
      assert(err:match("too many", 1, true) ~= nil)
      _, err = load("local function a (x) return x end; return " ..
                    string.rep("a(", 500))
      assert(err:match("too many", 1, true) ~= nil)
      _, err = load("a = f(x" .. string.rep(",x", 260) .. ")")
      assert(err:match("too many registers", 1, true) ~= nil)
      local locals = {}
      for i = 1, 300 do locals[i] = "a" .. i end
      _, err = load("\nfunction foo ()\n  local " .. table.concat(locals, ",") .. "\n")
      assert(err:match("line 2", 1, true) ~= nil and
             err:match("too many local variables", 1, true) ~= nil)
      local upsrc = "local function fooA ()\n  local "
      for i = 1, 100 do upsrc = upsrc .. "a" .. i .. ", " end
      upsrc = upsrc .. "b,c\nlocal function fooB ()\n  local "
      for i = 1, 100 do upsrc = upsrc .. "b" .. i .. ", " end
      upsrc = upsrc .. "b\nfunction fooC () return b+c"
      for i = 1, 100 do upsrc = upsrc .. "+a" .. i .. "+b" .. i end
      upsrc = upsrc .. "\nend end end"
      _, err = load(upsrc)
      assert(err:match("line 5", 1, true) ~= nil and
             err:match("too many upvalues", 1, true) ~= nil)
      local nup = 200
      local prog = { "local a1" }
      for i = 2, nup do prog[#prog + 1] = ", a" .. i end
      prog[#prog + 1] = " = 1"
      for i = 2, nup do prog[#prog + 1] = ", " .. i end
      local sum = 1
      prog[#prog + 1] = "; return function () return a1"
      for i = 2, nup do
        prog[#prog + 1] = " + a" .. i
        sum = sum + i
      end
      prog[#prog + 1] = " end"
      local manyuv = assert(load(table.concat(prog)))()
      assert(manyuv() == sum)
      manyuv = assert(load(string.dump(manyuv)))
      local uvvalue = 10
      local donor = function() return uvvalue end
      for i = 1, nup do
        debug.upvaluejoin(manyuv, i, donor, 1)
      end
      assert(manyuv() == uvvalue * nup)
    end
  end
  assert(load("::l1:: do ::l1:: end") == nil)
  assert(load("::l1:: local function f() ::l1:: end") ~= nil)
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
    assert(assert(load([[
      local function f()
        local x <close> = setmetatable({}, {
          __close = function()
            local info = debug.getinfo(1, "n")
            assert(info.namewhat == "metamethod" and info.name == "close")
            error("@lua54-close")
          end,
        })
      end
      local ok, msg = xpcall(f, debug.traceback)
      assert(ok == false and msg:find("@lua54%-close") ~= nil)
      -- The close call stays a plain Lua call so it can yield; debug metadata
      -- recognizes the compiler-emitted close helper and names the frame.
      assert(msg:find("in metamethod 'close'", 1, true) ~= nil)
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local log = {}
      local mt = {
        __close = function(self, err)
          assert(err == nil)
          log[#log + 1] = self.name
        end,
      }
      local function f()
        local a <close> = setmetatable({ name = "a" }, mt)
        local b <close> = setmetatable({ name = "b" }, mt)
        return "ok", #log
      end
      local value, seen = f()
      assert(value == "ok" and seen == 0)
      assert(table.concat(log, ",") == "b,a")
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local log = {}
      local mt = {
        __close = function(self, err)
          assert(err == nil)
          log[#log + 1] = self.name
        end,
      }
      local function values()
        log[#log + 1] = "values"
        return "a", nil, "c"
      end
      local function f()
        local x <close> = setmetatable({ name = "x" }, mt)
        return values()
      end
      local function g(...)
        local y <close> = setmetatable({ name = "y" }, mt)
        return "head", ...
      end
      local n, a, b, c = select("#", f()), f()
      assert(n == 3 and a == "a" and b == nil and c == "c")
      local vn, va, vb, vc = select("#", g(nil, "tail")), g(nil, "tail")
      assert(vn == 3 and va == "head" and vb == nil and vc == "tail")
      assert(table.concat(log, ",") == "values,x,values,x,y,y")
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local function func2close(f)
        return setmetatable({}, { __close = f })
      end
      local trace = {}
      local co = coroutine.wrap(function()
        do
          local x <close> = func2close(function(_, err)
            assert(err == nil)
            trace[#trace + 1] = "x1"
            coroutine.yield("x")
            trace[#trace + 1] = "x2"
          end)
          trace[#trace + 1] = "body"
        end
        return "done"
      end)
      assert(co() == "x")
      assert(co() == "done")
      assert(table.concat(trace, ",") == "body,x1,x2")
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local function func2close(f)
        return setmetatable({}, { __close = f })
      end
      local closed
      local co = coroutine.wrap(function()
        local x <close> = func2close(function(self, err)
          assert(err == nil)
          closed = self
          coroutine.yield("closing")
        end)
        return "head", x, nil, "tail"
      end)
      assert(co() == "closing")
      local out = table.pack(co())
      assert(out.n == 4 and out[1] == "head" and out[2] == closed and
             out[3] == nil and out[4] == "tail")
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local function func2close(f)
        return setmetatable({}, { __close = f })
      end
      local co = coroutine.create(function()
        local x <close> = func2close(function(_, err)
          assert(err == nil)
          coroutine.yield("dynamic closing")
          error("dynamic close boom", 0)
        end)
        return (function()
          return "head", nil, "tail"
        end)()
      end)
      local ok, value = coroutine.resume(co)
      assert(ok == true and value == "dynamic closing")
      local a, b, c, d = coroutine.resume(co)
      assert(a == false and b == "dynamic close boom" and c == nil and d == nil)
      assert(coroutine.status(co) == "dead")
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local raw_pcall = pcall
      local resume = coroutine.resume
      local create = coroutine.create
      local co = create(function()
        local x <close> = setmetatable({}, {
          __close = function()
            coroutine.yield("raw pcall close")
          end,
        })
        return "done"
      end)
      pcall = function()
        error("global pcall hijacked", 0)
      end
      local ok, value = resume(co)
      pcall = raw_pcall
      assert(ok == true and value == "raw pcall close")
      ok, value = resume(co)
      assert(ok == true and value == "done")
      local raw_reg_pcall = debug.getregistry()._LUA54_RAW_PCALL
      co = create(function()
        local x <close> = setmetatable({}, {
          __close = function()
            coroutine.yield("hidden raw pcall close")
          end,
        })
        return "hidden done"
      end)
      debug.getregistry()._LUA54_RAW_PCALL = nil
      pcall = function()
        error("registry raw pcall hijacked", 0)
      end
      ok, value = resume(co)
      pcall = raw_pcall
      debug.getregistry()._LUA54_RAW_PCALL = raw_reg_pcall
      assert(ok == true and value == "hidden raw pcall close")
      ok, value = resume(co)
      assert(ok == true and value == "hidden done")
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local events = {}
      local target
      local function hook(ev)
        if ev == "return" and debug.getinfo(2, "f").func == target then
          events[#events + 1] = "return"
        end
      end
      target = function()
        local x <close> = setmetatable({}, {
          __close = function()
            events[#events + 1] = "close"
          end,
        })
        return "ok"
      end
      debug.sethook(hook, "r")
      local out = target()
      debug.sethook()
      assert(out == "ok")
      assert(table.concat(events, ",") == "close,return")
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local events = {}
      local target
      local function hook(ev)
        if ev == "return" and debug.getinfo(2, "f").func == target then
          events[#events + 1] = "return"
        end
      end
      target = function()
        local x <close> = setmetatable({}, {
          __close = function()
            events[#events + 1] = "close"
            debug.sethook(hook, "r")
          end,
        })
        return "ok"
      end
      assert(target() == "ok")
      debug.sethook()
      assert(table.concat(events, ",") == "close,return")
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local function func2close(f)
        return setmetatable({}, { __close = f })
      end
      local events = {}
      local target
      local function values()
        events[#events + 1] = "values"
        return nil, "tail"
      end
      local function hook(ev)
        if ev == "return" and debug.getinfo(2, "f").func == target then
          events[#events + 1] = "return"
        end
      end
      target = function()
        local x <close> = func2close(function(_, err)
          assert(err == nil)
          events[#events + 1] = "close1"
          coroutine.yield("close yield")
          events[#events + 1] = "close2"
          debug.sethook(hook, "r")
        end)
        return "head", values()
      end
      local co = coroutine.create(target)
      local ok, value = coroutine.resume(co)
      assert(ok == true and value == "close yield")
      local out = table.pack(coroutine.resume(co))
      debug.sethook()
      assert(out.n == 4 and out[1] == true and out[2] == "head" and
             out[3] == nil and out[4] == "tail")
      assert(table.concat(events, ",") == "values,close1,close2,return")
      assert(coroutine.status(co) == "dead")
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local ok, err = pcall(function()
        local x <close> = setmetatable({}, {
          __close = function()
            error("close boom", 0)
          end,
        })
      end)
      assert(ok == false and err == "close boom")

      ok, err = pcall(function()
        local y <close> = setmetatable({}, { __close = function() end })
        getmetatable(y).__close = nil
      end)
      assert(ok == false and err:match("metamethod 'close'") ~= nil)
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local function func2close(f)
        return setmetatable({}, { __close = f })
      end
      local track = {}
      local function foo()
        local x0 <close> = func2close(function(_, msg)
          assert(msg == 202)
          track[#track + 1] = "x0"
        end)
        local x <close> = func2close(function()
          local xx <close> = func2close(function(_, msg)
            assert(msg == 101)
            track[#track + 1] = "xx"
            error(202)
          end)
          track[#track + 1] = "x"
          error(101)
        end)
        track[#track + 1] = "foo"
        return 20, 30, 40
      end
      local ok, err = pcall(foo)
      assert(ok == false and err == 202)
      assert(table.concat(track, ",") == "foo,x,xx,x0")
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local log = {}
      local mt = {
        __close = function(self, err)
          assert(err == nil)
          log[#log + 1] = self.name
        end,
      }
      while true do
        local a <close> = setmetatable({ name = "a" }, mt)
        do
          local b <close> = setmetatable({ name = "b" }, mt)
          break
        end
      end
      assert(table.concat(log, ",") == "b,a")
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local log = {}
      local mt = {
        __close = function(self, err)
          assert(err == nil)
          log[#log + 1] = self.name
        end,
      }
      local trips = 0
      ::again::
      do
        local a <close> = setmetatable({ name = "a" }, mt)
        trips = trips + 1
        if trips == 1 then goto again end
      end
      assert(table.concat(log, ",") == "a,a")
      return true
    ]]))())
  end
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
        goto done
      end
      ::done::
      assert(table.concat(log, ",") == "a")
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local n = 0
      local mt = { __close = function() n = n + 1 end }
      do
        goto done
        local a <close> = setmetatable({}, mt)
      end
      ::done::
      assert(n == 0)
      return true
    ]]))())
  end
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
        goto inside
        assert(false)
        ::inside::
        assert(#log == 0)
      end
      assert(table.concat(log, ",") == "a")
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local log = {}
      local mt = {
        __close = function(self, err)
          assert(err == nil)
          log[#log + 1] = self.name
        end,
      }
      local function iter(_, i)
        if i < 2 then return i + 1 end
      end
      for _ in iter, nil, 0, setmetatable({ name = "natural" }, mt) do
      end
      assert(table.concat(log, ",") == "natural")

      log = {}
      for _ in iter, nil, 0, setmetatable({ name = "break" }, mt) do
        break
      end
      assert(table.concat(log, ",") == "break")

      local ok, err = pcall(assert(load("for _ in function() end, nil, nil, 1 do end")))
      assert(ok == false and err:match("variable '%(for state%)' got a non%-closable value"))
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local log = {}
      local mt = {
        __close = function(self, err)
          log[#log + 1] = self.name..":"..tostring(err)
        end,
      }
      local ok, err = pcall(function()
        local a <close> = setmetatable({ name = "a" }, mt)
        do
          local b <close> = setmetatable({ name = "b" }, mt)
          error("close error path", 0)
        end
      end)
      assert(ok == false and err == "close error path")
      assert(table.concat(log, ",") == "b:close error path,a:close error path")
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local raw_xpcall = xpcall
      local raw_reg_xpcall = debug.getregistry()._LUA54_RAW_XPCALL
      local log = {}
      debug.getregistry()._LUA54_RAW_XPCALL = nil
      xpcall = function()
        error("global xpcall hijacked", 0)
      end
      local ok, value = pcall(function()
        do
          local x <close> = setmetatable({}, {
            __close = function()
              log[#log + 1] = "closed"
            end,
          })
        end
        return "ok"
      end)
      xpcall = raw_xpcall
      debug.getregistry()._LUA54_RAW_XPCALL = raw_reg_xpcall
      assert(ok == true and value == "ok")
      assert(table.concat(log, ",") == "closed")
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local log = {}
      local mt = {
        __close = function(self, err)
          log[#log + 1] = self.name..":"..tostring(err)
        end,
      }
      local ok, err = xpcall(function()
        local x <close> = setmetatable({ name = "x" }, mt)
        error("xpcall close path", 0)
      end, function(e)
        log[#log + 1] = "handler:"..tostring(e)
        return "handled:"..e
      end)
      assert(ok == false and err == "handled:xpcall close path")
      assert(table.concat(log, ",") ==
        "handler:xpcall close path,x:handled:xpcall close path")
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local log = {}
      local mt = {
        __close = function(self, err)
          log[#log + 1] = self.name..":"..tostring(err)
          error("close replacement", 0)
        end,
      }
      local ok, err = pcall(function()
        local x <close> = setmetatable({ name = "x" }, mt)
        error("body replacement", 0)
      end)
      assert(ok == false and err == "close replacement")
      assert(table.concat(log, ",") == "x:body replacement")
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local log = {}
      local mt = {
        __close = function(self, err)
          log[#log + 1] = self.name..":"..tostring(err)
        end,
      }
      local function iter(_, i)
        if i == 0 then return 1 end
        error("generic close path", 0)
      end
      local ok, err = pcall(function()
        for _ in iter, nil, 0, setmetatable({ name = "state" }, mt) do
        end
      end)
      assert(ok == false and err == "generic close path")
      assert(table.concat(log, ",") == "state:generic close path")
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
  assert(f ~= nil and err == nil)
  local ok
  ok, err = pcall(f)
  assert(ok == false and err:match("'for' step is zero") ~= nil)
  f, err = load("for i = 1, 3, 0.0 do end")
  assert(f ~= nil and err == nil)
  ok, err = pcall(f)
  assert(ok == false and err:match("'for' step is zero") ~= nil)
  ok, err = pcall(assert(load("local z = 0; for i = 1, 3, z do end")))
  assert(ok == false and err:match("'for' step is zero") ~= nil)
  ok, err = pcall(assert(load([[for i = 1, 3, "0" do end]])))
  assert(ok == false and err:match("'for' step is zero") ~= nil)
  local function forerr(src, what, got)
    local ok_for, err_for = pcall(assert(load(src)))
    assert(ok_for == false and err_for:match(what, 1, true) ~= nil and
           err_for:match(got, 1, true) ~= nil)
    return err_for
  end
  forerr("for i = {}, 10 do end", "bad 'for' initial value", "got table")
  forerr("for i = io.stdin, 10 do end", "bad 'for' initial value", "FILE")
  forerr([[for i = 1, "x", 10 do end]], "bad 'for' limit", "got string")
  assert(forerr([[local a
for i = 1, "x" do end]], "bad 'for' limit", "got string"):match(":2:", 1, true) ~= nil)
  forerr("for i = 1, {}, 10 do end", "bad 'for' limit", "got table")
  forerr("for i = 1, 10, print do end", "bad 'for' step", "got function")
  local c = 0
  for i = 1.0, 10 do
    assert(math.type(i) == "float")
    c = c + 1
  end
  assert(c == 10)
  c = 0
  for i = -1, -10, -1.0 do
    assert(math.type(i) == "float")
    c = c + 1
  end
  assert(c == 10)
  c = 0
  for i = 1, 10.9 do
    assert(math.type(i) == "integer")
    c = c + 1
  end
  assert(c == 10)
  c = 0
  for i = 10, 0.001, -1 do
    assert(math.type(i) == "integer")
    c = c + 1
  end
  assert(c == 10)
  local function fortypes(init, limit, step)
    local out = {}
    if step == nil then
      for i = init, limit do
	out[#out+1] = math.type(i)..":"..tostring(i)
      end
    else
      for i = init, limit, step do
	out[#out+1] = math.type(i)..":"..tostring(i)
      end
    end
    return table.concat(out, ",")
  end
  assert(fortypes("1", 3, 1) == "float:1.0,float:2.0,float:3.0")
  assert(fortypes(1, "3", 1) == "integer:1,integer:2,integer:3")
  assert(fortypes(1, 3, "1") == "float:1.0,float:2.0,float:3.0")
  assert(fortypes("0x1", "0x3") == "float:1.0,float:2.0,float:3.0")
  local cross32_for = {}
  for i = 2147483646, 2147483648 do
    cross32_for[#cross32_for+1] = math.type(i)..":"..tostring(i)
    assert(#cross32_for <= 4)
  end
  assert(table.concat(cross32_for, ",") ==
	 "integer:2147483646,integer:2147483647,integer:2147483648")
  cross32_for = {}
  for i = 2147483648, 2147483646, -1 do
    cross32_for[#cross32_for+1] = math.type(i)..":"..tostring(i)
    assert(#cross32_for <= 4)
  end
  assert(table.concat(cross32_for, ",") ==
	 "integer:2147483648,integer:2147483647,integer:2147483646")
  local float_i64_limit = {}
  for i = 2147483646.0, 2147483648, 1.0 do
    float_i64_limit[#float_i64_limit+1] = math.type(i)..":"..tostring(i)
    assert(#float_i64_limit <= 4)
  end
  assert(table.concat(float_i64_limit, ",") ==
	 "float:2147483646.0,float:2147483647.0,float:2147483648.0")
  local boxed_for = {}
  for i = 2147483648, 2147483650 do
    boxed_for[#boxed_for+1] = i
  end
  assert(boxed_for[1] == 2147483648)
  assert(boxed_for[2] == 2147483649)
  assert(boxed_for[3] == 2147483650)
  local boxed_closures = {}
  for i = 2147483648, 2147483650 do
    boxed_closures[#boxed_closures+1] = function() return i end
  end
  assert(boxed_closures[1]() == 2147483648)
  assert(boxed_closures[2]() == 2147483649)
  assert(boxed_closures[3]() == 2147483650)
  if jit and jit.opt and jit.opt.start then
    local function check_jit_float_for(init, limit, step)
      local n = 0
      jit.on()
      jit.flush()
      -- Force recording quickly: the bug is invisible with the default hotloop
      -- threshold, but official tests can hit it after earlier loop warmup.
      jit.opt.start("hotloop=1", "hotexit=1")
      for i = init, limit, step do
        assert(math.type(i) == "float")
        n = n + 1
      end
      jit.flush()
      -- jit.opt.start() without arguments only resets optimization flags, not
      -- hotloop/hotexit parameters. Restore explicit defaults so later hook
      -- tests do not trace immediately on ARM64.
      jit.opt.start("hotloop=56", "hotexit=10")
      return n
    end
    assert(check_jit_float_for(1.0, 10, 1) == 10)
    assert(check_jit_float_for(-1, -10, -1.0) == 10)
    assert(check_jit_float_for("1", 10, 1) == 10)
    assert(check_jit_float_for(1, 10, "1") == 10)
  end
end
do
  local function eval(src)
    return assert(load("return "..src))()
  end
  local intbits = math.floor(math.log(math.maxinteger, 2) + 0.5) + 1
  assert(string.packsize("j") * 8 == intbits)
  assert(string.packsize("J") == string.packsize("j"))
  assert(eval("1 << "..intbits) == 0)
  assert(math.mininteger == eval("1 << "..(intbits - 1)))
  assert(eval("(1 << "..(intbits - 1)..") >> "..(intbits - 1)) == 1)
  assert(eval("-1 >> 1") == math.maxinteger)
  assert(eval("-1 << "..(intbits - 1)) == math.mininteger)
  assert(eval("~0") == -1)
  assert(math.maxinteger == math.mininteger - 1)
  assert(eval("5 // 2") == 2)
  assert(eval("-5 // 2") == -3)
  assert(eval("5.5 // 2") == 2)
  assert(eval("math.mininteger // -1") == math.mininteger)
  assert(math.type(eval("math.mininteger // -1")) == "integer")
  assert(eval([["5" // 2]]) == 2)
  assert(eval([["5.5" // 2]]) == 2)
  assert(assert(load([[local a, b = "1.0" // "2"; return a == 0 and b == nil]]))())
  assert(assert(load("local a, b = 6, 3; return a & b"))() == 2)
  assert(eval("4 | 1") == 5)
  assert(eval("7 ~ 3") == 4)
  assert(eval("~7") == -8)
  assert(eval("1 << 3") == 8)
  assert(eval("8 >> 1") == 4)
  assert(eval("1 << 31") == 0x80000000)
  assert(eval("-(1 << 31)") == -0x80000000)
  assert(math.type(eval("-(1 << 31)")) == "integer")
  assert(eval("(1 << 31) - 1") == 0x7fffffff)
  assert(eval("1 << 32") == 0x100000000)
  assert(eval("1 << 40") == 0x10000000000)
  assert(eval("1 << 64") == 0)
  assert(eval("1 >> 64") == 0)
  assert(assert(load("local a, b = 6, 3; return (a + 1) & (b + 1)"))() == 4)
  do
    local fname = "lua54_i64_constants_loadfile.tmp"
    local f = assert(io.open(fname, "w"))
    f:write([[
local function arshift(a, b)
  a = a & 0xffffffff
  if b <= 0 or (a & 0x80000000) == 0 then
    return (a >> b) & 0xffffffff
  end
  return ((a >> b) | ~(0xffffffff >> b)) & 0xffffffff
end
for _ = 1, 20 do
  assert((-1 & 0xffffffff) == 0xffffffff)
  assert(arshift(0x12345678, 0) == 0x12345678)
  assert(arshift(-1, 1) == 0xffffffff)
end
return true
]])
    assert(f:close())
    collectgarbage("collect")
    assert(assert(loadfile(fname))())
    assert(os.remove(fname))
  end
  assert(eval("((1 << 4) | 3) ~ 5") == 22)
  assert(eval("8 >> -1") == 16)
  assert(eval("8 << -1") == 4)
  do
    local r = assert(load([[return "1" + "2"]]))()
    assert(r == 3 and math.type(r) == "integer")
    r = assert(load([[return "1" + 2]]))()
    assert(r == 3 and math.type(r) == "integer")
    r = assert(load([[return "1.0" + 2]]))()
    assert(r == 3 and math.type(r) == "float")
    r = assert(load([[return "1e0" + 2]]))()
    assert(r == 3 and math.type(r) == "float")
    do
      local a, b = 5, 2
      r = a % b
      assert(r == 1 and math.type(r) == "integer")
      a, b = 5, 0
      local ok, err = pcall(function() return a % b end)
      assert(ok == false and err:match("n%%0") ~= nil)
      ok, err = pcall(assert(load([[return {} % 1]])))
      assert(ok == false and err:match("perform arithmetic") ~= nil and
             err:match("mod") == nil)
      ok, err = pcall(assert(load([[return 1 % true]])))
      assert(ok == false and err:match("perform arithmetic") ~= nil and
             err:match("boolean value") ~= nil and err:match("mod") == nil)
      ok, err = pcall(assert(load([[local t = setmetatable({}, { __name = "Lua54Mod" }); return t % 1]])))
      assert(ok == false and err:match("Lua54Mod value") ~= nil and
             err:match("local 't'") ~= nil and err:match("mod") == nil)
      ok, err = pcall(assert(load([[return "x" % true]])))
      assert(ok == false and err:match("attempt to mod") ~= nil and
             err:match("'string'") ~= nil and err:match("'boolean'") ~= nil)
    end
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
    ok, err = pcall(function() return "x" + 1 end)
    assert(ok == false and err:match("attempt to add") ~= nil and
           err:match("%(temporary%)") == nil and
           err:match("smoke%.lua:%d+:") ~= nil)
    ok, err = pcall(assert(load([[return 1 + "x"]])))
    assert(ok == false and err:match("attempt to add") ~= nil and
           err:match("'number'") ~= nil and err:match("'string'") ~= nil)
    assert(assert(load([[return (2 ^ 40) & 1]]))() == 0)
    ok, err = pcall(assert(load([[return math.huge << 1]])))
    assert(ok == false and err:match("integer representation") ~= nil and
           err:match("field 'huge'") ~= nil)
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
           err:match("string value") ~= nil and
           err:find("constant '3'", 1, true) ~= nil)
    ok, err = pcall(assert(load([[return 1 & "3"]])))
    assert(ok == false and err:match("bitwise operation") ~= nil and
           err:match("string value") ~= nil and
           err:find("constant '3'", 1, true) ~= nil)
    ok, err = pcall(assert(load([[local x = "3"; return x & 1]])))
    assert(ok == false and err:match("bitwise operation") ~= nil and
           err:match("string value") ~= nil and
           err:match("local 'x'") ~= nil)
    ok, err = pcall(assert(load("return true & 1")))
    assert(ok == false and err:match("bitwise operation") ~= nil and
           err:match("boolean value") ~= nil)
    ok, err = pcall(assert(load("local b = true; return b & 1")))
    assert(ok == false and err:match("bitwise operation") ~= nil and
           err:match("boolean value") ~= nil and
           err:match("local 'b'") ~= nil)
    ok, err = pcall(assert(load("local t = {}; return t & 1")))
    assert(ok == false and err:match("bitwise operation") ~= nil and
           err:match("table value") ~= nil and
           err:match("local 't'") ~= nil)
    local string_bitwise_errors = {
      { [[return "7" & 3]], "constant '7'" },
      { [[return "7" | 3]], "constant '7'" },
      { [[return "7" ~ 3]], "constant '7'" },
      { [[return ~"3"]], "constant '3'" },
      { [[return "1" << 2]], "constant '1'" },
      { [[return "8" >> 1]], "constant '8'" },
    }
    for _, case in ipairs(string_bitwise_errors) do
      ok, err = pcall(assert(load(case[1])))
      assert(ok == false and err:match("bitwise operation") ~= nil and
             err:match("string value") ~= nil and
             err:find(case[2], 1, true) ~= nil)
    end
    _G.__lua54_named_bitwise = setmetatable({}, { __name = "Lua54Bitwise" })
    ok, err = pcall(assert(load("return __lua54_named_bitwise & 1")))
    assert(ok == false and err:match("bitwise operation") ~= nil and
           err:match("Lua54Bitwise value") ~= nil)
    ok, err = pcall(assert(load("local t = __lua54_named_bitwise; return t & 1")))
    assert(ok == false and err:match("bitwise operation") ~= nil and
           err:match("Lua54Bitwise value") ~= nil and
           err:match("local 't'") ~= nil)
    _G.__lua54_named_bitwise = nil
  end
  do
    local string_mt = debug.getmetatable("")
    local names = { "band", "bor", "bxor", "bnot", "shl", "shr", "idiv", "mod" }
    local old = {}
    for _, name in ipairs(names) do
      old[name] = string_mt["__"..name]
      string_mt["__"..name] = function(a, b) return name, a, b end
    end
    local cases = {
      { [[return "7" & 3]], "band" },
      { [[return 3 & "7"]], "band" },
      { [[return "x" | 3]], "bor" },
      { [[return "x" ~ 3]], "bxor" },
      { [[return ~"x"]], "bnot" },
      { [[return "x" << 3]], "shl" },
      { [[return "x" >> 3]], "shr" },
      { [[return "7" // 3]], "idiv" },
      { [[return "7" % 3]], "mod" },
    }
    for _, case in ipairs(cases) do
      local r1, r2 = assert(load(case[1]))()
      assert(r1 == case[2] and r2 == nil)
    end
    for _, name in ipairs(names) do
      string_mt["__"..name] = old[name]
    end
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
    local badcall = setmetatable({}, { __call = true })
    local function check(name, fn)
      local ok, err = pcall(fn)
      assert(ok == false and
             err:match("attempt to call a boolean value", 1, true) and
             err:match("metamethod '"..name.."'", 1, true))
    end
    check("add", function() return setmetatable({}, { __add = badcall }) + 1 end)
    check("unm", function() return -setmetatable({}, { __unm = badcall }) end)
    check("len", function() return #setmetatable({}, { __len = badcall }) end)
    check("lt", function() return setmetatable({}, { __lt = badcall }) < {} end)
    check("le", function() return setmetatable({}, { __le = badcall }) <= {} end)
    check("concat", function()
      return setmetatable({}, { __concat = badcall }) .. "x"
    end)
  end
  do
    local log = {}
    local left_mt = {
      __eq = function(a, b)
        log[#log + 1] = "left:"..(a.tag or "?")..":"..(b.tag or "?")
        return true
      end,
    }
    local right_mt = {
      __eq = function(a, b)
        log[#log + 1] = "right:"..(a.tag or "?")..":"..(b.tag or "?")
        return false
      end,
    }
    local left = setmetatable({ tag = "left" }, left_mt)
    local right = setmetatable({ tag = "right" }, right_mt)
    local raw = { tag = "raw" }
    -- Lua 5.4 uses the first operand's __eq if present, otherwise the
    -- second operand's one. Lua 5.1/LuaJIT required the same __eq on both.
    assert((left == raw) == true)
    assert((raw == left) == true)
    assert((left == right) == true)
    assert((right == left) == false)
    assert(table.concat(log, ",") ==
      "left:left:raw,left:raw:left,left:left:right,right:right:left")
    do
      local bad = setmetatable({}, { __eq = true })
      local ok, err = pcall(function() return bad == {} end)
      assert(ok == false and
             err:match("attempt to call a boolean value", 1, true) and
             err:match("metamethod 'eq'", 1, true))
      ok, err = pcall(function() return {} == bad end)
      assert(ok == false and
             err:match("attempt to call a boolean value", 1, true) and
             err:match("metamethod 'eq'", 1, true))
    end
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
    do
      local named = {}
      local function meta()
        local info = debug.getinfo(1, "n")
        assert(info.namewhat == "metamethod")
        return info.name, "extra"
      end
      setmetatable(named, { __mod = meta, __band = meta })
      _G.__lua54_meta_named = named
      -- Lowered helper calls must still look like real Lua 5.4 metamethods,
      -- and only the first metamethod result participates in the expression.
      local a, b, c = assert(load("return 3 % __lua54_meta_named"))()
      assert(a == "mod" and b == nil and c == nil)
      a, b, c = assert(load("return __lua54_meta_named & 3"))()
      assert(a == "band" and b == nil and c == nil)
      setmetatable(named, { __mod = function()
        local function inner()
          local here = debug.getinfo(1, "n")
          local caller = debug.getinfo(2, "n")
          assert(here.namewhat == "local" and here.name == "inner")
          assert(caller.namewhat == "metamethod" and caller.name == "mod")
        end
        inner()
        return "mod"
      end })
      assert(assert(load("return 3 % __lua54_meta_named"))() == "mod")
      _G.__lua54_meta_named = nil
    end
    _G.__lua54_meta_lhs = nil
    _G.__lua54_meta_rhs = nil
    _G.__lua54_meta_reverse = nil
  end
  do
    local badcall = setmetatable({}, { __call = true })
    _G.__lua54_badcall = badcall
    local function check(name, src)
      local ok, err = pcall(assert(load(src)))
      assert(ok == false and
             err:match("attempt to call a boolean value", 1, true) and
             err:match("metamethod '"..name.."'", 1, true))
    end
    check("idiv", "return setmetatable({}, { __idiv = __lua54_badcall }) // 1")
    check("mod", "return setmetatable({}, { __mod = __lua54_badcall }) % 1")
    check("band", "return setmetatable({}, { __band = __lua54_badcall }) & 1")
    check("bor", "return setmetatable({}, { __bor = __lua54_badcall }) | 1")
    check("bxor", "return setmetatable({}, { __bxor = __lua54_badcall }) ~ 1")
    check("bnot", "return ~setmetatable({}, { __bnot = __lua54_badcall })")
    check("shl", "return setmetatable({}, { __shl = __lua54_badcall }) << 1")
    check("shr", "return setmetatable({}, { __shr = __lua54_badcall }) >> 1")
    _G.__lua54_badcall = nil
  end
end
do
  local function lua54_iterator_name_probe()
    local info = debug.getinfo(1, "n")
    assert(info.namewhat == "for iterator" and info.name == "for iterator")
  end
  for _ in lua54_iterator_name_probe do end
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
assert(math.type(1) == "integer")
assert(math.type(1.0) == "float")
assert(math.type(1e0) == "float")
assert(math.type(0x1) == "integer")
assert(math.type(0x1p0) == "float")
assert(math.type(1.0 + 2) == "float")
assert(math.type(1 + 2.0) == "float")
assert(math.type(1 + 2) == "integer")
assert(math.type(4 / 2) == "float")
assert(math.type(2 ^ 3) == "float")
assert(tostring(1) == "1")
assert(tostring(1.0) == "1.0")
assert(tostring(1.5) == "1.5")
assert(("x" .. 1.0) == "x1.0")
assert(table.concat({ 1.0, 2, 3.5 }, ",") == "1.0,2,3.5")
do
  local ok = true
  for i = -80, 120 do
    -- The recorder must not narrow a Lua 5.4 float constant such as 0.0 to
    -- KINT, or hot loops change int+float results back to integer.
    if math.type(i + 0.0) ~= "float" then ok = false; break end
  end
  assert(ok)
end
assert(2^54 % 3 == 1)
assert((-2^54) % 3 == 2)
assert(2^54 % -3 == -2)
assert(math.type(tonumber("1")) == "integer")
assert(math.type(tonumber("1.0")) == "float")
assert(tonumber("0x1"..string.rep("0", 30)) == 0)
assert(math.type(tonumber("0x1"..string.rep("0", 30))) == "integer")
assert(assert(load("return 0x100000000"))() == 0x100000000)
assert(tonumber("ffffFFFF", 16) + 1 == 0x100000000)
assert(tonumber("-0ffffffFFFF", 16) - 1 == -0x10000000000)
do
  assert(assert(load([[
    local wide = "9007199254740993"
    assert(wide + 0 == 9007199254740993)
    assert(math.type(wide + 0) == "integer")
    assert(wide // 1 == 9007199254740993)
    assert(math.type(wide // 1) == "integer")
    assert(wide % 10 == 3)
    assert(math.type(wide % 10) == "integer")
    assert(("9223372036854775807" + 0) == math.maxinteger)
    assert(("-9223372036854775808" + 0) == math.mininteger)
    assert(math.type(wide + 0.0) == "float")
    return true
  ]]))())
end
do
  local i = 10
  local i2 = i * i
  local i10 = i2 * i2 * i2 * i2 * i2
  -- Lua 5.4 treats a present base argument as integer-base conversion even
  -- when the base is 10; it must not fall back to decimal float scanning.
  assert(tonumber("\t10000000000\t", i) == i10)
  assert(tonumber("1.0", 10) == nil)
  assert(tonumber("1\0", 2) == nil)
  assert(pcall(tonumber, 10, 10) == false)
  do
    local ok, err = pcall(tonumber, "10", 1)
    assert(ok == false and err:find("bad argument #2 to 'tonumber' (base out of range)", 1, true))
    ok, err = pcall(tonumber, "10", 37)
    assert(ok == false and err:find("bad argument #2 to 'tonumber' (base out of range)", 1, true))
    ok, err = pcall(tonumber, "10", 1099511627776)
    assert(ok == false and err:find("bad argument #2 to 'tonumber' (base out of range)", 1, true))
    ok, err = pcall(tonumber, "10", "1099511627776")
    assert(ok == false and err:find("bad argument #2 to 'tonumber' (base out of range)", 1, true))
  end
  if jit and jit.opt then
    jit.opt.start("hotloop=1", "hotexit=1")
    for base = 2, 36 do
      local b2 = base * base
      local b10 = b2 * b2 * b2 * b2 * b2
      -- The recorder must not use decimal STRTO for explicit-base tonumber();
      -- base 10 overflows the current 32-bit integer surface in the old trace.
      assert(tonumber("\t10000000000\t", base) == b10)
    end
    jit.opt.start("hotloop=56", "hotexit=10")
  end
end
assert(math.maxinteger == 9223372036854775807)
assert(math.mininteger == -9223372036854775808)
assert(math.maxinteger > 0 and math.mininteger < 0)
assert(0x7fffffffffffffff == math.maxinteger)
assert(0x8000000000000000 == math.mininteger)
assert(0xffffffffffffffff == -1)
assert(9223372036854775807 + 1 == math.mininteger)
assert(math.type(9223372036854775807 + 1) == "integer")
assert(0x8000000000000000 - 1 == math.maxinteger)
assert(math.type(0x8000000000000000 - 1) == "integer")
do
  local i32min = -2147483648
  local zero = 0
  assert(-i32min == 2147483648)
  assert(math.type(-i32min) == "integer")
  assert(-zero == 0)
  assert(math.type(-zero) == "integer")
  assert(math.abs(i32min) == 2147483648)
  assert(math.type(math.abs(i32min)) == "integer")
end
assert(3037000499 * 3037000499 == 9223372030926249001)
assert(math.type(3037000499 * 3037000499) == "integer")
assert(type(math.tointeger) == "function")
assert(type(math.ult) == "function")
assert(math.tointeger(nil) == nil)
assert(math.tointeger("12") == 12)
assert(math.tointeger(12.0) == 12)
assert(math.tointeger(12.5) == nil)
assert(math.tointeger(2147483648) == 2147483648)
assert(math.tointeger("9223372036854775807") == math.maxinteger)
assert(math.tointeger("-9223372036854775808") == math.mininteger)
assert(math.tointeger("9223372036854775808") == nil)
assert(math.tointeger("-9223372036854775809") == math.mininteger)
do
  local f64 = 1099511627776.0
  assert(f64 == 1099511627776)
  assert(not (f64 ~= 1099511627776))
  assert(9223372036854775807.0 > math.maxinteger)
  local t = {}
  t[f64] = "wide"
  t[-f64] = "neg-wide"
  local seen_pos, seen_neg = false, false
  for k, v in pairs(t) do
    assert(math.type(k) == "integer")
    if k == 1099511627776 then
      assert(v == "wide")
      seen_pos = true
    elseif k == -1099511627776 then
      assert(v == "neg-wide")
      seen_neg = true
    else
      error("unexpected wide integer key")
    end
  end
  assert(seen_pos and seen_neg)
  assert(t[1099511627776] == "wide")
  assert(t[-1099511627776] == "neg-wide")
  assert(not pcall(next, t, f64))
  local hashints = { [0] = "zero", [-1] = "neg", [2147483647] = "max32" }
  local seen_hashints = {}
  for k, v in pairs(hashints) do
    assert(math.type(k) == "integer")
    seen_hashints[v] = true
  end
  assert(seen_hashints.zero and seen_hashints.neg and seen_hashints.max32)
  assert(not pcall(next, { [1.0] = "one" }, 1.0))
  local hi = {}
  hi[9007199254740992.0] = "exact-float"
  hi[9007199254740993] = "integer"
  assert(hi[9007199254740992] == "exact-float")
  assert(hi[9007199254740993] == "integer")
  for k, v in pairs(hi) do
    assert(math.type(k) == "integer")
    if v == "exact-float" then
      assert(k == 9007199254740992)
    elseif v == "integer" then
      assert(k == 9007199254740993)
    else
      error("unexpected high integer key")
    end
  end
end
assert(string.format("%d", math.maxinteger) == "9223372036854775807")
assert(string.format("%d", math.mininteger) == "-9223372036854775808")
assert(string.format("%u", -1) == "18446744073709551615")
assert(string.format("%x", -1) == "ffffffffffffffff")
assert(table.concat({ "a", "b" }, 1099511627776) == "a1099511627776b")
assert(math.ult(1, 2) == true)
assert(math.ult(2, 1) == false)
assert(math.ult(1, -1) == true)
assert(math.ult(-1, 1) == false)
assert(math.ult(1099511627776, 1099511627777) == true)
assert(math.ult(1099511627777, 1099511627776) == false)
assert(math.ult("1099511627776", "1099511627777") == true)
assert(select(1, pcall(math.ult, 1.5, 2)) == false)
assert(math.abs(math.mininteger) == math.mininteger)
assert(math.type(math.abs(math.mininteger)) == "integer")
assert(math.abs(math.atan(1, 0) - math.pi / 2) < 1e-14)
assert(math.atan(1, nil) == math.atan(1))
assert(math.fmod(5, 2) == 1 and math.type(math.fmod(5, 2)) == "integer")
assert(math.fmod(5.0, 2) == 1 and math.type(math.fmod(5.0, 2)) == "float")
assert(math.fmod(math.mininteger, -1) == 0)
do
  local ok_fmod, err_fmod = pcall(math.fmod, 3, 0)
  assert(ok_fmod == false and
	 err_fmod:find("bad argument #2 to 'math.fmod' (zero)", 1, true))
  ok_fmod, err_fmod = pcall(math.fmod)
  assert(ok_fmod == false and
	 err_fmod:find("bad argument #2 to 'math.fmod' (number expected, got no value)", 1, true))
  ok_fmod, err_fmod = pcall(function() return math.fmod(nil) end)
  assert(ok_fmod == false and
	 err_fmod:find("bad argument #2 to 'fmod' (number expected, got no value)", 1, true))
  ok_fmod, err_fmod = pcall(math.fmod, nil, 1)
  assert(ok_fmod == false and
	 err_fmod:find("bad argument #1 to 'math.fmod' (number expected, got nil)", 1, true))
  -- Lua 5.4 only raises on the integer fast path; float zero follows C fmod.
  local fmod_nan1 = math.fmod(3, 0.0)
  local fmod_nan2 = math.fmod(3.0, 0)
  assert(fmod_nan1 ~= fmod_nan1 and math.type(fmod_nan1) == "float")
  assert(fmod_nan2 ~= fmod_nan2 and math.type(fmod_nan2) == "float")
end
do
  local named_math_errors = {
    { "abs", math.abs }, { "acos", math.acos }, { "asin", math.asin },
    { "ceil", math.ceil }, { "cos", math.cos }, { "exp", math.exp },
    { "floor", math.floor }, { "log", math.log }, { "modf", math.modf },
    { "sin", math.sin }, { "sqrt", math.sqrt }, { "tan", math.tan },
  }
  for _, item in ipairs(named_math_errors) do
    local ok_math, err_math = pcall(item[2], true)
    assert(ok_math == false and
	   err_math:find("bad argument #1 to 'math." .. item[1] ..
			 "' (number expected, got boolean)", 1, true))
  end
  do
    local f = math.abs
    local ok_math, err_math = pcall(function() return f(true) end)
    assert(ok_math == false and
	   err_math:find("bad argument #1 to 'f' " ..
			 "(number expected, got boolean)", 1, true))
  end
  assert(math.log(8, nil) == math.log(8))
  do
    local ok_log, err_log = pcall(math.log, 8, true)
    assert(ok_log == false and
	   err_log:find("bad argument #2 to 'math.log' " ..
			"(number expected, got boolean)", 1, true))
  end
end
do
  math.randomseed(1007)
  assert(math.random(0) == 0x7a7040a5a323c9d6)
  math.randomseed(1007, 0)
  assert(math.abs(math.random() - 0x0.7a7040a5a323c9d6) < 2^-53)
  local seed1, seed2 = math.randomseed(1099511627776, "1")
  assert(seed1 == 1099511627776 and seed2 == 1)
  assert(math.type(seed1) == "integer" and math.type(seed2) == "integer")
  local full = {
    -7928649372492011025, 2966187919354626699,
    -7324652882753459820, 5615477104289094605,
    1229524019937507, 7256633879735615446,
    4945512312844809479, -1506706736174479277,
  }
  local seen_hi_pos, seen_hi_neg, seen_wide = false, false, false
  for _, want in ipairs(full) do
    local got = math.random(0)
    assert(got == want and math.type(got) == "integer")
    if got > 9007199254740992 then seen_hi_pos = true end
    if got < -9007199254740992 then seen_hi_neg = true end
    if got > 4611686018427387904 or got < -4611686018427387904 then
      seen_wide = true
    end
  end
  assert(seen_hi_pos and seen_hi_neg and seen_wide)
  seed1, seed2 = math.randomseed(math.mininteger, math.maxinteger)
  assert(seed1 == math.mininteger and seed2 == math.maxinteger)
end
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
  assert(ok == false and
         (err:match("math%.deg") ~= nil or err:match("'deg'") ~= nil) and
         err:match("number expected") ~= nil)
  ok, err = pcall(math.rad, {})
  assert(ok == false and
         (err:match("math%.rad") ~= nil or err:match("'rad'") ~= nil) and
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
  assert(tostring(setmetatable({}, { __tostring = function() return 1 end })) == "1")
  assert(tostring(setmetatable({}, { __tostring = function() return 1.0 end })) == "1.0")
  local badtostring = setmetatable({}, { __tostring = function() return {} end })
  local ok, err = pcall(tostring, badtostring)
  assert(ok == false and err:match("'__tostring' must return a string") ~= nil)
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
  local ok, err = pcall(function() coroutine.status(true) end)
  assert(ok == false and
         err:find("bad argument #1 to 'status' (thread expected, got boolean)",
                  1, true) ~= nil)
  local f = coroutine.status
  ok, err = pcall(function() f(true) end)
  assert(ok == false and
         err:find("bad argument #1 to 'f' (thread expected, got boolean)",
                  1, true) ~= nil)
  ok, err = pcall(function() coroutine.create(true) end)
  assert(ok == false and
         err:find("bad argument #1 to 'create' (function expected, got boolean)",
                  1, true) ~= nil)
  f = coroutine.create
  ok, err = pcall(function() f(true) end)
  assert(ok == false and
         err:find("bad argument #1 to 'f' (function expected, got boolean)",
                  1, true) ~= nil)
  ok, err = pcall(function() coroutine.wrap(true) end)
  assert(ok == false and
         err:find("bad argument #1 to 'wrap' (function expected, got boolean)",
                  1, true) ~= nil)
  f = coroutine.wrap
  ok, err = pcall(function() f(true) end)
  assert(ok == false and
         err:find("bad argument #1 to 'f' (function expected, got boolean)",
                  1, true) ~= nil)
  ok, err = pcall(function() coroutine.isyieldable(true) end)
  assert(ok == false and
         err:find("bad argument #1 to 'isyieldable' (thread expected, got boolean)",
                  1, true) ~= nil)
  f = coroutine.isyieldable
  ok, err = pcall(function() f(true) end)
  assert(ok == false and
         err:find("bad argument #1 to 'f' (thread expected, got boolean)",
                  1, true) ~= nil)
  ok, err = pcall(function() coroutine.status(nil) end)
  assert(ok == false and err:find("test/smoke.lua:", 1, true) ~= nil and
         err:find("bad argument #1 to 'status' (thread expected, got nil)",
                  1, true) ~= nil,
         err)
  f = coroutine.status
  ok, err = pcall(function() f(nil) end)
  assert(ok == false and err:find("test/smoke.lua:", 1, true) ~= nil and
         err:find("bad argument #1 to 'f' (thread expected, got nil)",
                  1, true) ~= nil,
         err)
  ok, err = pcall(function() coroutine.create(nil) end)
  assert(ok == false and err:find("test/smoke.lua:", 1, true) ~= nil and
         err:find("bad argument #1 to 'create' (function expected, got nil)",
                  1, true) ~= nil,
         err)
  f = coroutine.create
  ok, err = pcall(function() f(nil) end)
  assert(ok == false and err:find("test/smoke.lua:", 1, true) ~= nil and
         err:find("bad argument #1 to 'f' (function expected, got nil)",
                  1, true) ~= nil,
         err)
  ok, err = pcall(function() coroutine.wrap(nil) end)
  assert(ok == false and err:find("test/smoke.lua:", 1, true) ~= nil and
         err:find("bad argument #1 to 'wrap' (function expected, got nil)",
                  1, true) ~= nil,
         err)
  f = coroutine.wrap
  ok, err = pcall(function() f(nil) end)
  assert(ok == false and err:find("test/smoke.lua:", 1, true) ~= nil and
         err:find("bad argument #1 to 'f' (function expected, got nil)",
                  1, true) ~= nil,
         err)
  ok, err = pcall(function() coroutine.isyieldable(nil) end)
  assert(ok == false and err:find("test/smoke.lua:", 1, true) ~= nil and
         err:find("bad argument #1 to 'isyieldable' (thread expected, got nil)",
                  1, true) ~= nil,
         err)
  f = coroutine.isyieldable
  ok, err = pcall(function() f(nil) end)
  assert(ok == false and err:find("test/smoke.lua:", 1, true) ~= nil and
         err:find("bad argument #1 to 'f' (thread expected, got nil)",
                  1, true) ~= nil,
         err)
  -- Tail-position calls to Lua 5.4 coroutine wrappers must keep the source
  -- call frame; otherwise errors fall back to coroutine.xxx instead of field.
  ok, err = pcall(function() return coroutine.status(nil) end)
  assert(ok == false and err:find("test/smoke.lua:", 1, true) ~= nil and
         err:find("bad argument #1 to 'status' (thread expected, got nil)",
                  1, true) ~= nil,
         err)
  ok, err = pcall(function() return coroutine.create(nil) end)
  assert(ok == false and err:find("test/smoke.lua:", 1, true) ~= nil and
         err:find("bad argument #1 to 'create' (function expected, got nil)",
                  1, true) ~= nil,
         err)
  ok, err = pcall(function() return coroutine.wrap(nil) end)
  assert(ok == false and err:find("test/smoke.lua:", 1, true) ~= nil and
         err:find("bad argument #1 to 'wrap' (function expected, got nil)",
                  1, true) ~= nil,
         err)
  ok, err = pcall(function() return coroutine.isyieldable(nil) end)
  assert(ok == false and err:find("test/smoke.lua:", 1, true) ~= nil and
         err:find("bad argument #1 to 'isyieldable' (thread expected, got nil)",
                  1, true) ~= nil,
         err)
  ok, err = pcall(coroutine.resume)
  assert(ok == false and
         err:find("bad argument #1 to 'coroutine.resume' (thread expected, got no value)",
                  1, true) ~= nil)
  ok, err = pcall(function() coroutine.resume() end)
  assert(ok == false and
         err:find("bad argument #1 to 'resume' (thread expected, got no value)",
                  1, true) ~= nil)
  ok, err = pcall(function() coroutine.resume(nil) end)
  assert(ok == false and
         err:find("bad argument #1 to 'resume' (thread expected, got nil)",
                  1, true) ~= nil)
  f = coroutine.resume
  ok, err = pcall(function() f() end)
  assert(ok == false and
         err:find("bad argument #1 to 'f' (thread expected, got no value)",
                  1, true) ~= nil)
  ok, err = pcall(function() return coroutine.resume() end)
  assert(ok == false and
         err:find("bad argument #1 to 'resume' (thread expected, got no value)",
                  1, true) ~= nil)
  f = coroutine.resume
  ok, err = pcall(function() return f() end)
  assert(ok == false and
         err:find("bad argument #1 to 'f' (thread expected, got no value)",
                  1, true) ~= nil)
  ok, err = pcall(coroutine.close)
  assert(ok == false and
         err:find("bad argument #1 to 'coroutine.close' (thread expected, got no value)",
                  1, true) ~= nil)
  ok, err = pcall(function() coroutine.close() end)
  assert(ok == false and
         err:find("bad argument #1 to 'close' (thread expected, got no value)",
                  1, true) ~= nil)
  ok, err = pcall(function() coroutine.close(nil) end)
  assert(ok == false and
         err:find("bad argument #1 to 'close' (thread expected, got nil)",
                  1, true) ~= nil)
  f = coroutine.close
  ok, err = pcall(function() f() end)
  assert(ok == false and
         err:find("bad argument #1 to 'f' (thread expected, got no value)",
                  1, true) ~= nil)
  ok, err = pcall(function() return coroutine.close() end)
  assert(ok == false and
         err:find("bad argument #1 to 'close' (thread expected, got no value)",
                  1, true) ~= nil)
  f = coroutine.close
  ok, err = pcall(function() return f() end)
  assert(ok == false and
         err:find("bad argument #1 to 'f' (thread expected, got no value)",
                  1, true) ~= nil)
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
  local function recurse_resume()
    local co = coroutine.create(recurse_resume)
    local _, err = coroutine.resume(co)
    return err
  end
  assert(recurse_resume():match("C stack overflow", 1, true) ~= nil)
end
do
  local overflow_line = debug.getinfo(1, "l").currentline + 1
  local function auxy() auxy() end
  local handler_line
  local function entry(x)
    handler_line = debug.getinfo(x, "l").currentline + 2
    collectgarbage("stop")
    auxy()
    collectgarbage("restart")
  end
  local _, tb = xpcall(entry, debug.traceback, 1)
  collectgarbage("restart")
  assert(type(tb) == "string" and tb:match("stack overflow", 1, true) ~= nil)
  local seen = 0
  for line in tb:gmatch("[^\n]*") do
    local curr = tonumber(line:match(":(%d+):"))
    if curr == handler_line then break end
    if curr then
      assert(curr == overflow_line)
      seen = seen + 1
    end
  end
  -- Lua 5.4 keeps enough emergency stack to let traceback show recursive
  -- overflow frames before returning to the xpcall handler.
  assert(seen > 5)
end
do
  local function loop()
    return 1 + loop()
  end
  local _, msg = xpcall(loop, function(err)
    assert(err:match("stack overflow", 1, true) ~= nil)
    local ok, nested = pcall(loop)
    -- A second overflow while already running an error handler is reported as
    -- error-handler failure, matching Lua 5.4's LUA_ERRERR behavior.
    assert(ok == false and nested:match("error handling", 1, true) ~= nil)
    return 15
  end)
  assert(msg == 15)
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
  assert(string.format("%s|%s|%s", -3, 2147483651, 31) ==
         "-3|2147483651|31")
  ok, err = pcall(string.format, "%d", true)
  assert(ok == false and err:match("string%.format") ~= nil and
         err:match("number expected") ~= nil)
  do
    local function field_format_error()
      return string.format("%d", true)
    end
    ok, err = pcall(field_format_error)
    assert(ok == false and
           err:find("test/smoke.lua:", 1, true) ~= nil and
           err:find("bad argument #2 to 'format' (number expected, got boolean)", 1, true) ~= nil,
           err)
    local f = string.format
    local function local_format_error()
      return f("%d", true)
    end
    ok, err = pcall(local_format_error)
    assert(ok == false and
           err:find("test/smoke.lua:", 1, true) ~= nil and
           err:find("bad argument #2 to 'f' (number expected, got boolean)", 1, true) ~= nil,
           err)
  end
  ok, err = pcall(string.format, "%I", 1)
  assert(ok == false and err == "invalid conversion '%I' to 'format'")
  ok, err = pcall(string.format, "%U", 0x20ac)
  assert(ok == false and err == "invalid conversion '%U' to 'format'")
  assert(string.format("%q", nil) == "nil")
  assert(string.format("%q", true) == "true")
  assert(string.format("%q", 1) == "1")
  assert(string.format("%q", 1.5) == "0x1.8p+0")
  assert(string.format("%q", -0.0) == "-0x0p+0")
  assert(string.format("%q", 0 / 0) == "(0/0)")
  assert(string.format("%q", math.huge) == "1e9999")
  assert(string.format("%q", -math.huge) == "-1e9999")
  ok, err = pcall(string.format, "%q", {})
  assert(ok == false and err:match("string%.format") ~= nil and
         err:match("literal form") ~= nil)
  ok, err = pcall(string.format, "%q",
    setmetatable({}, { __tostring = function() return "x" end }))
  assert(ok == false and err:match("string%.format") ~= nil and
         err:match("literal form") ~= nil)
  assert(select(1, pcall(string.format, "%c", 65.5)) == false)
  assert(string.format("%c", "65") == "A")
  assert(string.format("%p", nil) == "(null)")
  assert(string.format("%p", false) == "(null)")
  assert(string.format("%p", true) == "(null)")
  assert(string.format("%p", 1) == "(null)")
  assert(string.format("%p", "x") ~= "(null)")
  do
    local short1 = string.rep("a", 10)
    local short2 = string.rep("aa", 5)
    local long1 = string.rep("a", 300)
    local long2 = string.rep("aa", 150)
    -- Lua 5.4 only interns short strings. Long strings with equal bytes keep
    -- distinct object identities, but equality and table keys still use bytes.
    assert(short1 == short2)
    assert(string.format("%p", short1) == string.format("%p", short2))
    assert(long1 == long2)
    assert(string.format("%p", long1) ~= string.format("%p", long2))
    local t = {}
    t[long1] = 45
    assert(t[long2] == 45)
    local changed = (long1:gsub(".", function(c) return c end))
    assert(changed == long1)
    assert(string.format("%p", changed) ~= string.format("%p", long1))
  end
  do
    local old = os.setlocale(nil, "collate")
    local loc
    for _, name in ipairs({ "ptb", "pt_BR.iso88591", "ISO-8859-1" }) do
      if os.setlocale(name, "collate") then
        loc = name
        break
      end
    end
    if loc then
      local accented = string.char(0xe1) .. "lo"
      -- Lua 5.4 string ordering follows the active collate locale; byte order
      -- would put this accented byte after 'm' and fail the official test.
      assert("alo" < accented and accented < "amo")
      assert("alo" <= accented and accented <= "amo")
      if jit and jit.opt then
        jit.opt.start("hotloop=1", "hotexit=1")
        for _ = 1, 8 do
          assert("alo" < accented and accented < "amo")
        end
      end
    end
    if old then os.setlocale(old, "collate") end
  end
  do
    local old = os.setlocale(nil, "ctype")
    local loc
    for _, name in ipairs({ "ptb", "pt_BR.iso88591", "ISO-8859-1" }) do
      if os.setlocale(name, "ctype") then
        loc = name
        break
      end
    end
    if loc then
      local lower = string.char(0xe1, 0xe9, 0xed, 0xf3, 0xfa)
      -- Pattern character classes and case conversion are locale-sensitive in
      -- Lua 5.4, unlike LuaJIT's fixed ASCII lj_char table.
      assert((lower:gsub("%a", "x")) == "xxxxx")
      assert(string.upper(string.char(0xe1)) == string.char(0xc1))
      assert(string.lower(string.char(0xc1)) == string.char(0xe1))
    end
    if old then os.setlocale(old, "ctype") end
  end
  do
    local null = "(null)"
    assert(#string.format("%90p", {}) == 90)
    assert(#string.format("%-60p", {}) == 60)
    assert(string.format("%10p", false) == string.rep(" ", 10 - #null) .. null)
    assert(string.format("%-12p", 1.5) == null .. string.rep(" ", 12 - #null))
  end
  assert(string.format("%s", "\0") == "\0")
  ok, err = pcall(string.format, "%10s", "\0")
  assert(ok == false and err:match("contains zeros") ~= nil)
  ok, err = pcall(string.format, "%.1s", "\0")
  assert(ok == false and err:match("contains zeros") ~= nil)
  do
    local fmt_meta = setmetatable({}, {
      __tostring = function() return "hello" end,
      __name = "fmtmeta"
    })
    assert(string.format("%s %.10s", fmt_meta, fmt_meta) == "hello hello")
    getmetatable(fmt_meta).__tostring = nil
    assert(string.format("%.4s", fmt_meta) == "fmtm")
    getmetatable(fmt_meta).__tostring = function() return {} end
    ok, err = pcall(string.format, "%s", fmt_meta)
    assert(ok == false and err:find("'__tostring' must return a string", 1, true), err)
  end
  do
    local function expect_format_error(fmt, msg, value, exact)
      local ok_fmt, err_fmt = pcall(string.format, fmt, value or 10)
      if exact then
        assert(ok_fmt == false and err_fmt == msg, err_fmt)
      else
        assert(ok_fmt == false and err_fmt:match(msg) ~= nil)
      end
    end
    local function expect_format_noarg_error(fmt)
      local ok_fmt, err_fmt = pcall(string.format, fmt)
      assert(ok_fmt == false and
             err_fmt == "bad argument #2 to 'string.format' (no value)",
             err_fmt)
    end
    local long_width = string.rep("0", 600)
    expect_format_noarg_error("%")
    expect_format_noarg_error("%.")
    expect_format_noarg_error("%#i")
    expect_format_noarg_error("%F")
    expect_format_noarg_error("%" .. string.rep("9", 21) .. "s")
    expect_format_error("%100.3d", "invalid conversion")
    expect_format_error("%" .. string.rep("9", 21) .. "s",
                        "invalid format (too long)", "x", true)
    expect_format_error("%1." .. string.rep("9", 19) .. "s",
                        "invalid format (too long)", "x", true)
    expect_format_error("%1" .. long_width .. ".3d", "too long")
    expect_format_error("%1.100d", "invalid conversion")
    expect_format_error("%" .. long_width .. "d", "too long")
    expect_format_error("%010c", "invalid conversion")
    expect_format_error("%.10c", "invalid conversion")
    expect_format_error("%0.34s", "invalid conversion", "abc")
    expect_format_error("%#i", "invalid conversion")
    expect_format_error("%3.1p", "invalid conversion specification: '%3.1p'", {}, true)
    expect_format_error("%#p", "invalid conversion specification: '%#p'", {}, true)
    expect_format_error("%0.s", "invalid conversion", "abc")
    expect_format_error("%10q", "cannot have modifiers", "abc")
    expect_format_error("%F", "invalid conversion '%F' to 'format'", 1.5, true)
    expect_format_error("%#a", "modifiers for format '%a'/'%A' not implemented", 1.5, true)
    expect_format_error("%#A", "modifiers for format '%a'/'%A' not implemented", 1.5, true)
    expect_format_error("%10a", "modifiers for format '%a'/'%A' not implemented", 1.5, true)
    expect_format_error("%.3a", "modifiers for format '%a'/'%A' not implemented", 1.5, true)
    expect_format_error("%+a", "modifiers for format '%a'/'%A' not implemented", 1.5, true)
    assert(string.format("%a", 1.5) == "0x1.8p+0")
    assert(string.format("%A", 1.5) == "0X1.8P+0")
    expect_format_error("%d %d", "no value")
    do
      local function tail_format_error()
        return string.format("%#p", {})
      end
      local ok_tail, err_tail = pcall(tail_format_error)
      assert(ok_tail == false and
             err_tail:find("test/smoke.lua:", 1, true) ~= nil and
             err_tail:find("invalid conversion specification: '%#p'", 1, true) ~= nil,
             err_tail)
      local function dynamic_tail_format_error()
        return string.format("%" .. string.rep("9", 21) .. "s", "x")
      end
      ok_tail, err_tail = pcall(dynamic_tail_format_error)
      assert(ok_tail == false and
             err_tail:find("test/smoke.lua:", 1, true) ~= nil and
             err_tail:find("invalid format (too long)", 1, true) ~= nil, err_tail)
      local dynfmt = string.format
      local function dynamic_alias_tail_format_error()
        return dynfmt("%" .. string.rep("9", 21) .. "s", "x")
      end
      ok_tail, err_tail = pcall(dynamic_alias_tail_format_error)
      assert(ok_tail == false and
             err_tail:find("test/smoke.lua:", 1, true) ~= nil and
             err_tail:find("invalid format (too long)", 1, true) ~= nil, err_tail)
    end
  end
  do
    local function expect_pack_error(f, fname, msg, ...)
      local ok_pack, err_pack = pcall(f, ...)
      assert(ok_pack == false and err_pack:find("to '" .. fname .. "'", 1, true) ~= nil)
      assert(err_pack:find(msg, 1, true) ~= nil)
    end
    local function expect_pack_parser_error(f, msg, ...)
      local ok_pack, err_pack = pcall(f, ...)
      assert(ok_pack == false and err_pack:find(msg, 1, true) ~= nil, err_pack)
      assert(err_pack:find("bad argument", 1, true) == nil, err_pack)
    end
    expect_pack_parser_error(string.pack, "missing size for format option 'c'", "c", "x")
    expect_pack_error(string.pack, "string.pack", "string contains zeros", "z", "a\0b")
    expect_pack_error(string.pack, "string.pack", "invalid next option for option 'X'", "X ", 1)
    expect_pack_error(string.pack, "string.pack", "format asks for alignment not power of 2", "!3i", 1)
    expect_pack_error(string.pack, "string.pack", "number has no integer representation", "b", 1.2)
    expect_pack_error(string.pack, "string.pack", "number expected, got nil", "i1")
    expect_pack_error(string.pack, "string.pack", "number expected, got nil", "f")
    expect_pack_error(string.pack, "string.pack", "string expected, got nil", "c1")
    expect_pack_error(string.pack, "string.pack", "string expected, got nil", "z")
    expect_pack_error(string.pack, "string.pack", "string expected, got nil", "s1")
    expect_pack_parser_error(string.packsize, "missing size for format option 'c'", "c")
    expect_pack_error(string.packsize, "string.packsize", "variable-length format", "z")
    expect_pack_error(string.packsize, "string.packsize", "format asks for alignment not power of 2", "!3i")
    expect_pack_parser_error(string.unpack, "missing size for format option 'c'", "c", "")
    expect_pack_error(string.unpack, "string.unpack", "unfinished string for format 'z'", "z", "abc")
    expect_pack_error(string.unpack, "string.unpack", "data string too short", "s1", "")
    expect_pack_error(string.unpack, "string.unpack", "data string too short", "s1", string.char(2) .. "a")
    expect_pack_parser_error(string.packsize, "integral size (0) out of limits [1,16]", "!0i")
    expect_pack_parser_error(string.packsize, "integral size (17) out of limits [1,16]", "!17i")
    expect_pack_parser_error(string.pack, "integral size (17) out of limits [1,16]", "!17i", 1)
    expect_pack_parser_error(string.unpack, "integral size (17) out of limits [1,16]", "!17i", "")
    expect_pack_parser_error(string.packsize, "integral size (0) out of limits [1,16]", "s0")
    expect_pack_parser_error(string.packsize, "integral size (17) out of limits [1,16]", "s17")
    expect_pack_parser_error(string.packsize, "integral size (999999999) out of limits [1,16]",
                             "s999999999999999999999999")
    expect_pack_parser_error(string.packsize, "integral size (214748364) out of limits [1,16]",
                             "i2147483647")
    expect_pack_parser_error(string.packsize, "invalid format option '9'",
                             "c9999999999")
  end
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
  do
    local huge_rep = 1073741824
    local ok_rep, err_rep = pcall(string.rep, "aa", huge_rep)
    local ok_sep, err_sep = pcall(string.rep, "a", huge_rep, ",")
    assert(ok_rep == false and err_rep:match("too large") ~= nil)
    assert(ok_sep == false and err_sep:match("too large") ~= nil)
  end
  expect_bad_integer(string.find, "abc", "b", 1.2)
  expect_bad_integer(string.match, "abc", "b", 1.2)
  expect_bad_integer(string.gmatch, "abc", "b", 1.2)
  expect_bad_integer(string.gsub, "aaa", "a", "b", 1.2)
  do
    local wide = 1099511627776
    local bytes = { string.byte("abc", -wide, -1) }
    assert(#bytes == 3 and bytes[1] == 97 and bytes[2] == 98 and bytes[3] == 99)
    assert(select("#", string.byte("abc", wide)) == 0)
    assert(string.sub("abc", -wide, wide) == "abc")
    local first, last = string.find("abc", "b", -wide, true)
    assert(first == 2 and last == 2)
    local none = string.gmatch("abc", ".", wide)
    assert(none() == nil)
    local iter = string.gmatch("abc", ".", -wide)
    assert(iter() == "a")
  end
  do
    local function tail_byte_method(s)
      return s:byte({})
    end
    local function tail_find_method(s)
      return s:find({})
    end
    local function tail_format_method(s)
      return s:format(true)
    end
    -- String method syntax hides the self argument from Lua 5.4 diagnostics.
    -- Tail calls must keep the caller frame or the C fallback reports
    -- string.byte/string.find/string.format with the hidden self counted as
    -- argument #1.
    local ok_method, err_method = pcall(tail_byte_method, "abc")
    assert(ok_method == false and
	   err_method:find("test/smoke.lua:", 1, true) and
	   err_method:find("bad argument #1 to 'byte' (number expected, got table)",
			   1, true))
    ok_method, err_method = pcall(tail_find_method, "abc")
    assert(ok_method == false and
	   err_method:find("test/smoke.lua:", 1, true) and
	   err_method:find("bad argument #1 to 'find' (string expected, got table)",
			   1, true))
    ok_method, err_method = pcall(tail_format_method, "%d")
    assert(ok_method == false and
	   err_method:find("test/smoke.lua:", 1, true) and
	   err_method:find("bad argument #1 to 'format' (number expected, got boolean)",
			   1, true))
  end
  assert(string.gsub("a b cd", " *", "-") == "-a-b-c-d-")
  do
    local sub = "a  \nbc\t\td"
    local res = ""
    local i = 1
    for p, e in string.gmatch(sub, "()%s*()") do
      res = res .. string.sub(sub, i, p - 1) .. "-"
      i = e
    end
    assert(res .. string.sub(sub, i) == "-a-b-c-d-")
  end
  do
    local iter = string.gmatch("abc", ".")
    local info = debug.getinfo(iter, "u")
    assert(info.nups == 3)
    local n1, v1 = debug.getupvalue(iter, 1)
    local n2, v2 = debug.getupvalue(iter, 2)
    local n3, v3 = debug.getupvalue(iter, 3)
    assert(n1 == "" and v1 == "abc")
    assert(n2 == "" and v2 == ".")
    assert(n3 == "" and type(v3) == "userdata")
    assert(select("#", debug.getupvalue(iter, 4)) == 0)
    assert(iter() == "a" and iter() == "b" and iter() == "c")
    assert(iter() == nil)
  end
  do
    local ok_rep, err_rep = pcall(string.gsub, "alo", ".")
    assert(ok_rep == false and
	   err_rep:find("bad argument #3 to 'string.gsub' (string/function/table expected, got no value)", 1, true))
    ok_rep, err_rep = pcall(function() return string.gsub("alo", ".", nil) end)
    assert(ok_rep == false and
	   err_rep:find("bad argument #3 to 'gsub' (string/function/table expected, got nil)", 1, true))
    ok_rep, err_rep = pcall(function()
      local f = string.gsub
      return f("alo", ".", true)
    end)
    assert(ok_rep == false and
	   err_rep:find("bad argument #3 to 'f' (string/function/table expected, got boolean)", 1, true))
    local ok_cap, err_cap = pcall(string.gsub, "alo", ".", "%2")
    assert(ok_cap == false and err_cap:match("invalid capture index %%2") ~= nil)
    ok_cap, err_cap = pcall(string.gsub, "alo", "(%0)", "a")
    assert(ok_cap == false and err_cap:match("invalid capture index %%0") ~= nil)
    ok_cap, err_cap = pcall(string.gsub, "alo", "(%1)", "a")
    assert(ok_cap == false and err_cap:match("invalid capture index %%1") ~= nil)
    ok_cap, err_cap = pcall(string.gsub, "alo", ".", "%x")
    assert(ok_cap == false and err_cap:match("invalid use of '%%'") ~= nil)
    ok_cap, err_cap = pcall(string.gsub, "alo", ".", "%")
    assert(ok_cap == false and err_cap:match("invalid use of '%%'") ~= nil)
    ok_cap, err_cap = pcall(string.find, "a", "%b")
    assert(ok_cap == false and
      err_cap:match("missing arguments to '%%b'") ~= nil)
    ok_cap, err_cap = pcall(string.find, "a", "%ba")
    assert(ok_cap == false and
      err_cap:match("missing arguments to '%%b'") ~= nil)
  end
end
do
  assert(debug.getuservalue(io.stdout) == nil)
  assert(debug.getuservalue(io.stdout, 1) == nil)
  assert(debug.setuservalue(io.stdout, {}, 1) == nil)
  local light = debug.upvalueid(function() return debug end, 1)
  local ok, err = pcall(debug.setuservalue, light, {})
  assert(ok == false and err:match("light userdata", 1, true) ~= nil)
  local stripped = assert(load(string.dump(function(a) return a + 1 end, true)))
  ok, err = pcall(stripped, {})
  assert(ok == false and err:match("^%?:%-1:") ~= nil)
  _G.__lua54_named_object = setmetatable({}, { __name = "Lua54NamedObject" })
  ok, err = pcall(io.input, __lua54_named_object)
  _G.__lua54_named_object = nil
  assert(ok == false and err:match("FILE*", 1, true) ~= nil and
         err:match("Lua54NamedObject", 1, true) ~= nil)
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
    { "debug.getmetatable", function() return pcall(debug.getmetatable) end },
    { "debug.setmetatable", function() return pcall(debug.setmetatable) end },
    { "debug.setuservalue", function() return pcall(debug.setuservalue, nil, {}) end },
    { "debug.setcstacklimit", function() return pcall(debug.setcstacklimit, nil) end },
  }) do
    local ok, err = item[2]()
    assert(ok == false and err:match("to '"..item[1].."'") ~= nil)
  end
  local ok, err = pcall(debug.getmetatable)
  assert(ok == false and
         err:find("bad argument #1 to 'debug.getmetatable' (value expected)",
                  1, true) ~= nil)
  ok, err = pcall(debug.setmetatable, 1, true)
  assert(ok == false and
         err:find("bad argument #2 to 'debug.setmetatable' (nil or table expected, got boolean)",
                  1, true) ~= nil)
  ok, err = pcall(debug.setmetatable, 1, "x")
  assert(ok == false and
         err:find("bad argument #2 to 'debug.setmetatable' (nil or table expected, got string)",
                  1, true) ~= nil)
  assert(debug.getuservalue() == nil)
  assert(debug.getuservalue(nil) == nil)
  assert(debug.getuservalue(true) == nil)
  ok, err = pcall(debug.getuservalue, true, true)
  assert(ok == false and
         err:find("bad argument #2 to 'debug.getuservalue' (number expected, got boolean)",
                  1, true) ~= nil)
  ok, err = pcall(debug.setuservalue, io.stdout)
  assert(ok == false and
         err:find("bad argument #2 to 'debug.setuservalue' (value expected)",
                  1, true) ~= nil)
  ok, err = pcall(debug.setuservalue, true, true, true)
  assert(ok == false and
         err:find("bad argument #3 to 'debug.setuservalue' (number expected, got boolean)",
                  1, true) ~= nil)
end
do
  local ok, err = pcall(table.concat, nil)
  assert(ok == false and err:find("bad argument #1 to 'table.concat' (table expected, got nil)", 1, true) ~= nil)
  ok, err = pcall(table.concat, {}, true)
  assert(ok == false and err:find("bad argument #2 to 'table.concat' (string expected, got boolean)", 1, true) ~= nil)
  ok, err = pcall(table.insert, nil, 1)
  assert(ok == false and err:find("bad argument #1 to 'table.insert' (table expected, got nil)", 1, true) ~= nil)
  ok, err = pcall(table.remove, nil)
  assert(ok == false and err:find("bad argument #1 to 'table.remove' (table expected, got nil)", 1, true) ~= nil)
  ok, err = pcall(table.sort, nil)
  assert(ok == false and err:find("bad argument #1 to 'table.sort' (table expected, got nil)", 1, true) ~= nil)
  ok, err = pcall(table.sort, {}, true)
  assert(ok == true and err == nil)
  ok, err = pcall(table.sort, { 1 }, true)
  assert(ok == true and err == nil)
  ok, err = pcall(table.sort, { 2, 1 }, true)
  assert(ok == false and err:find("bad argument #2 to 'table.sort' (function expected, got boolean)", 1, true) ~= nil)
  ok, err = pcall(table.move)
  assert(ok == false and err:find("bad argument #2 to 'table.move' (number expected, got no value)", 1, true) ~= nil)
  ok, err = pcall(function() table.concat(nil) end)
  assert(ok == false and err:find("bad argument #1 to 'concat' (table expected, got nil)", 1, true) ~= nil)
  ok, err = pcall(function() return table.concat(nil) end)
  assert(ok == false and err:find("bad argument #1 to 'concat' (table expected, got nil)", 1, true) ~= nil)
  local f = table.concat
  ok, err = pcall(function() f(nil) end)
  assert(ok == false and err:find("bad argument #1 to 'f' (table expected, got nil)", 1, true) ~= nil)
  ok, err = pcall(function() return f(nil) end)
  assert(ok == false and err:find("bad argument #1 to 'f' (table expected, got nil)", 1, true) ~= nil)
  ok, err = pcall(function() return table.sort(nil) end)
  assert(ok == false and err:find("bad argument #1 to 'sort' (table expected, got nil)", 1, true) ~= nil)
  ok, err = pcall(function() table.sort({ 1, 2, 3 }, table.sort) end)
  assert(ok == false and err:find("bad argument #1 to 'table.sort' (table expected, got number)", 1, true) ~= nil)
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
  local big = 1099511627776
  local seen_key
  local wide_proxy = setmetatable({}, {
    __len = function() return big end,
    __index = function(_, k)
      seen_key = k
      if k == big then return "wide" end
    end,
  })
  assert(table.concat(wide_proxy, ",", big) == "wide")
  assert(seen_key == big and math.type(seen_key) == "integer")
  seen_key = nil
  assert(table.concat(wide_proxy, ",", big + 1) == "")
  assert(seen_key == nil)
  ok, err = pcall(table.concat, { 1, 2, 3 }, ",", 1.2, 2)
  assert(ok == false and err:match("number has no integer representation") ~= nil)
  ok, err = pcall(table.concat, { 1, 2, 3 }, ",", 1, 2.2)
  assert(ok == false and err:match("number has no integer representation") ~= nil)
  assert(table.concat({}, ",", 1, 0) == "")
  assert(table.concat({ 1, 2 }, ",", 2, 1) == "")
  do
    local maxi = math.maxinteger
    assert(table.concat({ [maxi] = "alo" }, "x", maxi, maxi) == "alo")
    assert(table.concat({ [maxi - 1] = "y", [maxi] = "alo" },
      "-", maxi - 1, maxi) == "y-alo")
  end
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
  local zero = { [0] = "ban" }
  assert(#zero == 0 and table.remove(zero) == "ban" and zero[0] == nil)
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
  local big = 1099511627776
  local nextbig = big + 1
  local get_keys = {}
  local set_keys = {}
  local wide_proxy = setmetatable({}, {
    __len = function() return big end,
    __index = function(_, k)
      get_keys[#get_keys + 1] = k
      if k == big then return "wide-last" end
    end,
    __newindex = function(_, k, v)
      assert(v == nil and math.type(k) == "integer")
      set_keys[#set_keys + 1] = k
    end,
  })
  assert(table.remove(wide_proxy, big) == "wide-last")
  assert(table.remove(wide_proxy, nextbig) == nil)
  assert(get_keys[1] == big and set_keys[1] == big)
  assert(get_keys[2] == nextbig and set_keys[2] == nextbig)
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
  ok, err = pcall(table.move, {}, 0, math.maxinteger, 1)
  assert(ok == false and err:find("bad argument #3 to 'table.move' (too many elements to move)", 1, true) ~= nil)
  ok, err = pcall(table.move, {}, 1, math.maxinteger, 2)
  assert(ok == false and err:find("bad argument #4 to 'table.move' (destination wrap around)", 1, true) ~= nil)
  local edge_dst = table.move({ [math.maxinteger - 2] = 1,
                                [math.maxinteger - 1] = 2,
                                [math.maxinteger] = 3 },
                              math.maxinteger - 2, math.maxinteger, -10, {})
  assert(edge_dst[-10] == 1 and edge_dst[-9] == 2 and edge_dst[-8] == 3)
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
  local oldmt = debug.getmetatable(0)
  local slots = {}
  local ok, err = pcall(function()
    debug.setmetatable(0, {
      __len = function(self) return #slots[self] end,
      __index = function(self, k) return slots[self][k] end,
      __newindex = function(self, k, v) slots[self][k] = v end,
    })
    slots[0] = { "a", "b" }
    assert(table.concat(0, ",") == "a,b")

    slots[0] = { "b", "c" }
    table.insert(0, 1, "a")
    assert(slots[0][1] == "a" and slots[0][2] == "b" and
	   slots[0][3] == "c")
    assert(table.remove(0, 2) == "b")
    assert(slots[0][1] == "a" and slots[0][2] == "c" and
	   slots[0][3] == nil)

    slots[0] = { 3, 1, 2 }
    table.sort(0)
    assert(slots[0][1] == 1 and slots[0][2] == 2 and slots[0][3] == 3)

    debug.setmetatable(0, {
      __index = function(self, k) return slots[self][k] end,
      __newindex = function(self, k, v) slots[self][k] = v end,
    })
    slots[0] = { "x", "y" }
    slots[1] = {}
    assert(table.move(0, 1, 2, 3, 1) == 1)
    assert(slots[1][3] == "x" and slots[1][4] == "y")
  end)
  debug.setmetatable(0, oldmt)
  assert(ok, err)
end
do
  local huge = setmetatable({}, { __len = function() return math.maxinteger end })
  local ok, err = pcall(table.sort, huge)
  assert(ok == false and err:match("too big", 1, true) ~= nil)
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
  assert(utf8.charpattern:byte(2) == 0)
  do
    local found = {}
    assert(("a"):match("^"..utf8.charpattern.."$") == "a")
    assert(("\0"):match("^"..utf8.charpattern.."$") == "\0")
    assert(extended:match("^"..utf8.charpattern.."$") == extended)
    for ch in ("a\0"..extended):gmatch(utf8.charpattern) do
      found[#found+1] = ch
    end
    assert(#found == 3 and found[1] == "a" and found[2] == "\0" and found[3] == extended)
  end
  do
    local subject = "xa\0bcy"
    local i, j = subject:find("a\0.c")
    assert(i == 2 and j == 5)
    assert(("a\0bc"):match("^a\0.c$") == "a\0bc")
    local replaced, n = ("a\0bc"):gsub("a\0.", "X")
    assert(replaced == "Xc" and n == 1)
  end
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
  do
    local f = assert(load("return '\\u{110000}', '\\u{200000}', '\\u{7fffffff}', '\\u{d800}'"))
    local u4, u5, u6, surrogate_literal = f()
    assert(u4 == utf8.char(0x110000))
    assert(u5 == utf8.char(0x200000))
    assert(u6 == utf8.char(0x7fffffff))
    assert(surrogate_literal == surrogate)
    assert(load("return '\\u{80000000}'") == nil)
  end
  local ok, err = pcall(utf8.char, 0x80000000)
  assert(ok == false and err:match("value out of range") ~= nil)
  assert(select(1, pcall(utf8.char, 97.2)) == false)
  assert(select(1, pcall(utf8.codepoint, s, 1.2)) == false)
  do
    local ok_i, err_i = pcall(utf8.codepoint, "abc", 0, 1)
    local ok_j, err_j = pcall(utf8.codepoint, "abc", 4)
    assert(ok_i == false and err_i:match("out of bounds"))
    assert(ok_j == false and err_j:match("out of bounds"))
    assert(select("#", utf8.codepoint("abc", 5, 3)) == 0)
  end
  assert(select(1, pcall(utf8.len, s, 1.2)) == false)
  assert(select(1, pcall(utf8.offset, s, 1.2)) == false)
  assert(select(1, pcall(utf8.offset, s, 1, 1.2)) == false)
  do
    local ok_direct, err_direct = pcall(function()
      return utf8.codes("\128")
    end)
    local ok_field, err_field = pcall(assert(load([[
      return utf8.codes("\128")
    ]])))
    local ok_alias, err_alias = pcall(assert(load([[
      local f = utf8.codes
      return f("\128")
    ]])))
    assert(ok_direct == false and
           err_direct:match("bad argument #1 to 'codes'", 1, true))
    assert(ok_field == false and
           err_field:match("bad argument #1 to 'codes'", 1, true))
    assert(ok_alias == false and
           err_alias:match("bad argument #1 to 'f'", 1, true))
  end
  do
    local ok_len_i, err_len_i = pcall(utf8.len, "abc", 0, 2)
    local ok_len_j, err_len_j = pcall(utf8.len, "abc", 1, 4)
    local ok_off, err_off = pcall(utf8.offset, "abc", 1, 5)
    assert(ok_len_i == false and err_len_i:match("initial position out of bounds"))
    assert(ok_len_j == false and err_len_j:match("final position out of bounds"))
    assert(ok_off == false and err_off:match("position out of bounds"))
  end
  do
    local wide = 1099511627776
    local ok_len, err_len = pcall(utf8.len, "abc", -wide, -1)
    local ok_off_pos, err_off_pos = pcall(utf8.offset, "abc", 1, wide)
    local ok_off_neg, err_off_neg = pcall(utf8.offset, "abc", 1, -wide)
    assert(ok_len == false and err_len:match("initial position out of bounds"))
    assert(ok_off_pos == false and err_off_pos:match("position out of bounds"))
    assert(ok_off_neg == false and err_off_neg:match("position out of bounds"))
  end
  assert(utf8.charpattern:find("\253", 1, true) ~= nil)
  do
    local f = utf8.codes("")
    assert(f("", 2) == nil)
    assert(f("", -1) == nil)
    assert(f("", math.mininteger) == nil)
  end
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
    do
      local extended_pair = extended..utf8.char(0x3ffffff)
      assert(utf8.offset(extended_pair, 2) == #extended + 1)
      assert(utf8.offset(extended_pair, -1, #extended_pair + 1) == #extended + 1)
    end
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
  do
    local function countlines(s)
      return select(2, string.gsub(s, "\n", ""))
    end
    local function deep(level, skip)
      if level == 0 then
        return (debug.traceback("message", skip))
      end
      return (deep(level - 1, skip))
    end
    local function checkdeep()
      local trace = deep(21, 1)
      return trace
    end
    local trace = coroutine.wrap(checkdeep)()
    local rest = assert(trace:match("^message\nstack traceback:\n(.*)$"))
    local brk = assert(rest:find("%.%.%.\t%(skip"))
    assert(countlines(rest:sub(1, brk)) == 10)
    assert(countlines(rest:sub(brk)) == 11)
    local ctrace = debug.traceback(nil)
    assert(ctrace:find("[C]: in ?", 1, true) ~= nil)
    assert(ctrace:find("[C]: at 0x", 1, true) == nil)
    local co = coroutine.create(function()
      return math.abs(true)
    end)
    local ok = coroutine.resume(co)
    assert(ok == false)
    local cotrace = debug.traceback(co)
    assert(cotrace:find("[C]: in function 'math.abs'", 1, true) ~= nil)
  end
  do
    local stripped = assert(load(string.dump(assert(load([[
      local debug = require "debug"
      local a = 12
      local f = function() return a end
      local n, v = debug.getupvalue(f, 1)
      assert(n == "(no name)" and v == 12)
      assert(debug.setupvalue(f, 1, 13) == "(no name)")
      local info = debug.getinfo(f)
      assert(info.linedefined == 1 and info.lastlinedefined == 1)
      assert(debug.getinfo(1).currentline == -1)
      return a
    ]])), true)))
    assert(stripped() == 13)
    local stripped_err = assert(load(string.dump(function()
      error("stripped boom")
    end, true)))
    local ok, err = pcall(stripped_err)
    assert(ok == false and err == "stripped boom")
  end
  do
    local function stripped_linehook_probe()
      local a = 1
      local b = 2
      return b
    end
    local stripped = assert(load(string.dump(stripped_linehook_probe, true)))
    local line = true
    debug.sethook(function(event, l)
      assert(event == "line")
      line = l
    end, "l")
    -- Stripped Lua 5.4 code keeps the entry line event, but there is no debug
    -- line table left, so the hook receives nil instead of a synthetic 0.
    assert(stripped() == 2); debug.sethook()
    assert(line == nil)
  end
  do
    local function lua54_vararg_probe(a, ...)
      local n, v = debug.getlocal(1, -1)
      assert(n == "(vararg)" and v == 2)
      assert(debug.setlocal(1, -1, 9) == "(vararg)")
      assert((...) == 9)
    end
    lua54_vararg_probe(1, 2)
    do
      local n, v = debug.getlocal(0, 1)
      assert(n == "(C temporary)" and v == 0)
    end
  end
  do
    local count = 0
    local function hook()
      local info = debug.getinfo(1, "n")
      assert(info.namewhat == "hook" and info.name == "?")
      assert(debug.traceback():find("in hook", 1, true))
      count = count + 1
    end
    debug.sethook(hook, "l")
    local lua54_hook_name_probe = 0
    lua54_hook_name_probe = lua54_hook_name_probe + 1
    debug.sethook()
    assert(count >= 2)
    assert(getmetatable(debug.getregistry()._HOOKKEY).__mode == "k")
  end
  do
    local count = 0
    local function hook()
      local fn = assert(load([[lua54_hook_arith_probe = "x" + 1]]))
      local ok, err = pcall(fn)
      -- The hook runs with FRAME_PCALLH active; Lua 5.4 string-arithmetic
      -- errors still need to stay inside the protected call.
      assert(ok == false and err:match("attempt to add") ~= nil and
             err:match("^%[string") ~= nil)
      count = count + 1
    end
    debug.sethook(hook, "c")
    local function lua54_hook_pcall_arith_probe() end
    lua54_hook_pcall_arith_probe()
    debug.sethook()
    assert(count >= 1)
    lua54_hook_arith_probe = nil
  end
  do
    local source = [[if
math.sin(1)
then
  lua54_linehook_if_probe = 1
else
  lua54_linehook_if_probe = 2
end
]]
    local expected = { 2, 3, 4, 7 }
    local seen = {}
    local function hook(event, line)
      if event == "line" and line <= 7 then
        seen[#seen+1] = line
      end
    end
    debug.sethook(hook, "l")
    assert(load(source))()
    debug.sethook()
    assert(table.concat(seen, ",") == table.concat(expected, ","))
    lua54_linehook_if_probe = nil
  end
  do
    local seen = {}
    local function lua54_oneline_linehook_probe() local a = 1 end
    local defline = debug.getinfo(lua54_oneline_linehook_probe, "S").linedefined
    local function hook(event, line)
      if event == "line" then
        seen[line] = true
      end
    end
    debug.sethook(hook, "l")
    lua54_oneline_linehook_probe()
    debug.sethook()
    assert(seen[defline] == true)
  end
  do
    local source = [[a=1
repeat
  lua54_linehook_repeat_probe = (lua54_linehook_repeat_probe or 1) + 1
until lua54_linehook_repeat_probe == 3
]]
    local expected = { 1, 3, 4, 3, 4 }
    local seen = {}
    local function hook(event, line)
      if event == "line" and line <= 4 then
        seen[#seen+1] = line
      end
    end
    debug.sethook(hook, "l")
    assert(load(source))()
    debug.sethook()
    assert(table.concat(seen, ",") == table.concat(expected, ","))
    lua54_linehook_repeat_probe = nil
  end
  do
    local source = [[
local b = {10}
lua54_linehook_gap_probe = b[1]
+
b[1]
lua54_linehook_gap_probe = 4
]]
    local expected = { 1, 3, 4, 3, 4, 5 }
    local seen = {}
    local function hook(event, line)
      if event == "line" and line <= 5 then
        seen[#seen+1] = line
      end
    end
    debug.sethook(hook, "l")
    assert(load(source))()
    debug.sethook()
    assert(table.concat(seen, ",") == table.concat(expected, ","))
    lua54_linehook_gap_probe = nil
  end
  do
    local seen = {}
    local function hook(event, line)
      if event == "line" then
        seen[#seen+1] = line
      end
    end
    -- Lua 5.4 does not report the caller line again when a hooked chunk
    -- returns to the rest of the same source line that invoked it.
    debug.sethook(hook, "l"); assert(load("lua54_linehook_if_probe = 1\n"))(); debug.sethook()
    assert(table.concat(seen, ",") == "1")
    lua54_linehook_if_probe = nil
  end
  do
    local seen = {}
    local function hook(ev)
      local info = debug.getinfo(2, "nr")
      if info.name == "lua54_transfer_probe" then
        seen[#seen+1] = ev..":"..info.ftransfer..":"..info.ntransfer
      end
    end
    local function lua54_transfer_probe(a, b)
      return a + b, a - b
    end
    debug.sethook(hook, "cr")
    local x, y = lua54_transfer_probe(3, 1)
    debug.sethook()
    assert(x == 4 and y == 2)
    assert(seen[1] == "call:1:2")
    assert(seen[2] == "return:3:2")
  end
  do
    local seen = {}
    local function hook(ev)
      local info = debug.getinfo(2, "nr")
      if info.name == "lua54_transfer_vararg" then
        seen[#seen+1] = ev..":"..info.ftransfer..":"..info.ntransfer
      end
    end
    local function lua54_transfer_vararg(a, ...)
      return a, ...
    end
    debug.sethook(hook, "cr")
    local a, b, c = lua54_transfer_vararg(1, 2, 3)
    debug.sethook()
    assert(a == 1 and b == 2 and c == 3)
    assert(seen[1] == "call:1:1")
    assert(seen[2] == "return:2:3")
  end
  do
    local seen = {}
    local function hook(ev)
      local info = debug.getinfo(2, "rtS")
      if info.what == "C" then
        seen[#seen+1] = ev..":"..info.ftransfer..":"..info.ntransfer
      end
    end
    debug.sethook(hook, "c")
    local v = math.max(1, 2, 3)
    debug.sethook()
    assert(v == 3)
    assert(seen[1] == "call:1:3")
  end
  do
    local seen = {}
    local function hook(ev)
      local info = debug.getinfo(2, "nrS")
      -- LuaJIT cannot always recover the library field name for fast C frames
      -- in a large chunk, so exclude the hook-management call and assert the
      -- transfer range itself.
      if info.what == "C" and info.name ~= "sethook" then
        seen[#seen+1] = ev..":"..info.ftransfer..":"..info.ntransfer
      end
    end
    debug.sethook(hook, "cr")
    local i, f = math.modf(1.5)
    debug.sethook()
    assert(i == 1 and f == 0.5)
    assert(seen[1] == "call:1:1")
    assert(seen[2] == "return:2:2")
  end
  do
    local inp, out
    local on = false
    local function hook(ev)
      if not on then return end
      local info = debug.getinfo(2, "ruS")
      if info.what ~= "C" then return end
      local t = {}
      for i = info.ftransfer, info.ftransfer + info.ntransfer - 1 do
        local _, v = debug.getlocal(2, i)
        t[#t+1] = v
      end
      if ev == "return" then out = t else inp = t end
    end
    debug.sethook(hook, "cr")
    on = true
    local v = math.sin(3)
    on = false
    debug.sethook()
    -- Fast C functions overwrite their frame slots with results before the
    -- normal C-return path runs; keep a stable Lua 5.4 transfer window so
    -- debug hooks can inspect both arguments and return values.
    assert(inp[1] == 3)
    assert(out[1] == v)
  end
  do
    local out
    local on = false
    local function hook(ev)
      if not on or ev ~= "return" then return end
      local info = debug.getinfo(2, "ruS")
      if info.what ~= "C" then return end
      local t = {}
      for i = info.ftransfer, info.ftransfer + info.ntransfer - 1 do
        local _, v = debug.getlocal(2, i)
        t[#t+1] = v
      end
      out = t
    end
    debug.sethook(hook, "cr")
    on = true
    local a, b, c = select(2, 10, 20, 30, 40)
    on = false
    debug.sethook()
    assert(a == 20 and b == 30 and c == 40)
    assert(out[1] == 20 and out[2] == 30 and out[3] == 40)
  end
  do
    local seen = {}
    local function hook(ev)
      local info = debug.getinfo(2, "rt")
      if ev == "tail call" or (ev == "return" and info.istailcall) then
        seen[#seen+1] = ev..":"..tostring(info.istailcall)..":"..
          info.ftransfer..":"..info.ntransfer
      end
    end
    local function tail_target(a, b, expect_tail)
      assert(debug.getinfo(1, "t").istailcall == expect_tail)
      return a + b
    end
    local function tail_caller(a, b)
      return tail_target(a, b, true)
    end
    debug.sethook(hook, "cr")
    local v = tail_caller(2, 3)
    debug.sethook()
    assert(v == 5)
    assert(seen[1] == "tail call:true:1:3")
    assert(seen[2] == "return:true:4:1")
    assert(tail_target(1, 2, false) == 3)
  end
  do
    local seen = {}
    local function hook(ev)
      local info = debug.getinfo(2, "rt")
      if ev == "tail call" or (ev == "return" and info.istailcall) then
        seen[#seen+1] = ev..":"..tostring(info.istailcall)..":"..
          info.ftransfer..":"..info.ntransfer
      end
    end
    local function tail_vararg_target(...)
      assert(debug.getinfo(1, "t").istailcall == true)
      return ...
    end
    local function tail_vararg_caller(...)
      return tail_vararg_target(...)
    end
    debug.sethook(hook, "cr")
    local a, b, c = tail_vararg_caller(1, 2, 3)
    debug.sethook()
    assert(a == 1 and b == 2 and c == 3)
    assert(seen[1] == "tail call:true:0:0")
    assert(seen[2] == "return:true:1:3")
  end
  do
    local g, g1
    local function target(x)
      if x then
        local caller = debug.getinfo(2)
        assert(debug.getinfo(1, "t").istailcall == true)
        assert(caller.func == g1 and caller.istailcall == true)
      end
    end
    function g(x) return target(x) end
    function g1(x) g(x) end
    local function h(x) local f = g1; return f(x) end
    h(true)
  end
  do
    local seen = {}
    local function hook(ev)
      local info = debug.getinfo(2, "rtS")
      if ev == "tail call" or info.what == "C" then
        seen[#seen+1] = ev..":"..tostring(info.istailcall)..":"..
          info.ftransfer..":"..info.ntransfer
      end
    end
    local function tail_c_position(a, b, c)
      return math.max(a, b, c)
    end
    debug.sethook(hook, "cr")
    local v = tail_c_position(1, 2, 3)
    debug.sethook()
    assert(v == 3)
    assert(seen[1] == "return:false:0:0")
    assert(seen[2] == "call:false:1:3")
    assert(seen[3] == "return:false:4:1")
  end
  do
    local seen = {}
    local function hook(ev)
      local info = debug.getinfo(2, "nrS")
      if ev == "return" and info.what == "C" and info.name == "pcall" then
        seen[#seen+1] = info.ftransfer..":"..info.ntransfer
      end
    end
    debug.sethook(hook, "r")
    local ok, a, b = pcall(function(x, y) return x + y, "ok" end, 2, 3)
    debug.sethook()
    assert(ok == true and a == 5 and b == "ok")
    assert(seen[1] == "1:3")
  end
  do
    local seen = {}
    local function hook(ev)
      local info = debug.getinfo(2, "nrS")
      if ev == "return" and info.what == "C" and info.name == "pcall" then
        seen[#seen+1] = info.ftransfer..":"..info.ntransfer
      end
    end
    debug.sethook(hook, "r")
    local ok, err = pcall(function() error("pcall hook error", 0) end)
    debug.sethook()
    assert(ok == false and err == "pcall hook error")
    assert(seen[1] == "3:2")
  end
  do
    local seen = {}
    local function hook(ev)
      local info = debug.getinfo(2, "nrS")
      if ev == "return" and info.what == "C" and info.name == "xpcall" then
        seen[#seen+1] = info.ftransfer..":"..info.ntransfer
      end
    end
    debug.sethook(hook, "r")
    local ok, value = xpcall(function() return 7 end, debug.traceback)
    debug.sethook()
    assert(ok == true and value == 7)
    assert(seen[1] == "3:2")
    seen = {}
    debug.sethook(hook, "r")
    ok, value = xpcall(function() error("xpcall hook error", 0) end,
      function(e) return "handled:"..e end)
    debug.sethook()
    assert(ok == false and value == "handled:xpcall hook error")
    assert(seen[1] == "5:2")
  end
  do
    local function boom()
      error("tail unwind", 0)
    end
    local function tailfail()
      return boom()
    end
    local function probe(expect_tail)
      assert(debug.getinfo(1, "t").istailcall == expect_tail)
      return "ok"
    end
    local function driver()
      local ok = pcall(tailfail)
      assert(ok == false)
      -- Error unwinding must clear side markers for frames that were removed;
      -- the following ordinary call is not in tail position.
      local v = probe(false)
      return v
    end
    assert(driver() == "ok")
  end
  do
    local function hot_count_probe()
      local s = 0
      for i = 1, 100 do s = s + i end
      return s
    end
    if jit then
      jit.opt.start("hotloop=1")
      for _ = 1, 8 do hot_count_probe() end
    end
    local n = 0
    debug.sethook(function() n = n + 1 end, "", 1)
    hot_count_probe()
    debug.sethook()
    -- Count hooks are defined in VM instruction units. Existing traces must
    -- not keep running after hooks are enabled, or Lua 5.4 debug counts collapse
    -- to a handful of trace exits instead of the interpreted instruction stream.
    assert(n > 100)
    if jit then jit.opt.start("hotloop=56", "hotexit=10") end
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
  do
    local a, pos = string.unpack("b", "abc", 0)
    assert(a == 97 and pos == 2)
    local c, cpos = string.unpack("c1", "abc", 0)
    assert(c == "a" and cpos == 2)
    local ok, err = pcall(string.unpack, "b", "", 0)
    assert(not ok and err:find("data string too short", 1, true))
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
    local big = 2^40
    local neg = -big
    assert(bytes(string.pack("<I8", big)) == "0,0,0,0,0,1,0,0")
    assert(bytes(string.pack("<i8", neg)) == "0,0,0,0,0,255,255,255")
    assert(bytes(string.pack("<I8", -1)) ==
	   "255,255,255,255,255,255,255,255")
    local u, upos = string.unpack("<I8", string.pack("<I8", big))
    local i, ipos = string.unpack("<i8", string.pack("<i8", neg))
    local wrap, wpos = string.unpack("<I8", string.pack("<I8", -1))
    assert(u == big and upos == 9)
    assert(i == neg and ipos == 9)
    assert(wrap == -1 and wpos == 9)
    assert(select(1, pcall(string.pack, "<I4", -1)) == false)
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
  local j_size = string.packsize("j")
  assert(j_size == (math.maxinteger > 0x7fffffff and 8 or 4))
  assert(string.packsize("jT") == j_size + string.packsize("T"))
  do
    local a, apos = string.unpack("<j", string.pack("<j", math.maxinteger))
    local b, bpos = string.unpack("<j", string.pack("<j", math.mininteger))
    assert(a == math.maxinteger and b == math.mininteger)
    assert(apos == j_size + 1 and bpos == j_size + 1)
  end
  do
    local a, b, pos = string.unpack("<jT", string.pack("<jT", -2, 5))
    assert(a == -2 and b == 5 and pos == j_size + string.packsize("T") + 1)
  end
  assert(bytes(string.pack("<J", -1)) == ("255,"):rep(j_size - 1).."255")
  do
    local a, pos = string.unpack("<J", string.pack("<J", -1))
    assert(a == -1 and pos == j_size + 1)
  end
  if string.packsize("T") == j_size then
    assert(bytes(string.pack("<T", -1)) == ("255,"):rep(j_size - 1).."255")
    local a, pos = string.unpack("<T", string.pack("<T", -1))
    assert(a == -1 and pos == j_size + 1)
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
  local long_pair_size = string.packsize("lL")
  -- Native long follows the target C ABI: Windows x64 is LLP64 (4-byte long),
  -- while Android/iOS/Linux 64-bit are LP64 (8-byte long).
  assert(long_pair_size == 8 or long_pair_size == 16)
  do
    local a, b, pos = string.unpack("<lL", string.pack("<lL", -2, 5))
    assert(a == -2 and b == 5 and pos == long_pair_size + 1)
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
  local function expect_package_error(f, fname, msg, ...)
    local ok_pkg, err_pkg = pcall(f, ...)
    assert(ok_pkg == false and err_pkg:find("to '" .. fname .. "'", 1, true) ~= nil)
    assert(err_pkg:find(msg, 1, true) ~= nil)
  end
  expect_package_error(require, "require", "string expected, got table", {})
  expect_package_error(require, "require", "string expected, got boolean", true)
  expect_package_error(package.searchpath, "package.searchpath",
                       "string expected, got table", {}, "?.lua")
  expect_package_error(package.searchpath, "package.searchpath",
                       "string expected, got table", "x", {})
  expect_package_error(package.searchpath, "package.searchpath",
                       "string expected, got table", "x", "?.lua", {})
  expect_package_error(package.searchpath, "package.searchpath",
                       "string expected, got table", "x", "?.lua", ".", {})
  expect_package_error(package.loadlib, "package.loadlib",
                       "string expected, got nil", nil, "x")
  expect_package_error(package.loadlib, "package.loadlib",
                       "string expected, got table", {}, "x")
  expect_package_error(package.loadlib, "package.loadlib",
                       "string expected, got nil", "x", nil)
  expect_package_error(package.loadlib, "package.loadlib",
                       "string expected, got table", "x", {})
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
  package.preload.__lua54_nil_module = function()
  end
  local m, loaderdata = require("__lua54_nil_module")
  assert(m == true and loaderdata == ":preload:")
  assert(package.loaded.__lua54_nil_module == true)
  local again, again_data = require("__lua54_nil_module")
  assert(again == true and again_data == nil)
  package.loaded.__lua54_nil_module = nil
  package.preload.__lua54_nil_module = nil
end

do
  local calls = 0
  package.preload.__lua54_error_module = function()
    calls = calls + 1
    error("lua54 require boom", 0)
  end
  local ok1, err1 = pcall(require, "__lua54_error_module")
  local ok2, err2 = pcall(require, "__lua54_error_module")
  assert(ok1 == false and err1 == "lua54 require boom")
  assert(ok2 == false and err2 == "lua54 require boom")
  assert(calls == 2)
  assert(package.loaded.__lua54_error_module == nil)
  package.preload.__lua54_error_module = nil
end

do
  local found, searcherr = package.searchpath("__lua54_missing__", "nope/?.lua;none/?.lua")
  assert(found == nil)
  assert(not searcherr:match("^\n\t"))
  assert(searcherr:match("\n\tno file") ~= nil)
  found, searcherr = package.searchpath(1099511627776, "?.lua")
  assert(found == nil and searcherr:find("1099511627776.lua", 1, true) ~= nil)
  found, searcherr = package.searchpath("__lua54_missing__", 1099511627776)
  assert(found == nil and searcherr:find("1099511627776", 1, true) ~= nil)
  local empty_found, empty_err = package.searchpath("__lua54_missing__", "?.lua;;")
  assert(empty_found == nil and empty_err:match("no file ''", 1, true) ~= nil)
  local no_path_found, no_path_err = package.searchpath("__lua54_missing__", "")
  assert(no_path_found == nil and no_path_err == "no file ''")
  local preloaderr = package.searchers[1]("__lua54_missing_preload__")
  assert(type(preloaderr) == "string" and not preloaderr:match("^\n\t"))
  local old_searchers = package.searchers
  package.searchers = {
    function() return "custom missing" end
  }
  local ok, err = pcall(require, "__lua54_missing_custom__")
  assert(ok == false and err:match("module '__lua54_missing_custom__' not found:\n\tcustom missing"))
  package.searchers = old_searchers
  do
    local missing = assert(load([[
      return require("__lua54_missing_loc__")
    ]]))
    ok, err = pcall(missing)
    assert(ok == false and err:find(": module '__lua54_missing_loc__' not found:", 1, true))
    local bad_searchers = assert(load([[
      package.searchers = 1
      return require("__lua54_bad_searchers__")
    ]]))
    old_searchers = package.searchers
    ok, err = pcall(bad_searchers)
    package.searchers = old_searchers
    assert(ok == false and err:find(": 'package.searchers' must be a table", 1, true))
  end
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
  local function result_count(...)
    return select("#", ...), ...
  end

  local co = coroutine.create(function()
    coroutine.yield("paused")
    return "done"
  end)
  local ok, value = coroutine.resume(co)
  assert(ok == true and value == "paused")
  assert(coroutine.status(co) == "suspended")
  local close_n, closed = result_count(coroutine.close(co))
  assert(close_n == 1 and closed == true)
  assert(coroutine.status(co) == "dead")
  assert(select(1, coroutine.resume(co)) == false)
  do
    local ok_close, err_close = pcall(coroutine.close, coroutine.running())
    assert(ok_close == false and err_close == "cannot close a running coroutine")
    ok_close, err_close = pcall(function()
      return coroutine.close(coroutine.running())
    end)
    assert(ok_close == false and err_close ~= "cannot close a running coroutine" and
	   err_close:find(": cannot close a running coroutine", 1, true))
  end
  do
    local main = coroutine.running()
    local checker = coroutine.create(function()
      local ok_close, err_close = pcall(coroutine.close, main)
      assert(ok_close == false and err_close == "cannot close a normal coroutine")
      ok_close, err_close = pcall(function()
	return coroutine.close(main)
      end)
      assert(ok_close == false and err_close ~= "cannot close a normal coroutine" and
	     err_close:find(": cannot close a normal coroutine", 1, true))
    end)
    assert(coroutine.resume(checker) == true)
  end
  do
    local done = coroutine.create(function() return "done" end)
    assert(select(1, coroutine.resume(done)) == true)
    assert(coroutine.status(done) == "dead")
    local done_n, done_closed = result_count(coroutine.close(done))
    assert(done_n == 1 and done_closed == true)
  end
  do
    local fresh = coroutine.create(function() return "fresh" end)
    local fresh_n, fresh_closed = result_count(coroutine.close(fresh))
    assert(fresh_n == 1 and fresh_closed == true)
    assert(coroutine.status(fresh) == "dead")
  end
  do
    local bad = coroutine.create(function() error("lua54 close error") end)
    assert(select(1, coroutine.resume(bad)) == false)
    local closed, err = coroutine.close(bad)
    assert(closed == false)
    assert(type(err) == "string" and err:match("lua54 close error"))
    bad = coroutine.create(error)
    local ok_resume, resume_err = coroutine.resume(bad, 100)
    assert(ok_resume == false and resume_err == 100)
    closed, err = coroutine.close(bad)
    assert(closed == false and err == 100)
    local reclose_n, reclosed = result_count(coroutine.close(bad))
    assert(reclose_n == 1 and reclosed == true)
  end
  do
    assert(assert(load([[
      local function result_count(...)
        return select("#", ...), ...
      end
      local log = {}
      local mt = {
        __close = function(self, err)
          log[#log + 1] = self.name..":"..tostring(err)
        end,
      }
      local co = coroutine.create(function()
        local x <close> = setmetatable({ name = "x" }, mt)
        coroutine.yield("paused")
      end)
      local ok, value = coroutine.resume(co)
      assert(ok == true and value == "paused")
      local close_n, closed = result_count(coroutine.close(co))
      assert(close_n == 1 and closed == true)
      assert(table.concat(log, ",") == "x:nil")
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local function result_count(...)
        return select("#", ...), ...
      end
      local log = {}
      local mt = {
        __close = function(self, err)
          log[#log + 1] = self.name..":"..tostring(err)
          if self.name == "b" then error("close coroutine boom", 0) end
        end,
      }
      local co = coroutine.create(function()
        local a <close> = setmetatable({ name = "a" }, mt)
        local b <close> = setmetatable({ name = "b" }, mt)
        coroutine.yield("paused")
      end)
      assert(select(1, coroutine.resume(co)) == true)
      local closed, err = coroutine.close(co)
      assert(closed == false and err == "close coroutine boom")
      assert(coroutine.status(co) == "dead")
      assert(table.concat(log, ",") == "b:nil,a:close coroutine boom")
      local reclose_n, reclosed = result_count(coroutine.close(co))
      assert(reclose_n == 1 and reclosed == true)
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local function result_count(...)
        return select("#", ...), ...
      end
      local co
      co = coroutine.create(function()
        local x <close> = setmetatable({}, {
          __close = function()
            local ok, err = pcall(coroutine.close, co)
            assert(ok == false and err:match("running coroutine"))
          end,
        })
        coroutine.yield("paused")
      end)
      assert(select(1, coroutine.resume(co)) == true)
      local close_n, closed = result_count(coroutine.close(co))
      assert(close_n == 1 and closed == true)
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local track = {}
      local function closable(fn)
        return setmetatable({}, { __close = fn })
      end
      local function h(o)
        local hv <close> = o
        return 1
      end
      local function body()
        local x <close> = closable(function(_, msg)
          track[#track + 1] = msg or false
          error(20)
        end)
        local y <close> = closable(function(_, msg)
          track[#track + 1] = msg or false
        end)
        local z <close> = closable(function(_, msg)
          track[#track + 1] = msg or false
          error(10)
        end)
        coroutine.yield(1)
        h(closable(function(_, msg)
          track[#track + 1] = msg or false
          error(2)
        end))
      end
      -- This is the official Lua 5.4 coroutine/pcall close-recovery shape:
      -- the first close error is protected by pcall, then outer __close
      -- methods continue with the replacement error until the last one wins.
      local co = coroutine.create(pcall)
      local ok, value = coroutine.resume(co, body)
      assert(ok == true and value == 1)
      local st, protected_ok, err = coroutine.resume(co)
      assert(st == true and protected_ok == false and err == 20)
      assert(coroutine.status(co) == "dead")
      assert(track[1] == false and track[2] == 2 and
             track[3] == 10 and track[4] == 10)
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local function closable(fn)
        return setmetatable({}, { __close = fn })
      end
      local closed = false
      do
        local a <close> = closable(function(_, msg)
          assert(msg == nil)
          closed = true
        end)
        local ok, err = pcall(function() error(77) end)
        assert(ok == false and err == 77 and closed == false)
      end
      assert(closed == true)
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local function closable(fn)
        return setmetatable({}, { __close = fn })
      end
      local log = {}
      local co = coroutine.create(function()
        local function foo(err)
          local z <close> = closable(function(_, msg)
            log[#log + 1] = "z:"..tostring(msg)
            coroutine.yield("z")
          end)
          local y <close> = closable(function(_, msg)
            log[#log + 1] = "y:"..tostring(msg)
            coroutine.yield("y")
            if err then error(err + 20) end
          end)
          local x <close> = closable(function(_, msg)
            log[#log + 1] = "x:"..tostring(msg)
            coroutine.yield("x")
          end)
          error(err)
        end
        return pcall(foo, 10)
      end)
      local ok, value = coroutine.resume(co)
      assert(ok == true and value == "x")
      ok, value = coroutine.resume(co)
      assert(ok == true and value == "y")
      ok, value = coroutine.resume(co)
      assert(ok == true and value == "z")
      local protected_ok, err
      ok, protected_ok, err = coroutine.resume(co)
      assert(ok == true and protected_ok == false and err == 30)
      assert(table.concat(log, ",") == "x:10,y:10,z:30")
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local function closable(fn)
        return setmetatable({}, { __close = fn })
      end
      local x, y = false, false
      local co = coroutine.wrap(function()
        local xv <close> = closable(function(_, msg)
          assert(msg == 23)
          x = true
        end)
        do
          local yv <close> = closable(function(_, msg)
            assert(msg == nil)
            y = true
          end)
          coroutine.yield(100)
        end
        coroutine.yield(200)
        error(23)
      end)
      assert(co() == 100 and x == false and y == false)
      assert(co() == 200 and x == false and y == true)
      local ok, err = pcall(co)
      assert(ok == false and err == 23 and x == true and y == true)
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local function closable(fn)
        return setmetatable({}, { __close = fn })
      end
      local n = 0
      local co = coroutine.wrap(function()
        local xx <close> = closable(function(_, msg)
          n = n + 1
          assert(string.find(msg, "@XXX"))
          error("@YYY")
        end)
        local xv <close> = closable(function()
          n = n + 1
          error("@XXX")
        end)
        coroutine.yield(100)
        error(200)
      end)
      assert(co() == 100 and n == 0)
      local ok, msg = pcall(co)
      assert(ok == false and n == 2 and string.find(msg, "@YYY"))
      -- Full GC used to traverse stale coroutine stack slots after close-error
      -- replacement forced stack growth during lua_closethread().
      collectgarbage()
      return true
    ]]))())
  end
  do
    assert(assert(load([[
      local function closable(fn)
        return setmetatable({}, { __close = fn })
      end
      local co
      co = coroutine.wrap(function()
        -- Official Lua 5.4.1 regression: a wrap reset must not leave the
        -- wrapped thread looking resumable while close metamethods re-enter it.
        local x <close> = closable(function()
          local ok = pcall(co)
          assert(ok == false)
        end)
        error(111)
      end)
      local ok, err = pcall(co)
      assert(ok == false and err == 111)
      ok, err = pcall(co)
      assert(ok == false and string.find(tostring(err), "dead coroutine"))
      return true
    ]]))())
  end
  do
    local function recurse(fn)
      coroutine.wrap(fn)(fn)
    end
    local ok, err = pcall(recurse, recurse)
    assert(ok == false and tostring(err):match("C stack overflow", 1, true) ~= nil)
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
  a, b = math.randomseed(1099511627776, 1)
  assert(a == 1099511627776 and b == 1 and math.type(a) == "integer")
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
  local function expect_random_error(f, fname, msg, ...)
    local ok_random, err_random = pcall(f, ...)
    assert(ok_random == false and err_random:find("to '" .. fname .. "'", 1, true) ~= nil)
    assert(err_random:find(msg, 1, true) ~= nil)
  end
  expect_random_error(math.random, "math.random", "interval is empty", -1)
  do
    local ok, err = pcall(math.random, 10, 5)
    assert(ok == false and err:find("to 'math.random'", 1, true) ~= nil and
           err:find("interval is empty", 1, true) ~= nil)
  end
  expect_random_error(math.random, "math.random",
                      "number has no integer representation", 1.5)
  expect_random_error(math.random, "math.random", "number expected, got boolean", true)
  expect_random_error(math.randomseed, "math.randomseed",
                      "number has no integer representation", 1.5)
  expect_random_error(math.randomseed, "math.randomseed",
                      "number expected, got boolean", true)
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

  do
    local name
    setmetatable({}, { __gc = function()
      local info = debug.getinfo(1, "n")
      assert(info.namewhat == "metamethod")
      name = info.name
    end })
    -- Lua 5.4 table finalizers are observable from allocation-driven GC; this
    -- keeps official repeat-until-finalized debug tests from spinning forever.
    for _ = 1, 10000 do
      local t = {}
      if name then break end
    end
    assert(name == "__gc")
  end

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
  assert(os.date("") == "")
  assert(os.date("!") == "")
  assert(os.date(1099511627776, 0) == "1099511627776")
  assert(os.date("\0\0") == "\0\0")
  assert(os.date("!\0\0") == "\0\0")
  do
    local t = os.date("*t\0ignored", 0)
    assert(type(t) == "table" and type(t.year) == "number")
    t = os.date("!*t\0ignored", 0)
    assert(type(t) == "table" and t.year == 1970 and
	   t.month == 1 and t.day == 1)
    assert(os.date("*tx", 0) == "*tx")
  end
  if package.config:sub(1, 1) == "\\" then
    assert(os.date("%c", 0) == os.date("%x %X", 0))
    assert(os.date("!%c", 0) == os.date("!%x %X", 0))
    assert(os.date("%#c", 0):find("1970", 1, true) ~= nil)
    assert(os.date("%#x", 0):find("1970", 1, true) ~= nil)
    assert(os.date("%#d", 0) == "1")
    local ok, err = pcall(os.date, "%#X", 0)
    assert(ok == false and
           err:find("to 'os.date' (invalid conversion specifier '%#X')",
                    1, true) ~= nil)
  end
  do
    local ok, err = pcall(os.date, "%Q", 0)
    assert(ok == false and
           err:find("to 'os.date' (invalid conversion specifier '%Q')",
                    1, true) ~= nil)
  end
  local ok, err = pcall(os.date, true)
  assert(ok == false and err:match("to 'os%.date'") ~= nil)
  ok, err = pcall(os.date, "%c", true)
  assert(ok == false and err:match("to 'os%.date'") ~= nil)
  ok, err = pcall(os.date, "%Y", 1.5)
  assert(ok == false and err:match("integer representation") ~= nil)
  ok, err = pcall(os.date, "%Y", "1.5")
  assert(ok == false and err:match("integer representation") ~= nil)
  ok, err = pcall(os.date, "%Y", 2^60)
  assert(ok == false and err:match("date result cannot be represented") ~= nil)
  ok, err = pcall(os.difftime, 1, true)
  assert(ok == false and err:match("to 'os%.difftime'") ~= nil)
  ok, err = pcall(os.difftime, 2.5, 1)
  assert(ok == false and err:match("integer representation") ~= nil)
  ok, err = pcall(os.difftime, 2, 1.5)
  assert(ok == false and err:match("integer representation") ~= nil)
  ok, err = pcall(os.difftime)
  assert(ok == false and
	 err:find("bad argument #1 to 'os.difftime' (number expected, got no value)", 1, true))
  ok, err = pcall(function() return os.difftime(nil) end)
  assert(ok == false and
	 err:find("bad argument #1 to 'difftime' (number expected, got nil)", 1, true))
  ok, err = pcall(os.difftime, 1)
  assert(ok == false and
	 err:find("bad argument #2 to 'os.difftime' (number expected, got no value)", 1, true))
  assert(os.difftime("3", "1") == 2)
  ok, err = pcall(os.execute, true)
  assert(ok == false and err:match("to 'os%.execute'") ~= nil)
  do
    local shell_exists = os.execute()
    if shell_exists then
      local is_windows = package.config:sub(1, 1) == "\\"
      local badcmd = is_windows and
        "__unlikely_lua54_command__ 2>NUL" or
        "__unlikely_lua54_command__ 2>/dev/null"
      local expected_status = is_windows and 1 or 127
      os.remove("__unlikely_lua54_missing_file__")
      -- os.execute() must clear stale errno before system(); otherwise a prior
      -- file failure is misreported as a system error instead of an exit tuple.
      local exec_ok, exec_why, exec_code = os.execute(badcmd)
      assert(exec_ok == nil and exec_why == "exit" and
             exec_code == expected_status)
    end
  end
  ok, err = pcall(os.exit, "x")
  assert(ok == false and err:match("to 'os%.exit'") ~= nil)
  ok, err = pcall(os.exit, 1.5)
  assert(ok == false and err:match("integer representation") ~= nil)
  ok, err = pcall(os.exit, {})
  assert(ok == false and err:match("to 'os%.exit'") ~= nil)
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
  ok, err = pcall(os.time, { year = 1000, month = 1, day = 1, hour = "x" })
  assert(ok == false and err:match("field 'hour' is not an integer") ~= nil)
  ok, err = pcall(os.time, { year = 1000, month = 1, day = 1, hour = 1.5 })
  assert(ok == false and err:match("field 'hour' is not an integer") ~= nil)
  ok, err = pcall(os.time, { hour = 12 })
  assert(ok == false and err:match("field 'year' missing") ~= nil)
  ok, err = pcall(os.time, { year = 2020 })
  assert(ok == false and err:match("field 'month' missing") ~= nil)
  ok, err = pcall(os.time, { year = 2020, month = 1 })
  assert(ok == false and err:match("field 'day' missing") ~= nil)
  ok, err = pcall(os.time, {
    year = 2020, month = 1, day = 1, hour = true, min = true, sec = true
  })
  assert(ok == false and err:match("field 'hour' is not an integer") ~= nil)
  ok, err = pcall(os.time, {
    year = 2020, month = 1, day = 1, min = true, sec = true
  })
  assert(ok == false and err:match("field 'min' is not an integer") ~= nil)
  ok, err = pcall(os.time, { year = 2020, month = 1, day = 1, sec = true })
  assert(ok == false and err:match("field 'sec' is not an integer") ~= nil)
  ok, err = pcall(os.time, { year = 4000, month = 1, day = 1 })
  assert(ok == false and err:match("time result cannot be represented") ~= nil)
  local stamp = os.time({ year = 2020, month = 5, day = 7,
			  hour = 12, min = 34, sec = 56 })
  assert(math.type(stamp) == "integer")
  do
    local future = { year = 2039, month = 1, day = 2,
		     hour = 12, min = 34, sec = 56, isdst = false }
    local future_ok, future_stamp = pcall(os.time, future)
    if future_ok then
      assert(math.type(future_stamp) == "integer")
      assert(future_stamp > 2147483647)
      assert(os.difftime(future_stamp + 7, future_stamp) == 7)
      assert(os.difftime(tostring(future_stamp + 7),
			 tostring(future_stamp)) == 7)
      assert(os.date("%Y-%m-%d", future_stamp) == "2039-01-02")
    end
  end
  local normalized = { year = 2005, month = 1, day = 1, hour = 1, min = 0, sec = -3602 }
  os.time(normalized)
  assert(normalized.day == 31 and normalized.month == 12 and
	 normalized.year == 2004 and normalized.hour == 23 and
	 normalized.min == 59 and normalized.sec == 58 and
	 normalized.yday == 366)
end

do
  local missing = "__lua54_rename_missing__"
  local ok, msg, code = os.rename(missing, "__lua54_rename_target__")
  assert(ok == nil and type(msg) == "string" and type(code) == "number")
  assert(msg:match(missing) == nil)
end

do
  local missing = "__lua54_remove_missing__"
  os.remove(missing)
  local ok, msg, code = os.remove(missing)
  assert(ok == nil and type(msg) == "string" and type(code) == "number")
  assert(msg:match(missing) ~= nil)
end

do
  local tmp = os.tmpname()
  assert(type(tmp) == "string" and tmp ~= "")
  local f = assert(io.open(tmp, "w"))
  f:write("tmp")
  assert(f:close())
  assert(os.remove(tmp))
end

do
  warn("@off")
  warn("ignored warning")
  warn("@on")
  warn("@off")
  local ok, err = pcall(warn, 1)
  assert(ok == true)
  ok, err = pcall(warn)
  assert(ok == false and
         err:find("bad argument #1 to 'warn' (string expected, got no value)",
                  1, true) ~= nil)
  ok, err = pcall(warn, nil)
  assert(ok == false and
         err:find("bad argument #1 to 'warn' (string expected, got nil)",
                  1, true) ~= nil)
  ok, err = pcall(warn, true)
  assert(ok == false and
         err:find("bad argument #1 to 'warn' (string expected, got boolean)",
                  1, true) ~= nil)
  ok, err = pcall(warn, setmetatable({}, {
    __tostring = function() return "not for warn" end,
  }))
  assert(ok == false and
         err:find("bad argument #1 to 'warn' (string expected, got table)",
                  1, true) ~= nil)
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
  local ok, err = pcall(iter)
  assert(ok == false and tostring(err):find("file is already closed", 1, true))
  os.remove(fname)
end

do
  local fname = "lua54_lines_option_close.tmp"
  local f = assert(io.open(fname, "w"))
  f:write("line\n")
  f:close()
  local iter, _, _, closing = io.lines(fname, "L")
  assert(iter() == "line\n")
  assert(iter() == nil)
  assert(io.type(closing) == "closed file")
  assert(os.remove(fname))
end

do
  local fname = "lua54_lines_debug_close.tmp"
  local f = assert(io.open(fname, "w"))
  f:write("line\n")
  f:close()
  local function gettoclose(lv)
    lv = lv + 1
    local statevars = 0
    for i = 1, 1000 do
      local n, v = debug.getlocal(lv, i)
      if n == "(for state)" then
        statevars = statevars + 1
        if statevars == 4 then return v end
      end
    end
  end
  local closing
  for _ in io.lines(fname) do
    closing = gettoclose(1)
    assert(io.type(closing) == "file")
    break
  end
  assert(io.type(closing) == "closed file")
  os.remove(fname)
end

do
  local function expect_file_method_self(fn, name, got)
    local ok, err = pcall(fn)
    err = tostring(err)
    assert(ok == false and err:find("test/smoke.lua:", 1, true) and
	   err:find("to '" .. name .. "'", 1, true) and
	   err:find("FILE* expected, got " .. got, 1, true))
  end
  expect_file_method_self(function() return io.stdin.close() end,
			  "close", "no value")
  expect_file_method_self(function() return io.stdin.read() end,
			  "read", "no value")
  expect_file_method_self(function() return io.stdin.write("x") end,
			  "write", "string")
  expect_file_method_self(function() return io.stdin.flush() end,
			  "flush", "no value")
  expect_file_method_self(function() return io.stdin.seek() end,
			  "seek", "no value")
  expect_file_method_self(function() return io.stdin.setvbuf("no") end,
			  "setvbuf", "string")
  expect_file_method_self(function() return io.stdin.lines() end,
			  "lines", "no value")
end

assert(assert(load([[
  local ok, err = pcall(io.stdin.close)
  assert(ok == false and tostring(err):find("got no value", 1, true))
  assert(tostring(err):find("to '?'", 1, true))
  ok, err = pcall(io.stdin.seek)
  assert(ok == false and tostring(err):find("bad argument #1 to '?'",
					    1, true) and
	 tostring(err):find("FILE* expected, got no value", 1, true))
  ok, err = pcall(io.type)
  assert(ok == false and tostring(err):find("to 'io.type'", 1, true) and
	 tostring(err):find("value expected", 1, true))
  ok, err = pcall(io.input, true)
  assert(ok == false and tostring(err):find("to 'io.input'", 1, true) and
	 tostring(err):find("FILE* expected", 1, true))
  ok, err = pcall(io.output, true)
  assert(ok == false and tostring(err):find("to 'io.output'", 1, true) and
	 tostring(err):find("FILE* expected", 1, true))
  ok, err = pcall(io.close, true)
  assert(ok == false and tostring(err):find("to 'io.close'", 1, true) and
	 tostring(err):find("FILE* expected", 1, true))
  ok, err = pcall(io.lines, true)
  assert(ok == false and tostring(err):find("to 'io.lines'", 1, true) and
	 tostring(err):find("string expected", 1, true))
  do
    local missing_io_path = "__lua54_missing_dir__/__no_such_file__"
    for _, f in ipairs({ io.input, io.output, io.lines }) do
      ok, err = pcall(f, missing_io_path)
      assert(ok == false and
	     tostring(err):find("cannot open file '" .. missing_io_path .. "'",
				1, true), tostring(err))
    end
  end
  ok, err = pcall(io.open, true, "r")
  assert(ok == false and tostring(err):find("to 'io.open'", 1, true) and
	 tostring(err):find("string expected", 1, true))
  ok, err = pcall(io.open, "lua54_invalid_open_mode.tmp", true)
  assert(ok == false and tostring(err):find("to 'io.open'", 1, true) and
	 tostring(err):find("string expected", 1, true))
  ok, err = pcall(io.popen, true, "r")
  assert(ok == false and tostring(err):find("to 'io.popen'", 1, true) and
	 tostring(err):find("string expected", 1, true))
  ok, err = pcall(io.popen, "lua54", true)
  assert(ok == false and tostring(err):find("to 'io.popen'", 1, true) and
	 tostring(err):find("string expected", 1, true))
  do
    local nf, nerr = io.open(1099511627776, "r")
    assert(nf == nil and tostring(nerr):find("1099511627776", 1, true))
  end
  ok, err = pcall(io.read, {})
  assert(ok == false and tostring(err):find("bad argument #1 to 'io.read'",
					    1, true) and
	 tostring(err):find("string expected, got table", 1, true))
  ok, err = pcall(io.write, true)
  assert(ok == false and tostring(err):find("bad argument #1 to 'io.write'",
					    1, true) and
	 tostring(err):find("string expected, got boolean", 1, true))
  for _, m in ipairs({ "rw", "rb+", "r+bk", "", "+", "b" }) do
    ok, err = pcall(io.open, "lua54_invalid_open_mode.tmp", m)
    assert(ok == false and tostring(err):find("to 'io.open'", 1, true) and
	   tostring(err):find("invalid mode", 1, true))
  end
  local fname = "lua54_valid_open_mode.tmp"
  local f = assert(io.open(fname, "w"))
  f:write("x")
  f:close()
  assert(io.open(fname, "r+b")):close()
  assert(io.open(fname, "r+")):close()
  assert(io.open(fname, "rb")):close()
  local F
  do
    local f <close> = assert(io.open(fname, "r"))
    F = f
    local mt = getmetatable(f)
    assert(type(mt.__close) == "function")
    assert(f:close())
  end
  assert(io.type(F) == "closed file")
  do
    local f <close> = assert(io.open(fname, "w"))
    f:write(string.format("0x%X\n", -math.maxinteger))
  end
  do
    local f <close> = assert(io.open(fname, "r"))
    assert(f:read("n") == -math.maxinteger)
  end
  do
    local f <close> = assert(io.open(fname, "w+"))
    f:write("abcdef")
    f:seek("set", 0)
    local ok, err = pcall(function() return f:read(1.5) end)
    assert(ok == false and tostring(err):find("integer representation", 1, true))
    ok, err = pcall(function() return f:read(1099511627776) end)
    assert(ok == false and tostring(err):find("not enough memory", 1, true))
    assert(f:seek() == 0)
    ok, err = pcall(function() return f:read(-1099511627776) end)
    assert(ok == false and tostring(err):find("not enough memory", 1, true))
    assert(f:seek() == 0)
    ok, err = pcall(function() return f:read("2") end)
    assert(ok == false and tostring(err):find("bad argument #1 to 'read'", 1, true) and
	   tostring(err):find("invalid format", 1, true))
    ok, err = pcall(function() return f:read({}) end)
    assert(ok == false and tostring(err):find("bad argument #1 to 'read'", 1, true) and
	   tostring(err):find("string expected, got table", 1, true))
  end
  do
    local f <close> = assert(io.open(fname, "w+"))
    f:write("abcdef")
    assert(f:seek("set", "2") == 2)
    local ok, err = pcall(function() return f:seek("set", 2147483648) end)
    assert(ok == false and tostring(err):find("not an integer in proper range", 1, true))
    ok, err = pcall(function() return f:seek("set", "2147483648") end)
    assert(ok == false and tostring(err):find("not an integer in proper range", 1, true))
    ok, err = pcall(function() return f:seek("set", 1099511627776) end)
    assert(ok == false and tostring(err):find("not an integer in proper range", 1, true))
    -- Lua 5.4 requires seek offsets to be exact integers.  The compat path
    -- must not silently truncate fraction numbers or numeric strings.
    ok, err = pcall(function() return f:seek("set", 1.5) end)
    assert(ok == false and tostring(err):find("integer representation", 1, true))
    ok, err = pcall(function() return f:seek("set", "1.5") end)
    assert(ok == false and tostring(err):find("integer representation", 1, true))
    ok, err = pcall(function() return f:seek("set", "x") end)
    assert(ok == false and tostring(err):find("bad argument #2 to 'seek'", 1, true) and
	   tostring(err):find("number expected, got string", 1, true))
  end
  do
    local f <close> = assert(io.open(fname, "w"))
    assert(f:setvbuf("no"))
    assert(f:setvbuf("full", "4096"))
    local ok, err = pcall(function() return f:setvbuf("full", 1.5) end)
    assert(ok == false and tostring(err):find("integer representation", 1, true))
    ok, err = pcall(function() return f:setvbuf("full", "1.5") end)
    assert(ok == false and tostring(err):find("integer representation", 1, true))
    ok, err = pcall(function() return f:setvbuf("full", "x") end)
    assert(ok == false and tostring(err):find("bad argument #2 to 'setvbuf'", 1, true) and
	   tostring(err):find("number expected, got string", 1, true))
  end
  do
    local f <close> = assert(io.open(fname, "w"))
    assert(f:write(1.0, ",", 3.5))
    local ok, err = pcall(function() return f:write({}) end)
    assert(ok == false and tostring(err):find("bad argument #1 to 'write'", 1, true) and
	   tostring(err):find("string expected, got table", 1, true))
    ok, err = pcall(io.write, {})
    assert(ok == false and tostring(err):find("bad argument #1 to 'io.write'", 1, true) and
	   tostring(err):find("string expected, got table", 1, true))
  end
  do
    local f <close> = assert(io.open(fname, "w"))
    f:write("local x, z = coroutine.yield(10)\n")
    f:write("local y = coroutine.yield(20)\n")
    f:write("return x + y * z\n")
  end
  do
    local co = coroutine.wrap(dofile)
    assert(co(fname) == 10)
    assert(co(100, 101) == 20)
    assert(co(200) == 100 + 200 * 101)
  end
  do
    local f <close> = assert(io.open(fname, "w"))
    f:write(string.rep("a", 300), "\n")
  end
  do
    local opts = {}
    for i = 1, 250 do opts[i] = 1 end
    local iter, _, _, closing = io.lines(fname, table.unpack(opts))
    local values = { iter() }
    assert(#values == 250 and values[1] == "a" and values[#values] == "a")
    closing:close()
    opts[#opts + 1] = 1
    local ok, err = pcall(io.lines, fname, table.unpack(opts))
    assert(ok == false and tostring(err):find("too many arguments", 1, true))
    local it, _, _, bad_closing = io.lines(fname, {})
    ok, err = pcall(function() return it() end)
    assert(ok == false and tostring(err):find("bad argument #2 to 'it'",
					    1, true) and
	   tostring(err):find("string expected, got table", 1, true))
    bad_closing:close()
    do
      local lines = io.lines
      local it_alias, _, _, closing_alias = lines(fname, {})
      ok, err = pcall(function() return it_alias() end)
      assert(ok == false and
	     tostring(err):find("bad argument #2 to 'it_alias'",
				1, true) and
	     tostring(err):find("string expected, got table", 1, true))
      closing_alias:close()
    end
    do
      local iter2, _, _, closing2 = io.lines(fname, {})
      local it2 = iter2
      ok, err = pcall(function() return it2() end)
      assert(ok == false and tostring(err):find("bad argument #2 to 'it2'",
					      1, true) and
	     tostring(err):find("string expected, got table", 1, true))
      closing2:close()
    end
    do
      local iter3, _, _, closing3 = io.lines(fname, {})
      local function make()
	local it3 = iter3
	return function() return it3() end
      end
      ok, err = pcall(make())
      assert(ok == false and tostring(err):find("bad argument #2 to 'it3'",
					      1, true) and
	     tostring(err):find("string expected, got table", 1, true))
      closing3:close()
    end
    do
      local iter4, _, _, closing4 = io.lines(fname, {})
      lua54_lines_global_it = iter4
      ok, err = pcall(function() return lua54_lines_global_it() end)
      assert(ok == false and
	     tostring(err):find("bad argument #2 to 'lua54_lines_global_it'",
				1, true) and
	     tostring(err):find("string expected, got table", 1, true))
      lua54_lines_global_it = nil
      closing4:close()
    end
    do
      local iter5, _, _, closing5 = io.lines(fname, {})
      lua54_lines_global_it = iter5
      local function call_global_iter()
	return lua54_lines_global_it()
      end
      ok, err = pcall(call_global_iter)
      assert(ok == false and
	     tostring(err):find("bad argument #2 to 'lua54_lines_global_it'",
				1, true) and
	     tostring(err):find("string expected, got table", 1, true))
      lua54_lines_global_it = nil
      closing5:close()
    end
    do
      local mf = assert(io.open(fname, "r"))
      local it = mf:lines({})
      ok, err = pcall(function() return it() end)
      assert(ok == false and tostring(err):find("bad argument #2 to 'it'",
					      1, true) and
	     tostring(err):find("string expected, got table", 1, true))
      mf:close()
    end
  end
  do
    io.input(fname)
    io.close(io.input())
    local ok, err = pcall(io.read)
    assert(ok == false and tostring(err):find("default input file is closed", 1, true))
    io.input(io.stdin)
    io.output(fname)
    io.close()
    ok, err = pcall(io.write, "x")
    assert(ok == false and tostring(err):find("default output file is closed", 1, true))
    io.output(io.stdout)
  end
  if not os.remove(fname) then
    collectgarbage()
    collectgarbage()
    assert(os.remove(fname))
  end
  return true
]]))())

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
  assert(n == "(no name)" and v == dump_env)
  assert(debug.getupvalue(loaded, 2) == nil)
  loaded = assert(load(with_upvalue, "=dumped-upvalue-number", "b", 5))
  -- Lua 5.4 initializes the first real upvalue to the exact env argument;
  -- unlike function environments, that slot is allowed to hold non-tables.
  assert(loaded() == 5)
  n, v = debug.getupvalue(loaded, 1)
  assert(n == "(no name)" and v == 5)
  loaded = assert(load(with_upvalue, "=dumped-upvalue-false", "b", false))
  assert(loaded() == false)
  loaded = assert(load(with_upvalue, "=dumped-upvalue-nil", "b", nil))
  assert(loaded() == nil)
  loaded = assert(load(string.dump(function() return 54 end, true), "=dumped-plain", "b"))
  assert(debug.getupvalue(loaded, 1) == nil)
  loaded = assert(load(string.dump(function() return math.type(1) end, true), "=dumped-global", "b"))
  n, v = debug.getupvalue(loaded, 1)
  assert(n == "(no name)" and v == _G)
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
  do
    local function once(v)
      local done = false
      return function()
	if done then return nil end
	done = true
	return v
      end
    end
    local bad_reader
    f, err = load(once(true))
    assert(f == nil and err:find(": reader function must return a string", 1, true))
    f, err = load(once({}))
    assert(f == nil and err:find(": reader function must return a string", 1, true))
    bad_reader = once(true)
    f, err = load(function()
      return bad_reader() or "return "
    end)
    assert(f == nil and err:find(": reader function must return a string", 1, true))
    do
      local nread = 0
      f, err = load(function()
	nread = nread + 1
	if nread == 1 then return "return " end
	return true
      end)
      assert(f == nil and
	     err:find(": reader function must return a string", 1, true) and
	     not err:find("concatenate", 1, true))
    end
    local ok, wrapped_f, wrapped_err = pcall(function()
      return load(once(true))
    end)
    assert(ok == true and wrapped_f == nil and
           wrapped_err:find("test/smoke.lua:", 1, true) ~= nil and
           wrapped_err:find("reader function must return a string", 1, true) ~= nil)
  end
end

do
  local ok, err = pcall(function() return dofile(true) end)
  assert(ok == false and err:find("test/smoke.lua:", 1, true) ~= nil and
         err:find("bad argument #1 to 'dofile' (string expected, got boolean)",
                  1, true) ~= nil)
end

do
  local fname = "lua54_missing_loadfile.tmp"
  os.remove(fname)
  local f, err, code = loadfile(fname)
  assert(f == nil and code == nil and
         err:find("cannot open " .. fname .. ":", 1, true) ~= nil)
  local ok, derr = pcall(dofile, fname)
  assert(ok == false and
         tostring(derr):find("cannot open " .. fname .. ":", 1, true) ~= nil)
end

do
  local fname = "lua54_loadfile_env.tmp"
  os.remove(fname)
  local f = assert(io.open(fname, "w"))
  f:write("return x\n")
  f:close()
  local env = { x = "file-env" }
  local loaded = assert(loadfile(fname, "t", env))
  local name, value = debug.getupvalue(loaded, 1)
  assert(name == "_ENV" and value == env)
  assert(loaded() == "file-env")
  assert(os.remove(fname))
end

do
  local fname = "lua54_loadfile_binary_env.tmp"
  os.remove(fname)
  local f = assert(io.open(fname, "wb"))
  f:write(string.dump(function() return x end, true))
  f:close()
  local env = { x = "binary-env" }
  local loaded = assert(loadfile(fname, "b", env))
  local name, value = debug.getupvalue(loaded, 1)
  assert(name == "(no name)" and value == env)
  assert(loaded() == "binary-env")
  assert(os.remove(fname))
end

do
  local fname = "lua54_loadfile_comment_binary.tmp"
  local f = assert(io.open(fname, "wb"))
  f:write("#this is a comment for a binary file\0\n",
	  string.dump(function() return 20, "\0\0\0" end))
  f:close()
  local a, b, c = assert(loadfile(fname))()
  assert(a == 20 and b == "\0\0\0" and c == nil)
  assert(os.remove(fname))
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
    local env_trace_loop = assert(load([[
      return function(n)
        local sum = 0
        for i = 1, n do
          sum = sum + x
        end
        return sum
      end
    ]]))()
    assert(debug.setupvalue(env_trace_loop, 1, { x = 7 }) == "_ENV")
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
    local function generic_for_close_loop(n)
      local t = {}
      local sum = 0
      for i = 1, n do t[i] = i end
      for _, v in next, t, nil, false do
        sum = sum + v
      end
      return sum
    end
    local function eq_meta_loop(n)
      local mt = { __eq = function(a, b) return a.v == b.v end }
      local a = setmetatable({ v = 1 }, mt)
      local b = { v = 1 }
      local c = 0
      for i = 1, n do
        if a == b and b == a then c = c + 1 end
      end
      return c
    end
    local function int_for_boundary_loop()
      local c = 0
      -- Near integer limits, Lua 5.4 must not record a trace that changes the
      -- control variable to float just to avoid possible int32 overflow.
      for i = -1, -math.huge, -1 do
        if i < -10 then break end
        assert(math.type(i) == "integer")
        c = c + 1
      end
      return c
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
    assert(env_trace_loop(80) == 560)
    assert(env_trace_loop(80) == 560)
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
    before = trace_highwater()
    assert(generic_for_close_loop(80) == 3240)
    assert(generic_for_close_loop(80) == 3240)
    assert(trace_highwater() > before)
    jit.flush()
    before = trace_highwater()
    assert(eq_meta_loop(80) == 80)
    assert(eq_meta_loop(80) == 80)
    assert(trace_highwater() > before)
    jit.flush()
    assert(int_for_boundary_loop() == 10)
    assert(int_for_boundary_loop() == 10)
    jit.flush()
    jitopt.start("hotloop=56", "hotexit=10")
  end
end
