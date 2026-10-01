# Chapter 31 — Module Host Lifecycle

Include `Grafitt-ModuleHost.hpp`.

## Host Responsibilities
- stage modules
- validate metadata and ABI compatibility
- resolve/start dependencies
- stop modules in safe order
- record diagnostics and lifecycle events

## Reporting
- dependency startup plan preview
- text/json host reports
