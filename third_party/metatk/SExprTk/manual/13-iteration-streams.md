# Chapter 13 — Iteration and Lazy Streams

`Iterator` is a read-only cursor over a `List`:

```cpp
for (sexprtk::Iterator it(list).begin(); it != it.end(); ++it) {
    inspect(*it);
}
```

It stores a raw `List*` and an index. `done()` is true for a null list or an index at/after the cell count. Dereferencing an exhausted iterator is undefined at the vector layer.

`begin()` and `end()` are member factories rather than standard container methods on `List`; direct range-for requires adapting the list or using its `cells` vector.

`LazyStream` is a minimal appendable byte buffer with a cursor:

- `empty()` checks whether `pos >= buffer.size()`;
- `peek()` observes the next character;
- `take()` consumes one character;
- `append()` extends the buffer.

It performs no compaction, blocking, tokenization, or incremental parser integration.
