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
| luajit_jit_on | 6.278603s | 2.41x | 3.81x | 8 | 3 |
| luajit_jit_off | 13.401729s | 1.13x | 8.59x | 81 | 13 |
| lua5.4.8 | 15.152180s | 1.00x | 1.00x | 0 | 0 |

## Key Findings

- JIT-on is faster on 172/180 strict workloads; the strict slower cases are table_int64_key_insert 0.78x (0.009092s vs 0.007136s), metamethod_bitwise 0.83x (0.004895s vs 0.004080s), table_int64_key_delete 0.84x (0.009850s vs 0.008293s), string_concat 0.85x (0.009384s vs 0.007932s), coroutine_create_resume 0.85x (0.011730s vs 0.009947s), int64_add_sub 0.86x (0.154051s vs 0.131724s), table_pairs_mixed_sparse_extra 0.99x (0.365479s vs 0.361907s), table_next_sparse_extra 0.99x (0.399715s vs 0.397191s).
- JIT-off is slower on 81/180 strict workloads; the worst confirmed cases are bit64_and_shift_extra 0.45x (0.092780s vs 0.041391s), table_float_key_lookup 0.49x (0.100446s vs 0.049133s), table_int64_key_miss 0.49x (0.041498s vs 0.020416s), bit_xor32 0.49x (0.181853s vs 0.089694s), array_read_stride 0.50x (0.079129s vs 0.039230s), bit_shift64_const 0.50x (0.060319s vs 0.030202s), metamethod_idiv 0.52x (0.006867s vs 0.003598s), idiv_power2 0.54x (0.200597s vs 0.107815s).
- C-similar parity workloads are allowed to be near parity, but below 0.80x is treated as too slow: JIT-on 3 case(s): gc_closure_alloc_collect_extra 0.35x (0.092966s vs 0.032091s), gc_table_alloc_collect 0.35x (0.126186s vs 0.043944s), debug_getlocal_loop 0.69x (0.001485s vs 0.001026s); JIT-off 13 case(s): math_abs_int64 0.35x (0.127555s vs 0.044139s), gc_closure_alloc_collect_extra 0.35x (0.091947s vs 0.032091s), gc_table_alloc_collect 0.35x (0.123954s vs 0.043944s), debug_getlocal_loop 0.56x (0.001843s vs 0.001026s), string_compare_loop 0.66x (0.078602s vs 0.051795s), string_char_many 0.67x (0.040738s vs 0.027421s), tonumber_int64_parse 0.70x (0.014130s vs 0.009869s), tonumber_hex64_parse 0.70x (0.010491s vs 0.007388s).
- Checksums matched for all modes, so the listed slowdowns are performance differences, not result mismatches.

## Strict Slower Than Lua 5.4.8

| Mode | Workload | LuaJIT time | Lua 5.4.8 time | Speedup |
| --- | --- | ---: | ---: | ---: |
| luajit_jit_off | bit64_and_shift_extra | 0.092780s | 0.041391s | 0.45x |
| luajit_jit_off | table_float_key_lookup | 0.100446s | 0.049133s | 0.49x |
| luajit_jit_off | table_int64_key_miss | 0.041498s | 0.020416s | 0.49x |
| luajit_jit_off | bit_xor32 | 0.181853s | 0.089694s | 0.49x |
| luajit_jit_off | array_read_stride | 0.079129s | 0.039230s | 0.50x |
| luajit_jit_off | bit_shift64_const | 0.060319s | 0.030202s | 0.50x |
| luajit_jit_off | metamethod_idiv | 0.006867s | 0.003598s | 0.52x |
| luajit_jit_off | idiv_power2 | 0.200597s | 0.107815s | 0.54x |
| luajit_jit_off | idiv_negative | 0.365003s | 0.197606s | 0.54x |
| luajit_jit_off | bit_or32 | 0.165914s | 0.089939s | 0.54x |
| luajit_jit_off | bit_or64 | 0.067356s | 0.036723s | 0.55x |
| luajit_jit_off | bit_and32 | 0.165750s | 0.090770s | 0.55x |
| luajit_jit_off | table_rawget_int64_key | 0.060399s | 0.033232s | 0.55x |
| luajit_jit_off | bit64_rotate_var_extra | 0.071585s | 0.040036s | 0.56x |
| luajit_jit_off | bit64_compare_mask_zero_extra | 0.038989s | 0.021807s | 0.56x |
| luajit_jit_off | bit_shift32_var | 0.289630s | 0.166565s | 0.58x |
| luajit_jit_off | bit64_branch_test | 0.068247s | 0.039978s | 0.59x |
| luajit_jit_off | bit_shift_mask | 0.334107s | 0.196355s | 0.59x |
| luajit_jit_off | bit_shift32_const | 0.316597s | 0.186423s | 0.59x |
| luajit_jit_off | int64_compare_table_bound_extra | 0.036692s | 0.021716s | 0.59x |
| luajit_jit_off | function_calls | 0.404031s | 0.242727s | 0.60x |
| luajit_jit_off | table_array_append_direct | 0.011782s | 0.007111s | 0.60x |
| luajit_jit_off | table_int64_key_delete | 0.013739s | 0.008293s | 0.60x |
| luajit_jit_off | metamethod_index_table | 0.030301s | 0.018814s | 0.62x |
| luajit_jit_off | array_sum_chunk | 0.183261s | 0.114146s | 0.62x |
| luajit_jit_off | bit64_math_ult_branch_extra | 0.144845s | 0.090831s | 0.63x |
| luajit_jit_off | bit64_const_mask_chain_extra | 0.044060s | 0.027912s | 0.63x |
| luajit_jit_off | bit_shift64_right | 0.069336s | 0.044005s | 0.63x |
| luajit_jit_off | bit_rotate64 | 0.144726s | 0.092374s | 0.64x |
| luajit_jit_off | bit_shift64_var | 0.088467s | 0.056484s | 0.64x |
| luajit_jit_off | bit_and64_power2_mask | 0.051274s | 0.033121s | 0.65x |
| luajit_jit_off | int64_minmax_chain_extra | 0.124137s | 0.080479s | 0.65x |
| luajit_jit_off | bit_and64 | 0.067933s | 0.044061s | 0.65x |
| luajit_jit_off | vararg_select_tail | 0.041047s | 0.027066s | 0.66x |
| luajit_jit_off | math_minmax_int64 | 0.101899s | 0.068045s | 0.67x |
| luajit_jit_off | math_minmax_mixed_int64 | 0.073544s | 0.049334s | 0.67x |
| luajit_jit_off | bit64_const_or_xor_chain_extra | 0.041420s | 0.027944s | 0.67x |
| luajit_jit_off | bit_shift_signed64 | 0.050478s | 0.034186s | 0.68x |
| luajit_jit_off | int64_add_sub | 0.193671s | 0.131724s | 0.68x |
| luajit_jit_off | float_arith | 0.298722s | 0.203660s | 0.68x |
| luajit_jit_off | numeric_for_int64_large_step_extra | 0.025544s | 0.017456s | 0.68x |
| luajit_jit_off | idiv_mod | 0.509856s | 0.349486s | 0.69x |
| luajit_jit_off | global_write_loop | 0.014273s | 0.010051s | 0.70x |
| luajit_jit_off | math_tointeger_int64_string | 0.011217s | 0.008104s | 0.72x |
| luajit_jit_off | bit_not64 | 0.048609s | 0.035504s | 0.73x |
| luajit_jit_off | coroutine_pingpong | 0.013743s | 0.010194s | 0.74x |
| luajit_jit_off | coroutine_yield_multi | 0.015615s | 0.011706s | 0.75x |
| luajit_jit_off | table_int64_key_insert | 0.009485s | 0.007136s | 0.75x |
| luajit_jit_off | select_vararg | 0.085380s | 0.064313s | 0.75x |
| luajit_jit_off | metamethod_bitwise | 0.005321s | 0.004080s | 0.77x |
| luajit_jit_off | table_pairs_int64_keys_extra | 0.240580s | 0.186340s | 0.77x |
| luajit_jit_off | coroutine_resume_args | 0.014578s | 0.011339s | 0.78x |
| luajit_jit_off | table_mixed_key_lookup | 0.042161s | 0.033080s | 0.78x |
| luajit_jit_on | table_int64_key_insert | 0.009092s | 0.007136s | 0.78x |
| luajit_jit_off | hash_update | 0.031856s | 0.025111s | 0.79x |
| luajit_jit_off | metamethod_index | 0.018211s | 0.014512s | 0.80x |
| luajit_jit_off | coroutine_resume | 0.019454s | 0.015542s | 0.80x |
| luajit_jit_off | method_call | 0.035560s | 0.028960s | 0.81x |
| luajit_jit_off | string_concat | 0.009530s | 0.007932s | 0.83x |
| luajit_jit_on | metamethod_bitwise | 0.004895s | 0.004080s | 0.83x |
| luajit_jit_on | table_int64_key_delete | 0.009850s | 0.008293s | 0.84x |
| luajit_jit_off | global_read_loop | 0.070912s | 0.059844s | 0.84x |
| luajit_jit_on | string_concat | 0.009384s | 0.007932s | 0.85x |
| luajit_jit_on | coroutine_create_resume | 0.011730s | 0.009947s | 0.85x |
| luajit_jit_off | vararg_sum | 0.113474s | 0.096275s | 0.85x |
| luajit_jit_on | int64_add_sub | 0.154051s | 0.131724s | 0.86x |
| luajit_jit_off | table_array_pop_direct | 0.018988s | 0.016249s | 0.86x |
| luajit_jit_off | metamethod_add | 0.010077s | 0.008639s | 0.86x |
| luajit_jit_off | nested_closure_call | 0.022744s | 0.019574s | 0.86x |
| luajit_jit_off | table_rawset_int64_key | 0.022175s | 0.019186s | 0.87x |
| luajit_jit_off | table_field_read_loop | 0.201623s | 0.176011s | 0.87x |
| luajit_jit_off | table_len_loop | 0.120162s | 0.105013s | 0.87x |
| luajit_jit_off | hash_lookup | 0.139633s | 0.122063s | 0.87x |
| luajit_jit_off | metamethod_len | 0.014885s | 0.013218s | 0.89x |
| luajit_jit_off | metamethod_newindex | 0.012045s | 0.010790s | 0.90x |
| luajit_jit_off | table_next_int64_keys_extra | 0.278023s | 0.257057s | 0.92x |
| luajit_jit_off | coroutine_create_resume | 0.010657s | 0.009947s | 0.93x |
| luajit_jit_off | table_field_write_loop | 0.031730s | 0.029989s | 0.95x |
| luajit_jit_off | int64_compare_zero_branch | 0.057721s | 0.054765s | 0.95x |
| luajit_jit_off | bitwise_idiv_unsigned | 0.109418s | 0.104264s | 0.95x |
| luajit_jit_off | array_sum_wide | 0.059304s | 0.056657s | 0.96x |
| luajit_jit_off | bit_mixed_const32 | 0.117252s | 0.113728s | 0.97x |
| luajit_jit_off | int64_add_const_box | 0.030613s | 0.029735s | 0.97x |
| luajit_jit_off | math_type_mixed | 0.037716s | 0.036651s | 0.97x |
| luajit_jit_off | float_compare_branch | 0.174784s | 0.170790s | 0.98x |
| luajit_jit_off | array_read_seq | 0.058079s | 0.056761s | 0.98x |
| luajit_jit_on | table_pairs_mixed_sparse_extra | 0.365479s | 0.361907s | 0.99x |
| luajit_jit_off | table_next_sparse_extra | 0.400058s | 0.397191s | 0.99x |
| luajit_jit_on | table_next_sparse_extra | 0.399715s | 0.397191s | 0.99x |

## Parity Workloads Below Threshold

| Mode | Workload | LuaJIT time | Lua 5.4.8 time | Speedup | Minimum |
| --- | --- | ---: | ---: | ---: | ---: |
| luajit_jit_on | gc_closure_alloc_collect_extra | 0.092966s | 0.032091s | 0.35x | 0.80x |
| luajit_jit_off | math_abs_int64 | 0.127555s | 0.044139s | 0.35x | 0.80x |
| luajit_jit_on | gc_table_alloc_collect | 0.126186s | 0.043944s | 0.35x | 0.80x |
| luajit_jit_off | gc_closure_alloc_collect_extra | 0.091947s | 0.032091s | 0.35x | 0.80x |
| luajit_jit_off | gc_table_alloc_collect | 0.123954s | 0.043944s | 0.35x | 0.80x |
| luajit_jit_off | debug_getlocal_loop | 0.001843s | 0.001026s | 0.56x | 0.80x |
| luajit_jit_off | string_compare_loop | 0.078602s | 0.051795s | 0.66x | 0.80x |
| luajit_jit_off | string_char_many | 0.040738s | 0.027421s | 0.67x | 0.80x |
| luajit_jit_on | debug_getlocal_loop | 0.001485s | 0.001026s | 0.69x | 0.80x |
| luajit_jit_off | tonumber_int64_parse | 0.014130s | 0.009869s | 0.70x | 0.80x |
| luajit_jit_off | tonumber_hex64_parse | 0.010491s | 0.007388s | 0.70x | 0.80x |
| luajit_jit_off | string_char_loop | 0.020340s | 0.014813s | 0.73x | 0.80x |
| luajit_jit_off | table_sort_tiny_cmp_extra | 0.007933s | 0.005840s | 0.74x | 0.80x |
| luajit_jit_off | math_fmod_int64 | 0.066671s | 0.049164s | 0.74x | 0.80x |
| luajit_jit_off | debug_setupvalue_extra | 0.002533s | 0.001988s | 0.78x | 0.80x |
| luajit_jit_off | utf8_offset_loop | 0.006407s | 0.005068s | 0.79x | 0.80x |

## Parity Workloads Slower But Within Threshold

| Mode | Workload | LuaJIT time | Lua 5.4.8 time | Speedup |
| --- | --- | ---: | ---: | ---: |
| luajit_jit_off | debug_getupvalue_extra | 0.002155s | 0.001765s | 0.82x |
| luajit_jit_off | string_pack_unpack_mixed | 0.025380s | 0.021102s | 0.83x |
| luajit_jit_off | string_sub_large_slice | 0.019816s | 0.016591s | 0.84x |
| luajit_jit_off | math_modf_loop | 0.076051s | 0.065594s | 0.86x |
| luajit_jit_off | string_sub_negative | 0.020139s | 0.017481s | 0.87x |
| luajit_jit_off | string_find_absent_plain | 0.006969s | 0.006508s | 0.93x |
| luajit_jit_off | string_byte_single | 0.097299s | 0.091964s | 0.95x |
| luajit_jit_off | tonumber_parse | 0.046546s | 0.045173s | 0.97x |
| luajit_jit_off | table_sort_wide_records_extra | 0.011903s | 0.011797s | 0.99x |
| luajit_jit_off | table_sort_equal_keys_extra | 0.024222s | 0.024107s | 1.00x |
| luajit_jit_off | math_floor_negative | 0.037522s | 0.037400s | 1.00x |

## Checksum/Correctness Mismatches

No checksum or per-run consistency mismatches.

## Slowdown Clusters

| Mode | Policy | Category | Slow workloads | Worst workload | Worst speedup |
| --- | --- | --- | ---: | --- | ---: |
| luajit_jit_off | parity | math library | 2 | math_abs_int64 | 0.35x |
| luajit_jit_off | parity | runtime/debug/gc | 4 | gc_closure_alloc_collect_extra | 0.35x |
| luajit_jit_off | parity | string/format/parse | 6 | string_compare_loop | 0.66x |
| luajit_jit_off | parity | table library | 1 | table_sort_tiny_cmp_extra | 0.74x |
| luajit_jit_off | strict | array/hash table | 8 | array_read_stride | 0.50x |
| luajit_jit_off | strict | bitwise/integer | 24 | bit64_and_shift_extra | 0.45x |
| luajit_jit_off | strict | call/metamethod | 13 | metamethod_idiv | 0.52x |
| luajit_jit_off | strict | integer/control | 9 | idiv_power2 | 0.54x |
| luajit_jit_off | strict | math library | 4 | math_minmax_int64 | 0.67x |
| luajit_jit_off | strict | other | 2 | float_arith | 0.68x |
| luajit_jit_off | strict | protected/coroutine | 5 | coroutine_pingpong | 0.74x |
| luajit_jit_off | strict | string/format/parse | 1 | string_concat | 0.83x |
| luajit_jit_off | strict | table library | 15 | table_float_key_lookup | 0.49x |
| luajit_jit_on | parity | runtime/debug/gc | 3 | gc_closure_alloc_collect_extra | 0.35x |
| luajit_jit_on | strict | call/metamethod | 1 | metamethod_bitwise | 0.83x |
| luajit_jit_on | strict | integer/control | 1 | int64_add_sub | 0.86x |
| luajit_jit_on | strict | protected/coroutine | 1 | coroutine_create_resume | 0.85x |
| luajit_jit_on | strict | string/format/parse | 1 | string_concat | 0.85x |
| luajit_jit_on | strict | table library | 4 | table_int64_key_insert | 0.78x |

## Full Results

| Workload | Policy | Checksum | JIT-on | On speedup | JIT-off | Off speedup | Lua 5.4.8 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| empty_loop | strict | 100000000 | 0.022212s | 13.35x | 0.252487s | 1.17x | 0.296478s |
| int_add_wide | strict | 191146379 | 0.315438s | 1.62x | 0.336217s | 1.52x | 0.509690s |
| int64_add_sub | strict | 127778 | 0.154051s | 0.86x | 0.193671s | 0.68x | 0.131724s |
| int_add32 | strict | 576647110 | 0.287156s | 2.98x | 0.653150s | 1.31x | 0.856828s |
| int_mul32 | strict | 311395809 | 0.054840s | 4.37x | 0.189838s | 1.26x | 0.239809s |
| int_mod_small | strict | 999740 | 0.157597s | 2.39x | 0.000000s | 376859000.00x | 0.376859s |
| idiv_const | strict | 0 | 0.021432s | 5.03x | 0.000000s | 107910000.00x | 0.107910s |
| idiv_negative | strict | 714300 | 0.066793s | 2.96x | 0.365003s | 0.54x | 0.197606s |
| idiv_power2 | strict | 65539 | 0.033310s | 3.24x | 0.200597s | 0.54x | 0.107815s |
| mod_const | strict | 500000 | 0.027974s | 4.92x | 0.000000s | 137718000.00x | 0.137718s |
| mod_power2 | strict | 387680 | 0.042013s | 3.27x | 0.000000s | 137533000.00x | 0.137533s |
| numeric_for_int64_span | strict | 999943 | 0.013168s | 1.78x | 0.000000s | 23455000.00x | 0.023455s |
| int_compare_branch | strict | 861515 | 0.044375s | 2.56x | 0.018555s | 6.13x | 0.113754s |
| int_compare_eq_chain | strict | 817169 | 0.037102s | 4.70x | 0.017820s | 9.78x | 0.174316s |
| int64_compare_minmax | strict | 0 | 0.031921s | 2.11x | 0.057798s | 1.17x | 0.067493s |
| int64_eq_branch | strict | 373544 | 0.010454s | 8.00x | 0.006173s | 13.54x | 0.083585s |
| int64_mod_const | strict | 992044 | 0.044364s | 1.70x | 0.000000s | 75255000.00x | 0.075255s |
| int64_idiv_const | strict | 632320 | 0.045563s | 1.04x | 0.000000s | 47532000.00x | 0.047532s |
| int64_mul_wrap | strict | 1744528928 | 0.005694s | 6.01x | 0.027467s | 1.25x | 0.034230s |
| float_compare_branch | strict | -963739252 | 0.127128s | 1.34x | 0.174784s | 0.98x | 0.170790s |
| float_arith | strict | 206120287761215.281250 | 0.152787s | 1.33x | 0.298722s | 0.68x | 0.203660s |
| pure_bit_xor | strict | 50000000 | 0.067836s | 1.33x | 0.000000s | 89961000.00x | 0.089961s |
| bit_and32 | strict | 33950784 | 0.047881s | 1.90x | 0.165750s | 0.55x | 0.090770s |
| bit_or32 | strict | 2147483647 | 0.032622s | 2.76x | 0.165914s | 0.54x | 0.089939s |
| bit_xor32 | strict | 102466744 | 0.032515s | 2.76x | 0.181853s | 0.49x | 0.089694s |
| bit_not32 | strict | 35000000 | 0.070848s | 1.11x | 0.000000s | 78648000.00x | 0.078648s |
| bit_shift32_const | strict | 300872905 | 0.054373s | 3.43x | 0.316597s | 0.59x | 0.186423s |
| bit_shift32_var | strict | 1592255438 | 0.047037s | 3.54x | 0.289630s | 0.58x | 0.166565s |
| bit_shift_mask | strict | 318080536 | 0.047356s | 4.15x | 0.334107s | 0.59x | 0.196355s |
| bitwise_idiv_unsigned | strict | 1821843616 | 0.038562s | 2.70x | 0.109418s | 0.95x | 0.104264s |
| bitwise_idiv_mix | strict | 714511537 | 0.060012s | 2.36x | 0.123128s | 1.15x | 0.141367s |
| idiv_mod | strict | 997901798 | 0.101216s | 3.45x | 0.509856s | 0.69x | 0.349486s |
| bit_shift64_const | strict | 295495246 | 0.019364s | 1.56x | 0.060319s | 0.50x | 0.030202s |
| bit_and64 | strict | 1890031 | 0.024200s | 1.82x | 0.067933s | 0.65x | 0.044061s |
| bit_and64_const | strict | 1456439740 | 0.014807s | 1.46x | 0.005525s | 3.90x | 0.021566s |
| bit_or64 | strict | 305419896 | 0.005577s | 6.58x | 0.067356s | 0.55x | 0.036723s |
| bit_or64_const | strict | 1459617791 | 0.008449s | 3.31x | 0.000000s | 27949000.00x | 0.027949s |
| bit_xor64 | strict | 1617852892 | 0.005889s | 8.31x | 0.005621s | 8.70x | 0.048911s |
| bit_xor64_const | strict | 1456439740 | 0.008476s | 4.65x | 0.000001s | 39452.00x | 0.039452s |
| bit_shift64_var | strict | 23590 | 0.013056s | 4.33x | 0.088467s | 0.64x | 0.056484s |
| bit_shift64_right | strict | 1846415822 | 0.008545s | 5.15x | 0.069336s | 0.63x | 0.044005s |
| bit_not64 | strict | 1633917628 | 0.008399s | 4.23x | 0.048609s | 0.73x | 0.035504s |
| bit64_branch_test | strict | 999746 | 0.009771s | 4.09x | 0.068247s | 0.59x | 0.039978s |
| array_sum_wide | strict | 967060 | 0.005624s | 10.07x | 0.059304s | 0.96x | 0.056657s |
| array_sum_chunk | strict | 562228 | 0.014784s | 7.72x | 0.183261s | 0.62x | 0.114146s |
| array_read_seq | strict | 967060 | 0.005444s | 10.43x | 0.058079s | 0.98x | 0.056761s |
| array_read_stride | strict | 989020 | 0.007074s | 5.55x | 0.079129s | 0.50x | 0.039230s |
| array_write_seq | strict | 500500 | 0.001454s | 6.67x | 0.007600s | 1.28x | 0.009695s |
| table_new_small | strict | 50000 | 0.011603s | 4.09x | 0.026557s | 1.79x | 0.047469s |
| hash_lookup | strict | 144320 | 0.011118s | 10.98x | 0.139633s | 0.87x | 0.122063s |
| hash_update | strict | 113833 | 0.007149s | 3.51x | 0.031856s | 0.79x | 0.025111s |
| table_int64_key_lookup | strict | 164192 | 0.006109s | 3.16x | 0.003408s | 5.67x | 0.019322s |
| table_int64_key_insert | strict | 461700 | 0.009092s | 0.78x | 0.009485s | 0.75x | 0.007136s |
| table_int64_key_update | strict | 64624 | 0.003852s | 3.37x | 0.000021s | 619.05x | 0.013000s |
| table_int64_key_miss | strict | 2500000 | 0.002920s | 6.99x | 0.041498s | 0.49x | 0.020416s |
| table_int64_key_delete | strict | 363200 | 0.009850s | 0.84x | 0.013739s | 0.60x | 0.008293s |
| table_float_key_lookup | strict | 599920 | 0.005938s | 8.27x | 0.100446s | 0.49x | 0.049133s |
| table_mixed_key_lookup | strict | 551488 | 0.006251s | 5.29x | 0.042161s | 0.78x | 0.033080s |
| table_next_dense | strict | 360000 | 0.048273s | 2.00x | 0.046602s | 2.08x | 0.096762s |
| table_next_hash | strict | 248000 | 0.047949s | 2.82x | 0.077567s | 1.74x | 0.134984s |
| rawget_rawset | strict | 900000 | 0.020444s | 2.25x | 0.038436s | 1.20x | 0.045998s |
| ipairs_iter | strict | 40000 | 0.076110s | 1.82x | 0.066700s | 2.08x | 0.138458s |
| pairs_iter | strict | 40000 | 0.058814s | 3.14x | 0.110668s | 1.67x | 0.184743s |
| function_calls | strict | 500000 | 0.009773s | 24.84x | 0.404031s | 0.60x | 0.242727s |
| closure_alloc | strict | 350000 | 0.021933s | 1.54x | 0.021527s | 1.57x | 0.033749s |
| pcall_success | strict | 375000 | 0.000798s | 7.35x | 0.002679s | 2.19x | 0.005865s |
| table_sort_int | parity | 99050 | 0.010421s | 3.66x | 0.007653s | 4.99x | 0.038159s |
| table_sort_cmp | parity | 65240 | 0.015303s | 1.40x | 0.019056s | 1.12x | 0.021417s |
| table_sort_strings | parity | 735 | 0.007322s | 1.87x | 0.008704s | 1.57x | 0.013656s |
| table_sort_reverse | parity | 285040 | 0.001559s | 18.14x | 0.002625s | 10.77x | 0.028277s |
| table_sort_int64 | parity | 358755 | 0.001280s | 3.05x | 0.001179s | 3.31x | 0.003908s |
| table_sort_floats | parity | 9290 | 0.002269s | 3.36x | 0.001733s | 4.40x | 0.007621s |
| string_concat | strict | 199680 | 0.009384s | 0.85x | 0.009530s | 0.83x | 0.007932s |
| table_concat | parity | 1524000 | 0.001040s | 3.63x | 0.001333s | 2.83x | 0.003773s |
| table_move_copy | parity | 710000 | 0.039324s | 1.39x | 0.041450s | 1.32x | 0.054661s |
| table_move_large_copy | parity | 343000 | 0.061193s | 1.26x | 0.060891s | 1.26x | 0.077017s |
| table_move_self_nooverlap | parity | 390000 | 0.001543s | 27.60x | 0.002395s | 17.78x | 0.042586s |
| table_pack_loop | parity | 325000 | 0.019784s | 1.67x | 0.017304s | 1.91x | 0.033103s |
| table_remove_front | parity | 948800 | 0.012818s | 9.40x | 0.014855s | 8.11x | 0.120465s |
| table_insert_tail | parity | 430800 | 0.001510s | 9.09x | 0.013143s | 1.04x | 0.013720s |
| table_remove_tail | parity | 172800 | 0.012589s | 1.32x | 0.016411s | 1.01x | 0.016567s |
| table_insert_front | parity | 474000 | 0.007722s | 6.07x | 0.009322s | 5.03x | 0.046869s |
| table_remove_middle | parity | 132800 | 0.005553s | 6.57x | 0.007271s | 5.02x | 0.036496s |
| table_move_overlap_forward | parity | 900000 | 0.002455s | 25.98x | 0.003634s | 17.55x | 0.063774s |
| table_move_overlap_backward | parity | 900000 | 0.002462s | 25.83x | 0.003592s | 17.70x | 0.063584s |
| table_string_key_insert | strict | 756600 | 0.005781s | 3.89x | 0.008824s | 2.55x | 0.022503s |
| table_concat_numbers | parity | 1638000 | 0.001474s | 13.83x | 0.001473s | 13.84x | 0.020383s |
| string_find_plain | parity | 520000 | 0.000042s | 93.31x | 0.003810s | 1.03x | 0.003919s |
| string_find_init | parity | 147568 | 0.001616s | 4.56x | 0.006737s | 1.09x | 0.007362s |
| string_find_pattern | parity | 520000 | 0.005229s | 1.30x | 0.005651s | 1.20x | 0.006784s |
| string_find_class | parity | 240000 | 0.007443s | 1.24x | 0.007907s | 1.17x | 0.009241s |
| string_gsub | parity | 23520000 | 0.023997s | 3.39x | 0.024462s | 3.32x | 0.081240s |
| string_gsub_capture | parity | 6048000 | 0.006242s | 6.82x | 0.006197s | 6.87x | 0.042554s |
| string_gsub_func | parity | 9600000 | 0.007632s | 15.31x | 0.007621s | 15.33x | 0.116839s |
| string_gsub_table | parity | 11200000 | 0.005568s | 20.43x | 0.005572s | 20.42x | 0.113753s |
| string_gsub_no_match | parity | 28800000 | 0.000626s | 139.35x | 0.000757s | 115.24x | 0.087235s |
| string_lower_upper | parity | 20160000 | 0.007048s | 2.22x | 0.006902s | 2.27x | 0.015637s |
| string_reverse_loop | parity | 16800000 | 0.000952s | 5.53x | 0.001041s | 5.06x | 0.005267s |
| string_rep_loop | parity | 8249776 | 0.006241s | 2.62x | 0.013124s | 1.24x | 0.016323s |
| string_char_loop | parity | 1500000 | 0.007829s | 1.89x | 0.020340s | 0.73x | 0.014813s |
| string_byte_range | parity | 800000 | 0.003359s | 20.55x | 0.057963s | 1.19x | 0.069020s |
| string_gmatch_captures | parity | 3600000 | 0.034604s | 1.22x | 0.034903s | 1.21x | 0.042070s |
| string_match_repeated | parity | 1170000 | 0.009598s | 1.30x | 0.010658s | 1.17x | 0.012512s |
| string_format_int64 | parity | 1830100 | 0.007184s | 2.13x | 0.005669s | 2.69x | 0.015277s |
| string_format_float | parity | 1430371 | 0.008967s | 2.49x | 0.013305s | 1.68x | 0.022353s |
| string_format_wide_hex | parity | 1460100 | 0.003062s | 4.54x | 0.005044s | 2.76x | 0.013907s |
| string_pack_unpack_mixed | parity | 171822 | 0.018055s | 1.17x | 0.025380s | 0.83x | 0.021102s |
| string_pack_unpack_many_i8 | parity | 285000 | 0.006906s | 1.89x | 0.008939s | 1.46x | 0.013047s |
| math_sin | parity | -0.116934 | 0.004798s | 6.02x | 0.020614s | 1.40x | 0.028905s |
| math_floor | parity | 125000 | 0.001643s | 25.76x | 0.040238s | 1.05x | 0.042326s |
| math_abs_int64 | parity | 500000 | 0.028170s | 1.57x | 0.127555s | 0.35x | 0.044139s |
| math_fmod_int64 | parity | 992779 | 0.018301s | 2.69x | 0.066671s | 0.74x | 0.049164s |
| math_sqrt_log | parity | 580649595.776236 | 0.001468s | 15.07x | 0.019464s | 1.14x | 0.022127s |
| math_type_loop | strict | 2500000 | 0.026899s | 1.42x | 0.030280s | 1.26x | 0.038239s |
| math_tointeger_loop | strict | 250000 | 0.039202s | 1.45x | 0.054063s | 1.05x | 0.056693s |
| math_ult_loop | strict | 0 | 0.003016s | 29.64x | 0.062027s | 1.44x | 0.089398s |
| math_minmax_int64 | strict | 257567 | 0.010373s | 6.56x | 0.101899s | 0.67x | 0.068045s |
| math_minmax_mixed_int64 | strict | 79040 | 0.007882s | 6.26x | 0.073544s | 0.67x | 0.049334s |
| coroutine_resume | strict | 250000 | 0.013211s | 1.18x | 0.019454s | 0.80x | 0.015542s |
| coroutine_resume_args | strict | 269999 | 0.010112s | 1.12x | 0.014578s | 0.78x | 0.011339s |
| coroutine_yield_multi | strict | 360000 | 0.010831s | 1.08x | 0.015615s | 0.75x | 0.011706s |
| coroutine_wrap_loop | strict | 220000 | 0.005087s | 2.41x | 0.005375s | 2.28x | 0.012256s |
| numeric_for_down | strict | 999827 | 0.008401s | 4.83x | 0.000000s | 40595000.00x | 0.040595s |
| branch_mod_loop | strict | 0 | 0.042613s | 2.22x | 0.013456s | 7.03x | 0.094657s |
| bit_rotate64 | strict | 1394249097 | 0.025643s | 3.60x | 0.144726s | 0.64x | 0.092374s |
| table_len_loop | strict | 0 | 0.006857s | 15.31x | 0.120162s | 0.87x | 0.105013s |
| table_insert_remove | parity | 360000 | 0.037681s | 2.32x | 0.068108s | 1.28x | 0.087423s |
| table_unpack_multi | parity | 0 | 0.014596s | 2.01x | 0.023117s | 1.27x | 0.029349s |
| string_byte_sum | parity | 200000 | 0.008496s | 26.93x | 0.219901s | 1.04x | 0.228811s |
| string_sub_loop | parity | 5600000 | 0.000642s | 26.28x | 0.014107s | 1.20x | 0.016874s |
| string_format_num | parity | 1688895 | 0.003424s | 5.28x | 0.005983s | 3.02x | 0.018095s |
| string_match_pattern | parity | 1080000 | 0.015857s | 1.28x | 0.017534s | 1.15x | 0.020218s |
| string_pack_unpack_i4 | parity | 500000 | 0.016011s | 1.74x | 0.017971s | 1.55x | 0.027811s |
| string_pack_unpack_i8 | parity | 770000 | 0.009063s | 1.96x | 0.014653s | 1.21x | 0.017719s |
| tonumber_parse | parity | 839824 | 0.024250s | 1.86x | 0.046546s | 0.97x | 0.045173s |
| tonumber_base16 | parity | 900816 | 0.004799s | 4.34x | 0.008906s | 2.34x | 0.020805s |
| tostring_int | parity | 7288896 | 0.098580s | 1.11x | 0.065020s | 1.68x | 0.109142s |
| tostring_float | parity | 4388895 | 0.037627s | 2.33x | 0.034598s | 2.53x | 0.087672s |
| math_min_max | strict | 970055 | 0.013210s | 22.48x | 0.291416s | 1.02x | 0.296964s |
| closure_upvalue_update | strict | 250000 | 0.009957s | 2.92x | 0.028991s | 1.00x | 0.029066s |
| upvalue_read_loop | strict | 0 | 0.002335s | 27.52x | 0.059964s | 1.07x | 0.064269s |
| method_call | strict | 250000 | 0.010023s | 2.89x | 0.035560s | 0.81x | 0.028960s |
| select_vararg | strict | 900000 | 0.012075s | 5.33x | 0.085380s | 0.75x | 0.064313s |
| vararg_sum | strict | 450000 | 0.016489s | 5.84x | 0.113474s | 0.85x | 0.096275s |
| tail_call_pair | strict | 500000 | 0.001622s | 53.91x | 0.059752s | 1.46x | 0.087444s |
| metamethod_index | strict | 400000 | 0.000267s | 54.35x | 0.018211s | 0.80x | 0.014512s |
| metamethod_index_table | strict | 600000 | 0.000566s | 33.24x | 0.030301s | 0.62x | 0.018814s |
| metamethod_add | strict | 250000 | 0.000171s | 50.52x | 0.010077s | 0.86x | 0.008639s |
| metamethod_call | strict | 50000 | 0.000165s | 52.42x | 0.007906s | 1.09x | 0.008649s |
| metamethod_len | strict | 400000 | 0.000271s | 48.77x | 0.014885s | 0.89x | 0.013218s |
| metamethod_newindex | strict | 400000 | 0.002660s | 4.06x | 0.012045s | 0.90x | 0.010790s |
| metamethod_compare | strict | 600000 | 0.000420s | 97.52x | 0.037002s | 1.11x | 0.040960s |
| metamethod_eq_false | strict | 900000 | 0.000210s | 75.56x | 0.013898s | 1.14x | 0.015868s |
| metamethod_concat | strict | 360000 | 0.007331s | 1.29x | 0.008714s | 1.09x | 0.009491s |
| global_read_loop | strict | 0 | 0.004507s | 13.28x | 0.070912s | 0.84x | 0.059844s |
| global_write_loop | strict | 0 | 0.001162s | 8.65x | 0.014273s | 0.70x | 0.010051s |
| table_field_read_loop | strict | 0 | 0.013986s | 12.58x | 0.201623s | 0.87x | 0.176011s |
| table_field_write_loop | strict | 0 | 0.015731s | 1.91x | 0.031730s | 0.95x | 0.029989s |
| table_array_append_direct | strict | 382400 | 0.001481s | 4.80x | 0.011782s | 0.60x | 0.007111s |
| table_array_pop_direct | strict | 894400 | 0.010152s | 1.60x | 0.018988s | 0.86x | 0.016249s |
| table_unpack_32 | parity | 200000 | 0.014332s | 4.62x | 0.043966s | 1.51x | 0.066250s |
| table_concat_slice | parity | 1072000 | 0.000914s | 3.30x | 0.001260s | 2.39x | 0.003015s |
| table_sort_nearly_sorted | parity | 280070 | 0.001094s | 18.86x | 0.001732s | 11.91x | 0.020628s |
| table_sort_many_tiny | parity | 279345 | 0.000195s | 6.96x | 0.000657s | 2.07x | 0.001358s |
| table_sort_cmp_int64 | parity | 359520 | 0.006269s | 1.38x | 0.007828s | 1.11x | 0.008682s |
| string_len_loop | parity | 0 | 0.004654s | 13.44x | 0.058739s | 1.06x | 0.062540s |
| string_byte_single | parity | 647155 | 0.004750s | 19.36x | 0.097299s | 0.95x | 0.091964s |
| string_sub_negative | parity | 5950000 | 0.000575s | 30.40x | 0.020139s | 0.87x | 0.017481s |
| string_compare_loop | parity | 0 | 0.034589s | 1.50x | 0.078602s | 0.66x | 0.051795s |
| string_intern_concat | strict | 2270694 | 0.005099s | 4.33x | 0.008543s | 2.58x | 0.022062s |
| string_format_quote | parity | 938894 | 0.001323s | 4.99x | 0.002473s | 2.67x | 0.006608s |
| tonumber_float_parse | parity | 986882480.496089 | 0.013379s | 2.13x | 0.020631s | 1.38x | 0.028560s |
| tonumber_int64_parse | parity | 600000 | 0.006027s | 1.64x | 0.014130s | 0.70x | 0.009869s |
| math_abs_small | strict | 0 | 0.058808s | 4.19x | 0.233871s | 1.05x | 0.246528s |
| math_floor_negative | parity | 357142 | 0.001892s | 19.77x | 0.037522s | 1.00x | 0.037400s |
| math_type_mixed | strict | 0 | 0.023581s | 1.55x | 0.037716s | 0.97x | 0.036651s |
| float_idiv_mod | strict | 999985 | 0.046624s | 1.67x | 0.052955s | 1.47x | 0.078074s |
| bit_mixed_const32 | strict | 332866815 | 0.069202s | 1.64x | 0.117252s | 0.97x | 0.113728s |
| bit_chain64_mixed | strict | 377178668 | 0.007742s | 6.47x | 0.048576s | 1.03x | 0.050068s |
| bit_shift_signed64 | strict | 603754476 | 0.005808s | 5.89x | 0.050478s | 0.68x | 0.034186s |
| int64_unary_minus | strict | 0 | 0.016449s | 2.32x | 0.000000s | 38217000.00x | 0.038217s |
| int64_compare_zero_branch | strict | 0 | 0.048812s | 1.12x | 0.057721s | 0.95x | 0.054765s |
| int64_add_compare_loop | strict | 900011 | 0.017398s | 2.22x | 0.000000s | 38624000.00x | 0.038624s |
| numeric_for_float_span | strict | 999278 | 0.093024s | 1.11x | 0.032281s | 3.19x | 0.102823s |
| while_loop_countdown | strict | 998963 | 0.019197s | 5.51x | 0.000000s | 105720000.00x | 0.105720s |
| repeat_until_countdown | strict | 998963 | 0.019333s | 5.35x | 0.000000s | 103518000.00x | 0.103518s |
| generic_for_iter_closure | strict | 0 | 0.002898s | 25.42x | 0.070293s | 1.05x | 0.073662s |
| pairs_update_values | strict | 374000 | 0.045297s | 2.58x | 0.102902s | 1.14x | 0.116865s |
| closure_factory | strict | 600000 | 0.012640s | 1.54x | 0.012805s | 1.52x | 0.019473s |
| nested_closure_call | strict | 0 | 0.000485s | 40.36x | 0.022744s | 0.86x | 0.019574s |
| tail_call_chain | strict | 500000 | 0.001366s | 75.69x | 0.056355s | 1.83x | 0.103388s |
| vararg_select_tail | strict | 500000 | 0.009688s | 2.79x | 0.041047s | 0.66x | 0.027066s |
| metamethod_bitwise | strict | 834592 | 0.004895s | 0.83x | 0.005321s | 0.77x | 0.004080s |
| metamethod_idiv | strict | 127156 | 0.001392s | 2.58x | 0.006867s | 0.52x | 0.003598s |
| metamethod_pairs_loop | strict | 700000 | 0.018749s | 2.02x | 0.018898s | 2.00x | 0.037875s |
| load_string_loop | parity | 199148 | 0.006379s | 1.62x | 0.006727s | 1.54x | 0.010346s |
| debug_getinfo_loop | parity | 50000 | 0.008492s | 2.09x | 0.008330s | 2.13x | 0.017769s |
| gc_table_alloc_collect | parity | 332325 | 0.126186s | 0.35x | 0.123954s | 0.35x | 0.043944s |
| gc_string_churn | parity | 1350000 | 0.004175s | 2.09x | 0.005820s | 1.50x | 0.008736s |
| pcall_error | strict | 120000 | 0.002388s | 4.16x | 0.002854s | 3.48x | 0.009926s |
| xpcall_error | strict | 80000 | 0.003864s | 1.93x | 0.007008s | 1.06x | 0.007456s |
| pcall_multi_return | strict | 900000 | 0.001261s | 4.66x | 0.003994s | 1.47x | 0.005876s |
| pcall_vararg_success | strict | 90000 | 0.000712s | 9.25x | 0.006401s | 1.03x | 0.006585s |
| coroutine_create_resume | strict | 120000 | 0.011730s | 0.85x | 0.010657s | 0.93x | 0.009947s |
| string_gmatch_words | parity | 2800000 | 0.015455s | 1.39x | 0.015065s | 1.42x | 0.021421s |
| utf8_codes_loop | parity | 760000 | 0.022275s | 1.08x | 0.021908s | 1.10x | 0.024116s |
| utf8_codes_lax_loop | parity | 760000 | 0.022660s | 1.05x | 0.022100s | 1.07x | 0.023727s |
| utf8_len_loop | parity | 5760000 | 0.008764s | 1.12x | 0.008629s | 1.13x | 0.009780s |
| utf8_len_ascii | parity | 34560000 | 0.011784s | 2.44x | 0.014507s | 1.98x | 0.028727s |
| utf8_codepoint_loop | parity | 1200000 | 0.002696s | 1.19x | 0.002722s | 1.18x | 0.003210s |
| utf8_char_build | parity | 1600000 | 0.008779s | 1.99x | 0.010684s | 1.63x | 0.017427s |
| utf8_offset_loop | parity | 509980 | 0.004186s | 1.21x | 0.006407s | 0.79x | 0.005068s |
| int64_add_const_box | strict | 627776 | 0.029694s | 1.00x | 0.030613s | 0.97x | 0.029735s |
| int64_sub_zero_compare | strict | 0 | 0.003437s | 8.78x | 0.000000s | 30175000.00x | 0.030175s |
| int64_mul_const_wrap | strict | 706027200 | 0.012388s | 3.31x | 0.034421s | 1.19x | 0.040964s |
| idiv_dynamic | strict | 90327 | 0.018918s | 2.72x | 0.008708s | 5.90x | 0.051384s |
| mod_dynamic | strict | 936739 | 0.015257s | 4.27x | 0.008757s | 7.45x | 0.065217s |
| numeric_for_int64_down | strict | 600376 | 0.011393s | 1.81x | 0.000000s | 20610000.00x | 0.020610s |
| numeric_for_int64_step3 | strict | 297 | 0.011313s | 1.83x | 0.000000s | 20700000.00x | 0.020700s |
| bit_and64_power2_mask | strict | 1081168960 | 0.014973s | 2.21x | 0.051274s | 0.65x | 0.033121s |
| bit_or64_small_const | strict | 1087329633 | 0.016885s | 2.10x | 0.008304s | 4.26x | 0.035410s |
| bit_xor64_small_const | strict | 9000000 | 0.013085s | 2.29x | 0.000000s | 29991000.00x | 0.029991s |
| bit_shift64_large_count | strict | 7000000 | 0.004912s | 9.63x | 0.039965s | 1.18x | 0.047287s |
| bit_shift64_neg_count | strict | 26591 | 0.005915s | 5.90x | 0.004931s | 7.08x | 0.034887s |
| bit_branch32_test | strict | 804674 | 0.011773s | 6.44x | 0.005744s | 13.21x | 0.075854s |
| table_rawget_int64_key | strict | 114272 | 0.029139s | 1.14x | 0.060399s | 0.55x | 0.033232s |
| table_rawset_int64_key | strict | 350000 | 0.009884s | 1.94x | 0.022175s | 0.87x | 0.019186s |
| table_set_int64_float_equiv | strict | 90000 | 0.003945s | 2.89x | 0.011113s | 1.03x | 0.011397s |
| table_string_key_miss | strict | 700000 | 0.006044s | 6.72x | 0.016533s | 2.46x | 0.040610s |
| table_sort_duplicates | parity | 1540 | 0.003306s | 7.99x | 0.005754s | 4.59x | 0.026404s |
| table_sort_cmp_strings | parity | 630 | 0.009108s | 1.43x | 0.011259s | 1.16x | 0.013068s |
| table_concat_large | parity | 5116000 | 0.003034s | 3.18x | 0.003472s | 2.78x | 0.009654s |
| table_unpack_slice_16 | parity | 800000 | 0.014837s | 3.56x | 0.043273s | 1.22x | 0.052816s |
| string_find_absent_plain | parity | 180000 | 0.000043s | 151.35x | 0.006969s | 0.93x | 0.006508s |
| string_find_long_plain | parity | 880000 | 0.000030s | 241.47x | 0.007036s | 1.03x | 0.007244s |
| string_match_no_capture | parity | 1260000 | 0.012203s | 1.23x | 0.012933s | 1.16x | 0.014966s |
| string_gsub_literal | parity | 12600000 | 0.004611s | 8.46x | 0.004718s | 8.27x | 0.039000s |
| string_sub_large_slice | parity | 28800000 | 0.014793s | 1.12x | 0.019816s | 0.84x | 0.016591s |
| string_byte_full_short | parity | 400000 | 0.002489s | 21.61x | 0.042850s | 1.26x | 0.053780s |
| string_char_many | parity | 2400000 | 0.021068s | 1.30x | 0.040738s | 0.67x | 0.027421s |
| string_rep_sep | parity | 4410000 | 0.003264s | 3.20x | 0.008654s | 1.21x | 0.010460s |
| string_format_mixed | parity | 1536768 | 0.007279s | 2.04x | 0.007333s | 2.03x | 0.014872s |
| string_pack_unpack_float | parity | 736924 | 0.015777s | 1.18x | 0.017629s | 1.06x | 0.018686s |
| tonumber_hex64_parse | parity | 263472 | 0.004106s | 1.80x | 0.010491s | 0.70x | 0.007388s |
| math_modf_loop | parity | 750042 | 0.017062s | 3.84x | 0.076051s | 0.86x | 0.065594s |
| math_floor_int_passthrough | parity | 0 | 0.002328s | 44.81x | 0.075699s | 1.38x | 0.104320s |
| math_tointeger_int64_string | strict | 46640 | 0.004771s | 1.70x | 0.011217s | 0.72x | 0.008104s |
| metamethod_order_int64 | strict | 800000 | 0.000327s | 63.01x | 0.019828s | 1.04x | 0.020603s |
| pcall_deep_stack | strict | 240000 | 0.000487s | 11.71x | 0.002541s | 2.25x | 0.005705s |
| coroutine_pingpong | strict | 240001 | 0.009387s | 1.09x | 0.013743s | 0.74x | 0.010194s |
| debug_getlocal_loop | parity | 40001 | 0.001485s | 0.69x | 0.001843s | 0.56x | 0.001026s |
| gc_table_churn_no_collect | parity | 478400 | 0.000441s | 56.84x | 0.013553s | 1.85x | 0.025065s |
| table_next_array_extra | strict | 0 | 0.262264s | 1.58x | 0.258117s | 1.60x | 0.414101s |
| table_next_sparse_extra | strict | 600000 | 0.399715s | 0.99x | 0.400058s | 0.99x | 0.397191s |
| table_pairs_array_extra | strict | 0 | 0.131782s | 2.12x | 0.131847s | 2.12x | 0.278971s |
| table_sort_cmp_upvalue_extra | parity | 949178 | 0.003023s | 11.88x | 0.004454s | 8.06x | 0.035921s |
| table_sort_records_extra | parity | 25802 | 0.008890s | 1.60x | 0.012662s | 1.12x | 0.014201s |
| table_sort_large_int_extra | parity | 815810 | 0.003234s | 5.04x | 0.004559s | 3.57x | 0.016286s |
| string_concat_small_extra | strict | 2160000 | 0.048501s | 1.49x | 0.069497s | 1.04x | 0.072310s |
| string_sub_tiny_extra | parity | 1200000 | 0.000984s | 27.08x | 0.023728s | 1.12x | 0.026649s |
| string_match_captures_extra | parity | 1800000 | 0.011849s | 1.30x | 0.013100s | 1.18x | 0.015401s |
| string_arith_int64_extra | strict | 885000 | 0.000718s | 2.97x | 0.002087s | 1.02x | 0.002134s |
| bit64_and_shift_extra | strict | 2019367627 | 0.013547s | 3.06x | 0.092780s | 0.45x | 0.041391s |
| bit64_branch_not_extra | strict | 0 | 0.005091s | 7.48x | 0.004934s | 7.72x | 0.038090s |
| int64_compare_threshold_extra | strict | 995968 | 0.005807s | 4.02x | 0.002175s | 10.74x | 0.023356s |
| int64_minmax_chain_extra | strict | 0 | 0.011295s | 7.13x | 0.124137s | 0.65x | 0.080479s |
| int64_compare_eq_const_extra | strict | 41016 | 0.003318s | 8.35x | 0.002201s | 12.58x | 0.027698s |
| int64_compare_between_extra | strict | 12 | 0.006073s | 3.16x | 0.001945s | 9.87x | 0.019204s |
| int64_compare_descending_extra | strict | 4032 | 0.006250s | 3.70x | 0.002188s | 10.57x | 0.023130s |
| bit64_const_mask_chain_extra | strict | 65477308 | 0.013442s | 2.08x | 0.044060s | 0.63x | 0.027912s |
| bit64_const_or_xor_chain_extra | strict | 7000001 | 0.012405s | 2.25x | 0.041420s | 0.67x | 0.027944s |
| bit64_compare_mask_zero_extra | strict | 109564 | 0.007779s | 2.80x | 0.038989s | 0.56x | 0.021807s |
| table_next_mixed_extra | strict | 744000 | 0.165554s | 1.52x | 0.169050s | 1.49x | 0.252339s |
| table_pairs_sparse_extra | strict | 600000 | 0.292239s | 1.03x | 0.246218s | 1.22x | 0.301082s |
| table_pairs_int64_keys_extra | strict | 520000 | 0.121109s | 1.54x | 0.240580s | 0.77x | 0.186340s |
| load_dump_many_protos_extra | parity | 54000 | 0.000233s | 3.96x | 0.000265s | 3.48x | 0.000922s |
| debug_getupvalue_extra | parity | 80000 | 0.001463s | 1.21x | 0.002155s | 0.82x | 0.001765s |
| debug_setupvalue_extra | parity | 467080 | 0.001533s | 1.30x | 0.002533s | 0.78x | 0.001988s |
| closure_call_mixed_extra | strict | 910272 | 0.001166s | 16.11x | 0.015220s | 1.23x | 0.018779s |
| load_dump_extra | parity | 762500 | 0.000364s | 5.64x | 0.000376s | 5.46x | 0.002054s |
| bit64_rotate_var_extra | strict | 1807377590 | 0.010209s | 3.92x | 0.071585s | 0.56x | 0.040036s |
| bit64_extract_insert_extra | strict | 536706047 | 0.009593s | 5.88x | 0.041300s | 1.37x | 0.056419s |
| bit32_extract_branch_extra | strict | 154058 | 0.053119s | 3.47x | 0.130625s | 1.41x | 0.184332s |
| bit64_math_ult_branch_extra | strict | 999988 | 0.012723s | 7.14x | 0.144845s | 0.63x | 0.090831s |
| int64_compare_float_threshold_extra | strict | 997120 | 0.012284s | 1.44x | 0.001561s | 11.34x | 0.017701s |
| int64_compare_table_bound_extra | strict | 328 | 0.006844s | 3.17x | 0.036692s | 0.59x | 0.021716s |
| numeric_for_int64_large_step_extra | strict | 927776 | 0.016057s | 1.09x | 0.025544s | 0.68x | 0.017456s |
| table_next_int64_keys_extra | strict | 520000 | 0.192951s | 1.33x | 0.278023s | 0.92x | 0.257057s |
| table_pairs_string_keys_extra | strict | 568000 | 0.144318s | 1.98x | 0.212991s | 1.34x | 0.285508s |
| table_pairs_mixed_sparse_extra | strict | 704000 | 0.365479s | 0.99x | 0.328502s | 1.10x | 0.361907s |
| table_sort_tiny_cmp_extra | parity | 126926 | 0.003966s | 1.47x | 0.007933s | 0.74x | 0.005840s |
| table_sort_cmp_desc_extra | parity | 870240 | 0.001483s | 32.66x | 0.003624s | 13.37x | 0.048439s |
| table_sort_large_strings_extra | parity | 504 | 0.002685s | 2.90x | 0.003562s | 2.19x | 0.007784s |
| table_sort_wide_records_extra | parity | 35772 | 0.007605s | 1.55x | 0.011903s | 0.99x | 0.011797s |
| table_sort_equal_keys_extra | parity | 62857 | 0.015148s | 1.59x | 0.024222s | 1.00x | 0.024107s |
| table_move_int64_extra | parity | 625000 | 0.061638s | 1.29x | 0.063840s | 1.24x | 0.079364s |
| string_find_frontier_extra | parity | 2520000 | 0.007498s | 1.30x | 0.008630s | 1.13x | 0.009737s |
| string_gsub_many_repl_extra | parity | 672000 | 0.003752s | 1.57x | 0.004088s | 1.44x | 0.005907s |
| string_format_int64_mix_extra | parity | 2730000 | 0.002348s | 6.25x | 0.005670s | 2.59x | 0.014664s |
| gc_closure_alloc_collect_extra | parity | 103040 | 0.092966s | 0.35x | 0.091947s | 0.35x | 0.032091s |
