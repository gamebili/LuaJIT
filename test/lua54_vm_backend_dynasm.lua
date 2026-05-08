local function file_exists(path)
  local f = io.open(path, "rb")
  if f then
    f:close()
    return true
  end
  return false
end

local function find_minilua()
  local candidates = {
    "src/host/minilua.exe",
    "src/host/minilua",
  }
  for _, path in ipairs(candidates) do
    if file_exists(path) then return path end
  end
  error("missing src/host/minilua; run the build before lua54_vm_backend_dynasm.lua")
end

local minilua = find_minilua()
local dynasm = "dynasm/dynasm.lua"
assert(file_exists(dynasm), "missing dynasm/dynasm.lua")

local function quote_arg(arg)
  if package.config:sub(1, 1) == "\\" then
    arg = arg:gsub("/", "\\")
  end
  -- Windows cmd.exe treats forward slashes in executable paths as option
  -- separators, so paths are normalized to backslashes before quoting checks.
  if not arg:find('[%s"&()<>|^]') then return arg end
  return '"' .. arg:gsub('"', '\\"') .. '"'
end

local function run(cmd)
  local ok, why, code = os.execute(cmd)
  if ok == true or ok == 0 then return end
  error("command failed (" .. tostring(code or why or ok) .. "): " .. cmd)
end

local function dynasm_case(name, dasc, flags)
  return {
    name = name,
    dasc = dasc,
    flags = flags,
    out = "src/host/buildvm_arch_lua54_dynasm_" .. name .. ".h",
  }
end

-- This is a template-generation gate only. It catches Lua 5.4 VM wiring syntax
-- regressions on non-host backends; real artifact/runtime smoke stays in TODO.md.
local cases = {
  dynasm_case("x86_dualnum", "src/vm_x86.dasc",
	      {"-D", "ENDIAN_LE", "-D", "JIT", "-D", "FFI", "-D", "DUALNUM", "-D", "FPU", "-D", "HFABI", "-D", "VER=0"}),
  dynasm_case("arm_le_hardfp", "src/vm_arm.dasc",
	      {"-D", "ENDIAN_LE", "-D", "JIT", "-D", "FFI", "-D", "DUALNUM", "-D", "FPU", "-D", "HFABI", "-D", "VER=70"}),
  dynasm_case("arm_le_softfp", "src/vm_arm.dasc",
	      {"-D", "ENDIAN_LE", "-D", "JIT", "-D", "FFI", "-D", "DUALNUM", "-D", "VER=70"}),
  dynasm_case("mips32_le_hardfp", "src/vm_mips.dasc",
	      {"-D", "ENDIAN_LE", "-D", "JIT", "-D", "FFI", "-D", "DUALNUM", "-D", "FPU", "-D", "HFABI", "-D", "VER=10"}),
  dynasm_case("mips32_be_softfp", "src/vm_mips.dasc",
	      {"-D", "ENDIAN_BE", "-D", "JIT", "-D", "FFI", "-D", "DUALNUM", "-D", "VER=10"}),
  dynasm_case("mips64_le_hardfp", "src/vm_mips64.dasc",
	      {"-D", "ENDIAN_LE", "-D", "P64", "-D", "JIT", "-D", "FFI", "-D", "DUALNUM", "-D", "FPU", "-D", "HFABI", "-D", "VER=10"}),
  dynasm_case("mips64_be_softfp", "src/vm_mips64.dasc",
	      {"-D", "ENDIAN_BE", "-D", "P64", "-D", "JIT", "-D", "FFI", "-D", "DUALNUM", "-D", "VER=10"}),
  dynasm_case("ppc_be_hardfp", "src/vm_ppc.dasc",
	      {"-D", "ENDIAN_BE", "-D", "JIT", "-D", "FFI", "-D", "DUALNUM", "-D", "FPU", "-D", "HFABI", "-D", "VER=0"}),
  dynasm_case("ppc_be_softfp", "src/vm_ppc.dasc",
	      {"-D", "ENDIAN_BE", "-D", "JIT", "-D", "DUALNUM", "-D", "VER=0"}),
  dynasm_case("ppc_be_gpr64_toc", "src/vm_ppc.dasc",
	      {"-D", "ENDIAN_BE", "-D", "JIT", "-D", "DUALNUM", "-D", "FPU", "-D", "HFABI", "-D", "GPR64", "-D", "PPE", "-D", "TOC", "-D", "VER=0"}),
}

local function cleanup()
  for _, case in ipairs(cases) do
    os.remove(case.out)
  end
end

local function build_command(case)
  local args = {minilua, dynasm}
  for _, flag in ipairs(case.flags) do
    args[#args + 1] = flag
  end
  args[#args + 1] = "-o"
  args[#args + 1] = case.out
  args[#args + 1] = case.dasc
  for i, arg in ipairs(args) do
    args[i] = quote_arg(arg)
  end
  return table.concat(args, " ")
end

local function verify_output(case)
  local f, err = io.open(case.out, "rb")
  assert(f, case.name .. ": missing DynASM output: " .. tostring(err))
  local size = f:seek("end")
  f:close()
  assert(size and size > 0, case.name .. ": empty DynASM output")
end

local function main()
  cleanup()
  for _, case in ipairs(cases) do
    run(build_command(case))
    verify_output(case)
  end
end

local ok, err = xpcall(main, function(e)
  return tostring(e) .. "\n" .. debug.traceback()
end)
cleanup()
if not ok then error(err, 0) end

print("lua54_vm_backend_dynasm.lua OK")
