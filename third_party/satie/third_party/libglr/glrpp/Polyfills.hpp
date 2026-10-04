#pragma once

#ifndef LIBGLR_GLRPP_POLYFILLS_HPP
#define LIBGLR_GLRPP_POLYFILLS_HPP

/**
 * @file Polyfills.hpp
 * @brief Header-only dynamic-loading and discovery helpers for GLRpp.
 *
 * This header provides the lower-level portability and dynamic-loading layer
 * used by `GLRpp.hpp`.
 *
 * The main responsibilities of this file are:
 *
 * - initialize and finalize libltdl safely;
 * - load the `libglr` shared object at runtime;
 * - provide typed symbol lookup helpers;
 * - optionally use libpkgconf to discover libglr installation metadata;
 * - provide convenience wrappers that higher-level GLRpp code can use without
 *   depending directly on libltdl/libpkgconf implementation details.
 *
 * This file does not define the C++ DSL.  The DSL lives in `GLRpp.hpp`.
 *
 * @note GLRpp is header-only, but this header still requires the application
 *       to link against libltdl and, if enabled, libpkgconf.
 *
 * Typical build flags:
 *
 * @code
 * pkg-config --cflags --libs ltdl
 * pkg-config --cflags --libs libpkgconf
 * @endcode
 *
 * You can disable libpkgconf support by defining:
 *
 * @code
 * #define GLRPP_DISABLE_PKGCONF 1
 * @endcode
 *
 * before including this header.
 */

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <exception>
#include <functional>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <ltdl.h>

#ifndef GLRPP_DISABLE_PKGCONF
#if __has_include(<pkgconf/libpkgconf.h>)
#include <pkgconf/libpkgconf.h>
#define GLRPP_HAS_LIBPKGCONF 1
#elif __has_include(<libpkgconf/libpkgconf.h>)
#include <libpkgconf/libpkgconf.h>
#define GLRPP_HAS_LIBPKGCONF 1
#else
#define GLRPP_HAS_LIBPKGCONF 0
#endif
#else
#define GLRPP_HAS_LIBPKGCONF 0
#endif

#ifndef GLRPP_DEFAULT_PACKAGE_NAME
#define GLRPP_DEFAULT_PACKAGE_NAME "libglr"
#endif

#ifndef GLRPP_DEFAULT_LIBRARY_NAME
#define GLRPP_DEFAULT_LIBRARY_NAME "glr"
#endif

#ifndef GLRPP_ENV_LIBRARY_PATH
#define GLRPP_ENV_LIBRARY_PATH "GLRPP_LIBGLR_PATH"
#endif

#ifndef GLRPP_ENV_SEARCH_PATH
#define GLRPP_ENV_SEARCH_PATH "GLRPP_LIBGLR_SEARCH_PATH"
#endif

namespace glrpp
{
inline namespace v1
{

/**
 * @brief Base exception type for GLRpp runtime-loading errors.
 */
class error : public std::runtime_error
{
public:
  explicit error (const std::string &message) : std::runtime_error (message) {}

  explicit error (const char *message) : std::runtime_error (message) {}
};

/**
 * @brief Exception thrown when libltdl initialization or usage fails.
 */
class ltdl_error : public error
{
public:
  explicit ltdl_error (const std::string &message) : error (message) {}
};

/**
 * @brief Exception thrown when a shared library cannot be loaded.
 */
class library_load_error : public error
{
public:
  explicit library_load_error (const std::string &message) : error (message) {}
};

/**
 * @brief Exception thrown when a required symbol cannot be found.
 */
class symbol_error : public error
{
public:
  explicit symbol_error (const std::string &message) : error (message) {}
};

/**
 * @brief Exception thrown when pkgconf lookup fails.
 */
class pkgconf_error : public error
{
public:
  explicit pkgconf_error (const std::string &message) : error (message) {}
};

namespace detail
{

/**
 * @brief Return the current libltdl error string.
 */
inline std::string
ltdl_last_error ()
{
  const char *err = lt_dlerror ();
  return err ? std::string (err) : std::string ("unknown libltdl error");
}

/**
 * @brief Split a path-list style environment variable.
 *
 * On POSIX systems this uses ':' as the separator.
 * On Windows systems this uses ';' as the separator.
 */
inline std::vector<std::string>
split_search_path (std::string_view value)
{
#if defined(_WIN32)
  constexpr char separator = ';';
#else
  constexpr char separator = ':';
#endif

  std::vector<std::string> result;
  std::string current;

  for (char ch : value)
    {
      if (ch == separator)
        {
          if (!current.empty ())
            result.push_back (current);
          current.clear ();
        }
      else
        {
          current.push_back (ch);
        }
    }

  if (!current.empty ())
    result.push_back (current);

  return result;
}

/**
 * @brief Read an environment variable into an optional string.
 */
inline std::optional<std::string>
getenv_string (const char *name)
{
  if (!name)
    return std::nullopt;

  const char *value = std::getenv (name);
  if (!value || !*value)
    return std::nullopt;

  return std::string (value);
}

/**
 * @brief Test whether a string starts with the given prefix.
 */
inline bool
starts_with (std::string_view s, std::string_view prefix)
{
  return s.size () >= prefix.size ()
         && std::equal (prefix.begin (), prefix.end (), s.begin ());
}

/**
 * @brief Test whether a string contains another string.
 */
inline bool
contains (std::string_view s, std::string_view needle)
{
  return s.find (needle) != std::string_view::npos;
}

/**
 * @brief Convert a generic pointer returned by lt_dlsym into a typed function
 * pointer.
 *
 * POSIX permits `dlsym`-style APIs to return `void*`.
 * C++ does not formally guarantee arbitrary conversion from object pointer to
 * function pointer, but this is the conventional and practical technique used
 * for C dynamic loading APIs.
 */
template <class Function>
inline Function
symbol_cast (void *ptr)
{
  static_assert (std::is_pointer_v<Function>,
                 "symbol_cast<T> expects T to be a pointer type");

  Function out{};
  std::memcpy (&out, &ptr, sizeof (out));
  return out;
}

} // namespace detail

/**
 * @brief RAII initializer for libltdl.
 *
 * libltdl requires global initialization through `lt_dlinit`.
 * This class performs reference-counted initialization and finalization.
 *
 * Most users should not instantiate this directly.  It is used internally by
 * `shared_library` and `libglr_loader`.
 */
class ltdl_runtime
{
public:
  /**
   * @brief Construct and ensure libltdl has been initialized.
   *
   * @throws ltdl_error if `lt_dlinit` fails.
   */
  ltdl_runtime ()
  {
    std::lock_guard<std::mutex> lock (mutex ());

    if (refcount () == 0)
      {
        if (lt_dlinit () != 0)
          {
            throw ltdl_error (std::string ("lt_dlinit failed: ")
                              + detail::ltdl_last_error ());
          }
      }

    ++refcount ();
  }

  /**
   * @brief Decrement the initialization reference count.
   *
   * When the final `ltdl_runtime` object is destroyed, `lt_dlexit` is called.
   *
   * @note Destructors must not throw.  If `lt_dlexit` fails, the error is
   *       intentionally ignored.
   */
  ~ltdl_runtime () noexcept
  {
    std::lock_guard<std::mutex> lock (mutex ());

    assert (refcount () > 0);

    --refcount ();

    if (refcount () == 0)
      static_cast<void> (lt_dlexit ());
  }

  ltdl_runtime (const ltdl_runtime &) = delete;
  ltdl_runtime &operator= (const ltdl_runtime &) = delete;

  ltdl_runtime (ltdl_runtime &&) = delete;
  ltdl_runtime &operator= (ltdl_runtime &&) = delete;

private:
  static std::mutex &
  mutex ()
  {
    static std::mutex m;
    return m;
  }

  static std::size_t &
  refcount ()
  {
    static std::size_t n = 0;
    return n;
  }
};

/**
 * @brief RAII wrapper around an `lt_dlhandle`.
 *
 * This class owns a libltdl module handle and closes it automatically.
 */
class shared_library
{
public:
  /**
   * @brief Construct an empty library handle.
   */
  shared_library () = default;

  /**
   * @brief Load a library by filename/path.
   *
   * @param path The library path or module name.
   *
   * @throws library_load_error if loading fails.
   */
  explicit shared_library (const std::string &path) { open (path); }

  /**
   * @brief Move-construct a library handle.
   */
  shared_library (shared_library &&other) noexcept
      : runtime_ (std::move (other.runtime_)),
        handle_ (std::exchange (other.handle_, nullptr)),
        path_ (std::move (other.path_))
  {
  }

  /**
   * @brief Move-assign a library handle.
   */
  shared_library &
  operator= (shared_library &&other) noexcept
  {
    if (this != &other)
      {
        close ();

        runtime_ = std::move (other.runtime_);
        handle_ = std::exchange (other.handle_, nullptr);
        path_ = std::move (other.path_);
      }

    return *this;
  }

  shared_library (const shared_library &) = delete;
  shared_library &operator= (const shared_library &) = delete;

  /**
   * @brief Close the library handle.
   */
  ~shared_library () noexcept { close (); }

  /**
   * @brief Return true if a library is currently loaded.
   */
  [[nodiscard]] bool
  loaded () const noexcept
  {
    return handle_ != nullptr;
  }

  /**
   * @brief Return the path/name used to load the library.
   */
  [[nodiscard]] const std::string &
  path () const noexcept
  {
    return path_;
  }

  /**
   * @brief Open a library by path or module name.
   *
   * Uses `lt_dlopenext`, allowing libltdl to apply platform-specific shared
   * library extensions.
   *
   * @throws library_load_error if the library cannot be loaded.
   */
  void
  open (const std::string &path)
  {
    close ();

    runtime_ = std::make_unique<ltdl_runtime> ();

    lt_dlhandle h = lt_dlopenext (path.c_str ());
    if (!h)
      {
        throw library_load_error ("unable to load shared library `" + path
                                  + "`: " + detail::ltdl_last_error ());
      }

    handle_ = h;
    path_ = path;
  }

  /**
   * @brief Close the currently loaded library.
   */
  void
  close () noexcept
  {
    if (handle_)
      {
        static_cast<void> (lt_dlclose (handle_));
        handle_ = nullptr;
      }

    path_.clear ();
    runtime_.reset ();
  }

  /**
   * @brief Return the underlying libltdl handle.
   */
  [[nodiscard]] lt_dlhandle
  native_handle () const noexcept
  {
    return handle_;
  }

  /**
   * @brief Look up a raw symbol by name.
   *
   * @param name Symbol name.
   *
   * @return Raw symbol pointer.
   *
   * @throws symbol_error if the symbol cannot be found.
   */
  [[nodiscard]] void *
  raw_symbol (const char *name) const
  {
    if (!handle_)
      throw symbol_error ("cannot look up symbol on an unloaded library");

    if (!name || !*name)
      throw symbol_error ("cannot look up an empty symbol name");

    void *ptr = lt_dlsym (handle_, name);
    if (!ptr)
      {
        throw symbol_error (std::string ("unable to find symbol `") + name
                            + "` in `" + path_
                            + "`: " + detail::ltdl_last_error ());
      }

    return ptr;
  }

  /**
   * @brief Look up a typed function symbol.
   *
   * Example:
   *
   * @code
   * using fn_type = int (*)(const char*);
   * auto fn = lib.symbol<fn_type>("some_c_function");
   * @endcode
   */
  template <class Function>
  [[nodiscard]] Function
  symbol (const char *name) const
  {
    return detail::symbol_cast<Function> (raw_symbol (name));
  }

  /**
   * @brief Look up an optional typed function symbol.
   *
   * Unlike `symbol`, this function does not throw if the symbol does not
   * exist.  It returns `std::nullopt` instead.
   */
  template <class Function>
  [[nodiscard]] std::optional<Function>
  optional_symbol (const char *name) const
  {
    if (!handle_ || !name || !*name)
      return std::nullopt;

    void *ptr = lt_dlsym (handle_, name);
    if (!ptr)
      return std::nullopt;

    return detail::symbol_cast<Function> (ptr);
  }

private:
  std::unique_ptr<ltdl_runtime> runtime_;
  lt_dlhandle handle_ = nullptr;
  std::string path_;
};

/**
 * @brief Information discovered for a pkgconf package.
 *
 * This type intentionally keeps the information generic.  The higher-level
 * loader uses the library directories and library names as hints for libltdl.
 */
struct package_info
{
  /**
   * @brief Package name, usually `libglr`.
   */
  std::string name;

  /**
   * @brief Version string if discovered.
   */
  std::string version;

  /**
   * @brief Include directories discovered from the package.
   */
  std::vector<std::string> include_dirs;

  /**
   * @brief Library search directories discovered from the package.
   */
  std::vector<std::string> library_dirs;

  /**
   * @brief Library names discovered from the package.
   *
   * For example, `-lglr` becomes `glr`.
   */
  std::vector<std::string> libraries;

  /**
   * @brief Raw linker flags if available.
   */
  std::vector<std::string> linker_flags;

  /**
   * @brief Raw compiler flags if available.
   */
  std::vector<std::string> compiler_flags;
};

namespace detail
{

#if GLRPP_HAS_LIBPKGCONF

/**
 * @brief libpkgconf error callback used by GLRpp.
 *
 * We keep the callback intentionally quiet by default.  Discovery failure is
 * reported through the return value of the public API.
 */
inline void
pkgconf_error_handler (const char *message, const pkgconf_client_t *,
                       const void *)
{
  static_cast<void> (message);
}

/**
 * @brief Parse a rendered pkgconf flag string.
 */
inline std::vector<std::string>
split_flags (const std::string &flags)
{
  std::vector<std::string> out;
  std::istringstream iss (flags);
  std::string part;

  while (iss >> part)
    out.push_back (part);

  return out;
}

/**
 * @brief Incorporate compiler/linker tokens into package_info.
 */
inline void
parse_pkgconf_tokens (package_info &info,
                      const std::vector<std::string> &tokens)
{
  for (const std::string &token : tokens)
    {
      if (starts_with (token, "-I") && token.size () > 2)
        {
          info.include_dirs.push_back (token.substr (2));
        }
      else if (starts_with (token, "-L") && token.size () > 2)
        {
          info.library_dirs.push_back (token.substr (2));
        }
      else if (starts_with (token, "-l") && token.size () > 2)
        {
          info.libraries.push_back (token.substr (2));
        }
    }
}

/**
 * @brief Remove duplicate strings from a vector while preserving order.
 */
inline void
unique_in_place (std::vector<std::string> &xs)
{
  std::vector<std::string> out;

  for (const auto &x : xs)
    {
      if (std::find (out.begin (), out.end (), x) == out.end ())
        out.push_back (x);
    }

  xs = std::move (out);
}

#endif // GLRPP_HAS_LIBPKGCONF

#if !GLRPP_HAS_LIBPKGCONF
inline void
unique_in_place (std::vector<std::string> &xs)
{
  std::vector<std::string> out;
  for (const auto &x : xs)
    {
      if (std::find (out.begin (), out.end (), x) == out.end ())
        out.push_back (x);
    }
  xs = std::move (out);
}
#endif

} // namespace detail

/**
 * @brief Discover package information for a pkgconf package.
 *
 * If libpkgconf headers were not available at compile time, this function
 * returns `std::nullopt`.
 *
 * @param package_name Package name, usually `libglr`.
 *
 * @return Discovered package info, or `std::nullopt` on failure.
 */
inline std::optional<package_info>
find_package_info (const std::string &package_name
                   = GLRPP_DEFAULT_PACKAGE_NAME)
{
#if GLRPP_HAS_LIBPKGCONF

  package_info info;
  info.name = package_name;

  pkgconf_client_t client{};

  /*
   * libpkgconf has had small API differences across releases.
   * The common public usage pattern is:
   *
   *   pkgconf_client_init(...)
   *   pkgconf_pkg_find(...)
   *   pkgconf_pkg_unref(...)
   *   pkgconf_client_deinit(...)
   *
   * The fragment rendering portions are also intentionally isolated here so
   * that downstream projects can patch this function easily if they target a
   * very specific libpkgconf release.
   */

  pkgconf_client_init (&client, detail::pkgconf_error_handler, nullptr,
                       nullptr);

  pkgconf_pkg_t *pkg = pkgconf_pkg_find (&client, package_name.c_str ());

  if (!pkg)
    {
      pkgconf_client_deinit (&client);
      return std::nullopt;
    }

  if (pkg->version)
    info.version = pkg->version;

  /*
   * Render cflags/libs through libpkgconf fragment APIs when available.
   *
   * Some distributions expose the fragment API in subtly different ways.
   * The following block targets the modern libpkgconf API shape.
   */
#if defined(PKGCONF_BUFSIZE)
  {
    char cflags_buf[PKGCONF_BUFSIZE]{};
    char libs_buf[PKGCONF_BUFSIZE]{};

    pkgconf_list_t cflags{};
    pkgconf_list_t libs{};

    pkgconf_fragment_copy (&cflags, &pkg->cflags);
    pkgconf_fragment_copy (&libs, &pkg->libs);

    pkgconf_fragment_render_buf (&cflags, cflags_buf, sizeof (cflags_buf),
                                 true, &client);

    pkgconf_fragment_render_buf (&libs, libs_buf, sizeof (libs_buf), true,
                                 &client);

    info.compiler_flags = detail::split_flags (cflags_buf);
    info.linker_flags = detail::split_flags (libs_buf);

    detail::parse_pkgconf_tokens (info, info.compiler_flags);
    detail::parse_pkgconf_tokens (info, info.linker_flags);

    pkgconf_fragment_free (&cflags);
    pkgconf_fragment_free (&libs);
  }
#else
  /*
   * Fallback: keep package discovery useful even if flag rendering is not
   * available through the visible libpkgconf headers.
   */
  static_cast<void> (pkg);
#endif

  pkgconf_pkg_unref (&client, pkg);
  pkgconf_client_deinit (&client);

  detail::unique_in_place (info.include_dirs);
  detail::unique_in_place (info.library_dirs);
  detail::unique_in_place (info.libraries);
  detail::unique_in_place (info.compiler_flags);
  detail::unique_in_place (info.linker_flags);

  return info;

#else

  static_cast<void> (package_name);
  return std::nullopt;

#endif
}

/**
 * @brief Runtime configuration used to locate and load libglr.
 */
struct loader_options
{
  /**
   * @brief pkgconf package name.
   */
  std::string package_name = GLRPP_DEFAULT_PACKAGE_NAME;

  /**
   * @brief Default library name.
   *
   * Passed to libltdl as a module name.  `glr` generally resolves to
   * `libglr.so`, `libglr.dylib`, or `glr.dll` depending on platform.
   */
  std::string library_name = GLRPP_DEFAULT_LIBRARY_NAME;

  /**
   * @brief Explicit full library path.
   *
   * If this is set, it is tried before all other candidates.
   */
  std::optional<std::string> explicit_path;

  /**
   * @brief Additional search directories.
   */
  std::vector<std::string> search_dirs;

  /**
   * @brief Whether to consult environment variables.
   *
   * Recognized variables:
   *
   * - `GLRPP_LIBGLR_PATH`
   * - `GLRPP_LIBGLR_SEARCH_PATH`
   */
  bool use_environment = true;

  /**
   * @brief Whether to consult libpkgconf.
   */
  bool use_pkgconf = true;

  /**
   * @brief Whether to try common soname candidates.
   */
  bool try_default_candidates = true;
};

/**
 * @brief Result of attempting to load libglr.
 */
struct load_result
{
  /**
   * @brief Loaded shared library.
   */
  shared_library library;

  /**
   * @brief Path or module name that succeeded.
   */
  std::string selected_candidate;

  /**
   * @brief pkgconf data used during lookup, if available.
   */
  std::optional<package_info> package;

  /**
   * @brief Failed candidates with diagnostic messages.
   */
  std::vector<std::string> diagnostics;
};

namespace detail
{

/**
 * @brief Construct common candidate names for libglr.
 */
inline std::vector<std::string>
default_library_candidates (const std::string &library_name)
{
  std::vector<std::string> out;

  if (!library_name.empty ())
    out.push_back (library_name);

  if (library_name != "glr")
    out.push_back ("glr");

  out.push_back ("libglr.so");
  out.push_back ("libglr.dylib");
  out.push_back ("glr.dll");
  out.push_back ("libglr.dll");

  unique_in_place (out);

  return out;
}

/**
 * @brief Join a directory and a library filename.
 */
inline std::string
join_path (const std::string &dir, const std::string &file)
{
  if (dir.empty ())
    return file;

#if defined(_WIN32)
  constexpr char slash = '\\';
  constexpr char alt_slash = '/';
#else
  constexpr char slash = '/';
  constexpr char alt_slash = '/';
#endif

  if (dir.back () == slash || dir.back () == alt_slash)
    return dir + file;

  return dir + slash + file;
}

/**
 * @brief Build the candidate list used by load_libglr.
 */
inline std::vector<std::string>
build_load_candidates (const loader_options &options,
                       const std::optional<package_info> &pkg)
{
  std::vector<std::string> candidates;

  if (options.explicit_path)
    candidates.push_back (*options.explicit_path);

  if (options.use_environment)
    {
      if (auto env_path = getenv_string (GLRPP_ENV_LIBRARY_PATH))
        candidates.push_back (*env_path);
    }

  std::vector<std::string> search_dirs = options.search_dirs;

  if (options.use_environment)
    {
      if (auto env_search = getenv_string (GLRPP_ENV_SEARCH_PATH))
        {
          auto dirs = split_search_path (*env_search);
          search_dirs.insert (search_dirs.end (), dirs.begin (), dirs.end ());
        }
    }

  if (pkg)
    {
      search_dirs.insert (search_dirs.end (), pkg->library_dirs.begin (),
                          pkg->library_dirs.end ());
    }

  unique_in_place (search_dirs);

  std::vector<std::string> names;

  if (pkg)
    {
      for (const auto &lib : pkg->libraries)
        {
          names.push_back (lib);
          names.push_back ("lib" + lib + ".so");
          names.push_back ("lib" + lib + ".dylib");
          names.push_back (lib + ".dll");
        }
    }

  if (options.try_default_candidates)
    {
      auto defaults = default_library_candidates (options.library_name);
      names.insert (names.end (), defaults.begin (), defaults.end ());
    }
  else if (!options.library_name.empty ())
    {
      names.push_back (options.library_name);
    }

  unique_in_place (names);

  for (const auto &dir : search_dirs)
    {
      for (const auto &name : names)
        candidates.push_back (join_path (dir, name));
    }

  for (const auto &name : names)
    candidates.push_back (name);

  unique_in_place (candidates);

  return candidates;
}

} // namespace detail

/**
 * @brief Load libglr using environment variables, pkgconf, and fallback names.
 *
 * Loading order:
 *
 * 1. explicit path from `loader_options`;
 * 2. `GLRPP_LIBGLR_PATH`;
 * 3. search directories from `loader_options`;
 * 4. `GLRPP_LIBGLR_SEARCH_PATH`;
 * 5. libpkgconf-discovered library directories;
 * 6. platform/common names such as `glr`, `libglr.so`, `libglr.dylib`.
 *
 * @throws library_load_error if no candidate can be loaded.
 */
inline load_result
load_libglr (const loader_options &options = {})
{
  load_result result;

  if (options.use_pkgconf)
    result.package = find_package_info (options.package_name);

  auto candidates = detail::build_load_candidates (options, result.package);

  if (candidates.empty ())
    candidates = detail::default_library_candidates (options.library_name);

  for (const auto &candidate : candidates)
    {
      try
        {
          shared_library lib (candidate);

          result.selected_candidate = candidate;
          result.library = std::move (lib);

          return result;
        }
      catch (const std::exception &e)
        {
          result.diagnostics.push_back (candidate + ": " + e.what ());
        }
    }

  std::ostringstream oss;
  oss << "unable to load libglr";

  if (!result.diagnostics.empty ())
    {
      oss << "; attempted candidates:";
      for (const auto &d : result.diagnostics)
        oss << "\n  - " << d;
    }

  throw library_load_error (oss.str ());
}

/**
 * @brief Lazy process-wide libglr loader.
 *
 * This class provides a simple singleton-style façade over `load_libglr`.
 *
 * Higher-level GLRpp code can use:
 *
 * @code
 * auto fn = libglr_loader::instance().symbol<fn_type>("glr_some_function");
 * @endcode
 */
class libglr_loader
{
public:
  /**
   * @brief Return the process-wide default loader.
   */
  static libglr_loader &
  instance ()
  {
    static libglr_loader loader;
    return loader;
  }

  libglr_loader (const libglr_loader &) = delete;
  libglr_loader &operator= (const libglr_loader &) = delete;

  /**
   * @brief Configure the loader before first use.
   *
   * @warning Calling this after the library has already been loaded throws.
   */
  void
  configure (loader_options options)
  {
    std::lock_guard<std::mutex> lock (mutex_);

    if (loaded_)
      throw library_load_error (
          "cannot reconfigure libglr_loader after libglr has been loaded");

    options_ = std::move (options);
  }

  /**
   * @brief Return true if libglr has already been loaded.
   */
  [[nodiscard]] bool
  loaded () const
  {
    std::lock_guard<std::mutex> lock (mutex_);
    return loaded_;
  }

  /**
   * @brief Ensure libglr is loaded.
   *
   * @return The loaded library.
   *
   * @throws library_load_error if loading fails.
   */
  shared_library &
  library ()
  {
    std::lock_guard<std::mutex> lock (mutex_);
    ensure_loaded_unlocked ();
    return result_->library;
  }

  /**
   * @brief Return the successful load candidate.
   */
  [[nodiscard]] std::string
  selected_candidate ()
  {
    std::lock_guard<std::mutex> lock (mutex_);
    ensure_loaded_unlocked ();
    return result_->selected_candidate;
  }

  /**
   * @brief Return discovered package info if available.
   */
  [[nodiscard]] std::optional<package_info>
  package ()
  {
    std::lock_guard<std::mutex> lock (mutex_);
    ensure_loaded_unlocked ();
    return result_->package;
  }

  /**
   * @brief Resolve a required libglr symbol.
   */
  template <class Function>
  [[nodiscard]] Function
  symbol (const char *name)
  {
    return library ().template symbol<Function> (name);
  }

  /**
   * @brief Resolve an optional libglr symbol.
   */
  template <class Function>
  [[nodiscard]] std::optional<Function>
  optional_symbol (const char *name)
  {
    return library ().template optional_symbol<Function> (name);
  }

private:
  libglr_loader () = default;

  void
  ensure_loaded_unlocked ()
  {
    if (loaded_)
      return;

    result_.emplace (load_libglr (options_));
    loaded_ = true;
  }

private:
  mutable std::mutex mutex_;
  loader_options options_{};
  std::optional<load_result> result_;
  bool loaded_ = false;
};

/**
 * @brief Small typed wrapper around a lazily loaded libglr C function.
 *
 * Example:
 *
 * @code
 * using fn_type = void* (*)(void);
 *
 * static glrpp::foreign_function<fn_type> create_parser{
 *     "glr_parser_create"
 * };
 *
 * void* parser = create_parser();
 * @endcode
 */
template <class Function> class foreign_function;

template <class R, class... Args> class foreign_function<R (*) (Args...)>
{
public:
  using function_type = R (*) (Args...);

  /**
   * @brief Construct an empty foreign function wrapper.
   */
  foreign_function () = default;

  /**
   * @brief Construct a foreign function wrapper by symbol name.
   */
  explicit foreign_function (const char *symbol_name)
      : symbol_name_ (symbol_name)
  {
  }

  /**
   * @brief Return the configured symbol name.
   */
  [[nodiscard]] const std::string &
  symbol_name () const noexcept
  {
    return symbol_name_;
  }

  /**
   * @brief Return true if this wrapper has a symbol name.
   */
  [[nodiscard]] explicit
  operator bool () const noexcept
  {
    return !symbol_name_.empty ();
  }

  /**
   * @brief Resolve and call the foreign function.
   */
  R
  operator() (Args... args) const
  {
    auto fn = get ();

    if constexpr (std::is_void_v<R>)
      {
        fn (std::forward<Args> (args)...);
      }
    else
      {
        return fn (std::forward<Args> (args)...);
      }
  }

  /**
   * @brief Resolve the underlying C function.
   */
  [[nodiscard]] function_type
  get () const
  {
    if (symbol_name_.empty ())
      throw symbol_error ("cannot resolve unnamed foreign function");

    std::call_once (
        flag_,
        [this]
          {
            cached_
                = libglr_loader::instance ().template symbol<function_type> (
                    symbol_name_.c_str ());
          });

    return cached_;
  }

private:
  std::string symbol_name_;
  mutable std::once_flag flag_;
  mutable function_type cached_ = nullptr;
};

/**
 * @brief Wrapper around an optional libglr foreign function.
 *
 * Unlike `foreign_function`, this type does not throw when the symbol is
 * absent until you call `operator()`.
 */
template <class Function> class optional_foreign_function;

template <class R, class... Args>
class optional_foreign_function<R (*) (Args...)>
{
public:
  using function_type = R (*) (Args...);

  optional_foreign_function () = default;

  explicit optional_foreign_function (const char *symbol_name)
      : symbol_name_ (symbol_name)
  {
  }

  [[nodiscard]] const std::string &
  symbol_name () const noexcept
  {
    return symbol_name_;
  }

  [[nodiscard]] explicit
  operator bool () const
  {
    return has_value ();
  }

  /**
   * @brief Return true if the optional symbol exists in libglr.
   */
  [[nodiscard]] bool
  has_value () const
  {
    return get ().has_value ();
  }

  /**
   * @brief Return the optional function pointer.
   */
  [[nodiscard]] std::optional<function_type>
  get () const
  {
    if (symbol_name_.empty ())
      return std::nullopt;

    std::call_once (flag_,
                    [this]
                      {
                        cached_
                            = libglr_loader::instance ()
                                  .template optional_symbol<function_type> (
                                      symbol_name_.c_str ());
                      });

    return cached_;
  }

  /**
   * @brief Call the optional symbol.
   *
   * @throws symbol_error if the symbol does not exist.
   */
  R
  operator() (Args... args) const
  {
    auto fn = get ();

    if (!fn)
      {
        throw symbol_error ("optional symbol `" + symbol_name_
                            + "` is not available in the loaded libglr");
      }

    if constexpr (std::is_void_v<R>)
      {
        (*fn) (std::forward<Args> (args)...);
      }
    else
      {
        return (*fn) (std::forward<Args> (args)...);
      }
  }

private:
  std::string symbol_name_;
  mutable std::once_flag flag_;
  mutable std::optional<function_type> cached_;
};

/**
 * @brief Convenience macro for declaring a required libglr C function wrapper.
 *
 * Example:
 *
 * @code
 * GLRPP_DECLARE_REQUIRED_FUNCTION(
 *     glr_parser_create,
 *     void* (*)(void)
 * );
 * @endcode
 */
#define GLRPP_DECLARE_REQUIRED_FUNCTION(name, type)                           \
  inline ::glrpp::foreign_function<type> name { #name }

/**
 * @brief Convenience macro for declaring an optional libglr C function
 * wrapper.
 */
#define GLRPP_DECLARE_OPTIONAL_FUNCTION(name, type)                           \
  inline ::glrpp::optional_foreign_function<type> name { #name }

namespace c_api
{

// -----------------------------------------------------------------------------
// libglr ABI declarations
// -----------------------------------------------------------------------------
//
// These declarations are intentionally conservative.
//
// The provided libglr documentation files expose many symbol names and struct
// member names, but they do not provide enough information to reconstruct a
// stable binary ABI: exact field types, enum values, function signatures,
// padding, anonymous unions, and alignment are not fully available.
//
// Therefore, most libglr structs are declared as opaque C types. This is safe
// for dynamic loading and pointer-based APIs. Concrete layout definitions
// should only be enabled after validating against the real libglr public
// headers.

namespace glrpp
{
inline namespace v1
{
namespace abi
{

extern "C"
{
  struct glr_action_set_t;
  struct glr_action_t;
  struct glr_associativity_state_t;
  struct glr_cache_config_t;
  struct glr_cache_stats_t;
  struct glr_cache_t;
  struct glr_dependency_t;
  struct glr_disambig_candidate_t;
  struct glr_disambig_context_t;
  struct glr_disambig_hook_t;
  struct glr_dp_state_t;
  struct glr_edit_t;
  struct glr_forest_cache_key_t;
  struct glr_forest_edge_t;
  struct glr_forest_node_t;

  struct glr_parser;
  struct glr_parser_t;
  struct glr_grammar_t;
  struct glr_forest_t;
  struct glr_lexer_hooks;
  struct glr_lexer_hooks_t;
}

// Enum-like ABI values.
//
// The documentation confirms names but does not provide explicit numeric
// values. Use int-compatible storage until verified from the real C headers.
using glr_node_type_t = int;
using glr_parse_error_t = int;
using glr_action_type_t = int;
using glr_associativity_t = int;
using glr_precedence_t = int;

// Common scalar aliases.
//
// These are intentionally generic. Replace with exact typedefs only after
// comparing against the public libglr headers.
using symbol_id_t = std::uint32_t;
using production_id_t = std::uint32_t;
using state_id_t = std::uint32_t;
using position_t = std::size_t;

} // namespace abi
} // inline namespace v1
} // namespace glrpp

} // namespace c_api

} // inline namespace v1
} // namespace glrpp

#endif // LIBGLR_GLRPP_POLYFILLS_HPP
