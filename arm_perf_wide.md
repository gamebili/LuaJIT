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
| luajit_jit_on | 5.913772s | 2.48x | 4.07x | 3 | 2 |
| luajit_jit_off | 15.592414s | 0.94x | 7.78x | 83 | 10 |
| lua5.4.8 | 14.684650s | 1.00x | 1.00x | 0 | 0 |

## Key Findings

- JIT-on is faster on 177/180 strict workloads; the strict slower cases are string_concat 0.92x (0.008473s vs 0.007817s), coroutine_create_resume 0.96x (0.009982s vs 0.009596s), table_next_sparse_extra 0.98x (0.389947s vs 0.382962s).
- JIT-off is slower on 83/180 strict workloads; the worst confirmed cases are bit64_const_mask_chain_extra 0.18x (0.150320s vs 0.027586s), bit64_const_or_xor_chain_extra 0.19x (0.109746s vs 0.020466s), bit_or64 0.19x (0.190603s vs 0.036148s), bit_shift64_const 0.19x (0.155232s vs 0.029744s), bit_shift64_right 0.19x (0.218518s vs 0.042392s), bit_shift_signed64 0.20x (0.172837s vs 0.034269s), bit_and64 0.20x (0.215988s vs 0.043393s), bit_not64 0.21x (0.168293s vs 0.035064s).
- C-similar parity workloads are allowed to be near parity, but below 0.80x is treated as too slow: JIT-on 2 case(s): gc_closure_alloc_collect_extra 0.35x (0.087511s vs 0.030805s), gc_table_alloc_collect 0.36x (0.116567s vs 0.041975s); JIT-off 10 case(s): gc_closure_alloc_collect_extra 0.36x (0.086612s vs 0.030805s), gc_table_alloc_collect 0.36x (0.116605s vs 0.041975s), math_abs_int64 0.40x (0.108969s vs 0.043558s), debug_getlocal_loop 0.66x (0.001531s vs 0.001016s), string_char_many 0.74x (0.035595s vs 0.026181s), tonumber_int64_parse 0.74x (0.013043s vs 0.009613s), tonumber_hex64_parse 0.76x (0.009255s vs 0.007048s), string_char_loop 0.76x (0.018800s vs 0.014317s).
- Checksums matched for all modes, so the listed slowdowns are performance differences, not result mismatches.

## Strict Slower Than Lua 5.4.8

| Mode | Workload | LuaJIT time | Lua 5.4.8 time | Speedup |
| --- | --- | ---: | ---: | ---: |
| luajit_jit_off | bit64_const_mask_chain_extra | 0.150320s | 0.027586s | 0.18x |
| luajit_jit_off | bit64_const_or_xor_chain_extra | 0.109746s | 0.020466s | 0.19x |
| luajit_jit_off | bit_or64 | 0.190603s | 0.036148s | 0.19x |
| luajit_jit_off | bit_shift64_const | 0.155232s | 0.029744s | 0.19x |
| luajit_jit_off | bit_shift64_right | 0.218518s | 0.042392s | 0.19x |
| luajit_jit_off | bit_shift_signed64 | 0.172837s | 0.034269s | 0.20x |
| luajit_jit_off | bit_and64 | 0.215988s | 0.043393s | 0.20x |
| luajit_jit_off | bit_not64 | 0.168293s | 0.035064s | 0.21x |
| luajit_jit_off | bit_chain64_mixed | 0.236023s | 0.049751s | 0.21x |
| luajit_jit_off | bit64_and_shift_extra | 0.196202s | 0.041740s | 0.21x |
| luajit_jit_off | bitwise_idiv_unsigned | 0.431408s | 0.102747s | 0.24x |
| luajit_jit_off | bit_mixed_const32 | 0.365720s | 0.111908s | 0.31x |
| luajit_jit_off | bitwise_idiv_mix | 0.441641s | 0.139815s | 0.32x |
| luajit_jit_off | bit_rotate64 | 0.276710s | 0.090121s | 0.33x |
| luajit_jit_off | bit64_branch_test | 0.117025s | 0.039681s | 0.34x |
| luajit_jit_off | bit_shift64_large_count | 0.132046s | 0.047120s | 0.36x |
| luajit_jit_off | bit_shift_mask | 0.517007s | 0.190861s | 0.37x |
| luajit_jit_off | int64_compare_zero_branch | 0.142504s | 0.053845s | 0.38x |
| luajit_jit_off | bit64_rotate_var_extra | 0.099669s | 0.039352s | 0.39x |
| luajit_jit_off | bit_shift32_const | 0.439736s | 0.180268s | 0.41x |
| luajit_jit_off | int64_compare_minmax | 0.161227s | 0.066582s | 0.41x |
| luajit_jit_off | bit64_compare_mask_zero_extra | 0.052066s | 0.021561s | 0.41x |
| luajit_jit_off | bit_and32 | 0.208440s | 0.089366s | 0.43x |
| luajit_jit_off | bit_shift64_var | 0.127875s | 0.057077s | 0.45x |
| luajit_jit_off | metamethod_idiv | 0.006880s | 0.003509s | 0.51x |
| luajit_jit_off | table_float_key_lookup | 0.094430s | 0.048637s | 0.52x |
| luajit_jit_off | bit_and64_power2_mask | 0.062694s | 0.032382s | 0.52x |
| luajit_jit_off | table_int64_key_miss | 0.037294s | 0.019711s | 0.53x |
| luajit_jit_off | array_read_stride | 0.072206s | 0.038370s | 0.53x |
| luajit_jit_off | bit_shift32_var | 0.305730s | 0.166800s | 0.55x |
| luajit_jit_off | bit_xor32 | 0.156781s | 0.088283s | 0.56x |
| luajit_jit_off | table_rawget_int64_key | 0.056308s | 0.032592s | 0.58x |
| luajit_jit_off | bit64_math_ult_branch_extra | 0.151694s | 0.089219s | 0.59x |
| luajit_jit_off | idiv_negative | 0.314316s | 0.187738s | 0.60x |
| luajit_jit_off | table_array_append_direct | 0.011536s | 0.006906s | 0.60x |
| luajit_jit_off | table_int64_key_delete | 0.012887s | 0.008073s | 0.63x |
| luajit_jit_off | generic_for_iter_closure | 0.114033s | 0.072357s | 0.63x |
| luajit_jit_off | bit_or32 | 0.139335s | 0.088550s | 0.64x |
| luajit_jit_off | int64_compare_table_bound_extra | 0.032260s | 0.020876s | 0.65x |
| luajit_jit_off | int64_minmax_chain_extra | 0.114758s | 0.078122s | 0.68x |
| luajit_jit_off | array_sum_chunk | 0.163816s | 0.112632s | 0.69x |
| luajit_jit_off | int64_mul_const_wrap | 0.055755s | 0.038390s | 0.69x |
| luajit_jit_off | numeric_for_int64_large_step_extra | 0.023552s | 0.016298s | 0.69x |
| luajit_jit_off | metamethod_index_table | 0.026698s | 0.018579s | 0.70x |
| luajit_jit_off | float_arith | 0.283394s | 0.197826s | 0.70x |
| luajit_jit_off | idiv_mod | 0.449663s | 0.316984s | 0.70x |
| luajit_jit_off | math_minmax_int64 | 0.094493s | 0.067103s | 0.71x |
| luajit_jit_off | int64_add_sub | 0.172402s | 0.123437s | 0.72x |
| luajit_jit_off | int64_mul_wrap | 0.044274s | 0.031830s | 0.72x |
| luajit_jit_off | math_minmax_mixed_int64 | 0.067806s | 0.048909s | 0.72x |
| luajit_jit_off | global_write_loop | 0.013191s | 0.009794s | 0.74x |
| luajit_jit_off | coroutine_resume | 0.019427s | 0.014455s | 0.74x |
| luajit_jit_off | math_tointeger_int64_string | 0.010304s | 0.007673s | 0.74x |
| luajit_jit_off | coroutine_resume_args | 0.014213s | 0.010793s | 0.76x |
| luajit_jit_off | table_int64_key_insert | 0.009089s | 0.006914s | 0.76x |
| luajit_jit_off | metamethod_bitwise | 0.005232s | 0.003995s | 0.76x |
| luajit_jit_off | coroutine_pingpong | 0.013079s | 0.010070s | 0.77x |
| luajit_jit_off | coroutine_yield_multi | 0.014576s | 0.011227s | 0.77x |
| luajit_jit_off | idiv_power2 | 0.135441s | 0.105607s | 0.78x |
| luajit_jit_off | function_calls | 0.302470s | 0.238510s | 0.79x |
| luajit_jit_off | method_call | 0.033187s | 0.028194s | 0.85x |
| luajit_jit_off | vararg_select_tail | 0.031473s | 0.026827s | 0.85x |
| luajit_jit_off | int_mul32 | 0.261709s | 0.223732s | 0.85x |
| luajit_jit_off | table_rawset_int64_key | 0.021528s | 0.018702s | 0.87x |
| luajit_jit_off | hash_update | 0.028868s | 0.025260s | 0.88x |
| luajit_jit_off | table_mixed_key_lookup | 0.037497s | 0.032886s | 0.88x |
| luajit_jit_off | string_concat | 0.008793s | 0.007817s | 0.89x |
| luajit_jit_off | table_array_pop_direct | 0.017748s | 0.015857s | 0.89x |
| luajit_jit_off | table_pairs_int64_keys_extra | 0.204611s | 0.183801s | 0.90x |
| luajit_jit_off | global_read_loop | 0.064605s | 0.058794s | 0.91x |
| luajit_jit_off | metamethod_index | 0.016023s | 0.014679s | 0.92x |
| luajit_jit_off | select_vararg | 0.070901s | 0.065350s | 0.92x |
| luajit_jit_off | metamethod_newindex | 0.011337s | 0.010454s | 0.92x |
| luajit_jit_on | string_concat | 0.008473s | 0.007817s | 0.92x |
| luajit_jit_off | coroutine_create_resume | 0.010232s | 0.009596s | 0.94x |
| luajit_jit_off | nested_closure_call | 0.020162s | 0.019119s | 0.95x |
| luajit_jit_off | metamethod_add | 0.009035s | 0.008595s | 0.95x |
| luajit_jit_off | closure_upvalue_update | 0.029541s | 0.028225s | 0.96x |
| luajit_jit_off | table_next_int64_keys_extra | 0.254067s | 0.242981s | 0.96x |
| luajit_jit_off | vararg_sum | 0.102097s | 0.097673s | 0.96x |
| luajit_jit_on | coroutine_create_resume | 0.009982s | 0.009596s | 0.96x |
| luajit_jit_off | table_field_read_loop | 0.174270s | 0.170594s | 0.98x |
| luajit_jit_on | table_next_sparse_extra | 0.389947s | 0.382962s | 0.98x |
| luajit_jit_off | metamethod_len | 0.012918s | 0.012720s | 0.98x |
| luajit_jit_off | hash_lookup | 0.127525s | 0.125863s | 0.99x |
| luajit_jit_off | table_len_loop | 0.101935s | 0.101218s | 0.99x |

## Parity Workloads Below Threshold

| Mode | Workload | LuaJIT time | Lua 5.4.8 time | Speedup | Minimum |
| --- | --- | ---: | ---: | ---: | ---: |
| luajit_jit_on | gc_closure_alloc_collect_extra | 0.087511s | 0.030805s | 0.35x | 0.80x |
| luajit_jit_off | gc_closure_alloc_collect_extra | 0.086612s | 0.030805s | 0.36x | 0.80x |
| luajit_jit_off | gc_table_alloc_collect | 0.116605s | 0.041975s | 0.36x | 0.80x |
| luajit_jit_on | gc_table_alloc_collect | 0.116567s | 0.041975s | 0.36x | 0.80x |
| luajit_jit_off | math_abs_int64 | 0.108969s | 0.043558s | 0.40x | 0.80x |
| luajit_jit_off | debug_getlocal_loop | 0.001531s | 0.001016s | 0.66x | 0.80x |
| luajit_jit_off | string_char_many | 0.035595s | 0.026181s | 0.74x | 0.80x |
| luajit_jit_off | tonumber_int64_parse | 0.013043s | 0.009613s | 0.74x | 0.80x |
| luajit_jit_off | tonumber_hex64_parse | 0.009255s | 0.007048s | 0.76x | 0.80x |
| luajit_jit_off | string_char_loop | 0.018800s | 0.014317s | 0.76x | 0.80x |
| luajit_jit_off | table_sort_tiny_cmp_extra | 0.007361s | 0.005720s | 0.78x | 0.80x |
| luajit_jit_off | math_fmod_int64 | 0.059574s | 0.046912s | 0.79x | 0.80x |

## Parity Workloads Slower But Within Threshold

| Mode | Workload | LuaJIT time | Lua 5.4.8 time | Speedup |
| --- | --- | ---: | ---: | ---: |
| luajit_jit_off | math_modf_loop | 0.073325s | 0.060794s | 0.83x |
| luajit_jit_off | utf8_offset_loop | 0.005938s | 0.005009s | 0.84x |
| luajit_jit_on | debug_getlocal_loop | 0.001184s | 0.001016s | 0.86x |
| luajit_jit_off | debug_setupvalue_extra | 0.002231s | 0.001972s | 0.88x |
| luajit_jit_off | string_compare_loop | 0.066412s | 0.058837s | 0.89x |
| luajit_jit_off | debug_getupvalue_extra | 0.001968s | 0.001778s | 0.90x |
| luajit_jit_off | string_sub_large_slice | 0.017619s | 0.016086s | 0.91x |
| luajit_jit_off | string_pack_unpack_mixed | 0.022044s | 0.020514s | 0.93x |
| luajit_jit_off | math_floor_negative | 0.036787s | 0.035326s | 0.96x |
| luajit_jit_off | string_sub_negative | 0.017523s | 0.017090s | 0.98x |
| luajit_jit_off | table_remove_tail | 0.016489s | 0.016183s | 0.98x |
| luajit_jit_off | string_byte_single | 0.092148s | 0.090783s | 0.99x |
| luajit_jit_off | table_sort_wide_records_extra | 0.011573s | 0.011518s | 1.00x |

## Checksum/Correctness Mismatches

No checksum or per-run consistency mismatches.

## Slowdown Clusters

| Mode | Policy | Category | Slow workloads | Worst workload | Worst speedup |
| --- | --- | --- | ---: | --- | ---: |
| luajit_jit_off | parity | math library | 2 | math_abs_int64 | 0.40x |
| luajit_jit_off | parity | runtime/debug/gc | 3 | gc_closure_alloc_collect_extra | 0.36x |
| luajit_jit_off | parity | string/format/parse | 4 | string_char_many | 0.74x |
| luajit_jit_off | parity | table library | 1 | table_sort_tiny_cmp_extra | 0.78x |
| luajit_jit_off | strict | array/hash table | 6 | array_read_stride | 0.53x |
| luajit_jit_off | strict | bitwise/integer | 27 | bit64_const_mask_chain_extra | 0.18x |
| luajit_jit_off | strict | call/metamethod | 15 | metamethod_idiv | 0.51x |
| luajit_jit_off | strict | integer/control | 12 | int64_compare_zero_branch | 0.38x |
| luajit_jit_off | strict | math library | 3 | math_minmax_int64 | 0.71x |
| luajit_jit_off | strict | other | 1 | float_arith | 0.70x |
| luajit_jit_off | strict | protected/coroutine | 5 | coroutine_resume | 0.74x |
| luajit_jit_off | strict | string/format/parse | 1 | string_concat | 0.89x |
| luajit_jit_off | strict | table library | 13 | table_float_key_lookup | 0.52x |
| luajit_jit_on | parity | runtime/debug/gc | 2 | gc_closure_alloc_collect_extra | 0.35x |
| luajit_jit_on | strict | protected/coroutine | 1 | coroutine_create_resume | 0.96x |
| luajit_jit_on | strict | string/format/parse | 1 | string_concat | 0.92x |
| luajit_jit_on | strict | table library | 1 | table_next_sparse_extra | 0.98x |

## Full Results

| Workload | Policy | Checksum | JIT-on | On speedup | JIT-off | Off speedup | Lua 5.4.8 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| empty_loop | strict | 100000000 | 0.022277s | 12.47x | 0.216039s | 1.29x | 0.277743s |
| int_add_wide | strict | 191146379 | 0.304013s | 1.56x | 0.309659s | 1.54x | 0.475479s |
| int64_add_sub | strict | 127778 | 0.072993s | 1.69x | 0.172402s | 0.72x | 0.123437s |
| int_add32 | strict | 576647110 | 0.280002s | 2.85x | 0.588164s | 1.36x | 0.799133s |
| int_mul32 | strict | 311395809 | 0.054736s | 4.09x | 0.261709s | 0.85x | 0.223732s |
| int_mod_small | strict | 999740 | 0.157059s | 2.24x | 0.000000s | 352271000.00x | 0.352271s |
| idiv_const | strict | 0 | 0.020485s | 5.14x | 0.000000s | 105349000.00x | 0.105349s |
| idiv_negative | strict | 714300 | 0.066734s | 2.81x | 0.314316s | 0.60x | 0.187738s |
| idiv_power2 | strict | 65539 | 0.033957s | 3.11x | 0.135441s | 0.78x | 0.105607s |
| mod_const | strict | 500000 | 0.026778s | 5.02x | 0.000000s | 134444000.00x | 0.134444s |
| mod_power2 | strict | 387680 | 0.042481s | 3.17x | 0.000000s | 134519000.00x | 0.134519s |
| numeric_for_int64_span | strict | 999943 | 0.012539s | 1.75x | 0.000000s | 21893000.00x | 0.021893s |
| int_compare_branch | strict | 861515 | 0.042344s | 2.65x | 0.017487s | 6.41x | 0.112063s |
| int_compare_eq_chain | strict | 817169 | 0.035688s | 4.87x | 0.016494s | 10.55x | 0.173941s |
| int64_compare_minmax | strict | 0 | 0.031368s | 2.12x | 0.161227s | 0.41x | 0.066582s |
| int64_eq_branch | strict | 373544 | 0.009952s | 8.29x | 0.005815s | 14.19x | 0.082521s |
| int64_mod_const | strict | 992044 | 0.043079s | 1.64x | 0.000000s | 70657000.00x | 0.070657s |
| int64_idiv_const | strict | 632320 | 0.044179s | 1.02x | 0.000000s | 45026000.00x | 0.045026s |
| int64_mul_wrap | strict | 1744528928 | 0.005503s | 5.78x | 0.044274s | 0.72x | 0.031830s |
| float_compare_branch | strict | -963739252 | 0.121816s | 1.35x | 0.159142s | 1.03x | 0.164523s |
| float_arith | strict | 206120287761215.281250 | 0.149650s | 1.32x | 0.283394s | 0.70x | 0.197826s |
| pure_bit_xor | strict | 50000000 | 0.066966s | 1.32x | 0.000000s | 88680000.00x | 0.088680s |
| bit_and32 | strict | 33950784 | 0.046590s | 1.92x | 0.208440s | 0.43x | 0.089366s |
| bit_or32 | strict | 2147483647 | 0.031689s | 2.79x | 0.139335s | 0.64x | 0.088550s |
| bit_xor32 | strict | 102466744 | 0.031581s | 2.80x | 0.156781s | 0.56x | 0.088283s |
| bit_not32 | strict | 35000000 | 0.070410s | 1.10x | 0.000000s | 77229000.00x | 0.077229s |
| bit_shift32_const | strict | 300872905 | 0.054330s | 3.32x | 0.439736s | 0.41x | 0.180268s |
| bit_shift32_var | strict | 1592255438 | 0.045536s | 3.66x | 0.305730s | 0.55x | 0.166800s |
| bit_shift_mask | strict | 318080536 | 0.047440s | 4.02x | 0.517007s | 0.37x | 0.190861s |
| bitwise_idiv_unsigned | strict | 1821843616 | 0.037013s | 2.78x | 0.431408s | 0.24x | 0.102747s |
| bitwise_idiv_mix | strict | 714511537 | 0.057146s | 2.45x | 0.441641s | 0.32x | 0.139815s |
| idiv_mod | strict | 997901798 | 0.101927s | 3.11x | 0.449663s | 0.70x | 0.316984s |
| bit_shift64_const | strict | 295495246 | 0.018907s | 1.57x | 0.155232s | 0.19x | 0.029744s |
| bit_and64 | strict | 1890031 | 0.023770s | 1.83x | 0.215988s | 0.20x | 0.043393s |
| bit_and64_const | strict | 1456439740 | 0.014641s | 1.45x | 0.005234s | 4.05x | 0.021192s |
| bit_or64 | strict | 305419896 | 0.005235s | 6.91x | 0.190603s | 0.19x | 0.036148s |
| bit_or64_const | strict | 1459617791 | 0.007885s | 3.49x | 0.000000s | 27544000.00x | 0.027544s |
| bit_xor64 | strict | 1617852892 | 0.005608s | 8.56x | 0.005286s | 9.08x | 0.047984s |
| bit_xor64_const | strict | 1456439740 | 0.007905s | 3.49x | 0.000000s | 27592000.00x | 0.027592s |
| bit_shift64_var | strict | 23590 | 0.012240s | 4.66x | 0.127875s | 0.45x | 0.057077s |
| bit_shift64_right | strict | 1846415822 | 0.007919s | 5.35x | 0.218518s | 0.19x | 0.042392s |
| bit_not64 | strict | 1633917628 | 0.007955s | 4.41x | 0.168293s | 0.21x | 0.035064s |
| bit64_branch_test | strict | 999746 | 0.009219s | 4.30x | 0.117025s | 0.34x | 0.039681s |
| array_sum_wide | strict | 967060 | 0.005109s | 10.77x | 0.054133s | 1.02x | 0.055006s |
| array_sum_chunk | strict | 562228 | 0.013544s | 8.32x | 0.163816s | 0.69x | 0.112632s |
| array_read_seq | strict | 967060 | 0.005121s | 10.80x | 0.054099s | 1.02x | 0.055305s |
| array_read_stride | strict | 989020 | 0.006565s | 5.84x | 0.072206s | 0.53x | 0.038370s |
| array_write_seq | strict | 500500 | 0.001496s | 6.25x | 0.006894s | 1.36x | 0.009346s |
| table_new_small | strict | 50000 | 0.009663s | 4.67x | 0.023913s | 1.89x | 0.045150s |
| hash_lookup | strict | 144320 | 0.010447s | 12.05x | 0.127525s | 0.99x | 0.125863s |
| hash_update | strict | 113833 | 0.006968s | 3.63x | 0.028868s | 0.88x | 0.025260s |
| table_int64_key_lookup | strict | 164192 | 0.005857s | 3.26x | 0.003186s | 6.00x | 0.019120s |
| table_int64_key_insert | strict | 461700 | 0.004606s | 1.50x | 0.009089s | 0.76x | 0.006914s |
| table_int64_key_update | strict | 64624 | 0.003669s | 3.60x | 0.000020s | 660.65x | 0.013213s |
| table_int64_key_miss | strict | 2500000 | 0.002789s | 7.07x | 0.037294s | 0.53x | 0.019711s |
| table_int64_key_delete | strict | 363200 | 0.004566s | 1.77x | 0.012887s | 0.63x | 0.008073s |
| table_float_key_lookup | strict | 599920 | 0.005757s | 8.45x | 0.094430s | 0.52x | 0.048637s |
| table_mixed_key_lookup | strict | 551488 | 0.005891s | 5.58x | 0.037497s | 0.88x | 0.032886s |
| table_next_dense | strict | 360000 | 0.045690s | 2.08x | 0.045669s | 2.08x | 0.094819s |
| table_next_hash | strict | 248000 | 0.041743s | 3.17x | 0.071140s | 1.86x | 0.132485s |
| rawget_rawset | strict | 900000 | 0.020433s | 2.14x | 0.038865s | 1.12x | 0.043643s |
| ipairs_iter | strict | 40000 | 0.005649s | 23.93x | 0.066697s | 2.03x | 0.135179s |
| pairs_iter | strict | 40000 | 0.054292s | 3.25x | 0.092026s | 1.91x | 0.176221s |
| function_calls | strict | 500000 | 0.009207s | 25.91x | 0.302470s | 0.79x | 0.238510s |
| closure_alloc | strict | 350000 | 0.020979s | 1.60x | 0.020471s | 1.64x | 0.033520s |
| pcall_success | strict | 375000 | 0.000749s | 7.77x | 0.002445s | 2.38x | 0.005818s |
| table_sort_int | parity | 99050 | 0.003886s | 9.37x | 0.007347s | 4.96x | 0.036406s |
| table_sort_cmp | parity | 65240 | 0.014149s | 1.44x | 0.018418s | 1.11x | 0.020440s |
| table_sort_strings | parity | 735 | 0.006559s | 2.04x | 0.008094s | 1.65x | 0.013348s |
| table_sort_reverse | parity | 285040 | 0.001495s | 17.81x | 0.002571s | 10.36x | 0.026629s |
| table_sort_int64 | parity | 358755 | 0.000577s | 6.26x | 0.001121s | 3.22x | 0.003611s |
| table_sort_floats | parity | 9290 | 0.000750s | 9.44x | 0.001675s | 4.23x | 0.007080s |
| string_concat | strict | 199680 | 0.008473s | 0.92x | 0.008793s | 0.89x | 0.007817s |
| table_concat | parity | 1524000 | 0.001274s | 2.72x | 0.001401s | 2.47x | 0.003461s |
| table_move_copy | parity | 710000 | 0.038194s | 1.40x | 0.039552s | 1.35x | 0.053359s |
| table_move_large_copy | parity | 343000 | 0.058851s | 1.28x | 0.059043s | 1.27x | 0.075189s |
| table_move_self_nooverlap | parity | 390000 | 0.001456s | 28.46x | 0.002177s | 19.04x | 0.041440s |
| table_pack_loop | parity | 325000 | 0.014488s | 2.45x | 0.015652s | 2.26x | 0.035442s |
| table_remove_front | parity | 948800 | 0.012396s | 9.65x | 0.014886s | 8.03x | 0.119580s |
| table_insert_tail | parity | 430800 | 0.001444s | 9.24x | 0.013116s | 1.02x | 0.013346s |
| table_remove_tail | parity | 172800 | 0.012418s | 1.30x | 0.016489s | 0.98x | 0.016183s |
| table_insert_front | parity | 474000 | 0.007283s | 6.41x | 0.008904s | 5.24x | 0.046648s |
| table_remove_middle | parity | 132800 | 0.005220s | 6.97x | 0.007071s | 5.15x | 0.036382s |
| table_move_overlap_forward | parity | 900000 | 0.002314s | 27.13x | 0.003238s | 19.39x | 0.062781s |
| table_move_overlap_backward | parity | 900000 | 0.002356s | 26.61x | 0.003067s | 20.44x | 0.062685s |
| table_string_key_insert | strict | 756600 | 0.005319s | 4.16x | 0.008074s | 2.74x | 0.022103s |
| table_concat_numbers | parity | 1638000 | 0.001270s | 16.32x | 0.001680s | 12.33x | 0.020722s |
| string_find_plain | parity | 520000 | 0.000040s | 92.67x | 0.003381s | 1.10x | 0.003707s |
| string_find_init | parity | 147568 | 0.001675s | 4.32x | 0.006334s | 1.14x | 0.007237s |
| string_find_pattern | parity | 520000 | 0.004635s | 1.41x | 0.005334s | 1.22x | 0.006533s |
| string_find_class | parity | 240000 | 0.006827s | 1.31x | 0.007544s | 1.19x | 0.008968s |
| string_gsub | parity | 23520000 | 0.021286s | 3.78x | 0.021181s | 3.80x | 0.080401s |
| string_gsub_capture | parity | 6048000 | 0.005805s | 6.98x | 0.006515s | 6.22x | 0.040518s |
| string_gsub_func | parity | 9600000 | 0.006705s | 18.68x | 0.007354s | 17.03x | 0.125224s |
| string_gsub_table | parity | 11200000 | 0.005041s | 21.90x | 0.005152s | 21.43x | 0.110390s |
| string_gsub_no_match | parity | 28800000 | 0.000591s | 144.67x | 0.000707s | 120.94x | 0.085502s |
| string_lower_upper | parity | 20160000 | 0.006351s | 2.39x | 0.006669s | 2.28x | 0.015182s |
| string_reverse_loop | parity | 16800000 | 0.000905s | 5.44x | 0.000991s | 4.97x | 0.004925s |
| string_rep_loop | parity | 8249776 | 0.005282s | 3.02x | 0.010962s | 1.46x | 0.015952s |
| string_char_loop | parity | 1500000 | 0.007654s | 1.87x | 0.018800s | 0.76x | 0.014317s |
| string_byte_range | parity | 800000 | 0.003130s | 21.22x | 0.047718s | 1.39x | 0.066419s |
| string_gmatch_captures | parity | 3600000 | 0.034758s | 1.20x | 0.033991s | 1.23x | 0.041703s |
| string_match_repeated | parity | 1170000 | 0.009590s | 1.23x | 0.010511s | 1.12x | 0.011769s |
| string_format_int64 | parity | 1830100 | 0.006059s | 2.43x | 0.005444s | 2.70x | 0.014723s |
| string_format_float | parity | 1430371 | 0.008329s | 2.55x | 0.012330s | 1.72x | 0.021259s |
| string_format_wide_hex | parity | 1460100 | 0.002702s | 4.96x | 0.004653s | 2.88x | 0.013409s |
| string_pack_unpack_mixed | parity | 171822 | 0.015335s | 1.34x | 0.022044s | 0.93x | 0.020514s |
| string_pack_unpack_many_i8 | parity | 285000 | 0.005248s | 2.41x | 0.007655s | 1.65x | 0.012664s |
| math_sin | parity | -0.116934 | 0.004651s | 6.06x | 0.020018s | 1.41x | 0.028175s |
| math_floor | parity | 125000 | 0.001537s | 25.98x | 0.039525s | 1.01x | 0.039934s |
| math_abs_int64 | parity | 500000 | 0.027680s | 1.57x | 0.108969s | 0.40x | 0.043558s |
| math_fmod_int64 | parity | 992779 | 0.017683s | 2.65x | 0.059574s | 0.79x | 0.046912s |
| math_sqrt_log | parity | 580649595.776236 | 0.001394s | 15.34x | 0.017892s | 1.20x | 0.021389s |
| math_type_loop | strict | 2500000 | 0.026390s | 1.37x | 0.028934s | 1.25x | 0.036282s |
| math_tointeger_loop | strict | 250000 | 0.040713s | 1.36x | 0.054041s | 1.03x | 0.055432s |
| math_ult_loop | strict | 0 | 0.003042s | 29.30x | 0.059042s | 1.51x | 0.089131s |
| math_minmax_int64 | strict | 257567 | 0.009870s | 6.80x | 0.094493s | 0.71x | 0.067103s |
| math_minmax_mixed_int64 | strict | 79040 | 0.007459s | 6.56x | 0.067806s | 0.72x | 0.048909s |
| coroutine_resume | strict | 250000 | 0.012851s | 1.12x | 0.019427s | 0.74x | 0.014455s |
| coroutine_resume_args | strict | 269999 | 0.009294s | 1.16x | 0.014213s | 0.76x | 0.010793s |
| coroutine_yield_multi | strict | 360000 | 0.010064s | 1.12x | 0.014576s | 0.77x | 0.011227s |
| coroutine_wrap_loop | strict | 220000 | 0.004965s | 2.33x | 0.004951s | 2.34x | 0.011588s |
| numeric_for_down | strict | 999827 | 0.007900s | 5.04x | 0.000000s | 39848000.00x | 0.039848s |
| branch_mod_loop | strict | 0 | 0.041212s | 2.26x | 0.012818s | 7.27x | 0.093156s |
| bit_rotate64 | strict | 1394249097 | 0.025495s | 3.53x | 0.276710s | 0.33x | 0.090121s |
| table_len_loop | strict | 0 | 0.006704s | 15.10x | 0.101935s | 0.99x | 0.101218s |
| table_insert_remove | parity | 360000 | 0.036332s | 2.34x | 0.068895s | 1.23x | 0.084850s |
| table_unpack_multi | parity | 0 | 0.014633s | 1.97x | 0.020254s | 1.42x | 0.028823s |
| string_byte_sum | parity | 200000 | 0.008430s | 26.89x | 0.216890s | 1.05x | 0.226711s |
| string_sub_loop | parity | 5600000 | 0.000635s | 26.28x | 0.013441s | 1.24x | 0.016685s |
| string_format_num | parity | 1688895 | 0.003330s | 5.52x | 0.005644s | 3.26x | 0.018384s |
| string_match_pattern | parity | 1080000 | 0.015497s | 1.26x | 0.017060s | 1.15x | 0.019577s |
| string_pack_unpack_i4 | parity | 500000 | 0.013965s | 1.96x | 0.016018s | 1.71x | 0.027316s |
| string_pack_unpack_i8 | parity | 770000 | 0.008742s | 1.89x | 0.013051s | 1.26x | 0.016490s |
| tonumber_parse | parity | 839824 | 0.023794s | 1.87x | 0.041870s | 1.06x | 0.044460s |
| tonumber_base16 | parity | 900816 | 0.004635s | 4.42x | 0.008156s | 2.51x | 0.020509s |
| tostring_int | parity | 7288896 | 0.081640s | 1.36x | 0.056636s | 1.96x | 0.110798s |
| tostring_float | parity | 4388895 | 0.032343s | 2.67x | 0.030212s | 2.86x | 0.086259s |
| math_min_max | strict | 970055 | 0.012535s | 23.52x | 0.267486s | 1.10x | 0.294770s |
| closure_upvalue_update | strict | 250000 | 0.009860s | 2.86x | 0.029541s | 0.96x | 0.028225s |
| upvalue_read_loop | strict | 0 | 0.001753s | 36.11x | 0.052926s | 1.20x | 0.063299s |
| method_call | strict | 250000 | 0.009811s | 2.87x | 0.033187s | 0.85x | 0.028194s |
| select_vararg | strict | 900000 | 0.005412s | 12.08x | 0.070901s | 0.92x | 0.065350s |
| vararg_sum | strict | 450000 | 0.015794s | 6.18x | 0.102097s | 0.96x | 0.097673s |
| tail_call_pair | strict | 500000 | 0.001521s | 56.11x | 0.054081s | 1.58x | 0.085350s |
| metamethod_index | strict | 400000 | 0.000263s | 55.81x | 0.016023s | 0.92x | 0.014679s |
| metamethod_index_table | strict | 600000 | 0.000539s | 34.47x | 0.026698s | 0.70x | 0.018579s |
| metamethod_add | strict | 250000 | 0.000177s | 48.56x | 0.009035s | 0.95x | 0.008595s |
| metamethod_call | strict | 50000 | 0.000161s | 55.53x | 0.007291s | 1.23x | 0.008941s |
| metamethod_len | strict | 400000 | 0.000261s | 48.74x | 0.012918s | 0.98x | 0.012720s |
| metamethod_newindex | strict | 400000 | 0.002584s | 4.05x | 0.011337s | 0.92x | 0.010454s |
| metamethod_compare | strict | 600000 | 0.000401s | 107.02x | 0.035226s | 1.22x | 0.042917s |
| metamethod_eq_false | strict | 900000 | 0.000195s | 79.80x | 0.012997s | 1.20x | 0.015561s |
| metamethod_concat | strict | 360000 | 0.007105s | 1.28x | 0.008419s | 1.08x | 0.009124s |
| global_read_loop | strict | 0 | 0.004360s | 13.48x | 0.064605s | 0.91x | 0.058794s |
| global_write_loop | strict | 0 | 0.001086s | 9.02x | 0.013191s | 0.74x | 0.009794s |
| table_field_read_loop | strict | 0 | 0.013570s | 12.57x | 0.174270s | 0.98x | 0.170594s |
| table_field_write_loop | strict | 0 | 0.015681s | 1.93x | 0.029349s | 1.03x | 0.030287s |
| table_array_append_direct | strict | 382400 | 0.001389s | 4.97x | 0.011536s | 0.60x | 0.006906s |
| table_array_pop_direct | strict | 894400 | 0.009107s | 1.74x | 0.017748s | 0.89x | 0.015857s |
| table_unpack_32 | parity | 200000 | 0.014279s | 4.45x | 0.039181s | 1.62x | 0.063606s |
| table_concat_slice | parity | 1072000 | 0.000967s | 3.05x | 0.001282s | 2.30x | 0.002948s |
| table_sort_nearly_sorted | parity | 280070 | 0.001067s | 17.96x | 0.001596s | 12.01x | 0.019166s |
| table_sort_many_tiny | parity | 279345 | 0.000197s | 6.71x | 0.000627s | 2.11x | 0.001321s |
| table_sort_cmp_int64 | parity | 359520 | 0.006080s | 1.39x | 0.007772s | 1.09x | 0.008457s |
| string_len_loop | parity | 0 | 0.004361s | 14.20x | 0.054487s | 1.14x | 0.061922s |
| string_byte_single | parity | 647155 | 0.004621s | 19.65x | 0.092148s | 0.99x | 0.090783s |
| string_sub_negative | parity | 5950000 | 0.000548s | 31.19x | 0.017523s | 0.98x | 0.017090s |
| string_compare_loop | parity | 0 | 0.033343s | 1.76x | 0.066412s | 0.89x | 0.058837s |
| string_intern_concat | strict | 2270694 | 0.005050s | 4.38x | 0.007874s | 2.81x | 0.022123s |
| string_format_quote | parity | 938894 | 0.001305s | 4.82x | 0.002433s | 2.58x | 0.006286s |
| tonumber_float_parse | parity | 986882480.496089 | 0.011983s | 2.30x | 0.018545s | 1.49x | 0.027559s |
| tonumber_int64_parse | parity | 600000 | 0.005896s | 1.63x | 0.013043s | 0.74x | 0.009613s |
| math_abs_small | strict | 0 | 0.058162s | 4.21x | 0.189214s | 1.29x | 0.244611s |
| math_floor_negative | parity | 357142 | 0.002011s | 17.57x | 0.036787s | 0.96x | 0.035326s |
| math_type_mixed | strict | 0 | 0.023576s | 1.48x | 0.033806s | 1.03x | 0.034780s |
| float_idiv_mod | strict | 999985 | 0.042835s | 1.79x | 0.046982s | 1.63x | 0.076762s |
| bit_mixed_const32 | strict | 332866815 | 0.067189s | 1.67x | 0.365720s | 0.31x | 0.111908s |
| bit_chain64_mixed | strict | 377178668 | 0.007353s | 6.77x | 0.236023s | 0.21x | 0.049751s |
| bit_shift_signed64 | strict | 603754476 | 0.005568s | 6.15x | 0.172837s | 0.20x | 0.034269s |
| int64_unary_minus | strict | 0 | 0.015918s | 2.30x | 0.000001s | 36618.00x | 0.036618s |
| int64_compare_zero_branch | strict | 0 | 0.046921s | 1.15x | 0.142504s | 0.38x | 0.053845s |
| int64_add_compare_loop | strict | 900011 | 0.016961s | 2.24x | 0.000000s | 38060000.00x | 0.038060s |
| numeric_for_float_span | strict | 999278 | 0.087823s | 1.19x | 0.031540s | 3.32x | 0.104636s |
| while_loop_countdown | strict | 998963 | 0.018020s | 5.76x | 0.000000s | 103874000.00x | 0.103874s |
| repeat_until_countdown | strict | 998963 | 0.017562s | 5.81x | 0.000000s | 102024000.00x | 0.102024s |
| generic_for_iter_closure | strict | 0 | 0.002775s | 26.07x | 0.114033s | 0.63x | 0.072357s |
| pairs_update_values | strict | 374000 | 0.048628s | 2.42x | 0.095306s | 1.23x | 0.117663s |
| closure_factory | strict | 600000 | 0.011911s | 1.70x | 0.012353s | 1.64x | 0.020264s |
| nested_closure_call | strict | 0 | 0.000436s | 43.85x | 0.020162s | 0.95x | 0.019119s |
| tail_call_chain | strict | 500000 | 0.001095s | 97.02x | 0.052136s | 2.04x | 0.106239s |
| vararg_select_tail | strict | 500000 | 0.005860s | 4.58x | 0.031473s | 0.85x | 0.026827s |
| metamethod_bitwise | strict | 834592 | 0.001145s | 3.49x | 0.005232s | 0.76x | 0.003995s |
| metamethod_idiv | strict | 127156 | 0.001303s | 2.69x | 0.006880s | 0.51x | 0.003509s |
| metamethod_pairs_loop | strict | 700000 | 0.017932s | 2.10x | 0.018094s | 2.08x | 0.037587s |
| load_string_loop | parity | 199148 | 0.005895s | 1.68x | 0.006290s | 1.58x | 0.009907s |
| debug_getinfo_loop | parity | 50000 | 0.007634s | 2.25x | 0.007944s | 2.16x | 0.017152s |
| gc_table_alloc_collect | parity | 332325 | 0.116567s | 0.36x | 0.116605s | 0.36x | 0.041975s |
| gc_string_churn | parity | 1350000 | 0.003818s | 2.21x | 0.005415s | 1.56x | 0.008422s |
| pcall_error | strict | 120000 | 0.002302s | 4.13x | 0.002635s | 3.61x | 0.009507s |
| xpcall_error | strict | 80000 | 0.003727s | 1.94x | 0.006826s | 1.06x | 0.007225s |
| pcall_multi_return | strict | 900000 | 0.001028s | 5.60x | 0.003261s | 1.77x | 0.005759s |
| pcall_vararg_success | strict | 90000 | 0.000667s | 9.16x | 0.004776s | 1.28x | 0.006111s |
| coroutine_create_resume | strict | 120000 | 0.009982s | 0.96x | 0.010232s | 0.94x | 0.009596s |
| string_gmatch_words | parity | 2800000 | 0.013711s | 1.49x | 0.014181s | 1.44x | 0.020406s |
| utf8_codes_loop | parity | 760000 | 0.020827s | 1.14x | 0.020507s | 1.15x | 0.023646s |
| utf8_codes_lax_loop | parity | 760000 | 0.020837s | 1.15x | 0.020903s | 1.14x | 0.023889s |
| utf8_len_loop | parity | 5760000 | 0.008074s | 1.31x | 0.008926s | 1.18x | 0.010572s |
| utf8_len_ascii | parity | 34560000 | 0.010895s | 2.48x | 0.012092s | 2.23x | 0.027001s |
| utf8_codepoint_loop | parity | 1200000 | 0.002535s | 1.15x | 0.002578s | 1.13x | 0.002907s |
| utf8_char_build | parity | 1600000 | 0.007851s | 2.16x | 0.009569s | 1.77x | 0.016942s |
| utf8_offset_loop | parity | 509980 | 0.004003s | 1.25x | 0.005938s | 0.84x | 0.005009s |
| int64_add_const_box | strict | 627776 | 0.006289s | 4.56x | 0.027440s | 1.05x | 0.028704s |
| int64_sub_zero_compare | strict | 0 | 0.002172s | 13.61x | 0.000000s | 29571000.00x | 0.029571s |
| int64_mul_const_wrap | strict | 706027200 | 0.012008s | 3.20x | 0.055755s | 0.69x | 0.038390s |
| idiv_dynamic | strict | 90327 | 0.017719s | 2.89x | 0.008139s | 6.29x | 0.051187s |
| mod_dynamic | strict | 936739 | 0.014218s | 4.38x | 0.008246s | 7.55x | 0.062240s |
| numeric_for_int64_down | strict | 600376 | 0.010744s | 1.79x | 0.000000s | 19234000.00x | 0.019234s |
| numeric_for_int64_step3 | strict | 297 | 0.010939s | 1.76x | 0.000000s | 19304000.00x | 0.019304s |
| bit_and64_power2_mask | strict | 1081168960 | 0.014099s | 2.30x | 0.062694s | 0.52x | 0.032382s |
| bit_or64_small_const | strict | 1087329633 | 0.016137s | 2.15x | 0.007852s | 4.43x | 0.034749s |
| bit_xor64_small_const | strict | 9000000 | 0.013117s | 1.60x | 0.000000s | 20955000.00x | 0.020955s |
| bit_shift64_large_count | strict | 7000000 | 0.004595s | 10.25x | 0.132046s | 0.36x | 0.047120s |
| bit_shift64_neg_count | strict | 26591 | 0.005689s | 6.07x | 0.004622s | 7.48x | 0.034552s |
| bit_branch32_test | strict | 804674 | 0.009193s | 8.11x | 0.005445s | 13.69x | 0.074554s |
| table_rawget_int64_key | strict | 114272 | 0.028904s | 1.13x | 0.056308s | 0.58x | 0.032592s |
| table_rawset_int64_key | strict | 350000 | 0.009743s | 1.92x | 0.021528s | 0.87x | 0.018702s |
| table_set_int64_float_equiv | strict | 90000 | 0.003765s | 2.82x | 0.010424s | 1.02x | 0.010626s |
| table_string_key_miss | strict | 700000 | 0.005781s | 6.97x | 0.015464s | 2.61x | 0.040294s |
| table_sort_duplicates | parity | 1540 | 0.003096s | 7.97x | 0.005252s | 4.70x | 0.024683s |
| table_sort_cmp_strings | parity | 630 | 0.008386s | 1.52x | 0.010721s | 1.19x | 0.012737s |
| table_concat_large | parity | 5116000 | 0.002788s | 3.25x | 0.003252s | 2.78x | 0.009048s |
| table_unpack_slice_16 | parity | 800000 | 0.014636s | 3.52x | 0.037948s | 1.36x | 0.051559s |
| string_find_absent_plain | parity | 180000 | 0.000040s | 159.37x | 0.006227s | 1.02x | 0.006375s |
| string_find_long_plain | parity | 880000 | 0.000028s | 246.50x | 0.006567s | 1.05x | 0.006902s |
| string_match_no_capture | parity | 1260000 | 0.011630s | 1.26x | 0.012626s | 1.16x | 0.014706s |
| string_gsub_literal | parity | 12600000 | 0.004160s | 9.11x | 0.004236s | 8.95x | 0.037892s |
| string_sub_large_slice | parity | 28800000 | 0.014082s | 1.14x | 0.017619s | 0.91x | 0.016086s |
| string_byte_full_short | parity | 400000 | 0.002448s | 21.38x | 0.037924s | 1.38x | 0.052327s |
| string_char_many | parity | 2400000 | 0.017979s | 1.46x | 0.035595s | 0.74x | 0.026181s |
| string_rep_sep | parity | 4410000 | 0.002955s | 3.40x | 0.008122s | 1.24x | 0.010034s |
| string_format_mixed | parity | 1536768 | 0.006787s | 2.10x | 0.006943s | 2.05x | 0.014244s |
| string_pack_unpack_float | parity | 736924 | 0.013105s | 1.32x | 0.016497s | 1.05x | 0.017308s |
| tonumber_hex64_parse | parity | 263472 | 0.003983s | 1.77x | 0.009255s | 0.76x | 0.007048s |
| math_modf_loop | parity | 750042 | 0.007891s | 7.70x | 0.073325s | 0.83x | 0.060794s |
| math_floor_int_passthrough | parity | 0 | 0.002176s | 45.73x | 0.073497s | 1.35x | 0.099500s |
| math_tointeger_int64_string | strict | 46640 | 0.004751s | 1.62x | 0.010304s | 0.74x | 0.007673s |
| metamethod_order_int64 | strict | 800000 | 0.000305s | 72.49x | 0.019610s | 1.13x | 0.022110s |
| pcall_deep_stack | strict | 240000 | 0.000461s | 12.44x | 0.002388s | 2.40x | 0.005734s |
| coroutine_pingpong | strict | 240001 | 0.008623s | 1.17x | 0.013079s | 0.77x | 0.010070s |
| debug_getlocal_loop | parity | 40001 | 0.001184s | 0.86x | 0.001531s | 0.66x | 0.001016s |
| gc_table_churn_no_collect | parity | 478400 | 0.000405s | 60.67x | 0.012161s | 2.02x | 0.024572s |
| table_next_array_extra | strict | 0 | 0.264612s | 1.56x | 0.252459s | 1.64x | 0.413170s |
| table_next_sparse_extra | strict | 600000 | 0.389947s | 0.98x | 0.377920s | 1.01x | 0.382962s |
| table_pairs_array_extra | strict | 0 | 0.127688s | 2.14x | 0.128754s | 2.12x | 0.272968s |
| table_sort_cmp_upvalue_extra | parity | 949178 | 0.002785s | 12.55x | 0.004547s | 7.69x | 0.034955s |
| table_sort_records_extra | parity | 25802 | 0.007945s | 1.78x | 0.012639s | 1.12x | 0.014156s |
| table_sort_large_int_extra | parity | 815810 | 0.003045s | 5.11x | 0.004264s | 3.65x | 0.015554s |
| string_concat_small_extra | strict | 2160000 | 0.047213s | 1.48x | 0.065515s | 1.07x | 0.069990s |
| string_sub_tiny_extra | parity | 1200000 | 0.000927s | 29.86x | 0.023128s | 1.20x | 0.027683s |
| string_match_captures_extra | parity | 1800000 | 0.011987s | 1.24x | 0.012967s | 1.15x | 0.014885s |
| string_arith_int64_extra | strict | 885000 | 0.000667s | 3.02x | 0.001904s | 1.06x | 0.002014s |
| bit64_and_shift_extra | strict | 2019367627 | 0.012647s | 3.30x | 0.196202s | 0.21x | 0.041740s |
| bit64_branch_not_extra | strict | 0 | 0.004775s | 7.84x | 0.004586s | 8.16x | 0.037435s |
| int64_compare_threshold_extra | strict | 995968 | 0.005517s | 4.19x | 0.002036s | 11.34x | 0.023092s |
| int64_minmax_chain_extra | strict | 0 | 0.012104s | 6.45x | 0.114758s | 0.68x | 0.078122s |
| int64_compare_eq_const_extra | strict | 41016 | 0.003183s | 8.53x | 0.002060s | 13.18x | 0.027153s |
| int64_compare_between_extra | strict | 12 | 0.005932s | 3.20x | 0.001834s | 10.35x | 0.018973s |
| int64_compare_descending_extra | strict | 4032 | 0.005937s | 3.87x | 0.002037s | 11.27x | 0.022952s |
| bit64_const_mask_chain_extra | strict | 65477308 | 0.012671s | 2.18x | 0.150320s | 0.18x | 0.027586s |
| bit64_const_or_xor_chain_extra | strict | 7000001 | 0.011651s | 1.76x | 0.109746s | 0.19x | 0.020466s |
| bit64_compare_mask_zero_extra | strict | 109564 | 0.007505s | 2.87x | 0.052066s | 0.41x | 0.021561s |
| table_next_mixed_extra | strict | 744000 | 0.163403s | 1.47x | 0.156753s | 1.53x | 0.240084s |
| table_pairs_sparse_extra | strict | 600000 | 0.288252s | 1.04x | 0.236716s | 1.27x | 0.300387s |
| table_pairs_int64_keys_extra | strict | 520000 | 0.117195s | 1.57x | 0.204611s | 0.90x | 0.183801s |
| load_dump_many_protos_extra | parity | 54000 | 0.000198s | 4.45x | 0.000238s | 3.70x | 0.000881s |
| debug_getupvalue_extra | parity | 80000 | 0.001470s | 1.21x | 0.001968s | 0.90x | 0.001778s |
| debug_setupvalue_extra | parity | 467080 | 0.001477s | 1.34x | 0.002231s | 0.88x | 0.001972s |
| closure_call_mixed_extra | strict | 910272 | 0.001086s | 17.28x | 0.016426s | 1.14x | 0.018761s |
| load_dump_extra | parity | 762500 | 0.000340s | 5.83x | 0.000348s | 5.70x | 0.001983s |
| bit64_rotate_var_extra | strict | 1807377590 | 0.009597s | 4.10x | 0.099669s | 0.39x | 0.039352s |
| bit64_extract_insert_extra | strict | 536706047 | 0.009059s | 5.96x | 0.039699s | 1.36x | 0.053994s |
| bit32_extract_branch_extra | strict | 154058 | 0.050749s | 3.59x | 0.127709s | 1.43x | 0.182362s |
| bit64_math_ult_branch_extra | strict | 999988 | 0.012378s | 7.21x | 0.151694s | 0.59x | 0.089219s |
| int64_compare_float_threshold_extra | strict | 997120 | 0.011736s | 1.50x | 0.001456s | 12.07x | 0.017577s |
| int64_compare_table_bound_extra | strict | 328 | 0.006679s | 3.13x | 0.032260s | 0.65x | 0.020876s |
| numeric_for_int64_large_step_extra | strict | 927776 | 0.015520s | 1.05x | 0.023552s | 0.69x | 0.016298s |
| table_next_int64_keys_extra | strict | 520000 | 0.189142s | 1.28x | 0.254067s | 0.96x | 0.242981s |
| table_pairs_string_keys_extra | strict | 568000 | 0.138571s | 1.94x | 0.230092s | 1.17x | 0.268176s |
| table_pairs_mixed_sparse_extra | strict | 704000 | 0.353724s | 1.01x | 0.312596s | 1.14x | 0.355601s |
| table_sort_tiny_cmp_extra | parity | 126926 | 0.003881s | 1.47x | 0.007361s | 0.78x | 0.005720s |
| table_sort_cmp_desc_extra | parity | 870240 | 0.001418s | 32.15x | 0.003826s | 11.91x | 0.045584s |
| table_sort_large_strings_extra | parity | 504 | 0.002377s | 3.23x | 0.003659s | 2.10x | 0.007667s |
| table_sort_wide_records_extra | parity | 35772 | 0.007360s | 1.56x | 0.011573s | 1.00x | 0.011518s |
| table_sort_equal_keys_extra | parity | 62857 | 0.014647s | 1.59x | 0.022941s | 1.02x | 0.023294s |
| table_move_int64_extra | parity | 625000 | 0.063250s | 1.21x | 0.063159s | 1.21x | 0.076437s |
| string_find_frontier_extra | parity | 2520000 | 0.007250s | 1.27x | 0.007897s | 1.17x | 0.009203s |
| string_gsub_many_repl_extra | parity | 672000 | 0.003641s | 1.49x | 0.004225s | 1.28x | 0.005416s |
| string_format_int64_mix_extra | parity | 2730000 | 0.002235s | 6.47x | 0.005447s | 2.65x | 0.014460s |
| gc_closure_alloc_collect_extra | parity | 103040 | 0.087511s | 0.35x | 0.086612s | 0.36x | 0.030805s |
