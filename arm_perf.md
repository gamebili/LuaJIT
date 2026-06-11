# ARM64 LuaJIT vs Lua 5.4.8 Performance Report

Generated: 2026-06-11 15:24 CST

## Environment

- Machine: Apple M5 Max
- OS/arch: Darwin 25.5.0, `arm64`
- LuaJIT build: `./src/luajit`
  - `_VERSION`: Lua 5.4
  - `jit.version`: LuaJIT 2.1.1781100735
  - `jit.arch`: arm64
- Baseline Lua: `/Users/gongliang/git/lua-5.4.8/lua`
  - Version: Lua 5.4.8

The current Lua 5.4 compatibility performance gate also passed via
`run-perf-lua54compat-tests`; this report is an additional same-machine
comparison against the original Lua 5.4.8 binary.

## Method

- Each workload was run with the same Lua source on all three modes:
  - `luajit_jit_on`: LuaJIT with JIT enabled, `jit.opt.start("3", "hotloop=8", "hotexit=2")`.
  - `luajit_jit_off`: LuaJIT with `jit.off(); jit.flush()`.
  - `lua5.4.8`: original Lua 5.4.8 binary.
- Timing source: `os.clock()`.
- Each workload used one warmup pass and then 5 measured passes; the table uses
  the best measured time.
- `speedup` is `Lua 5.4.8 seconds / LuaJIT seconds`; values above `1.00x` mean
  LuaJIT is faster.
- Checksums matched across all three modes for every reported workload.

## Summary

| Mode | Total time | Total speedup vs Lua 5.4.8 | Geomean speedup |
| --- | ---: | ---: | ---: |
| LuaJIT JIT-on | 0.784017s | 1.72x | 1.59x |
| LuaJIT JIT-off | 3.545345s | 0.38x | 0.41x |
| Lua 5.4.8 | 1.346673s | 1.00x | 1.00x |

Conclusion: on this ARM64 machine, LuaJIT with JIT enabled is faster than
original Lua 5.4.8 overall on the stable hot-loop suite. LuaJIT with JIT disabled
is substantially slower than Lua 5.4.8 on this suite; the non-JIT mode is useful
as a compatibility and fallback mode, not as the performance target.

## Detailed Results

| Workload | Checksum | LuaJIT JIT-on | JIT-on speedup | LuaJIT JIT-off | JIT-off speedup | Lua 5.4.8 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| numeric_arith | 191897.25 | 0.073020s | 4.06x | 0.700352s | 0.42x | 0.296823s |
| bitwise_idiv | 309439622 | 0.265253s | 0.73x | 0.996411s | 0.20x | 0.194919s |
| array_sum | 0 | 0.120089s | 0.83x | 0.472141s | 0.21x | 0.099330s |
| hash_lookup | 604864 | 0.071071s | 2.64x | 0.270223s | 0.70x | 0.187902s |
| function_calls | 0 | 0.196911s | 2.61x | 1.034041s | 0.50x | 0.513037s |
| table_sort | 890278 | 0.057673s | 0.95x | 0.072177s | 0.76x | 0.054662s |

## Workload Notes

- `numeric_arith`: numeric `for` loop with floating arithmetic and modulo.
- `bitwise_idiv`: Lua 5.4 bitwise operators plus integer floor division.
- `array_sum`: repeated sequential array reads.
- `hash_lookup`: repeated string-key table reads.
- `function_calls`: small Lua function call in a hot loop.
- `table_sort`: repeated `table.sort` on 5,000-element numeric tables.

The reported set only includes workloads that completed successfully in all
three modes with matching checksums.
