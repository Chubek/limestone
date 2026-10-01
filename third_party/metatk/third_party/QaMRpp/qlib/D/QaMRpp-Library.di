module qamrpp.library;

extern(C) {
    enum QAMRPP_LIBRARY_API_VERSION = 1;
    enum QAMRPP_TYPE_NIL = 0, QAMRPP_TYPE_BOOL = 1, QAMRPP_TYPE_INT = 2,
         QAMRPP_TYPE_FLOAT = 3, QAMRPP_TYPE_STRING = 4, QAMRPP_TYPE_FUNCTION = 5,
         QAMRPP_TYPE_USERDATA = 6, QAMRPP_TYPE_TABLE = 7;

    struct qamrpp_context;
    struct qamrpp_value;
    alias NativeFunction = qamrpp_value* function(qamrpp_context*, qamrpp_value**, size_t);

    struct qamrpp_host_api {
        uint api_version;
        qamrpp_value* function(qamrpp_context*) value_nil;
        qamrpp_value* function(qamrpp_context*, int) value_bool;
        qamrpp_value* function(qamrpp_context*, long) value_int;
        qamrpp_value* function(qamrpp_context*, double) value_float;
        qamrpp_value* function(qamrpp_context*, const(char)*, size_t) value_string;
        qamrpp_value* function(qamrpp_context*, void*, void function(void*)) value_userdata;
        qamrpp_value* function(qamrpp_context*) value_table;
        int function(qamrpp_value*) value_get_type;
        int function(qamrpp_value*) value_as_bool;
        long function(qamrpp_value*) value_as_int;
        double function(qamrpp_value*) value_as_float;
        const(char)* function(qamrpp_value*, size_t*) value_as_string;
        void* function(qamrpp_value*) value_as_userdata;
        qamrpp_value* function(qamrpp_context*, qamrpp_value*, qamrpp_value*) table_raw_get;
        int function(qamrpp_context*, qamrpp_value*, qamrpp_value*, qamrpp_value*) table_raw_set;
        qamrpp_value* function(qamrpp_context*, qamrpp_value*, qamrpp_value*) table_get;
        int function(qamrpp_context*, qamrpp_value*, qamrpp_value*, qamrpp_value*) table_set;
        qamrpp_value* function(qamrpp_context*, qamrpp_value*) value_get_metatable;
        int function(qamrpp_context*, qamrpp_value*, qamrpp_value*) value_set_metatable;
        void function(qamrpp_context*, int, const(char)*) set_error;
        void* function(qamrpp_context*) context_get_userdata;
        void function(qamrpp_context*, void*) context_set_userdata;
        void function(qamrpp_context*, const(char)*, qamrpp_value*) set_global;
        qamrpp_value* function(qamrpp_context*, const(char)*) get_global;
    }

    struct qamrpp_native_binding { const(char)* name; NativeFunction fn; }
    struct qamrpp_library_descriptor {
        uint api_version;
        const(char)* name;
        qamrpp_native_binding* functions;
        size_t function_count;
        int function(qamrpp_context*, const(qamrpp_host_api)*) on_load;
        void function(qamrpp_context*) on_unload;
    }
}

struct Library {
    qamrpp_context* context;
    qamrpp_host_api* host;
    this(qamrpp_context* context, qamrpp_host_api* host) { this.context = context; this.host = host; }
    qamrpp_value* nil() { return host.value_nil(context); }
    qamrpp_value* boolean(bool value) { return host.value_bool(context, value ? 1 : 0); }
    qamrpp_value* integer(long value) { return host.value_int(context, value); }
    qamrpp_value* number(double value) { return host.value_float(context, value); }
    qamrpp_value* text(const(char)[] value) { return host.value_string(context, value.ptr, value.length); }
    qamrpp_value* table() { return host.value_table(context); }
    void global(const(char)[] name, qamrpp_value* value) { host.set_global(context, name.ptr, value); }
    qamrpp_value* global(const(char)[] name) { return host.get_global(context, name.ptr); }
    void error(int code, const(char)[] message) { host.set_error(context, code, message.ptr); }
}

struct Binding { string name; NativeFunction fn; }

struct Module {
    string name;
    Binding[] bindings;
    int function(qamrpp_context*, const(qamrpp_host_api)*) onLoad;
    void function(qamrpp_context*) onUnload;
}
