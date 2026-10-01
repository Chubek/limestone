#pragma once
// Native function and class publication. Python owns all callback state.
namespace dompybind {
namespace detail {
template<class T> struct signature;
template<class R, class... A> struct signature<R(*)(A...)> { using result = R; using args = std::tuple<A...>; };
template<class R, class... A> struct signature<R(*)(A...) noexcept> : signature<R(*)(A...)> {};
template<class R, class C, class... A> struct signature<R(C::*)(A...)> { using result = R; using args = std::tuple<C&,A...>; };
template<class R, class C, class... A> struct signature<R(C::*)(A...) const> { using result = R; using args = std::tuple<const C&,A...>; };
template<class R, class C, class... A> struct signature<R(C::*)(A...) noexcept> : signature<R(C::*)(A...)> {};
template<class R, class C, class... A> struct signature<R(C::*)(A...) const noexcept> : signature<R(C::*)(A...) const> {};
template<class T> struct lambda_signature;
template<class R, class C, class... A> struct lambda_signature<R(C::*)(A...) const> : signature<R(*)(A...)> {};
template<class R, class C, class... A> struct lambda_signature<R(C::*)(A...)> : signature<R(*)(A...)> {};
template<class R, class C, class... A> struct lambda_signature<R(C::*)(A...) const noexcept> : signature<R(*)(A...)> {};
template<class R, class C, class... A> struct lambda_signature<R(C::*)(A...) noexcept> : signature<R(*)(A...)> {};
template<class T> struct signature : lambda_signature<decltype(&T::operator())> {};

struct call_options {
    std::vector<arg> arguments;
    std::string doc;
    rv_policy policy = rv_policy::automatic;
    void add(const arg &a) { arguments.push_back(a); }
    void add(const char *s) { doc = s; }
    void add(rv_policy p) { policy = p; }
};
inline std::vector<object> bind_arguments(PyObject *pos, PyObject *kw, size_t count,
                                          const call_options &options, bool method) {
    const size_t supplied = static_cast<size_t>(PyTuple_Size(pos));
    if (supplied > count) throw argument_error("too many positional arguments");
    std::vector<object> values(count);
    for (size_t i=0; i<supplied; ++i) values[i] = borrow(PyTuple_GetItem(pos, i));
    size_t consumed = 0;
    for (size_t i=0; i<count; ++i) {
        const size_t offset = method ? 1 : 0;
        if (i < offset || i-offset >= options.arguments.size()) continue;
        const auto &param = options.arguments[i-offset];
        PyObject *v = kw ? PyDict_GetItemString(kw, param.name.c_str()) : nullptr;
        if (v) {
            if (values[i]) throw argument_error("multiple values for argument '" + param.name + "'");
            values[i] = borrow(v); ++consumed;
        }
        if (!values[i] && param.value) values[i] = param.value;
    }
    if (kw && consumed != static_cast<size_t>(PyDict_Size(kw))) throw argument_error("unexpected keyword argument");
    for (const auto &v : values) if (!v) throw argument_error("missing required argument");
    return values;
}
template<class T> auto load_argument(const object &o) {
    using U = bare<T>;
    try {
    if constexpr (native_type<U> && std::is_lvalue_reference_v<T>) return std::ref(cast<T>(o));
    else return cast<U>(o);
    } catch (...) { throw argument_error(std::current_exception()); }
}
template<class T> object return_value(T &&v, rv_policy policy, const object &parent) {
    using U = bare<T>;
    if constexpr (native_type<U> && std::is_lvalue_reference_v<T>) {
        if (policy == rv_policy::reference || policy == rv_policy::reference_internal)
            return wrap_pointer(&v, false, policy == rv_policy::reference_internal ? parent : object{});
    }
    if constexpr (std::is_pointer_v<U> && !std::is_convertible_v<U,const char*>) {
        return wrap_pointer(v, policy == rv_policy::take_ownership,
                            policy == rv_policy::reference_internal ? parent : object{});
    } else return cast(std::forward<T>(v));
}
template<class F, size_t... I> object invoke(F &f, const std::vector<object> &values,
                                          const call_options &opts, std::index_sequence<I...>) {
    using sig = signature<F>;
    // Materialize conversion temporaries until the entire invocation returns.
    auto args = std::make_tuple(load_argument<std::tuple_element_t<I,typename sig::args>>(values[I])...);
    if constexpr (std::is_void_v<typename sig::result>) {
        std::apply(f, args); return none();
    } else {
        decltype(auto) value = std::apply(f, args);
        return return_value(std::forward<decltype(value)>(value), opts.policy, values.empty() ? object{} : values[0]);
    }
}
struct callback {
    std::string name, doc;
    PyMethodDef method{};
    std::vector<std::function<object(PyObject*,PyObject*)>> overloads;
};
inline PyObject *dispatch(PyObject *capsule, PyObject *args, PyObject *kwargs) noexcept {
    try {
        auto *cb = static_cast<callback*>(PyCapsule_GetPointer(capsule, "dompybind.callback"));
        if (!cb) throw python_error();
        std::exception_ptr failure;
        for (auto &invoke : cb->overloads) {
            try { return invoke(args, kwargs).release_ptr(); }
            catch (const argument_error &) { failure = std::current_exception(); }
        }
        if (failure) std::rethrow_exception(failure);
        throw std::invalid_argument("no overload accepts these arguments");
    } catch (...) { translate_exception(); return nullptr; }
}
inline object make_callback(const char *name, std::string doc,
                            std::function<object(PyObject*,PyObject*)> invoke) {
    auto cb = std::make_unique<callback>();
    cb->name = name; cb->doc = std::move(doc); cb->overloads.push_back(std::move(invoke));
    cb->method = {cb->name.c_str(), reinterpret_cast<PyCFunction>(reinterpret_cast<void(*)(void)>(dispatch)),
                  METH_VARARGS | METH_KEYWORDS, cb->doc.c_str()};
    auto capsule = steal(checked(PyCapsule_New(cb.get(), "dompybind.callback", [](PyObject *p) {
        delete static_cast<callback*>(PyCapsule_GetPointer(p, "dompybind.callback"));
    })));
    auto *raw = cb.release();
    return steal(checked(PyCFunction_NewEx(&raw->method, capsule.ptr(), nullptr)));
}
template<class F, class... E> object make_function(const char *name, F &&f, bool method, E &&...extra) {
    using Fn = bare<F>;
    call_options opts; (opts.add(std::forward<E>(extra)), ...);
    constexpr size_t n = std::tuple_size_v<typename signature<Fn>::args>;
    if (opts.arguments.size() > n - (method ? 1u : 0u)) throw std::invalid_argument("too many binding argument names");
    std::string doc = opts.doc;
    return make_callback(name, std::move(doc), [fn = Fn(std::forward<F>(f)), opts = std::move(opts), method](PyObject *args, PyObject *kw) mutable {
        auto values = bind_arguments(args, kw, n, opts, method);
        return invoke(fn, values, opts, std::make_index_sequence<n>{});
    });
}
inline object combine_overloads(const object &owner, const char *name, object next) {
    PyObject *raw = nullptr;
    int found = PyObject_GetOptionalAttrString(owner.ptr(), name, &raw);
    checked(found);
    auto old = steal(raw);
    if (!old) return next;
    if (hasattr(old, "_dompybind_native")) old = old.attr("_dompybind_native");
    if (!PyCFunction_Check(old.ptr())) return next;
    auto *capsule = PyCFunction_GetSelf(old.ptr());
    if (!capsule || !PyCapsule_IsValid(capsule, "dompybind.callback")) return next;
    auto *existing = static_cast<callback*>(PyCapsule_GetPointer(capsule, "dompybind.callback"));
    auto *added = static_cast<callback*>(PyCapsule_GetPointer(PyCFunction_GetSelf(next.ptr()), "dompybind.callback"));
    if (!added) throw python_error();
    for (auto &overload : added->overloads) existing->overloads.push_back(std::move(overload));
    return old;
}
inline object method_descriptor(const object &fn) {
    // A Python function supplies descriptor binding (including keyword calls)
    // while the capsule-backed C function performs conversion and invocation.
    dict globals; globals["__builtins__"] = borrow(PyEval_GetBuiltins());
    auto factory = steal(checked(PyRun_String("lambda native: lambda self, *args, **kw: native(self, *args, **kw)", Py_eval_input, globals.ptr(), globals.ptr())));
    auto result = factory(fn);
    result.attr("_dompybind_native") = fn;
    return result;
}
inline void instance_dealloc(PyObject *o) noexcept {
    auto *p = reinterpret_cast<instance*>(o);
    PyObject_GC_UnTrack(o);
    if (p->destroy) p->destroy(p->holder);
    Py_XDECREF(p->owner);
    auto *type = Py_TYPE(o);
    type->tp_free(o);
    Py_DECREF(type);
}
inline int instance_traverse(PyObject *o, visitproc visit, void *arg) {
    Py_VISIT(reinterpret_cast<instance*>(o)->owner);
    Py_VISIT(Py_TYPE(o));
    return 0;
}
inline int instance_clear(PyObject *o) { Py_CLEAR(reinterpret_cast<instance*>(o)->owner); return 0; }
inline int instance_init(PyObject *o, PyObject *args, PyObject *kw) noexcept {
    try {
        if (reinterpret_cast<instance*>(o)->value) throw std::invalid_argument("native object already initialized");
        auto init = borrow(o).attr("_dompybind_init");
        if (!init) throw std::invalid_argument("this native type has no public constructor");
        auto result = steal(checked(PyObject_Call(init.ptr(), args, kw)));
        return 0;
    } catch (...) { translate_exception(); return -1; }
}
}
template<class F, class... E> void module_::def(const char *name, F &&f, E &&...extra) {
    attr(name) = detail::combine_overloads(*this, name, detail::make_function(name, std::forward<F>(f), false, std::forward<E>(extra)...));
}
template<class... A> struct init {};
template<class T> class class_ : public object {
public:
    class_(module_ &module, const char *name) {
        std::string qualified = cast<std::string>(module.attr("__name__")) + "." + name;
        PyType_Slot slots[] = {
            {Py_tp_new, reinterpret_cast<void*>(PyType_GenericNew)},
            {Py_tp_init, reinterpret_cast<void*>(detail::instance_init)},
            {Py_tp_dealloc, reinterpret_cast<void*>(detail::instance_dealloc)},
            {Py_tp_traverse, reinterpret_cast<void*>(detail::instance_traverse)},
            {Py_tp_clear, reinterpret_cast<void*>(detail::instance_clear)}, {0,nullptr}};
        PyType_Spec spec{qualified.c_str(), sizeof(detail::instance), 0,
                        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, slots};
        p_ = detail::checked(PyType_FromSpec(&spec));
        detail::checked(PyDict_SetItemString(detail::registry(), typeid(T).name(), p_));
        module.attr(name) = *this;
    }
    template<class F, class... E> class_ &def(const char *name, F &&f, E &&...extra) {
        auto fn = detail::make_function(name, std::forward<F>(f), true, std::forward<E>(extra)...);
        attr(name) = detail::method_descriptor(detail::combine_overloads(*this, name, fn)); return *this;
    }
    template<class... A, class... E> class_ &def(init<A...>, E &&...extra) {
        detail::call_options opts; (opts.add(std::forward<E>(extra)), ...);
        auto fn = detail::make_callback("__init__", opts.doc, [opts](PyObject *args, PyObject *kw) {
            auto values = detail::bind_arguments(args, kw, sizeof...(A)+1, opts, true);
            construct<A...>(values, std::index_sequence_for<A...>{});
            return none();
        });
        attr("_dompybind_init") = detail::method_descriptor(detail::combine_overloads(*this, "_dompybind_init", fn));
        return *this;
    }
    template<class F, class... E> class_ &def_static(const char *name, F &&f, E &&...extra) {
        auto fn = detail::make_function(name, std::forward<F>(f), false, std::forward<E>(extra)...);
        attr(name) = steal(detail::checked(PyStaticMethod_New(fn.ptr()))); return *this;
    }
    template<class F, class... E> class_ &def_prop_ro(const char *name, F &&f, E &&...extra) {
        auto fn = detail::make_function(name, std::forward<F>(f), true, std::forward<E>(extra)...);
        attr(name) = module_::import_("builtins").attr("property")(fn); return *this;
    }
    template<class F, class S, class... E> class_ &def_prop_rw(const char *name, F &&f, S &&s, E &&...extra) {
        auto get = detail::make_function(name, std::forward<F>(f), true, std::forward<E>(extra)...);
        auto set = detail::make_function(name, std::forward<S>(s), true);
        attr(name) = module_::import_("builtins").attr("property")(get, set); return *this;
    }
    template<class V, class C, class... E> class_ &def_ro(const char *name, V C::*member, E &&...extra) {
        return def_prop_ro(name, [member](const T &self) -> const V& { return self.*member; }, std::forward<E>(extra)...);
    }
    template<class F, class... E> class_ &def_ro(const char *name, F &&f, E &&...extra) {
        return def_prop_ro(name, std::forward<F>(f), std::forward<E>(extra)...);
    }
    template<class V, class C, class... E> class_ &def_rw(const char *name, V C::*member, E &&...extra) {
        return def_prop_rw(name, [member](const T &self) -> const V& { return self.*member; },
                           [member](T &self, V v) { self.*member = std::move(v); }, std::forward<E>(extra)...);
    }
private:
    template<class... A, size_t... I> static void construct(const std::vector<object> &values, std::index_sequence<I...>) {
        if (!PyObject_TypeCheck(values[0].ptr(), detail::registered_type<T>())) throw std::invalid_argument("incompatible constructor self");
        auto *self = reinterpret_cast<detail::instance*>(values[0].ptr());
        if (self->value) throw std::invalid_argument("native object already initialized");
        auto args = std::make_tuple(detail::load_argument<A>(values[I+1])...);
        auto value = std::apply([](auto &&...a) { return std::make_unique<T>(std::forward<decltype(a)>(a)...); }, args);
        self->destroy = +[](void *v) { delete static_cast<T*>(v); };
        self->value = value.release();
        self->holder = self->value;
    }
};
// Publish a genuine IntEnum after each value addition. The finalized enum is
// available before dependent class/function defaults are registered.
template<class T> class enum_ {
    dompybind::module_ module_handle_;
    std::string name_;
    dict values_;
public:
    enum_(dompybind::module_ &m, const char *name) : module_handle_(borrow<dompybind::module_>(m)), name_(name) {}
    enum_ &value(const char *name, T value) {
        values_[name] = static_cast<std::underlying_type_t<T>>(value);
        auto type = dompybind::module_::import_("enum").attr("IntEnum")(name_, values_, arg("module") = cast<std::string>(module_handle_.attr("__name__")));
        module_handle_.attr(name_.c_str()) = type;
        detail::checked(PyDict_SetItemString(detail::registry(), typeid(T).name(), type.ptr()));
        return *this;
    }
    enum_ &export_values() {
        auto type = module_handle_.attr(name_.c_str());
        for (auto item : values_) module_handle_.attr(cast<std::string>(item.first).c_str()) = type[item.first];
        return *this;
    }
};
} // namespace dompybind

#define DOMPYBIND_MODULE(name, variable) \
    static void dompybind_init_##name(::dompybind::module_ &); \
    PyMODINIT_FUNC PyInit_##name(void) { \
        static PyModuleDef def = {PyModuleDef_HEAD_INIT, #name, nullptr, -1, nullptr, nullptr, nullptr, nullptr, nullptr}; \
        try { \
            auto module = ::dompybind::steal<::dompybind::module_>(::dompybind::detail::checked(PyModule_Create(&def))); \
            dompybind_init_##name(module); \
            return module.release_ptr(); \
        } catch (...) { ::dompybind::detail::translate_exception(); return nullptr; } \
    } \
    static void dompybind_init_##name(::dompybind::module_ &variable)
