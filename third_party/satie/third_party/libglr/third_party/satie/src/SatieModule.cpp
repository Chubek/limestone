#include "SatieModule.hpp"
#include "SatieModule.h"

#include <mutex>
#include <string>
#include <vector>

namespace satie
{

void satie_register_adt_theory ();
void satie_register_automata_theory ();
void satie_register_bag_theory ();
void satie_register_bv_theory ();
void satie_register_euf_theory ();
void satie_register_fp_theory ();
void satie_register_idl_theory ();
void satie_register_isl_theory ();
void satie_register_lia_theory ();
void satie_register_lp_theory ();
void satie_register_milp_theory ();
void satie_register_nra_theory ();
void satie_register_ode_theory ();
void satie_register_poly_theory ();
void satie_register_quant_theory ();
void satie_register_sequence_theory ();
void satie_register_set_theory ();
void satie_register_symsolve_theory ();

void ensure_theories_registered ()
{
  static std::once_flag registered;
  std::call_once (registered, [] {
    satie_register_adt_theory ();
    satie_register_automata_theory ();
    satie_register_bag_theory ();
    satie_register_bv_theory ();
    satie_register_euf_theory ();
    satie_register_fp_theory ();
    satie_register_idl_theory ();
    satie_register_isl_theory ();
    satie_register_lia_theory ();
    satie_register_lp_theory ();
    satie_register_milp_theory ();
    satie_register_nra_theory ();
    satie_register_ode_theory ();
    satie_register_poly_theory ();
    satie_register_quant_theory ();
    satie_register_sequence_theory ();
    satie_register_set_theory ();
    satie_register_symsolve_theory ();
  });
}

std::size_t registered_theory_count ()
{
  ensure_theories_registered ();
  return TheoryRegistry::instance ().size ();
}

} // namespace satie

namespace
{
thread_local std::string g_module_error;

void set_module_error (const std::string &message) { g_module_error = message; }
} // namespace

struct SatieCModule
{
  std::unique_ptr<satie::TheoryModule> module;
  satie::CNF problem;
};

extern "C"
{

size_t satie_module_count (void)
{
  satie::ensure_theories_registered ();
  return satie::TheoryRegistry::instance ().size ();
}

const char *satie_module_name_at (size_t index)
{
  // The registry owns the strings; cache the snapshot on the calling thread
  // so the returned pointer stays valid until the next call on this thread.
  satie::ensure_theories_registered ();
  thread_local std::vector<std::string> snapshot;
  snapshot = satie::TheoryRegistry::instance ().names ();
  if (index >= snapshot.size ())
    return nullptr;
  return snapshot[index].c_str ();
}

SatieCModule *satie_module_create (const char *theory_name)
{
  if (theory_name == nullptr)
    {
      set_module_error ("module create requires a theory name");
      return nullptr;
    }
  try
    {
      satie::ensure_theories_registered ();
      auto module = satie::TheoryRegistry::instance ().create (theory_name);
      if (module == nullptr)
        {
          set_module_error (std::string ("unknown theory: ") + theory_name);
          return nullptr;
        }
      auto *handle = new SatieCModule{};
      handle->module = std::move (module);
      return handle;
    }
  catch (const std::exception &e)
    {
      set_module_error (e.what ());
      return nullptr;
    }
}

void satie_module_destroy (SatieCModule *module) { delete module; }

const char *satie_module_name (const SatieCModule *module)
{
  if (module == nullptr || module->module == nullptr)
    return nullptr;
  // string_view points into the registry-owned name or a static literal.
  thread_local std::string cached;
  cached = std::string (module->module->name ());
  return cached.c_str ();
}

int satie_module_add_clause (SatieCModule *module, const int *literals, size_t count)
{
  if (module == nullptr)
    {
      set_module_error ("module add_clause requires a module");
      return -1;
    }
  if (count != 0 && literals == nullptr)
    {
      set_module_error ("module add_clause received a null literal array");
      return -1;
    }
  try
    {
      satie::Clause clause;
      clause.reserve (count);
      for (size_t i = 0; i < count; ++i)
        {
          if (literals[i] == 0)
            {
              set_module_error ("zero is a DIMACS terminator, not a literal");
              return -1;
            }
          clause.push_back (literals[i]);
        }
      module->problem.add_clause (std::move (clause));
      return 0;
    }
  catch (const std::exception &e)
    {
      set_module_error (e.what ());
      return -1;
    }
}

int satie_module_check (SatieCModule *module)
{
  if (module == nullptr || module->module == nullptr)
    {
      set_module_error ("module check requires a module");
      return SATIE_C_MODULE_ERROR;
    }
  try
    {
      satie::SolveResult result = module->module->check (module->problem);
      if (result.status == satie::SolveStatus::SAT)
        return SATIE_C_MODULE_SAT;
      if (result.status == satie::SolveStatus::UNSAT)
        return SATIE_C_MODULE_UNSAT;
      return SATIE_C_MODULE_UNKNOWN;
    }
  catch (const std::exception &e)
    {
      set_module_error (e.what ());
      return SATIE_C_MODULE_ERROR;
    }
}

const char *satie_module_last_error (void) { return g_module_error.c_str (); }

} // extern "C"
