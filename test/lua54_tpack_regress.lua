local pack = string.pack
local packsize = string.packsize
local unpack = string.unpack

local NB = 16
local sizeLI = packsize("j")

assert(sizeLI == (math.maxinteger > 0x7fffffff and 8 or 4))
assert(packsize("!xXi16") == 8)

for i = 1, NB do
  local s = string.rep("\xff", i)
  assert(pack("i" .. i, -1) == s)
  assert(packsize("i" .. i) == i)
  assert(unpack("i" .. i, s) == -1)

  s = "\xAA" .. string.rep("\0", i - 1)
  assert(pack("<I" .. i, 0xAA) == s)
  assert(unpack("<I" .. i, s) == 0xAA)
end

do
  local lnum = 0x13121110090807060504030201
  local s = pack("<j", -lnum)
  local unum = unpack("<I" .. sizeLI, s)
  for i = sizeLI + 1, NB do
    assert(unpack("<i" .. i, s .. ("\xff"):rep(i - sizeLI)) == -lnum)
    assert(unpack(">i" .. i, ("\xff"):rep(i - sizeLI) .. s:reverse()) == -lnum)
    assert(unpack("<I" .. i, s .. ("\0"):rep(i - sizeLI)) == unum)
  end
end

do
  local ok, err = pcall(unpack, "i16", string.rep("\3", 16))
  assert(ok == false and tostring(err):find("16%-byte integer"))
end

assert(unpack("<I4", pack("<I4", 4000000000)) == 4000000000)
assert(unpack("L", pack("L", 0xffffffff)) == 0xffffffff)
assert(unpack("<J", pack("<j", -1)) == -1)
assert(pack("<J", -1) == string.rep("\xff", sizeLI))
assert(unpack("<J", pack("<J", -1)) == -1)
assert(unpack("<J", pack("<J", math.mininteger)) == math.mininteger)
if packsize("T") == sizeLI then
  assert(pack("<T", -1) == string.rep("\xff", sizeLI))
  assert(unpack("<T", pack("<T", -1)) == -1)
end

do
  local ok, err = pcall(pack, "!17", 0)
  assert(ok == false and tostring(err):find("out of limits", 1, true))
end

do
  for _, f in ipairs({ pack, packsize, unpack }) do
    local ok, err = pcall(f, "c", "")
    assert(ok == false and tostring(err):find("missing size", 1, true))
    assert(not tostring(err):find("bad argument", 1, true))
  end
end

do
  local ok, err = pcall(packsize, "c1" .. string.rep("0", 40))
  assert(ok == false and tostring(err):find("invalid format", 1, true))
end

do
  local ok, err = pcall(packsize, string.rep("c268435456", 8))
  assert(ok == false and tostring(err):find("too large", 1, true))
  assert(packsize(string.rep("c268435456", 7) .. "c268435455") == 0x7fffffff)
end

do
  for _, f in ipairs({ pack, unpack }) do
    local ok, err = pcall(f, "X i", "")
    assert(ok == false and tostring(err):find("invalid next option", 1, true))
  end
end

do
  local x = pack("i4i4i4i4", 1, 2, 3, 4)
  local i, p = unpack("!4 i4", x, -4)
  assert(i == 4 and p == 17)
  i, p = unpack("!4 i4", x, -#x)
  assert(i == 1 and p == 5)
  i, p = unpack("b", "abc", -1099511627776)
  assert(i == 97 and p == 2)
  i, p = unpack("b", "abc", math.mininteger)
  assert(i == 97 and p == 2)
end
