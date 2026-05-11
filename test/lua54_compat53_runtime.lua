assert(_VERSION == "Lua 5.4", "lua54_compat53_runtime.lua must run in Lua 5.4 compat mode")
assert(jit and jit.lua54compat == true)

local function near(a, b)
  return math.abs(a - b) < 1e-12
end

for _, name in ipairs{
  "atan2", "pow", "log10", "sinh", "cosh", "tanh", "frexp", "ldexp",
} do
  assert(type(math[name]) == "function", "missing compat math." .. name)
end

assert(near(math.atan2(1, 1), math.atan(1, 1)))
assert(math.pow(2, 3) == 8)
assert(near(math.log10(100), 2))
assert(near(math.sinh(0), 0))
assert(near(math.cosh(0), 1))
assert(near(math.tanh(0), 0))

local mantissa, exponent = math.frexp(8)
assert(mantissa == 0.5 and exponent == 4)
assert(math.ldexp(mantissa, exponent) == 8)

print("lua54_compat53_runtime.lua OK")
