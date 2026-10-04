#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace satie::stdlib::variables
{

/// Scoped fresh-variable supply: `fresh` hands out consecutive ids,
/// `checkpoint`/`rollback` rewind to an earlier point (for speculative
/// encodings that must not leak temporaries).
class VarSupply
{
public:
  explicit VarSupply (std::int32_t first = 1) : first_ (first), next_ (first)
  {
    if (first <= 0)
      throw std::invalid_argument ("variables::VarSupply starts at a positive id");
  }

  std::int32_t fresh ()
  {
    if (next_ == std::numeric_limits<std::int32_t>::max ())
      throw std::overflow_error ("variables::VarSupply exhausted");
    return next_++;
  }

  std::vector<std::int32_t> fresh_many (std::size_t count)
  {
    std::vector<std::int32_t> out;
    out.reserve (count);
    for (std::size_t i = 0; i < count; ++i)
      out.push_back (fresh ());
    return out;
  }

  std::int32_t checkpoint () const { return next_; }

  void rollback (std::int32_t point)
  {
    if (point <= 0 || point > next_)
      throw std::invalid_argument ("variables::VarSupply invalid rollback point");
    next_ = point;
  }

  std::size_t issued () const { return static_cast<std::size_t> (next_ - first_); }

private:
  std::int32_t first_ = 1;
  std::int32_t next_ = 1;
};

} // namespace satie::stdlib::variables
