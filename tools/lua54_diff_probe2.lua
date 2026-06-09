-- Differential probe battery, phase 2: operators, metamethod error
-- surfaces, numeric edges and string.format. Companion to
-- lua54_diff_probe.lua; see that file for usage.
--
-- Known accepted difference (1 probe): official Lua 5.4 compiles
-- 'x - 0' with a constant integer zero into OP_ADDI, so '-0.0 - 0'
-- prints 0.0 there but -0.0 here (and -0.0 in official with a dynamic
-- zero). Replicating the codegen quirk would either need a new bytecode
-- carrying the original __sub event or misdispatch __sub to __add.

if jit then jit.off() end
io.stdout:setvbuf("line")

local function norm(s)
  s = tostring(s)
  s = s:gsub("0[xX]%x+", "PTR")
  s = s:gsub("builtin#%d+", "PTR")
  s = s:gsub("(table: )%S+", "%1PTR")
  s = s:gsub("(function: )%S+", "%1PTR")
  s = s:gsub("(thread: )%S+", "%1PTR")
  s = s:gsub("(userdata: )%S+", "%1PTR")
  s = s:gsub("[^%s:]*lua54_diff_probe2%.lua", "PROBE")
  s = s:gsub("[%z\1-\9\11-\31\127-\255]", function(c)
    return ("\\%03d"):format(c:byte())
  end)
  return s
end

local function emit(id, ...)
  local n = select("#", ...)
  local parts = {}
  for i = 1, n do
    parts[i] = norm((select(i, ...)))
  end
  io.write("#", id, " ", table.concat(parts, " | "), "\n")
end

-- Operand battery: expression source snippets, compiled fresh per probe so
-- literal subtype handling (1 vs 1.0) is part of the surface.
local operands = {
  "nil", "true", "false", "{}", "'x'", "'2'", "'2.5'", "''",
  "0", "2", "-3", "2.5", "-0.0", "(1 << 40)", "(1 << 62)",
  "math.mininteger", "math.maxinteger", "math.huge", "-math.huge",
  "0/0", "2^53", "7", "-7", "3", "-3", "3.5", "-3.5",
}

local binops = {
  "+", "-", "*", "/", "//", "%", "^", "..",
  "==", "~=", "<", "<=", ">", ">=",
  "&", "|", "~", "<<", ">>",
}

local unops = { "-", "#", "~", "not " }

for _, op in ipairs(binops) do
  for _, a in ipairs(operands) do
    for _, b in ipairs(operands) do
      local src = "return (" .. a .. ") " .. op .. " (" .. b .. ")"
      local f = load(src, "=op")
      if f then
        local id = "op:" .. a .. " " .. op .. " " .. b
        emit(id, pcall(f))
      end
    end
  end
end

for _, op in ipairs(unops) do
  for _, a in ipairs(operands) do
    local src = "return " .. op .. "(" .. a .. ")"
    local f = load(src, "=unop")
    if f then
      emit("unop:" .. op .. a, pcall(f))
    end
  end
end

-- Indexing, calling and assignment errors on non-table values.
local victims = { "nil", "true", "0", "'x'", "{}" }
for _, v in ipairs(victims) do
  local fi = load("local a = (" .. v .. "); return a.k", "=index")
  if fi then emit("index:" .. v, pcall(fi)) end
  local fn = load("local a = (" .. v .. "); return a[1]", "=index")
  if fn then emit("indexnum:" .. v, pcall(fn)) end
  local fc = load("local a = (" .. v .. "); return a()", "=call")
  if fc then emit("call:" .. v, pcall(fc)) end
  local fs = load("local a = (" .. v .. "); a.k = 1; return 'ok'", "=newindex")
  if fs then emit("newindex:" .. v, pcall(fs)) end
end

-- Numeric for loop control-variable edges.
local fors = {
  "1, 3", "1, 3, 2", "3, 1, -1", "1.5, 3", "1, 3.5", "1, 3, 0.5",
  "1, 3, 0", "1, 3, -0.0", "'1', 3", "1, '3'", "1, 3, '1'",
  "nil, 3", "1, nil", "1, 3, nil", "true, 3", "1, {}",
  "math.maxinteger - 2, math.maxinteger", "math.mininteger, math.mininteger + 2, -1",
  "1, math.huge", "math.huge, 1, -1", "1, 0/0",
}
for _, args in ipairs(fors) do
  local src = "local n, last = 0; for i = " .. args ..
    " do n = n + 1; last = i; if n > 6 then break end end return n, last, math.type(last)"
  local f = load(src, "=for")
  if f then emit("for:" .. args, pcall(f)) end
end

-- tostring / string.format numeric edge formatting.
local numexprs = {
  "0", "-0.0", "0.0", "1", "1.0", "-1", "2.5", "1e100", "1e-100",
  "2^53", "2^53 + 1.0", "(1 << 53)", "(1 << 62)", "math.mininteger",
  "math.maxinteger", "math.huge", "-math.huge", "0/0", "-(0/0)",
  "math.pi", "5 // 2", "5.0 // 2", "5 % 2", "5.0 % 2", "-5 // 2",
  "-5 % 2", "5 / 2", "4 / 2", "2^31", "-2^31", "2^31 - 1", "1/3",
  "math.mininteger // -1", "math.mininteger % -1", "7 // 0.0",
  "-7 // 0.0", "7 % 0.0", "0.0 / 0.0",
}
for _, e in ipairs(numexprs) do
  local f = load("return (" .. e .. ")", "=num")
  if f then
    local ok, v = pcall(f)
    if ok then
      emit("num:" .. e, math.type(v), tostring(v))
    else
      emit("num:" .. e, "error", v)
    end
  end
end

local fmts = { "%d", "%i", "%x", "%X", "%o", "%e", "%E", "%f", "%g", "%G",
	       "%a", "%A", "%q", "%s", "%5.2f", "%-8d", "%08.3f", "%c",
	       "%.0f", "%#x", "%+d" }
for _, fmt in ipairs(fmts) do
  for _, e in ipairs(numexprs) do
    local f = load("return string.format('" .. fmt .. "', (" .. e .. "))",
		   "=fmt")
    if f then
      emit("fmt:" .. fmt .. ":" .. e, pcall(f))
    end
  end
end

-- Integer shift width edges.
for _, sh in ipairs({ "0", "1", "31", "32", "63", "64", "65", "-1", "-63",
		      "-64", "-65" }) do
  for _, v in ipairs({ "1", "-1", "math.mininteger", "math.maxinteger" }) do
    local f = load("return (" .. v .. ") << (" .. sh .. "), (" .. v ..
		   ") >> (" .. sh .. ")", "=shift")
    if f then emit("shift:" .. v .. ":" .. sh, pcall(f)) end
  end
end

-- String/number comparison and concat coercion edges.
for _, a in ipairs({ "'10'", "'9'", "'abc'", "'ABC'", "''" }) do
  for _, b in ipairs({ "'10'", "'9'", "'abc'", "'ABC'", "''" }) do
    local f = load("return (" .. a .. ") < (" .. b .. "), (" .. a ..
		   ") <= (" .. b .. ")", "=strcmp")
    if f then emit("strcmp:" .. a .. "<" .. b, pcall(f)) end
  end
end

-- Method-syntax string calls keep their own diagnostics.
local methods = {
  "('x'):rep(2)", "('x'):rep(0)", "('x'):upper()", "('x'):byte()",
  "('x'):sub(-1)", "('abc'):find('b')", "('abc'):find({})",
  "('%d'):format(5)", "('%d'):format('x')", "('x'):bad()",
  "('abc'):byte(1, -1)", "('x'):len()",
}
for _, m in ipairs(methods) do
  local f = load("return " .. m, "=method")
  if f then emit("method:" .. m, pcall(f)) end
end

io.write("#done2\n")
