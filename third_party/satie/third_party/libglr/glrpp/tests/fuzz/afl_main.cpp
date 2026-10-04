#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int main() {
  std::vector<uint8_t> buf(1 << 16);
  size_t n = fread(buf.data(), 1, buf.size(), stdin);
  return LLVMFuzzerTestOneInput(buf.data(), n);
}
