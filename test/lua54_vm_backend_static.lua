local files = {
  "src/vm_x86.dasc",
  "src/vm_arm.dasc",
  "src/vm_mips.dasc",
  "src/vm_mips64.dasc",
  "src/vm_ppc.dasc",
}

local close_cont_files = {
  "src/vm_arm.dasc",
  "src/vm_mips.dasc",
  "src/vm_mips64.dasc",
  "src/vm_ppc.dasc",
  "src/vm_x86.dasc",
  "src/vm_x64.dasc",
  "src/vm_arm64.dasc",
}

local function readfile(path)
  local f, err = io.open(path, "rb")
  assert(f, err)
  local data = f:read("*a")
  f:close()
  return data
end

local function count_plain(data, needle)
  local n, pos = 0, 1
  while true do
    pos = data:find(needle, pos, true)
    if not pos then return n end
    n = n + 1
    pos = pos + #needle
  end
end

local function assert_near(path, data, marker, needle, span)
  local pos = assert(data:find(marker, 1, true),
		     path .. ": missing " .. marker)
  local chunk = data:sub(pos, pos + span)
  assert(chunk:find(needle, 1, true),
	 path .. ": " .. marker .. " missing nearby " .. needle)
end

local function count_lua54_long_string_guards(data)
  local n = 0
  for _ in data:gmatch("LJ_STR_MAXSHORT") do
    n = n + 1
  end
  return n
end

local meta = readfile("src/lj_meta.c")
assert(meta:find("lj_meta_equal_lstr", 1, true),
       "src/lj_meta.c: missing common long-string equality helper")
assert(meta:find("lj_str_equal", 1, true),
       "src/lj_meta.c: common long-string equality helper must call lj_str_equal")

for _, path in ipairs(files) do
  local data = readfile(path)
  local guards = count_lua54_long_string_guards(data)
  -- Lua 5.4 only interns short strings. Every non-x64/ARM64 VM backend must
  -- avoid the string sid fast path for long string keys on TGETV/TGETS/TSETV/TSETS.
  assert(guards >= 4, path .. ": missing long-string table key fast-path guards")
  -- Runtime long strings are not interned in Lua 5.4. Equality fast paths must
  -- leave object-identity-only paths and delegate to the shared bytewise helper.
  assert(data:find("vmeta_equal_lstr", 1, true),
	 path .. ": missing long-string equality bytewise fallback")
end

do
  local frame = readfile("src/lj_frame.h")
  local close = readfile("src/lj_close.c")
  assert(frame:find("LJ_CONT_CLOSE", 1, true),
	 "src/lj_frame.h: missing close continuation id")
  assert(frame:find("LJ_CONT_CLOSE_CFRAME", 1, true),
	 "src/lj_frame.h: missing C-return close continuation id")
  assert(frame:find("LJ_CONT_CLOSE_RETURN", 1, true),
	 "src/lj_frame.h: missing Lua-return close continuation id")
  assert(frame:find("LJ_CONT_CLOSE_RETURN_HOOK", 1, true),
	 "src/lj_frame.h: missing Lua-return hook close continuation id")
  assert(close:find("lj_close_prepare_pcall", 1, true),
	 "src/lj_close.c: missing close pcall preparation helper")
  assert(close:find("lj_close_continue_pcall", 1, true),
	 "src/lj_close.c: missing close pcall continuation helper")
  assert(close:find("lj_close_prepare_cframe_pcall", 1, true),
	 "src/lj_close.c: missing C-return close pcall preparation helper")
  assert(close:find("lj_close_continue_cframe_pcall", 1, true),
	 "src/lj_close.c: missing C-return close pcall continuation helper")
  assert(close:find("lj_close_prepare_return_pcall", 1, true),
	 "src/lj_close.c: missing Lua-return close pcall preparation helper")
  assert(close:find("lj_close_continue_return_pcall", 1, true),
	 "src/lj_close.c: missing Lua-return close pcall continuation helper")
  assert(close:find("LJ_TARGET_ARM || LJ_TARGET_MIPS || LJ_TARGET_MIPS64 || LJ_TARGET_PPC || LJ_TARGET_X86 || LJ_TARGET_X64 || LJ_TARGET_ARM64", 1, true),
	 "src/lj_close.c: close continuation backend gate changed unexpectedly")
end

for _, path in ipairs(close_cont_files) do
  local data = readfile(path)
  -- Error-unwind __close can yield only if the VM backend enters a real pcall
  -- frame and resumes through LJ_CONT_CLOSE. This static gate complements the
  -- DynASM generation matrix; real artifact/runtime smoke stays platform-bound.
  assert(data:find("lj_close_prepare_pcall", 1, true),
	 path .. ": missing close pcall preparation call")
  assert(data:find("LJ_CONT_CLOSE", 1, true),
	 path .. ": missing close continuation dispatch id")
  assert(data:find("cont_close", 1, true),
	 path .. ": missing close continuation dispatch label")
  assert(data:find("lj_close_continue_pcall", 1, true),
	 path .. ": missing close pcall continuation call")
  assert(data:find("lj_close_prepare_cframe_pcall", 1, true),
	 path .. ": missing C-return close pcall preparation call")
  assert(data:find("LJ_CONT_CLOSE_CFRAME", 1, true),
	 path .. ": missing C-return close continuation dispatch id")
  assert(data:find("cont_close_cframe", 1, true),
	 path .. ": missing C-return close continuation dispatch label")
  assert(data:find("lj_close_continue_cframe_pcall", 1, true),
	 path .. ": missing C-return close pcall continuation call")
  assert(data:find("vm_returnc_closed", 1, true),
	 path .. ": missing shared C-return close completion label")
end

do
  local path = "src/vm_x64.dasc"
  local data = readfile(path)
  -- x64 is the first backend where close-active Lua return no longer lowers to
  -- parser pack/close/unpack helpers. The VM must close before return hooks and
  -- resume through a dedicated Lua-return continuation when __close yields.
  assert(data:find("lj_close_prepare_return_pcall", 1, true),
	 path .. ": missing Lua-return close pcall preparation call")
  assert(data:find("LJ_CONT_CLOSE_RETURN", 1, true),
	 path .. ": missing Lua-return close continuation dispatch id")
  assert(data:find("cont_close_return", 1, true),
	 path .. ": missing Lua-return close continuation dispatch label")
  assert(data:find("lj_close_continue_return_pcall", 1, true),
	 path .. ": missing Lua-return close pcall continuation call")
  assert(data:find("lj_close_prepare_return_hook_pcall", 1, true),
	 path .. ": missing Lua-return hook close pcall preparation call")
  assert(data:find("LJ_CONT_CLOSE_RETURN_HOOK", 1, true),
	 path .. ": missing Lua-return hook close continuation dispatch id")
  assert(data:find("cont_close_return_hook", 1, true),
	 path .. ": missing Lua-return hook close continuation dispatch label")
  assert(data:find("BC_RET_CLOSE_DONE", 1, true),
	 path .. ": missing shared Lua-return close completion label")
end

do
  local path = "src/vm_arm64.dasc"
  local data = readfile(path)
  local barrier_start = assert(data:find("|.macro barrierback", 1, true),
			       path .. ": missing ARM64 table write barrier")
  local barrier_end = assert(data:find("|.endmacro", barrier_start, true),
			     path .. ": unterminated ARM64 table write barrier")
  local barrier = data:sub(barrier_start, barrier_end)
  assert(barrier:find("GL->gc_mode54", 1, true) and
	 barrier:find("LJ_GC_AGE_TOUCHED1", 1, true) and
	 barrier:find("LJ_GC_AGE_TOUCHED2", 1, true) and
	 barrier:find("beq target", 1, true),
	 path .. ": ARM64 barrierback must preserve Lua 5.4 generational age")
  assert(not barrier:find("|9:", 1, true),
	 path .. ": ARM64 barrierback macro must not capture TSETV's >9 label")
  -- GC64 tags use bit 47. After adding the Lua 5.4 int64 GC type, LJ_TTAB
  -- became odd, so composing only the high 16 bits turns table values into
  -- userdata values on ARM64.
  assert(not data:find("LJ_TTAB>>1", 1, true),
	 path .. ": table TValue tag must preserve bit 47")
  assert(not data:find("LJ_TISNUM>>1", 1, true),
	 path .. ": integer TValue tag must preserve bit 47")
  assert(data:find("add CRET1, CRET1, TMP0, lsl #47", 1, true),
	 path .. ": table allocation must tag results with LJ_TTAB << 47")
  assert(data:find("vmeta_equal_i64", 1, true) and
	 data:find("lj_meta_equal_i64", 1, true),
	 path .. ": boxed int64 equality must use Lua 5.4 numeric semantics")
  assert(data:find("lj_meta_fori64", 1, true),
	 path .. ": boxed int64 numeric for must use Lua 5.4 helper")
  assert(data:find("mov RB, BASE", data:find("|->vmeta_binop", 1, true), true),
	 path .. ": ARM64 binop metamethod dispatch must preserve caller base")
  local unm = assert(data:find("case BC_UNM:", 1, true),
		     path .. ": missing ARM64 unary minus")
  local negzero = data:find("movz CARG3, #0x8000", unm, true)
  assert(negzero and
	 data:sub(math.max(unm, negzero - 128), negzero):find("#if !LJ_54",
							      1, true),
	 path .. ": ARM64 Lua 5.4 unary minus must keep integer zero")
  assert(data:find("sub RC, CARG1, RA", 1, true),
	 path .. ": coroutine resume after yield must count results from RA")
  assert(data:find("GL->gc.fin_check", 1, true) and
	 data:find("add TMP1, TMP1, #32", 1, true),
	 path .. ": ARM64 finalizer GC window must preserve nearby live slots")
  assert(data:find("lj_gc_step_fixtop", 1, true) and
	 data:find("ldr INSw, [PC, #-4]", 1, true) and
	 data:find("decode_RD RC, INS", 1, true),
	 path .. ": table allocation must reload destination after GC")
end

do
  local path = "src/vm_x86.dasc"
  local data = readfile(path)
  assert(data:find("|.macro clear_int64_owner", 1, true),
	 path .. ": missing boxed int64 owner clearing macro")
  assert(data:find("|.macro clear_int64_owner_ptr", 1, true),
	 path .. ": missing pointer boxed int64 owner clearing macro")
  assert(count_plain(data, "clear_int64_owner") >= 8,
	 path .. ": boxed int64 owner clearing is not wired through VM paths")
  assert(data:find("|.macro load_bitint64_1", 1, true) and
	 data:find("|.macro load_bitint64_2", 1, true) and
	 data:find("|.macro load_arithint64_1", 1, true) and
	 data:find("|.macro load_arithint64_2", 1, true) and
	 data:find("|.macro store_bitint64_1", 1, true) and
	 data:find("|.macro store_arithint64_1", 1, true),
	 path .. ": missing x64 boxed int64 fast-path macros")
  assert_near(path, data, "case BC_BAND:", "load_bitint64_2", 2400)
  assert_near(path, data, "case BC_BAND:", "store_bitint64_1", 2400)
  assert_near(path, data, "case BC_BOR:", "load_bitint64_2", 2400)
  assert_near(path, data, "case BC_BOR:", "store_bitint64_1", 2400)
  assert_near(path, data, "case BC_BXOR:", "load_bitint64_2", 2400)
  assert_near(path, data, "case BC_BXOR:", "store_bitint64_1", 2400)
  assert_near(path, data, "case BC_BSHL:", "load_bitint64_2", 2400)
  assert_near(path, data, "case BC_BSHL:", "store_bitint64_1", 2400)
  assert_near(path, data, "case BC_BSHR:", "load_bitint64_2", 2400)
  assert_near(path, data, "case BC_BSHR:", "store_bitint64_1", 2400)
  assert_near(path, data, "case BC_BNOT:", "load_bitint64_1", 1800)
  assert_near(path, data, "case BC_BNOT:", "store_bitint64_1", 1800)
  assert_near(path, data, "case BC_IDIV:", "load_arithint64_2", 2600)
  assert_near(path, data, "case BC_IDIV:", "store_arithint64_1", 2600)
end

do
  local path = "src/vm_x64.dasc"
  local data = readfile(path)
  assert(data:find("|.macro clear_int64_owner_val", 1, true),
	 path .. ": missing GC64 boxed int64 owner clearing macro")
  assert(data:find("|.macro load_int64_1", 1, true) and
	 data:find("|.macro load_int64_2", 1, true) and
	 data:find("|.macro store_int64_result", 1, true),
	 path .. ": missing GC64 boxed int64 fast-path macros")
  assert(count_plain(data, "clear_int64_owner_val") >= 10,
	 path .. ": GC64 boxed int64 owner clearing is not wired through VM copy paths")
  assert_near(path, data, "case BC_ADDVN:", "store_int64_result", 2600)
  assert_near(path, data, "case BC_SUBVN:", "store_int64_result", 2600)
  assert_near(path, data, "case BC_MULVN:", "store_int64_result", 2600)
  assert_near(path, data, "case BC_MODVN:", "store_int64_result", 3600)
  assert_near(path, data, "case BC_MODNV:", "store_int64_result", 3600)
  assert_near(path, data, "case BC_MODVV:", "store_int64_result", 3600)
  assert_near(path, data, "case BC_BAND:", "load_int64_2", 2600)
  assert_near(path, data, "case BC_BAND:", "store_int64_result", 2600)
  assert_near(path, data, "case BC_BSHL:", "load_int64_2", 2600)
  assert_near(path, data, "case BC_BSHL:", "store_int64_result", 2600)
  assert_near(path, data, "case BC_BNOT:", "load_int64_1", 1800)
  assert_near(path, data, "case BC_BNOT:", "store_int64_result", 1800)
  assert_near(path, data, "case BC_IDIV:", "load_int64_2", 3000)
  assert_near(path, data, "case BC_IDIV:", "store_int64_result", 3000)
  assert(data:find("push rdx", 1, true) and data:find("pop rdx", 1, true),
	 path .. ": GC64 idiv paths must preserve BASE in rdx")
end

do
  local path = "src/lj_obj.h"
  local data = readfile(path)
  assert(data:find("LJ_TARGET_X64", 1, true) and
	 not data:find("LJ_TARGET_X64 && !LJ_GC64", 1, true),
	 path .. ": x64 GC64 must keep boxed int64 owner support enabled")
end

do
  local path = "src/lj_dispatch.c"
  local data = readfile(path)
  assert(data:find("#if LJ_54 && LJ_TARGET_ARM64", 1, true),
	 path .. ": ARM64 hook deferral must stay target-gated")
  assert(data:find("case BC_CALL:", 1, true) and
	 data:find("bc_b(ins) != 0", 1, true),
	 path .. ": ARM64 fixed-result BC_CALL top slot must include results")
  assert(data:find("arm64_lua54_close_return_hook", 1, true) and
	 data:find("lj_debug_frame(L, 0", 1, true) and
	 data:find("close_return_hook && (g->hookmask & LUA_MASKRET)", 1, true),
	 path .. ": ARM64 close return hooks must hide internal close-pcall C returns")
end

do
  local path = "src/lj_gc.h"
  local data = readfile(path)
  assert(data:find("#if LJ_TARGET_ARM64", 1, true) and
	 data:find("LJ_GC_FIN_CHECK_CYCLES", 1, true),
	 path .. ": table-finalizer responsiveness must stay ARM64-gated")
end

do
  local path = "src/lj_meta.c"
  local data = readfile(path)
  assert(data:find("#if !LJ_TARGET_ARM64", 1, true) and
	 data:find("#if LJ_TARGET_ARM64", 1, true),
	 path .. ": bitop top adjustment must keep non-ARM64 behavior unchanged")
  assert(data:find("meta_arm64_live_top", 1, true) and
	 data:find("bad 'for' %s", 1, true),
	 path .. ": ARM64 numeric-for errors must protect live slots before formatting")
  assert(data:find("lj_gc_fullgc(L)", 1, true) and
	 data:find("G(L)->gc.threshold != LJ_MAX_MEM", 1, true),
	 path .. ": ARM64 bitwise error recovery must not run when GC is stopped")
end

print("lua54_vm_backend_static.lua OK")
