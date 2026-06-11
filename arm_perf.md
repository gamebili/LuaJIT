# ARM64 LuaJIT vs Lua 5.4.8 Performance Report

Generated: 2026-06-11 16:26 CST

## Environment

- Machine: Apple M5 Max
- OS/arch: Darwin 25.5.0, `arm64`
- LuaJIT build: `./src/luajit`
  - `_VERSION`: Lua 5.4
  - `jit.version`: LuaJIT 2.1.1781163261
  - `jit.arch`: arm64
- Baseline Lua: `/Users/gongliang/git/lua-5.4.8/lua`
  - Version: Lua 5.4.8

## Method

- Each workload was run with the same Lua source on all three modes:
  - `luajit_jit_on`: LuaJIT with JIT enabled, `jit.opt.start("3", "hotloop=8", "hotexit=2")`.
  - `luajit_jit_off`: LuaJIT with `jit.off(); jit.flush()`.
  - `lua5.4.8`: original Lua 5.4.8 binary.
- Timing source: `os.clock()`.
- Each workload used one warmup pass and then 5 measured passes; the table uses the best measured time.
- `speedup` is `Lua 5.4.8 seconds / LuaJIT seconds`; values above `1.00x` mean LuaJIT is faster.
- Checksums matched across all three modes for every reported workload.

## Summary

| Mode | Total time | Total speedup vs Lua 5.4.8 | Geomean speedup |
| --- | ---: | ---: | ---: |
| LuaJIT JIT-on | 0.785403s | 1.65x | 1.53x |
| LuaJIT JIT-off | 2.534639s | 0.51x | 0.55x |
| Lua 5.4.8 | 1.294738s | 1.00x | 1.00x |

Conclusion: on this ARM64 machine, LuaJIT with JIT enabled is faster than original Lua 5.4.8 overall on this suite. LuaJIT with JIT disabled is improved compared with the earlier baseline, but it is still slower than Lua 5.4.8 overall; the current ARM64 performance gate therefore does not pass for JIT-off mode yet.

## Detailed Results

| Workload | Checksum | LuaJIT JIT-on | JIT-on speedup | LuaJIT JIT-off | JIT-off speedup | Lua 5.4.8 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| numeric_arith | 191897.25 | 0.073648s | 3.85x | 0.748842s | 0.38x | 0.283385s |
| bitwise_idiv | 309439622 | 0.262443s | 0.69x | 0.609401s | 0.30x | 0.181485s |
| array_sum | 0 | 0.120863s | 0.79x | 0.277495s | 0.34x | 0.095705s |
| hash_lookup | 604864 | 0.072520s | 2.55x | 0.170106s | 1.09x | 0.184768s |
| function_calls | 0 | 0.199557s | 2.48x | 0.666940s | 0.74x | 0.494415s |
| table_sort | 890278 | 0.056372s | 0.98x | 0.061855s | 0.89x | 0.054980s |

## Workload Notes

- `numeric_arith`: numeric `for` loop with floating arithmetic and modulo.
- `bitwise_idiv`: Lua 5.4 bitwise operators plus integer floor division.
- `array_sum`: repeated sequential array reads.
- `hash_lookup`: repeated string-key table reads.
- `function_calls`: small Lua function call in a hot loop.
- `table_sort`: repeated `table.sort` on 5,000-element numeric tables.

The reported set only includes workloads that completed successfully in all three modes with matching checksums.
