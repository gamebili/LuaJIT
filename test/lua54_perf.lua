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
local jit_opt_flags = os.getenv("LUA54_PERF_JIT_OPT") or
  "hotloop=3,hotexit=2,instunroll=4,loopunroll=4"

local function split_opts(s)
  local out = {}
  for opt in string.gmatch(s, "[^,]+") do
    out[#out+1] = opt
  end
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

local function run_suite(mode_name, enable_jit)
  if enable_jit then
    jit.on()
    jit.flush()
    if jit.opt and jit.opt.start then
      -- Performance guard results are only comparable with a fixed optimizer
      -- profile. Allow an env override for local investigation, but never rely
      -- on whatever defaults the embedding application happened to set.
      jit.opt.start(unpack(split_opts(jit_opt_flags)))
    end
    assert(jit.status(), "jit.on did not enable JIT")
    print("[lua54_perf] mode="..mode_name.." jit_opt="..jit_opt_flags)
  else
    jit.off()
    jit.flush()
    assert(not jit.status(), "jit.off did not disable JIT")
    print("[lua54_perf] mode="..mode_name.." jit=off")
  end

  local iter_n = enable_jit and 3000 or 900
  local arith_n = enable_jit and 300000 or 90000
  local hook_n = enable_jit and 4000 or 1500

  local t_next, r_next = timeit(mode_name..":next", next_sum, iter_n)
  local t_pairs, r_pairs = timeit(mode_name..":pairs", pairs_sum, iter_n)
  assert(r_pairs == r_next)
  ratio_check(mode_name..":pairs_vs_next", t_next, t_pairs)

  local t_meta, r_meta = timeit(mode_name..":__pairs", metapairs_sum, iter_n)
  assert(r_meta == r_pairs)
  ratio_check(mode_name..":metapairs_vs_pairs", t_pairs, t_meta)

  local t_floor, r_floor = timeit(mode_name..":floor_divmod", floor_divmod, arith_n)
  local t_lua54, r_lua54 = timeit(mode_name..":lua54_divmod", lua54_divmod, arith_n)
  assert(r_lua54 == r_floor)
  ratio_check(mode_name..":divmod_vs_floor", t_floor, t_lua54)

  assert(timeit(mode_name..":hook_churn", hook_churn, hook_n))
end

if mode_arg == "jit_on" or mode_arg == "on" then
  run_suite("jit_on", true)
elseif mode_arg == "jit_off" or mode_arg == "off" then
  run_suite("jit_off", false)
else
  run_suite("jit_on", true)
  run_suite("jit_off", false)
end

print("[lua54_perf] ok")
