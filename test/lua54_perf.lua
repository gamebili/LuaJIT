local mode_arg = arg and arg[1] or "all"
local unpack = table.unpack or unpack

assert(_VERSION == "Lua 5.4", "lua54_perf.lua must run in Lua 5.4 compat mode")
assert(type(jit) == "table" and type(jit.status) == "function",
       "lua54_perf.lua requires LuaJIT with the jit module available")

local function envnum(name, default)
  local v = os.getenv(name)
  if v == nil or v == "" then return default end
  v = tonumber(v)
  assert(v and v > 0, name.." must be a positive number")
  return v
end

local ratio_limit = envnum("LUA54_PERF_RATIO", 16)
local abs_limit = envnum("LUA54_PERF_ABS", 4.0)
local mem_limit_kb = envnum("LUA54_PERF_MEM_KB", 1024)
local jit_opt_profiles = os.getenv("LUA54_PERF_JIT_OPTS") or
  os.getenv("LUA54_PERF_JIT_OPT") or
  "3,hotloop=3,hotexit=2,instunroll=4,loopunroll=4;3,hotloop=56,hotexit=10;0,hotloop=3,hotexit=2"

local function split_opts(s)
  local out = {}
  for opt in string.gmatch(s, "[^,]+") do
    out[#out+1] = opt
  end
  return out
end

local function split_profiles(s)
  local out = {}
  for profile in string.gmatch(s, "[^;]+") do
    -- LuaJIT optimizer flags heavily dominate timing. Require every perf
    -- profile to pin the optimization level and trace trigger thresholds, so
    -- a local override cannot accidentally inherit stale process defaults.
    local opts = split_opts(profile)
    local has_level, has_hotloop, has_hotexit = false, false, false
    for i = 1, #opts do
      local opt = opts[i]
      has_level = has_level or not not string.match(opt, "^[0-3]$")
      has_hotloop = has_hotloop or not not string.match(opt, "^hotloop=")
      has_hotexit = has_hotexit or not not string.match(opt, "^hotexit=")
    end
    assert(has_level,
           "each LUA54_PERF_JIT_OPTS profile must pin opt level 0..3: "..profile)
    assert(has_hotloop and has_hotexit,
           "each LUA54_PERF_JIT_OPTS profile must pin hotloop and hotexit: "..profile)
    out[#out+1] = profile
  end
  assert(#out > 0, "LUA54_PERF_JIT_OPTS must contain at least one profile")
  return out
end

local data = {}
for i = 1, 128 do
  data["k"..i] = i
end
local meta_data = setmetatable({}, {
  __pairs = function()
    return next, data, nil
  end,
})
local raw_meta_subject = setmetatable({ 1, 2 }, {
  __metatable = "locked",
  __len = function() return 99 end,
  __index = function() return 99 end,
  __newindex = function() error("rawset used __newindex") end,
})
local raw_meta_peer = {}
local value_meta = {
  __name = "Lua54Value",
  __tostring = function(o) return o.label end,
}
local value_object = setmetatable({ label = "value" }, value_meta)
local named_value_object = setmetatable({}, { __name = "Lua54Name" })
local load_mode_binary = string.dump(function() return 54 end)
local protected_marker = {}

local function set_global_abs_alias()
  lua54_perf_global_abs_alias = math.abs
end
set_global_abs_alias()

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
  src[#src + 1] = "  return sum\nend\n"
  return assert(load(table.concat(src), "=lua54-perf-many-upvalues", "t"))()
end

local many_upvalue_counter = make_many_upvalue_counter()
local many_upvalue_seed = 149 * 150 / 2
local many_upvalue_last = 150

local function many_upvalue_helpers(n)
  local first = many_upvalue_last + 1
  local last = many_upvalue_last + n
  local expected = n * many_upvalue_seed + (first + last) * n / 2
  local sum = many_upvalue_counter(n)
  many_upvalue_last = last
  -- Keep a high-index mutable upvalue in the perf window. Lua 5.4 allows 200
  -- upvalues, so the recorder should not permanently exit at index >= 128.
  if sum == expected then return n end
  return -1
end

local lua54_perf_table_abs_alias = {}
local function set_table_abs_alias()
  lua54_perf_table_abs_alias.f = math.abs
end
set_table_abs_alias()
lua54_perf_table_global_alias = lua54_perf_table_abs_alias
lua54_perf_table_global_alias.h = math.abs

local lua54_perf_table_field_source = {}
local lua54_perf_table_field_holder = {}
lua54_perf_table_field_holder.inner = lua54_perf_table_field_source
lua54_perf_table_field_holder.inner.k = math.abs

local lua54_perf_table_ctor_source = {}
local lua54_perf_table_ctor_holder = {
  inner = lua54_perf_table_ctor_source,
}
lua54_perf_table_ctor_holder.inner.l = math.abs

local lua54_perf_table_rhs_source = {}
local lua54_perf_table_rhs_base = {}
lua54_perf_table_rhs_base.inner = lua54_perf_table_rhs_source
local lua54_perf_table_rhs_ctor_holder = {
  alias = lua54_perf_table_rhs_base.inner,
}
lua54_perf_table_rhs_ctor_holder.alias.m = math.abs
local lua54_perf_table_rhs_assign_holder = {}
lua54_perf_table_rhs_assign_holder.alias = lua54_perf_table_rhs_base.inner
lua54_perf_table_rhs_assign_holder.alias.n = math.abs

local function protected_add(a, b)
  return a + b
end

local function protected_fail_marker()
  error(protected_marker, 0)
end

local function protected_fail_string()
  error("lua54 protected boom", 0)
end

local function protected_handler(err)
  return { handled = err }
end

local function next_sum(n)
  local sum = 0
  for _ = 1, n do
    local k, v = next(data, nil)
    while k ~= nil do
      sum = sum + v
      k, v = next(data, k)
    end
  end
  return sum
end

local function pairs_sum(n)
  local sum = 0
  for _ = 1, n do
    for _, v in pairs(data) do
      sum = sum + v
    end
  end
  return sum
end

local function metapairs_sum(n)
  local sum = 0
  for _ = 1, n do
    for _, v in pairs(meta_data) do
      sum = sum + v
    end
  end
  return sum
end

local function base_raw_helpers(n)
  local sum = 0
  local p = pairs
  for _ = 1, n do
    if getmetatable(raw_meta_subject) == "locked" then sum = sum + 1 end
    sum = sum + rawlen(raw_meta_subject)
    if rawget(raw_meta_subject, 3) == nil then sum = sum + 1 end
    rawset(raw_meta_subject, 3, 7)
    sum = sum + rawget(raw_meta_subject, 3)
    rawset(raw_meta_subject, 3, nil)
    if not rawequal(raw_meta_subject, raw_meta_peer) then sum = sum + 1 end
    if select("#", next({})) == 1 and next({}) == nil then sum = sum + 1 end
    do
      local iter, state, index = ipairs({})
      if select("#", iter(state, index)) == 1 and iter(state, index) == nil then
	sum = sum + 1
      end
    end
    do
      local ok_pairs, err_pairs = pcall(function()
	local iter, state, key = p(nil)
	return iter(state, key)
      end)
      if not ok_pairs and
	 err_pairs:find("bad argument #1 to 'iter'", 1, true) then
	sum = sum + 1
      end
    end
  end
  return sum
end

local function base_value_helpers(n)
  local sum = 0
  for _ = 1, n do
    if type(value_object) == "table" then sum = sum + 1 end
    if tostring(value_object) == "value" then sum = sum + 1 end
    if tostring(nil) == "nil" then sum = sum + 1 end
    if tostring(named_value_object):find("Lua54Name:", 1, true) == 1 then
      sum = sum + 1
    end
    sum = sum + select("#", "a", nil, "c")
    local a, b = select("2", "a", "b", "c")
    if a == "b" and b == "c" then sum = sum + 1 end
    local packed = table.pack("a", nil, "c")
    if packed.n == 3 and packed[1] == "a" and packed[2] == nil and
       packed[3] == "c" then
      sum = sum + 1
    end
    if assert(true, "ok") == true then sum = sum + 1 end
    local ok_abs, err_abs = pcall(function()
      return lua54_perf_global_abs_alias(true)
    end)
    if not ok_abs and
       err_abs:find("bad argument #1 to 'lua54_perf_global_abs_alias'",
		    1, true) then
      sum = sum + 1
    end
    local ok_table_abs, err_table_abs = pcall(function()
      return lua54_perf_table_abs_alias.f(true)
    end)
    if not ok_table_abs and
       err_table_abs:find("bad argument #1 to 'f'", 1, true) then
      sum = sum + 1
    end
    local ok_global_table_abs, err_global_table_abs = pcall(function()
      return lua54_perf_table_global_alias.h(true)
    end)
    if not ok_global_table_abs and
       err_global_table_abs:find("bad argument #1 to 'h'", 1, true) then
      sum = sum + 1
    end
    local ok_table_field_source, err_table_field_source = pcall(function()
      return lua54_perf_table_field_source.k(true)
    end)
    if not ok_table_field_source and
       err_table_field_source:find("bad argument #1 to 'k'", 1, true) then
      sum = sum + 1
    end
    local ok_table_field_read, err_table_field_read = pcall(function()
      return lua54_perf_table_field_holder.inner.k(true)
    end)
    if not ok_table_field_read and
       err_table_field_read:find("bad argument #1 to 'k'", 1, true) then
      sum = sum + 1
    end
    local ok_table_ctor_source, err_table_ctor_source = pcall(function()
      return lua54_perf_table_ctor_source.l(true)
    end)
    if not ok_table_ctor_source and
       err_table_ctor_source:find("bad argument #1 to 'l'", 1, true) then
      sum = sum + 1
    end
    local ok_table_ctor_read, err_table_ctor_read = pcall(function()
      return lua54_perf_table_ctor_holder.inner.l(true)
    end)
    if not ok_table_ctor_read and
       err_table_ctor_read:find("bad argument #1 to 'l'", 1, true) then
      sum = sum + 1
    end
    local ok_table_rhs_ctor_source, err_table_rhs_ctor_source =
      pcall(function()
	return lua54_perf_table_rhs_source.m(true)
      end)
    if not ok_table_rhs_ctor_source and
       err_table_rhs_ctor_source:find("bad argument #1 to 'm'", 1, true) then
      sum = sum + 1
    end
    local ok_table_rhs_ctor_read, err_table_rhs_ctor_read = pcall(function()
      return lua54_perf_table_rhs_ctor_holder.alias.m(true)
    end)
    if not ok_table_rhs_ctor_read and
       err_table_rhs_ctor_read:find("bad argument #1 to 'm'", 1, true) then
      sum = sum + 1
    end
    local ok_table_rhs_assign_source, err_table_rhs_assign_source =
      pcall(function()
	return lua54_perf_table_rhs_source.n(true)
      end)
    if not ok_table_rhs_assign_source and
       err_table_rhs_assign_source:find("bad argument #1 to 'n'", 1, true) then
      sum = sum + 1
    end
    local ok_table_rhs_assign_read, err_table_rhs_assign_read =
      pcall(function()
	return lua54_perf_table_rhs_assign_holder.alias.n(true)
      end)
    if not ok_table_rhs_assign_read and
       err_table_rhs_assign_read:find("bad argument #1 to 'n'", 1, true) then
      sum = sum + 1
    end
    local text_fn, text_err = load("return 54", "lua54-load-text", "b")
    local bin_fn, bin_err = load(load_mode_binary, "lua54-load-bin", "t")
    if text_fn == nil and
       text_err:find("attempt to load a text chunk (mode is 'b')", 1, true) and
       bin_fn == nil and
       bin_err:find("attempt to load a binary chunk (mode is 't')", 1, true) then
      sum = sum + 1
    end
  end
  return sum
end

local function protected_call_helpers(n)
  local sum = 0
  for _ = 1, n do
    local ok_add, add_value = pcall(protected_add, 4, 5)
    if ok_add then sum = sum + add_value end
    local ok_marker, err_marker = pcall(protected_fail_marker)
    if not ok_marker and err_marker == protected_marker then sum = sum + 1 end
    local ok_string, err_string = pcall(protected_fail_string)
    if not ok_string and err_string == "lua54 protected boom" then
      sum = sum + 1
    end
    local ok_xadd, xadd_value = xpcall(protected_add, protected_handler, 6, 7)
    if ok_xadd then sum = sum + xadd_value end
    local ok_xfail, handled = xpcall(protected_fail_marker, protected_handler)
    if not ok_xfail and handled.handled == protected_marker then sum = sum + 1 end
    local ok_error, err_error = pcall(error, protected_marker, 1)
    if not ok_error and err_error == protected_marker then sum = sum + 1 end
  end
  return sum
end

local coroutine_marker = {}

local function result_count(...)
  return select("#", ...), ...
end

local function coroutine_tail_status_nil()
  return coroutine.status(nil)
end

local function coroutine_tail_create_nil()
  return coroutine.create(nil)
end

local function coroutine_tail_wrap_nil()
  return coroutine.wrap(nil)
end

local function coroutine_tail_isyieldable_nil()
  return coroutine.isyieldable(nil)
end

local function coroutine_helpers(n)
  local sum = 0
  for i = 1, n do
    local mainco, ismain = coroutine.running()
    if ismain and coroutine.status(mainco) == "running" and
       not coroutine.isyieldable(mainco) then
      sum = sum + 1
    end

    local co = coroutine.create(function(a, b)
      return a + b, nil, "x"
    end)
    local ok, a, b, c = coroutine.resume(co, i, 2)
    if ok and a == i + 2 and b == nil and c == "x" and
       coroutine.status(co) == "dead" then
      sum = sum + 1
    end

    co = coroutine.create(function()
      coroutine.yield("pause")
    end)
    local ok_yield, value = coroutine.resume(co)
    local was_suspended = coroutine.status(co) == "suspended"
    local close_n, close_ok = result_count(coroutine.close(co))
    if ok_yield and value == "pause" and was_suspended and
       close_n == 1 and close_ok == true and
       coroutine.status(co) == "dead" then
      sum = sum + 1
    end

    co = coroutine.create(function()
      local x <close> = setmetatable({}, {
	__close = function()
	  error(coroutine_marker, 0)
	end
      })
      coroutine.yield("pause")
    end)
    assert(coroutine.resume(co))
    local ok_close, err_close = coroutine.close(co)
    if not ok_close and err_close == coroutine_marker and
       coroutine.status(co) == "dead" then
      sum = sum + 1
    end

    local log = {}
    local wrapped = coroutine.wrap(function()
      local x <close> = setmetatable({}, {
	__close = function()
	  log[1] = "closed"
	end
      })
      error(coroutine_marker, 0)
    end)
    local ok_wrap, err_wrap = pcall(wrapped)
    if not ok_wrap and err_wrap == coroutine_marker and log[1] == "closed" then
      sum = sum + 1
    end

    -- Keep wrapper error-name recovery in the JIT on/off perf window.  These
    -- calls must stay source-named even though the wrapper itself tail-returns.
    local ok_status, err_status = pcall(coroutine_tail_status_nil)
    local ok_create, err_create = pcall(coroutine_tail_create_nil)
    local ok_wrap_nil, err_wrap_nil = pcall(coroutine_tail_wrap_nil)
    local ok_yieldable, err_yieldable = pcall(coroutine_tail_isyieldable_nil)
    if not ok_status and
       err_status:find("bad argument #1 to 'status'", 1, true) then
      sum = sum + 1
    end
    if not ok_create and
       err_create:find("bad argument #1 to 'create'", 1, true) then
      sum = sum + 1
    end
    if not ok_wrap_nil and
       err_wrap_nil:find("bad argument #1 to 'wrap'", 1, true) then
      sum = sum + 1
    end
    if not ok_yieldable and
       err_yieldable:find("bad argument #1 to 'isyieldable'", 1, true) then
      sum = sum + 1
    end
  end
  return sum
end

local function gc_mode_helpers(n)
  local sum = 0
  collectgarbage("restart")
  collectgarbage("generational")
  for i = 1, n do
    local old_mode = collectgarbage((i % 2 == 0) and
				    "generational" or "incremental")
    if old_mode == "generational" or old_mode == "incremental" then
      sum = sum + 1
    end
    if collectgarbage("isrunning", true) then sum = sum + 1 end
    if type(collectgarbage("count", true)) == "number" then sum = sum + 1 end
    local stepped = collectgarbage("step", "1")
    if stepped == true or stepped == false then sum = sum + 1 end
    -- Mode tuning arguments are currently a compatibility shim over LuaJIT's
    -- collector, but they must keep Lua 5.4's integer-checking surface in hot
    -- loops instead of becoming ignored varargs.
    local old_mode_with_args = collectgarbage((i % 2 == 0) and
					     "generational" or "incremental",
					     "20", "30", "4")
    if old_mode_with_args == "generational" or
       old_mode_with_args == "incremental" then
      sum = sum + 1
    end
    local ok_step, err_step = pcall(collectgarbage, "step", 1.2)
    if not ok_step and err_step:find("integer representation", 1, true) then
      sum = sum + 1
    end
    local ok_mode, err_mode = pcall(collectgarbage, "incremental",
				    200, 300, 12.5)
    if not ok_mode and err_mode:find("bad argument #4 to 'collectgarbage'",
				     1, true) then
      sum = sum + 1
    end
  end
  return sum
end

local function package_helpers(n)
  local sum = 0
  package.loaded.__lua54_perf_loaded = { v = 7 }

  local nil_loader_count = 0
  package.loaded.__lua54_perf_nil = nil
  package.preload.__lua54_perf_nil = function()
    nil_loader_count = nil_loader_count + 1
    return nil
  end

  package.loaded.__lua54_perf_err = nil
  package.preload.__lua54_perf_err = function()
    error("lua54 package perf boom", 0)
  end

  for _ = 1, n do
    local loaded, loaded_data = require("__lua54_perf_loaded")
    if loaded.v == 7 and loaded_data == nil then sum = sum + 1 end

    local nil_loaded, nil_data = require("__lua54_perf_nil")
    if nil_loaded == true and (nil_data == nil or nil_data == ":preload:") then
      sum = sum + 1
    end

    local ok_err, err = pcall(require, "__lua54_perf_err")
    if not ok_err and err == "lua54 package perf boom" and
       package.loaded.__lua54_perf_err == nil then
      sum = sum + 1
    end

    local found, searcherr = package.searchpath("__lua54_perf_missing__",
						"?.lua;;none/?.lua")
    if found == nil and searcherr:find("no file ''", 1, true) then
      sum = sum + 1
    end

    found, searcherr = package.searchpath("__lua54_perf_missing__", "")
    if found == nil and searcherr == "no file ''" then
      sum = sum + 1
    end

    found, searcherr = package.searchpath(1099511627776, "?.lua")
    if found == nil and searcherr:find("1099511627776.lua", 1, true) then
      sum = sum + 1
    end
  end

  package.loaded.__lua54_perf_loaded = nil
  package.loaded.__lua54_perf_nil = nil
  package.preload.__lua54_perf_nil = nil
  package.preload.__lua54_perf_err = nil
  assert(nil_loader_count == 1)
  return sum
end

local function debug_helpers(n)
  local sum = 0
  local fn = function() end
  local old_cstack = debug.setcstacklimit(200)
  for _ = 1, n do
    if select("#", debug.upvalueid(fn, 0)) == 1 and
       debug.upvalueid(fn, 0) == nil then
      sum = sum + 1
    end
    if select("#", debug.setupvalue(fn, 1, "hidden")) == 0 and
       select("#", debug.setupvalue(print, 1, "hidden")) == 0 then
      sum = sum + 1
    end
    if debug.setcstacklimit(200) == 200 then sum = sum + 1 end
    if select("#", debug.gethook()) == 1 and debug.gethook() == nil then
      sum = sum + 1
    end
    debug.sethook(function() end, "", 0)
    if select("#", debug.gethook()) == 1 and debug.gethook() == nil then
      sum = sum + 1
    end
    local co = coroutine.create(function() end)
    if select("#", debug.gethook(co)) == 1 and debug.gethook(co) == nil then
      sum = sum + 1
    end
    if select("#", debug.getupvalue(print, 1)) == 0 and
       select("#", debug.getupvalue(pairs, 1)) == 0 and
       select("#", debug.getupvalue(ipairs, 1)) == 0 and
       select("#", debug.upvalueid(print, 1)) == 1 and
       debug.upvalueid(print, 1) == nil then
      sum = sum + 1
    end
  end
  debug.setcstacklimit(old_cstack)
  return sum
end

local os_time_stamp = os.time({
  year = 2020, month = 5, day = 7, hour = 12, min = 34, sec = 56
})
assert(math.type(os_time_stamp) == "integer")
local os_future_fields = {
  year = 2039, month = 1, day = 2, hour = 12, min = 34, sec = 56,
  isdst = false
}
local os_future_ok, os_future_stamp = pcall(os.time, os_future_fields)
local os_future_supported = os_future_ok and os_future_stamp > 2147483647
local os_path_present = os.getenv("PATH") ~= nil

local function os_helpers(n)
  local sum = 0
  for i = 1, n do
    if math.type(os_time_stamp) == "integer" then sum = sum + 1 end
    if os.date("%Y-%m-%d", os_time_stamp) == "2020-05-07" then
      sum = sum + 1
    end
    if os.date("!\0\0", os_time_stamp) == "\0\0" then sum = sum + 1 end
    if os.date(1099511627776, os_time_stamp) == "1099511627776" then
      sum = sum + 1
    end
    if os.difftime(os_time_stamp + 7, os_time_stamp) == 7 then
      sum = sum + 1
    end
    if os_future_supported and math.type(os_future_stamp) == "integer" and
       os.difftime(os_future_stamp + 7, os_future_stamp) == 7 and
       os.difftime(tostring(os_future_stamp + 7),
		   tostring(os_future_stamp)) == 7 and
       os.date("%Y-%m-%d", os_future_stamp) == "2039-01-02" then
      sum = sum + 1
    end

    -- Keep os.time(table) in the perf window, but avoid midnight/DST edges so
    -- the test measures compatibility overhead instead of platform timezone
    -- normalization quirks.
    local sec = (i % 50) + 1
    local fields = {
      year = 2020, month = 5, day = 7, hour = 12, min = 34, sec = sec
    }
    local value = os.time(fields)
    if math.type(value) == "integer" and fields.year == 2020 and
       fields.month == 5 and fields.day == 7 and fields.hour == 12 and
       fields.min == 34 and fields.sec == sec and fields.yday == 128 then
      sum = sum + 1
    end

    if (os.getenv("PATH") ~= nil) == os_path_present then sum = sum + 1 end
    if type(os.setlocale(nil, "time")) == "string" then sum = sum + 1 end

    local ok_time, err_time = pcall(os.time, {
      year = 2020, month = 5, day = 7, hour = 1.5
    })
    if not ok_time and
       err_time:find("field 'hour' is not an integer", 1, true) then
      sum = sum + 1
    end

    local ok_date, err_date = pcall(os.date, true)
    if not ok_date and err_date:find("to 'os.date'", 1, true) then
      sum = sum + 1
    end

    local ok_date_time, err_date_time = pcall(os.date, "%Y", 1.5)
    if not ok_date_time and
       err_date_time:find("integer representation", 1, true) then
      sum = sum + 1
    end

    local ok_diff1, err_diff1 = pcall(os.difftime, 2.5, 1)
    local ok_diff2, err_diff2 = pcall(os.difftime, 2, 1.5)
    local ok_diff_missing, err_diff_missing = pcall(os.difftime, 1)
    local ok_diff_noargs, err_diff_noargs = pcall(os.difftime)
    local ok_diff_nil, err_diff_nil = pcall(function()
      return os.difftime(nil)
    end)
    if not ok_diff1 and err_diff1:find("integer representation", 1, true) and
       not ok_diff2 and err_diff2:find("integer representation", 1, true) and
       not ok_diff_missing and
       err_diff_missing:find("bad argument #2 to 'os.difftime'", 1, true) and
       not ok_diff_noargs and
       err_diff_noargs:find("bad argument #1 to 'os.difftime'", 1, true) and
       not ok_diff_nil and
       err_diff_nil:find("bad argument #1 to 'difftime'", 1, true) then
      sum = sum + 1
    end

    local ok_date_huge, err_date_huge = pcall(os.date, "%Y", 2^60)
    local ok_time_repr, err_time_repr = pcall(os.time, {
      year = 4000, month = 1, day = 1
    })
    if not ok_date_huge and
       err_date_huge:find("date result cannot be represented", 1, true) and
       not ok_time_repr and
       err_time_repr:find("time result cannot be represented", 1, true) then
      sum = sum + 1
    end
  end
  return sum
end

local function io_helpers(n)
  local fname = "lua54_perf_io.tmp"
  local vfname = "lua54_perf_io_setvbuf.tmp"
  local f = assert(io.open(fname, "w+b"))
  local vf = assert(io.open(vfname, "w"))
  f:write("123\n16\nabc\n")
  f:flush()
  local it, _, _, line_closing = io.lines(fname, {})
  local it2_src, _, _, line_closing2 = io.lines(fname, {})
  local itg_src, _, _, line_closing3 = io.lines(fname, {})
  local it2 = it2_src
  lua54_perf_io_global_it = itg_src
  local sum = 0
  for _ = 1, n do
    if f:seek("set", 0) == 0 then sum = sum + 1 end
    local a = f:read("n")
    if a == 123 and math.type(a) == "integer" then sum = sum + 1 end
    if f:seek("set", "4") == 4 then sum = sum + 1 end
    local b = f:read("n")
    if b == 16 and math.type(b) == "integer" then sum = sum + 1 end
    if f:seek("set", 7) == 7 then sum = sum + 1 end
    if f:read(3) == "abc" then sum = sum + 1 end
    if f:seek("cur", 0) == 10 then sum = sum + 1 end
    if vf:setvbuf("no") == true then sum = sum + 1 end

    -- Lua 5.4 seek offsets are lua_Integer values.  Keep the strict
    -- conversion in the perf window so file-position and buffering helpers
    -- cannot silently truncate fractional numbers under JIT on/off profiles.
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
      return lua54_perf_io_global_it()
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
       err_lines_global:find("bad argument #2 to 'lua54_perf_io_global_it'",
			     1, true) and
       err_lines_global:find("string expected, got table", 1, true) and
       not ok_bufnum and err_bufnum:find("integer representation", 1, true) and
       not ok_bufstrnum and
       err_bufstrnum:find("integer representation", 1, true) and
       not ok_bufstr and
       err_bufstr:find("bad argument #2 to 'setvbuf'", 1, true) and
       err_bufstr:find("number expected, got string", 1, true) then
      sum = sum + 1
    end
  end
  f:close()
  vf:close()
  line_closing:close()
  line_closing2:close()
  line_closing3:close()
  lua54_perf_io_global_it = nil
  os.remove(fname)
  os.remove(vfname)
  return sum
end

local function number_pack_helpers(n)
  local sum = 0
  local float_input = "1.5"
  local int_input = "0xff"
  local bad_input = "nan"
  local bad_word_input = "not-a-number"
  local bad_suffix_input = "123abc"
  local comma_const_input = "3,4"
  local base16_input = "ff"
  local base16_bad_prefix = "0x10"
  local base10_bad_float = "1.0"
  local base10_dynamic_input = "42"
  local base16_dynamic_num = 16.0
  local base10_dynamic_string = "10"
  local base16_dynamic_string = "0x10"
  local tointeger_wide = "1099511627776"
  local tointeger_max = "9223372036854775807"
  local tointeger_over = "9223372036854775808"
  local old_numeric = os.setlocale(nil, "numeric")
  assert(os.setlocale("C", "numeric"))
  for _ = 1, n do
    if math.type(1) == "integer" then sum = sum + 1 end
    if math.type(1.0) == "float" then sum = sum + 1 end
    if math.type("1") == nil then sum = sum + 1 end
    -- No-base tonumber() has a separate recorder surface from explicit-base
    -- conversion: it must preserve integer/float subtypes and reject LuaJIT
    -- numeric extensions under the fixed jit.opt perf profiles.
    if math.type(tonumber(float_input)) == "float" then sum = sum + 1 end
    if math.type(tonumber(int_input)) == "integer" then sum = sum + 1 end
    if tonumber(bad_input) == nil then sum = sum + 1 end
    if tonumber(bad_word_input) == nil then sum = sum + 1 end
    if tonumber(bad_suffix_input) == nil then sum = sum + 1 end
    -- Locale-sensitive comma constants must stay in the perf window. Force C
    -- locale here so this helper has a deterministic expected count.
    if tonumber(comma_const_input) == nil then sum = sum + 1 end
    sum = sum + assert(math.tointeger("123"))
    local ti_wide = assert(math.tointeger(tointeger_wide))
    local ti_max = assert(math.tointeger(tointeger_max))
    if ti_wide == 1099511627776 and ti_max == math.maxinteger and
       math.type(ti_wide) == "integer" and math.type(ti_max) == "integer" then
      sum = sum + 1
    end
    if math.abs(-ti_wide) == ti_wide and math.type(math.abs(-ti_wide)) == "integer" then
      sum = sum + 1
    end
    if math.tointeger(tointeger_over) == nil then sum = sum + 1 end
    if math.tointeger(1.5) == nil then sum = sum + 1 end
    if math.ult(1, -1) and not math.ult(-1, 1) then sum = sum + 1 end
    local ok_fmod_missing, err_fmod_missing = pcall(math.fmod)
    local ok_fmod_nil, err_fmod_nil = pcall(function()
      return math.fmod(nil)
    end)
    local ok_fmod_nil_present, err_fmod_nil_present =
      pcall(math.fmod, nil, 1)
    if not ok_fmod_missing and
       err_fmod_missing:find("bad argument #2 to 'math.fmod'", 1, true) and
       not ok_fmod_nil and
       err_fmod_nil:find("bad argument #2 to 'fmod'", 1, true) and
       not ok_fmod_nil_present and
       err_fmod_nil_present:find("bad argument #1 to 'math.fmod'", 1, true) then
      sum = sum + 1
    end
    -- Keep explicit-base tonumber in the performance window. The old decimal
    -- recorder must not bypass the Lua 5.4 integer-base scanner here.
    if tonumber("\t10000000000\t", 10) == 10000000000 then sum = sum + 1 end
    if tonumber("1.0", 10) == nil then sum = sum + 1 end
    if math.type(tonumber(base16_input, 16)) == "integer" then
      sum = sum + 1
    end
    if tonumber(base16_bad_prefix, 16) == nil then sum = sum + 1 end
    if tonumber(base10_bad_float, 10) == nil then sum = sum + 1 end
    -- Dynamic base values must stay in the perf window too. They exercise the
    -- same luaL_checkinteger() coercion surface as the interpreter.
    if tonumber(base16_input, base16_dynamic_num) == 255 then sum = sum + 1 end
    if tonumber(base10_dynamic_input, base10_dynamic_string) == 42 then
      sum = sum + 1
    end
    if tonumber(base16_input, base16_dynamic_string) == 255 then
      sum = sum + 1
    end
    sum = sum + string.packsize("!8bi8")
    local ok, err = pcall(string.packsize, "z")
    if not ok and err:find("variable%-length format") then sum = sum + 1 end
    local unpacked, unpack_pos = string.unpack("b", "abc", 0)
    if unpacked == 97 and unpack_pos == 2 then sum = sum + 1 end
    local ok_unpack, err_unpack = pcall(string.unpack, "b", "", 0)
    if not ok_unpack and
       err_unpack:find("data string too short", 1, true) then
      sum = sum + 1
    end
    local ok_pack_num, err_pack_num = pcall(string.pack, "i1")
    local ok_pack_str, err_pack_str = pcall(string.pack, "c1")
    if not ok_pack_num and
       err_pack_num:find("number expected, got nil", 1, true) and
       not ok_pack_str and
       err_pack_str:find("string expected, got nil", 1, true) then
      sum = sum + 1
    end
  end
  if old_numeric then os.setlocale(old_numeric, "numeric") end
  return sum
end

local concat_numbers = { 1.0, 2, 3.5 }

local function number_string_helpers(n)
  local sum = 0
  for _ = 1, n do
    -- Lua 5.4 preserves integer vs float subtype when numbers become strings.
    -- Keep tostring, concat and table.concat in the perf window so the compat
    -- formatting path cannot regress silently under different jit.opt profiles.
    if tostring(1) == "1" then sum = sum + 1 end
    if tostring(1.0) == "1.0" then sum = sum + 1 end
    if ("x" .. 1.0) == "x1.0" then sum = sum + 1 end
    if table.concat(concat_numbers, ",") == "1.0,2,3.5" then sum = sum + 1 end
    if table.concat({ "a", "b" }, 1099511627776) ==
       "a1099511627776b" then sum = sum + 1 end
  end
  return sum
end

local function random_helpers(n)
  local sum = 0
  local seed1, seed2 = math.randomseed(1099511627776, "1")
  local full_probe = math.random(0)
  if full_probe == -7928649372492011025 and
     math.type(full_probe) == "integer" then sum = sum + 1 end
  for _ = 1, n do
    -- Keep all Lua 5.4 random recorder surfaces in the perf window: floats,
    -- full-width integer samples, and constant/dynamic integer intervals.
    if seed1 == 1099511627776 and seed2 == 1 and
       math.type(seed1) == "integer" then sum = sum + 1 end
    local bounded = math.random(1, 4)
    if math.type(bounded) == "integer" and bounded >= 1 and bounded <= 4 then
      sum = sum + 1
    end
    if math.type(math.random(0)) == "integer" then sum = sum + 1 end
    local f = math.random()
    if f >= 0 and f < 1 then sum = sum + 1 end
    local low = _
    local dyn = math.random(low, low + 3)
    if math.type(dyn) == "integer" and dyn >= low and dyn <= low + 3 then
      sum = sum + 1
    end
    local single_up = _
    local dyn_one = math.random(single_up)
    if math.type(dyn_one) == "integer" and
       dyn_one >= 1 and dyn_one <= single_up then
      sum = sum + 1
    end
    local low_s = tostring(_)
    local up_s = tostring(_ + 3)
    local dyn_s = math.random(low_s, up_s)
    if math.type(dyn_s) == "integer" and
       dyn_s >= _ and dyn_s <= _ + 3 then
      sum = sum + 1
    end
    local single_s = tostring(_)
    local dyn_one_s = math.random(single_s)
    if math.type(dyn_one_s) == "integer" and
       dyn_one_s >= 1 and dyn_one_s <= _ then
      sum = sum + 1
    end
  end
  return sum
end

local order_a = "lua54-perf-order\0a"
local order_b = "lua54-perf-order\0b"

local function string_order_score(a, b)
  local score = 0
  if a < b then score = score + 1 end
  if a <= b then score = score + 1 end
  if b > a then score = score + 1 end
  if b >= a then score = score + 1 end
  if not (b < a) then score = score + 1 end
  if not (a > b) then score = score + 1 end
  return score
end

local string_order_expected = string_order_score(order_a, order_b)

local function string_order_helpers(n)
  local sum = 0
  for _ = 1, n do
    -- Lua 5.4 string ordering follows the active collate locale. Keep both
    -- taken and not-taken ordered comparison guards in the perf/memory smoke.
    sum = sum + string_order_score(order_a, order_b)
  end
  return sum
end

local function clone_long_string(s)
  return s:sub(1, 41) .. s:sub(42)
end

local long_key_seed = ("lua54-perf-long-key-"):rep(4)
local long_key_a = clone_long_string(long_key_seed)
local long_key_b = clone_long_string(long_key_seed)
assert(#long_key_a > 40 and long_key_a == long_key_b)
local long_string_keys = {}
for i = 1, 128 do
  long_string_keys[i] = (i % 2 == 0) and long_key_a or long_key_b
end
local long_string_table = { [long_key_a] = 17 }
local long_string_store_table = { [long_key_a] = 0 }
local long_string_gc_store_table = { [long_key_a] = "lua54-perf-gc-seed" }
local long_string_gc_value = "lua54-perf-gc-value"
local long_string_new_keys = {}
for i = 1, 4096 do
  long_string_new_keys[i] =
    clone_long_string(("lua54-perf-new-key-%04d-"):format(i):rep(4))
end
local long_string_new_gc_value = "lua54-perf-new-gc-value"
local long_string_eq_pairs = {}
for i = 1, 256 do
  local s = ("lua54-perf-generic-eq-%04d-"):format(i):rep(4)
  local a = clone_long_string(s)
  local b = clone_long_string(s)
  assert(a == b and string.format("%p", a) ~= string.format("%p", b))
  long_string_eq_pairs[i] = { a, b }
end

local function long_string_equality_helpers(n)
  local sum = 0
  for i = 1, n do
    local p = long_string_eq_pairs[((i - 1) % 256) + 1]
    -- Rotate through many equal-but-distinct long-string objects so JIT mode
    -- cannot pass this workload by guarding on only the first pair identity.
    if p[1] == p[2] then
      sum = sum + 1
    end
  end
  return sum
end

local function long_string_table_helpers(n)
  local sum = 0
  for i = 1, n do
    -- Runtime Lua 5.4 long strings are different GC objects even when their
    -- bytes match. Keep bytewise table-key lookup in both JIT on/off perf
    -- windows so the recorder cannot silently fall back to object identity.
    sum = sum + long_string_table[long_string_keys[((i - 1) % 128) + 1]]
  end
  return sum
end

local function long_string_table_store_helpers(n)
  local sum = 0
  for i = 1, n do
    -- Existing long-string slots are the safe JIT surface: the recorder must
    -- still run a bytewise lookup, but it can raw-store non-GC TValue payloads
    -- without invoking NEWREF or a GC value barrier.
    long_string_store_table[long_string_keys[((i - 1) % 128) + 1]] = i
    sum = sum + long_string_store_table[long_key_b]
  end
  return sum
end

local function long_string_table_gc_store_helpers(n)
  local sum = 0
  for i = 1, n do
    -- GC values use the same bytewise long-key slot lookup, but must also keep
    -- the table barrier in the trace so incremental collection stays correct.
    long_string_gc_store_table[long_string_keys[((i - 1) % 128) + 1]] =
      long_string_gc_value
    if long_string_gc_store_table[long_key_b] == long_string_gc_value then
      sum = sum + 1
    end
  end
  return sum
end

local function long_string_table_new_store_helpers(n)
  local t = {}
  local sum = 0
  for i = 1, n do
    -- Start from an empty table so the measured pass keeps exercising the
    -- lj_tab_setstr() new-key path, not only the existing-slot fast surface.
    local key = long_string_new_keys[i]
    t[key] = i
    sum = sum + t[key]
  end
  return sum
end

local function long_string_table_new_gc_store_helpers(n)
  local t = {}
  local sum = 0
  for i = 1, n do
    local key = long_string_new_keys[i]
    t[key] = long_string_new_gc_value
    if t[key] == long_string_new_gc_value then
      sum = sum + 1
    end
  end
  return sum
end

local function floor_divmod(n)
  local sum = 0
  for i = 1, n do
    local q = math.floor(i / 7)
    sum = sum + q + (i - q * 7)
  end
  return sum
end

local function lua54_divmod(n)
  local sum = 0
  for i = 1, n do
    sum = sum + (i // 7) + (i % 7)
  end
  return sum
end

local string_subject = ("abc123-"):rep(8)
local utf8_extended = utf8.char(0x200000)
local utf8_subject = "a"..utf8_extended.."z"

local function string_tail_byte_method_bad(s)
  return s:byte({})
end

local function string_tail_find_method_bad(s)
  return s:find({})
end

local function string_helpers(n)
  local sum = 0
  for _ = 1, n do
    local first, last = string.find(string_subject, "123", "2", true)
    sum = sum + first + last
    local piece = string.sub(string_subject, "2", "5")
    if string.rep(piece, 2, ":") == "bc12:bc12" then sum = sum + 1 end
    -- Keep Lua 5.4-only formatting in the perf window: old recorders must not
    -- trade correctness for speed on %q infinities or hex-float text.
    if string.format("%q", math.huge) == "1e9999" then sum = sum + 1 end
    if string.format("%a", 1.5) == "0x1.8p+0" then sum = sum + 1 end
    if string.format("%s", 1.0) == "1.0" then sum = sum + 1 end
    if string.format("%6s", 12) == "    12" then sum = sum + 1 end
    local replaced, count = string.gsub("a b cd", " *", "-")
    sum = sum + #replaced + count
    local ok_missing, err_missing = pcall(string.gsub, "abc", "a")
    local ok_nil, err_nil = pcall(function()
      return string.gsub("abc", "a", nil)
    end)
    local ok_bool, err_bool = pcall(function()
      local f = string.gsub
      return f("abc", "a", true)
    end)
    if not ok_missing and err_missing:find("got no value", 1, true) and
       not ok_nil and err_nil:find("got nil", 1, true) and
       not ok_bool and err_bool:find("got boolean", 1, true) then
      sum = sum + 1
    end
    -- String method calls must keep Lua 5.4 public argument numbering under
    -- JIT on/off profiles, even when the method call is in return position.
    local ok_byte_method, err_byte_method =
      pcall(string_tail_byte_method_bad, "abc")
    local ok_find_method, err_find_method =
      pcall(string_tail_find_method_bad, "abc")
    if not ok_byte_method and
       err_byte_method:find("bad argument #1 to 'byte'", 1, true) then
      sum = sum + 1
    end
    if not ok_find_method and
       err_find_method:find("bad argument #1 to 'find'", 1, true) then
      sum = sum + 1
    end
    for a, b in string.gmatch("a b cd", "()%s*()", "2") do
      sum = sum + a + b
    end
    local word, digits = string.match("abc123", "(%a+)(%d+)", "1")
    sum = sum + #word + tonumber(digits)
    if string.reverse("abcd") == "dcba" then sum = sum + 1 end
    if string.lower("ABC") == "abc" and string.upper("abc") == "ABC" then
      sum = sum + 2
    end
    sum = sum + string.len("abcd")
  end
  return sum
end

local function utf8_helpers(n)
  local sum = 0
  for _ = 1, n do
    if utf8.len(utf8_subject, 1, -1, true) == 3 then sum = sum + 1 end
    if utf8.codepoint(utf8_subject, 2, 2, true) == 0x200000 then
      sum = sum + 1
    end
    if utf8.offset(utf8_subject, 3) == 7 then sum = sum + 1 end
    for _, cp in utf8.codes(utf8_subject, true) do
      if cp == 97 or cp == 0x200000 or cp == 122 then sum = sum + 1 end
    end
    if #utf8.char(97, 0x200000) == 6 then sum = sum + 1 end
    if utf8_subject:match("^"..utf8.charpattern..utf8.charpattern..
			  utf8.charpattern.."$") == utf8_subject then
      sum = sum + 1
    end
    local seen = 0
    for ch in utf8_subject:gmatch(utf8.charpattern) do
      seen = seen + 1
      if ch == "a" or ch == utf8_extended or ch == "z" then
	sum = sum + 1
      end
    end
    if seen == 3 then sum = sum + 1 end
    local len, badpos = utf8.len("\255")
    if len == nil and badpos == 1 then sum = sum + 1 end
    local ok_codes, err_codes = pcall(function()
      for _ in utf8.codes("\128") do end
    end)
    if not ok_codes and err_codes:find("invalid UTF-8 code", 1, true) then
      sum = sum + 1
    end
  end
  return sum
end

local function table_sort_helpers(n)
  local sum = 0
  local nums = {}
  local mt = {
    __lt = function(a, b)
      return a.v < b.v
    end
  }
  local a = setmetatable({ v = 3 }, mt)
  local b = setmetatable({ v = 1 }, mt)
  local c = setmetatable({ v = 2 }, mt)
  local objects = {}
  local proxy_base = { 3, 2, 1 }
  local proxy = setmetatable({}, {
    __len = function()
      return 3
    end,
    __index = function(_, k)
      return proxy_base[k]
    end,
    __newindex = function(_, k, v)
      proxy_base[k] = v
    end
  })
  local number_slots = {}
  local old_number_mt = debug.getmetatable(0)
  debug.setmetatable(0, {
    __len = function(self)
      return #number_slots[self]
    end,
    __index = function(self, k)
      return number_slots[self][k]
    end,
    __newindex = function(self, k, v)
      number_slots[self][k] = v
    end,
  })
  for _ = 1, n do
    nums[1], nums[2], nums[3] = 3, 1, 2
    table.sort(nums)
    sum = sum + nums[1] + nums[2] + nums[3]

    objects[1], objects[2], objects[3] = a, b, c
    table.sort(objects)
    sum = sum + objects[1].v * 100 + objects[2].v * 10 + objects[3].v

    -- Lua 5.4 table.sort must keep using public get/set operations here;
    -- a raw fast path would silently stop sorting proxy tables correctly.
    proxy_base[1], proxy_base[2], proxy_base[3] = 3, 2, 1
    table.sort(proxy)
    sum = sum + proxy_base[1] * 100 + proxy_base[2] * 10 + proxy_base[3]

    number_slots[0] = { "a", "b" }
    if table.concat(0, ",") == "a,b" then sum = sum + 1 end
    number_slots[0] = { "b", "c" }
    table.insert(0, 1, "a")
    if number_slots[0][1] == "a" and number_slots[0][3] == "c" then
      sum = sum + 1
    end
    if table.remove(0, 2) == "b" and number_slots[0][2] == "c" then
      sum = sum + 1
    end
    number_slots[0] = { 3, 1, 2 }
    table.sort(0)
    sum = sum + number_slots[0][1] * 100 +
		number_slots[0][2] * 10 + number_slots[0][3]
    number_slots[0] = { "u", "v" }
    local a, b = table.unpack(0)
    if a == "u" and b == "v" then sum = sum + 1 end
    number_slots[0] = { 10, nil, 30 }
    local c, d, e = table.unpack(0, 1, 3)
    if c == 10 and d == nil and e == 30 then sum = sum + 60 end
    number_slots[0] = { "x", "y" }
    number_slots[1] = {}
    if table.move(0, 1, 2, 3, 1) == 1 and number_slots[1][4] == "y" then
      sum = sum + 1
    end
  end
  debug.setmetatable(0, old_number_mt)
  return sum
end

local function hook_churn(n)
  for _ = 1, n do
    debug.sethook(function() end, "")
    debug.sethook(nil)
  end
  return n
end

local function collect_kb()
  collectgarbage("collect")
  collectgarbage("collect")
  return collectgarbage("count")
end

local function timeit(label, fn, n)
  fn(math.min(n, 64)) -- Warm up traces before the measured memory window.
  local mem0 = collect_kb()
  local t0 = os.clock()
  local result = fn(n)
  local elapsed = os.clock() - t0
  local mem1 = collect_kb()
  local growth = mem1 - mem0
  assert(elapsed <= abs_limit,
         string.format("%s took %.3fs over abs limit %.3fs", label, elapsed, abs_limit))
  assert(growth <= mem_limit_kb,
         string.format("%s retained %.1fKB over limit %.1fKB", label, growth, mem_limit_kb))
  print(string.format("[lua54_perf] %-28s time=%.4fs mem_delta=%.1fKB",
                      label, elapsed, growth))
  return elapsed, result
end

local function ratio_check(label, base_time, test_time)
  local ratio = test_time / math.max(base_time, 0.001)
  assert(ratio <= ratio_limit,
         string.format("%s ratio %.2fx over limit %.2fx", label, ratio, ratio_limit))
end

local function run_suite(mode_name, enable_jit, opt_flags)
  if enable_jit then
    jit.on()
    jit.flush()
    if jit.opt and jit.opt.start then
      -- Performance guard results are only comparable with a fixed optimizer
      -- profile. Allow an env override for local investigation, but never rely
      -- on whatever defaults the embedding application happened to set.
      jit.opt.start()
      -- The explicit "3" in the default profile matters: optimization level
      -- changes can dominate any Lua 5.4 compatibility cost we are measuring.
      jit.opt.start(unpack(split_opts(opt_flags)))
    end
    assert(jit.status(), "jit.on did not enable JIT")
    print("[lua54_perf] mode="..mode_name.." jit_opt="..opt_flags)
  else
    jit.off()
    jit.flush()
    assert(not jit.status(), "jit.off did not disable JIT")
    print("[lua54_perf] mode="..mode_name.." jit=off")
  end

  local iter_n = enable_jit and 3000 or 900
  local arith_n = enable_jit and 300000 or 90000
  local string_n = enable_jit and 20000 or 6000
  local utf8_n = enable_jit and 12000 or 3600
  local sort_n = enable_jit and 6000 or 1800
  local hook_n = enable_jit and 4000 or 1500

  local t_next, r_next = timeit(mode_name..":next", next_sum, iter_n)
  local t_pairs, r_pairs = timeit(mode_name..":pairs", pairs_sum, iter_n)
  assert(r_pairs == r_next)
  ratio_check(mode_name..":pairs_vs_next", t_next, t_pairs)

  local t_meta, r_meta = timeit(mode_name..":__pairs", metapairs_sum, iter_n)
  assert(r_meta == r_pairs)
  ratio_check(mode_name..":metapairs_vs_pairs", t_pairs, t_meta)

  local _, r_base = timeit(mode_name..":base_raw_helpers",
			   base_raw_helpers, iter_n)
  assert(r_base == iter_n * 15)

  local _, r_value = timeit(mode_name..":base_value_helpers",
			    base_value_helpers, iter_n)
  assert(r_value == iter_n * 22)

  local _, r_protected = timeit(mode_name..":protected_call_helpers",
				protected_call_helpers, iter_n)
  assert(r_protected == iter_n * 26)

  local _, r_coroutine = timeit(mode_name..":coroutine_helpers",
				coroutine_helpers, iter_n)
  assert(r_coroutine == iter_n * 9)

  local _, r_gc_mode = timeit(mode_name..":gc_mode_helpers",
			      gc_mode_helpers, iter_n)
  assert(r_gc_mode == iter_n * 7)

  local _, r_package = timeit(mode_name..":package_helpers",
			      package_helpers, iter_n)
  assert(r_package == iter_n * 6)

  local _, r_debug = timeit(mode_name..":debug_helpers",
			    debug_helpers, iter_n)
  assert(r_debug == iter_n * 7)

  local _, r_many_upvalue = timeit(mode_name..":many_upvalue_helpers",
				   many_upvalue_helpers, iter_n)
  assert(r_many_upvalue == iter_n)

  local _, r_os = timeit(mode_name..":os_helpers", os_helpers, iter_n)
  assert(r_os == iter_n * (13 + (os_future_supported and 1 or 0)))

  local _, r_io = timeit(mode_name..":io_helpers", io_helpers, iter_n)
  assert(r_io == iter_n * 9)

  local _, r_number_pack = timeit(mode_name..":number_pack_helpers",
				  number_pack_helpers, iter_n)
  assert(r_number_pack == iter_n * 166)

  local _, r_number_string = timeit(mode_name..":number_string_helpers",
				    number_string_helpers, iter_n)
  assert(r_number_string == iter_n * 5)

  local _, r_random = timeit(mode_name..":random_helpers",
			     random_helpers, iter_n)
  assert(r_random == iter_n * 8 + 1)

  local _, r_string_order = timeit(mode_name..":string_order_helpers",
				   string_order_helpers, iter_n)
  assert(r_string_order == iter_n * string_order_expected)

  local _, r_long_string_eq = timeit(mode_name..":long_string_equality",
				     long_string_equality_helpers, iter_n)
  assert(r_long_string_eq == iter_n)

  local _, r_long_string_table = timeit(mode_name..":long_string_table",
					long_string_table_helpers, iter_n)
  assert(r_long_string_table == iter_n * 17)

  local _, r_long_string_store =
    timeit(mode_name..":long_string_table_store",
	   long_string_table_store_helpers, iter_n)
  assert(r_long_string_store == iter_n * (iter_n + 1) / 2)

  local _, r_long_string_gc_store =
    timeit(mode_name..":long_string_table_gc_store",
	   long_string_table_gc_store_helpers, iter_n)
  assert(r_long_string_gc_store == iter_n)

  local _, r_long_string_new_store =
    timeit(mode_name..":long_string_table_new_store",
	   long_string_table_new_store_helpers, iter_n)
  assert(r_long_string_new_store == iter_n * (iter_n + 1) / 2)

  local _, r_long_string_new_gc_store =
    timeit(mode_name..":long_string_table_new_gc_store",
	   long_string_table_new_gc_store_helpers, iter_n)
  assert(r_long_string_new_gc_store == iter_n)

  local t_floor, r_floor = timeit(mode_name..":floor_divmod", floor_divmod, arith_n)
  local t_lua54, r_lua54 = timeit(mode_name..":lua54_divmod", lua54_divmod, arith_n)
  assert(r_lua54 == r_floor)
  ratio_check(mode_name..":divmod_vs_floor", t_floor, t_lua54)

  local _, r_string = timeit(mode_name..":string_helpers", string_helpers, string_n)
  assert(r_string == string_n * 205)

  local _, r_utf8 = timeit(mode_name..":utf8_helpers", utf8_helpers, utf8_n)
  assert(r_utf8 == utf8_n * 14)

  local _, r_sort = timeit(mode_name..":table_sort_helpers",
			   table_sort_helpers, sort_n)
  assert(r_sort == sort_n * 440)

  assert(timeit(mode_name..":hook_churn", hook_churn, hook_n))
end

if mode_arg == "jit_on" or mode_arg == "on" then
  local profiles = split_profiles(jit_opt_profiles)
  for i = 1, #profiles do
    run_suite("jit_on_opt"..i, true, profiles[i])
  end
elseif mode_arg == "jit_off" or mode_arg == "off" then
  run_suite("jit_off", false)
else
  local profiles = split_profiles(jit_opt_profiles)
  for i = 1, #profiles do
    run_suite("jit_on_opt"..i, true, profiles[i])
  end
  run_suite("jit_off", false)
end

print("[lua54_perf] ok")
