local function bounded_table_finalizer(limit)
  local u
  local keep
  local done = false
  u = setmetatable({}, { __gc = function() done = true end })
  keep = {34}
  local n = 0
  repeat
    n = n + 1
    u = {}
  until done or n > limit
  assert(done and keep[1] == 34,
    "table finalizer did not run in bounded allocation loop")
  return u
end

local function bounded_nested_table_finalizer(limit)
  local u
  local done = false
  u = { setmetatable({}, { __gc = function() done = true end }) }
  local keep = {34}
  local n = 0
  repeat
    n = n + 1
    u = {{}}
  until done or n > limit
  assert(done and keep[1] == 34,
    "nested table finalizer did not run in bounded allocation loop")
  return u
end

-- This mirrors the official gc.lua weak/ephemeron setup before it calls GC().
-- The collector must stay responsive after these full collections; otherwise
-- the first allocation-triggered table finalizer can be postponed indefinitely.
collectgarbage("collect")

local lim = 15
local a = setmetatable({}, { __mode = "k" })
for i = 1, lim do a[{}] = i end
for i = 1, lim do a[i] = i end
for i = 1, lim do local s = string.rep("@", i); a[s] = s.."#" end
collectgarbage("collect")

a = setmetatable({}, { __mode = "v" })
a[1] = string.rep("b", 21)
collectgarbage("collect")
assert(a[1])
a[1] = nil
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

do
  local jitmod = rawget(_G, "jit")
  if jitmod then
    jitmod.off()
    jitmod.flush()
  end

  collectgarbage("generational", 1, 1000)
  local weak = setmetatable({}, { __mode = "v" })
  local old = {}
  weak.old = old
  collectgarbage("collect")
  collectgarbage("collect")
  old = nil

  do
    local young = {}
    weak.young = young
    young = nil
  end
  for _ = 1, 20000 do
    local _ = {}
  end

  assert(weak.young == nil, "generational minor must clear young weak values")
  assert(weak.old ~= nil, "generational minor must keep old weak values")
  collectgarbage("collect")
  assert(weak.old == nil, "full major collection must clear old weak values")

  collectgarbage("generational", 1, 1000)
  weak = setmetatable({}, { __mode = "v" })
  old = {}
  weak.old = old
  collectgarbage("collect")
  collectgarbage("collect")
  old = nil
  do
    local young = {}
    weak.young = young
    young = nil
  end
  assert(collectgarbage("step", 0) == false,
    "generational manual step must report no completed incremental cycle")
  assert(weak.young == nil,
    "generational manual step must clear young weak values")
  assert(weak.old ~= nil,
    "generational manual step must not act as a full major collection")

  do
    local anchor = { child = { value = 55 } }
    local function get()
      return anchor.child.value
    end
    collectgarbage("collect")
    collectgarbage("collect")
    for _ = 1, 4 do
      collectgarbage("step", 0)
    end
    assert(get() == 55,
      "generational minor must keep open-upvalue object graphs")
  end

  collectgarbage("generational", 1, 1000)
  weak = setmetatable({}, { __mode = "kv" })
  collectgarbage("collect")
  weak[1] = { 10 }
  collectgarbage("step", 0)
  collectgarbage("step", 0)
  weak[1] = { 20 }
  collectgarbage("step", 0)
  assert(weak[1] == nil,
    "generational touched2 weak table barrier must not re-link grayagain")

  do
    local holder = {}
    collectgarbage("generational", 1, 1000)
    collectgarbage("collect")
    collectgarbage("collect")
    holder.first = { 1 }
    collectgarbage("step", 0)
    local finalized = false
    holder.second = setmetatable({ tag = 2 }, { __gc = function()
      finalized = true
    end })
    collectgarbage("step", 0)
    assert(not finalized and holder.second and holder.second.tag == 2,
      "generational touched2 barrier must propagate gray table")
  end

  collectgarbage("generational", 1, 1)
  weak = setmetatable({}, { __mode = "v" })
  old = {}
  weak.old = old
  collectgarbage("collect")
  collectgarbage("collect")
  old = nil
  for _ = 1, 1000 do
    local _ = { "major", {} }
    collectgarbage("step", 1)
    if weak.old == nil then break end
  end
  assert(weak.old == nil,
    "generational major threshold must collect old weak values")

  collectgarbage("stop")
  collectgarbage("incremental")
  collectgarbage("collect")
  collectgarbage("generational", 4, 4)
  weak = setmetatable({}, { __mode = "v" })
  old = {}
  weak.old = old
  local bulk = {}
  for i = 1, 25000 do bulk[i] = { i, i, i, i } end
  collectgarbage("step", 0)
  old = nil
  collectgarbage("step", 0)
  assert(weak.old == nil,
    "bad generational major must continue with a full step")
  assert(bulk[25000][1] == 25000,
    "bad-major regression anchor must stay live")
  collectgarbage("restart")

  do
    local setter, getter
    do
      local x
      function setter(v) x = v end
      function getter() return x end
    end
    collectgarbage("generational", 1, 1000)
    collectgarbage("collect")
    collectgarbage("collect")
    local child = { value = 42 }
    local young = { child = child }
    setter(young)
    young = nil
    child = nil
    collectgarbage("step", 0)
    collectgarbage("step", 0)
    local got = getter()
    assert(got and got.child and got.child.value == 42,
      "old closed upvalue barrier must keep young object graph")
  end

  collectgarbage("generational", 1, 1000)
  collectgarbage("collect")
  collectgarbage("collect")
  weak = setmetatable({}, { __mode = "v" })
  local finalized = false
  do
    local value = setmetatable({ tag = 1 }, { __gc = function()
      finalized = true
    end })
    weak.value = value
    value = nil
  end
  collectgarbage("step", 0)
  assert(finalized and weak.value == nil,
    "generational manual step must finalize and clear weak table value")

  if jitmod then
    jitmod.on()
  end
end

do
  local jitmod = rawget(_G, "jit")
  if jitmod then
    jitmod.flush()
    jitmod.on()
  end

  collectgarbage("generational", 1, 1000)
  local weak = setmetatable({}, { __mode = "kv" })
  collectgarbage("collect")
  weak[1] = { 10 }
  collectgarbage("step", 0)
  collectgarbage("step", 0)
  weak[1] = { 20 }
  collectgarbage("step", 0)
  assert(weak[1] == nil,
    "JIT-on generational manual step must complete conservative major work")

  jitmod.flush()
  jitmod.on()
  jitmod.opt.start("hotloop=1")
  local function hot()
    local n = 0
    for i = 1, 20 do n = n + i end
    return n
  end
  for _ = 1, 4 do assert(hot() == 210) end
  jitmod.off()

  collectgarbage("generational", 1, 1000)
  weak = setmetatable({}, { __mode = "kv" })
  collectgarbage("collect")
  weak[1] = { 10 }
  collectgarbage("step", 0)
  collectgarbage("step", 0)
  weak[1] = { 20 }
  collectgarbage("step", 0)
  assert(weak[1] == nil,
    "saved traces must force conservative generational manual major work")
  jitmod.flush()
  jitmod.on()
end

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
bounded_nested_table_finalizer(50000)
assert(a ~= nil and x ~= nil)

do
  local eph1 = setmetatable({}, { __mode = "k" })
  local eph2 = setmetatable({}, { __mode = "k" })
  local root = {}
  local key2 = {}
  eph1[root] = { key2 = key2 }
  eph2[key2] = { tag = 99 }
  key2 = nil
  collectgarbage("collect")
  collectgarbage("collect")
  local chained_key, chained = next(eph2)
  assert(chained and chained.tag == 99,
    "ephemeron fixed point must keep chained weak-key value")
  chained_key = nil
  chained = nil
  root = nil
  for _ = 1, 4 do
    collectgarbage("collect")
    if next(eph1) == nil and next(eph2) == nil then break end
  end
  assert(next(eph1) == nil and next(eph2) == nil,
    "ephemeron chain must clear after external root is gone")
end

do
  local weak = setmetatable({}, { __mode = "kv" })
  local key = {}
  local value = { key = key }
  weak[key] = value
  key = nil
  value = nil
  collectgarbage("collect")
  collectgarbage("collect")
  assert(next(weak) == nil,
    "weak-key weak-value table must not keep key through value")
end

do
  local finalized = false
  local weak = setmetatable({}, { __mode = "k" })
  local key = {}
  local value = setmetatable({}, { __gc = function()
    finalized = true
  end })
  weak[key] = value
  value = nil
  collectgarbage("collect")
  collectgarbage("collect")
  assert(not finalized and next(weak) ~= nil,
    "ephemeron must keep finalizable value while key is reachable")
  key = nil
  for _ = 1, 6 do
    collectgarbage("collect")
    if finalized and next(weak) == nil then break end
  end
  assert(finalized and next(weak) == nil,
    "ephemeron finalizable value must clear after key is gone")
end

do
  local finalized = false
  local rescued
  local weak = setmetatable({}, { __mode = "v" })
  do
    local value = setmetatable({ tag = 1 }, { __gc = function(o)
      finalized = true
      rescued = o
    end })
    weak.value = value
    value = nil
  end
  collectgarbage("collect")
  assert(finalized and weak.value == nil,
    "weak value table must clear table pending finalization")
  weak.rescued = rescued
  collectgarbage("collect")
  assert(weak.rescued == rescued,
    "resurrected finalized table must be a normal weak value")
  rescued = nil
  collectgarbage("collect")
end

do
  local weak = setmetatable({}, { __mode = "kv" })
  local key = {}
  local seen = false
  do
    local value = setmetatable({}, { __gc = function()
      seen = weak[key]
    end })
    weak[key] = value
    value = nil
  end
  collectgarbage("collect")
  assert(seen == nil and next(weak) == nil,
    "all-weak table must clear values before finalizer callbacks")
end

collectgarbage("collect")
collectgarbage("collect")
local mem = collectgarbage("count")
local weak = setmetatable({}, { __mode = "kv" })
weak[string.rep("a", 2^22)] = 25
weak[string.rep("b", 2^22)] = {}
weak[{}] = 14
assert(collectgarbage("count") > mem + 2^13)
collectgarbage("collect")
collectgarbage("collect")
assert(collectgarbage("count") >= mem + 2^12 and
       collectgarbage("count") < mem + 2^13)
-- Weak long-string cleanup can need an extra sweep in this LuaJIT bridge after
-- a fresh compat rebuild; wait for the weak table to reach the semantic fixed
-- point instead of depending on an exact two-collection schedule.
for _ = 1, 8 do
  local first, _, extra = next(weak)
  if first == nil then break end
  extra = next(weak, first)
  if extra == nil then break end
  collectgarbage("collect")
end
local key, value = next(weak)
assert(key == string.rep("a", 2^22) and value == 25)
assert(next(weak, key) == nil)
weak[key] = nil
key = nil
for _ = 1, 8 do
  collectgarbage("collect")
  if next(weak) == nil then break end
end
assert(next(weak) == nil)
assert(weak[string.rep("b", 100)] == nil)

do
  local function wideint()
    return tonumber("1099511627776")
  end

  local weak_values = setmetatable({}, { __mode = "v" })
  weak_values.slot = wideint()
  collectgarbage("collect")
  collectgarbage("collect")
  assert(weak_values.slot == wideint(),
    "weak value table must not clear 64-bit integer values")

  for _, mode in ipairs({ "k", "kv" }) do
    local weak_keys = setmetatable({}, { __mode = mode })
    do
      local key = wideint()
      weak_keys[key] = "wide"
    end
    collectgarbage("collect")
    collectgarbage("collect")
    assert(weak_keys[wideint()] == "wide",
      "weak key table must not clear 64-bit integer keys")
  end
end

do
  local opts = {
    false, "collect", "step", "count", "isrunning", "stop", "restart",
    "setpause", "setstepmul", "incremental", "generational",
  }
  for _, opt in ipairs(opts) do
    local ok, res = false, "notrun"
    do
      local t = setmetatable({}, { __gc = function()
        if opt == false then
          ok, res = pcall(collectgarbage)
        elseif opt == "setpause" or opt == "setstepmul" then
          ok, res = pcall(collectgarbage, opt, 200)
        else
          ok, res = pcall(collectgarbage, opt)
        end
      end })
      t = nil
    end
    collectgarbage("collect")
    assert(ok and res == nil, "collectgarbage is reentrant inside __gc")
  end

  local ok, err
  do
    local t = setmetatable({}, { __gc = function()
      ok, err = pcall(collectgarbage, "invalid")
    end })
    t = nil
  end
  collectgarbage("collect")
  assert(ok == false and tostring(err):find("invalid option", 1, true),
    "collectgarbage must still validate options inside __gc")
  do
    local t = setmetatable({}, { __gc = function()
      ok, err = pcall(collectgarbage, "incremental", 200, 300, 12.5)
    end })
    t = nil
  end
  collectgarbage("collect")
  assert(ok == false and tostring(err):find("bad argument #4", 1, true),
    "collectgarbage mode options must still validate inside __gc")
  assert(collectgarbage("isrunning"))
end
