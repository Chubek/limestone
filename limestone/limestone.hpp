#pragma once
#include "../limestone/foundation.hpp"
namespace limestone {
struct PipelineOptions { bool optimize=true, schedule=true, allocate=true; };
struct Module { std::string name; std::string machine_ir; };
Result<Module> run_pipeline(std::string_view input, const PipelineOptions& options = PipelineOptions{});
}
