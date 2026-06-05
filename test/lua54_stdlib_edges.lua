assert(_VERSION == "Lua 5.4", "lua54_stdlib_edges.lua must run in Lua 5.4 compat mode")

local cases = {
  { "base.assert.noargs", "return assert()", err = "bad argument #1 to 'assert' (value expected)" },
  { "base.assert.false", "return assert(false, 'x', 2)", err = "x" },
  { "base.collectgarbage.badopt", "return collectgarbage('bad')", err = "bad argument #1 to 'collectgarbage' (invalid option 'bad')" },
  { "base.collectgarbage.badarg", "return collectgarbage(true)", err = "bad argument #1 to 'collectgarbage' (string expected, got boolean)" },
  { "base.dofile.bad", "return dofile(true)", err = "bad argument #1 to 'dofile' (string expected, got boolean)" },
  { "base.error.level.float", "return error('x', 1.2)", err = "bad argument #2 to 'error' (number has no integer representation)" },
  { "base.getmetatable.noarg", "return getmetatable()", err = "bad argument #1 to 'getmetatable' (value expected)" },
  { "base.ipairs.nil", "return ipairs(nil)", ok = { "function", "nil:nil", "number:0" } },
  { "base.load.badchunk", "return load(true)", err = "bad argument #1 to 'load' (function expected, got boolean)" },
  { "base.load.badmode", "return load('', 'x', true)", err = "bad argument #3 to 'load' (string expected, got boolean)" },
  { "base.load.badenv", "return load('', 'x', 't', true)", ok = { "function" } },
  { "base.load.reader.bool", "local done = false; return load(function() if not done then done = true; return true end end)", ok = { "nil:nil", "string:base.load.reader.bool:1: reader function must return a string" } },
  { "base.next.nil", "return next(nil)", err = "bad argument #1 to 'next' (table expected, got nil)" },
  { "base.pairs.nil", "return pairs(nil)", ok = { "function", "nil:nil", "nil:nil" } },
  { "base.pcall.noarg", "return pcall()", err = "bad argument #1 to 'pcall' (value expected)" },
  { "base.rawequal.one", "return rawequal(1)", err = "bad argument #2 to 'rawequal' (value expected)" },
  { "base.rawget.noarg", "return rawget()", err = "bad argument #1 to 'rawget' (table expected, got no value)" },
  { "base.rawget.nokey", "return rawget({})", err = "bad argument #2 to 'rawget' (value expected)" },
  { "base.rawlen.noarg", "return rawlen()", err = "bad argument #1 to 'rawlen' (table or string expected, got no value)" },
  { "base.rawset.noarg", "return rawset()", err = "bad argument #1 to 'rawset' (table expected, got no value)" },
  { "base.rawset.noval", "return rawset({}, 1)", err = "bad argument #3 to 'rawset' (value expected)" },
  { "base.select.bad", "return select(0, 1)", err = "bad argument #1 to 'select' (index out of range)" },
  { "base.select.widepos", "return select('#', select(1099511627776, 'a', 'b'))", ok = { "number:0" } },
  { "base.select.widestr", "return select('#', select('1099511627776', 'a', 'b'))", ok = { "number:0" } },
  { "base.select.wideneg", "return select(-1099511627776, 'a', 'b')", err = "bad argument #1 to 'select' (index out of range)" },
  { "base.setmetatable.noarg", "return setmetatable()", err = "bad argument #1 to 'setmetatable' (table expected, got no value)" },
  { "base.setmetatable.bad2", "return setmetatable({}, true)", err = "bad argument #2 to 'setmetatable' (nil or table expected, got boolean)" },
  { "base.tonumber.badbase", "return tonumber('10', 1)", err = "bad argument #2 to 'tonumber' (base out of range)" },
  { "base.tonumber.floatbase", "return tonumber('10', 10.2)", err = "bad argument #2 to 'tonumber' (number has no integer representation)" },
  { "base.tonumber.widebase", "return tonumber('10', 1099511627776)", err = "bad argument #2 to 'tonumber' (base out of range)" },
  { "base.tostring.noarg", "return tostring()", err = "bad argument #1 to 'tostring' (value expected)" },
  { "base.type.noarg", "return type()", err = "bad argument #1 to 'type' (value expected)" },
  { "base.warn.noarg", "return warn()", err = "bad argument #1 to 'warn' (string expected, got no value)" },
  { "base.xpcall.nohandler", "return xpcall(function() end)", err = "bad argument #2 to 'xpcall' (function expected, got no value)" },

  { "coroutine.close.noarg", "return coroutine.close()", err = "bad argument #1 to 'close' (thread expected, got no value)" },
  { "coroutine.create.bad", "return coroutine.create(true)", err = "bad argument #1 to 'create' (function expected, got boolean)" },
  { "coroutine.resume.noarg", "return coroutine.resume()", err = "bad argument #1 to 'resume' (thread expected, got no value)" },
  { "coroutine.status.noarg", "return coroutine.status()", err = "bad argument #1 to 'status' (thread expected, got no value)" },
  { "coroutine.wrap.bad", "return coroutine.wrap(true)", err = "bad argument #1 to 'wrap' (function expected, got boolean)" },
  { "coroutine.isyieldable.bad", "return coroutine.isyieldable(true)", err = "bad argument #1 to 'isyieldable' (thread expected, got boolean)" },

  { "debug.gethook.bad", "return debug.gethook(true)", ok = { "nil:nil" } },
  { "debug.getinfo.badlevel", "return debug.getinfo(1.2)", err = "bad argument #1 to 'getinfo' (number has no integer representation)" },
  { "debug.getinfo.widelevel", "return debug.getinfo(1099511627776, 'S').what", ok = { "string:C" } },
  { "debug.getinfo.maxlevel", "return debug.getinfo(math.maxinteger)", ok = { "nil:nil" } },
  { "debug.getinfo.badwhat", "return debug.getinfo(1, 'z')", err = "bad argument #2 to 'getinfo' (invalid option)" },
  { "debug.getinfo.badgtlevel", "return debug.getinfo(1, '>')", err = "bad argument #2 to 'getinfo' (invalid option '>')" },
  { "debug.getinfo.badgtfunc", "return debug.getinfo(function() end, '>')", err = "bad argument #2 to 'getinfo' (invalid option '>')" },
  { "debug.getlocal.badarg", "return debug.getlocal(true, 1)", err = "bad argument #1 to 'getlocal' (number expected, got boolean)" },
  { "debug.getmetatable.noarg", "return debug.getmetatable()", err = "bad argument #1 to 'getmetatable' (value expected)" },
  { "debug.getregistry.extra", "return debug.getregistry(1)", ok = { "table" } },
  { "debug.getupvalue.badfunc", "return debug.getupvalue(true, 1)", err = "bad argument #1 to 'getupvalue' (function expected, got boolean)" },
  { "debug.getuservalue.noarg", "return debug.getuservalue()", ok = { "nil:nil" } },
  { "debug.getuservalue.badslot", "return debug.getuservalue({}, true)", err = "bad argument #2 to 'getuservalue' (number expected, got boolean)" },
  { "debug.sethook.badthread", "return debug.sethook(true)", err = "bad argument #2 to 'sethook' (string expected, got no value)" },
  { "debug.sethook.badmask", "return debug.sethook(function() end, 'z')", ok = {} },
  { "debug.setlocal.badarg", "return debug.setlocal(true, 1, 2)", err = "bad argument #1 to 'setlocal' (number expected, got boolean)" },
  { "debug.setmetatable.noarg", "return debug.setmetatable()", err = "bad argument #2 to 'setmetatable' (nil or table expected, got no value)" },
  { "debug.setmetatable.bad2", "return debug.setmetatable({}, true)", err = "bad argument #2 to 'setmetatable' (nil or table expected, got boolean)" },
  { "debug.setupvalue.badfunc", "return debug.setupvalue(true, 1, 2)", err = "bad argument #1 to 'setupvalue' (function expected, got boolean)" },
  { "debug.setuservalue.noarg", "return debug.setuservalue()", err = "bad argument #1 to 'setuservalue' (userdata expected, got no value)" },
  { "debug.setuservalue.badslot", "return debug.setuservalue({}, 1, true)", err = "bad argument #3 to 'setuservalue' (number expected, got boolean)" },
  { "debug.traceback.badthread", "return debug.traceback(true)", ok = { "boolean:true" } },
  { "debug.traceback.widelevel", "return debug.traceback('m', 1099511627776):match('stack traceback') ~= nil", ok = { "boolean:true" } },
  { "debug.upvalueid.badfunc", "return debug.upvalueid(true, 1)", err = "bad argument #1 to 'upvalueid' (function expected, got boolean)" },
  { "debug.upvaluejoin.badfunc1", "return debug.upvaluejoin(true, 1, function() end, 1)", err = "bad argument #1 to 'upvaluejoin' (function expected, got boolean)" },
  { "debug.upvaluejoin.badfunc2", "return debug.upvaluejoin(function() end, 1, true, 1)", err = "bad argument #2 to 'upvaluejoin' (invalid upvalue index)" },

  { "io.read.wide", "local f = assert(io.tmpfile()); return f:read(1099511627776)", err = "not enough memory" },
  { "io.read.wideneg", "local f = assert(io.tmpfile()); return f:read(-1099511627776)", err = "not enough memory" },
  { "io.seek.wide", "local f = assert(io.tmpfile()); return f:seek('set', 2147483648)", err = "bad argument #2 to 'seek' (not an integer in proper range)" },
  { "io.seek.widestr", "local f = assert(io.tmpfile()); return f:seek('set', '2147483648')", err = "bad argument #2 to 'seek' (not an integer in proper range)" },
  { "io.seek.pcall.badofs", "local f = assert(io.tmpfile()); local ok, err = pcall(f.seek, f, 'set', {}); if ok then error('unexpected success', 0) end; error(err, 0)", err = "bad argument #3 to '?' (number expected, got table)" },
  { "io.seek.dot.badofs", "local f = assert(io.tmpfile()); return f.seek(f, 'set', {})", err = "bad argument #3 to 'seek' (number expected, got table)" },
  { "io.seek.alias.badofs", "local f = assert(io.tmpfile()); local s = f.seek; return s(f, 'set', {})", err = "bad argument #3 to 's' (number expected, got table)" },
  { "io.setvbuf.pcall.badsize", "local f = assert(io.tmpfile()); local ok, err = pcall(f.setvbuf, f, 'full', {}); if ok then error('unexpected success', 0) end; error(err, 0)", err = "bad argument #3 to '?' (number expected, got table)" },
  { "io.setvbuf.dot.badsize", "local f = assert(io.tmpfile()); return f.setvbuf(f, 'full', {})", err = "bad argument #3 to 'setvbuf' (number expected, got table)" },
  { "io.setvbuf.alias.badsize", "local f = assert(io.tmpfile()); local s = f.setvbuf; return s(f, 'full', {})", err = "bad argument #3 to 's' (number expected, got table)" },

  { "math.abs.bad", "return math.abs(true)", err = "bad argument #1 to 'abs' (number expected, got boolean)" },
  { "math.atan.noarg", "return math.atan()", err = "bad argument #1 to 'atan' (number expected, got no value)" },
  { "math.ceil.bad", "return math.ceil(true)", err = "bad argument #1 to 'ceil' (number expected, got boolean)" },
  { "math.fmod.zero", "return math.fmod(1, 0)", err = "bad argument #2 to 'fmod' (zero)" },
  { "math.log.badbase", "return math.log(1, true)", err = "bad argument #2 to 'log' (number expected, got boolean)" },
  { "math.max.noarg", "return math.max()", err = "bad argument #1 to 'max' (value expected)" },
  { "math.min.noarg", "return math.min()", err = "bad argument #1 to 'min' (value expected)" },
  { "math.random.bad3", "return math.random(1, 2, 3)", err = "wrong number of arguments" },
  { "math.random.zerozero", "return math.random(0, 0)", ok = { "number:0" } },
  { "math.random.empty", "return math.random(10, 5)", err = "bad argument #1 to 'random' (interval is empty)" },
  { "math.random.empty10", "return math.random(1, 0)", err = "bad argument #1 to 'random' (interval is empty)" },
  { "math.random.badarg", "return math.random(true)", err = "bad argument #1 to 'random' (number expected, got boolean)" },
  { "math.randomseed.bad", "return math.randomseed(true)", err = "bad argument #1 to 'randomseed' (number expected, got boolean)" },
  { "math.tointeger.noarg", "return math.tointeger()", err = "bad argument #1 to 'tointeger' (value expected)" },
  { "math.ult.noarg", "return math.ult()", err = "bad argument #1 to 'ult' (number expected, got no value)" },

  { "os.date.badfmt", "return os.date(true)", err = "bad argument #1 to 'date' (string expected, got boolean)" },
  { "os.difftime.noarg", "return os.difftime()", err = "bad argument #1 to 'difftime' (number expected, got no value)" },
  { "os.execute.bad", "return os.execute(true)", err = "bad argument #1 to 'execute' (string expected, got boolean)" },
  { "os.exit.bad1", "return os.exit({})", err = "bad argument #1 to 'exit' (number expected, got table)" },
  { "os.remove.noarg", "return os.remove()", err = "bad argument #1 to 'remove' (string expected, got no value)" },
  { "os.rename.noarg", "return os.rename()", err = "bad argument #1 to 'rename' (string expected, got no value)" },
  { "os.setlocale.badcat", "return os.setlocale('', 'bad')", err = "bad argument #2 to 'setlocale' (invalid option 'bad')" },
  { "os.time.bad", "return os.time(true)", err = "bad argument #1 to 'time' (table expected, got boolean)" },

  { "package.require.noarg", "return require()", err = "bad argument #1 to 'require' (string expected, got no value)" },
  { "package.searchpath.noarg", "return package.searchpath()", err = "bad argument #2 to 'searchpath' (string expected, got no value)" },
  { "package.searchpath.badsep", "return package.searchpath('a', '?.lua', true)", err = "bad argument #3 to 'searchpath' (string expected, got boolean)" },
  { "package.searchpath.dotsep", "return package.searchpath('a.b', '?.lua', '.', '/')", ok = { "nil:nil", "string:no file 'a/b.lua'" } },
  { "package.searchpath.emptyname", "return package.searchpath('', '?.lua')", ok = { "nil:nil", "string:no file '.lua'" } },
  { "package.searchpath.empty", "return package.searchpath('x', '')", ok = { "nil:nil", "string:no file ''" } },

  { "string.byte.noarg", "return string.byte()", err = "bad argument #1 to 'byte' (string expected, got no value)" },
  { "string.char.bad", "return string.char(0x80000000)", err = "bad argument #1 to 'char' (value out of range)" },
  { "string.dump.noarg", "return string.dump()", err = "bad argument #1 to 'dump' (function expected, got no value)" },
  { "string.find.noarg", "return string.find()", err = "bad argument #1 to 'find' (string expected, got no value)" },
  { "string.format.noarg", "return string.format()", err = "bad argument #1 to 'format' (string expected, got no value)" },
  { "string.format.missing", "return string.format('%d')", err = "bad argument #2 to 'format' (no value)" },
  { "string.format.badfmt", "return string.format('%999999999999999999999999d', 1)", err = "invalid format (too long)" },
  { "string.gsub.norepl", "return string.gsub('a', 'a')", err = "bad argument #3 to 'gsub' (string/function/table expected, got no value)" },
  { "string.len.noarg", "return string.len()", err = "bad argument #1 to 'len' (string expected, got no value)" },
  { "string.match.noarg", "return string.match()", err = "bad argument #1 to 'match' (string expected, got no value)" },
  { "string.pack.noarg", "return string.pack()", err = "bad argument #1 to 'pack' (string expected, got no value)" },
  { "string.pack.badfmt", "return string.pack('z')", err = "bad argument #2 to 'pack' (string expected, got nil)" },
  { "string.pack.missing", "return string.pack('i')", err = "bad argument #2 to 'pack' (number expected, got nil)" },
  { "string.pack.Xspace", "return string.pack('X i', 1)", err = "bad argument #1 to 'pack' (invalid next option for option 'X')" },
  { "string.packsize.badfmt", "return string.packsize('z')", err = "bad argument #1 to 'packsize' (variable-length format)" },
  { "string.rep.noarg", "return string.rep()", err = "bad argument #1 to 'rep' (string expected, got no value)" },
  { "string.sub.noarg", "return string.sub()", err = "bad argument #1 to 'sub' (string expected, got no value)" },
  { "string.unpack.noarg", "return string.unpack()", err = "bad argument #1 to 'unpack' (string expected, got no value)" },
  { "string.unpack.short", "return string.unpack('i4', '')", err = "bad argument #2 to 'unpack' (data string too short)" },
  { "string.unpack.pos0", "return string.unpack('b', 'abc', 0)", ok = { "number:97", "number:2" } },
  { "string.unpack.pos5", "return string.unpack('b', 'abc', 5)", err = "bad argument #3 to 'unpack' (initial position out of string)" },
  { "string.unpack.wideneg", "return string.unpack('b', 'abc', -1099511627776)", ok = { "number:97", "number:2" } },

  { "table.concat.noarg", "return table.concat()", err = "bad argument #1 to 'concat' (table expected, got no value)" },
  { "table.concat.badsep", "return table.concat({}, true)", err = "bad argument #2 to 'concat' (string expected, got boolean)" },
  { "table.insert.noarg", "return table.insert()", err = "bad argument #1 to 'insert' (table expected, got no value)" },
  { "table.insert.badpos", "return table.insert({1,2}, 1.2, 'x')", err = "bad argument #2 to 'insert' (number has no integer representation)" },
  { "table.insert.pos0", "return table.insert({1}, 0, 'x')", err = "bad argument #2 to 'insert' (position out of bounds)" },
  { "table.insert.pos3", "return table.insert({1}, 3, 'x')", err = "bad argument #2 to 'insert' (position out of bounds)" },
  { "table.move.noarg", "return table.move()", err = "bad argument #2 to 'move' (number expected, got no value)" },
  { "table.move.badfrom", "return table.move({}, 1.2, 2, 1)", err = "bad argument #2 to 'move' (number has no integer representation)" },
  { "table.move.stridx", "return table.move({}, '1', '2', '3')", ok = { "table" } },
  { "table.move.wrap", "return table.move({}, 1, math.maxinteger, 2)", err = "bad argument #4 to 'move' (destination wrap around)" },
  { "table.pack.extra", "return table.pack(1, nil, 3).n", ok = { "number:3" } },
  { "table.remove.noarg", "return table.remove()", err = "bad argument #1 to 'remove' (table expected, got no value)" },
  { "table.remove.pos0", "return table.remove({1}, 0)", err = "bad argument #2 to 'remove' (position out of bounds)" },
  { "table.remove.pos2", "return table.remove({1}, 2)", ok = { "nil:nil" } },
  { "table.sort.noarg", "return table.sort()", err = "bad argument #1 to 'sort' (table expected, got no value)" },
  { "table.sort.badcomp", "return table.sort({}, true)", ok = {} },
  { "table.unpack.noarg", "return table.unpack()", err = "attempt to get length of a nil value" },

  { "utf8.char.noarg", "return utf8.char()", ok = { "string:" } },
  { "utf8.char.bad", "return utf8.char(0x80000000)", err = "bad argument #1 to 'char' (value out of range)" },
  { "utf8.codes.noarg", "return utf8.codes()", err = "bad argument #1 to 'codes' (string expected, got no value)" },
  { "utf8.codes.invalid", "for _ in utf8.codes('\\128') do end; return true", err = "bad argument #1 to 'codes' (invalid UTF-8 code)" },
  { "utf8.codepoint.noarg", "return utf8.codepoint()", err = "bad argument #1 to 'codepoint' (string expected, got no value)" },
  { "utf8.codepoint.badidx", "return utf8.codepoint('abc', 0, 1)", err = "bad argument #2 to 'codepoint' (out of bounds)" },
  { "utf8.codepoint.badfinal", "return utf8.codepoint('abc', 1, 4)", err = "bad argument #3 to 'codepoint' (out of bounds)" },
  { "utf8.codepoint.emptyrange", "return utf8.codepoint('abc', 3, 2)", ok = {} },
  { "utf8.len.noarg", "return utf8.len()", err = "bad argument #1 to 'len' (string expected, got no value)" },
  { "utf8.len.badidx", "return utf8.len('abc', 0, 2)", err = "bad argument #2 to 'len' (initial position out of bounds)" },
  { "utf8.len.badfinal", "return utf8.len('abc', 1, 4)", err = "bad argument #3 to 'len' (final position out of bounds)" },
  { "utf8.offset.noarg", "return utf8.offset()", err = "bad argument #1 to 'offset' (string expected, got no value)" },
  { "utf8.offset.badn", "return utf8.offset('abc', 1.2)", err = "bad argument #2 to 'offset' (number has no integer representation)" },
  { "utf8.offset.zero.cont", "return utf8.offset('\194\128', 0, 2)", ok = { "number:1" } },
  { "utf8.offset.pos0", "return utf8.offset('abc', 1, 0)", err = "bad argument #3 to 'offset' (position out of bounds)" },
}

local function normerr(e)
  e = tostring(e)
  e = e:gsub('^%[string "[^"]*"%]:%d+: ', '')
  e = e:gsub('^[^:]+:%d+: ', '')
  return e
end

local function val(v)
  local tv = type(v)
  if tv == "string" then return "string:" .. v:gsub("\n", "\\n") end
  if tv == "number" or tv == "boolean" or tv == "nil" then
    return tv .. ":" .. tostring(v)
  end
  return tv
end

for _, c in ipairs(cases) do
  local name, code = c[1], c[2]
  local f, err = load(code, "=" .. name)
  assert(f, name .. " load failed: " .. tostring(err))
  local r = table.pack(pcall(f))
  if c.err then
    assert(not r[1], name .. " unexpectedly succeeded")
    local got = normerr(r[2])
    assert(got == c.err, name .. " error mismatch\nwant: " .. c.err .. "\n got: " .. got)
  else
    assert(r[1], name .. " unexpectedly failed: " .. normerr(r[2]))
    assert(r.n - 1 == #c.ok,
      name .. " result count mismatch: want " .. #c.ok .. ", got " .. (r.n - 1))
    for i = 1, #c.ok do
      local got = val(r[i + 1])
      assert(got == c.ok[i],
        name .. " result #" .. i .. " mismatch: want " .. c.ok[i] .. ", got " .. got)
    end
  end
end

print("lua54_stdlib_edges.lua OK")
