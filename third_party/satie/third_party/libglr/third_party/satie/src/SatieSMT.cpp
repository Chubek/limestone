#include "SatieSMT.hpp"

namespace satie::smt
{

namespace
{
constexpr char kSmtVersion[] = "0.1.0";
}

const char *smt_component_version () noexcept { return kSmtVersion; }

const char *smt_logic_name (Logic logic) noexcept
{
  switch (logic)
    {
    case Logic::QF_UF:
      return "QF_UF";
    case Logic::QF_LIA:
      return "QF_LIA";
    case Logic::QF_BV:
      return "QF_BV";
    case Logic::QF_FP:
      return "QF_FP";
    case Logic::QF_NRA:
      return "QF_NRA";
    case Logic::ALL:
      return "ALL";
    }
  return "UNKNOWN";
}

} // namespace satie::smt
