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

local function shquote(s)
  return "'" .. tostring(s):gsub("'", "'\\''") .. "'"
end

local function sorted_env_keys(env)
  local keys = {}
  for k in pairs(env) do keys[#keys + 1] = k end
  table.sort(keys)
  return keys
end

local function batch_escape(s)
  return tostring(s):gsub("%%", "%%%%")
end

local function batch_set_value(s)
  return tostring(s):gsub("%%", "%%%%"):gsub('"', '^"')
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
  if opts.env and iswin then
    local bat = note(prefix .. name .. ".bat")
    local lines = { "@echo off\n" }
    for _, k in ipairs(sorted_env_keys(opts.env)) do
      lines[#lines + 1] = ("set \"%s=%s\"\n"):format(k, batch_set_value(opts.env[k]))
    end
    lines[#lines + 1] = batch_escape(cmd) .. "\n"
    writefile(bat, table.concat(lines))
    cmd = "cmd /c " .. q(bat:gsub("/", "\\"))
  elseif opts.env then
    local envparts = {}
    for _, k in ipairs(sorted_env_keys(opts.env)) do
      envparts[#envparts + 1] = k .. "=" .. shquote(opts.env[k])
    end
    cmd = table.concat(envparts, " ") .. " " .. cmd
  elseif iswin then
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

local function expect_script_fail(name, src, needle)
  local script = writefile(note(prefix .. name .. ".lua"), src)
  expect_fail(name, q(script), needle)
end

local function expect_contains(name, text, needle)
  assert(text:find(needle, 1, true),
	 name .. " missing fragment " .. needle .. ": " .. text)
end

local function expect_not_contains(name, text, needle)
  assert(not text:find(needle, 1, true),
	 name .. " unexpected fragment " .. needle .. ": " .. text)
end

local function count_lines(text, want)
  local n = 0
  for line in (text .. "\n"):gmatch("(.-)\n") do
    if line == want then n = n + 1 end
  end
  return n
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

  local r = run("version_exec", "-v -e " .. q("print'hello'"))
  assert(r.ok, "version + exec failed: " .. r.err)
  expect_contains("version line", r.out, "Lua 5.4.8")
  expect_contains("version copyright", r.out, "PUC-Rio")
  expect_contains("version continued chunk", r.out, "\nhello\n")
  assert(r.err == "", "version stderr mismatch: " .. r.err)

  r = run("env_init_versioned",
	  "-e " .. q("assert(lua54_init_marker == 54)"),
	  { env = {
	      LUA_INIT = "error(10)",
	      LUA_INIT_5_4 = "lua54_init_marker=54",
	    } })
  assert(r.ok, "versioned LUA_INIT failed: " .. r.err)

  r = run("env_package_versioned",
	  "-e " .. q("assert(package.path:find('v54/?.lua', 1, true) == 1); assert(package.cpath:find('v54/?.dll', 1, true) == 1)"),
	  { env = {
	      LUA_PATH = "old/?.lua",
	      LUA_PATH_5_4 = "v54/?.lua",
	      LUA_CPATH = "old/?.dll",
	      LUA_CPATH_5_4 = "v54/?.dll",
	    } })
  assert(r.ok, "versioned package path failed: " .. r.err)

  r = run("env_ignored_by_E",
	  "-E -e " .. q("assert(not package.path:find('bad/', 1, true)); assert(not package.cpath:find('bad/', 1, true))"),
	  { env = {
	      LUA_INIT_5_4 = "error(10)",
	      LUA_PATH_5_4 = "bad/?.lua",
	      LUA_CPATH_5_4 = "bad/?.dll",
	    } })
  assert(r.ok, "-E environment isolation failed: " .. r.err)

  local path_cases = {
    {
      name = "env_lua_path_semicolon",
      value = ";",
      code = "assert(package.path == ';')",
    },
    {
      name = "env_lua_path_double",
      value = ";;",
      code = "local p=package.path; assert(p:sub(1,1) ~= ';' and p:sub(-1) ~= ';', p)",
    },
    {
      name = "env_lua_path_suffix",
      value = ";;b",
      code = "local p=package.path; assert(p:sub(1,1) ~= ';' and p:sub(-2) == ';b', p)",
    },
    {
      name = "env_lua_path_prefix",
      value = "a;;",
      code = "local p=package.path; assert(p:sub(1,2) == 'a;' and p:sub(-1) ~= ';', p)",
    },
    {
      name = "env_lua_path_middle",
      value = "a;b;;c",
      code = "local p=package.path; assert(p:sub(1,4) == 'a;b;' and p:sub(-2) == ';c', p)",
    },
  }
  for _, case in ipairs(path_cases) do
    r = run(case.name, "-e " .. q(case.code),
	    { env = { LUA_PATH = case.value } })
    assert(r.ok, case.name .. " failed: " .. r.err)
  end

  for _, opt in ipairs({ "-h", "---", "-Ex", "-vv", "-iv" }) do
    local name = "bad_option_" .. opt:gsub("[^%w]", "_")
    expect_fail(name, opt, "unrecognized option '" .. opt .. "'")
  end
  expect_fail("bad_missing_e", "-e", "'-e' needs argument")
  expect_fail("bad_missing_l", "-l", "'-l' needs argument")
  expect_fail("bad_missing_e_before_option", "-e -v", "'-e' needs argument")
  expect_fail("bad_missing_l_before_option", "-l -e", "'-l' needs argument")
  expect_fail("bad_e_syntax", "-e a", "syntax error")

  local many = writefile(note(prefix .. "many.lua"), "print(({...})[30])\n")
  expect_ok("many_args", q(many) .. string.rep(" a", 30), "a\n")

  local loption_mod = writefile(note("test/lua54_standalone_loption_mod.lua"),
				"print(1); a=2; return {x=15}\n")
  local loption_other = writefile(note("test/lua54_standalone_loption_other.lua"),
				  "print(a); print(_G.lua54_standalone_loption_mod.x)\n")
  r = run("loption_multiple",
	  "-l lua54_standalone_loption_mod -llua54_standalone_loption_other -e " .. q(""),
	  { env = { LUA_PATH = "test/?.lua;;" } })
  assert(r.ok, "multiple -l failed: " .. r.err)
  assert(r.out == "1\n2\n15\n", "multiple -l stdout mismatch: " .. r.out)
  assert(r.err == "", "multiple -l stderr mismatch: " .. r.err)

  loption_mod = writefile(note("test/lua54_standalone_loption_alias.lua"),
			  "return {x=16}\n")
  r = run("loption_alias",
	  "-l alias54=lua54_standalone_loption_alias -e " ..
	  q("assert(alias54.x == 16 and _G.lua54_standalone_loption_alias == nil)"),
	  { env = { LUA_PATH = "test/?.lua;;" } })
  assert(r.ok, "-l alias failed: " .. r.err)

  loption_mod = writefile(note("test/lua54_standalone_loption_v2-v2.lua"),
			  "return {x=17}\n")
  r = run("loption_version_suffix",
	  "-l lua54_standalone_loption_v2-v2 -e " ..
	  q("assert(lua54_standalone_loption_v2.x == 17 and _G['lua54_standalone_loption_v2-v2'] == nil)"),
	  { env = { LUA_PATH = "test/?.lua;;" } })
  assert(r.ok, "-l version suffix failed: " .. r.err)

  local debug_input = writefile(note(prefix .. "debug_input.lua"),
				"io.stderr:write(1000)\ncont\n")
  local debug_run = run("debug_debug", "-e " .. q("require'debug'.debug()"),
			{ stdin = debug_input })
  assert(debug_run.ok, "debug.debug failed: " .. debug_run.err)
  assert(debug_run.out == "", "debug.debug stdout mismatch: " .. debug_run.out)
  assert(debug_run.err == "lua_debug> 1000lua_debug> ",
	 "debug.debug stderr mismatch: " .. debug_run.err)

  local arg_not_table = writefile(note(prefix .. "arg_not_table.lua"), "\n")
  expect_fail("arg_not_table", "-e " .. q("arg = 1") .. " -",
	      "'arg' is not a table", { stdin = arg_not_table })

  expect_script_fail("error_object_table", "error({})\n",
		     "(error object is a table value)")
  expect_script_fail("error_object_boolean", "error(false)\n",
		     "(error object is a boolean value)")
  expect_script_fail("error_object_nil", "error(nil)\n",
		     "(error object is a nil value)")
  expect_script_fail("error_object_tostring",
		     "error(setmetatable({}, {__tostring=function() return 'OBJ54' end}))\n",
		     "OBJ54")
  expect_script_fail("error_object_line", [[
debug = require "debug"
m = {x=0}
setmetatable(m, {__tostring = function(x)
  return tostring(debug.getinfo(4).currentline + x.x)
end})
error(m)
]], ": 6")

  r = run("warn_error_recovery",
	  "-e " .. q("warn('@on'); local ok = pcall(warn, 'SHOULD NOT APPEAR', {}); assert(not ok); warn('VISIBLE')"))
  assert(r.ok, "warning recovery failed: " .. r.err)
  assert(r.out == "", "warning recovery stdout mismatch: " .. r.out)
  expect_contains("warning recovery visible", r.err, "Lua warning: VISIBLE")
  expect_not_contains("warning recovery hidden", r.err, "SHOULD NOT APPEAR")

  r = run("print_tolstring",
	  "-e " .. q("local old=tostring; tostring=nil; print(setmetatable({}, {__tostring=function() return 'PRINT54' end})); tostring=function() return {} end; print('RAW54'); tostring=old"))
  assert(r.ok, "print tostring isolation failed: " .. r.err)
  assert(r.out == "PRINT54\nRAW54\n",
	 "print tostring isolation stdout mismatch: " .. r.out)
  assert(r.err == "", "print tostring isolation stderr mismatch: " .. r.err)

  local empty_prompts = "-e " .. q("_PROMPT='' _PROMPT2=''") .. " -i"
  local interactive_expr = writefile(note(prefix .. "interactive_expr.lua"),
				     "10\n")
  local r = run("interactive_expr", empty_prompts,
		{ stdin = interactive_expr })
  assert(r.ok, "interactive expression failed: " .. r.err)
  expect_contains("interactive expression stdout", r.out, "\n10\n")
  assert(r.err == "", "interactive expression stderr mismatch: " .. r.err)

  local interactive_print = writefile(note(prefix .. "interactive_print.lua"),
				      "10\n")
  r = run("interactive_print_error", "-e " .. q("print=nil") .. " -i",
	  { stdin = interactive_print })
  assert(r.ok, "interactive print error run failed: " .. r.err)
  expect_contains("interactive print error", r.err, "error calling 'print'")

  local interactive_multiline = writefile(note(prefix .. "interactive_multiline.lua"),
					  "(6*2-6) -- ===\na =\n10\nprint(a)\na\n")
  r = run("interactive_multiline", empty_prompts,
	  { stdin = interactive_multiline })
  assert(r.ok, "interactive multiline failed: " .. r.err)
  expect_contains("interactive multiline result", r.out, "\n6\n")
  assert(count_lines(r.out, "10") >= 2,
	 "interactive multiline missing repeated 10: " .. r.out)
  expect_not_contains("interactive multiline syntax", r.out .. r.err,
		      "unexpected symbol")
  expect_not_contains("interactive multiline nil", r.out, "\nnil\n")

  local interactive_longstring = writefile(note(prefix .. "interactive_longstring.lua"),
					   "a = [[b\nc\nd\ne]]\n=a\n")
  r = run("interactive_longstring", empty_prompts,
	  { stdin = interactive_longstring })
  assert(r.ok, "interactive long string failed: " .. r.err)
  expect_contains("interactive long string b", r.out, "\nb\n")
  expect_contains("interactive long string c", r.out, "\nc\n")
  expect_contains("interactive long string d", r.out, "\nd\n")
  expect_contains("interactive long string e", r.out, "\ne\n")
  expect_not_contains("interactive long string syntax", r.out .. r.err,
		      "syntax error")
  expect_not_contains("interactive long string unfinished", r.out .. r.err,
		      "unfinished long string")

  local prompt_meta = writefile(note(prefix .. "interactive_prompt_meta.lua"),
				" --\na = 2\n")
  r = run("interactive_prompt_meta",
	  "-e " .. q("local C=0; _PROMPT=setmetatable({},{__tostring=function() C=C+1; return C end})") .. " -i",
	  { stdin = prompt_meta })
  assert(r.ok, "interactive prompt metamethod failed: " .. r.err)
  expect_contains("interactive prompt metamethod", r.out, "\n123\n")
  expect_not_contains("interactive prompt fallback", r.out, "> > >")

  local interactive_interrupt = writefile(note(prefix .. "interactive_interrupt.lua"),
					  "a.\n")
  r = run("interactive_interrupt", "-i", { stdin = interactive_interrupt })
  assert(r.ok, "interactive interrupt run failed")
  expect_contains("interactive interrupt error", r.err,
		  "<name> expected near <eof>")
  expect_not_contains("interactive interrupt old quotes", r.err,
		      "'<name>' expected")

  local interactive_exit = writefile(note(prefix .. "interactive_exit.lua"),
				     "os.exit()\n")
  r = run("interactive_exit_nojit", "-i", { stdin = interactive_exit })
  assert(r.ok, "interactive os.exit failed: " .. r.err)
  expect_not_contains("interactive os.exit JIT status", r.out .. r.err,
		      "JIT:")

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
  expect_ok("exit_wide_status",
	    "-e " .. q("os.exit(1099511627776, true)"), "")
  expect_ok("exit_wide_string_status",
	    "-e " .. q("os.exit('1099511627776', true)"), "")
  expect_exit_failure("exit_1", "1")
  expect_exit_failure("exit_false", "false")

  local exit_close = writefile(note(prefix .. "exit_close.lua"), [[
local x <close> = setmetatable({}, {
  __close = function(self, err)
    assert(err == nil)
    print("Ok")
  end
})
local e1 <close> = setmetatable({}, {
  __close = function()
    print(120)
  end
})
os.exit(true, true)
]])
  expect_ok("exit_close", q(exit_close), "120\nOk\n")

  local close_finalizer_reentry = writefile(note(prefix .. "close_finalizer_reentry.lua"), [[
setmetatable({}, {__gc = function() print(1) end})
setmetatable({}, {__gc = function()
  print(2)
  setmetatable({}, {__gc = function() print(3) end})
  print(collectgarbage())
  os.exit(0, true)
end})
]])
  expect_ok("close_finalizer_reentry", q(close_finalizer_reentry),
	    "2\nnil\n1\n")
end)

cleanup()
assert(ok, err)
print("lua54_standalone_regress.lua OK")
