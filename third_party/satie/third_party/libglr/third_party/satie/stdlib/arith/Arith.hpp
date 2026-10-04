#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>

namespace satie::stdlib::arith
{

inline std::int64_t abs_val (std::int64_t v) { return v < 0 ? -v : v; }

inline std::int64_t gcd (std::int64_t a, std::int64_t b)
{
  a = abs_val (a);
  b = abs_val (b);
  while (b != 0)
    {
      const std::int64_t r = a % b;
      a = b;
      b = r;
    }
  return a;
}

inline std::int64_t lcm (std::int64_t a, std::int64_t b)
{
  if (a == 0 || b == 0)
    return 0;
  const std::int64_t g = gcd (a, b);
  const std::int64_t q = a / g;
  const std::uint64_t mag_q =
      q < 0 ? std::uint64_t{ 0 } - static_cast<std::uint64_t> (q)
            : static_cast<std::uint64_t> (q);
  const std::uint64_t mag_b = static_cast<std::uint64_t> (abs_val (b));
  if (mag_b != 0 && mag_q > static_cast<std::uint64_t> (std::numeric_limits<std::int64_t>::max ()) / mag_b)
    throw std::overflow_error ("arith::lcm overflow");
  return q * abs_val (b) < 0 ? -(q * abs_val (b)) : q * abs_val (b);
}

inline bool mul_overflows (std::int64_t a, std::int64_t b)
{
  if (a == 0 || b == 0)
    return false;
  const std::uint64_t mag_a =
      a < 0 ? std::uint64_t{ 0 } - static_cast<std::uint64_t> (a)
            : static_cast<std::uint64_t> (a);
  const std::uint64_t mag_b =
      b < 0 ? std::uint64_t{ 0 } - static_cast<std::uint64_t> (b)
            : static_cast<std::uint64_t> (b);
  const bool negative = (a < 0) != (b < 0);
  const std::uint64_t limit =
      negative ? (std::uint64_t{ 1 } << 63)
               : static_cast<std::uint64_t> (std::numeric_limits<std::int64_t>::max ());
  return mag_a > limit / mag_b;
}

/// base^exp with overflow detection (empty optional on overflow).
inline std::optional<std::int64_t> pow_int (std::int64_t base, unsigned exp)
{
  std::int64_t result = 1;
  while (exp != 0)
    {
      if ((exp & 1u) != 0)
        {
          if (mul_overflows (result, base))
            return std::nullopt;
          result *= base;
        }
      exp >>= 1;
      if (exp != 0)
        {
          if (mul_overflows (base, base))
            return std::nullopt;
          base *= base;
        }
    }
  return result;
}

inline std::optional<std::int64_t> factorial (unsigned n)
{
  std::int64_t result = 1;
  for (unsigned i = 2; i <= n; ++i)
    {
      if (mul_overflows (result, static_cast<std::int64_t> (i)))
        return std::nullopt;
      result *= static_cast<std::int64_t> (i);
    }
  return result;
}

inline bool is_prime (std::int64_t n)
{
  if (n < 2)
    return false;
  if (n % 2 == 0)
    return n == 2;
  for (std::int64_t d = 3; d <= n / d; d += 2)
    if (n % d == 0)
      return false;
  return true;
}

template <typename T> T clamp_val (T v, T lo, T hi)
{
  if (lo > hi)
    throw std::invalid_argument ("arith::clamp_val inverted range");
  return v < lo ? lo : (v > hi ? hi : v);
}

template <typename T> int sign_of (T v) { return v < 0 ? -1 : (v > 0 ? 1 : 0); }

} // namespace satie::stdlib::arith
