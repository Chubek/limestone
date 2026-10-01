#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parent

TOPICS = [
    "Architecture", "Grammar", "Lexemes", "Lists", "Atoms", "Cells",
    "Cartables", "Sources", "Streams", "Iterators", "Serialization", "JSON",
    "Packaging", "Manifests", "Events", "Datagrams", "Dispatch", "Analysis",
    "Transformation", "Kernels", "Lua Bridge", "S7 Bridge", "Data Exchange",
    "PikoLisp", "Validation", "Error Model", "Comments", "Quoting",
    "Traversal", "Normalization", "Metadata", "Embedding", "Testing",
    "Build System", "Performance", "Deployment",
]

PARAGRAPH = (
    "SExprTk keeps semantic structure explicit, preserves list topology, and "
    "treats surface syntax as transport rather than authority. The implementation "
    "favors deterministic parsing, reproducible serialization, shallow extension "
    "points, and a compact event protocol suitable for streaming, testing, and "
    "cross-process exchange without hidden runtime state."
)

CODE_SAMPLES = [
    "```text\n(alpha beta gamma)\n```",
    "```cpp\nauto cartable = sexprtk::SExprTk{}.parse(sexprtk::Source::from_string(\"(k v)\"));\n```",
    "```text\n1|atom|payload\n```",
    "```cpp\nsexprtk_xas::DatagramFrame::from_event({7, sexprtk_xas::EventType::Atom, \"x\"});\n```",
]

for index, topic in enumerate(TOPICS, start=1):
    chapter = ROOT / f"chapter{index:02d}.md"
    lines = [f"# Chapter {index:02d}: {topic}", ""]
    for p in range(72):
        lines.append(
            f"{topic} paragraph {p + 1}. {PARAGRAPH} "
            f"This chapter instance records invariants, layer boundaries, canonical forms, "
            f"and failure handling rules for the {topic.lower()} surface."
        )
        lines.append("")
        if p in {17, 35, 53, 71}:
            lines.append(CODE_SAMPLES[[17, 35, 53, 71].index(p)])
            lines.append("")
    chapter.write_text("\n".join(lines), encoding="utf-8")

