#include "Grafitt.hpp"
#include <fstream>
#include <string>

int main() {
    auto path = grafitt::gbin::default_schema_path();
    std::ofstream os(path);
    if (!os) return 1;
    os << "schema GBIN {\n";
    os << "  magic \"GBIN\"\n";
    os << "  version 1\n";
    os << "}\n";
    return 0;
}
