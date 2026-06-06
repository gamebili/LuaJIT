assert(_VERSION == "Lua 5.4", "lua54_compat53_runtime.lua must run in Lua 5.4 compat mode")
assert(jit and jit.lua54compat == true)

local function near(a, b)
  return math.abs(a - b) < 1e-12
end

local function expect_error(fragment, fn)
  local ok, err = pcall(fn)
  assert(ok == false and err:find(fragment, 1, true), err)
end

for _, name in ipairs{
  "atan2", "pow", "log10", "sinh", "cosh", "tanh", "frexp", "ldexp",
} do
  assert(type(math[name]) == "function", "missing compat math." .. name)
end

assert(near(math.atan2(1, 1), math.atan(1, 1)))
assert(math.pow(2, 3) == 8)
assert(near(math.log10(100), 2))
assert(near(math.sinh(0), 0))
assert(near(math.cosh(0), 1))
assert(near(math.tanh(0), 0))

assert(near(math.atan2("1", "1"), math.atan(1, 1)))
do
  local p = math.pow("2", "3")
  assert(p == 8.0 and math.type(p) == "float")
end
assert(math.log10("100") == 2.0)
assert(math.sinh("0") == 0.0)
assert(math.cosh("0") == 1.0)
assert(math.tanh("0") == 0.0)

local mantissa, exponent = math.frexp(8)
assert(mantissa == 0.5 and exponent == 4)
assert(math.ldexp(mantissa, exponent) == 8)
local mantissa_s, exponent_s = math.frexp("8")
assert(mantissa_s == 0.5 and exponent_s == 4 and
       math.type(mantissa_s) == "float" and math.type(exponent_s) == "integer")
local ldexp_strfloat = math.ldexp(1, "1.0")
assert(ldexp_strfloat == 2.0 and math.type(ldexp_strfloat) == "float")

local ldexp_wide_number = math.ldexp(1, 4294967297)
assert(type(ldexp_wide_number) == "number")
assert(math.ldexp(1, "4294967297") == ldexp_wide_number)
assert(math.ldexp(1, "4294967297.0") == ldexp_wide_number)
do
  local ok, err = pcall(math.ldexp, 1, 1.5)
  assert(ok == false and err:find("number has no integer representation",
				  1, true))
end

expect_error("bad argument #1 to 'math.atan2' (number expected, got no value)",
	     function() return math.atan2() end)
expect_error("bad argument #2 to 'math.pow' (number expected, got table)",
	     function() return math.pow(2, {}) end)
expect_error("bad argument #1 to 'math.log10' (number expected, got no value)",
	     function() return math.log10() end)
expect_error("bad argument #1 to 'math.cosh' (number expected, got boolean)",
	     function() return math.cosh(true) end)
expect_error("bad argument #1 to 'math.frexp' (number expected, got no value)",
	     function() return math.frexp() end)
expect_error("bad argument #2 to 'math.ldexp' (number expected, got no value)",
	     function() return math.ldexp(1) end)

do
  local ok_util, jutil = pcall(require, "jit.util")
  if ok_util then
    local bit = require("bit")
    local vmdef = require("jit.vmdef")

    local function trace_highwater()
      local n = 0
      for i = 1, 1000 do
	if jutil.traceinfo(i) then n = i end
      end
      return n
    end
    jit.off(trace_highwater, true)

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
      jit.off()
      jit.flush()
      collectgarbage()
      jit.on()
      jit.opt.start("hotloop=1", "hotexit=1")
      local before = trace_highwater()
      fn()
      local after = trace_highwater()
      assert(after > before, what .. " did not record a trace")
      assert(trace_has_ir_op(before + 1, after, opname),
	     what .. " did not record IR_" .. opname)
    end

    local function assert_records_ir_call(fn, what, callname)
      jit.off()
      jit.flush()
      collectgarbage()
      jit.on()
      jit.opt.start("hotloop=1", "hotexit=1")
      local before = trace_highwater()
      fn()
      local after = trace_highwater()
      assert(after > before, what .. " did not record a trace")
      assert(trace_has_ir_call(before + 1, after, callname),
	     what .. " did not record " .. callname)
    end

    for _, name in ipairs{
      "atan2", "pow", "log10", "sinh", "cosh", "tanh", "frexp", "ldexp",
    } do
      assert(jutil.funcinfo(math[name]).ffid,
	     "compat math." .. name .. " must keep fast-function id")
    end

    assert_records_ir_op(function()
      local n = 0
      for _ = 1, 80 do
	local r = math.pow(1099511627776, "1")
	if r == 1099511627776.0 and math.type(r) == "float" then
	  n = n + 1
	end
      end
      assert(n == 80)
    end, "compat math.pow int64", "POW")

    assert_records_ir_call(function()
      local n = 0
      local expected = math.atan2(1099511627776, "2")
      for _ = 1, 80 do
	local r = math.atan2(1099511627776, "2")
	if r == expected and math.type(r) == "float" then n = n + 1 end
      end
      assert(n == 80)
    end, "compat math.atan2 int64", "atan2")

    assert_records_ir_call(function()
      local n = 0
      for _ = 1, 80 do
	if math.log10("100") == 2.0 then n = n + 1 end
      end
      assert(n == 80)
    end, "compat math.log10 string", "log10")

    assert_records_ir_op(function()
      local function ldexp_loop(a, e)
	local n = 0
	for _ = 1, 80 do
	  if math.ldexp(a, e) == 2199023255552.0 then
	    n = n + 1
	  end
	end
	return n
      end
      assert(ldexp_loop(1099511627776, 1) == 80)
    end, "compat math.ldexp int64", "LDEXP")

    assert_records_ir_op(function()
      local function ldexp_wide_loop(a, e, se)
	local n = 0
	for _ = 1, 80 do
	  if math.ldexp(a, e) == ldexp_wide_number and
	     math.ldexp(a, se) == ldexp_wide_number then
	    n = n + 1
	  end
	end
	return n
      end
      assert(ldexp_wide_loop(1, 4294967297, "4294967297") == 80)
    end, "compat math.ldexp wide exponent", "LDEXP")

    do
      jit.off()
      jit.flush()
      collectgarbage()
      jit.on()
      jit.opt.start("hotloop=1", "hotexit=1")
      local e = 1.0
      local function ldexp_guard_loop()
	local n = 0
	for _ = 1, 80 do
	  n = n + math.ldexp(1, e)
	end
	return n
      end
      local before = trace_highwater()
      assert(ldexp_guard_loop() == 160)
      assert(trace_highwater() > before,
	     "compat math.ldexp integer guard did not record")
      e = 1.5
      local ok, err = pcall(ldexp_guard_loop)
      assert(ok == false and err:find("number has no integer representation",
				      1, true))
    end
  end
end

do
  local calls = 0
  local mt = {
    __lt = function(a, b)
      calls = calls + 1
      return a.x < b.x
    end,
  }
  local a = setmetatable({ x = 1 }, mt)
  local b = setmetatable({ x = 2 }, mt)
  assert(a <= b)
  assert(calls == 1)

  jit.opt.start("hotloop=1", "hotexit=1")
  local function compat_le_loop(n)
    local ok = true
    for _ = 1, n do
      ok = ok and (a <= b)
    end
    return ok
  end
  assert(compat_le_loop(20))
end

print("lua54_compat53_runtime.lua OK")
