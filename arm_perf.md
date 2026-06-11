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
| luajit_jit_on | 0.968540s | 2.40x | 2.70x |
| luajit_jit_off | 1.331905s | 1.74x | 2.08x |
| lua5.4.8 | 2.323660s | 1.00x | 1.00x |

## Details

| Workload | Checksum | LuaJIT JIT-on | JIT-on speedup | LuaJIT JIT-off | JIT-off speedup | Lua 5.4.8 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| numeric_arith | 0 | 0.362177s | 2.14x | 0.343220s | 2.26x | 0.774541s |
| bitwise_idiv | 429764176 | 0.238125s | 2.90x | 0.426103s | 1.62x | 0.690044s |
| array_sum | 0 | 0.084678s | 1.24x | 0.087524s | 1.20x | 0.105348s |
| hash_lookup | 604864 | 0.075123s | 2.44x | 0.092569s | 1.98x | 0.183457s |
| function_calls | 0 | 0.201354s | 2.55x | 0.374110s | 1.37x | 0.513376s |
| table_sort | 890278 | 0.007083s | 8.03x | 0.008379s | 6.79x | 0.056894s |

## Workloads

- `numeric_arith`: integer numeric loop with multiplication, addition, and modulo.
- `bitwise_idiv`: integer loop with `//`, shift, xor, and mask operations.
- `array_sum`: repeated sequential array reads with per-round checked accumulation.
- `hash_lookup`: repeated string-key table reads.
- `function_calls`: small Lua function call in a hot loop.
- `table_sort`: repeated `table.sort` on 5,000-element numeric tables.
