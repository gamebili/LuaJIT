#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
LUAJIT_BIN=${LUAJIT_BIN:-"$ROOT/src/luajit"}
LUA54_BIN=${LUA54_BIN:-/Users/gongliang/git/lua-5.4.8/lua}
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

tmpdir=${TMPDIR:-/tmp}/luajit-arm64-perf.$$
mkdir -p "$tmpdir"
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

bench_lua=$tmpdir/bench.lua
results_tsv=$tmpdir/results.tsv

cat >"$bench_lua" <<'LUA'
local mode = os.getenv("BENCH_MODE") or "unknown"
if jit then
  if mode == "luajit_jit_on" then
    jit.on()
    jit.flush()
    jit.opt.start("3", "hotloop=8", "hotexit=2")
  elseif mode == "luajit_jit_off" then
    jit.off()
    jit.flush()
  end
end

local clock = os.clock

local function best_of(fn, n, reps)
  fn(math.max(1, math.floor(n / 100)))
  local best, got
  for _ = 1, reps do
    collectgarbage("collect")
    local t0 = clock()
    got = fn(n)
    local dt = clock() - t0
    if not best or dt < best then best = dt end
  end
  return best, got
end

local arr = {}
for i = 1, 100000 do arr[i] = i * 3 end

local hash, keys = {}, {}
for i = 1, 1024 do
  local k = "key_" .. i
  keys[i] = k
  hash[k] = i * 7
end

local function numeric_arith(n)
  local s = 0
  for i = 1, n do
    s = (s + i * 3 + 7) % 1000000
  end
  return tostring(s)
end

local function bitwise_idiv(n)
  local s = 0
  for i = 1, n do
    s = s + i + i + i + i + i + i + i + i + (i // 3)
    s = (s ~ (i << 3)) & 0x7fffffff
  end
  return tostring(s)
end

local function array_sum(reps)
  local s = 0
  for _ = 1, reps do
    local chunk = 0
    for i = 1, 100000 do chunk = chunk + arr[i] end
    s = (s + chunk) % 1000000
  end
  return tostring(s)
end

local function hash_lookup(n)
  local s = 0
  for i = 1, n do
    local k = keys[(i % 1024) + 1]
    s = s + hash[k]
  end
  return tostring(s % 1000000)
end

local function function_calls(n)
  local function f(a, b, c) return (a + b) * c - b end
  local s = 0
  for i = 1, n do s = s + f(i, 3, 5) end
  return tostring(s % 1000000)
end

local function table_sort(rounds)
  local total = 0
  for r = 1, rounds do
    local t = {}
    for i = 1, 5000 do
      t[i] = (i * 1103515245 + r * 12345) % 2147483647
    end
    table.sort(t)
    total = total + t[1] + t[#t]
  end
  return tostring(total % 1000000)
end

local benches = {
  {"numeric_arith", numeric_arith, 60000000},
  {"bitwise_idiv", bitwise_idiv, 25000000},
  {"array_sum", array_sum, 300},
  {"hash_lookup", hash_lookup, 14000000},
  {"function_calls", function_calls, 50000000},
  {"table_sort", table_sort, 100},
}

print("mode\tbench\tseconds\tchecksum")
for _, b in ipairs(benches) do
  local sec, got = best_of(b[2], b[3], 5)
  print(string.format("%s\t%s\t%.6f\t%s", mode, b[1], sec, got))
end
LUA

"$LUAJIT_BIN" -e 'assert(jit and jit.arch == "arm64", "requires ARM64 LuaJIT")'
"$LUA54_BIN" -v >/dev/null

{
  BENCH_MODE=luajit_jit_on "$LUAJIT_BIN" "$bench_lua"
  BENCH_MODE=luajit_jit_off "$LUAJIT_BIN" "$bench_lua" | sed '1d'
  BENCH_MODE=lua5.4.8 "$LUA54_BIN" "$bench_lua" | sed '1d'
} | tee "$results_tsv"

python3 - "$results_tsv" "$MARKDOWN_OUT" "$ENFORCE" <<'PY'
import math
import pathlib
import sys

tsv_path = pathlib.Path(sys.argv[1])
markdown_out = sys.argv[2]
enforce = sys.argv[3] == "1"

rows = []
for line in tsv_path.read_text().splitlines():
    if not line or line.startswith("mode\t"):
        continue
    mode, bench, seconds, checksum = line.split("\t")
    rows.append((mode, bench, float(seconds), checksum))

data = {}
for mode, bench, seconds, checksum in rows:
    data.setdefault(mode, {})[bench] = (seconds, checksum)

required = ["luajit_jit_on", "luajit_jit_off", "lua5.4.8"]
missing = [mode for mode in required if mode not in data]
if missing:
    raise SystemExit("missing benchmark modes: " + ", ".join(missing))

benches = list(data["lua5.4.8"].keys())
for bench in benches:
    checksums = {data[mode][bench][1] for mode in required}
    if len(checksums) != 1:
        raise SystemExit(f"checksum mismatch for {bench}: {checksums}")

def total(mode):
    return sum(data[mode][bench][0] for bench in benches)

def geomean_speedup(mode):
    ratios = [data["lua5.4.8"][bench][0] / data[mode][bench][0]
              for bench in benches]
    return math.exp(sum(math.log(r) for r in ratios) / len(ratios))

summary = []
for mode in required:
    speedup = total("lua5.4.8") / total(mode)
    gm = 1.0 if mode == "lua5.4.8" else geomean_speedup(mode)
    summary.append((mode, total(mode), speedup, gm))

lines = []
lines.append("# ARM64 LuaJIT vs Lua 5.4.8 Performance Report")
lines.append("")
lines.append("## Method")
lines.append("")
lines.append("- `luajit_jit_on`: `jit.on(); jit.flush(); jit.opt.start(\"3\", \"hotloop=8\", \"hotexit=2\")`.")
lines.append("- `luajit_jit_off`: `jit.off(); jit.flush()`.")
lines.append("- `lua5.4.8`: `/Users/gongliang/git/lua-5.4.8/lua`.")
lines.append("- Each workload runs one warmup pass and five measured passes; the report uses the best measured `os.clock()` time.")
lines.append("- `--enforce` requires every workload in both LuaJIT modes to be faster than Lua 5.4.8, with matching checksums.")
lines.append("")
lines.append("## Summary")
lines.append("")
lines.append("| Mode | Total time | Total speedup vs Lua 5.4.8 | Geomean speedup |")
lines.append("| --- | ---: | ---: | ---: |")
for mode, seconds, speedup, gm in summary:
    lines.append(f"| {mode} | {seconds:.6f}s | {speedup:.2f}x | {gm:.2f}x |")
lines.append("")
lines.append("## Details")
lines.append("")
lines.append("| Workload | Checksum | LuaJIT JIT-on | JIT-on speedup | LuaJIT JIT-off | JIT-off speedup | Lua 5.4.8 |")
lines.append("| --- | ---: | ---: | ---: | ---: | ---: | ---: |")
for bench in benches:
    lua = data["lua5.4.8"][bench][0]
    on = data["luajit_jit_on"][bench][0]
    off = data["luajit_jit_off"][bench][0]
    checksum = data["lua5.4.8"][bench][1]
    lines.append(
        f"| {bench} | {checksum} | {on:.6f}s | {lua/on:.2f}x | "
        f"{off:.6f}s | {lua/off:.2f}x | {lua:.6f}s |"
    )
lines.append("")
lines.append("## Workloads")
lines.append("")
lines.append("- `numeric_arith`: integer numeric loop with multiplication, addition, and modulo.")
lines.append("- `bitwise_idiv`: integer loop with `//`, shift, xor, and mask operations.")
lines.append("- `array_sum`: repeated sequential array reads with per-round checked accumulation.")
lines.append("- `hash_lookup`: repeated string-key table reads.")
lines.append("- `function_calls`: small Lua function call in a hot loop.")
lines.append("- `table_sort`: repeated `table.sort` on 5,000-element numeric tables.")

report = "\n".join(lines) + "\n"
if markdown_out:
    pathlib.Path(markdown_out).write_text(report)

if enforce:
    failures = []
    for mode in ("luajit_jit_on", "luajit_jit_off"):
        speedup = total("lua5.4.8") / total(mode)
        if speedup <= 1.0:
            failures.append(f"{mode} total speedup {speedup:.2f}x <= 1.00x")
        for bench in benches:
            bench_speedup = data["lua5.4.8"][bench][0] / data[mode][bench][0]
            if bench_speedup <= 1.0:
                failures.append(
                    f"{mode} {bench} speedup {bench_speedup:.2f}x <= 1.00x"
                )
    if failures:
        print("\n".join(failures), file=sys.stderr)
        raise SystemExit(1)
PY
