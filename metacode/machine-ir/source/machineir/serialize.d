module machineir.serialize;

import std.array : appender;
import machineir.ir;
import machineir.isa;

string dump(const(MachineSpec) m) {
    auto a=appender!string();
    a.put("machine "); a.put(m.arch); a.put(" {\n");
    a.put("  family = "); a.put(m.family); a.put("\n");
    a.put("  model = "); a.put(m.model); a.put("\n");
    a.put("  kind = "); a.put(kindName(m.kind)); a.put("\n");
    a.put("  word_size = "); a.put(toString(m.wordSize)); a.put("\n");
    a.put("  address_size = "); a.put(toString(m.addressSize)); a.put("\n");
    a.put("  register_classes = "); a.put(toString(m.registerClasses.length)); a.put("\n");
    a.put("  encodings = "); a.put(toString(m.encodings.length)); a.put("\n");
    a.put("  instructions = "); a.put(toString(m.instructions.length)); a.put("\n");
    a.put("}\n");
    return a.data;
}
private string kindName(MachineKind k) { return toString(k); }
private string toString(T)(T x) { import std.conv : to; return to!string(x); }
