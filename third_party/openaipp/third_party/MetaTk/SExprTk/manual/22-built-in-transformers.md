# Chapter 22 — Built-in Transformers

`FlattenTransformer` recursively removes nested list structure into a single-level list. It is a structural normalization, not a semantics-preserving optimization for arbitrary languages.

`ConstantFoldTransformer` recognizes a symbol-headed cell whose symbol is `+` or `*`, recursively folds arguments, and replaces the form when all arguments are nonempty integer cells. Identities are:

- `+`: accumulator zero;
- `*`: accumulator one.

It does not fold floats, nested pair tails with noncanonical shapes, variables, overflow, or empty forms. Integer overflow follows C++ signed arithmetic rules and is not diagnosed.

`MapTransformer` applies a user `std::function<Cell(const Cell&)>` to top-level root cells only. It does not recursively map nested cells unless the callback performs recursion. Its optional name labels the pass.

All three preserve the surrounding `Cartable` fields by copy.
