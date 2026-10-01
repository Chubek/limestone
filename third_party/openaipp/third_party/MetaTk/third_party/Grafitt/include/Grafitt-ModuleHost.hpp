#pragma once

#include "Grafitt-Module.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <sstream>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace grafitt::modulehost {

enum class host_diag_level {
    info = 0,
    warning = 1,
    error = 2
};

enum class lifecycle_event_kind {
    staged,
    start_requested,
    started,
    stop_requested,
    stopped,
    start_failed,
    stop_failed
};

struct host_diagnostic {
    host_diag_level level { host_diag_level::info };
    std::string code;
    std::string message;
    std::string module_name;
};

struct module_runtime_record {
    std::string module_name;
    GM_SemVersion module_version { 0u, 0u, 0u, 0u };
    bool started = false;
    std::vector<GM_CapabilityTable> capabilities;
};

enum class host_status {
    ok,
    invalid_argument,
    abi_mismatch,
    duplicate_module,
    metadata_invalid,
    dependency_missing,
    dependency_cycle,
    start_failed,
    end_failed,
    not_found
};

struct host_result {
    host_status status { host_status::ok };
    GM_Status module_status { GM_STATUS_OK };
    std::string module_name;

    [[nodiscard]] bool ok() const noexcept { return status == host_status::ok; }
};

struct lifecycle_event {
    lifecycle_event_kind kind { lifecycle_event_kind::staged };
    std::string module_name;
    host_status status { host_status::ok };
    GM_Status module_status { GM_STATUS_OK };
    std::string message;
};

struct host_options {
  uint32_t host_abi_version = GM_MODULE_ABI_VERSION;
  GM_SemVersion host_version { 0u, 1u, 0u, 0u };
  bool reject_duplicate_capabilities = false;
};

struct module_entry_points {
    GM_ModMetadata_Fn metadata = nullptr;
    GM_ModStart_Fn start = nullptr;
    GM_ModEnd_Fn end = nullptr;
};

class module_manager {
private:
    struct staged_module {
        module_entry_points ep;
        GM_ModMetadata_Record metadata {};
        bool started = false;
        std::vector<GM_CapabilityTable> capabilities;
    };

    host_options options_;
    std::vector<host_diagnostic> diagnostics_;
    std::vector<lifecycle_event> lifecycle_;
    std::unordered_map<std::string, staged_module> modules_;
    std::unordered_map<std::string, std::size_t> capability_owner_count_;
    std::vector<std::string> startup_order_;

    static std::string sv_to_string(GM_StringView sv) {
        if (sv.data == nullptr || sv.size == 0) return {};
        return std::string{sv.data, sv.data + sv.size};
    }

    static GM_StringView to_sv(const std::string& s) {
        return GM_StringView{ s.c_str(), static_cast<uint32_t>(s.size()) };
    }

    static const char* kind_to_cstr(lifecycle_event_kind kind) {
        switch (kind) {
            case lifecycle_event_kind::staged: return "staged";
            case lifecycle_event_kind::start_requested: return "start_requested";
            case lifecycle_event_kind::started: return "started";
            case lifecycle_event_kind::stop_requested: return "stop_requested";
            case lifecycle_event_kind::stopped: return "stopped";
            case lifecycle_event_kind::start_failed: return "start_failed";
            case lifecycle_event_kind::stop_failed: return "stop_failed";
        }
        return "unknown";
    }

    static const char* status_to_cstr(host_status st) {
        switch (st) {
            case host_status::ok: return "ok";
            case host_status::invalid_argument: return "invalid_argument";
            case host_status::abi_mismatch: return "abi_mismatch";
            case host_status::duplicate_module: return "duplicate_module";
            case host_status::metadata_invalid: return "metadata_invalid";
            case host_status::dependency_missing: return "dependency_missing";
            case host_status::dependency_cycle: return "dependency_cycle";
            case host_status::start_failed: return "start_failed";
            case host_status::end_failed: return "end_failed";
            case host_status::not_found: return "not_found";
        }
        return "ok";
    }

    static const char* diag_to_cstr(host_diag_level lv) {
        switch (lv) {
            case host_diag_level::info: return "info";
            case host_diag_level::warning: return "warning";
            case host_diag_level::error: return "error";
        }
        return "info";
    }

    static std::string escape_json(std::string_view s) {
        std::string out;
        out.reserve(s.size() + 8);
        for (char c : s) {
            switch (c) {
                case '\"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out.push_back(c); break;
            }
        }
        return out;
    }

    static host_status map_status(GM_Status st) {
        switch (st) {
            case GM_STATUS_OK: return host_status::ok;
            case GM_STATUS_INVALID_ARGUMENT: return host_status::invalid_argument;
            case GM_STATUS_ABI_MISMATCH: return host_status::abi_mismatch;
            case GM_STATUS_START_FAILED: return host_status::start_failed;
            case GM_STATUS_END_FAILED: return host_status::end_failed;
            default: return host_status::metadata_invalid;
        }
    }

    static void host_log_bridge(void* user, int level, GM_StringView message) {
        auto* self = static_cast<module_manager*>(user);
        if (!self) return;
        host_diag_level lv = host_diag_level::info;
        if (level >= 2) lv = host_diag_level::error;
        else if (level == 1) lv = host_diag_level::warning;
        self->diagnostics_.push_back(host_diagnostic{
            .level = lv,
            .code = "module.log",
            .message = sv_to_string(message),
            .module_name = {}
        });
    }

    static void* host_alloc_bridge(void*, size_t size, size_t) {
        return ::operator new(size, std::nothrow);
    }

    static void host_free_bridge(void*, void* ptr) {
        ::operator delete(ptr);
    }

    static GM_Status host_register_capability_bridge(void* user, const GM_CapabilityTable* table) {
        auto* self = static_cast<module_manager*>(user);
        if (!self || !table) return GM_STATUS_INVALID_ARGUMENT;
        if (table->capability_id == 0u || table->table == nullptr) return GM_STATUS_INVALID_ARGUMENT;
        return GM_STATUS_OK;
    }

    static void host_diag_bridge(void* user, int level, GM_StringView code, GM_StringView message) {
        auto* self = static_cast<module_manager*>(user);
        if (!self) return;
        host_diag_level lv = host_diag_level::info;
        if (level >= 2) lv = host_diag_level::error;
        else if (level == 1) lv = host_diag_level::warning;
        self->diagnostics_.push_back(host_diagnostic{
            .level = lv,
            .code = sv_to_string(code),
            .message = sv_to_string(message),
            .module_name = {}
        });
    }

public:
    module_manager() = default;
    explicit module_manager(host_options opts) : options_(opts) {}

    [[nodiscard]] const std::vector<host_diagnostic>& diagnostics() const noexcept {
        return diagnostics_;
    }

    [[nodiscard]] const std::vector<lifecycle_event>& lifecycle_events() const noexcept {
        return lifecycle_;
    }

    [[nodiscard]] std::vector<module_runtime_record> active_modules() const {
        std::vector<module_runtime_record> out;
        out.reserve(modules_.size());
        for (const auto& [name, m] : modules_) {
            module_runtime_record r;
            r.module_name = name;
            r.module_version = m.metadata.module_version;
            r.started = m.started;
            r.capabilities = m.capabilities;
            out.push_back(std::move(r));
        }
        std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.module_name < b.module_name; });
        return out;
    }

    [[nodiscard]] host_result stage_module(module_entry_points ep) {
        host_result out;
        if (!ep.metadata || !ep.start || !ep.end) {
            out.status = host_status::invalid_argument;
            return out;
        }
        const auto* meta = ep.metadata();
        const auto v = GM_validate_metadata(meta, options_.host_abi_version);
        if (v != GM_STATUS_OK) {
            out.status = map_status(v);
            out.module_status = v;
            diagnostics_.push_back(host_diagnostic{
                .level = host_diag_level::error,
                .code = "module.metadata.invalid",
                .message = "Module metadata validation failed.",
                .module_name = {}
            });
            return out;
        }

        const auto module_name = sv_to_string(meta->module_name);
        out.module_name = module_name;
        if (modules_.contains(module_name)) {
            out.status = host_status::duplicate_module;
            diagnostics_.push_back(host_diagnostic{
                .level = host_diag_level::error,
                .code = "module.duplicate",
                .message = "Duplicate module name rejected.",
                .module_name = module_name
            });
            return out;
        }

        staged_module sm;
        sm.ep = ep;
        sm.metadata = *meta;
        modules_.emplace(module_name, std::move(sm));
        lifecycle_.push_back(lifecycle_event{
            .kind = lifecycle_event_kind::staged,
            .module_name = module_name,
            .status = host_status::ok,
            .module_status = GM_STATUS_OK,
            .message = "Module staged."
        });

        diagnostics_.push_back(host_diagnostic{
            .level = host_diag_level::info,
            .code = "module.staged",
            .message = "Module metadata accepted.",
            .module_name = module_name
        });
        return out;
    }

    [[nodiscard]] host_result start_module(const std::string& module_name) {
        host_result out;
        out.module_name = module_name;
        lifecycle_.push_back(lifecycle_event{
            .kind = lifecycle_event_kind::start_requested,
            .module_name = module_name,
            .status = host_status::ok,
            .module_status = GM_STATUS_OK,
            .message = "Start requested."
        });
        auto it = modules_.find(module_name);
        if (it == modules_.end()) {
            out.status = host_status::not_found;
            lifecycle_.push_back(lifecycle_event{
                .kind = lifecycle_event_kind::start_failed,
                .module_name = module_name,
                .status = out.status,
                .module_status = out.module_status,
                .message = "Module not found."
            });
            return out;
        }
        auto& module = it->second;
        if (module.started) return out;

        for (uint32_t i = 0; i < module.metadata.dependency_count; ++i) {
            const auto& dep = module.metadata.dependencies[i];
            const auto dep_name = sv_to_string(dep.module_name);
            auto dep_it = modules_.find(dep_name);
            if (dep_it == modules_.end()) {
                out.status = host_status::dependency_missing;
                diagnostics_.push_back(host_diagnostic{
                    .level = host_diag_level::error,
                    .code = "module.dependency.missing",
                    .message = "Missing required dependency: " + dep_name,
                    .module_name = module_name
                });
                lifecycle_.push_back(lifecycle_event{
                    .kind = lifecycle_event_kind::start_failed,
                    .module_name = module_name,
                    .status = out.status,
                    .module_status = out.module_status,
                    .message = "Missing required dependency: " + dep_name
                });
                return out;
            }
            if (dep.required != 0u && !dep_it->second.started) {
                auto dep_started = start_module(dep_name);
                if (!dep_started.ok()) {
                    out.status = dep_started.status;
                    out.module_status = dep_started.module_status;
                    return out;
                }
            }
        }

        GM_HostAPI host_api {};
        host_api.size = static_cast<uint32_t>(sizeof(GM_HostAPI));
        host_api.host_abi_version = options_.host_abi_version;
        host_api.host_version = options_.host_version;
        host_api.user_data = this;
        host_api.log_fn = &module_manager::host_log_bridge;
        host_api.alloc_fn = &module_manager::host_alloc_bridge;
        host_api.free_fn = &module_manager::host_free_bridge;
        host_api.register_capability_fn = &module_manager::host_register_capability_bridge;
        host_api.report_diagnostic_fn = &module_manager::host_diag_bridge;

        auto hs = GM_validate_host_api(&host_api);
        if (hs != GM_STATUS_OK) {
            out.status = map_status(hs);
            out.module_status = hs;
            lifecycle_.push_back(lifecycle_event{
                .kind = lifecycle_event_kind::start_failed,
                .module_name = module_name,
                .status = out.status,
                .module_status = out.module_status,
                .message = "Host API validation failed."
            });
            return out;
        }

        uint32_t needed = 0u;
        auto st = module.ep.start(&host_api, nullptr, &needed);
        if (st != GM_STATUS_OK) {
            out.status = map_status(st);
            out.module_status = st;
            diagnostics_.push_back(host_diagnostic{
                .level = host_diag_level::error,
                .code = "module.start.failed",
                .message = "Module start (count query) failed.",
                .module_name = module_name
            });
            lifecycle_.push_back(lifecycle_event{
                .kind = lifecycle_event_kind::start_failed,
                .module_name = module_name,
                .status = out.status,
                .module_status = out.module_status,
                .message = "Module start count query failed."
            });
            return out;
        }

        std::vector<GM_CapabilityTable> tables(needed);
        for (auto& t : tables) t.size = static_cast<uint32_t>(sizeof(GM_CapabilityTable));
        st = module.ep.start(&host_api, tables.data(), &needed);
        if (st != GM_STATUS_OK) {
            out.status = map_status(st);
            out.module_status = st;
            diagnostics_.push_back(host_diagnostic{
                .level = host_diag_level::error,
                .code = "module.start.failed",
                .message = "Module start (table fetch) failed.",
                .module_name = module_name
            });
            lifecycle_.push_back(lifecycle_event{
                .kind = lifecycle_event_kind::start_failed,
                .module_name = module_name,
                .status = out.status,
                .module_status = out.module_status,
                .message = "Module start table fetch failed."
            });
            return out;
        }
        tables.resize(needed);
        st = GM_validate_capability_tables(tables.data(), needed);
        if (st != GM_STATUS_OK) {
            out.status = map_status(st);
            out.module_status = st;
            diagnostics_.push_back(host_diagnostic{
                .level = host_diag_level::error,
                .code = "module.capabilities.invalid",
                .message = "Capability tables validation failed.",
                .module_name = module_name
            });
            lifecycle_.push_back(lifecycle_event{
                .kind = lifecycle_event_kind::start_failed,
                .module_name = module_name,
                .status = out.status,
                .module_status = out.module_status,
                .message = "Capability table validation failed."
            });
            return out;
        }

        for (const auto& c : tables) {
            bool duplicate = false;
            for (const auto& [other_name, other_module] : modules_) {
                if (other_name == module_name || !other_module.started) continue;
                for (const auto& oc : other_module.capabilities) {
                    if (oc.capability_id == c.capability_id &&
                        oc.interface_version == c.interface_version) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) break;
            }
            if (duplicate) {
                const auto msg = "Capability collision for id=" + std::to_string(c.capability_id) +
                                 " interface_version=" + std::to_string(c.interface_version);
                if (options_.reject_duplicate_capabilities) {
                    out.status = host_status::metadata_invalid;
                    out.module_status = GM_STATUS_INCOMPATIBLE_MODULE;
                    diagnostics_.push_back(host_diagnostic{
                        .level = host_diag_level::error,
                        .code = "module.capability.duplicate.rejected",
                        .message = msg,
                        .module_name = module_name
                    });
                    lifecycle_.push_back(lifecycle_event{
                        .kind = lifecycle_event_kind::start_failed,
                        .module_name = module_name,
                        .status = out.status,
                        .module_status = out.module_status,
                        .message = msg
                    });
                    return out;
                }
                diagnostics_.push_back(host_diagnostic{
                    .level = host_diag_level::warning,
                    .code = "module.capability.duplicate.allowed",
                    .message = msg,
                    .module_name = module_name
                });
            }
        }

        module.capabilities = tables;
        module.started = true;
        if (std::find(startup_order_.begin(), startup_order_.end(), module_name) == startup_order_.end()) {
            startup_order_.push_back(module_name);
        }
        for (const auto& c : module.capabilities) {
            const auto key = module_name + "#" + std::to_string(c.capability_id);
            capability_owner_count_[key] += 1;
        }

        diagnostics_.push_back(host_diagnostic{
            .level = host_diag_level::info,
            .code = "module.started",
            .message = "Module started successfully.",
            .module_name = module_name
        });
        lifecycle_.push_back(lifecycle_event{
            .kind = lifecycle_event_kind::started,
            .module_name = module_name,
            .status = host_status::ok,
            .module_status = GM_STATUS_OK,
            .message = "Module started."
        });
        return out;
    }

    [[nodiscard]] host_result start_all() {
        host_result out;
        std::unordered_map<std::string, std::set<std::string>> deps;
        std::unordered_map<std::string, std::size_t> indeg;
        std::unordered_map<std::string, std::vector<std::string>> rev;

        for (const auto& [name, module] : modules_) {
            indeg[name] = 0;
            deps[name] = {};
            if (module.metadata.dependency_count > 0 && module.metadata.dependencies == nullptr) {
                out.status = host_status::metadata_invalid;
                out.module_name = name;
                return out;
            }
            for (uint32_t i = 0; i < module.metadata.dependency_count; ++i) {
                const auto dep_name = sv_to_string(module.metadata.dependencies[i].module_name);
                if (dep_name.empty()) continue;
                if (!modules_.contains(dep_name)) {
                    if (module.metadata.dependencies[i].required != 0u) {
                        out.status = host_status::dependency_missing;
                        out.module_name = name;
                        diagnostics_.push_back(host_diagnostic{
                            .level = host_diag_level::error,
                            .code = "module.dependency.missing",
                            .message = "Missing required dependency: " + dep_name,
                            .module_name = name
                        });
                        return out;
                    }
                    continue;
                }
                if (deps[name].insert(dep_name).second) {
                    ++indeg[name];
                    rev[dep_name].push_back(name);
                }
            }
        }

        std::deque<std::string> q;
        for (const auto& [name, d] : indeg) {
            if (d == 0) q.push_back(name);
        }
        std::vector<std::string> order;
        order.reserve(modules_.size());
        while (!q.empty()) {
            auto n = q.front();
            q.pop_front();
            order.push_back(n);
            for (const auto& nxt : rev[n]) {
                auto it = indeg.find(nxt);
                if (it == indeg.end()) continue;
                if (--it->second == 0) q.push_back(nxt);
            }
        }
        if (order.size() != modules_.size()) {
            out.status = host_status::dependency_cycle;
            diagnostics_.push_back(host_diagnostic{
                .level = host_diag_level::error,
                .code = "module.dependency.cycle",
                .message = "Dependency cycle detected during start_all.",
                .module_name = {}
            });
            return out;
        }

        for (const auto& name : order) {
            auto res = start_module(name);
            if (!res.ok()) return res;
        }
        return out;
    }

    [[nodiscard]] host_result stop_module(const std::string& module_name) {
        host_result out;
        out.module_name = module_name;
        lifecycle_.push_back(lifecycle_event{
            .kind = lifecycle_event_kind::stop_requested,
            .module_name = module_name,
            .status = host_status::ok,
            .module_status = GM_STATUS_OK,
            .message = "Stop requested."
        });
        auto it = modules_.find(module_name);
        if (it == modules_.end()) {
            out.status = host_status::not_found;
            lifecycle_.push_back(lifecycle_event{
                .kind = lifecycle_event_kind::stop_failed,
                .module_name = module_name,
                .status = out.status,
                .module_status = out.module_status,
                .message = "Module not found."
            });
            return out;
        }
        auto& module = it->second;
        if (!module.started) return out;

        const auto st = module.ep.end();
        if (st != GM_STATUS_OK) {
            out.status = map_status(st);
            out.module_status = st;
            diagnostics_.push_back(host_diagnostic{
                .level = host_diag_level::error,
                .code = "module.stop.failed",
                .message = "Module shutdown failed.",
                .module_name = module_name
            });
            lifecycle_.push_back(lifecycle_event{
                .kind = lifecycle_event_kind::stop_failed,
                .module_name = module_name,
                .status = out.status,
                .module_status = out.module_status,
                .message = "Module shutdown failed."
            });
            return out;
        }

        for (const auto& c : module.capabilities) {
            const auto key = module_name + "#" + std::to_string(c.capability_id);
            auto cnt = capability_owner_count_.find(key);
            if (cnt != capability_owner_count_.end()) {
                if (cnt->second <= 1) capability_owner_count_.erase(cnt);
                else --cnt->second;
            }
        }
        module.capabilities.clear();
        module.started = false;
        startup_order_.erase(std::remove(startup_order_.begin(), startup_order_.end(), module_name), startup_order_.end());
        diagnostics_.push_back(host_diagnostic{
            .level = host_diag_level::info,
            .code = "module.stopped",
            .message = "Module stopped successfully.",
            .module_name = module_name
        });
        lifecycle_.push_back(lifecycle_event{
            .kind = lifecycle_event_kind::stopped,
            .module_name = module_name,
            .status = host_status::ok,
            .module_status = GM_STATUS_OK,
            .message = "Module stopped."
        });
        return out;
    }

    [[nodiscard]] host_result stop_all() {
        host_result out;
        for (auto it = startup_order_.rbegin(); it != startup_order_.rend(); ++it) {
            auto res = stop_module(*it);
            if (!res.ok()) return res;
        }
        startup_order_.clear();
        return out;
    }

    [[nodiscard]] std::pair<host_result, std::vector<std::string>> startup_plan() const {
        host_result out;
        std::unordered_map<std::string, std::size_t> indeg;
        std::unordered_map<std::string, std::vector<std::string>> rev;

        for (const auto& [name, module] : modules_) {
            indeg[name] = 0;
            for (uint32_t i = 0; i < module.metadata.dependency_count; ++i) {
                const auto dep_name = sv_to_string(module.metadata.dependencies[i].module_name);
                if (dep_name.empty()) continue;
                if (!modules_.contains(dep_name)) {
                    if (module.metadata.dependencies[i].required != 0u) {
                        out.status = host_status::dependency_missing;
                        out.module_name = name;
                        return {out, {}};
                    }
                    continue;
                }
                ++indeg[name];
                rev[dep_name].push_back(name);
            }
        }

        std::deque<std::string> q;
        for (const auto& [name, d] : indeg) if (d == 0) q.push_back(name);
        std::vector<std::string> order;
        while (!q.empty()) {
            auto n = q.front();
            q.pop_front();
            order.push_back(n);
            for (const auto& nxt : rev[n]) {
                auto it = indeg.find(nxt);
                if (it != indeg.end() && --it->second == 0) q.push_back(nxt);
            }
        }
        if (order.size() != modules_.size()) {
            out.status = host_status::dependency_cycle;
            return {out, {}};
        }
        return {out, order};
    }

    [[nodiscard]] std::string to_text_report() const {
        std::ostringstream oss;
        oss << "modulehost.modules=" << modules_.size()
            << " active=" << active_modules().size()
            << " startup_order=" << startup_order_.size() << "\n";
        oss << "lifecycle(" << lifecycle_.size() << "):\n";
        for (const auto& e : lifecycle_) {
            oss << "  - " << kind_to_cstr(e.kind)
                << " module=" << e.module_name
                << " status=" << status_to_cstr(e.status)
                << " module_status=" << static_cast<int>(e.module_status)
                << " msg=" << e.message << "\n";
        }
        oss << "diagnostics(" << diagnostics_.size() << "):\n";
        for (const auto& d : diagnostics_) {
            oss << "  - [" << diag_to_cstr(d.level) << "] "
                << d.module_name << " code=" << d.code
                << " msg=" << d.message << "\n";
        }
        return oss.str();
    }

    [[nodiscard]] std::string to_json_report() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"module_count\":" << modules_.size() << ",";
        oss << "\"active_count\":" << active_modules().size() << ",";
        oss << "\"startup_order_count\":" << startup_order_.size() << ",";
        oss << "\"lifecycle\":[";
        for (std::size_t i = 0; i < lifecycle_.size(); ++i) {
            const auto& e = lifecycle_[i];
            if (i) oss << ",";
            oss << "{"
                << "\"kind\":\"" << kind_to_cstr(e.kind) << "\","
                << "\"module_name\":\"" << escape_json(e.module_name) << "\","
                << "\"status\":\"" << status_to_cstr(e.status) << "\","
                << "\"module_status\":" << static_cast<int>(e.module_status) << ","
                << "\"message\":\"" << escape_json(e.message) << "\""
                << "}";
        }
        oss << "],";
        oss << "\"diagnostics\":[";
        for (std::size_t i = 0; i < diagnostics_.size(); ++i) {
            const auto& d = diagnostics_[i];
            if (i) oss << ",";
            oss << "{"
                << "\"level\":\"" << diag_to_cstr(d.level) << "\","
                << "\"module_name\":\"" << escape_json(d.module_name) << "\","
                << "\"code\":\"" << escape_json(d.code) << "\","
                << "\"message\":\"" << escape_json(d.message) << "\""
                << "}";
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

} // namespace grafitt::modulehost
