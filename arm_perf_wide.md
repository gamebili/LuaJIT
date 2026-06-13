# ARM64 Wide Performance Sweep

Broader ARM64 sweep to find workloads where LuaJIT Lua 5.4 compatibility is slower than original Lua 5.4.8.

## Method

- `luajit_jit_on`: `jit.on(); jit.flush(); jit.opt.start("3", "hotloop=8", "hotexit=2")`.
- `luajit_jit_off`: `jit.off(); jit.flush()`.
- `lua5.4.8`: `/Users/gongliang/git/lua-5.4.8/lua`.
- LuaJIT modes reset their JIT state before each workload warmup, so unrelated workloads do not pollute the trace cache.
- JIT-on runs 1 full-size warmup pass(es) after the small warmup; these warmup passes are excluded from timed measurements.
- Each workload runs one small warmup pass and 5 measured passes; the report uses the best measured `os.clock()` time.
- Every measured pass must return a stable checksum; mismatched workloads are listed separately and excluded from speedup totals.
- Workloads dominated by similar C library algorithms are classified as `parity`: they are measured and listed, but excluded from scored speedup totals.
- `--enforce` requires `strict` workloads to beat Lua 5.4.8 and `parity` workloads to stay at or above 0.80x of Lua 5.4.8, with stable matching checksums.

## Summary

- Workloads: 300
- Comparable workloads: 300
- Scored strict workloads: 180
- C-similar parity workloads: 120
- Checksum/consistency mismatches: 0

| Mode | Strict total time | Strict total speedup | Strict geomean speedup | Strict slower | Parity below min |
| --- | ---: | ---: | ---: | ---: | ---: |
| luajit_jit_on | 6.075190s | 2.46x | 3.99x | 3 | 2 |
| luajit_jit_off | 16.081922s | 0.93x | 8.06x | 83 | 13 |
| lua5.4.8 | 14.946734s | 1.00x | 1.00x | 0 | 0 |

## Key Findings

- JIT-on is faster on 177/180 strict workloads; the strict slower cases are string_concat 0.87x (0.009080s vs 0.007919s), table_next_sparse_extra 0.98x (0.397920s vs 0.390526s), coroutine_create_resume 0.99x (0.009951s vs 0.009812s).
- JIT-off is slower on 83/180 strict workloads; the worst confirmed cases are bit64_const_mask_chain_extra 0.18x (0.153706s vs 0.028371s), bit64_const_or_xor_chain_extra 0.19x (0.110953s vs 0.020756s), bit_shift_signed64 0.19x (0.178160s vs 0.033741s), bit_shift64_const 0.19x (0.157638s vs 0.030271s), bit_or64 0.19x (0.190533s vs 0.036674s), bit_shift64_right 0.19x (0.223843s vs 0.043366s), bit_and64 0.20x (0.220638s vs 0.044022s), bit_not64 0.21x (0.172113s vs 0.035533s).
- C-similar parity workloads are allowed to be near parity, but below 0.80x is treated as too slow: JIT-on 2 case(s): gc_closure_alloc_collect_extra 0.35x (0.091184s vs 0.031485s), gc_table_alloc_collect 0.36x (0.121342s vs 0.043188s); JIT-off 13 case(s): gc_closure_alloc_collect_extra 0.35x (0.090676s vs 0.031485s), gc_table_alloc_collect 0.35x (0.122400s vs 0.043188s), math_abs_int64 0.37x (0.119089s vs 0.043958s), debug_getlocal_loop 0.63x (0.001666s vs 0.001048s), string_char_many 0.69x (0.039127s vs 0.027173s), tonumber_int64_parse 0.71x (0.013846s vs 0.009887s), tonumber_hex64_parse 0.72x (0.009951s vs 0.007210s), string_char_loop 0.73x (0.019965s vs 0.014604s).
- Checksums matched for all modes, so the listed slowdowns are performance differences, not result mismatches.

## Strict Slower Than Lua 5.4.8

| Mode | Workload | LuaJIT time | Lua 5.4.8 time | Speedup |
| --- | --- | ---: | ---: | ---: |
| luajit_jit_off | bit64_const_mask_chain_extra | 0.153706s | 0.028371s | 0.18x |
| luajit_jit_off | bit64_const_or_xor_chain_extra | 0.110953s | 0.020756s | 0.19x |
| luajit_jit_off | bit_shift_signed64 | 0.178160s | 0.033741s | 0.19x |
| luajit_jit_off | bit_shift64_const | 0.157638s | 0.030271s | 0.19x |
| luajit_jit_off | bit_or64 | 0.190533s | 0.036674s | 0.19x |
| luajit_jit_off | bit_shift64_right | 0.223843s | 0.043366s | 0.19x |
| luajit_jit_off | bit_and64 | 0.220638s | 0.044022s | 0.20x |
| luajit_jit_off | bit_not64 | 0.172113s | 0.035533s | 0.21x |
| luajit_jit_off | bit64_and_shift_extra | 0.199493s | 0.041312s | 0.21x |
| luajit_jit_off | bit_chain64_mixed | 0.236748s | 0.049889s | 0.21x |
| luajit_jit_off | bitwise_idiv_unsigned | 0.451950s | 0.103981s | 0.23x |
| luajit_jit_off | bit_mixed_const32 | 0.395905s | 0.115299s | 0.29x |
| luajit_jit_off | bitwise_idiv_mix | 0.458296s | 0.140813s | 0.31x |
| luajit_jit_off | bit_rotate64 | 0.289233s | 0.092569s | 0.32x |
| luajit_jit_off | bit64_branch_test | 0.117908s | 0.039970s | 0.34x |
| luajit_jit_off | bit_shift64_large_count | 0.132056s | 0.048217s | 0.37x |
| luajit_jit_off | int64_compare_zero_branch | 0.149831s | 0.055126s | 0.37x |
| luajit_jit_off | bit_shift_mask | 0.528613s | 0.195691s | 0.37x |
| luajit_jit_off | bit64_rotate_var_extra | 0.101680s | 0.040008s | 0.39x |
| luajit_jit_off | int64_compare_minmax | 0.168151s | 0.067867s | 0.40x |
| luajit_jit_off | bit64_compare_mask_zero_extra | 0.053589s | 0.021763s | 0.41x |
| luajit_jit_off | bit_shift32_const | 0.449609s | 0.185080s | 0.41x |
| luajit_jit_off | bit_and32 | 0.204846s | 0.090549s | 0.44x |
| luajit_jit_off | bit_shift64_var | 0.132058s | 0.060769s | 0.46x |
| luajit_jit_off | table_float_key_lookup | 0.097837s | 0.049801s | 0.51x |
| luajit_jit_off | array_read_stride | 0.075538s | 0.038571s | 0.51x |
| luajit_jit_off | table_int64_key_miss | 0.038731s | 0.019925s | 0.51x |
| luajit_jit_off | bit_and64_power2_mask | 0.064060s | 0.033020s | 0.52x |
| luajit_jit_off | bit_shift32_var | 0.313584s | 0.163881s | 0.52x |
| luajit_jit_off | metamethod_idiv | 0.006731s | 0.003533s | 0.52x |
| luajit_jit_off | bit64_math_ult_branch_extra | 0.157901s | 0.090253s | 0.57x |
| luajit_jit_off | bit_xor32 | 0.156188s | 0.089513s | 0.57x |
| luajit_jit_off | table_rawget_int64_key | 0.057900s | 0.033572s | 0.58x |
| luajit_jit_off | idiv_negative | 0.336986s | 0.195632s | 0.58x |
| luajit_jit_off | table_array_append_direct | 0.011868s | 0.007000s | 0.59x |
| luajit_jit_off | table_int64_key_delete | 0.013126s | 0.008143s | 0.62x |
| luajit_jit_off | int64_compare_table_bound_extra | 0.033088s | 0.021152s | 0.64x |
| luajit_jit_off | math_minmax_mixed_int64 | 0.074283s | 0.048372s | 0.65x |
| luajit_jit_off | int64_minmax_chain_extra | 0.119448s | 0.077933s | 0.65x |
| luajit_jit_off | math_minmax_int64 | 0.100512s | 0.066098s | 0.66x |
| luajit_jit_off | math_tointeger_int64_string | 0.011680s | 0.007900s | 0.68x |
| luajit_jit_off | float_arith | 0.289328s | 0.196052s | 0.68x |
| luajit_jit_off | bit_or32 | 0.132670s | 0.089978s | 0.68x |
| luajit_jit_off | numeric_for_int64_large_step_extra | 0.025036s | 0.017012s | 0.68x |
| luajit_jit_off | array_sum_chunk | 0.168325s | 0.115186s | 0.68x |
| luajit_jit_off | int64_add_sub | 0.185267s | 0.129052s | 0.70x |
| luajit_jit_off | idiv_mod | 0.479733s | 0.336932s | 0.70x |
| luajit_jit_off | int64_mul_wrap | 0.046402s | 0.033605s | 0.72x |
| luajit_jit_off | int64_mul_const_wrap | 0.055484s | 0.040493s | 0.73x |
| luajit_jit_off | idiv_power2 | 0.147477s | 0.107754s | 0.73x |
| luajit_jit_off | metamethod_bitwise | 0.005448s | 0.004102s | 0.75x |
| luajit_jit_off | table_int64_key_insert | 0.009271s | 0.007037s | 0.76x |
| luajit_jit_off | coroutine_yield_multi | 0.015213s | 0.011627s | 0.76x |
| luajit_jit_off | function_calls | 0.322914s | 0.248058s | 0.77x |
| luajit_jit_off | coroutine_resume | 0.019957s | 0.015426s | 0.77x |
| luajit_jit_off | coroutine_resume_args | 0.014706s | 0.011390s | 0.77x |
| luajit_jit_off | coroutine_pingpong | 0.013564s | 0.010687s | 0.79x |
| luajit_jit_off | generic_for_iter_closure | 0.093045s | 0.073643s | 0.79x |
| luajit_jit_off | table_mixed_key_lookup | 0.040879s | 0.032652s | 0.80x |
| luajit_jit_off | int_mul32 | 0.294240s | 0.235228s | 0.80x |
| luajit_jit_off | metamethod_index_table | 0.026072s | 0.021694s | 0.83x |
| luajit_jit_off | string_concat | 0.009488s | 0.007919s | 0.83x |
| luajit_jit_off | method_call | 0.034410s | 0.028740s | 0.84x |
| luajit_jit_off | hash_update | 0.030323s | 0.025421s | 0.84x |
| luajit_jit_off | vararg_select_tail | 0.031410s | 0.027138s | 0.86x |
| luajit_jit_on | string_concat | 0.009080s | 0.007919s | 0.87x |
| luajit_jit_off | select_vararg | 0.073817s | 0.064577s | 0.87x |
| luajit_jit_off | table_array_pop_direct | 0.018405s | 0.016376s | 0.89x |
| luajit_jit_off | metamethod_index | 0.015887s | 0.014417s | 0.91x |
| luajit_jit_off | global_read_loop | 0.065584s | 0.059552s | 0.91x |
| luajit_jit_off | table_pairs_int64_keys_extra | 0.204156s | 0.185468s | 0.91x |
| luajit_jit_off | table_rawset_int64_key | 0.021657s | 0.019744s | 0.91x |
| luajit_jit_off | vararg_sum | 0.105433s | 0.097457s | 0.92x |
| luajit_jit_off | metamethod_add | 0.009120s | 0.008722s | 0.96x |
| luajit_jit_off | hash_lookup | 0.132583s | 0.127747s | 0.96x |
| luajit_jit_off | table_next_int64_keys_extra | 0.258003s | 0.251334s | 0.97x |
| luajit_jit_off | math_type_mixed | 0.035891s | 0.034997s | 0.98x |
| luajit_jit_off | table_field_read_loop | 0.179713s | 0.175879s | 0.98x |
| luajit_jit_on | table_next_sparse_extra | 0.397920s | 0.390526s | 0.98x |
| luajit_jit_off | metamethod_len | 0.013276s | 0.013063s | 0.98x |
| luajit_jit_on | coroutine_create_resume | 0.009951s | 0.009812s | 0.99x |
| luajit_jit_off | metamethod_newindex | 0.011446s | 0.011296s | 0.99x |
| luajit_jit_off | coroutine_create_resume | 0.009935s | 0.009812s | 0.99x |
| luajit_jit_off | string_arith_int64_extra | 0.002060s | 0.002042s | 0.99x |
| luajit_jit_off | nested_closure_call | 0.020142s | 0.020022s | 0.99x |
| luajit_jit_off | float_compare_branch | 0.169259s | 0.168864s | 1.00x |

## Parity Workloads Below Threshold

| Mode | Workload | LuaJIT time | Lua 5.4.8 time | Speedup | Minimum |
| --- | --- | ---: | ---: | ---: | ---: |
| luajit_jit_on | gc_closure_alloc_collect_extra | 0.091184s | 0.031485s | 0.35x | 0.80x |
| luajit_jit_off | gc_closure_alloc_collect_extra | 0.090676s | 0.031485s | 0.35x | 0.80x |
| luajit_jit_off | gc_table_alloc_collect | 0.122400s | 0.043188s | 0.35x | 0.80x |
| luajit_jit_on | gc_table_alloc_collect | 0.121342s | 0.043188s | 0.36x | 0.80x |
| luajit_jit_off | math_abs_int64 | 0.119089s | 0.043958s | 0.37x | 0.80x |
| luajit_jit_off | debug_getlocal_loop | 0.001666s | 0.001048s | 0.63x | 0.80x |
| luajit_jit_off | string_char_many | 0.039127s | 0.027173s | 0.69x | 0.80x |
| luajit_jit_off | tonumber_int64_parse | 0.013846s | 0.009887s | 0.71x | 0.80x |
| luajit_jit_off | tonumber_hex64_parse | 0.009951s | 0.007210s | 0.72x | 0.80x |
| luajit_jit_off | string_char_loop | 0.019965s | 0.014604s | 0.73x | 0.80x |
| luajit_jit_off | table_sort_tiny_cmp_extra | 0.007788s | 0.005747s | 0.74x | 0.80x |
| luajit_jit_off | string_compare_loop | 0.067962s | 0.050173s | 0.74x | 0.80x |
| luajit_jit_off | math_fmod_int64 | 0.062970s | 0.046738s | 0.74x | 0.80x |
| luajit_jit_off | utf8_offset_loop | 0.006325s | 0.004979s | 0.79x | 0.80x |
| luajit_jit_off | math_modf_loop | 0.078283s | 0.061635s | 0.79x | 0.80x |

## Parity Workloads Slower But Within Threshold

| Mode | Workload | LuaJIT time | Lua 5.4.8 time | Speedup |
| --- | --- | ---: | ---: | ---: |
| luajit_jit_off | string_sub_large_slice | 0.019280s | 0.016134s | 0.84x |
| luajit_jit_off | string_pack_unpack_mixed | 0.024622s | 0.020744s | 0.84x |
| luajit_jit_off | debug_setupvalue_extra | 0.002323s | 0.001986s | 0.85x |
| luajit_jit_on | debug_getlocal_loop | 0.001188s | 0.001048s | 0.88x |
| luajit_jit_off | debug_getupvalue_extra | 0.001964s | 0.001783s | 0.91x |
| luajit_jit_off | string_sub_negative | 0.019139s | 0.017431s | 0.91x |
| luajit_jit_off | math_floor_negative | 0.038773s | 0.035514s | 0.92x |
| luajit_jit_off | string_find_absent_plain | 0.006677s | 0.006517s | 0.98x |
| luajit_jit_off | table_sort_wide_records_extra | 0.011980s | 0.011764s | 0.98x |

## Checksum/Correctness Mismatches

No checksum or per-run consistency mismatches.

## Slowdown Clusters

| Mode | Policy | Category | Slow workloads | Worst workload | Worst speedup |
| --- | --- | --- | ---: | --- | ---: |
| luajit_jit_off | parity | math library | 3 | math_abs_int64 | 0.37x |
| luajit_jit_off | parity | runtime/debug/gc | 3 | gc_closure_alloc_collect_extra | 0.35x |
| luajit_jit_off | parity | string/format/parse | 6 | string_char_many | 0.69x |
| luajit_jit_off | parity | table library | 1 | table_sort_tiny_cmp_extra | 0.74x |
| luajit_jit_off | strict | array/hash table | 5 | array_read_stride | 0.51x |
| luajit_jit_off | strict | bitwise/integer | 27 | bit64_const_mask_chain_extra | 0.18x |
| luajit_jit_off | strict | call/metamethod | 14 | metamethod_idiv | 0.52x |
| luajit_jit_off | strict | integer/control | 12 | int64_compare_zero_branch | 0.37x |
| luajit_jit_off | strict | math library | 4 | math_minmax_mixed_int64 | 0.65x |
| luajit_jit_off | strict | other | 2 | float_arith | 0.68x |
| luajit_jit_off | strict | protected/coroutine | 5 | coroutine_yield_multi | 0.76x |
| luajit_jit_off | strict | string/format/parse | 2 | string_concat | 0.83x |
| luajit_jit_off | strict | table library | 12 | table_float_key_lookup | 0.51x |
| luajit_jit_on | parity | runtime/debug/gc | 2 | gc_closure_alloc_collect_extra | 0.35x |
| luajit_jit_on | strict | protected/coroutine | 1 | coroutine_create_resume | 0.99x |
| luajit_jit_on | strict | string/format/parse | 1 | string_concat | 0.87x |
| luajit_jit_on | strict | table library | 1 | table_next_sparse_extra | 0.98x |

## Full Results

| Workload | Policy | Checksum | JIT-on | On speedup | JIT-off | Off speedup | Lua 5.4.8 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| empty_loop | strict | 100000000 | 0.022167s | 13.12x | 0.220494s | 1.32x | 0.290739s |
| int_add_wide | strict | 191146379 | 0.305699s | 1.57x | 0.323515s | 1.49x | 0.481347s |
| int64_add_sub | strict | 127778 | 0.073164s | 1.76x | 0.185267s | 0.70x | 0.129052s |
| int_add32 | strict | 576647110 | 0.288294s | 2.84x | 0.622154s | 1.32x | 0.819359s |
| int_mul32 | strict | 311395809 | 0.054685s | 4.30x | 0.294240s | 0.80x | 0.235228s |
| int_mod_small | strict | 999740 | 0.155753s | 2.38x | 0.000000s | 371104000.00x | 0.371104s |
| idiv_const | strict | 0 | 0.020480s | 5.24x | 0.000000s | 107404000.00x | 0.107404s |
| idiv_negative | strict | 714300 | 0.066545s | 2.94x | 0.336986s | 0.58x | 0.195632s |
| idiv_power2 | strict | 65539 | 0.031271s | 3.45x | 0.147477s | 0.73x | 0.107754s |
| mod_const | strict | 500000 | 0.026711s | 5.12x | 0.000000s | 136849000.00x | 0.136849s |
| mod_power2 | strict | 387680 | 0.041199s | 3.33x | 0.000000s | 137071000.00x | 0.137071s |
| numeric_for_int64_span | strict | 999943 | 0.013042s | 1.76x | 0.000000s | 22955000.00x | 0.022955s |
| int_compare_branch | strict | 861515 | 0.043171s | 2.63x | 0.018281s | 6.20x | 0.113409s |
| int_compare_eq_chain | strict | 817169 | 0.036904s | 4.69x | 0.017274s | 10.02x | 0.173042s |
| int64_compare_minmax | strict | 0 | 0.031939s | 2.12x | 0.168151s | 0.40x | 0.067867s |
| int64_eq_branch | strict | 373544 | 0.010272s | 8.10x | 0.005972s | 13.93x | 0.083188s |
| int64_mod_const | strict | 992044 | 0.044484s | 1.67x | 0.000000s | 74423000.00x | 0.074423s |
| int64_idiv_const | strict | 632320 | 0.045323s | 1.04x | 0.000000s | 47125000.00x | 0.047125s |
| int64_mul_wrap | strict | 1744528928 | 0.005502s | 6.11x | 0.046402s | 0.72x | 0.033605s |
| float_compare_branch | strict | -963739252 | 0.123386s | 1.37x | 0.169259s | 1.00x | 0.168864s |
| float_arith | strict | 206120287761215.281250 | 0.148339s | 1.32x | 0.289328s | 0.68x | 0.196052s |
| pure_bit_xor | strict | 50000000 | 0.066561s | 1.35x | 0.000000s | 89978000.00x | 0.089978s |
| bit_and32 | strict | 33950784 | 0.046527s | 1.95x | 0.204846s | 0.44x | 0.090549s |
| bit_or32 | strict | 2147483647 | 0.031475s | 2.86x | 0.132670s | 0.68x | 0.089978s |
| bit_xor32 | strict | 102466744 | 0.031526s | 2.84x | 0.156188s | 0.57x | 0.089513s |
| bit_not32 | strict | 35000000 | 0.069799s | 1.12x | 0.000000s | 78372000.00x | 0.078372s |
| bit_shift32_const | strict | 300872905 | 0.054292s | 3.41x | 0.449609s | 0.41x | 0.185080s |
| bit_shift32_var | strict | 1592255438 | 0.045907s | 3.57x | 0.313584s | 0.52x | 0.163881s |
| bit_shift_mask | strict | 318080536 | 0.047196s | 4.15x | 0.528613s | 0.37x | 0.195691s |
| bitwise_idiv_unsigned | strict | 1821843616 | 0.036781s | 2.83x | 0.451950s | 0.23x | 0.103981s |
| bitwise_idiv_mix | strict | 714511537 | 0.057036s | 2.47x | 0.458296s | 0.31x | 0.140813s |
| idiv_mod | strict | 997901798 | 0.100958s | 3.34x | 0.479733s | 0.70x | 0.336932s |
| bit_shift64_const | strict | 295495246 | 0.018948s | 1.60x | 0.157638s | 0.19x | 0.030271s |
| bit_and64 | strict | 1890031 | 0.024031s | 1.83x | 0.220638s | 0.20x | 0.044022s |
| bit_and64_const | strict | 1456439740 | 0.014738s | 1.46x | 0.005229s | 4.11x | 0.021482s |
| bit_or64 | strict | 305419896 | 0.005285s | 6.94x | 0.190533s | 0.19x | 0.036674s |
| bit_or64_const | strict | 1459617791 | 0.008034s | 3.46x | 0.000000s | 27802000.00x | 0.027802s |
| bit_xor64 | strict | 1617852892 | 0.005785s | 8.43x | 0.005321s | 9.16x | 0.048753s |
| bit_xor64_const | strict | 1456439740 | 0.008050s | 3.48x | 0.000000s | 27990000.00x | 0.027990s |
| bit_shift64_var | strict | 23590 | 0.012618s | 4.82x | 0.132058s | 0.46x | 0.060769s |
| bit_shift64_right | strict | 1846415822 | 0.008061s | 5.38x | 0.223843s | 0.19x | 0.043366s |
| bit_not64 | strict | 1633917628 | 0.008127s | 4.37x | 0.172113s | 0.21x | 0.035533s |
| bit64_branch_test | strict | 999746 | 0.009501s | 4.21x | 0.117908s | 0.34x | 0.039970s |
| array_sum_wide | strict | 967060 | 0.005200s | 10.75x | 0.055094s | 1.01x | 0.055919s |
| array_sum_chunk | strict | 562228 | 0.013872s | 8.30x | 0.168325s | 0.68x | 0.115186s |
| array_read_seq | strict | 967060 | 0.005192s | 10.86x | 0.054993s | 1.03x | 0.056408s |
| array_read_stride | strict | 989020 | 0.006785s | 5.68x | 0.075538s | 0.51x | 0.038571s |
| array_write_seq | strict | 500500 | 0.001509s | 6.34x | 0.007093s | 1.35x | 0.009574s |
| table_new_small | strict | 50000 | 0.009857s | 4.78x | 0.025417s | 1.85x | 0.047118s |
| hash_lookup | strict | 144320 | 0.011183s | 11.42x | 0.132583s | 0.96x | 0.127747s |
| hash_update | strict | 113833 | 0.005804s | 4.38x | 0.030323s | 0.84x | 0.025421s |
| table_int64_key_lookup | strict | 164192 | 0.006076s | 3.17x | 0.003382s | 5.70x | 0.019262s |
| table_int64_key_insert | strict | 461700 | 0.005140s | 1.37x | 0.009271s | 0.76x | 0.007037s |
| table_int64_key_update | strict | 64624 | 0.003777s | 3.63x | 0.000022s | 622.95x | 0.013705s |
| table_int64_key_miss | strict | 2500000 | 0.003391s | 5.88x | 0.038731s | 0.51x | 0.019925s |
| table_int64_key_delete | strict | 363200 | 0.005020s | 1.62x | 0.013126s | 0.62x | 0.008143s |
| table_float_key_lookup | strict | 599920 | 0.005826s | 8.55x | 0.097837s | 0.51x | 0.049801s |
| table_mixed_key_lookup | strict | 551488 | 0.005902s | 5.53x | 0.040879s | 0.80x | 0.032652s |
| table_next_dense | strict | 360000 | 0.046479s | 2.07x | 0.046774s | 2.05x | 0.096039s |
| table_next_hash | strict | 248000 | 0.051354s | 2.63x | 0.075971s | 1.78x | 0.135156s |
| rawget_rawset | strict | 900000 | 0.020523s | 2.30x | 0.037909s | 1.25x | 0.047293s |
| ipairs_iter | strict | 40000 | 0.071655s | 1.93x | 0.068848s | 2.01x | 0.138096s |
| pairs_iter | strict | 40000 | 0.052020s | 3.52x | 0.097802s | 1.87x | 0.183037s |
| function_calls | strict | 500000 | 0.009510s | 26.08x | 0.322914s | 0.77x | 0.248058s |
| closure_alloc | strict | 350000 | 0.021865s | 1.55x | 0.021128s | 1.61x | 0.033956s |
| pcall_success | strict | 375000 | 0.000780s | 7.80x | 0.002462s | 2.47x | 0.006083s |
| table_sort_int | parity | 99050 | 0.004641s | 8.12x | 0.008999s | 4.19x | 0.037701s |
| table_sort_cmp | parity | 65240 | 0.014237s | 1.46x | 0.019076s | 1.09x | 0.020766s |
| table_sort_strings | parity | 735 | 0.006772s | 1.99x | 0.008690s | 1.55x | 0.013487s |
| table_sort_reverse | parity | 285040 | 0.001506s | 18.89x | 0.002607s | 10.91x | 0.028454s |
| table_sort_int64 | parity | 358755 | 0.000639s | 6.04x | 0.001223s | 3.15x | 0.003857s |
| table_sort_floats | parity | 9290 | 0.000790s | 9.40x | 0.001856s | 4.00x | 0.007427s |
| string_concat | strict | 199680 | 0.009080s | 0.87x | 0.009488s | 0.83x | 0.007919s |
| table_concat | parity | 1524000 | 0.001270s | 2.96x | 0.001490s | 2.52x | 0.003754s |
| table_move_copy | parity | 710000 | 0.040021s | 1.35x | 0.040671s | 1.33x | 0.053921s |
| table_move_large_copy | parity | 343000 | 0.060450s | 1.25x | 0.060636s | 1.25x | 0.075539s |
| table_move_self_nooverlap | parity | 390000 | 0.001484s | 28.60x | 0.002208s | 19.23x | 0.042449s |
| table_pack_loop | parity | 325000 | 0.015986s | 2.19x | 0.016211s | 2.16x | 0.035052s |
| table_remove_front | parity | 948800 | 0.012723s | 9.54x | 0.014274s | 8.51x | 0.121434s |
| table_insert_tail | parity | 430800 | 0.001485s | 8.98x | 0.012811s | 1.04x | 0.013332s |
| table_remove_tail | parity | 172800 | 0.012859s | 1.28x | 0.015874s | 1.04x | 0.016483s |
| table_insert_front | parity | 474000 | 0.007478s | 6.31x | 0.008984s | 5.25x | 0.047184s |
| table_remove_middle | parity | 132800 | 0.005466s | 6.71x | 0.007204s | 5.09x | 0.036681s |
| table_move_overlap_forward | parity | 900000 | 0.002448s | 26.05x | 0.003251s | 19.61x | 0.063760s |
| table_move_overlap_backward | parity | 900000 | 0.002389s | 26.58x | 0.003183s | 19.95x | 0.063510s |
| table_string_key_insert | strict | 756600 | 0.005429s | 4.20x | 0.009067s | 2.52x | 0.022805s |
| table_concat_numbers | parity | 1638000 | 0.001631s | 12.76x | 0.001576s | 13.21x | 0.020816s |
| string_find_plain | parity | 520000 | 0.000043s | 92.47x | 0.003671s | 1.08x | 0.003976s |
| string_find_init | parity | 147568 | 0.001591s | 4.71x | 0.006649s | 1.13x | 0.007487s |
| string_find_pattern | parity | 520000 | 0.005178s | 1.30x | 0.005589s | 1.20x | 0.006717s |
| string_find_class | parity | 240000 | 0.007316s | 1.27x | 0.008014s | 1.16x | 0.009273s |
| string_gsub | parity | 23520000 | 0.022599s | 3.60x | 0.023361s | 3.48x | 0.081262s |
| string_gsub_capture | parity | 6048000 | 0.007107s | 5.97x | 0.007395s | 5.74x | 0.042451s |
| string_gsub_func | parity | 9600000 | 0.008153s | 14.41x | 0.007905s | 14.86x | 0.117501s |
| string_gsub_table | parity | 11200000 | 0.005201s | 21.52x | 0.005215s | 21.47x | 0.111946s |
| string_gsub_no_match | parity | 28800000 | 0.000592s | 146.82x | 0.000719s | 120.88x | 0.086916s |
| string_lower_upper | parity | 20160000 | 0.006640s | 2.34x | 0.006948s | 2.24x | 0.015562s |
| string_reverse_loop | parity | 16800000 | 0.000895s | 5.76x | 0.001031s | 5.00x | 0.005158s |
| string_rep_loop | parity | 8249776 | 0.005384s | 2.99x | 0.012008s | 1.34x | 0.016084s |
| string_char_loop | parity | 1500000 | 0.007873s | 1.85x | 0.019965s | 0.73x | 0.014604s |
| string_byte_range | parity | 800000 | 0.003180s | 22.17x | 0.049190s | 1.43x | 0.070485s |
| string_gmatch_captures | parity | 3600000 | 0.036557s | 1.14x | 0.035049s | 1.19x | 0.041736s |
| string_match_repeated | parity | 1170000 | 0.010157s | 1.20x | 0.010851s | 1.12x | 0.012168s |
| string_format_int64 | parity | 1830100 | 0.006454s | 2.36x | 0.005448s | 2.80x | 0.015255s |
| string_format_float | parity | 1430371 | 0.008654s | 2.55x | 0.013560s | 1.63x | 0.022065s |
| string_format_wide_hex | parity | 1460100 | 0.002743s | 5.03x | 0.004905s | 2.81x | 0.013802s |
| string_pack_unpack_mixed | parity | 171822 | 0.016665s | 1.24x | 0.024622s | 0.84x | 0.020744s |
| string_pack_unpack_many_i8 | parity | 285000 | 0.005547s | 2.30x | 0.007975s | 1.60x | 0.012764s |
| math_sin | parity | -0.116934 | 0.004706s | 6.09x | 0.020646s | 1.39x | 0.028669s |
| math_floor | parity | 125000 | 0.001618s | 25.66x | 0.038946s | 1.07x | 0.041516s |
| math_abs_int64 | parity | 500000 | 0.028073s | 1.57x | 0.119089s | 0.37x | 0.043958s |
| math_fmod_int64 | parity | 992779 | 0.018251s | 2.56x | 0.062970s | 0.74x | 0.046738s |
| math_sqrt_log | parity | 580649595.776236 | 0.001454s | 14.79x | 0.018947s | 1.13x | 0.021502s |
| math_type_loop | strict | 2500000 | 0.026741s | 1.37x | 0.029188s | 1.26x | 0.036649s |
| math_tointeger_loop | strict | 250000 | 0.041462s | 1.32x | 0.048917s | 1.12x | 0.054880s |
| math_ult_loop | strict | 0 | 0.003188s | 27.82x | 0.060365s | 1.47x | 0.088694s |
| math_minmax_int64 | strict | 257567 | 0.009911s | 6.67x | 0.100512s | 0.66x | 0.066098s |
| math_minmax_mixed_int64 | strict | 79040 | 0.007494s | 6.45x | 0.074283s | 0.65x | 0.048372s |
| coroutine_resume | strict | 250000 | 0.013262s | 1.16x | 0.019957s | 0.77x | 0.015426s |
| coroutine_resume_args | strict | 269999 | 0.009911s | 1.15x | 0.014706s | 0.77x | 0.011390s |
| coroutine_yield_multi | strict | 360000 | 0.010457s | 1.11x | 0.015213s | 0.76x | 0.011627s |
| coroutine_wrap_loop | strict | 220000 | 0.005213s | 2.33x | 0.005202s | 2.34x | 0.012163s |
| numeric_for_down | strict | 999827 | 0.007890s | 5.13x | 0.000000s | 40456000.00x | 0.040456s |
| branch_mod_loop | strict | 0 | 0.042479s | 2.22x | 0.013378s | 7.06x | 0.094454s |
| bit_rotate64 | strict | 1394249097 | 0.025456s | 3.64x | 0.289233s | 0.32x | 0.092569s |
| table_len_loop | strict | 0 | 0.006543s | 16.06x | 0.093957s | 1.12x | 0.105070s |
| table_insert_remove | parity | 360000 | 0.037098s | 2.33x | 0.067707s | 1.28x | 0.086561s |
| table_unpack_multi | parity | 0 | 0.014629s | 2.00x | 0.020743s | 1.41x | 0.029289s |
| string_byte_sum | parity | 200000 | 0.010907s | 20.64x | 0.192115s | 1.17x | 0.225077s |
| string_sub_loop | parity | 5600000 | 0.000605s | 27.59x | 0.013628s | 1.22x | 0.016693s |
| string_format_num | parity | 1688895 | 0.003429s | 5.30x | 0.005808s | 3.13x | 0.018170s |
| string_match_pattern | parity | 1080000 | 0.016340s | 1.22x | 0.017511s | 1.14x | 0.020014s |
| string_pack_unpack_i4 | parity | 500000 | 0.014646s | 1.85x | 0.017389s | 1.56x | 0.027100s |
| string_pack_unpack_i8 | parity | 770000 | 0.008972s | 1.86x | 0.013585s | 1.23x | 0.016700s |
| tonumber_parse | parity | 839824 | 0.024249s | 1.86x | 0.043355s | 1.04x | 0.045029s |
| tonumber_base16 | parity | 900816 | 0.004569s | 4.45x | 0.008784s | 2.31x | 0.020313s |
| tostring_int | parity | 7288896 | 0.086989s | 1.27x | 0.059740s | 1.85x | 0.110654s |
| tostring_float | parity | 4388895 | 0.032953s | 2.68x | 0.032205s | 2.74x | 0.088220s |
| math_min_max | strict | 970055 | 0.012797s | 22.67x | 0.280789s | 1.03x | 0.290057s |
| closure_upvalue_update | strict | 250000 | 0.009835s | 2.99x | 0.029277s | 1.01x | 0.029439s |
| upvalue_read_loop | strict | 0 | 0.001742s | 36.88x | 0.053118s | 1.21x | 0.064244s |
| method_call | strict | 250000 | 0.009874s | 2.91x | 0.034410s | 0.84x | 0.028740s |
| select_vararg | strict | 900000 | 0.005578s | 11.58x | 0.073817s | 0.87x | 0.064577s |
| vararg_sum | strict | 450000 | 0.015878s | 6.14x | 0.105433s | 0.92x | 0.097457s |
| tail_call_pair | strict | 500000 | 0.002693s | 31.33x | 0.054414s | 1.55x | 0.084367s |
| metamethod_index | strict | 400000 | 0.000261s | 55.24x | 0.015887s | 0.91x | 0.014417s |
| metamethod_index_table | strict | 600000 | 0.000539s | 40.25x | 0.026072s | 0.83x | 0.021694s |
| metamethod_add | strict | 250000 | 0.000177s | 49.28x | 0.009120s | 0.96x | 0.008722s |
| metamethod_call | strict | 50000 | 0.000275s | 30.13x | 0.006944s | 1.19x | 0.008285s |
| metamethod_len | strict | 400000 | 0.000261s | 50.05x | 0.013276s | 0.98x | 0.013063s |
| metamethod_newindex | strict | 400000 | 0.002619s | 4.31x | 0.011446s | 0.99x | 0.011296s |
| metamethod_compare | strict | 600000 | 0.000418s | 101.62x | 0.036526s | 1.16x | 0.042479s |
| metamethod_eq_false | strict | 900000 | 0.000210s | 74.77x | 0.012969s | 1.21x | 0.015702s |
| metamethod_concat | strict | 360000 | 0.007518s | 1.22x | 0.008453s | 1.08x | 0.009150s |
| global_read_loop | strict | 0 | 0.004359s | 13.66x | 0.065584s | 0.91x | 0.059552s |
| global_write_loop | strict | 0 | 0.001086s | 12.97x | 0.013137s | 1.07x | 0.014081s |
| table_field_read_loop | strict | 0 | 0.013563s | 12.97x | 0.179713s | 0.98x | 0.175879s |
| table_field_write_loop | strict | 0 | 0.015654s | 2.19x | 0.029655s | 1.16x | 0.034261s |
| table_array_append_direct | strict | 382400 | 0.001440s | 4.86x | 0.011868s | 0.59x | 0.007000s |
| table_array_pop_direct | strict | 894400 | 0.009655s | 1.70x | 0.018405s | 0.89x | 0.016376s |
| table_unpack_32 | parity | 200000 | 0.014162s | 4.67x | 0.040388s | 1.64x | 0.066189s |
| table_concat_slice | parity | 1072000 | 0.000960s | 3.08x | 0.001335s | 2.22x | 0.002961s |
| table_sort_nearly_sorted | parity | 280070 | 0.001040s | 18.75x | 0.001664s | 11.72x | 0.019499s |
| table_sort_many_tiny | parity | 279345 | 0.000187s | 7.16x | 0.000653s | 2.05x | 0.001339s |
| table_sort_cmp_int64 | parity | 359520 | 0.006159s | 1.44x | 0.007880s | 1.12x | 0.008862s |
| string_len_loop | parity | 0 | 0.004350s | 14.36x | 0.055576s | 1.12x | 0.062459s |
| string_byte_single | parity | 647155 | 0.004594s | 19.71x | 0.083595s | 1.08x | 0.090555s |
| string_sub_negative | parity | 5950000 | 0.000465s | 37.49x | 0.019139s | 0.91x | 0.017431s |
| string_compare_loop | parity | 0 | 0.034195s | 1.47x | 0.067962s | 0.74x | 0.050173s |
| string_intern_concat | strict | 2270694 | 0.005015s | 4.37x | 0.008416s | 2.60x | 0.021910s |
| string_format_quote | parity | 938894 | 0.001271s | 5.16x | 0.002526s | 2.60x | 0.006560s |
| tonumber_float_parse | parity | 986882480.496089 | 0.012181s | 2.32x | 0.019608s | 1.44x | 0.028296s |
| tonumber_int64_parse | parity | 600000 | 0.005932s | 1.67x | 0.013846s | 0.71x | 0.009887s |
| math_abs_small | strict | 0 | 0.058262s | 4.09x | 0.195305s | 1.22x | 0.238501s |
| math_floor_negative | parity | 357142 | 0.002048s | 17.34x | 0.038773s | 0.92x | 0.035514s |
| math_type_mixed | strict | 0 | 0.023457s | 1.49x | 0.035891s | 0.98x | 0.034997s |
| float_idiv_mod | strict | 999985 | 0.045889s | 1.71x | 0.048811s | 1.61x | 0.078398s |
| bit_mixed_const32 | strict | 332866815 | 0.067418s | 1.71x | 0.395905s | 0.29x | 0.115299s |
| bit_chain64_mixed | strict | 377178668 | 0.007588s | 6.57x | 0.236748s | 0.21x | 0.049889s |
| bit_shift_signed64 | strict | 603754476 | 0.005715s | 5.90x | 0.178160s | 0.19x | 0.033741s |
| int64_unary_minus | strict | 0 | 0.015894s | 2.39x | 0.000000s | 37992000.00x | 0.037992s |
| int64_compare_zero_branch | strict | 0 | 0.048231s | 1.14x | 0.149831s | 0.37x | 0.055126s |
| int64_add_compare_loop | strict | 900011 | 0.017232s | 2.23x | 0.000000s | 38483000.00x | 0.038483s |
| numeric_for_float_span | strict | 999278 | 0.093298s | 1.13x | 0.032320s | 3.25x | 0.105046s |
| while_loop_countdown | strict | 998963 | 0.018149s | 5.82x | 0.000000s | 105566000.00x | 0.105566s |
| repeat_until_countdown | strict | 998963 | 0.018317s | 5.63x | 0.000000s | 103141000.00x | 0.103141s |
| generic_for_iter_closure | strict | 0 | 0.002758s | 26.70x | 0.093045s | 0.79x | 0.073643s |
| pairs_update_values | strict | 374000 | 0.046518s | 2.51x | 0.098689s | 1.19x | 0.116986s |
| closure_factory | strict | 600000 | 0.012933s | 1.48x | 0.012423s | 1.54x | 0.019163s |
| nested_closure_call | strict | 0 | 0.000450s | 44.49x | 0.020142s | 0.99x | 0.020022s |
| tail_call_chain | strict | 500000 | 0.001104s | 90.53x | 0.053050s | 1.88x | 0.099944s |
| vararg_select_tail | strict | 500000 | 0.006019s | 4.51x | 0.031410s | 0.86x | 0.027138s |
| metamethod_bitwise | strict | 834592 | 0.001144s | 3.59x | 0.005448s | 0.75x | 0.004102s |
| metamethod_idiv | strict | 127156 | 0.001330s | 2.66x | 0.006731s | 0.52x | 0.003533s |
| metamethod_pairs_loop | strict | 700000 | 0.018520s | 2.06x | 0.018826s | 2.02x | 0.038077s |
| load_string_loop | parity | 199148 | 0.007027s | 1.44x | 0.006529s | 1.55x | 0.010127s |
| debug_getinfo_loop | parity | 50000 | 0.008321s | 2.11x | 0.008607s | 2.04x | 0.017533s |
| gc_table_alloc_collect | parity | 332325 | 0.121342s | 0.36x | 0.122400s | 0.35x | 0.043188s |
| gc_string_churn | parity | 1350000 | 0.004101s | 2.14x | 0.005802s | 1.51x | 0.008757s |
| pcall_error | strict | 120000 | 0.002409s | 4.09x | 0.002669s | 3.69x | 0.009852s |
| xpcall_error | strict | 80000 | 0.003843s | 1.96x | 0.006937s | 1.09x | 0.007549s |
| pcall_multi_return | strict | 900000 | 0.001054s | 5.56x | 0.003296s | 1.78x | 0.005860s |
| pcall_vararg_success | strict | 90000 | 0.000687s | 9.80x | 0.004994s | 1.35x | 0.006730s |
| coroutine_create_resume | strict | 120000 | 0.009951s | 0.99x | 0.009935s | 0.99x | 0.009812s |
| string_gmatch_words | parity | 2800000 | 0.014942s | 1.40x | 0.014668s | 1.43x | 0.020941s |
| utf8_codes_loop | parity | 760000 | 0.021543s | 1.10x | 0.021681s | 1.09x | 0.023686s |
| utf8_codes_lax_loop | parity | 760000 | 0.020855s | 1.11x | 0.021310s | 1.09x | 0.023253s |
| utf8_len_loop | parity | 5760000 | 0.009159s | 1.06x | 0.009581s | 1.01x | 0.009694s |
| utf8_len_ascii | parity | 34560000 | 0.011355s | 2.49x | 0.012381s | 2.28x | 0.028232s |
| utf8_codepoint_loop | parity | 1200000 | 0.002514s | 1.20x | 0.002714s | 1.11x | 0.003026s |
| utf8_char_build | parity | 1600000 | 0.007753s | 2.23x | 0.010304s | 1.68x | 0.017279s |
| utf8_offset_loop | parity | 509980 | 0.004124s | 1.21x | 0.006325s | 0.79x | 0.004979s |
| int64_add_const_box | strict | 627776 | 0.006406s | 4.63x | 0.028391s | 1.04x | 0.029663s |
| int64_sub_zero_compare | strict | 0 | 0.002188s | 13.74x | 0.000000s | 30068000.00x | 0.030068s |
| int64_mul_const_wrap | strict | 706027200 | 0.012102s | 3.35x | 0.055484s | 0.73x | 0.040493s |
| idiv_dynamic | strict | 90327 | 0.018471s | 2.76x | 0.008491s | 6.01x | 0.051013s |
| mod_dynamic | strict | 936739 | 0.014367s | 4.42x | 0.008684s | 7.31x | 0.063481s |
| numeric_for_int64_down | strict | 600376 | 0.011302s | 1.81x | 0.000000s | 20426000.00x | 0.020426s |
| numeric_for_int64_step3 | strict | 297 | 0.011365s | 1.80x | 0.000000s | 20411000.00x | 0.020411s |
| bit_and64_power2_mask | strict | 1081168960 | 0.014317s | 2.31x | 0.064060s | 0.52x | 0.033020s |
| bit_or64_small_const | strict | 1087329633 | 0.016267s | 2.17x | 0.007887s | 4.48x | 0.035327s |
| bit_xor64_small_const | strict | 9000000 | 0.011851s | 1.79x | 0.000000s | 21221000.00x | 0.021221s |
| bit_shift64_large_count | strict | 7000000 | 0.004740s | 10.17x | 0.132056s | 0.37x | 0.048217s |
| bit_shift64_neg_count | strict | 26591 | 0.005718s | 6.11x | 0.004743s | 7.37x | 0.034965s |
| bit_branch32_test | strict | 804674 | 0.009283s | 8.15x | 0.005496s | 13.76x | 0.075614s |
| table_rawget_int64_key | strict | 114272 | 0.029662s | 1.13x | 0.057900s | 0.58x | 0.033572s |
| table_rawset_int64_key | strict | 350000 | 0.009896s | 2.00x | 0.021657s | 0.91x | 0.019744s |
| table_set_int64_float_equiv | strict | 90000 | 0.003948s | 2.84x | 0.010629s | 1.06x | 0.011220s |
| table_string_key_miss | strict | 700000 | 0.005999s | 6.94x | 0.016011s | 2.60x | 0.041645s |
| table_sort_duplicates | parity | 1540 | 0.003328s | 7.68x | 0.005709s | 4.47x | 0.025545s |
| table_sort_cmp_strings | parity | 630 | 0.008516s | 1.53x | 0.010549s | 1.23x | 0.013027s |
| table_concat_large | parity | 5116000 | 0.003146s | 2.97x | 0.003447s | 2.71x | 0.009331s |
| table_unpack_slice_16 | parity | 800000 | 0.015073s | 3.52x | 0.038937s | 1.36x | 0.053026s |
| string_find_absent_plain | parity | 180000 | 0.000042s | 155.17x | 0.006677s | 0.98x | 0.006517s |
| string_find_long_plain | parity | 880000 | 0.000029s | 250.24x | 0.006913s | 1.05x | 0.007257s |
| string_match_no_capture | parity | 1260000 | 0.011845s | 1.28x | 0.012763s | 1.19x | 0.015172s |
| string_gsub_literal | parity | 12600000 | 0.004393s | 8.87x | 0.004623s | 8.43x | 0.038986s |
| string_sub_large_slice | parity | 28800000 | 0.015522s | 1.04x | 0.019280s | 0.84x | 0.016134s |
| string_byte_full_short | parity | 400000 | 0.002478s | 21.25x | 0.038020s | 1.39x | 0.052665s |
| string_char_many | parity | 2400000 | 0.019203s | 1.42x | 0.039127s | 0.69x | 0.027173s |
| string_rep_sep | parity | 4410000 | 0.003091s | 3.32x | 0.008611s | 1.19x | 0.010264s |
| string_format_mixed | parity | 1536768 | 0.007038s | 2.10x | 0.007214s | 2.05x | 0.014781s |
| string_pack_unpack_float | parity | 736924 | 0.014841s | 1.21x | 0.017488s | 1.03x | 0.017978s |
| tonumber_hex64_parse | parity | 263472 | 0.004073s | 1.77x | 0.009951s | 0.72x | 0.007210s |
| math_modf_loop | parity | 750042 | 0.008161s | 7.55x | 0.078283s | 0.79x | 0.061635s |
| math_floor_int_passthrough | parity | 0 | 0.002219s | 45.46x | 0.075575s | 1.33x | 0.100866s |
| math_tointeger_int64_string | strict | 46640 | 0.004954s | 1.59x | 0.011680s | 0.68x | 0.007900s |
| metamethod_order_int64 | strict | 800000 | 0.000326s | 66.94x | 0.019540s | 1.12x | 0.021823s |
| pcall_deep_stack | strict | 240000 | 0.000491s | 11.14x | 0.002388s | 2.29x | 0.005470s |
| coroutine_pingpong | strict | 240001 | 0.009128s | 1.17x | 0.013564s | 0.79x | 0.010687s |
| debug_getlocal_loop | parity | 40001 | 0.001188s | 0.88x | 0.001666s | 0.63x | 0.001048s |
| gc_table_churn_no_collect | parity | 478400 | 0.000413s | 60.04x | 0.012464s | 1.99x | 0.024797s |
| table_next_array_extra | strict | 0 | 0.269662s | 1.58x | 0.251407s | 1.69x | 0.425976s |
| table_next_sparse_extra | strict | 600000 | 0.397920s | 0.98x | 0.389108s | 1.00x | 0.390526s |
| table_pairs_array_extra | strict | 0 | 0.130154s | 2.12x | 0.130653s | 2.11x | 0.275381s |
| table_sort_cmp_upvalue_extra | parity | 949178 | 0.002843s | 12.66x | 0.004792s | 7.51x | 0.035980s |
| table_sort_records_extra | parity | 25802 | 0.008836s | 1.63x | 0.012670s | 1.13x | 0.014377s |
| table_sort_large_int_extra | parity | 815810 | 0.003105s | 5.40x | 0.004526s | 3.70x | 0.016767s |
| string_concat_small_extra | strict | 2160000 | 0.047916s | 1.47x | 0.068242s | 1.03x | 0.070275s |
| string_sub_tiny_extra | parity | 1200000 | 0.000988s | 26.58x | 0.021756s | 1.21x | 0.026264s |
| string_match_captures_extra | parity | 1800000 | 0.012322s | 1.24x | 0.012987s | 1.18x | 0.015315s |
| string_arith_int64_extra | strict | 885000 | 0.000676s | 3.02x | 0.002060s | 0.99x | 0.002042s |
| bit64_and_shift_extra | strict | 2019367627 | 0.013132s | 3.15x | 0.199493s | 0.21x | 0.041312s |
| bit64_branch_not_extra | strict | 0 | 0.004924s | 7.75x | 0.004669s | 8.18x | 0.038173s |
| int64_compare_threshold_extra | strict | 995968 | 0.006112s | 3.81x | 0.002120s | 10.99x | 0.023290s |
| int64_minmax_chain_extra | strict | 0 | 0.013254s | 5.88x | 0.119448s | 0.65x | 0.077933s |
| int64_compare_eq_const_extra | strict | 41016 | 0.003973s | 6.95x | 0.002120s | 13.02x | 0.027593s |
| int64_compare_between_extra | strict | 12 | 0.006135s | 3.11x | 0.001937s | 9.85x | 0.019089s |
| int64_compare_descending_extra | strict | 4032 | 0.006091s | 3.80x | 0.002173s | 10.65x | 0.023133s |
| bit64_const_mask_chain_extra | strict | 65477308 | 0.012795s | 2.22x | 0.153706s | 0.18x | 0.028371s |
| bit64_const_or_xor_chain_extra | strict | 7000001 | 0.011742s | 1.77x | 0.110953s | 0.19x | 0.020756s |
| bit64_compare_mask_zero_extra | strict | 109564 | 0.007787s | 2.79x | 0.053589s | 0.41x | 0.021763s |
| table_next_mixed_extra | strict | 744000 | 0.163464s | 1.47x | 0.159876s | 1.51x | 0.240925s |
| table_pairs_sparse_extra | strict | 600000 | 0.296358s | 1.01x | 0.248809s | 1.20x | 0.299461s |
| table_pairs_int64_keys_extra | strict | 520000 | 0.118524s | 1.56x | 0.204156s | 0.91x | 0.185468s |
| load_dump_many_protos_extra | parity | 54000 | 0.000233s | 3.93x | 0.000262s | 3.50x | 0.000916s |
| debug_getupvalue_extra | parity | 80000 | 0.001498s | 1.19x | 0.001964s | 0.91x | 0.001783s |
| debug_setupvalue_extra | parity | 467080 | 0.001562s | 1.27x | 0.002323s | 0.85x | 0.001986s |
| closure_call_mixed_extra | strict | 910272 | 0.001107s | 16.75x | 0.016401s | 1.13x | 0.018537s |
| load_dump_extra | parity | 762500 | 0.000360s | 5.63x | 0.000346s | 5.86x | 0.002028s |
| bit64_rotate_var_extra | strict | 1807377590 | 0.010049s | 3.98x | 0.101680s | 0.39x | 0.040008s |
| bit64_extract_insert_extra | strict | 536706047 | 0.009492s | 5.68x | 0.040978s | 1.32x | 0.053923s |
| bit32_extract_branch_extra | strict | 154058 | 0.052765s | 3.47x | 0.130885s | 1.40x | 0.182985s |
| bit64_math_ult_branch_extra | strict | 999988 | 0.012651s | 7.13x | 0.157901s | 0.57x | 0.090253s |
| int64_compare_float_threshold_extra | strict | 997120 | 0.012462s | 1.41x | 0.001529s | 11.52x | 0.017612s |
| int64_compare_table_bound_extra | strict | 328 | 0.006813s | 3.10x | 0.033088s | 0.64x | 0.021152s |
| numeric_for_int64_large_step_extra | strict | 927776 | 0.015904s | 1.07x | 0.025036s | 0.68x | 0.017012s |
| table_next_int64_keys_extra | strict | 520000 | 0.192197s | 1.31x | 0.258003s | 0.97x | 0.251334s |
| table_pairs_string_keys_extra | strict | 568000 | 0.152636s | 1.73x | 0.229488s | 1.15x | 0.263305s |
| table_pairs_mixed_sparse_extra | strict | 704000 | 0.353295s | 1.01x | 0.319000s | 1.11x | 0.355600s |
| table_sort_tiny_cmp_extra | parity | 126926 | 0.003939s | 1.46x | 0.007788s | 0.74x | 0.005747s |
| table_sort_cmp_desc_extra | parity | 870240 | 0.001476s | 31.13x | 0.004085s | 11.25x | 0.045946s |
| table_sort_large_strings_extra | parity | 504 | 0.002506s | 3.09x | 0.003903s | 1.98x | 0.007736s |
| table_sort_wide_records_extra | parity | 35772 | 0.006968s | 1.69x | 0.011980s | 0.98x | 0.011764s |
| table_sort_equal_keys_extra | parity | 62857 | 0.013993s | 1.71x | 0.023724s | 1.01x | 0.023987s |
| table_move_int64_extra | parity | 625000 | 0.066602s | 1.16x | 0.063338s | 1.22x | 0.077587s |
| string_find_frontier_extra | parity | 2520000 | 0.007395s | 1.27x | 0.008415s | 1.12x | 0.009395s |
| string_gsub_many_repl_extra | parity | 672000 | 0.003787s | 1.48x | 0.003842s | 1.46x | 0.005620s |
| string_format_int64_mix_extra | parity | 2730000 | 0.002300s | 6.45x | 0.005431s | 2.73x | 0.014826s |
| gc_closure_alloc_collect_extra | parity | 103040 | 0.091184s | 0.35x | 0.090676s | 0.35x | 0.031485s |
