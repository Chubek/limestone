# Exolayer

Exolayer is Limestone's C ABI/FFI boundary. Public symbols use `exl_`; link
`Limestone::exolayer` and include `<exolayer/exolayer.h>`.

An opaque context owns symbol registrations. `exl_register` registers a host
callback; `exl_register_typed` additionally validates scalar argument/result kinds.
Callbacks have the `exl_native_fn` ABI: value array/count, result pointer, and host
userdata. This callback ABI is distinct from a foreign native function signature.

`exl_register_native` prepares a scalar native address/signature with an explicit
`exl_callconv_t`. `exl_library_open_native` and `exl_library_bind` retain library
ownership while symbols are registered/invoked. Supported argument kinds are
`EXL_I64`, `EXL_F64`, and `EXL_PTR`, with scalar/void results and at most
`EXL_NATIVE_MAX_ARGS` arguments. `exl_native_available` reports adapter availability.
`exl_register_native_variadic` and `exl_library_bind_variadic` prepare one exact
variadic call shape with an explicit fixed-argument count. Ellipsis argument
types are already C-promoted: `long long`, `double`, and `void *`. Each distinct
argument shape is registered separately; calls validate every argument kind and
the total count. The same ownership and library-close rules apply. Aggregate and
vector native calls need additional adapters.

The supplied FFItk entry point returns `FFI_ENOSYS`; the isolated `native.c`
adapter uses libffi when `LIMESTONE_ENABLE_NATIVE_FFI=ON`. Disabling it keeps
callbacks, layouts, and extension loading, while native registration reports
unavailability. Native declaration/call failures return errors through the stable
C interface.

Extension callbacks retain their library through active calls, including unload
and registration changes. `EXL_EXTENSION_ABI_VERSION` is 1. Scalar layouts
are independently inspectable and validated through `exl_kind_layout`.
Owning contexts and library handles have explicit release paths.

The full API, conventions, extension contract, and native examples are in
[`19_exolayer.md`](../third_party/exolangtk/docs/manual/19_exolayer.md). The
authoritative symbol list is
`third_party/exolangtk/manifests/Exolayer-Modules.yaml`, checked by CTest.
