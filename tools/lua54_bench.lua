-- Cross-runtime Lua 5.4 benchmark: runs identically on the LuaJIT Lua 5.4
-- compat build (JIT on / -joff) and on the official lua54.exe.
-- Output: one "BENCH name time_s alloc_kb" line per workload (min of REPS
-- timed runs; JIT warmup happens inside the discarded first rep) and one
-- "MEM name retained_kb" line per retained-memory probe.

local REPS = 3

-- Windows CRT treats _IOLBF as full buffering on redirected streams, so
-- use "no" to keep per-bench progress visible in live logs.
io.stdout:setvbuf("no")

local function bench(name, n, f)
  io.write(("RUN   %-18s\n"):format(name))
  f(math.max(1, n // 20))            -- warmup (lets the JIT compile traces)
  local best, alloc = math.huge, 0
  for _ = 1, REPS do
    collectgarbage("collect")
    collectgarbage("collect")
    local h0 = collectgarbage("count")
    local t0 = os.clock()
    f(n)
    local dt = os.clock() - t0
    local h1 = collectgarbage("count")
    if dt < best then best, alloc = dt, h1 - h0 end
  end
  io.write(("BENCH %-18s time_s=%8.4f alloc_kb=%10.1f\n")
           :format(name, best, alloc))
end

local function mem(name, f)
  collectgarbage("collect")
  collectgarbage("collect")
  local h0 = collectgarbage("count")
  local keep = f()
  collectgarbage("collect")
  collectgarbage("collect")
  local h1 = collectgarbage("count")
  io.write(("MEM   %-18s retained_kb=%10.1f\n"):format(name, h1 - h0))
  return keep
end

-- 1. recursive calls -------------------------------------------------------
local function fib(n) if n < 2 then return n end return fib(n-1) + fib(n-2) end
bench("fib30", 30, function(n) assert(fib(n) >= 0) end)

-- 2. integer arithmetic loop ------------------------------------------------
bench("int_arith", 5e6, function(n)
  local s = 0
  for i = 1, n do s = s + i * 3 // 2 % 1000 - 1 end
  assert(s ~= 0)
end)

-- 3. float arithmetic loop ---------------------------------------------------
bench("float_arith", 5e6, function(n)
  local s = 0.5
  for i = 1, n do s = s + i * 0.25 - s * 0.5 end
  assert(s > 0)
end)

-- 4. bitwise loop -------------------------------------------------------------
bench("bitwise", 5e6, function(n)
  local s = 0
  for i = 1, n do s = (s ~ i) & 0xffffffff | (i << 7) >> 3 end
  assert(s ~= 1)
end)

-- 5. sieve of Eratosthenes (array part) ---------------------------------------
bench("sieve", 2e6, function(n)
  local flags, count = {}, 0
  for i = 2, n do flags[i] = true end
  for i = 2, n do
    if flags[i] then
      count = count + 1
      for k = i + i, n, i do flags[k] = false end
    end
  end
  assert(count > 0)
end)

-- 6. hash table store/load ------------------------------------------------------
bench("hash_table", 3e5, function(n)
  local t = {}
  for i = 1, n do t["key" .. (i % 1000)] = i end
  local s = 0
  for i = 1, n do s = s + (t["key" .. (i % 1000)] or 0) end
  assert(s > 0)
end)

-- 7. string concat via table.concat ----------------------------------------------
bench("str_concat", 2e5, function(n)
  local parts = {}
  for i = 1, n do parts[i] = "x" .. i end
  local s = table.concat(parts, ",")
  assert(#s > n)
end)

-- 8. pattern matching --------------------------------------------------------------
bench("gsub_gmatch", 2e3, function(n)
  local text = ("the quick brown fox jumps over the lazy dog "):rep(50)
  local c = 0
  for _ = 1, n do
    local s = text:gsub("(%w+)", "<%1>")
    for _ in s:gmatch("<%w+>") do c = c + 1 end
  end
  assert(c > 0)
end)

-- 9. string.format -------------------------------------------------------------------
bench("str_format", 3e5, function(n)
  local s
  for i = 1, n do
    s = ("%d|%8.3f|%s|%x"):format(i, i * 0.5, "tag", i)
  end
  assert(#s > 0)
end)

-- 10. closure creation + upvalue churn ----------------------------------------------
bench("closures", 5e5, function(n)
  local acc = 0
  for i = 1, n do
    local x = i
    local f = function() x = x + 1 return x end
    acc = acc + f()
  end
  assert(acc > 0)
end)

-- 11. OO method dispatch via metatable -----------------------------------------------
bench("oo_dispatch", 2e6, function(n)
  local Point = {}
  Point.__index = Point
  function Point.new(x, y) return setmetatable({x = x, y = y}, Point) end
  function Point:dot(o) return self.x * o.x + self.y * o.y end
  local a, b = Point.new(1, 2), Point.new(3, 4)
  local s = 0
  for _ = 1, n do s = s + a:dot(b) end
  assert(s > 0)
end)

-- 12. coroutine ping-pong ---------------------------------------------------------------
bench("coroutines", 2e5, function(n)
  local co = coroutine.create(function(x)
    while true do x = coroutine.yield(x + 1) end
  end)
  local v = 0
  for i = 1, n do
    local ok, r = coroutine.resume(co, i)
    v = v + r
    assert(ok)
  end
  assert(v > 0)
end)

-- 13. table.sort ---------------------------------------------------------------------------
bench("sort", 2e5, function(n)
  local t = {}
  local seed = 12345
  for i = 1, n do
    seed = (seed * 1103515245 + 12345) % 2147483648
    t[i] = seed
  end
  table.sort(t)
  table.sort(t, function(a, b) return a > b end)
  assert(t[1] >= t[n])
end)

-- 14. string slicing/bytes ------------------------------------------------------------------
bench("str_slice", 5e5, function(n)
  local text = ("abcdefghijklmnopqrstuvwxyz"):rep(10)
  local s = 0
  for i = 1, n do
    local j = i % 200 + 1
    s = s + #text:sub(j, j + 16) + text:byte(j)
  end
  assert(s > 0)
end)

-- 15. numeric kernel (mandelbrot-ish escape iteration) ---------------------------------------
bench("mandel", 4e2, function(n)
  local count = 0
  for py = 1, n do
    local y0 = py / n * 2 - 1
    for px = 1, n do
      local x0 = px / n * 3 - 2
      local x, y, it = 0.0, 0.0, 0
      while x * x + y * y <= 4 and it < 50 do
        x, y = x * x - y * y + x0, 2 * x * y + y0
        it = it + 1
      end
      if it == 50 then count = count + 1 end
    end
  end
  assert(count > 0)
end)

-- retained-memory probes -----------------------------------------------------------------------
local sink = {}
sink[1] = mem("array_1M_ints", function()
  local t = {}
  for i = 1, 1000000 do t[i] = i end
  return t
end)
sink[2] = mem("tables_100k_small", function()
  local t = {}
  for i = 1, 100000 do t[i] = {x = i, y = i + 1} end
  return t
end)
sink[3] = mem("strings_100k", function()
  local t = {}
  for i = 1, 100000 do t[i] = "payload_string_" .. i end
  return t
end)
assert(sink[1][1] == 1)

io.write("BENCH-DONE\n")
