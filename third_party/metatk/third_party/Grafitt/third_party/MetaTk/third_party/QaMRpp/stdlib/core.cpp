#include "../include/QaMRpp.hpp"

namespace qamrpp {
namespace stdlib {

void load_core(Context& ctx) {
    (void)ctx.load_library_named("core");
}

} // namespace stdlib
} // namespace qamrpp
