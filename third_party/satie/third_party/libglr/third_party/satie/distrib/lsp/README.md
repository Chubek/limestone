# Satie language server

Run `node server/index.js` from this directory and connect an LSP client over
standard input and output. Requires Node.js 18 or newer; no npm install is needed.

The server supports document synchronization, DIMACS header/terminator checks,
parenthesis diagnostics for the CNF DSL, command and engine completion, and hover
for the SAT engines and common REPL commands. It does not run a theory solver or
validate full CNF grammar; use `satie-cli` for authoritative parsing and solving.
