# ARM64 Wide Performance Sweep

Broader ARM64 sweep to find workloads where LuaJIT Lua 5.4 compatibility is slower than original Lua 5.4.8.

## Method

- `luajit_jit_on`: `jit.on(); jit.flush(); jit.opt.start("3", "hotloop=8", "hotexit=2")`.
- `luajit_jit_off`: `jit.off(); jit.flush()`.
- `lua5.4.8`: `/Users/gongliang/git/lua-5.4.8/lua`.
- LuaJIT modes reset their JIT state before each workload warmup, so unrelated workloads do not pollute the trace cache.
- Each workload runs one warmup pass and 5 measured passes; the report uses the best measured `os.clock()` time.
- Every measured pass must return a stable checksum; mismatched workloads are listed separately and excluded from speedup totals.
- `--enforce` requires every workload in both LuaJIT modes to be faster than Lua 5.4.8, with stable matching checksums.

## Summary

- Workloads: 300
- Comparable workloads: 300
- Checksum/consistency mismatches: 0

| Mode | Comparable total time | Total speedup | Geomean speedup | Slower workloads |
| --- | ---: | ---: | ---: | ---: |
| luajit_jit_on | 7.837443s | 2.43x | 3.81x | 13 |
| luajit_jit_off | 18.833285s | 1.01x | 1.24x | 116 |
| lua5.4.8 | 19.027959s | 1.00x | 1.00x | 0 |

## Key Findings

- JIT-on is faster on 287/300 comparable workloads; the remaining slower cases are gc_closure_alloc_collect_extra 0.35x (0.089693s vs 0.031506s), gc_table_alloc_collect 0.35x (0.124085s vs 0.043824s), string_concat 0.62x (0.012685s vs 0.007906s), string_format_int64 0.63x (0.023889s vs 0.015160s), string_gmatch_captures 0.67x (0.062942s vs 0.042086s), table_int64_key_insert 0.74x (0.009260s vs 0.006874s), closure_alloc 0.77x (0.042523s vs 0.032902s), debug_getlocal_loop 0.82x (0.001247s vs 0.001018s), table_int64_key_delete 0.85x (0.009518s vs 0.008079s), string_pack_unpack_mixed 0.91x (0.023087s vs 0.021111s), coroutine_create_resume 0.92x (0.010857s vs 0.009954s), table_pairs_mixed_sparse_extra 0.97x (0.372349s vs 0.360136s), table_pairs_sparse_extra 0.99x (0.306750s vs 0.302160s).
- JIT-off is slower on 116/300 comparable workloads; the worst confirmed cases are bit64_extract_insert_extra 0.28x (0.192090s vs 0.054381s), int64_compare_float_threshold_extra 0.31x (0.057903s vs 0.017835s), branch_mod_loop 0.34x (0.281862s vs 0.094650s), bit32_extract_branch_extra 0.34x (0.540472s vs 0.184082s), gc_closure_alloc_collect_extra 0.34x (0.091351s vs 0.031506s), gc_table_alloc_collect 0.35x (0.124435s vs 0.043824s), int64_compare_threshold_extra 0.36x (0.063951s vs 0.023194s), int64_compare_descending_extra 0.38x (0.062295s vs 0.023719s).
- Checksums matched for all modes, so the listed slowdowns are performance differences, not result mismatches.

## High-Repetition Confirmations

Focused reruns with 13 measured passes were used to separate stable changes from full-sweep noise.

| Workload | JIT-on | JIT-off | Lua 5.4.8 | Status |
| --- | ---: | ---: | ---: | --- |
| string_format_int64 | 0.010668s | 0.007613s | 0.015649s | Both modes faster; full-sweep JIT-on slowdown is noise-sensitive. |
| string_format_int64_mix_extra | 0.002289s | 0.005906s | 0.014952s | Both modes faster. |
| math_ult_loop | 0.003040s | 0.063987s | 0.094994s | Both modes faster after the ARM64 integer fast path. |
| bit64_math_ult_branch_extra | 0.012852s | 0.123160s | 0.089046s | JIT-on faster; JIT-off still slower. |
| math_minmax_int64 | 0.010587s | 0.074506s | 0.067512s | JIT-on faster; JIT-off still slightly slower. |
| math_minmax_mixed_int64 | 0.008119s | 0.043146s | 0.049815s | Both modes faster after the ARM64 two-argument fast path. |
| bit64_extract_insert_extra | 0.009670s | 0.194682s | 0.055347s | JIT-on faster; JIT-off remains the largest bitwise gap. |
| bit32_extract_branch_extra | 0.053394s | 0.542475s | 0.183077s | JIT-on faster; JIT-off remains much slower. |
| int64_compare_threshold_extra | 0.005839s | 0.064939s | 0.024528s | JIT-on faster; JIT-off int64 compare remains much slower. |
| table_sort_int | 0.004496s | 0.006489s | 0.037594s | Both modes faster. |
| table_sort_int64 | 0.000656s | 0.000826s | 0.003765s | Both modes faster. |
| table_sort_wide_records_extra | 0.007037s | 0.012112s | 0.011470s | JIT-on faster; JIT-off is near parity and noise-sensitive. |
| table_sort_equal_keys_extra | 0.014276s | 0.023742s | 0.023578s | JIT-on faster; JIT-off is near parity and noise-sensitive. |

## Slower Than Lua 5.4.8

| Mode | Workload | LuaJIT time | Lua 5.4.8 time | Speedup |
| --- | --- | ---: | ---: | ---: |
| luajit_jit_off | bit64_extract_insert_extra | 0.192090s | 0.054381s | 0.28x |
| luajit_jit_off | int64_compare_float_threshold_extra | 0.057903s | 0.017835s | 0.31x |
| luajit_jit_off | branch_mod_loop | 0.281862s | 0.094650s | 0.34x |
| luajit_jit_off | bit32_extract_branch_extra | 0.540472s | 0.184082s | 0.34x |
| luajit_jit_off | gc_closure_alloc_collect_extra | 0.091351s | 0.031506s | 0.34x |
| luajit_jit_on | gc_closure_alloc_collect_extra | 0.089693s | 0.031506s | 0.35x |
| luajit_jit_off | gc_table_alloc_collect | 0.124435s | 0.043824s | 0.35x |
| luajit_jit_on | gc_table_alloc_collect | 0.124085s | 0.043824s | 0.35x |
| luajit_jit_off | int64_compare_threshold_extra | 0.063951s | 0.023194s | 0.36x |
| luajit_jit_off | int64_compare_descending_extra | 0.062295s | 0.023719s | 0.38x |
| luajit_jit_off | int64_compare_between_extra | 0.048840s | 0.019214s | 0.39x |
| luajit_jit_off | int64_compare_table_bound_extra | 0.053774s | 0.021203s | 0.39x |
| luajit_jit_off | int64_sub_zero_compare | 0.072564s | 0.030031s | 0.41x |
| luajit_jit_off | int64_eq_branch | 0.202732s | 0.083988s | 0.41x |
| luajit_jit_off | int64_compare_eq_const_extra | 0.065383s | 0.027558s | 0.42x |
| luajit_jit_off | bit_xor64 | 0.109382s | 0.049118s | 0.45x |
| luajit_jit_off | int_compare_eq_chain | 0.381211s | 0.175090s | 0.46x |
| luajit_jit_off | bit_or64_small_const | 0.076870s | 0.035376s | 0.46x |
| luajit_jit_off | int_compare_branch | 0.244437s | 0.113871s | 0.47x |
| luajit_jit_off | while_loop_countdown | 0.226420s | 0.105700s | 0.47x |
| luajit_jit_off | bit_branch32_test | 0.162170s | 0.076111s | 0.47x |
| luajit_jit_off | bit64_branch_not_extra | 0.080747s | 0.038147s | 0.47x |
| luajit_jit_off | bit_shift64_neg_count | 0.073876s | 0.035058s | 0.47x |
| luajit_jit_off | bit64_rotate_var_extra | 0.082881s | 0.039593s | 0.48x |
| luajit_jit_off | table_int64_key_lookup | 0.039808s | 0.019441s | 0.49x |
| luajit_jit_off | table_int64_key_update | 0.027616s | 0.013545s | 0.49x |
| luajit_jit_off | bit_shift32_var | 0.324793s | 0.160110s | 0.49x |
| luajit_jit_off | bit_shift_signed64 | 0.068087s | 0.033633s | 0.49x |
| luajit_jit_off | bit_and64_const | 0.043368s | 0.021547s | 0.50x |
| luajit_jit_off | pure_bit_xor | 0.181073s | 0.090088s | 0.50x |
| luajit_jit_off | bit64_and_shift_extra | 0.082021s | 0.041531s | 0.51x |
| luajit_jit_off | bit64_branch_test | 0.075931s | 0.039599s | 0.52x |
| luajit_jit_off | bit_shift64_const | 0.057549s | 0.030323s | 0.53x |
| luajit_jit_off | bit_or64_const | 0.051730s | 0.028083s | 0.54x |
| luajit_jit_off | idiv_const | 0.198677s | 0.108122s | 0.54x |
| luajit_jit_off | bit_shift64_var | 0.103999s | 0.058374s | 0.56x |
| luajit_jit_off | table_int64_key_delete | 0.014064s | 0.008079s | 0.57x |
| luajit_jit_off | debug_getlocal_loop | 0.001754s | 0.001018s | 0.58x |
| luajit_jit_off | bit_shift_mask | 0.335068s | 0.196156s | 0.59x |
| luajit_jit_off | table_array_append_direct | 0.011932s | 0.006989s | 0.59x |
| luajit_jit_off | bit_not32 | 0.133988s | 0.078929s | 0.59x |
| luajit_jit_off | bit_shift32_const | 0.310274s | 0.186237s | 0.60x |
| luajit_jit_off | int64_idiv_const | 0.079093s | 0.047731s | 0.60x |
| luajit_jit_off | bit_or64 | 0.060582s | 0.036628s | 0.60x |
| luajit_jit_off | bit64_compare_mask_zero_extra | 0.035126s | 0.021655s | 0.62x |
| luajit_jit_on | string_concat | 0.012685s | 0.007906s | 0.62x |
| luajit_jit_off | idiv_power2 | 0.171474s | 0.108467s | 0.63x |
| luajit_jit_on | string_format_int64 | 0.023889s | 0.015160s | 0.63x |
| luajit_jit_off | metamethod_index_table | 0.030099s | 0.019422s | 0.65x |
| luajit_jit_off | array_sum_chunk | 0.175722s | 0.113716s | 0.65x |
| luajit_jit_off | bit_and64_power2_mask | 0.051288s | 0.033236s | 0.65x |
| luajit_jit_off | table_float_key_lookup | 0.075937s | 0.049588s | 0.65x |
| luajit_jit_off | math_abs_int64 | 0.069202s | 0.045303s | 0.65x |
| luajit_jit_off | bit_rotate64 | 0.139415s | 0.093203s | 0.67x |
| luajit_jit_on | string_gmatch_captures | 0.062942s | 0.042086s | 0.67x |
| luajit_jit_off | int64_unary_minus | 0.056071s | 0.037917s | 0.68x |
| luajit_jit_off | function_calls | 0.357316s | 0.244589s | 0.68x |
| luajit_jit_off | bit_and32 | 0.129683s | 0.090801s | 0.70x |
| luajit_jit_off | idiv_dynamic | 0.072961s | 0.051176s | 0.70x |
| luajit_jit_off | bit_or32 | 0.127485s | 0.090904s | 0.71x |
| luajit_jit_off | bit_and64 | 0.061796s | 0.044289s | 0.72x |
| luajit_jit_off | table_int64_key_miss | 0.027697s | 0.019928s | 0.72x |
| luajit_jit_off | repeat_until_countdown | 0.143633s | 0.103446s | 0.72x |
| luajit_jit_off | vararg_select_tail | 0.036954s | 0.027015s | 0.73x |
| luajit_jit_on | table_int64_key_insert | 0.009260s | 0.006874s | 0.74x |
| luajit_jit_off | bit_xor32 | 0.120443s | 0.089504s | 0.74x |
| luajit_jit_off | mod_const | 0.185056s | 0.137573s | 0.74x |
| luajit_jit_off | bit64_const_mask_chain_extra | 0.037236s | 0.027764s | 0.75x |
| luajit_jit_off | bit64_math_ult_branch_extra | 0.122690s | 0.091627s | 0.75x |
| luajit_jit_off | table_int64_key_insert | 0.009127s | 0.006874s | 0.75x |
| luajit_jit_off | int64_mod_const | 0.099450s | 0.075780s | 0.76x |
| luajit_jit_off | global_write_loop | 0.012903s | 0.009852s | 0.76x |
| luajit_jit_off | int64_minmax_chain_extra | 0.108731s | 0.083576s | 0.77x |
| luajit_jit_off | coroutine_pingpong | 0.013799s | 0.010654s | 0.77x |
| luajit_jit_off | table_rawget_int64_key | 0.042620s | 0.032963s | 0.77x |
| luajit_jit_on | closure_alloc | 0.042523s | 0.032902s | 0.77x |
| luajit_jit_off | string_compare_loop | 0.066194s | 0.051365s | 0.78x |
| luajit_jit_off | bit_xor64_const | 0.050707s | 0.039541s | 0.78x |
| luajit_jit_off | mod_dynamic | 0.082509s | 0.064352s | 0.78x |
| luajit_jit_off | coroutine_resume_args | 0.014626s | 0.011439s | 0.78x |
| luajit_jit_off | bit_xor64_small_const | 0.038160s | 0.029986s | 0.79x |
| luajit_jit_off | metamethod_idiv | 0.004671s | 0.003694s | 0.79x |
| luajit_jit_off | bit_not64 | 0.045090s | 0.035675s | 0.79x |
| luajit_jit_off | coroutine_resume | 0.019755s | 0.015741s | 0.80x |
| luajit_jit_off | int64_add_compare_loop | 0.048360s | 0.038675s | 0.80x |
| luajit_jit_off | numeric_for_int64_down | 0.025497s | 0.020407s | 0.80x |
| luajit_jit_off | string_concat | 0.009846s | 0.007906s | 0.80x |
| luajit_jit_off | coroutine_yield_multi | 0.015063s | 0.012127s | 0.81x |
| luajit_jit_on | debug_getlocal_loop | 0.001247s | 0.001018s | 0.82x |
| luajit_jit_off | table_pairs_int64_keys_extra | 0.226345s | 0.186382s | 0.82x |
| luajit_jit_off | select_vararg | 0.081020s | 0.066781s | 0.82x |
| luajit_jit_off | numeric_for_int64_step3 | 0.024544s | 0.020240s | 0.82x |
| luajit_jit_off | string_sub_large_slice | 0.019990s | 0.016523s | 0.83x |
| luajit_jit_off | table_array_pop_direct | 0.019753s | 0.016358s | 0.83x |
| luajit_jit_off | global_read_loop | 0.071988s | 0.059725s | 0.83x |
| luajit_jit_off | debug_setupvalue_extra | 0.002474s | 0.002053s | 0.83x |
| luajit_jit_off | bit_shift64_right | 0.053466s | 0.044368s | 0.83x |
| luajit_jit_off | numeric_for_int64_span | 0.027644s | 0.023394s | 0.85x |
| luajit_jit_on | table_int64_key_delete | 0.009518s | 0.008079s | 0.85x |
| luajit_jit_off | metamethod_index | 0.017126s | 0.014659s | 0.86x |
| luajit_jit_off | float_compare_branch | 0.197735s | 0.169579s | 0.86x |
| luajit_jit_off | debug_getupvalue_extra | 0.002090s | 0.001828s | 0.87x |
| luajit_jit_off | coroutine_create_resume | 0.011108s | 0.009954s | 0.90x |
| luajit_jit_off | metamethod_len | 0.014594s | 0.013166s | 0.90x |
| luajit_jit_off | string_char_many | 0.030361s | 0.027402s | 0.90x |
| luajit_jit_off | numeric_for_down | 0.044688s | 0.040369s | 0.90x |
| luajit_jit_off | math_modf_loop | 0.071746s | 0.064837s | 0.90x |
| luajit_jit_off | vararg_sum | 0.110130s | 0.100259s | 0.91x |
| luajit_jit_on | string_pack_unpack_mixed | 0.023087s | 0.021111s | 0.91x |
| luajit_jit_off | bit64_const_or_xor_chain_extra | 0.030329s | 0.027801s | 0.92x |
| luajit_jit_on | coroutine_create_resume | 0.010857s | 0.009954s | 0.92x |
| luajit_jit_off | numeric_for_int64_large_step_extra | 0.018580s | 0.017094s | 0.92x |
| luajit_jit_off | float_arith | 0.227139s | 0.209470s | 0.92x |
| luajit_jit_off | table_field_read_loop | 0.190074s | 0.176682s | 0.93x |
| luajit_jit_off | array_read_stride | 0.041877s | 0.039020s | 0.93x |
| luajit_jit_off | string_pack_unpack_mixed | 0.022570s | 0.021111s | 0.94x |
| luajit_jit_off | metamethod_add | 0.009387s | 0.008800s | 0.94x |
| luajit_jit_off | table_next_int64_keys_extra | 0.273482s | 0.256561s | 0.94x |
| luajit_jit_off | metamethod_newindex | 0.011666s | 0.010989s | 0.94x |
| luajit_jit_off | math_minmax_int64 | 0.073738s | 0.069989s | 0.95x |
| luajit_jit_off | string_find_absent_plain | 0.006747s | 0.006513s | 0.97x |
| luajit_jit_on | table_pairs_mixed_sparse_extra | 0.372349s | 0.360136s | 0.97x |
| luajit_jit_off | idiv_negative | 0.201447s | 0.197290s | 0.98x |
| luajit_jit_off | array_sum_wide | 0.057273s | 0.056362s | 0.98x |
| luajit_jit_on | table_pairs_sparse_extra | 0.306750s | 0.302160s | 0.99x |
| luajit_jit_off | string_byte_single | 0.091942s | 0.091127s | 0.99x |
| luajit_jit_off | string_find_plain | 0.003799s | 0.003766s | 0.99x |
| luajit_jit_off | table_remove_tail | 0.016660s | 0.016562s | 0.99x |
| luajit_jit_off | utf8_len_loop | 0.009728s | 0.009685s | 1.00x |

## Checksum/Correctness Mismatches

No checksum or per-run consistency mismatches.

## Slowdown Clusters

| Mode | Category | Slow workloads | Worst workload | Worst speedup |
| --- | --- | ---: | --- | ---: |
| luajit_jit_off | array/hash table | 5 | array_sum_chunk | 0.65x |
| luajit_jit_off | bitwise/integer | 35 | bit64_extract_insert_extra | 0.28x |
| luajit_jit_off | call/metamethod | 10 | metamethod_index_table | 0.65x |
| luajit_jit_off | integer/control | 29 | int64_compare_float_threshold_extra | 0.31x |
| luajit_jit_off | math library | 3 | math_abs_int64 | 0.65x |
| luajit_jit_off | other | 2 | float_compare_branch | 0.86x |
| luajit_jit_off | protected/coroutine | 5 | coroutine_pingpong | 0.77x |
| luajit_jit_off | runtime/debug/gc | 5 | gc_closure_alloc_collect_extra | 0.34x |
| luajit_jit_off | string/format/parse | 9 | string_compare_loop | 0.78x |
| luajit_jit_off | table library | 13 | table_int64_key_lookup | 0.49x |
| luajit_jit_on | call/metamethod | 1 | closure_alloc | 0.77x |
| luajit_jit_on | protected/coroutine | 1 | coroutine_create_resume | 0.92x |
| luajit_jit_on | runtime/debug/gc | 3 | gc_closure_alloc_collect_extra | 0.35x |
| luajit_jit_on | string/format/parse | 4 | string_concat | 0.62x |
| luajit_jit_on | table library | 4 | table_int64_key_insert | 0.74x |

## Full Results

| Workload | Checksum | JIT-on | On speedup | JIT-off | Off speedup | Lua 5.4.8 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| empty_loop | 100000000 | 0.022247s | 13.28x | 0.229972s | 1.29x | 0.295520s |
| int_add_wide | 191146379 | 0.317096s | 1.59x | 0.176849s | 2.85x | 0.504530s |
| int64_add_sub | 127778 | 0.116821s | 1.12x | 0.118848s | 1.10x | 0.131088s |
| int_add32 | 576647110 | 0.281297s | 3.02x | 0.306852s | 2.77x | 0.849320s |
| int_mul32 | 311395809 | 0.055418s | 4.31x | 0.147970s | 1.62x | 0.239093s |
| int_mod_small | 999740 | 0.156584s | 2.40x | 0.301227s | 1.25x | 0.376024s |
| idiv_const | 0 | 0.021590s | 5.01x | 0.198677s | 0.54x | 0.108122s |
| idiv_negative | 714300 | 0.067109s | 2.94x | 0.201447s | 0.98x | 0.197290s |
| idiv_power2 | 65539 | 0.033679s | 3.22x | 0.171474s | 0.63x | 0.108467s |
| mod_const | 500000 | 0.028034s | 4.91x | 0.185056s | 0.74x | 0.137573s |
| mod_power2 | 387680 | 0.041240s | 3.33x | 0.133954s | 1.03x | 0.137352s |
| numeric_for_int64_span | 999943 | 0.013076s | 1.79x | 0.027644s | 0.85x | 0.023394s |
| int_compare_branch | 861515 | 0.044297s | 2.57x | 0.244437s | 0.47x | 0.113871s |
| int_compare_eq_chain | 817169 | 0.036987s | 4.73x | 0.381211s | 0.46x | 0.175090s |
| int64_compare_minmax | 0 | 0.032938s | 2.05x | 0.053098s | 1.27x | 0.067653s |
| int64_eq_branch | 373544 | 0.010461s | 8.03x | 0.202732s | 0.41x | 0.083988s |
| int64_mod_const | 992044 | 0.045441s | 1.67x | 0.099450s | 0.76x | 0.075780s |
| int64_idiv_const | 632320 | 0.045555s | 1.05x | 0.079093s | 0.60x | 0.047731s |
| int64_mul_wrap | 1744528928 | 0.005778s | 5.87x | 0.022779s | 1.49x | 0.033932s |
| float_compare_branch | -963739252 | 0.129550s | 1.31x | 0.197735s | 0.86x | 0.169579s |
| float_arith | 206120287761215.281250 | 0.154824s | 1.35x | 0.227139s | 0.92x | 0.209470s |
| pure_bit_xor | 50000000 | 0.068147s | 1.32x | 0.181073s | 0.50x | 0.090088s |
| bit_and32 | 33950784 | 0.048250s | 1.88x | 0.129683s | 0.70x | 0.090801s |
| bit_or32 | 2147483647 | 0.033148s | 2.74x | 0.127485s | 0.71x | 0.090904s |
| bit_xor32 | 102466744 | 0.032439s | 2.76x | 0.120443s | 0.74x | 0.089504s |
| bit_not32 | 35000000 | 0.071162s | 1.11x | 0.133988s | 0.59x | 0.078929s |
| bit_shift32_const | 300872905 | 0.055048s | 3.38x | 0.310274s | 0.60x | 0.186237s |
| bit_shift32_var | 1592255438 | 0.047715s | 3.36x | 0.324793s | 0.49x | 0.160110s |
| bit_shift_mask | 318080536 | 0.048719s | 4.03x | 0.335068s | 0.59x | 0.196156s |
| bitwise_idiv_unsigned | 1821843616 | 0.038613s | 2.71x | 0.097205s | 1.08x | 0.104589s |
| bitwise_idiv_mix | 714511537 | 0.060255s | 2.35x | 0.118001s | 1.20x | 0.141299s |
| idiv_mod | 997901798 | 0.102400s | 3.37x | 0.315476s | 1.09x | 0.345177s |
| bit_shift64_const | 295495246 | 0.019142s | 1.58x | 0.057549s | 0.53x | 0.030323s |
| bit_and64 | 1890031 | 0.024857s | 1.78x | 0.061796s | 0.72x | 0.044289s |
| bit_and64_const | 1456439740 | 0.014796s | 1.46x | 0.043368s | 0.50x | 0.021547s |
| bit_or64 | 305419896 | 0.005566s | 6.58x | 0.060582s | 0.60x | 0.036628s |
| bit_or64_const | 1459617791 | 0.008379s | 3.35x | 0.051730s | 0.54x | 0.028083s |
| bit_xor64 | 1617852892 | 0.005978s | 8.22x | 0.109382s | 0.45x | 0.049118s |
| bit_xor64_const | 1456439740 | 0.008399s | 4.71x | 0.050707s | 0.78x | 0.039541s |
| bit_shift64_var | 23590 | 0.012969s | 4.50x | 0.103999s | 0.56x | 0.058374s |
| bit_shift64_right | 1846415822 | 0.008309s | 5.34x | 0.053466s | 0.83x | 0.044368s |
| bit_not64 | 1633917628 | 0.008460s | 4.22x | 0.045090s | 0.79x | 0.035675s |
| bit64_branch_test | 999746 | 0.009823s | 4.03x | 0.075931s | 0.52x | 0.039599s |
| array_sum_wide | 967060 | 0.005454s | 10.33x | 0.057273s | 0.98x | 0.056362s |
| array_sum_chunk | 562228 | 0.014511s | 7.84x | 0.175722s | 0.65x | 0.113716s |
| array_read_seq | 967060 | 0.005497s | 10.22x | 0.056044s | 1.00x | 0.056166s |
| array_read_stride | 989020 | 0.006814s | 5.73x | 0.041877s | 0.93x | 0.039020s |
| array_write_seq | 500500 | 0.001438s | 6.73x | 0.008080s | 1.20x | 0.009677s |
| table_new_small | 50000 | 0.011558s | 4.06x | 0.024846s | 1.89x | 0.046882s |
| hash_lookup | 144320 | 0.010868s | 12.13x | 0.082671s | 1.59x | 0.131785s |
| hash_update | 113833 | 0.007015s | 3.65x | 0.016302s | 1.57x | 0.025620s |
| table_int64_key_lookup | 164192 | 0.006097s | 3.19x | 0.039808s | 0.49x | 0.019441s |
| table_int64_key_insert | 461700 | 0.009260s | 0.74x | 0.009127s | 0.75x | 0.006874s |
| table_int64_key_update | 64624 | 0.003859s | 3.51x | 0.027616s | 0.49x | 0.013545s |
| table_int64_key_miss | 2500000 | 0.002908s | 6.85x | 0.027697s | 0.72x | 0.019928s |
| table_int64_key_delete | 363200 | 0.009518s | 0.85x | 0.014064s | 0.57x | 0.008079s |
| table_float_key_lookup | 599920 | 0.005888s | 8.42x | 0.075937s | 0.65x | 0.049588s |
| table_mixed_key_lookup | 551488 | 0.006073s | 5.49x | 0.026094s | 1.28x | 0.033311s |
| table_next_dense | 360000 | 0.046577s | 2.07x | 0.046696s | 2.06x | 0.096235s |
| table_next_hash | 248000 | 0.055967s | 2.49x | 0.082083s | 1.69x | 0.139089s |
| rawget_rawset | 900000 | 0.021056s | 2.13x | 0.038184s | 1.18x | 0.044879s |
| ipairs_iter | 40000 | 0.076043s | 1.82x | 0.071745s | 1.93x | 0.138439s |
| pairs_iter | 40000 | 0.073224s | 2.53x | 0.116543s | 1.59x | 0.185438s |
| function_calls | 500000 | 0.009666s | 25.30x | 0.357316s | 0.68x | 0.244589s |
| closure_alloc | 350000 | 0.042523s | 0.77x | 0.020912s | 1.57x | 0.032902s |
| pcall_success | 375000 | 0.000604s | 9.82x | 0.002683s | 2.21x | 0.005930s |
| table_sort_int | 99050 | 0.014123s | 2.72x | 0.005373s | 7.16x | 0.038456s |
| table_sort_cmp | 65240 | 0.015310s | 1.36x | 0.018489s | 1.13x | 0.020837s |
| table_sort_strings | 735 | 0.008085s | 1.65x | 0.008129s | 1.65x | 0.013376s |
| table_sort_reverse | 285040 | 0.001569s | 17.92x | 0.002644s | 10.63x | 0.028113s |
| table_sort_int64 | 358755 | 0.001217s | 3.23x | 0.000862s | 4.56x | 0.003931s |
| table_sort_floats | 9290 | 0.001671s | 4.53x | 0.001280s | 5.91x | 0.007567s |
| string_concat | 199680 | 0.012685s | 0.62x | 0.009846s | 0.80x | 0.007906s |
| table_concat | 1524000 | 0.001458s | 2.53x | 0.001461s | 2.52x | 0.003689s |
| table_move_copy | 710000 | 0.039613s | 1.36x | 0.041461s | 1.30x | 0.053989s |
| table_move_large_copy | 343000 | 0.063040s | 1.21x | 0.063332s | 1.21x | 0.076575s |
| table_move_self_nooverlap | 390000 | 0.001572s | 26.80x | 0.002386s | 17.66x | 0.042132s |
| table_pack_loop | 325000 | 0.030486s | 1.13x | 0.016449s | 2.10x | 0.034482s |
| table_remove_front | 948800 | 0.012928s | 9.39x | 0.015770s | 7.70x | 0.121379s |
| table_insert_tail | 430800 | 0.001555s | 8.70x | 0.013062s | 1.04x | 0.013531s |
| table_remove_tail | 172800 | 0.013384s | 1.24x | 0.016660s | 0.99x | 0.016562s |
| table_insert_front | 474000 | 0.007902s | 5.98x | 0.009186s | 5.14x | 0.047223s |
| table_remove_middle | 132800 | 0.005459s | 6.75x | 0.007459s | 4.94x | 0.036857s |
| table_move_overlap_forward | 900000 | 0.002456s | 26.05x | 0.003482s | 18.38x | 0.063990s |
| table_move_overlap_backward | 900000 | 0.002446s | 26.10x | 0.003430s | 18.61x | 0.063832s |
| table_string_key_insert | 756600 | 0.005819s | 3.90x | 0.008892s | 2.55x | 0.022704s |
| table_concat_numbers | 1638000 | 0.001651s | 12.35x | 0.001810s | 11.27x | 0.020390s |
| string_find_plain | 520000 | 0.000041s | 91.85x | 0.003799s | 0.99x | 0.003766s |
| string_find_init | 147568 | 0.001768s | 4.12x | 0.006541s | 1.11x | 0.007283s |
| string_find_pattern | 520000 | 0.004705s | 1.42x | 0.005652s | 1.18x | 0.006686s |
| string_find_class | 240000 | 0.007017s | 1.32x | 0.007788s | 1.19x | 0.009282s |
| string_gsub | 23520000 | 0.023013s | 3.54x | 0.024160s | 3.37x | 0.081527s |
| string_gsub_capture | 6048000 | 0.006163s | 6.76x | 0.007481s | 5.57x | 0.041691s |
| string_gsub_func | 9600000 | 0.007604s | 15.34x | 0.007668s | 15.22x | 0.116683s |
| string_gsub_table | 11200000 | 0.005418s | 20.83x | 0.005429s | 20.78x | 0.112833s |
| string_gsub_no_match | 28800000 | 0.000605s | 146.06x | 0.000743s | 118.93x | 0.088367s |
| string_lower_upper | 20160000 | 0.006947s | 2.25x | 0.007131s | 2.20x | 0.015665s |
| string_reverse_loop | 16800000 | 0.000951s | 5.55x | 0.001042s | 5.06x | 0.005277s |
| string_rep_loop | 8249776 | 0.009106s | 1.78x | 0.011159s | 1.46x | 0.016240s |
| string_char_loop | 1500000 | 0.008035s | 1.83x | 0.012911s | 1.14x | 0.014699s |
| string_byte_range | 800000 | 0.003185s | 21.44x | 0.052339s | 1.30x | 0.068294s |
| string_gmatch_captures | 3600000 | 0.062942s | 0.67x | 0.035137s | 1.20x | 0.042086s |
| string_match_repeated | 1170000 | 0.009912s | 1.27x | 0.010864s | 1.16x | 0.012559s |
| string_format_int64 | 1830100 | 0.023889s | 0.63x | 0.005791s | 2.62x | 0.015160s |
| string_format_float | 1430371 | 0.010597s | 2.07x | 0.013436s | 1.64x | 0.021968s |
| string_format_wide_hex | 1460100 | 0.003845s | 3.69x | 0.005067s | 2.80x | 0.014174s |
| string_pack_unpack_mixed | 171822 | 0.023087s | 0.91x | 0.022570s | 0.94x | 0.021111s |
| string_pack_unpack_many_i8 | 285000 | 0.011546s | 1.12x | 0.006382s | 2.03x | 0.012924s |
| math_sin | -0.116934 | 0.004809s | 6.00x | 0.020162s | 1.43x | 0.028861s |
| math_floor | 125000 | 0.001650s | 25.91x | 0.035358s | 1.21x | 0.042753s |
| math_abs_int64 | 500000 | 0.028403s | 1.60x | 0.069202s | 0.65x | 0.045303s |
| math_fmod_int64 | 992779 | 0.018555s | 2.63x | 0.047286s | 1.03x | 0.048725s |
| math_sqrt_log | 580649595.776236 | 0.001469s | 15.92x | 0.018539s | 1.26x | 0.023388s |
| math_type_loop | 2500000 | 0.028798s | 1.31x | 0.030356s | 1.24x | 0.037615s |
| math_tointeger_loop | 250000 | 0.042285s | 1.33x | 0.052657s | 1.07x | 0.056139s |
| math_ult_loop | 0 | 0.003582s | 26.55x | 0.061972s | 1.53x | 0.095117s |
| math_minmax_int64 | 257567 | 0.010271s | 6.81x | 0.073738s | 0.95x | 0.069989s |
| math_minmax_mixed_int64 | 79040 | 0.007774s | 6.61x | 0.043427s | 1.18x | 0.051395s |
| coroutine_resume | 250000 | 0.013214s | 1.19x | 0.019755s | 0.80x | 0.015741s |
| coroutine_resume_args | 269999 | 0.009788s | 1.17x | 0.014626s | 0.78x | 0.011439s |
| coroutine_yield_multi | 360000 | 0.010279s | 1.18x | 0.015063s | 0.81x | 0.012127s |
| coroutine_wrap_loop | 220000 | 0.005103s | 2.41x | 0.005263s | 2.33x | 0.012273s |
| numeric_for_down | 999827 | 0.008339s | 4.84x | 0.044688s | 0.90x | 0.040369s |
| branch_mod_loop | 0 | 0.042531s | 2.23x | 0.281862s | 0.34x | 0.094650s |
| bit_rotate64 | 1394249097 | 0.025951s | 3.59x | 0.139415s | 0.67x | 0.093203s |
| table_len_loop | 0 | 0.007503s | 13.92x | 0.100466s | 1.04x | 0.104474s |
| table_insert_remove | 360000 | 0.037969s | 2.29x | 0.069685s | 1.25x | 0.086803s |
| table_unpack_multi | 0 | 0.015123s | 1.89x | 0.022963s | 1.24x | 0.028563s |
| string_byte_sum | 200000 | 0.009814s | 23.09x | 0.217531s | 1.04x | 0.226618s |
| string_sub_loop | 5600000 | 0.000656s | 25.57x | 0.013746s | 1.22x | 0.016772s |
| string_format_num | 1688895 | 0.003466s | 5.29x | 0.006111s | 3.00x | 0.018340s |
| string_match_pattern | 1080000 | 0.016701s | 1.19x | 0.018251s | 1.09x | 0.019956s |
| string_pack_unpack_i4 | 500000 | 0.016199s | 1.71x | 0.016672s | 1.66x | 0.027661s |
| string_pack_unpack_i8 | 770000 | 0.009688s | 1.78x | 0.012550s | 1.37x | 0.017246s |
| tonumber_parse | 839824 | 0.025995s | 1.75x | 0.033427s | 1.36x | 0.045485s |
| tonumber_base16 | 900816 | 0.004652s | 4.40x | 0.008161s | 2.51x | 0.020489s |
| tostring_int | 7288896 | 0.099915s | 1.09x | 0.065912s | 1.66x | 0.109354s |
| tostring_float | 4388895 | 0.034367s | 2.55x | 0.032959s | 2.66x | 0.087662s |
| math_min_max | 970055 | 0.013282s | 22.83x | 0.228935s | 1.32x | 0.303247s |
| closure_upvalue_update | 250000 | 0.009862s | 2.94x | 0.019059s | 1.52x | 0.028990s |
| upvalue_read_loop | 0 | 0.001842s | 34.48x | 0.054465s | 1.17x | 0.063514s |
| method_call | 250000 | 0.009870s | 2.90x | 0.024699s | 1.16x | 0.028650s |
| select_vararg | 900000 | 0.011916s | 5.60x | 0.081020s | 0.82x | 0.066781s |
| vararg_sum | 450000 | 0.015364s | 6.53x | 0.110130s | 0.91x | 0.100259s |
| tail_call_pair | 500000 | 0.001755s | 48.85x | 0.058854s | 1.46x | 0.085731s |
| metamethod_index | 400000 | 0.000281s | 52.17x | 0.017126s | 0.86x | 0.014659s |
| metamethod_index_table | 600000 | 0.000580s | 33.49x | 0.030099s | 0.65x | 0.019422s |
| metamethod_add | 250000 | 0.000169s | 52.07x | 0.009387s | 0.94x | 0.008800s |
| metamethod_call | 50000 | 0.000163s | 51.18x | 0.007538s | 1.11x | 0.008343s |
| metamethod_len | 400000 | 0.000281s | 46.85x | 0.014594s | 0.90x | 0.013166s |
| metamethod_newindex | 400000 | 0.002667s | 4.12x | 0.011666s | 0.94x | 0.010989s |
| metamethod_compare | 600000 | 0.000412s | 102.82x | 0.034834s | 1.22x | 0.042361s |
| metamethod_eq_false | 900000 | 0.000209s | 74.29x | 0.012422s | 1.25x | 0.015527s |
| metamethod_concat | 360000 | 0.007499s | 1.25x | 0.008648s | 1.09x | 0.009402s |
| global_read_loop | 0 | 0.004602s | 12.98x | 0.071988s | 0.83x | 0.059725s |
| global_write_loop | 0 | 0.001121s | 8.79x | 0.012903s | 0.76x | 0.009852s |
| table_field_read_loop | 0 | 0.013646s | 12.95x | 0.190074s | 0.93x | 0.176682s |
| table_field_write_loop | 0 | 0.015688s | 2.12x | 0.031822s | 1.04x | 0.033216s |
| table_array_append_direct | 382400 | 0.001485s | 4.71x | 0.011932s | 0.59x | 0.006989s |
| table_array_pop_direct | 894400 | 0.010167s | 1.61x | 0.019753s | 0.83x | 0.016358s |
| table_unpack_32 | 200000 | 0.014560s | 4.60x | 0.041583s | 1.61x | 0.066941s |
| table_concat_slice | 1072000 | 0.000998s | 2.93x | 0.001395s | 2.10x | 0.002924s |
| table_sort_nearly_sorted | 280070 | 0.001067s | 19.08x | 0.001763s | 11.55x | 0.020362s |
| table_sort_many_tiny | 279345 | 0.000221s | 6.00x | 0.000467s | 2.84x | 0.001327s |
| table_sort_cmp_int64 | 359520 | 0.006222s | 1.39x | 0.007777s | 1.12x | 0.008676s |
| string_len_loop | 0 | 0.004570s | 13.73x | 0.056418s | 1.11x | 0.062734s |
| string_byte_single | 647155 | 0.004731s | 19.26x | 0.091942s | 0.99x | 0.091127s |
| string_sub_negative | 5950000 | 0.000722s | 24.39x | 0.015900s | 1.11x | 0.017609s |
| string_compare_loop | 0 | 0.034820s | 1.48x | 0.066194s | 0.78x | 0.051365s |
| string_intern_concat | 2270694 | 0.005082s | 4.31x | 0.006993s | 3.13x | 0.021890s |
| string_format_quote | 938894 | 0.001027s | 6.47x | 0.002219s | 3.00x | 0.006648s |
| tonumber_float_parse | 986882480.496089 | 0.013386s | 2.12x | 0.016382s | 1.73x | 0.028387s |
| tonumber_int64_parse | 600000 | 0.006078s | 1.63x | 0.008802s | 1.12x | 0.009897s |
| math_abs_small | 0 | 0.058709s | 4.35x | 0.215764s | 1.18x | 0.255514s |
| math_floor_negative | 357142 | 0.002144s | 17.55x | 0.036643s | 1.03x | 0.037621s |
| math_type_mixed | 0 | 0.024128s | 1.58x | 0.037217s | 1.02x | 0.038144s |
| float_idiv_mod | 999985 | 0.045462s | 1.71x | 0.046225s | 1.68x | 0.077518s |
| bit_mixed_const32 | 332866815 | 0.070048s | 1.61x | 0.094885s | 1.19x | 0.112757s |
| bit_chain64_mixed | 377178668 | 0.007758s | 6.66x | 0.046831s | 1.10x | 0.051681s |
| bit_shift_signed64 | 603754476 | 0.005888s | 5.71x | 0.068087s | 0.49x | 0.033633s |
| int64_unary_minus | 0 | 0.016355s | 2.32x | 0.056071s | 0.68x | 0.037917s |
| int64_compare_zero_branch | 0 | 0.050670s | 1.06x | 0.053802s | 1.00x | 0.053885s |
| int64_add_compare_loop | 900011 | 0.017343s | 2.23x | 0.048360s | 0.80x | 0.038675s |
| numeric_for_float_span | 999278 | 0.095386s | 1.10x | 0.032460s | 3.23x | 0.104751s |
| while_loop_countdown | 998963 | 0.019348s | 5.46x | 0.226420s | 0.47x | 0.105700s |
| repeat_until_countdown | 998963 | 0.018428s | 5.61x | 0.143633s | 0.72x | 0.103446s |
| generic_for_iter_closure | 0 | 0.002898s | 25.37x | 0.069622s | 1.06x | 0.073523s |
| pairs_update_values | 374000 | 0.046544s | 2.54x | 0.075405s | 1.57x | 0.118308s |
| closure_factory | 600000 | 0.012826s | 1.49x | 0.012694s | 1.50x | 0.019104s |
| nested_closure_call | 0 | 0.000481s | 41.06x | 0.018845s | 1.05x | 0.019750s |
| tail_call_chain | 500000 | 0.001176s | 91.75x | 0.053688s | 2.01x | 0.107894s |
| vararg_select_tail | 500000 | 0.009648s | 2.80x | 0.036954s | 0.73x | 0.027015s |
| metamethod_bitwise | 834592 | 0.003753s | 1.09x | 0.003979s | 1.03x | 0.004097s |
| metamethod_idiv | 127156 | 0.001380s | 2.68x | 0.004671s | 0.79x | 0.003694s |
| metamethod_pairs_loop | 700000 | 0.017879s | 2.15x | 0.019387s | 1.99x | 0.038521s |
| load_string_loop | 199148 | 0.006212s | 1.62x | 0.006492s | 1.55x | 0.010060s |
| debug_getinfo_loop | 700000 | 0.008239s | 2.12x | 0.008649s | 2.02x | 0.017471s |
| gc_table_alloc_collect | 332325 | 0.124085s | 0.35x | 0.124435s | 0.35x | 0.043824s |
| gc_string_churn | 1350000 | 0.004141s | 2.09x | 0.005747s | 1.51x | 0.008674s |
| pcall_error | 120000 | 0.002306s | 4.29x | 0.002714s | 3.65x | 0.009904s |
| xpcall_error | 80000 | 0.003752s | 2.00x | 0.007367s | 1.02x | 0.007500s |
| pcall_multi_return | 900000 | 0.001247s | 4.69x | 0.003634s | 1.61x | 0.005850s |
| pcall_vararg_success | 90000 | 0.000559s | 11.28x | 0.005811s | 1.09x | 0.006305s |
| coroutine_create_resume | 120000 | 0.010857s | 0.92x | 0.011108s | 0.90x | 0.009954s |
| string_gmatch_words | 2800000 | 0.014876s | 1.45x | 0.015617s | 1.38x | 0.021505s |
| utf8_codes_loop | 760000 | 0.022510s | 1.08x | 0.022801s | 1.07x | 0.024308s |
| utf8_codes_lax_loop | 760000 | 0.022067s | 1.09x | 0.022498s | 1.07x | 0.024147s |
| utf8_len_loop | 5760000 | 0.009603s | 1.01x | 0.009728s | 1.00x | 0.009685s |
| utf8_len_ascii | 34560000 | 0.011662s | 2.42x | 0.012414s | 2.27x | 0.028241s |
| utf8_codepoint_loop | 1200000 | 0.002582s | 1.17x | 0.002549s | 1.18x | 0.003020s |
| utf8_char_build | 1600000 | 0.008839s | 1.99x | 0.010736s | 1.64x | 0.017592s |
| utf8_offset_loop | 509980 | 0.004323s | 1.18x | 0.005085s | 1.00x | 0.005091s |
| int64_add_const_box | 627776 | 0.028574s | 1.04x | 0.029410s | 1.01x | 0.029791s |
| int64_sub_zero_compare | 0 | 0.002348s | 12.79x | 0.072564s | 0.41x | 0.030031s |
| int64_mul_const_wrap | 706027200 | 0.012421s | 3.28x | 0.026087s | 1.56x | 0.040740s |
| idiv_dynamic | 90327 | 0.018975s | 2.70x | 0.072961s | 0.70x | 0.051176s |
| mod_dynamic | 936739 | 0.015380s | 4.18x | 0.082509s | 0.78x | 0.064352s |
| numeric_for_int64_down | 600376 | 0.011601s | 1.76x | 0.025497s | 0.80x | 0.020407s |
| numeric_for_int64_step3 | 297 | 0.011631s | 1.74x | 0.024544s | 0.82x | 0.020240s |
| bit_and64_power2_mask | 1081168960 | 0.014956s | 2.22x | 0.051288s | 0.65x | 0.033236s |
| bit_or64_small_const | 1087329633 | 0.016946s | 2.09x | 0.076870s | 0.46x | 0.035376s |
| bit_xor64_small_const | 9000000 | 0.011992s | 2.50x | 0.038160s | 0.79x | 0.029986s |
| bit_shift64_large_count | 7000000 | 0.004910s | 9.58x | 0.037558s | 1.25x | 0.047042s |
| bit_shift64_neg_count | 26591 | 0.005874s | 5.97x | 0.073876s | 0.47x | 0.035058s |
| bit_branch32_test | 804674 | 0.009765s | 7.79x | 0.162170s | 0.47x | 0.076111s |
| table_rawget_int64_key | 114272 | 0.029899s | 1.10x | 0.042620s | 0.77x | 0.032963s |
| table_rawset_int64_key | 350000 | 0.010146s | 1.90x | 0.018388s | 1.05x | 0.019305s |
| table_set_int64_float_equiv | 90000 | 0.003929s | 2.88x | 0.009955s | 1.14x | 0.011309s |
| table_string_key_miss | 700000 | 0.005779s | 7.01x | 0.012395s | 3.27x | 0.040495s |
| table_sort_duplicates | 1540 | 0.003309s | 7.96x | 0.004433s | 5.94x | 0.026339s |
| table_sort_cmp_strings | 630 | 0.008523s | 1.54x | 0.011055s | 1.18x | 0.013097s |
| table_concat_large | 5116000 | 0.003442s | 2.67x | 0.003497s | 2.63x | 0.009188s |
| table_unpack_slice_16 | 800000 | 0.015662s | 3.42x | 0.042491s | 1.26x | 0.053544s |
| string_find_absent_plain | 180000 | 0.000042s | 155.07x | 0.006747s | 0.97x | 0.006513s |
| string_find_long_plain | 880000 | 0.000029s | 246.28x | 0.006984s | 1.02x | 0.007142s |
| string_match_no_capture | 1260000 | 0.012156s | 1.23x | 0.013017s | 1.15x | 0.014932s |
| string_gsub_literal | 12600000 | 0.004291s | 9.08x | 0.004503s | 8.66x | 0.038976s |
| string_sub_large_slice | 28800000 | 0.015913s | 1.04x | 0.019990s | 0.83x | 0.016523s |
| string_byte_full_short | 400000 | 0.002487s | 21.83x | 0.042304s | 1.28x | 0.054302s |
| string_char_many | 2400000 | 0.019516s | 1.40x | 0.030361s | 0.90x | 0.027402s |
| string_rep_sep | 4410000 | 0.002549s | 4.03x | 0.007586s | 1.35x | 0.010265s |
| string_format_mixed | 1536768 | 0.006919s | 2.17x | 0.007208s | 2.09x | 0.015041s |
| string_pack_unpack_float | 736924 | 0.014279s | 1.29x | 0.017342s | 1.07x | 0.018488s |
| tonumber_hex64_parse | 263472 | 0.004246s | 1.69x | 0.006280s | 1.14x | 0.007180s |
| math_modf_loop | 750042 | 0.017245s | 3.76x | 0.071746s | 0.90x | 0.064837s |
| math_floor_int_passthrough | 0 | 0.002343s | 46.06x | 0.076233s | 1.42x | 0.107927s |
| math_tointeger_int64_string | 46640 | 0.004810s | 1.67x | 0.006854s | 1.17x | 0.008024s |
| metamethod_order_int64 | 800000 | 0.000324s | 65.73x | 0.020540s | 1.04x | 0.021296s |
| pcall_deep_stack | 240000 | 0.000378s | 15.13x | 0.002498s | 2.29x | 0.005719s |
| coroutine_pingpong | 240001 | 0.009042s | 1.18x | 0.013799s | 0.77x | 0.010654s |
| debug_getlocal_loop | 40001 | 0.001247s | 0.82x | 0.001754s | 0.58x | 0.001018s |
| gc_table_churn_no_collect | 478400 | 0.000421s | 59.52x | 0.013435s | 1.87x | 0.025059s |
| table_next_array_extra | 0 | 0.270524s | 1.60x | 0.250325s | 1.73x | 0.433601s |
| table_next_sparse_extra | 600000 | 0.406817s | 1.01x | 0.387579s | 1.06x | 0.411620s |
| table_pairs_array_extra | 0 | 0.131546s | 2.12x | 0.131111s | 2.13x | 0.278829s |
| table_sort_cmp_upvalue_extra | 949178 | 0.002978s | 12.10x | 0.004386s | 8.22x | 0.036034s |
| table_sort_records_extra | 25802 | 0.008182s | 1.77x | 0.012647s | 1.15x | 0.014500s |
| table_sort_large_int_extra | 815810 | 0.003280s | 4.94x | 0.003598s | 4.51x | 0.016214s |
| string_concat_small_extra | 2160000 | 0.049166s | 1.50x | 0.059996s | 1.23x | 0.073644s |
| string_sub_tiny_extra | 1200000 | 0.000984s | 27.94x | 0.024552s | 1.12x | 0.027497s |
| string_match_captures_extra | 1800000 | 0.012237s | 1.25x | 0.013251s | 1.16x | 0.015357s |
| string_arith_int64_extra | 885000 | 0.000709s | 2.93x | 0.001728s | 1.20x | 0.002080s |
| bit64_and_shift_extra | 2019367627 | 0.013550s | 3.07x | 0.082021s | 0.51x | 0.041531s |
| bit64_branch_not_extra | 0 | 0.005049s | 7.56x | 0.080747s | 0.47x | 0.038147s |
| int64_compare_threshold_extra | 995968 | 0.005810s | 3.99x | 0.063951s | 0.36x | 0.023194s |
| int64_minmax_chain_extra | 0 | 0.011383s | 7.34x | 0.108731s | 0.77x | 0.083576s |
| int64_compare_eq_const_extra | 41016 | 0.003402s | 8.10x | 0.065383s | 0.42x | 0.027558s |
| int64_compare_between_extra | 12 | 0.006093s | 3.15x | 0.048840s | 0.39x | 0.019214s |
| int64_compare_descending_extra | 4032 | 0.006168s | 3.85x | 0.062295s | 0.38x | 0.023719s |
| bit64_const_mask_chain_extra | 65477308 | 0.013461s | 2.06x | 0.037236s | 0.75x | 0.027764s |
| bit64_const_or_xor_chain_extra | 7000001 | 0.012298s | 2.26x | 0.030329s | 0.92x | 0.027801s |
| bit64_compare_mask_zero_extra | 109564 | 0.007781s | 2.78x | 0.035126s | 0.62x | 0.021655s |
| table_next_mixed_extra | 744000 | 0.171199s | 1.45x | 0.163183s | 1.52x | 0.248716s |
| table_pairs_sparse_extra | 600000 | 0.306750s | 0.99x | 0.286051s | 1.06x | 0.302160s |
| table_pairs_int64_keys_extra | 520000 | 0.121261s | 1.54x | 0.226345s | 0.82x | 0.186382s |
| load_dump_many_protos_extra | 54000 | 0.000234s | 3.93x | 0.000216s | 4.25x | 0.000919s |
| debug_getupvalue_extra | 80000 | 0.001594s | 1.15x | 0.002090s | 0.87x | 0.001828s |
| debug_setupvalue_extra | 467080 | 0.001543s | 1.33x | 0.002474s | 0.83x | 0.002053s |
| closure_call_mixed_extra | 910272 | 0.001157s | 16.18x | 0.014471s | 1.29x | 0.018723s |
| load_dump_extra | 762500 | 0.000364s | 5.59x | 0.000374s | 5.44x | 0.002034s |
| bit64_rotate_var_extra | 1807377590 | 0.010247s | 3.86x | 0.082881s | 0.48x | 0.039593s |
| bit64_extract_insert_extra | 536706047 | 0.009598s | 5.67x | 0.192090s | 0.28x | 0.054381s |
| bit32_extract_branch_extra | 154058 | 0.053117s | 3.47x | 0.540472s | 0.34x | 0.184082s |
| bit64_math_ult_branch_extra | 999988 | 0.012718s | 7.20x | 0.122690s | 0.75x | 0.091627s |
| int64_compare_float_threshold_extra | 997120 | 0.012856s | 1.39x | 0.057903s | 0.31x | 0.017835s |
| int64_compare_table_bound_extra | 328 | 0.006842s | 3.10x | 0.053774s | 0.39x | 0.021203s |
| numeric_for_int64_large_step_extra | 927776 | 0.016202s | 1.06x | 0.018580s | 0.92x | 0.017094s |
| table_next_int64_keys_extra | 520000 | 0.195681s | 1.31x | 0.273482s | 0.94x | 0.256561s |
| table_pairs_string_keys_extra | 568000 | 0.161985s | 1.68x | 0.236614s | 1.15x | 0.272157s |
| table_pairs_mixed_sparse_extra | 704000 | 0.372349s | 0.97x | 0.356815s | 1.01x | 0.360136s |
| table_sort_tiny_cmp_extra | 126926 | 0.004009s | 1.47x | 0.005636s | 1.05x | 0.005906s |
| table_sort_cmp_desc_extra | 870240 | 0.001538s | 32.16x | 0.003346s | 14.78x | 0.049455s |
| table_sort_large_strings_extra | 504 | 0.002563s | 2.99x | 0.003591s | 2.13x | 0.007664s |
| table_sort_wide_records_extra | 35772 | 0.007505s | 1.60x | 0.011743s | 1.02x | 0.011994s |
| table_sort_equal_keys_extra | 62857 | 0.015250s | 1.59x | 0.023398s | 1.04x | 0.024303s |
| table_move_int64_extra | 625000 | 0.064829s | 1.21x | 0.065125s | 1.20x | 0.078278s |
| string_find_frontier_extra | 2520000 | 0.007420s | 1.29x | 0.008383s | 1.14x | 0.009554s |
| string_gsub_many_repl_extra | 672000 | 0.003805s | 1.52x | 0.004029s | 1.44x | 0.005795s |
| string_format_int64_mix_extra | 2730000 | 0.002292s | 6.35x | 0.005617s | 2.59x | 0.014565s |
| gc_closure_alloc_collect_extra | 103040 | 0.089693s | 0.35x | 0.091351s | 0.34x | 0.031506s |
