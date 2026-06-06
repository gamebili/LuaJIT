local jitmod = rawget(_G, "jit")
if not jitmod then
  return
end

assert(_VERSION == "Lua 5.4")

local ok_util, jutil = pcall(require, "jit.util")
if not ok_util then
  return
end
local bit = require("bit")
local vmdef = require("jit.vmdef")

local function trace_highwater()
  local n = 0
  for i = 1, 1000 do
    if jutil.traceinfo(i) then n = i end
  end
  return n
end
jitmod.off(trace_highwater, true)

local function clone_long(s)
  return s:sub(1, 41) .. s:sub(42)
end

local function long_pair(tag)
  local s = ("lua54-" .. tag .. "-"):rep(8)
  assert(#s > 40)
  local a = clone_long(s)
  local b = clone_long(s)
  assert(a == b)
  assert(string.format("%p", a) ~= string.format("%p", b),
	 "runtime long strings must keep distinct object identity")
  return a, b
end

local function assert_no_trace(fn, what)
  jitmod.off()
  jitmod.flush()
  -- Some error-path recorders leave short-lived trace objects until the next
  -- GC step; collect between isolated cases so one negative test cannot poison
  -- the following positive recorder check.
  collectgarbage()
  jitmod.on()
  jit.opt.start("hotloop=1", "hotexit=1")
  local before = trace_highwater()
  fn()
  assert(trace_highwater() == before, what .. " unexpectedly recorded a trace")
end

local function assert_records_trace(fn, what)
  jitmod.off()
  jitmod.flush()
  -- Keep each recorder scenario isolated even after pcall/error hot loops.
  collectgarbage()
  jitmod.on()
  jit.opt.start("hotloop=1", "hotexit=1")
  local before = trace_highwater()
  fn()
  assert(trace_highwater() > before, what .. " did not record a trace")
end

local function trace_has_ir_op(first_trace, last_trace, opname)
  for tr = first_trace, last_trace do
    if jutil.traceinfo(tr) then
      for ins = 1, 1000 do
	local _, ot = jutil.traceir(tr, ins)
	if not ot then break end
	local opidx = bit.rshift(ot, 8)
	local op = vmdef.irnames:sub(opidx * 6 + 1, opidx * 6 + 6)
	if op:gsub("%s+$", "") == opname then return true end
      end
    end
  end
  return false
end

local function trace_has_ir_call(first_trace, last_trace, callname)
  for tr = first_trace, last_trace do
    if jutil.traceinfo(tr) then
      for ins = 1, 1000 do
	local _, ot, _, op2 = jutil.traceir(tr, ins)
	if not ot then break end
	local opidx = bit.rshift(ot, 8)
	local op = vmdef.irnames:sub(opidx * 6 + 1, opidx * 6 + 6)
	op = op:gsub("%s+$", "")
	if (op == "CALLN" or op == "CALLA" or op == "CALLL" or
	    op == "CALLS") and vmdef.ircall[op2] == callname then
	  return true
	end
      end
    end
  end
  return false
end

local function assert_records_ir_op(fn, what, opname)
  jitmod.off()
  jitmod.flush()
  collectgarbage()
  jitmod.on()
  jit.opt.start("hotloop=1", "hotexit=1")
  local before = trace_highwater()
  fn()
  local after = trace_highwater()
  assert(after > before, what .. " did not record a trace")
  assert(trace_has_ir_op(before + 1, after, opname),
	 what .. " did not record IR_" .. opname)
end

local function assert_no_ir_op(fn, what, opname)
  jitmod.off()
  jitmod.flush()
  collectgarbage()
  jitmod.on()
  jit.opt.start("hotloop=1", "hotexit=1")
  local before = trace_highwater()
  fn()
  local after = trace_highwater()
  assert(not trace_has_ir_op(before + 1, after, opname),
	 what .. " unexpectedly recorded IR_" .. opname)
end

local function assert_records_ir_call(fn, what, callname)
  jitmod.off()
  jitmod.flush()
  collectgarbage()
  jitmod.on()
  jit.opt.start("hotloop=1", "hotexit=1")
  local before = trace_highwater()
  fn()
  local after = trace_highwater()
  assert(after > before, what .. " did not record a trace")
  assert(trace_has_ir_call(before + 1, after, callname),
	 what .. " did not record " .. callname)
end

local function assert_records_ir_calls(fn, what, callnames)
  jitmod.off()
  jitmod.flush()
  collectgarbage()
  jitmod.on()
  jit.opt.start("hotloop=1", "hotexit=1")
  local before = trace_highwater()
  fn()
  local after = trace_highwater()
  assert(after > before, what .. " did not record a trace")
  for i = 1, #callnames do
    local callname = callnames[i]
    assert(trace_has_ir_call(before + 1, after, callname),
	   what .. " did not record " .. callname)
  end
end

local function assert_records_single_ir_trace(fn, what, opname)
  local saw_trace = false
  -- Whole-file runs can occasionally allocate an unrelated side trace number
  -- after prior recorder cases. The regression that matters is whether the
  -- fresh trace range contains the expected recorder IR, not whether no side
  -- trace was numbered next to it.
  local last_detail = ""
  for _ = 1, 3 do
    jitmod.off()
    jitmod.flush()
    collectgarbage()
    jitmod.on()
    jit.opt.start("hotloop=1", "hotexit=10")
    local before = trace_highwater()
    fn()
    local after = trace_highwater()
    last_detail = " (" .. before .. "->" .. after .. ")"
    saw_trace = saw_trace or after > before
    if after > before and trace_has_ir_op(before + 1, after, opname) then
      return
    end
  end
  assert(saw_trace, what .. " did not record a trace")
  assert(false, what .. " did not record IR_" .. opname .. last_detail)
end

local function make_many_upvalue_counter()
  local src = {}
  for i = 1, 150 do
    src[#src + 1] = ("local u%d = %d\n"):format(i, i)
  end
  src[#src + 1] = "return function(n)\n  local seed = "
  for i = 1, 149 do
    if i > 1 then src[#src + 1] = " + " end
    src[#src + 1] = "u" .. i
  end
  src[#src + 1] = "\n  local sum = 0\n"
  src[#src + 1] = "  for _ = 1, n do\n"
  src[#src + 1] = "    u150 = u150 + 1\n"
  src[#src + 1] = "    sum = sum + seed + u150\n"
  src[#src + 1] = "  end\n"
  src[#src + 1] = "  return sum, u150\nend\n"
  return assert(load(table.concat(src), "=lua54-many-upvalues", "t"))()
end

jit.opt.start("hotloop=1", "hotexit=1")

do
  local f = make_many_upvalue_counter()
  local seed = 149 * 150 / 2
  assert_records_trace(function()
    local sum, last = f(80)
    -- Lua 5.4 raises the upvalue limit to 200. High-index mutable upvalues
    -- must be recordable too, not forced back through the interpreter forever.
    assert(last == 230)
    assert(sum == 80 * seed + (151 + 230) * 80 / 2)
  end, "Lua 5.4 high-index upvalue load/store")
end

do
  local a = "lua54-order\0a"
  local b = "lua54-order\0b"
  local function order_score(x, y)
    local score = 0
    if x < y then score = score + 1 end
    if x <= y then score = score + 1 end
    if y > x then score = score + 1 end
    if y >= x then score = score + 1 end
    if not (y < x) then score = score + 1 end
    if not (x > y) then score = score + 1 end
    return score
  end
  local expected = order_score(a, b) * 80
  local function order_loop(x, y)
    local n = 0
    for _ = 1, 80 do
      -- Lua 5.4 string ordering is locale-sensitive and compares embedded
      -- NUL-separated segments. The trace must call the 5.4 helper instead
      -- of permanently exiting or reusing the old bytewise comparator.
      n = n + order_score(x, y)
    end
    return n
  end
  assert_records_trace(function()
    assert(order_loop(a, b) == expected)
  end, "Lua 5.4 locale string ordered comparison")
end

do
  local a, b = long_pair("eq")
  local function eq_loop(x, y)
    local n = 0
    for i = 1, 80 do
      if x == y then n = n + 1 end
    end
    return n
  end
  assert_records_trace(function()
    assert(eq_loop(a, b) == 80)
  end, "long-string equality")
end

do
  local long_pairs = {}
  for i = 1, 80 do
    local s = ("lua54-generic-eq-%03d-"):format(i):rep(4)
    local a = clone_long(s)
    local b = clone_long(s)
    assert(a == b)
    assert(string.format("%p", a) ~= string.format("%p", b))
    long_pairs[i] = { a, b }
  end
  local function generic_eq_loop()
    local n = 0
    for _ = 1, 3 do
      for i = 1, 80 do
        local p = long_pairs[i]
        -- The recorder must compare long-string bytes, not specialize the trace
        -- to the first pair of GC objects seen by the hot loop.
        if p[1] == p[2] then n = n + 1 end
      end
    end
    return n
  end
  assert_records_trace(function()
    assert(generic_eq_loop() == 240)
  end, "generic long-string equality")
end

do
  local a, b = long_pair("table-load")
  local keys = {}
  for i = 1, 80 do keys[i] = (i % 2 == 0) and a or b end
  local t = { [a] = 37 }
  assert_records_trace(function()
    local n = 0
    for i = 1, 80 do
      n = n + t[keys[i]]
    end
    assert(n == 2960)
  end, "long-string table load")
end

do
  local a, b = long_pair("table-store-existing")
  local keys = {}
  for i = 1, 80 do keys[i] = (i % 2 == 0) and a or b end
  local t = { [a] = 0 }
  assert_records_trace(function()
    for i = 1, 80 do
      t[keys[i]] = i
    end
  end, "long-string table existing-key store")
  assert(t[a] == 80 and t[b] == 80)
  local n = 0
  for _ in pairs(t) do n = n + 1 end
  assert(n == 1)
end

do
  local a, b = long_pair("table-store-existing-float")
  local keys = {}
  for i = 1, 80 do keys[i] = (i % 2 == 0) and a or b end
  local t = { [a] = 0.5 }
  assert_records_trace(function()
    for i = 1, 80 do
      t[keys[i]] = i + 0.5
    end
  end, "long-string table existing-key float store")
  assert(t[a] == 80.5 and t[b] == 80.5 and math.type(t[a]) == "float")
  local n = 0
  for _ in pairs(t) do n = n + 1 end
  assert(n == 1)
end

do
  local a, b = long_pair("table-store-existing-bool")
  local keys = {}
  for i = 1, 80 do keys[i] = (i % 2 == 0) and a or b end
  local t = { [a] = false }
  assert_records_trace(function()
    for i = 1, 80 do
      t[keys[i]] = true
    end
  end, "long-string table existing-key boolean store")
  assert(t[a] == true and t[b] == true)
  local n = 0
  for _ in pairs(t) do n = n + 1 end
  assert(n == 1)
end

do
  local a, b = long_pair("table-store-existing-gc")
  local keys = {}
  for i = 1, 80 do keys[i] = (i % 2 == 0) and a or b end
  local value = "lua54-gc-store-value"
  local t = { [a] = "lua54-gc-store-seed" }
  assert_records_ir_op(function()
    for i = 1, 80 do
      -- Storing a GC value through the long-string bytewise lookup must still
      -- emit the table write barrier; otherwise black tables can retain white
      -- values without being revisited by the collector.
      t[keys[i]] = value
    end
  end, "long-string table existing-key GC store", "TBAR")
  assert(t[a] == value and t[b] == value)
  local n = 0
  for _ in pairs(t) do n = n + 1 end
  assert(n == 1)
end

do
  local keys = {}
  for i = 1, 80 do
    keys[i] = clone_long(("lua54-table-store-new-%03d-"):format(i):rep(4))
    assert(#keys[i] > 40)
  end
  local t = {}
  assert_records_trace(function()
    for i = 1, 80 do
      t[keys[i]] = i
    end
  end, "long-string table new-key store")
  for i = 1, 80 do assert(t[keys[i]] == i) end
  local n = 0
  for _ in pairs(t) do n = n + 1 end
  assert(n == 80)
end

do
  local keys = {}
  for i = 1, 80 do
    keys[i] = clone_long(("lua54-table-store-new-gc-%03d-"):format(i):rep(4))
    assert(#keys[i] > 40)
  end
  local value = "lua54-new-gc-store-value"
  local t = {}
  assert_records_ir_op(function()
    for i = 1, 80 do
      -- New long-string keys go through lj_tab_setstr() so rehash and key
      -- barriers stay centralized, while the traced value write still needs a
      -- table barrier for GC payloads.
      t[keys[i]] = value
    end
  end, "long-string table new-key GC store", "TBAR")
  for i = 1, 80 do assert(t[keys[i]] == value) end
  local n = 0
  for _ in pairs(t) do n = n + 1 end
  assert(n == 80)
end

do
  local k1 = 1099511627776
  local k2 = k1 + 1
  local keys = {}
  for i = 1, 80 do keys[i] = (i % 2 == 0) and k1 or k2 end
  local t = { [k1] = 11, [k2] = 13 }
  assert(math.type(k1) == "integer" and math.type(k2) == "integer")

  assert_records_trace(function()
    local n = 0
    for i = 1, 80 do
      n = n + t[keys[i]]
    end
    assert(n == 960)
  end, "Lua 5.4 boxed int64 table load")

  assert_records_trace(function()
    for i = 1, 80 do
      t[keys[i]] = i
    end
  end, "Lua 5.4 boxed int64 table store")
  assert(t[k1] == 80 and t[k2] == 79)

  assert_records_trace(function()
    local seen, sum = 0, 0
    for _ = 1, 80 do
      for k, v in pairs(t) do
	assert(math.type(k) == "integer")
	if k == k1 or k == k2 then
	  seen = seen + 1
	  sum = sum + v
	end
      end
    end
    assert(seen == 160 and sum == 12720)
  end, "Lua 5.4 boxed int64 pairs")
end

do
  local t = setmetatable({ 1 }, {
    __index = function(_, k)
      if k <= 80 then return k end
    end
  })
  local function ipairs_index_loop(n)
    local sum = 0
    for _ = 1, n do
      local last = 0
      for i, v in ipairs(t) do
        sum = sum + v
        last = i
      end
      if last ~= 80 then return -last end
    end
    return sum
  end
  assert_records_trace(function()
    assert(ipairs_index_loop(3) == 9720)
    assert(ipairs_index_loop(3) == 9720)
  end, "Lua 5.4 ipairs __index")
end

do
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local iter, state, index = ipairs({})
      if select("#", iter(state, index)) == 1 and iter(state, index) == nil then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 ipairs exhausted nil result")
end

do
  local p = pairs
  assert_no_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok_direct, err_direct = pcall(function()
	local iter, state, key = pairs(nil)
	return iter(state, key)
      end)
      local ok_alias, err_alias = pcall(function()
	local iter, state, key = p(nil)
	return iter(state, key)
      end)
      local ok_iter_alias, err_iter_alias = pcall(function()
	local iter, state, key = pairs(nil)
	local f = iter
	return f(state, key)
      end)
      local ok_global, err_global = pcall(function()
	lua54_jit_pairs_global_it = pairs(nil)
	return lua54_jit_pairs_global_it(nil, nil)
      end)
      local ok_field, err_field = pcall(function()
	local holder = {}
	holder.iter = pairs(nil)
	return holder.iter(nil, nil)
      end)
      lua54_jit_pairs_global_it = nil
      if not ok_direct and
	 err_direct:find("bad argument #1 to 'iter'", 1, true) and
	 not ok_alias and
	 err_alias:find("bad argument #1 to 'iter'", 1, true) and
	 not ok_iter_alias and
	 err_iter_alias:find("bad argument #1 to 'f'", 1, true) and
	 not ok_global and
	 err_global:find("bad argument #1 to 'lua54_jit_pairs_global_it'",
			 1, true) and
	 not ok_field and
	 err_field:find("bad argument #1 to 'iter'", 1, true) then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 pairs iterator call names")
end

do
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      if string.format("%p", false) == "(null)" then n = n + 1 end
      if string.format("%q", math.huge) == "1e9999" then n = n + 1 end
      if string.format("%q", 0/0) == "(0/0)" then n = n + 1 end
      if string.format("%a", 1.5) == "0x1.8p+0" then n = n + 1 end
      if string.format("%A", 1.5) == "0X1.8P+0" then n = n + 1 end
    end
    assert(n == 400)
  end, "Lua 5.4 string.format %p/%q/%a")

  assert_no_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok, err = pcall(string.format, "%10s", "\0")
      if not ok and err:find("string contains zeros", 1, true) then n = n + 1 end
    end
    assert(n == 80)
  end, "Lua 5.4 string.format width NUL string")

  assert_no_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok, err = pcall(string.format, "%#a", 1.5)
      if not ok and err:find("modifiers for format '%a'/'%A' not implemented", 1, true) then
	n = n + 1
      end
      ok, err = pcall(string.format, "%#i")
      if not ok and err:find("bad argument #2 to 'string.format' (no value)", 1, true) then
	n = n + 1
      end
    end
    assert(n == 160)
  end, "Lua 5.4 string.format modifier and no-value errors")
end

do
  local int_text = {}
  local float_text = {}
  local width_text = {}
  for i = 1, 80 do
    int_text[i] = tostring(i)
    float_text[i] = tostring(i + 0.5)
    width_text[i] = string.format("%6s", i)
  end
  assert_records_ir_op(function()
    local n = 0
    for i = 1, 80 do
      if string.format("%s", i) == int_text[i] then n = n + 1 end
      if string.format("%s", i + 0.5) == float_text[i] then n = n + 1 end
      if string.format("%6s", i) == width_text[i] then n = n + 1 end
    end
    assert(n == 240)
  end, "Lua 5.4 string.format %s number", "TOSTR")
end

do
  assert_records_ir_call(function()
    local big = 1099511627776
    local text = "1099511627776"
    local n = 0
    for _ = 1, 80 do
      if tostring(big) == text then n = n + 1 end
      if "id:" .. big == "id:" .. text then n = n + 1 end
      if string.sub(big, 1, 4) == "1099" then n = n + 1 end
      if string.format("%s", big) == text then n = n + 1 end
      if string.format("%d", big) == text then n = n + 1 end
      if string.format("%x", big) == "10000000000" then n = n + 1 end
    end
    assert(n == 480)
  end, "Lua 5.4 boxed int64 string conversion", "lj_strfmt_i64")
end

do
  local function random_mix()
    local range_sum = 0
    local full_count = 0
    local float_count = 0
    for _ = 1, 80 do
      range_sum = range_sum + math.random(1, 4)
      if math.type(math.random(0)) == "integer" then full_count = full_count + 1 end
      local f = math.random()
      if f >= 0 and f < 1 then float_count = float_count + 1 end
    end
    return range_sum, full_count, float_count
  end
  jitmod.off()
  math.randomseed(0x24681357, 0x13572468)
  local expect_sum, expect_full, expect_float = random_mix()

  assert_records_ir_op(function()
    math.randomseed(0x24681357, 0x13572468)
    local got_sum, got_full, got_float = random_mix()
    -- The random calls themselves should be in the trace. Recording only the
    -- surrounding loop still pays the C helper dispatch cost on every sample.
    assert(got_sum == expect_sum)
    assert(got_full == expect_full and got_float == expect_float)
  end, "Lua 5.4 math.random recorder", "CALLS")

  jitmod.off()
  local seed1, seed2 = math.randomseed(1099511627776, "1")
  assert(seed1 == 1099511627776 and seed2 == 1)
  assert(math.random(0) == -7928649372492011025)
  assert(math.random(0) == 2966187919354626699)

  assert_records_ir_calls(function()
    math.randomseed(1099511627776, "1")
    local n, hi_pos, hi_neg, very_wide = 0, 0, 0, 0
    for _ = 1, 80 do
      local v = math.random(0)
      if math.type(v) == "integer" then n = n + 1 end
      if v > 9007199254740992 then hi_pos = hi_pos + 1 end
      if v < -9007199254740992 then hi_neg = hi_neg + 1 end
      if v > 4611686018427387904 or v < -4611686018427387904 then
	very_wide = very_wide + 1
      end
    end
    assert(n == 80)
    assert(hi_pos > 0 and hi_neg > 0 and very_wide > 0)
  end, "Lua 5.4 math.randomseed int64 seeds",
  { "lj_prng_i64_random54", "lj_obj_newint64" })
end

do
  local function random_dynamic_mix(offset)
    local sum = 0
    for i = 1, 80 do
      local low = i + offset
      local up = low + 3
      local v = math.random(low, up)
      if v >= low and v <= up then sum = sum + v end

      local single_up = i + 1
      local w = math.random(single_up)
      if w >= 1 and w <= single_up then sum = sum + w end
    end
    return sum
  end

  jitmod.off()
  math.randomseed(0x6a09e667, 0xbb67ae85)
  local expect_sum = random_dynamic_mix(1)

  assert_records_ir_op(function()
    math.randomseed(0x6a09e667, 0xbb67ae85)
    local got_sum = random_dynamic_mix(1)
    -- Dynamic integer intervals should guard before consuming PRNG state, then
    -- use the same xoshiro helper as constant intervals once the guard passes.
    assert(got_sum == expect_sum)
  end, "Lua 5.4 math.random dynamic interval recorder", "CALLS")
end

do
  local function random_string_interval_mix(low_s, up_s, single_s)
    local sum = 0
    for _ = 1, 80 do
      local v = math.random(low_s, up_s)
      if v >= 2 and v <= 5 then sum = sum + v end

      local w = math.random(single_s)
      if w >= 1 and w <= 7 then sum = sum + w end
    end
    return sum
  end

  jitmod.off()
  math.randomseed(0x31415926, 0x27182818)
  local expect_sum = random_string_interval_mix("2", "5", "7")

  assert_records_ir_op(function()
    math.randomseed(0x31415926, 0x27182818)
    local got_sum = random_string_interval_mix("2", "5", "7")
    -- Lua 5.4 math.random uses luaL_checkinteger() for interval arguments, so
    -- dynamic string integers must guard before the xoshiro helper mutates PRNG.
    assert(got_sum == expect_sum)
  end, "Lua 5.4 math.random string interval recorder", "CALLS")
end

do
  local wide_low = 1099511627776
  local wide_up = wide_low + 3

  local function random_wide_interval_mix(low, up, low_s, up_s, single_s)
    local sum = 0
    for _ = 1, 80 do
      local v = math.random(low, up)
      if math.type(v) == "integer" and v >= wide_low and v <= wide_up then
	sum = sum + (v - wide_low)
      end

      local w = math.random(low_s, up_s)
      if math.type(w) == "integer" and w >= wide_low and w <= wide_up then
	sum = sum + (w - wide_low)
      end

      local single = math.random(single_s)
      if math.type(single) == "integer" and single >= 1 and single <= wide_up then
	sum = sum + (single % 17)
      end
    end
    return sum
  end

  jitmod.off()
  math.randomseed(0x12345678, 0x2468ace0)
  local expect_sum = random_wide_interval_mix(wide_low, wide_up,
					      tostring(wide_low),
					      tostring(wide_up),
					      tostring(wide_up))

  assert_records_ir_calls(function()
    math.randomseed(0x12345678, 0x2468ace0)
    local got_sum = random_wide_interval_mix(wide_low, wide_up,
					     tostring(wide_low),
					     tostring(wide_up),
					     tostring(wide_up))
    assert(got_sum == expect_sum)
  end, "Lua 5.4 math.random wide interval recorder",
  { "lj_prng_i64_random54", "lj_obj_newint64" })
end

do
  local function random_sequence()
    local out = {}
    for i = 1, 120 do
      out[i] = math.random(1, 4)
    end
    return out
  end
  local function assert_same_sequence(got, want)
    assert(#got == #want)
    for i = 1, #want do
      assert(got[i] == want[i])
    end
  end

  jitmod.off()
  math.randomseed(0x12345678, 0x13579bdf)
  local expected = random_sequence()

  jitmod.flush()
  jitmod.on()
  jit.opt.start("hotloop=1", "hotexit=1")
  local before = trace_highwater()
  math.randomseed(0x12345678, 0x13579bdf)
  local traced = random_sequence()
  assert(trace_highwater() > before, "Lua 5.4 math.random did not record a loop trace")
  -- The loop may trace around the C helper, but it must not revive LuaJIT's old
  -- random recorder/PRNG.  The same seed must produce the interpreter sequence.
  assert_same_sequence(traced, expected)

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok, err = pcall(math.random, 10, 5)
      if not ok and err:find("interval is empty", 1, true) then n = n + 1 end
    end
    assert(n == 80)
  end, "Lua 5.4 math.random error")
  collectgarbage("collect")
end

do
  local mt = {
    __lt = function(a, b)
      return a.v < b.v
    end
  }
  local a = setmetatable({ v = 1 }, mt)
  local b = setmetatable({ v = 2 }, mt)
  local function min_loop(want)
    local n = 0
    for _ = 1, 80 do
      if math.min(a, b) == want then n = n + 1 end
    end
    return n
  end
  assert_records_trace(function()
    assert(min_loop(a) == 80)
  end, "Lua 5.4 math.min metamethod")
  mt.__lt = function(x, y)
    return x.v > y.v
  end
  assert(min_loop(b) == 80)

  assert_records_trace(function()
    local n = 0
    for i = 1, 80 do
      if math.type(i) == "integer" and math.type(i + 0.0) == "float" and
	 math.type("1") == nil then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 math.type")

  assert_records_ir_op(function()
    local function min_loop(a, b, c)
      local n = 0
      for _ = 1, 80 do
	local x = math.min(a, b, c)
	if x == 1099511627776 and math.type(x) == "integer" then
	  n = n + 1
	end
      end
      return n
    end
    assert(min_loop(1099511627778, 1099511627776, 1099511627777) == 80)
  end, "Lua 5.4 int64 math.min", "MIN")

  assert_records_ir_op(function()
    local function max_loop(a, b, c)
      local n = 0
      for _ = 1, 80 do
	local x = math.max(a, b, c)
	if x == 1099511627778 and math.type(x) == "integer" then
	  n = n + 1
	end
      end
      return n
    end
    assert(max_loop(1099511627776, 1099511627778, 1099511627777) == 80)
  end, "Lua 5.4 int64 math.max", "MAX")

  assert_records_ir_op(function()
    local n = 0
    for _ = 1, 80 do
      if math.abs(-1099511627776) == 1099511627776 then n = n + 1 end
    end
    assert(n == 80)
  end, "Lua 5.4 boxed int64 math.abs", "NEG")

  assert_records_ir_op(function()
    local function sqrt_log_loop(a)
      local n = 0
      local expected_log = math.log(a, 2)
      for _ = 1, 80 do
	local r = math.sqrt(a)
	local l = math.log(a, 2)
	if r == 1048576.0 and l == expected_log and
	   math.type(r) == "float" and math.type(l) == "float" then
	  n = n + 1
	end
      end
      return n
    end
    assert(sqrt_log_loop(1099511627776) == 80)
  end, "Lua 5.4 boxed int64 math.sqrt/log", "FPMATH")

  assert_records_ir_op(function()
    local function sin_loop(a)
      local n = 0
      local expected = math.sin(a)
      for _ = 1, 80 do
	local r = math.sin(a)
	if r == expected and math.type(r) == "float" then n = n + 1 end
      end
      return n
    end
    assert(sin_loop(1099511627776) == 80)
  end, "Lua 5.4 boxed int64 math.call", "CALLN")

  assert_records_ir_calls(function()
    local function sqrt_loop(s)
      local n = 0
      for _ = 1, 80 do
	local r = math.sqrt(s)
	if r == 1048576.0 and math.type(r) == "float" then n = n + 1 end
      end
      return n
    end
    assert(sqrt_loop("1099511627776") == 80)
  end, "Lua 5.4 string int64 math.sqrt",
  { "lj_strscan_numtype54s", "lj_strscan_tonum54s" })

  assert_records_ir_op(function()
    local function log_loop(a)
      local n = 0
      local expected = math.log(a)
      for _ = 1, 80 do
	if math.log(a, nil) == expected then n = n + 1 end
      end
      return n
    end
    assert(log_loop(100.0) == 80)
  end, "Lua 5.4 math.log nil base", "FPMATH")

  assert_records_ir_call(function()
    local function atan_loop(a, x)
      local n = 0
      local expected = math.atan(a, x)
      for _ = 1, 80 do
	local r = math.atan(a, x)
	if r == expected and math.type(r) == "float" then n = n + 1 end
      end
      return n
    end
    assert(atan_loop(1099511627776, "2") == 80)
  end, "Lua 5.4 boxed int64 math.atan2", "atan2")

  assert_records_ir_call(function()
    local function atan_nil_loop(a)
      local n = 0
      local expected = math.atan(a)
      for _ = 1, 80 do
	local r = math.atan(a, nil)
	if r == expected and math.type(r) == "float" then n = n + 1 end
      end
      return n
    end
    assert(atan_nil_loop(100.0) == 80)
  end, "Lua 5.4 math.atan nil base", "atan")

  assert_records_ir_op(function()
    local function round_loop(a, b)
      local n = 0
      for _ = 1, 80 do
	local x = math.floor(a)
	local y = math.ceil(b)
	if x == 1099511627776 and y == 1099511627776 and
	   math.type(x) == "integer" and math.type(y) == "integer" then
	  n = n + 1
	end
      end
      return n
    end
    assert(round_loop(1099511627776.5, 1099511627775.5) == 80)
  end, "Lua 5.4 int64 math.floor/ceil", "FPMATH")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local x = math.floor("1e20")
      if x == 1e20 and math.type(x) == "float" then n = n + 1 end
    end
    assert(n == 80)
  end, "Lua 5.4 non-integer math.floor string")

  assert_records_ir_op(function()
    local function modf_loop(a, b)
      local n = 0
      for _ = 1, 80 do
	local i, f = math.modf(a)
	local wi, wf = math.modf(b)
	if i == 1099511627776 and f == 0.5 and
	   math.type(i) == "integer" and math.type(f) == "float" and
	   wi == 1099511627776 and wf == 0.0 and
	   math.type(wi) == "integer" and math.type(wf) == "float" then
	  n = n + 1
	end
      end
      return n
    end
    assert(modf_loop(1099511627776.5, 1099511627776) == 80)
  end, "Lua 5.4 int64 math.modf", "FPMATH")

  assert_records_trace(function()
    local x = 0
    for _ = 1, 80 do
      x = x + 2147483648
    end
    assert(x == 171798691840 and math.type(x) == "integer")
  end, "Lua 5.4 boxed int64 addition")

  assert_records_trace(function()
    local x = 2147483647
    for _ = 1, 80 do
      x = x + 1
    end
    assert(x == 2147483727 and math.type(x) == "integer")
  end, "Lua 5.4 int32 boundary addition")

  assert_records_trace(function()
    local x = 2147483648
    for _ = 1, 8 do
      x = x * 2
    end
    assert(x == 549755813888 and math.type(x) == "integer")
  end, "Lua 5.4 boxed int64 multiplication")

  assert_records_trace(function()
    local x = 1099511627776
    for _ = 1, 80 do
      x = -x
    end
    assert(x == 1099511627776 and math.type(x) == "integer")
  end, "Lua 5.4 boxed int64 unary minus")

  assert_records_ir_op(function()
    local function pow_loop(a, b)
      local n = 0
      for _ = 1, 80 do
	local x = a ^ b
	if x == 1099511627776.0 and math.type(x) == "float" then
	  n = n + 1
	end
      end
      return n
    end
    assert(pow_loop(1099511627776, 1) == 80)
  end, "Lua 5.4 boxed int64 power", "POW")

  assert_records_ir_op(function()
    local function pow_loop(a, b, c)
      local n = 0
      for _ = 1, 80 do
	local x = a ^ b
	local y = c ^ 1
	if x == 1099511627776.0 and y == 1099511627776.0 and
	   math.type(x) == "float" and math.type(y) == "float" then
	  n = n + 1
	end
      end
      return n
    end
    assert(pow_loop(1099511627776, "1", "1099511627776") == 80)
  end, "Lua 5.4 mixed int64 string power", "POW")

  assert_records_trace(function()
    local x = 0
    for _ = 1, 80 do
      x = 9223372036854775807 + 1
    end
    assert(x == math.mininteger and math.type(x) == "integer")
  end, "Lua 5.4 int64 constant add wrap")

  assert_records_trace(function()
    local x = 0
    for _ = 1, 80 do
      x = 3037000499 * 3037000499
    end
    assert(x == 9223372030926249001 and math.type(x) == "integer")
  end, "Lua 5.4 int64 constant mul fold")

  assert_records_ir_call(function()
    local wide = "9007199254740993"
    local x = 0
    for _ = 1, 80 do
      x = x + (wide + 0) % 10
    end
    assert(x == 240 and math.type(x) == "integer")
  end, "Lua 5.4 string int64 addition", "lj_strscan_toint6454")

  assert_records_ir_call(function()
    local wide = "9007199254740993"
    local x = 0
    for _ = 1, 80 do
      x = x + (-wide + 9007199254740993)
    end
    assert(x == 0 and math.type(x) == "integer")
  end, "Lua 5.4 string int64 unary minus", "lj_strscan_toint6454")

  assert_records_trace(function()
    local function order_loop(a, b)
      local n = 0
      for _ = 1, 80 do
	if a < b then n = n + 1 end
	if b > a then n = n + 1 end
	if a <= a then n = n + 1 end
	if b >= a then n = n + 1 end
      end
      return n
    end
    assert(order_loop(2147483648, 2147483649) == 320)
  end, "Lua 5.4 boxed int64 ordered comparison")

  assert_records_trace(function()
    local function eq_loop(a, b)
      local n = 0
      for _ = 1, 80 do
	if a == b then n = n + 1 end
	if a ~= b + 1 then n = n + 1 end
      end
      return n
    end
    local a = assert(tonumber("2147483648"))
    local b = assert(tonumber("2147483648"))
    assert(a == b and math.type(a) == "integer" and math.type(b) == "integer")
    assert(eq_loop(a, b) == 160)
  end, "Lua 5.4 boxed int64 equality")

  assert_records_trace(function()
    local function eq_loop(a, b)
      local n = 0
      for _ = 1, 80 do
	if a == b then n = n + 1 end
      end
      return n
    end
    local a = assert(tonumber("9007199254740993"))
    local b = 9007199254740992.0
    assert(math.type(a) == "integer" and math.type(b) == "float")
    assert(eq_loop(a, b) == 0)
    assert(eq_loop(assert(tonumber("9007199254740992")), b) == 80)
  end, "Lua 5.4 mixed int64/float equality")

  assert_records_trace(function()
    local function order_loop(a, b, c, nan)
      local n = 0
      for _ = 1, 80 do
	if a < b then n = n + 1 end
	if b > a then n = n + 1 end
	if a <= b then n = n + 1 end
	if not (a < c) then n = n + 1 end
	if not (a <= nan) then n = n + 1 end
      end
      return n
    end
    local a = assert(tonumber("9007199254740993"))
    local b = 9007199254740994.0
    local c = 9007199254740992.0
    local nan = 0 / 0
    assert(math.type(a) == "integer" and math.type(b) == "float")
    assert(order_loop(a, b, c, nan) == 400)
  end, "Lua 5.4 mixed int64/float ordered comparison")

  assert_records_ir_op(function()
    local x = 0
    for _ = 1, 80 do
      x = (x | 1099511627776) & 1099511628031
    end
    assert(x == 1099511627776 and math.type(x) == "integer")
  end, "Lua 5.4 boxed int64 bitwise and/or", "BAND")

  assert_records_ir_op(function()
    local a = 1099511627776.0
    local b = 1099511628031.0
    local x = 0
    for _ = 1, 80 do
      x = a & b
    end
    assert(x == 1099511627776 and math.type(x) == "integer")
  end, "Lua 5.4 float int64 bitwise coercion", "BAND")

  assert_no_ir_op(function()
    local a = "1099511627776"
    local b = "1099511628031"
    local function bad_bitwise()
      return a & b
    end
    local n = 0
    for _ = 1, 80 do
      local ok, err = pcall(bad_bitwise)
      if not ok and err:find("bitwise operation", 1, true) and
	 err:find("string value", 1, true) then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 string int64 bitwise rejects coercion", "BAND")

  assert_records_ir_op(function()
    local a = 1099511627776
    local b = 1099511628031
    local x = 0
    for _ = 1, 80 do
      x = a ~ b
    end
    assert(x == 255 and math.type(x) == "integer")
  end, "Lua 5.4 boxed int64 bitwise xor", "BXOR")

  assert_records_ir_op(function()
    local x = 1
    for _ = 1, 40 do
      x = x << 1
    end
    assert(x == 1099511627776 and math.type(x) == "integer")
  end, "Lua 5.4 boxed int64 shift left", "BSHL")

  assert_records_ir_op(function()
    local a = 1.0
    local sh = 40.0
    local x = 0
    for _ = 1, 80 do
      x = a << sh
    end
    assert(x == 1099511627776 and math.type(x) == "integer")
  end, "Lua 5.4 float int64 shift coercion", "BSHL")

  assert_no_ir_op(function()
    local a = "1.0"
    local sh = "40"
    local function bad_shift()
      return a << sh
    end
    local n = 0
    for _ = 1, 80 do
      local ok, err = pcall(bad_shift)
      if not ok and err:find("bitwise operation", 1, true) and
	 err:find("string value", 1, true) then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 string int64 shift rejects coercion", "BSHL")

  assert_records_ir_op(function()
    local a = 2199023255552
    local x = 0
    for _ = 1, 80 do
      x = a >> 1
    end
    assert(x == 1099511627776 and math.type(x) == "integer")
  end, "Lua 5.4 boxed int64 shift right", "BSHR")

  assert_records_trace(function()
    local function shift_loop(a, b, c)
      local n = 0
      for _ = 1, 80 do
	if (a >> b) == 1099511627776 then n = n + 1 end
	if (a << c) == 1099511627776 then n = n + 1 end
	if (a << 64) == 0 then n = n + 1 end
      end
      return n
    end
    assert(shift_loop(2199023255552, 1, -1) == 240)
  end, "Lua 5.4 boxed int64 shift right/negative/out-of-range")

  assert_records_ir_op(function()
    local function edge_loop(one, negone, zero, sh)
      local n = 0
      for _ = 1, 80 do
	if (one << sh) == math.mininteger then n = n + 1 end
	if ((one << sh) >> sh) == 1 then n = n + 1 end
	if (negone >> 1) == math.maxinteger then n = n + 1 end
	if (negone << sh) == math.mininteger then n = n + 1 end
	if (~zero) == -1 then n = n + 1 end
      end
      return n
    end
    assert(edge_loop(1, -1, 0, 63) == 400)
  end, "Lua 5.4 full-width int64 bitwise edges", "BSHR")

  assert_records_ir_op(function()
    local a = 1099511627776
    local x = 0
    for _ = 1, 80 do
      x = ~a
    end
    assert(x == -1099511627777 and math.type(x) == "integer")
  end, "Lua 5.4 boxed int64 bitwise not", "BNOT")

  assert_records_ir_call(function()
    local a = -1099511627777
    local b = 3
    local x = 0
    for _ = 1, 80 do
      x = a // b
    end
    assert(x == -366503875926 and math.type(x) == "integer")
  end, "Lua 5.4 boxed int64 floor division", "lj_obj_i64idiv")

  assert_records_ir_call(function()
    local a = -1099511627777
    local b = 3
    local x = 0
    for _ = 1, 80 do
      x = a % b
    end
    assert(x == 1 and math.type(x) == "integer")
  end, "Lua 5.4 boxed int64 modulo", "lj_obj_i64mod")

  assert_records_ir_calls(function()
    local a = "9007199254740993"
    local b = "3"
    local x = 0
    for _ = 1, 80 do
      x = a // b
    end
    assert(x == 3002399751580331 and math.type(x) == "integer")
  end, "Lua 5.4 string int64 floor division",
  { "lj_strscan_toint6454", "lj_obj_i64idiv" })

  assert_records_ir_calls(function()
    local a = "9007199254740993"
    local b = "10"
    local x = 0
    for _ = 1, 80 do
      x = a % b
    end
    assert(x == 3 and math.type(x) == "integer")
  end, "Lua 5.4 string int64 modulo",
  { "lj_strscan_toint6454", "lj_obj_i64mod" })

  assert_records_ir_op(function()
    local a = -1099511627777
    local b = 3
    local x = 0
    for _ = 1, 80 do
      x = x + math.fmod(a, b)
      x = x + math.fmod(math.mininteger, -1)
    end
    assert(x == -160 and math.type(math.fmod(a, b)) == "integer")
  end, "Lua 5.4 boxed int64 math.fmod", "MOD")

  assert_records_trace(function()
    local n = 0
    for i = "1", "80" do
      if math.type(i) == "float" then n = n + i end
    end
    assert(n == 3240.0 and math.type(n) == "float")
  end, "Lua 5.4 numeric for string init")

  assert_records_trace(function()
    local n = 0
    for i = 1, 80, "1" do
      if math.type(i) == "float" then n = n + i end
    end
    assert(n == 3240.0 and math.type(n) == "float")
  end, "Lua 5.4 numeric for string step")

  assert_records_trace(function()
    local n, last, tlast = 0
    for i = 1099511627776.0, 1099511627855, 1 do
      n = n + 1
      last = i
      tlast = math.type(i)
    end
    assert(n == 80 and last == 1099511627855.0 and tlast == "float")
  end, "Lua 5.4 numeric for float init int64 range")

  assert_records_trace(function()
    local n, last, tlast = 0
    for i = 1099511627776, 1099511627855, 1.0 do
      n = n + 1
      last = i
      tlast = math.type(i)
    end
    assert(n == 80 and last == 1099511627855.0 and tlast == "float")
  end, "Lua 5.4 numeric for float step int64 range")

  assert_records_ir_call(function()
    local n, last = 0
    for i = 1099511627776, 1099511627855 do
      n = n + 1
      last = i
    end
    assert(n == 80 and last == 1099511627855 and
	   math.type(last) == "integer")
    collectgarbage()
  end, "Lua 5.4 boxed int64 numeric for trace", "lj_obj_newint64")

  assert_records_ir_call(function()
    local n, last = 0
    for i = 1099511627855, 1099511627776, -1 do
      n = n + 1
      last = i
    end
    assert(n == 80 and last == 1099511627776 and
	   math.type(last) == "integer")
    collectgarbage()
  end, "Lua 5.4 boxed int64 numeric for negative trace", "lj_obj_newint64")

  assert_records_ir_call(function()
    local n, last, tlast = 0
    for i = 2147483646, 2147483653 do
      n = n + 1
      last = i
      tlast = math.type(i)
    end
    assert(n == 8 and last == 2147483653 and tlast == "integer")
    collectgarbage()
  end, "Lua 5.4 boxed int64 numeric for boundary trace", "lj_obj_newint64")

  assert_records_ir_call(function()
    local n, last, tlast = 0
    for i = 2147483646, 2147483653.0 do
      n = n + 1
      last = i
      tlast = math.type(i)
    end
    assert(n == 8 and last == 2147483653 and tlast == "integer")
    collectgarbage()
  end, "Lua 5.4 boxed int64 numeric for float limit trace", "lj_obj_newint64")

  do
    local function boundary_churn(n)
      local sum = 0
      for _ = 1, n do
	local count, last, tlast = 0
	for i = 2147483646, 2147483653 do
	  count = count + 1
	  last = i
	  tlast = math.type(i)
	end
	if count == 8 and last == 2147483653 and tlast == "integer" then
	  sum = sum + 1
	end
	count, last, tlast = 0
	for i = 2147483646, 2147483653.0 do
	  count = count + 1
	  last = i
	  tlast = math.type(i)
	end
	if count == 8 and last == 2147483653 and tlast == "integer" then
	  sum = sum + 1
	end
      end
      return sum
    end
    jitmod.off()
    jitmod.flush()
    collectgarbage()
    jitmod.on()
    jit.opt.start("0", "hotloop=3", "hotexit=2")
    assert(boundary_churn(20) == 40,
      "Lua 5.4 int32-to-boxed-int64 numeric for must survive opt0 recording")
  end

  assert_records_trace(function()
    local n = 0
    local wide = "1099511627776"
    local max = "9223372036854775807"
    local over = "9223372036854775808"
    for _ = 1, 80 do
      n = n + assert(math.tointeger("123"))
      local w = assert(math.tointeger(wide))
      local m = assert(math.tointeger(max))
      if w == 1099511627776 and m == math.maxinteger and
	 math.type(w) == "integer" and math.type(m) == "integer" then
	n = n + 1
      end
      assert(math.tointeger(over) == nil)
      assert(math.tointeger(1.5) == nil)
    end
    assert(n == 9920)
  end, "Lua 5.4 math.tointeger")

  assert_records_ir_op(function()
    local function ult_loop(a, b, c)
      local n = 0
      for _ = 1, 80 do
	if math.ult(a, b) and math.ult(c, -1) and not math.ult(-1, c) then
	  n = n + 1
	end
      end
      return n
    end
    assert(ult_loop(1099511627776, "1099511627777",
		    "9223372036854775807") == 80)
  end, "Lua 5.4 int64 math.ult", "ULT")

  assert_records_trace(function()
    local n = 0
    local function set_global_abs_alias()
      lua54_jit_global_abs_alias = math.abs
    end
    local lua54_jit_table_abs_alias = {}
    local function set_table_abs_alias()
      lua54_jit_table_abs_alias.f = math.abs
    end
    local lua54_jit_table_global_source = {}
    set_global_abs_alias()
    set_table_abs_alias()
    lua54_jit_table_global_alias = lua54_jit_table_global_source
    lua54_jit_table_global_alias.h = math.abs
    lua54_jit_table_abs_alias.j = math.abs
    lua54_jit_table_global_alias2 = lua54_jit_table_abs_alias
    local lua54_jit_table_field_source = {}
    local lua54_jit_table_field_holder = {}
    lua54_jit_table_field_holder.inner = lua54_jit_table_field_source
    lua54_jit_table_field_holder.inner.k = math.abs
    local lua54_jit_table_ctor_source = {}
    local lua54_jit_table_ctor_holder = {
      inner = lua54_jit_table_ctor_source,
    }
    lua54_jit_table_ctor_holder.inner.l = math.abs
    local lua54_jit_table_rhs_source = {}
    local lua54_jit_table_rhs_base = {}
    lua54_jit_table_rhs_base.inner = lua54_jit_table_rhs_source
    local lua54_jit_table_rhs_ctor_holder = {
      alias = lua54_jit_table_rhs_base.inner,
    }
    lua54_jit_table_rhs_ctor_holder.alias.m = math.abs
    local lua54_jit_table_rhs_assign_holder = {}
    lua54_jit_table_rhs_assign_holder.alias = lua54_jit_table_rhs_base.inner
    lua54_jit_table_rhs_assign_holder.alias.n = math.abs
    local lua54_jit_concat_ctor_key = "u" .. "2"
    local lua54_jit_concat_ctor_holder = {
      [lua54_jit_concat_ctor_key] = math.abs,
    }
    local lua54_jit_template_key_source = { name = "u3" }
    local lua54_jit_template_key = lua54_jit_template_key_source.name
    local lua54_jit_template_key_holder = {
      [lua54_jit_template_key] = math.abs,
    }
    local lua54_jit_concat_assign_holder = {}
    local lua54_jit_concat_assign_key = "w" .. "2"
    lua54_jit_concat_assign_holder[lua54_jit_concat_assign_key] = math.abs
    local lua54_jit_nested_concat_key_holder = { inner = {} }
    local lua54_jit_nested_concat_key = "x" .. "2"
    lua54_jit_nested_concat_key_holder.inner[lua54_jit_nested_concat_key] =
      math.abs
    local function lua54_jit_runtime_ctor_key()
      return "u4"
    end
    local lua54_jit_runtime_ctor_key_holder = {
      [lua54_jit_runtime_ctor_key()] = math.abs,
    }
    local function lua54_jit_runtime_assign_key()
      return "w4"
    end
    local lua54_jit_runtime_assign_key_holder = {}
    lua54_jit_runtime_assign_key_holder[lua54_jit_runtime_assign_key()] =
      math.abs
    local function lua54_jit_runtime_nested_key()
      return "x4"
    end
    local lua54_jit_runtime_nested_key_holder = { inner = {} }
    lua54_jit_runtime_nested_key_holder.inner[lua54_jit_runtime_nested_key()] =
      math.abs
    local function lua54_jit_runtime_global_key()
      return "y4"
    end
    lua54_jit_runtime_global_key_holder = {}
    lua54_jit_runtime_global_key_holder[lua54_jit_runtime_global_key()] =
      math.abs
    for _ = 1, 80 do
      local ok_fmod_missing, err_fmod_missing = pcall(math.fmod)
      local ok_fmod_nil, err_fmod_nil = pcall(function()
	return math.fmod(nil)
      end)
      local ok_fmod_nil_present, err_fmod_nil_present =
	pcall(math.fmod, nil, 1)
      local ok_global_abs, err_global_abs = pcall(function()
	return lua54_jit_global_abs_alias(true)
      end)
      local ok_table_abs, err_table_abs = pcall(function()
	return lua54_jit_table_abs_alias.f(true)
      end)
      local ok_global_table_write, err_global_table_write = pcall(function()
	return lua54_jit_table_global_source.h(true)
      end)
      local ok_global_table_read, err_global_table_read = pcall(function()
	return lua54_jit_table_global_alias2.j(true)
      end)
      local ok_table_field_source, err_table_field_source = pcall(function()
	return lua54_jit_table_field_source.k(true)
      end)
      local ok_table_field_read, err_table_field_read = pcall(function()
	return lua54_jit_table_field_holder.inner.k(true)
      end)
      local ok_table_ctor_source, err_table_ctor_source = pcall(function()
	return lua54_jit_table_ctor_source.l(true)
      end)
      local ok_table_ctor_read, err_table_ctor_read = pcall(function()
	return lua54_jit_table_ctor_holder.inner.l(true)
      end)
      local ok_table_rhs_ctor_source, err_table_rhs_ctor_source =
	pcall(function()
	  return lua54_jit_table_rhs_source.m(true)
	end)
      local ok_table_rhs_ctor_read, err_table_rhs_ctor_read = pcall(function()
	return lua54_jit_table_rhs_ctor_holder.alias.m(true)
      end)
      local ok_table_rhs_assign_source, err_table_rhs_assign_source =
	pcall(function()
	  return lua54_jit_table_rhs_source.n(true)
	end)
      local ok_table_rhs_assign_read, err_table_rhs_assign_read =
	pcall(function()
	  return lua54_jit_table_rhs_assign_holder.alias.n(true)
	end)
      local ok_concat_ctor_key, err_concat_ctor_key = pcall(function()
	return lua54_jit_concat_ctor_holder.u2(true)
      end)
      local ok_template_key, err_template_key = pcall(function()
	return lua54_jit_template_key_holder.u3(true)
      end)
      local ok_concat_assign_key, err_concat_assign_key = pcall(function()
	return lua54_jit_concat_assign_holder.w2(true)
      end)
      local ok_nested_concat_key, err_nested_concat_key = pcall(function()
	return lua54_jit_nested_concat_key_holder.inner.x2(true)
      end)
      local ok_runtime_ctor_key, err_runtime_ctor_key = pcall(function()
	return lua54_jit_runtime_ctor_key_holder.u4(true)
      end)
      local ok_runtime_assign_key, err_runtime_assign_key = pcall(function()
	return lua54_jit_runtime_assign_key_holder.w4(true)
      end)
      local ok_runtime_nested_key, err_runtime_nested_key = pcall(function()
	return lua54_jit_runtime_nested_key_holder.inner.x4(true)
      end)
      local ok_runtime_global_key, err_runtime_global_key = pcall(function()
	return lua54_jit_runtime_global_key_holder.y4(true)
      end)
      if not ok_fmod_missing and
	 err_fmod_missing:find("bad argument #2 to 'math.fmod'", 1, true) and
	 not ok_fmod_nil and
	 err_fmod_nil:find("bad argument #2 to 'fmod'", 1, true) and
	 not ok_fmod_nil_present and
	 err_fmod_nil_present:find("bad argument #1 to 'math.fmod'", 1, true) then
	n = n + 1
      end
      if not ok_global_abs and
	 err_global_abs:find("bad argument #1 to 'lua54_jit_global_abs_alias'",
			     1, true) then
	n = n + 1
      end
      if not ok_table_abs and
	 err_table_abs:find("bad argument #1 to 'f'", 1, true) then
	n = n + 1
      end
      if not ok_global_table_write and
	 err_global_table_write:find("bad argument #1 to 'h'", 1, true) then
	n = n + 1
      end
      if not ok_global_table_read and
	 err_global_table_read:find("bad argument #1 to 'j'", 1, true) then
	n = n + 1
      end
      if not ok_table_field_source and
	 err_table_field_source:find("bad argument #1 to 'k'", 1, true) then
	n = n + 1
      end
      if not ok_table_field_read and
	 err_table_field_read:find("bad argument #1 to 'k'", 1, true) then
	n = n + 1
      end
      if not ok_table_ctor_source and
	 err_table_ctor_source:find("bad argument #1 to 'l'", 1, true) then
	n = n + 1
      end
      if not ok_table_ctor_read and
	 err_table_ctor_read:find("bad argument #1 to 'l'", 1, true) then
	n = n + 1
      end
      if not ok_table_rhs_ctor_source and
	 err_table_rhs_ctor_source:find("bad argument #1 to 'm'", 1, true) then
	n = n + 1
      end
      if not ok_table_rhs_ctor_read and
	 err_table_rhs_ctor_read:find("bad argument #1 to 'm'", 1, true) then
	n = n + 1
      end
      if not ok_table_rhs_assign_source and
	 err_table_rhs_assign_source:find("bad argument #1 to 'n'", 1, true) then
	n = n + 1
      end
      if not ok_table_rhs_assign_read and
	 err_table_rhs_assign_read:find("bad argument #1 to 'n'", 1, true) then
	n = n + 1
      end
      if not ok_concat_ctor_key and
	 err_concat_ctor_key:find("bad argument #1 to 'u2'", 1, true) then
	n = n + 1
      end
      if not ok_template_key and
	 err_template_key:find("bad argument #1 to 'u3'", 1, true) then
	n = n + 1
      end
      if not ok_concat_assign_key and
	 err_concat_assign_key:find("bad argument #1 to 'w2'", 1, true) then
	n = n + 1
      end
      if not ok_nested_concat_key and
	 err_nested_concat_key:find("bad argument #1 to 'x2'", 1, true) then
	n = n + 1
      end
      if not ok_runtime_ctor_key and
	 err_runtime_ctor_key:find("bad argument #1 to 'u4'", 1, true) then
	n = n + 1
      end
      if not ok_runtime_assign_key and
	 err_runtime_assign_key:find("bad argument #1 to 'w4'", 1, true) then
	n = n + 1
      end
      if not ok_runtime_nested_key and
	 err_runtime_nested_key:find("bad argument #1 to 'x4'", 1, true) then
	n = n + 1
      end
      if not ok_runtime_global_key and
	 err_runtime_global_key:find("bad argument #1 to 'y4'", 1, true) then
	n = n + 1
      end
      if math.ult(-1, 1) == false and math.ult(1, -1) == true then
	n = n + 1
      end
    end
    lua54_jit_global_abs_alias = nil
    lua54_jit_table_global_alias = nil
    lua54_jit_table_global_alias2 = nil
    lua54_jit_runtime_global_key_holder = nil
    assert(n == 1760)
  end, "Lua 5.4 math.ult")
end

do
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      -- A present base argument selects Lua 5.4 integer-base conversion, even
      -- for base 10. It must not fall back to decimal STRTO tracing.
      if tonumber("\t10000000000\t", 10) == 10000000000 then n = n + 1 end
      if tonumber("1.0", 10) == nil then n = n + 1 end
      if tonumber("1\0", 2) == nil then n = n + 1 end
      if tonumber("0x10", 16) == nil then n = n + 1 end
      if tonumber("0x10", 34) == 38182 then n = n + 1 end
    end
    assert(n == 400)
  end, "Lua 5.4 tonumber explicit base")

  assert_records_ir_call(function()
    local n = 0
    for _ = 1, 3 do
      for i = 2, 36 do
	local i2 = i * i
	local i10 = i2 * i2 * i2 * i2 * i2
	local v = tonumber("\t10000000000\t", i)
	if v == i10 and math.type(v) == "integer" then n = n + 1 end
      end
    end
    assert(n == 105)
  end, "Lua 5.4 tonumber dynamic base 64-bit", "lj_strscan_tobaseintvalue54")
end

do
  local function tonumber_no_base_mix(float_input, int_input, bad_input)
    local n = 0
    for _ = 1, 80 do
      local f = tonumber(float_input)
      local iv = tonumber(int_input)
      local direct64 = tonumber(1099511627776)
      local bad = tonumber(bad_input)
      -- This is the no-base tonumber() recorder surface: floats must record
      -- through STRTO, integers must keep the Lua 5.4 integer subtype, and
      -- rejected LuaJIT-only numerals must still return nil in the hot loop.
      if math.type(f) == "float" then n = n + 1 end
      if math.type(iv) == "integer" then n = n + 1 end
      if direct64 == 1099511627776 and math.type(direct64) == "integer" then
	n = n + 1
      end
      if bad == nil then n = n + 1 end
    end
    return n
  end

  assert_records_ir_op(function()
    assert(tonumber_no_base_mix("1.5", "0xff", "nan") == 320)
  end, "Lua 5.4 tonumber no-base recorder", "STRTO")
end

do
  local int64_strings = {
    "9223372036854775807",
    "1099511627776",
    "123",
  }

  local function tonumber_no_base_int64_constants()
    local n = 0
    for _ = 1, 80 do
      for _, s in ipairs(int64_strings) do
	local v = tonumber(s)
	if s == "9223372036854775807" then
	  if v == math.maxinteger and math.type(v) == "integer" then
	    n = n + 1
	  end
	elseif s == "1099511627776" then
	  if v == 1099511627776 and math.type(v) == "integer" then
	    n = n + 1
	  end
	else
	  if v == 123 and math.type(v) == "integer" then n = n + 1 end
	end
      end
    end
    return n
  end

  assert_records_ir_call(function()
    assert(tonumber_no_base_int64_constants() == 240)
  end, "Lua 5.4 tonumber no-base constant int64 recorder", "lj_obj_newint64")
end

do
  local function tonumber_no_base_invalid_mix(bad_word, bad_suffix)
    local n = 0
    for _ = 1, 80 do
      local a = tonumber(bad_word)
      local b = tonumber(bad_suffix)
      -- Generic invalid strings should be guarded through the same runtime
      -- classifier as numeric strings. If locale or grammar details make a
      -- later value valid, the guard exits and the interpreter decides.
      if a == nil then n = n + 1 end
      if b == nil then n = n + 1 end
    end
    return n
  end

  assert_records_single_ir_trace(function()
    assert(tonumber_no_base_invalid_mix("not-a-number", "123abc") == 160)
  end, "Lua 5.4 tonumber no-base generic invalid recorder", "CALLN")
end

do
  local comma_locale_names = {
    "ptb", "Portuguese_Brazil.1252", "Portuguese_Brazil",
    "German_Germany.1252", "de_DE"
  }

  local function find_comma_numeric_locale()
    local old = os.setlocale(nil, "numeric")
    for _, name in ipairs(comma_locale_names) do
      local ok = os.setlocale(name, "numeric")
      if ok and tonumber("3,4") == 34 / 10 then
	if old then os.setlocale(old, "numeric") end
	return ok
      end
    end
    if old then os.setlocale(old, "numeric") end
  end

  local function with_numeric_locale(name, fn)
    local old = os.setlocale(nil, "numeric")
    assert(os.setlocale(name, "numeric"))
    local ok, err = pcall(fn)
    if old then os.setlocale(old, "numeric") end
    assert(ok, err)
  end

  local function tonumber_comma_const_mix(expect)
    local n = 0
    for _ = 1, 80 do
      local v = tonumber("3,4")
      -- Comma constants are locale-sensitive in Lua 5.4. The recorder must
      -- call the runtime scanner instead of folding one locale's answer.
      if expect == nil then
	if v == nil then n = n + 1 end
      elseif v == expect and math.type(v) == "float" then
	n = n + 1
      end
    end
    return n
  end

  with_numeric_locale("C", function()
    assert_records_ir_op(function()
      assert(tonumber_comma_const_mix(nil) == 80)
    end, "Lua 5.4 tonumber comma constant C-locale recorder", "CALLS")
  end)

  local comma_locale = find_comma_numeric_locale()
  if comma_locale then
    with_numeric_locale(comma_locale, function()
      assert_records_ir_op(function()
	assert(tonumber_comma_const_mix(34 / 10) == 80)
      end, "Lua 5.4 tonumber comma constant numeric-locale recorder", "CALLS")
    end)
  end
end

do
  local function tonumber_base_mix(hex_input, bad_prefix, bad_float)
    local n = 0
    for _ = 1, 80 do
      local a = tonumber(hex_input, 16)
      local b = tonumber(bad_prefix, 16)
      local c = tonumber(bad_float, 10)
      -- Explicit-base tonumber() uses Lua 5.4's integer scanner, not decimal
      -- STRTO. Dynamic strings should record a helper call and keep invalid
      -- prefix/fraction cases as nil inside the hot loop.
      if a == 255 and math.type(a) == "integer" then n = n + 1 end
      if b == nil then n = n + 1 end
      if c == nil then n = n + 1 end
    end
    return n
  end

  assert_records_ir_op(function()
    assert(tonumber_base_mix("ff", "0x10", "1.0") == 240)
  end, "Lua 5.4 tonumber explicit-base dynamic string recorder", "CALLN")
end

do
  local function tonumber_dynamic_base_mix(hex_input, decimal_input, bad_input,
					   base16_num, base10_string,
					   base16_string)
    local n = 0
    for _ = 1, 80 do
      local a = tonumber(hex_input, base16_num)
      local b = tonumber(decimal_input, base10_string)
      local c = tonumber(hex_input, base16_string)
      local d = tonumber(bad_input, base16_num)
      -- The base argument itself is part of the Lua 5.4 compatibility surface:
      -- number, decimal string and hex-string bases all use luaL_checkinteger()
      -- before the explicit-base integer scanner runs.
      if a == 255 and math.type(a) == "integer" then n = n + 1 end
      if b == 42 and math.type(b) == "integer" then n = n + 1 end
      if c == 255 and math.type(c) == "integer" then n = n + 1 end
      if d == nil then n = n + 1 end
    end
    return n
  end

  assert_records_ir_op(function()
    assert(tonumber_dynamic_base_mix("ff", "42", "0x10",
				     16.0, "10", "0x10") == 320)
  end, "Lua 5.4 tonumber explicit-base dynamic base recorder", "CALLN")
end

do
  local mt = { __metatable = "locked" }
  local t = setmetatable({}, mt)
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      if getmetatable(t) == "locked" then n = n + 1 end
    end
    assert(n == 80)
  end, "Lua 5.4 getmetatable __metatable")
  mt.__metatable = "changed"
  local n = 0
  for _ = 1, 80 do
    if getmetatable(t) == "changed" then n = n + 1 end
  end
  assert(n == 80)

  assert_records_trace(function()
    local mt1, mt2 = {}, {}
    local open = {}
    local changed = 0
    for i = 1, 80 do
      local want = (i % 2 == 0) and mt1 or mt2
      if setmetatable(open, want) == open and getmetatable(open) == want then
	changed = changed + 1
      end
    end
    assert(changed == 80)
  end, "Lua 5.4 setmetatable unprotected")

  assert_no_trace(function()
    local fails = 0
    for _ = 1, 80 do
      local ok, err = pcall(setmetatable, t, {})
      if not ok and err:find("protected metatable", 1, true) then
	fails = fails + 1
      end
    end
    assert(fails == 80)
  end, "Lua 5.4 setmetatable protected")
end

do
  local mt = {
    __len = function() return 99 end,
    __index = function() return 99 end,
    __newindex = function() error("rawset used __newindex") end,
    __eq = function() return true end
  }
  local t = setmetatable({ 1, 2 }, mt)
  local u = setmetatable({}, mt)
  assert_records_trace(function()
    local n = 0
    for i = 1, 80 do
      if rawlen(t) == 2 then n = n + 1 end
      if rawget(t, 3) == nil then n = n + 1 end
      rawset(t, 3, i)
      if rawget(t, 3) == i then n = n + 1 end
      rawset(t, 3, nil)
      if rawequal(t, u) == false then n = n + 1 end
    end
    assert(n == 320)
  end, "Lua 5.4 raw helpers bypass metamethods")
end

do
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      if select("#", next({})) == 1 and next({}) == nil then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 next empty table nil result")
end

do
  assert_records_trace(function()
    local n = 0
    for i = 1, 80 do
      local t = table.pack("a", nil, i)
      if t.n == 3 and t[1] == "a" and t[2] == nil and t[3] == i then
	n = n + 1
      end
      local empty = table.pack()
      if empty.n == 0 and empty[1] == nil then n = n + 1 end
    end
    assert(n == 160)
  end, "Lua 5.4 table.pack nil holes")
end

do
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      if tostring(123) == "123" and tostring(nil) == "nil" and
	 tostring(1.0) == "1.0" then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 tostring primitive")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      if ("x" .. 1.0) == "x1.0" and (2 .. 3.5) == "23.5" then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 number concat subtype")

  local mt = { __tostring = function(self) return self.label end }
  local t = setmetatable({ label = "first" }, mt)
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      if tostring(t) == "first" then n = n + 1 end
    end
    assert(n == 80)
  end, "Lua 5.4 tostring __tostring")
  mt.__tostring = function(self) return self.label.."2" end
  t.label = "next"
  local n = 0
  for _ = 1, 80 do
    if tostring(t) == "next2" then n = n + 1 end
  end
  assert(n == 80)

  local numstr = setmetatable({ value = 1.0 }, {
    __tostring = function(self) return self.value end
  })
  assert_records_trace(function()
    local m = 0
    for _ = 1, 80 do
      if tostring(numstr) == "1.0" then m = m + 1 end
    end
    assert(m == 80)
  end, "Lua 5.4 tostring numeric __tostring")

  local bad = setmetatable({}, { __tostring = function() return {} end })
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok, err = pcall(tostring, bad)
      if not ok and err:find("__tostring", 1, true) then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 tostring bad __tostring")

  local named = setmetatable({}, { __name = "Lua54Name" })
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      if tostring(named):find("Lua54Name:", 1, true) == 1 then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 tostring __name")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      if type(named) == "table" and type(false) == "boolean" and
	 type(nil) == "nil" then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 type helper")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      n = n + select("#", "a", nil, "c")
      local a, b = select("2", "a", "b", "c")
      if a == "b" and b == "c" then n = n + 1 end
    end
    assert(n == 320)
  end, "Lua 5.4 select helper")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok, err = pcall(select, 1.2, "a")
      local ok_zero, err_zero = pcall(select, 0, "a")
      if not ok and err:find("integer representation", 1, true) and
	 not ok_zero and
	 err_zero:find("bad argument #1 to 'select'", 1, true) and
	 err_zero:find("index out of range", 1, true) then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 select integer error")

  local marker = {}
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      if assert(true, "ok") == true then n = n + 1 end
    end
    assert(n == 80)
  end, "Lua 5.4 assert helper")

  assert_no_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok, err = pcall(assert, false, marker)
      if not ok and err == marker then n = n + 1 end
    end
    assert(n == 80)
  end, "Lua 5.4 assert non-string error object")

  local binary = string.dump(function() return 54 end)
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local text_fn, text_err = load("return 54", "lua54-load-text", "b")
      local bin_fn, bin_err = load(binary, "lua54-load-bin", "t")
      if text_fn == nil and
	 text_err:find("attempt to load a text chunk (mode is 'b')",
		       1, true) and
	 bin_fn == nil and
	 bin_err:find("attempt to load a binary chunk (mode is 't')",
		      1, true) then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 load mode errors")

  local function result_count(...)
    return select("#", ...), ...
  end

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok_step, err_step = pcall(collectgarbage, "step", true)
      local error_n, ok_error, err_error = result_count(pcall(error))
      local ok_level, err_level = pcall(error, "x", true)
      local ok_rawlen, err_rawlen = pcall(rawlen, true)
      local ok_select, err_select = pcall(select)
      local ok_tonumber, err_tonumber = pcall(function()
	return tonumber()
      end)
      local ok_warn, err_warn = pcall(warn, {})
      if not ok_step and
	 err_step:find("bad argument #2 to 'collectgarbage'", 1, true) and
	 err_step:find("number expected, got boolean", 1, true) then
	n = n + 1
      end
      if error_n == 2 and not ok_error and err_error == nil then
	n = n + 1
      end
      if not ok_level and
	 err_level:find("bad argument #2 to 'error'", 1, true) and
	 err_level:find("number expected, got boolean", 1, true) then
	n = n + 1
      end
      if not ok_rawlen and
	 err_rawlen:find("bad argument #1 to 'rawlen'", 1, true) and
	 err_rawlen:find("table or string expected, got boolean", 1, true) then
	n = n + 1
      end
      if not ok_select and
	 err_select:find("bad argument #1 to 'select'", 1, true) and
	 err_select:find("number expected, got no value", 1, true) then
	n = n + 1
      end
      if not ok_tonumber and
	 err_tonumber:find("bad argument #1 to 'tonumber'", 1, true) and
	 err_tonumber:find("value expected", 1, true) then
	n = n + 1
      end
      if tonumber({}) == nil then n = n + 1 end
      if not ok_warn and
	 err_warn:find("bad argument #1 to 'warn'", 1, true) and
	 err_warn:find("string expected, got table", 1, true) then
	n = n + 1
      end
    end
    assert(n == 640)
  end, "Lua 5.4 base stdlib edge errors")
end

do
  local marker = {}
  local function add(a, b)
    return a + b
  end
  local function fail_marker()
    error(marker, 0)
  end
  local function fail_string()
    error("lua54 protected boom", 0)
  end
  local function handler(err)
    return { handled = err }
  end

  assert_records_trace(function()
    local n = 0
    for i = 1, 80 do
      local ok, value = pcall(add, i, 2)
      if ok then n = n + value end
    end
    assert(n == 3400)
  end, "Lua 5.4 pcall success args")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok, err = pcall(fail_marker)
      if not ok and err == marker then n = n + 1 end
    end
    assert(n == 80)
  end, "Lua 5.4 pcall non-string error")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok, err = pcall(fail_string)
      if not ok and err == "lua54 protected boom" then n = n + 1 end
    end
    assert(n == 80)
  end, "Lua 5.4 pcall string error")

  assert_records_trace(function()
    local n = 0
    for i = 1, 80 do
      local ok, value = xpcall(add, handler, i, 3)
      if ok then n = n + value end
    end
    assert(n == 3480)
  end, "Lua 5.4 xpcall success args")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok, result = xpcall(fail_marker, handler)
      if not ok and result.handled == marker then n = n + 1 end
    end
    assert(n == 80)
  end, "Lua 5.4 xpcall handler object")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok, err = pcall(error, marker, 1)
      if not ok and err == marker then n = n + 1 end
    end
    assert(n == 80)
  end, "Lua 5.4 error non-string object")
end

do
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local co, ismain = coroutine.running()
      if ismain and coroutine.status(co) == "running" and
	 not coroutine.isyieldable(co) then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 coroutine running helpers")

  assert_records_trace(function()
    local n = 0
    for i = 1, 80 do
      local co = coroutine.create(function(a, b)
	return a + b, nil, "x"
      end)
      local ok, a, b, c = coroutine.resume(co, i, 2)
      if ok and a == i + 2 and b == nil and c == "x" and
	 coroutine.status(co) == "dead" then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 coroutine resume values")

  local function result_count(...)
    return select("#", ...), ...
  end

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local co = coroutine.create(function()
	coroutine.yield("pause")
      end)
      local ok, value = coroutine.resume(co)
      local was_suspended = coroutine.status(co) == "suspended"
      local close_n, close_ok = result_count(coroutine.close(co))
      if ok and value == "pause" and was_suspended and
	 close_n == 1 and close_ok == true and coroutine.status(co) == "dead" then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 coroutine close suspended")

  local main_thread = coroutine.running()
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok_run, err_run = pcall(coroutine.close, coroutine.running())
      local co = coroutine.create(function()
	return pcall(coroutine.close, main_thread)
      end)
      local ok_resume, ok_norm, err_norm = coroutine.resume(co)
      if not ok_run and err_run == "cannot close a running coroutine" and
	 ok_resume and not ok_norm and
	 err_norm == "cannot close a normal coroutine" then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 coroutine close state errors")

  local close_marker = {}
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local co = coroutine.create(function()
	local x <close> = setmetatable({}, {
	  __close = function()
	    error(close_marker, 0)
	  end
	})
	coroutine.yield("pause")
      end)
      assert(coroutine.resume(co))
      local ok, err = coroutine.close(co)
      if not ok and err == close_marker and coroutine.status(co) == "dead" then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 coroutine close error object")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local seen, same, obj
      obj = setmetatable({}, {
	__close = function(self, err)
	  same = self == obj
	  seen = err
	end
      })
      local co = coroutine.create(function()
	local x <close> = obj
	coroutine.yield("pause")
      end)
      local ok, value = coroutine.resume(co)
      local close_n, close_ok = result_count(coroutine.close(co))
      if ok and value == "pause" and same and seen == nil and
	 close_n == 1 and close_ok == true and coroutine.status(co) == "dead" then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 coroutine close passes nil close error")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local co = coroutine.create(function()
	local x <close> = setmetatable({}, {
	  __close = function()
	    error(close_marker, 0)
	  end
	})
	coroutine.yield("pause")
      end)
      assert(coroutine.resume(co))
      local ok, err = coroutine.close(co)
      local reclose_n, reclosed = result_count(coroutine.close(co))
      if not ok and err == close_marker and reclose_n == 1 and
	 reclosed == true and coroutine.status(co) == "dead" then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 coroutine close error reclose")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local log = {}
      local wrapped = coroutine.wrap(function()
	local x <close> = setmetatable({}, {
	  __close = function()
	    log[1] = "closed"
	  end
	})
	error(close_marker, 0)
      end)
      local ok, err = pcall(wrapped)
      if not ok and err == close_marker and log[1] == "closed" then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 coroutine.wrap closes on error")

  local function tail_status_nil()
    return coroutine.status(nil)
  end

  local function tail_create_nil()
    return coroutine.create(nil)
  end

  local function tail_wrap_nil()
    return coroutine.wrap(nil)
  end

  local function tail_isyieldable_nil()
    return coroutine.isyieldable(nil)
  end

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok_status, err_status = pcall(tail_status_nil)
      local ok_create, err_create = pcall(tail_create_nil)
      local ok_wrap, err_wrap = pcall(tail_wrap_nil)
      local ok_yieldable, err_yieldable = pcall(tail_isyieldable_nil)
      if not ok_status and
	 err_status:find("bad argument #1 to 'status'", 1, true) then
	n = n + 1
      end
      if not ok_create and
	 err_create:find("bad argument #1 to 'create'", 1, true) then
	n = n + 1
      end
      if not ok_wrap and
	 err_wrap:find("bad argument #1 to 'wrap'", 1, true) then
	n = n + 1
      end
      if not ok_yieldable and
	 err_yieldable:find("bad argument #1 to 'isyieldable'", 1, true) then
	n = n + 1
      end
    end
    assert(n == 320)
  end, "Lua 5.4 coroutine tail wrapper error names")
end

do
  assert_records_trace(function()
    collectgarbage("restart")
    collectgarbage("generational")
    local n = 0
    for i = 1, 80 do
      local old = collectgarbage((i % 2 == 0) and "generational" or "incremental")
      if old == "generational" or old == "incremental" then n = n + 1 end
      if collectgarbage("isrunning", true) then n = n + 1 end
      if type(collectgarbage("count", true)) == "number" then n = n + 1 end
    end
    assert(n == 240)
  end, "Lua 5.4 collectgarbage mode helpers")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local stepped = collectgarbage("step", "1")
      if stepped == true or stepped == false then n = n + 1 end
    end
    assert(n == 80)
  end, "Lua 5.4 collectgarbage step string integer")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok, err = pcall(collectgarbage, "step", 1.2)
      if not ok and err:find("integer representation", 1, true) then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 collectgarbage integer error")

  assert_records_trace(function()
    collectgarbage("incremental")
    local n = 0
    for i = 1, 80 do
      -- Lua 5.4 accepts but still type-checks the collector-mode tuning
      -- arguments. Keep this visible to the recorder so hot helper paths cannot
      -- silently ignore bad optional arguments.
      local old = collectgarbage((i % 2 == 0) and "generational" or
				 "incremental", "20", "30", "4")
      if old == "generational" or old == "incremental" then n = n + 1 end
    end
    assert(n == 80)
  end, "Lua 5.4 collectgarbage mode integer options")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok, err = pcall(collectgarbage, "incremental", 200, 300, 12.5)
      if not ok and err:find("bad argument #4 to 'collectgarbage'",
			     1, true) then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 collectgarbage mode integer option error")
end

do
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      if select("#", debug.gethook()) == 1 and debug.gethook() == nil then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 debug.gethook absent hook nil result")

  assert_no_trace(function()
    local n = 0
    local hook = function() end
    for _ = 1, 80 do
      local ok_thread, err_thread = pcall(debug.sethook, true)
      if not ok_thread and
	 err_thread:find("bad argument #2 to 'debug.sethook'",
			 1, true) and
	 err_thread:find("string expected, got no value", 1, true) then
	n = n + 1
      end

      debug.sethook(hook, "", "3")
      local h, mask, count = debug.gethook()
      if h == hook and mask == "" and count == 3 then n = n + 1 end
      debug.sethook()

      debug.sethook(hook, "", 1)
      h, mask, count = debug.gethook()
      if h == hook and mask == "" and count == 1 then n = n + 1 end
      debug.sethook()

      local ok_count, err_count = pcall(debug.sethook, hook, "", 1.2)
      if not ok_count and
	 err_count:find("bad argument #3 to 'debug.sethook'",
			1, true) and
	 err_count:find("number has no integer representation", 1, true) then
	n = n + 1
      end
    end
    debug.sethook()
    assert(n == 80 * 4)
  end, "Lua 5.4 debug.sethook count edges")

  assert_no_trace(function()
    local n = 0
    local mt = { tag = "debug-meta" }
    for _ = 1, 80 do
      local ok_get, err_get = pcall(debug.getmetatable)
      if not ok_get and
	 err_get:find("bad argument #1 to 'debug.getmetatable'",
		      1, true) and
	 err_get:find("value expected", 1, true) then
	n = n + 1
      end

      if debug.getmetatable({}) == nil then n = n + 1 end
      if type(debug.getregistry(1)) == "table" then n = n + 1 end

      local protected = setmetatable({}, {
	__metatable = "locked",
	tag = "real",
      })
      if debug.getmetatable(protected).tag == "real" then n = n + 1 end
      if debug.setmetatable(protected, mt) == protected and
	 getmetatable(protected) == mt then
	n = n + 1
      end

      local t = {}
      if debug.setmetatable(t, mt) == t and
	 getmetatable(t) == mt then
	n = n + 1
      end
      if debug.setmetatable(t, nil) == t and
	 getmetatable(t) == nil then
	n = n + 1
      end

      local ok_set0, err_set0 = pcall(debug.setmetatable)
      local ok_set2, err_set2 = pcall(debug.setmetatable, {}, true)
      if not ok_set0 and
	 err_set0:find("bad argument #2 to 'debug.setmetatable'",
		       1, true) and
	 err_set0:find("got no value", 1, true) and
	 not ok_set2 and
	 err_set2:find("bad argument #2 to 'debug.setmetatable'",
		       1, true) and
	 err_set2:find("got boolean", 1, true) then
	n = n + 1
      end
    end
    assert(n == 80 * 8)
  end, "Lua 5.4 debug metatable registry edges")

  do
    local old = debug.setcstacklimit(200)
    assert_records_trace(function()
      local n = 0
      for _ = 1, 80 do
	if debug.setcstacklimit(200) == 200 then n = n + 1 end
      end
      assert(n == 80)
    end, "Lua 5.4 debug.setcstacklimit hot path")
    debug.setcstacklimit(old)
  end

  assert_records_trace(function()
    local fn = function() end
    local n = 0
    for _ = 1, 80 do
      if select("#", debug.upvalueid(fn, 0)) == 1 and
	 debug.upvalueid(fn, 0) == nil then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 debug.upvalueid missing upvalue nil result")

  assert_records_trace(function()
    local fn = function() end
    local n = 0
    for _ = 1, 80 do
      if select("#", debug.setupvalue(fn, 1, "hidden")) == 0 and
	 select("#", debug.setupvalue(print, 1, "hidden")) == 0 then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 debug.setupvalue missing upvalue no result")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      if select("#", debug.getupvalue(print, 1)) == 0 and
	 select("#", debug.getupvalue(pairs, 1)) == 0 and
	 select("#", debug.getupvalue(ipairs, 1)) == 0 and
	 select("#", debug.upvalueid(print, 1)) == 1 and
	 debug.upvalueid(print, 1) == nil then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 hides standard C closure upvalues")

  assert_records_trace(function()
    local up = 3
    local fn = function() return up end
    local n = 0
    for _ = 1, 80 do
      local name, value = debug.getupvalue(fn, 1)
      if name == "up" and value == up then n = n + 1 end

      if debug.setupvalue(fn, 1, 4) == "up" and fn() == 4 then
	n = n + 1
      end
      debug.setupvalue(fn, 1, 3)

      local ok_getbad, err_getbad = pcall(debug.getupvalue, true, 1)
      local ok_setbad, err_setbad = pcall(debug.setupvalue, true, 1, 2)
      if not ok_getbad and
	 err_getbad:find("bad argument #1 to 'debug.getupvalue'",
			 1, true) and
	 err_getbad:find("function expected, got boolean", 1, true) and
	 not ok_setbad and
	 err_setbad:find("bad argument #1 to 'debug.setupvalue'",
			 1, true) and
	 err_setbad:find("function expected, got boolean", 1, true) then
	n = n + 1
      end
    end
    assert(n == 80 * 3)
  end, "Lua 5.4 debug upvalue access edges")

  assert_records_trace(function()
    local function result_count(...)
      return select("#", ...), ...
    end
    local n = 0
    for _ = 1, 80 do
      local x, y = 1, 2
      local f = function() return x end
      local g = function(v)
	if v ~= nil then y = v end
	return y
      end
      local join_n = result_count(debug.upvaluejoin(f, 1, g, 1))
      g(3)
      if join_n == 0 and f() == 3 and
	 debug.upvalueid(f, 1) == debug.upvalueid(g, 1) then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 debug.upvaluejoin shared upvalue")

  assert_records_trace(function()
    local up
    local fn = function() return up end
    local n = 0
    for _ = 1, 80 do
      local ok_first, err_first = pcall(debug.upvaluejoin,
					print, 1, print, 1)
      local ok_second, err_second = pcall(debug.upvaluejoin,
					  fn, 1, print, 1)
      if not ok_first and
	 err_first:find("bad argument #2 to 'debug.upvaluejoin'",
			1, true) and
	 err_first:find("invalid upvalue index", 1, true) and
	 not ok_second and
	 err_second:find("bad argument #4 to 'debug.upvaluejoin'",
			 1, true) and
	 err_second:find("invalid upvalue index", 1, true) then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 debug.upvaluejoin C function index errors")

  assert_records_trace(function()
    local up
    local fn = function() return up end
    local n = 0
    for _ = 1, 80 do
      local ok_id_noarg, err_id_noarg = pcall(debug.upvalueid)
      local ok_id_badfunc, err_id_badfunc = pcall(debug.upvalueid, true)
      local ok_join_noarg, err_join_noarg = pcall(debug.upvaluejoin)
      local ok_join_second, err_join_second = pcall(debug.upvaluejoin,
						    fn, 1)
      local ok_join_badf2_noidx, err_join_badf2_noidx =
	pcall(debug.upvaluejoin, fn, 1, true)
      local ok_join_badf2, err_join_badf2 =
	pcall(debug.upvaluejoin, fn, 1, true, 1)
      if not ok_id_noarg and
	 err_id_noarg:find("bad argument #2 to 'debug.upvalueid'",
			   1, true) and
	 not ok_id_badfunc and
	 err_id_badfunc:find("bad argument #2 to 'debug.upvalueid'",
			     1, true) and
	 not ok_join_noarg and
	 err_join_noarg:find("bad argument #2 to 'debug.upvaluejoin'",
			     1, true) and
	 not ok_join_second and
	 err_join_second:find("bad argument #4 to 'debug.upvaluejoin'",
			      1, true) and
	 not ok_join_badf2_noidx and
	 err_join_badf2_noidx:find("bad argument #4 to 'debug.upvaluejoin'",
				   1, true) and
	 not ok_join_badf2 and
	 err_join_badf2:find("bad argument #3 to 'debug.upvaluejoin'",
			     1, true) then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 debug upvalue argument order errors")

  assert_records_trace(function()
    local co = coroutine.create(function()
      local x = 1
      coroutine.yield()
      return x
    end)
    assert(coroutine.resume(co))
    local n = 0
    for _ = 1, 80 do
      local ok_get, err_get = pcall(debug.getlocal, 999, 1)
      local ok_set, err_set = pcall(debug.setlocal, 999, 1, true)
      local ok_tget, err_tget = pcall(debug.getlocal, co, 999, 1)
      local ok_tset, err_tset = pcall(debug.setlocal, co, 999, 1, true)
      if not ok_get and
	 err_get:find("bad argument #1 to 'debug.getlocal'",
		      1, true) and
	 not ok_set and
	 err_set:find("bad argument #1 to 'debug.setlocal'",
		      1, true) and
	 not ok_tget and
	 err_tget:find("bad argument #2 to 'debug.getlocal'",
		       1, true) and
	 not ok_tset and
	 err_tset:find("bad argument #2 to 'debug.setlocal'",
		       1, true) then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 debug local level error names")

  assert_records_trace(function()
    local function result_count(...)
      return select("#", ...), ...
    end
    local n = 0
    for _ = 1, 80 do
      local missing_n, missing_name = result_count(debug.getlocal(1, 999))
      local set_missing_n, set_missing_name =
	result_count(debug.setlocal(1, 999, "x"))
      if missing_n == 1 and missing_name == nil and
	 set_missing_n == 1 and set_missing_name == nil then
	n = n + 1
      end

      local local_co = coroutine.create(function(a)
	local x = a + 1
	coroutine.yield(x)
	return x
      end)
      local ok_yield, yielded = coroutine.resume(local_co, 41)
      local n1, v1 = debug.getlocal(local_co, 1, 1)
      local n2, v2 = debug.getlocal(local_co, 1, 2)
      local set_name = debug.setlocal(local_co, 1, 2, 99)
      local ok_done, done = coroutine.resume(local_co)
      if ok_yield and yielded == 42 and
	 n1 == "a" and v1 == 41 and
	 n2 == "x" and v2 == 42 and
	 set_name == "x" and ok_done and done == 99 then
	n = n + 1
      end
    end
    assert(n == 80 * 2)
  end, "Lua 5.4 debug local access edges")

  assert_records_trace(function()
    local fn = function(a, ...) return a end
    local n = 0
    for _ = 1, 80 do
      local wide = debug.getinfo(1099511627776, "S")
      if wide and wide.what == "C" then n = n + 1 end
      if debug.getinfo(math.maxinteger) == nil then n = n + 1 end

      local ok_level, err_level = pcall(debug.getinfo, 1.2)
      local ok_what, err_what = pcall(debug.getinfo, 1, "z")
      local ok_gt, err_gt = pcall(debug.getinfo, 1, ">")
      if not ok_level and
	 err_level:find("number has no integer representation", 1, true) and
	 not ok_what and err_what:find("invalid option", 1, true) and
	 not ok_gt and err_gt:find("invalid option '>'", 1, true) then
	n = n + 1
      end

      local u = debug.getinfo(fn, "u")
      if u.nparams == 1 and u.isvararg == true then n = n + 1 end
      if debug.traceback():find("stack traceback", 1, true) then
	n = n + 1
      end
      if debug.traceback(true) == true then n = n + 1 end
      if debug.traceback("m", 1099511627776):
	 find("stack traceback", 1, true) then
	n = n + 1
      end
    end
    assert(n == 80 * 7)
  end, "Lua 5.4 debug.getinfo traceback edges")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      if select("#", debug.getuservalue()) == 1 and
	 debug.getuservalue() == nil then
	n = n + 1
      end
      if debug.getuservalue(nil) == nil and
	 debug.getuservalue(true) == nil then
	n = n + 1
      end
      if debug.getuservalue(io.stdout, 1) == nil and
	 debug.setuservalue(io.stdout, {}, 1) == nil then
	n = n + 1
      end
      local ok_getslot, err_getslot = pcall(debug.getuservalue, {}, true)
      local ok_set_noarg, err_set_noarg = pcall(debug.setuservalue)
      local ok_setslot, err_setslot = pcall(debug.setuservalue, {},
					    1, true)
      if not ok_getslot and
	 err_getslot:find("bad argument #2 to 'debug.getuservalue'",
			  1, true) and
	 not ok_set_noarg and
	 err_set_noarg:find("bad argument #1 to 'debug.setuservalue'",
			    1, true) and
	 not ok_setslot and
	 err_setslot:find("bad argument #3 to 'debug.setuservalue'",
			  1, true) then
	n = n + 1
      end
    end
    assert(n == 80 * 4)
  end, "Lua 5.4 debug uservalue edges")
end

do
  assert_records_trace(function()
    package.loaded.__lua54_jit_loaded = { v = 7 }
    local n = 0
    for _ = 1, 80 do
      local mod, loaderdata = require("__lua54_jit_loaded")
      if mod.v == 7 and loaderdata == nil then n = n + 1 end
    end
    package.loaded.__lua54_jit_loaded = nil
    assert(n == 80)
  end, "Lua 5.4 require loaded fast path")

  assert_records_trace(function()
    local count = 0
    package.loaded.__lua54_jit_nil = nil
    package.preload.__lua54_jit_nil = function()
      count = count + 1
      return nil
    end
    local n = 0
    for _ = 1, 80 do
      local mod, loaderdata = require("__lua54_jit_nil")
      if mod == true and (loaderdata == nil or loaderdata == ":preload:") then
	n = n + 1
      end
    end
    package.loaded.__lua54_jit_nil = nil
    package.preload.__lua54_jit_nil = nil
    assert(n == 80 and count == 1)
  end, "Lua 5.4 require nil loader")

  assert_records_trace(function()
    package.loaded.__lua54_jit_err = nil
    package.preload.__lua54_jit_err = function()
      error("lua54 package boom", 0)
    end
    local n = 0
    for _ = 1, 80 do
      local ok, err = pcall(require, "__lua54_jit_err")
      if not ok and err == "lua54 package boom" and
	 package.loaded.__lua54_jit_err == nil then
	n = n + 1
      end
    end
    package.preload.__lua54_jit_err = nil
    assert(n == 80)
  end, "Lua 5.4 require loader error")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local found, err = package.searchpath("__lua54_missing__",
					    "?.lua;;none/?.lua")
      if found == nil and err:find("no file ''", 1, true) then
	n = n + 1
      end
      found, err = package.searchpath("__lua54_missing__", "")
      if found == nil and err == "no file ''" then
	n = n + 1
      end
      found, err = package.searchpath(1099511627776, "?.lua")
      if found == nil and err:find("1099511627776.lua", 1, true) then
	n = n + 1
      end
    end
    assert(n == 240)
  end, "Lua 5.4 package.searchpath string coercion")

  local function result_count(...)
    return select("#", ...), ...
  end

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok_xpcall_handler, err_xpcall_handler = pcall(function()
	return xpcall(function() end, true)
      end)
      local ok_pcall_noncall, err_pcall_noncall = pcall(1)
      local ok_create, err_create = pcall(function()
	return coroutine.create()
      end)
      local ok_close_bad, err_close_bad = pcall(function()
	return coroutine.close(true)
      end)
      local running_co, running_main = coroutine.running(true)
      local ok_resume, err_resume = pcall(function()
	return coroutine.resume(true)
      end)
      local log_nil = math.log(8, nil)
      local log_strbase = math.log(8, "2")
      local ok_sqrt_noarg, err_sqrt_noarg = pcall(function()
	return math.sqrt()
      end)
      local sqrt_string = math.sqrt("4")
      local ok_deg_noarg, err_deg_noarg = pcall(function()
	return math.deg()
      end)
      local ok_modf_noarg, err_modf_noarg = pcall(function()
	return math.modf()
      end)
      local modf_int, modf_frac = math.modf("4.5")
      local execute_noarg = os.execute()
      local ok_getenv_noarg, err_getenv_noarg = pcall(function()
	return os.getenv()
      end)
      local ok_getenv_bad, err_getenv_bad = pcall(function()
	return os.getenv(true)
      end)
      local ok_rename, err_rename = pcall(function()
	return os.rename("x", true)
      end)
      local ok_remove_bad, err_remove_bad = pcall(function()
	return os.remove(true)
      end)
      local ok_setlocale1, err_setlocale1 = pcall(function()
	return os.setlocale(true)
      end)
      local ok_setlocale2, err_setlocale2 = pcall(function()
	return os.setlocale("", true)
      end)
      local ok_searchrep, err_searchrep = pcall(function()
	return package.searchpath("a", "?.lua", ".", true)
      end)
      local found_nilsep, err_nilsep = package.searchpath("a.b", "?.lua",
							  nil, "/")
      local ok_byte, err_byte = pcall(function()
	return string.byte("x", 1, true)
      end)
      local byte_empty_n = result_count(string.byte(""))
      local byte_range_n = result_count(string.byte("abc", 3, 2))
      local char_empty = string.char()
      local first, last = string.find("a", "a", 1, {})
      local ok_find_frac, err_find_frac = pcall(function()
	return string.find("abc", "a", 1.2, true)
      end)
      local ok_match_frac, err_match_frac = pcall(function()
	return string.match("abc", "a", 1.2)
      end)
      local ok_gmatch_bool, err_gmatch_bool = pcall(function()
	return string.gmatch("abc", ".", true)
      end)
      local unpack_value, unpack_pos = string.unpack("b", "abc", -0)
      local unpack_empty_n = result_count(table.unpack({}, 2, 1))
      local ok_sort_bad, err_sort_bad = pcall(function()
	return table.sort({ 2, 1 }, true)
      end)
      local ok_offset, err_offset = pcall(function()
	return utf8.offset("a", true)
      end)
      local utf8_len_empty = utf8.len("abc", 3, 2)
      local utf8_offset_neg = utf8.offset("abc", -1)
      if not ok_xpcall_handler and
	 err_xpcall_handler:find("bad argument #2 to 'xpcall'", 1, true) and
	 err_xpcall_handler:find("function expected, got boolean",
				 1, true) then
	n = n + 1
      end
      if not ok_pcall_noncall and
	 err_pcall_noncall:find("attempt to call a number value", 1, true) then
	n = n + 1
      end
      if not ok_create and
	 err_create:find("bad argument #1 to 'create'", 1, true) and
	 err_create:find("function expected, got no value", 1, true) then
	n = n + 1
      end
      if not ok_close_bad and
	 err_close_bad:find("bad argument #1 to 'close'", 1, true) and
	 err_close_bad:find("thread expected, got boolean", 1, true) then
	n = n + 1
      end
      if type(running_co) == "thread" and running_main == true then n = n + 1 end
      if not ok_resume and
	 err_resume:find("bad argument #1 to 'resume'", 1, true) and
	 err_resume:find("thread expected, got boolean", 1, true) then
	n = n + 1
      end
      if math.abs(log_nil - math.log(8)) < 1e-12 then n = n + 1 end
      if math.abs(log_strbase - 3) < 1e-12 then n = n + 1 end
      if not ok_sqrt_noarg and
	 err_sqrt_noarg:find("bad argument #1 to 'sqrt'", 1, true) and
	 err_sqrt_noarg:find("number expected, got no value", 1, true) then
	n = n + 1
      end
      if sqrt_string == 2 then n = n + 1 end
      if not ok_deg_noarg and
	 err_deg_noarg:find("bad argument #1 to 'deg'", 1, true) and
	 err_deg_noarg:find("number expected, got no value", 1, true) then
	n = n + 1
      end
      if not ok_modf_noarg and
	 err_modf_noarg:find("bad argument #1 to 'modf'", 1, true) and
	 err_modf_noarg:find("number expected, got no value", 1, true) then
	n = n + 1
      end
      if modf_int == 4 and modf_frac == 0.5 then n = n + 1 end
      if execute_noarg == true then n = n + 1 end
      if not ok_getenv_noarg and
	 err_getenv_noarg:find("bad argument #1 to 'getenv'", 1, true) and
	 err_getenv_noarg:find("string expected, got no value", 1, true) then
	n = n + 1
      end
      if not ok_getenv_bad and
	 err_getenv_bad:find("bad argument #1 to 'getenv'", 1, true) and
	 err_getenv_bad:find("string expected, got boolean", 1, true) then
	n = n + 1
      end
      if not ok_rename and
	 err_rename:find("bad argument #2 to 'rename'", 1, true) and
	 err_rename:find("string expected, got boolean", 1, true) then
	n = n + 1
      end
      if not ok_remove_bad and
	 err_remove_bad:find("bad argument #1 to 'remove'", 1, true) and
	 err_remove_bad:find("string expected, got boolean", 1, true) then
	n = n + 1
      end
      if not ok_setlocale1 and
	 err_setlocale1:find("bad argument #1 to 'setlocale'", 1, true) and
	 err_setlocale1:find("string expected, got boolean", 1, true) then
	n = n + 1
      end
      if not ok_setlocale2 and
	 err_setlocale2:find("bad argument #2 to 'setlocale'", 1, true) and
	 err_setlocale2:find("string expected, got boolean", 1, true) then
	n = n + 1
      end
      if not ok_searchrep and
	 err_searchrep:find("bad argument #4 to 'searchpath'", 1, true) and
	 err_searchrep:find("string expected, got boolean", 1, true) then
	n = n + 1
      end
      if found_nilsep == nil and
	 err_nilsep:find("no file 'a/b.lua'", 1, true) then
	n = n + 1
      end
      if not ok_byte and
	 err_byte:find("bad argument #3 to 'byte'", 1, true) and
	 err_byte:find("number expected, got boolean", 1, true) then
	n = n + 1
      end
      if byte_empty_n == 0 then n = n + 1 end
      if byte_range_n == 0 then n = n + 1 end
      if char_empty == "" then n = n + 1 end
      if first == 1 and last == 1 then n = n + 1 end
      if not ok_find_frac and
	 err_find_frac:find("bad argument #3 to 'find'", 1, true) and
	 err_find_frac:find("integer representation", 1, true) then
	n = n + 1
      end
      if not ok_match_frac and
	 err_match_frac:find("bad argument #3 to 'match'", 1, true) and
	 err_match_frac:find("integer representation", 1, true) then
	n = n + 1
      end
      if not ok_gmatch_bool and
	 err_gmatch_bool:find("bad argument #3 to 'gmatch'", 1, true) and
	 err_gmatch_bool:find("number expected, got boolean", 1, true) then
	n = n + 1
      end
      if unpack_value == 97 and unpack_pos == 2 then n = n + 1 end
      if unpack_empty_n == 0 then n = n + 1 end
      if not ok_sort_bad and
	 err_sort_bad:find("bad argument #2 to 'sort'", 1, true) and
	 err_sort_bad:find("function expected, got boolean", 1, true) then
	n = n + 1
      end
      if not ok_offset and
	 err_offset:find("bad argument #2 to 'offset'", 1, true) and
	 err_offset:find("number expected, got boolean", 1, true) then
	n = n + 1
      end
      if utf8_len_empty == 0 then n = n + 1 end
      if utf8_offset_neg == 3 then n = n + 1 end
    end
    assert(n == 2880)
  end, "Lua 5.4 mixed stdlib edge errors")
end

do
  local stamp = os.time({
    year = 2020, month = 5, day = 7, hour = 12, min = 34, sec = 56
  })
  local future_fields = {
    year = 2039, month = 1, day = 2, hour = 12, min = 34, sec = 56,
    isdst = false
  }
  local future_ok, future_stamp = pcall(os.time, future_fields)
  local future_supported = future_ok and future_stamp > 2147483647
  local path_present = os.getenv("PATH") ~= nil

  assert_records_trace(function()
    local n = 0
    for i = 1, 80 do
      if math.type(stamp) == "integer" then n = n + 1 end
      if os.date("%Y-%m-%d", stamp) == "2020-05-07" then n = n + 1 end
      if os.date("!\0\0", stamp) == "\0\0" then n = n + 1 end
      if os.date(1099511627776, stamp) == "1099511627776" then n = n + 1 end
      if os.difftime(stamp + 7, stamp) == 7 then n = n + 1 end
      if future_supported and math.type(future_stamp) == "integer" and
	 os.difftime(future_stamp + 7, future_stamp) == 7 and
	 os.difftime(tostring(future_stamp + 7), tostring(future_stamp)) == 7 and
	 os.date("%Y-%m-%d", future_stamp) == "2039-01-02" then
	n = n + 1
      end

      -- Use a midday timestamp and valid 1..50 seconds so Windows local-time
      -- normalization cannot turn this into a timezone or DST edge test.
      local sec = (i % 50) + 1
      local fields = {
	year = 2020, month = 5, day = 7, hour = 12, min = 34, sec = sec
      }
      local value = os.time(fields)
      if math.type(value) == "integer" and fields.year == 2020 and
	 fields.month == 5 and fields.day == 7 and fields.hour == 12 and
	 fields.min == 34 and fields.sec == sec and fields.yday == 128 then
	n = n + 1
      end

      if (os.getenv("PATH") ~= nil) == path_present then n = n + 1 end
      if type(os.setlocale(nil, "time")) == "string" then n = n + 1 end
    end
    assert(n == 640 + (future_supported and 80 or 0))
  end, "Lua 5.4 os date/time helpers")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok_time, err_time = pcall(os.time, {
	year = 2020, month = 5, day = 7, hour = 1.5
      })
      local ok_date, err_date = pcall(os.date, true)
      local ok_date_time, err_date_time = pcall(os.date, "%Y", 1.5)
      local ok_diff1, err_diff1 = pcall(os.difftime, 2.5, 1)
      local ok_diff2, err_diff2 = pcall(os.difftime, 2, 1.5)
      local ok_diff_missing, err_diff_missing = pcall(os.difftime, 1)
      local ok_diff_noargs, err_diff_noargs = pcall(os.difftime)
      local ok_diff_nil, err_diff_nil = pcall(function()
	return os.difftime(nil)
      end)
      local ok_date_huge, err_date_huge = pcall(os.date, "%Y", 2^60)
      local ok_time_repr, err_time_repr = pcall(os.time, {
	year = 4000, month = 1, day = 1
      })
      local ok_time_year, err_time_year = pcall(os.time, { hour = 12 })
      local ok_time_month, err_time_month = pcall(os.time, { year = 2020 })
      local ok_time_day, err_time_day = pcall(os.time, {
	year = 2020, month = 1
      })
      local ok_time_min, err_time_min = pcall(os.time, {
	year = 2020, month = 1, day = 1, min = true, sec = true
      })
      if not ok_time and
	 err_time:find("field 'hour' is not an integer", 1, true) and
	 not ok_date and err_date:find("to 'os.date'", 1, true) and
	 not ok_date_time and
	 err_date_time:find("integer representation", 1, true) and
	 not ok_diff1 and
	 err_diff1:find("integer representation", 1, true) and
	 not ok_diff2 and
	 err_diff2:find("integer representation", 1, true) and
	 not ok_diff_missing and
	 err_diff_missing:find("bad argument #2 to 'os.difftime'", 1, true) and
	 not ok_diff_noargs and
	 err_diff_noargs:find("bad argument #1 to 'os.difftime'", 1, true) and
	 not ok_diff_nil and
	 err_diff_nil:find("bad argument #1 to 'difftime'", 1, true) and
	 not ok_date_huge and
	 err_date_huge:find("date result cannot be represented", 1, true) and
	 not ok_time_repr and
	 err_time_repr:find("time result cannot be represented", 1, true) and
	 not ok_time_year and
	 err_time_year:find("field 'year' missing", 1, true) and
	 not ok_time_month and
	 err_time_month:find("field 'month' missing", 1, true) and
	 not ok_time_day and
	 err_time_day:find("field 'day' missing", 1, true) and
	 not ok_time_min and
	 err_time_min:find("field 'min' is not an integer", 1, true) then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 os helper errors")
end

do
  local fname = "lua54_jit_io.tmp"
  local vfname = "lua54_jit_io_setvbuf.tmp"
  local f = assert(io.open(fname, "w+b"))
  local vf = assert(io.open(vfname, "w"))
  f:write("123\n16\nabc\n")
  f:flush()
  local it, _, _, line_closing = io.lines(fname, {})
  local it2_src, _, _, line_closing2 = io.lines(fname, {})
  local itg_src, _, _, line_closing3 = io.lines(fname, {})
  local it2 = it2_src
  lua54_jit_io_global_it = itg_src

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      if f:seek("set", 0) == 0 then n = n + 1 end
      local a = f:read("n")
      if a == 123 and math.type(a) == "integer" then n = n + 1 end
      if f:seek("set", "4") == 4 then n = n + 1 end
      local b = f:read("n")
      if b == 16 and math.type(b) == "integer" then n = n + 1 end
      if f:seek("set", 7) == 7 then n = n + 1 end
      if f:read(3) == "abc" then n = n + 1 end
      if f:seek("cur", 0) == 10 then n = n + 1 end
      if vf:setvbuf("no") == true then n = n + 1 end
    end
    assert(n == 640)
  end, "Lua 5.4 file read/seek helpers")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok_num, err_num = pcall(function() return f:seek("set", 1.5) end)
      local ok_strnum, err_strnum = pcall(function()
	return f:seek("set", "1.5")
      end)
      local ok_str, err_str = pcall(function() return f:seek("set", "x") end)
      local ok_readnum, err_readnum = pcall(function() return f:read(1.5) end)
      local ok_readfmt, err_readfmt = pcall(function() return f:read("2") end)
      local ok_readtype, err_readtype = pcall(function() return f:read({}) end)
      local ok_write, err_write = pcall(function() return f:write({}) end)
      local ok_iowrite, err_iowrite = pcall(io.write, {})
      local ok_lines, err_lines = pcall(function()
	return it()
      end)
      local ok_lines_alias, err_lines_alias = pcall(function()
	return it2()
      end)
      local ok_lines_global, err_lines_global = pcall(function()
	return lua54_jit_io_global_it()
      end)
      local ok_bufnum, err_bufnum = pcall(function()
	return vf:setvbuf("full", 1.5)
      end)
      local ok_bufstrnum, err_bufstrnum = pcall(function()
	return vf:setvbuf("full", "1.5")
      end)
      local ok_bufstr, err_bufstr = pcall(function()
	return vf:setvbuf("full", "x")
      end)
      if not ok_num and err_num:find("integer representation", 1, true) and
	 not ok_strnum and err_strnum:find("integer representation", 1, true) and
	 not ok_str and err_str:find("bad argument #2 to 'seek'", 1, true) and
	 err_str:find("number expected, got string", 1, true) and
	 not ok_readnum and
	 err_readnum:find("integer representation", 1, true) and
	 not ok_readfmt and
	 err_readfmt:find("bad argument #1 to 'read'", 1, true) and
	 err_readfmt:find("invalid format", 1, true) and
	 not ok_readtype and
	 err_readtype:find("bad argument #1 to 'read'", 1, true) and
	 err_readtype:find("string expected, got table", 1, true) and
	 not ok_write and
	 err_write:find("bad argument #1 to 'write'", 1, true) and
	 err_write:find("string expected, got table", 1, true) and
	 not ok_iowrite and
	 err_iowrite:find("bad argument #1 to 'io.write'", 1, true) and
	 err_iowrite:find("string expected, got table", 1, true) and
	 not ok_lines and
	 err_lines:find("bad argument #2 to 'it'", 1, true) and
	 err_lines:find("string expected, got table", 1, true) and
	 not ok_lines_alias and
	 err_lines_alias:find("bad argument #2 to 'it2'", 1, true) and
	 err_lines_alias:find("string expected, got table", 1, true) and
	 not ok_lines_global and
	 err_lines_global:find("bad argument #2 to 'lua54_jit_io_global_it'",
			       1, true) and
	 err_lines_global:find("string expected, got table", 1, true) and
	 not ok_bufnum and
	 err_bufnum:find("integer representation", 1, true) and
	 not ok_bufstrnum and
	 err_bufstrnum:find("integer representation", 1, true) and
	 not ok_bufstr and
	 err_bufstr:find("bad argument #2 to 'setvbuf'", 1, true) and
	 err_bufstr:find("number expected, got string", 1, true) then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 file seek integer errors")

  f:close()
  vf:close()
  line_closing:close()
  line_closing2:close()
  line_closing3:close()
  lua54_jit_io_global_it = nil
  os.remove(fname)
  os.remove(vfname)
end

do
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      n = n + assert(string.byte("abc", "2"))
      if string.sub("abcdef", "2", "4") == "bcd" then n = n + 1 end
    end
    assert(n == 7920)
  end, "Lua 5.4 string range string-integer args")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok_sub, err_sub = pcall(string.sub, "abc", 1.2)
      local ok_char, err_char = pcall(string.char, 256)
      local ok_byte_method, err_byte_method = pcall(function()
	return ("abc"):byte({})
      end)
      local ok_find_method, err_find_method = pcall(function()
	return ("abc"):find({})
      end)
      local ok_format_method, err_format_method = pcall(function()
	return ("%d"):format(true)
      end)
      if not ok_sub and err_sub:find("integer representation", 1, true) and
	 not ok_char and err_char:find("value out of range", 1, true) then
	n = n + 1
      end
      if not ok_byte_method and
	 err_byte_method:find("bad argument #1 to 'byte'", 1, true) then
	n = n + 1
      end
      if not ok_find_method and
	 err_find_method:find("bad argument #1 to 'find'", 1, true) then
	n = n + 1
      end
      if not ok_format_method and
	 err_format_method:find("bad argument #1 to 'format'", 1, true) then
	n = n + 1
      end
    end
    assert(n == 320)
  end, "Lua 5.4 string integer errors")
end

do
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local first, last = string.find("abcabcabc", "bc", "2", true)
      if first == 2 and last == 3 then n = n + first + last end
      if string.find("abc", "a", "5", true) == nil then n = n + 1 end
    end
    assert(n == 480)
  end, "Lua 5.4 string.find plain string-integer init")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local first, last, cap1, cap2 = string.find("a1 b22", "(%a)(%d+)")
      if first == 1 and last == 2 and cap1 == "a" and cap2 == "1" then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 string.find pattern capture")

  assert_no_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok, err = pcall(string.find, "abc", "b", 1.2, true)
      if not ok and err:find("integer representation", 1, true) then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 string.find init integer error")
end

do
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local seen = 0
      for first, last in string.gmatch("a b cd", "()%s*()", "2") do
	seen = seen + 1
	n = n + first + last
      end
      assert(seen == 4)
    end
    assert(n == 3200)
  end, "Lua 5.4 string.gmatch init empty match")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local out, count = string.gsub("a b cd", " *", "-")
      if out == "-a-b-c-d-" then n = n + count end
      local number_out, number_count = string.gsub("a", "a", 1)
      if number_out == "1" and number_count == 1 then n = n + 1 end
      local table_zero_out, table_zero_count = string.gsub("ab", ".", {
	a = 0,
	b = "x"
      })
      if table_zero_out == "0x" and table_zero_count == 2 then n = n + 1 end
      local table_out, table_count = string.gsub("ab", ".", {
	a = false,
	b = "y"
      })
      if table_out == "ay" and table_count == 2 then n = n + 1 end
      local fn_out, fn_count = string.gsub("ab", ".", function(c)
	if c == "a" then return nil end
	return "y"
      end)
      if fn_out == "ay" and fn_count == 2 then n = n + 1 end
      local limit0_out, limit0_count = string.gsub("aaa", "a", "x", 0)
      if limit0_out == "aaa" and limit0_count == 0 then n = n + 1 end
      local limitstr_out, limitstr_count = string.gsub("aaa", "a", "x", "2")
      if limitstr_out == "xxa" and limitstr_count == 2 then n = n + 1 end
      local limitneg_out, limitneg_count = string.gsub("aaa", "a", "x", -1)
      if limitneg_out == "aaa" and limitneg_count == 0 then n = n + 1 end
      local limitwide_out, limitwide_count =
	string.gsub("aaa", "a", "x", 1099511627776)
      if limitwide_out == "xxx" and limitwide_count == 3 then n = n + 1 end
      local ok_missing, err_missing = pcall(string.gsub, "abc", "a")
      local ok_nil, err_nil = pcall(function()
	return string.gsub("abc", "a", nil)
      end)
      local ok_bool, err_bool = pcall(function()
	local f = string.gsub
	return f("abc", "a", true)
      end)
      local ok_pct, err_pct = pcall(string.gsub, "abc", "a", "%")
      local ok_limitfrac, err_limitfrac =
	pcall(string.gsub, "aaa", "a", "x", 1.2)
      if not ok_missing and
	 err_missing:find("got no value", 1, true) and
	 not ok_nil and err_nil:find("got nil", 1, true) and
	 not ok_bool and err_bool:find("got boolean", 1, true) and
	 not ok_pct and err_pct:find("invalid use of '%'", 1, true) and
	 not ok_limitfrac and
	 err_limitfrac:find("integer representation", 1, true) then
	n = n + 5
      end
    end
    assert(n == 1440)
  end, "Lua 5.4 string.gsub empty match")
end

do
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local word, digits = string.match("abc123", "(%a+)(%d+)", "1")
      if word == "abc" and digits == "123" then n = n + 1 end
      if string.reverse("abcd") == "dcba" then n = n + 1 end
      if string.lower("ABC") == "abc" and string.upper("abc") == "ABC" then
	n = n + 1
      end
      n = n + string.len("abcd")
    end
    assert(n == 560)
  end, "Lua 5.4 string match/case helpers")
end

do
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      if string.rep("a", 3, "-") == "a-a-a" then n = n + 1 end
      if string.rep("xy", 2, "") == "xyxy" then n = n + 1 end
    end
    assert(n == 160)
  end, "Lua 5.4 string.rep separator")

  assert_no_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok, err = pcall(string.rep, "aa", 1073741824)
      if not ok and err:find("resulting string too large", 1, true) then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 string.rep overflow")
end

do
  assert_records_trace(function()
    local n = 0
    for i = 1, 80 do
      local packed = string.pack(">i2", i)
      local value, pos = string.unpack(">i2", packed)
      local first, first_pos = string.unpack("b", "abc", 0)
      n = n + value + pos + first + first_pos
    end
    assert(n == 11400)
  end, "Lua 5.4 string.pack/unpack integer")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok, err = pcall(string.pack, "z", "a\0b")
      if not ok and err:find("string contains zeros", 1, true) then n = n + 1 end
      local ok_num, err_num = pcall(string.pack, "i1")
      local ok_str, err_str = pcall(string.pack, "c1")
      local ok_cfmt, err_cfmt = pcall(string.pack, "c", "x")
      if not ok_num and err_num:find("number expected, got nil", 1, true) and
	 not ok_str and err_str:find("string expected, got nil", 1, true) and
	 not ok_cfmt and
	 err_cfmt:find("missing size for format option 'c'", 1, true) and
	 not err_cfmt:find("bad argument", 1, true) then
	n = n + 1
      end
    end
    assert(n == 160)
  end, "Lua 5.4 string.pack error text")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok, err = pcall(string.unpack, "s1", "")
      if not ok and err:find("data string too short", 1, true) then n = n + 1 end
      local ok_pos0, err_pos0 = pcall(string.unpack, "b", "", 0)
      if not ok_pos0 and
	 err_pos0:find("data string too short", 1, true) then
	n = n + 1
      end
    end
    assert(n == 160)
  end, "Lua 5.4 string.unpack short data")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      n = n + string.packsize("!8bi8")
      if string.packsize("bBhH<i2I2<i4I4fdc4x") == 35 then n = n + 1 end
    end
    assert(n == 1360)
  end, "Lua 5.4 string.packsize fixed formats")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok_var, err_var = pcall(string.packsize, "z")
      local ok_c, err_c = pcall(string.packsize, "c")
      if not ok_var and err_var:find("variable%-length format") and
	 not ok_c and
	 err_c:find("missing size for format option 'c'", 1, true) and
	 not err_c:find("bad argument", 1, true) then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 string.packsize errors")
end

do
  local extended = utf8.char(0x200000)
  local subject = "a"..extended.."z"

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      if utf8.len(subject, 1, -1, true) == 3 then n = n + 1 end
      if utf8.codepoint(subject, 2, 2, true) == 0x200000 then n = n + 1 end
      if utf8.offset(subject, 3) == 7 then n = n + 1 end
      for _, cp in utf8.codes(subject, true) do
	if cp == 97 or cp == 0x200000 or cp == 122 then n = n + 1 end
      end
      if #utf8.char(97, 0x200000) == 6 then n = n + 1 end
      if subject:match("^"..utf8.charpattern..utf8.charpattern..
		       utf8.charpattern.."$") == subject then
	n = n + 1
      end
      local seen = 0
      for ch in subject:gmatch(utf8.charpattern) do
	seen = seen + 1
	if ch == "a" or ch == extended or ch == "z" then n = n + 1 end
      end
      if seen == 3 then n = n + 1 end
    end
    assert(n == 960)
  end, "Lua 5.4 utf8 lax helpers")

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok_char, err_char = pcall(utf8.char, 0x80000000)
      local ok_offset, err_offset = pcall(utf8.offset, "abc", 1.2)
      local ok_codes, err_codes = pcall(function()
	for _ in utf8.codes("\128") do end
      end)
      local len, badpos = utf8.len("\255")
      if not ok_char and err_char:find("value out of range", 1, true) and
	 not ok_offset and err_offset:find("integer representation", 1, true) and
	 not ok_codes and err_codes:find("invalid UTF-8 code", 1, true) and
	 len == nil and badpos == 1 then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 utf8 error helpers")
end

do
  assert_records_ir_calls(function()
    local bigf = 1099511627776.0
    local t = {}
    for i = 1, 80 do
      t[bigf] = i
      t[bigf] = nil
    end
    t[bigf] = "wide"
    local k, v = next(t)
    assert(k == 1099511627776 and math.type(k) == "integer")
    assert(v == "wide" and t[1099511627776] == "wide")
  end, "Lua 5.4 exact int64 float table key",
     { "lj_tab_seti64", "lj_tab_geti64" })

  assert_records_ir_call(function()
    local base = 1099511627776
    local t = { [base] = 7 }
    local n = 0
    for _ = 1, 80 do
      local k = base + 0
      n = n + t[k]
    end
    assert(n == 560)
  end, "Lua 5.4 boxed int64 table key value lookup", "lj_tab_geti64")

  assert_records_ir_call(function()
    local key = 1099511627776
    local t = {}
    local hits = 0
    local function run(v)
      for _ = 1, 80 do
	t[key] = v
	t[key] = nil
      end
    end
    run("direct")
    setmetatable(t, {
      __newindex = function(_, k, v)
	if k == key and v == "via-mm" then hits = hits + 1 end
      end
    })
    run("via-mm")
    assert(hits == 80 and t[key] == nil)
  end, "Lua 5.4 int64 table __newindex guard", "lj_tab_seti64")

  assert_records_ir_call(function()
    local base = 1099511627776
    local t = { [base] = "x", [base + 1] = base }
    local n = 0
    for _ = 1, 80 do
      if table.concat(t, ",", base, base + 1) == "x,1099511627776" then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 table.concat int64 index range", "lj_buf_puttab_i64")

  local numeric = { 1.0, 2, 3.5 }
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      if table.concat(numeric, ",") == "1.0,2,3.5" then n = n + 1 end
    end
    assert(n == 80)
  end, "Lua 5.4 table.concat number subtype")

  local mt = {
    __index = function(_, k)
      if k == 3 then return "c" end
    end
  }
  local t = setmetatable({ "a", "b" }, mt)
  local function concat_loop(expected)
    local n = 0
    for _ = 1, 80 do
      if table.concat(t, ",", 1, 3) == expected then n = n + 1 end
    end
    return n
  end
  assert_records_trace(function()
    assert(concat_loop("a,b,c") == 80)
  end, "Lua 5.4 table.concat __index")
  mt.__index = function(_, k)
    if k == 3 then return "d" end
  end
  assert(concat_loop("a,b,d") == 80)

  do
    local base = 1099511627776
    local proxy = setmetatable({}, {
      __len = function() return base end,
      __index = function(_, k)
	if k == base then return "wide" end
      end
    })
    for _ = 1, 20 do
      assert(table.concat(proxy, ",", base) == "wide")
    end
  end
end

do
  local src_mt = {
    __index = function(_, k)
      if k == 4 then return 40 end
    end
  }
  local dst_mt = {
    __newindex = function(self, k, v)
      rawset(self, k, v * 2)
    end
  }
  local src = setmetatable({ 1, 2, 3 }, src_mt)
  local dst = setmetatable({}, dst_mt)
  local function move_loop(mult)
    local n = 0
    for _ = 1, 80 do
      table.move(src, 1, 4, 1, dst)
      n = n + dst[4]
      for i = 1, 4 do rawset(dst, i, nil) end
    end
    return n == 80 * 40 * mult
  end
  assert_records_trace(function()
    assert(move_loop(2))
  end, "Lua 5.4 table.move metamethods")
  dst_mt.__newindex = function(self, k, v)
    rawset(self, k, v * 3)
  end
  assert(move_loop(3))
end

do
  local mt = {
    __index = function(_, k)
      if k == 2 then return 2 end
    end
  }
  local t = setmetatable({ 1 }, mt)
  local function unpack_loop(extra)
    local n = 0
    for _ = 1, 80 do
      local a, b = table.unpack(t, 1, 2)
      n = n + a + b
    end
    return n == 80 * (1 + extra)
  end
  assert_records_trace(function()
    assert(unpack_loop(2))
  end, "Lua 5.4 table.unpack __index")
  mt.__index = function(_, k)
    if k == 2 then return 5 end
  end
  assert(unpack_loop(5))
end

do
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      if table.concat({ "a", "b", "c" }, ",", "2", "3") == "b,c" then
	n = n + 1
      end

      local inserted = { 1 }
      table.insert(inserted, "2", "x")
      if table.concat(inserted, ",") == "1,x" then n = n + 1 end

      local moved = { 1, 2, 3 }
      if table.move(moved, "1", "2", "2") == moved and
	 table.concat(moved, ",") == "1,1,2" then
	n = n + 1
      end

      local removed = { 1, 2, 3 }
      local value = table.remove(removed, "2")
      if value == 2 and table.concat(removed, ",") == "1,3" then
	n = n + 1
      end

      local a, b = table.unpack({ 1, 2, 3 }, "2", "3")
      if a == 2 and b == 3 then n = n + 1 end
    end
    assert(n == 80 * 5)
  end, "Lua 5.4 table string index helpers")
end

do
  local mt = {
    __newindex = function(self, k, v)
      rawset(self, k, v * 10)
    end
  }
  local t = setmetatable({ 1, 2, 3 }, mt)
  local function insert_loop(mult)
    for _ = 1, 80 do
      table.insert(t, 4, 4)
      assert(t[4] == 4 * mult)
      rawset(t, 4, nil)
    end
  end
  assert_records_trace(function()
    insert_loop(10)
  end, "Lua 5.4 table.insert __newindex")
  mt.__newindex = function(self, k, v)
    rawset(self, k, v * 20)
  end
  insert_loop(20)
end

do
  local mt = {
    __len = function()
      return 2
    end,
    __index = function(_, k)
      if k == 1 then return 10 end
      if k == 2 then return 20 end
    end,
    __newindex = function(self, k, v)
      -- table.remove must keep its Lua 5.4 API get/set path under trace:
      -- shifting index 2 to index 1 should not become a raw array write.
      if v == nil then
	rawset(self, k, nil)
      else
	rawset(self, k, v * 2)
      end
    end
  }
  local t = setmetatable({}, mt)
  local function remove_loop(mult)
    local n = 0
    for _ = 1, 80 do
      rawset(t, 1, nil)
      rawset(t, 2, nil)
      n = n + table.remove(t, 1)
      assert(t[1] == 20 * mult)
    end
    return n == 800
  end
  assert_records_trace(function()
    assert(remove_loop(2))
  end, "Lua 5.4 table.remove metamethods")
  mt.__newindex = function(self, k, v)
    if v == nil then
      rawset(self, k, nil)
    else
      rawset(self, k, v * 3)
    end
  end
  assert(remove_loop(3))
end

do
  assert_records_trace(function()
    local n = 0
    local t = {}
    for _ = 1, 80 do
      t[1], t[2], t[3] = 3, 1, 2
      table.sort(t)
      n = n + t[1] * 100 + t[2] * 10 + t[3]
    end
    assert(n == 9840)
  end, "Lua 5.4 table.sort numeric")

  local mt = {
    __lt = function(a, b)
      return a.v < b.v
    end
  }
  local a = setmetatable({ v = 3 }, mt)
  local b = setmetatable({ v = 1 }, mt)
  local c = setmetatable({ v = 2 }, mt)
  local t = {}
  local function sort_objects_loop(w1, w2, w3)
    local n = 0
    for _ = 1, 80 do
      t[1], t[2], t[3] = a, b, c
      table.sort(t)
      n = n + t[1].v * 100 + t[2].v * 10 + t[3].v
    end
    return n == 80 * (w1 * 100 + w2 * 10 + w3)
  end
  assert_records_trace(function()
    assert(sort_objects_loop(1, 2, 3))
  end, "Lua 5.4 table.sort __lt")
  mt.__lt = function(x, y)
    return x.v > y.v
  end
  assert(sort_objects_loop(3, 2, 1))

  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      local ok, err = pcall(table.sort, { 3, 2, 1, 0 },
			    function(x, y) return x <= y end)
      if not ok and err:find("invalid order function", 1, true) then
	n = n + 1
      end
      local one = { 1 }
      if select("#", table.sort(one, true)) == 0 and one[1] == 1 then
	n = n + 1
      end
      local ok_items, err_items = pcall(table.sort, { {}, {} })
      if not ok_items and
	 err_items:find("attempt to compare two table values", 1, true) then
	n = n + 1
      end
      local ok_cmp, err_cmp = pcall(table.sort, { 2, 1 },
				   function() error("cmp boom", 0) end)
      if not ok_cmp and err_cmp == "cmp boom" then n = n + 1 end
    end
    assert(n == 80 * 4)
  end, "Lua 5.4 table.sort edge errors")

  assert_records_trace(function()
    local base = { 3, 2, 1 }
    local reads, writes = 0, 0
    local proxy = setmetatable({}, {
      __len = function()
	return 3
      end,
      __index = function(_, k)
	reads = reads + 1
	return base[k]
      end,
      __newindex = function(_, k, v)
	writes = writes + 1
	base[k] = v
      end
    })
    for _ = 1, 80 do
      base[1], base[2], base[3] = 3, 2, 1
      table.sort(proxy)
      assert(base[1] == 1 and base[2] == 2 and base[3] == 3)
    end
    assert(reads > 0 and writes > 0)
  end, "Lua 5.4 table.sort proxy")
end

do
  local slots = {}
  local oldmt = debug.getmetatable(0)
  debug.setmetatable(0, {
    __len = function(self)
      return #slots[self]
    end,
    __index = function(self, k)
      return slots[self][k]
    end,
    __newindex = function(self, k, v)
      slots[self][k] = v
    end,
  })
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      slots[0] = { "a", "b" }
      if table.concat(0, ",") == "a,b" then n = n + 1 end

      slots[0] = { "b", "c" }
      table.insert(0, 1, "a")
      if slots[0][1] == "a" and slots[0][3] == "c" then n = n + 1 end
      if table.remove(0, 2) == "b" and slots[0][2] == "c" then n = n + 1 end

      slots[0] = { 3, 1, 2 }
      table.sort(0)
      if slots[0][1] == 1 and slots[0][3] == 3 then n = n + 1 end

      slots[0] = { "u", "v" }
      local a, b = table.unpack(0)
      if a == "u" and b == "v" then n = n + 1 end
      slots[0] = { "x", nil, "z" }
      local c, d, e = table.unpack(0, 1, 3)
      if c == "x" and d == nil and e == "z" then n = n + 1 end
    end
    assert(n == 80 * 6)
  end, "Lua 5.4 table helpers non-table proxy")
  debug.setmetatable(0, oldmt)

  slots = {}
  oldmt = debug.getmetatable(0)
  debug.setmetatable(0, {
    __index = function(self, k)
      return slots[self][k]
    end,
    __newindex = function(self, k, v)
      slots[self][k] = v
    end,
  })
  assert_records_trace(function()
    local n = 0
    for _ = 1, 80 do
      slots[0] = { "x", "y" }
      slots[1] = {}
      local target = table.move(0, 1, 2, 3, 1)
      if target == 1 and slots[1][3] == "x" and slots[1][4] == "y" then
	n = n + 1
      end
    end
    assert(n == 80)
  end, "Lua 5.4 table.move non-table proxy")
  debug.setmetatable(0, oldmt)
end

jitmod.flush()
jitmod.on()
jit.opt.start("hotloop=1", "hotexit=1")

do
  local n = 0
  local weak = {{}}
  setmetatable(weak, { __mode = "kv" })
  while weak[1] and n < 200000 do
    -- The temporary string creates GC pressure; the weak table load must stay
    -- observable because a GC step may clear the slot between loop iterations.
    local s = n .. n .. n .. n
    n = n + 1
  end
  assert(weak[1] == nil, "JIT kept a weak-table load stable across GC")
end
