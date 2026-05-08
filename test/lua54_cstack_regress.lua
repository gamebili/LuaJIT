do
  local r = table.pack(pcall(function(a, b) return a, nil, b end, "x", "y"))
  assert(r.n == 4 and r[1] == true and r[2] == "x" and r[3] == nil and
         r[4] == "y")
end

do
  local ok, a, b = xpcall(function(a, b) return a, b end,
                          function(e) return "handled:" .. tostring(e) end,
                          "a", "b")
  assert(ok == true and a == "a" and b == "b")
end

do
  local ok, err = pcall(pcall)
  assert(ok == false and err:match("bad argument #1 to 'pcall'") ~= nil)
  ok, err = pcall(xpcall, function() end, nil)
  assert(ok == false and err:match("bad argument #2 to 'xpcall'") ~= nil)
end

do
  local function loop()
    assert(pcall(loop))
  end
  local ok, err = xpcall(loop, loop)
  assert(ok == false and tostring(err):match("error") ~= nil)
end

do
  local count = 0
  local function foo()
    count = count + 1
    string.gsub("a", ".", foo)
  end
  local ok, err = pcall(foo)
  assert(ok == false and tostring(err):match("stack overflow") ~= nil)
  assert(count > 0)
end

do
  local count = 0
  local foo
  local t = setmetatable({}, { __index = function()
    return foo()
  end })
  foo = function()
    count = count + 1
    string.gsub("a", ".", t)
  end
  local ok, err = pcall(foo)
  assert(ok == false and tostring(err):match("stack overflow") ~= nil)
  assert(count > 0)
end

do
  local count = 0
  local coro = false
  for _ = 1, 220 do
    local previous = coro
    coro = coroutine.create(function()
      local closer <close> = setmetatable({}, { __close = function()
        count = count + 1
        if previous then assert(coroutine.close(previous)) end
      end })
      coroutine.yield()
    end)
    assert(coroutine.resume(coro))
  end
  local ok, err = coroutine.close(coro)
  assert(ok == false and tostring(err):match("C stack overflow", 1, true) ~= nil)
  assert(count > 0)
end
