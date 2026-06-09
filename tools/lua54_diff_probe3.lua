-- Differential probe battery, phase 3: metamethod dispatch sequences,
-- pattern errors, deterministic os.date and debug.getinfo surfaces.
-- Companion to lua54_diff_probe.lua; see that file for usage.

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
  s = s:gsub("[^%s:]*lua54_diff_probe3%.lua", "PROBE")
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

-- Metamethod dispatch sequences. Each proxy logs which metamethod fired on
-- which proxy, plus the argument shapes it received.
local mmlog
local function argdesc(v)
  local t = type(v)
  if t == "table" then return rawget(v, "tag") or "table" end
  if t == "string" or t == "number" or t == "boolean" then
    return tostring(v)
  end
  return t
end
local mmnames = {
  "__add", "__sub", "__mul", "__div", "__mod", "__pow", "__idiv",
  "__band", "__bor", "__bxor", "__shl", "__shr", "__unm", "__bnot",
  "__concat", "__len", "__eq", "__lt", "__le", "__index", "__newindex",
  "__call",
}
local function mkproxy(tag)
  local mt = {}
  for _, mm in ipairs(mmnames) do
    mt[mm] = function(a, b, c)
      mmlog[#mmlog + 1] = tag .. ":" .. mm:sub(3) .. "(" .. argdesc(a) ..
        "," .. argdesc(b) .. (c ~= nil and ("," .. argdesc(c)) or "") .. ")"
      return tag .. ":" .. mm:sub(3)
    end
  end
  return setmetatable({ tag = tag }, mt)
end
local function mkplain(tag)
  return setmetatable({ tag = tag }, {})
end

local mmexprs = {
  "P + Q", "P + 1", "1 + P", "P + 'x'", "'x' + P",
  "P - Q", "P * 2", "P / 2", "P % 2", "P ^ 2", "P // 2",
  "P & 1", "1 | P", "P ~ Q", "P << 1", "1 >> P",
  "-P", "~P", "#P", "P .. 'x'", "'x' .. P", "P .. Q",
  "'a' .. 'b' .. P", "P .. 'a' .. 'b'",
  "P == Q", "P == P", "P ~= Q", "P < Q", "P <= Q", "P > Q", "P >= Q",
  "P < 1", "P <= 'x'", "1 < P",
  "P.k", "P[1]", "P.k == nil", "P()", "P(1, 'x')",
  "R + Q", "Q + R", "R == S", "R < S", "R <= S",
  "P == R", "R == P",
}
for _, e in ipairs(mmexprs) do
  mmlog = {}
  local f = load(
    "local P, Q, R, S = ...; return " .. e, "=mm")
  if f then
    local P, Q = mkproxy("P"), mkproxy("Q")
    local R, S = mkplain("R"), mkplain("S")
    local ok, r1, r2 = pcall(f, P, Q, R, S)
    emit("mm:" .. e, ok, r1, r2, "log=" .. table.concat(mmlog, ";"))
  end
end

-- __newindex variants: function, table redirect, chained.
do
  mmlog = {}
  local sink = {}
  local p = setmetatable({}, { __newindex = sink })
  p.k = 11
  emit("mmnewindex:tableredirect", sink.k, rawget(p, "k"))
  local chain2 = setmetatable({}, { __newindex = function(t, k, v)
    mmlog[#mmlog + 1] = "leaf(" .. tostring(k) .. "," .. tostring(v) .. ")"
  end })
  local chain1 = setmetatable({}, { __newindex = chain2 })
  chain1.k = 22
  emit("mmnewindex:chain", table.concat(mmlog, ";"))
end

-- __index chains and depth.
do
  local leaf = { k = "leafvalue" }
  local mid = setmetatable({}, { __index = leaf })
  local top = setmetatable({}, { __index = mid })
  emit("mmindex:chain", top.k, top.missing)
end

-- __eq applicability: tables vs userdata vs mixed primitive.
do
  local mt = { __eq = function() return true end }
  local a, b = setmetatable({}, mt), setmetatable({}, mt)
  emit("mmeq:tables", a == b, a ~= b)
  emit("mmeq:tablenum", a == 1, 1 == a)
  emit("mmeq:tablestr", a == "x", "x" == a)
end

-- Pattern error and capture surfaces.
local patterns = {
  { "abc", "%l+" }, { "abc", "(" }, { "abc", ")" }, { "abc", "(()" },
  { "abc", "%" }, { "abc", "%z" }, { "abc", "[a" }, { "abc", "[]" },
  { "abc", "[^]" }, { "abc", "%b" }, { "abc", "%ba" }, { "abc", "%bab" },
  { "abc", "%f" }, { "abc", "%fa" }, { "abc", "%f[a]" }, { "abc", "a**" },
  { "abc", "%1" }, { "(a)(b)", "((a)%2)" }, { "aaa", "(a)%1" },
  { "abc", ".-" }, { "abc", "a-" }, { "", "" }, { "abc", "()" },
  { "abc", "(a)()" }, { "abc", "[%a]" }, { "abc", "[%z]" },
  { "abc", "%g+" }, { "\1\2abc", "%g+" }, { "abc", "%u*" },
  { "abc{}", "%p+" }, { "a\0b", "%z" }, { "a\0b", "." },
  { "abc", string.rep("(", 33) }, { "abc", string.rep("(a)", 33) },
}
for _, sp in ipairs(patterns) do
  local s, p = sp[1], sp[2]
  emit("pat:find:" .. s .. ":" .. p, pcall(string.find, s, p))
  emit("pat:match:" .. s .. ":" .. p, pcall(string.match, s, p))
  emit("pat:gsub:" .. s .. ":" .. p, pcall(string.gsub, s, p, "-"))
end

-- gsub replacement surfaces.
local gsubs = {
  { "abc", "b", "%%" }, { "abc", "b", "%0%0" }, { "abc", "(b)", "%1%1" },
  { "abc", "b", "%2" }, { "abc", "(b)", "[%0]" }, { "hello world", "(%w+)", "%1 %1" },
  { "abc", "%w+", "%0" }, { "abc", ".", "x" },
}
for _, g in ipairs(gsubs) do
  emit("gsub:" .. g[1] .. ":" .. g[2] .. ":" .. g[3],
       pcall(string.gsub, g[1], g[2], g[3]))
end

-- Deterministic UTC os.date battery over two fixed timestamps.
local stamps = { 0, 1234567890 }
local datefmts = {
  "%Y", "%y", "%m", "%d", "%H", "%M", "%S", "%p", "%A", "%a", "%B", "%b",
  "%j", "%w", "%x", "%X", "%c", "%%", "%n", "%t", "%D?", "*t",
}
for _, ts in ipairs(stamps) do
  for _, f in ipairs(datefmts) do
    if f == "*t" then
      local ok, t = pcall(os.date, "!*t", ts)
      if ok then
        emit("date:*t:" .. ts, t.year, t.month, t.day, t.hour, t.min,
             t.sec, t.wday, t.yday, t.isdst)
      else
        emit("date:*t:" .. ts, false, t)
      end
    else
      emit("date:" .. f .. ":" .. ts, pcall(os.date, "!" .. f, ts))
    end
  end
end

-- debug.getinfo surfaces for a known chunk layout.
do
  local f = load("local x = 1\nreturn function() return x end", "=chunkname")
  local inner = f()
  local gi = debug.getinfo(inner, "Slu")
  emit("getinfo:inner", gi.source, gi.short_src, gi.linedefined,
       gi.lastlinedefined, gi.what, gi.nups, gi.nparams, gi.isvararg)
  local gm = debug.getinfo(f, "Slu")
  emit("getinfo:main", gm.source, gm.short_src, gm.linedefined,
       gm.lastlinedefined, gm.what, gm.nups, gm.nparams, gm.isvararg)
  local gc2 = debug.getinfo(print, "Slu")
  emit("getinfo:cfunc", gc2.source, gc2.short_src, gc2.linedefined,
       gc2.lastlinedefined, gc2.what, gc2.nups, gc2.nparams, gc2.isvararg)
  for _, name in ipairs({ "@file.lua", "plaintext\nsecond", "=short",
			  string.rep("x", 200) }) do
    local g = load("return 1", name)
    local gi2 = debug.getinfo(g, "S")
    emit("getinfo:src", gi2.source:sub(1, 80), gi2.short_src)
  end
end

io.write("#done3\n")
