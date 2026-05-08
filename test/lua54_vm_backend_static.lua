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
  assert(close:find("lj_close_prepare_pcall", 1, true),
	 "src/lj_close.c: missing close pcall preparation helper")
  assert(close:find("lj_close_continue_pcall", 1, true),
	 "src/lj_close.c: missing close pcall continuation helper")
  assert(close:find("lj_close_prepare_cframe_pcall", 1, true),
	 "src/lj_close.c: missing C-return close pcall preparation helper")
  assert(close:find("lj_close_continue_cframe_pcall", 1, true),
	 "src/lj_close.c: missing C-return close pcall continuation helper")
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

print("lua54_vm_backend_static.lua OK")
