# Chapter 32 — Migration Notes & Best Practices

## Migration Guidance
- Keep older `gbin::deserialize(...)` calls; migrate to detailed decode where diagnostics matter.
- Move ad hoc pass wiring toward registry + presets for reproducibility.
- Adopt typed analysis keys for robust analysis reuse.

## Best Practices
- Prefer rich result types over booleans in application code.
- Validate condensation/covers before downstream consumption.
- Use report exporters in CI for pass/module audit trails.
- Treat module ABI boundaries as strict C contracts.
