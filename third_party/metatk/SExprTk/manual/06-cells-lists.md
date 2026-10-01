# Chapter 6 — Cells and Lists

`List` owns `std::vector<Cell> cells` and supplies vector-like operations: `push`, `pop`, `empty`, `size`, `front`, `back`, and indexed access.

`Cell` contains:

- `head`: the primary atom;
- `tail`: zero or more subordinate cells.

Classification:

- `is_atom()`: empty tail and non-list head;
- `is_pair()`: nonempty tail and non-list head;
- `is_list_cell()`: list-valued head.

For a list-valued head, `car()` returns the first nested cell. For an ordinary cell, `car()` returns the cell itself. `cdr()` returns a copy of `tail`. Empty nested lists make `car()` unsafe because the implementation calls `front()` without an emptiness check.

The parser represents `(a b c)` as a list-valued cell whose nested `List` contains three cells. The document root is another `List`, so a parsed document containing one form serializes with an additional outer pair of parentheses.
