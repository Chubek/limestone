-- satie stdlib: arith (integer arithmetic).
-- Pure arithmetic only; no math library is required.
local Arith = {}

Arith.abs = function(v)
  if v < 0 then return -v end
  return v
end

Arith.gcd = function(a, b)
  a = Arith.abs(a)
  b = Arith.abs(b)
  while b ~= 0 do
    local r = a % b
    a = b
    b = r
  end
  return a
end

Arith.lcm = function(a, b)
  if a == 0 or b == 0 then return 0 end
  return (a / Arith.gcd(a, b)) * b
end

Arith.pow = function(base, exp)
  local result = 1
  local i = 0
  while i < exp do
    result = result * base
    i = i + 1
  end
  return result
end

Arith.fact = function(n)
  local result = 1
  local i = 2
  while i <= n do
    result = result * i
    i = i + 1
  end
  return result
end

Arith.is_even = function(n)
  return n % 2 == 0
end

Arith.clamp = function(v, lo, hi)
  if v < lo then return lo end
  if v > hi then return hi end
  return v
end

Arith.sign = function(v)
  if v < 0 then return -1 end
  if v > 0 then return 1 end
  return 0
end

Arith.min2 = function(a, b)
  if a < b then return a end
  return b
end

Arith.max2 = function(a, b)
  if a > b then return a end
  return b
end

return Arith
