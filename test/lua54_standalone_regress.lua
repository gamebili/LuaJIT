local exe = arg[1] or arg[-1] or "./src/luajit"
local iswin = package.config:sub(1, 1) == "\\"
local prefix = "test/lua54_standalone_regress.tmp."
local files = {}

assert(_VERSION == "Lua 5.4", "standalone regressions require Lua 5.4 mode")

local function note(path)
  files[#files + 1] = path
  os.remove(path)
  return path
end

local function q(path)
  return '"' .. tostring(path):gsub('"', '\\"') .. '"'
end

local function readfile(path)
  local f = assert(io.open(path, "rb"))
  local data = f:read("*a")
  assert(f:close())
  return data:gsub("\r\n", "\n")
end

local function writefile(path, data, mode)
  local f = assert(io.open(path, mode or "wb"))
  f:write(data)
  assert(f:close())
  return path
end

local function cleanup()
  for i = #files, 1, -1 do
    os.remove(files[i])
  end
end

local function run(name, args, opts)
  opts = opts or {}
  local out = note(prefix .. name .. ".out")
  local err = note(prefix .. name .. ".err")
  local cmd = q(exe)
  if args and args ~= "" then cmd = cmd .. " " .. args end
  if opts.stdin then cmd = cmd .. " < " .. q(opts.stdin) end
  cmd = cmd .. " > " .. q(out) .. " 2> " .. q(err)
  if iswin then
    cmd = 'cmd /c "' .. cmd .. '"'
  end
  local ok, why, code = os.execute(cmd)
  local success = ok == true or ok == 0
  return {
    ok = success,
    why = why,
    code = code,
    out = readfile(out),
    err = readfile(err),
  }
end

local function expect_ok(name, args, expected, opts)
  local r = run(name, args, opts)
  assert(r.ok, name .. " failed: " .. r.err)
  assert(r.out == expected, name .. " stdout mismatch: " .. r.out)
  assert(r.err == "", name .. " stderr mismatch: " .. r.err)
end

local function expect_fail(name, args, needle, opts)
  local r = run(name, args, opts)
  assert(not r.ok, name .. " unexpectedly succeeded")
  assert((r.out .. r.err):find(needle, 1, true),
	 name .. " missing error fragment " .. needle .. ": " .. r.err)
end

local function expect_exit_failure(name, code)
  local script = note(prefix .. name .. ".lua")
  writefile(script, "os.exit(" .. code .. ", true)\n")
  local r = run(name, q(script))
  assert(not r.ok, name .. " unexpectedly succeeded")
  assert(r.out == "" and r.err == "", name .. " produced output")
end

local ok, err = pcall(function()
  local stdin_empty = writefile(note(prefix .. "stdin_empty.lua"), "")
  expect_ok("stdin_empty", "-", "", { stdin = stdin_empty })

  local stdin_expr = writefile(note(prefix .. "stdin_expr.lua"),
			       "print(\n1, a\n)\n")
  expect_ok("stdin_expr", "-", "1\tnil\n", { stdin = stdin_expr })

  expect_ok("bom_empty", q(writefile(note(prefix .. "bom_empty.lua"),
				     "\239\187\191")), "")
  expect_ok("bom_print", q(writefile(note(prefix .. "bom_print.lua"),
				     "\239\187\191print(3)")), "3\n")
  expect_ok("bom_comment", q(writefile(note(prefix .. "bom_comment.lua"),
				       "\239\187\191# comment!!\nprint(3)")),
	    "3\n")
  expect_fail("bad_bom_1", q(writefile(note(prefix .. "bad_bom_1.lua"),
				       "\239")), "unexpected symbol")
  expect_fail("bad_bom_2", q(writefile(note(prefix .. "bad_bom_2.lua"),
				       "\239\187")), "unexpected symbol")
  expect_fail("bad_bom_tail", q(writefile(note(prefix .. "bad_bom_tail.lua"),
					  "\239print(3)")),
	      "unexpected symbol")

  expect_ok("compact_e", "-eprint(1) -ea=3 -e " .. q("print(a)"), "1\n3\n")

  local many = writefile(note(prefix .. "many.lua"), "print(({...})[30])\n")
  expect_ok("many_args", q(many) .. string.rep(" a", 30), "a\n")

  local debug_input = writefile(note(prefix .. "debug_input.lua"),
				"io.stderr:write(1000)\ncont\n")
  local debug_run = run("debug_debug", "-e " .. q("require'debug'.debug()"),
			{ stdin = debug_input })
  assert(debug_run.ok, "debug.debug failed: " .. debug_run.err)
  assert(debug_run.out == "", "debug.debug stdout mismatch: " .. debug_run.out)
  assert(debug_run.err == "lua_debug> 1000lua_debug> ",
	 "debug.debug stderr mismatch: " .. debug_run.err)

  expect_ok("first_line_comment",
	    q(writefile(note(prefix .. "first_line_comment.lua"),
			"#comment in 1st line without newline")), "")

  local bytecode = "#comment\n" .. string.dump(assert(load("print(3)")), true)
  expect_ok("comment_binary", q(writefile(note(prefix .. "comment_binary.lua"),
					  bytecode)), "3\n")

  local close_out = note(prefix .. "close_open_file.payload")
  local close_script = writefile(note(prefix .. "close_open_file.lua"),
				 ("io.output(%q); io.write('alo')\n"):format(close_out))
  expect_ok("close_open_file", q(close_script), "")
  assert(readfile(close_out) == "alo", "open file was not closed on exit")

  for _, code in ipairs({ "nil", "0", "true" }) do
    local script = writefile(note(prefix .. "exit_" .. code .. ".lua"),
			     "os.exit(" .. code .. ", true)\n")
    expect_ok("exit_" .. code, q(script), "")
  end
  expect_exit_failure("exit_1", "1")
  expect_exit_failure("exit_false", "false")
end)

cleanup()
assert(ok, err)
print("lua54_standalone_regress.lua OK")
