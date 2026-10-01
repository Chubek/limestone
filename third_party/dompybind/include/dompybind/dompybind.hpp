#pragma once
// DomPyBind C++ ownership and conversion layer over generated Q primitives.
#include <dompybind/capi.h>
#include <cstdint>
#include <exception>
#include <functional>
#include <variant>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dompybind {
class object;
class accessor;
class iterator;
class arg;
template<class T> decltype(auto) cast(const object &);
template<class T> object cast(T &&);

class gil_scoped_acquire {
    PyGILState_STATE state_;
public:
    gil_scoped_acquire() {
        if (!pyinit_is_initialized()) throw std::runtime_error("Python interpreter is not initialized");
        state_ = pyinit_gil_ensure();
    }
    ~gil_scoped_acquire() { pyinit_gil_release(state_); }
    gil_scoped_acquire(const gil_scoped_acquire &) = delete;
    gil_scoped_acquire &operator=(const gil_scoped_acquire &) = delete;
};
class gil_scoped_release {
    PyThreadState *state_;
public:
    gil_scoped_release() : state_(PyEval_SaveThread()) {}
    ~gil_scoped_release() { PyEval_RestoreThread(state_); }
    gil_scoped_release(const gil_scoped_release &) = delete;
};
namespace detail {
inline void incref(PyObject *p) {
    if (p) { gil_scoped_acquire gil; pyobj_incref(p); }
}
inline void decref(PyObject *p) noexcept {
    if (p && pyinit_is_initialized()) {
        auto state = pyinit_gil_ensure();
        pyobj_decref(p);
        pyinit_gil_release(state);
    }
}
struct steal_t {};
struct borrow_t {};
}
class object {
protected:
    PyObject *p_ = nullptr;
    PyObject *lookup_error_ = nullptr;
public:
    object() = default;
    object(PyObject *p, detail::steal_t) noexcept : p_(p) {}
    object(PyObject *p, detail::borrow_t) : p_(p) { detail::incref(p_); }
    object(const object &o) : object(o.p_, detail::borrow_t{}) { lookup_error_ = o.lookup_error_; detail::incref(lookup_error_); }
    object(object &&o) noexcept : p_(std::exchange(o.p_, nullptr)), lookup_error_(std::exchange(o.lookup_error_, nullptr)) {}
    ~object() { detail::decref(p_); detail::decref(lookup_error_); }
    object &operator=(object o) noexcept { std::swap(p_, o.p_); std::swap(lookup_error_, o.lookup_error_); return *this; }
    PyObject *ptr() const noexcept { return p_; }
    PyObject *release() noexcept { return std::exchange(p_, nullptr); }
    PyObject *release_ptr() noexcept { return std::exchange(p_, nullptr); }
    PyObject *lookup_error() const noexcept { return lookup_error_; }
    bool is_valid() const noexcept { return p_ != nullptr; }
    bool is_none() const noexcept { return p_ == Py_None; }
    explicit operator bool() const noexcept { return is_valid(); }
    bool is(const object &o) const noexcept { return p_ == o.p_; }
    accessor attr(const char *name) const;
    accessor operator[](const char *key) const;
    accessor operator[](const object &key) const;
    accessor operator[](Py_ssize_t i) const;
    template<class... A> object operator()(A &&...args) const;
    size_t size() const;
    iterator begin() const;
    iterator end() const;
};
// Handles retain references as well: temporaries cannot leave dangling borrowed
// handles in native clients. Use ptr() only while its owner remains alive.
using handle = object;
template<class T = object> T borrow(const object &o) { return T(o.ptr(), detail::borrow_t{}); }
template<class T = object> T borrow(PyObject *p) { return T(p, detail::borrow_t{}); }
template<class T = object> T steal(PyObject *p) { return T(p, detail::steal_t{}); }

class python_error : public std::exception {
    object value_;
    std::string message_;
public:
    python_error() : value_(PyErr_GetRaisedException(), detail::steal_t{}) {
        if (!value_) { message_ = "Python operation failed without an exception"; return; }
        object text(pyobj_str(value_.ptr()), detail::steal_t{});
        const char *s = text ? PyUnicode_AsUTF8(text.ptr()) : nullptr;
        message_ = s ? s : "Python exception";
        PyErr_Clear();
    }
    const char *what() const noexcept override { return message_.c_str(); }
    object value() const { return value_; }
    void restore() const {
        if (value_) PyErr_SetRaisedException(Py_NewRef(value_.ptr()));
        else PyErr_SetString(PyExc_RuntimeError, what());
    }
    bool matches(handle type) const { return value_ && PyErr_GivenExceptionMatches(value_.ptr(), type.ptr()); }
};
namespace detail {
inline PyObject *checked(PyObject *p) { if (!p) throw python_error(); return p; }
inline void checked(int rc) { if (rc < 0) throw python_error(); }
inline void require(const object &o) {
    if (o.lookup_error()) { PyErr_SetRaisedException(Py_NewRef(o.lookup_error())); throw python_error(); }
    if (!o) throw std::runtime_error("empty Python handle");
}
struct argument_error {
    std::exception_ptr cause;
    explicit argument_error(std::exception_ptr e) : cause(e) {}
    explicit argument_error(const std::string &s) : cause(std::make_exception_ptr(std::invalid_argument(s))) {}
};
inline void translate_exception() noexcept {
    try { throw; }
    catch (const argument_error &e) { try { std::rethrow_exception(e.cause); } catch (...) { translate_exception(); } }
    catch (const python_error &e) { e.restore(); }
    catch (const std::bad_alloc &) { PyErr_NoMemory(); }
    catch (const std::overflow_error &e) { PyErr_SetString(PyExc_OverflowError, e.what()); }
    catch (const std::out_of_range &e) { PyErr_SetString(PyExc_IndexError, e.what()); }
    catch (const std::invalid_argument &e) { PyErr_SetString(PyExc_TypeError, e.what()); }
    catch (const std::exception &e) { PyErr_SetString(PyExc_RuntimeError, e.what()); }
    catch (...) { PyErr_SetString(PyExc_RuntimeError, "unrecognized C++ exception"); }
}
}
class accessor : public object {
    object parent_, key_;
    bool attribute_;
public:
    accessor(const object &p, const object &k, bool attr) : parent_(p), key_(k), attribute_(attr) {
        detail::require(p);
        // Missing items are permitted only for assignment; conversion/call of
        // an unreadable accessor re-raises the original lookup exception.
        p_ = attr ? PyObject_GetAttr(p.ptr(), k.ptr()) : PyObject_GetItem(p.ptr(), k.ptr());
        if (!p_) {
            if (!PyErr_ExceptionMatches(PyExc_KeyError) && !PyErr_ExceptionMatches(PyExc_AttributeError))
                throw python_error();
            lookup_error_ = PyErr_GetRaisedException();
        }
    }
    accessor &operator=(const accessor &v) { return assign(v); }
    template<class T> accessor &operator=(T &&v) { return assign(std::forward<T>(v)); }
private:
    template<class T> accessor &assign(T &&v) {
        object value = cast(std::forward<T>(v));
        detail::require(value);
        detail::checked(attribute_ ? PyObject_SetAttr(parent_.ptr(), key_.ptr(), value.ptr())
                                  : PyObject_SetItem(parent_.ptr(), key_.ptr(), value.ptr()));
        object::operator=(value);
        return *this;
    }
};
inline accessor object::attr(const char *name) const {
    return accessor(*this, steal(detail::checked(PyUnicode_FromString(name))), true);
}
inline accessor object::operator[](const char *key) const {
    return accessor(*this, steal(detail::checked(PyUnicode_FromString(key))), false);
}
inline accessor object::operator[](const object &key) const { return accessor(*this, key, false); }
inline accessor object::operator[](Py_ssize_t i) const { return accessor(*this, steal(detail::checked(PyLong_FromSsize_t(i))), false); }
inline size_t object::size() const {
    detail::require(*this); auto n = PyObject_Size(p_); if (n < 0) throw python_error(); return static_cast<size_t>(n);
}
inline bool hasattr(const object &o, const char *name) {
    detail::require(o); int rc = PyObject_HasAttrStringWithError(o.ptr(), name); detail::checked(rc); return rc != 0;
}
class iterator {
    object iter_, value_;
    void advance() {
        value_ = steal(PyIter_Next(iter_.ptr()));
        if (!value_ && PyErr_Occurred()) throw python_error();
    }
public:
    iterator() = default;
    explicit iterator(const object &o) : iter_(steal(detail::checked(PyObject_GetIter(o.ptr())))) { advance(); }
    const object &operator*() const { return value_; }
    iterator &operator++() { advance(); return *this; }
    bool operator!=(const iterator &o) const { return value_.ptr() != o.value_.ptr(); }
};
inline iterator object::begin() const { detail::require(*this); return iterator(*this); }
inline iterator object::end() const { return iterator(); }
class iterable : public object { public: using object::object; };
class str : public object {
public:
    using object::object;
    explicit str(const char *s) : object(detail::checked(PyUnicode_FromString(s)), detail::steal_t{}) {}
    explicit str(const std::string &s) : object(detail::checked(PyUnicode_FromStringAndSize(s.data(), s.size())), detail::steal_t{}) {}
    explicit str(const object &o) : object(detail::checked(PyObject_Str(o.ptr())), detail::steal_t{}) {}
    const char *c_str() const { return PyUnicode_AsUTF8(p_); }
};
class bytes : public object {
public:
    using object::object;
    bytes(const char *s, size_t n) : object(detail::checked(PyBytes_FromStringAndSize(s, n)), detail::steal_t{}) {}
    const char *c_str() const { return PyBytes_AsString(p_); }
};
class int_ : public object {
public:
    using object::object;
    explicit int_(long long n) : object(detail::checked(PyLong_FromLongLong(n)), detail::steal_t{}) {}
};
class float_ : public object {
public: using object::object;
    explicit float_(double n) : object(detail::checked(PyFloat_FromDouble(n)), detail::steal_t{}) {}
};
class bool_ : public object {
public: using object::object;
    explicit bool_(bool n) : object(PyBool_FromLong(n), detail::steal_t{}) {}
};
inline object none() { return borrow(Py_None); }
class list : public object {
public:
    using object::object;
    list() : object(detail::checked(PyList_New(0)), detail::steal_t{}) {}
    template<class T> void append(T &&v) { auto o = cast(std::forward<T>(v)); detail::checked(PyList_Append(p_, o.ptr())); }
};
class tuple : public object {
public:
    using object::object;
    explicit tuple(size_t n = 0) : object(detail::checked(PyTuple_New(n)), detail::steal_t{}) {}
};
class dict : public object {
public:
    using object::object;
    dict() : object(detail::checked(PyDict_New()), detail::steal_t{}) {}
    template<class K> bool contains(K &&k) const { auto key = cast(std::forward<K>(k)); int rc = PyDict_Contains(p_, key.ptr()); detail::checked(rc); return rc != 0; }
    class dict_iterator {
        object items_; Py_ssize_t i_ = 0;
    public:
        dict_iterator() = default;
        explicit dict_iterator(const object &d) : items_(steal(detail::checked(PyDict_Items(d.ptr())))) {}
        std::pair<object,object> operator*() const {
            auto *pair = PyList_GetItem(items_.ptr(), i_);
            return {borrow(PyTuple_GetItem(pair,0)), borrow(PyTuple_GetItem(pair,1))};
        }
        dict_iterator &operator++() { ++i_; return *this; }
        bool operator!=(const dict_iterator &) const { return items_ && i_ < PyList_Size(items_.ptr()); }
    };
    dict_iterator begin() const { return dict_iterator(*this); }
    dict_iterator end() const { return dict_iterator(); }
};

enum class rv_policy { automatic, copy, move, reference, reference_internal, take_ownership };
namespace detail {
template<class T> using bare = std::remove_cv_t<std::remove_reference_t<T>>;
template<class T> struct vector_traits : std::false_type {};
template<class T, class A> struct vector_traits<std::vector<T,A>> : std::true_type { using item = T; };
template<class T> struct map_traits : std::false_type {};
template<class K, class V, class... A> struct map_traits<std::map<K,V,A...>> : std::true_type { using key = K; using mapped = V; };
template<class K, class V, class... A> struct map_traits<std::unordered_map<K,V,A...>> : std::true_type { using key = K; using mapped = V; };
template<class T> struct optional_traits : std::false_type {};
template<class T> struct optional_traits<std::optional<T>> : std::true_type { using item = T; };
template<class T> struct unique_traits : std::false_type {};
template<class T> struct unique_traits<std::unique_ptr<T>> : std::true_type { using item = T; };
template<class T> struct shared_traits : std::false_type {};
template<class T> struct shared_traits<std::shared_ptr<T>> : std::true_type { using item = T; };
template<class T> struct variant_traits : std::false_type {};
template<class... T> struct variant_traits<std::variant<T...>> : std::true_type {};
template<class T, size_t I = 0> T load_variant(const object &o) {
    if constexpr (I == std::variant_size_v<T>) throw std::invalid_argument("no matching variant alternative");
    else {
        try { return T(std::in_place_index<I>, cast<std::variant_alternative_t<I,T>>(o)); }
        catch (const python_error &) { return load_variant<T,I+1>(o); }
        catch (const std::invalid_argument &) { return load_variant<T,I+1>(o); }
    }
}
template<class T> constexpr bool native_type = !std::is_arithmetic_v<T> && !std::is_enum_v<T> &&
    !std::is_base_of_v<object,T> && !std::is_pointer_v<T> && !std::is_array_v<T> &&
    !std::is_same_v<T,std::string> && !std::is_same_v<T,std::string_view> &&
    !vector_traits<T>::value && !map_traits<T>::value && !optional_traits<T>::value &&
    !unique_traits<T>::value && !shared_traits<T>::value && !variant_traits<T>::value;
// Registry belongs to the interpreter, not a process-global static PyObject.
inline PyObject *registry() {
    auto *state = PyInterpreterState_GetDict(PyInterpreterState_Get());
    if (!state) throw std::runtime_error("interpreter dictionary unavailable");
    auto *r = PyDict_GetItemString(state, "_dompybind_types_v1");
    if (!r) {
        auto value = steal(checked(PyDict_New()));
        checked(PyDict_SetItemString(state, "_dompybind_types_v1", value.ptr())); r = value.ptr();
    }
    return r;
}
template<class T> PyTypeObject *registered_type() {
    auto *p = PyDict_GetItemString(registry(), typeid(T).name());
    if (!p) throw std::invalid_argument(std::string("unregistered native type: ") + typeid(T).name());
    return reinterpret_cast<PyTypeObject*>(p);
}
struct instance {
    PyObject_HEAD
    void *value;
    void *holder;
    void (*destroy)(void *);
    PyObject *owner;
};
template<class T> T *native_pointer(const object &o) {
    if (!o || !PyObject_TypeCheck(o.ptr(), registered_type<T>())) throw std::invalid_argument("incompatible native object type");
    auto *p = reinterpret_cast<instance*>(o.ptr())->value;
    if (!p) throw std::invalid_argument("native object is not initialized");
    return static_cast<T*>(p);
}
template<class T> object wrap_pointer(T *value, bool own, const object &parent = {}) {
    if (!value) return none();
    auto *type = registered_type<std::remove_const_t<T>>();
    auto *p = reinterpret_cast<instance*>(checked(type->tp_alloc(type, 0)));
    p->value = const_cast<std::remove_const_t<T>*>(value);
    p->holder = p->value;
    p->destroy = own ? +[](void *v) { delete static_cast<T*>(v); } : nullptr;
    p->owner = parent ? Py_NewRef(parent.ptr()) : nullptr;
    return steal(reinterpret_cast<PyObject*>(p));
}
}

template<class T> decltype(auto) cast(const object &o) {
    using U = detail::bare<T>;
    detail::require(o);
    if constexpr (std::is_base_of_v<object,U>) {
        bool ok = true;
        if constexpr (std::is_same_v<U,dict>) ok = PyDict_Check(o.ptr());
        if constexpr (std::is_same_v<U,list>) ok = PyList_Check(o.ptr());
        if constexpr (std::is_same_v<U,tuple>) ok = PyTuple_Check(o.ptr());
        if constexpr (std::is_same_v<U,str>) ok = PyUnicode_Check(o.ptr());
        if constexpr (std::is_same_v<U,bytes>) ok = PyBytes_Check(o.ptr());
        if (!ok) throw std::invalid_argument("incompatible Python container type");
        return U(o.ptr(), detail::borrow_t{});
    } else if constexpr (std::is_same_v<U,std::string>) {
        Py_ssize_t n; const char *s = PyUnicode_AsUTF8AndSize(o.ptr(), &n);
        if (!s) throw python_error();
        return std::string(s, static_cast<size_t>(n));
    } else if constexpr (std::is_same_v<U,bool>) {
        int n = PyObject_IsTrue(o.ptr()); detail::checked(n); return n != 0;
    } else if constexpr (std::is_integral_v<U>) {
        if constexpr (std::is_signed_v<U>) {
            long long n = PyLong_AsLongLong(o.ptr());
            if (PyErr_Occurred()) throw python_error();
            if (n < std::numeric_limits<U>::min() || n > std::numeric_limits<U>::max()) throw std::overflow_error("integer conversion overflow");
            return static_cast<U>(n);
        } else {
            unsigned long long n = PyLong_AsUnsignedLongLong(o.ptr());
            if (PyErr_Occurred()) throw python_error();
            if (n > std::numeric_limits<U>::max()) throw std::overflow_error("integer conversion overflow");
            return static_cast<U>(n);
        }
    } else if constexpr (std::is_floating_point_v<U>) {
        double n = PyFloat_AsDouble(o.ptr()); if (PyErr_Occurred()) throw python_error(); return static_cast<U>(n);
    } else if constexpr (std::is_enum_v<U>) {
        return static_cast<U>(cast<std::underlying_type_t<U>>(o));
    } else if constexpr (detail::vector_traits<U>::value) {
        U result; for (auto v : o) result.push_back(cast<typename U::value_type>(v)); return result;
    } else if constexpr (detail::map_traits<U>::value) {
        U result; for (auto entry : cast<dict>(o)) result.emplace(cast<typename U::key_type>(entry.first), cast<typename U::mapped_type>(entry.second)); return result;
    } else if constexpr (detail::optional_traits<U>::value) {
        if (o.is_none()) return U{};
        return U{cast<typename U::value_type>(o)};
    } else if constexpr (std::is_pointer_v<U>) {
        if (o.is_none()) return static_cast<U>(nullptr);
        return detail::native_pointer<std::remove_const_t<std::remove_pointer_t<U>>>(o);
    } else if constexpr (detail::shared_traits<U>::value) {
        if (o.is_none()) return U{};
        using V = typename U::element_type;
        return U(detail::native_pointer<V>(o), [owner = object(o)](V *) mutable { owner = {}; });
    } else if constexpr (detail::variant_traits<U>::value) {
        return detail::load_variant<U>(o);
    } else if constexpr (std::is_reference_v<T>) {
        return static_cast<T>(*detail::native_pointer<U>(o));
    } else {
        return U(*detail::native_pointer<U>(o));
    }
}
template<class T> object cast(T &&v) {
    using U = detail::bare<T>;
    if constexpr (std::is_base_of_v<object,U>) { detail::require(v); return object(v); }
    else if constexpr (std::is_same_v<U,std::nullptr_t> || std::is_same_v<U,std::nullopt_t> || std::is_same_v<U,std::monostate>) return none();
    else if constexpr (std::is_same_v<U,std::string> || std::is_same_v<U,std::string_view>) return steal(detail::checked(PyUnicode_FromStringAndSize(v.data(), v.size())));
    else if constexpr (std::is_convertible_v<T,const char*>) {
        if constexpr (std::is_array_v<U>) return steal(detail::checked(PyUnicode_FromString(v)));
        else return v ? steal(detail::checked(PyUnicode_FromString(v))) : none();
    }
    else if constexpr (std::is_same_v<U,bool>) return steal(PyBool_FromLong(v));
    else if constexpr (std::is_integral_v<U> && std::is_signed_v<U>) return steal(detail::checked(PyLong_FromLongLong(v)));
    else if constexpr (std::is_integral_v<U>) return steal(detail::checked(PyLong_FromUnsignedLongLong(v)));
    else if constexpr (std::is_floating_point_v<U>) return steal(detail::checked(PyFloat_FromDouble(v)));
    else if constexpr (std::is_enum_v<U>) {
        auto integer = cast(static_cast<std::underlying_type_t<U>>(v));
        auto *type = PyDict_GetItemString(detail::registry(), typeid(U).name());
        return type ? borrow(type)(integer) : integer;
    }
    else if constexpr (detail::vector_traits<U>::value) { list r; for (auto &&item : v) r.append(item); return r; }
    else if constexpr (detail::map_traits<U>::value) { dict r; for (auto &&item : v) r[cast(item.first)] = cast(item.second); return r; }
    else if constexpr (detail::optional_traits<U>::value) { return v ? cast(*v) : none(); }
    else if constexpr (detail::unique_traits<U>::value) {
        auto *p = v.get(); auto result = detail::wrap_pointer(p, true); v.release(); return result;
    } else if constexpr (detail::shared_traits<U>::value) {
        if (!v) return none();
        auto holder = std::make_unique<U>(v);
        auto result = detail::wrap_pointer(v.get(), false);
        auto *instance = reinterpret_cast<detail::instance*>(result.ptr());
        instance->holder = holder.release();
        instance->destroy = +[](void *p) { delete static_cast<U*>(p); };
        return result;
    } else if constexpr (detail::variant_traits<U>::value) {
        return std::visit([](auto &&item) { return cast(std::forward<decltype(item)>(item)); }, std::forward<T>(v));
    } else if constexpr (std::is_pointer_v<U>) return detail::wrap_pointer(v, false);
    else { auto p = std::make_unique<U>(std::forward<T>(v)); auto result = detail::wrap_pointer(p.get(), true); p.release(); return result; }
}
// Explicit requested casts must win over the forwarding-to-Python overload.
template<class T, class O, std::enable_if_t<std::is_base_of_v<object, detail::bare<O>> && !std::is_same_v<detail::bare<O>,object>,int> = 0>
decltype(auto) cast(const O &o) { return cast<T>(static_cast<const object&>(o)); }
template<class T> bool isinstance(const object &o) {
    if (!o) return false;
    if constexpr (std::is_same_v<T,dict>) return PyDict_Check(o.ptr());
    else if constexpr (std::is_same_v<T,list>) return PyList_Check(o.ptr());
    else if constexpr (std::is_same_v<T,str>) return PyUnicode_Check(o.ptr());
    else if constexpr (std::is_same_v<T,bool_>) return PyBool_Check(o.ptr());
    else if constexpr (std::is_same_v<T,int_>) return PyLong_Check(o.ptr());
    else if constexpr (std::is_same_v<T,float_>) return PyFloat_Check(o.ptr());
    else return PyObject_TypeCheck(o.ptr(), detail::registered_type<T>());
}
struct arg {
    std::string name;
    object value;
    explicit arg(const char *s) : name(s) {}
    template<class T> arg operator=(T &&v) const { arg a(*this); a.value = cast(std::forward<T>(v)); return a; }
};
namespace detail {
inline void append_call_arg(list &, dict &kwargs, const arg &a) {
    if (!a.value) throw std::invalid_argument("keyword argument has no value");
    if (kwargs.contains(a.name)) throw std::invalid_argument("duplicate keyword argument");
    kwargs[a.name.c_str()] = a.value;
}
template<class T> void append_call_arg(list &args, dict &, T &&v) { args.append(std::forward<T>(v)); }
inline void append_call_arg(list &args, dict &kwargs, arg &&a) { append_call_arg(args, kwargs, static_cast<const arg&>(a)); }
inline void append_call_arg(list &args, dict &kwargs, arg &a) { append_call_arg(args, kwargs, static_cast<const arg&>(a)); }
}
template<class... A> object object::operator()(A &&...args) const {
    detail::require(*this); list positional; dict keyword;
    (detail::append_call_arg(positional, keyword, std::forward<A>(args)), ...);
    auto tuple_args = steal(detail::checked(PyList_AsTuple(positional.ptr())));
    return steal(detail::checked(pycall_call(p_, tuple_args.ptr(), keyword.ptr())));
}
template<class... A> tuple make_tuple(A &&...args) {
    list values; (values.append(std::forward<A>(args)), ...);
    return steal<tuple>(detail::checked(PyList_AsTuple(values.ptr())));
}

class module_ : public object {
public:
    using object::object;
    static module_ import_(const char *name) { return steal<module_>(detail::checked(PyImport_ImportModule(name))); }
    accessor doc() const { return attr("__doc__"); }
    template<class F, class... E> void def(const char *name, F &&f, E &&...extra);
};
} // namespace dompybind
#include "binding.hpp"
