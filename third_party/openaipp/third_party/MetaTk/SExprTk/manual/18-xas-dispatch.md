# Chapter 18 — Dispatch, Sources, Sinks, and Pumping

`XASEventDispatcher` buffers every event and optionally invokes a C++ sink:

```cpp
XASEventDispatcher d;
d.sink = [](const XASEvent& e) { consume(e); };
auto c = rt.parse(source, &d);
```

`as_c_sink()` returns a `sexprtk_xas_event_sink` whose trampoline converts C events into owned C++ `XASEvent` values.

`CartableDispatcher` generates sequences and convenience events (`document_begin`, `begin_list`, `atom`, `end_list`, `document_end`). Without a dispatcher it still increments its sequence.

`sexprtk_xas_pump` repeatedly calls a source’s `next`, then the sink’s `handle`. `ERR_TRUNCATED` from `next` means clean end-of-stream and is converted to `OK`. Any other source error propagates. A negative sink return aborts and propagates unchanged. Null callbacks return `ERR_INVALID`.

Pump consumers must define ownership and event lifetime explicitly; the protocol itself does not retain source payloads.
