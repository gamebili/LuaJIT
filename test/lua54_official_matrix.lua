local default_dir = os.getenv("LUA54_TESTES_DIR") or
  "D:/p4_gl2/pristine/tools/lua/lua-5.4.8-src/lua-5.4.8/testes"
local dir = arg[1] or default_dir
local case_arg = 2

if dir == "--case" then
  dir = default_dir
  case_arg = 1
end

local requested_case
if arg[case_arg] == "--case" then
  requested_case = assert(arg[case_arg + 1],
    "missing official Lua 5.4 matrix case name after --case")
end

dir = dir:gsub("\\", "/"):gsub("/$", "")

local exe = arg[-1] or arg[-2] or "./src/luajit"
local lua_path = dir .. "/?.lua;" .. dir .. "/?/init.lua;;"

if jit then jit.off() end

local function dq(s)
  return '"' .. tostring(s):gsub('"', '\\"') .. '"'
end

local function longstr(s)
  return "[=[" .. tostring(s):gsub("]=]", "]= ]") .. "]=]"
end

local function readfile(path)
  local f = assert(io.open(path, "rb"))
  local data = f:read("*a")
  assert(f:close())
  return data
end

local function run_child(name, code)
  -- The matrix cases run in parallel under make -jN. os.tmpname() deletes its
  -- Win32 placeholder file before returning, so concurrent luajit processes
  -- can be handed the same name and overwrite each other's wrapper chunk.
  -- Case names are unique within one matrix run, so derive a deterministic
  -- per-case wrapper path instead.
  local tmpdir = (os.getenv("TMP") or os.getenv("TEMP") or "."):gsub("\\", "/"):gsub("/$", "")
  local tmp = tmpdir .. "/lua54_official_" .. name:gsub("[^%w]", "_") .. ".lua"
  local f = assert(io.open(tmp, "w"))
  f:write("if jit then jit.off() end\n",
	  "package.path=", longstr(lua_path), "\n", code, "\n")
  assert(f:close())

  local cmd
  if package.config:sub(1, 1) == "\\" then
    -- Windows cmd.exe needs the extra outer quote pair only when the
    -- executable path itself must be quoted.
    if exe:find("%s") then
      cmd = 'cmd /c ""' .. exe .. '" "' .. tmp .. '""'
    else
      cmd = exe .. " " .. dq(tmp)
    end
  else
    cmd = dq(exe) .. " " .. dq(tmp)
  end
  if os.getenv("LUA54_MATRIX_ECHO") then print(cmd) end
  local ok, why, status = os.execute(cmd)
  os.remove(tmp)
  if ok ~= true and ok ~= 0 then
    error(("official Lua 5.4 test failed: %s (%s, %s)"):format(name, tostring(why), tostring(status)))
  end
end

local function run(name, fn)
  print("official54: " .. name)
  run_child(name, fn())
end

local function runfile(name, expected, opts)
  run(name, function()
    local code = ""
    if opts and opts.port ~= nil then
      code = code .. "_port=" .. tostring(opts.port) .. "\n"
    end
    if opts and opts.soft ~= nil then
      code = code .. "_soft=" .. tostring(opts.soft) .. "\n"
    end
    if opts and opts.patch_constructs_load_gc then
      code = code .. table.concat({
        "local path=" .. longstr(dir .. "/" .. name),
        "local f=assert(io.open(path, 'rb'))",
        "local data=f:read('*a')",
        "assert(f:close())",
        "local n",
        "data,n=data:gsub('local p = load%(' ..",
        "  'string%.format%(%s*prog,%s*s,%s*s%),' ..",
        "  '%s*\"\"%)',",
        "  'collectgarbage(\"collect\")\\n    local p = load(string.format(prog, s, s), \"\")')",
        "assert(n == 1)",
        "data=data:gsub('assert%(p%(%) == v%[2%] and IX == not not v%[2%]%)',",
        "  'collectgarbage(\"collect\")\\n    assert(p() == v[2] and IX == not not v[2])')",
        "local r=assert(load(data, path, 't'))()",
      }, "\n") .. "\n"
    else
      code = code .. "local r=dofile(" .. longstr(dir .. "/" .. name) .. ")\n"
    end
    if expected ~= nil then
      code = code .. "assert(r==" .. tostring(expected) .. ")\n"
    end
    return code
  end)
end

local function run_calls_prebinary()
  run("calls.lua pre-binary", function()
    local path = dir .. "/calls.lua"
    local data = readfile(path)
    local marker = 'print("testing binary chunks")'
    assert(data:find(marker, 1, true),
      "official calls.lua layout changed: missing binary chunk marker")
    return table.concat({
      "local path=" .. longstr(path),
      "local marker=" .. longstr(marker),
      "local f=assert(io.open(path, 'rb'))",
      "local data=f:read('*a')",
      "assert(f:close())",
      "local pos=assert(data:find(marker, 1, true))",
      "-- LuaJIT intentionally keeps LuaJIT bytecode instead of PUC-Rio 5.4 binary chunks.",
      "-- Still run everything before the official binary-header assertions.",
      "assert(load(data:sub(1, pos - 1), path, 't'))()",
    }, "\n")
  end)
end

local direct = {
  "literals.lua",
  "strings.lua",
  "pm.lua",
  "math.lua",
  "bitwise.lua",
  "bwcoercion.lua",
  "gc.lua",
  "gengc.lua",
  "tracegc.lua",
  "tpack.lua",
  "closure.lua",
  "cstack.lua",
  "constructs.lua",
  "goto.lua",
  "db.lua",
  "nextvar.lua",
  "utf8.lua",
  "vararg.lua",
  "sort.lua",
  "errors.lua",
  "coroutine.lua",
  "code.lua",
  "api.lua",
}

local direct_opts = {
  ["constructs.lua"] = { patch_constructs_load_gc = true, soft = true },
}

local case_order = {}
local case_fns = {}

local function add_case(name, fn)
  assert(case_fns[name] == nil, "duplicate official Lua 5.4 matrix case: " .. name)
  case_order[#case_order + 1] = name
  case_fns[name] = fn
end

for _, name in ipairs(direct) do
  add_case(name, function()
    runfile(name, nil, direct_opts[name])
  end)
end

add_case("calls-prebinary", run_calls_prebinary)

-- Windows stdin seek behavior differs from the Unix-like assumption guarded by
-- _port in the official file; keep the rest of files.lua active.
add_case("files.lua", function()
  runfile("files.lua", nil, { port = true })
end)
add_case("attrib.lua", function()
  runfile("attrib.lua", 27, { port = true })
end)
add_case("locals.lua", function()
  runfile("locals.lua", 5)
end)
add_case("events.lua", function()
  runfile("events.lua", 12)
end)
add_case("verybig.lua", function()
  runfile("verybig.lua", 10)
end)

-- big.lua intentionally yields in the main chunk; official all.lua drives it
-- through coroutine.wrap, so the matrix mirrors that harness instead of direct
-- dofile execution.
add_case("big.lua", function()
  run("big.lua", function()
    return "local f=coroutine.wrap(assert(loadfile(" .. longstr(dir .. "/big.lua") ..
      ")))\nassert(f()==\"b\")\nassert(f()==\"a\")\n"
  end)
end)

if requested_case then
  local fn = case_fns[requested_case]
  assert(fn, "unknown official Lua 5.4 matrix case: " .. requested_case)
  fn()
  return
end

for _, name in ipairs(case_order) do
  case_fns[name]()
end

-- main.lua assumes a Unix-like shell and calls out through platform-specific
-- pipes/redirection. calls.lua is covered up to the documented LuaJIT bytecode
-- vs. official Lua 5.4 binary chunk boundary above.
print("official54: skipped main.lua on Windows shell portability boundary")
print("official54: skipped calls.lua binary chunk header block on documented boundary")
print("official54: OK")
