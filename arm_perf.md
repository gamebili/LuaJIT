# ARM64 LuaJIT vs Lua 5.4.8 Performance Report

## Method

- `luajit_jit_on`: `jit.on(); jit.flush(); jit.opt.start("3", "hotloop=8", "hotexit=2")`.
- `luajit_jit_off`: `jit.off(); jit.flush()`.
- `lua5.4.8`: `/Users/gongliang/git/lua-5.4.8/lua`.
- Each workload runs one warmup pass and five measured passes; the report uses the best measured `os.clock()` time.
- `--enforce` requires every workload in both LuaJIT modes to be faster than Lua 5.4.8, with matching checksums.

## Summary

| Mode | Total time | Total speedup vs Lua 5.4.8 | Geomean speedup |
| --- | ---: | ---: | ---: |
| luajit_jit_on | 0.939604s | 2.46x | 2.71x |
| luajit_jit_off | 1.662764s | 1.39x | 1.82x |
| lua5.4.8 | 2.315837s | 1.00x | 1.00x |

## Details

| Workload | Checksum | LuaJIT JIT-on | JIT-on speedup | LuaJIT JIT-off | JIT-off speedup | Lua 5.4.8 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| numeric_arith | 0 | 0.345097s | 2.22x | 0.419621s | 1.83x | 0.766264s |
| bitwise_idiv | 429764176 | 0.226793s | 3.05x | 0.641578s | 1.08x | 0.690876s |
| array_sum | 0 | 0.084246s | 1.16x | 0.092950s | 1.05x | 0.097669s |
| hash_lookup | 604864 | 0.072767s | 2.54x | 0.093611s | 1.97x | 0.184486s |
| function_calls | 0 | 0.203712s | 2.56x | 0.406995s | 1.28x | 0.521563s |
| table_sort | 890278 | 0.006989s | 7.87x | 0.008009s | 6.86x | 0.054979s |

## Workloads

- `numeric_arith`: integer numeric loop with multiplication, addition, and modulo.
- `bitwise_idiv`: integer loop with `//`, shift, xor, and mask operations.
- `array_sum`: repeated sequential array reads with per-round checked accumulation.
- `hash_lookup`: repeated string-key table reads.
- `function_calls`: small Lua function call in a hot loop.
- `table_sort`: repeated `table.sort` on 5,000-element numeric tables.
