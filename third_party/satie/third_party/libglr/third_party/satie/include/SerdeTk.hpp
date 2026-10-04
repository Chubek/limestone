#pragma once
#include <string>
#include <utility>
#include <vector>

// Minimal compatibility declarations used by the bundled QaMRpp header when
// the optional SerdeTk/MiniZIP dependency is not present.
namespace minizip {
namespace detail { using byte_vector = std::vector<unsigned char>; }
namespace api {
template <typename T> class result {
public:
  result () = default;
  explicit result (T value) : value_ (std::move (value)), ok_ (true) {}
  static result failure (std::string message) { result r; r.message_ = std::move (message); return r; }
  bool ok () const noexcept { return ok_; }
  const std::string &message () const noexcept { return message_; }
  T &value () { return value_; }
  const T &value () const { return value_; }
private:
  T value_{};
  std::string message_;
  bool ok_ = false;
};
template <> class result<void> {
public:
  static result success () { result r; r.ok_ = true; return r; }
  static result failure (std::string message) { result r; r.message_ = std::move (message); return r; }
  bool ok () const noexcept { return ok_; }
  const std::string &message () const noexcept { return message_; }
private:
  std::string message_;
  bool ok_ = false;
};
class extractor {
public:
  static result<extractor> open (const std::string &) { return result<extractor>::failure ("MiniZIP support unavailable"); }
  result<void> verify () const { return result<void>::failure ("MiniZIP support unavailable"); }
  result<detail::byte_vector> extract_bytes (const std::string &) const { return result<detail::byte_vector>::failure ("MiniZIP support unavailable"); }
};
} // namespace api
} // namespace minizip
