#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
LUAJIT_BIN=${LUAJIT_BIN:-"$ROOT/src/luajit"}
LUA54_BIN=${LUA54_BIN:-/Users/gongliang/git/lua-5.4.8/lua}
LUAJIT_LUA_PATH=${LUAJIT_LUA_PATH:-"$ROOT/src/?.lua;$ROOT/src/?/init.lua;;"}
MARKDOWN_OUT=
ENFORCE=0

while [ "$#" -gt 0 ]; do
  case "$1" in
    --markdown)
      MARKDOWN_OUT=$2
      shift 2
      ;;
    --enforce)
      ENFORCE=1
      shift
      ;;
    *)
      echo "usage: $0 [--markdown path] [--enforce]" >&2
      exit 2
      ;;
  esac
done

tmpdir=${TMPDIR:-/tmp}/luajit-arm64-wide-perf.$$
mkdir -p "$tmpdir"
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

bench_lua=$tmpdir/bench_wide.lua
results_tsv=$tmpdir/results.tsv

cat >"$bench_lua" <<'LUA'
local mode = os.getenv("BENCH_MODE") or "unknown"
io.stdout:setvbuf("line")
local function reset_jit_mode()
  if not jit then return end
  jit.flush()
  if mode == "luajit_jit_on" then
    jit.on()
    jit.opt.start("3", "hotloop=8", "hotexit=2")
  elseif mode == "luajit_jit_off" then
    jit.off()
  end
end
reset_jit_mode()

local clock = os.clock
local reps = tonumber(os.getenv("BENCH_REPS") or "5")
local bench_filter = os.getenv("BENCH_FILTER")

function best_of(fn, n)
  reset_jit_mode()
  fn(math.max(1, math.floor(n / 100)))
  local best, got, first_got, stable
  stable = true
  for _ = 1, reps do
    collectgarbage("collect")
    local t0 = clock()
    got = fn(n)
    local dt = clock() - t0
    if first_got == nil then
      first_got = got
    elseif got ~= first_got then
      stable = false
    end
    if not best or dt < best then best = dt end
  end
  return best, got, stable
end

function checksum(v)
  if type(v) == "number" then
    if v == math.floor(v) then
      return string.format("%.0f", v)
    end
    return string.format("%.6f", v)
  end
  return tostring(v)
end

local arr = {}
for i = 1, 100000 do arr[i] = (i * 3) % 97 end

local small_arr = {}
for i = 1, 128 do small_arr[i] = i end

local hash, keys = {}, {}
for i = 1, 1024 do
  local k = "key_" .. i
  keys[i] = k
  hash[k] = i * 7
end

local pair_data = {}
for i = 1, 128 do pair_data["k" .. i] = i end

function empty_loop(n)
  local s = 0
  for _ = 1, n do s = s + 1 end
  return s
end

function int_add_wide(n)
  local s = 0
  for i = 1, n do
    s = (s + i + 1099511627776) % 1000000007
  end
  return s
end

function int_mod_small(n)
  local s = 0
  for i = 1, n do
    s = (s + (i * 3 + 7) % 97) % 1000000
  end
  return s
end

function float_arith(n)
  local s = 0.5
  for i = 1, n do
    s = s + i * 0.25
    s = s * 1.0000001 - 0.125
    if s > 100000 then s = s - 100000 end
  end
  return s
end

function pure_bit_xor(n)
  local s = 0
  for i = 1, n do s = (s ~ i) & 0x7fffffff end
  return s
end

function bit_shift_mask(n)
  local s = 0x12345678
  for i = 1, n do
    s = ((s << 5) ~ (i >> 3) ~ i) & 0x7fffffff
  end
  return s
end

function bitwise_idiv_unsigned(n)
  local s = 0
  for i = 1, n do
    s = ((s ~ (i << 7)) + (i // 3)) & 0x7fffffff
  end
  return s
end

function bitwise_idiv_mix(n)
  local s = 17
  for i = 1, n do
    s = ((s + (i // 5)) ~ (i << 2) ~ (i >> 1)) & 0x7fffffff
  end
  return s
end

function idiv_mod(n)
  local s = 0
  for i = 1, n do
    s = (s + (i // 7) + (i % 13)) % 1000000007
  end
  return s
end

function array_sum_wide(rounds)
  local s = 0
  for _ = 1, rounds do
    local chunk = 0
    for i = 1, 100000 do chunk = chunk + arr[i] end
    s = (s + chunk) % 1000000
  end
  return s
end

function array_sum_chunk(rounds)
  local s = 0
  for r = 1, rounds do
    local base = (r % 99000) + 1
    for i = 0, 255 do s = s + arr[base + i] end
  end
  return s % 1000000
end

function array_write_seq(rounds)
  local total = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 2000 do t[i] = i + r end
    total = total + t[2000]
  end
  return total % 1000000
end

function table_new_small(n)
  local s = 0
  for i = 1, n do
    local t = { i, i + 1, a = i * 2 }
    s = s + t[1] + t.a
  end
  return s % 1000000
end

function hash_lookup(n)
  local s = 0
  for i = 1, n do
    s = s + hash[keys[(i % 1024) + 1]]
  end
  return s % 1000000
end

function hash_update(n)
  local h = {}
  for i = 1, 1024 do h[keys[i]] = i * 7 end
  for i = 1, n do
    local k = keys[(i % 1024) + 1]
    h[k] = (h[k] + i) % 1000003
  end
  return h["key_1"]
end

function ipairs_iter(n)
  local s = 0
  for _ = 1, n do
    for _, v in ipairs(small_arr) do s = s + v end
  end
  return s % 1000000
end

function pairs_iter(n)
  local s = 0
  for _ = 1, n do
    for _, v in pairs(pair_data) do s = s + v end
  end
  return s % 1000000
end

function function_calls(n)
  local function f(a, b, c) return (a + b) * c - b end
  local s = 0
  for i = 1, n do s = s + f(i, 3, 5) end
  return s % 1000000
end

function closure_alloc(n)
  local s = 0
  for i = 1, n do
    local function f() return i end
    s = s + f()
  end
  return s % 1000000
end

function pcall_success(n)
  local function f(x) return x + 1 end
  local s = 0
  for i = 1, n do
    local ok, v = pcall(f, i)
    if ok then s = s + v end
  end
  return s % 1000000
end

function table_sort_int(rounds)
  local total = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 5000 do
      t[i] = (i * 1103515245 + r * 12345) % 2147483647
    end
    table.sort(t)
    total = total + t[1] + t[#t]
  end
  return total % 1000000
end

function table_sort_cmp(rounds)
  local total = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 2000 do
      t[i] = { k = (i * 1103515245 + r * 12345) % 2147483647 }
    end
    table.sort(t, function(a, b) return a.k < b.k end)
    total = total + t[1].k + t[#t].k
  end
  return total % 1000000
end

function string_concat(n)
  local total = 0
  local s = ""
  for _ = 1, n do
    s = s .. "a"
    if #s >= 1024 then
      total = total + #s
      s = ""
    end
  end
  return total
end

local concat_parts = {}
for i = 1, 32 do concat_parts[i] = ("x%02d"):format(i) end

function table_concat(n)
  local total = 0
  for _ = 1, n do
    total = total + #table.concat(concat_parts, ",")
  end
  return total
end

local find_text = ("abc0123456789xyz;"):rep(64)
function string_find_plain(n)
  local total = 0
  for i = 1, n do
    local a = find_text:find("xyz", 1, true)
    total = total + (a or 0)
  end
  return total % 1000000
end

local gsub_text = ("the quick brown fox jumps over the lazy dog; "):rep(40)
function string_gsub(n)
  local total = 0
  for _ = 1, n do
    local s, c = gsub_text:gsub("o", "0")
    total = total + #s + c
  end
  return total
end

function math_sin(n)
  local s = 0
  for i = 1, n do s = s + math.sin(i) end
  return s
end

function math_floor(n)
  local s = 0
  for i = 1, n do s = s + math.floor(i * 0.25) end
  return s % 1000000
end

function coroutine_resume(n)
  local co = coroutine.create(function()
    while true do coroutine.yield(1) end
  end)
  local s = 0
  for _ = 1, n do
    local ok, v = coroutine.resume(co)
    if ok then s = s + v end
  end
  return s
end

function numeric_for_down(n)
  local s = 0
  for i = n, 1, -3 do
    s = s + (i % 97)
  end
  return s % 1000000
end

function branch_mod_loop(n)
  local s = 0
  for i = 1, n do
    if (i % 7) < 3 then
      s = s + i
    else
      s = s - i
    end
  end
  return s % 1000000
end

function bit_rotate64(n)
  local s = 0x123456789abc
  for i = 1, n do
    s = ((s << 13) ~ (s >> 7) ~ i) & 0xffffffffffff
  end
  return s & 0x7fffffff
end

local len_table = {}
for i = 1, 4096 do len_table[i] = i end

function table_len_loop(n)
  local s = 0
  for _ = 1, n do s = s + #len_table end
  return s % 1000000
end

function table_insert_remove(n)
  local s = 0
  for r = 1, n do
    local t = {}
    for i = 1, 64 do table.insert(t, i + r) end
    for _ = 1, 64 do s = s + table.remove(t) end
  end
  return s % 1000000
end

local unpack_data = {}
for i = 1, 16 do unpack_data[i] = i end

function table_unpack_multi(n)
  local unpack = table.unpack
  local s = 0
  for _ = 1, n do
    local a, _, _, _, _, _, _, h = unpack(unpack_data, 1, 8)
    s = s + a + h
  end
  return s % 1000000
end

local byte_text = ("LuaJIT-arm64-byte-scan-0123456789;"):rep(16)

function string_byte_sum(n)
  local s = 0
  for _ = 1, n do
    for i = 1, #byte_text do
      s = s + byte_text:byte(i)
    end
  end
  return s % 1000000
end

local sub_text = ("abcdefghijklmnopqrstuvwxyz0123456789"):rep(16)

function string_sub_loop(n)
  local total = 0
  local limit = #sub_text - 8
  for i = 1, n do
    local p = (i % limit) + 1
    total = total + #sub_text:sub(p, p + 7)
  end
  return total
end

function string_format_num(n)
  local total = 0
  for i = 1, n do
    total = total + #string.format("%d:%08x", i, i)
  end
  return total
end

local match_text = "alpha=123 beta=456 gamma=789 delta=42"

function string_match_pattern(n)
  local total = 0
  for _ = 1, n do
    local a, b = match_text:match("beta=(%d+) gamma=(%d+)")
    total = total + #a + #b
  end
  return total
end

local number_strings = {}
for i = 1, 128 do number_strings[i] = tostring(i * 7919) end

function tonumber_parse(n)
  local s = 0
  for i = 1, n do
    s = s + tonumber(number_strings[(i % 128) + 1])
  end
  return s % 1000000
end

function tostring_int(n)
  local total = 0
  for i = 1, n do total = total + #tostring(i) end
  return total
end

function math_min_max(n)
  local s = 0
  for i = 1, n do
    local a = i % 101
    s = s + math.max(a, 50) - math.min(a, 50)
  end
  return s % 1000000
end

function closure_upvalue_update(n)
  local x = 0
  local function inc(v)
    x = (x + v) % 1000000
  end
  for i = 1, n do inc(i) end
  return x
end

local method_obj = { x = 0 }
function method_obj:add(v)
  self.x = (self.x + v) % 1000000
end

function method_call(n)
  local obj = { x = 0, add = method_obj.add }
  for i = 1, n do obj:add(i) end
  return obj.x
end

local index_obj = setmetatable({}, {
  __index = function(_, k)
    if k == "value" then return 7 end
    return 0
  end
})

function metamethod_index(n)
  local s = 0
  for _ = 1, n do s = s + index_obj.value end
  return s % 1000000
end

function pcall_error(n)
  local function fail() error("expected", 0) end
  local s = 0
  for _ = 1, n do
    local ok = pcall(fail)
    if not ok then s = s + 1 end
  end
  return s
end

function coroutine_create_resume(n)
  local s = 0
  for i = 1, n do
    local co = coroutine.create(function(x) return x + 1 end)
    local ok, v = coroutine.resume(co, i)
    if ok then s = s + v end
  end
  return s % 1000000
end

local words_text = "alpha beta22 gamma333 delta4444 epsilon55555"

function string_gmatch_words(n)
  local total = 0
  for _ = 1, n do
    for w in words_text:gmatch("%w+") do
      total = total + #w
    end
  end
  return total
end

function int_add32(n)
  local s = 0
  for i = 1, n do s = (s + i + 3) % 2147483647 end
  return s
end

function int_mul32(n)
  local s = 1
  for i = 1, n do s = (s * 1103515245 + i) & 0x7fffffff end
  return s
end

function idiv_const(n)
  local s = 0
  for i = 1, n do s = s + (i // 10) end
  return s % 1000000
end

function mod_const(n)
  local s = 0
  for i = 1, n do s = s + (i % 10) end
  return s % 1000000
end

function int_compare_branch(n)
  local s = 0
  for i = 1, n do
    local x = i % 257
    if x < 128 then s = s + x else s = s - x end
  end
  return s % 1000000
end

function bit_and32(n)
  local s = 0x7fffffff
  for i = 1, n do s = (s & (i * 2654435761)) ~ i end
  return s & 0x7fffffff
end

function bit_not32(n)
  local s = 0
  for i = 1, n do s = (~(s ~ i)) & 0x7fffffff end
  return s
end

function bit_shift64_const(n)
  local s = 0x123456789abc
  for i = 1, n do s = ((s << 1) ~ i) & 0xffffffffffff end
  return s & 0x7fffffff
end

function bit_and64(n)
  local s = 0xffffffffffff
  for i = 1, n do
    s = ((s & ((i << 20) ~ 0xabcdef)) ~ i) & 0xffffffffffff
  end
  return s & 0x7fffffff
end

function array_read_seq(rounds)
  local s = 0
  for _ = 1, rounds do
    for i = 1, #arr do s = s + arr[i] end
  end
  return s % 1000000
end

function array_read_stride(n)
  local s = 0
  for i = 1, n do
    local j = ((i * 17) % 100000) + 1
    s = s + arr[j]
  end
  return s % 1000000
end

local wide_key_table = {}
for i = 1, 1024 do wide_key_table[1099511627776 + i] = i * 3 end

function table_int64_key_lookup(n)
  local s = 0
  for i = 1, n do
    local k = 1099511627776 + ((i % 1024) + 1)
    s = s + wide_key_table[k]
  end
  return s % 1000000
end

function rawget_rawset(n)
  local t = {}
  local s = 0
  for i = 1, n do
    rawset(t, "x", i)
    s = s + rawget(t, "x")
  end
  return s % 1000000
end

local move_src = {}
for i = 1, 256 do move_src[i] = i end

function table_move_copy(n)
  local s = 0
  for _ = 1, n do
    local t = {}
    table.move(move_src, 1, 256, 1, t)
    s = s + t[1] + t[256]
  end
  return s % 1000000
end

function table_pack_loop(n)
  local s = 0
  for i = 1, n do
    local t = table.pack(i, i + 1, nil, i + 3)
    s = s + t.n + t[1] + t[2] + t[4]
  end
  return s % 1000000
end

function table_remove_front(rounds)
  local s = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 256 do t[i] = i + r end
    for _ = 1, 256 do s = s + table.remove(t, 1) end
  end
  return s % 1000000
end

function table_sort_strings(rounds)
  local total = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 1500 do
      t[i] = ("s%06d"):format((i * 1103515245 + r * 12345) % 2147483647)
    end
    table.sort(t)
    total = total + #t[1] + #t[#t]
  end
  return total % 1000000
end

function table_sort_reverse(rounds)
  local total = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 4000 do t[i] = 4001 - i + r end
    table.sort(t)
    total = total + t[1] + t[#t]
  end
  return total % 1000000
end

local lower_text = ("LuaJIT Arm64 Wide Sweep 0123456789;"):rep(24)

function string_lower_upper(n)
  local total = 0
  for _ = 1, n do
    total = total + #lower_text:lower() + #lower_text:upper()
  end
  return total
end

function string_reverse_loop(n)
  local total = 0
  for _ = 1, n do total = total + #lower_text:reverse() end
  return total
end

function string_rep_loop(n)
  local total = 0
  for i = 1, n do total = total + #("ab"):rep((i % 32) + 1) end
  return total
end

function string_char_loop(n)
  local total = 0
  for i = 1, n do
    total = total + #string.char(65 + (i % 26), 97 + (i % 26), 48 + (i % 10))
  end
  return total
end

local pattern_text = ("id=12345 name=luajit status=ok;"):rep(24)

function string_find_pattern(n)
  local total = 0
  for _ = 1, n do
    local a, b, id = pattern_text:find("id=(%d+)%s+name")
    total = total + (a or 0) + (b or 0) + #(id or "")
  end
  return total % 1000000
end

function string_gsub_capture(n)
  local total = 0
  for _ = 1, n do
    local s, c = pattern_text:gsub("(%d)", "%1")
    total = total + #s + c
  end
  return total
end

function string_pack_unpack_i4(n)
  local total = 0
  for i = 1, n do
    local s = string.pack("<i4I4", i, i * 3)
    local a, b = string.unpack("<i4I4", s)
    total = (total + a + b) % 1000000
  end
  return total
end

function string_pack_unpack_i8(n)
  local total = 0
  for i = 1, n do
    local v = 1099511627776 + i
    local s = string.pack("<j", v)
    local a = string.unpack("<j", s)
    total = (total + (a % 1000000)) % 1000000
  end
  return total
end

local utf8_text = utf8.char(0x41, 0x7ff, 0x4e2d, 0x1f600):rep(12)

function utf8_codes_loop(n)
  local total = 0
  for _ = 1, n do
    for _, c in utf8.codes(utf8_text) do total = total + c end
  end
  return total % 1000000
end

function tonumber_base16(n)
  local s = 0
  for i = 1, n do
    s = s + tonumber(string.format("%x", i % 65536), 16)
  end
  return s % 1000000
end

function tostring_float(n)
  local total = 0
  for i = 1, n do total = total + #tostring(i + 0.25) end
  return total
end

function math_abs_int64(n)
  local s = 0
  for i = 1, n do
    local v = 1099511627776 + i
    if (i & 1) == 0 then v = -v end
    s = (s + (math.abs(v) % 1000000)) % 1000000
  end
  return s
end

function math_sqrt_log(n)
  local s = 0
  for i = 1, n do s = s + math.sqrt(i) + math.log(i) end
  return s
end

function math_type_loop(n)
  local s = 0
  for i = 1, n do
    if math.type(i) == "integer" then s = s + 1 end
  end
  return s
end

function select_vararg(n)
  local function f(...)
    return select("#", ...), select(3, ...)
  end
  local s = 0
  for i = 1, n do
    local c, v = f(1, 2, i, 4, 5)
    s = s + c + v
  end
  return s % 1000000
end

function xpcall_error(n)
  local function fail() error("expected", 0) end
  local function handler() return "handled" end
  local s = 0
  for _ = 1, n do
    local ok, v = xpcall(fail, handler)
    if not ok and v == "handled" then s = s + 1 end
  end
  return s
end

local add_meta = { __add = function(a, b) return a.v + b end }
local add_obj = setmetatable({ v = 7 }, add_meta)

function metamethod_add(n)
  local s = 0
  for i = 1, n do s = s + (add_obj + i) end
  return s % 1000000
end

local call_obj = setmetatable({ base = 11 }, {
  __call = function(self, v) return self.base + v end
})

function metamethod_call(n)
  local s = 0
  for i = 1, n do s = s + call_obj(i) end
  return s % 1000000
end

local len_obj = setmetatable({}, { __len = function() return 42 end })

function metamethod_len(n)
  local s = 0
  for _ = 1, n do s = s + #len_obj end
  return s % 1000000
end

function coroutine_yield_multi(n)
  local co = coroutine.create(function()
    local i = 0
    while true do
      i = i + 1
      coroutine.yield(i, i + 1)
    end
  end)
  local s = 0
  for _ = 1, n do
    local ok, a, b = coroutine.resume(co)
    if ok then s = s + a + b end
  end
  return s % 1000000
end

function int64_add_sub(n)
  local s = 1099511627776
  local floor = 1099511627776
  local ceiling = floor + 1000000
  for i = 1, n do
    s = s + i - (i % 7)
    if s > ceiling then s = s - 1000000 end
  end
  return s % 1000000
end

function idiv_negative(n)
  local s = 0
  for i = 1, n do
    s = (s + ((-i) // 7) + ((-i) % 13)) % 1000000
  end
  return s
end

function idiv_power2(n)
  local s = 0
  for i = 1, n do s = s + (i // 1024) end
  return s % 1000000
end

function mod_power2(n)
  local s = 0
  for i = 1, n do s = s + (i % 1024) end
  return s % 1000000
end

function numeric_for_int64_span(n)
  local s = 0
  local first = 1099511627776
  for i = first, first + n - 1 do
    s = (s + (i % 97)) % 1000000
  end
  return s
end

function int64_compare_minmax(n)
  local s = 0
  local hi = math.maxinteger - 1000000
  local lo = math.mininteger + 1000000
  for i = 1, n do
    local v = (i & 1) == 0 and (hi - i) or (lo + i)
    if v < 0 then s = s + 3 else s = s + 7 end
  end
  return s % 1000000
end

function bit_or64(n)
  local s = 0
  for i = 1, n do
    s = (s | ((i << 32) ~ 0x12345678)) & 0xffffffffffff
  end
  return s & 0x7fffffff
end

function bit_xor64(n)
  local s = 0x123456789abc
  for i = 1, n do
    s = (s ~ ((i << 17) | (i >> 3))) & 0xffffffffffff
  end
  return s & 0x7fffffff
end

function bit_shift64_var(n)
  local s = 0x123456789abc
  for i = 1, n do
    local sh = i & 31
    s = ((s << sh) ~ (s >> (63 - sh)) ~ i) & 0xffffffffffff
  end
  return s & 0x7fffffff
end

function bit_not64(n)
  local s = 0x123456789abc
  for i = 1, n do
    s = (~(s ~ (i << 8))) & 0xffffffffffff
  end
  return s & 0x7fffffff
end

local float_key_table = {}
for i = 1, 1024 do float_key_table[i + 0.5] = i * 5 end

function table_float_key_lookup(n)
  local s = 0
  for i = 1, n do
    local k = ((i % 1024) + 1) + 0.5
    s = s + float_key_table[k]
  end
  return s % 1000000
end

local mixed_key_table = {}
for i = 1, 512 do
  mixed_key_table[i] = i
  mixed_key_table["k" .. i] = i * 2
  mixed_key_table[1099511627776 + i] = i * 3
end

function table_mixed_key_lookup(n)
  local s = 0
  for i = 1, n do
    local j = (i % 512) + 1
    if (i % 3) == 0 then
      s = s + mixed_key_table[j]
    elseif (i % 3) == 1 then
      s = s + mixed_key_table["k" .. j]
    else
      s = s + mixed_key_table[1099511627776 + j]
    end
  end
  return s % 1000000
end

function table_int64_key_insert(rounds)
  local s = 0
  for r = 1, rounds do
    local t = {}
    local base = 1099511627776 + r * 1024
    for i = 1, 512 do t[base + i] = i end
    s = s + t[base + 1] + t[base + 512]
  end
  return s % 1000000
end

function table_int64_key_update(n)
  local t = {}
  for i = 1, 1024 do t[1099511627776 + i] = i end
  local s = 0
  for i = 1, n do
    local k = 1099511627776 + ((i % 1024) + 1)
    local v = t[k] + 1
    t[k] = v
    s = s + v
  end
  return s % 1000000
end

local dense_next_table = {}
for i = 1, 256 do dense_next_table[i] = i end

function table_next_dense(n)
  local s = 0
  for _ = 1, n do
    for _, v in next, dense_next_table do s = s + v end
  end
  return s % 1000000
end

function table_insert_front(rounds)
  local s = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 128 do table.insert(t, 1, i + r) end
    s = s + t[1] + t[#t]
  end
  return s % 1000000
end

function table_remove_middle(rounds)
  local s = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 192 do t[i] = i + r end
    for _ = 1, 96 do s = s + table.remove(t, 64) end
  end
  return s % 1000000
end

local overlap_move_data = {}
for i = 1, 512 do overlap_move_data[i] = i end

function table_move_overlap_forward(n)
  local s = 0
  for _ = 1, n do
    table.move(overlap_move_data, 1, 384, 65)
    s = s + overlap_move_data[65] + overlap_move_data[448]
  end
  return s % 1000000
end

function table_move_overlap_backward(n)
  local s = 0
  for _ = 1, n do
    table.move(overlap_move_data, 65, 448, 1)
    s = s + overlap_move_data[1] + overlap_move_data[384]
  end
  return s % 1000000
end

local concat_number_parts = {}
for i = 1, 64 do concat_number_parts[i] = i end

function table_concat_numbers(n)
  local total = 0
  for _ = 1, n do total = total + #table.concat(concat_number_parts, ":") end
  return total
end

function string_byte_range(n)
  local s = 0
  for _ = 1, n do
    local a,b,c,d,e,f,g,h,i,j,k,l,m,o,p,q = byte_text:byte(1, 16)
    s = s + a+b+c+d+e+f+g+h+i+j+k+l+m+o+p+q
  end
  return s % 1000000
end

function string_find_init(n)
  local total = 0
  local limit = #find_text - 3
  for i = 1, n do
    local p = (i % limit) + 1
    local a = find_text:find("xyz", p, true)
    total = total + (a or 0)
  end
  return total % 1000000
end

function string_gsub_func(n)
  local total = 0
  local function repl(c) return c end
  for _ = 1, n do
    local s, c = gsub_text:gsub("(%a)", repl)
    total = total + #s + c
  end
  return total
end

function string_gmatch_captures(n)
  local total = 0
  for _ = 1, n do
    for k, v in match_text:gmatch("(%a+)=(%d+)") do
      total = total + #k + #v
    end
  end
  return total
end

function string_format_int64(n)
  local total = 0
  for i = 1, n do
    total = total + #string.format("%d:%x", 1099511627776 + i, i)
  end
  return total
end

function string_pack_unpack_mixed(n)
  local total = 0
  for i = 1, n do
    local wide = 1099511627776 + i
    local s = string.pack("<i2i4jI4", i % 32767, i, wide, i * 3)
    local a, b, c, d = string.unpack("<i2i4jI4", s)
    total = (total + a + b + (c % 1000000) + d) % 1000000
  end
  return total
end

function utf8_len_loop(n)
  local total = 0
  for _ = 1, n do total = total + utf8.len(utf8_text) end
  return total
end

function utf8_codepoint_loop(n)
  local total = 0
  for _ = 1, n do
    total = total + select("#", utf8.codepoint(utf8_text, 1, #utf8_text))
  end
  return total
end

function utf8_char_build(n)
  local total = 0
  for _ = 1, n do
    total = total + #utf8.char(0x41, 0x7ff, 0x4e2d, 0x1f600)
  end
  return total
end

function utf8_offset_loop(n)
  local total = 0
  for i = 1, n do
    total = total + (utf8.offset(utf8_text, (i % 12) + 1) or 0)
  end
  return total % 1000000
end

function math_tointeger_loop(n)
  local s = 0
  for i = 1, n do s = s + (math.tointeger(i + 0.0) or 0) end
  return s % 1000000
end

function math_ult_loop(n)
  local s = 0
  for i = 1, n do
    if math.ult(-i, i) then s = s + 1 else s = s + 3 end
  end
  return s % 1000000
end

function math_minmax_int64(n)
  local s = 0
  for i = 1, n do
    local a = 1099511627776 + i
    local b = 1099511627776 + (i % 257)
    s = (s + (math.max(a, b) - math.min(a, b))) % 1000000
  end
  return s
end

function vararg_sum(n)
  local function f(...)
    local s = 0
    for i = 1, select("#", ...) do s = s + select(i, ...) end
    return s
  end
  local total = 0
  for i = 1, n do total = total + f(i, 2, 3, 4, 5, 6) end
  return total % 1000000
end

function tail_call_pair(n)
  local function g(x) return x + 1 end
  local function f(x) return g(x) end
  local s = 0
  for i = 1, n do s = s + f(i) end
  return s % 1000000
end

function upvalue_read_loop(n)
  local x = 17
  local function f(v) return x + v end
  local s = 0
  for i = 1, n do s = s + f(i) end
  return s % 1000000
end

local newindex_store = {}
local newindex_obj = setmetatable({}, {
  __newindex = function(_, k, v) newindex_store[k] = v end
})

function metamethod_newindex(n)
  local s = 0
  for i = 1, n do
    newindex_obj.value = i
    s = s + newindex_store.value
  end
  return s % 1000000
end

local order_meta = {
  __eq = function(a, b) return a.v == b.v end,
  __lt = function(a, b) return a.v < b.v end,
  __le = function(a, b) return a.v <= b.v end,
}
local order_a = setmetatable({ v = 7 }, order_meta)
local order_b = setmetatable({ v = 11 }, order_meta)

function metamethod_compare(n)
  local s = 0
  for i = 1, n do
    if order_a < order_b then s = s + 1 end
    if order_a <= order_b then s = s + 3 end
    if order_a == order_b then s = s + 7 end
  end
  return s % 1000000
end

local concat_obj = setmetatable({ v = "x" }, {
  __concat = function(a, b)
    local av = type(a) == "table" and a.v or a
    local bv = type(b) == "table" and b.v or b
    return av .. bv
  end
})

function metamethod_concat(n)
  local total = 0
  for _ = 1, n do total = total + #(concat_obj .. "y") end
  return total
end

function pcall_vararg_success(n)
  local function f(a, b, c, d) return a + b + c + d end
  local s = 0
  for i = 1, n do
    local ok, v = pcall(f, i, 2, 3, 4)
    if ok then s = s + v end
  end
  return s % 1000000
end

function coroutine_wrap_loop(n)
  local f = coroutine.wrap(function()
    while true do coroutine.yield(1) end
  end)
  local s = 0
  for _ = 1, n do s = s + f() end
  return s
end

function int_compare_eq_chain(n)
  local s = 0
  for i = 1, n do
    local x = (i * 17 + 3) % 251
    if x == 0 or x == 17 or x == 91 then
      s = s + x
    else
      s = s - x
    end
  end
  return s % 1000000
end

function int64_eq_branch(n)
  local s = 0
  local base = 1099511627776
  for i = 1, n do
    local x = base + (i % 257)
    if x == base + 17 or x == base + 128 then
      s = s + 5
    else
      s = s + 1
    end
  end
  return s % 1000000
end

function int64_mod_const(n)
  local s = 0
  local base = 1099511627776
  for i = 1, n do s = (s + ((base + i) % 257)) % 1000000 end
  return s
end

function int64_idiv_const(n)
  local s = 0
  local base = 1099511627776
  for i = 1, n do s = (s + (((base + i) // 257) % 1000000)) % 1000000 end
  return s
end

function int64_mul_wrap(n)
  local s = 1099511627776
  for i = 1, n do
    s = (s * 6364136223846793005 + i) & 0xffffffffffff
  end
  return s & 0x7fffffff
end

function float_compare_branch(n)
  local s = 0
  for i = 1, n do
    local x = (i * 0.5) % 257.0
    if x < 128.25 then s = s + x else s = s - x end
  end
  return s
end

function bit_or32(n)
  local s = 0
  for i = 1, n do s = (s | (i * 2654435761)) & 0x7fffffff end
  return s
end

function bit_xor32(n)
  local s = 0x12345678
  for i = 1, n do s = (s ~ (i * 1103515245)) & 0x7fffffff end
  return s
end

function bit_shift32_const(n)
  local s = 0x12345678
  for i = 1, n do s = ((s << 3) ~ (s >> 5) ~ i) & 0x7fffffff end
  return s
end

function bit_shift32_var(n)
  local s = 0x12345678
  for i = 1, n do
    local sh = i & 15
    s = ((s << sh) ~ (s >> (31 - sh)) ~ i) & 0x7fffffff
  end
  return s
end

function bit_and64_const(n)
  local s = 0xffffffffffff
  for i = 1, n do s = (s & 0x123456789abc) ~ i end
  return s & 0x7fffffff
end

function bit_or64_const(n)
  local s = 0
  for i = 1, n do s = (s | 0x123456789abc | i) & 0xffffffffffff end
  return s & 0x7fffffff
end

function bit_xor64_const(n)
  local s = 0x123456789abc
  for i = 1, n do s = (s ~ 0xfedcba987654 ~ i) & 0xffffffffffff end
  return s & 0x7fffffff
end

function bit_shift64_right(n)
  local s = 0xf23456789abc
  for i = 1, n do s = ((s >> 1) ~ (i << 33)) & 0xffffffffffff end
  return s & 0x7fffffff
end

function bit64_branch_test(n)
  local s = 0
  local base = 1099511627776
  for i = 1, n do
    local x = base + i
    if (x & 0x100) ~= 0 then s = s + 3 else s = s + 1 end
  end
  return s % 1000000
end

function table_insert_tail(rounds)
  local s = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 256 do table.insert(t, i + r) end
    s = s + t[1] + t[#t]
  end
  return s % 1000000
end

function table_remove_tail(rounds)
  local s = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 256 do t[i] = i + r end
    for _ = 1, 256 do s = s + table.remove(t) end
  end
  return s % 1000000
end

large_move_src = {}
for i = 1, 2048 do large_move_src[i] = i end

function table_move_large_copy(n)
  local s = 0
  for _ = 1, n do
    local t = {}
    table.move(large_move_src, 1, 2048, 1, t)
    s = s + t[1] + t[2048]
  end
  return s % 1000000
end

self_move_data = {}
for i = 1, 2048 do self_move_data[i] = i end

function table_move_self_nooverlap(n)
  local s = 0
  for _ = 1, n do
    table.move(self_move_data, 1, 512, 1025)
    s = s + self_move_data[1025] + self_move_data[1536]
  end
  return s % 1000000
end

function table_int64_key_miss(n)
  local s = 0
  for i = 1, n do
    if wide_key_table[1099511627776 + 4096 + (i % 1024)] == nil then s = s + 1 end
  end
  return s
end

function table_int64_key_delete(rounds)
  local s = 0
  for r = 1, rounds do
    local t = {}
    local base = 1099511627776 + r * 2048
    for i = 1, 512 do t[base + i] = i end
    for i = 1, 512 do
      s = s + (t[base + i] or 0)
      t[base + i] = nil
    end
  end
  return s % 1000000
end

function table_string_key_insert(rounds)
  local s = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 512 do t["k" .. i] = i + r end
    s = s + t.k1 + t.k512
  end
  return s % 1000000
end

function table_sort_int64(rounds)
  local total = 0
  for r = 1, rounds do
    local t = {}
    local base = 1099511627776 + r
    for i = 1, 1200 do
      t[i] = base + ((i * 1103515245 + r * 12345) % 2147483647)
    end
    table.sort(t)
    total = (total + (t[1] % 1000000) + (t[#t] % 1000000)) % 1000000
  end
  return total
end

function table_sort_floats(rounds)
  local total = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 2000 do
      t[i] = ((i * 1103515245 + r * 12345) % 2147483647) / 7.0
    end
    table.sort(t)
    total = total + math.floor(t[1]) + math.floor(t[#t])
  end
  return total % 1000000
end

hash_next_table = {}
for i = 1, 512 do hash_next_table["h" .. i] = i end

function table_next_hash(n)
  local s = 0
  for _ = 1, n do
    for _, v in next, hash_next_table do s = s + v end
  end
  return s % 1000000
end

function string_find_class(n)
  local total = 0
  for _ = 1, n do
    local a, b, m = pattern_text:find("name=(%a+)")
    total = total + (a or 0) + (b or 0) + #(m or "")
  end
  return total % 1000000
end

function string_gsub_table(n)
  local repl = { a = "A", e = "E", i = "I", o = "O", u = "U" }
  local total = 0
  for _ = 1, n do
    local s, c = gsub_text:gsub("([aeiou])", repl)
    total = total + #s + c
  end
  return total
end

function string_gsub_no_match(n)
  local total = 0
  for _ = 1, n do
    local s, c = gsub_text:gsub("Z+", "z")
    total = total + #s + c
  end
  return total
end

function string_match_repeated(n)
  local total = 0
  for _ = 1, n do
    local a, b, c = pattern_text:match("id=(%d+)%s+name=(%a+)%s+status=(%a+)")
    total = total + #a + #b + #c
  end
  return total
end

function string_format_float(n)
  local total = 0
  for i = 1, n do total = total + #string.format("%.3f:%g", i / 7, i / 13) end
  return total
end

function string_format_wide_hex(n)
  local total = 0
  for i = 1, n do total = total + #string.format("%x:%X", 1099511627776 + i, i) end
  return total
end

function string_pack_unpack_many_i8(n)
  local total = 0
  for i = 1, n do
    local a = 1099511627776 + i
    local s = string.pack("<jjj", a, a + 7, -a)
    local x, y, z = string.unpack("<jjj", s)
    total = (total + (x % 1000000) + (y % 1000000) + ((-z) % 1000000)) % 1000000
  end
  return total
end

ascii_text = ("abcdefghijklmnopqrstuvwxyz0123456789"):rep(8)

function utf8_len_ascii(n)
  local total = 0
  for _ = 1, n do total = total + utf8.len(ascii_text) end
  return total
end

function utf8_codes_lax_loop(n)
  local total = 0
  for _ = 1, n do
    for _, c in utf8.codes(utf8_text, true) do total = total + c end
  end
  return total % 1000000
end

function math_fmod_int64(n)
  local s = 0
  local base = 1099511627776
  for i = 1, n do s = (s + math.fmod(base + i, 257)) % 1000000 end
  return s
end

function math_minmax_mixed_int64(n)
  local s = 0
  local base = 1099511627776
  for i = 1, n do
    local a = base + i
    local b = i % 1024
    s = (s + (math.max(a, b) % 1000000) - math.min(a, b)) % 1000000
  end
  return s
end

function pcall_multi_return(n)
  local function f(x) return x, x + 1, x + 2 end
  local s = 0
  for i = 1, n do
    local ok, a, b, c = pcall(f, i)
    if ok then s = s + a + b + c end
  end
  return s % 1000000
end

function coroutine_resume_args(n)
  local co = coroutine.create(function()
    local x = 0
    while true do x = coroutine.yield(x + 1) or 0 end
  end)
  local s = 0
  for i = 1, n do
    local ok, v = coroutine.resume(co, i)
    if ok then s = s + v end
  end
  return s % 1000000
end

chain_obj = setmetatable({}, { __index = { a = 3, b = 5 } })

function metamethod_index_table(n)
  local s = 0
  for _ = 1, n do s = s + chain_obj.a + chain_obj.b end
  return s % 1000000
end

eq_a = setmetatable({ v = 7 }, order_meta)
eq_b = setmetatable({ v = 8 }, order_meta)

function metamethod_eq_false(n)
  local s = 0
  for _ = 1, n do
    if eq_a == eq_b then s = s + 7 else s = s + 1 end
  end
  return s % 1000000
end

perf_global_value = 17
perf_global_sink = 0

function global_read_loop(n)
  local s = 0
  for _ = 1, n do s = s + perf_global_value end
  return s % 1000000
end

function global_write_loop(n)
  for i = 1, n do perf_global_sink = i end
  return perf_global_sink % 1000000
end

local field_obj = { a = 3, b = 5, c = 7 }

function table_field_read_loop(n)
  local t = field_obj
  local s = 0
  for _ = 1, n do s = s + t.a + t.b + t.c end
  return s % 1000000
end

function table_field_write_loop(n)
  local t = { v = 0 }
  local s = 0
  for i = 1, n do
    t.v = i
    s = s + t.v
  end
  return s % 1000000
end

function table_array_append_direct(rounds)
  local s = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 512 do t[#t + 1] = i + r end
    s = s + t[1] + t[#t]
  end
  return s % 1000000
end

function table_array_pop_direct(rounds)
  local s = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 512 do t[i] = i + r end
    for _ = 1, 512 do
      s = s + t[#t]
      t[#t] = nil
    end
  end
  return s % 1000000
end

local unpack32_data = {}
for i = 1, 32 do unpack32_data[i] = i end

function table_unpack_32(n)
  local unpack = table.unpack
  local s = 0
  for _ = 1, n do
    local a,b,c,d,e,f,g,h,i,j,k,l,m,o,p,q,r,s1,t,u,v,w,x,y,z,aa,ab,ac,ad,ae,af = unpack(unpack32_data, 1, 32)
    s = s + a + h + q + z + af + b + c + d + e + f + g + i + j + k + l + m + o + p + r + s1 + t + u + v + w + x + y + aa + ab + ac + ad + ae
  end
  return s % 1000000
end

function table_concat_slice(n)
  local total = 0
  for _ = 1, n do total = total + #table.concat(concat_parts, ",", 8, 24) end
  return total
end

function table_sort_nearly_sorted(rounds)
  local total = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 4000 do t[i] = i end
    for i = 1, 64 do
      local j = ((i * 97 + r) % 4000) + 1
      t[j], t[4001 - j] = t[4001 - j], t[j]
    end
    table.sort(t)
    total = total + t[1] + t[#t]
  end
  return total % 1000000
end

function table_sort_many_tiny(rounds)
  local total = 0
  for r = 1, rounds do
    local t = {
      (r * 17) % 257, (r * 29) % 257, (r * 43) % 257, (r * 71) % 257,
      (r * 89) % 257, (r * 97) % 257, (r * 113) % 257, (r * 131) % 257,
    }
    table.sort(t)
    total = total + t[1] + t[#t]
  end
  return total % 1000000
end

function table_sort_cmp_int64(rounds)
  local total = 0
  for r = 1, rounds do
    local t = {}
    local base = 1099511627776 + r
    for i = 1, 1000 do
      t[i] = { k = base + ((i * 1103515245 + r * 12345) % 2147483647) }
    end
    table.sort(t, function(a, b) return a.k < b.k end)
    total = (total + (t[1].k % 1000000) + (t[#t].k % 1000000)) % 1000000
  end
  return total
end

local len_text = ("LuaJIT arm64 wider text 0123456789;"):rep(16)

function string_len_loop(n)
  local s = 0
  for _ = 1, n do s = s + #len_text end
  return s % 1000000
end

function string_byte_single(n)
  local s = 0
  local limit = #byte_text
  for i = 1, n do s = s + byte_text:byte((i % limit) + 1) end
  return s % 1000000
end

function string_sub_negative(n)
  local total = 0
  for i = 1, n do
    local tail = (i % 16) + 1
    total = total + #sub_text:sub(-tail, -1)
  end
  return total
end

function string_compare_loop(n)
  local a = "arm64-luajit-0001"
  local b = "arm64-luajit-9999"
  local s = 0
  for i = 1, n do
    if (i & 1) == 0 then
      if a < b then s = s + 1 end
    else
      if b > a then s = s + 3 end
    end
  end
  return s % 1000000
end

function string_intern_concat(n)
  local total = 0
  for i = 1, n do
    local s = "wide_key_" .. (i % 4096) .. "_" .. (i % 17)
    total = total + #s
  end
  return total
end

function string_format_quote(n)
  local total = 0
  for i = 1, n do total = total + #string.format("%q:%d", "arm64\nwide", i) end
  return total
end

local float_strings = {}
for i = 1, 128 do float_strings[i] = string.format("%d.%03d", i * 17, i % 1000) end

function tonumber_float_parse(n)
  local s = 0
  for i = 1, n do s = s + tonumber(float_strings[(i % 128) + 1]) end
  return s
end

local int64_strings = {}
for i = 1, 128 do int64_strings[i] = tostring(1099511627776 + i * 7919) end

function tonumber_int64_parse(n)
  local s = 0
  for i = 1, n do s = (s + (tonumber(int64_strings[(i % 128) + 1]) % 1000000)) % 1000000 end
  return s
end

function math_abs_small(n)
  local s = 0
  for i = 1, n do
    local v = (i & 1) == 0 and -i or i
    s = s + math.abs(v)
  end
  return s % 1000000
end

function math_floor_negative(n)
  local s = 0
  for i = 1, n do s = s + math.floor(-i / 7.0) end
  return s % 1000000
end

function math_type_mixed(n)
  local s = 0
  for i = 1, n do
    local v = (i & 1) == 0 and i or (i + 0.5)
    if math.type(v) == "integer" then s = s + 1 else s = s + 3 end
  end
  return s % 1000000
end

function float_idiv_mod(n)
  local s = 0
  for i = 1, n do
    local x = i + 0.5
    s = s + (x // 7.0) + (x % 13.0)
  end
  return s % 1000000
end

function bit_mixed_const32(n)
  local s = 0x13572468
  for i = 1, n do
    s = (((s & 0x7f7f7f7f) | (i & 0x00ff00ff)) ~ 0x55aa55aa) & 0x7fffffff
  end
  return s
end

function bit_chain64_mixed(n)
  local s = 0x123456789abc
  for i = 1, n do
    local x = (i << 21) | (i >> 5)
    s = (((s ~ x) & 0xfffffffffffe) | (i & 1)) & 0xffffffffffff
  end
  return s & 0x7fffffff
end

function bit_shift_signed64(n)
  local s = -1099511627776
  for i = 1, n do
    s = ((s >> (i & 15)) ~ (i << 32)) & 0xffffffffffff
  end
  return s & 0x7fffffff
end

function int64_unary_minus(n)
  local s = 0
  local base = 1099511627776
  for i = 1, n do
    local v = -(base + i)
    s = (s + ((-v) % 1000000)) % 1000000
  end
  return s
end

function int64_compare_zero_branch(n)
  local s = 0
  local base = 1099511627776
  for i = 1, n do
    local v = (i & 1) == 0 and (base + i) or -(base + i)
    if v >= 0 then s = s + 5 else s = s + 1 end
  end
  return s % 1000000
end

function int64_add_compare_loop(n)
  local s = 0
  local base = 1099511627776
  for i = 1, n do
    local v = base + i
    if v > base + 1024 then s = s + (v % 97) else s = s - (v % 97) end
  end
  return s % 1000000
end

function numeric_for_float_span(n)
  local s = 0
  for i = 1.5, n + 0.5, 1.0 do s = s + (i % 97.0) end
  return s % 1000000
end

function while_loop_countdown(n)
  local s = 0
  while n > 0 do
    s = s + (n % 97)
    n = n - 1
  end
  return s % 1000000
end

function repeat_until_countdown(n)
  local s = 0
  repeat
    s = s + (n % 97)
    n = n - 1
  until n == 0
  return s % 1000000
end

function generic_for_iter_closure(n)
  local function iter(state, idx)
    idx = idx + 1
    if idx <= state.n then return idx, idx * 3 end
  end
  local state = { n = 64 }
  local s = 0
  for _ = 1, n do
    for _, v in iter, state, 0 do s = s + v end
  end
  return s % 1000000
end

function pairs_update_values(n)
  local t = {}
  for i = 1, 128 do t["p" .. i] = i end
  local s = 0
  for r = 1, n do
    for k, v in pairs(t) do
      local nv = (v + r) % 1000
      t[k] = nv
      s = s + nv
    end
  end
  return s % 1000000
end

function closure_factory(n)
  local s = 0
  for i = 1, n do
    local x = i
    local function f() return x + 1 end
    s = s + f()
  end
  return s % 1000000
end

function nested_closure_call(n)
  local function make(a)
    return function(b)
      return function(c) return a + b + c end
    end
  end
  local f = make(3)(5)
  local s = 0
  for i = 1, n do s = s + f(i) end
  return s % 1000000
end

function tail_call_chain(n)
  local function h(x) return x + 1 end
  local function g(x) return h(x) end
  local function f(x) return g(x) end
  local s = 0
  for i = 1, n do s = s + f(i) end
  return s % 1000000
end

function vararg_select_tail(n)
  local function f(...)
    return select(4, ...)
  end
  local s = 0
  for i = 1, n do
    local a, b, c = f(1, 2, 3, i, i + 1, i + 2)
    s = s + a + b + c
  end
  return s % 1000000
end

local bit_meta = { __band = function(a, b) return a.v & b end }
local bit_obj = setmetatable({ v = 0x12345678 }, bit_meta)

function metamethod_bitwise(n)
  local s = 0
  for i = 1, n do s = (s + (bit_obj & i)) % 1000000 end
  return s
end

local idiv_meta = { __idiv = function(a, b) return a.v // b end }
local idiv_obj = setmetatable({ v = 1099511627776 }, idiv_meta)

function metamethod_idiv(n)
  local s = 0
  for i = 1, n do s = (s + (idiv_obj // ((i % 97) + 1))) % 1000000 end
  return s
end

local pairs_meta_obj = setmetatable({ 3, 5, 7, 11 }, {
  __pairs = function(t)
    return next, t, nil
  end
})

function metamethod_pairs_loop(n)
  local s = 0
  for _ = 1, n do
    for _, v in pairs(pairs_meta_obj) do s = s + v end
  end
  return s % 1000000
end

function load_string_loop(n)
  local s = 0
  for i = 1, n do
    local f = assert(load("return " .. (i % 97)))
    s = s + f()
  end
  return s % 1000000
end

function debug_getinfo_loop(n)
  local s = 0
  local function f() return 1 end
  for _ = 1, n do
    local info = debug.getinfo(f, "Sln")
    s = s + (info.linedefined or 0) + #info.what
  end
  return s % 1000000
end

function gc_table_alloc_collect(rounds)
  local s = 0
  for r = 1, rounds do
    local bucket = {}
    for i = 1, 512 do bucket[i] = { i, r, i + r } end
    s = s + bucket[1][1] + bucket[#bucket][3]
    collectgarbage("collect")
  end
  return s % 1000000
end

function gc_string_churn(n)
  local s = 0
  for i = 1, n do
    local v = ("gc-string-%08x-%08x"):format(i, i * 17)
    s = s + #v
  end
  collectgarbage("collect")
  return s
end

function int64_add_const_box(n)
  local s = 1099511627776
  local hi = 1099512627776
  for _ = 1, n do
    s = s + 13
    if s > hi then s = s - 1000000 end
  end
  return s % 1000000
end

function int64_sub_zero_compare(n)
  local s = 0
  local base = 1099511627776
  for i = 1, n do
    local v = base - i
    if v > 0 then s = s + 1 else s = s + 7 end
  end
  return s % 1000000
end

function int64_mul_const_wrap(n)
  local s = 1099511627776
  for i = 1, n do
    s = (s * 33 + i) & 0xffffffffffff
  end
  return s & 0x7fffffff
end

function idiv_dynamic(n)
  local s = 0
  for i = 1, n do
    local d = (i % 97) + 1
    s = s + (i // d)
  end
  return s % 1000000
end

function mod_dynamic(n)
  local s = 0
  for i = 1, n do
    local d = (i % 97) + 1
    s = s + (i % d)
  end
  return s % 1000000
end

function numeric_for_int64_down(n)
  local s = 0
  local last = 1099511627776
  for i = last + n, last, -1 do
    s = (s + (i % 97)) % 1000000
  end
  return s
end

function numeric_for_int64_step3(n)
  local s = 0
  local first = 1099511627776
  for i = first, first + n * 3, 3 do
    s = (s + (i % 101)) % 1000000
  end
  return s
end

function bit_and64_power2_mask(n)
  local s = 0x123456789abc
  for i = 1, n do
    s = ((s + i) & 0xffffffff0000) ~ i
  end
  return s & 0x7fffffff
end

function bit_or64_small_const(n)
  local s = 0
  for i = 1, n do
    s = ((s | 0x100000000 | i) + 17) & 0xffffffffffff
  end
  return s & 0x7fffffff
end

function bit_xor64_small_const(n)
  local s = 0x100000000
  for i = 1, n do
    s = (s ~ 0x100000001 ~ i) & 0xffffffffffff
  end
  return s & 0x7fffffff
end

function bit_shift64_large_count(n)
  local s = 0x123456789abc
  for i = 1, n do
    s = ((s << ((i & 63) + 64)) ~ (s >> ((i & 63) + 64)) ~ i) & 0xffffffffffff
  end
  return s & 0x7fffffff
end

function bit_shift64_neg_count(n)
  local s = 0x123456789abc
  for i = 1, n do
    local sh = -((i & 31) + 1)
    s = ((s << sh) ~ (i << 32)) & 0xffffffffffff
  end
  return s & 0x7fffffff
end

function bit_branch32_test(n)
  local s = 0
  for i = 1, n do
    if (i & 0x55aa) ~= 0 then s = s + 3 else s = s + 1 end
  end
  return s % 1000000
end

function table_rawget_int64_key(n)
  local s = 0
  for i = 1, n do
    local k = 1099511627776 + ((i % 1024) + 1)
    s = s + rawget(wide_key_table, k)
  end
  return s % 1000000
end

function table_rawset_int64_key(n)
  local t = {}
  local s = 0
  for i = 1, n do
    local k = 1099511627776 + (i % 1024)
    rawset(t, k, i)
    s = s + rawget(t, k)
  end
  return s % 1000000
end

function table_set_int64_float_equiv(n)
  local s = 0
  local base = 1099511627776
  for i = 1, n do
    local k = base + (i % 256)
    local t = {}
    t[k + 0.0] = i
    s = s + (t[k] or 0)
  end
  return s % 1000000
end

function table_string_key_miss(n)
  local s = 0
  for i = 1, n do
    if hash["missing_" .. (i % 1024)] == nil then s = s + 1 end
  end
  return s
end

function table_sort_duplicates(rounds)
  local total = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 4000 do t[i] = (i + r) % 23 end
    table.sort(t)
    total = total + t[1] + t[#t]
  end
  return total % 1000000
end

function table_sort_cmp_strings(rounds)
  local total = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 1000 do
      t[i] = { k = ("k%06d"):format((i * 1103515245 + r) % 2147483647) }
    end
    table.sort(t, function(a, b) return a.k < b.k end)
    total = total + #t[1].k + #t[#t].k
  end
  return total
end

local concat_large_parts = {}
for i = 1, 256 do concat_large_parts[i] = ("p%03d"):format(i) end

function table_concat_large(n)
  local total = 0
  for _ = 1, n do total = total + #table.concat(concat_large_parts, "|") end
  return total
end

function table_unpack_slice_16(n)
  local unpack = table.unpack
  local s = 0
  for _ = 1, n do
    local a,b,c,d,e,f,g,h,i,j,k,l,m,o,p,q = unpack(unpack32_data, 9, 24)
    s = s + a+b+c+d+e+f+g+h+i+j+k+l+m+o+p+q
  end
  return s % 1000000
end

function string_find_absent_plain(n)
  local total = 0
  for _ = 1, n do
    local a = find_text:find("not-there", 1, true)
    total = total + (a or 1)
  end
  return total % 1000000
end

function string_find_long_plain(n)
  local hay = ("0123456789abcdef"):rep(128) .. "needle-plain-end"
  local total = 0
  for _ = 1, n do
    local a = hay:find("needle-plain-end", 1, true)
    total = total + (a or 0)
  end
  return total % 1000000
end

function string_match_no_capture(n)
  local total = 0
  for _ = 1, n do
    local m = pattern_text:match("status=%a+")
    total = total + #(m or "")
  end
  return total
end

function string_gsub_literal(n)
  local total = 0
  for _ = 1, n do
    local s, c = gsub_text:gsub("quick", "slow")
    total = total + #s + c
  end
  return total
end

function string_sub_large_slice(n)
  local total = 0
  local text = sub_text:rep(8)
  local limit = #text - 64
  for i = 1, n do
    local p = (i % limit) + 1
    total = total + #text:sub(p, p + 63)
  end
  return total
end

function string_byte_full_short(n)
  local s = 0
  local text = "abcdefghijklmnop"
  for _ = 1, n do
    local a,b,c,d,e,f,g,h,i,j,k,l,m,o,p,q = text:byte(1, -1)
    s = s + a+b+c+d+e+f+g+h+i+j+k+l+m+o+p+q
  end
  return s % 1000000
end

function string_char_many(n)
  local total = 0
  for i = 1, n do
    total = total + #string.char(
      65 + (i % 26), 66 + (i % 25), 67 + (i % 24), 68 + (i % 23),
      69 + (i % 22), 70 + (i % 21), 71 + (i % 20), 72 + (i % 19)
    )
  end
  return total
end

function string_rep_sep(n)
  local total = 0
  for i = 1, n do total = total + #("ab"):rep((i % 16) + 1, ",") end
  return total
end

function string_format_mixed(n)
  local total = 0
  for i = 1, n do
    total = total + #string.format("%d/%x/%.2f", 1099511627776 + i, i, i / 17)
  end
  return total
end

function string_pack_unpack_float(n)
  local total = 0
  for i = 1, n do
    local s = string.pack("<fd", i / 7, i / 13)
    local a, b = string.unpack("<fd", s)
    total = total + math.floor(a) + math.floor(b)
  end
  return total % 1000000
end

local hex64_strings = {}
for i = 1, 128 do hex64_strings[i] = string.format("%x", 1099511627776 + i * 257) end

function tonumber_hex64_parse(n)
  local s = 0
  for i = 1, n do
    s = (s + (tonumber(hex64_strings[(i % 128) + 1], 16) % 1000000)) % 1000000
  end
  return s
end

function math_modf_loop(n)
  local s = 0
  for i = 1, n do
    local a, b = math.modf(i / 7)
    s = s + a + math.floor(b * 100)
  end
  return s % 1000000
end

function math_floor_int_passthrough(n)
  local s = 0
  for i = 1, n do s = s + math.floor(i) end
  return s % 1000000
end

function math_tointeger_int64_string(n)
  local s = 0
  for i = 1, n do
    s = (s + (math.tointeger(int64_strings[(i % 128) + 1]) % 1000000)) % 1000000
  end
  return s
end

local order64_a = setmetatable({ v = 1099511627776 + 7 }, order_meta)
local order64_b = setmetatable({ v = 1099511627776 + 11 }, order_meta)

function metamethod_order_int64(n)
  local s = 0
  for _ = 1, n do
    if order64_a < order64_b then s = s + 1 end
    if order64_a <= order64_b then s = s + 3 end
  end
  return s % 1000000
end

function pcall_deep_stack(n)
  local function c(x) return x + 1 end
  local function b(x) return c(x) end
  local function a(x) return b(x) end
  local s = 0
  for i = 1, n do
    local ok, v = pcall(a, i)
    if ok then s = s + v end
  end
  return s % 1000000
end

function coroutine_pingpong(n)
  local co = coroutine.create(function(x)
    while true do x = coroutine.yield((x or 0) + 1) end
  end)
  local ok, v = coroutine.resume(co, 0)
  local s = ok and v or 0
  for i = 1, n do
    ok, v = coroutine.resume(co, i)
    if ok then s = s + v end
  end
  return s % 1000000
end

function debug_getlocal_loop(n)
  local s = 0
  local x = 17
  for _ = 1, n do
    local name, value = debug.getlocal(1, 2)
    s = s + #(name or "") + ((value == x) and 1 or 0)
  end
  return s % 1000000
end

function gc_table_churn_no_collect(rounds)
  local s = 0
  for r = 1, rounds do
    for i = 1, 512 do
      local t = { a = i, b = r, c = i + r }
      s = s + t.a + t.c
    end
  end
  collectgarbage("collect")
  return s % 1000000
end

local next_array_extra_data = {}
local next_sparse_extra_data = {}
for i = 1, 1024 do
  next_array_extra_data[i] = i
  next_sparse_extra_data[i * 16] = i
end

function table_next_array_extra(rounds)
  local s = 0
  for _ = 1, rounds do
    local k
    repeat
      local v
      k, v = next(next_array_extra_data, k)
      if v then s = s + v end
    until k == nil
  end
  return s % 1000000
end

function table_next_sparse_extra(rounds)
  local s = 0
  for _ = 1, rounds do
    local k
    repeat
      local v
      k, v = next(next_sparse_extra_data, k)
      if v then s = s + v end
    until k == nil
  end
  return s % 1000000
end

function table_pairs_array_extra(rounds)
  local s = 0
  for _ = 1, rounds do
    for _, v in pairs(next_array_extra_data) do s = s + v end
  end
  return s % 1000000
end

function table_sort_cmp_upvalue_extra(rounds)
  local total = 0
  local salt = 17
  local function before(a, b)
    return ((a ~ salt) & 0xffff) < ((b ~ salt) & 0xffff)
  end
  for r = 1, rounds do
    local t = {}
    for i = 1, 3000 do t[i] = (i * 1103515245 + r) & 0xffff end
    table.sort(t, before)
    total = total + t[1] + t[#t]
  end
  return total % 1000000
end

function table_sort_records_extra(rounds)
  local total = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 1200 do
      t[i] = { a = (i * 97 + r) % 4096, b = i }
    end
    table.sort(t, function(x, y)
      if x.a == y.a then return x.b < y.b end
      return x.a < y.a
    end)
    total = total + t[1].a + t[#t].b
  end
  return total % 1000000
end

function table_sort_large_int_extra(rounds)
  local total = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 12000 do
      t[i] = (i * 1664525 + r * 1013904223) % 2147483647
    end
    table.sort(t)
    total = total + t[1] + t[#t]
  end
  return total % 1000000
end

function string_concat_small_extra(n)
  local total = 0
  for i = 1, n do
    local s = ""
    for j = 1, 12 do s = s .. string.char(64 + ((i + j) % 26)) end
    total = total + #s
  end
  return total
end

function string_sub_tiny_extra(n)
  local total = 0
  local text = ("abcdefghijklmnopqrstuvwxyz0123456789"):rep(8)
  local limit = #text
  for i = 1, n do
    local p = (i % limit) + 1
    total = total + #text:sub(p, p)
  end
  return total
end

function string_match_captures_extra(n)
  local total = 0
  local text = "name=alpha id=12345 status=ready"
  for _ = 1, n do
    local a, b, c = text:match("name=(%a+) id=(%d+) status=(%a+)")
    total = total + #a + #b + #c
  end
  return total
end

function string_arith_int64_extra(n)
  local s = 0
  local a = "1099511627776"
  for i = 1, n do
    s = (s + a + i) % 1000000
  end
  return s
end

function bit64_and_shift_extra(n)
  local s = 0x123456789abc
  for i = 1, n do
    s = (((s & 0xffffffffff) << (i & 15)) ~ (s >> ((i & 7) + 1))) & 0xffffffffffff
  end
  return s & 0x7fffffff
end

function bit64_branch_not_extra(n)
  local s = 0x100000000
  local c = 0
  for i = 1, n do
    s = (~(s + i)) & 0xffffffffffff
    if (s & 0x80000000) ~= 0 then c = c + 3 else c = c + 1 end
  end
  return c % 1000000
end

function int64_compare_threshold_extra(n)
  local s = 0
  local threshold = 1099511627776 + 4096
  for i = 1, n do
    local v = 1099511627776 + (i & 8191)
    if v < threshold then s = s + 1 else s = s + 3 end
  end
  return s % 1000000
end

function int64_minmax_chain_extra(n)
  local s = 0
  for i = 1, n do
    local a = 1099511627776 + i
    local b = -1099511627776 - i
    s = (s + math.min(a, b) + math.max(a, b)) % 1000000
  end
  return s
end

function int64_compare_eq_const_extra(n)
  local s = 0
  local base = 1099511627776
  local needle = base + 512
  for i = 1, n do
    local v = base + (i & 1023)
    if v == needle then s = s + 7 else s = s + 1 end
  end
  return s % 1000000
end

function int64_compare_between_extra(n)
  local s = 0
  local base = 1099511627776
  local lo = base + 2048
  local hi = base + 6144
  for i = 1, n do
    local v = base + (i & 8191)
    if v >= lo and v <= hi then s = s + 5 else s = s + 1 end
  end
  return s % 1000000
end

function int64_compare_descending_extra(n)
  local s = 0
  local top = math.maxinteger - 4096
  local limit = math.maxinteger - 8192
  for i = 1, n do
    local v = top - (i & 8191)
    if v > limit then s = s + 3 else s = s + 1 end
  end
  return s % 1000000
end

function bit64_const_mask_chain_extra(n)
  local s = 0xffffffffffff
  for i = 1, n do
    s = (((s & 0x123456789abc) | 0x100000000) ~ (i << 9)) & 0xffffffffffff
  end
  return s & 0x7fffffff
end

function bit64_const_or_xor_chain_extra(n)
  local s = 0x100000000
  for i = 1, n do
    s = ((s | 0x100000001) ~ 0x123456789abc ~ i) & 0xffffffffffff
  end
  return s & 0x7fffffff
end

function bit64_compare_mask_zero_extra(n)
  local s = 0
  local base = 1099511627776
  for i = 1, n do
    local v = (base + i) & 0xff00
    if v == 0 then s = s + 5 else s = s + 1 end
  end
  return s % 1000000
end

local next_mixed_extra_data = {}
for i = 1, 512 do
  next_mixed_extra_data[i] = i
  next_mixed_extra_data["k" .. i] = i * 2
  next_mixed_extra_data[1099511627776 + i] = i * 3
end

function table_next_mixed_extra(rounds)
  local s = 0
  for _ = 1, rounds do
    local k
    repeat
      local v
      k, v = next(next_mixed_extra_data, k)
      if v then s = s + v end
    until k == nil
  end
  return s % 1000000
end

function table_pairs_sparse_extra(rounds)
  local s = 0
  for _ = 1, rounds do
    for _, v in pairs(next_sparse_extra_data) do s = s + v end
  end
  return s % 1000000
end

function table_pairs_int64_keys_extra(rounds)
  local s = 0
  for _ = 1, rounds do
    for k, v in pairs(wide_key_table) do
      s = s + (k & 255) + v
    end
  end
  return s % 1000000
end

function load_dump_many_protos_extra(n)
  local dumps = {}
  for j = 1, 16 do
    local f = assert(load("return function(x) return x + " .. j .. " end"))()
    dumps[j] = string.dump(f)
  end
  local s = 0
  for i = 1, n do
    local f = assert(load(dumps[(i % 16) + 1]))
    s = s + f(i)
  end
  return s % 1000000
end

local debug_upvalue_extra_target = (function()
  local value = 7
  return function() return value end
end)()

function debug_getupvalue_extra(n)
  local s = 0
  for _ = 1, n do
    local name, value = debug.getupvalue(debug_upvalue_extra_target, 1)
    s = s + #name + value
  end
  return s % 1000000
end

function debug_setupvalue_extra(n)
  local value = 0
  local function f() return value end
  local s = 0
  for i = 1, n do
    debug.setupvalue(f, 1, i & 255)
    s = s + f()
  end
  return s % 1000000
end

function closure_call_mixed_extra(n)
  local x = 3
  local function a(v) x = (x + v) & 0xffff; return x end
  local function b(v) return a(v + 1) end
  local s = 0
  for i = 1, n do s = s + b(i) end
  return s % 1000000
end

function load_dump_extra(n)
  local dumped = string.dump(function(x) return x + 17 end)
  local s = 0
  for i = 1, n do
    local f = assert(load(dumped))
    s = s + f(i)
  end
  return s % 1000000
end

function bit64_rotate_var_extra(n)
  local s = 0x123456789abc
  for i = 1, n do
    local c = i & 31
    s = (((s << c) | (s >> (48 - c))) ~ i) & 0xffffffffffff
  end
  return s & 0x7fffffff
end

function bit64_extract_insert_extra(n)
  local s = 0x123456789abc
  for i = 1, n do
    local from = (i >> 1) & 31
    local to = (i >> 4) & 31
    local field = (s >> from) & 0xff
    s = ((s & ~(0xff << to)) | (field << to) | i) & 0xffffffffffff
  end
  return s & 0x7fffffff
end

function bit32_extract_branch_extra(n)
  local x = 0x12345678
  local s = 0
  for i = 1, n do
    x = ((x ~ (i * 1103515245)) + i) & 0xffffffff
    local b = (x >> ((i & 3) * 8)) & 0xff
    if b < 64 then s = s + 3 else s = s + 1 end
  end
  return s % 1000000
end

function bit64_math_ult_branch_extra(n)
  local s = 0
  local base = 0x7ffffffff000
  local threshold = base + 2048
  for i = 1, n do
    local v = (base + ((i * 2654435761) & 4095)) & 0xffffffffffff
    if math.ult(v, threshold) then s = s + 5 else s = s + 1 end
  end
  return s % 1000000
end

function int64_compare_float_threshold_extra(n)
  local s = 0
  local base = 1099511627776
  local threshold = 1099511627776.0 + 4096.0
  for i = 1, n do
    local v = base + (i & 8191)
    if v < threshold then s = s + 1 else s = s + 3 end
  end
  return s % 1000000
end

function int64_compare_table_bound_extra(n)
  local s = 0
  local base = 1099511627776
  local bounds = { base + 1024, base + 7168 }
  for i = 1, n do
    local v = base + (i & 8191)
    if v >= bounds[1] and v <= bounds[2] then s = s + 9 else s = s + 1 end
  end
  return s % 1000000
end

function numeric_for_int64_large_step_extra(n)
  local s = 0
  local base = 1099511627776
  local limit = base + n * 5
  for i = base, limit, 5 do
    s = (s + i) % 1000000
  end
  return s
end

function table_next_int64_keys_extra(rounds)
  local s = 0
  for _ = 1, rounds do
    local k
    repeat
      local v
      k, v = next(wide_key_table, k)
      if v then s = s + (k & 255) + v end
    until k == nil
  end
  return s % 1000000
end

local pair_string_extra_data = {}
for i = 1, 1024 do pair_string_extra_data[("wide_key_%04d"):format(i)] = i end

function table_pairs_string_keys_extra(rounds)
  local s = 0
  for _ = 1, rounds do
    for k, v in pairs(pair_string_extra_data) do
      s = s + #k + v
    end
  end
  return s % 1000000
end

function table_pairs_mixed_sparse_extra(rounds)
  local s = 0
  for _ = 1, rounds do
    for k, v in pairs(next_mixed_extra_data) do
      if type(k) == "number" then
        s = s + v + (k & 15)
      else
        s = s + v + #k
      end
    end
  end
  return s % 1000000
end

function table_sort_tiny_cmp_extra(rounds)
  local total = 0
  local function before(a, b)
    local ka = (a * 17) % 257
    local kb = (b * 17) % 257
    if ka == kb then return a < b end
    return ka < kb
  end
  for r = 1, rounds do
    local t = {
      (r * 17) % 1021, (r * 29) % 1021, (r * 43) % 1021, (r * 71) % 1021,
      (r * 89) % 1021, (r * 97) % 1021, (r * 113) % 1021, (r * 131) % 1021,
      (r * 149) % 1021, (r * 167) % 1021, (r * 181) % 1021, (r * 193) % 1021,
    }
    table.sort(t, before)
    total = total + t[1] + t[#t]
  end
  return total % 1000000
end

function table_sort_cmp_desc_extra(rounds)
  local total = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 5000 do t[i] = (i * 1103515245 + r) & 0x7fffffff end
    table.sort(t, function(a, b) return a > b end)
    total = total + t[1] + t[#t]
  end
  return total % 1000000
end

function table_sort_large_strings_extra(rounds)
  local total = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 1600 do
      t[i] = ("s%08x_%04d"):format((i * 1664525 + r * 1013904223) & 0xffffffff, i)
    end
    table.sort(t)
    total = total + #t[1] + #t[#t]
  end
  return total
end

function table_sort_wide_records_extra(rounds)
  local total = 0
  for r = 1, rounds do
    local t = {}
    local base = 1099511627776 + r
    for i = 1, 1500 do
      t[i] = { k = base + ((i * 48271 + r) % 1048576), i = i }
    end
    table.sort(t, function(a, b)
      if a.k == b.k then return a.i < b.i end
      return a.k < b.k
    end)
    total = (total + (t[1].k & 0xffff) + t[#t].i) % 1000000
  end
  return total
end

function table_sort_equal_keys_extra(rounds)
  local total = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 1800 do
      t[i] = { k = (i + r) % 9, i = i }
    end
    table.sort(t, function(a, b)
      if a.k == b.k then return a.i < b.i end
      return a.k < b.k
    end)
    total = total + t[1].k + t[#t].i
  end
  return total % 1000000
end

local move_int64_extra_src = {}
for i = 1, 512 do move_int64_extra_src[i] = 1099511627776 + i end

function table_move_int64_extra(n)
  local s = 0
  for _ = 1, n do
    local t = {}
    table.move(move_int64_extra_src, 1, 512, 1, t)
    s = (s + t[1] + t[512]) % 1000000
  end
  return s
end

function string_find_frontier_extra(n)
  local total = 0
  local text = ("alpha beta gamma delta epsilon "):rep(8)
  for _ = 1, n do
    local a, b = text:find("%f[%a]gamma%f[%A]")
    total = total + a + b
  end
  return total
end

function string_gsub_many_repl_extra(n)
  local total = 0
  local text = ("a1 b22 c333 d4444 "):rep(16)
  for _ = 1, n do
    local s = text:gsub("%d+", "N")
    total = total + #s
  end
  return total
end

function string_format_int64_mix_extra(n)
  local total = 0
  local base = 1099511627776
  for i = 1, n do
    local v = base + (i & 1023)
    total = total + #("%d/%x/%s"):format(v, v, v)
  end
  return total
end

function gc_closure_alloc_collect_extra(rounds)
  local s = 0
  for r = 1, rounds do
    local funcs = {}
    for i = 1, 512 do
      local x = i + r
      funcs[i] = function() return x end
    end
    s = s + funcs[(r % 512) + 1]()
    collectgarbage("collect")
  end
  return s % 1000000
end

local benches = {
  { "empty_loop", empty_loop, 100000000 },
  { "int_add_wide", int_add_wide, 40000000 },
  { "int64_add_sub", int64_add_sub, 15000000 },
  { "int_add32", int_add32, 70000000 },
  { "int_mul32", int_mul32, 35000000 },
  { "int_mod_small", int_mod_small, 40000000 },
  { "idiv_const", idiv_const, 35000000 },
  { "idiv_negative", idiv_negative, 16000000 },
  { "idiv_power2", idiv_power2, 35000000 },
  { "mod_const", mod_const, 45000000 },
  { "mod_power2", mod_power2, 45000000 },
  { "numeric_for_int64_span", numeric_for_int64_span, 2500000 },
  { "int_compare_branch", int_compare_branch, 35000000 },
  { "int_compare_eq_chain", int_compare_eq_chain, 30000000 },
  { "int64_compare_minmax", int64_compare_minmax, 14000000 },
  { "int64_eq_branch", int64_eq_branch, 12000000 },
  { "int64_mod_const", int64_mod_const, 8000000 },
  { "int64_idiv_const", int64_idiv_const, 5000000 },
  { "int64_mul_wrap", int64_mul_wrap, 5000000 },
  { "float_compare_branch", float_compare_branch, 15000000 },
  { "float_arith", float_arith, 25000000 },
  { "pure_bit_xor", pure_bit_xor, 50000000 },
  { "bit_and32", bit_and32, 35000000 },
  { "bit_or32", bit_or32, 35000000 },
  { "bit_xor32", bit_xor32, 35000000 },
  { "bit_not32", bit_not32, 35000000 },
  { "bit_shift32_const", bit_shift32_const, 30000000 },
  { "bit_shift32_var", bit_shift32_var, 25000000 },
  { "bit_shift_mask", bit_shift_mask, 35000000 },
  { "bitwise_idiv_unsigned", bitwise_idiv_unsigned, 25000000 },
  { "bitwise_idiv_mix", bitwise_idiv_mix, 25000000 },
  { "idiv_mod", idiv_mod, 25000000 },
  { "bit_shift64_const", bit_shift64_const, 12000000 },
  { "bit_and64", bit_and64, 12000000 },
  { "bit_and64_const", bit_and64_const, 12000000 },
  { "bit_or64", bit_or64, 12000000 },
  { "bit_or64_const", bit_or64_const, 12000000 },
  { "bit_xor64", bit_xor64, 12000000 },
  { "bit_xor64_const", bit_xor64_const, 12000000 },
  { "bit_shift64_var", bit_shift64_var, 9000000 },
  { "bit_shift64_right", bit_shift64_right, 12000000 },
  { "bit_not64", bit_not64, 12000000 },
  { "bit64_branch_test", bit64_branch_test, 12000000 },
  { "array_sum_wide", array_sum_wide, 180 },
  { "array_sum_chunk", array_sum_chunk, 120000 },
  { "array_read_seq", array_read_seq, 180 },
  { "array_read_stride", array_read_stride, 6000000 },
  { "array_write_seq", array_write_seq, 1000 },
  { "table_new_small", table_new_small, 700000 },
  { "hash_lookup", hash_lookup, 10000000 },
  { "hash_update", hash_update, 1500000 },
  { "table_int64_key_lookup", table_int64_key_lookup, 3000000 },
  { "table_int64_key_insert", table_int64_key_insert, 900 },
  { "table_int64_key_update", table_int64_key_update, 1500000 },
  { "table_int64_key_miss", table_int64_key_miss, 2500000 },
  { "table_int64_key_delete", table_int64_key_delete, 650 },
  { "table_float_key_lookup", table_float_key_lookup, 4500000 },
  { "table_mixed_key_lookup", table_mixed_key_lookup, 1200000 },
  { "table_next_dense", table_next_dense, 35000 },
  { "table_next_hash", table_next_hash, 16000 },
  { "rawget_rawset", rawget_rawset, 1800000 },
  { "ipairs_iter", ipairs_iter, 90000 },
  { "pairs_iter", pairs_iter, 90000 },
  { "function_calls", function_calls, 25000000 },
  { "closure_alloc", closure_alloc, 700000 },
  { "pcall_success", pcall_success, 250000 },
  { "table_sort_int", table_sort_int, 70 },
  { "table_sort_cmp", table_sort_cmp, 35 },
  { "table_sort_strings", table_sort_strings, 35 },
  { "table_sort_reverse", table_sort_reverse, 70 },
  { "table_sort_int64", table_sort_int64, 35 },
  { "table_sort_floats", table_sort_floats, 35 },
  { "string_concat", string_concat, 200000 },
  { "table_concat", table_concat, 12000 },
  { "table_move_copy", table_move_copy, 30000 },
  { "table_move_large_copy", table_move_large_copy, 7000 },
  { "table_move_self_nooverlap", table_move_self_nooverlap, 30000 },
  { "table_pack_loop", table_pack_loop, 350000 },
  { "table_remove_front", table_remove_front, 1200 },
  { "table_insert_tail", table_insert_tail, 2600 },
  { "table_remove_tail", table_remove_tail, 2200 },
  { "table_insert_front", table_insert_front, 1800 },
  { "table_remove_middle", table_remove_middle, 1400 },
  { "table_move_overlap_forward", table_move_overlap_forward, 60000 },
  { "table_move_overlap_backward", table_move_overlap_backward, 60000 },
  { "table_string_key_insert", table_string_key_insert, 650 },
  { "table_concat_numbers", table_concat_numbers, 9000 },
  { "string_find_plain", string_find_plain, 180000 },
  { "string_find_init", string_find_init, 300000 },
  { "string_find_pattern", string_find_pattern, 80000 },
  { "string_find_class", string_find_class, 90000 },
  { "string_gsub", string_gsub, 12000 },
  { "string_gsub_capture", string_gsub_capture, 7000 },
  { "string_gsub_func", string_gsub_func, 3000 },
  { "string_gsub_table", string_gsub_table, 5000 },
  { "string_gsub_no_match", string_gsub_no_match, 16000 },
  { "string_lower_upper", string_lower_upper, 12000 },
  { "string_reverse_loop", string_reverse_loop, 20000 },
  { "string_rep_loop", string_rep_loop, 250000 },
  { "string_char_loop", string_char_loop, 500000 },
  { "string_byte_range", string_byte_range, 900000 },
  { "string_gmatch_captures", string_gmatch_captures, 120000 },
  { "string_match_repeated", string_match_repeated, 90000 },
  { "string_format_int64", string_format_int64, 100000 },
  { "string_format_float", string_format_float, 90000 },
  { "string_format_wide_hex", string_format_wide_hex, 90000 },
  { "string_pack_unpack_mixed", string_pack_unpack_mixed, 140000 },
  { "string_pack_unpack_many_i8", string_pack_unpack_many_i8, 90000 },
  { "math_sin", math_sin, 1800000 },
  { "math_floor", math_floor, 3500000 },
  { "math_abs_int64", math_abs_int64, 3000000 },
  { "math_fmod_int64", math_fmod_int64, 3500000 },
  { "math_sqrt_log", math_sqrt_log, 900000 },
  { "math_type_loop", math_type_loop, 2500000 },
  { "math_tointeger_loop", math_tointeger_loop, 4500000 },
  { "math_ult_loop", math_ult_loop, 7000000 },
  { "math_minmax_int64", math_minmax_int64, 2500000 },
  { "math_minmax_mixed_int64", math_minmax_mixed_int64, 1800000 },
  { "coroutine_resume", coroutine_resume, 250000 },
  { "coroutine_resume_args", coroutine_resume_args, 180000 },
  { "coroutine_yield_multi", coroutine_yield_multi, 180000 },
  { "coroutine_wrap_loop", coroutine_wrap_loop, 220000 },
  { "numeric_for_down", numeric_for_down, 40000000 },
  { "branch_mod_loop", branch_mod_loop, 30000000 },
  { "bit_rotate64", bit_rotate64, 15000000 },
  { "table_len_loop", table_len_loop, 30000000 },
  { "table_insert_remove", table_insert_remove, 30000 },
  { "table_unpack_multi", table_unpack_multi, 1000000 },
  { "string_byte_sum", string_byte_sum, 25000 },
  { "string_sub_loop", string_sub_loop, 700000 },
  { "string_format_num", string_format_num, 120000 },
  { "string_match_pattern", string_match_pattern, 180000 },
  { "string_pack_unpack_i4", string_pack_unpack_i4, 250000 },
  { "string_pack_unpack_i8", string_pack_unpack_i8, 180000 },
  { "tonumber_parse", tonumber_parse, 2500000 },
  { "tonumber_base16", tonumber_base16, 180000 },
  { "tostring_int", tostring_int, 1200000 },
  { "tostring_float", tostring_float, 500000 },
  { "math_min_max", math_min_max, 12000000 },
  { "closure_upvalue_update", closure_upvalue_update, 2500000 },
  { "upvalue_read_loop", upvalue_read_loop, 8000000 },
  { "method_call", method_call, 2500000 },
  { "select_vararg", select_vararg, 1800000 },
  { "vararg_sum", vararg_sum, 900000 },
  { "tail_call_pair", tail_call_pair, 7000000 },
  { "metamethod_index", metamethod_index, 1200000 },
  { "metamethod_index_table", metamethod_index_table, 1200000 },
  { "metamethod_add", metamethod_add, 700000 },
  { "metamethod_call", metamethod_call, 700000 },
  { "metamethod_len", metamethod_len, 1200000 },
  { "metamethod_newindex", metamethod_newindex, 800000 },
  { "metamethod_compare", metamethod_compare, 900000 },
  { "metamethod_eq_false", metamethod_eq_false, 900000 },
  { "metamethod_concat", metamethod_concat, 180000 },
  { "global_read_loop", global_read_loop, 20000000 },
  { "global_write_loop", global_write_loop, 5000000 },
  { "table_field_read_loop", table_field_read_loop, 20000000 },
  { "table_field_write_loop", table_field_write_loop, 8000000 },
  { "table_array_append_direct", table_array_append_direct, 1600 },
  { "table_array_pop_direct", table_array_pop_direct, 1600 },
  { "table_unpack_32", table_unpack_32, 450000 },
  { "table_concat_slice", table_concat_slice, 16000 },
  { "table_sort_nearly_sorted", table_sort_nearly_sorted, 70 },
  { "table_sort_many_tiny", table_sort_many_tiny, 5000 },
  { "table_sort_cmp_int64", table_sort_cmp_int64, 30 },
  { "string_len_loop", string_len_loop, 20000000 },
  { "string_byte_single", string_byte_single, 5000000 },
  { "string_sub_negative", string_sub_negative, 700000 },
  { "string_compare_loop", string_compare_loop, 5000000 },
  { "string_intern_concat", string_intern_concat, 150000 },
  { "string_format_quote", string_format_quote, 50000 },
  { "tonumber_float_parse", tonumber_float_parse, 900000 },
  { "tonumber_int64_parse", tonumber_int64_parse, 400000 },
  { "math_abs_small", math_abs_small, 20000000 },
  { "math_floor_negative", math_floor_negative, 3000000 },
  { "math_type_mixed", math_type_mixed, 2000000 },
  { "float_idiv_mod", float_idiv_mod, 5000000 },
  { "bit_mixed_const32", bit_mixed_const32, 25000000 },
  { "bit_chain64_mixed", bit_chain64_mixed, 8000000 },
  { "bit_shift_signed64", bit_shift_signed64, 8000000 },
  { "int64_unary_minus", int64_unary_minus, 4000000 },
  { "int64_compare_zero_branch", int64_compare_zero_branch, 12000000 },
  { "int64_add_compare_loop", int64_add_compare_loop, 8000000 },
  { "numeric_for_float_span", numeric_for_float_span, 10000000 },
  { "while_loop_countdown", while_loop_countdown, 30000000 },
  { "repeat_until_countdown", repeat_until_countdown, 30000000 },
  { "generic_for_iter_closure", generic_for_iter_closure, 100000 },
  { "pairs_update_values", pairs_update_values, 45000 },
  { "closure_factory", closure_factory, 400000 },
  { "nested_closure_call", nested_closure_call, 2000000 },
  { "tail_call_chain", tail_call_chain, 5000000 },
  { "vararg_select_tail", vararg_select_tail, 1000000 },
  { "metamethod_bitwise", metamethod_bitwise, 300000 },
  { "metamethod_idiv", metamethod_idiv, 220000 },
  { "metamethod_pairs_loop", metamethod_pairs_loop, 450000 },
  { "load_string_loop", load_string_loop, 25000 },
  { "debug_getinfo_loop", debug_getinfo_loop, 50000 },
  { "gc_table_alloc_collect", gc_table_alloc_collect, 450 },
  { "gc_string_churn", gc_string_churn, 50000 },
  { "pcall_error", pcall_error, 120000 },
  { "xpcall_error", xpcall_error, 80000 },
  { "pcall_multi_return", pcall_multi_return, 200000 },
  { "pcall_vararg_success", pcall_vararg_success, 220000 },
  { "coroutine_create_resume", coroutine_create_resume, 80000 },
  { "string_gmatch_words", string_gmatch_words, 70000 },
  { "utf8_codes_loop", utf8_codes_loop, 40000 },
  { "utf8_codes_lax_loop", utf8_codes_lax_loop, 40000 },
  { "utf8_len_loop", utf8_len_loop, 120000 },
  { "utf8_len_ascii", utf8_len_ascii, 120000 },
  { "utf8_codepoint_loop", utf8_codepoint_loop, 25000 },
  { "utf8_char_build", utf8_char_build, 160000 },
  { "utf8_offset_loop", utf8_offset_loop, 260000 },
  { "int64_add_const_box", int64_add_const_box, 10000000 },
  { "int64_sub_zero_compare", int64_sub_zero_compare, 10000000 },
  { "int64_mul_const_wrap", int64_mul_const_wrap, 6000000 },
  { "idiv_dynamic", idiv_dynamic, 12000000 },
  { "mod_dynamic", mod_dynamic, 12000000 },
  { "numeric_for_int64_down", numeric_for_int64_down, 2200000 },
  { "numeric_for_int64_step3", numeric_for_int64_step3, 2200000 },
  { "bit_and64_power2_mask", bit_and64_power2_mask, 9000000 },
  { "bit_or64_small_const", bit_or64_small_const, 9000000 },
  { "bit_xor64_small_const", bit_xor64_small_const, 9000000 },
  { "bit_shift64_large_count", bit_shift64_large_count, 7000000 },
  { "bit_shift64_neg_count", bit_shift64_neg_count, 7000000 },
  { "bit_branch32_test", bit_branch32_test, 25000000 },
  { "table_rawget_int64_key", table_rawget_int64_key, 2200000 },
  { "table_rawset_int64_key", table_rawset_int64_key, 700000 },
  { "table_set_int64_float_equiv", table_set_int64_float_equiv, 180000 },
  { "table_string_key_miss", table_string_key_miss, 700000 },
  { "table_sort_duplicates", table_sort_duplicates, 70 },
  { "table_sort_cmp_strings", table_sort_cmp_strings, 30 },
  { "table_concat_large", table_concat_large, 4000 },
  { "table_unpack_slice_16", table_unpack_slice_16, 700000 },
  { "string_find_absent_plain", string_find_absent_plain, 180000 },
  { "string_find_long_plain", string_find_long_plain, 120000 },
  { "string_match_no_capture", string_match_no_capture, 140000 },
  { "string_gsub_literal", string_gsub_literal, 7000 },
  { "string_sub_large_slice", string_sub_large_slice, 450000 },
  { "string_byte_full_short", string_byte_full_short, 700000 },
  { "string_char_many", string_char_many, 300000 },
  { "string_rep_sep", string_rep_sep, 180000 },
  { "string_format_mixed", string_format_mixed, 60000 },
  { "string_pack_unpack_float", string_pack_unpack_float, 140000 },
  { "tonumber_hex64_parse", tonumber_hex64_parse, 260000 },
  { "math_modf_loop", math_modf_loop, 2500000 },
  { "math_floor_int_passthrough", math_floor_int_passthrough, 10000000 },
  { "math_tointeger_int64_string", math_tointeger_int64_string, 300000 },
  { "metamethod_order_int64", metamethod_order_int64, 700000 },
  { "pcall_deep_stack", pcall_deep_stack, 160000 },
  { "coroutine_pingpong", coroutine_pingpong, 160000 },
  { "debug_getlocal_loop", debug_getlocal_loop, 40000 },
  { "gc_table_churn_no_collect", gc_table_churn_no_collect, 700 },
  { "table_next_array_extra", table_next_array_extra, 25000 },
  { "table_next_sparse_extra", table_next_sparse_extra, 22000 },
  { "table_pairs_array_extra", table_pairs_array_extra, 25000 },
  { "table_sort_cmp_upvalue_extra", table_sort_cmp_upvalue_extra, 45 },
  { "table_sort_records_extra", table_sort_records_extra, 35 },
  { "table_sort_large_int_extra", table_sort_large_int_extra, 12 },
  { "string_concat_small_extra", string_concat_small_extra, 180000 },
  { "string_sub_tiny_extra", string_sub_tiny_extra, 1200000 },
  { "string_match_captures_extra", string_match_captures_extra, 120000 },
  { "string_arith_int64_extra", string_arith_int64_extra, 90000 },
  { "bit64_and_shift_extra", bit64_and_shift_extra, 7000000 },
  { "bit64_branch_not_extra", bit64_branch_not_extra, 7000000 },
  { "int64_compare_threshold_extra", int64_compare_threshold_extra, 7000000 },
  { "int64_minmax_chain_extra", int64_minmax_chain_extra, 3000000 },
  { "int64_compare_eq_const_extra", int64_compare_eq_const_extra, 7000000 },
  { "int64_compare_between_extra", int64_compare_between_extra, 5000000 },
  { "int64_compare_descending_extra", int64_compare_descending_extra, 7000000 },
  { "bit64_const_mask_chain_extra", bit64_const_mask_chain_extra, 7000000 },
  { "bit64_const_or_xor_chain_extra", bit64_const_or_xor_chain_extra, 7000000 },
  { "bit64_compare_mask_zero_extra", bit64_compare_mask_zero_extra, 7000000 },
  { "table_next_mixed_extra", table_next_mixed_extra, 8000 },
  { "table_pairs_sparse_extra", table_pairs_sparse_extra, 22000 },
  { "table_pairs_int64_keys_extra", table_pairs_int64_keys_extra, 12000 },
  { "load_dump_many_protos_extra", load_dump_many_protos_extra, 6000 },
  { "debug_getupvalue_extra", debug_getupvalue_extra, 90000 },
  { "debug_setupvalue_extra", debug_setupvalue_extra, 90000 },
  { "closure_call_mixed_extra", closure_call_mixed_extra, 1200000 },
  { "load_dump_extra", load_dump_extra, 15000 },
  { "bit64_rotate_var_extra", bit64_rotate_var_extra, 7000000 },
  { "bit64_extract_insert_extra", bit64_extract_insert_extra, 5000000 },
  { "bit32_extract_branch_extra", bit32_extract_branch_extra, 18000000 },
  { "bit64_math_ult_branch_extra", bit64_math_ult_branch_extra, 5000000 },
  { "int64_compare_float_threshold_extra", int64_compare_float_threshold_extra, 5000000 },
  { "int64_compare_table_bound_extra", int64_compare_table_bound_extra, 4000000 },
  { "numeric_for_int64_large_step_extra", numeric_for_int64_large_step_extra, 1800000 },
  { "table_next_int64_keys_extra", table_next_int64_keys_extra, 12000 },
  { "table_pairs_string_keys_extra", table_pairs_string_keys_extra, 14000 },
  { "table_pairs_mixed_sparse_extra", table_pairs_mixed_sparse_extra, 8000 },
  { "table_sort_tiny_cmp_extra", table_sort_tiny_cmp_extra, 5000 },
  { "table_sort_cmp_desc_extra", table_sort_cmp_desc_extra, 40 },
  { "table_sort_large_strings_extra", table_sort_large_strings_extra, 18 },
  { "table_sort_wide_records_extra", table_sort_wide_records_extra, 22 },
  { "table_sort_equal_keys_extra", table_sort_equal_keys_extra, 35 },
  { "table_move_int64_extra", table_move_int64_extra, 25000 },
  { "string_find_frontier_extra", string_find_frontier_extra, 90000 },
  { "string_gsub_many_repl_extra", string_gsub_many_repl_extra, 3500 },
  { "string_format_int64_mix_extra", string_format_int64_mix_extra, 70000 },
  { "gc_closure_alloc_collect_extra", gc_closure_alloc_collect_extra, 320 },
}

print("mode\tbench\tseconds\tchecksum\tstable")
for _, b in ipairs(benches) do
  if not bench_filter or b[1]:find(bench_filter) then
    local seconds, got, stable = best_of(b[2], b[3])
    print(string.format("%s\t%s\t%.6f\t%s\t%s", mode, b[1], seconds,
			checksum(got), stable and "stable" or "unstable"))
  end
end
LUA

LUA_PATH="$LUAJIT_LUA_PATH" "$LUAJIT_BIN" -e 'assert(jit and jit.arch == "arm64", "requires ARM64 LuaJIT")'
"$LUA54_BIN" -v >/dev/null

{
  LUA_PATH="$LUAJIT_LUA_PATH" BENCH_MODE=luajit_jit_on "$LUAJIT_BIN" "$bench_lua"
  LUA_PATH="$LUAJIT_LUA_PATH" BENCH_MODE=luajit_jit_off "$LUAJIT_BIN" "$bench_lua" | sed '1d'
  BENCH_MODE=lua5.4.8 "$LUA54_BIN" "$bench_lua" | sed '1d'
} | tee "$results_tsv"

python3 - "$results_tsv" "$MARKDOWN_OUT" "$ENFORCE" <<'PY'
import math
import os
import pathlib
import sys

tsv_path = pathlib.Path(sys.argv[1])
markdown_out = sys.argv[2]
enforce = sys.argv[3] == "1"
bench_reps = os.environ.get("BENCH_REPS", "5")

rows = []
for line in tsv_path.read_text().splitlines():
    if not line or line.startswith("mode\t"):
        continue
    parts = line.split("\t")
    if len(parts) == 4:
        mode, bench, seconds, checksum = parts
        stable = "stable"
    else:
        mode, bench, seconds, checksum, stable = parts
    rows.append((mode, bench, float(seconds), checksum, stable))

data = {}
order = []
stable_by_mode = {}
for mode, bench, seconds, checksum, stable in rows:
    if bench not in order:
        order.append(bench)
    data.setdefault(mode, {})[bench] = (seconds, checksum)
    stable_by_mode.setdefault(mode, {})[bench] = stable == "stable"

required = ["luajit_jit_on", "luajit_jit_off", "lua5.4.8"]
missing = [mode for mode in required if mode not in data]
if missing:
    raise SystemExit("missing benchmark modes: " + ", ".join(missing))

mismatches = []
for bench in order:
    checksums = {mode: data[mode][bench][1] for mode in required}
    stables = {mode: stable_by_mode[mode][bench] for mode in required}
    if len(set(checksums.values())) != 1 or not all(stables.values()):
        mismatches.append((bench, checksums, stables))

mismatch_set = {bench for bench, _, _ in mismatches}
comparable_order = [bench for bench in order if bench not in mismatch_set]

def total(mode):
    return sum(data[mode][bench][0] for bench in comparable_order)

def geomean_speedup(mode):
    ratios = [
        data["lua5.4.8"][bench][0] / data[mode][bench][0]
        for bench in comparable_order
    ]
    if not ratios:
        return float("nan")
    return math.exp(sum(math.log(r) for r in ratios) / len(ratios))

def fmt_speedup(value):
    if value < 0.01:
        return f"{value:.4f}x"
    if value < 0.1:
        return f"{value:.3f}x"
    return f"{value:.2f}x"

def category(bench):
    if bench.startswith(("bit", "pure_bit")):
        return "bitwise/integer"
    if bench.startswith(("int", "idiv", "mod", "numeric", "branch", "while", "repeat", "float_idiv")):
        return "integer/control"
    if bench.startswith(("array", "hash", "rawget", "global")):
        return "array/hash table"
    if bench.startswith("table"):
        return "table library"
    if bench.startswith("string") or bench.startswith(("tonumber", "tostring", "utf8")):
        return "string/format/parse"
    if bench.startswith("math"):
        return "math library"
    if bench.startswith(("coroutine", "pcall", "xpcall")):
        return "protected/coroutine"
    if bench.startswith(("closure", "function", "method", "select", "metamethod", "generic", "nested", "tail", "vararg", "pairs")):
        return "call/metamethod"
    if bench.startswith(("debug", "load", "gc")):
        return "runtime/debug/gc"
    return "other"

slower = []
for mode in ("luajit_jit_on", "luajit_jit_off"):
    for bench in comparable_order:
        lua = data["lua5.4.8"][bench][0]
        lj = data[mode][bench][0]
        speedup = lua / lj
        if speedup <= 1.0:
            slower.append((speedup, mode, bench, lj, lua))
slower.sort()

def fmt_workload(item):
    speedup, _, bench, lj, lua = item
    return f"{bench} {fmt_speedup(speedup)} ({lj:.6f}s vs {lua:.6f}s)"

lines = []
lines.append("# ARM64 Wide Performance Sweep")
lines.append("")
lines.append("Broader ARM64 sweep to find workloads where LuaJIT Lua 5.4 compatibility is slower than original Lua 5.4.8.")
lines.append("")
lines.append("## Method")
lines.append("")
lines.append("- `luajit_jit_on`: `jit.on(); jit.flush(); jit.opt.start(\"3\", \"hotloop=8\", \"hotexit=2\")`.")
lines.append("- `luajit_jit_off`: `jit.off(); jit.flush()`.")
lines.append("- `lua5.4.8`: `/Users/gongliang/git/lua-5.4.8/lua`.")
lines.append("- LuaJIT modes reset their JIT state before each workload warmup, so unrelated workloads do not pollute the trace cache.")
lines.append(f"- Each workload runs one warmup pass and {bench_reps} measured passes; the report uses the best measured `os.clock()` time.")
lines.append("- Every measured pass must return a stable checksum; mismatched workloads are listed separately and excluded from speedup totals.")
lines.append("- `--enforce` requires every workload in both LuaJIT modes to be faster than Lua 5.4.8, with stable matching checksums.")
lines.append("")
lines.append("## Summary")
lines.append("")
lines.append(f"- Workloads: {len(order)}")
lines.append(f"- Comparable workloads: {len(comparable_order)}")
lines.append(f"- Checksum/consistency mismatches: {len(mismatches)}")
lines.append("")
lines.append("| Mode | Comparable total time | Total speedup | Geomean speedup | Slower workloads |")
lines.append("| --- | ---: | ---: | ---: | ---: |")
for mode in required:
    seconds = total(mode)
    speedup = total("lua5.4.8") / seconds if seconds else float("nan")
    gm = 1.0 if mode == "lua5.4.8" else geomean_speedup(mode)
    slow_count = sum(1 for x in slower if x[1] == mode)
    lines.append(f"| {mode} | {seconds:.6f}s | {fmt_speedup(speedup)} | {fmt_speedup(gm)} | {slow_count} |")
lines.append("")
lines.append("## Key Findings")
lines.append("")
on_slow = [x for x in slower if x[1] == "luajit_jit_on"]
off_slow = [x for x in slower if x[1] == "luajit_jit_off"]
if on_slow:
    lines.append(
        f"- JIT-on is faster on {len(comparable_order) - len(on_slow)}/{len(comparable_order)} comparable workloads; "
        f"the remaining slower cases are {', '.join(fmt_workload(x) for x in on_slow)}."
    )
else:
    lines.append(f"- JIT-on is faster than Lua 5.4.8 on all {len(comparable_order)} comparable workloads.")
if off_slow:
    lines.append(
        f"- JIT-off is slower on {len(off_slow)}/{len(comparable_order)} comparable workloads; "
        f"the worst confirmed cases are {', '.join(fmt_workload(x) for x in off_slow[:8])}."
    )
if mismatches:
    lines.append(f"- {len(mismatches)} workloads had checksum or per-run consistency mismatches and are listed separately below.")
else:
    lines.append("- Checksums matched for all modes, so the listed slowdowns are performance differences, not result mismatches.")
lines.append("")
lines.append("## Slower Than Lua 5.4.8")
lines.append("")
if slower:
    lines.append("| Mode | Workload | LuaJIT time | Lua 5.4.8 time | Speedup |")
    lines.append("| --- | --- | ---: | ---: | ---: |")
    for speedup, mode, bench, lj, lua in slower:
        lines.append(f"| {mode} | {bench} | {lj:.6f}s | {lua:.6f}s | {fmt_speedup(speedup)} |")
else:
    lines.append("No workload was slower than Lua 5.4.8.")
lines.append("")
lines.append("## Checksum/Correctness Mismatches")
lines.append("")
if mismatches:
    lines.append("| Workload | JIT-on checksum | JIT-on stable | JIT-off checksum | JIT-off stable | Lua 5.4.8 checksum | Lua stable |")
    lines.append("| --- | ---: | --- | ---: | --- | ---: | --- |")
    for bench, checksums, stables in mismatches:
        lines.append(
            f"| {bench} | {checksums['luajit_jit_on']} | {stables['luajit_jit_on']} | "
            f"{checksums['luajit_jit_off']} | {stables['luajit_jit_off']} | "
            f"{checksums['lua5.4.8']} | {stables['lua5.4.8']} |"
        )
else:
    lines.append("No checksum or per-run consistency mismatches.")
lines.append("")
lines.append("## Slowdown Clusters")
lines.append("")
if slower:
    clusters = {}
    for speedup, mode, bench, lj, lua in slower:
        key = (mode, category(bench))
        count, worst_speedup, worst_bench = clusters.get(key, (0, 2.0, ""))
        if speedup < worst_speedup:
            worst_speedup, worst_bench = speedup, bench
        clusters[key] = (count + 1, worst_speedup, worst_bench)
    lines.append("| Mode | Category | Slow workloads | Worst workload | Worst speedup |")
    lines.append("| --- | --- | ---: | --- | ---: |")
    for (mode, cat), (count, worst_speedup, worst_bench) in sorted(
        clusters.items(), key=lambda item: (item[0][0], item[0][1])
    ):
        lines.append(
            f"| {mode} | {cat} | {count} | {worst_bench} | "
            f"{fmt_speedup(worst_speedup)} |"
        )
else:
    lines.append("No slowdown clusters.")
lines.append("")
lines.append("## Full Results")
lines.append("")
lines.append("| Workload | Checksum | JIT-on | On speedup | JIT-off | Off speedup | Lua 5.4.8 |")
lines.append("| --- | ---: | ---: | ---: | ---: | ---: | ---: |")
for bench in order:
    lua = data["lua5.4.8"][bench][0]
    on = data["luajit_jit_on"][bench][0]
    off = data["luajit_jit_off"][bench][0]
    checksum = data["lua5.4.8"][bench][1]
    if bench in mismatch_set:
        on_speed = off_speed = "n/a"
    else:
        on_speed = fmt_speedup(lua/on)
        off_speed = fmt_speedup(lua/off)
    lines.append(
        f"| {bench} | {checksum} | {on:.6f}s | {on_speed} | "
        f"{off:.6f}s | {off_speed} | {lua:.6f}s |"
    )
report = "\n".join(lines) + "\n"

if markdown_out:
    pathlib.Path(markdown_out).write_text(report)

if enforce and (mismatches or slower):
    for bench, checksums, stables in mismatches:
        print(
            f"{bench} checksum/stability mismatch: {checksums} stable={stables}",
            file=sys.stderr,
        )
    for speedup, mode, bench, lj, lua in slower:
        print(
            f"{mode} {bench} speedup {speedup:.2f}x <= 1.00x "
            f"({lj:.6f}s vs {lua:.6f}s)",
            file=sys.stderr,
        )
    raise SystemExit(1)
PY
