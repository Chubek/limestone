#include "SatieFrontendIPASIR.hpp"

#include <stdexcept>

namespace satie::frontend
{

namespace
{
constexpr char kVersion[] = "0.1.0";
}

const char *frontend_ipasir_component_version () noexcept { return kVersion; }

} // namespace satie::frontend

struct SatieIpasirHandle
{
  satie::frontend::IpasirSolver solver;
};

extern "C"
{

SatieIpasirHandle *satie_ipasir_init (void)
{
  try
    {
      return new SatieIpasirHandle{};
    }
  catch (const std::exception &)
    {
      return nullptr;
    }
}

void satie_ipasir_release (SatieIpasirHandle *handle) { delete handle; }

void satie_ipasir_add (SatieIpasirHandle *handle, int lit)
{
  if (handle == nullptr)
    return;
  handle->solver.add (lit);
}

void satie_ipasir_assume (SatieIpasirHandle *handle, int lit)
{
  if (handle == nullptr)
    return;
  handle->solver.assume (lit);
}

int satie_ipasir_solve (SatieIpasirHandle *handle)
{
  if (handle == nullptr)
    return 0;
  return handle->solver.solve ();
}

int satie_ipasir_val (SatieIpasirHandle *handle, int var)
{
  if (handle == nullptr || var <= 0)
    return 0;
  return handle->solver.val (var);
}

int satie_ipasir_failed (SatieIpasirHandle *handle, int lit)
{
  if (handle == nullptr || lit == 0)
    return 0;
  return handle->solver.failed (lit);
}

} // extern "C"
